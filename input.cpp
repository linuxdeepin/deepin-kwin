/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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
#include "input.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "touch_input.h"
#include "client.h"
#include "effects.h"
#include "globalshortcuts.h"
#include "logind.h"
#include "main.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "unmanaged.h"
#include "screenedge.h"
#include "screens.h"
#include "workspace.h"
#if HAVE_INPUT
#include "libinput/connection.h"
#include "libinput/device.h"
#endif
#include "platform.h"
#include "shell_client.h"
#include "wayland_server.h"
#include <KWayland/Server/display.h>
#include <KWayland/Server/fakeinput_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <decorations/decoratedclient.h>
#include <KDecoration2/Decoration>
//screenlocker
#include <KScreenLocker/KsldApp>
// Qt
#include <QKeyEvent>

#include <xkbcommon/xkbcommon.h>

namespace KWin
{

InputEventFilter::InputEventFilter() = default;

InputEventFilter::~InputEventFilter()
{
    if (input()) {
        input()->uninstallInputEventFilter(this);
    }
}

bool InputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    return false;
}

bool InputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    return false;
}

bool InputEventFilter::keyEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    return false;
}

bool InputEventFilter::touchDown(quint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::touchMotion(quint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::touchUp(quint32 id, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::pinchGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time)
{
    Q_UNUSED(scale)
    Q_UNUSED(angleDelta)
    Q_UNUSED(delta)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::pinchGestureEnd(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::pinchGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::swipeGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::swipeGestureUpdate(const QSizeF &delta, quint32 time)
{
    Q_UNUSED(delta)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::swipeGestureEnd(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::swipeGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
    return false;
}

void InputEventFilter::passToWaylandServer(QKeyEvent *event)
{
    Q_ASSERT(waylandServer());
    if (event->isAutoRepeat()) {
        return;
    }
    switch (event->type()) {
    case QEvent::KeyPress:
        waylandServer()->seat()->keyPressed(event->nativeScanCode());
        break;
    case QEvent::KeyRelease:
        waylandServer()->seat()->keyReleased(event->nativeScanCode());
        break;
    default:
        break;
    }
}

#if HAVE_INPUT
class VirtualTerminalFilter : public InputEventFilter {
public:
    bool keyEvent(QKeyEvent *event) override {
        // really on press and not on release? X11 switches on press.
        if (event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
            const xkb_keysym_t keysym = event->nativeVirtualKey();
            if (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
                LogindIntegration::self()->switchVirtualTerminal(keysym - XKB_KEY_XF86Switch_VT_1 + 1);
                return true;
            }
        }
        return false;
    }
};
#endif

class TerminateServerFilter : public InputEventFilter {
public:
    bool keyEvent(QKeyEvent *event) override {
        if (event->type() == QEvent::KeyPress && !event->isAutoRepeat()) {
            if (event->nativeVirtualKey() == XKB_KEY_Terminate_Server) {
                qCWarning(KWIN_CORE) << "Request to terminate server";
                QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
                return true;
            }
        }
        return false;
    }
};

class LockScreenFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        if (event->type() == QEvent::MouseMove) {
            if (event->buttons() == Qt::NoButton) {
                // update pointer window only if no button is pressed
                input()->pointer()->update();
            }
            if (pointerSurfaceAllowed()) {
                seat->setPointerPos(event->screenPos().toPoint());
            }
        } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            if (pointerSurfaceAllowed()) {
                event->type() == QEvent::MouseButtonPress ? seat->pointerButtonPressed(nativeButton) : seat->pointerButtonReleased(nativeButton);
            }
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        if (pointerSurfaceAllowed()) {
            seat->setTimestamp(event->timestamp());
            const Qt::Orientation orientation = event->angleDelta().x() == 0 ? Qt::Vertical : Qt::Horizontal;
            seat->pointerAxis(orientation, orientation == Qt::Horizontal ? event->angleDelta().x() : event->angleDelta().y());
        }
        return true;
    }
    bool keyEvent(QKeyEvent * event) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        if (event->isAutoRepeat()) {
            // wayland client takes care of it
            return true;
        }
        // send event to KSldApp for global accel
        // if event is set to accepted it means a whitelisted shortcut was triggered
        // in that case we filter it out and don't process it further
        event->setAccepted(false);
        QCoreApplication::sendEvent(ScreenLocker::KSldApp::self(), event);
        if (event->isAccepted()) {
            return true;
        }

        // continue normal processing
        input()->keyboard()->update();
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        if (!keyboardSurfaceAllowed()) {
            // don't pass event to seat
            return true;
        }
        switch (event->type()) {
        case QEvent::KeyPress:
            seat->keyPressed(event->nativeScanCode());
            break;
        case QEvent::KeyRelease:
            seat->keyReleased(event->nativeScanCode());
            break;
        default:
            break;
        }
        return true;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (!seat->isTouchSequence()) {
            input()->touch()->update(pos);
        }
        if (touchSurfaceAllowed()) {
            input()->touch()->insertId(id, seat->touchDown(pos));
        }
        return true;
    }
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (touchSurfaceAllowed()) {
            const qint32 kwaylandId = input()->touch()->mappedId(id);
            if (kwaylandId != -1) {
                seat->touchMove(kwaylandId, pos);
            }
        }
        return true;
    }
    bool touchUp(quint32 id, quint32 time) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (touchSurfaceAllowed()) {
            const qint32 kwaylandId = input()->touch()->mappedId(id);
            if (kwaylandId != -1) {
                seat->touchUp(kwaylandId);
                input()->touch()->removeId(id);
            }
        }
        return true;
    }
private:
    bool surfaceAllowed(KWayland::Server::SurfaceInterface *(KWayland::Server::SeatInterface::*method)() const) const {
        if (KWayland::Server::SurfaceInterface *s = (waylandServer()->seat()->*method)()) {
            if (Toplevel *t = waylandServer()->findClient(s)) {
                return t->isLockScreen() || t->isInputMethod();
            }
            return false;
        }
        return true;
    }
    bool pointerSurfaceAllowed() const {
        return surfaceAllowed(&KWayland::Server::SeatInterface::focusedPointerSurface);
    }
    bool keyboardSurfaceAllowed() const {
        return surfaceAllowed(&KWayland::Server::SeatInterface::focusedKeyboardSurface);
    }
    bool touchSurfaceAllowed() const {
        return surfaceAllowed(&KWayland::Server::SeatInterface::focusedTouchSurface);
    }
};

class EffectsFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(event);
    }
    bool keyEvent(QKeyEvent *event) override {
        if (!effects || !static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()) {
            return false;
        }
        waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
        passToWaylandServer(event);
        static_cast< EffectsHandlerImpl* >(effects)->grabbedKeyboardEvent(event);
        return true;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        if (!effects) {
            return false;
        }
        return static_cast< EffectsHandlerImpl* >(effects)->touchDown(id, pos, time);
    }
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override {
        if (!effects) {
            return false;
        }
        return static_cast< EffectsHandlerImpl* >(effects)->touchMotion(id, pos, time);
    }
    bool touchUp(quint32 id, quint32 time) override {
        if (!effects) {
            return false;
        }
        return static_cast< EffectsHandlerImpl* >(effects)->touchUp(id, time);
    }
};

class MoveResizeFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        AbstractClient *c = workspace()->getMovingClient();
        if (!c) {
            return false;
        }
        switch (event->type()) {
        case QEvent::MouseMove:
            c->updateMoveResize(event->screenPos().toPoint());
            break;
        case QEvent::MouseButtonRelease:
            if (event->buttons() == Qt::NoButton) {
                c->endMoveResize();
            }
            break;
        default:
            break;
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        Q_UNUSED(event)
        // filter out while moving a window
        return workspace()->getMovingClient() != nullptr;
    }
    bool keyEvent(QKeyEvent *event) override {
        AbstractClient *c = workspace()->getMovingClient();
        if (!c) {
            return false;
        }
        if (event->type() == QEvent::KeyPress) {
            c->keyPressEvent(event->key() | event->modifiers());
            if (c->isMove() || c->isResize()) {
                // only update if mode didn't end
                c->updateMoveResize(input()->globalPointer());
            }
        }
        return true;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(pos)
        Q_UNUSED(time)

        AbstractClient *c = workspace()->getMovingClient();
        // 记录move resize未开始之前的第一次触摸按下
        if (!c && m_touch_id == UINT_MAX) {
            m_touch_id = id;
        }

        return false;
    }
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(time)

        if (id != m_touch_id) {
            return false;
        }

        AbstractClient *c = workspace()->getMovingClient();
        if (!c) {
            return false;
        }

        c->updateMoveResize(pos);

        return true;
    }
    bool touchUp(quint32 id, quint32 time) override {
        Q_UNUSED(time)

        if (id != m_touch_id) {
            return false;
        }

        // 重置touch id
        m_touch_id = UINT_MAX;

        AbstractClient *c = workspace()->getMovingClient();
        if (!c) {
            return false;
        }

        c->endMoveResize();

        return true;
    }

private:
    quint32 m_touch_id = UINT_MAX;
};

class GlobalShortcutFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton);
        if (event->type() == QEvent::MouseButtonPress) {
            if (input()->shortcuts()->processPointerPressed(event->modifiers(), event->buttons())) {
                return true;
            }
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (event->modifiers() == Qt::NoModifier) {
            return false;
        }
        PointerAxisDirection direction = PointerAxisUp;
        if (event->angleDelta().x() < 0) {
            direction = PointerAxisRight;
        } else if (event->angleDelta().x() > 0) {
            direction = PointerAxisLeft;
        } else if (event->angleDelta().y() < 0) {
            direction = PointerAxisDown;
        } else if (event->angleDelta().y() > 0) {
            direction = PointerAxisUp;
        }
        return input()->shortcuts()->processAxis(event->modifiers(), direction);
    }
    bool keyEvent(QKeyEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            return input()->shortcuts()->processKey(input()->keyboard()->xkb()->modifiersRelevantForGlobalShortcuts(), event->nativeVirtualKey(), event->key());
        }
        return false;
    }
};

