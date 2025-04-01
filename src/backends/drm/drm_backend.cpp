/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_backend.h"

#include <config-kwin.h>

#include "backends/libinput/libinputbackend.h"
#include "core/outputconfiguration.h"
#include "core/renderloop.h"
#include "core/session.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_egl_backend.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_plane.h"
#include "drm_qpainter_backend.h"
#include "drm_render_backend.h"
#include "drm_virtual_output.h"
#include "gbm_dmabuf.h"
#include "utils/udev.h"
// KF6
#include <KCoreAddons>
#include <KLocalizedString>
// Qt
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QStringBuilder>
// system
#include <algorithm>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
// drm
#include <gbm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>

namespace KWin
{

static QStringList splitPathList(const QString &input, const QChar delimiter)
{
    QStringList ret;
    QString tmp;
    for (int i = 0; i < input.size(); i++) {
        if (input[i] == delimiter) {
            if (i > 0 && input[i - 1] == '\\') {
                tmp[tmp.size() - 1] = delimiter;
            } else if (!tmp.isEmpty()) {
                ret.append(tmp);
                tmp = QString();
            }
        } else {
            tmp.append(input[i]);
        }
    }
    if (!tmp.isEmpty()) {
        ret.append(tmp);
    }
    return ret;
}

DrmBackend::DrmBackend(Session *session, QObject *parent)
    : OutputBackend(parent)
    , m_updateOutputTimer(new QTimer(this))
    , m_udev(std::make_unique<Udev>())
    , m_udevMonitor(m_udev->monitor())
    , m_session(session)
    , m_explicitGpus(splitPathList(qEnvironmentVariable("KWIN_DRM_DEVICES"), ':'))
    , m_dpmsFilter()
{
    connect(m_updateOutputTimer, &QTimer::timeout, this, [this]{
            qCDebug(KWIN_DRM) << "Timeout and updateOutputs";
            updateOutputs();
            });
    m_updateOutputTimer->setSingleShot(true);
    m_updateOutputTimer->setInterval(2000);
}

DrmBackend::~DrmBackend() = default;

Session *DrmBackend::session() const
{
    return m_session;
}

Outputs DrmBackend::outputs() const
{
    return m_outputs;
}

void DrmBackend::createDpmsFilter()
{
    if (m_dpmsFilter) {
        // already another output is off
        return;
    }
    m_dpmsFilter = std::make_unique<DpmsInputEventFilter>();
    //input()->prependInputEventFilter(m_dpmsFilter.get());
}

void DrmBackend::turnOutputsOn()
{
    m_dpmsFilter.reset();
    for (Output *output : std::as_const(m_outputs)) {
        if (output->isEnabled()) {
            output->setDpmsMode(Output::DpmsMode::On);
        }
    }
}

void DrmBackend::checkOutputsAreOn()
{
    if (!m_dpmsFilter) {
        // already disabled, all outputs are on
        return;
    }
    for (Output *output : std::as_const(m_outputs)) {
        if (output->isEnabled() && output->dpmsMode() != Output::DpmsMode::On) {
            // dpms still disabled, need to keep the filter
            return;
        }
    }
    // all outputs are on, disable the filter
    m_dpmsFilter.reset();
}

bool DrmBackend::initialize()
{
    connect(m_session, &Session::devicePaused, this, [this](dev_t deviceId) {
        if (auto gpu = findGpu(deviceId)) {
            gpu->setActive(false);
        }
    });
    connect(m_session, &Session::deviceResumed, this, [this](dev_t deviceId) {
        if (auto gpu = findGpu(deviceId)) {
            gpu->setActive(true);
        }
    });
    connect(m_session, &Session::awoke, this, &DrmBackend::turnOutputsOn);

    if (!m_explicitGpus.isEmpty()) {
        for (const QString &fileName : m_explicitGpus) {
            addGpu(fileName);
        }
    } else {
        const auto devices = m_udev->listGPUs();
        for (const UdevDevice::Ptr &device : devices) {
            if (device->seat() == m_session->seat()) {
                addGpu(device->devNode());
            }
        }
    }

    if (m_gpus.empty()) {
        qCWarning(KWIN_DRM) << "No suitable DRM devices have been found";
        return false;
    }

    // setup udevMonitor
    if (m_udevMonitor) {
        m_udevMonitor->filterSubsystemDevType("drm");
        const int fd = m_udevMonitor->fd();
        if (fd != -1) {
            m_socketNotifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
            connect(m_socketNotifier.get(), &QSocketNotifier::activated, this, &DrmBackend::handleUdevEvent);
            m_udevMonitor->enable();
        }
    }
    return true;
}

void DrmBackend::handleUdevEvent()
{
    while (auto device = m_udevMonitor->getDevice()) {

        // Ignore the device seat if the KWIN_DRM_DEVICES envvar is set.
        if (!m_explicitGpus.isEmpty()) {
            if (!m_explicitGpus.contains(device->devNode())) {
                continue;
            }
        } else {
            if (device->seat() != m_session->seat()) {
                continue;
            }
        }

        if (device->action() == QStringLiteral("add")) {
            qCDebug(KWIN_DRM) << "Received add event for monitored drm device: " << device->devNode();
            if (addGpu(device->devNode())) {
                updateOutputs();
            }
        } else if (device->action() == QStringLiteral("remove")) {
            qCDebug(KWIN_DRM) << "Received remove event for monitored drm device: " << device->devNode();
            DrmGpu *gpu = findGpu(device->devNum());
            if (gpu) {
                if (primaryGpu() == gpu) {
                    qCCritical(KWIN_DRM) << "Primary gpu has been removed! Quitting...";
                    QCoreApplication::exit(1);
                    return;
                } else {
                    gpu->setRemoved();
                    updateOutputs();
                }
            }
        } else if (device->action() == QStringLiteral("change")) {
            DrmGpu *gpu = findGpu(device->devNum());
            if (!gpu) {
                gpu = addGpu(device->devNode());
            }
            if (gpu && gpu->isActive()) {
                qCDebug(KWIN_DRM) << "Received change event for monitored drm device:" << device->devNode();
                m_updateOutputTimer->start();
            }
        }
    }
}

DrmGpu *DrmBackend::addGpu(const QString &fileName)
{
    int fd = m_session->openRestricted(fileName);
    if (fd < 0) {
        qCWarning(KWIN_DRM) << "failed to open drm device at" << fileName;
        return nullptr;
    }

    // try to make a simple drm get resource call, if it fails it is not useful for us
    drmModeRes *resources = drmModeGetResources(fd);
    if (!resources) {
        qCDebug(KWIN_DRM) << "Skipping KMS incapable drm device node at" << fileName;
        m_session->closeRestricted(fd);
        return nullptr;
    }
    drmModeFreeResources(resources);

    struct stat buf;
    if (fstat(fd, &buf) == -1) {
        qCDebug(KWIN_DRM, "Failed to fstat %s: %s", qPrintable(fileName), strerror(errno));
        m_session->closeRestricted(fd);
        return nullptr;
    }

    qCDebug(KWIN_DRM, "adding GPU %s", qPrintable(fileName));
    m_gpus.push_back(std::make_unique<DrmGpu>(this, fileName, fd, buf.st_rdev));
    auto gpu = m_gpus.back().get();
    connect(gpu, &DrmGpu::outputAdded, this, &DrmBackend::addOutput);
    connect(gpu, &DrmGpu::outputRemoved, this, &DrmBackend::removeOutput);
    Q_EMIT gpuAdded(gpu);
    return gpu;
}

void DrmBackend::addOutput(DrmAbstractOutput *o)
{
    if (m_disableMultiScreens && !m_outputs.isEmpty()) {
        qCDebug(KWIN_DRM, "skip for disableMultiScreens, already has pending Connector");
        return;
    }
    m_outputs.append(o);
    Q_EMIT outputAdded(o);
    o->updateEnabled(true);
}

void DrmBackend::removeOutput(DrmAbstractOutput *o)
{
    o->updateEnabled(false);
    m_outputs.removeOne(o);
    Q_EMIT outputRemoved(o);
}

void DrmBackend::updateOutputs()
{
    for (auto it = m_gpus.begin(); it != m_gpus.end(); ++it) {
        if ((*it)->isRemoved()) {
            (*it)->removeOutputs();
        } else {
            (*it)->updateOutputs();
        }
    }

    Q_EMIT outputsQueried();

    for (auto it = m_gpus.begin(); it != m_gpus.end();) {
        DrmGpu *gpu = it->get();
        if (gpu->isRemoved() || (gpu != primaryGpu() && gpu->drmOutputs().isEmpty())) {
            qCDebug(KWIN_DRM) << "Removing GPU" << (*it)->devNode();
            const std::unique_ptr<DrmGpu> keepAlive = std::move(*it);
            it = m_gpus.erase(it);
            Q_EMIT gpuRemoved(keepAlive.get());
        } else {
            it++;
        }
    }
}

std::unique_ptr<InputBackend> DrmBackend::createInputBackend()
{
    return std::make_unique<LibinputBackend>(m_session);
}

std::unique_ptr<QPainterBackend> DrmBackend::createQPainterBackend()
{
    return std::make_unique<DrmQPainterBackend>(this);
}

std::unique_ptr<OpenGLBackend> DrmBackend::createOpenGLBackend()
{
    return std::make_unique<EglGbmBackend>(this);
}

void DrmBackend::sceneInitialized()
{
    if (m_outputs.isEmpty()) {
        updateOutputs();
    } else {
        for (const auto &gpu : std::as_const(m_gpus)) {
            gpu->recreateSurfaces();
        }
    }
}

QVector<CompositingType> DrmBackend::supportedCompositors() const
{
    return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
}

QString DrmBackend::supportInformation() const
{
    QString supportInfo;
    QDebug s(&supportInfo);
    s.nospace();
    s << "Name: "
      << "DRM\n";
    for (size_t g = 0; g < m_gpus.size(); g++) {
        s << "Atomic Mode Setting on GPU " << g << ": " << m_gpus.at(g)->atomicModeSetting() << "\n";
    }
    return supportInfo;
}

Output *DrmBackend::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    auto output = primaryGpu()->createVirtualOutput(name, size * scale, scale);
    Q_EMIT outputsQueried();
    return output;
}

