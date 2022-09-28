/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// own
#include "workspace.h"
// kwin libs
#include <kwinglplatform.h>
// kwin
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif
#include "appmenu.h"
#include "atoms.h"
#include "client.h"
#include "composite.h"
#include "cursor.h"
#include "dbusinterface.h"
#include "deleted.h"
#include "effects.h"
#include "focuschain.h"
#include "group.h"
#include "input.h"
#include "logind.h"
#include "moving_client_x11_filter.h"
#include "killwindow.h"
#include "netinfo.h"
#include "outline.h"
#include "placement.h"
#include "rules.h"
#include "screenedge.h"
#include "screens.h"
#include "platform.h"
#include "scripting/scripting.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "unmanaged.h"
#include "useractions.h"
#include "virtualdesktops.h"
#include "shell_client.h"
#include "was_user_interaction_x11_filter.h"
#include "wayland_server.h"
#include "surface_interface.h"
#include "buffer_interface.h"
#include "xcbutils.h"
#include "main.h"
#include "decorations/decorationbridge.h"
// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KStartupInfo>
// Qt
#include <QtConcurrentRun>
#include <qscreen.h>

#define  ENABLE_DRAG_CONGTENT 1

#define DBUS_DEEPIN_WM_SERVICE "com.deepin.wm"
#define DBUS_DEEPIN_WM_OBJ "/com/deepin/wm"
#define DBUS_DEEPIN_WM_INTF "com.deepin.wm"

namespace KWin
{

extern int screen_number;
extern bool is_multihead;

ColorMapper::ColorMapper(QObject *parent)
    : QObject(parent)
    , m_default(defaultScreen()->default_colormap)
    , m_installed(defaultScreen()->default_colormap)
{
}

ColorMapper::~ColorMapper()
{
}

void ColorMapper::update()
{
    xcb_colormap_t cmap = m_default;
    if (Client *c = dynamic_cast<Client*>(Workspace::self()->activeClient())) {
        if (c->colormap() != XCB_COLORMAP_NONE) {
            cmap = c->colormap();
        }
    }
    if (cmap != m_installed) {
        xcb_install_colormap(connection(), cmap);
        m_installed = cmap;
    }
}

Workspace* Workspace::_self = 0;

Workspace::Workspace(const QString &sessionKey)
    : QObject(0)
    , m_compositor(NULL)
    // Unsorted
    , active_popup(NULL)
    , active_popup_client(NULL)
    , m_initialDesktop(1)
    , active_client(0)
    , last_active_client(0)
    , most_recently_raised(0)
    , movingClient(0)
    , delayfocus_client(0)
    , force_restacking(false)
    , showing_desktop(false)
    , showing_desktop_timestamp(-1U)
    , was_user_interaction(false)
    , session_saving(false)
    , block_focus(0)
    , m_userActionsMenu(new UserActionsMenu(this))
    , client_keys_dialog(NULL)
    , client_keys_client(NULL)
    , global_shortcuts_disabled_for_client(false)
    , global_shortcuts_disabled_by_user(false)
    , hot_keys_disabled_for_client(false)
    , gesture_disabled_for_client(false)
    , workspaceInit(true)
    , startup(0)
    , set_active_client_recursion(0)
    , block_stacking_updates(0)
{
    // If KWin was already running it saved its configuration after loosing the selection -> Reread
    QFuture<void> reparseConfigFuture = QtConcurrent::run(options, &Options::reparseConfiguration);

    ApplicationMenu::create(this);

    _self = this;

#ifdef KWIN_BUILD_ACTIVITIES
    Activities *activities = nullptr;
    if (kwinApp()->usesKActivities()) {
        activities = Activities::create(this);
    }
    if (activities) {
        connect(activities, SIGNAL(currentChanged(QString)), SLOT(updateCurrentActivity(QString)));
    }
#endif

    // PluginMgr needs access to the config file, so we need to wait for it for finishing
    reparseConfigFuture.waitForFinished();

    options->loadConfig();
    options->loadCompositingConfig(false);

    delayFocusTimer = 0;

    if (!sessionKey.isEmpty())
        loadSessionInfo(sessionKey);
    connect(qApp, &QGuiApplication::commitDataRequest, this, &Workspace::commitData);
    connect(qApp, &QGuiApplication::saveStateRequest, this, &Workspace::saveState);

    RuleBook::create(this)->load();

    ScreenEdges::create(this);

    // VirtualDesktopManager needs to be created prior to init shortcuts
    // and prior to TabBox, due to TabBox connecting to signals
    // actual initialization happens in init()
    VirtualDesktopManager::create(this);
    //dbus interface
    new VirtualDesktopManagerDBusInterface(VirtualDesktopManager::self());

#ifdef KWIN_BUILD_TABBOX
    // need to create the tabbox before compositing scene is setup
    TabBox::TabBox::create(this);
#endif

    if (Compositor::self()) {
        m_compositor = Compositor::self();
    } else {
        m_compositor = Compositor::create(this);
    }
    connect(this, &Workspace::currentDesktopChanged, m_compositor, &Compositor::addRepaintFull);
    connect(m_compositor, &QObject::destroyed, this, [this] { m_compositor = nullptr; });

    auto decorationBridge = Decoration::DecorationBridge::create(this);
    decorationBridge->init();
    connect(this, &Workspace::configChanged, decorationBridge, &Decoration::DecorationBridge::reconfigure);

    new DBusInterface(this);

    Outline::create(this);

    initShortcuts();

    init();
}

void Workspace::init()
{
    KSharedConfigPtr config = kwinApp()->config();
    kwinApp()->createScreens();
    Screens *screens = Screens::self();
    // get screen support
    connect(screens, SIGNAL(changed()), SLOT(desktopResized()));
    connect(screens, SIGNAL(changed()), SLOT(screensChanged()));
    screens->setConfig(config);
    screens->reconfigure();
    connect(options, SIGNAL(configChanged()), screens, SLOT(reconfigure()));
    ScreenEdges *screenEdges = ScreenEdges::self();
    screenEdges->setConfig(config);
    screenEdges->init();
    connect(options, SIGNAL(configChanged()), screenEdges, SLOT(reconfigure()));
    connect(VirtualDesktopManager::self(), SIGNAL(layoutChanged(int,int)), screenEdges, SLOT(updateLayout()));
    connect(this, &Workspace::clientActivated, screenEdges, &ScreenEdges::checkBlocking);

    FocusChain *focusChain = FocusChain::create(this);
    connect(this, &Workspace::clientRemoved, focusChain, &FocusChain::remove);
    connect(this, &Workspace::clientActivated, focusChain, &FocusChain::setActiveClient);
    connect(VirtualDesktopManager::self(), SIGNAL(countChanged(uint,uint)), focusChain, SLOT(resize(uint,uint)));
    connect(VirtualDesktopManager::self(), SIGNAL(currentChanged(uint,uint)), focusChain, SLOT(setCurrentDesktop(uint,uint)));
    connect(options, SIGNAL(separateScreenFocusChanged(bool)), focusChain, SLOT(setSeparateScreenFocus(bool)));
    focusChain->setSeparateScreenFocus(options->isSeparateScreenFocus());

    // create VirtualDesktopManager and perform dependency injection
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(vds, &VirtualDesktopManager::desktopRemoved, this,
        [this](KWin::VirtualDesktop *desktop) {
            //Wayland
            if (kwinApp()->operationMode() == Application::OperationModeWaylandOnly ||
                kwinApp()->operationMode() == Application::OperationModeXwayland) {
                for (auto it = m_allClients.constBegin(); it != m_allClients.constEnd(); ++it) {
                    if (!(*it)->desktops().contains(desktop)) {
                        continue;
                    }
                    if ((*it)->desktops().count() > 1) {
                        (*it)->leaveDesktop(desktop);
                    } else {
                        sendClientToDesktop(*it, qMin(desktop->x11DesktopNumber(), VirtualDesktopManager::self()->count()), true);
                    }
                }
            //X11
            } else {
                for (auto it = m_allClients.constBegin(); it != m_allClients.constEnd(); ++it) {
                    if (!(*it)->isOnAllDesktops() && ((*it)->desktop() > static_cast<int>(VirtualDesktopManager::self()->count()))) {
                        sendClientToDesktop(*it, VirtualDesktopManager::self()->count(), true);
                    }
                }
            }
        }
    );

    connect(vds, SIGNAL(countChanged(uint,uint)), SLOT(slotDesktopCountChanged(uint,uint)));
    connect(vds, SIGNAL(currentChanged(uint,uint)), SLOT(slotCurrentDesktopChanged(uint,uint)));
    vds->setNavigationWrappingAround(options->isRollOverDesktops());
    connect(options, SIGNAL(rollOverDesktopsChanged(bool)), vds, SLOT(setNavigationWrappingAround(bool)));
    vds->setConfig(config);

    // Now we know how many desktops we'll have, thus we initialize the positioning object
    Placement::create(this);

    // positioning object needs to be created before the virtual desktops are loaded.
    vds->load();
    vds->updateLayout();
    //makes sure any autogenerated id is saved, necessary as in case of xwayland, load will be called 2 times
    // load is needed to be called again when starting xwayalnd to sync to RootInfo, see BUG 385260
    vds->save();

    if (config->group("Workspace").hasKey("DraggingWithContent"))
        m_ClientDragingWithContent = config->group("Workspace").readEntry("DraggingWithContent") == "true" ? true : false;

    m_initialDesktop = config->group("Workspace").readEntry("CurrentDesktop",1);
    if (!VirtualDesktopManager::self()->setCurrent(m_initialDesktop))
        VirtualDesktopManager::self()->setCurrent(1);

    reconfigureTimer.setSingleShot(true);
    updateToolWindowsTimer.setSingleShot(true);

    connect(&reconfigureTimer, SIGNAL(timeout()), this, SLOT(slotReconfigure()));
    connect(&updateToolWindowsTimer, SIGNAL(timeout()), this, SLOT(slotUpdateToolWindows()));

    // TODO: do we really need to reconfigure everything when fonts change?
    // maybe just reconfigure the decorations? Move this into libkdecoration?
    QDBusConnection::sessionBus().connect(QString(),
                                          QStringLiteral("/KDEPlatformTheme"),
                                          QStringLiteral("org.kde.KDEPlatformTheme"),
                                          QStringLiteral("refreshFonts"),
                                          this, SLOT(reconfigure()));

    active_client = NULL;

    initWithX11();

    Scripting::create(this);
    connect(this, &Workspace::windowStateChanged, this, &Workspace::updateWindowStates);

    if (auto w = waylandServer()) {
        connect(w, &WaylandServer::shellClientAdded, this,
            [this] (ShellClient *c) {
                setupClientConnections(c);
                c->updateDecoration(false);
                updateClientLayer(c);
                if (!c->isInternal()) {
                    QRect area = clientArea(PlacementArea, Screens::self()->current(), c->desktop());
                    bool placementDone = false;
                    if (c->isInitialPositionSet()) {
                        placementDone = true;
                    }
                    if (c->isFullScreen()) {
                        placementDone = true;
                    }
                    if (!placementDone) {
                        c->placeIn(area);
                    }
                    m_allClients.append(c);
                    if (!unconstrained_stacking_order.contains(c))
                        unconstrained_stacking_order.append(c);   // Raise if it hasn't got any stacking position yet
                    if (!stacking_order.contains(c))    // It'll be updated later, and updateToolWindows() requires
                        stacking_order.append(c);      // c to be in stacking_order
                }
                markXStackingOrderAsDirty();
                updateStackingOrder(true);
                updateClientArea();
                if (c->wantsInput() && !c->isMinimized()) {
                    activateClient(c);
                }
                connect(c, &ShellClient::windowShown, this,
                    [this, c] {
                        updateClientLayer(c);
                        // TODO: when else should we send the client through placement?
                        if (c->hasTransientPlacementHint()) {
                            QRect area = clientArea(PlacementArea, Screens::self()->current(), c->desktop());
                            c->placeIn(area);
                        }
                        markXStackingOrderAsDirty();
                        updateStackingOrder(true);
                        updateClientArea();
                        if (c->wantsInput()) {
                            activateClient(c);
                        }
                    }
                );
                connect(c, &ShellClient::windowHidden, this,
                    [this] {
                        markXStackingOrderAsDirty();
                        updateStackingOrder(true);
                        updateClientArea();
                    }
                );
                emit windowStateChanged();
                emit shellClientAdded(c);
            }
        );
        connect(w, &WaylandServer::shellClientRemoved, this,
            [this] (ShellClient *c) {
                m_allClients.removeAll(c);
                if (c == most_recently_raised) {
                    most_recently_raised = nullptr;
                }
                if (c == delayfocus_client) {
                    cancelDelayFocus();
                }
                if (c == last_active_client) {
                    last_active_client = nullptr;
                }
                if (client_keys_client == c) {
                    setupWindowShortcutDone(false);
                }
                if (!c->shortcut().isEmpty()) {
                    c->setShortcut(QString());   // Remove from client_keys
                }
                clientHidden(c);
                emit clientRemoved(c);
                markXStackingOrderAsDirty();
                updateStackingOrder(true);
                updateClientArea();
                emit windowStateChanged();
            }
        );
    }

    // SELI TODO: This won't work with unreasonable focus policies,
    // and maybe in rare cases also if the selected client doesn't
    // want focus
    workspaceInit = false;

    // broadcast that Workspace is ready, but first process all events.
    QMetaObject::invokeMethod(this, "workspaceInitialized", Qt::QueuedConnection);

    QDBusConnection::sessionBus().connect(KWinDBusService, KWinDBusPath, KWinDBusPropertyInterface,
                                          "PropertiesChanged", this, SLOT(qtactivecolorChanged()));

    // TODO: ungrabXServer()
}

void Workspace::updateWindowStates()
{
    qDeleteAll(m_windowStates);
    m_windowStates.clear();

    foreach (Toplevel * win, stacking_order) {
        AbstractClient *client = qobject_cast<AbstractClient*>(win);
        if (!client || m_allClients.indexOf(client) < 0)
            continue;
        auto windowState = new WindowState();

        windowState->pid                = client->pid();
        windowState->windowId           = client->windowId();
        windowState->isMinimized        = client->isMinimized();
        windowState->isFullScreen       = client->isFullScreen();
        windowState->isActive           = client->isActive();
        windowState->geometry.x         = client->geometry().x();
        windowState->geometry.y         = client->geometry().y();
        windowState->geometry.width     = client->geometry().width();
        windowState->geometry.height    = client->geometry().height();
        memcpy(windowState->resourceName, client->resourceName().data(), client->resourceName().size());

        m_windowStates.append(windowState);
    }

    if (waylandServer()) {
        auto clientmanagement = waylandServer()->clientManagement();
        if (clientmanagement) {
            clientmanagement->setWindowStates(m_windowStates);
        }
    }
}

void Workspace::captureWindowImage(int windowId, wl_resource *buffer)
{
    KWayland::Server::ClientManagementInterface *clientmanagement = nullptr;
    if (waylandServer()) {
        clientmanagement = waylandServer()->clientManagement();
    }
    if (!clientmanagement) {
        return;
    }

    foreach (Toplevel *win, stacking_order) {
        AbstractClient *client = qobject_cast<AbstractClient *>(win);
        if (!client || m_allClients.indexOf(client) < 0) {
            continue;
        }

        if (windowId == client->windowId()) {
            clientmanagement->sendWindowCaption(windowId, buffer, client->surface());
            return;
        }
    }

    clientmanagement->sendWindowCaption(windowId, buffer, nullptr);
}

void Workspace::initWithX11()
{
    if (!kwinApp()->x11Connection()) {
        connect(kwinApp(), &Application::x11ConnectionChanged, this, &Workspace::initWithX11, Qt::UniqueConnection);
        return;
    }
    disconnect(kwinApp(), &Application::x11ConnectionChanged, this, &Workspace::initWithX11);

    atoms->retrieveHelpers();

    // first initialize the extensions
    Xcb::Extensions::self();
    ColorMapper *colormaps = new ColorMapper(this);
    connect(this, &Workspace::clientActivated, colormaps, &ColorMapper::update);

    // Call this before XSelectInput() on the root window
    startup = new KStartupInfo(
        KStartupInfo::DisableKWinModule | KStartupInfo::AnnounceSilenceChanges, this);

    // Select windowmanager privileges
    selectWmInputEventMask();

    // Compatibility
    int32_t data = 1;

    xcb_change_property(connection(), XCB_PROP_MODE_APPEND, rootWindow(), atoms->kwin_running,
                        atoms->kwin_running, 32, 1, &data);

    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        m_wasUserInteractionFilter.reset(new WasUserInteractionX11Filter);
        m_movingClientFilter.reset(new MovingClientX11Filter);
    }
    updateXTime(); // Needed for proper initialization of user_time in Client ctor

