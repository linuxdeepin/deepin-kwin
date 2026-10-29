/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "outgoinggrid.h"
#include "outgoingwindow.h"
#include "outgoingwindowinput.h"

#include <QRectF>
#include <QVector>

namespace KWin
{

class EffectWindow;

class OutgoingLayer
{
public:
    /**
     * @brief Captures the current grid into an outgoing snapshot that animates away.
     * @param slideOffsetX Horizontal slide offset to animate from.
     * @param wins Windows with their source, current geometry and opacity.
     * @param cellBase Base cell rectangles.
     * @param rowHeight Height of a grid row.
     * @param osScale OS scale factor.
     * @param durationMs Transition duration in milliseconds.
     */
    void snapshot(qreal slideOffsetX, const QVector<OutgoingWindowInput> &wins,
                  const QVector<QRectF> &cellBase, qreal rowHeight, qreal osScale, int durationMs);

    /**
     * @brief Advances all outgoing grids by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advance(int dtMs);

    /**
     * @brief Tests whether any outgoing grid is animating.
     * @return True while at least one grid is animating.
     */
    bool animating() const;

    /**
     * @brief Returns the outgoing grids.
     * @return The outgoing grids.
     */
    const QVector<OutgoingGrid> &grids() const
    {
        return m_grids;
    }

    /**
     * @brief Takes the windows whose outgoing animation has finished.
     * @return Windows to release, and clears the finished list.
     */
    QVector<EffectWindow *> takeFinished();

    /**
     * @brief Removes a window from every outgoing grid.
     * @param w Window to remove.
     */
    void removeWindow(EffectWindow *w);

    /**
     * @brief Clears all outgoing grids and finished windows.
     */
    void clear()
    {
        m_grids.clear();
        m_finished.clear();
    }

private:
    QVector<OutgoingGrid> m_grids;
    QVector<EffectWindow *> m_finished;
};

}
