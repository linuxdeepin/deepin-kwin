// Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>
// Copyright (C) 2018 Roman Gilg <subdiff@gmail.com>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_POINTER_INPUT_H
#define KWIN_POINTER_INPUT_H

#include "input.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QPointF>

class QWindow;

namespace KWayland
{
namespace Server
{
class SurfaceInterface;
}
}

namespace KWin
{
class CursorImage;
class InputRedirection;
class Toplevel;
class WaylandCursorTheme;
class CursorShape;

namespace Decoration
{
class DecoratedClientImpl;
}

namespace LibInput
{
class Device;
}

uint32_t qtMouseButtonToButton(Qt::MouseButton button);

class KWIN_EXPORT PointerInputRedirection : public InputDeviceHandler
{
    Q_OBJECT
public:
    explicit PointerInputRedirection(InputRedirection *parent);
    virtual ~PointerInputRedirection();

    void init();
    void updatePosition(const QPointF &pos);

    void updateAfterScreenChange();
    bool supportsWarping() const;
    void warp(const QPointF &pos);

    QPointF pos() const {
        return m_pos;
    }
    Qt::MouseButtons buttons() const {
        return m_qtButtons;
    }
    bool areButtonsPressed() const;

    void setCursorShape(Qt::CursorShape);
    QImage cursorImage() const;
    QPoint cursorHotSpot() const;
    void markCursorAsRendered();
    void setEffectsOverrideCursor(Qt::CursorShape shape);
    void removeEffectsOverrideCursor();
    void setWindowSelectionCursor(const QByteArray &shape);
    void removeWindowSelectionCursor();

    void updatePointerConstraints();

    void setEnableConstraints(bool set);

    bool isConstrained() const {
        return m_confined || m_locked;
    }

    bool focusUpdatesBlocked() override;

    /**
     * @internal
     */
    void enableToggleMotion() {
        m_needToggleMotion = true;
    }
    /**
     * @internal
     */
    void disableToggleMotion() {
        m_needToggleMotion = false;
    }
    /**
     * @internal
     */
    void toggleMotion(const QPointF &pos, uint32_t time, LibInput::Device *device = nullptr);

    /**
     * @internal
     */
    void processMotion(const QPointF &pos, uint32_t time, LibInput::Device *device = nullptr);
    /**
     * @internal
     **/
    void processMotion(const QPointF &pos, const QSizeF &delta, const QSizeF &deltaNonAccelerated, uint32_t time, quint64 timeUsec, LibInput::Device *device);
    /**
     * @internal
     */
    void processButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time, LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time, LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureUpdate(const QSizeF &delta, quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureEnd(quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processSwipeGestureCancelled(quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureBegin(int fingerCount, quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureEnd(quint32 time, KWin::LibInput::Device *device = nullptr);
    /**
     * @internal
     */
    void processPinchGestureCancelled(quint32 time, KWin::LibInput::Device *device = nullptr);

private:
    void cleanupInternalWindow(QWindow *old, QWindow *now) override;
    void cleanupDecoration(Decoration::DecoratedClientImpl *old, Decoration::DecoratedClientImpl *now) override;

    void focusUpdate(Toplevel *focusOld, Toplevel *focusNow) override;

    QPointF position() const override;

    void updateOnStartMoveResize();
    void updateToReset();
    void updateButton(uint32_t button, InputRedirection::PointerButtonState state);
    void warpXcbOnSurfaceLeft(KWayland::Server::SurfaceInterface *surface);
    QPointF applyPointerConfinement(const QPointF &pos) const;
    void disconnectConfinedPointerRegionConnection();
    void disconnectLockedPointerAboutToBeUnboundConnection();
    void disconnectPointerConstraintsConnection();
    void breakPointerConstraints(KWayland::Server::SurfaceInterface *surface);
    CursorImage *m_cursor;
    bool m_supportsWarping;
    QPointF m_pos;
    QHash<uint32_t, InputRedirection::PointerButtonState> m_buttons;
    Qt::MouseButtons m_qtButtons;
    QMetaObject::Connection m_focusGeometryConnection;
    QMetaObject::Connection m_internalWindowConnection;
    QMetaObject::Connection m_constraintsConnection;
    QMetaObject::Connection m_constraintsActivatedConnection;
    QMetaObject::Connection m_confinedPointerRegionConnection;
    QMetaObject::Connection m_lockedPointerAboutToBeUnboundConnection;
    QMetaObject::Connection m_decorationGeometryConnection;
    bool m_confined = false;
    bool m_locked = false;
    bool m_enableConstraints = true;

    struct Motion {
       QSizeF delta;
       QSizeF deltaNonAccelerated;
    };
    Motion m_reservedMotion;
    bool m_needToggleMotion = false;
};

class CursorImage : public QObject
{
    Q_OBJECT
public:
    explicit CursorImage(PointerInputRedirection *parent = nullptr);
    virtual ~CursorImage();

    void setEffectsOverrideCursor(Qt::CursorShape shape);
    void removeEffectsOverrideCursor();
    void setWindowSelectionCursor(const QByteArray &shape);
    void removeWindowSelectionCursor();
    void setCursorShape(Qt::CursorShape);
    QImage image() const;
    QPoint hotSpot() const;
    void markAsRendered();

    enum class CursorSource {
        LockScreen,
        EffectsOverride,
        MoveResize,
        PointerSurface,
        Decoration,
        DragAndDrop,
        Fallback,
        WindowSelector
    };
    Q_ENUM(CursorSource);

Q_SIGNALS:
    void changed();

private:
    void reevaluteSource();
    void update();
    void updateServerCursor();
    void updateDecoration();
    void updateDecorationCursor();
    void updateMoveResize();
    void updateDrag();
    void updateDragCursor();
    void loadTheme();
    struct Image {
        QImage image;
        QPoint hotSpot;
    };
    void loadThemeCursor(CursorShape shape, Image *image);
    void loadThemeCursor(const QByteArray &shape, Image *image);
    template <typename T>
    void loadThemeCursor(const T &shape, QHash<T, Image> &cursors, Image *image);

    void setSource(CursorSource source);

    PointerInputRedirection *m_pointer;
    CursorSource m_currentSource = CursorSource::Fallback;
    WaylandCursorTheme *m_cursorTheme = nullptr;
    struct {
        QMetaObject::Connection connection;
        QImage image;
        QPoint hotSpot;
    } m_serverCursor;

    Image m_effectsCursor;
    Image m_decorationCursor;
    QMetaObject::Connection m_decorationConnection;
    Image m_fallbackCursor;
    Image m_moveResizeCursor;
    Image m_windowSelectionCursor;
    QHash<CursorShape, Image> m_cursors;
    QHash<QByteArray, Image> m_cursorsByName;
    QElapsedTimer m_surfaceRenderedTimer;
    struct {
        Image cursor;
        QMetaObject::Connection connection;
    } m_drag;
};

}

#endif
