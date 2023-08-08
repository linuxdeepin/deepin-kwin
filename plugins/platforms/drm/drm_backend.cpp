// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#define LOG_TAG "DrmBackend"

#include "drm_backend.h"
#include "drm_output.h"
#include "drm_object_connector.h"
#include "drm_object_crtc.h"
#include "drm_object_plane.h"
#include "composite.h"
#include "cursor.h"
#include "logging.h"
#include "logind.h"
#include "main.h"
#include "scene_qpainter_drm_backend.h"
#include "screens_drm.h"
#include "udev.h"
#include "wayland_server.h"
#if HAVE_GBM
#include "egl_gbm_backend.h"
#include <gbm.h>
#include "gbm_dmabuf.h"
#endif
#if HAVE_EGL_STREAMS
#include "egl_stream_backend.h"
#endif
// KWayland
#include <KWayland/Server/seat_interface.h>
// KF5
#include <KConfigGroup>
#include <KCoreAddons>
#include <KLocalizedString>
#include <KSharedConfig>
// Qt
#include <QCryptographicHash>
#include <QSocketNotifier>
#include <QPainter>
// system
#include <algorithm>
#include <unistd.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>

#include "drm_gpu.h"

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

#define KWIN_DRM_EVENT_CONTEXT_VERSION 2

namespace KWin
{

DrmBackend::DrmBackend(QObject *parent)
    : Platform(parent)
    , m_udev(new Udev)
    , m_udevMonitor(m_udev->monitor())
    , m_dpmsFilter()
{
#if HAVE_EGL_STREAMS
    if (qEnvironmentVariableIsSet("KWIN_DRM_USE_EGL_STREAMS")) {
        m_useEglStreams = true;
    }
#endif
    setSupportsGammaControl(true);
    handleOutputs();
}

DrmBackend::~DrmBackend()
{
    if (m_gpus.size() > 0) {
        // wait for pageflips
        while (m_pageFlipsPending != 0) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
        }
        qDeleteAll(m_gpus);
    }
}

void DrmBackend::init()
{
    LogindIntegration *logind = LogindIntegration::self();
    auto takeControl = [logind, this]() {
        if (logind->hasSessionControl()) {
            openDrm();
        } else {
            logind->takeControl();
            connect(logind, &LogindIntegration::hasSessionControlChanged, this, &DrmBackend::openDrm);
        }
    };
    if (logind->isConnected()) {
        takeControl();
    } else {
        connect(logind, &LogindIntegration::connectedChanged, this, takeControl);
    }
}

Outputs DrmBackend::outputs() const
{
    return m_outputs;
}

Outputs DrmBackend::enabledOutputs() const
{
    return m_enabledOutputs;
}

void DrmBackend::outputWentOff()
{
    if (!m_dpmsFilter.isNull()) {
        // already another output is off
        return;
    }
    m_dpmsFilter.reset(new DpmsInputEventFilter(this));
    //input()->prependInputEventFilter(m_dpmsFilter.data());
}

void DrmBackend::turnOutputsOn()
{
    m_dpmsFilter.reset();
    for (auto it = m_enabledOutputs.constBegin(), end = m_enabledOutputs.constEnd(); it != end; it++) {
        (*it)->updateDpms(KWayland::Server::OutputInterface::DpmsMode::On);
    }
}

void DrmBackend::checkOutputsAreOn()
{
    if (m_dpmsFilter.isNull()) {
        // already disabled, all outputs are on
        return;
    }
    for (auto it = m_enabledOutputs.constBegin(), end = m_enabledOutputs.constEnd(); it != end; it++) {
        if (!(*it)->isDpmsEnabled()) {
            // dpms still disabled, need to keep the filter
            return;
        }
    }
    // all outputs are on, disable the filter
    m_dpmsFilter.reset();
}

void DrmBackend::activate(bool active)
{
    if (active) {
        qCDebug(KWIN_DRM) << "Activating session.";
        reactivate();
    } else {
        qCDebug(KWIN_DRM) << "Deactivating session.";
        deactivate();
    }
}

