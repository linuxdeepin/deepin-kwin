// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// Copyright (C) 2018 David Edmundson <davidedmundson@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shell_client.h"
#include "composite.h"
#include "cursor.h"
#include "deleted.h"
#include "placement.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "useractions.h"
#include "virtualdesktops.h"
#include "screens.h"
#include "decorations/decorationbridge.h"
#include "decorations/decoratedclient.h"
#include <KDecoration2/Decoration>
#include <KDecoration2/DecoratedClient>

#include <KWayland/Client/surface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/clientconnection.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/shell_interface.h>
#include <KWayland/Server/surface_interface.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/plasmashell_interface.h>
#include <KWayland/Server/shadow_interface.h>
#include <KWayland/Server/server_decoration_interface.h>
#include <KWayland/Server/qtsurfaceextension_interface.h>
#include <KWayland/Server/plasmawindowmanagement_interface.h>
#include <KWayland/Server/appmenu_interface.h>
#include <KWayland/Server/server_decoration_palette_interface.h>
#include <KWayland/Server/xdgdecoration_interface.h>
#include <KWayland/Server/ddeshell_interface.h>
#include <KWayland/Server/datadevice_interface.h>

#include <KDesktopFile>

#include <QFileInfo>
#include <QOpenGLFramebufferObject>
#include <QWindow>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

using namespace KWayland::Server;

static const QByteArray s_skipClosePropertyName = QByteArrayLiteral("KWIN_SKIP_CLOSE_ANIMATION");

namespace KWin
{
#define SHOW_SCALE 2/3

ShellClient::ShellClient(ShellSurfaceInterface *surface)
    : AbstractClient()
    , m_shellSurface(surface)
    , m_xdgShellSurface(nullptr)
    , m_xdgShellPopup(nullptr)
    , m_internal(surface->client() == waylandServer()->internalConnection())
{
    setSurface(surface->surface());
    init();
}

ShellClient::ShellClient(XdgShellSurfaceInterface *surface)
    : AbstractClient()
    , m_shellSurface(nullptr)
    , m_xdgShellSurface(surface)
    , m_xdgShellPopup(nullptr)
    , m_internal(surface->client() == waylandServer()->internalConnection())
{
    setSurface(surface->surface());
    init();
}

ShellClient::ShellClient(XdgShellPopupInterface *surface)
    : AbstractClient()
    , m_shellSurface(nullptr)
    , m_xdgShellSurface(nullptr)
    , m_xdgShellPopup(surface)
    , m_internal(surface->client() == waylandServer()->internalConnection())
{
    setSurface(surface->surface());
    init();
}

ShellClient::~ShellClient() = default;

template <class T>
void ShellClient::initSurface(T *shellSurface)
{
    if(!m_internalWindow)
        m_caption = shellSurface->title().simplified();
    // delay till end of init
    QTimer::singleShot(0, this, &ShellClient::updateCaption);
    connect(shellSurface, &T::destroyed, this, &ShellClient::destroyClient);
    connect(shellSurface, &T::titleChanged, this,
        [this] (const QString &s) {
            const auto oldSuffix = m_captionSuffix;
            m_caption = s.simplified();
            updateCaption();
            if (m_captionSuffix == oldSuffix) {
                // don't emit caption change twice
                // it already got emitted by the changing suffix
                emit captionChanged();
            }
        }
    );
    connect(shellSurface, &T::moveRequested, this,
        [this] {
            // TODO: check the seat and serial
            performMouseCommand(Options::MouseMove, Cursor::pos());
        }
    );

    // determine the resource name, this is inspired from ICCCM 4.1.2.5
    // the binary name of the invoked client
    QFileInfo info{shellSurface->client()->executablePath()};
    QByteArray resourceName;
    if (info.exists()) {
        resourceName = info.fileName().toUtf8();
    }
    setResourceClass(resourceName, shellSurface->windowClass());
    connect(shellSurface, &T::windowClassChanged, this,
        [this, resourceName] (const QByteArray &windowClass) {
            setResourceClass(resourceName, windowClass);
            if (!m_internal) {
                setupWindowRules(true);
                applyWindowRules();
            }
            setDesktopFileName(windowClass);
        }
    );
    connect(shellSurface, &T::resizeRequested, this,
        [this] (SeatInterface *seat, quint32 serial, Qt::Edges edges) {
            // TODO: check the seat and serial
            Q_UNUSED(seat)
            Q_UNUSED(serial)
            if (!isResizable() || isShade()) {
                return;
            }
            if (isMoveResize()) {
                finishMoveResize(false);
            }
            setMoveResizePointerButtonDown(true);
            setMoveOffset(Cursor::pos() - pos());  // map from global
            setInvertedMoveOffset(rect().bottomRight() - moveOffset());
            setUnrestrictedMoveResize(false);
            auto toPosition = [edges] {
                Position pos = PositionCenter;
                if (edges.testFlag(Qt::TopEdge)) {
                    pos = PositionTop;
                } else if (edges.testFlag(Qt::BottomEdge)) {
                    pos = PositionBottom;
                }
                if (edges.testFlag(Qt::LeftEdge)) {
                    pos = Position(pos | PositionLeft);
                } else if (edges.testFlag(Qt::RightEdge)) {
                    pos = Position(pos | PositionRight);
                }
                return pos;
            };
            setMoveResizePointerMode(toPosition());
            if (!startMoveResize())
                setMoveResizePointerButtonDown(false);
            updateCursor();
        }
    );
    connect(shellSurface, &T::maximizedChanged, this,
        [this] (bool maximized) {
            if (m_shellSurface && isFullScreen()) {
                // ignore for wl_shell - there it is mutual exclusive and messes with the geometry
                return;
            }
            maximize(maximized ? MaximizeFull : MaximizeRestore);
        }
    );
    // TODO: consider output!
    connect(shellSurface, &T::fullscreenChanged, this, &ShellClient::clientFullScreenChanged);

    connect(shellSurface, &T::transientForChanged, this, &ShellClient::setTransient);

    connect(this, &ShellClient::geometryChanged, this, &ShellClient::updateClientOutputs);
    connect(screens(), &Screens::changed, this, &ShellClient::handleScreenChanged,
            Qt::QueuedConnection);
    connect(screens(), &Screens::outputResourceChanged, this, &ShellClient::updateClientOutputs,
            Qt::QueuedConnection);

    if (!m_internal) {
        setupWindowRules(false);
    }
    setDesktopFileName(rules()->checkDesktopFile(shellSurface->windowClass(), true).toUtf8());
}

void ShellClient::init()
{
    connect(this, &ShellClient::desktopFileNameChanged, this, &ShellClient::updateIcon);
    findInternalWindow();
    createWindowId();
    setupCompositing();
    updateIcon();
    updateClientOutputs();
    SurfaceInterface *s = surface();
    Q_ASSERT(s);
    if (s->buffer()) {
        setReadyForPainting();
        if (shouldExposeToWindowManagement()) {
            setupWindowManagementInterface();
        }
        m_unmapped = false;
        m_clientSize = s->size();
    } else {
        ready_for_painting = false;
    }
    if (m_internalWindow) {
        updateInternalWindowGeometry();
        updateDecoration(true);
        setWindowRadius(QPointF(8, 8));
    } else {
        doSetGeometry(QRect(QPoint(0, 0), m_clientSize));
    }
    if (waylandServer()->inputMethodConnection() == s->client()) {
        m_windowType = NET::OnScreenDisplay;
    }

    connect(s, &SurfaceInterface::sizeChanged, this,
        [this] {
            m_clientSize = surface()->size();
            QRect rect = QRect(geom.topLeft(), m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
            // restore m_client_size when client start with setGeometry before showFullScreen, if not the restore will be empty
            if (isFullScreen() && !surface()->size().isEmpty() && m_geomFsRestore.isEmpty()) {
                m_geomFsRestore.setSize(m_clientSize);
            }
            doSetGeometry(rect);
            if (m_requestedMaximizeMode == MaximizeMode::MaximizeRestore && !isSplitscreen())
                setGeometryRestore(rect);

	    if ((!isSpecialWindow() || isToolbar()) && !isFullScreen()) {
                QRect area = workspace()->clientArea(WorkArea, this);
                if(keepAbove())
                    keepInArea(workspace()->clientArea(FullArea, this));
                else
                    keepInArea(area);
            }
        }
    );
    connect(s, &SurfaceInterface::dragPositionChanged, this,
        [this] (const QPointF &globalPos) {
            QPoint pos(globalPos.x(), globalPos.y());
            QRect rect = QRect(pos, m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
            doSetGeometry(rect);
        }
    );
    connect(s, &SurfaceInterface::unmapped, this, &ShellClient::unmap);
    connect(s, &SurfaceInterface::unbound, this, &ShellClient::destroyClient);
    connect(s, &SurfaceInterface::destroyed, this, &ShellClient::destroyClient);
    if (m_shellSurface) {
        initSurface(m_shellSurface);
        auto setPopup = [this] {
            // TODO: verify grab serial
            m_hasPopupGrab = m_shellSurface->isPopup();
        };
        connect(m_shellSurface, &ShellSurfaceInterface::popupChanged, this, setPopup);
        setPopup();
    } else if (m_xdgShellSurface) {
        initSurface(m_xdgShellSurface);

        auto global = static_cast<XdgShellInterface *>(m_xdgShellSurface->global());
        connect(global, &XdgShellInterface::pingDelayed,
            this, [this](qint32 serial) {
                auto it = m_pingSerials.find(serial);
                if (it != m_pingSerials.end()) {
                    qCDebug(KWIN_CORE) << "First ping timeout:" << caption();
                    setUnresponsive(true);
                }
            });

        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::configureAcknowledged, this, [this](int serial) {
           m_lastAckedConfigureRequest = serial;
        });

        connect(global, &XdgShellInterface::pingTimeout,
            this, [this](qint32 serial) {
                auto it = m_pingSerials.find(serial);
                if (it != m_pingSerials.end()) {
                    if (it.value() == PingReason::CloseWindow) {
                        qCDebug(KWIN_CORE) << "Final ping timeout on a close attempt, asking to kill:" << caption();

                        //for internal windows, killing the window will delete this
                        QPointer<QObject> guard(this);
                        killWindow();
                        if (!guard) {
                            return;
                        }
                    }
                    m_pingSerials.erase(it);
                }
            });

        connect(global, &XdgShellInterface::pongReceived,
            this, [this](qint32 serial){
                auto it = m_pingSerials.find(serial);
                if (it != m_pingSerials.end()) {
                    setUnresponsive(false);
                    m_pingSerials.erase(it);
                }
            });

        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::windowMenuRequested, this,
            [this] (SeatInterface *seat, quint32 serial, const QPoint &surfacePos) {
                // TODO: check serial on seat
                Q_UNUSED(seat)
                Q_UNUSED(serial)
                performMouseCommand(Options::MouseOperationsMenu, pos() + surfacePos);
            }
        );
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::minimizeRequested, this,
            [this] {
                performMouseCommand(Options::MouseMinimize, Cursor::pos());
            }
        );
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::minSizeChanged, this,
            [this] (const QSize &size) {
                if (size.width() < 1 && size.height() < 1) {
                    return;
                }
                m_clientMinSize = size;
            }
        );
        connect(m_xdgShellSurface, &XdgShellSurfaceInterface::maxSizeChanged, this,
            [this] (const QSize &size) {
                if (size.width() < 1 && size.height() < 1) {
                    return;
                }
                m_clientMaxSize = size;
            }
        );
        auto configure = [this] {
            if (m_closing) {
                return;
            }
            if (m_requestGeometryBlockCounter != 0 || areGeometryUpdatesBlocked()) {
                return;
            }
            m_xdgShellSurface->configure(xdgSurfaceStates(), m_requestedClientSize);
        };
        configure();
        connect(this, &AbstractClient::activeChanged, this, configure);
        connect(this, &AbstractClient::clientStartUserMovedResized, this, configure);
        connect(this, &AbstractClient::clientFinishUserMovedResized, this, configure);
    } else if (m_xdgShellPopup) {
        connect(m_xdgShellPopup, &XdgShellPopupInterface::grabRequested, this, [this](SeatInterface *seat, quint32 serial) {
            Q_UNUSED(seat)
            Q_UNUSED(serial)
            //TODO - should check the parent had focus
            m_hasPopupGrab = true;
        });

        connect(m_xdgShellPopup, &XdgShellPopupInterface::configureAcknowledged, this, [this](int serial) {
           m_lastAckedConfigureRequest = serial;
        });

        QRect position = QRect(m_xdgShellPopup->transientOffset(), m_xdgShellPopup->initialSize());
        m_xdgShellPopup->configure(position);

        connect(m_xdgShellPopup, &XdgShellPopupInterface::destroyed, this, &ShellClient::destroyClient);
    }

    // set initial desktop
    setDesktop(rules()->checkDesktop(m_internal ? int(NET::OnAllDesktops) : VirtualDesktopManager::self()->current(), true));
    // TODO: merge in checks from Client::manage?
    if (rules()->checkMinimize(false, true)) {
        minimize(true);   // No animation
    }
    setSkipTaskbar(rules()->checkSkipTaskbar(m_plasmaShellSurface ? m_plasmaShellSurface->skipTaskbar() : false, true));
    setSkipPager(rules()->checkSkipPager(false, true));
    setSkipSwitcher(rules()->checkSkipSwitcher(false, true));
    setKeepAbove(rules()->checkKeepAbove(false, true));
    setKeepBelow(rules()->checkKeepBelow(false, true));
    setShortcut(rules()->checkShortcut(QString(), true));

    // setup shadow integration
    getShadow();
    connect(s, &SurfaceInterface::shadowChanged, this, &Toplevel::getShadow);

    connect(waylandServer(), &WaylandServer::foreignTransientChanged, this, [this](KWayland::Server::SurfaceInterface *child) {
        if (child == surface()) {
            setTransient();
        }
    });
    setTransient();

    AbstractClient::updateColorScheme(QString());

    if (!m_internal) {
        discardTemporaryRules();
        applyWindowRules(); // Just in case
        RuleBook::self()->discardUsed(this, false);   // Remove ApplyNow rules
        updateWindowRules(Rules::All); // Was blocked while !isManaged()
    }

    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::dragStarted, this,
        [this] {
            auto seat = waylandServer()->seat();
            if (seat->dragSource() && (surface() == seat->dragSource()->icon()))
            {
                m_isDragWindow = true;
            }
        }
    );

    connect(screens(), &Screens::changed, this, [this]() {
       this->setSplitable(this->checkClientAllowToTile());
    });
}