    const uint32_t nullFocusValues[] = {true};
    m_nullFocus.reset(new Xcb::Window(QRect(-1, -1, 1, 1), XCB_WINDOW_CLASS_INPUT_ONLY, XCB_CW_OVERRIDE_REDIRECT, nullFocusValues));
    m_nullFocus->map();

    RootInfo *rootInfo = RootInfo::create();
    const auto vds = VirtualDesktopManager::self();
    vds->setRootInfo(rootInfo);
    // load again to sync to RootInfo, see BUG 385260
    vds->load();
    vds->updateRootInfo();
    rootInfo->setCurrentDesktop(vds->currentDesktop()->x11DesktopNumber());

    // TODO: only in X11 mode
    // Extra NETRootInfo instance in Client mode is needed to get the values of the properties
    NETRootInfo client_info(connection(), NET::ActiveWindow | NET::CurrentDesktop);
    if (!qApp->isSessionRestored()) {
        m_initialDesktop = client_info.currentDesktop();
        vds->setCurrent(m_initialDesktop);
    }

    // TODO: better value
    rootInfo->setActiveWindow(None);
    focusToNull();

    if (!qApp->isSessionRestored())
        ++block_focus; // Because it will be set below

    {
        // Begin updates blocker block
        StackingUpdatesBlocker blocker(this);

        Xcb::Tree tree(rootWindow());
        xcb_window_t *wins = xcb_query_tree_children(tree.data());

        QVector<Xcb::WindowAttributes> windowAttributes(tree->children_len);
        QVector<Xcb::WindowGeometry> windowGeometries(tree->children_len);

        // Request the attributes and geometries of all toplevel windows
        for (int i = 0; i < tree->children_len; i++) {
            windowAttributes[i] = Xcb::WindowAttributes(wins[i]);
            windowGeometries[i] = Xcb::WindowGeometry(wins[i]);
        }

        // Get the replies
        for (int i = 0; i < tree->children_len; i++) {
            Xcb::WindowAttributes attr(windowAttributes.at(i));

            if (attr.isNull()) {
                continue;
            }

            if (attr->override_redirect) {
                if (attr->map_state == XCB_MAP_STATE_VIEWABLE &&
                    attr->_class != XCB_WINDOW_CLASS_INPUT_ONLY)
                    // ### This will request the attributes again
                    createUnmanaged(wins[i]);
            } else if (attr->map_state != XCB_MAP_STATE_UNMAPPED) {
                if (Application::wasCrash()) {
                    fixPositionAfterCrash(wins[i], windowGeometries.at(i).data());
                }

                // ### This will request the attributes again
                createClient(wins[i], true);
            }
        }

        // Propagate clients, will really happen at the end of the updates blocker block
        updateStackingOrder(true);

        saveOldScreenSizes();
        updateClientArea();

        // NETWM spec says we have to set it to (0,0) if we don't support it
        NETPoint* viewports = new NETPoint[VirtualDesktopManager::self()->count()];
        rootInfo->setDesktopViewport(VirtualDesktopManager::self()->count(), *viewports);
        delete[] viewports;
        QRect geom;
        for (int i = 0; i < screens()->count(); i++) {
            geom |= screens()->geometry(i);
        }
        NETSize desktop_geometry;
        desktop_geometry.width = geom.width();
        desktop_geometry.height = geom.height();
        rootInfo->setDesktopGeometry(desktop_geometry);
        setShowingDesktop(false);
        setPreviewClientList({});

    } // End updates blocker block