class InternalWindowEventFilter : public InputEventFilter {
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        auto internal = input()->pointer()->internalWindow();
        if (!internal) {
            return false;
        }
        if (event->buttons() == Qt::NoButton) {
            // update pointer window only if no button is pressed
            input()->pointer()->update();
        }
        if (!internal) {
            return false;
        }
        QMouseEvent e(event->type(),
                        event->pos() - internal->position(),
                        event->globalPos(),
                        event->button(), event->buttons(), event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);
        return e.isAccepted();
    }
    bool wheelEvent(QWheelEvent *event) override {
        auto internal = input()->pointer()->internalWindow();
        if (!internal) {
            return false;
        }
        const QPointF localPos = event->globalPosF() - QPointF(internal->x(), internal->y());
        const Qt::Orientation orientation = (event->angleDelta().x() != 0) ? Qt::Horizontal : Qt::Vertical;
        const int delta = event->angleDelta().x() != 0 ? event->angleDelta().x() : event->angleDelta().y();
        QWheelEvent e(localPos, event->globalPosF(), QPoint(),
                        event->angleDelta() * -1,
                        delta * -1,
                        orientation,
                        event->buttons(),
                        event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);
        return e.isAccepted();
    }
    bool keyEvent(QKeyEvent *event) override {
        const auto &internalClients = waylandServer()->internalClients();
        if (internalClients.isEmpty()) {
            return false;
        }
        QWindow *found = nullptr;
        auto it = internalClients.end();
        do {
            it--;
            if (QWindow *w = (*it)->internalWindow()) {
                if (!w->isVisible()) {
                    continue;
                }
                if (!screens()->geometry().contains(w->geometry())) {
                    continue;
                }
                if (w->property("_q_showWithoutActivating").toBool()) {
                    continue;
                }
                found = w;
                break;
            }
        } while (it != internalClients.begin());
        if (!found) {
            return false;
        }
        event->setAccepted(false);
        if (QCoreApplication::sendEvent(found, event)) {
            waylandServer()->seat()->setFocusedKeyboardSurface(nullptr);
            passToWaylandServer(event);
            return true;
        }
        return false;
    }

    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            // something else is getting the events
            return false;
        }
        auto touch = input()->touch();
        if (touch->internalPressId() != -1) {
            // already on a decoration, ignore further touch points, but filter out
            return true;
        }
        // a new touch point
        seat->setTimestamp(time);
        touch->update(pos);
        auto internal = touch->internalWindow();
        if (!internal) {
            return false;
        }
        touch->setInternalPressId(id);
        // Qt's touch event API is rather complex, let's do fake mouse events instead
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - QPointF(internal->x(), internal->y());
        QMouseEvent e(QEvent::MouseButtonPress, m_lastLocalTouchPos, pos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);
        return true;
    }
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override {
        auto touch = input()->touch();
        auto internal = touch->internalWindow();
        if (!internal) {
            return false;
        }
        if (touch->internalPressId() == -1) {
            return false;
        }
        waylandServer()->seat()->setTimestamp(time);
        if (touch->internalPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - QPointF(internal->x(), internal->y());
        QMouseEvent e(QEvent::MouseMove, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        QCoreApplication::instance()->sendEvent(internal.data(), &e);
        return true;
    }
    bool touchUp(quint32 id, quint32 time) override {
        auto touch = input()->touch();
        auto internal = touch->internalWindow();
        if (!internal) {
            return false;
        }
        if (touch->internalPressId() == -1) {
            return false;
        }
        waylandServer()->seat()->setTimestamp(time);
        if (touch->internalPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }
        // send mouse up
        QMouseEvent e(QEvent::MouseButtonRelease, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::MouseButtons(), input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);

        m_lastGlobalTouchPos = QPointF();
        m_lastLocalTouchPos = QPointF();
        input()->touch()->setInternalPressId(-1);
        return true;
    }
private:
    QPointF m_lastGlobalTouchPos;
    QPointF m_lastLocalTouchPos;
};

class DecorationEventFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF p = event->globalPos() - decoration->client()->pos();
        switch (event->type()) {
        case QEvent::MouseMove: {
            if (event->buttons() == Qt::NoButton) {
                return false;
            }
            QHoverEvent e(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
            decoration->client()->processDecorationMove(p.toPoint(), event->globalPos());
            return true;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease: {
            QMouseEvent e(event->type(), p, event->globalPos(), event->button(), event->buttons(), event->modifiers());
            e.setAccepted(false);
            QCoreApplication::sendEvent(decoration->decoration(), &e);
            if (!e.isAccepted() && event->type() == QEvent::MouseButtonPress) {
                decoration->client()->processDecorationButtonPress(&e);
            }
            if (event->type() == QEvent::MouseButtonRelease) {
                decoration->client()->processDecorationButtonRelease(&e);
            }
            return true;
        }
        default:
            break;
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF localPos = event->globalPosF() - decoration->client()->pos();
        const Qt::Orientation orientation = (event->angleDelta().x() != 0) ? Qt::Horizontal : Qt::Vertical;
        const int delta = event->angleDelta().x() != 0 ? event->angleDelta().x() : event->angleDelta().y();
        QWheelEvent e(localPos, event->globalPosF(), QPoint(),
                        event->angleDelta(),
                        delta,
                        orientation,
                        event->buttons(),
                        event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration.data(), &e);
        if (e.isAccepted()) {
            return true;
        }
        if ((orientation == Qt::Vertical) && decoration->client()->titlebarPositionUnderMouse()) {
            decoration->client()->performMouseCommand(options->operationTitlebarMouseWheel(delta * -1),
                                                        event->globalPosF().toPoint());
        }
        return true;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            return false;
        }
        if (input()->touch()->decorationPressId() != -1) {
            // already on a decoration, ignore further touch points, but filter out
            return true;
        }
        seat->setTimestamp(time);
        input()->touch()->update(pos);
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            return false;
        }
        input()->touch()->setDecorationPressId(id);
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - decoration->client()->pos();
        QMouseEvent e(QEvent::MouseButtonPress, m_lastLocalTouchPos, pos, Qt::LeftButton, Qt::LeftButton, input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        if (!e.isAccepted()) {
            decoration->client()->processDecorationButtonPress(&e);
        }
        return true;
    }
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(time)
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            return false;
        }
        if (input()->touch()->decorationPressId() == -1) {
            return false;
        }
        if (input()->touch()->decorationPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }
        m_lastGlobalTouchPos = pos;
        m_lastLocalTouchPos = pos - decoration->client()->pos();
        QHoverEvent e(QEvent::HoverMove, m_lastLocalTouchPos, m_lastLocalTouchPos);
        QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
        decoration->client()->processDecorationMove(m_lastLocalTouchPos.toPoint(), pos.toPoint());
        return true;
    }
    bool touchUp(quint32 id, quint32 time) override {
        Q_UNUSED(time);
        auto decoration = input()->touch()->decoration();
        if (!decoration) {
            return false;
        }
        if (input()->touch()->decorationPressId() == -1) {
            return false;
        }
        if (input()->touch()->decorationPressId() != qint32(id)) {
            // ignore, but filter out
            return true;
        }
        // send mouse up
        QMouseEvent e(QEvent::MouseButtonRelease, m_lastLocalTouchPos, m_lastGlobalTouchPos, Qt::LeftButton, Qt::MouseButtons(), input()->keyboardModifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration->decoration(), &e);
        decoration->client()->processDecorationButtonRelease(&e);

        m_lastGlobalTouchPos = QPointF();
        m_lastLocalTouchPos = QPointF();
        input()->touch()->setDecorationPressId(-1);
        return true;
    }
