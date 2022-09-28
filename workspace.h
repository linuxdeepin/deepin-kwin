/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_WORKSPACE_H
#define KWIN_WORKSPACE_H

// kwin
#include "sm.h"
#include "options.h"
#include "utils.h"
// Qt
#include <QTimer>
#include <QVector>
// std
#include <functional>
#include <memory>
#include <KWayland/Server/clientmanagement_interface.h>
#include "wayland-server.h"

// TODO: Cleanup the order of things in this .h file

#define KWinDBusService "com.deepin.daemon.Appearance"
#define KWinDBusPath    "/com/deepin/daemon/Appearance"
#define KWinDBusInterface "com.deepin.daemon.Appearance"
#define KWinDBusPropertyInterface "org.freedesktop.DBus.Properties"

class QStringList;
class KConfig;
class KConfigGroup;
class KStartupInfo;
class KStartupInfoId;
class KStartupInfoData;

namespace KWin
{

namespace Xcb
{
class Tree;
class Window;
}

class AbstractClient;
class Client;
class KillWindow;
class ShortcutDialog;
class UserActionsMenu;
class Compositor;
class X11EventFilter;
enum class Predicate;

typedef KWayland::Server::ClientManagementInterface::WindowState WindowState;

class KWIN_EXPORT Workspace : public QObject
{
    Q_OBJECT
public:
    explicit Workspace(const QString &sessionKey = QString());
    virtual ~Workspace();

    static Workspace* self() {
        return _self;
    }

    bool workspaceEvent(xcb_generic_event_t*);
    bool workspaceEvent(QEvent*);

    bool hasClient(const Client*);
    bool hasClient(const AbstractClient*);

    /**
     * @brief Finds the first Client matching the condition expressed by passed in @p func.
     *
     * Internally findClient uses the std::find_if algorithm and that determines how the function
     * needs to be implemented. An example usage for finding a Client with a matching windowId
     * @code
     * xcb_window_t w; // our test window
     * Client *client = findClient([w](const Client *c) -> bool {
     *     return c->window() == w;
     * });
     * @endcode
     *
     * For the standard cases of matching the window id with one of the Client's windows use
     * the simplified overload method findClient(Predicate, xcb_window_t). Above example
     * can be simplified to:
     * @code
     * xcb_window_t w; // our test window
     * Client *client = findClient(Predicate::WindowMatch, w);
     * @endcode
     *
     * @param func Unary function that accepts a Client* as argument and
     * returns a value convertible to bool. The value returned indicates whether the
     * Client* is considered a match in the context of this function.
     * The function shall not modify its argument.
     * This can either be a function pointer or a function object.
     * @return KWin::Client* The found Client or @c null
     * @see findClient(Predicate, xcb_window_t)
     */
    Client *findClient(std::function<bool (const Client*)> func) const;
    AbstractClient *findAbstractClient(std::function<bool (const AbstractClient*)> func) const;
    /**
     * @brief Finds the Client matching the given match @p predicate for the given window.
     *
     * @param predicate Which window should be compared
     * @param w The window id to test against
     * @return KWin::Client* The found Client or @c null
     * @see findClient(std::function<bool (const Client*)>)
     */
    Client *findClient(Predicate predicate, xcb_window_t w) const;
    void forEachClient(std::function<void (Client*)> func);
    void forEachAbstractClient(std::function<void (AbstractClient*)> func);
    Unmanaged *findUnmanaged(std::function<bool (const Unmanaged*)> func) const;
    /**
     * @brief Finds the Unmanaged with the given window id.
     *
     * @param w The window id to search for
     * @return KWin::Unmanaged* Found Unmanaged or @c null if there is no Unmanaged with given Id.
     */
    Unmanaged *findUnmanaged(xcb_window_t w) const;
    void forEachUnmanaged(std::function<void (Unmanaged*)> func);
    Toplevel *findToplevel(std::function<bool (const Toplevel*)> func) const;
    /**
     * Finds the Toplevel for the KWin internal window @p w.
     * On Wayland this is normally a ShellClient. For X11 an Unmanaged.
     **/
    Toplevel *findToplevel(QWindow *w) const;
    Toplevel *findToplevel(const QUuid &internalId) const;
    /**
     * @brief Finds a Toplevel for the internal window @p w.
     *
     * Internal window means a window created by KWin itself. On X11 this is an Unmanaged
     * and mapped by the window id, on Wayland a ShellClient mapped on the internal window id.
     *
     * @returns Toplevel
     **/
    Toplevel *findInternal(QWindow *w) const;

