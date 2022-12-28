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
#include "log.h"
#if HAVE_GBM
#include <gbm.h>
#include "gbm_dmabuf.h"
#endif
// KWayland
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/outputconfiguration_interface.h>
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
#include <chrono>

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

#define KWIN_DRM_EVENT_CONTEXT_VERSION 2

#include <sys/sdt.h>

namespace KWin
{

int drmIsMaster(int fd)
{
    return drmAuthMagic(fd, 0) != -EACCES;
}

DrmBackend::DrmBackend(QObject *parent)
    : Platform(parent)
    , m_udev(new Udev)
    , m_udevMonitor(m_udev->monitor())
    , m_dpmsFilter()
{
    setSupportsGammaControl(true);
    handleOutputs();
}

DrmBackend::~DrmBackend()
{
#if HAVE_GBM
    if (m_gbmDevice) {
        gbm_device_destroy(m_gbmDevice);
    }
#endif
    if (m_fd >= 0) {
        // wait for pageflips
        while (m_pageFlipsPending != 0) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
        }
        // we need to first remove all outputs
        qDeleteAll(m_outputs);
        m_outputs.clear();
        m_enabledOutputs.clear();

        qDeleteAll(m_planes);
        qDeleteAll(m_crtcs);
        qDeleteAll(m_connectors);
        close(m_fd);
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
    static std::chrono::time_point<std::chrono::steady_clock> st0 = std::chrono::steady_clock::now();
    auto st1 = std::chrono::steady_clock::now();
    if (m_enabledOutputs.count() > 1) {
        if (m_enabledOutputs[0]->geometry() == m_enabledOutputs[1]->geometry()) {
            if (1000 > std::chrono::duration_cast<std::chrono::milliseconds>(st1 - st0).count())
                return;
        }
    }
    st0 = st1;

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
    for (auto it = m_enabledOutputs.constBegin(); it != m_enabledOutputs.constEnd(); ++it) {
        (*it)->m_modesetRequested = true;
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

void DrmBackend::openDrm()
{
    connect(LogindIntegration::self(), &LogindIntegration::sessionActiveChanged, this, &DrmBackend::activate);
    UdevDevice::Ptr device = m_udev->primaryGpu();
    if (!device) {
        qCWarning(KWIN_DRM) << "Did not find a GPU";
        return;
    }
    int fd = LogindIntegration::self()->takeDevice(device->devNode());
    if (fd < 0) {
        qCWarning(KWIN_DRM) << "failed to open drm device at" << device->devNode();
        return;
    }
    m_fd = fd;
    m_active = true;
    QSocketNotifier *notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this,
        [this] {
            if (!LogindIntegration::self()->isActiveSession()) {
                return;
            }
            drmEventContext e;
            memset(&e, 0, sizeof e);
            e.version = KWIN_DRM_EVENT_CONTEXT_VERSION;
            e.page_flip_handler = pageFlipHandler;
            drmHandleEvent(m_fd, &e);
        }
    );
    m_drmId = device->sysNum();

    // trying to activate Atomic Mode Setting (this means also Universal Planes)
    if (!qEnvironmentVariableIsSet("KWIN_DRM_NO_AMS")) {
        if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1) == 0) {
            qCDebug(KWIN_DRM) << "Using Atomic Mode Setting.";
            m_atomicModeSetting = true;

            ScopedDrmPointer<drmModePlaneRes, &drmModeFreePlaneResources> planeResources(drmModeGetPlaneResources(m_fd));
            if (!planeResources) {
                qCWarning(KWIN_DRM) << "Failed to get plane resources. Falling back to legacy mode";
                m_atomicModeSetting = false;
            }

            if (m_atomicModeSetting) {
                qCDebug(KWIN_DRM) << "Number of planes:" << planeResources->count_planes;

                // create the plane objects
                for (unsigned int i = 0; i < planeResources->count_planes; ++i) {
                    drmModePlane *kplane = drmModeGetPlane(m_fd, planeResources->planes[i]);
                    DrmPlane *p = new DrmPlane(kplane->plane_id, m_fd);
                    if (p->atomicInit()) {
                        m_planes << p;
                        if (p->type() == DrmPlane::TypeIndex::Overlay) {
                            m_overlayPlanes << p;
                        }
                    } else {
                        delete p;
                    }
                }

                if (m_planes.isEmpty()) {
                    qCWarning(KWIN_DRM) << "Failed to create any plane. Falling back to legacy mode";
                    m_atomicModeSetting = false;
                }
            }
        } else {
            qCWarning(KWIN_DRM) << "drmSetClientCap for Atomic Mode Setting failed. Using legacy mode.";
        }
    }