private:
    QPointF m_lastGlobalTouchPos;
    QPointF m_lastLocalTouchPos;
};

#ifdef KWIN_BUILD_TABBOX
class TabBoxInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 button) override {
        Q_UNUSED(button)
        if (!TabBox::TabBox::self() || !TabBox::TabBox::self()->isGrabbed()) {
            return false;
        }
        return TabBox::TabBox::self()->handleMouseEvent(event);
    }
    bool keyEvent(QKeyEvent *event) override {
        if (!TabBox::TabBox::self() || !TabBox::TabBox::self()->isGrabbed()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setFocusedKeyboardSurface(nullptr);
        // pass the key event to the seat, so that it has a proper model of the currently hold keys
        // this is important for combinations like alt+shift to ensure that shift is not considered pressed
        passToWaylandServer(event);

        if (event->type() == QEvent::KeyPress) {
            TabBox::TabBox::self()->keyPress(event->modifiers() | event->key());
        } else if (input()->keyboard()->xkb()->modifiersRelevantForGlobalShortcuts() == Qt::NoModifier) {
            TabBox::TabBox::self()->modifiersReleased();
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (!TabBox::TabBox::self() || !TabBox::TabBox::self()->isGrabbed()) {
            return false;
        }
        return TabBox::TabBox::self()->handleWheelEvent(event);
    }
};
#endif

class ScreenEdgeInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        ScreenEdges::self()->isEntered(event);
        // always forward
        return false;
    }
};

/**
 * This filter implements window actions. If the event should not be passed to the
 * current pointer window it will filter out the event
 **/
class WindowActionInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        if (event->type() != QEvent::MouseButtonPress) {
            return false;
        }
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->pointer()->window().data());
        if (!c) {
            return false;
        }
        bool wasAction = false;
        Options::MouseCommand command = Options::MouseNothing;
        if (event->modifiers() == options->commandAllModifier()) {
            wasAction = true;
            switch (event->button()) {
            case Qt::LeftButton:
                command = options->commandAll1();
                break;
            case Qt::MiddleButton:
                command = options->commandAll2();
                break;
            case Qt::RightButton:
                command = options->commandAll3();
                break;
            default:
                // nothing
                break;
            }
        } else {
            command = c->getMouseCommand(event->button(), &wasAction);
        }
        if (wasAction) {
            return !c->performMouseCommand(command, event->globalPos());
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (event->angleDelta().y() == 0) {
            // only actions on vertical scroll
            return false;
        }
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->pointer()->window().data());
        if (!c) {
            return false;
        }
        bool wasAction = false;
        Options::MouseCommand command = Options::MouseNothing;
        if (event->modifiers() == options->commandAllModifier()) {
            wasAction = true;
            command = options->operationWindowMouseWheel(-1 * event->angleDelta().y());
        } else {
            command = c->getWheelCommand(Qt::Vertical, &wasAction);
        }
        if (wasAction) {
            return !c->performMouseCommand(command, event->globalPos());
        }
        return false;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(id)
        Q_UNUSED(time)
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            return false;
        }
        input()->touch()->update(pos);
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->touch()->window().data());
        if (!c) {
            return false;
        }
        bool wasAction = false;
        const Options::MouseCommand command = c->getMouseCommand(Qt::LeftButton, &wasAction);
        if (wasAction) {
            return !c->performMouseCommand(command, pos.toPoint());
        }
        return false;
    }
};

/**
 * The remaining default input filter which forwards events to other windows
 **/
class ForwardInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        switch (event->type()) {
        case QEvent::MouseMove:
            if (event->buttons() == Qt::NoButton) {
                // update pointer window only if no button is pressed
                input()->pointer()->update();
            }
            seat->setPointerPos(event->globalPos());
            break;
        case QEvent::MouseButtonPress:
            seat->pointerButtonPressed(nativeButton);
            break;
        case QEvent::MouseButtonRelease:
            seat->pointerButtonReleased(nativeButton);
            if (event->buttons() == Qt::NoButton) {
                input()->pointer()->update();
            }
            break;
        default:
            break;
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        const Qt::Orientation orientation = event->angleDelta().x() == 0 ? Qt::Vertical : Qt::Horizontal;
        seat->pointerAxis(orientation, orientation == Qt::Horizontal ? event->angleDelta().x() : event->angleDelta().y());
        return true;
    }
    bool keyEvent(QKeyEvent *event) override {
        if (!workspace()) {
            return false;
        }
        if (event->isAutoRepeat()) {
            // handled by Wayland client
            return false;
        }
        auto seat = waylandServer()->seat();
        input()->keyboard()->update();
        seat->setTimestamp(event->timestamp());
        passToWaylandServer(event);
        return true;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (!seat->isTouchSequence()) {
            input()->touch()->update(pos);
        }
        input()->touch()->insertId(id, seat->touchDown(pos));
        return true;
    }
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        const qint32 kwaylandId = input()->touch()->mappedId(id);
        if (kwaylandId != -1) {
            seat->touchMove(kwaylandId, pos);
        }
        return true;
    }
    bool touchUp(quint32 id, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        const qint32 kwaylandId = input()->touch()->mappedId(id);
        if (kwaylandId != -1) {
            seat->touchUp(kwaylandId);
            input()->touch()->removeId(id);
        }
        return true;
    }
};

class DragAndDropInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        auto seat = waylandServer()->seat();
        if (!seat->isDragPointer()) {
            return false;
        }
        seat->setTimestamp(event->timestamp());
        switch (event->type()) {
        case QEvent::MouseMove: {
            if (Toplevel *t = input()->findToplevel(event->globalPos())) {
                // TODO: consider decorations
                if (t->surface() != seat->dragSurface()) {
                    if (AbstractClient *c = qobject_cast<AbstractClient*>(t)) {
                        workspace()->raiseClient(c);
                    }
                    seat->setPointerPos(event->globalPos());
                    seat->setDragTarget(t->surface(), event->globalPos(), t->inputTransformation());
                }
            } else {
                // no window at that place, if we have a surface we need to reset
                seat->setDragTarget(nullptr);
            }
            seat->setPointerPos(event->globalPos());
            break;
        }
        case QEvent::MouseButtonPress:
            seat->pointerButtonPressed(nativeButton);
            break;
        case QEvent::MouseButtonRelease:
            seat->pointerButtonReleased(nativeButton);
            break;
        default:
            break;
        }
        // TODO: should we pass through effects?
        return true;
    }
};

KWIN_SINGLETON_FACTORY(InputRedirection)

InputRedirection::InputRedirection(QObject *parent)
    : QObject(parent)
    , m_keyboard(new KeyboardInputRedirection(this))
    , m_pointer(new PointerInputRedirection(this))
    , m_touch(new TouchInputRedirection(this))
    , m_shortcuts(new GlobalShortcutsManager(this))
{
    qRegisterMetaType<KWin::InputRedirection::KeyboardKeyState>();
    qRegisterMetaType<KWin::InputRedirection::PointerButtonState>();
    qRegisterMetaType<KWin::InputRedirection::PointerAxis>();
#if HAVE_INPUT
    if (Application::usesLibinput()) {
        if (LogindIntegration::self()->hasSessionControl()) {
            setupLibInput();
        } else {
            if (LogindIntegration::self()->isConnected()) {
                LogindIntegration::self()->takeControl();
            } else {
                connect(LogindIntegration::self(), &LogindIntegration::connectedChanged, LogindIntegration::self(), &LogindIntegration::takeControl);
            }
            connect(LogindIntegration::self(), &LogindIntegration::hasSessionControlChanged, this,
                [this] (bool sessionControl) {
                    if (sessionControl) {
                        setupLibInput();
                    }
                }
            );
        }
        m_inputConfig = KSharedConfig::openConfig(QStringLiteral("kcminputrc"));
    }
#endif
    connect(kwinApp(), &Application::workspaceCreated, this, &InputRedirection::setupWorkspace);
    reconfigure();
}

InputRedirection::~InputRedirection()
{
    s_self = NULL;
    qDeleteAll(m_filters);
}

void InputRedirection::installInputEventFilter(InputEventFilter *filter)
{
    m_filters << filter;
}

void InputRedirection::prepandInputEventFilter(InputEventFilter *filter)
{
    m_filters.prepend(filter);
}

void InputRedirection::uninstallInputEventFilter(InputEventFilter *filter)
{
    m_filters.removeAll(filter);
}

void InputRedirection::init()
{
    m_shortcuts->init();
}

void InputRedirection::setupWorkspace()
{
    if (waylandServer()) {
        using namespace KWayland::Server;
        FakeInputInterface *fakeInput = waylandServer()->display()->createFakeInput(this);
        fakeInput->create();
        connect(fakeInput, &FakeInputInterface::deviceCreated, this,
            [this] (FakeInputDevice *device) {
                connect(device, &FakeInputDevice::authenticationRequested, this,
                    [this, device] (const QString &application, const QString &reason) {
                        Q_UNUSED(application)
                        Q_UNUSED(reason)
                        // TODO: make secure
                        device->setAuthentication(true);
                    }
                );
                connect(device, &FakeInputDevice::pointerMotionRequested, this,
                    [this] (const QSizeF &delta) {
                        // TODO: Fix time
                        m_pointer->processMotion(globalPointer() + QPointF(delta.width(), delta.height()), 0);
                    }
                );
                connect(device, &FakeInputDevice::pointerButtonPressRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        m_pointer->processButton(button, InputRedirection::PointerButtonPressed, 0);
                    }
                );
                connect(device, &FakeInputDevice::pointerButtonReleaseRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        m_pointer->processButton(button, InputRedirection::PointerButtonReleased, 0);
                    }
                );
                connect(device, &FakeInputDevice::pointerAxisRequested, this,
                    [this] (Qt::Orientation orientation, qreal delta) {
                        // TODO: Fix time
                        InputRedirection::PointerAxis axis;
                        switch (orientation) {
                        case Qt::Horizontal:
                            axis = InputRedirection::PointerAxisHorizontal;
                            break;
                        case Qt::Vertical:
                            axis = InputRedirection::PointerAxisVertical;
                            break;
                        default:
                            Q_UNREACHABLE();
                            break;
                        }
                        // TODO: Fix time
                        m_pointer->processAxis(axis, delta, 0);
                    }
                );
                connect(device, &FakeInputDevice::touchDownRequested, this,
                   [this] (quint32 id, const QPointF &pos) {
                       // TODO: Fix time
                       m_touch->processDown(id, pos, 0);
                   }
                );
                connect(device, &FakeInputDevice::touchMotionRequested, this,
                   [this] (quint32 id, const QPointF &pos) {
                       // TODO: Fix time
                       m_touch->processMotion(id, pos, 0);
                   }
                );
                connect(device, &FakeInputDevice::touchUpRequested, this,
                    [this] (quint32 id) {
                        // TODO: Fix time
                        m_touch->processUp(id, 0);
                    }
                );
                connect(device, &FakeInputDevice::touchCancelRequested, this,
                    [this] () {
                        m_touch->cancel();
                    }
                );
                connect(device, &FakeInputDevice::touchFrameRequested, this,
                   [this] () {
                       m_touch->frame();
                   }
                );
            }
        );
        connect(workspace(), &Workspace::configChanged, this, &InputRedirection::reconfigure);

        m_keyboard->init();
        m_pointer->init();
        m_touch->init();
    }
    setupInputFilters();
}