void DrmBackend::reactivate()
{
    if (m_active) {
        return;
    }
    m_active = true;
    if (!usesSoftwareCursor()) {
        const QPoint cp = Cursor::pos() - softwareCursorHotspot();
        for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
            DrmOutput *o = *it;
            // only relevant in atomic mode
            o->m_modesetRequested = true;
            o->pageFlipped();   // TODO: Do we really need this?
            o->m_crtc->blank();
            if (m_enabledOutputs.contains(o)) {
                if (o->isDpmsEnabled()) {
                    o->showCursor();
                    o->moveCursor(cp);
                }
            }
        }
    }
    // restart compositor
    m_pageFlipsPending = 0;
    if (Compositor *compositor = Compositor::self()) {
        compositor->bufferSwapComplete();
        compositor->addRepaintFull();
    }
}

void DrmBackend::deactivate()
{
    if (!m_active) {
        return;
    }
    // block compositor
    if (m_pageFlipsPending == 0 && Compositor::self()) {
        Compositor::self()->aboutToSwapBuffers();
    }
    // hide cursor and disable
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        DrmOutput *o = *it;
        o->hideCursor();
    }
    m_active = false;
}

void DrmBackend::pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data)
{
    Q_UNUSED(fd)
    Q_UNUSED(frame)
    Q_UNUSED(sec)
    Q_UNUSED(usec)
    auto output = reinterpret_cast<DrmOutput*>(data);
    output->pageFlipped();
    output->m_backend->m_pageFlipsPending--;
    if (output->m_backend->m_pageFlipsPending == 0) {
        // TODO: improve, this currently means we wait for all page flips or all outputs.
        // It would be better to driver the repaint per output

        if (output->m_dpmsAtomicOffPending) {
            output->m_modesetRequested = true;
            output->dpmsAtomicOff();
        }

        if (Compositor::self()) {
            Compositor::self()->bufferSwapComplete();
        }
    }
}

DrmGpu *DrmBackend::addGpu(UdevDevice *device)
{
    auto devNode = QByteArray(device->devNode());
    int fd = LogindIntegration::self()->takeDevice(devNode.constData());
    if (fd < 0) {
        qCWarning(KWIN_DRM) << "failed to open drm device at" << devNode;
        return nullptr;
    } else {
        qCInfo(KWIN_DRM) << "finished to open drm device at" << devNode;
    }
    m_active = true;
    QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [fd] {
        if (!LogindIntegration::self()->isActiveSession()) {
            return;
        }
        drmEventContext e;
        memset(&e, 0, sizeof e);
        e.version = KWIN_DRM_EVENT_CONTEXT_VERSION;
        e.page_flip_handler = pageFlipHandler;
        drmHandleEvent(fd, &e);
    });
    DrmGpu *gpu = new DrmGpu(this, devNode, fd, device->sysNum());
    connect(gpu, &DrmGpu::outputAdded, this, &DrmBackend::addOutput);
    connect(gpu, &DrmGpu::outputRemoved, this, &DrmBackend::removeOutput);
    m_gpus.append(gpu);

    if (!m_eglGbmBackend)
        createOpenGLBackend();
    Q_ASSERT(m_eglGbmBackend);
    m_eglGbmBackend->connectToGpu(gpu);

    return gpu;
}

void DrmBackend::openDrm()
{
    connect(LogindIntegration::self(), &LogindIntegration::sessionActiveChanged, this, &DrmBackend::activate);
    std::vector<UdevDevice::Ptr> devices = m_udev->listGPUs();
    if (devices.size() == 0) {
        qCWarning(KWIN_DRM) << "Did not find a GPU";
        return;
    } else {
        qCInfo(KWIN_DRM) << "Found" << devices.size() << "GPUs";
    }

    for (unsigned int gpu_index = 0; gpu_index < devices.size(); gpu_index++) {
        auto device = std::move(devices.at(gpu_index));
        addGpu(device.get());
    }
    
    // trying to activate Atomic Mode Setting (this means also Universal Planes)
    if (!qEnvironmentVariableIsSet("KWIN_DRM_NO_AMS")) {
        for (auto gpu : m_gpus)
            gpu->tryAMS();
    }

    initCursor();
    if (!updateOutputs())
        return;

    if (m_outputs.isEmpty()) {
        qCDebug(KWIN_DRM) << "No connected outputs found on startup.";
    }
    
    // setup udevMonitor
    if (m_udevMonitor) {
        m_udevMonitor->filterSubsystemDevType("drm");
        const int fd = m_udevMonitor->fd();
        if (fd != -1) {
            QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(notifier, &QSocketNotifier::activated, this, [this] {
                    auto device = m_udevMonitor->getDevice();
                    if (!device) {
                        return;
                    }
                    bool drm = false;
                    for (auto gpu : m_gpus) {
                        if (gpu->drmId() == device->sysNum()) {
                            drm = true;
                            break;
                        }
                    }
                    if (!drm && !QByteArray(device->devNode()).startsWith("/dev/dri/card")) {
                        return;
                    }

                    if (device->action() == "add") {
                        qCDebug(KWIN_DRM) << "Received hot plug GPU add event:" << device->devNode();
                        if (auto gpu = addGpu(device.get())) {
                            if (!qEnvironmentVariableIsSet("KWIN_DRM_NO_AMS")) {
                                gpu->tryAMS();
                            }
                            updateOutputs();
                            updateCursor();
                        }
                    } else if (device->action() == "remove") {
                        DrmGpu *gpu = findGpu(device->sysNum());
                        if (gpu) {
                           if (primaryGpu() == gpu) {
                               qCCritical(KWIN_DRM) << "Primary gpu has been removed! Quitting...";
                               QCoreApplication::exit(1);
                               return;
                           } else {
		              gpu->handleGPURemove();
                              m_gpus.removeAll(gpu);
                              updateOutputs();
                                updateCursor();
                           }
                        }
                    } else if (device->action() == "change") {
                        DrmGpu *gpu = findGpu(device->sysNum());
                        if (!gpu) {
                            gpu = addGpu(device.get());
                        }
                        if (gpu) {
                            qCDebug(KWIN_DRM) << "Received change event for monitored drm device" << gpu->devNode();
                            updateOutputs();
                            updateCursor();
                        }
                    }
                }
            );
            m_udevMonitor->enable();
        }
    }
    setReady(true);
}