    ScopedDrmPointer<_drmModeRes, &drmModeFreeResources> resources(drmModeGetResources(m_fd));
    drmModeRes *res = resources.data();
    if (!resources) {
        qCWarning(KWIN_DRM) << "drmModeGetResources failed";
        DLOGC("Drm get resources failed!FD[%d]", m_fd);
        return;
    }

    for (int i = 0; i < res->count_connectors; ++i) {
        DLOGD("Get connector!ID[%d],Index[%d]", res->connectors[i], i);
        m_connectors << new DrmConnector(res->connectors[i], m_fd);
    }
    for (int i = 0; i < res->count_crtcs; ++i) {
        DLOGD("Get Crtcs!ID[%d],Index[%d]", res->crtcs[i], i);
        m_crtcs << new DrmCrtc(res->crtcs[i], this, i);
    }

    if (m_atomicModeSetting) {
        auto tryAtomicInit = [] (DrmObject *obj) -> bool {
            if (obj->atomicInit()) {
                return false;
            } else {
                delete obj;
                return true;
            }
        };
        m_connectors.erase(std::remove_if(m_connectors.begin(), m_connectors.end(), tryAtomicInit), m_connectors.end());
        m_crtcs.erase(std::remove_if(m_crtcs.begin(), m_crtcs.end(), tryAtomicInit), m_crtcs.end());
    }

    initCursor();
    updateOutputs();

    if (m_outputs.isEmpty()) {
        qCDebug(KWIN_DRM) << "No connected outputs found on startup.";
        DLOGC("No connected outputs found on startup!");
    }

    // setup udevMonitor
    if (m_udevMonitor) {
        m_udevMonitor->filterSubsystemDevType("drm");
        const int fd = m_udevMonitor->fd();
        if (fd != -1) {
            QSocketNotifier *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(notifier, &QSocketNotifier::activated, this,
                [this] {
                    auto device = m_udevMonitor->getDevice();
                    if (!device) {
                        return;
                    }
                    if (device->sysNum() != m_drmId) {
                        return;
                    }
                    if (device->hasProperty("HOTPLUG", "1")) {
                        qCDebug(KWIN_DRM) << "Received hot plug event for monitored drm device";
                        DLOGD("Received hot plug event for %s!", device->devNode());
                        updateOutputs();
                        updateCursor();
                    }
                }
            );
            m_udevMonitor->enable();
        }
    }
    setReady(true);
}