    QRect clientArea(clientAreaOption, const QPoint& p, int desktop) const;
    QRect clientArea(clientAreaOption, const AbstractClient* c) const;
    QRect clientArea(clientAreaOption, int screen, int desktop) const;

    QRegion restrictedMoveArea(int desktop, StrutAreas areas = StrutAreaAll) const;

    bool initializing() const;

    /**
     * Returns the active client, i.e. the client that has the focus (or None
     * if no client has the focus)
     */
    AbstractClient* activeClient() const;
    /**
     * Client that was activated, but it's not yet really activeClient(), because
     * we didn't process yet the matching FocusIn event. Used mostly in focus
     * stealing prevention code.
     */
    AbstractClient* mostRecentlyActivatedClient() const;

    AbstractClient* clientUnderMouse(int screen) const;

    bool checkClientAllowToSplit(AbstractClient *c);
    void setClientSplit(AbstractClient* c, int mode, bool isShowPreview);
    void updateSplitOutlinePos(int screen, int desktop);

    void activateClient(AbstractClient*, bool force = false);
    void requestFocus(AbstractClient* c, bool force = false);
    enum ActivityFlag {
        ActivityFocus = 1 << 0, // focus the window
        ActivityFocusForce = 1 << 1 | ActivityFocus, // focus even if Dock etc.
        ActivityRaise = 1 << 2 // raise the window
    };
    Q_DECLARE_FLAGS(ActivityFlags, ActivityFlag)
    void takeActivity(AbstractClient* c, ActivityFlags flags);
    bool allowClientActivation(const AbstractClient* c, xcb_timestamp_t time = -1U, bool focus_in = false,
                               bool ignore_desktop = false);
    void restoreFocus();
    void gotFocusIn(const AbstractClient*);
    void setShouldGetFocus(AbstractClient*);
    bool activateNextClient(AbstractClient* c);
    bool focusChangeEnabled() {
        return block_focus == 0;
    }

    /**
     * Indicates that the client c is being moved around by the user.
     */
    void setClientIsMoving(AbstractClient* c);

    QPoint adjustClientPosition(AbstractClient* c, QPoint pos, bool unrestricted, double snapAdjust = 1.0);
    QRect adjustClientSize(AbstractClient* c, QRect moveResizeGeom, int mode);
    void raiseClient(AbstractClient* c, bool nogroup = false);
    void lowerClient(AbstractClient* c, bool nogroup = false);
    void raiseClientRequest(AbstractClient* c, NET::RequestSource src = NET::FromApplication, xcb_timestamp_t timestamp = 0);
    void lowerClientRequest(Client* c, NET::RequestSource src, xcb_timestamp_t timestamp);
    void lowerClientRequest(AbstractClient* c);
    void restackClientUnderActive(AbstractClient*);
    void restack(AbstractClient *c, AbstractClient *under, bool force = false);
    void updateClientLayer(AbstractClient* c);
    void raiseOrLowerClient(AbstractClient*);
    void resetUpdateToolWindowsTimer();
    void restoreSessionStackingOrder(Client* c);
    void updateStackingOrder(bool propagate_new_clients = false);
    void forceRestacking();

    void clientHidden(AbstractClient*);
    void clientAttentionChanged(AbstractClient* c, bool set);

    /**
     * @return List of clients currently managed by Workspace
     **/
    const ClientList &clientList() const {
        return clients;
    }
    /**
     * @return List of unmanaged "clients" currently registered in Workspace
     **/
    const UnmanagedList &unmanagedList() const {
        return unmanaged;
    }
    /**
     * @return List of desktop "clients" currently managed by Workspace
     **/
    const ClientList &desktopList() const {
        return desktops;
    }
    /**
     * @return List of deleted "clients" currently managed by Workspace
     **/
    const DeletedList &deletedList() const {
        return deleted;
    }
    /**
     * @returns List of all clients (either X11 or Wayland) currently managed by Workspace
     **/
    const QList<AbstractClient*> allClientList() const {
        return m_allClients;
    }