void DrmBackend::addOutput(DrmOutput *o)
{
    m_outputs.append(o);
    m_enabledOutputs.append(o);
    emit o->gpu()->outputEnabled(o);
}

void DrmBackend::removeOutput(DrmOutput *o)
{
    emit o->gpu()->outputDisabled(o);
    m_outputs.removeOne(o);
    m_enabledOutputs.removeOne(o);
}

bool DrmBackend::updateOutputs()
{
    if (m_gpus.size() == 0) {
        return false;
    }
    const auto oldOutputs = m_outputs;
    for (auto gpu : m_gpus)
        gpu->updateOutputs();
    
    std::sort(m_outputs.begin(), m_outputs.end(), [&] (DrmOutput *a, DrmOutput *b) {
        const int aIndex = oldOutputs.indexOf(a);
        const int bIndex = oldOutputs.indexOf(b);
        if (aIndex == bIndex)
            return a->m_conn->id() > b->m_conn->id();
        return bIndex < 0 || (aIndex >= 0 && aIndex < bIndex);
    });

    readOutputsConfiguration();
    updateOutputsEnabled();
    if (!m_outputs.isEmpty()) {
        emit screensQueried();
    }
    return true;
}

void DrmBackend::readOutputsConfiguration()
{
    if (m_outputs.isEmpty()) {
        return;
    }
    const QByteArray uuid = generateOutputConfigurationUuid();
    const auto outputGroup = kwinApp()->config()->group("DrmOutputs");
    const auto configGroup = outputGroup.group(uuid);
    // default position goes from left to right
    QPoint pos(0, 0);
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        qCDebug(KWIN_DRM) << "Reading output configuration for [" << uuid << "] ["<< (*it)->uuid() << "]";
        const auto outputConfig = configGroup.group((*it)->uuid());
        (*it)->setGlobalPos(outputConfig.readEntry<QPoint>("Position", pos));
        // TODO: add mode
        if (outputConfig.hasKey("Scale"))
            (*it)->setScale(outputConfig.readEntry("Scale", 1.0));
        pos.setX(pos.x() + (*it)->geometry().width());
    }
}

void DrmBackend::writeOutputsConfiguration()
{
    if (m_outputs.isEmpty()) {
        return;
    }
    const QByteArray uuid = generateOutputConfigurationUuid();
    auto configGroup = KSharedConfig::openConfig()->group("DrmOutputs").group(uuid);
    // default position goes from left to right
    for (auto it = m_outputs.cbegin(); it != m_outputs.cend(); ++it) {
        qCDebug(KWIN_DRM) << "Writing output configuration for [" << uuid << "] ["<< (*it)->uuid() << "]";
        auto outputConfig = configGroup.group((*it)->uuid());
        outputConfig.writeEntry("Scale", (*it)->scale());
    }
}

