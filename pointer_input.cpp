/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2018 Roman Gilg <subdiff@gmail.com>

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
#include "pointer_input.h"
#include "platform.h"
#include "client.h"
#include "effects.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "osd.h"
#include "screens.h"
#include "useractions.h"
#include "shell_client.h"
#include "wayland_cursor_theme.h"
#include "wayland_server.h"
#include "workspace.h"
#include "decorations/decoratedclient.h"
#include "screens.h"
// KDecoration
#include <KDecoration2/Decoration>
// KWayland
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/buffer.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/datadevice_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/pointerconstraints_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>
// screenlocker
#include <KScreenLocker/KsldApp>

#include <KLocalizedString>

#include <QHoverEvent>
#include <QWindow>
// Wayland
#include <wayland-cursor.h>

#include <linux/input.h>

namespace KWin
{

static const QHash<uint32_t, Qt::MouseButton> s_buttonToQtMouseButton = {
    { BTN_LEFT , Qt::LeftButton },
    { BTN_MIDDLE , Qt::MiddleButton },
    { BTN_RIGHT , Qt::RightButton },
    // in QtWayland mapped like that
    { BTN_SIDE , Qt::ExtraButton1 },
    // in QtWayland mapped like that
    { BTN_EXTRA , Qt::ExtraButton2 },
    { BTN_BACK , Qt::BackButton },
    { BTN_FORWARD , Qt::ForwardButton },
    { BTN_TASK , Qt::TaskButton },
    // mapped like that in QtWayland
    { 0x118 , Qt::ExtraButton6 },
    { 0x119 , Qt::ExtraButton7 },
    { 0x11a , Qt::ExtraButton8 },
    { 0x11b , Qt::ExtraButton9 },
    { 0x11c , Qt::ExtraButton10 },
    { 0x11d , Qt::ExtraButton11 },
    { 0x11e , Qt::ExtraButton12 },
    { 0x11f , Qt::ExtraButton13 },
};

uint32_t qtMouseButtonToButton(Qt::MouseButton button)
{
    return s_buttonToQtMouseButton.key(button);
}

Qt::MouseButton buttonToQtMouseButton(uint32_t button)
{
    // all other values get mapped to ExtraButton24
    // this is actually incorrect but doesn't matter in our usage
    // KWin internally doesn't use these high extra buttons anyway
    // it's only needed for recognizing whether buttons are pressed
    // if multiple buttons are mapped to the value the evaluation whether
    // buttons are pressed is correct and that's all we care about.
    return s_buttonToQtMouseButton.value(button, Qt::ExtraButton24);
}

static bool screenContainsPos(const QPointF &pos)
{
    for (int i = 0; i < screens()->count(); ++i) {
        if (screens()->geometry(i).contains(pos.toPoint())) {
            return true;
        }
    }
    return false;
}

static QPointF confineToBoundingBox(const QPointF &pos, const QRectF &boundingBox)
{
    return QPointF(
        qBound(boundingBox.left(), pos.x(), boundingBox.right() - 1.0),
        qBound(boundingBox.top(), pos.y(), boundingBox.bottom() - 1.0)
    );
}

PointerInputRedirection::PointerInputRedirection(InputRedirection* parent)
    : InputDeviceHandler(parent)
    , m_cursor(nullptr)
    , m_supportsWarping(Application::usesLibinput())
{
}

PointerInputRedirection::~PointerInputRedirection() = default;

void PointerInputRedirection::init()
{
    Q_ASSERT(!inited());
    m_cursor = new CursorImage(this);
    setInited(true);
    InputDeviceHandler::init();

    connect(m_cursor, &CursorImage::changed, kwinApp()->platform(), &Platform::cursorChanged);
    emit m_cursor->changed();

    connect(screens(), &Screens::changed, this, &PointerInputRedirection::updateAfterScreenChange);
    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this,
            [this] {
                waylandServer()->seat()->cancelPointerPinchGesture();
                waylandServer()->seat()->cancelPointerSwipeGesture();
                update();
            }
        );
    }
    connect(workspace(), &QObject::destroyed, this, [this] { setInited(false); });
    connect(waylandServer(), &QObject::destroyed, this, [this] { setInited(false); });
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::dragEnded, this,
        [this] {
            // need to force a focused pointer change
            waylandServer()->seat()->setFocusedPointerSurface(nullptr);
            setFocus(nullptr);
            update();
        }
    );
    // connect the move resize of all window
    auto setupMoveResizeConnection = [this] (AbstractClient *c) {
        connect(c, &AbstractClient::clientStartUserMovedResized, this, &PointerInputRedirection::updateOnStartMoveResize);
        //connect(c, &AbstractClient::clientFinishUserMovedResized, this, &PointerInputRedirection::update);
        connect(c, &AbstractClient::clientFinishUserMovedResized, this, [=]() {
            // need to force a focused pointer change
            waylandServer()->seat()->setFocusedPointerSurface(nullptr);
            setFocus(nullptr);
            update();
        });
    };
    const auto clients = workspace()->allClientList();
    std::for_each(clients.begin(), clients.end(), setupMoveResizeConnection);
    connect(workspace(), &Workspace::clientAdded, this, setupMoveResizeConnection);
    connect(waylandServer(), &WaylandServer::shellClientAdded, this, setupMoveResizeConnection);

    // warp the cursor to center of screen
    warp(screens()->geometry().center());
    updateAfterScreenChange();
}

