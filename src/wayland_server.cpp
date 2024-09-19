/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "wayland_server.h"

#include <config-kwin.h>

#include "backends/drm/drm_backend.h"
#include "composite.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "idle_inhibition.h"
#include "inputpanelv1integration.h"
#include "keyboard_input.h"
#include "layershellv1integration.h"
#include "main.h"
#include "scene/workspacescene.h"
#include "unmanaged.h"
#include "utils/serviceutils.h"
#include "virtualdesktops.h"
#include "wayland/appmenu_interface.h"
#include "wayland/blur_interface.h"
#include "wayland/compositor_interface.h"
#include "wayland/contenttype_v1_interface.h"
#include "wayland/datacontroldevicemanager_v1_interface.h"
#include "wayland/datadevicemanager_interface.h"
#include "wayland/datasource_interface.h"
#include "wayland/display.h"
#include "wayland/dpms_interface.h"
#include "wayland/drmlease_v1_interface.h"
#include "wayland/filtered_display.h"
#include "wayland/fractionalscale_v1_interface.h"
#include "wayland/idle_interface.h"
#include "wayland/idleinhibit_v1_interface.h"
#include "wayland/idlenotify_v1_interface.h"
#include "wayland/inputmethod_v1_interface.h"
#include "wayland/keyboard_shortcuts_inhibit_v1_interface.h"
#include "wayland/keystate_interface.h"
#include "wayland/linuxdmabufv1clientbuffer.h"
#include "wayland/lockscreen_overlay_v1_interface.h"
#include "wayland/output_interface.h"
#include "wayland/output_order_v1_interface.h"
#include "wayland/outputdevice_v2_interface.h"
#include "wayland/outputmanagement_v2_interface.h"
#include "wayland/outputdevice_v1_interface.h"
#include "wayland/outputmanagement_v1_interface.h"
#include "wayland/plasmashell_interface.h"
#include "wayland/plasmavirtualdesktop_interface.h"
#include "wayland/plasmawindowmanagement_interface.h"
#include "wayland/pointerconstraints_v1_interface.h"
#include "wayland/pointergestures_v1_interface.h"
#include "wayland/primaryselectiondevicemanager_v1_interface.h"
#include "wayland/relativepointer_v1_interface.h"
#include "wayland/seat_interface.h"
#include "wayland/server_decoration_interface.h"
#include "wayland/server_decoration_palette_interface.h"
#include "wayland/shadow_interface.h"
#include "wayland/subcompositor_interface.h"
#include "wayland/tablet_v2_interface.h"
#include "wayland/tearingcontrol_v1_interface.h"
#include "wayland/viewporter_interface.h"
#include "wayland/xdgactivation_v1_interface.h"
#include "wayland/xdgdecoration_v1_interface.h"
#include "wayland/xdgforeign_v2_interface.h"
#include "wayland/xdgoutput_v1_interface.h"
#include "wayland/xdgshell_interface.h"
#include "wayland/xwaylandkeyboardgrab_v1_interface.h"
#include "wayland/xwaylandshell_v1_interface.h"
#include "wayland/ddeshell_interface.h"
#include "wayland/ddeseat_interface.h"
#include <wayland/ddesecurity_interface.h>
#include <wayland/dderestrict_interface.h>
#include <wayland/ddekvm_interface.h>
#include "wayland/strut_interface.h"
#include "workspace.h"
#include "x11window.h"
#include "xdgactivationv1.h"
#include "xdgshellintegration.h"
#include "xdgshellwindow.h"
#include "xdgshellv6integration.h"
#include "xdgshellv6window.h"

// Qt
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QWindow>

// system
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// screenlocker
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif

using namespace KWaylandServer;

#ifndef OPAQUE_PTR_Wl_Resource
  #define OPAQUE_PTR_Wl_Resource
  Q_DECLARE_OPAQUE_POINTER(wl_resource*)
#endif

namespace KWin
{

KWIN_SINGLETON_FACTORY(WaylandServer)

class KWinDisplay : public KWaylandServer::FilteredDisplay
{
public:
    KWinDisplay(QObject *parent)
        : KWaylandServer::FilteredDisplay(parent)
    {
        QString enableBlackList = QString::fromUtf8(qgetenv("KWIN_ENABLE_INTERFACES_BLACKLIST"));
        if (enableBlackList.isEmpty() || enableBlackList.toLower() == "false") {
            qCDebug(KWIN_CORE) << "Interfaces blacklist is not enabled";
            m_enableBlackList = false;
        }
    }