void ShellClient::destroyClient()
{
    m_closing = true;
    Deleted *del = nullptr;
    if (isMoveResize()) {
        leaveMoveResize();
    }
    if (workspace()) {
        del = Deleted::create(this);
    }
    emit windowClosed(this, del);

    cancelSplitOutline();

    destroyWindowManagementInterface();
    destroyDecoration();

    if (workspace()) {
        StackingUpdatesBlocker blocker(workspace());
        if (transientFor()) {
            transientFor()->removeTransient(this);
        }
        for (auto it = transients().constBegin(); it != transients().constEnd();) {
            if ((*it)->transientFor() == this) {
                removeTransient(*it);
                it = transients().constBegin(); // restart, just in case something more has changed with the list
            } else {
                ++it;
            }
        }
    }
    waylandServer()->removeClient(this);

    if (del) {
        del->unrefWindow();
    }
    m_shellSurface = nullptr;
    m_xdgShellSurface = nullptr;
    m_xdgShellPopup = nullptr;
    deleteClient(this);
}

void ShellClient::deleteClient(ShellClient *c)
{
    if (workspace() != nullptr) {
        workspace()->updateScreenSplitApp(c, true);
        delete c;
    }
}

QStringList ShellClient::activities() const
{
    // TODO: implement
    return QStringList();
}

QPoint ShellClient::clientContentPos() const
{
    return -1 * clientPos();
}

QSize ShellClient::clientSize() const
{
    return m_clientSize;
}

void ShellClient::debug(QDebug &stream) const
{
    stream.nospace();
    stream << "\'ShellClient:" << surface() << ";WMCLASS:" << resourceClass() << ":"
           << resourceName() << ";Caption:" << caption() << "\'";
}

Layer ShellClient::layerForDock() const
{
    if (m_plasmaShellSurface) {
        switch (m_plasmaShellSurface->panelBehavior()) {
        case PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover:
            return NormalLayer;
        case PlasmaShellSurfaceInterface::PanelBehavior::AutoHide:
            return AboveLayer;
        case PlasmaShellSurfaceInterface::PanelBehavior::WindowsGoBelow:
        case PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible:
            return DockLayer;
        default:
            Q_UNREACHABLE();
            break;
        }
    }
    return AbstractClient::layerForDock();
}

QRect ShellClient::transparentRect() const
{
    // TODO: implement
    return QRect();
}

NET::WindowType ShellClient::windowType(bool direct, int supported_types) const
{
    // TODO: implement
    Q_UNUSED(direct)
    Q_UNUSED(supported_types)
    return m_windowType;
}

double ShellClient::opacity() const
{
    return m_opacity;
}

void ShellClient::setOpacity(double opacity)
{
    const qreal newOpacity = qBound(0.0, opacity, 1.0);
    if (newOpacity == m_opacity) {
        return;
    }
    const qreal oldOpacity = m_opacity;
    m_opacity = newOpacity;
    addRepaintFull();
    emit opacityChanged(this, oldOpacity);
}

void ShellClient::addDamage(const QRegion &damage)
{
    auto s = surface();
    if (s->size().isValid()) {
        m_clientSize = s->size();
        updatePendingGeometry();
    }
    workspace()->updatePendingClients(this);
    markAsMapped();
    setDepth((s->buffer()->hasAlphaChannel() && !isDesktop()) ? 32 : 24);
    repaints_region += damage.translated(clientPos());
    Toplevel::addDamage(damage);
}

void ShellClient::setInternalFramebufferObject(const QSharedPointer<QOpenGLFramebufferObject> &fbo)
{
    if (fbo.isNull()) {
        unmap();
        return;
    }

    m_clientSize = fbo->size() / surface()->scale();
    markAsMapped();
    doSetGeometry(QRect(geom.topLeft(), m_clientSize));
    Toplevel::setInternalFramebufferObject(fbo);
    Toplevel::addDamage(QRegion(0, 0, width(), height()));
}

void ShellClient::markAsMapped()
{
    if (!m_unmapped) {
        return;
    }

    m_unmapped = false;
    if (!ready_for_painting) {
        setReadyForPainting();
    } else {
        addRepaintFull();
        emit windowShown(this);
    }

    if (isTransient() && NET::Normal == windowType()) {
        if (workspace() && workspace()->isKwinDebug()) {
            qDebug() << "isTransient" <<geometry()<<resourceClass()<<"surface@"<<surfaceId()<<"windowtype@"<<windowType()<<"layer@"<<layer()<<"pid@"<<pid();
        }
        QRect area = workspace()->clientArea(clientAreaOption::PlacementArea, Screens::self()->current(), desktop());
        placeIn(area);
        if (workspace() && workspace()->isKwinDebug()) {
            qDebug() << "after placeIn" <<geometry()<<resourceClass()<<"surface@"<<surfaceId()<<"windowtype@"<<windowType()<<"layer@"<<layer()<<"pid@"<<pid();
        }
    }

    if (shouldExposeToWindowManagement()) {
        setupWindowManagementInterface();
    }
    updateShowOnScreenEdge();
}

QPoint ShellClient::resetPosition(const QPoint &position, int gravity)
{
    int dx, dy;
    dx = dy = 0;

    if (gravity < 0) {
        // error, use the default gravity
        gravity = getWindowGravity();
    }
    // dx, dy specify how the client window moves to make space for the frame
    switch(gravity) {
    case FRAME_TOP: // titlebar on the top
        dx = 0;
        dy = borderTop();
        break;
    // todo
    // need implement if titlebar is on the bottom/left/right
    default:
        break;
    }

    int x, y;
    x = position.x() - dx;
    y = position.y() - dy;
    // if y < 0, reassign y to 0, so window geometry will not out of screen
    y = (y < 0) ? 0 : y;

    return QPoint(x, y);
}