QByteArray DrmBackend::generateOutputConfigurationUuid() const
{
    auto it = m_outputs.constBegin();
    if (m_outputs.size() == 1) {
        // special case: one output
        return (*it)->uuid();
    }
    QCryptographicHash hash(QCryptographicHash::Md5);
    for (; it != m_outputs.constEnd(); ++it) {
        hash.addData((*it)->uuid());
    }
    return hash.result().toHex().left(10);
}

void DrmBackend::enableOutput(DrmOutput *output, bool enable)
{
    if (enable) {
        Q_ASSERT(!m_enabledOutputs.contains(output));
        m_enabledOutputs << output;
        emit output->gpu()->outputEnabled(output);
    } else {
        Q_ASSERT(m_enabledOutputs.contains(output));
        m_enabledOutputs.removeOne(output);
        Q_ASSERT(!m_enabledOutputs.contains(output));
        emit output->gpu()->outputDisabled(output);
    }
    updateOutputsEnabled();
    checkOutputsAreOn();
    emit screensQueried();
}

DrmOutput *DrmBackend::findOutput(quint32 connector)
{
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(), [connector] (DrmOutput *o) {
        if (o->m_isVirtual) {
            return false;
        }

        return o->m_conn->id() == connector;
    });
    if (it != m_outputs.constEnd()) {
        return *it;
    }
    return nullptr;
}

bool DrmBackend::present(DrmBuffer *buffer, DrmOutput *output)
{
    if (!buffer || buffer->bufferId() == 0) {
        if (m_deleteBufferAfterPageFlip) {
            delete buffer;
        }
        return false;
    }

    if (output->present(buffer)) {
        m_pageFlipsPending++;
        if (m_pageFlipsPending == 1 && Compositor::self()) {
            Compositor::self()->aboutToSwapBuffers();
        }
        return true;
    } else if (m_deleteBufferAfterPageFlip) {
        delete buffer;
    }
    return false;
}

void DrmBackend::initCursor()
{

#if HAVE_EGL_STREAMS
    // Hardware cursors aren't currently supported with EGLStream backend,
    // possibly an NVIDIA driver bug
    if (m_useEglStreams) {
        setSoftWareCursor(true);
    }
#endif
    m_cursorEnabled = waylandServer()->seat()->hasPointer();
    qDebug() << "m_cursorEnabled" << m_cursorEnabled;
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::hasPointerChanged, this,
        [this] {
            m_cursorEnabled = waylandServer()->seat()->hasPointer();
            qDebug() << "hasPointerChanged m_cursorEnabled" << m_cursorEnabled;
            if (usesSoftwareCursor()) {
                return;
            }
            for (auto it = m_enabledOutputs.constBegin(); it != m_enabledOutputs.constEnd(); ++it) {
                if (m_cursorEnabled && (*it)->isDpmsEnabled()) {
                    if (!(*it)->showCursor()) {
                        setSoftWareCursor(true);
                    }
                } else {
                    (*it)->hideCursor();
                }
            }
        }
    );
    uint64_t capability = 0;
    QSize cursorSize;
    cursorSize.setWidth(64);
    for (auto gpu : m_gpus) {
        if (drmGetCap(gpu->fd(), DRM_CAP_CURSOR_WIDTH, &capability) == 0) {
            cursorSize.setWidth(capability);
        }
    }
    cursorSize.setHeight(64);
    for (auto gpu : m_gpus) {
        if (drmGetCap(gpu->fd(), DRM_CAP_CURSOR_HEIGHT, &capability) == 0) {
            cursorSize.setHeight(capability);
        }
    }
    m_cursorSize = cursorSize;
    // now we have screens and can set cursors, so start tracking
    connect(this, &DrmBackend::cursorChanged, this, &DrmBackend::updateCursor);
    connect(Cursor::self(), &Cursor::posChanged, this, &DrmBackend::moveCursor);
}

void DrmBackend::setCursor()
{
    if (m_cursorEnabled) {
        for (auto it = m_enabledOutputs.constBegin(); it != m_enabledOutputs.constEnd(); ++it) {
            if ((*it)->isDpmsEnabled()) {
                if (!(*it)->showCursor()) {
                    setSoftWareCursor(true);
                }
        }
        }
    }
    markCursorAsRendered();
}

void DrmBackend::updateCursor()
{
    if (usesSoftwareCursor()) {
        return;
    }
    if (isCursorHidden()) {
        return;
    }

    const QImage &cursorImage = softwareCursor();
    if (cursorImage.isNull()) {
        doHideCursor();
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->updateCursor();
    }

    setCursor();
    moveCursor();
}