    static QByteArray sha256(const QString &fileName)
    {
        QFile f(fileName);
        if (f.open(QFile::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if (hash.addData(&f)) {
                return hash.result();
            }
        }
        return QByteArray();
    }

    bool isTrustedOrigin(KWaylandServer::ClientConnection *client) const
    {
        const auto fullPathSha = sha256(client->executablePath());
        const auto localSha = sha256(QLatin1String("/proc/") + QString::number(client->processId()) + QLatin1String("/exe"));
        const bool trusted = !localSha.isEmpty() && fullPathSha == localSha;

        if (!trusted) {
            qCWarning(KWIN_CORE) << "Could not trust" << client->executablePath() << "sha" << localSha << fullPathSha;
        }

        return trusted;
    }

    QStringList fetchRequestedInterfaces(KWaylandServer::ClientConnection *client) const
    {
        return KWin::fetchRequestedInterfaces(client->executablePath());
    }

    const QSet<QByteArray> interfacesBlackList = {
        QByteArrayLiteral("org_kde_kwin_remote_access_manager"),
        QByteArrayLiteral("org_kde_plasma_window_management"),
        QByteArrayLiteral("org_kde_kwin_fake_input"),
        QByteArrayLiteral("org_kde_kwin_keystate"),
        QByteArrayLiteral("zkde_screencast_unstable_v1"),
        QByteArrayLiteral("org_kde_plasma_activation_feedback"),
        QByteArrayLiteral("kde_lockscreen_overlay_v1"),
    };

    const QSet<QByteArray> inputmethodInterfaces = {"zwp_input_panel_v1", "zwp_input_method_v1"};
    const QSet<QByteArray> xwaylandInterfaces = {
        QByteArrayLiteral("zwp_xwayland_keyboard_grab_manager_v1"),
        QByteArrayLiteral("xwayland_shell_v1"),
    };

    QSet<QString> m_reported;
    bool m_enableBlackList = true;

    bool allowInterface(KWaylandServer::ClientConnection *client, const QByteArray &interfaceName) override
    {
        if (client->processId() == getpid()) {
            return true;
        }

        if (client != waylandServer()->inputMethodConnection() && inputmethodInterfaces.contains(interfaceName)) {
            return false;
        }

        if (!interfacesBlackList.contains(interfaceName)) {
            return true;
        }

        if (!m_enableBlackList) {
            return true;
        }

        if (client->executablePath().isEmpty()) {
            qCDebug(KWIN_CORE) << "Could not identify process with pid" << client->processId();
            return false;
        }

        static bool permissionCheckDisabled = qEnvironmentVariableIntValue("KWIN_WAYLAND_NO_PERMISSION_CHECKS") == 1;
        if (!permissionCheckDisabled) {
            auto requestedInterfaces = client->property("requestedInterfaces");
            if (requestedInterfaces.isNull()) {
                requestedInterfaces = fetchRequestedInterfaces(client);
                client->setProperty("requestedInterfaces", requestedInterfaces);
            }
            if (!requestedInterfaces.toStringList().contains(QString::fromUtf8(interfaceName))) {
                if (KWIN_CORE().isDebugEnabled()) {
                    const QString id = client->executablePath() + QLatin1Char('|') + QString::fromUtf8(interfaceName);
                    if (!m_reported.contains(id)) {
                        m_reported.insert(id);
                        qCDebug(KWIN_CORE) << "Interface" << interfaceName << "not in X-KDE-Wayland-Interfaces of" << client->executablePath();
                    }
                }
                return false;
            }
        }

        {
            auto trustedOrigin = client->property("isPrivileged");
            if (trustedOrigin.isNull()) {
                trustedOrigin = isTrustedOrigin(client);
                client->setProperty("isPrivileged", trustedOrigin);
            }

            if (!trustedOrigin.toBool()) {
                return false;
            }
        }
        qCDebug(KWIN_CORE) << "authorized" << client->executablePath() << interfaceName;
        return true;
    }
};

WaylandServer::WaylandServer(QObject *parent)
    : QObject(parent)
    , m_display(new KWinDisplay(this))
{
}

WaylandServer::~WaylandServer()
{
    s_self = nullptr;
}

KWaylandServer::ClientConnection *WaylandServer::xWaylandConnection() const
{
    return m_xwaylandConnection;
}

KWaylandServer::ClientConnection *WaylandServer::inputMethodConnection() const
{
    return m_inputMethodServerConnection;
}

void WaylandServer::registerWindow(Window *window)
{
    if (window->readyForPainting()) {
        Q_EMIT windowAdded(window);
    } else {
        connect(window, &Window::windowShown, this, &WaylandServer::windowShown);
    }
    m_windows << window;
}

void WaylandServer::registerXdgToplevelWindow(XdgToplevelWindow *window)
{
    // TODO: Find a better way and more generic to install extensions.

    SurfaceInterface *surface = window->surface();

    registerWindow(window);

    if (auto shellSurface = PlasmaShellSurfaceInterface::get(surface)) {
        window->installPlasmaShellSurface(shellSurface);
    }
    if (auto decoration = ServerSideDecorationInterface::get(surface)) {
        window->installServerDecoration(decoration);
    }
    if (auto decoration = XdgToplevelDecorationV1Interface::get(window->shellSurface())) {
        window->installXdgDecoration(decoration);
    }
    if (auto menu = m_appMenuManager->appMenuForSurface(surface)) {
        window->installAppMenu(menu);
    }
    if (auto palette = m_paletteManager->paletteForSurface(surface)) {
        window->installPalette(palette);
    }
    if (auto shellSurface = DDEShellSurfaceInterface::get(surface)) {
        window->installDDEShellSurface(shellSurface);
    }

    connect(m_XdgForeign, &XdgForeignV2Interface::transientChanged, window, [this](SurfaceInterface *child) {
        Q_EMIT foreignTransientChanged(child);
    });

    workspace()->checkIfFirstWindowOfProcess(window);
}

void WaylandServer::registerXdgToplevelV6Window(XdgToplevelV6Window *window)
{
    // TODO: Find a better way and more generic to install extensions.

    SurfaceInterface *surface = window->surface();

    registerWindow(window);

    if (auto shellSurface = PlasmaShellSurfaceInterface::get(surface)) {
        window->installPlasmaShellSurface(shellSurface);
    }
    if (auto decoration = ServerSideDecorationInterface::get(surface)) {
        window->installServerDecoration(decoration);
    }
    // if (auto decoration = XdgToplevelDecorationV1Interface::get(window->shellSurface())) {
    //     window->installXdgDecoration(decoration);
    // }
    if (auto menu = m_appMenuManager->appMenuForSurface(surface)) {
        window->installAppMenu(menu);
    }
    if (auto palette = m_paletteManager->paletteForSurface(surface)) {
        window->installPalette(palette);
    }
    if (auto shellSurface = DDEShellSurfaceInterface::get(surface)) {
        window->installDDEShellSurface(shellSurface);
    }

    connect(m_XdgForeign, &XdgForeignV2Interface::transientChanged, window, [this](SurfaceInterface *child) {
        Q_EMIT foreignTransientChanged(child);
    });

    workspace()->checkIfFirstWindowOfProcess(window);
}

void WaylandServer::registerXdgGenericWindow(Window *window)
{
    FUNC_DEBUG_LOG(Q_FUNC_INFO, window->surface()->id());
    if (auto toplevel = qobject_cast<XdgToplevelWindow *>(window)) {
        registerXdgToplevelWindow(toplevel);
        return;
    }
    if (auto popup = qobject_cast<XdgPopupWindow *>(window)) {
        registerWindow(popup);
        if (auto shellSurface = PlasmaShellSurfaceInterface::get(popup->surface())) {
            popup->installPlasmaShellSurface(shellSurface);
        }
        if (auto shellSurface = DDEShellSurfaceInterface::get(window->surface())) {
            popup->installDDEShellSurface(shellSurface);
        }
        return;
    }
    qCDebug(KWIN_CORE) << "Received invalid xdg shell window:" << window->surface();
}

void WaylandServer::registerXdgGenericV6Window(Window *window)
{
    if (auto toplevel = qobject_cast<XdgToplevelV6Window *>(window)) {
        registerXdgToplevelV6Window(toplevel);
        return;
    }
    if (auto popup = qobject_cast<XdgPopupV6Window *>(window)) {
        registerWindow(popup);
        if (auto shellSurface = PlasmaShellSurfaceInterface::get(popup->surface())) {
            popup->installPlasmaShellSurface(shellSurface);
        }
        if (auto shellSurface = DDEShellSurfaceInterface::get(window->surface())) {
            popup->installDDEShellSurface(shellSurface);
        }
        return;
    }
    qCDebug(KWIN_CORE) << "Received invalid xdg shell window:" << window->surface();
}

void WaylandServer::handleOutputAdded(Output *output)
{
    if (!output->isPlaceholder() && !output->isNonDesktop()) {
        m_waylandOutputDevices.insert(output, new KWaylandServer::OutputDeviceV2Interface(m_display, output));
    }
    if (!output->isPlaceholder() && !output->isNonDesktop()) {
        m_waylandOutputV1Devices.insert(output, new KWaylandServer::OutputDeviceInterface(m_display, output));
    }
}

void WaylandServer::handleOutputRemoved(Output *output)
{
    if (auto outputDevice = m_waylandOutputDevices.take(output)) {
        outputDevice->remove();
    }
    if (auto outputDevice = m_waylandOutputV1Devices.take(output)) {
        outputDevice->remove();
    }
}

void WaylandServer::handleOutputEnabled(Output *output)
{
    if (!output->isPlaceholder() && !output->isNonDesktop()) {
        auto waylandOutput = new KWaylandServer::OutputInterface(waylandServer()->display(), output);
        m_xdgOutputManagerV1->offer(waylandOutput);

        m_waylandOutputs.insert(output, waylandOutput);
    }
}

void WaylandServer::handleOutputDisabled(Output *output)
{
    if (auto waylandOutput = m_waylandOutputs.take(output)) {
        waylandOutput->remove();
    }
}

KWaylandServer::OutputInterface *WaylandServer::findWaylandOutput(Output * output) const
{
    if (m_waylandOutputs.contains(output))
    {
        return m_waylandOutputs[output];
    }
    return nullptr;
}

bool WaylandServer::start()
{
    return m_display->start();
}

bool WaylandServer::init(const QString &socketName, InitializationFlags flags)
{
    if (!m_display->addSocketName(socketName)) {
        return false;
    }
    return init(flags);
}

bool WaylandServer::init(InitializationFlags flags)
{
    m_initFlags = flags;
    m_compositor = new CompositorInterface(m_display, m_display);
    connect(m_compositor, &CompositorInterface::surfaceCreated, this, [this](SurfaceInterface *surface) {
        // check whether we have a Window with the Surface's id
        Workspace *ws = Workspace::self();
        if (!ws) {
            // it's possible that a Surface gets created before Workspace is created
            return;
        }
        if (surface->client() != xWaylandConnection()) {
            // setting surface is only relevant for Xwayland clients
            return;
        }

        X11Window *window = ws->findClient([surface](const X11Window *window) {
            return window->pendingSurfaceId() == surface->id();
        });
        if (window) {
            window->setSurface(surface);
            return;
        }

        Unmanaged *unmanaged = ws->findUnmanaged([surface](const Unmanaged *unmanaged) {
            return unmanaged->pendingSurfaceId() == surface->id();
        });
        if (unmanaged) {
            unmanaged->setSurface(surface);
            return;
        }

        // The surface will be bound later when a WL_SURFACE_ID message is received.
    });

    m_xwaylandShell = new KWaylandServer::XwaylandShellV1Interface(m_display, m_display);
    connect(m_xwaylandShell, &KWaylandServer::XwaylandShellV1Interface::surfaceAssociated, this, [](KWaylandServer::XwaylandSurfaceV1Interface *surface) {
        X11Window *window = workspace()->findClient([&surface](const X11Window *window) {
            return window->surfaceSerial() == surface->serial();
        });
        if (window) {
            window->setSurface(surface->surface());
            return;
        }

        Unmanaged *unmanaged = workspace()->findUnmanaged([&surface](const Unmanaged *window) {
            return window->surfaceSerial() == surface->serial();
        });
        if (unmanaged) {
            unmanaged->setSurface(surface->surface());
            return;
        }
    });

    m_tabletManagerV2 = new TabletManagerV2Interface(m_display, m_display);
    m_keyboardShortcutsInhibitManager = new KeyboardShortcutsInhibitManagerV1Interface(m_display, m_display);

    auto inputPanelV1Integration = new InputPanelV1Integration(this);
    connect(inputPanelV1Integration, &InputPanelV1Integration::windowCreated,
            this, &WaylandServer::registerWindow);

    auto xdgShellIntegration = new XdgShellIntegration(this);
    connect(xdgShellIntegration, &XdgShellIntegration::windowCreated,
            this, &WaylandServer::registerXdgGenericWindow);

    auto xdgShellV6Integration = new XdgShellV6Integration(this);
    connect(xdgShellV6Integration, &XdgShellV6Integration::windowCreated,
            this, &WaylandServer::registerXdgGenericV6Window);

    auto layerShellV1Integration = new LayerShellV1Integration(this);
    connect(layerShellV1Integration, &LayerShellV1Integration::windowCreated,
            this, &WaylandServer::registerWindow);

    m_xdgDecorationManagerV1 = new XdgDecorationManagerV1Interface(m_display, m_display);
    connect(m_xdgDecorationManagerV1, &XdgDecorationManagerV1Interface::decorationCreated, this, [this](XdgToplevelDecorationV1Interface *decoration) {
        if (XdgToplevelWindow *toplevel = findXdgToplevelWindow(decoration->toplevel()->surface())) {
            toplevel->installXdgDecoration(decoration);
        }
    });

    new ViewporterInterface(m_display, m_display);
    new FractionalScaleManagerV1Interface(m_display, m_display);
    m_display->createShm();
    m_seat = new SeatInterface(m_display, m_display);
    new PointerGesturesV1Interface(m_display, m_display);
    new PointerConstraintsV1Interface(m_display, m_display);
    new RelativePointerManagerV1Interface(m_display, m_display);
    m_dataDeviceManager = new DataDeviceManagerInterface(m_display, m_display);
    new DataControlDeviceManagerV1Interface(m_display, m_display);

    const auto kwinConfig = kwinApp()->config();
    if (kwinConfig->group("Wayland").readEntry("EnablePrimarySelection", true)) {
        new PrimarySelectionDeviceManagerV1Interface(m_display, m_display);
    }

    m_idle = new IdleInterface(m_display, m_display);
    auto idleInhibition = new IdleInhibition(m_idle);
    connect(this, &WaylandServer::windowAdded, idleInhibition, &IdleInhibition::registerClient);
    new IdleInhibitManagerV1Interface(m_display, m_display);
    new IdleNotifyV1Interface(m_display, m_display);
    m_plasmaShell = new PlasmaShellInterface(m_display, m_display);
    connect(m_plasmaShell, &PlasmaShellInterface::surfaceCreated, this, [this](PlasmaShellSurfaceInterface *surface) {
        if (XdgSurfaceWindow *window = findXdgSurfaceWindow(surface->surface())) {
            window->installPlasmaShellSurface(surface);
        }
        if (XdgSurfaceV6Window *window = findXdgSurfaceV6Window(surface->surface())) {
            window->installPlasmaShellSurface(surface);
        }
    });
    m_appMenuManager = new AppMenuManagerInterface(m_display, m_display);
    connect(m_appMenuManager, &AppMenuManagerInterface::appMenuCreated, this, [this](AppMenuInterface *appMenu) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(appMenu->surface())) {
            window->installAppMenu(appMenu);
        }
    });
    m_paletteManager = new ServerSideDecorationPaletteManagerInterface(m_display, m_display);
    connect(m_paletteManager, &ServerSideDecorationPaletteManagerInterface::paletteCreated, this, [this](ServerSideDecorationPaletteInterface *palette) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(palette->surface())) {
            window->installPalette(palette);
        }
        if (XdgToplevelV6Window *window = findXdgToplevelV6Window(palette->surface())) {
            window->installPalette(palette);
        }
    });

    m_windowManagement = new PlasmaWindowManagementInterface(m_display, m_display);
    m_windowManagement->setShowingDesktopState(PlasmaWindowManagementInterface::ShowingDesktopState::Disabled);
    connect(m_windowManagement, &PlasmaWindowManagementInterface::requestChangeShowingDesktop, this, [](PlasmaWindowManagementInterface::ShowingDesktopState state) {
        if (!workspace()) {
            return;
        }
        bool set = false;
        switch (state) {
        case PlasmaWindowManagementInterface::ShowingDesktopState::Disabled:
            set = false;
            break;
        case PlasmaWindowManagementInterface::ShowingDesktopState::Enabled:
            set = true;
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
        if (set == workspace()->showingDesktop()) {
            return;
        }
        workspace()->setShowingDesktop(set);
    });

    m_virtualDesktopManagement = new PlasmaVirtualDesktopManagementInterface(m_display, m_display);
    m_windowManagement->setPlasmaVirtualDesktopManagementInterface(m_virtualDesktopManagement);

    m_plasmaActivationFeedback = new PlasmaWindowActivationFeedbackInterface(m_display, m_display);

    new ShadowManagerInterface(m_display, m_display);
    new DpmsManagerInterface(m_display, m_display);

    m_decorationManager = new ServerSideDecorationManagerInterface(m_display, m_display);
    connect(m_decorationManager, &ServerSideDecorationManagerInterface::decorationCreated, this, [this](ServerSideDecorationInterface *decoration) {
        if (XdgToplevelWindow *window = findXdgToplevelWindow(decoration->surface())) {
            window->installServerDecoration(decoration);
        }
        if (XdgToplevelV6Window *window = findXdgToplevelV6Window(decoration->surface())) {
            window->installServerDecoration(decoration);
        }
    });

    m_outputManagement = new OutputManagementV2Interface(m_display, m_display);
    m_outputV1Management = new OutputManagementInterface(m_display, m_display);

    m_xdgOutputManagerV1 = new XdgOutputManagerV1Interface(m_display, m_display);
    new SubCompositorInterface(m_display, m_display);
    m_XdgForeign = new XdgForeignV2Interface(m_display, m_display);
    m_inputMethod = new InputMethodV1Interface(m_display, m_display);
    m_xWaylandKeyboardGrabManager = new XWaylandKeyboardGrabManagerV1Interface(m_display, m_display);
    connect(m_xWaylandKeyboardGrabManager, &XWaylandKeyboardGrabManagerV1Interface::XWaylandKeyboardGrabV1Created,
        [this] (XWaylandKeyboardGrabV1Interface *grab) {
            m_grabClient = grab;
            qDebug() << "grab successfully";
        }
    );
    connect(m_xWaylandKeyboardGrabManager, &XWaylandKeyboardGrabManagerV1Interface::XWaylandKeyboardGrabV1Destroyed,
        [this] () {
            m_grabClient = nullptr;
            qDebug() << "grab destroyed";
        }
    );

    m_clientManagement = new ClientManagementInterface(m_display, m_display);
    connect(m_clientManagement, &ClientManagementInterface::windowStatesRequest, this,
        [this] () {
            if (!workspace()) {
                qWarning () << "windowStatesRequest before workspace initilized";
                return;
            }
            workspace()->updateWindowStates();
        }
    );
    connect(m_clientManagement, &ClientManagementInterface::windowFromPointRequest, this,
        [this] () {
            if (!waylandServer())
                return;
            auto client_management = waylandServer()->clientManagement();
            if (!client_management)
                return;
            Window *target = input()->findToplevel(Cursors::self()->mouse()->pos());
            if (target)
                client_management->sendWindowFromPoint(target->frameId());
        }
    );
    connect(m_clientManagement, &ClientManagementInterface::showSplitMenuRequest, this,
        [this] (const QRect &botton_rect, uint32_t wid) {
            workspace()->showSplitMenu(botton_rect, wid);
        }
    );
    connect(m_clientManagement, &ClientManagementInterface::hideSplitMenuRequest, this,
        [this] (bool delay) {
            workspace()->hideSplitMenu(delay);
        }
    );
    connect(m_clientManagement, &ClientManagementInterface::allWindowIdRequest, this,
        [this] () {
            if (!waylandServer())
                return;
            auto client_management = waylandServer()->clientManagement();
            if (!client_management)
                return;
            const QList<Window *> &window_list = workspace()->stackingOrder();
            QList<uint32_t> id_list;
            for (Window *w : window_list)
                id_list.append(w->window() ? w->window() : w->frameId());
            client_management->sendAllWindowId(id_list);
        }
    );
    connect(m_clientManagement, &ClientManagementInterface::specificWindowStateRequest, this,
        [this] (uint32_t wid) {
            if (!waylandServer())
                return;
            auto client_management = waylandServer()->clientManagement();
            if (!client_management)
                return;
            const Window *client = workspace()->findToplevel(
                [wid] (const Window *w)->bool {
                    return w->window() ? w->window() == wid : w->frameId() == wid;
                }
            );
            if (!client) {
                const WindowState invalid = {
                    .pid = 0,
                    .windowId = 0
                };
                client_management->sendSpecificWindowState(invalid);
                return;
            }

            std::string name;
            if (!client->resourceName().isEmpty() || !client->resourceClass().isEmpty())
                name = QString(client->resourceName() + " " + client->resourceClass()).toStdString();
            const std::string uuid = client->internalId().toString().toStdString();
            WindowState state = {
                .pid = client->pid(),
                .windowId = wid,
                .resourceName = {0},
                .geometry = {
                    .x = client->frameGeometry().x(),
                    .y = client->frameGeometry().y(),
                    .width = client->frameGeometry().width(),
                    .height = client->frameGeometry().height()
                },
                .isMinimized = client->isMinimized(),
                .isFullScreen = client->isFullScreen(),
                .isActive = client->isActive(),
                .splitable = 0,
                .uuid = {0}
            };
            std::copy_n(name.begin(),
                    std::min(static_cast<decltype(name.length())>(sizeof(state.resourceName) - 1), name.length()),
                    state.resourceName);
            std::copy_n(uuid.begin(),
                    std::min(static_cast<decltype(uuid.length())>(sizeof(state.uuid) - 1), uuid.length()),
                    state.uuid);
            client_management->sendSpecificWindowState(state);
        }
    );

    auto activation = new KWaylandServer::XdgActivationV1Interface(m_display, this);
    auto init = [this, activation] {
        m_xdgActivationIntegration = new XdgActivationV1Integration(activation, this);
    };
    if (Workspace::self()) {
        init();
    } else {
        connect(static_cast<Application *>(qApp), &Application::workspaceCreated, this, init);
    }

    auto aboveLockscreen = new KWaylandServer::LockscreenOverlayV1Interface(m_display, this);
    connect(aboveLockscreen, &KWaylandServer::LockscreenOverlayV1Interface::allowRequested, this, [](SurfaceInterface *surface) {
        auto w = waylandServer()->findWindow(surface);
        if (!w) {
            return;
        }
        w->setLockScreenOverlay(true);
    });

    m_contentTypeManager = new KWaylandServer::ContentTypeManagerV1Interface(m_display, m_display);
    m_tearingControlInterface = new KWaylandServer::TearingControlManagerV1Interface(m_display, m_display);

    m_ddeShell = new DDEShellInterface(m_display, m_display);
    connect(m_ddeShell, &DDEShellInterface::shellSurfaceCreated,
        [this] (DDEShellSurfaceInterface *shellSurface) {
            if (XdgSurfaceWindow *client = findXdgSurfaceWindow(shellSurface->surface())) {
                client->installDDEShellSurface(shellSurface);
            }
            if (XdgSurfaceV6Window *client = findXdgSurfaceV6Window(shellSurface->surface())) {
                client->installDDEShellSurface(shellSurface);
            }
        }
    );

    m_strut = new StrutInterface(m_display, m_display);
    connect(m_strut, &StrutInterface::setStrut,
        [this] (SurfaceInterface *surface, struct deepinKwinStrut& strutArea) {
            if (Window *client = findWindow(surface)) {
                client->setStrut(strutArea);
                workspace()->updateClientArea();
            } else {
                qDebug("Client does not exist!!!");
            }
        }
    );

    m_ddeSeat = new DDESeatInterface(m_display, m_display);

    m_ddeSecurity = new DDESecurityInterface(m_display, m_display);
    m_seat->addSecurityInterface(m_ddeSecurity);

    m_ddeRestrict = new DDERestrictInterface(m_display, m_display);
    m_ddeKvm = new DDEKvmInterface(m_display, m_display);

    return true;
}

