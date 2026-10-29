/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "bardragmodel.h"
#include "barpredicates.h"

#include <QRectF>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace KWin
{

/**
 * @brief Computes the x position of the add-preview item.
 * @param screenWidth Screen width.
 * @param itemWidth Width of the preview item.
 * @return The right-aligned preview x position.
 */
inline qreal previewXFor(qreal screenWidth, qreal itemWidth)
{
    const qreal margin = screenWidth * 20.0 / 1920.0;
    return qMax<qreal>(0.0, screenWidth - margin - itemWidth);
}

struct BarDragTarget
{
    int id = -1;
    qreal x = 0;
    qreal y = 0;
    qreal cellX = 0;
    qreal w = 0;
    qreal placeholderX = 0;
    qreal placeholderW = 0;
    qreal opacity = 1.0;
};

/**
 * @brief Lays out the workspace bar items, accounting for an active drag.
 * @param drag Current workspace bar drag state.
 * @param order Item ids in display order.
 * @param previewX X position of the add-preview item, or a negative value when absent.
 * @param previewActive Whether the add-preview is visible.
 * @param scrollOffset Horizontal scroll offset to apply.
 * @return One layout target per item, in order, plus the preview when present.
 */
inline QVector<BarDragTarget> layoutBarDragTargets(const WorkspaceBarDragModel &drag,
                                                   const QVector<int> &order,
                                                   qreal previewX = -1.0,
                                                   bool previewActive = true,
                                                   qreal scrollOffset = 0.0)
{
    const auto &cfg = drag.config();
    const int n = order.size();
    const qreal slotW = cfg.workspaceWidth + cfg.spacing;
    const qreal w = cfg.workspaceWidth;
    const qreal half = w / 2.0;
    const qreal sp = cfg.spacing;
    const qreal center = cfg.listX + (n * w + (n - 1) * sp) / 2.0;
    const bool active = drag.isActive();
    const bool vertical = active && drag.isVertical();
    const bool makeWay = active && !vertical;
    qreal total = (n - 1) * sp;
    for (int row = 0; row < n; ++row) {
        total += (makeWay && order[row] == drag.draggedId()) ? half : w;
    }
    qreal cursor = center - total / 2.0;
    qreal stillCursor = center - ((n - 1) * sp + n * w) / 2.0;
    QVector<BarDragTarget> out;
    out.reserve(n);
    for (int row = 0; row < n; ++row) {
        const int id = order[row];
        const bool dragged = active && id == drag.draggedId();
        BarDragTarget t;
        t.id = id;
        t.w = w;
        t.cellX = stillCursor;
        stillCursor += w + sp;
        if (dragged && vertical) {
            t.x = cursor;
            t.y = drag.clampedVerticalOffset();
            t.opacity = t.y < 0.0 ? 0.5 : 1.0;
            t.placeholderW = 0;
            cursor += w + sp;
        } else if (dragged) {
            t.x = drag.pointerX() - w / 2.0;
            t.placeholderX = cursor;
            t.placeholderW = half;
            cursor += half + sp;
        } else {
            t.x = cursor;
            t.placeholderW = 0;
            cursor += w + sp;
        }
        out.append(t);
    }
    if (previewX >= 0.0 && !active) {
        BarDragTarget t;
        t.id = kAddPreviewId;
        t.x = previewX;
        t.w = w;
        t.cellX = previewX;
        t.placeholderW = 0;
        t.opacity = previewActive ? 1.0 : 0.0;
        out.append(t);
    }
    if (!qFuzzyIsNull(scrollOffset)) {
        for (BarDragTarget &t : out) {
            t.x -= scrollOffset;
            t.cellX -= scrollOffset;
            t.placeholderX -= scrollOffset;
        }
    }
    return out;
}

/**
 * @brief Computes the maximum horizontal scroll offset of the bar.
 * @param contentWidth Total content width.
 * @param viewportWidth Visible viewport width.
 * @return The maximum scroll offset, never negative.
 */
inline qreal barMaxScroll(qreal contentWidth, qreal viewportWidth)
{
    return qMax<qreal>(0.0, contentWidth - viewportWidth);
}

/**
 * @brief Tests whether the delete-drag tip should be shown.
 * @param dragDelete Whether the drag is currently over the delete zone.
 * @param desktopCount Current number of desktops.
 * @return True when deleting is possible and more than one desktop exists.
 */
inline bool shouldShowDragTip(bool dragDelete, int desktopCount)
{
    return dragDelete && desktopCount > 1;
}

struct DragTipLayout
{
    QRectF rect;
    bool showText = true;
};

/**
 * @brief Lays out the drag tip inside an item rectangle.
 * @param itemRect Rectangle of the item the tip belongs to.
 * @param iconSize Icon size.
 * @param textWidth Width needed for the text.
 * @param textHeight Height needed for the text.
 * @param spacing Gap between icon and text.
 * @param minHeight Minimum tip height.
 * @return The tip rectangle and whether the text fits.
 */
inline DragTipLayout dragTipLayout(const QRectF &itemRect, qreal iconSize, qreal textWidth, qreal textHeight,
                                   qreal spacing = 13.0, qreal minHeight = 28.0)
{
    const bool showText = itemRect.width() >= iconSize + textWidth + spacing;
    const qreal w = showText ? iconSize + textWidth + spacing : iconSize;
    const qreal h = qMax(qMax(iconSize, showText ? textHeight : 0.0), minHeight);
    DragTipLayout out;
    out.showText = showText;
    out.rect = QRectF(itemRect.x() + (itemRect.width() - w) / 2.0,
                      itemRect.y() + (itemRect.height() - h) / 2.0, w, h);
    return out;
}

}