void ShellClient::createDecoration(const QRect &oldGeom)
{
    KDecoration2::Decoration *decoration = Decoration::DecorationBridge::self()->createDecoration(this);
    if (decoration) {
        QMetaObject::invokeMethod(decoration, "update", Qt::QueuedConnection);
        connect(decoration, &KDecoration2::Decoration::shadowChanged, this, &Toplevel::getShadow);
        connect(decoration, &KDecoration2::Decoration::bordersChanged, this,
            [this]() {
                GeometryUpdatesBlocker blocker(this);
                RequestGeometryBlocker requestBlocker(this);
                QRect oldgeom = geometry();
                if (!isShade())
                    checkWorkspacePosition(oldgeom);
                emit geometryShapeChanged(this, oldgeom);
            }
        );
        if (m_noTitleBar != -1) {
            disconnect(m_ddeShellSurface, &DDEShellSurfaceInterface::noTitleBarPropertyRequested, this, nullptr);
            emit m_ddeShellSurface->noTitleBarPropertyRequested(m_noTitleBar);
        }
        if (m_isSetWindowRadius) {
            disconnect(m_ddeShellSurface, &KWayland::Server::DDEShellSurfaceInterface::windowRadiusPropertyRequested, this, nullptr);
            emit m_ddeShellSurface->windowRadiusPropertyRequested(m_windowRadius);
        }
    }
    setDecoration(decoration);
    // TODO: ensure the new geometry still fits into the client area (e.g. maximized windows)
    QPoint newPosition = resetPosition(oldGeom.topLeft(), getWindowGravity());
    doSetGeometry(QRect(newPosition, m_clientSize + (decoration ? QSize(decoration->borderLeft() + decoration->borderRight(),
                                                               decoration->borderBottom() + decoration->borderTop()) : QSize())));

    emit geometryShapeChanged(this, oldGeom);
}

void ShellClient::updateDecoration(bool check_workspace_pos, bool force)
{
    if (!force &&
            ((!isDecorated() && noBorder()) || (isDecorated() && !noBorder())))
        return;
    QRect oldgeom = geometry();
    QRect oldClientGeom = oldgeom.adjusted(borderLeft(), borderTop(), -borderRight(), -borderBottom());
    blockGeometryUpdates(true);
    if (force)
        destroyDecoration();
    if (!noBorder()) {
        createDecoration(oldgeom);
    } else
        destroyDecoration();
    if (m_serverDecoration && isDecorated()) {
        m_serverDecoration->setMode(KWayland::Server::ServerSideDecorationManagerInterface::Mode::Server);
    }
    if (m_xdgDecoration) {
        auto mode = isDecorated() || m_userNoBorder ? XdgDecorationInterface::Mode::ServerSide: XdgDecorationInterface::Mode::ClientSide;
        m_xdgDecoration->configure(mode);
        m_xdgShellSurface->configure(xdgSurfaceStates(), m_requestedClientSize);
    }
    getShadow();
    if (check_workspace_pos)
        checkWorkspacePosition(oldgeom, -2, oldClientGeom);
    blockGeometryUpdates(false);
    setGeometryRestore(geometry());
}

void ShellClient::setGeometry(int x, int y, int w, int h, ForceGeometry_t force)
{
    if (areGeometryUpdatesBlocked()) {
        // when the GeometryUpdateBlocker exits the current geom is passed to setGeometry
        // thus we need to set it here.
        geom = QRect(x, y, w, h);
        m_cfgGeom = geom;
        if (pendingGeometryUpdate() == PendingGeometryForced)
            {} // maximum, nothing needed
        else if (force == ForceGeometrySet)
            setPendingGeometryUpdate(PendingGeometryForced);
        else
            setPendingGeometryUpdate(PendingGeometryNormal);
        return;
    }
    if (pendingGeometryUpdate() != PendingGeometryNone) {
        // reset geometry to the one before blocking, so that we can compare properly
        geom = geometryBeforeUpdateBlocking();
    }
    // TODO: better merge with Client's implementation
    const QSize requestedClientSize = QSize(w, h) - QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
    if (requestedClientSize == m_clientSize && !isWaitingForMoveResizeSync()) {
        // size didn't change, update directly
        doSetGeometry(QRect(x, y, w, h));
        updateMaximizeMode(m_requestedMaximizeMode);
    } else {
        // size did change, Client needs to provide a new buffer
        requestGeometry(QRect(x, y, w, h));
    }
}

void ShellClient::doSetGeometry(const QRect &rect)
{
    m_screenRect = screens()->geometry(screens()->number(QPoint(rect.x(), rect.y())));
    if (!m_geomMaximizeRestore.isEmpty() && geom == rect && pendingGeometryUpdate() == PendingGeometryNone) {
        //  workaround to avoid screen flash when partial update enabled
        if (!m_unmapped) {
            addWorkspaceRepaint(visibleRect());
        }
        return;
    }
    if (!m_unmapped) {
        addWorkspaceRepaint(visibleRect());
    }
    geom = rect;
    if (m_geomMaximizeRestore.isEmpty() && !geom.isEmpty()) {
        //maximizable window should wait window mapped to change maximizeMode
        if (!m_unmapped) {
            const QRect clientArea = isElectricBorderMaximizing() ?
                    workspace()->clientArea(MaximizeArea, Cursor::pos(), desktop()) :
                    workspace()->clientArea(MaximizeArea, this);
            if (isMaximizable() && (geom.size() == clientArea.size() || m_clientSize == clientArea.size())) {
                maximize(MaximizeFull);
                updateMaximizeMode(MaximizeFull);
            } else {
                m_geomMaximizeRestore = geom;
            }
        } else {
            if (!isMaximizable()) {
                // use first valid geometry as restore geometry
                m_geomMaximizeRestore = geom;
            }
        }
    }

    if (!m_unmapped) {
        addWorkspaceRepaint(visibleRect());
    }
    syncGeometryToInternalWindow();
    if (hasStrut() || isSplitscreen()) {
        workspace()->updateClientArea();
    }
    const auto old = geometryBeforeUpdateBlocking();
    updateGeometryBeforeUpdateBlocking();
    emit geometryShapeChanged(this, old);

    if (isResize()) {
        performMoveResize();
    }
}

void ShellClient::doMove(int x, int y)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    syncGeometryToInternalWindow();
}

void ShellClient::leaveMoveResize() {
    AbstractClient::leaveMoveResize();
    if (m_plasmaShellSurface) {
        m_plasmaShellSurface->resetPositionSet();
    }
}

void ShellClient::clearPendingRequest()
{
    if (isWaitingForMoveResizeSync()) {
        m_pendingConfigureRequests.clear();
    }
}

void ShellClient::adjustClientMinSize(const Position mode)
{
    QRect orig = initialMoveResizeGeometry();
    QRect moveResizeRect= moveResizeGeometry();

    if (m_clientMinSize.width() > 0 && m_clientMinSize.height() > 0 && m_clientMinSize == m_clientMaxSize) {
        setMoveResizeGeometry(orig);
        return;
    }
    QSize clientMinSize = m_clientMinSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
    QSize clientMaxSize = m_clientMaxSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
    if (m_clientMinSize.width() > 0) {
        if ((mode & PositionLeft) && moveResizeRect.width() < clientMinSize.width()) {
            moveResizeRect.setWidth(clientMinSize.width());
            moveResizeRect.moveRight(orig.right());
        }
        if ((mode & PositionRight) && moveResizeRect.width() < clientMinSize.width()) {
            moveResizeRect.setWidth(clientMinSize.width());
        }
    }
    if (m_clientMinSize.height() > 0) {
        if ((mode & PositionTop) && moveResizeRect.height() < clientMinSize.height()) {
            moveResizeRect.setHeight(clientMinSize.height());
            moveResizeRect.moveBottom(orig.bottom());
        }
        if ((mode & PositionBottom) && moveResizeRect.height() < clientMinSize.height()) {
            moveResizeRect.setHeight(clientMinSize.height());
        }
    }
    if (m_clientMaxSize.width() > 0) {
        if ((mode & PositionLeft) && moveResizeRect.width() > clientMaxSize.width()) {
            moveResizeRect.setWidth(clientMaxSize.width());
            moveResizeRect.moveRight(orig.right());
        }
        if ((mode & PositionRight) && moveResizeRect.width() > clientMaxSize.width()) {
            moveResizeRect.setWidth(clientMaxSize.width());
        }
    }
    if (m_clientMaxSize.height() > 0) {
        if ((mode & PositionTop) && moveResizeRect.height() > clientMaxSize.height()) {
            moveResizeRect.setHeight(clientMaxSize.height());
            moveResizeRect.moveBottom(orig.bottom());
        }
        if ((mode & PositionBottom) && moveResizeRect.height() > clientMaxSize.height()) {
            moveResizeRect.setHeight(clientMaxSize.height());
        }
    }
    setMoveResizeGeometry(moveResizeRect);
}