    void stackScreenEdgesUnderOverrideRedirect();

    QString ActiveColor();
    void setActiveColor(QString color);
    void enableKwinDebug(bool enable) {
        m_kwinDebug = enable;
    }
    bool isKwinDebug() {
        return m_kwinDebug;
    }

    void enablePrintKwinFps(bool enable) {
        m_printKwinFps = enable;
    }
    bool isPrintKwinFps() {
        return m_printKwinFps;
    }

    void dumpOutputBuffer() {
        m_dumpOutputBuffer = true;
    }
    void dumpOutputBufferFinish() {
        m_dumpOutputBuffer = false;
    }
    bool isDumpOutputBuffer() {
        return m_dumpOutputBuffer;
    }

public:
    QPoint cascadeOffset(const AbstractClient *c) const;

private:
    Compositor *m_compositor;

    //-------------------------------------------------
    // Unsorted

public:

    bool isDragingWithContent()const {return m_ClientDragingWithContent;}

    bool isOnCurrentHead();
    // True when performing Workspace::updateClientArea().
    // The calls below are valid only in that case.
    bool inUpdateClientArea() const;
    QRegion previousRestrictedMoveArea(int desktop, StrutAreas areas = StrutAreaAll) const;
    QVector< QRect > previousScreenSizes() const;
    int oldDisplayWidth() const;
    int oldDisplayHeight() const;

    /**
     * Returns the list of clients sorted in stacking order, with topmost client
     * at the last position
     */
    const ToplevelList& stackingOrder() const;
    ToplevelList xStackingOrder() const;
    ClientList ensureStackingOrder(const ClientList& clients) const;
    QList<AbstractClient*> ensureStackingOrder(const QList<AbstractClient*> &clients) const;

    AbstractClient* topClientOnDesktop(int desktop, int screen, bool unconstrained = false,
                               bool only_normal = true) const;
    AbstractClient* findDesktop(bool topmost, int desktop) const;
    void sendClientToDesktop(AbstractClient* c, int desktop, bool dont_activate);
    void windowToPreviousDesktop(AbstractClient* c);
    void windowToNextDesktop(AbstractClient* c);
    void sendClientToScreen(AbstractClient* c, int screen);

    void addManualOverlay(xcb_window_t id) {
        manual_overlays << id;
    }
    void removeManualOverlay(xcb_window_t id) {
        manual_overlays.removeOne(id);
    }

    /**
     * Shows the menu operations menu for the client and makes it active if
     * it's not already.
     */
    void showWindowMenu(const QRect& pos, AbstractClient* cl);
    const UserActionsMenu *userActionsMenu() const {
        return m_userActionsMenu;
    }

    void showApplicationMenu(const QRect &pos, AbstractClient *c, int actionId);

    void updateMinimizedOfTransients(AbstractClient*);
    void updateOnAllDesktopsOfTransients(AbstractClient*);
    void checkTransients(xcb_window_t w);

    void storeSession(KConfig* config, SMSavePhase phase);
    void storeClient(KConfigGroup &cg, int num, Client *c);
    void storeSubSession(const QString &name, QSet<QByteArray> sessionIds);
    void loadSubSessionInfo(const QString &name);

    SessionInfo* takeSessionInfo(Client*);

    // D-Bus interface
    QString supportInformation() const;

    void setCurrentScreen(int new_screen);

    void setShowingDesktop(bool showing);
    bool showingDesktop() const;
    xcb_timestamp_t showingDesktopTimestamp() const;

    Q_SLOT void setPreviewClientList(const QList<AbstractClient *> &list);
    Q_SLOT bool previewingClientList() const;
    Q_SLOT bool previewingClient(const AbstractClient *c) const;

    void sendPingToWindow(xcb_window_t w, xcb_timestamp_t timestamp);   // Called from Client::pingWindow()

    void removeClient(Client*);   // Only called from Client::destroyClient() or Client::releaseWindow()
    void setActiveClient(AbstractClient*);
    Group* findGroup(xcb_window_t leader) const;
    void addGroup(Group* group);
    void removeGroup(Group* group);
    Group* findClientLeaderGroup(const Client* c) const;

