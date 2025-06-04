/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drm_pipeline.h"

#include <errno.h>

#include "core/session.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_buffer_gbm.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_layer.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_plane.h"

#include <drm_fourcc.h>
#include <gbm.h>

namespace KWin
{

static const QMap<uint32_t, QVector<uint64_t>> legacyFormats = {{DRM_FORMAT_XRGB8888, {}}};
static const QMap<uint32_t, QVector<uint64_t>> legacyCursorFormats = {{DRM_FORMAT_ARGB8888, {}}};

DrmPipeline::DrmPipeline(DrmConnector *conn)
    : m_connector(conn)
{
}

bool DrmPipeline::testScanout()
{
    // TODO make the modeset check only be tested at most once per scanout cycle
    if (gpu()->needsModeset()) {
        return false;
    }
    if (gpu()->atomicModeSetting()) {
        return commitPipelines({this}, CommitMode::Test) == Error::None;
    } else {
        if (m_pending.layer->currentBuffer()->buffer()->size() != m_pending.mode->size()) {
            // scaling isn't supported with the legacy API
            return false;
        }
        // no other way to test than to do it.
        // As we only have a maximum of one test per scanout cycle, this is fine
        return presentLegacy() == Error::None;
    }
}

DrmPipeline::Error DrmPipeline::present()
{
    Q_ASSERT(m_pending.crtc);
    if (gpu()->atomicModeSetting()) {
        return commitPipelines({this}, CommitMode::Commit);
    } else {
        if (m_pending.layer->hasDirectScanoutBuffer()) {
            // already presented
            return Error::None;
        }
        return presentLegacy();
    }
}

bool DrmPipeline::maybeModeset()
{
    m_modesetPresentPending = true;
    return gpu()->maybeModeset();
}

DrmPipeline::Error DrmPipeline::commitPipelines(const QVector<DrmPipeline *> &pipelines, CommitMode mode, const QVector<DrmObject *> &unusedObjects)
{
    Q_ASSERT(!pipelines.isEmpty());
    if (pipelines[0]->gpu()->atomicModeSetting()) {
        return commitPipelinesAtomic(pipelines, mode, unusedObjects);
    } else {
        return commitPipelinesLegacy(pipelines, mode);
    }
}

DrmPipeline::Error DrmPipeline::commitPipelinesAtomic(const QVector<DrmPipeline *> &pipelines, CommitMode mode, const QVector<DrmObject *> &unusedObjects)
{
    const auto &failed = [&pipelines, &unusedObjects]() {
        for (const auto &pipeline : pipelines) {
            pipeline->printDebugInfo();
            pipeline->atomicCommitFailed();
        }
        for (const auto &obj : unusedObjects) {
            obj->printProps(DrmObject::PrintMode::OnlyChanged);
            obj->rollbackPending();
        }
    };

    DrmUniquePtr<drmModeAtomicReq> req{drmModeAtomicAlloc()};
    if (!req) {
        qCCritical(KWIN_DRM) << "Failed to allocate drmModeAtomicReq!" << strerror(errno);
        return Error::OutofMemory;
    }
    for (const auto &pipeline : pipelines) {
        pipeline->checkHardwareRotation();
        if (pipeline->activePending()) {
            if (!pipeline->m_pending.layer->checkTestBuffer()) {
                qCWarning(KWIN_DRM) << "Checking test buffer failed for" << mode;
                failed();
                return Error::TestBufferFailed;
            }
            pipeline->prepareAtomicPresentation();
            if (mode == CommitMode::TestAllowModeset || mode == CommitMode::CommitModeset) {
                pipeline->prepareAtomicModeset();
            }
        } else {
            pipeline->prepareAtomicDisable();
        }
        if (!pipeline->populateAtomicValues(req.get())) {
            failed();
            return errnoToError();
        }
    }
    for (const auto &unused : unusedObjects) {
        unused->disable();
        if (!unused->atomicPopulate(req.get())) {
            qCWarning(KWIN_DRM) << "Populating atomic values failed for unused resource" << unused;
            failed();
            return errnoToError();
        }
    }
    const auto gpu = pipelines[0]->gpu();
    switch (mode) {
    case CommitMode::TestAllowModeset: {
        bool withModeset = drmModeAtomicCommit(gpu->fd(), req.get(), DRM_MODE_ATOMIC_ALLOW_MODESET | DRM_MODE_ATOMIC_TEST_ONLY, nullptr) == 0;
        if (!withModeset) {
            qCDebug(KWIN_DRM, "Atomic modeset test on GPU %s failed! %s", qPrintable(gpu->devNode()), strerror(errno));
            failed();
            return errnoToError();
        }
        bool withoutModeset = drmModeAtomicCommit(gpu->fd(), req.get(), DRM_MODE_ATOMIC_TEST_ONLY, nullptr) == 0;
        for (const auto &pipeline : pipelines) {
            pipeline->m_pending.needsModeset = !withoutModeset;
        }
        std::for_each(pipelines.begin(), pipelines.end(), std::mem_fn(&DrmPipeline::atomicTestSuccessful));
        std::for_each(unusedObjects.begin(), unusedObjects.end(), std::mem_fn(&DrmObject::commitPending));
        return Error::None;
    }
    case CommitMode::CommitModeset: {
        // The kernel fails commits with DRM_MODE_PAGE_FLIP_EVENT when a crtc is disabled in the commit
        // and already was disabled before, to work around some quirks in old userspace.
        // Instead of using DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK, do the modeset in a blocking
        // fashion without page flip events and directly call the pageFlipped method afterwards
        bool commit = drmModeAtomicCommit(gpu->fd(), req.get(), DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr) == 0;
        if (!commit) {
            qCCritical(KWIN_DRM, "Atomic modeset commit on GPU %s failed! %s", qPrintable(gpu->devNode()), strerror(errno));
            failed();
            return errnoToError();
        }
        std::for_each(pipelines.begin(), pipelines.end(), std::mem_fn(&DrmPipeline::atomicModesetSuccessful));
        for (const auto &obj : unusedObjects) {
            obj->commitPending();
            obj->commit();
            if (auto crtc = dynamic_cast<DrmCrtc *>(obj)) {
                crtc->flipBuffer();
            } else if (auto plane = dynamic_cast<DrmPlane *>(obj)) {
                plane->flipBuffer();
            }
        }
        return Error::None;
    }
    case CommitMode::Test: {
        bool test = drmModeAtomicCommit(pipelines[0]->gpu()->fd(), req.get(), DRM_MODE_ATOMIC_TEST_ONLY, nullptr) == 0;
        if (!test) {
            qCDebug(KWIN_DRM, "Atomic test on GPU %s failed! %s", qPrintable(pipelines[0]->gpu()->devNode()), strerror(errno));
            failed();
            return errnoToError();
        }
        std::for_each(pipelines.begin(), pipelines.end(), std::mem_fn(&DrmPipeline::atomicTestSuccessful));
        Q_ASSERT(unusedObjects.isEmpty());
        return Error::None;
    }
    case CommitMode::Commit: {
        bool commit = drmModeAtomicCommit(pipelines[0]->gpu()->fd(), req.get(), DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, gpu) == 0;
        if (!commit) {
            qCCritical(KWIN_DRM, "Atomic commit on GPU %s failed! %s", qPrintable(pipelines[0]->gpu()->devNode()), strerror(errno));
            failed();
            return errnoToError();
        }
        std::for_each(pipelines.begin(), pipelines.end(), std::mem_fn(&DrmPipeline::atomicCommitSuccessful));
        Q_ASSERT(unusedObjects.isEmpty());
        return Error::None;
    }
    default:
        Q_UNREACHABLE();
    }
}

static QSize orientateSize(const QSize &size, DrmPlane::Transformations transforms)
{
    if (transforms & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) {
        return size.transposed();
    } else {
        return size;
    }
}

static QRect centerBuffer(const QSize &bufferSize, const QSize &modeSize)
{
    const double widthScale = bufferSize.width() / double(modeSize.width());
    const double heightScale = bufferSize.height() / double(modeSize.height());
    if (widthScale > heightScale) {
        const QSize size = bufferSize / widthScale;
        const uint32_t yOffset = (modeSize.height() - size.height()) / 2;
        return QRect(QPoint(0, yOffset), size);
    } else {
        const QSize size = bufferSize / heightScale;
        const uint32_t xOffset = (modeSize.width() - size.width()) / 2;
        return QRect(QPoint(xOffset, 0), size);
    }
}

void DrmPipeline::prepareAtomicPresentation()
{
    if (const auto contentType = m_connector->getProp(DrmConnector::PropertyIndex::ContentType)) {
        contentType->setEnum(m_pending.contentType);
    }

    m_pending.crtc->setPending(DrmCrtc::PropertyIndex::VrrEnabled, m_pending.syncMode == RenderLoopPrivate::SyncMode::Adaptive || m_pending.syncMode == RenderLoopPrivate::SyncMode::AdaptiveAsync);

    const auto fb = m_pending.layer->currentBuffer().get();
    m_pending.crtc->primaryPlane()->set(QPoint(0, 0), fb->buffer()->size(), centerBuffer(orientateSize(fb->buffer()->size(), m_pending.bufferOrientation), m_pending.mode->size()));
    m_pending.crtc->primaryPlane()->setBuffer(fb);

    if (m_pending.crtc->cursorPlane()) {
        const auto layer = cursorLayer();
        m_pending.crtc->cursorPlane()->set(QPoint(0, 0), gpu()->cursorSize(), QRect(layer->position(), gpu()->cursorSize()));
        m_pending.crtc->cursorPlane()->setBuffer(layer->isVisible() ? layer->currentBuffer().get() : nullptr);
        m_pending.crtc->cursorPlane()->setPending(DrmPlane::PropertyIndex::CrtcId, layer->isVisible() ? m_pending.crtc->id() : 0);
    }
}

void DrmPipeline::prepareAtomicDisable()
{
    m_connector->disable();
    if (m_pending.crtc) {
        m_pending.crtc->disable();
        m_pending.crtc->primaryPlane()->disable();
        if (auto cursor = m_pending.crtc->cursorPlane()) {
            cursor->disable();
        }
    }
}

void DrmPipeline::prepareAtomicModeset()
{
    m_connector->setPending(DrmConnector::PropertyIndex::CrtcId, m_pending.crtc->id());
    if (const auto prop = m_connector->getProp(DrmConnector::PropertyIndex::Broadcast_RGB)) {
        prop->setEnum(m_pending.rgbRange);
    }
    if (const auto prop = m_connector->getProp(DrmConnector::PropertyIndex::LinkStatus)) {
        prop->setEnum(DrmConnector::LinkStatus::Good);
    }
    if (const auto overscan = m_connector->getProp(DrmConnector::PropertyIndex::Overscan)) {
        overscan->setPending(m_pending.overscan);
    } else if (const auto underscan = m_connector->getProp(DrmConnector::PropertyIndex::Underscan)) {
        const uint32_t hborder = calculateUnderscan();
        underscan->setEnum(m_pending.overscan != 0 ? DrmConnector::UnderscanOptions::On : DrmConnector::UnderscanOptions::Off);
        m_connector->getProp(DrmConnector::PropertyIndex::Underscan_vborder)->setPending(m_pending.overscan);
        m_connector->getProp(DrmConnector::PropertyIndex::Underscan_hborder)->setPending(hborder);
    }
    if (const auto bpc = m_connector->getProp(DrmConnector::PropertyIndex::MaxBpc)) {
        uint64_t preferred = 8;
        if (auto backend = dynamic_cast<EglGbmBackend *>(gpu()->platform()->renderBackend()); backend && backend->prefer10bpc()) {
            preferred = 10;
        }
        bpc->setPending(std::min(bpc->maxValue(), preferred));
    }
    if (const auto hdr = m_connector->getProp(DrmConnector::PropertyIndex::HdrMetadata)) {
        hdr->setPending(0);
    }

    m_pending.crtc->setPending(DrmCrtc::PropertyIndex::Active, 1);
    m_pending.crtc->setPending(DrmCrtc::PropertyIndex::ModeId, m_pending.mode->blob()->blobId());

    m_pending.crtc->primaryPlane()->setPending(DrmPlane::PropertyIndex::CrtcId, m_pending.crtc->id());
    if (const auto rotation = m_pending.crtc->primaryPlane()->getProp(DrmPlane::PropertyIndex::Rotation)) {
        rotation->setEnum(m_pending.bufferOrientation);
    }
    if (m_pending.crtc->cursorPlane()) {
        if (const auto rotation = m_pending.crtc->cursorPlane()->getProp(DrmPlane::PropertyIndex::Rotation)) {
            rotation->setEnum(DrmPlane::Transformation::Rotate0);
        }
    }

    if (needUpdateBrightness()) {
        m_connector->setPending(DrmConnector::PropertyIndex::BrightnessId, m_pending.brightness);
    }
    if (const auto gammaProp = m_pending.crtc->getProp(DrmCrtc::PropertyIndex::Gamma_LUT)) {
        if (m_pending.gamma && !m_pending.crtc->hasColorMode()) {
            gammaProp->setPending(m_pending.gamma->blob()->blobId());
        } else if (needUpdateColorMode()) {
            gammaProp->setPending(m_pending.colorMode->blob()->blobId());
        }
    }
    if (const auto ctmProp = m_pending.crtc->getProp(DrmCrtc::PropertyIndex::CTM)) {
        if (needUpdateCTM()) {
            ctmProp->setPending(m_pending.ctm->blob()->blobId());
        }
    }
}

bool DrmPipeline::populateAtomicValues(drmModeAtomicReq *req)
{
    if (!m_connector->atomicPopulate(req)) {
        return false;
    }
    if (m_pending.crtc) {
        if (!m_pending.crtc->atomicPopulate(req)) {
            return false;
        }
        if (!m_pending.crtc->primaryPlane()->atomicPopulate(req)) {
            return false;
        }
        if (m_pending.crtc->cursorPlane() && !m_pending.crtc->cursorPlane()->atomicPopulate(req)) {
            return false;
        }
    }
    return true;
}

void DrmPipeline::checkHardwareRotation()
{
    if (m_pending.crtc && m_pending.crtc->primaryPlane()) {
        const bool supported = (m_pending.bufferOrientation & m_pending.crtc->primaryPlane()->supportedTransformations());
        if (!supported) {
            m_pending.bufferOrientation = DrmPlane::Transformation::Rotate0;
        }
    } else {
        m_pending.bufferOrientation = DrmPlane::Transformation::Rotate0;
    }
}

uint32_t DrmPipeline::calculateUnderscan()
{
    const auto size = m_pending.mode->size();
    const float aspectRatio = size.width() / static_cast<float>(size.height());
    uint32_t hborder = m_pending.overscan * aspectRatio;
    if (hborder > 128) {
        // overscan only goes from 0-100 so we cut off the 101-128 value range of underscan_vborder
        hborder = 128;
        m_pending.overscan = 128 / aspectRatio;
    }
    return hborder;
}

DrmPipeline::Error DrmPipeline::errnoToError()
{
    switch (errno) {
    case EINVAL:
        return Error::InvalidArguments;
    case EBUSY:
        return Error::FramePending;
    case ENOMEM:
        return Error::OutofMemory;
    case EACCES:
        return Error::NoPermission;
    default:
        return Error::Unknown;
    }
}

void DrmPipeline::atomicCommitFailed()
{
    m_connector->rollbackPending();
    if (m_pending.crtc) {
        m_pending.crtc->rollbackPending();
        m_pending.crtc->primaryPlane()->rollbackPending();
        if (m_pending.crtc->cursorPlane()) {
            m_pending.crtc->cursorPlane()->rollbackPending();
        }
    }
}

void DrmPipeline::atomicTestSuccessful()
{
    m_connector->commitPending();
    if (m_pending.crtc) {
        m_pending.crtc->commitPending();
        m_pending.crtc->primaryPlane()->commitPending();
        if (m_pending.crtc->cursorPlane()) {
            m_pending.crtc->cursorPlane()->commitPending();
        }
    }
}

void DrmPipeline::atomicCommitSuccessful()
{
    atomicTestSuccessful();
    m_pending.needsModeset = false;
    if (activePending()) {
        m_pageflipPending = true;
    }
    m_connector->commit();
    if (m_pending.crtc) {
        m_pending.crtc->commit();
        m_pending.crtc->primaryPlane()->setNext(m_pending.layer->currentBuffer());
        m_pending.crtc->primaryPlane()->commit();
        if (m_pending.crtc->cursorPlane()) {
            m_pending.crtc->cursorPlane()->setNext(cursorLayer()->currentBuffer());
            m_pending.crtc->cursorPlane()->commit();
        }
    }
    m_current = m_pending;
}

void DrmPipeline::atomicModesetSuccessful()
{
    atomicCommitSuccessful();
    m_pending.needsModeset = false;
    if (activePending()) {
        pageFlipped(std::chrono::steady_clock::now().time_since_epoch());
    }
}

bool DrmPipeline::setCursor(const QPoint &hotspot)
{
    bool result;
    m_pending.cursorHotspot = hotspot;
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (m_pending.crtc->cursorPlane()) {
        result = commitPipelines({this}, CommitMode::Test) == Error::None;
        if (result && m_output) {
            m_output->renderLoop()->scheduleRepaint();
        }
    } else {
        result = setCursorLegacy();
    }
    if (result) {
        m_next = m_pending;
    } else {
        m_pending = m_next;
    }
    return result;
}

bool DrmPipeline::moveCursor()
{
    bool result;
    // explicitly check for the cursor plane and not for AMS, as we might not always have one
    if (m_pending.crtc->cursorPlane()) {
        result = commitPipelines({this}, CommitMode::Test) == Error::None;
    } else {
        result = moveCursorLegacy();
    }
    if (result) {
        m_next = m_pending;
        if (m_output) {
            m_output->renderLoop()->scheduleRepaint();
        }
    } else {
        m_pending = m_next;
    }
    return result;
}

void DrmPipeline::applyPendingChanges()
{
    m_next = m_pending;
}

QSize DrmPipeline::bufferSize() const
{
    const auto modeSize = m_pending.mode->size();
    if (m_pending.bufferOrientation & (DrmPlane::Transformation::Rotate90 | DrmPlane::Transformation::Rotate270)) {
        return modeSize.transposed();
    }
    return modeSize;
}

DrmConnector *DrmPipeline::connector() const
{
    return m_connector;
}

DrmGpu *DrmPipeline::gpu() const
{
    return m_connector->gpu();
}

void DrmPipeline::pageFlipped(std::chrono::nanoseconds timestamp)
{
    m_current.crtc->flipBuffer();
    if (m_current.crtc->primaryPlane()) {
        m_current.crtc->primaryPlane()->flipBuffer();
    }
    if (m_current.crtc->cursorPlane()) {
        m_current.crtc->cursorPlane()->flipBuffer();
    }
    m_pageflipPending = false;
    if (m_output) {
        m_output->pageFlipped(timestamp);
    }
}

void DrmPipeline::setOutput(DrmOutput *output)
{
    m_output = output;
}

DrmOutput *DrmPipeline::output() const
{
    return m_output;
}

QMap<uint32_t, QVector<uint64_t>> DrmPipeline::formats() const
{
    return m_pending.formats;
}

QMap<uint32_t, QVector<uint64_t>> DrmPipeline::cursorFormats() const
{
    if (m_pending.crtc && m_pending.crtc->cursorPlane()) {
        return m_pending.crtc->cursorPlane()->formats();
    } else {
        return legacyCursorFormats;
    }
}

bool DrmPipeline::pruneModifier()
{
    if (!m_pending.layer->currentBuffer()
        || m_pending.layer->currentBuffer()->buffer()->modifier() == DRM_FORMAT_MOD_NONE
        || m_pending.layer->currentBuffer()->buffer()->modifier() == DRM_FORMAT_MOD_INVALID) {
        return false;
    }
    auto &modifiers = m_pending.formats[m_pending.layer->currentBuffer()->buffer()->format()];
    if (modifiers.empty()) {
        return false;
    } else {
        modifiers.clear();
        return true;
    }
}

bool DrmPipeline::needsModeset() const
{
    return m_pending.needsModeset;
}

bool DrmPipeline::needUpdateBrightness()
{
    if (!m_connector->hasBrightness()) {
        return false;
    }
    if (m_pending.brightness <= 0) {
        return false;
    }
    return m_pending.brightness != m_current.brightness;
}

bool DrmPipeline::needUpdateCTM()
{
    if (!m_pending.ctm) {
        return false;
    }
    if (m_current.ctm) {
        return m_pending.ctm->ctmValue() != m_current.ctm->ctmValue();
    } else {
        return m_pending.ctm->ctmValue();
    }
}

bool DrmPipeline::needUpdateColorMode()
{
    if (!m_pending.colorMode) {
        return false;
    }
    if (m_current.colorMode) {
        return m_pending.colorMode->colorModeValue() != m_current.colorMode->colorModeValue();
    } else {
        return m_pending.colorMode->colorModeValue() != Output::ColorMode::Native;
    }
}

bool DrmPipeline::activePending() const
{
    return m_pending.crtc && m_pending.mode && m_pending.active;
}

void DrmPipeline::revertPendingChanges()
{
    m_pending = m_next;
}

bool DrmPipeline::pageflipPending() const
{
    return m_pageflipPending;
}

bool DrmPipeline::modesetPresentPending() const
{
    return m_modesetPresentPending;
}

void DrmPipeline::resetModesetPresentPending()
{
    m_modesetPresentPending = false;
}

DrmCrtc *DrmPipeline::currentCrtc() const
{
    return m_current.crtc;
}

void DrmPipeline::printFlags(uint32_t flags)
{
    if (flags == 0) {
        qCDebug(KWIN_DRM) << "Flags: none";
    } else {
        qCDebug(KWIN_DRM) << "Flags:";
        if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
            qCDebug(KWIN_DRM) << "\t DRM_MODE_PAGE_FLIP_EVENT";
        }
        if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET) {
            qCDebug(KWIN_DRM) << "\t DRM_MODE_ATOMIC_ALLOW_MODESET";
        }
        if (flags & DRM_MODE_PAGE_FLIP_ASYNC) {
            qCDebug(KWIN_DRM) << "\t DRM_MODE_PAGE_FLIP_ASYNC";
        }
    }
}

