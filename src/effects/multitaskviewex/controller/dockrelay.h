/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointF>

namespace KWin
{

/**
 * @brief Remembers the dock click swallowed while the view was closing.
 *
 * Clicking the dock requests the view to exit; the click is relayed to the dock
 * once the exit animation finished, so the click is not lost.
 */
class DockRelay
{
public:
    /**
     * @brief Stores a click that must be relayed after the view exits.
     * @param pos Global click position.
     * @param button Mouse button that was pressed.
     */
    void remember(const QPointF &pos, Qt::MouseButton button)
    {
        m_pos = pos;
        m_button = button;
    }

    /**
     * @brief Tests whether a click is pending.
     * @return True when a click is waiting to be relayed.
     */
    bool pending() const
    {
        return m_button != Qt::NoButton;
    }

    /**
     * @brief Returns the pending click position.
     * @return The stored global position.
     */
    QPointF pos() const
    {
        return m_pos;
    }

    /**
     * @brief Returns the pending click button.
     * @return The stored mouse button.
     */
    Qt::MouseButton button() const
    {
        return m_button;
    }

    /**
     * @brief Forgets the pending click.
     */
    void clear()
    {
        m_pos = QPointF();
        m_button = Qt::NoButton;
    }

private:
    QPointF m_pos;
    Qt::MouseButton m_button = Qt::NoButton;
};

}