KWaylandServer::LinuxDmaBufV1ClientBufferIntegration *WaylandServer::linuxDmabuf()
{
    if (!m_linuxDmabuf) {
        m_linuxDmabuf = new LinuxDmaBufV1ClientBufferIntegration(m_display);
    }
    return m_linuxDmabuf;
}

SurfaceInterface *WaylandServer::findForeignTransientForSurface(SurfaceInterface *surface)
{
    return m_XdgForeign->transientFor(surface);
}

void WaylandServer::windowShown(Window *window)
{
    disconnect(window, &Window::windowShown, this, &WaylandServer::windowShown);
    Q_EMIT windowAdded(window);
}

void WaylandServer::initWorkspace()
{
    new KeyStateInterface(m_display, m_display);

    VirtualDesktopManager::self()->setVirtualDesktopManagement(m_virtualDesktopManagement);

    if (m_windowManagement) {
        connect(workspace(), &Workspace::showingDesktopChanged, this, [this](bool set) {
            using namespace KWaylandServer;
            m_windowManagement->setShowingDesktopState(set ? PlasmaWindowManagementInterface::ShowingDesktopState::Enabled : PlasmaWindowManagementInterface::ShowingDesktopState::Disabled);
        });

        connect(workspace(), &Workspace::workspaceInitialized, this, [this] {
            auto f = [this]() {
                QVector<quint32> ids;
                QVector<QString> uuids;
                for (Window *toplevel : workspace()->stackingOrder()) {
                    if (toplevel->windowManagementInterface()) {
                        ids << toplevel->windowManagementInterface()->internalId();
                        uuids << toplevel->windowManagementInterface()->uuid();
                    }
                }
                m_windowManagement->setStackingOrder(ids);
                m_windowManagement->setStackingOrderUuids(uuids);
            };
            f();
            connect(workspace(), &Workspace::stackingOrderChanged, this, f);
        });
    }

    const auto availableOutputs = kwinApp()->outputBackend()->outputs();
    for (Output *output : availableOutputs) {
        handleOutputAdded(output);
    }
    connect(kwinApp()->outputBackend(), &OutputBackend::outputAdded, this, &WaylandServer::handleOutputAdded);
    connect(kwinApp()->outputBackend(), &OutputBackend::outputRemoved, this, &WaylandServer::handleOutputRemoved);

    const auto outputs = workspace()->outputs();
    for (Output *output : outputs) {
        handleOutputEnabled(output);
    }
    connect(workspace(), &Workspace::outputAdded, this, &WaylandServer::handleOutputEnabled);
    connect(workspace(), &Workspace::outputRemoved, this, &WaylandServer::handleOutputDisabled);

    if (hasScreenLockerIntegration()) {
        initScreenLocker();
    }

    if (auto backend = qobject_cast<DrmBackend *>(kwinApp()->outputBackend())) {
        m_leaseManager = new KWaylandServer::DrmLeaseManagerV1(backend, m_display, m_display);
    }

    m_outputOrder = new KWaylandServer::OutputOrderV1Interface(m_display, m_display);
    m_outputOrder->setOutputOrder(workspace()->outputOrder());
    connect(workspace(), &Workspace::outputOrderChanged, m_outputOrder, [this]() {
        m_outputOrder->setOutputOrder(workspace()->outputOrder());
    });

    Q_EMIT initialized();
}