void DrmPipeline::printDebugInfo() const
{
    qCDebug(KWIN_DRM) << "Drm objects:";
    m_connector->printProps(DrmObject::PrintMode::All);
    if (m_pending.crtc) {
        m_pending.crtc->printProps(DrmObject::PrintMode::All);
        if (m_pending.crtc->primaryPlane()) {
            m_pending.crtc->primaryPlane()->printProps(DrmObject::PrintMode::All);
        }
        if (m_pending.crtc->cursorPlane()) {
            m_pending.crtc->cursorPlane()->printProps(DrmObject::PrintMode::All);
        }
    }
}

DrmCrtc *DrmPipeline::crtc() const
{
    return m_pending.crtc;
}

std::shared_ptr<DrmConnectorMode> DrmPipeline::mode() const
{
    return m_pending.mode;
}

bool DrmPipeline::active() const
{
    return m_pending.active;
}

bool DrmPipeline::enabled() const
{
    return m_pending.enabled;
}

DrmPipelineLayer *DrmPipeline::primaryLayer() const
{
    return m_pending.layer.get();
}

DrmOverlayLayer *DrmPipeline::cursorLayer() const
{
    return m_pending.cursorLayer.get();
}

DrmPlane::Transformations DrmPipeline::renderOrientation() const
{
    return m_pending.renderOrientation;
}

