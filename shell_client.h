// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// Copyright (C) 2018 David Edmundson <davidedmundson@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SHELL_CLIENT_H
#define KWIN_SHELL_CLIENT_H

#include "screens.h"

#include "abstract_client.h"
#include <KWayland/Server/xdgshell_interface.h>
#include <KWayland/Server/strut_interface.h>
#include <KWayland/Server/ddeshell_interface.h>

namespace KWayland
{
namespace Server
{
class ShellSurfaceInterface;
class ServerSideDecorationInterface;
class ServerSideDecorationPaletteInterface;
class AppMenuInterface;
class PlasmaShellSurfaceInterface;
class QtExtendedSurfaceInterface;
class XdgDecorationInterface;
class DDEShellSurfaceInterface;
struct deepinKwinStrut;
class StrutInterface;
}
}

namespace KWin
{

/**
 * @brief The reason for which the server pinged a client surface
 */
enum class PingReason {
    CloseWindow = 0,
    FocusWindow
};

class KWIN_EXPORT ShellClient : public AbstractClient
{
    Q_OBJECT
    Q_PROPERTY(QPointF windowRadius READ windowRadius WRITE setWindowRadius NOTIFY windowRadiusChanged)
public:
    ShellClient(KWayland::Server::ShellSurfaceInterface *surface);
    ShellClient(KWayland::Server::XdgShellSurfaceInterface *surface);
    ShellClient(KWayland::Server::XdgShellPopupInterface *surface);
    virtual ~ShellClient();

    bool eventFilter(QObject *watched, QEvent *event) override;

    QStringList activities() const override;
    QPoint clientContentPos() const override;
    QSize clientSize() const override;
    QRect transparentRect() const override;
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    void debug(QDebug &stream) const override;
    double opacity() const override;
    void setOpacity(double opacity) override;
    QByteArray windowRole() const override;

    KWayland::Server::ShellSurfaceInterface *shellSurface() const {
        return m_shellSurface;
    }

    KWayland::Server::DDEShellSurfaceInterface *ddeShellSurface() const;

    void blockActivityUpdates(bool b = true) override;
    QString captionNormal() const override {
        return m_caption;
    }
    QString captionSuffix() const override {
        return m_captionSuffix;
    }
    void closeWindow() override;
    AbstractClient *findModal(bool allow_itself = false) override;
    bool isCloseable() const override;
    bool isFullScreen() const override;
    bool isMaximizable() const override;
    bool isMinimizable(bool isMinFunc = false) const override;
    bool isMovable() const override;
    bool isMovableAcrossScreens() const override;
    bool isResizable() const override;
    bool isShown(bool shaded_is_shown) const override;
    bool isHiddenInternal() const override {
        return m_unmapped || m_hidden;
    }
    void hideClient(bool hide) override;
    MaximizeMode maximizeMode() const override;
    MaximizeMode requestedMaximizeMode() const override;

    QRect geometryRestore() const override {
        QRect rect = m_geomMaximizeRestore;
        if (screens()->count() > 1 && rect.left() > screens()->size().width()) {
            rect.moveTopLeft(QPoint(0, 0));
        }
        return rect;
    }
    bool noBorder() const override;
    void setFullScreen(bool set, bool user = true) override;
    void setNoBorder(bool set) override;
    void updateDecoration(bool check_workspace_pos, bool force = false) override;
    void setOnAllActivities(bool set) override;
    void takeFocus() override;
    bool userCanSetFullScreen() const override;
    bool userCanSetNoBorder() const override;
    bool wantsInput() const override;
    bool dockWantsInput() const override;
    using AbstractClient::resizeWithChecks;
    void resizeWithChecks(int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    using AbstractClient::setGeometry;
    void setGeometry(int x, int y, int w, int h, ForceGeometry_t force = NormalGeometrySet) override;
    bool hasStrut() const override;
    void setStrut(KWayland::Server::deepinKwinStrut& strutArea){m_strutArea = strutArea;}
    KWayland::Server::deepinKwinStrut& strut(){return m_strutArea;}
    QSize calculateClientSize(const QSize& wsize);

    void setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo) override;

    quint32 windowId() const override {
        return m_windowId;
    }

    /**
     * The process for this client.
     * Note that processes started by kwin will share its process id.
     * @since 5.11
     * @returns the process if for this client.
     **/
    pid_t pid() const override;

    bool isStandAlone() const override;
    bool isOverride() const override;
    bool isActiveFullScreenRole() const override;
    bool isInternal() const;
    bool isLockScreen() const override;
    bool isInputMethod() const override;
    QWindow *internalWindow() const {
        return m_internalWindow;
    }

