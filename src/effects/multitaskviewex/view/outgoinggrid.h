/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>
#include <QVector>

#include "animation/visualnode.h"
#include "outgoingwindow.h"

namespace KWin
{

struct OutgoingGrid
{
    QVector<OutgoingWindow> windows;
    QVector<VisualNode> cells;
    QVector<qreal> cellRadii;
    qreal slideDx = 0;

    /**
     * @brief Tests whether the grid holds no windows or cells.
     * @return True when the grid is empty.
     */
    bool empty() const
    {
        return windows.isEmpty() && cells.isEmpty();
    }

    /**
     * @brief Tests whether any node in the grid is animating.
     * @return True while at least one node is animating.
     */
    bool animating() const;

    /**
     * @brief Advances all nodes in the grid by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advance(int dtMs);
};

}