void DrmBackend::updateOutputs()
{
    if (m_fd < 0) {
        DLOGC("Drm fd error!");
        return;
    }

    ScopedDrmPointer<_drmModeRes, &drmModeFreeResources> resources(drmModeGetResources(m_fd));
    if (!resources) {
        qCWarning(KWIN_DRM) << "drmModeGetResources failed";
        DLOGC("Drm get resources failed!FD[%d]", m_fd);
        return;
    }

    QVector<DrmOutput*> connectedOutputs;
    QVector<DrmConnector*> pendingConnectors;

    bool outputChanged = false;

    // split up connected connectors in already or not yet assigned ones
    for (DrmConnector *con : qAsConst(m_connectors)) {
        if (!con->isConnected()) {
            DLOGC("Connector is not connected!ID[%d]", con->id());
            continue;
        }

        if (DrmOutput *o = findOutput(con->id())) {
            DLOGD("Connector is reconnected!ID[%d]", con->id());
            connectedOutputs << o;
        } else {
            DLOGD("New Connector is connected!ID[%d]", con->id());
            pendingConnectors << con;
            outputChanged = true;
        }
    }

    // check for outputs which got removed
    auto it = m_outputs.begin();
    while (it != m_outputs.end()) {
        if (connectedOutputs.contains(*it)) {
            it++;
            continue;
        }
        DrmOutput *removed = *it;

        // handle it later
        if (removed == m_defaultOutput) {
            it++;
            continue;
        }

        outputChanged = true;

        it = m_outputs.erase(it);
        m_enabledOutputs.removeOne(removed);
        emit outputRemoved(removed);
        qCDebug(KWIN_DRM) << "request output teardown from hotplug event" << removed->name() << removed->uuid();
        DLOGD("Request output[%s:%x] teardown from hotplug event!", removed->name().toLocal8Bit().data(), removed->uuid().data());
        removed->teardown();
    }

    if (m_disableMultiScreens && !connectedOutputs.isEmpty()) {
        DLOGD("skip updateOutputs for disableMultiScreens, already has connected screen");
        return;
    }

    // now check new connections
    for (DrmConnector *con : qAsConst(pendingConnectors)) {
        ScopedDrmPointer<_drmModeConnector, &drmModeFreeConnector> connector(drmModeGetConnector(m_fd, con->id()));
        if (!connector) {
            DLOGC("Can't find connector!");
            continue;
        }
        if (connector->count_modes == 0) {
            DLOGC("Connector's mode count is 0!");
            continue;
        }
        bool outputDone = false;

        QVector<uint32_t> encoders = con->encoders();
        for (auto encId : qAsConst(encoders)) {
            ScopedDrmPointer<_drmModeEncoder, &drmModeFreeEncoder> encoder(drmModeGetEncoder(m_fd, encId));
            if (!encoder) {
                DLOGC("Get encoder error!");
                continue;
            }
            for (DrmCrtc *crtc : qAsConst(m_crtcs)) {
                if (!(encoder->possible_crtcs & (1 << crtc->resIndex()))) {
                    DLOGC("No this crtc.ID[%d],Index[%d],Bitmask of possible CRTCs[%d]!", crtc->id(), crtc->resIndex(), encoder->possible_crtcs);
                    continue;
                }

                // check if crtc isn't used yet -- currently we don't allow multiple outputs on one crtc (cloned mode)
                auto it = std::find_if(connectedOutputs.constBegin(), connectedOutputs.constEnd(),
                    [crtc] (DrmOutput *o) {
                        return o->m_crtc == crtc;
                    }
                );
                if (it != connectedOutputs.constEnd()) {
                    continue;
                }

                // we found a suitable encoder+crtc
                // TODO: we could avoid these lib drm calls if we store all struct data in DrmCrtc and DrmConnector in the beginning
                ScopedDrmPointer<_drmModeCrtc, &drmModeFreeCrtc> modeCrtc(drmModeGetCrtc(m_fd, crtc->id()));
                if (!modeCrtc) {
                    DLOGC("Get crtc[%d] error!", crtc->id());
                    continue;
                }

                DrmOutput *output = new DrmOutput(this);
                con->setOutput(output);
                output->m_conn = con;
                crtc->setOutput(output);
                output->m_crtc = crtc;
                connect(output, &DrmOutput::dpmsChanged, this, &DrmBackend::outputDpmsChanged);

                if (connector) {
                    DLOGD("Use connector mode");
                    output->m_mode = connector->modes[0];
                } else if (modeCrtc->mode_valid) {
                    DLOGD("Use crtc mode");
                    output->m_mode = modeCrtc->mode;
                }
                qCDebug(KWIN_DRM) << "For new output use mode " << output->m_mode.name << output->name() \
                                  << "connector mode" << connector->modes[0].name \
                                  << "crtc mode" << modeCrtc->mode.name;
                DLOGD("Output[%s] mode[%s]", output->name().toLocal8Bit().data(), output->m_mode.name);

                if (!output->init(connector.data())) {
                    qCWarning(KWIN_DRM) << "Failed to create output for connector " << con->id();
                    DLOGC("Failed to create output for connector[%d].", con->id());
                    delete output;
                    continue;
                }
                if (!output->initCursor(m_cursorSize)) {
                    DLOGD("Use software cursor.");
                    qCDebug(KWIN_DRM) << "Use software cursor.";
                    setSoftWareCursor(true);
                }
                qCDebug(KWIN_DRM) << "Found new output with uuid" << output->uuid();
                DLOGD("Found new output with uuid[%s]", output->uuid().data());

                connectedOutputs << output;
                m_enabledOutputs << output;
                emit outputAdded(output);
                outputDone = true;
                break;
            }
            if (outputDone) {
                break;
            }
        }
        if (m_disableMultiScreens && !connectedOutputs.isEmpty()) {
            DLOGD("skip for disableMultiScreens, already has pending Connector");
            break;
        }
    }
    std::sort(connectedOutputs.begin(), connectedOutputs.end(), [] (DrmOutput *a, DrmOutput *b) { return a->m_conn->id() < b->m_conn->id(); });

    if (!outputChanged) {
        qCDebug(KWIN_DRM) << "Not output chenged, stop update outputs";
        return;
    }

    m_outputs = connectedOutputs;

    if (!m_outputs.isEmpty() && m_defaultOutput && m_defaultOutput->isEnabled()) {
        m_defaultOutput->waylandOutputDevice()->setEnabled(KWayland::Server::OutputDeviceInterface::Enablement::Disabled);
        m_defaultOutput->waylandOutputDevice()->destroy();
        m_defaultOutput->waylandOutput()->destroy();
        m_enabledOutputs.removeOne(m_defaultOutput);
        emit outputRemoved(m_defaultOutput);
    }

    if (m_outputs.size() == 1 && m_enabledOutputs.isEmpty()) {
        auto* output = m_outputs.first();
        output->setEnabled(true);
    }

    readOutputsConfiguration();
    outputDpmsChanged();

    if (!m_outputs.isEmpty()) {
        qDebug() << "emit screensQueried";
        emit screensQueried();
    } else if (!m_defaultOutput) {
        QTimer::singleShot(100, this, [=] { emit startWithoutScreen(); });
    }
}