void DrmBackend::removeVirtualOutput(Output *output)
{
    auto virtualOutput = static_cast<DrmVirtualOutput *>(output);
    if (!virtualOutput) {
        return;
    }
    primaryGpu()->removeVirtualOutput(virtualOutput);
    Q_EMIT outputsQueried();
}

gbm_bo *DrmBackend::createBo(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers)
{
    const auto eglBackend = dynamic_cast<EglGbmBackend *>(m_renderBackend);
    if (!eglBackend || !primaryGpu()->gbmDevice()) {
        return nullptr;
    }

    return createGbmBo(primaryGpu()->gbmDevice(), size, format, modifiers);
}

std::optional<DmaBufParams> DrmBackend::testCreateDmaBuf(const QSize &size, quint32 format, const QVector<uint64_t> &modifiers)
{
    gbm_bo *bo = createBo(size, format, modifiers);
    if (!bo) {
        return {};
    }

    auto ret = dmaBufParamsForBo(bo);
    gbm_bo_destroy(bo);
    return ret;
}

std::shared_ptr<DmaBufTexture> DrmBackend::createDmaBufTexture(const QSize &size, quint32 format, uint64_t modifier)
{
    QVector<uint64_t> mods = {modifier};
    gbm_bo *bo = createBo(size, format, mods);
    if (!bo) {
        return {};
    }

    // The bo will be kept around until the last fd is closed.
    DmaBufAttributes attributes = dmaBufAttributesForBo(bo);
    gbm_bo_destroy(bo);
    const auto eglBackend = static_cast<EglGbmBackend *>(m_renderBackend);
    eglBackend->makeCurrent();
    if (auto texture = eglBackend->importDmaBufAsTexture(attributes)) {
        return std::make_shared<DmaBufTexture>(texture, std::move(attributes));
    } else {
        return nullptr;
    }
}