    // TODO: only on X11?
    AbstractClient* new_active_client = nullptr;
    if (!qApp->isSessionRestored()) {
        --block_focus;
        new_active_client = findClient(Predicate::WindowMatch, client_info.activeWindow());
    }
    if (new_active_client == NULL
            && activeClient() == NULL && should_get_focus.count() == 0) {
        // No client activated in manage()
        if (new_active_client == NULL)
            new_active_client = topClientOnDesktop(VirtualDesktopManager::self()->current(), -1);
        if (new_active_client == NULL && !desktops.isEmpty())
            new_active_client = findDesktop(true, VirtualDesktopManager::self()->current());
    }
    if (new_active_client != NULL)
        activateClient(new_active_client);
}

Workspace::~Workspace()
{
    blockStackingUpdates(true);

    // TODO: grabXServer();

    // Use stacking_order, so that kwin --replace keeps stacking order
    const ToplevelList stack = stacking_order;
    // "mutex" the stackingorder, since anything trying to access it from now on will find
    // many dangeling pointers and crash
    stacking_order.clear();

    for (ToplevelList::const_iterator it = stack.constBegin(), end = stack.constEnd(); it != end; ++it) {
        Client *c = qobject_cast<Client*>(const_cast<Toplevel*>(*it));
        if (!c) {
            continue;
        }
        // Only release the window
        c->releaseWindow(true);
        // No removeClient() is called, it does more than just removing.
        // However, remove from some lists to e.g. prevent performTransiencyCheck()
        // from crashing.
        clients.removeAll(c);
        m_allClients.removeAll(c);
        desktops.removeAll(c);
        emit windowStateChanged();
    }
    Client::cleanupX11();
    for (UnmanagedList::iterator it = unmanaged.begin(), end = unmanaged.end(); it != end; ++it)
        (*it)->release(ReleaseReason::KWinShutsDown);

    if (auto c = kwinApp()->x11Connection()) {
        xcb_delete_property(c, kwinApp()->x11RootWindow(), atoms->kwin_running);
    }

    for (auto it = deleted.begin(); it != deleted.end();) {
        emit deletedRemoved(*it);
        it = deleted.erase(it);
    }

    delete RuleBook::self();
    kwinApp()->config()->sync();

    RootInfo::destroy();
    delete startup;
    delete Placement::self();
    delete client_keys_dialog;
    foreach (SessionInfo * s, session)
    delete s;

    qDeleteAll(m_windowStates);
    m_windowStates.clear();

    // TODO: ungrabXServer();

    Xcb::Extensions::destroy();
    _self = 0;
    m_windowPropertyMap.clear();
}

void Workspace::setupClientConnections(AbstractClient *c)
{
    connect(c, &Toplevel::needsRepaint, m_compositor, &Compositor::scheduleRepaint);
    connect(c, &AbstractClient::desktopPresenceChanged, this, &Workspace::desktopPresenceChanged);
    connect(c, &AbstractClient::minimizedChanged, this, std::bind(&Workspace::clientMinimizedChanged, this, c));
}

Client* Workspace::createClient(xcb_window_t w, bool is_mapped)
{
    StackingUpdatesBlocker blocker(this);
    Client* c = new Client();
    setupClientConnections(c);
    connect(c, SIGNAL(blockingCompositingChanged(KWin::Client*)), m_compositor, SLOT(updateCompositeBlocking(KWin::Client*)));
    connect(c, SIGNAL(clientFullScreenSet(KWin::Client*,bool,bool)), ScreenEdges::self(), SIGNAL(checkBlocking()));
    if (!c->manage(w, is_mapped)) {
        Client::deleteClient(c);
        return NULL;
    }
    addClient(c);
    if (c->checkClientAllowToTile()) {
        setWinSplitState(c);
    }
    emit windowStateChanged();
    if (c->checkClientAllowToTile()) {
        setWinSplitState(c);
    }
    return c;
}

Unmanaged* Workspace::createUnmanaged(xcb_window_t w)
{
    if (m_compositor && m_compositor->checkForOverlayWindow(w))
        return NULL;
    Unmanaged* c = new Unmanaged();
    if (!c->track(w)) {
        Unmanaged::deleteUnmanaged(c);
        return NULL;
    }
    connect(c, SIGNAL(needsRepaint()), m_compositor, SLOT(scheduleRepaint()));
    if (c->fetchWindowForLockScreen()) {
        // The focusToNull used to cancel context menus, etc.
        focusToNull();
        foreach (auto u,unmanaged) {
            if (u->isKeepAbove() && !u->fetchWindowForLockScreen()) {
                if (u->resourceName() != "deepin-watermark") {
                    xcb_unmap_window(connection(),u->window());
                }
            }
        }
        if (compositing()) {
            emit effects->closeEffect(true);
        }
    }
    addUnmanaged(c);
    emit unmanagedAdded(c);
    emit windowStateChanged();
    return c;
}

void Workspace::addClient(Client* c)
{
    Group* grp = findGroup(c->window());

    emit clientAdded(c);

    if (grp != NULL)
        grp->gotLeader(c);

    if (c->isDesktop()) {
        desktops.append(c);
        if (active_client == NULL && should_get_focus.isEmpty() && c->isOnCurrentDesktop())
            requestFocus(c);   // TODO: Make sure desktop is active after startup if there's no other window active
    } else {
        FocusChain::self()->update(c, FocusChain::Update);
        clients.append(c);
        m_allClients.append(c);
    }
    if (!unconstrained_stacking_order.contains(c))
        unconstrained_stacking_order.append(c);   // Raise if it hasn't got any stacking position yet
    if (!stacking_order.contains(c))    // It'll be updated later, and updateToolWindows() requires
        stacking_order.append(c);      // c to be in stacking_order
    markXStackingOrderAsDirty();
    updateClientArea(); // This cannot be in manage(), because the client got added only now
    updateClientLayer(c);
    if (c->isDesktop()) {
        raiseClient(c);
        // If there's no active client, make this desktop the active one
        if (activeClient() == NULL && should_get_focus.count() == 0)
            activateClient(findDesktop(true, VirtualDesktopManager::self()->current()));
    }
    c->checkActiveModal();
    checkTransients(c->window());   // SELI TODO: Does this really belong here?
    updateStackingOrder(true);   // Propagate new client
    if (c->isUtility() || c->isMenu() || c->isToolbar())
        updateToolWindows(true);
#ifdef KWIN_BUILD_TABBOX
    if (TabBox::TabBox::self()->isDisplayed())
        TabBox::TabBox::self()->reset(true);
#endif
}

void Workspace::addUnmanaged(Unmanaged* c)
{
    unmanaged.append(c);
    markXStackingOrderAsDirty();
}

/**
 * Destroys the client \a c
 */
void Workspace::removeClient(Client* c)
{
    if (c == active_popup_client)
        closeActivePopup();
    if (m_userActionsMenu->isMenuClient(c)) {
        m_userActionsMenu->close();
    }

    c->untab(QRect(), true);

    if (client_keys_client == c)
        setupWindowShortcutDone(false);
    if (!c->shortcut().isEmpty()) {
        c->setShortcut(QString());   // Remove from client_keys
        clientShortcutUpdated(c);   // Needed, since this is otherwise delayed by setShortcut() and wouldn't run
    }

#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox = TabBox::TabBox::self();
    if (tabBox->isDisplayed() && tabBox->currentClient() == c)
        tabBox->nextPrev(true);
#endif

    Q_ASSERT(clients.contains(c) || desktops.contains(c));
    // TODO: if marked client is removed, notify the marked list
    clients.removeAll(c);
    m_allClients.removeAll(c);
    desktops.removeAll(c);
    markXStackingOrderAsDirty();
    attention_chain.removeAll(c);
    Group* group = findGroup(c->window());
    if (group != NULL)
        group->lostLeader();

    if (c == most_recently_raised)
        most_recently_raised = 0;
    should_get_focus.removeAll(c);
    Q_ASSERT(c != active_client);
    if (c == last_active_client)
        last_active_client = 0;
    if (c == delayfocus_client)
        cancelDelayFocus();

    emit clientRemoved(c);
    updateStackingOrder(true);
    emit windowStateChanged();

#ifdef KWIN_BUILD_TABBOX
    if (tabBox->isDisplayed())
        tabBox->reset(true);
#endif

    updateClientArea();
}