void DrmBackend::doShowCursor()
{
    updateCursor();
}

void DrmBackend::doHideCursor()
{
    if (!m_cursorEnabled || usesSoftwareCursor()) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->hideCursor();
    }
}

void DrmBackend::moveCursor()
{
    if (!m_cursorEnabled || isCursorHidden() || usesSoftwareCursor()) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->moveCursor(Cursor::pos());
    }
}

Screens *DrmBackend::createScreens(QObject *parent)
{
    return new DrmScreens(this, parent);
}

QPainterBackend *DrmBackend::createQPainterBackend()
{
    m_deleteBufferAfterPageFlip = false;
    return new DrmQPainterBackend(this, primaryGpu());
}

OpenGLBackend *DrmBackend::createOpenGLBackend()
{
#if HAVE_EGL_STREAMS
    if (m_useEglStreams) {
        m_deleteBufferAfterPageFlip = false;
        return new EglStreamBackend(this, primaryGpu());
    }
#endif

#if HAVE_GBM
    if (!m_eglGbmBackend) {
        m_deleteBufferAfterPageFlip = true;
        m_eglGbmBackend = new EglGbmBackend(this, primaryGpu());
    }
    return m_eglGbmBackend;
#else
    return Platform::createOpenGLBackend();
#endif
}

OpenGLBackend *DrmBackend::getOpenGLBackend()
{
    return m_eglGbmBackend;
}

bool DrmBackend::requiresCompositing() const
{
    return true;
}

void DrmBackend::updateOutputsEnabled()
{
    bool enabled = false;
    for (auto it = m_enabledOutputs.constBegin(); it != m_enabledOutputs.constEnd(); ++it) {
        enabled = enabled || (*it)->isDpmsEnabled();
    }
    setOutputsEnabled(enabled);
}

QVector<CompositingType> DrmBackend::supportedCompositors() const
{
#if HAVE_GBM
    return QVector<CompositingType>{OpenGLCompositing, QPainterCompositing};
#elif HAVE_EGL_STREAMS
    return m_useEglStreams ?
        QVector<CompositingType>{OpenGLCompositing, QPainterCompositing} :
        QVector<CompositingType>{QPainterCompositing};
#else
    return QVector<CompositingType>{QPainterCompositing};
#endif
}

QString DrmBackend::supportInformation() const
{
    QString supportInfo;
    QDebug s(&supportInfo);
    s.nospace();
    s << "Name: " << "DRM" << endl;
    s << "Active: " << m_active << endl;
    for (int g = 0; g < m_gpus.size(); g++) {
        s << "Atomic Mode Setting on GPU " << g << ": " << m_gpus.at(g)->atomicModeSetting() << endl;
    }
#if HAVE_EGL_STREAMS
    s << "Using EGL Streams: " << m_useEglStreams << endl;
#endif
    return supportInfo;
}

void DrmBackend::changeCursorType(CursorType cursorType)
{
    //switch hardware cursor to software cursor when the screen is recording
    if (!usesSoftwareCursor() && !isCursorHidden() && cursorType == SoftwareCursor) {
        hideCursor();
        setSoftWareCursor(true);
        qDebug() << "switch hardware cursor to software cursor";
    }

    //switch software cursor to hardware cursor when the screen recording is done
    if (usesSoftwareCursor() && m_cursorEnabled && isCursorHidden() && cursorType == HardwareCursor) {
        setSoftWareCursor(false);
        showCursor();
        qDebug() << "switch software cursor to hardware cursor";
    }
}

DmaBufTexture *DrmBackend::createDmaBufTexture(const QSize &size)
{
#if HAVE_GBM
    // gpu_index is a fixed 0 here
    // as the first GPU is assumed to always be the one used for scene rendering
    // and this function is only used for Pipewire
    return GbmDmaBuf::createBuffer(size, m_gpus.at(0)->gbmDevice());
#else
    return nullptr;
#endif
}

DrmGpu *DrmBackend::primaryGpu() const
{
    return m_gpus.isEmpty() ? nullptr : m_gpus.first();
}

DrmGpu *DrmBackend::findGpu(int sysNum) const
{
    auto it = std::find_if(m_gpus.begin(), m_gpus.end(), [sysNum](const auto &gpu) {
        return gpu->drmId() == sysNum;
    });
    return it == m_gpus.end() ? nullptr : *it;
}

}
