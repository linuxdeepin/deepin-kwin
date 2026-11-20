/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector2D>

namespace KWin
{

/**
 * @brief Placement of a window quad inside a canvas.
 */
struct WindowPaintPlacement
{
    QVector2D scale = QVector2D(1.0f, 1.0f); ///< Scale applied to the window's own geometry.
    QPointF translation; ///< Translation applied to the window's own geometry.
};

/**
 * @brief Composes the placement that lands a window on its destination inside a canvas.
 *
 * KWin positions a window quad at the window's own screen geometry and then applies the translation
 * of the paint data before projecting. A canvas, in contrast, is projected from its own origin, so
 * the placement has to move the screen geometry into canvas coordinates as well.
 *
 * @param from Window geometry in canvas coordinates.
 * @param to Destination rectangle in canvas coordinates.
 * @param canvasOrigin Canvas origin in screen coordinates.
 * @return The placement a painter hands to KWin with the canvas projection.
 */
inline WindowPaintPlacement windowPaintPlacement(const QRectF &from, const QRectF &to,
                                                 const QPointF &canvasOrigin)
{
    WindowPaintPlacement placement;
    if (from.width() <= 0.0 || from.height() <= 0.0) {
        return placement;
    }
    placement.scale = QVector2D(to.width() / from.width(), to.height() / from.height());
    placement.translation = to.topLeft() - from.topLeft() - canvasOrigin;
    return placement;
}

} // namespace KWin