    void removeUnmanaged(Unmanaged*);   // Only called from Unmanaged::release()
    void removeDeleted(Deleted*);
    void addDeleted(Deleted*, Toplevel*);

    bool checkStartupNotification(xcb_window_t w, KStartupInfoId& id, KStartupInfoData& data);

    void focusToNull(); // SELI TODO: Public?

    void clientShortcutUpdated(AbstractClient* c);
    bool shortcutAvailable(const QKeySequence &cut, AbstractClient* ignore = NULL) const;
    bool globalShortcutsDisabled() const;
    void disableGlobalShortcutsForClient(bool disable);
    void setDisableGlobalShortcutsByUser(bool yes);
    void disableHotKeysForClient(bool disable);
    bool isDisableHotKeys();
    void disableGestureForClient(bool disable);
    bool isDisableGesture();

    void sessionSaveStarted();
    void sessionSaveDone();
    void setWasUserInteraction();
    bool wasUserInteraction() const;
    bool sessionSaving() const;

    int packPositionLeft(const AbstractClient* cl, int oldx, bool left_edge) const;
    int packPositionRight(const AbstractClient* cl, int oldx, bool right_edge) const;
    int packPositionUp(const AbstractClient* cl, int oldy, bool top_edge) const;
    int packPositionDown(const AbstractClient* cl, int oldy, bool bottom_edge) const;

    void cancelDelayFocus();
    void requestDelayFocus(AbstractClient*);

    /**
    * updates the mouse position to track whether a focus follow mouse focus change was caused by
    * an actual mouse move
    * is esp. called on enter/motion events of inactive windows
    * since an active window doesn't receive mouse events, it must also be invoked if a (potentially)
    * active window might be moved/resize away from the cursor (causing a leave event)
    */
    void updateFocusMousePosition(const QPoint& pos);
    QPoint focusMousePosition() const;

    AbstractClient* getMovingClient() {
        return movingClient;
    }

    /**
     * @returns Whether we have a Compositor and it is active (Scene created)
     **/
    bool compositing() const;

    void registerEventFilter(X11EventFilter *filter);
    void unregisterEventFilter(X11EventFilter *filter);

    void markXStackingOrderAsDirty();

    void quickTileWindow(QuickTileMode mode);

    enum Direction {
        DirectionNorth,
        DirectionEast,
        DirectionSouth,
        DirectionWest
    };
    void switchWindow(Direction direction);

    ShortcutDialog *shortcutDialog() const {
        return client_keys_dialog;
    }

    void updateScreenSplitApp(Toplevel *t, bool onlyRemove = false);

    void updateSplitOutlineState(uint oldDesktop, uint newDesktop, bool isReCheckScreen = false);

    void searchSplitScreenClient(uint Desktop, bool isReCheckScreen = false);

    void clearSplitOutline();

    void updateSplitOutlineLayerShowHide();
    inline void setMouseRaised(bool flag) {
        m_mouseRaised = flag;
    }
    //set/get/del window property
    void setWindowProperty(wl_resource* surface, const QString& name, const QVariant& value);
    const QMap< QString, QVariant >& getWindowProperty(wl_resource* surface) const;
    QMap< QString, QVariant >& getWindowProperty(wl_resource* surface);
    void delWindowProperty(wl_resource* surface);

public Q_SLOTS:
    void performWindowOperation(KWin::AbstractClient* c, Options::WindowOperation op);
    void slotSetClientSplit(KWin::AbstractClient* c, int mode, bool isShowPreview);
    // Keybindings
    //void slotSwitchToWindow( int );
    void slotWindowToDesktop(uint i);

    //void slotWindowToListPosition( int );
    void slotSwitchToScreen();
    void slotWindowToScreen();
    void slotSwitchToNextScreen();
    void slotWindowToNextScreen();
    void slotSwitchToPrevScreen();
    void slotWindowToPrevScreen();
    void slotToggleShowDesktop();

