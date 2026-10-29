/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace KWin
{

struct CoverVisual
{
    int r = 0xA0;
    int g = 0xA0;
    int b = 0xA0;
    qreal opacity = 0.5;
};

/**
 * @brief Returns the default cover visual used for placeholder cells.
 * @return The default cover color and opacity.
 */
inline CoverVisual coverVisual()
{
    return CoverVisual{};
}

/**
 * @brief Returns the corner radius for a window, keyed by grid row.
 * @param row Grid row the window belongs to.
 * @param osScale System scale factor.
 * @return The scaled corner radius; larger for the first two rows.
 */
inline qreal windowCornerRadius(int row, qreal osScale = 1.0)
{
    return (row < 2 ? 18.0 : 10.0) * osScale;
}

/**
 * @brief Finds the grid row whose top edge matches a y coordinate.
 * @param y Y coordinate to look up.
 * @param uniqueYs Distinct row top edges.
 * @return The matching row index, or 0 when none matches.
 */
inline int gridRowOf(qreal y, const QVector<qreal> &uniqueYs)
{
    for (int i = 0; i < uniqueYs.size(); ++i) {
        if (qAbs(uniqueYs[i] - y) < 1.0) {
            return i;
        }
    }
    return 0;
}

/**
 * @brief Collects the distinct row top edges of a set of cells, sorted ascending.
 * @param cells Cell rectangles.
 * @return The distinct row top edges.
 */
inline QVector<qreal> uniqueRowYs(const QVector<QRectF> &cells)
{
    QVector<qreal> rowYs;
    for (const QRectF &c : cells) {
        bool seen = false;
        for (int i = 0; i < rowYs.size(); ++i) {
            if (qAbs(rowYs[i] - c.y()) < 1.0) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            rowYs.append(c.y());
        }
    }
    std::sort(rowYs.begin(), rowYs.end());
    return rowYs;
}

}