void WaylandServer::initScreenLocker()
{
#if KWIN_BUILD_SCREENLOCKER
    auto *screenLockerApp = ScreenLocker::KSldApp::self();

    ScreenLocker::KSldApp::self()->setGreeterEnvironment(kwinApp()->processStartupEnvironment());

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::aboutToLock, this, [this, screenLockerApp]() {
        if (m_screenLockerClientConnection) {
            // Already sent data to KScreenLocker.
            return;
        }
        int clientFd = createScreenLockerConnection();
        if (clientFd < 0) {
            return;
        }
        ScreenLocker::KSldApp::self()->setWaylandFd(clientFd);

        new LockScreenPresentationWatcher(this);

        const QVector<SeatInterface *> seatIfaces = m_display->seats();
        for (auto *seat : seatIfaces) {
            connect(seat, &KWaylandServer::SeatInterface::timestampChanged,
                    screenLockerApp, &ScreenLocker::KSldApp::userActivity);
        }
    });

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::unlocked, this, [this, screenLockerApp]() {
        if (m_screenLockerClientConnection) {
            m_screenLockerClientConnection->destroy();
            delete m_screenLockerClientConnection;
            m_screenLockerClientConnection = nullptr;
        }

        const QVector<SeatInterface *> seatIfaces = m_display->seats();
        for (auto *seat : seatIfaces) {
            disconnect(seat, &KWaylandServer::SeatInterface::timestampChanged,
                       screenLockerApp, &ScreenLocker::KSldApp::userActivity);
        }
        ScreenLocker::KSldApp::self()->setWaylandFd(-1);
    });

    connect(screenLockerApp, &ScreenLocker::KSldApp::lockStateChanged, this, &WaylandServer::lockStateChanged);

    ScreenLocker::KSldApp::self()->initialize();

    if (m_initFlags.testFlag(InitializationFlag::LockScreen)) {
        ScreenLocker::KSldApp::self()->lock(ScreenLocker::EstablishLock::Immediate);
    }
