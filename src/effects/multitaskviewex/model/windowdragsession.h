/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>

namespace KWin
{

class WindowDragSession
{
public:
    /**
     * @brief Starts a drag session anchored to a source cell.
     * @param cell Source cell rectangle.
     */
    void begin(const QRectF &cell)
    {
        m_cell = cell;
        m_active = true;
    }

    /**
     * @brief Tests whether a drag session is active.
     * @return True while a session is active.
     */
    bool active() const
    {
        return m_active;
    }

    /**
     * @brief Returns the source cell of the drag.
     * @return The source cell rectangle.
     */
    QRectF cell() const
    {
        return m_cell;
    }

    /**
     * @brief Ends the drag session and clears the stored cell.
     */
    void end()
    {
        m_active = false;
        m_cell = QRectF();
    }

private:
    QRectF m_cell;
    bool m_active = false;
};

}
