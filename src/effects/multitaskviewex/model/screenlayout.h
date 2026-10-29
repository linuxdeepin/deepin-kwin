/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QVector>

namespace KWin
{

struct ScreenLayout
{
    QRectF fullRect;
    QRectF clientRect;
    QRectF gridArea;
    QSizeF barSize;
    qreal offX = 0;
    qreal offY = 0;
};

/**
 * @brief Builds the screen layout from the full and client rectangles.
 * @param fullRect Full screen rectangle.
 * @param clientRect Usable client rectangle.
 * @param managerHeight Height reserved for the workspace bar.
 * @return The resolved screen layout.
 */
inline ScreenLayout screenLayoutFor(const QRectF &fullRect, const QRectF &clientRect,
                                    qreal managerHeight)
{
    ScreenLayout out;
    out.fullRect = fullRect;
    out.clientRect = clientRect;
    out.offX = clientRect.x();
    out.offY = clientRect.y();
    out.barSize = QSizeF(clientRect.width(), clientRect.height());
    const qreal gridH = clientRect.height() - managerHeight;
    out.gridArea = QRectF(clientRect.x(), clientRect.y() + managerHeight,
                          clientRect.width(), gridH > 0 ? gridH : 0);
    return out;
}

/**
 * @brief Translates an absolute screen layout into screen-local coordinates.
 * @param absolute Absolute layout.
 * @param screenFullRect Full rectangle of the target screen.
 * @return The layout translated so the screen's top-left becomes the origin.
 */
inline ScreenLayout screenLocalLayout(const ScreenLayout &absolute, const QRectF &screenFullRect)
{
    ScreenLayout out = absolute;
    const QPointF o = screenFullRect.topLeft();
    out.fullRect = absolute.fullRect.translated(-o);
    out.clientRect = absolute.clientRect.translated(-o);
    out.gridArea = absolute.gridArea.translated(-o);
    out.offX = out.clientRect.x();
    out.offY = out.clientRect.y();
    return out;
}

/**
 * @brief Returns the indices of screens intersecting a render target rectangle.
 * @param screenFullRects Full rectangle of each screen.
 * @param renderTargetRect Rectangle being rendered.
 * @return Indices of intersecting screens, in ascending order.
 */
inline QVector<int> paintTargets(const QVector<QRectF> &screenFullRects,
                                 const QRectF &renderTargetRect)
{
    QVector<int> out;
    for (int i = 0; i < screenFullRects.size(); ++i) {
        if (screenFullRects.at(i).intersects(renderTargetRect)) {
            out.append(i);
        }
    }
    return out;
}
}
