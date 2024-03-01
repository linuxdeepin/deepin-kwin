/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "xwaylandwindow.h"
#include "wayland/surface_interface.h"

using namespace KWaylandServer;

namespace KWin
{

XwaylandWindow::XwaylandWindow()
{
    // The wayland surface is associated with the Xwayland window asynchronously.
    connect(this, &Window::surfaceChanged, this, &XwaylandWindow::associate);
}

void XwaylandWindow::associate()
{
    if (surface()->isMapped()) {
        initialize();
    } else {
        // Queued connection because we want to mark the window ready for painting after
        // the associated surface item has processed the new surface state.
        connect(surface(), &SurfaceInterface::mapped, this, &XwaylandWindow::initialize, Qt::QueuedConnection);
    }
}

void XwaylandWindow::initialize()
{
    if (!readyForPainting()) { // avoid "setReadyForPainting()" function calling overhead
        if (syncRequest().counter == XCB_NONE) { // cannot detect complete redraw, consider done now
            setReadyForPainting();
        }
    }
}

void XwaylandWindow::recordShape(xcb_window_t id, xcb_shape_kind_t kind)
{
    if ((kind == XCB_SHAPE_SK_BOUNDING || kind == 255) && shape()) {
        auto cookie = xcb_shape_get_rectangles_unchecked(kwinApp()->x11Connection(), id, XCB_SHAPE_SK_BOUNDING);
        UniqueCPtr<xcb_shape_get_rectangles_reply_t> reply(xcb_shape_get_rectangles_reply(kwinApp()->x11Connection(), cookie, nullptr));
        if (reply) {
            m_shapeBoundingRegion.clear();
            const xcb_rectangle_t *rects = xcb_shape_get_rectangles_rectangles(reply.get());
            const int rectCount = xcb_shape_get_rectangles_rectangles_length(reply.get());
            for (int i = 0; i < rectCount; ++i) {
                QRectF region = Xcb::fromXNative(QRect(rects[i].x, rects[i].y, rects[i].width, rects[i].height)).toAlignedRect();
                m_shapeBoundingRegion += region;
            }
        }
    }

    if (kind == XCB_SHAPE_SK_INPUT || kind == 255) {
        auto cookie = xcb_shape_get_rectangles_unchecked(kwinApp()->x11Connection(), id, XCB_SHAPE_SK_INPUT);
        UniqueCPtr<xcb_shape_get_rectangles_reply_t> reply(xcb_shape_get_rectangles_reply(kwinApp()->x11Connection(), cookie, nullptr));
        if (reply) {
            m_shapeInputRegion.clear();
            const xcb_rectangle_t *rects = xcb_shape_get_rectangles_rectangles(reply.get());
            const int rectCount = xcb_shape_get_rectangles_rectangles_length(reply.get());
            for (int i = 0; i < rectCount; ++i) {
                QRectF region = Xcb::fromXNative(QRect(rects[i].x, rects[i].y, rects[i].width, rects[i].height)).toAlignedRect();
                m_shapeInputRegion += region;
            }
        }
    }
}

bool XwaylandWindow::hitTest(const QPointF &point) const
{
    if (isDecorated()) {
        if (decorationInputRegion().contains(flooredPoint(mapToFrame(point)))) {
            return true;
        }
    }
    const QPointF relativePoint = point - clientGeometry().topLeft();

    bool isInBounding = false, isInInput = false;
    if (shape()) {
        for (const auto& rect: m_shapeBoundingRegion) {
            if (rect.contains(flooredPoint(relativePoint))) {
                isInBounding = true;
                break;
            }
        }
    }
    for (const auto& rect: m_shapeInputRegion) {
        if (rect.contains(flooredPoint(relativePoint))) {
            isInInput = true;
            break;
        }
    }
    if (shape()) {
        return isInBounding && isInInput;
    } else {
        return isInInput;
    }
}

bool XwaylandWindow::wantsSyncCounter() const
{
    // When the frame window is resized, the attached buffer will be destroyed by
    // Xwayland, causing unexpected invalid previous and current window pixmaps.
    // With the addition of multiple window buffers in Xwayland 1.21, X11 clients
    // are no longer able to destroy the buffer after it's been committed and not
    // released by the compositor yet.
    static const quint32 xwaylandVersion = xcb_get_setup(kwinApp()->x11Connection())->release_number;
    return xwaylandVersion >= 12100000;
}

} // namespace KWin