DrmPlane::Transformations DrmPipeline::bufferOrientation() const
{
    return m_pending.bufferOrientation;
}

RenderLoopPrivate::SyncMode DrmPipeline::syncMode() const
{
    return m_pending.syncMode;
}

uint32_t DrmPipeline::overscan() const
{
    return m_pending.overscan;
}

Output::RgbRange DrmPipeline::rgbRange() const
{
    return m_pending.rgbRange;
}

DrmConnector::DrmContentType DrmPipeline::contentType() const
{
    return m_pending.contentType;
}

void DrmPipeline::setCrtc(DrmCrtc *crtc)
{
    m_pending.crtc = crtc;
    if (crtc) {
        m_pending.formats = crtc->primaryPlane() ? crtc->primaryPlane()->formats() : legacyFormats;
    } else {
        m_pending.formats = {};
    }
}

void DrmPipeline::setMode(const std::shared_ptr<DrmConnectorMode> &mode)
{
    m_pending.mode = mode;
}

void DrmPipeline::setActive(bool active)
{
    m_pending.active = active;
}

void DrmPipeline::setEnable(bool enable)
{
    m_pending.enabled = enable;
}

void DrmPipeline::setLayers(const std::shared_ptr<DrmPipelineLayer> &primaryLayer, const std::shared_ptr<DrmOverlayLayer> &cursorLayer)
{
    m_pending.layer = primaryLayer;
    m_pending.cursorLayer = cursorLayer;
}