void PointerInputRedirection::updateOnStartMoveResize()
{
    breakPointerConstraints(focus() ? focus()->surface() : nullptr);
    disconnectPointerConstraintsConnection();
    setFocus(nullptr);
    waylandServer()->seat()->setFocusedPointerSurface(nullptr);
}

void PointerInputRedirection::updateToReset()
{
    if (internalWindow()) {
        disconnect(m_internalWindowConnection);
        m_internalWindowConnection = QMetaObject::Connection();
        QEvent event(QEvent::Leave);
        QCoreApplication::sendEvent(internalWindow().data(), &event);
        setInternalWindow(nullptr);
    }
    if (decoration()) {
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(decoration()->decoration(), &event);
        setDecoration(nullptr);
    }
    if (focus()) {
        if (AbstractClient *c = qobject_cast<AbstractClient*>(focus().data())) {
            c->leaveEvent();
        }
        disconnect(m_focusGeometryConnection);
        m_focusGeometryConnection = QMetaObject::Connection();
        breakPointerConstraints(focus()->surface());
        disconnectPointerConstraintsConnection();
        setFocus(nullptr);
    }
    waylandServer()->seat()->setFocusedPointerSurface(nullptr);
}

void PointerInputRedirection::processMotion(const QPointF &pos, uint32_t time, LibInput::Device *device)
{
    processMotion(pos, QSizeF(), QSizeF(), time, 0, device);
}

class PositionUpdateBlocker
{
public:
    PositionUpdateBlocker(PointerInputRedirection *pointer)
        : m_pointer(pointer)
    {
        s_counter++;
    }
    ~PositionUpdateBlocker() {
        s_counter--;
        if (s_counter == 0) {
            if (!s_scheduledPositions.isEmpty()) {
                const auto pos = s_scheduledPositions.takeFirst();
                m_pointer->processMotion(pos.pos, pos.delta, pos.deltaNonAccelerated, pos.time, pos.timeUsec, nullptr);
            }
        }
    }

    static bool isPositionBlocked() {
        return s_counter > 0;
    }

    static void schedulePosition(const QPointF &pos, const QSizeF &delta, const QSizeF &deltaNonAccelerated, uint32_t time, quint64 timeUsec) {
        s_scheduledPositions.append({pos, delta, deltaNonAccelerated, time, timeUsec});
    }

private:
    static int s_counter;
    struct ScheduledPosition {
        QPointF pos;
        QSizeF delta;
        QSizeF deltaNonAccelerated;
        quint32 time;
        quint64 timeUsec;
    };
    static QVector<ScheduledPosition> s_scheduledPositions;

    PointerInputRedirection *m_pointer;
};

int PositionUpdateBlocker::s_counter = 0;
QVector<PositionUpdateBlocker::ScheduledPosition> PositionUpdateBlocker::s_scheduledPositions;

void PointerInputRedirection::processMotion(const QPointF &pos, const QSizeF &delta, const QSizeF &deltaNonAccelerated, uint32_t time, quint64 timeUsec, LibInput::Device *device)
{
    if (!inited()) {
        return;
    }
    if (PositionUpdateBlocker::isPositionBlocked()) {
        PositionUpdateBlocker::schedulePosition(pos, delta, deltaNonAccelerated, time, timeUsec);
        return;
    }

    PositionUpdateBlocker blocker(this);
    updatePosition(pos);
    MouseEvent event(QEvent::MouseMove, m_pos, Qt::NoButton, m_qtButtons,
                     input()->keyboardModifiers(), time,
                     delta, deltaNonAccelerated, timeUsec, device);
    event.setModifiersRelevantForGlobalShortcuts(input()->modifiersRelevantForGlobalShortcuts());

    update();
    input()->processSpies(std::bind(&InputEventSpy::pointerEvent, std::placeholders::_1, &event));
    input()->processFilters(std::bind(&InputEventFilter::pointerEvent, std::placeholders::_1, &event, 0));
}

void PointerInputRedirection::processButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time, LibInput::Device *device)
{
    QEvent::Type type;
    switch (state) {
    case InputRedirection::PointerButtonReleased:
        type = QEvent::MouseButtonRelease;
        break;
    case InputRedirection::PointerButtonPressed:
        type = QEvent::MouseButtonPress;
        update();
        break;
    default:
        Q_UNREACHABLE();
        return;
    }

    updateButton(button, state);

    MouseEvent event(type, m_pos, buttonToQtMouseButton(button), m_qtButtons,
                     input()->keyboardModifiers(), time, QSizeF(), QSizeF(), 0, device);
    event.setModifiersRelevantForGlobalShortcuts(input()->modifiersRelevantForGlobalShortcuts());
    event.setNativeButton(button);

    input()->processSpies(std::bind(&InputEventSpy::pointerEvent, std::placeholders::_1, &event));

    if (!inited()) {
        return;
    }

    input()->processFilters(std::bind(&InputEventFilter::pointerEvent, std::placeholders::_1, &event, button));

    if (type == QEvent::MouseButtonPress && workspace()->userActionsMenu()->isShown()) {
        const_cast<UserActionsMenu*>(workspace()->userActionsMenu())->handleClick(m_pos.toPoint());
    }

    if (state == InputRedirection::PointerButtonReleased) {
        update();
    }
}