void InputRedirection::setupInputFilters()
{
#if HAVE_INPUT
    if (LogindIntegration::self()->hasSessionControl()) {
        installInputEventFilter(new VirtualTerminalFilter);
    }
#endif
    MoveResizeFilter *mr_filter = new MoveResizeFilter;
    if (waylandServer()) {
        installInputEventFilter(new TerminateServerFilter);
        installInputEventFilter(new DragAndDropInputFilter);
        installInputEventFilter(new LockScreenFilter);
    } else {
        // 非wayland平台时保证move resize事件过滤生效
        m_firstFilter = mr_filter;
    }
    installInputEventFilter(new ScreenEdgeInputFilter);
    installInputEventFilter(new EffectsFilter);
    installInputEventFilter(mr_filter);
#ifdef KWIN_BUILD_TABBOX
    installInputEventFilter(new TabBoxInputFilter);
#endif
    installInputEventFilter(new GlobalShortcutFilter);
    installInputEventFilter(new InternalWindowEventFilter);
    installInputEventFilter(new DecorationEventFilter);
    if (waylandServer()) {
        installInputEventFilter(new WindowActionInputFilter);
        installInputEventFilter(new ForwardInputFilter);
    }
}

void InputRedirection::reconfigure()
{
#if HAVE_INPUT
    if (Application::usesLibinput()) {
        m_inputConfig->reparseConfiguration();
        const auto config = m_inputConfig->group(QStringLiteral("keyboard"));
        const int delay = config.readEntry("RepeatDelay", 660);
        const int rate = config.readEntry("RepeatRate", 25);
        const bool enabled = config.readEntry("KeyboardRepeating", 0) == 0;

        waylandServer()->seat()->setKeyRepeatInfo(enabled ? rate : 0, delay);
    }
#endif
}

static KWayland::Server::SeatInterface *findSeat()
{
    auto server = waylandServer();
    if (!server) {
        return nullptr;
    }
    return server->seat();
}

void InputRedirection::setupLibInput()
{
#if HAVE_INPUT
    if (!Application::usesLibinput()) {
        return;
    }
    if (m_libInput) {
        return;
    }
    LibInput::Connection *conn = LibInput::Connection::create(this);
    m_libInput = conn;
    if (conn) {
        conn->setInputConfig(m_inputConfig);
        conn->updateLEDs(m_keyboard->xkb()->leds());
        conn->setup();
        connect(m_keyboard, &KeyboardInputRedirection::ledsChanged, conn, &LibInput::Connection::updateLEDs);
        connect(conn, &LibInput::Connection::eventsRead, this,
            [this] {
                m_libInput->processEvents();
            }, Qt::QueuedConnection
        );
        connect(conn, &LibInput::Connection::pointerButtonChanged, m_pointer, &PointerInputRedirection::processButton);
        connect(conn, &LibInput::Connection::pointerAxisChanged, m_pointer, &PointerInputRedirection::processAxis);
        connect(conn, &LibInput::Connection::pinchGestureBegin, m_pointer, &PointerInputRedirection::processPinchGestureBegin);
        connect(conn, &LibInput::Connection::pinchGestureUpdate, m_pointer, &PointerInputRedirection::processPinchGestureUpdate);
        connect(conn, &LibInput::Connection::pinchGestureEnd, m_pointer, &PointerInputRedirection::processPinchGestureEnd);
        connect(conn, &LibInput::Connection::pinchGestureCancelled, m_pointer, &PointerInputRedirection::processPinchGestureCancelled);
        connect(conn, &LibInput::Connection::swipeGestureBegin, m_pointer, &PointerInputRedirection::processSwipeGestureBegin);
        connect(conn, &LibInput::Connection::swipeGestureUpdate, m_pointer, &PointerInputRedirection::processSwipeGestureUpdate);
        connect(conn, &LibInput::Connection::swipeGestureEnd, m_pointer, &PointerInputRedirection::processSwipeGestureEnd);
        connect(conn, &LibInput::Connection::swipeGestureCancelled, m_pointer, &PointerInputRedirection::processSwipeGestureCancelled);
        connect(conn, &LibInput::Connection::keyChanged, m_keyboard, &KeyboardInputRedirection::processKey);
        connect(conn, &LibInput::Connection::pointerMotion, this,
            [this] (QPointF delta, uint32_t time, LibInput::Device *device) {
                m_pointer->processMotion(m_pointer->pos() + delta, time, device);
            }
        );
        connect(conn, &LibInput::Connection::pointerMotionAbsolute, this,
            [this] (QPointF orig, QPointF screen, uint32_t time, LibInput::Device *device) {
                Q_UNUSED(orig)
                m_pointer->processMotion(screen, time, device);
            }
        );
        connect(conn, &LibInput::Connection::touchDown, m_touch, &TouchInputRedirection::processDown);
        connect(conn, &LibInput::Connection::touchUp, m_touch, &TouchInputRedirection::processUp);
        connect(conn, &LibInput::Connection::touchMotion, m_touch, &TouchInputRedirection::processMotion);
        connect(conn, &LibInput::Connection::touchCanceled, m_touch, &TouchInputRedirection::cancel);
        connect(conn, &LibInput::Connection::touchFrame, m_touch, &TouchInputRedirection::frame);
        if (screens()) {
            setupLibInputWithScreens();
        } else {
            connect(kwinApp(), &Application::screensCreated, this, &InputRedirection::setupLibInputWithScreens);
        }
        if (auto s = findSeat()) {
            // Workaround for QTBUG-54371: if there is no real keyboard Qt doesn't request virtual keyboard
            s->setHasKeyboard(true);
            s->setHasPointer(conn->hasPointer());
            s->setHasTouch(conn->hasTouch());
            connect(conn, &LibInput::Connection::hasAlphaNumericKeyboardChanged, this,
                [this] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    // TODO: this should update the seat, only workaround for QTBUG-54371
                    emit hasAlphaNumericKeyboardChanged(set);
                }
            );
            connect(conn, &LibInput::Connection::hasPointerChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasPointer(set);
                }
            );
            connect(conn, &LibInput::Connection::hasTouchChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasTouch(set);
                }
            );
        }
        connect(LogindIntegration::self(), &LogindIntegration::sessionActiveChanged, m_libInput,
            [this] (bool active) {
                if (!active) {
                    m_libInput->deactivate();
                }
            }
        );
    }