    void slotWindowMaximize();
    void slotWindowMaximizeVertical();
    void slotWindowMaximizeHorizontal();
    void slotWindowMinimize();
    void slotWindowShade();
    void slotWindowRaise();
    void slotWindowLower();
    void slotWindowRaiseOrLower();
    void slotActivateAttentionWindow();
    void slotWindowPackLeft();
    void slotWindowPackRight();
    void slotWindowPackUp();
    void slotWindowPackDown();
    void slotWindowGrowHorizontal();
    void slotWindowGrowVertical();
    void slotWindowShrinkHorizontal();
    void slotWindowShrinkVertical();

    void slotIncreaseWindowOpacity();
    void slotLowerWindowOpacity();

    void slotWindowOperations();
    void slotWindowClose();
    void slotWindowMove();
    void slotWindowResize();
    void slotWindowAbove();
    void slotWindowBelow();
    void slotWindowOnAllDesktops();
    void slotWindowFullScreen();
    void slotWindowNoBorder();

    void slotWindowToNextDesktop();
    void slotWindowToPreviousDesktop();
    void slotWindowToDesktopRight();
    void slotWindowToDesktopLeft();
    void slotWindowToDesktopUp();
    void slotWindowToDesktopDown();

    void reconfigure();
    void slotReconfigure();

    void slotKillWindow();

    void slotSetupWindowShortcut();
    void setupWindowShortcutDone(bool);

    void updateClientArea();

    void slotActivateNextTab(); // Slot to move left the active Client.
    void slotActivatePrevTab(); // Slot to move right the active Client.
    void slotUntab(); // Slot to remove the active client from its group.
    void updateWindowStates();

    void slotTouchPadTomoveWindow(int x, int y);
    void slotEndTouchPadToMoveWindow();

    void qtactivecolorChanged();
    void screensChanged();

private Q_SLOTS:
    void desktopResized();
    void selectWmInputEventMask();
    void slotUpdateToolWindows();
    void delayFocus();
    void slotReloadConfig();
    void updateCurrentActivity(const QString &new_activity);
    // virtual desktop handling
    void slotDesktopCountChanged(uint previousCount, uint newCount);
    void slotCurrentDesktopChanged(uint oldDesktop, uint newDesktop);

    // session management
    void saveState(QSessionManager &sm);
    void commitData(QSessionManager &sm);

Q_SIGNALS:
    /**
     * Emitted after the Workspace has setup the complete initialization process.
     * This can be used to connect to for performing post-workspace initialization.
     **/
    void workspaceInitialized();

    //Signals required for the scripting interface
    void desktopPresenceChanged(KWin::AbstractClient*, int);
    void currentDesktopChanged(int, KWin::AbstractClient*);
    void clientAdded(KWin::Client*);
    void clientRemoved(KWin::AbstractClient*);
    void clientActivated(KWin::AbstractClient*);
    void clientDemandsAttentionChanged(KWin::AbstractClient*, bool);
    void clientMinimizedChanged(KWin::AbstractClient*);
    void groupAdded(KWin::Group*);
    void unmanagedAdded(KWin::Unmanaged*);
    void unmanagedRemoved(KWin::Unmanaged*);
    void deletedRemoved(KWin::Deleted*);
    void configChanged();
    void reinitializeCompositing();
    void showingDesktopChanged(bool showing);
    void previewClientListChanged(QList<AbstractClient*> list);
    /**
     * This signels is emitted when ever the stacking order is change, ie. a window is risen
     * or lowered
     */
    void stackingOrderChanged();
    void outputModeChanged();
    void windowStateChanged();

    void activeColorChanged();

private:
    void init();
    void initWithX11();
    void initShortcuts();
    template <typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut,
                      Slot slot, const QVariant &data = QVariant());
    template <typename T, typename Slot>
    void initShortcut(const QString &actionName, const QString &description, const QKeySequence &shortcut, T *receiver, Slot slot, const QVariant &data = QVariant());
    void setupWindowShortcut(AbstractClient* c);
    bool switchWindow(AbstractClient *c, Direction direction, QPoint curPos, int desktop);