void DrmBackend::disableMultiScreens()
{
    m_disableMultiScreens = true;
}

void DrmBackend::installDefaultDisplay()
{
    for (DrmConnector *con : qAsConst(m_connectors)) {
        if (con->isConnected()) {
            DLOGD("Already received physical display, will not create virtual display");
            return;
        }
    }
    DLOGD("There is no physical display, install virtual display for system");
    DrmOutput *output = new DrmOutput(this);
    output->m_isVirtual = true;

    drmModeModeInfo mode;
    strcpy(mode.name, "1920x1080");
    mode.clock = 148500;
    mode.hdisplay = 1920;
    mode.hsync_start = 2008;
    mode.hsync_end = 2052;
    mode.htotal = 2200;
    mode.hskew = 0;

    mode.vdisplay = 1080;
    mode.vsync_start = 1084;
    mode.vsync_end = 1089;
    mode.vtotal = 1125,
    mode.vscan = 0;

    mode.vrefresh = 60;

    mode.flags = 5;
    mode.type = 72;

    //connector_id  48   47   1   1   530   300   15   5

    //1.Add mode info
    output->m_mode = mode;

    output->setRawPhysicalSize(QSize(0, 0));

    //2.init wayland output device
    QString name = "VGA-1";
    QString model = "VGA-1000-unknown";
    QString manufacturer = "UOS";
    QByteArray uuid = "ffffffffff";
    QVector<KWayland::Server::OutputDeviceInterface::Mode> modes;
    KWayland::Server::OutputDeviceInterface::Mode dmode;
    dmode.id = 0;
    dmode.size = QSize(mode.hdisplay, mode.vdisplay);
    dmode.flags = KWayland::Server::OutputDeviceInterface::ModeFlag::Current;
    dmode.refreshRate = mode.vrefresh * 1000LL;
    modes << dmode;
    output->initWaylandOutputDevice(name, model, manufacturer, uuid, modes);

    //3.set enable
    output->setEnabled(true);

    //4.set position and scale
    QPoint pos(0, 0);
    output->setGlobalPos(pos);
    output->setScale(1.0);

    //5.set wayland mode
    output->setWaylandMode();

    m_defaultOutput = output;

    m_outputs << output;
    m_enabledOutputs << output;
    emit outputAdded(output);

    setOutputsEnabled(true);
    setSoftWareCursor(true);

    emit screensQueried();
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
        const auto outputConfig = configGroup.group((*it)->uuid());
        if ((*it)->hasSetGlobalPosition()) {
            pos = (*it)->globalPos();
        }
        qCDebug(KWIN_DRM) << "Reading output configuration for [" << uuid << "] ["<< (*it)->uuid() << "]"
            << outputConfig.readEntry<QPoint>("Position", pos);
        (*it)->setGlobalPos(outputConfig.readEntry<QPoint>("Position", pos));
        // TODO: add mode
        (*it)->setScale(outputConfig.readEntry("Scale", 1.0));
        if ((*it)->isEnabled()) {
            pos.setX(pos.x() + (*it)->geometry().width());
        }
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
    qDebug() << "output" << output->uuid() << output->geometry() << output->globalPos() << enable;
    if (enable) {
        Q_ASSERT(!m_enabledOutputs.contains(output));
        m_enabledOutputs << output;
        if (!usesSoftwareCursor()) {
            qDebug() << "showCursor" ;
            output->setShowCursor(true);
        }
        emit outputAdded(output);
    } else {
        if (m_enabledOutputs.contains(output)) {
            //m_enabledOutputs.removeOne(output);
            m_enabledOutputs.removeAll(output);
            Q_ASSERT(!m_enabledOutputs.contains(output));
            emit outputRemoved(output);
        } 
    }
    outputDpmsChanged();
    checkOutputsAreOn();
    emit screensQueried();
}