void PointerInputRedirection::processAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time, LibInput::Device *device)
{
    if (delta == 0) {
        return;
    }
    update();

    emit input()->pointerAxisChanged(axis, delta);

    WheelEvent wheelEvent(m_pos, delta,
                           (axis == InputRedirection::PointerAxisHorizontal) ? Qt::Horizontal : Qt::Vertical,
                           m_qtButtons, input()->keyboardModifiers(), time, device);
    wheelEvent.setModifiersRelevantForGlobalShortcuts(input()->modifiersRelevantForGlobalShortcuts());

    input()->processSpies(std::bind(&InputEventSpy::wheelEvent, std::placeholders::_1, &wheelEvent));

    if (!inited()) {
        return;
    }
    input()->processFilters(std::bind(&InputEventFilter::wheelEvent, std::placeholders::_1, &wheelEvent));
}

void PointerInputRedirection::processSwipeGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!inited()) {
        return;
    }

    // During the lock screen, certain gestures will trigger some actions to unlock the screen.
    // we added an dbus interface to disable gestures to fix this bug.
    // During the lock screen, after the client calls the disableGestureForClient interface, kwin does not handle gesture events.
    if (workspace()->isDisableGesture()) {
        return;
    }

    input()->processSpies(std::bind(&InputEventSpy::swipeGestureBegin, std::placeholders::_1, fingerCount, time));
    input()->processFilters(std::bind(&InputEventFilter::swipeGestureBegin, std::placeholders::_1, fingerCount, time));
}

void PointerInputRedirection::processSwipeGestureUpdate(const QSizeF &delta, quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!inited()) {
        return;
    }

    // During the lock screen, certain gestures will trigger some actions to unlock the screen.
    // we added an dbus interface to disable gestures to fix this bug.
    // During the lock screen, after the client calls the disableGestureForClient interface, kwin does not handle gesture events.
    if (workspace()->isDisableGesture()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::swipeGestureUpdate, std::placeholders::_1, delta, time));
    input()->processFilters(std::bind(&InputEventFilter::swipeGestureUpdate, std::placeholders::_1, delta, time));
}

void PointerInputRedirection::processSwipeGestureEnd(quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!inited()) {
        return;
    }

    // During the lock screen, certain gestures will trigger some actions to unlock the screen.
    // we added an dbus interface to disable gestures to fix this bug.
    // During the lock screen, after the client calls the disableGestureForClient interface, kwin does not handle gesture events.
    if (workspace()->isDisableGesture()) {
        return;
    }

    update();

    input()->processSpies(std::bind(&InputEventSpy::swipeGestureEnd, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::swipeGestureEnd, std::placeholders::_1, time));
}

void PointerInputRedirection::processSwipeGestureCancelled(quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::swipeGestureCancelled, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::swipeGestureCancelled, std::placeholders::_1, time));
}

void PointerInputRedirection::processPinchGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::pinchGestureBegin, std::placeholders::_1, fingerCount, time));
    input()->processFilters(std::bind(&InputEventFilter::pinchGestureBegin, std::placeholders::_1, fingerCount, time));
}

void PointerInputRedirection::processPinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::pinchGestureUpdate, std::placeholders::_1, scale, angleDelta, delta, time));
    input()->processFilters(std::bind(&InputEventFilter::pinchGestureUpdate, std::placeholders::_1, scale, angleDelta, delta, time));
}

void PointerInputRedirection::processPinchGestureEnd(quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::pinchGestureEnd, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::pinchGestureEnd, std::placeholders::_1, time));
}

void PointerInputRedirection::processPinchGestureCancelled(quint32 time, KWin::LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!inited()) {
        return;
    }
    update();

    input()->processSpies(std::bind(&InputEventSpy::pinchGestureCancelled, std::placeholders::_1, time));
    input()->processFilters(std::bind(&InputEventFilter::pinchGestureCancelled, std::placeholders::_1, time));
}

bool PointerInputRedirection::areButtonsPressed() const
{
    for (auto state : m_buttons) {
        if (state == InputRedirection::PointerButtonPressed) {
            return true;
        }
    }
    return false;
}

bool PointerInputRedirection::focusUpdatesBlocked()
{
    if (!inited()) {
        return true;
    }
    if (waylandServer()->seat()->isDragPointer()) {
        // ignore during drag and drop
        return true;
    }
    if (waylandServer()->seat()->isTouchSequence()) {
        // ignore during touch operations
        return true;
    }
    if (input()->isSelectingWindow()) {
        return true;
    }
    if (areButtonsPressed()) {
        return true;
    }
    return false;
}

void PointerInputRedirection::cleanupInternalWindow(QWindow *old, QWindow *now)
{
    disconnect(m_internalWindowConnection);
    m_internalWindowConnection = QMetaObject::Connection();

    if (old) {
        // leave internal window
        // TODO: do this instead via Wayland protocol as below
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(old, &leaveEvent);
    }

    if (now) {
        m_internalWindowConnection = connect(internalWindow().data(), &QWindow::visibleChanged, this,
            [this] (bool visible) {
                if (!visible) {
                    update();
                }
            }
        );
    }
}

