/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>
#include <QSize>
#include <QVector>

namespace KWin
{

/**
 * @brief Scales a window down so it fits inside a grid row.
 * @param natural Natural window size.
 * @param scaleHeight Target row height.
 * @param maxWidth Maximum allowed width.
 * @return The fitted size, preserving the aspect ratio when scaled.
 */
inline QSizeF fitWindowIntoRow(const QSize &natural, qreal scaleHeight, qreal maxWidth)
{
    qreal w = natural.width();
    qreal h = natural.height();
    if (h > scaleHeight && h > 0.0) {
        w = w * (scaleHeight / h);
        h = scaleHeight;
    }
    if (w > maxWidth && w > 0.0) {
        const qreal s = maxWidth / w;
        w = maxWidth;
        h = h * s;
    }
    return QSizeF(w, h);
}

/**
 * @brief Computes the scale that fits a cell inside a drag box.
 * @param cell Cell rectangle.
 * @param boxW Target box width.
 * @param boxH Target box height.
 * @return The uniform fit scale, or 0 when the cell has no area.
 */
inline qreal dragGhostFitScale(const QRectF &cell, qreal boxW, qreal boxH)
{
    if (cell.width() <= 0.0 || cell.height() <= 0.0) {
        return 0.0;
    }
    return qMin(boxW / cell.width(), boxH / cell.height());
}

/**
 * @brief Computes the ghost rectangle centered on the mouse cursor.
 * @param cell Source cell rectangle.
 * @param mouse Mouse position.
 * @param scale Scale to apply to the cell.
 * @return The ghost rectangle.
 */
inline QRectF dragGhostRect(const QRectF &cell, const QPointF &mouse, qreal scale)
{
    const qreal w = cell.width() * scale;
    const qreal h = cell.height() * scale;
    return QRectF(mouse.x() - w / 2, mouse.y() - h / 2, w, h);
}

/**
 * @brief Computes the ghost scale from the pointer's vertical position near the bar.
 * @param mouseY Current mouse y.
 * @param barBottomY Bottom edge of the bar.
 * @param shrinkZoneH Height of the zone over which the ghost shrinks.
 * @return The ghost scale in [0.5, 1.0].
 */
inline qreal dragGhostScale(qreal mouseY, qreal barBottomY, qreal shrinkZoneH = 140.0)
{
    if (mouseY <= barBottomY) {
        return 0.5;
    }
    if (mouseY >= barBottomY + shrinkZoneH) {
        return 1.0;
    }
    const qreal t = (barBottomY + shrinkZoneH - mouseY) / shrinkZoneH;
    return 1.0 - 0.5 * t;
}

/**
 * @brief Expands a cell vertically to cover the full row height.
 * @param cell Cell rectangle.
 * @param rowHeight Height of the row.
 * @return The cell expanded around its vertical center.
 */
inline QRectF coverFor(const QRectF &cell, qreal rowHeight)
{
    const qreal pad = qMax<qreal>(0.0, (rowHeight - cell.height()) / 2);
    return cell.adjusted(0, -pad, 0, pad);
}

/**
 * @brief Lays out window rectangles into rows inside a grid area.
 * @param naturals Natural size of each window.
 * @param area Grid area to fill.
 * @param screenH Screen height used for row-height heuristics.
 * @param spacingH Vertical spacing between rows.
 * @param spacingW Horizontal spacing between cells.
 * @param outScaleHeight Optional output receiving the resolved row height.
 * @return One rectangle per window, in input order.
 */
QVector<QRectF> layoutWindowGrid(const QVector<QSize> &naturals,
                                 const QRectF &area,
                                 qreal screenH,
                                 qreal spacingH,
                                 qreal spacingW,
                                 qreal *outScaleHeight = nullptr);

}