void ShellClient::syncGeometryToInternalWindow()
{
    if (!m_internalWindow) {
        return;
    }
    const QRect windowRect = QRect(geom.topLeft() + QPoint(borderLeft(), borderTop()),
                                    geom.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
    if (m_internalWindow->geometry() != windowRect) {
        // delay to end of cycle to prevent freeze, see BUG 384441
        QTimer::singleShot(0, m_internalWindow, std::bind(static_cast<void (QWindow::*)(const QRect&)>(&QWindow::setGeometry), m_internalWindow, windowRect));
    }
}

QByteArray ShellClient::windowRole() const
{
    return QByteArray();
}

bool ShellClient::belongsToSameApplication(const AbstractClient *other, SameApplicationChecks checks) const
{
    if (checks.testFlag(SameApplicationCheck::AllowCrossProcesses)) {
        if (other->desktopFileName() == desktopFileName()) {
            return true;
        }
    }
    if (auto s = other->surface()) {
        return s->client() == surface()->client();
    }
    return false;
}

void ShellClient::blockActivityUpdates(bool b)
{
    Q_UNUSED(b)
}

void ShellClient::updateCaption()
{
    const QString oldSuffix = m_captionSuffix;
    const auto shortcut = shortcutCaptionSuffix();
    m_captionSuffix = shortcut;
    if ((!isSpecialWindow() || isToolbar()) && findClientWithSameCaption()) {
        int i = 2;
        do {
            m_captionSuffix = shortcut + QLatin1String(" <") + QString::number(i) + QLatin1Char('>');
            i++;
        } while (findClientWithSameCaption());
    }
    if (m_captionSuffix != oldSuffix) {
        emit captionChanged();
    }
}

void ShellClient::closeWindow()
{
    if (workspace() && surface() && isCloseable()) {
        wl_resource* surfaceResource = surface()->resource();
        if (surfaceResource) {
            workspace()->delWindowProperty(surfaceResource);
        }
    }

    if (m_xdgShellSurface && isCloseable()) {
        m_xdgShellSurface->close();
        const qint32 pingSerial = static_cast<XdgShellInterface *>(m_xdgShellSurface->global())->ping(m_xdgShellSurface);
        m_pingSerials.insert(pingSerial, PingReason::CloseWindow);
    } else if (m_qtExtendedSurface && isCloseable()) {
        m_qtExtendedSurface->close();
    } else if (m_internalWindow) {
        m_internalWindow->hide();
    }
}

AbstractClient *ShellClient::findModal(bool allow_itself)
{
    AbstractClient* root = this;
    auto p = transientFor();
    while (p) {
        root = p;
        p = p->transientFor();
    }

    QVector<AbstractClient*> mods;
    QStack<AbstractClient*> stack;
    stack.push(root);
    while (!stack.empty()) {
        auto *cur = stack.top();
        stack.pop();
        if (cur->isModal())
            mods.push_back(cur);
        for (auto it = cur->transients().constBegin(); it != cur->transients().constEnd(); ++it)
            stack.push(*it);
    }

    if (mods.count() > 0)
        return mods.last();
    else 
        return nullptr;
}

bool ShellClient::isCloseable() const
{
    if (m_windowType == NET::Desktop || m_windowType == NET::Dock) {
        return false;
    }
    if (m_xdgShellSurface) {
        return true;
    }
    if (m_internal) {
        return true;
    }
    return m_qtExtendedSurface ? true : false;
}

bool ShellClient::isFullScreen() const
{
    return m_fullScreen;
}

bool ShellClient::isMaximizable() const
{
    if (workspace()->userActionsMenu()->isShown()) {
        return false;
    }
    if (!isResizable()) {
        return false;
    }
    if (m_internal || !m_maxmizable) {
        return false;
    }
    if (!transients().isEmpty()) {
        for (auto it = transients().constBegin(),
                                  end = transients().constEnd(); it != end; ++it) {
            if ((*it)->isTooltip())
                return false;
        }
    }
    return true;
}

bool ShellClient::isMinimizable(bool isMinFunc) const
{
    if (isTransient()) {
        bool shown_mainwindow = false;
        auto mainclients = mainClients();
        for (auto it = mainclients.constBegin();
                it != mainclients.constEnd();
                ++it)
            if ((*it)->isShown(true))
                shown_mainwindow = true;
        if (!shown_mainwindow)
            return true;
    }
    if (m_internal || !m_minimizable) {
        return false;
    }
    return (!m_plasmaShellSurface || m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal);
}

bool ShellClient::isMovable() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool ShellClient::isMovableAcrossScreens() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool ShellClient::isResizable() const
{
    if (!m_resizable || (m_clientMinSize.isValid() && !m_clientMinSize.isNull() && (m_clientMinSize == m_clientMaxSize))) {
        return false;
    }

    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Normal;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    return true;
}

bool ShellClient::isShown(bool shaded_is_shown) const
{
    Q_UNUSED(shaded_is_shown)
    return !m_closing && !m_unmapped && !isMinimized() && !m_hidden;
}

void ShellClient::hideClient(bool hide)
{
    if (m_hidden == hide) {
        return;
    }
    m_hidden = hide;
    if (hide) {
        addWorkspaceRepaint(visibleRect());
        workspace()->clientHidden(this);
        emit windowHidden(this);
    } else {
        emit windowShown(this);
    }
}

static bool changeMaximizeRecursion = false;
void ShellClient::changeMaximize(bool horizontal, bool vertical, bool adjust)
{
    if (changeMaximizeRecursion) {
        return;
    }

    if (!isResizable()) {
        return;
    }

    const QRect clientArea = isElectricBorderMaximizing() ?
        workspace()->clientArea(MaximizeArea, Cursor::pos(), desktop()) :
        workspace()->clientArea(MaximizeArea, this);

    const MaximizeMode oldMode = m_requestedMaximizeMode;
    const QRect oldGeometry = geometry();

    StackingUpdatesBlocker blocker(workspace());
    if (!isMoveResize()) {
        RequestGeometryBlocker geometryBlocker(this);
    }

    // 'adjust == true' means to update the size only, e.g. after changing workspace size
    if (!adjust) {
        if (vertical)
            m_requestedMaximizeMode = MaximizeMode(m_requestedMaximizeMode ^ MaximizeVertical);
        if (horizontal)
            m_requestedMaximizeMode = MaximizeMode(m_requestedMaximizeMode ^ MaximizeHorizontal);
    }
    // TODO: add more checks as in Client

    // call into decoration update borders
    if (isDecorated() && decoration()->client() && !(options->borderlessMaximizedWindows() && m_requestedMaximizeMode == KWin::MaximizeFull)) {
        changeMaximizeRecursion = true;
        const auto c = decoration()->client().data();
        if ((m_requestedMaximizeMode & MaximizeVertical) != (oldMode & MaximizeVertical)) {
            emit c->maximizedVerticallyChanged(m_requestedMaximizeMode & MaximizeVertical);
        }
        if ((m_requestedMaximizeMode & MaximizeHorizontal) != (oldMode & MaximizeHorizontal)) {
            emit c->maximizedHorizontallyChanged(m_requestedMaximizeMode & MaximizeHorizontal);
        }
        if ((m_requestedMaximizeMode == MaximizeFull) != (oldMode == MaximizeFull)) {
            emit c->maximizedChanged(m_requestedMaximizeMode & MaximizeFull);
        }
        changeMaximizeRecursion = false;
    }

    if (options->borderlessMaximizedWindows()) {
        // triggers a maximize change.
        // The next setNoBorder interation will exit since there's no change but the first recursion pullutes the restore geometry
        changeMaximizeRecursion = true;
        setNoBorder(rules()->checkNoBorder(m_requestedMaximizeMode == MaximizeFull));
        changeMaximizeRecursion = false;
    }

    // Conditional quick tiling exit points
    const auto oldQuickTileMode = quickTileMode();
    if (quickTileMode() != QuickTileMode(QuickTileFlag::None)) {
        if (oldMode == MaximizeFull &&
                !clientArea.contains(m_geomMaximizeRestore.center())) {
            // Not restoring on the same screen
            // TODO: The following doesn't work for some reason
            //quick_tile_mode = QuickTileNone; // And exit quick tile mode manually
        } else if ((oldMode == MaximizeVertical && m_requestedMaximizeMode == MaximizeRestore) ||
                  (oldMode == MaximizeFull && m_requestedMaximizeMode == MaximizeHorizontal)) {
            // Modifying geometry of a tiled window
            updateQuickTileMode(QuickTileFlag::None); // Exit quick tile mode without restoring geometry
        }
    }

    // TODO: check rules
    if (m_requestedMaximizeMode == MaximizeFull) {
        if (!m_geomMaximizeRestore.isValid()) {
            m_geomMaximizeRestore = oldGeometry;
        }
        // TODO: Client has more checks
        if (options->electricBorderMaximize()) {
            updateQuickTileMode(QuickTileFlag::Maximize);
        } else {
            updateQuickTileMode(QuickTileFlag::None);
        }
        if (quickTileMode() != oldQuickTileMode) {
            emit quickTileModeChanged();
        }
        setGeometry(workspace()->clientArea(MaximizeArea, this));
        workspace()->raiseClient(this);
    } else {
        if (m_requestedMaximizeMode == MaximizeRestore) {
            updateQuickTileMode(QuickTileFlag::None);
        }
        if (quickTileMode() != oldQuickTileMode) {
            emit quickTileModeChanged();
        }

        if (m_geomMaximizeRestore.isValid()) {
            auto maxArea = workspace()->clientArea(MaximizeArea, this);
            if (m_geomMaximizeRestore.width() == maxArea.width() && m_geomMaximizeRestore.height() == maxArea.height()) {
                QSize calSize = calculateClientSize(geometry().size());
                m_geomMaximizeRestore = QRect(m_geomMaximizeRestore.topLeft(), calSize);
            }
            setGeometry(m_geomMaximizeRestore);
        } else {
            setGeometry(workspace()->clientArea(PlacementArea, this));
        }
    }
}

QSize ShellClient::calculateClientSize(const QSize& wsize)
{
    if (wsize.isEmpty())
        return m_clientMinSize;
    int w = wsize.width();
    int h = wsize.height();

    w = qMax(m_clientMinSize.width(), w * SHOW_SCALE);
    h = qMax(m_clientMinSize.height(), h * SHOW_SCALE);
    if (!m_clientMaxSize.isEmpty()) {
        w = qMin(m_clientMaxSize.width(), w);
        h = qMin(m_clientMaxSize.height(), h);
    }

    return QSize(w,h);
}

MaximizeMode ShellClient::maximizeMode() const
{
    return m_maximizeMode;
}

MaximizeMode ShellClient::requestedMaximizeMode() const
{
    return m_requestedMaximizeMode;
}

bool ShellClient::noBorder() const
{
    if (isInternal()) {
        return m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
    }
    if (m_serverDecoration) {
        if (m_serverDecoration->mode() == ServerSideDecorationManagerInterface::Mode::Server) {
            return m_userNoBorder || isFullScreen();
        }
    }
    if (m_xdgDecoration && m_xdgDecoration->requestedMode() != XdgDecorationInterface::Mode::ClientSide) {
        return m_userNoBorder || isFullScreen();
    }
    return true;
}

void ShellClient::setFullScreen(bool set, bool user)
{
    if (!isFullScreen() && !set)
        return;
    if (user && !userCanSetFullScreen())
        return;
    //当由双屏变回单屏时需要判断 m_geomFsRestore的位置是否在当前的Screen上，如果不在则需要调整
    if (screens()->count()==1 && !screens()->geometry().intersects(m_geomFsRestore)) {
        m_geomFsRestore.moveTo(0,0);
    }
    set = rules()->checkFullScreen(set && !isSpecialWindow());
    setShade(ShadeNone);
    bool was_fs = isFullScreen();
    if (was_fs) {
        workspace()->updateFocusMousePosition(Cursor::pos()); // may cause leave event
    } else {
        // in shell surface, maximise mode and fullscreen are exclusive
        // fullscreen->toplevel should restore the state we had before maximising
        if ((m_shellSurface || m_xdgShellSurface) && m_maximizeMode == MaximizeMode::MaximizeFull) {
            m_geomFsRestore = m_geomMaximizeRestore;
        } else {
            m_geomFsRestore = geometry();
        }
    }
    m_fullScreen = set;
    if (was_fs == isFullScreen())
        return;
    if (set) {
        untab();
        workspace()->raiseClient(this);
    }
    RequestGeometryBlocker requestBlocker(this);
    StackingUpdatesBlocker blocker1(workspace());
    GeometryUpdatesBlocker blocker2(this);
    setActive(true);
    //workaround: if client is fullscrenn with shouldgetfocus, it will never change layers to show other windows
//    workspace()->setShouldGetFocus(this);
    invalidateLayer();
    workspace()->updateClientLayer(this);   // active fullscreens get different layer
    qDebug() << "---------" << __func__ << layer() << belongsToLayer() << isFullScreen()
        << workspace()->mostRecentlyActivatedClient();
    updateDecoration(false, false);
    if (isFullScreen()) {
        setGeometry(workspace()->clientArea(FullScreenArea, this));
    } else {
        if (!m_geomFsRestore.isNull()) {
            int currentScreen = screen();
            if (m_maximizeMode == MaximizeRestore) {
                m_geomMaximizeRestore = m_geomFsRestore;
                setGeometry(QRect(m_geomFsRestore.topLeft(), adjustedSize(m_geomFsRestore.size())));
            } else {
                setGeometry(workspace()->clientArea(MaximizeArea, this));
                m_geomMaximizeRestore = m_geomFsRestore;
            }
            if (currentScreen != screen())
                workspace()->sendClientToScreen( this, currentScreen );
        } else {
            // does this ever happen?
            setGeometry(workspace()->clientArea(MaximizeArea, this));
        }
    }
    updateWindowRules(Rules::Fullscreen|Rules::Position|Rules::Size);

    cancelSplitOutline();

    if (was_fs != isFullScreen()) {
        emit fullScreenChanged();
        emit workspace()->windowStateChanged();
    }
}

void ShellClient::setNoBorder(bool set)
{
    if (!userCanSetNoBorder()) {
        return;
    }
    set = rules()->checkNoBorder(set);
    if (m_userNoBorder == set) {
        return;
    }
    m_userNoBorder = set;
    updateDecoration(true, false);
    updateWindowRules(Rules::NoBorder);
}

void ShellClient::setOnAllActivities(bool set)
{
    Q_UNUSED(set)
}

void ShellClient::takeFocus()
{
    if (rules()->checkAcceptFocus(wantsInput())) {
        if (m_xdgShellSurface) {
            const qint32 pingSerial = static_cast<XdgShellInterface *>(m_xdgShellSurface->global())->ping(m_xdgShellSurface);
            m_pingSerials.insert(pingSerial, PingReason::FocusWindow);
        }
        setActive(true);
    }

    bool breakShowingDesktop = !(m_windowType == NET::Override);
    if (breakShowingDesktop) {
        // check that it doesn't belong to the desktop
        const auto &clients = waylandServer()->clients();
        for (auto c: clients) {
            if (!belongsToSameApplication(c, SameApplicationChecks())) {
                continue;
            }
            // add isMovable to check the current client is a desktop rather than an attribute window
            // if current client is attribute window, breakShowingDesktop is true, show all attribute window.
            if (c->isDesktop() && !isMovable()) {
                breakShowingDesktop = false;
                break;
            }
            if (c->isDock() && !isMovable()) {
                breakShowingDesktop = false;
                break;
            }
        }
    }
    if (breakShowingDesktop) {
        if (workspace()->showingDesktop()) {
            // minimize all other windows
            for (AbstractClient *c : workspace()->allClientList()) {
                if (this == c || c->isDock() || c->isDesktop() || skipTaskbar()) {
                    continue;
                }

                //if 'this' is dialog , it's parent cannot minimize
                if (this->transientFor() == c)
                    continue;
                // c is dialog and it's child of this ,cannot minimize
                if (c->transientFor() != this)
                    c->minimize(true);
            }

            workspace()->setShowingDesktop(false);
        }
    }
}

void ShellClient::doSetActive()
{
    if (!isActive()) {
        return;
    }
    StackingUpdatesBlocker blocker(workspace());
    workspace()->focusToNull();
}

bool ShellClient::userCanSetFullScreen() const
{
    if (m_xdgShellSurface) {
        return true;
    }
    return false;
}

bool ShellClient::userCanSetNoBorder() const
{
    if (m_serverDecoration && m_serverDecoration->mode() == ServerSideDecorationManagerInterface::Mode::Server) {
        return !isFullScreen() && !isShade() && !tabGroup();
    }
    if (m_xdgDecoration && m_xdgDecoration->requestedMode() != XdgDecorationInterface::Mode::ClientSide) {
        return !isFullScreen() && !isShade() && !tabGroup();
    }
    if (m_internal) {
        return !m_internalWindowFlags.testFlag(Qt::FramelessWindowHint) || m_internalWindowFlags.testFlag(Qt::Popup);
    }
    return false;
}

bool ShellClient::wantsInput() const
{
    return rules()->checkAcceptFocus(acceptsFocus());
}

bool ShellClient::acceptsFocus() const
{
    if (isInternal()) {
        return false;
    }
    if (waylandServer()->inputMethodConnection() == surface()->client()) {
        return false;
    }
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::ToolTip ||
            m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Notification) {
            return false;
        }
    }
    if (m_closing) {
        // a closing window does not accept focus
        return false;
    }
    if (m_unmapped) {
        // an unmapped window does not accept focus
        return false;
    }

    //  Just return false. In case return false finally if m_acceptFoucus is true.
    if (!m_ddeShellSurface.isNull() && !m_acceptFocus) {
        return m_acceptFocus;
    }
    if (m_shellSurface) {
        if (m_shellSurface->isPopup()) {
            return false;
        }
        return m_shellSurface->acceptsKeyboardFocus();
    }
    if (m_xdgShellSurface) {
        // TODO: proper
        return true;
    }

    // grab popup need accept focus
    if (m_xdgShellPopup && m_hasPopupGrab) {
        return true;
    }

    return false;
}

