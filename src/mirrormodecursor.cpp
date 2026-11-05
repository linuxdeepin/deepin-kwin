/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mirrormodecursor.h"

#include "composite.h"
#include "core/output.h"
#include "cursor.h"
#include "main.h"
#include "scene/workspacescene.h"
#include "utils/common.h"
#include "workspace.h"

#include <xcb/xcb.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace KWin
{

// A pointer barrier stops physical motion at the edge of the primary output, but not
// XWarpPointer, so the position is polled as well. The same timer picks up the cursor
// image, which the X server does not hand out in any other way.
static const int kPollInterval = 100;

MirrorModeCursor::MirrorModeCursor(Workspace *workspace)
    : QObject(workspace)
    , m_workspace(workspace)
{
    m_pollTimer.setInterval(kPollInterval);
    connect(&m_pollTimer, &QTimer::timeout, this, &MirrorModeCursor::poll);

    connect(m_workspace, &Workspace::mirrorModeChanged, this, &MirrorModeCursor::applyMode);
    connect(m_workspace, &Workspace::geometryChanged, this, [this]() {
        if (!m_workspace->isMirrorMode()) {
            return;
        }
        // The primary output or its geometry changed: the barriers follow the edges and
        // the mirrored cursor moves with it.
        createBarriers();
        updateMirrorRegion();
    });
    connect(Cursors::self(), &Cursors::positionChanged, this, [this]() {
        updateMirrorRegion();
    });
    connect(Cursors::self(), &Cursors::hiddenChanged, this, [this]() {
        updateMirrorRegion();
    });

    applyMode(m_workspace->isMirrorMode());
}

MirrorModeCursor::~MirrorModeCursor()
{
    destroyBarriers();
}

QImage MirrorModeCursor::image() const
{
    if (!m_workspace->isMirrorMode() || Cursors::self()->isCursorHidden()) {
        return QImage();
    }
    return m_image;
}

QRectF MirrorModeCursor::mirroredCursorRect(const QRectF &mirrorGeometry) const
{
    const Output *primary = m_workspace->mirrorModePrimaryOutput();
    Cursor *cursor = Cursors::self()->currentCursor();
    if (!primary || !cursor || m_image.isNull() || Cursors::self()->isCursorHidden()) {
        return QRectF();
    }
    // cursor->pos() is the hotspot position, so offsetting by the hotspot maps the image
    // itself and keeps the mirrored hotspot aligned with the primary pointer.
    const QRectF cursorRect(QPointF(cursor->pos()) - QPointF(m_hotspot), QSizeF(m_image.size()));
    return Workspace::mirrorModeMirrorRect(cursorRect, primary->fractionalGeometry(), mirrorGeometry);
}

void MirrorModeCursor::applyMode(bool enabled)
{
    if (enabled) {
        createBarriers();
        updateMirrorRegion(queryCursor());
        m_pollTimer.start();
    } else {
        m_pollTimer.stop();
        destroyBarriers();
        const QRegion oldRegion = m_mirrorRegion;
        m_mirrorRegion = QRegion();
        m_image = QImage();
        m_hotspot = QPoint();
        if (!oldRegion.isEmpty() && Compositor::compositing() && Compositor::self()->scene()) {
            Compositor::self()->scene()->addRepaint(oldRegion);
        }
    }
}

void MirrorModeCursor::poll()
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    const Output *primary = m_workspace->mirrorModePrimaryOutput();
    if (!connection || !primary) {
        return;
    }
    const QRectF box = primary->fractionalGeometry();
    if (box.isEmpty()) {
        return;
    }

    UniqueCPtr<xcb_query_pointer_reply_t> reply(xcb_query_pointer_reply(connection, xcb_query_pointer(connection, kwinApp()->x11RootWindow()), nullptr));
    if (reply) {
        const QPointF pos(reply->root_x, reply->root_y);
        if (!box.contains(pos)) {
            // Warps are not constrained by the barriers, so put the pointer back and let
            // the normal input path update the cached position.
            const qreal x = std::clamp(pos.x(), box.left(), box.right() - 1.0);
            const qreal y = std::clamp(pos.y(), box.top(), box.bottom() - 1.0);
            xcb_warp_pointer(connection, XCB_NONE, kwinApp()->x11RootWindow(), 0, 0, 0, 0, qRound(x), qRound(y));
            xcb_flush(connection);
        }
    }

    updateMirrorRegion(queryCursor());
}