void PointerInputRedirection::cleanupDecoration(Decoration::DecoratedClientImpl *old, Decoration::DecoratedClientImpl *now)
{
    disconnect(m_decorationGeometryConnection);
    m_decorationGeometryConnection = QMetaObject::Connection();
    workspace()->updateFocusMousePosition(position().toPoint());

    if (old) {
        // send leave event to old decoration
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(old->decoration(), &event);
    }
    if (!now) {
        // left decoration
        return;
    }

    waylandServer()->seat()->setFocusedPointerSurface(nullptr);

    auto pos = m_pos - now->client()->pos();
    QHoverEvent event(QEvent::HoverEnter, pos, pos);
    QCoreApplication::instance()->sendEvent(now->decoration(), &event);
    now->client()->processDecorationMove(pos.toPoint(), m_pos.toPoint());

    m_decorationGeometryConnection = connect(decoration()->client(), &AbstractClient::geometryChanged, this,
        [this] {
            // ensure maximize button gets the leave event when maximizing/restore a window, see BUG 385140
            const auto oldDeco = decoration();
            update();
            if (oldDeco &&
                    oldDeco == decoration() &&
                    !decoration()->client()->isMove() &&
                    !decoration()->client()->isResize() &&
                    !areButtonsPressed()) {
                // position of window did not change, we need to send HoverMotion manually
                const QPointF p = m_pos - decoration()->client()->pos();
                QHoverEvent event(QEvent::HoverMove, p, p);
                QCoreApplication::instance()->sendEvent(decoration()->decoration(), &event);
            }
        }, Qt::QueuedConnection);
}

static bool s_cursorUpdateBlocking = false;

void PointerInputRedirection::focusUpdate(Toplevel *focusOld, Toplevel *focusNow)
{
    if (AbstractClient *ac = qobject_cast<AbstractClient*>(focusOld)) {
        ac->leaveEvent();
        breakPointerConstraints(ac->surface());
        disconnectPointerConstraintsConnection();
    }
    disconnect(m_focusGeometryConnection);
    m_focusGeometryConnection = QMetaObject::Connection();

    if (AbstractClient *ac = qobject_cast<AbstractClient*>(focusNow)) {
        ac->enterEvent(m_pos.toPoint());
        workspace()->updateFocusMousePosition(m_pos.toPoint());
    }

    auto seat = waylandServer()->seat();
    if (!focusNow || !focusNow->surface() || decoration()) {
        // no new surface or internal window or on decoration -> cleanup
        warpXcbOnSurfaceLeft(nullptr);
        seat->setFocusedPointerSurface(nullptr);
        return;
    }

    if (internalWindow()) {
        // enter internal window
        // TODO: do this instead via Wayland protocol as below
        const auto pos = at()->pos();
        QEnterEvent enterEvent(pos, pos, m_pos);
        QCoreApplication::sendEvent(internalWindow().data(), &enterEvent);
    }

    // TODO: add convenient API to update global pos together with updating focused surface
    warpXcbOnSurfaceLeft(focusNow->surface());

    // TODO: why? in order to reset the cursor icon?
    s_cursorUpdateBlocking = true;
    seat->setFocusedPointerSurface(nullptr);
    s_cursorUpdateBlocking = false;

    seat->setPointerPos(m_pos.toPoint());
    seat->setFocusedPointerSurface(focusNow->surface(), focusNow->inputTransformation());

    m_focusGeometryConnection = connect(focusNow, &Toplevel::geometryChanged, this,
        [this] {
            // TODO: why no assert possible?
            if (!focus()) {
                return;
            }
            // TODO: can we check on the client instead?
            if (workspace()->getMovingClient()) {
                // don't update while moving
                return;
            }
            auto seat = waylandServer()->seat();
            if (focus()->surface() != seat->focusedPointerSurface()) {
                return;
            }
            seat->setFocusedPointerSurfaceTransformation(focus()->inputTransformation());
        }
    );

    m_constraintsConnection = connect(focusNow->surface(), &KWayland::Server::SurfaceInterface::pointerConstraintsChanged,
                                      this, &PointerInputRedirection::updatePointerConstraints);
    m_constraintsActivatedConnection = connect(workspace(), &Workspace::clientActivated,
                                               this, &PointerInputRedirection::updatePointerConstraints);
    updatePointerConstraints();
}

void PointerInputRedirection::breakPointerConstraints(KWayland::Server::SurfaceInterface *surface)
{
    // cancel pointer constraints
    if (surface) {
        auto c = surface->confinedPointer();
        if (c && c->isConfined()) {
            c->setConfined(false);
        }
        auto l = surface->lockedPointer();
        if (l && l->isLocked()) {
            l->setLocked(false);
        }
    }
    disconnectConfinedPointerRegionConnection();
    m_confined = false;
    m_locked = false;
}

void PointerInputRedirection::disconnectConfinedPointerRegionConnection()
{
    disconnect(m_confinedPointerRegionConnection);
    m_confinedPointerRegionConnection = QMetaObject::Connection();
}

void PointerInputRedirection::disconnectLockedPointerAboutToBeUnboundConnection()
{
    disconnect(m_lockedPointerAboutToBeUnboundConnection);
    m_lockedPointerAboutToBeUnboundConnection = QMetaObject::Connection();
}

void PointerInputRedirection::disconnectPointerConstraintsConnection()
{
    disconnect(m_constraintsConnection);
    m_constraintsConnection = QMetaObject::Connection();

    disconnect(m_constraintsActivatedConnection);
    m_constraintsActivatedConnection = QMetaObject::Connection();
}

template <typename T>
static QRegion getConstraintRegion(Toplevel *t, T *constraint)
{
    const QRegion windowShape = t->inputShape();
    const QRegion windowRegion = windowShape.isEmpty() ? QRegion(0, 0, t->clientSize().width(), t->clientSize().height()) : windowShape;
    const QRegion intersected = constraint->region().isEmpty() ? windowRegion : windowRegion.intersected(constraint->region());
    return intersected.translated(t->pos() + t->clientPos());
}

void PointerInputRedirection::setEnableConstraints(bool set)
{
    if (m_enableConstraints == set) {
        return;
    }
    m_enableConstraints = set;
    updatePointerConstraints();
}