    void propagateClients(bool propagate_new_clients);   // Called only from updateStackingOrder
    ToplevelList constrainedStackingOrder();
    void raiseClientWithinApplication(AbstractClient* c);
    void lowerClientWithinApplication(AbstractClient* c);
    bool allowFullClientRaising(const AbstractClient* c, xcb_timestamp_t timestamp);
    bool keepTransientAbove(const AbstractClient* mainwindow, const AbstractClient* transient);
    bool keepDeletedTransientAbove(const Toplevel *mainWindow, const Deleted *transient) const;
    void blockStackingUpdates(bool block);
    void updateToolWindows(bool also_hide);
    void fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geom);
    void saveOldScreenSizes();

    /// This is the right way to create a new client
    Client* createClient(xcb_window_t w, bool is_mapped);
    void setupClientConnections(AbstractClient *client);
    void addClient(Client* c);
    Unmanaged* createUnmanaged(xcb_window_t w);
    void addUnmanaged(Unmanaged* c);

    //---------------------------------------------------------------------

    void setWinSplitState(AbstractClient *client);
    void closeActivePopup();
    void updateClientArea(bool force);
    void resetClientAreas(uint desktopCount);
    void updateClientVisibilityOnDesktopChange(uint newDesktop);
    void activateClientOnNewDesktop(uint desktop);
    AbstractClient *findClientToActivateOnDesktop(uint desktop);

    QWidget* active_popup;
    AbstractClient* active_popup_client;



    bool m_ClientDragingWithContent;//拖动的时候，带内容


    int m_initialDesktop;
    void loadSessionInfo(const QString &key);
    void addSessionInfo(KConfigGroup &cg);

    QList<SessionInfo*> session;
    static const char* windowTypeToTxt(NET::WindowType type);
    static NET::WindowType txtToWindowType(const char* txt);
    static bool sessionInfoWindowTypeMatch(Client* c, SessionInfo* info);

    void updateXStackingOrder();

    inline bool isMouseRaised() {
        return m_mouseRaised;
    }

    AbstractClient* active_client;
    AbstractClient* last_active_client;
    AbstractClient* most_recently_raised; // Used ONLY by raiseOrLowerClient()
    AbstractClient* movingClient;

    // Delay(ed) window focus timer and client
    QTimer* delayFocusTimer;
    AbstractClient* delayfocus_client;
    QPoint focusMousePos;

    ClientList clients;
    QList<AbstractClient*> m_allClients;
    ClientList desktops;
    UnmanagedList unmanaged;
    DeletedList deleted;

    ToplevelList splitapp_stacking_order;    //split app
    ToplevelList unconstrained_stacking_order; // Topmost last
    ToplevelList stacking_order; // Topmost last
    QVector<xcb_window_t> manual_overlays; //Topmost last
    bool force_restacking;
    ToplevelList x_stacking; // From XQueryTree()
    std::unique_ptr<Xcb::Tree> m_xStackingQueryTree;
    bool m_xStackingDirty = false;
    QList<AbstractClient*> should_get_focus; // Last is most recent
    QList<AbstractClient*> attention_chain;

    bool showing_desktop;
    xcb_timestamp_t showing_desktop_timestamp;
    QList<AbstractClient*> previewClients;

    GroupList groups;

    bool was_user_interaction;
    QScopedPointer<X11EventFilter> m_wasUserInteractionFilter;
    bool session_saving;
    int session_active_client;
    int session_desktop;

    int block_focus;

    /**
     * Holds the menu containing the user actions which is shown
     * on e.g. right click the window decoration.
     **/
    UserActionsMenu *m_userActionsMenu;

    void modalActionsSwitch(bool enabled);

    ShortcutDialog* client_keys_dialog;
    AbstractClient* client_keys_client;
    bool global_shortcuts_disabled_for_client;
    bool global_shortcuts_disabled_by_user;
    bool hot_keys_disabled_for_client;
    bool gesture_disabled_for_client;

    // Timer to collect requests for 'reconfigure'
    QTimer reconfigureTimer;

    QTimer updateToolWindowsTimer;

    static Workspace* _self;

    bool workspaceInit;

    KStartupInfo* startup;

    QVector<QRect> workarea; // Array of workareas for virtual desktops
    // Array of restricted areas that window cannot be moved into
    QVector<StrutRects> restrictedmovearea;
    // Array of the previous restricted areas that window cannot be moved into
    QVector<StrutRects> oldrestrictedmovearea;
    QVector< QVector<QRect> > screenarea; // Array of workareas per xinerama screen for all virtual desktops
    QVector< QRect > oldscreensizes; // array of previous sizes of xinerama screens
    QSize olddisplaysize; // previous sizes od displayWidth()/displayHeight()

    int set_active_client_recursion;
    int block_stacking_updates; // When > 0, stacking updates are temporarily disabled
    bool blocked_propagating_new_clients; // Propagate also new clients after enabling stacking updates?
    QScopedPointer<Xcb::Window> m_nullFocus;
    friend class StackingUpdatesBlocker;

    QScopedPointer<KillWindow> m_windowKiller;

    QList<X11EventFilter *> m_eventFilters;
    QList<X11EventFilter *> m_genericEventFilters;
    QScopedPointer<X11EventFilter> m_movingClientFilter;
    QList<WindowState*> m_windowStates;
    bool m_mouseRaised = false;

    QString activeColor;
    QString m_screenSerialNum = "";
    //window property map
    QMap< wl_resource*, QMap< QString, QVariant > > m_windowPropertyMap;

    bool m_kwinDebug = false;
    bool m_printKwinFps = false;
    bool m_dumpOutputBuffer = false;