#endif
}

bool InputRedirection::hasAlphaNumericKeyboard()
{
#if HAVE_INPUT
    if (m_libInput) {
        return m_libInput->hasAlphaNumericKeyboard();
    }
#endif
    return true;
}

void InputRedirection::setupLibInputWithScreens()
{
#if HAVE_INPUT
    if (!screens() || !m_libInput) {
        return;
    }
    m_libInput->setScreenSize(screens()->size());
    connect(screens(), &Screens::sizeChanged, this,
        [this] {
            m_libInput->setScreenSize(screens()->size());
        }
    );
#endif
}

void InputRedirection::processPointerMotion(const QPointF &pos, uint32_t time)
{
    QMouseEvent event(QEvent::MouseMove, pos, QPointF(), pos, Qt::NoButton,
                      qApp->mouseButtons(), qApp->queryKeyboardModifiers(),
                      Qt::MouseEventSynthesizedByApplication);

    if (m_firstFilter && m_firstFilter->pointerEvent(&event, 0)) {
        return;
    }

    m_pointer->processMotion(pos, time);
}

void InputRedirection::processPointerButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time)
{
    const QPointF pos = QCursor::pos();
    QMouseEvent event(state == PointerButtonReleased ? QEvent::MouseButtonRelease : QEvent::MouseButtonPress,
                      pos, QPointF(), pos, buttonToQtMouseButton(button),
                      qApp->mouseButtons(), qApp->queryKeyboardModifiers(),
                      Qt::MouseEventSynthesizedByApplication);

    if (m_firstFilter && m_firstFilter->pointerEvent(&event, button)) {
        return;
    }

    m_pointer->processButton(button, state, time);
}

void InputRedirection::processPointerAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time)
{
    m_pointer->processAxis(axis, delta, time);
}

void InputRedirection::processKeyboardKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time)
{
    m_keyboard->processKey(key, state, time);
}

void InputRedirection::processKeyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    m_keyboard->processModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void InputRedirection::processKeymapChange(int fd, uint32_t size)
{
    m_keyboard->processKeymapChange(fd, size);
}

void InputRedirection::processTouchDown(qint32 id, const QPointF &pos, quint32 time)
{
    if (m_firstFilter && m_firstFilter->touchDown(id, pos, time)) {
        return;
    }

    m_touch->processDown(id, pos, time);
}

void InputRedirection::processTouchUp(qint32 id, quint32 time)
{
    if (m_firstFilter && m_firstFilter->touchUp(id, time)) {
        return;
    }

    m_touch->processUp(id, time);
}

void InputRedirection::processTouchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    if (m_firstFilter && m_firstFilter->touchMotion(id, pos, time)) {
        return;
    }

    m_touch->processMotion(id, pos, time);
}

void InputRedirection::cancelTouch()
{
    m_touch->cancel();
}

void InputRedirection::touchFrame()
{
    m_touch->frame();
}

Qt::MouseButtons InputRedirection::qtButtonStates() const
{
    return m_pointer->buttons();
}

static bool acceptsInput(Toplevel *t, const QPoint &pos)
{
    const QRegion input = t->inputShape();
    if (input.isEmpty()) {
        return true;
    }
    return input.translated(t->pos()).contains(pos);
}

Toplevel *InputRedirection::findToplevel(const QPoint &pos)
{
    if (!Workspace::self()) {
        return nullptr;
    }
    const bool isScreenLocked = waylandServer() && waylandServer()->isScreenLocked();
    // TODO: check whether the unmanaged wants input events at all
    if (!isScreenLocked) {
        // if an effect overrides the cursor we don't have a window to focus
        if (effects && static_cast<EffectsHandlerImpl*>(effects)->isMouseInterception()) {
            return nullptr;
        }
        const UnmanagedList &unmanaged = Workspace::self()->unmanagedList();
        foreach (Unmanaged *u, unmanaged) {
            if (u->geometry().contains(pos) && acceptsInput(u, pos)) {
                return u;
            }
        }
    }
    const ToplevelList &stacking = Workspace::self()->stackingOrder();
    if (stacking.isEmpty()) {
        return NULL;
    }
    auto it = stacking.end();
    do {
        --it;
        Toplevel *t = (*it);
        if (t->isDeleted()) {
            // a deleted window doesn't get mouse events
            continue;
        }
        if (AbstractClient *c = dynamic_cast<AbstractClient*>(t)) {
            if (!c->isOnCurrentActivity() || !c->isOnCurrentDesktop() || c->isMinimized() || !c->isCurrentTab() || c->isHiddenInternal()) {
                continue;
            }
        }
        if (!t->readyForPainting()) {
            continue;
        }
        if (isScreenLocked) {
            if (!t->isLockScreen() && !t->isInputMethod()) {
                continue;
            }
        }
        if (t->inputGeometry().contains(pos) && acceptsInput(t, pos)) {
            return t;
        }
    } while (it != stacking.begin());
    return NULL;
}