void MirrorModeCursor::createBarriers()
{
    destroyBarriers();

    xcb_connection_t *connection = kwinApp()->x11Connection();
    const Output *primary = m_workspace->mirrorModePrimaryOutput();
    if (!connection || !primary) {
        return;
    }
    if (!supportsPointerBarriers()) {
        return;
    }

    const QRectF box = primary->fractionalGeometry();
    const int left = qCeil(box.left());
    const int right = qCeil(box.right());
    const int top = qCeil(box.top());
    const int bottom = qCeil(box.bottom());
    const xcb_window_t root = kwinApp()->x11RootWindow();

    // The directions field names the directions that may still be crossed, so each
    // border only allows motion towards the inside of the primary output.
    static const uint32_t directions[4] = {
        XCB_XFIXES_BARRIER_DIRECTIONS_POSITIVE_X,
        XCB_XFIXES_BARRIER_DIRECTIONS_NEGATIVE_X,
        XCB_XFIXES_BARRIER_DIRECTIONS_POSITIVE_Y,
        XCB_XFIXES_BARRIER_DIRECTIONS_NEGATIVE_Y,
    };
    const int coordinates[4][4] = {
        {left, top, left, bottom},
        {right, top, right, bottom},
        {left, top, right, top},
        {left, bottom, right, bottom},
    };

    m_barriers.reserve(4);
    for (int i = 0; i < 4; i++) {
        const xcb_xfixes_barrier_t id = xcb_generate_id(connection);
        const xcb_void_cookie_t cookie = xcb_xfixes_create_pointer_barrier(connection, id, root,
                                                                          coordinates[i][0], coordinates[i][1],
                                                                          coordinates[i][2], coordinates[i][3],
                                                                          directions[i], 0, nullptr);
        if (xcb_generic_error_t *error = xcb_request_check(connection, cookie)) {
            qCWarning(KWIN_CORE) << "Mirror mode: failed to create pointer barrier, error" << error->error_code;
            free(error);
            continue;
        }
        m_barriers.push_back(id);
    }
}

bool MirrorModeCursor::supportsPointerBarriers()
{
    if (m_pointerBarriersAvailable >= 0) {
        return m_pointerBarriersAvailable == 1;
    }
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        return false;
    }
    UniqueCPtr<xcb_xfixes_query_version_reply_t> version(xcb_xfixes_query_version_reply(connection, xcb_xfixes_query_version(connection, 5, 0), nullptr));
    m_pointerBarriersAvailable = (version && version->major_version >= 5) ? 1 : 0;
    if (m_pointerBarriersAvailable == 0) {
        qCWarning(KWIN_CORE) << "Mirror mode: XFixes 5.0 pointer barriers are unavailable, falling back to polling";
    }
    return m_pointerBarriersAvailable == 1;
}

void MirrorModeCursor::destroyBarriers()
{
    if (m_barriers.empty()) {
        return;
    }
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (connection) {
        for (const xcb_xfixes_barrier_t id : std::as_const(m_barriers)) {
            xcb_xfixes_delete_pointer_barrier(connection, id);
        }
    }
    m_barriers.clear();
}

bool MirrorModeCursor::queryCursor()
{
    xcb_connection_t *connection = kwinApp()->x11Connection();
    if (!connection) {
        return false;
    }
    UniqueCPtr<xcb_xfixes_get_cursor_image_reply_t> reply(xcb_xfixes_get_cursor_image_reply(connection, xcb_xfixes_get_cursor_image(connection), nullptr));
    if (!reply || reply->width == 0 || reply->height == 0) {
        return false;
    }
    const uint32_t *data = xcb_xfixes_get_cursor_image_cursor_image(reply.get());
    if (!data) {
        return false;
    }

    QImage image(reply->width, reply->height, QImage::Format_ARGB32_Premultiplied);
    memcpy(image.bits(), data, size_t(reply->width) * size_t(reply->height) * sizeof(uint32_t));
    const QPoint hotspot(reply->xhot, reply->yhot);
    // The mirrored cursor has to be repainted when the image changes even if it keeps its
    // position: the mirrored rect only depends on the position, the hotspot and the size.
    const bool changed = image != m_image || hotspot != m_hotspot;
    m_image = image;
    m_hotspot = hotspot;
    return changed;
}

void MirrorModeCursor::updateMirrorRegion(bool force)
{
    QRegion region;
    const Output *primary = m_workspace->mirrorModePrimaryOutput();
    if (m_workspace->isMirrorMode() && primary && !m_image.isNull()) {
        for (const Output *mirror : m_workspace->mirrorModeMirrors()) {
            region += mirroredCursorRect(mirror->fractionalGeometry()).toAlignedRect();
        }
    }
    if (region == m_mirrorRegion && !force) {
        return;
    }
    if (Compositor::compositing() && Compositor::self()->scene()) {
        // Repaint both the old and the new mirror cursor area to avoid trails.
        Compositor::self()->scene()->addRepaint(region | m_mirrorRegion);
    }
    m_mirrorRegion = region;
}

} // namespace KWin