void ShellClient::createWindowId()
{
    if (m_internalWindow) {
        m_windowId = m_internalWindow->winId();
    } else {
        m_windowId = waylandServer()->createWindowId(surface());
    }
}

void ShellClient::findInternalWindow()
{
    if (surface()->client() != waylandServer()->internalConnection()) {
        return;
    }
    const QWindowList windows = kwinApp()->topLevelWindows();
    for (QWindow *w: windows) {
        auto s = KWayland::Client::Surface::fromWindow(w);
        if (!s) {
            continue;
        }
        if (s->id() != surface()->id()) {
            continue;
        }
        m_internalWindow = w;
        m_internalWindowFlags = m_internalWindow->flags();
        connect(m_internalWindow, &QWindow::xChanged, this, &ShellClient::updateInternalWindowGeometry);
        connect(m_internalWindow, &QWindow::yChanged, this, &ShellClient::updateInternalWindowGeometry);
        connect(m_internalWindow, &QWindow::destroyed, this, [this] { m_internalWindow = nullptr; });
        connect(m_internalWindow, &QWindow::opacityChanged, this, &ShellClient::setOpacity);

        // Try reading the window type from the QWindow. PlasmaCore.Dialog provides a dynamic type property
        // let's check whether it exists, if it does it's our window type
        const QVariant windowType = m_internalWindow->property("type");
        if (!windowType.isNull()) {
            m_windowType = static_cast<NET::WindowType>(windowType.toInt());
        }
        const QVariant title = m_internalWindow->property("title");
        if(!title.isNull()) {
            m_caption = title.toString();
        }
        setOpacity(m_internalWindow->opacity());

        // skip close animation support
        setSkipCloseAnimation(m_internalWindow->property(s_skipClosePropertyName).toBool());
        m_internalWindow->installEventFilter(this);
        return;
    }
}

void ShellClient::updateInternalWindowGeometry()
{
    if (!m_internalWindow) {
        return;
    }
    doSetGeometry(QRect(m_internalWindow->geometry().topLeft() - QPoint(borderLeft(), borderTop()),
                        m_internalWindow->geometry().size() + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
}

pid_t ShellClient::pid() const
{
    return surface()->client()->processId();
}

bool ShellClient::isInternal() const
{
    return m_internal;
}

// if we want to do some special action only for Role::StandAlone surface
// we first call isStandAlone
bool ShellClient::isStandAlone() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::StandAlone;
    }
    return false;
}

bool ShellClient::isOverride() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Override;
    }
    return false;
}

bool ShellClient::isActiveFullScreenRole() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::ActiveFullScreen;
    }
    return false;
}

bool ShellClient::isLockScreen() const
{
    if (m_internalWindow) {
        return m_internalWindow->property("org_kde_ksld_emergency").toBool();
    }
    return surface()->client() == waylandServer()->screenLockerClientConnection();
}

bool ShellClient::isInputMethod() const
{
    if (m_internal && m_internalWindow) {
        return m_internalWindow->property("__kwin_input_method").toBool();
    }
    return surface()->client() == waylandServer()->inputMethodConnection();
}