void PointerInputRedirection::updatePointerConstraints()
{
    if (focus().isNull()) {
        return;
    }
    const auto s = focus()->surface();
    if (!s) {
        return;
    }
    if (s != waylandServer()->seat()->focusedPointerSurface()) {
        return;
    }
    if (!supportsWarping()) {
        return;
    }
    const bool canConstrain = m_enableConstraints && focus() == workspace()->activeClient();
    const auto cf = s->confinedPointer();
    if (cf) {
        if (cf->isConfined()) {
            if (!canConstrain) {
                cf->setConfined(false);
                m_confined = false;
                disconnectConfinedPointerRegionConnection();
            }
            return;
        }
        const QRegion r = getConstraintRegion(focus().data(), cf.data());
        if (canConstrain && r.contains(m_pos.toPoint())) {
            cf->setConfined(true);
            m_confined = true;
            m_confinedPointerRegionConnection = connect(cf.data(), &KWayland::Server::ConfinedPointerInterface::regionChanged, this,
                [this] {
                    if (!focus()) {
                        return;
                    }
                    const auto s = focus()->surface();
                    if (!s) {
                        return;
                    }
                    const auto cf = s->confinedPointer();
                    if (!getConstraintRegion(focus().data(), cf.data()).contains(m_pos.toPoint())) {
                        // pointer no longer in confined region, break the confinement
                        cf->setConfined(false);
                        m_confined = false;
                    } else {
                        if (!cf->isConfined()) {
                            cf->setConfined(true);
                            m_confined = true;
                        }
                    }
                }
            );
            return;
        }
    } else {
        m_confined = false;
        disconnectConfinedPointerRegionConnection();
    }
    const auto lock = s->lockedPointer();
    if (lock) {
        if (lock->isLocked()) {
            if (!canConstrain) {
                const auto hint = lock->cursorPositionHint();
                lock->setLocked(false);
                m_locked = false;
                disconnectLockedPointerAboutToBeUnboundConnection();
                if (! (hint.x() < 0 || hint.y() < 0) && focus()) {
                    processMotion(focus()->pos() - focus()->clientContentPos() + hint, waylandServer()->seat()->timestamp());
                }
            }
            return;
        }
        const QRegion r = getConstraintRegion(focus().data(), lock.data());
        if (canConstrain && r.contains(m_pos.toPoint())) {
            lock->setLocked(true);
            m_locked = true;

            // The client might cancel pointer locking from its side by unbinding the LockedPointerInterface.
            // In this case the cached cursor position hint must be fetched before the resource goes away
            m_lockedPointerAboutToBeUnboundConnection = connect(lock.data(), &KWayland::Server::LockedPointerInterface::aboutToBeUnbound, this,
                [this, lock]() {
                    const auto hint = lock->cursorPositionHint();
                    if (hint.x() < 0 || hint.y() < 0 || !focus()) {
                        return;
                    }
                    auto globalHint = focus()->pos() - focus()->clientContentPos() + hint;

                    // When the resource finally goes away, reposition the cursor according to the hint
                    connect(lock.data(), &KWayland::Server::LockedPointerInterface::unbound, this,
                        [this, globalHint]() {
                            processMotion(globalHint, waylandServer()->seat()->timestamp());
                    });
                }
            );
            // TODO: connect to region change - is it needed at all? If the pointer is locked it's always in the region
        }
    } else {
        m_locked = false;
        disconnectLockedPointerAboutToBeUnboundConnection();
    }
}

void PointerInputRedirection::warpXcbOnSurfaceLeft(KWayland::Server::SurfaceInterface *newSurface)
{
    auto xc = waylandServer()->xWaylandConnection();
    if (!xc) {
        // No XWayland, no point in warping the x cursor
        return;
    }
    const auto c = kwinApp()->x11Connection();
    if (!c) {
        return;
    }
    static bool s_hasXWayland119 = xcb_get_setup(c)->release_number >= 11900000;
    if (s_hasXWayland119) {
        return;
    }
    if (newSurface && newSurface->client() == xc) {
        // new window is an X window
        return;
    }
    auto s = waylandServer()->seat()->focusedPointerSurface();
    if (!s || s->client() != xc) {
        // pointer was not on an X window
        return;
    }
    // warp pointer to 0/0 to trigger leave events on previously focused X window
    xcb_warp_pointer(c, XCB_WINDOW_NONE, kwinApp()->x11RootWindow(), 0, 0, 0, 0, 0, 0),
    xcb_flush(c);
}

QPointF PointerInputRedirection::applyPointerConfinement(const QPointF &pos) const
{
    if (!focus()) {
        return pos;
    }
    auto s = focus()->surface();
    if (!s) {
        return pos;
    }
    auto cf = s->confinedPointer();
    if (!cf) {
        return pos;
    }
    if (!cf->isConfined()) {
        return pos;
    }

    const QRegion confinementRegion = getConstraintRegion(focus().data(), cf.data());
    if (confinementRegion.contains(pos.toPoint())) {
        return pos;
    }
    QPointF p = pos;
    // allow either x or y to pass
    p = QPointF(m_pos.x(), pos.y());
    if (confinementRegion.contains(p.toPoint())) {
        return p;
    }
    p = QPointF(pos.x(), m_pos.y());
    if (confinementRegion.contains(p.toPoint())) {
        return p;
    }

    return m_pos;
}