void Workspace::removeUnmanaged(Unmanaged* c)
{
    assert(unmanaged.contains(c));
    unmanaged.removeAll(c);
    emit unmanagedRemoved(c);
    markXStackingOrderAsDirty();
    emit windowStateChanged();
}

void Workspace::addDeleted(Deleted* c, Toplevel *orig)
{
    assert(!deleted.contains(c));
    deleted.append(c);
    const int unconstraintedIndex = unconstrained_stacking_order.indexOf(orig);
    if (unconstraintedIndex != -1) {
        unconstrained_stacking_order.replace(unconstraintedIndex, c);
    } else {
        unconstrained_stacking_order.append(c);
    }
    const int index = stacking_order.indexOf(orig);
    if (index != -1) {
        stacking_order.replace(index, c);
    } else {
        stacking_order.append(c);
    }
    markXStackingOrderAsDirty();
    connect(c, SIGNAL(needsRepaint()), m_compositor, SLOT(scheduleRepaint()));
}

void Workspace::removeDeleted(Deleted* c)
{
    assert(deleted.contains(c));
    emit deletedRemoved(c);
    deleted.removeAll(c);
    unconstrained_stacking_order.removeAll(c);
    stacking_order.removeAll(c);
    markXStackingOrderAsDirty();
    if (c->wasClient() && m_compositor) {
        m_compositor->updateCompositeBlocking();
    }
}

void Workspace::updateToolWindows(bool also_hide)
{
    // TODO: What if Client's transiency/group changes? should this be called too? (I'm paranoid, am I not?)
    if (!options->isHideUtilityWindowsForInactive()) {
        for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it)
            if (!(*it)->tabGroup() || (*it)->tabGroup()->current() == *it)
                (*it)->hideClient(false);
        return;
    }
    const Group* group = nullptr;
    auto client = active_client;
    // Go up in transiency hiearchy, if the top is found, only tool transients for the top mainwindow
    // will be shown; if a group transient is group, all tools in the group will be shown
    while (client != nullptr) {
        if (!client->isTransient())
            break;
        if (client->groupTransient()) {
            group = client->group();
            break;
        }
        client = client->transientFor();
    }
    // Use stacking order only to reduce flicker, it doesn't matter if block_stacking_updates == 0,
    // I.e. if it's not up to date

    // SELI TODO: But maybe it should - what if a new client has been added that's not in stacking order yet?
    QVector<AbstractClient*> to_show, to_hide;
    for (ToplevelList::ConstIterator it = stacking_order.constBegin();
            it != stacking_order.constEnd();
            ++it) {
        auto c = qobject_cast<AbstractClient*>(*it);
        if (!c) {
            continue;
        }
        if (c->isUtility() || c->isMenu() || c->isToolbar()) {
            bool show = true;
            if (!c->isTransient()) {
                if (!c->group() || c->group()->members().count() == 1)   // Has its own group, keep always visible
                    show = true;
                else if (client != NULL && c->group() == client->group())
                    show = true;
                else
                    show = false;
            } else {
                if (group != NULL && c->group() == group)
                    show = true;
                else if (client != NULL && client->hasTransient(c, true))
                    show = true;
                else
                    show = false;
            }
            if (!show && also_hide) {
                const auto mainclients = c->mainClients();
                // Don't hide utility windows which are standalone(?) or
                // have e.g. kicker as mainwindow
                if (mainclients.isEmpty())
                    show = true;
                for (auto it2 = mainclients.constBegin();
                        it2 != mainclients.constEnd();
                        ++it2) {
                    if ((*it2)->isSpecialWindow())
                        show = true;
                }
                if (!show)
                    to_hide.append(c);
            }
            if (show)
                to_show.append(c);
        }
    } // First show new ones, then hide
    for (int i = to_show.size() - 1;
            i >= 0;
            --i)  // From topmost
        // TODO: Since this is in stacking order, the order of taskbar entries changes :(
        to_show.at(i)->hideClient(false);
    if (also_hide) {
        for (auto it = to_hide.constBegin();
                it != to_hide.constEnd();
                ++it)  // From bottommost
            (*it)->hideClient(true);
        updateToolWindowsTimer.stop();
    } else // setActiveClient() is after called with NULL client, quickly followed
        // by setting a new client, which would result in flickering
        resetUpdateToolWindowsTimer();
}


void Workspace::resetUpdateToolWindowsTimer()
{
    updateToolWindowsTimer.start(200);
}

void Workspace::slotUpdateToolWindows()
{
    updateToolWindows(true);
}

void Workspace::slotReloadConfig()
{
    reconfigure();
}

void Workspace::reconfigure()
{
#ifdef __aarch64__
    // delay may cause kwin to fail due to some unknown reason
    reconfigureTimer.start(0);
#else
    reconfigureTimer.start(200);
#endif
}

/**
 * Reread settings
 */

void Workspace::slotReconfigure()
{
    qCDebug(KWIN_CORE) << "Workspace::slotReconfigure()";
    reconfigureTimer.stop();

    bool borderlessMaximizedWindows = options->borderlessMaximizedWindows();

    kwinApp()->config()->reparseConfiguration();
    options->updateSettings();


    m_ClientDragingWithContent = kwinApp()->config()->group("Workspace").readEntry("DraggingWithContent") == "true" ? true : false;


    emit configChanged();
    m_userActionsMenu->discard();
    updateToolWindows(true);

    RuleBook::self()->load();
    for (auto it = m_allClients.begin();
            it != m_allClients.end();
            ++it) {
        (*it)->setupWindowRules(true);
        (*it)->applyWindowRules();
        RuleBook::self()->discardUsed(*it, false);
    }

    if (borderlessMaximizedWindows != options->borderlessMaximizedWindows() &&
            !options->borderlessMaximizedWindows()) {
        // in case borderless maximized windows option changed and new option
        // is to have borders, we need to unset the borders for all maximized windows
        for (auto it = m_allClients.begin();
                it != m_allClients.end();
                ++it) {
            if ((*it)->maximizeMode() == MaximizeFull)
                (*it)->checkNoBorder();
        }
    }
}

void Workspace::slotCurrentDesktopChanged(uint oldDesktop, uint newDesktop)
{
    closeActivePopup();
    ++block_focus;
    StackingUpdatesBlocker blocker(this);
    updateClientVisibilityOnDesktopChange(newDesktop);
    // Restore the focus on this desktop
    --block_focus;

    activateClientOnNewDesktop(newDesktop);
    updateSplitOutlineState(oldDesktop, newDesktop);
    kwinApp()->config()->group("Workspace").writeEntry("CurrentDesktop", newDesktop);
    kwinApp()->config()->sync();
    emit currentDesktopChanged(oldDesktop, movingClient);
}

void Workspace::updateClientVisibilityOnDesktopChange(uint newDesktop)
{
    for (ToplevelList::ConstIterator it = stacking_order.constBegin();
            it != stacking_order.constEnd();
            ++it) {
        Client *c = qobject_cast<Client*>(*it);
        if (!c) {
            continue;
        }
        if (!c->isOnDesktop(newDesktop) && c != movingClient && c->isOnCurrentActivity()) {
            (c)->updateVisibility();
        }
    }
    // Now propagate the change, after hiding, before showing
    if (rootInfo()) {
        rootInfo()->setCurrentDesktop(VirtualDesktopManager::self()->current());
    }

    if (movingClient && !movingClient->isOnDesktop(newDesktop)) {
        movingClient->setDesktop(newDesktop);
    }

    for (int i = stacking_order.size() - 1; i >= 0 ; --i) {
        Client *c = qobject_cast<Client*>(stacking_order.at(i));
        if (!c) {
            continue;
        }
        if (c->isOnDesktop(newDesktop) && c->isOnCurrentActivity())
            c->updateVisibility();
    }
    if (showingDesktop())   // Do this only after desktop change to avoid flicker
        setShowingDesktop(false);

    setPreviewClientList({});
}

void Workspace::activateClientOnNewDesktop(uint desktop)
{
    AbstractClient* c = NULL;
    if (options->focusPolicyIsReasonable()) {
        c = findClientToActivateOnDesktop(desktop);
    }
    // If "unreasonable focus policy" and active_client is on_all_desktops and
    // under mouse (Hence == old_active_client), conserve focus.
    // (Thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if (active_client && active_client->isShown(true) && active_client->isOnCurrentDesktop())
        c = active_client;

    if (c == NULL && !desktops.isEmpty())
        c = findDesktop(true, desktop);

    if (c != active_client)
        setActiveClient(NULL);

    if (c)
        requestFocus(c);
    else if (!desktops.isEmpty())
        requestFocus(findDesktop(true, desktop));
    else
        focusToNull();
}

AbstractClient *Workspace::findClientToActivateOnDesktop(uint desktop)
{
    if (movingClient != NULL && active_client == movingClient &&
        FocusChain::self()->contains(active_client, desktop) &&
        active_client->isShown(true) && active_client->isOnCurrentDesktop()) {
        // A requestFocus call will fail, as the client is already active
        return active_client;
    }
    // from actiavtion.cpp
    if (options->isNextFocusPrefersMouse()) {
        ToplevelList::const_iterator it = stackingOrder().constEnd();
        while (it != stackingOrder().constBegin()) {
            Client *client = qobject_cast<Client*>(*(--it));
            if (!client) {
                continue;
            }

            if (!(client->isShown(false) && client->isOnDesktop(desktop) &&
                client->isOnCurrentActivity() && client->isOnActiveScreen()))
                continue;

            if (client->geometry().contains(Cursor::pos())) {
                if (!client->isDesktop())
                    return client;
            break; // unconditional break  - we do not pass the focus to some client below an unusable one
            }
        }
    }
    return FocusChain::self()->getForActivation(desktop);
}