void ShellClient::requestGeometry(const QRect &rect)
{
    if (m_requestGeometryBlockCounter != 0) {
        m_blockedRequestGeometry = rect;
        return;
    }
    PendingConfigureRequest configureRequest;
    configureRequest.positionAfterResize = rect.topLeft();
    configureRequest.maximizeMode = m_requestedMaximizeMode;

    const QSize size = rect.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom());
    m_requestedClientSize = size;

    if (m_shellSurface) {
        m_shellSurface->requestSize(size);
    }
    if (m_xdgShellSurface) {
        configureRequest.serialId = m_xdgShellSurface->configure(xdgSurfaceStates(), size);
    }
    if (m_xdgShellPopup) {
        auto parent = transientFor();
        if (parent) {
            const QPoint globalClientContentPos = parent->geometry().topLeft() + parent->clientPos();
            const QPoint relativeOffset = rect.topLeft() -globalClientContentPos;
            configureRequest.serialId = m_xdgShellPopup->configure(QRect(relativeOffset, rect.size()));
        }
    }

    m_pendingConfigureRequests.append(configureRequest);

    m_blockedRequestGeometry = QRect();
    if (m_internal) {
        m_internalWindow->setGeometry(QRect(rect.topLeft() + QPoint(borderLeft(), borderTop()), rect.size() - QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    }
}

void ShellClient::updatePendingGeometry()
{
    QPoint position = geom.topLeft();
    MaximizeMode maximizeMode = m_maximizeMode;
    for (auto it = m_pendingConfigureRequests.begin(); it != m_pendingConfigureRequests.end(); it++) {
        if (it->serialId > m_lastAckedConfigureRequest) {
            //this serial is not acked yet, therefore we know all future serials are not
            break;
        }
        if (it->serialId == m_lastAckedConfigureRequest) {
            if (position != it->positionAfterResize) {
                addLayerRepaint(geometry());
            }
            position = it->positionAfterResize;
            maximizeMode = it->maximizeMode;

            m_pendingConfigureRequests.erase(m_pendingConfigureRequests.begin(), ++it);
            break;
        }
        //else serialId < m_lastAckedConfigureRequest and the state is now irrelevant and can be ignored
    }
    if(!areGeometryUpdatesBlocked()) {
        doSetGeometry(QRect(position, m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    }

    updateMaximizeMode(maximizeMode);
}

void ShellClient::clientFullScreenChanged(bool fullScreen)
{
    setFullScreen(fullScreen, false);
}

void ShellClient::resizeWithChecks(int w, int h, ForceGeometry_t force)
{
    Q_UNUSED(force)
    QRect area = workspace()->clientArea(WorkArea, this);
    // don't allow growing larger than workarea
    if (w > area.width()) {
        w = area.width();
    }
    if (h > area.height()) {
        h = area.height();
    }
    if (m_shellSurface) {
        m_shellSurface->requestSize(QSize(w, h));
    }
    if (m_xdgShellSurface) {
        m_xdgShellSurface->configure(xdgSurfaceStates(), QSize(w, h));
    }
    if (m_internal) {
        m_internalWindow->setGeometry(QRect(pos() + QPoint(borderLeft(), borderTop()), QSize(w, h) - QSize(borderLeft() + borderRight(), borderTop() + borderBottom())));
    }
}

void ShellClient::unmap()
{
    m_unmapped = true;
    m_requestedClientSize = QSize(0, 0);
    destroyWindowManagementInterface();
    if (Workspace::self()) {
        addWorkspaceRepaint(visibleRect());
        workspace()->clientHidden(this);
    }
    emit windowHidden(this);
}

void ShellClient::installDDEShellSurface(DDEShellSurfaceInterface *shellSurface)
{
    m_ddeShellSurface = shellSurface;

    connect(this, &ShellClient::geometryChanged, this,
        [this] {
            if (isDecorated()) {
                QRect clientGeom(geom.topLeft() + clientPos(), clientSize());
                m_ddeShellSurface->sendGeometry(clientGeom);
            } else {
                m_ddeShellSurface->sendGeometry(geom);
            }
        }
    );

    connect(this, &ShellClient::geometryChanged, this, &ShellClient::updateClientOutputs);

    connect(this, &AbstractClient::activeChanged, this,
            [this] {
                m_ddeShellSurface->setActive(isActive());
                emit workspace()->windowStateChanged();
                }
            );
    connect(this, &AbstractClient::fullScreenChanged, this,
            [this] {
                m_ddeShellSurface->setFullscreen(isFullScreen());
                emit workspace()->windowStateChanged();
                }
            );
    connect(this, &AbstractClient::keepAboveChanged, m_ddeShellSurface, &DDEShellSurfaceInterface::setKeepAbove);
    connect(this, &AbstractClient::keepBelowChanged, m_ddeShellSurface, &DDEShellSurfaceInterface::setKeepBelow);
    connect(this, &AbstractClient::minimizedChanged, this,
            [this] {
                m_ddeShellSurface->setMinimized(isMinimized());
                emit workspace()->windowStateChanged();
                }
            );
    connect(this, static_cast<void (AbstractClient::*)(AbstractClient*,MaximizeMode)>(&AbstractClient::clientMaximizedStateChanged), this,
        [this] (KWin::AbstractClient *c, MaximizeMode mode) {
            Q_UNUSED(c);
            m_ddeShellSurface->setMaximized(mode == KWin::MaximizeFull);
            emit workspace()->windowStateChanged();
        }
    );

    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::activationRequested, this,
        [this] {
            workspace()->activateClient(reinterpret_cast<AbstractClient*>(this));
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::minimizedRequested, this,
        [this] (bool set) {
            if (set) {
                minimize();
            } else {
                unminimize();
            }
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::maximizedRequested, this,
        [this] (bool set) {
            maximize(set ? MaximizeFull : MaximizeRestore);
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::keepAboveRequested, this,
        [this] (bool set) {
            setKeepAbove(set);
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::keepBelowRequested, this,
        [this] (bool set) {
            setKeepBelow(set);
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::activeRequested, this,
        [this] (bool set) {
            if (set) {
                workspace()->activateClient(reinterpret_cast<AbstractClient*>(this), true);
            }
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::onAllDesktopsRequested, this,
        [this] (bool set) {
            if (set) {
                setOnAllDesktops(set);
            }
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::minimizeableRequested, this,
        [this] (bool set) {
            setMinimizeable(set);
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::maximizeableRequested, this,
        [this] (bool set) {
            setMaximizeable(set);
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::resizableRequested, this,
        [this] (bool set) {
            setResizable(set);
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::acceptFocusRequested, this,
        [this] (bool set) {
            setAcceptFocus(set);
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::modalityRequested, this,
        [this] (bool set) {
            setModal(set);
        }
    );
    connect(m_ddeShellSurface, &DDEShellSurfaceInterface::noTitleBarPropertyRequested, this,
        [this] (qint32 value) {
            if (!decoration()) {
                m_noTitleBar = value;
            }
        }
    );
    connect(m_ddeShellSurface, &KWayland::Server::DDEShellSurfaceInterface::windowRadiusPropertyRequested, this,
        [this] (QPointF windowRadius) {
            m_isSetWindowRadius = true;
            setWindowRadius(windowRadius);
        }
    );
    connect(m_ddeShellSurface, &KWayland::Server::DDEShellSurfaceInterface::splitWindowRequested, this,
        [this] (KWayland::Server::DDEShellSurfaceInterface::SplitType type) {
            cancelSplitOutline();
            workspace()->setClientSplit(reinterpret_cast<AbstractClient*>(this), (int)type, true);
        }
    );
}

void ShellClient::installPlasmaShellSurface(PlasmaShellSurfaceInterface *surface)
{
    m_plasmaShellSurface = surface;
    auto updatePosition = [this, surface] {
        if (isFullScreen()) {
            QPoint newPosition = resetPosition(surface->position(), getWindowGravity());
            m_geomFsRestore.moveTo(newPosition);
            return;
        }
        QPoint newPosition = resetPosition(surface->position(), getWindowGravity());
        QRect rect = QRect(newPosition, m_clientSize + QSize(borderLeft() + borderRight(), borderTop() + borderBottom()));
        setGeometryRestore(QRect(newPosition, m_geomMaximizeRestore.size()));
        // Shell surfaces of internal windows are sometimes desync to current value.
        // Make sure to not set window geometry of internal windows to invalid values (bug 386304)
        if (!m_internal || rect.isValid()) {
            doSetGeometry(rect);
        }
    };
    auto updateRole = [this, surface] {
        NET::WindowType type = NET::Unknown;
        switch (surface->role()) {
        case PlasmaShellSurfaceInterface::Role::Desktop:
            type = NET::Desktop;
            break;
        case PlasmaShellSurfaceInterface::Role::Panel:
            type = NET::Dock;
            break;
        case PlasmaShellSurfaceInterface::Role::OnScreenDisplay:
            type = NET::OnScreenDisplay;
            break;
        case PlasmaShellSurfaceInterface::Role::Notification:
            type = NET::Notification;
            break;
        case PlasmaShellSurfaceInterface::Role::ToolTip:
            type = NET::Tooltip;
            break;
        case PlasmaShellSurfaceInterface::Role::Override:
            type = NET::Override;
            break;
        case PlasmaShellSurfaceInterface::Role::Normal:
        case PlasmaShellSurfaceInterface::Role::StandAlone:
        default:
            type = NET::Normal;
            break;
        }
        if (type != m_windowType) {
            m_windowType = type;
            if (m_windowType == NET::Desktop || type == NET::Dock || type == NET::Notification || type == NET::Tooltip) {
                setOnAllDesktops(true);
            }
            workspace()->updateClientArea();
        }
    };
    connect(surface, &PlasmaShellSurfaceInterface::positionChanged, this, updatePosition);
    connect(surface, &PlasmaShellSurfaceInterface::roleChanged, this, updateRole);
    connect(surface, &PlasmaShellSurfaceInterface::panelBehaviorChanged, this,
        [this] {
            updateShowOnScreenEdge();
            workspace()->updateClientArea();
        }
    );
    connect(surface, &PlasmaShellSurfaceInterface::panelAutoHideHideRequested, this,
        [this] {
            hideClient(true);
            m_plasmaShellSurface->hideAutoHidingPanel();
            updateShowOnScreenEdge();
        }
    );
    connect(surface, &PlasmaShellSurfaceInterface::panelAutoHideShowRequested, this,
        [this] {
            hideClient(false);
            ScreenEdges::self()->reserve(this, ElectricNone);
            m_plasmaShellSurface->showAutoHidingPanel();
        }
    );
    updatePosition();
    updateRole();
    updateShowOnScreenEdge();
    connect(this, &ShellClient::geometryChanged, this, &ShellClient::updateShowOnScreenEdge);

    setSkipTaskbar(surface->skipTaskbar());
    connect(surface, &PlasmaShellSurfaceInterface::skipTaskbarChanged, this, [this] {
        setSkipTaskbar(m_plasmaShellSurface->skipTaskbar());
    });

    setSkipSwitcher(surface->skipSwitcher());
    connect(surface, &PlasmaShellSurfaceInterface::skipSwitcherChanged, this, [this] {
        setSkipSwitcher(m_plasmaShellSurface->skipSwitcher());
    });
}

void ShellClient::updateShowOnScreenEdge()
{
    if (!ScreenEdges::self()) {
        return;
    }
    if (m_unmapped || !m_plasmaShellSurface || m_plasmaShellSurface->role() != PlasmaShellSurfaceInterface::Role::Panel) {
        ScreenEdges::self()->reserve(this, ElectricNone);
        return;
    }
    if ((m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::AutoHide && m_hidden) ||
        m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::WindowsCanCover) {
        // screen edge API requires an edge, thus we need to figure out which edge the window borders
        Qt::Edges edges;
        for (int i = 0; i < screens()->count(); i++) {
            const auto &screenGeo = screens()->geometry(i);
            if (screenGeo.x() == geom.x()) {
                edges |= Qt::LeftEdge;
            }
            if (screenGeo.x() + screenGeo.width() == geom.x() + geom.width()) {
                edges |= Qt::RightEdge;
            }
            if (screenGeo.y() == geom.y()) {
                edges |= Qt::TopEdge;
            }
            if (screenGeo.y() + screenGeo.height() == geom.y() + geom.height()) {
                edges |= Qt::BottomEdge;
            }
        }
        // a panel might border multiple screen edges. E.g. a horizontal panel at the bottom will
        // also border the left and right edge
        // let's remove such cases
        if (edges.testFlag(Qt::LeftEdge) && edges.testFlag(Qt::RightEdge)) {
            edges = edges & (~(Qt::LeftEdge | Qt::RightEdge));
        }
        if (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::BottomEdge)) {
            edges = edges & (~(Qt::TopEdge | Qt::BottomEdge));
        }
        // it's still possible that a panel borders two edges, e.g. bottom and left
        // in that case the one which is sharing more with the edge wins
        auto check = [this](Qt::Edges edges, Qt::Edge horiz, Qt::Edge vert) {
            if (edges.testFlag(horiz) && edges.testFlag(vert)) {
                if (geom.width() >= geom.height()) {
                    return edges & ~horiz;
                } else {
                    return edges & ~vert;
                }
            }
            return edges;
        };
        edges = check(edges, Qt::LeftEdge, Qt::TopEdge);
        edges = check(edges, Qt::LeftEdge, Qt::BottomEdge);
        edges = check(edges, Qt::RightEdge, Qt::TopEdge);
        edges = check(edges, Qt::RightEdge, Qt::BottomEdge);

        ElectricBorder border = ElectricNone;
        if (edges.testFlag(Qt::LeftEdge)) {
            border = ElectricLeft;
        }
        if (edges.testFlag(Qt::RightEdge)) {
            border = ElectricRight;
        }
        if (edges.testFlag(Qt::TopEdge)) {
            border = ElectricTop;
        }
        if (edges.testFlag(Qt::BottomEdge)) {
            border = ElectricBottom;
        }
        ScreenEdges::self()->reserve(this, border);
    } else {
        ScreenEdges::self()->reserve(this, ElectricNone);
    }
}

bool ShellClient::isInitialPositionSet() const
{
    if (m_plasmaShellSurface) {
        return m_plasmaShellSurface->isPositionSet();
    }
    return false;
}

void ShellClient::installQtExtendedSurface(QtExtendedSurfaceInterface *surface)
{
    m_qtExtendedSurface = surface;

    connect(m_qtExtendedSurface.data(), &QtExtendedSurfaceInterface::raiseRequested, this, [this]() {
        workspace()->raiseClientRequest(this);
    });
    connect(m_qtExtendedSurface.data(), &QtExtendedSurfaceInterface::lowerRequested, this, [this]() {
        workspace()->lowerClientRequest(this);
    });
    m_qtExtendedSurface->installEventFilter(this);
}

void ShellClient::installAppMenu(AppMenuInterface *menu)
{
    m_appMenuInterface = menu;

    auto updateMenu = [this](AppMenuInterface::InterfaceAddress address) {
        updateApplicationMenuServiceName(address.serviceName);
        updateApplicationMenuObjectPath(address.objectPath);
    };
    connect(m_appMenuInterface, &AppMenuInterface::addressChanged, this, [=](AppMenuInterface::InterfaceAddress address) {
        updateMenu(address);
    });
    updateMenu(menu->address());
}

void ShellClient::installPalette(ServerSideDecorationPaletteInterface *palette)
{
    m_paletteInterface = palette;

    auto updatePalette = [this](const QString &palette) {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(palette));
    };
    connect(m_paletteInterface, &ServerSideDecorationPaletteInterface::paletteChanged, this, [=](const QString &palette) {
        updatePalette(palette);
    });
    connect(m_paletteInterface, &QObject::destroyed, this, [=]() {
        updatePalette(QString());
    });
    updatePalette(palette->palette());
}


bool ShellClient::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_internalWindow && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (pe->propertyName() == s_skipClosePropertyName) {
            setSkipCloseAnimation(m_internalWindow->property(s_skipClosePropertyName).toBool());
        }
    }
    return false;
}

void ShellClient::updateColorScheme()
{
    if (m_paletteInterface) {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(m_paletteInterface->palette()));
    } else {
        AbstractClient::updateColorScheme(rules()->checkDecoColor(QString()));
    }
}

void ShellClient::updateMaximizeMode(MaximizeMode maximizeMode)
{
    if (maximizeMode == m_maximizeMode) {
        return;
    }

    m_maximizeMode = maximizeMode;

    emit clientMaximizedStateChanged(this, m_maximizeMode);
    emit clientMaximizedStateChanged(this, m_maximizeMode & MaximizeHorizontal, m_maximizeMode & MaximizeVertical);
}

bool ShellClient::hasStrut() const
{
    if (!isShown(true)) {
        return false;
    }
    if (!m_plasmaShellSurface) {
        return false;
    }
    if (m_plasmaShellSurface->role() != PlasmaShellSurfaceInterface::Role::Panel) {
        return false;
    }
    if (m_strutArea.left == 0 && m_strutArea.right == 0 && m_strutArea.top== 0 && m_strutArea.bottom == 0) {
        return false;
    }
    return m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::AlwaysVisible;
}

void ShellClient::updateIcon()
{
    const QString waylandIconName = QStringLiteral("dde");
    const QString dfIconName = iconFromDesktopFile();
    const QString iconName = dfIconName.isEmpty() ? waylandIconName : dfIconName;
    if (iconName == icon().name()) {
        return;
    }
    setIcon(QIcon::fromTheme(iconName));
}

bool ShellClient::isTransient() const
{
    return m_transient;
}

void ShellClient::setTransient()
{
    SurfaceInterface *s = nullptr;
    if (m_shellSurface) {
        s = m_shellSurface->transientFor().data();
    }
    if (m_xdgShellSurface) {
        if (auto transient = m_xdgShellSurface->transientFor().data()) {
            s = transient->surface();
        }
    }
    if (m_xdgShellPopup) {
        s = m_xdgShellPopup->transientFor().data();
    }
    if (!s) {
        s = waylandServer()->findForeignTransientForSurface(surface());
    }
    auto t = waylandServer()->findClient(s);
    if (t != transientFor()) {
        // remove from main client
        if (transientFor())
            transientFor()->removeTransient(this);
        setTransientFor(t);
        if (t) {
            t->addTransient(this);
        }
    }
    m_transient = (s != nullptr);
}

bool ShellClient::hasTransientPlacementHint() const
{
    return isTransient() && transientFor() != nullptr &&
            (m_shellSurface || m_xdgShellPopup);
}

QRect ShellClient::transientPlacement(const QRect &bounds) const
{
    QRect anchorRect;
    Qt::Edges anchorEdge;
    Qt::Edges gravity;
    QPoint offset;
    PositionerConstraints constraintAdjustments;

    const QPoint parentClientPos = transientFor()->pos() + transientFor()->clientPos();
    QRect popupPosition;

    // returns if a target is within the supplied bounds, optional edges argument states which side to check
    auto inBounds = [bounds](const QRect &target, Qt::Edges edges = Qt::LeftEdge | Qt::RightEdge | Qt::TopEdge | Qt::BottomEdge) -> bool {
        if (edges & Qt::LeftEdge && target.left() < bounds.left()) {
            return false;
        }
        if (edges & Qt::TopEdge && target.top() < bounds.top()) {
            return false;
        }
        if (edges & Qt::RightEdge && target.right() > bounds.right()) {
            //normal QRect::right issue cancels out
            return false;
        }
        if (edges & Qt::BottomEdge && target.bottom() > bounds.bottom()) {
            return false;
        }
        return true;
    };

    if (m_shellSurface) {
        anchorRect = QRect(m_shellSurface->transientOffset(), QSize(1,1));
        anchorEdge = Qt::TopEdge | Qt::LeftEdge;
        gravity = Qt::BottomEdge | Qt::RightEdge; //our single point represents the top left of the popup
        constraintAdjustments = (PositionerConstraint::SlideX | PositionerConstraint::SlideY);
    } else if (m_xdgShellPopup) {
        anchorRect = m_xdgShellPopup->anchorRect();
        anchorEdge = m_xdgShellPopup->anchorEdge();
        gravity = m_xdgShellPopup->gravity();
        offset = m_xdgShellPopup->anchorOffset();
        constraintAdjustments = m_xdgShellPopup->constraintAdjustments();
    } else {
        Q_UNREACHABLE();
    }

    //initial position
    popupPosition = QRect(popupOffset(anchorRect, anchorEdge, gravity) + offset + parentClientPos, geometry().size());

    //if that fits, we don't need to do anything
    if (inBounds(popupPosition)) {
        return popupPosition;
    }
    //otherwise apply constraint adjustment per axis in order XDG Shell Popup states

    if (constraintAdjustments & PositionerConstraint::FlipX) {
        if (!inBounds(popupPosition, Qt::LeftEdge | Qt::RightEdge)) {
            //flip both edges (if either bit is set, XOR both)
            auto flippedAnchorEdge = anchorEdge;
            if (flippedAnchorEdge & (Qt::LeftEdge | Qt::RightEdge)) {
                flippedAnchorEdge ^= (Qt::LeftEdge | Qt::RightEdge);
            }
            auto flippedGravity = gravity;
            if (flippedGravity & (Qt::LeftEdge | Qt::RightEdge)) {
                flippedGravity ^= (Qt::LeftEdge | Qt::RightEdge);
            }
            auto flippedPopupPosition = QRect(popupOffset(anchorRect, flippedAnchorEdge, flippedGravity) + offset + parentClientPos, geometry().size());

            //if it still doesn't fit we should continue with the unflipped version
            if (inBounds(flippedPopupPosition, Qt::LeftEdge | Qt::RightEdge)) {
                popupPosition.setX(flippedPopupPosition.x());
            }
        }
    }
    if (constraintAdjustments & PositionerConstraint::SlideX) {
        if (!inBounds(popupPosition, Qt::LeftEdge)) {
            popupPosition.setX(bounds.x());
        }
        if (!inBounds(popupPosition, Qt::RightEdge)) {
            popupPosition.setX(bounds.x() + bounds.width() - geometry().width());
        }
    }
    if (constraintAdjustments & PositionerConstraint::ResizeX) {
        //TODO
        //but we need to sort out when this is run as resize should only happen before first configure
    }

    if (constraintAdjustments & PositionerConstraint::FlipY) {
        if (!inBounds(popupPosition, Qt::TopEdge | Qt::BottomEdge)) {
            //flip both edges (if either bit is set, XOR both)
            auto flippedAnchorEdge = anchorEdge;
            if (flippedAnchorEdge & (Qt::TopEdge | Qt::BottomEdge)) {
                flippedAnchorEdge ^= (Qt::TopEdge | Qt::BottomEdge);
            }
            auto flippedGravity = gravity;
            if (flippedGravity & (Qt::TopEdge | Qt::BottomEdge)) {
                flippedGravity ^= (Qt::TopEdge | Qt::BottomEdge);
            }
            auto flippedPopupPosition = QRect(popupOffset(anchorRect, flippedAnchorEdge, flippedGravity) + offset + parentClientPos, geometry().size());

            //if it still doesn't fit we should continue with the unflipped version
            if (inBounds(flippedPopupPosition, Qt::TopEdge | Qt::BottomEdge)) {
                popupPosition.setY(flippedPopupPosition.y());
            }
        }
    }
    if (constraintAdjustments & PositionerConstraint::SlideY) {
        if (!inBounds(popupPosition, Qt::TopEdge)) {
            popupPosition.setY(bounds.y());
        }
        if (!inBounds(popupPosition, Qt::BottomEdge)) {
            popupPosition.setY(bounds.y() + bounds.height() - geometry().height());
        }
    }
    if (constraintAdjustments & PositionerConstraint::ResizeY) {
        //TODO
    }

    return popupPosition;
}

QPoint ShellClient::popupOffset(const QRect &anchorRect, const Qt::Edges anchorEdge, const Qt::Edges gravity) const
{
    const QSize popupSize = geometry().size();
    QPoint anchorPoint;
    switch (anchorEdge & (Qt::LeftEdge | Qt::RightEdge)) {
    case Qt::LeftEdge:
        anchorPoint.setX(anchorRect.x());
        break;
    case Qt::RightEdge:
        anchorPoint.setX(anchorRect.x() + anchorRect.width());
        break;
    default:
        anchorPoint.setX(qRound(anchorRect.x() + anchorRect.width() / 2.0));
    }
    switch (anchorEdge & (Qt::TopEdge | Qt::BottomEdge)) {
    case Qt::TopEdge:
        anchorPoint.setY(anchorRect.y());
        break;
    case Qt::BottomEdge:
        anchorPoint.setY(anchorRect.y() + anchorRect.height());
        break;
    default:
        anchorPoint.setY(qRound(anchorRect.y() + anchorRect.height() / 2.0));
    }

    // calculate where the top left point of the popup will end up with the applied gravity
    // gravity indicates direction. i.e if gravitating towards the top the popup's bottom edge
    // will next to the anchor point
    QPoint popupPosAdjust;
    switch (gravity & (Qt::LeftEdge | Qt::RightEdge)) {
    case Qt::LeftEdge:
        popupPosAdjust.setX(-popupSize.width());
        break;
    case Qt::RightEdge:
        popupPosAdjust.setX(0);
        break;
    default:
        popupPosAdjust.setX(qRound(-popupSize.width() / 2.0));
    }
    switch (gravity & (Qt::TopEdge | Qt::BottomEdge)) {
    case Qt::TopEdge:
        popupPosAdjust.setY(-popupSize.height());
        break;
    case Qt::BottomEdge:
        popupPosAdjust.setY(0);
        break;
    default:
        popupPosAdjust.setY(qRound(-popupSize.height() / 2.0));
    }

    return anchorPoint + popupPosAdjust;
}

bool ShellClient::isWaitingForMoveResizeSync() const
{
    return !m_pendingConfigureRequests.isEmpty();
}

void ShellClient::doResizeSync()
{
    requestGeometry(moveResizeGeometry());
}

QMatrix4x4 ShellClient::inputTransformation() const
{
    QMatrix4x4 m = Toplevel::inputTransformation();
    m.translate(-borderLeft(), -borderTop());
    return m;
}

void ShellClient::installServerSideDecoration(KWayland::Server::ServerSideDecorationInterface *deco)
{
    if (m_serverDecoration == deco) {
        return;
    }
    m_serverDecoration = deco;
    connect(m_serverDecoration, &ServerSideDecorationInterface::destroyed, this,
        [this] {
            m_serverDecoration = nullptr;
            if (m_closing || !Workspace::self()) {
                return;
            }
            if (!m_unmapped) {
                // maybe delay to next event cycle in case the ShellClient is getting destroyed, too
                updateDecoration(true);
            }
        }
    );
    if (!m_unmapped) {
        updateDecoration(true);
    }
    connect(m_serverDecoration, &ServerSideDecorationInterface::modeRequested, this,
        [this] (ServerSideDecorationManagerInterface::Mode mode) {
            const bool changed = mode != m_serverDecoration->mode();
            if (changed && !m_unmapped) {
                updateDecoration(false);
            }
        }
    );
}

void ShellClient::installXdgDecoration(XdgDecorationInterface *deco)
{
    Q_ASSERT(m_xdgShellSurface);

    m_xdgDecoration = deco;

    connect(m_xdgDecoration, &QObject::destroyed, this,
        [this] {
            m_xdgDecoration = nullptr;
            if (m_closing || !Workspace::self()) {
                return;
            }
            updateDecoration(true);
        }
    );

    connect(m_xdgDecoration, &XdgDecorationInterface::modeRequested, this,
        [this] () {
        //force is true as we must send a new configure response
        updateDecoration(false, true);
    });
}

bool ShellClient::shouldExposeToWindowManagement()
{
    if (isInternal()) {
        return false;
    }
    if (isLockScreen()) {
        return false;
    }
    if (m_xdgShellPopup) {
        return false;
    }
    if (m_shellSurface) {
        if (m_shellSurface->isTransient() && !m_shellSurface->acceptsKeyboardFocus()) {
            return false;
        }
    }
    return true;
}

KWayland::Server::XdgShellSurfaceInterface::States ShellClient::xdgSurfaceStates() const
{
    XdgShellSurfaceInterface::States states;
    if (isActive()) {
        states |= XdgShellSurfaceInterface::State::Activated;
    }
    if (isFullScreen()) {
        states |= XdgShellSurfaceInterface::State::Fullscreen;
    }
    if (m_requestedMaximizeMode == MaximizeMode::MaximizeFull) {
        states |= XdgShellSurfaceInterface::State::Maximized;
    }
    if (isResize()) {
        states |= XdgShellSurfaceInterface::State::Resizing;
    }
    return states;
}

void ShellClient::doMinimize()
{
    if (isMinimized()) {
        cancelSplitOutline();
        workspace()->clientHidden(this);
    } else {
        handlequickTileModeChanged();
        emit windowShown(this);
    }
    workspace()->updateMinimizedOfTransients(this);
}

bool ShellClient::setupCompositing()
{
    if (m_compositingSetup) {
        return true;
    }
    m_compositingSetup = Toplevel::setupCompositing();
    return m_compositingSetup;
}

void ShellClient::finishCompositing(ReleaseReason releaseReason)
{
    m_compositingSetup = false;
    Toplevel::finishCompositing(releaseReason);
}

void ShellClient::placeIn(QRect &area)
{
    Placement::self()->place(this, area);
    setGeometryRestore(geometry());
}

void ShellClient::showOnScreenEdge()
{
    if (!m_plasmaShellSurface || m_unmapped) {
        return;
    }
    hideClient(false);
    workspace()->raiseClient(this);
    if (m_plasmaShellSurface->panelBehavior() == PlasmaShellSurfaceInterface::PanelBehavior::AutoHide) {
        m_plasmaShellSurface->showAutoHidingPanel();
    }
}

bool ShellClient::dockWantsInput() const
{
    if (m_plasmaShellSurface) {
        if (m_plasmaShellSurface->role() == PlasmaShellSurfaceInterface::Role::Panel) {
            return m_plasmaShellSurface->panelTakesFocus();
        }
    }
    return false;
}

void ShellClient::killWindow()
{
    if (isInternal()) {
        return;
    }
    if (!surface()) {
        return;
    }
    auto c = surface()->client();
    if (c->processId() == getpid() || c->processId() == 0) {
        c->destroy();
        return;
    }
    ::kill(c->processId(), SIGTERM);
    // give it time to terminate and only if terminate fails, try destroy Wayland connection
    QTimer::singleShot(5000, c, &ClientConnection::destroy);
}

bool ShellClient::hasPopupGrab() const
{
    return m_hasPopupGrab;
}

void ShellClient::popupDone()
{
    if (m_shellSurface) {
        m_shellSurface->popupDone();
    }
    if (m_xdgShellPopup) {
        m_xdgShellPopup->popupDone();
    }
}

void ShellClient::handleScreenChanged()
{
    QRect screenarea = workspace()->clientArea(ScreenArea, this);
    if (isOnScreenDisplay() && !screenarea.intersects(geometry())) {
        QRect rect = QRect(screenarea.topLeft(), geometry().size());
        if (rect.isValid()) {
            doSetGeometry(rect);
        }
    }
    updateClientOutputs();
}

void ShellClient::updateClientOutputs()
{
    QVector<OutputInterface*> clientOutputs;
    const auto outputs = waylandServer()->display()->outputs();
    int count = screens()->count();
    if (outputs.size() != count) {
        qWarning() << "------- screens' size mismatch outputs, ignore update"<<"screen count"<<count<<"output count"<<outputs.size()<<"pid@"<<pid()<<"surface@"<<surfaceId()<<resourceClass();
        return;
    }

    for (OutputInterface* output: qAsConst(outputs)) {
        const QRect outputGeom(output->globalPosition(), output->pixelSize() / output->scale());
        const auto resources = output->clientResources(surface()->client());
        // 屏幕插拔，概率性出现kwin崩溃
        // 通过日志发现，屏幕拔出时，output移除，emit screensQueried触发updateClientOutputs走leave output流程
        // 概率性出现随后geometryChanged再次触发updateClientOutputs，但因为上一次已经走了leave流程，这次又会对移除的output走enter output流程
        // 此处增加了一个outputRemoved标志，当output移除后，置outputRemoved为true
        // 此时对于第二次重入，通过检测outputRemoved标志，如果为true, surface不更新该output
        if (output->isOutputRemoved()) {
            qDebug() <<"pid@"<<pid()<<"surface@"<<surfaceId()<<resourceClass()<<"have leaved output, no need to update again";
            continue;
        }
        if (resources.size() > 0 && geometry().intersects(outputGeom)) {
            clientOutputs << output;
            if (workspace() && workspace()->isKwinDebug()) {
                qDebug()<<resourceClass()<<"surface@"<<surfaceId()<<"output"<<output->manufacturer();
            }
        }
    }
    surface()->setOutputs(clientOutputs);
}

bool ShellClient::isPopupWindow() const
{
    if (Toplevel::isPopupWindow()) {
        return true;
    }
    if (isInternal()) {
        return m_internalWindowFlags.testFlag(Qt::Popup);
    }
    if (m_shellSurface != nullptr) {
        return m_shellSurface->isPopup();
    }
    if (m_xdgShellPopup != nullptr) {
        return true;
    }
    return false;
}

KWayland::Server::DDEShellSurfaceInterface *ShellClient::ddeShellSurface() const {
    return m_ddeShellSurface.data();
}

QPointF ShellClient::windowRadius() const
{
    return m_windowRadius;
}

void ShellClient::setWindowRadius(QPointF radius)
{
    if (radius == m_windowRadius) {
        return;
    }
    m_windowRadius = radius;
    emit windowRadiusChanged();
}

}