private:
    friend bool performTransiencyCheck();
    friend Workspace *workspace();
};

/**
 * Helper for Workspace::blockStackingUpdates() being called in pairs (True/false)
 */
class StackingUpdatesBlocker
{
public:
    explicit StackingUpdatesBlocker(Workspace* w)
        : ws(w) {
        ws->blockStackingUpdates(true);
    }
    ~StackingUpdatesBlocker() {
        ws->blockStackingUpdates(false);
    }

private:
    Workspace* ws;
};

class ColorMapper : public QObject
{
    Q_OBJECT
public:
    ColorMapper(QObject *parent);
    virtual ~ColorMapper();
public Q_SLOTS:
    void update();
private:
    xcb_colormap_t m_default;
    xcb_colormap_t m_installed;
};

//---------------------------------------------------------
// Unsorted

inline bool Workspace::initializing() const
{
    return workspaceInit;
}

inline AbstractClient *Workspace::activeClient() const
{
    return active_client;
}

inline AbstractClient *Workspace::mostRecentlyActivatedClient() const
{
    return should_get_focus.count() > 0 ? should_get_focus.last() : active_client;
}

inline void Workspace::addGroup(Group* group)
{
    emit groupAdded(group);
    groups.append(group);
}

inline void Workspace::removeGroup(Group* group)
{
    groups.removeAll(group);
}

inline const ToplevelList& Workspace::stackingOrder() const
{
    // TODO: Q_ASSERT( block_stacking_updates == 0 );
    return stacking_order;
}

inline bool Workspace::wasUserInteraction() const
{
    return was_user_interaction;
}

inline void Workspace::sessionSaveStarted()
{
    session_saving = true;
}

inline bool Workspace::sessionSaving() const
{
    return session_saving;
}

inline bool Workspace::showingDesktop() const
{
    return showing_desktop;
}

inline xcb_timestamp_t Workspace::showingDesktopTimestamp() const
{
    return showing_desktop_timestamp;
}

inline bool Workspace::globalShortcutsDisabled() const
{
    return global_shortcuts_disabled_for_client;
}

inline void Workspace::forceRestacking()
{
    force_restacking = true;
    StackingUpdatesBlocker blocker(this);   // Do restacking if not blocked
}

inline void Workspace::updateFocusMousePosition(const QPoint& pos)
{
    focusMousePos = pos;
}

inline QPoint Workspace::focusMousePosition() const
{
    return focusMousePos;
}

inline
void Workspace::forEachClient(std::function< void (Client*) > func)
{
    std::for_each(clients.constBegin(), clients.constEnd(), func);
    std::for_each(desktops.constBegin(), desktops.constEnd(), func);
}

inline
void Workspace::forEachUnmanaged(std::function< void (Unmanaged*) > func)
{
    std::for_each(unmanaged.constBegin(), unmanaged.constEnd(), func);
}

inline bool Workspace::hasClient(const Client* c)
{
    return findClient([c](const Client *test) {
        return test == c;
    });
}

inline Workspace *workspace()
{
    return Workspace::_self;
}

} // namespace
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Workspace::ActivityFlags)

#endif