/**
 * Updates the current activity when it changes
 * do *not* call this directly; it does not set the activity.
 *
 * Shows/Hides windows according to the stacking order
 */

void Workspace::updateCurrentActivity(const QString &new_activity)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return;
    }
    //closeActivePopup();
    ++block_focus;
    // TODO: Q_ASSERT( block_stacking_updates == 0 ); // Make sure stacking_order is up to date
    StackingUpdatesBlocker blocker(this);

    // Optimized Desktop switching: unmapping done from back to front
    // mapping done from front to back => less exposure events
    //Notify::raise((Notify::Event) (Notify::DesktopChange+new_desktop));

    for (ToplevelList::ConstIterator it = stacking_order.constBegin();
            it != stacking_order.constEnd();
            ++it) {
        Client *c = qobject_cast<Client*>(*it);
        if (!c) {
            continue;
        }
        if (!c->isOnActivity(new_activity) && c != movingClient && c->isOnCurrentDesktop()) {
            c->updateVisibility();
        }
    }

    // Now propagate the change, after hiding, before showing
    //rootInfo->setCurrentDesktop( currentDesktop() );

    /* TODO someday enable dragging windows to other activities
    if ( movingClient && !movingClient->isOnDesktop( new_desktop ))
        {
        movingClient->setDesktop( new_desktop );
        */

    for (int i = stacking_order.size() - 1; i >= 0 ; --i) {
        Client *c = qobject_cast<Client*>(stacking_order.at(i));
        if (!c) {
            continue;
        }
        if (c->isOnActivity(new_activity))
            c->updateVisibility();
    }

    //FIXME not sure if I should do this either
    if (showingDesktop())   // Do this only after desktop change to avoid flicker
        setShowingDesktop(false);

    setPreviewClientList({});

    // Restore the focus on this desktop
    --block_focus;
    AbstractClient* c = 0;

    //FIXME below here is a lot of focuschain stuff, probably all wrong now
    if (options->focusPolicyIsReasonable()) {
        // Search in focus chain
        c = FocusChain::self()->getForActivation(VirtualDesktopManager::self()->current());
    }
    // If "unreasonable focus policy" and active_client is on_all_desktops and
    // under mouse (Hence == old_active_client), conserve focus.
    // (Thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if (active_client && active_client->isShown(true) && active_client->isOnCurrentDesktop() && active_client->isOnCurrentActivity())
        c = active_client;

    if (c == NULL && !desktops.isEmpty())
        c = findDesktop(true, VirtualDesktopManager::self()->current());

    if (c != active_client)
        setActiveClient(NULL);

    if (c)
        requestFocus(c);
    else if (!desktops.isEmpty())
        requestFocus(findDesktop(true, VirtualDesktopManager::self()->current()));
    else
        focusToNull();

    // Not for the very first time, only if something changed and there are more than 1 desktops

    //if ( effects != NULL && old_desktop != 0 && old_desktop != new_desktop )
    //    static_cast<EffectsHandlerImpl*>( effects )->desktopChanged( old_desktop );
    if (compositing() && m_compositor)
        m_compositor->addRepaintFull();
#else
    Q_UNUSED(new_activity)
#endif
}

void Workspace::slotDesktopCountChanged(uint previousCount, uint newCount)
{
    Q_UNUSED(previousCount)
    Placement::self()->reinitCascading(0);

    resetClientAreas(newCount);
}

void Workspace::resetClientAreas(uint desktopCount)
{
    // Make it +1, so that it can be accessed as [1..numberofdesktops]
    workarea.clear();
    workarea.resize(desktopCount + 1);
    restrictedmovearea.clear();
    restrictedmovearea.resize(desktopCount + 1);
    screenarea.clear();

    updateClientArea(true);
}

void Workspace::selectWmInputEventMask()
{
    uint32_t presentMask = 0;
    Xcb::WindowAttributes attr(rootWindow());
    if (!attr.isNull()) {
        presentMask = attr->your_event_mask;
    }

    Xcb::selectInput(rootWindow(),
                     presentMask |
                     XCB_EVENT_MASK_KEY_PRESS |
                     XCB_EVENT_MASK_PROPERTY_CHANGE |
                     XCB_EVENT_MASK_COLOR_MAP_CHANGE |
                     XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                     XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                     XCB_EVENT_MASK_FOCUS_CHANGE | // For NotifyDetailNone
                     XCB_EVENT_MASK_EXPOSURE
    );
}

/**
 * Sends client \a c to desktop \a desk.
 *
 * Takes care of transients as well.
 */
void Workspace::sendClientToDesktop(AbstractClient* c, int desk, bool dont_activate)
{
    if ((desk < 1 && desk != NET::OnAllDesktops) || desk > static_cast<int>(VirtualDesktopManager::self()->count()))
        return;
    int old_desktop = c->desktop();
    bool was_on_desktop = c->isOnDesktop(desk) || c->isOnAllDesktops();
    c->setDesktop(desk);
    if (c->desktop() != desk)   // No change or desktop forced
        return;
    desk = c->desktop(); // Client did range checking

    if (c->isOnDesktop(VirtualDesktopManager::self()->current())) {
        if (c->wantsTabFocus() && options->focusPolicyIsReasonable() &&
                !was_on_desktop && // for stickyness changes
                !dont_activate)
            requestFocus(c);
        else
            restackClientUnderActive(c);
    } else
        raiseClient(c);

    c->checkWorkspacePosition( QRect(), old_desktop );

    auto transients_stacking_order = ensureStackingOrder(c->transients());
    for (auto it = transients_stacking_order.constBegin();
            it != transients_stacking_order.constEnd();
            ++it)
        sendClientToDesktop(*it, desk, dont_activate);
    updateClientArea();
}

/**
 * checks whether the X Window with the input focus is on our X11 screen
 * if the window cannot be determined or inspected, resturn depends on whether there's actually
 * more than one screen
 *
 * this is NOT in any way related to XRandR multiscreen
 *
 */
extern bool is_multihead; // main.cpp
bool Workspace::isOnCurrentHead()
{
    if (!is_multihead) {
        return true;
    }

    Xcb::CurrentInput currentInput;
    if (currentInput.window() == XCB_WINDOW_NONE) {
        return !is_multihead;
    }

    Xcb::WindowGeometry geometry(currentInput.window());
    if (geometry.isNull()) { // should not happen
        return !is_multihead;
    }

    return rootWindow() == geometry->root;
}

void Workspace::sendClientToScreen(AbstractClient* c, int screen)
{
    c->sendToScreen(screen);
}

void Workspace::sendPingToWindow(xcb_window_t window, xcb_timestamp_t timestamp)
{
    if (rootInfo()) {
        rootInfo()->sendPing(window, timestamp);
    }
}

/**
 * Delayed focus functions
 */
void Workspace::delayFocus()
{
    requestFocus(delayfocus_client);
    cancelDelayFocus();
}

void Workspace::requestDelayFocus(AbstractClient* c)
{
    delayfocus_client = c;
    delete delayFocusTimer;
    delayFocusTimer = new QTimer(this);
    connect(delayFocusTimer, SIGNAL(timeout()), this, SLOT(delayFocus()));
    delayFocusTimer->setSingleShot(true);
    delayFocusTimer->start(options->delayFocusInterval());
}

void Workspace::cancelDelayFocus()
{
    delete delayFocusTimer;
    delayFocusTimer = 0;
}

bool Workspace::checkStartupNotification(xcb_window_t w, KStartupInfoId &id, KStartupInfoData &data)
{
    return startup->checkStartup(w, id, data) == KStartupInfo::Match;
}

/**
 * Puts the focus on a dummy window
 * Just using XSetInputFocus() with None would block keyboard input
 */
void Workspace::focusToNull()
{
    if (m_nullFocus) {
        m_nullFocus->focus();
    }
}

void Workspace::setShowingDesktop(bool showing)
{
    const bool changed = showing != showing_desktop;
    if (rootInfo() && changed) {
        rootInfo()->setShowingDesktop(showing);
    }
    showing_desktop = showing;
    // 记录时间戳，用于判断哪些客户端是在进入到显示桌面模式后新创建的
    showing_desktop_timestamp = kwinApp()->x11Time();

    AbstractClient *topDesk = nullptr;

    { // for the blocker RAII
    StackingUpdatesBlocker blocker(this); // updateLayer & lowerClient would invalidate stacking_order
    for (int i = stacking_order.count() - 1; i > -1; --i) {
        AbstractClient *c = qobject_cast<AbstractClient*>(stacking_order.at(i));
        if (c && c->isOnCurrentDesktop()) {
            if (c->isDock()) {
                c->updateLayer();
            } else if (c->isDesktop() && c->isShown(true)) {
                c->updateLayer();
                lowerClient(c);
                if (!topDesk)
                    topDesk = c;
                if (auto group = c->group()) {
                    foreach (Client *cm, group->members()) {
                        cm->updateLayer();
                    }
                }
            }
        }
    }
    } // ~StackingUpdatesBlocker

    if (showing_desktop && topDesk) {
        requestFocus(topDesk);
    } else if (!showing_desktop && changed) {
        const auto client = FocusChain::self()->getForActivation(VirtualDesktopManager::self()->current());
        if (client) {
            activateClient(client);
        }
    }
    if (changed)
        emit showingDesktopChanged(showing);

    if (showing_desktop) {
        clearSplitOutline();
    } else {
        uint desktop = VirtualDesktopManager::self()->current();
        workspace()->searchSplitScreenClient(desktop);
    }
    if (!waylandServer()) {
        QDBusInterface wm(DBUS_DEEPIN_WM_SERVICE, DBUS_DEEPIN_WM_OBJ, DBUS_DEEPIN_WM_INTF);
        wm.asyncCall("SetShowDesktop", showing);
    }
}