void PointerInputRedirection::updatePosition(const QPointF &pos)
{
    if (m_locked) {
        // locked pointer should not move
        return;
    }
    // verify that at least one screen contains the pointer position
    QPointF p = pos;
    if (!screenContainsPos(p)) {
        const QRectF unitedScreensGeometry = screens()->geometry();
        p = confineToBoundingBox(p, unitedScreensGeometry);
        if (!screenContainsPos(p)) {
            const QRectF currentScreenGeometry = screens()->geometry(screens()->number(m_pos.toPoint()));
            p = confineToBoundingBox(p, currentScreenGeometry);
        }
    }
    p = applyPointerConfinement(p);
    if (p == m_pos) {
        // didn't change due to confinement
        return;
    }
    // verify screen confinement
    if (!screenContainsPos(p)) {
        return;
    }
    m_pos = p;
    emit input()->globalPointerChanged(m_pos);
}

void PointerInputRedirection::updateButton(uint32_t button, InputRedirection::PointerButtonState state)
{
    m_buttons[button] = state;

    // update Qt buttons
    m_qtButtons = Qt::NoButton;
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it) {
        if (it.value() == InputRedirection::PointerButtonReleased) {
            continue;
        }
        m_qtButtons |= buttonToQtMouseButton(it.key());
    }

    emit input()->pointerButtonStateChanged(button, state);
}

void PointerInputRedirection::warp(const QPointF &pos)
{
    if (supportsWarping()) {
        kwinApp()->platform()->warpPointer(pos);
        processMotion(pos, waylandServer()->seat()->timestamp());
    }
}

bool PointerInputRedirection::supportsWarping() const
{
    if (!inited()) {
        return false;
    }
    if (m_supportsWarping) {
        return true;
    }
    if (kwinApp()->platform()->supportsPointerWarping()) {
        return true;
    }
    return false;
}

void PointerInputRedirection::updateAfterScreenChange()
{
    if (!inited()) {
        return;
    }
    if (screenContainsPos(m_pos)) {
        // pointer still on a screen
        return;
    }
    // pointer no longer on a screen, reposition to closes screen
    const QPointF pos = screens()->geometry(screens()->number(m_pos.toPoint())).center();
    // TODO: better way to get timestamps
    processMotion(pos, waylandServer()->seat()->timestamp());
}

QImage PointerInputRedirection::cursorImage() const
{
    if (!inited()) {
        return QImage();
    }
    return m_cursor->image();
}

QPoint PointerInputRedirection::cursorHotSpot() const
{
    if (!inited()) {
        return QPoint();
    }
    return m_cursor->hotSpot();
}

void PointerInputRedirection::markCursorAsRendered()
{
    if (!inited()) {
        return;
    }
    m_cursor->markAsRendered();
}

QPointF PointerInputRedirection::position() const
{
    return m_pos.toPoint();
}

void PointerInputRedirection::setEffectsOverrideCursor(Qt::CursorShape shape)
{
    if (!inited()) {
        return;
    }
    // current pointer focus window should get a leave event
    update();
    m_cursor->setEffectsOverrideCursor(shape);
}

void PointerInputRedirection::removeEffectsOverrideCursor()
{
    if (!inited()) {
        return;
    }
    // cursor position might have changed while there was an effect in place
    update();
    m_cursor->removeEffectsOverrideCursor();
}

void PointerInputRedirection::setWindowSelectionCursor(const QByteArray &shape)
{
    if (!inited()) {
        return;
    }
    // send leave to current pointer focus window
    updateToReset();
    m_cursor->setWindowSelectionCursor(shape);
}

void PointerInputRedirection::removeWindowSelectionCursor()
{
    if (!inited()) {
        return;
    }
    update();
    m_cursor->removeWindowSelectionCursor();
}

CursorImage::CursorImage(PointerInputRedirection *parent)
    : QObject(parent)
    , m_pointer(parent)
{
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::focusedPointerChanged, this, &CursorImage::update);
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::dragStarted, this, &CursorImage::updateDrag);
    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::dragEnded, this,
        [this] {
            disconnect(m_drag.connection);
            reevaluteSource();
        }
    );
    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &CursorImage::reevaluteSource);
    }
    connect(m_pointer, &PointerInputRedirection::decorationChanged, this, &CursorImage::updateDecoration);
    // connect the move resize of all window
    auto setupMoveResizeConnection = [this] (AbstractClient *c) {
        connect(c, &AbstractClient::moveResizedChanged, this, &CursorImage::updateMoveResize);
        connect(c, &AbstractClient::moveResizeCursorChanged, this, &CursorImage::updateMoveResize);
    };
    const auto clients = workspace()->allClientList();
    std::for_each(clients.begin(), clients.end(), setupMoveResizeConnection);
    connect(workspace(), &Workspace::clientAdded, this, setupMoveResizeConnection);
    connect(waylandServer(), &WaylandServer::shellClientAdded, this, setupMoveResizeConnection);
    loadThemeCursor(Qt::ArrowCursor, &m_fallbackCursor);
    if (m_cursorTheme) {
        connect(m_cursorTheme, &WaylandCursorTheme::themeChanged, this,
            [this] {
                m_cursors.clear();
                m_cursorsByName.clear();
                loadThemeCursor(Qt::ArrowCursor, &m_fallbackCursor);
                updateDecorationCursor();
                updateMoveResize();
                // TODO: update effects
            }
        );
    }
    m_surfaceRenderedTimer.start();
}

CursorImage::~CursorImage() = default;