#endif
}

WaylandServer::SocketPairConnection WaylandServer::createConnection()
{
    SocketPairConnection ret;
    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_CORE) << "Could not create socket";
        return ret;
    }
    ret.connection = m_display->createClient(sx[0]);
    ret.fd = sx[1];
    return ret;
}

int WaylandServer::createScreenLockerConnection()
{
    const auto socket = createConnection();
    if (!socket.connection) {
        return -1;
    }
    m_screenLockerClientConnection = socket.connection;
    connect(m_screenLockerClientConnection, &KWaylandServer::ClientConnection::disconnected, this, [this]() {
        m_screenLockerClientConnection = nullptr;
    });
    return socket.fd;
}

int WaylandServer::createXWaylandConnection()
{
    const auto socket = createConnection();
    if (!socket.connection) {
        return -1;
    }
    m_xwaylandConnection = socket.connection;

    m_xwaylandConnection->setScaleOverride(kwinApp()->xwaylandScale());
    connect(kwinApp(), &Application::xwaylandScaleChanged, m_xwaylandConnection, [this]() {
        m_xwaylandConnection->setScaleOverride(kwinApp()->xwaylandScale());
    });

    return socket.fd;
}

void WaylandServer::destroyXWaylandConnection()
{
    if (!m_xwaylandConnection) {
        return;
    }
    m_xwaylandConnection->destroy();
    m_xwaylandConnection = nullptr;
}