void Workspace::setPreviewClientList(const QList<AbstractClient*> &list)
{
    const bool changed = previewClients != list;

    if (!changed)
        return;

    previewClients = list;

    { // for the blocker RAII
    StackingUpdatesBlocker blocker(this); // updateLayer & lowerClient would invalidate stacking_order
    for (int i = stacking_order.count() - 1; i > -1; --i) {
        AbstractClient *c = qobject_cast<AbstractClient*>(stacking_order.at(i));
        if (c && c->isOnCurrentDesktop()) {
            if (!c->isDock() && !c->isDesktop()) {
                if (list.contains(c)) {
                    if (c->isMinimized()) {
                        // 记录窗口的最小化状态
                        c->setProperty("__deepin_kwin_minimized", true);
                        // 恢复最小化以便预览窗口
                        c->unminimize(true);
                    }
                } else {
                    if (c->property("__deepin_kwin_minimized").toBool()) {
                        c->setProperty("__deepin_kwin_minimized", QVariant());
                        // 恢复窗口被预览之前的状态
                        c->minimize(true);
                    }
                }

                c->updateLayer();
            }
        }
    }
    } // ~StackingUpdatesBlocker

    emit previewClientListChanged(list);
}

bool Workspace::previewingClientList() const
{
    return !previewClients.isEmpty();
}

bool Workspace::previewingClient(const AbstractClient *c) const
{
    return previewClients.contains(const_cast<AbstractClient*>(c));
}

void Workspace::setDisableGlobalShortcutsByUser(bool yes)
{
    if (global_shortcuts_disabled_by_user != yes) {
        global_shortcuts_disabled_by_user = yes;
    }
}

void Workspace::disableGlobalShortcutsForClient(bool disable)
{
    if (global_shortcuts_disabled_for_client == disable)
        return;

    if (global_shortcuts_disabled_by_user && !disable) {
        return;
    }

    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/kglobalaccel"),
                                                          QStringLiteral("org.kde.KGlobalAccel"),
                                                          QStringLiteral("blockGlobalShortcuts"));
    message.setArguments(QList<QVariant>() << disable);
    QDBusConnection::sessionBus().asyncCall(message);

    global_shortcuts_disabled_for_client = disable;
    // Update also Alt+LMB actions etc.
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it)
        (*it)->updateMouseGrab();
}

bool Workspace::isDisableHotKeys()
{
    return hot_keys_disabled_for_client;
}

void Workspace::disableHotKeysForClient(bool disable)
{
    if (hot_keys_disabled_for_client == disable)
        return;

    hot_keys_disabled_for_client = disable;
}

bool Workspace::isDisableGesture()
{
    return gesture_disabled_for_client;
}

void Workspace::disableGestureForClient(bool disable)
{
    if (gesture_disabled_for_client == disable)
        return;

    gesture_disabled_for_client = disable;
}

QString Workspace::supportInformation() const
{
    QString support;
    const QString yes = QStringLiteral("yes\n");
    const QString no = QStringLiteral("no\n");

    support.append(ki18nc("Introductory text shown in the support information.",
        "KWin Support Information:\n"
        "The following information should be used when requesting support on e.g. https://forum.kde.org.\n"
        "It provides information about the currently running instance, which options are used,\n"
        "what OpenGL driver and which effects are running.\n"
        "Please post the information provided underneath this introductory text to a paste bin service\n"
        "like http://paste.kde.org instead of pasting into support threads.\n").toString());
    support.append(QStringLiteral("\n==========================\n\n"));
    // all following strings are intended for support. They need to be pasted to e.g forums.kde.org
    // it is expected that the support will happen in English language or that the people providing
    // help understand English. Because of that all texts are not translated
    support.append(QStringLiteral("Version\n"));
    support.append(QStringLiteral("=======\n"));
    support.append(QStringLiteral("KWin version: "));
    support.append(QStringLiteral(KWIN_VERSION_STRING));
    support.append(QStringLiteral("\n"));
    support.append(QStringLiteral("Qt Version: "));
    support.append(QString::fromUtf8(qVersion()));
    support.append(QStringLiteral("\n"));
    support.append(QStringLiteral("Qt compile version: %1\n").arg(QStringLiteral(QT_VERSION_STR)));
    support.append(QStringLiteral("XCB compile version: %1\n\n").arg(QStringLiteral(XCB_VERSION_STRING)));
    support.append(QStringLiteral("Operation Mode: "));
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeX11:
        support.append(QStringLiteral("X11 only"));
        break;
    case Application::OperationModeWaylandOnly:
        support.append(QStringLiteral("Wayland Only"));
        break;
    case Application::OperationModeXwayland:
        support.append(QStringLiteral("Xwayland"));
        break;
    }
    support.append(QStringLiteral("\n\n"));

    support.append(QStringLiteral("Build Options\n"));
    support.append(QStringLiteral("=============\n"));

    support.append(QStringLiteral("KWIN_BUILD_DECORATIONS: "));
#ifdef KWIN_BUILD_DECORATIONS
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("KWIN_BUILD_TABBOX: "));
#ifdef KWIN_BUILD_TABBOX
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("KWIN_BUILD_ACTIVITIES: "));
#ifdef KWIN_BUILD_ACTIVITIES
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_DRM: "));
#if HAVE_DRM
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_GBM: "));
#if HAVE_GBM
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_X11_XCB: "));
#if HAVE_X11_XCB
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_EPOXY_GLX: "));
#if HAVE_EPOXY_GLX
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("HAVE_WAYLAND_EGL: "));
#if HAVE_WAYLAND_EGL
    support.append(yes);
#else
    support.append(no);