void CursorImage::markAsRendered()
{
    if (m_currentSource == CursorSource::DragAndDrop) {
        // always sending a frame rendered to the drag icon surface to not freeze QtWayland (see https://bugreports.qt.io/browse/QTBUG-51599 )
        if (auto ddi = waylandServer()->seat()->dragSource()) {
            if (auto s = ddi->icon()) {
                s->frameRendered(m_surfaceRenderedTimer.elapsed());
            }
        }
        auto p = waylandServer()->seat()->dragPointer();
        if (!p) {
            return;
        }
        auto c = p->cursor();
        if (!c) {
            return;
        }
        auto cursorSurface = c->surface();
        if (cursorSurface.isNull()) {
            return;
        }
        cursorSurface->frameRendered(m_surfaceRenderedTimer.elapsed());
        return;
    }
    if (m_currentSource != CursorSource::LockScreen && m_currentSource != CursorSource::PointerSurface) {
        return;
    }
    auto p = waylandServer()->seat()->focusedPointer();
    if (!p) {
        return;
    }
    auto c = p->cursor();
    if (!c) {
        return;
    }
    auto cursorSurface = c->surface();
    if (cursorSurface.isNull()) {
        return;
    }
    cursorSurface->frameRendered(m_surfaceRenderedTimer.elapsed());
}

void CursorImage::update()
{
    if (s_cursorUpdateBlocking) {
        return;
    }
    using namespace KWayland::Server;
    disconnect(m_serverCursor.connection);
    auto p = waylandServer()->seat()->focusedPointer();
    if (p) {
        m_serverCursor.connection = connect(p, &PointerInterface::cursorChanged, this, &CursorImage::updateServerCursor);
    } else {
        m_serverCursor.connection = QMetaObject::Connection();
        reevaluteSource();
    }
}

void CursorImage::updateDecoration()
{
    disconnect(m_decorationConnection);
    auto deco = m_pointer->decoration();
    AbstractClient *c = deco.isNull() ? nullptr : deco->client();
    if (c) {
        m_decorationConnection = connect(c, &AbstractClient::moveResizeCursorChanged, this, &CursorImage::updateDecorationCursor);
    } else {
        m_decorationConnection = QMetaObject::Connection();
    }
    updateDecorationCursor();
}

void CursorImage::updateDecorationCursor()
{
    m_decorationCursor.image = QImage();
    m_decorationCursor.hotSpot = QPoint();

    auto deco = m_pointer->decoration();
    if (AbstractClient *c = deco.isNull() ? nullptr : deco->client()) {
        loadThemeCursor(c->cursor(), &m_decorationCursor);
        if (m_currentSource == CursorSource::Decoration) {
            emit changed();
        }
    }
    reevaluteSource();
}

void CursorImage::updateMoveResize()
{
    m_moveResizeCursor.image = QImage();
    m_moveResizeCursor.hotSpot = QPoint();
    if (AbstractClient *c = workspace()->getMovingClient()) {
        loadThemeCursor(c->cursor(), &m_moveResizeCursor);
        if (m_currentSource == CursorSource::MoveResize) {
            emit changed();
        }
    }
    reevaluteSource();
}

void CursorImage::updateServerCursor()
{
    m_serverCursor.image = QImage();
    m_serverCursor.hotSpot = QPoint();
    reevaluteSource();
    const bool needsEmit = m_currentSource == CursorSource::LockScreen || m_currentSource == CursorSource::PointerSurface;
    auto p = waylandServer()->seat()->focusedPointer();
    if (!p) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto c = p->cursor();
    if (!c) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto cursorSurface = c->surface();
    if (cursorSurface.isNull()) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto buffer = cursorSurface.data()->buffer();
    if (!buffer) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    m_serverCursor.hotSpot = c->hotspot();
    m_serverCursor.image = buffer->data().copy();
    m_serverCursor.image.setDevicePixelRatio(cursorSurface->scale());
    if (needsEmit) {
        emit changed();
    }
}

void CursorImage::loadTheme()
{
    if (m_cursorTheme) {
        return;
    }
    // check whether we can create it
    if (waylandServer()->internalShmPool()) {
        m_cursorTheme = new WaylandCursorTheme(waylandServer()->internalShmPool(), this);
        connect(waylandServer(), &WaylandServer::terminatingInternalClientConnection, this,
            [this] {
                delete m_cursorTheme;
                m_cursorTheme = nullptr;
            }
        );
    }
}

void CursorImage::setEffectsOverrideCursor(Qt::CursorShape shape)
{
    loadThemeCursor(shape, &m_effectsCursor);
    if (m_currentSource == CursorSource::EffectsOverride) {
        emit changed();
    }
    reevaluteSource();
}

void CursorImage::removeEffectsOverrideCursor()
{
    reevaluteSource();
}

void CursorImage::setWindowSelectionCursor(const QByteArray &shape)
{
    if (shape.isEmpty()) {
        loadThemeCursor(Qt::CrossCursor, &m_windowSelectionCursor);
    } else {
        loadThemeCursor(shape, &m_windowSelectionCursor);
    }
    if (m_currentSource == CursorSource::WindowSelector) {
        emit changed();
    }
    reevaluteSource();
}

void CursorImage::removeWindowSelectionCursor()
{
    reevaluteSource();
}

void CursorImage::updateDrag()
{
    using namespace KWayland::Server;
    disconnect(m_drag.connection);
    m_drag.cursor.image = QImage();
    m_drag.cursor.hotSpot = QPoint();
    reevaluteSource();
    if (auto p = waylandServer()->seat()->dragPointer()) {
        m_drag.connection = connect(p, &PointerInterface::cursorChanged, this, &CursorImage::updateDragCursor);
    } else {
        m_drag.connection = QMetaObject::Connection();
    }
    updateDragCursor();
}