int WaylandServer::createInputMethodConnection()
{
    const auto socket = createConnection();
    if (!socket.connection) {
        return -1;
    }
    m_inputMethodServerConnection = socket.connection;
    return socket.fd;
}

void WaylandServer::destroyInputMethodConnection()
{
    if (!m_inputMethodServerConnection) {
        return;
    }
    m_inputMethodServerConnection->destroy();
    m_inputMethodServerConnection = nullptr;
}

void WaylandServer::removeWindow(Window *c)
{
    m_windows.removeAll(c);
    if (c->readyForPainting()) {
        Q_EMIT windowRemoved(c);
    }
}

static Window *findWindowInList(const QList<Window *> &windows, quint32 id)
{
    auto it = std::find_if(windows.begin(), windows.end(), [id](Window *w) {
        return w->frameId() == id;
    });
    if (it == windows.end()) {
        return nullptr;
    }
    return *it;
}

static Window *findWindowInList(const QList<Window *> &windows, const KWaylandServer::SurfaceInterface *surface)
{
    auto it = std::find_if(windows.begin(), windows.end(), [surface](Window *w) {
        return w->surface() == surface;
    });
    if (it == windows.end()) {
        return nullptr;
    }
    return *it;
}

Window *WaylandServer::findWindow(quint32 id) const
{
    if (id == 0) {
        return nullptr;
    }
    if (Window *c = findWindowInList(m_windows, id)) {
        return c;
    }
    return nullptr;
}