#endif
    support.append(QStringLiteral("\n"));

    if (auto c = kwinApp()->x11Connection()) {
        support.append(QStringLiteral("X11\n"));
        support.append(QStringLiteral("===\n"));
        auto x11setup = xcb_get_setup(c);
        support.append(QStringLiteral("Vendor: %1\n").arg(QString::fromUtf8(QByteArray::fromRawData(xcb_setup_vendor(x11setup), xcb_setup_vendor_length(x11setup)))));
        support.append(QStringLiteral("Vendor Release: %1\n").arg(x11setup->release_number));
        support.append(QStringLiteral("Protocol Version/Revision: %1/%2\n").arg(x11setup->protocol_major_version).arg(x11setup->protocol_minor_version));
        const auto extensions = Xcb::Extensions::self()->extensions();
        for (const auto &e : extensions) {
            support.append(QStringLiteral("%1: %2; Version: 0x%3\n").arg(QString::fromUtf8(e.name))
                                                                    .arg(e.present ? yes.trimmed() : no.trimmed())
                                                                    .arg(QString::number(e.version, 16)));
        }
        support.append(QStringLiteral("\n"));
    }

    if (auto bridge = Decoration::DecorationBridge::self()) {
        support.append(QStringLiteral("Decoration\n"));
        support.append(QStringLiteral("==========\n"));
        support.append(bridge->supportInformation());
        support.append(QStringLiteral("\n"));
    }
    support.append(QStringLiteral("Platform\n"));
    support.append(QStringLiteral("==========\n"));
    support.append(kwinApp()->platform()->supportInformation());
    support.append(QStringLiteral("\n"));

    support.append(QStringLiteral("Options\n"));
    support.append(QStringLiteral("=======\n"));
    const QMetaObject *metaOptions = options->metaObject();
    auto printProperty = [] (const QVariant &variant) {
        if (variant.type() == QVariant::Size) {
            const QSize &s = variant.toSize();
            return QStringLiteral("%1x%2").arg(QString::number(s.width())).arg(QString::number(s.height()));
        }
        if (QLatin1String(variant.typeName()) == QLatin1String("KWin::OpenGLPlatformInterface") ||
                QLatin1String(variant.typeName()) == QLatin1String("KWin::Options::WindowOperation")) {
            return QString::number(variant.toInt());
        }
        return variant.toString();
    };
    for (int i=0; i<metaOptions->propertyCount(); ++i) {
        const QMetaProperty property = metaOptions->property(i);
        if (QLatin1String(property.name()) == QLatin1String("objectName")) {
            continue;
        }
        support.append(QStringLiteral("%1: %2\n").arg(property.name()).arg(printProperty(options->property(property.name()))));
    }
    support.append(QStringLiteral("\nScreen Edges\n"));
    support.append(QStringLiteral(  "============\n"));
    const QMetaObject *metaScreenEdges = ScreenEdges::self()->metaObject();
    for (int i=0; i<metaScreenEdges->propertyCount(); ++i) {
        const QMetaProperty property = metaScreenEdges->property(i);
        if (QLatin1String(property.name()) == QLatin1String("objectName")) {
            continue;
        }
        support.append(QStringLiteral("%1: %2\n").arg(property.name()).arg(printProperty(ScreenEdges::self()->property(property.name()))));
    }
    support.append(QStringLiteral("\nScreens\n"));
    support.append(QStringLiteral(  "=======\n"));
    support.append(QStringLiteral("Multi-Head: "));
    if (is_multihead) {
        support.append(QStringLiteral("yes\n"));
        support.append(QStringLiteral("Head: %1\n").arg(screen_number));
    } else {
        support.append(QStringLiteral("no\n"));
    }
    support.append(QStringLiteral("Active screen follows mouse: "));
    if (screens()->isCurrentFollowsMouse())
        support.append(QStringLiteral(" yes\n"));
    else
        support.append(QStringLiteral(" no\n"));
    support.append(QStringLiteral("Number of Screens: %1\n\n").arg(screens()->count()));
    for (int i=0; i<screens()->count(); ++i) {
        const QRect geo = screens()->geometry(i);
        support.append(QStringLiteral("Screen %1:\n").arg(i));
        support.append(QStringLiteral("---------\n"));
        support.append(QStringLiteral("Name: %1\n").arg(screens()->name(i)));
        support.append(QStringLiteral("Geometry: %1,%2,%3x%4\n")
                              .arg(geo.x())
                              .arg(geo.y())
                              .arg(geo.width())
                              .arg(geo.height()));
        support.append(QStringLiteral("Scale: %1\n").arg(screens()->scale(i)));
        support.append(QStringLiteral("Refresh Rate: %1\n\n").arg(screens()->refreshRate(i)));
    }
    support.append(QStringLiteral("\nCompositing\n"));
    support.append(QStringLiteral(  "===========\n"));
    if (effects) {
        support.append(QStringLiteral("Compositing is active\n"));
        switch (effects->compositingType()) {
        case OpenGL2Compositing:
        case OpenGLCompositing: {
            GLPlatform *platform = GLPlatform::instance();
            if (platform->isGLES()) {
                support.append(QStringLiteral("Compositing Type: OpenGL ES 2.0\n"));
            } else {
                support.append(QStringLiteral("Compositing Type: OpenGL\n"));
            }
            support.append(QStringLiteral("OpenGL vendor string: ") +   QString::fromUtf8(platform->glVendorString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL renderer string: ") + QString::fromUtf8(platform->glRendererString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL version string: ") +  QString::fromUtf8(platform->glVersionString()) + QStringLiteral("\n"));
            support.append(QStringLiteral("OpenGL platform interface: "));
            switch (platform->platformInterface()) {
            case GlxPlatformInterface:
                support.append(QStringLiteral("GLX"));
                break;
            case EglPlatformInterface:
                support.append(QStringLiteral("EGL"));
                break;
            default:
                support.append(QStringLiteral("UNKNOWN"));
            }
            support.append(QStringLiteral("\n"));

            if (platform->supports(LimitedGLSL) || platform->supports(GLSL))
                support.append(QStringLiteral("OpenGL shading language version string: ") + QString::fromUtf8(platform->glShadingLanguageVersionString()) + QStringLiteral("\n"));

            support.append(QStringLiteral("Driver: ") + GLPlatform::driverToString(platform->driver()) + QStringLiteral("\n"));
            if (!platform->isMesaDriver())
                support.append(QStringLiteral("Driver version: ") + GLPlatform::versionToString(platform->driverVersion()) + QStringLiteral("\n"));

            support.append(QStringLiteral("GPU class: ") + GLPlatform::chipClassToString(platform->chipClass()) + QStringLiteral("\n"));

            support.append(QStringLiteral("OpenGL version: ") + GLPlatform::versionToString(platform->glVersion()) + QStringLiteral("\n"));

            if (platform->supports(LimitedGLSL) || platform->supports(GLSL))
                support.append(QStringLiteral("GLSL version: ") + GLPlatform::versionToString(platform->glslVersion()) + QStringLiteral("\n"));

            if (platform->isMesaDriver())
                support.append(QStringLiteral("Mesa version: ") + GLPlatform::versionToString(platform->mesaVersion()) + QStringLiteral("\n"));
            if (platform->serverVersion() > 0)
                support.append(QStringLiteral("X server version: ") + GLPlatform::versionToString(platform->serverVersion()) + QStringLiteral("\n"));
            if (platform->kernelVersion() > 0)
                support.append(QStringLiteral("Linux kernel version: ") + GLPlatform::versionToString(platform->kernelVersion()) + QStringLiteral("\n"));

            support.append(QStringLiteral("Direct rendering: "));
            support.append(QStringLiteral("Requires strict binding: "));
            if (!platform->isLooseBinding()) {
                support.append(QStringLiteral("yes\n"));
            } else {
                support.append(QStringLiteral("no\n"));
            }
            support.append(QStringLiteral("GLSL shaders: "));
            if (platform->supports(GLSL)) {
                if (platform->supports(LimitedGLSL)) {
                    support.append(QStringLiteral(" limited\n"));
                } else {
                    support.append(QStringLiteral(" yes\n"));
                }
            } else {
                support.append(QStringLiteral(" no\n"));
            }
            support.append(QStringLiteral("Texture NPOT support: "));
            if (platform->supports(TextureNPOT)) {
                if (platform->supports(LimitedNPOT)) {
                    support.append(QStringLiteral(" limited\n"));
                } else {
                    support.append(QStringLiteral(" yes\n"));
                }
            } else {
                support.append(QStringLiteral(" no\n"));
            }
            support.append(QStringLiteral("Virtual Machine: "));
            if (platform->isVirtualMachine()) {
                support.append(QStringLiteral(" yes\n"));
            } else {
                support.append(QStringLiteral(" no\n"));
            }

            support.append(QStringLiteral("OpenGL 2 Shaders are used\n"));
            support.append(QStringLiteral("Painting blocks for vertical retrace: "));
            if (m_compositor->scene()->blocksForRetrace())
                support.append(QStringLiteral(" yes\n"));
            else
                support.append(QStringLiteral(" no\n"));
            break;
        }
        case XRenderCompositing:
            support.append(QStringLiteral("Compositing Type: XRender\n"));
            break;
        case QPainterCompositing:
            support.append("Compositing Type: QPainter\n");
            break;
        case NoCompositing:
        default:
            support.append(QStringLiteral("Something is really broken, neither OpenGL nor XRender is used"));
        }
        support.append(QStringLiteral("\nLoaded Effects:\n"));
        support.append(QStringLiteral(  "---------------\n"));
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->loadedEffects()) {
            support.append(effect + QStringLiteral("\n"));
        }
        support.append(QStringLiteral("\nCurrently Active Effects:\n"));
        support.append(QStringLiteral(  "-------------------------\n"));
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->activeEffects()) {
            support.append(effect + QStringLiteral("\n"));
        }
        support.append(QStringLiteral("\nEffect Settings:\n"));
        support.append(QStringLiteral(  "----------------\n"));
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->loadedEffects()) {
            support.append(static_cast<EffectsHandlerImpl*>(effects)->supportInformation(effect));
            support.append(QStringLiteral("\n"));
        }
    } else {
        support.append(QStringLiteral("Compositing is not active\n"));
    }
    return support;
}

Client *Workspace::findClient(std::function<bool (const Client*)> func) const
{
    if (Client *ret = Toplevel::findInList(clients, func)) {
        return ret;
    }
    if (Client *ret = Toplevel::findInList(desktops, func)) {
        return ret;
    }
    return nullptr;
}

AbstractClient *Workspace::findAbstractClient(std::function<bool (const AbstractClient*)> func) const
{
    if (AbstractClient *ret = Toplevel::findInList(m_allClients, func)) {
        return ret;
    }
    if (Client *ret = Toplevel::findInList(desktops, func)) {
        return ret;
    }
    if (waylandServer()) {
        if (AbstractClient *ret = Toplevel::findInList(waylandServer()->internalClients(), func)) {
            return ret;
        }
    }
    return nullptr;
}

Unmanaged *Workspace::findUnmanaged(std::function<bool (const Unmanaged*)> func) const
{
    return Toplevel::findInList(unmanaged, func);
}

Unmanaged *Workspace::findUnmanaged(xcb_window_t w) const
{
    return findUnmanaged([w](const Unmanaged *u) {
        return u->window() == w;
    });
}

Client *Workspace::findClient(Predicate predicate, xcb_window_t w) const
{
    switch (predicate) {
    case Predicate::WindowMatch:
        return findClient([w](const Client *c) {
            return c->window() == w;
        });
    case Predicate::WrapperIdMatch:
        return findClient([w](const Client *c) {
            return c->wrapperId() == w;
        });
    case Predicate::FrameIdMatch:
        return findClient([w](const Client *c) {
            return c->frameId() == w;
        });
    case Predicate::InputIdMatch:
        return findClient([w](const Client *c) {
            return c->inputId() == w;
        });
    }
    return nullptr;
}

ShellClient *Workspace::findShellClient(quint32 window) const
{
    if (waylandServer())
        return waylandServer()->findClient(window);
    return nullptr;
}

Toplevel *Workspace::findToplevel(std::function<bool (const Toplevel*)> func) const
{
    if (Client *ret = Toplevel::findInList(clients, func)) {
        return ret;
    }
    if (Client *ret = Toplevel::findInList(desktops, func)) {
        return ret;
    }
    if (Unmanaged *ret = Toplevel::findInList(unmanaged, func)) {
        return ret;
    }
    return nullptr;
}

Toplevel *Workspace::findToplevel(const QUuid &internalId) const
{
    return findToplevel([internalId] (const KWin::Toplevel* l) -> bool {
        return internalId == l->internalId();
    });
}

Toplevel *Workspace::findToplevel(QWindow *w) const
{
    if (!w) {
        return nullptr;
    }
    if (waylandServer()) {
        if (auto c = waylandServer()->findClient(w)) {
            return c;
        }
    }
    return findUnmanaged(w->winId());
}

bool Workspace::hasClient(const AbstractClient *c)
{
    if (auto cc = dynamic_cast<const Client*>(c)) {
        return hasClient(cc);
    } else {
        return findAbstractClient([c](const AbstractClient *test) {
            return test == c;
        }) != nullptr;
    }
    return false;
}