DrmGpu *DrmBackend::primaryGpu() const
{
    return m_gpus.empty() ? nullptr : m_gpus.front().get();
}

DrmGpu *DrmBackend::findGpu(dev_t deviceId) const
{
    auto it = std::find_if(m_gpus.begin(), m_gpus.end(), [deviceId](const auto &gpu) {
        return gpu->deviceId() == deviceId;
    });
    return it == m_gpus.end() ? nullptr : it->get();
}

size_t DrmBackend::gpuCount() const
{
    return m_gpus.size();
}

bool DrmBackend::applyOutputChanges(const OutputConfiguration &config)
{
    QVector<DrmOutput *> toBeEnabled;
    QVector<DrmOutput *> toBeDisabled;
    for (const auto &gpu : std::as_const(m_gpus)) {
        const auto &outputs = gpu->drmOutputs();
        for (const auto &output : outputs) {
            if (output->isNonDesktop()) {
                continue;
            }
            output->queueChanges(config);
            if (config.constChangeSet(output)->enabled) {
                toBeEnabled << output;
            } else {
                toBeDisabled << output;
            }
        }
        if (gpu->testPendingConfiguration() != DrmPipeline::Error::None) {
            for (const auto &output : std::as_const(toBeEnabled)) {
                output->revertQueuedChanges();
            }
            for (const auto &output : std::as_const(toBeDisabled)) {
                output->revertQueuedChanges();
            }
            return false;
        }
    }
    // first, apply changes to drm outputs.
    // This may remove the placeholder output and thus change m_outputs!
    for (const auto &output : std::as_const(toBeEnabled)) {
        output->applyQueuedChanges(config);
    }
    for (const auto &output : std::as_const(toBeDisabled)) {
        output->applyQueuedChanges(config);
    }
    // only then apply changes to the virtual outputs
    for (const auto &gpu : std::as_const(m_gpus)) {
        const auto &outputs = gpu->virtualOutputs();
        for (const auto &output : outputs) {
            output->applyChanges(config);
        }
    }
    return true;
}

void DrmBackend::setRenderBackend(DrmRenderBackend *backend)
{
    m_renderBackend = backend;
}

DrmRenderBackend *DrmBackend::renderBackend() const
{
    return m_renderBackend;
}

void DrmBackend::releaseBuffers()
{
    for (const auto &gpu : std::as_const(m_gpus)) {
        gpu->releaseBuffers();
    }
}

const std::vector<std::unique_ptr<DrmGpu>> &DrmBackend::gpus() const
{
    return m_gpus;
}

void DrmBackend::disableMultiScreens()
{
    m_disableMultiScreens = true;
}
}