Window *WaylandServer::findWindow(const KWaylandServer::SurfaceInterface *surface) const
{
    if (!surface) {
        return nullptr;
    }
    if (Window *c = findWindowInList(m_windows, surface)) {
        return c;
    }
    return nullptr;
}

XdgToplevelWindow *WaylandServer::findXdgToplevelWindow(SurfaceInterface *surface) const
{
    return qobject_cast<XdgToplevelWindow *>(findWindow(surface));
}

XdgSurfaceWindow *WaylandServer::findXdgSurfaceWindow(SurfaceInterface *surface) const
{
    return qobject_cast<XdgSurfaceWindow *>(findWindow(surface));
}

XdgToplevelV6Window *WaylandServer::findXdgToplevelV6Window(SurfaceInterface *surface) const
{
    return qobject_cast<XdgToplevelV6Window *>(findWindow(surface));
}

XdgSurfaceV6Window *WaylandServer::findXdgSurfaceV6Window(SurfaceInterface *surface) const
{
    return qobject_cast<XdgSurfaceV6Window *>(findWindow(surface));
}

quint32 WaylandServer::createWindowId(SurfaceInterface *surface)
{
    auto it = m_clientIds.constFind(surface->client());
    quint16 clientId = 0;
    if (it != m_clientIds.constEnd()) {
        clientId = it.value();
    } else {
        clientId = createClientId(surface->client());
    }
    Q_ASSERT(clientId != 0);
    quint32 id = clientId;
    // TODO: this does not prevent that two surfaces of same client get same id
    id = (id << 16) | (surface->id() & 0xFFFF);
    if (findWindow(id)) {
        qCWarning(KWIN_CORE) << "Invalid client windowId generated:" << id;
        return 0;
    }
    return id;
}