void Workspace::forEachAbstractClient(std::function< void (AbstractClient*) > func)
{
    std::for_each(m_allClients.constBegin(), m_allClients.constEnd(), func);
    std::for_each(desktops.constBegin(), desktops.constEnd(), func);
}

Toplevel *Workspace::findInternal(QWindow *w) const
{
    if (!w) {
        return nullptr;
    }
    if (kwinApp()->operationMode() == Application::OperationModeX11) {
        return findUnmanaged(w->winId());
    } else {
        return waylandServer()->findClient(w);
    }
}

void Workspace::markXStackingOrderAsDirty()
{
    m_xStackingDirty = true;
    if (kwinApp()->x11Connection()) {
        m_xStackingQueryTree.reset(new Xcb::Tree(kwinApp()->x11RootWindow()));
    }
}

void Workspace::setWasUserInteraction()
{
    if (was_user_interaction) {
        return;
    }
    was_user_interaction = true;
    // might be called from within the filter, so delay till we now the filter returned
    QTimer::singleShot(0, this,
        [this] {
            m_wasUserInteractionFilter.reset();
        }
    );
}

QString Workspace::getScreenNameforWayland(int screen)
{
    QString res;
    QString name = screens()->name(screen);
    QStringList names = name.split(" ");
    if (names.size() >= 1) {
        names = names[1].split("-");
    } else {
        names = names[0].split("-");
    }
    if (names.size() > 2) {
        for (int i = 0; i < names.size(); i++) {
            if (names[i].toInt() >= 1) {
                res += names[i];
                break;
            } else {
                res += names[i];
                res += "-";
            }
        }
    } else {
        if (names.size() >= 1) {
            res = names[1];
        } else {
            res = names[0];
        }
    }

    return res;
}

QString Workspace::ActiveColor()
{
    if (activeColor.isEmpty())
        activeColor = QDBusInterface(KWinDBusService, KWinDBusPath, KWinDBusInterface).property("QtActiveColor").toString();
    return activeColor;
}

void Workspace::setActiveColor(QString color)
{
    activeColor = color;
}

bool Workspace::isDarkTheme()
{
    return m_isDarkTheme;
}

void Workspace::setDarkTheme(bool isDark)
{
    m_isDarkTheme = isDark;
}

void Workspace::qtactivecolorChanged()
{
    QString clr = QDBusInterface(KWinDBusService, KWinDBusPath, KWinDBusInterface).property("QtActiveColor").toString();
    setActiveColor(clr);
}

void Workspace::executeLock()
{
    LogindIntegration::self()->requestLock();
}

void Workspace::screensChanged()
{
    if (compositing()) {
        emit effects->closeEffect(true);    //close multitask view
    }
    updateSplitOutlineState(1, active_client ? active_client->desktop() : 1, true);
}

bool Workspace::checkClientAllowToSplit(AbstractClient *c)
{
    return c->checkClientAllowToTile();
}

void Workspace::setClientSplit(AbstractClient *c, int mode, bool isShowPreview)
{
    if (c == nullptr)
        return;

    c->splitWinAgain(mode);
    if (mode == 1 || mode == 2)
        workspace()->updateScreenSplitApp(c);
    if (isShowPreview) {
        emit c->showSplitPreview(c);
    }
    if (active_client != c) {
        activateClient(c);
    }
}

void Workspace::slotSetClientSplit(KWin::AbstractClient* c, int mode, bool isShowPreview)
{
    c->cancelSplitOutline();
    setClientSplit(c, mode, isShowPreview);
}

void Workspace::slotGetDdeShellSurface(AbstractClient* c, QObject *& dss)
{
    if (!c) {
        return;
    }

    ShellClient *sc = dynamic_cast<ShellClient *>(c);
    if (sc) {
        dss = sc->ddeShellSurface();
    }
}

void Workspace::updateSplitOutlinePos(int screen, int desktop)
{
    auto it = AbstractClient::splitManage.find(screen);
    if (AbstractClient::splitManage.end() == it) {
        return;
    }

    it.value()->updateOutlineStatus();
}

void Workspace::updateScreenSplitApp(Toplevel *t, bool onlyRemove)
{

    if (splitapp_stacking_order.contains(t)) {
        splitapp_stacking_order.removeOne(t);
    }

    if (!onlyRemove) {
        foreach (auto it, splitapp_stacking_order) {
            auto c = qobject_cast<AbstractClient*>(it);
            if (!c)
                continue;

            auto target = qobject_cast<AbstractClient*>(t);
            if (c->screen() == t->screen() && c->desktop() == t->desktop()) {
                if (c->electricBorderMode() == target->electricBorderMode()) {
                    c->quitSplitStatus();
                    splitapp_stacking_order.removeOne(it);
                } else {
                    c->resetSplitGeometry(c->electricBorderMode());
                }
            }
        }
        splitapp_stacking_order.append(t);
    }
}

void Workspace::updateSplitOutlineState(uint oldDesktop, uint newDesktop, bool isReCheckScreen)
{
    if (!compositing()) {
        return ;
    }

    clearSplitOutline();
    searchSplitScreenClient(newDesktop, isReCheckScreen);
}

void Workspace::searchSplitScreenClient(uint Desktop, bool isReCheckScreen, bool isSwitcheffects)
{
    for (int i = stacking_order.count() - 1; i > -1; --i) {
         AbstractClient *c = qobject_cast<AbstractClient*>(stacking_order.at(i));
         if (c && !c->isMinimized() && c->desktop() == Desktop && (c->quickTileMode() == QuickTileMode(QuickTileFlag::Left) || c->quickTileMode() == QuickTileMode(QuickTileFlag::Right))) {
             if (isSwitcheffects == true) {
                 c->handleSwitcheffectsQuickTileSize();
             }
             c->handlequickTileModeChanged(isReCheckScreen);
         }
    }
}

void Workspace::clearSplitOutline()
{
   QMap<int, SplitOutline*>::iterator it;
     int key;
     for (it = AbstractClient::splitManage.begin(); it!=AbstractClient::splitManage.end();)
     {
          key = it.key();
          it++;
          SplitOutline* splitOutline = AbstractClient::splitManage.take(key);
          delete splitOutline;
          splitOutline = nullptr;
     }
}

void Workspace::updateSplitOutlineLayerShowHide()
{
    if (!compositing()) {
        return ;
    }
    QVector<AbstractClient*> client_stack;
    int desktop = VirtualDesktopManager::self()->current();
    const Screens *s = Screens::self();
    int nscreens = s->count();
    while (nscreens)  {
        for (ToplevelList::ConstIterator it = stacking_order.constBegin();
             it != stacking_order.constEnd();
             ++it) {
            auto c = qobject_cast<AbstractClient*>(*it);
            if (stacking_order.contains(c) && c->screen() == nscreens-1 && c->desktop() == desktop && !c->isMinimized()) {
                client_stack.append(c);
            }
        }
        if (!client_stack.isEmpty() && client_stack.constLast()->quickTileMode() != QuickTileMode(QuickTileFlag::Left)
                                    && client_stack.constLast()->quickTileMode() != QuickTileMode(QuickTileFlag::Right)
                                    && AbstractClient::splitManage.contains(client_stack.constLast()->screen())) {
              AbstractClient::splitManage.find(client_stack.constLast()->screen()).value()->noActiveHide();
        } else if(!client_stack.isEmpty() && (client_stack.constLast()->quickTileMode() == QuickTileMode(QuickTileFlag::Left)
                                          || client_stack.constLast()->quickTileMode() == QuickTileMode(QuickTileFlag::Right))
                                          && AbstractClient::splitManage.contains(client_stack.constLast()->screen())) {
              AbstractClient::splitManage.find(client_stack.constLast()->screen()).value()->activeShow();
        }
        nscreens--;
    }
}

void Workspace::setWinSplitState(AbstractClient *client)
{
    int32_t ldata = 1;
    xcb_intern_atom_cookie_t cookie_st = xcb_intern_atom( connection(), 0, strlen( "_DEEPIN_NET_SUPPORTED"), "_DEEPIN_NET_SUPPORTED");
    xcb_intern_atom_reply_t *reply_st = xcb_intern_atom_reply( connection(), cookie_st, NULL);
    if (reply_st) {
        xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, client->windowId(), (*reply_st).atom,
                            XCB_ATOM_CARDINAL, 32, 1, &ldata);
        free(reply_st);
    }
}


//set window property
void Workspace::setWindowProperty(wl_resource* surface, const QString& name, const QVariant& value)
{
    if (surface && !name.isNull() && !value.isNull()) {
        m_windowPropertyMap[surface][name]=value;
    } else {
        qCritical()<<__FILE__<< __FUNCTION__<<__LINE__<<"set window property error!";
    }
}

//get window property
QMap< QString, QVariant >& Workspace::getWindowProperty(wl_resource* surface)
{
    if (!surface) {
        qCritical()<<__FILE__<< __FUNCTION__<<__LINE__<<"get window property error!";
    }
    return m_windowPropertyMap[surface];
}

const QMap< QString, QVariant >& Workspace::getWindowProperty(wl_resource* surface) const
{
    if (!surface) {
        qCritical()<<__FILE__<< __FUNCTION__<<__LINE__<<"get window property error!";
    }
    return m_windowPropertyMap[surface];
}

//del window property
void Workspace::delWindowProperty(wl_resource* surface)
{
    if (surface) {
        m_windowPropertyMap.remove(surface);
    } else {
        qCritical()<<__FILE__<< __FUNCTION__<<__LINE__<<"delete window property error!";
    }
}

} // namespace