    void installDDEShellSurface(KWayland::Server::DDEShellSurfaceInterface *shellSurface);
    void installPlasmaShellSurface(KWayland::Server::PlasmaShellSurfaceInterface *surface);
    void installQtExtendedSurface(KWayland::Server::QtExtendedSurfaceInterface *surface);
    void installServerSideDecoration(KWayland::Server::ServerSideDecorationInterface *decoration);
    void installAppMenu(KWayland::Server::AppMenuInterface *appmenu);
    void installPalette(KWayland::Server::ServerSideDecorationPaletteInterface *palette);
    void installXdgDecoration(KWayland::Server::XdgDecorationInterface *decoration);

    bool isInitialPositionSet() const override;

    bool isTransient() const override;
    bool hasTransientPlacementHint() const override;
    QRect transientPlacement(const QRect &bounds) const override;

    QMatrix4x4 inputTransformation() const override;

    bool setupCompositing() override;
    void finishCompositing(ReleaseReason releaseReason = ReleaseReason::Release) override;

    void showOnScreenEdge() override;

    void killWindow() override;

    // TODO: const-ref
    void placeIn(QRect &area);

    bool hasPopupGrab() const override;
    void popupDone() override;

    void updateColorScheme() override;

    bool isPopupWindow() const override;

    bool isLocalhost() const override
    {
        return true;
    }

    void setMinimizeable(bool set)
    {
        m_minimizable = set;
    }

    void setMaximizeable(bool set)
    {
        m_maxmizable = set;
    }

    void setResizable(bool set)
    {
        m_resizable = set;
    }

    void setAcceptFocus(bool set)
    {
        m_acceptFocus = set;
    }

    inline bool isDragWindow()
    {
        return m_isDragWindow;
    }

    enum {
        /**  titlebar put in which gravity of client window  */
        FRAME_TOP                 = 0,
        FRAME_BOTTOM              = 1,
        FRAME_LETF                = 2,
        FRAME_RIGHT               = 3,
    };

    int getWindowGravity()
    {
        return m_windowGravity;
    }
    void setWindowGravity(int gravity)
    {
        m_windowGravity = gravity;
    }

    QSize minSize() const override {
        return m_clientMinSize;
    }
    QSize maxSize() const override {
        return m_clientMaxSize == QSize(0, 0) ? AbstractClient::maxSize() : m_clientMaxSize;
    }

    void setSplitable(bool splitable) {
        if (!m_ddeShellSurface)
            return;
        m_ddeShellSurface->sendSplitable(splitable);
    }
    QRect getConfigGeometry() override {
        return m_cfgGeom;
    }
protected:
    void addDamage(const QRegion &damage) override;
    bool belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const override;
    void doSetActive() override;
    Layer layerForDock() const override;
    void changeMaximize(bool horizontal, bool vertical, bool adjust) override;
    void setGeometryRestore(const QRect &geo) override {
        m_geomMaximizeRestore = geo;
    }
    void doResizeSync() override;
    bool isWaitingForMoveResizeSync() const override;
    bool acceptsFocus() const override;
    void doMinimize() override;
    void doMove(int x, int y) override;
    void updateCaption() override;
    void leaveMoveResize() override;
    void adjustClientMinSize(const Position mode) override;
    void clearPendingRequest() override;
    QPointF windowRadius() const;
    void setWindowRadius(QPointF radius);

private Q_SLOTS:
    void clientFullScreenChanged(bool fullScreen);

Q_SIGNALS:
    void windowRadiusChanged();

private:
    void init();
    template <class T>
    void initSurface(T *shellSurface);
    void requestGeometry(const QRect &rect);
    QPoint resetPosition(const QPoint &position, int gravity);
    void doSetGeometry(const QRect &rect);
    void createDecoration(const QRect &oldgeom);
    void destroyClient();
    void unmap();
    void createWindowId();
    void findInternalWindow();
    void updateInternalWindowGeometry();
    void syncGeometryToInternalWindow();
    void updateIcon();
    void markAsMapped();
    void setTransient();
    bool shouldExposeToWindowManagement();
    void handleScreenChanged();
    void updateClientOutputs();
    KWayland::Server::XdgShellSurfaceInterface::States xdgSurfaceStates() const;
    void updateShowOnScreenEdge();
    void updateMaximizeMode(MaximizeMode maximizeMode);
    // called on surface commit and processes all m_pendingConfigureRequests up to m_lastAckedConfigureReqest
    void updatePendingGeometry();
    QPoint popupOffset(const QRect &anchorRect, const Qt::Edges anchorEdge, const Qt::Edges gravity) const;
    static void deleteClient(ShellClient *c);