quint16 WaylandServer::createClientId(ClientConnection *c)
{
    auto ids = m_clientIds.values().toSet();
    quint16 id = 1;
    if (!ids.isEmpty()) {
        for (quint16 i = ids.count() + 1; i >= 1 ; i--) {
            if (!ids.contains(i)) {
                id = i;
                break;
            }
        }
    }
    Q_ASSERT(!ids.contains(id));
    m_clientIds.insert(c, id);
    connect(c, &ClientConnection::disconnected, this,
        [this] (ClientConnection *c) {
            m_clientIds.remove(c);
        }
    );
    return id;
}

bool WaylandServer::isScreenLocked() const
{
#if KWIN_BUILD_SCREENLOCKER
    if (!hasScreenLockerIntegration()) {
        return false;
    }
    return ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::Locked || ScreenLocker::KSldApp::self()->lockState() == ScreenLocker::KSldApp::AcquiringLock;
#else
    return false;
#endif
}

bool WaylandServer::hasScreenLockerIntegration() const
{
#if KWIN_BUILD_SCREENLOCKER
    return !m_initFlags.testFlag(InitializationFlag::NoLockScreenIntegration);
#else
    return false;
#endif
}

bool WaylandServer::hasGlobalShortcutSupport() const
{
    return !m_initFlags.testFlag(InitializationFlag::NoGlobalShortcuts);
}

bool WaylandServer::hasVtSwitchSupport() const
{
    return !m_initFlags.testFlag(InitializationFlag::NoVtSwitch);
}

static Window *findClientInList(const QList<Window*> &clients, const QByteArray &resource)
{
    auto it = std::find_if(clients.begin(), clients.end(),
        [resource] (Window *c) {
            return c->resourceClass() == resource;
        }
    );
    if (it == clients.end()) {
        return nullptr;
    }
    return *it;
}

bool WaylandServer::hasProhibitWindows() const
{
    QList<QByteArray> white_windows = m_ddeRestrict->clientWhitelists();
    if(white_windows.isEmpty())
        return true;

    for (int i = 0; i < white_windows.count(); i++) {
        QByteArray resource = white_windows.at(i);
        if(resource.isEmpty())
            continue;
        auto c = findClient(resource);
        if (c && c->isShown()) {
            return true;
        }
    }
    return false;
}

Window *WaylandServer::findClient(const QByteArray &resource) const
{
    if (resource.isEmpty()) {
        return nullptr;
    }
    if (Window *c = findClientInList(m_windows, resource)) {
        return c;
    }
    return nullptr;
    // if (Window *c = findClientInList(m_internalWindows, resource)) {
    //     return c;
    // }

    // return workspace()->findClient([resource](const Client *c) {
    //     return c->resourceName() == resource;
    // });
}

bool WaylandServer::isKeyboardShortcutsInhibited() const
{
    auto surface = seat()->focusedKeyboardSurface();
    if (surface) {
        auto inhibitor = keyboardShortcutsInhibitManager()->findInhibitor(surface, seat());
        if (inhibitor && inhibitor->isActive()) {
            return true;
        }
        if (m_xWaylandKeyboardGrabManager->hasGrab(surface, seat())) {
            return true;
        }
    }
    return false;
}

QString WaylandServer::socketName() const
{
    const QStringList socketNames = display()->socketNames();
    if (!socketNames.isEmpty()) {
        return socketNames.first();
    }
    return QString();
}

#if KWIN_BUILD_SCREENLOCKER
WaylandServer::LockScreenPresentationWatcher::LockScreenPresentationWatcher(WaylandServer *server)
{
    connect(server, &WaylandServer::windowAdded, this, [this](Window *window) {
        if (window->isLockScreen()) {
            // only signal lockScreenShown once all outputs have been presented at least once
            connect(window->output()->renderLoop(), &RenderLoop::framePresented, this, [this, windowGuard = QPointer(window)]() {
                // window might be destroyed before a frame is presented, so it's wrapped in QPointer
                if (windowGuard) {
                    m_signaledOutputs << windowGuard->output();
                    if (m_signaledOutputs.size() == workspace()->outputs().size()) {
                        ScreenLocker::KSldApp::self()->lockScreenShown();
                        delete this;
                    }
                }
            });
        }
    });
    QTimer::singleShot(1000, this, [this]() {
        ScreenLocker::KSldApp::self()->lockScreenShown();
        delete this;
    });
}
#endif
}