void CursorImage::updateDragCursor()
{
    m_drag.cursor.image = QImage();
    m_drag.cursor.hotSpot = QPoint();
    const bool needsEmit = m_currentSource == CursorSource::DragAndDrop;
    QImage additionalIcon;
    if (auto ddi = waylandServer()->seat()->dragSource()) {
        if (auto dragIcon = ddi->icon()) {
            if (auto buffer = dragIcon->buffer()) {
                additionalIcon = buffer->data().copy();
            }
        }
    }
    auto p = waylandServer()->seat()->dragPointer();
    if (!p) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto c = p->cursor();
    if (!c) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto cursorSurface = c->surface();
    if (cursorSurface.isNull()) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    auto buffer = cursorSurface.data()->buffer();
    if (!buffer) {
        if (needsEmit) {
            emit changed();
        }
        return;
    }
    m_drag.cursor.hotSpot = c->hotspot();
    m_drag.cursor.image = buffer->data().copy();
    if (needsEmit) {
        emit changed();
    }
    // TODO: add the cursor image
}

void CursorImage::loadThemeCursor(CursorShape shape, Image *image)
{
    loadThemeCursor(shape, m_cursors, image);
}

void CursorImage::loadThemeCursor(const QByteArray &shape, Image *image)
{
    loadThemeCursor(shape, m_cursorsByName, image);
}

template <typename T>
void CursorImage::loadThemeCursor(const T &shape, QHash<T, Image> &cursors, Image *image)
{
    loadTheme();
    if (!m_cursorTheme) {
        return;
    }
    auto it = cursors.constFind(shape);
    if (it == cursors.constEnd()) {
        image->image = QImage();
        image->hotSpot = QPoint();
        wl_cursor_image *cursor = m_cursorTheme->get(shape);
        if (!cursor) {
            return;
        }
        wl_buffer *b = wl_cursor_image_get_buffer(cursor);
        if (!b) {
            return;
        }
        waylandServer()->internalClientConection()->flush();
        waylandServer()->dispatch();
        auto buffer = KWayland::Server::BufferInterface::get(waylandServer()->internalConnection()->getResource(KWayland::Client::Buffer::getId(b)));
        if (!buffer) {
            return;
        }
        auto scale = screens()->maxScale();
        int hotSpotX = qRound(cursor->hotspot_x / scale);
        int hotSpotY = qRound(cursor->hotspot_y / scale);
        QImage img = buffer->data().copy();
        img.setDevicePixelRatio(scale);
        it = decltype(it)(cursors.insert(shape, {img, QPoint(hotSpotX, hotSpotY)}));
    }
    image->hotSpot = it.value().hotSpot;
    image->image = it.value().image;
}

void CursorImage::reevaluteSource()
{
    if (waylandServer()->seat()->isDragPointer()) {
        // TODO: touch drag?
        setSource(CursorSource::DragAndDrop);
        return;
    }
    if (waylandServer()->isScreenLocked()) {
        setSource(CursorSource::LockScreen);
        return;
    }
    if (input()->isSelectingWindow()) {
        setSource(CursorSource::WindowSelector);
        return;
    }
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->isMouseInterception()) {
        setSource(CursorSource::EffectsOverride);
        return;
    }
    if (workspace() && workspace()->getMovingClient()) {
        setSource(CursorSource::MoveResize);
        return;
    }
    if (!m_pointer->decoration().isNull()) {
        setSource(CursorSource::Decoration);
        return;
    }
    if (!m_pointer->focus().isNull() && waylandServer()->seat()->focusedPointer()) {
        setSource(CursorSource::PointerSurface);
        return;
    }
    setSource(CursorSource::Fallback);
}

void CursorImage::setSource(CursorSource source)
{
    if (m_currentSource == source) {
        return;
    }
    m_currentSource = source;
    emit changed();
}

QImage CursorImage::image() const
{
    switch (m_currentSource) {
    case CursorSource::EffectsOverride:
        return m_effectsCursor.image;
    case CursorSource::MoveResize:
        return m_moveResizeCursor.image;
    case CursorSource::LockScreen:
    case CursorSource::PointerSurface:
        // lockscreen also uses server cursor image
        return m_serverCursor.image;
    case CursorSource::Decoration:
        return m_decorationCursor.image;
    case CursorSource::DragAndDrop:
        return m_drag.cursor.image;
    case CursorSource::Fallback:
        return m_fallbackCursor.image;
    case CursorSource::WindowSelector:
        return m_windowSelectionCursor.image;
    default:
        Q_UNREACHABLE();
    }
}

QPoint CursorImage::hotSpot() const
{
    switch (m_currentSource) {
    case CursorSource::EffectsOverride:
        return m_effectsCursor.hotSpot;
    case CursorSource::MoveResize:
        return m_moveResizeCursor.hotSpot;
    case CursorSource::LockScreen:
    case CursorSource::PointerSurface:
        // lockscreen also uses server cursor image
        return m_serverCursor.hotSpot;
    case CursorSource::Decoration:
        return m_decorationCursor.hotSpot;
    case CursorSource::DragAndDrop:
        return m_drag.cursor.hotSpot;
    case CursorSource::Fallback:
        return m_fallbackCursor.hotSpot;
    case CursorSource::WindowSelector:
        return m_windowSelectionCursor.hotSpot;
    default:
        Q_UNREACHABLE();
    }
}

}