void DrmPipeline::setRenderOrientation(DrmPlane::Transformations orientation)
{
    m_pending.renderOrientation = orientation;
}

void DrmPipeline::setBufferOrientation(DrmPlane::Transformations orientation)
{
    m_pending.bufferOrientation = orientation;
}

void DrmPipeline::setSyncMode(RenderLoopPrivate::SyncMode mode)
{
    m_pending.syncMode = mode;
}

void DrmPipeline::setOverscan(uint32_t overscan)
{
    m_pending.overscan = overscan;
}

void DrmPipeline::setRgbRange(Output::RgbRange range)
{
    m_pending.rgbRange = range;
}

void DrmPipeline::setColorTransformation(const std::shared_ptr<ColorTransformation> &transformation)
{
    if (m_pending.crtc && m_pending.crtc->gammaRampSize() > 0) {
        m_pending.gamma = std::make_shared<DrmGammaRamp>(m_pending.crtc, transformation);
    }
}

void DrmPipeline::setContentType(DrmConnector::DrmContentType type)
{
    m_pending.contentType = type;
}

void DrmPipeline::setBrightness(int32_t brightness)
{
    m_pending.brightness = brightness;
}

int32_t DrmPipeline::brightness() const
{
    return m_pending.brightness;
}

void DrmPipeline::setCTM(const Output::CtmValue &ctmValue)
{
    if (m_pending.crtc && m_pending.crtc->hasCTM()) {
        m_pending.ctm = std::make_shared<DrmCTM>(m_pending.crtc, ctmValue);
    }
}

Output::CtmValue DrmPipeline::ctmValue() const
{
    return m_pending.ctm ? m_pending.ctm->ctmValue() : Output::CtmValue{};
}

void DrmPipeline::setColorCurves(const Output::ColorCurves &colorCurves)
{
    if (m_pending.crtc && m_pending.crtc->gammaRampSize() > 0) {
        m_pending.gamma = std::make_shared<DrmGammaRamp>(m_pending.crtc, colorCurves);
    }
}

Output::ColorCurves DrmPipeline::colorCurves() const
{
    return m_pending.gamma ? m_pending.gamma->lut().colorCurves() : Output::ColorCurves{{0}, {0}, {0}};
}

void DrmPipeline::setColorMode(const Output::ColorMode &colorModeValue)
{
    if (m_pending.crtc && m_pending.crtc->hasColorMode()) {
        m_pending.colorMode = std::make_shared<DrmColorMode>(m_pending.crtc, colorModeValue);
    }
}

Output::ColorMode DrmPipeline::colorMode() const
{
    return m_pending.colorMode ? m_pending.colorMode->colorModeValue() : Output::ColorMode::Native;
}
}