Qt::KeyboardModifiers InputRedirection::keyboardModifiers() const
{
    return m_keyboard->modifiers();
}

void InputRedirection::registerShortcut(const QKeySequence &shortcut, QAction *action)
{
    m_shortcuts->registerShortcut(action, shortcut);
    registerShortcutForGlobalAccelTimestamp(action);
}

void InputRedirection::registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action)
{
    m_shortcuts->registerPointerShortcut(action, modifiers, pointerButtons);
}

void InputRedirection::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
    m_shortcuts->registerAxisShortcut(action, modifiers, axis);
}

void InputRedirection::registerGlobalAccel(KGlobalAccelInterface *interface)
{
    m_shortcuts->setKGlobalAccelInterface(interface);
}

void InputRedirection::registerShortcutForGlobalAccelTimestamp(QAction *action)
{
    connect(action, &QAction::triggered, kwinApp(), [action] {
        QVariant timestamp = action->property("org.kde.kglobalaccel.activationTimestamp");
        bool ok = false;
        const quint32 t = timestamp.toULongLong(&ok);
        if (ok) {
            kwinApp()->setX11Time(t);
        }
    });
}

void InputRedirection::warpPointer(const QPointF &pos)
{
    m_pointer->warp(pos);
}

bool InputRedirection::supportsPointerWarping() const
{
    return m_pointer->supportsWarping();
}


QPointF InputRedirection::globalPointer() const
{
    return m_pointer->pos();
}

InputDeviceHandler::InputDeviceHandler(InputRedirection *input)
    : QObject(input)
    , m_input(input)
{
}

InputDeviceHandler::~InputDeviceHandler() = default;

void InputDeviceHandler::updateDecoration(Toplevel *t, const QPointF &pos)
{
    const auto oldDeco = m_decoration;
    bool needsReset = waylandServer()->isScreenLocked();
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(t)) {
        // check whether it's on a Decoration
        if (c->decoratedClient()) {
            const QRect clientRect = QRect(c->clientPos(), c->clientSize()).translated(c->pos());
            if (!clientRect.contains(pos.toPoint())) {
                m_decoration = c->decoratedClient();
            } else {
                needsReset = true;
            }
        } else {
            needsReset = true;
        }
    } else {
        needsReset = true;
    }
    if (needsReset) {
        m_decoration.clear();
    }

    bool leftSend = false;
    auto oldWindow = qobject_cast<AbstractClient*>(m_window.data());
    if (oldWindow && (m_decoration && m_decoration->client() != oldWindow)) {
        leftSend = true;
        oldWindow->leaveEvent();
    }

    if (oldDeco && oldDeco != m_decoration) {
        if (oldDeco->client() != t && !leftSend) {
            leftSend = true;
            oldDeco->client()->leaveEvent();
        }
        // send leave
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(oldDeco->decoration(), &event);
    }
    if (m_decoration) {
        if (m_decoration->client() != oldWindow) {
            m_decoration->client()->enterEvent(pos.toPoint());
            workspace()->updateFocusMousePosition(pos.toPoint());
        }
        const QPointF p = pos - t->pos();
        QHoverEvent event(QEvent::HoverMove, p, p);
        QCoreApplication::instance()->sendEvent(m_decoration->decoration(), &event);
        m_decoration->client()->processDecorationMove(p.toPoint(), pos.toPoint());
    }
}

void InputDeviceHandler::updateInternalWindow(const QPointF &pos)
{
    const auto oldInternalWindow = m_internalWindow;
    bool found = false;
    // TODO: screen locked check without going through wayland server
    bool needsReset = waylandServer()->isScreenLocked();
    const auto &internalClients = waylandServer()->internalClients();
    const bool change = m_internalWindow.isNull() || !(m_internalWindow->flags().testFlag(Qt::Popup) && m_internalWindow->isVisible());
    if (!internalClients.isEmpty() && change) {
        auto it = internalClients.end();
        do {
            it--;
            if (QWindow *w = (*it)->internalWindow()) {
                if (!w->isVisible()) {
                    continue;
                }
                if ((*it)->geometry().contains(pos.toPoint())) {
                    // check input mask
                    const QRegion mask = w->mask().translated(w->geometry().topLeft());
                    if (!mask.isEmpty() && !mask.contains(pos.toPoint())) {
                        continue;
                    }
                    m_internalWindow = QPointer<QWindow>(w);
                    found = true;
                    break;
                }
            }
        } while (it != internalClients.begin());
        if (!found) {
            needsReset = true;
        }
    }
    if (needsReset) {
        m_internalWindow.clear();
    }
    if (oldInternalWindow != m_internalWindow) {
        // changed
        if (oldInternalWindow) {
            QEvent event(QEvent::Leave);
            QCoreApplication::sendEvent(oldInternalWindow.data(), &event);
        }
        if (m_internalWindow) {
            QEnterEvent event(pos - m_internalWindow->position(),
                              pos - m_internalWindow->position(),
                              pos);
            QCoreApplication::sendEvent(m_internalWindow.data(), &event);
        }
        emit internalWindowChanged();
    }
}

} // namespace
