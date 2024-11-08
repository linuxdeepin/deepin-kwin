/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_crtc.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pointer.h"
#include <cerrno>

namespace KWin
{

DrmCrtc::DrmCrtc(DrmGpu *gpu, uint32_t crtcId, int pipeIndex, DrmPlane *primaryPlane, DrmPlane *cursorPlane)
    : DrmObject(gpu, crtcId, {PropertyDefinition(QByteArrayLiteral("MODE_ID"), Requirement::Required), PropertyDefinition(QByteArrayLiteral("ACTIVE"), Requirement::Required), PropertyDefinition(QByteArrayLiteral("VRR_ENABLED"), Requirement::Optional), PropertyDefinition(QByteArrayLiteral("GAMMA_LUT"), Requirement::Optional), PropertyDefinition(QByteArrayLiteral("GAMMA_LUT_SIZE"), Requirement::Optional), PropertyDefinition(QByteArrayLiteral("CTM"), Requirement::Optional)}, DRM_MODE_OBJECT_CRTC)
    , m_crtc(drmModeGetCrtc(gpu->fd(), crtcId))
    , m_pipeIndex(pipeIndex)
    , m_primaryPlane(primaryPlane)
    , m_cursorPlane(cursorPlane)
{
    if (gpu->isHisi()) {
        m_ctmEnabled = (qEnvironmentVariableIntValue("KWIN_CTM_DISABLED") == 1) ? false : true;
    } else {
        //非华为机器默认关闭，遗留后续解决。初步推断是驱动问题导致
        m_ctmEnabled = qEnvironmentVariableIntValue("KWIN_CTM_ENABLED") == 1;
    }
}

bool DrmCrtc::init()
{
    if (!m_crtc) {
        return false;
    }
    return initProps();
}

void DrmCrtc::flipBuffer()
{
    m_currentBuffer = m_nextBuffer;
    m_nextBuffer = nullptr;
}

drmModeModeInfo DrmCrtc::queryCurrentMode()
{
    m_crtc.reset(drmModeGetCrtc(gpu()->fd(), id()));
    return m_crtc->mode;
}

int DrmCrtc::pipeIndex() const
{
    return m_pipeIndex;
}

std::shared_ptr<DrmFramebuffer> DrmCrtc::current() const
{
    return m_currentBuffer;
}

std::shared_ptr<DrmFramebuffer> DrmCrtc::next() const
{
    return m_nextBuffer;
}

void DrmCrtc::setCurrent(const std::shared_ptr<DrmFramebuffer> &buffer)
{
    m_currentBuffer = buffer;
}

void DrmCrtc::setNext(const std::shared_ptr<DrmFramebuffer> &buffer)
{
    m_nextBuffer = buffer;
}

int DrmCrtc::gammaRampSize() const
{
    if (gpu()->atomicModeSetting()) {
        // limit atomic gamma ramp to 4096 to work around https://gitlab.freedesktop.org/drm/intel/-/issues/3916
        if (auto prop = getProp(PropertyIndex::Gamma_LUT_Size); prop && prop->current() <= 4096) {
            return prop->current();
        }
    }
    return m_crtc->gamma_size;
}

DrmPlane *DrmCrtc::primaryPlane() const
{
    return m_primaryPlane;
}

DrmPlane *DrmCrtc::cursorPlane() const
{
    return m_cursorPlane;
}

void DrmCrtc::disable()
{
    setPending(PropertyIndex::Active, 0);
    setPending(PropertyIndex::ModeId, 0);
}

void DrmCrtc::releaseBuffers()
{
    if (m_nextBuffer) {
        m_nextBuffer->releaseBuffer();
    }
    if (m_currentBuffer) {
        m_currentBuffer->releaseBuffer();
    }
}

bool DrmCrtc::hasCTM() const
{
    if (!m_ctmEnabled) {
        return false;
    }
    return getProp(PropertyIndex::CTM);
}

bool DrmCrtc::hasColorMode() const
{
    // FlemingX机器通过GAMMA_LUT传递色彩模式标签，华为drm内核驱动规定GAMMA_LUT_SIZE == 1
    return gammaRampSize() == 1 && getProp(PropertyIndex::Gamma_LUT);
}

DrmGammaRamp::DrmGammaRamp(DrmCrtc *crtc, const std::shared_ptr<ColorTransformation> &transformation)
    : DrmBlob<DrmCrtc, DrmCrtc::PropertyIndex::Gamma_LUT>(crtc)
    , m_lut(transformation, crtc->gammaRampSize())
{
    init(crtc);
}

DrmGammaRamp::DrmGammaRamp(DrmCrtc *crtc, const Output::ColorCurves &colorCurves)
    : DrmBlob<DrmCrtc, DrmCrtc::PropertyIndex::Gamma_LUT>(crtc)
    , m_lut(colorCurves, crtc->gammaRampSize())
{
    init(crtc);
}

void DrmGammaRamp::init(DrmCrtc *crtc)
{
    if (crtc->gpu()->atomicModeSetting()) {
        QVector<struct drm_color_lut> atomicLut(m_lut.size());
        for (uint32_t i = 0; i < m_lut.size(); i++) {
            atomicLut[i].red = m_lut.red()[i];
            atomicLut[i].green = m_lut.green()[i];
            atomicLut[i].blue = m_lut.blue()[i];
        }
        if (!(m_blob = DrmBlobFactory::create(crtc->gpu(), atomicLut.data(), sizeof(drm_color_lut) * atomicLut.size()))) {
            qCWarning(KWIN_DRM) << "Failed to create gamma blob!" << strerror(errno);
        }
    }
}

const ColorLUT &DrmGammaRamp::lut() const
{
    return m_lut;
}

DrmCTM::DrmCTM(DrmCrtc *crtc, const Output::CtmValue &ctmValue)
    : DrmBlob<DrmCrtc, DrmCrtc::PropertyIndex::CTM>(crtc)
    , m_ctmValue(ctmValue)
{
    if (crtc->gpu()->atomicModeSetting()) {
        struct drm_color_ctm blob = {.matrix = {
                                        ctmValue.r, 0, 0,
                                        0, ctmValue.g, 0,
                                        0, 0, ctmValue.b}};
        if (!(m_blob = DrmBlobFactory::create(crtc->gpu(), &blob, sizeof(drm_color_ctm)))) {
            qCWarning(KWIN_DRM) << "Failed to create ctm blob!" << strerror(errno);
        }
    }
}

const Output::CtmValue &DrmCTM::ctmValue() const
{
    return m_ctmValue;
}

DrmColorMode::DrmColorMode(DrmCrtc *crtc, const Output::ColorMode &colorMode)
    : DrmBlob<DrmCrtc, DrmCrtc::PropertyIndex::Gamma_LUT>(crtc)
    , m_colorModeValue(colorMode)
{
    if (crtc->gpu()->atomicModeSetting()) {
        struct drm_color_lut blob = {
            .red = 0,
            .green = 0,
            .blue = 0,
            .reserved = static_cast<uint16_t>(colorMode)};
        if (!(m_blob = DrmBlobFactory::create(crtc->gpu(), &blob, sizeof(drm_color_lut)))) {
            qCWarning(KWIN_DRM) << "Failed to create colormode blob (drm_color_lut[1])!" << strerror(errno);
        }
    }
}

const Output::ColorMode &DrmColorMode::colorModeValue() const
{
    return m_colorModeValue;
}

}