    KWayland::Server::ShellSurfaceInterface *m_shellSurface;
    KWayland::Server::XdgShellSurfaceInterface *m_xdgShellSurface;
    KWayland::Server::XdgShellPopupInterface *m_xdgShellPopup;

    // size of the last buffer
    QSize m_clientSize;
    // last size we requested or empty if we haven't sent an explicit request to the client
    // if empty the client should choose their own default size
    QSize m_requestedClientSize = QSize(0, 0);

    struct PendingConfigureRequest {
        //note for wl_shell we have no serial, so serialId and m_lastAckedConfigureRequest will always be 0
        //meaning we treat a surface commit as having processed all requests
        quint32 serialId = 0;
        // position to apply after a resize operation has been completed
        QPoint positionAfterResize;
        MaximizeMode maximizeMode;
    };
    QVector<PendingConfigureRequest> m_pendingConfigureRequests;
    quint32 m_lastAckedConfigureRequest = 0;

    //mode in use by the current buffer
    MaximizeMode m_maximizeMode = MaximizeRestore;
    //mode we currently want to be, could be pending on client updating, could be not sent yet
    MaximizeMode m_requestedMaximizeMode = MaximizeRestore;

    QRect m_geomFsRestore; //size and position of the window before it was set to fullscreen
    bool m_closing = false;
    quint32 m_windowId = 0;
    QWindow *m_internalWindow = nullptr;
    Qt::WindowFlags m_internalWindowFlags = Qt::WindowFlags();
    bool m_unmapped = true;
    QRect m_geomMaximizeRestore; // size and position of the window before it was set to maximize
    NET::WindowType m_windowType = NET::Normal;
    QPointer<KWayland::Server::DDEShellSurfaceInterface> m_ddeShellSurface;
    QPointer<KWayland::Server::PlasmaShellSurfaceInterface> m_plasmaShellSurface;
    QPointer<KWayland::Server::QtExtendedSurfaceInterface> m_qtExtendedSurface;
    QPointer<KWayland::Server::AppMenuInterface> m_appMenuInterface;
    QPointer<KWayland::Server::ServerSideDecorationPaletteInterface> m_paletteInterface;
    KWayland::Server::ServerSideDecorationInterface *m_serverDecoration = nullptr;
    KWayland::Server::XdgDecorationInterface *m_xdgDecoration = nullptr;
    bool m_userNoBorder = false;
    bool m_fullScreen = false;
    bool m_transient = false;
    bool m_hidden = false;
    bool m_internal;
    bool m_hasPopupGrab = false;
    bool m_minimizable = true;
    bool m_maxmizable = true;
    bool m_resizable = true;
    bool m_acceptFocus = true;
    bool m_isDragWindow = false;
    int m_windowGravity = FRAME_TOP;
    qreal m_opacity = 1.0;

    KWayland::Server::deepinKwinStrut m_strutArea;
    class RequestGeometryBlocker {
    public:
        RequestGeometryBlocker(ShellClient *client)
            : m_client(client)
        {
            m_client->m_requestGeometryBlockCounter++;
        }
        ~RequestGeometryBlocker()
        {
            m_client->m_requestGeometryBlockCounter--;
            if (m_client->m_requestGeometryBlockCounter == 0) {
                if (m_client->m_blockedRequestGeometry.isValid()) {
                    m_client->requestGeometry(m_client->m_blockedRequestGeometry);
                } else if (m_client->m_xdgShellSurface) {
                    m_client->m_xdgShellSurface->configure(m_client->xdgSurfaceStates());
                }
            }
        }
    private:
        ShellClient *m_client;
    };
    friend class RequestGeometryBlocker;
    int m_requestGeometryBlockCounter = 0;
    QRect m_blockedRequestGeometry;
    QString m_caption;
    QString m_captionSuffix;
    QHash<qint32, PingReason> m_pingSerials;

    bool m_compositingSetup = false;

    // min size of the buffer
    QSize m_clientMinSize = QSize(0, 0);
    QSize m_clientMaxSize = QSize(0, 0);
    /*The rect of the change to be performed by the client
     *The current rect will be ahead of the client changes so that we can use and calculate
     */
    QRect m_cfgGeom;
    int m_noTitleBar = -1;
    QPointF m_windowRadius = QPointF(0.0,0.0);
    bool m_isSetWindowRadius = false;
};

}

Q_DECLARE_METATYPE(KWin::ShellClient*)

#endif