void DrmBackend::configurationChangeRequested(KWayland::Server::OutputConfigurationInterface *config)
{
    const auto changes = config->changes();
    bool countChanged = false;

    //process all non-disabling changes
    for (auto it = changes.begin(); it != changes.end(); it++) {
        KWayland::Server::OutputChangeSet *changeset = it.value();

        auto drmoutput = findOutput(it.key()->uuid());
        if (drmoutput == nullptr) {
            qCWarning(KWIN_DRM) << "Could NOT find DrmOutput matching " << it.key()->uuid();
            continue;
        }
        if (changeset->enabledChanged() && changeset->enabled() == KWayland::Server::OutputDeviceInterface::Enablement::Enabled) {
            qDebug() << "-----------" << __func__ << "enable" << drmoutput->uuid() << drmoutput->geometry();
            drmoutput->setEnabled(true);
            countChanged = true;
        }
        drmoutput->setChanges(changeset);
    }
    //process any disable requests
    for (auto it = changes.begin(); it != changes.end(); it++) {
        KWayland::Server::OutputChangeSet *changeset = it.value();
        if (changeset->enabledChanged() && changeset->enabled() == KWayland::Server::OutputDeviceInterface::Enablement::Disabled) {
            if (m_enabledOutputs.count() == 1) {
                qCWarning(KWIN_DRM) << "Not disabling final screen" << it.key()->uuid();
                continue;
            }
            auto drmoutput = findOutput(it.key()->uuid());
            if (drmoutput == nullptr) {
                qCWarning(KWIN_DRM) << "Could NOT find DrmOutput matching " << it.key()->uuid();
                continue;
            }
            qDebug() << "-----------" << __func__ << "disable" << drmoutput->uuid();
            drmoutput->setEnabled(false);
            countChanged = true;
        }
    }

    if (countChanged) {
        emit screensQueried();
    } else {
        emit screens()->changed();
    }
    config->setApplied();
    updateCursor();
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

DrmOutput *DrmBackend::findOutput(const QByteArray &uuid)
{
    auto it = std::find_if(m_outputs.constBegin(), m_outputs.constEnd(), [uuid] (DrmOutput *o) {
        return o->m_uuid == uuid;
    });
    if (it != m_outputs.constEnd()) {
        return *it;
    }
    return nullptr;
}

void DrmBackend::present(DrmBuffer *buffer, DrmOutput *output)
{
    if (!buffer || buffer->bufferId() == 0) {
        if (m_deleteBufferAfterPageFlip) {
            delete buffer;
        }
        return;
    }

    if (output->present(buffer)) {
        m_pageFlipsPending++;
        if (m_pageFlipsPending == 1 && Compositor::self()) {
            Compositor::self()->aboutToSwapBuffers();
        }
    } else if (m_deleteBufferAfterPageFlip) {
        delete buffer;
    }
}

void DrmBackend::initCursor()
{
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
                    if (!(*it)->showCursor() && drmIsMaster(fd())) {
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
    if (drmGetCap(m_fd, DRM_CAP_CURSOR_WIDTH, &capability) == 0) {
        cursorSize.setWidth(capability);
    } else {
        cursorSize.setWidth(64);
    }
    if (drmGetCap(m_fd, DRM_CAP_CURSOR_HEIGHT, &capability) == 0) {
        cursorSize.setHeight(capability);
    } else {
        cursorSize.setHeight(64);
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
                if (!(*it)->showCursor() && drmIsMaster(fd())) {
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
    if (!m_cursorEnabled) {
        return;
    }
    for (auto it = m_outputs.constBegin(); it != m_outputs.constEnd(); ++it) {
        (*it)->hideCursor();
    }
}

void DrmBackend::moveCursor()
{
    if (!m_cursorEnabled || isCursorHidden()) {
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
    return new DrmQPainterBackend(this);
}

OpenGLBackend *DrmBackend::createOpenGLBackend()
{
#if HAVE_GBM
    m_deleteBufferAfterPageFlip = true;
    m_eglGbmBackend =  new EglGbmBackend(this);
    return m_eglGbmBackend;
#else
    return Platform::createOpenGLBackend();
#endif
}

OpenGLBackend *DrmBackend::getOpenGLBackend()
{
    return m_eglGbmBackend;
}

DmaBufTexture *DrmBackend::createDmaBufTexture(const QSize &size)
{
#if HAVE_GBM
    return GbmDmaBuf::createBuffer(size, m_gbmDevice);
#else
    return nullptr;
#endif
}

bool DrmBackend::requiresCompositing() const
{
    return true;
}

DrmDumbBuffer *DrmBackend::createBuffer(const QSize &size)
{
    DrmDumbBuffer *b = new DrmDumbBuffer(m_fd, size);
    DTRACE_PROBE2(drm_backend, createBufferSize, size.width(), size.height());
    return b;
}

#if HAVE_GBM
DrmSurfaceBuffer *DrmBackend::createBuffer(const std::shared_ptr<GbmSurface> &surface)
{
    DrmSurfaceBuffer *b = new DrmSurfaceBuffer(m_fd, surface);
    DTRACE_PROBE(drm_backend, createBufferSurface);
    return b;
}

DrmSurfaceBuffer *DrmBackend::createBuffer(const std::shared_ptr<GbmSurface> &surface, uint32_t format, QVector<uint64_t> &modifiers)
{
    DrmSurfaceBuffer *b = new DrmSurfaceBuffer(m_fd, surface, format, modifiers);
    DTRACE_PROBE1(drm_backend, createBufferSurfaceFormat, format);
    return b;
}
#endif

void DrmBackend::outputDpmsChanged()
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
    s << "Atomic Mode Setting: " << m_atomicModeSetting << endl;
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

}
