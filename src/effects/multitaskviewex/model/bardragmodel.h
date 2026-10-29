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

class WorkspaceBarDragModel
{
public:
    struct Config
    {
        qreal workspaceWidth = 0;
        qreal workspaceHeight = 0;
        qreal spacing = 0;
        qreal listX = 0;
        qreal directionLockPx = 4.0;
        qreal hysteresisRatio = 0.25;
    };

    /**
     * @brief Sets the bar geometry used to resolve slots.
     * @param c Bar drag configuration.
     */
    void setConfig(const Config &c)
    {
        m_cfg = c;
    }

    /**
     * @brief Returns the current bar drag configuration.
     * @return The configuration previously set.
     */
    const Config &config() const
    {
        return m_cfg;
    }

    /**
     * @brief Tests whether a bar drag is in progress.
     * @return True while a drag session is active.
     */
    bool isActive() const
    {
        return m_active;
    }

    /**
     * @brief Returns the current display order of the workspaces.
     * @return Workspace ids in display order.
     */
    QVector<int> order() const
    {
        return m_order;
    }

    /**
     * @brief Returns the number of workspaces in the bar.
     * @return Workspace count.
     */
    int count() const
    {
        return m_order.size();
    }

    /**
     * @brief Returns the id of the dragged workspace.
     * @return The dragged workspace id, or -1 when no drag is active.
     */
    int draggedId() const
    {
        return m_active ? m_draggedId : -1;
    }

    /**
     * @brief Returns the slot index currently occupied by the dragged item.
     * @return The dragged slot index.
     */
    int draggedSlot() const
    {
        return m_draggedSlot;
    }

    /**
     * @brief Returns the effective pointer x position of the drag.
     * @return Pointer x in bar coordinates.
     */
    qreal pointerX() const
    {
        return m_pointerX;
    }

    /**
     * @brief Tests whether the drag is currently over the delete zone.
     * @return True when releasing would delete the workspace.
     */
    bool dragDelete() const
    {
        return m_dragDelete;
    }

    /**
     * @brief Returns the locked drag direction.
     * @return 0 while undecided, 1 for vertical, 2 for horizontal.
     */
    int direction() const
    {
        return m_direction;
    }

    /**
     * @brief Returns the raw vertical displacement of the pointer.
     * @return Pointer y minus press y.
     */
    qreal verticalOffset() const
    {
        return m_pointerY - m_pressY;
    }

    /**
     * @brief Returns the vertical displacement clamped to the allowed range.
     * @return The displacement, clamped to [-0.9 * workspaceHeight, 0].
     */
    qreal clampedVerticalOffset() const
    {
        return qBound(-0.9 * m_cfg.workspaceHeight, m_pointerY - m_pressY, 0.0);
    }

    /**
     * @brief Tests whether the drag direction is vertical.
     * @return True when the drag is locked to the vertical axis.
     */
    bool isVertical() const
    {
        return m_direction == 1;
    }

    /**
     * @brief Starts a drag session on the given workspace.
     * @param order Workspace ids in display order.
     * @param draggedId Id of the dragged workspace.
     * @param slot Slot index the drag started from.
     * @param pressX Pointer x at press time.
     * @param pressY Pointer y at press time.
     */
    void begin(const QVector<int> &order, int draggedId, int slot, qreal pressX, qreal pressY);

    /**
     * @brief Updates the drag with a new pointer position, locking direction and reordering slots.
     * @param x Pointer x.
     * @param y Pointer y.
     */
    void movePointer(qreal x, qreal y);

    /**
     * @brief Ends the drag session, keeping the current order.
     */
    void end()
    {
        m_active = false;
        m_dragDelete = false;
    }

private:
    qreal slotWidth() const
    {
        return m_cfg.workspaceWidth + m_cfg.spacing;
    }

    qreal slotCenter(int slot) const
    {
        return m_cfg.listX + slot * slotWidth() + m_cfg.workspaceWidth / 2.0;
    }

    void swapSlots(int a, int b);

    Config m_cfg;
    bool m_active = false;
    QVector<int> m_order;
    int m_draggedId = -1;
    int m_draggedSlot = -1;
    qreal m_pressX = 0;
    qreal m_pressY = 0;
    qreal m_pointerX = 0;
    qreal m_pointerY = 0;
    int m_direction = 0;
    bool m_dragDelete = false;
};

inline void WorkspaceBarDragModel::begin(const QVector<int> &order, int draggedId, int slot,
                                         qreal pressX, qreal pressY)
{
    m_active = true;
    m_order = order;
    m_draggedId = draggedId;
    m_draggedSlot = slot;
    m_pressX = pressX;
    m_pressY = pressY;
    m_pointerX = pressX;
    m_pointerY = pressY;
    m_direction = 0;
    m_dragDelete = false;
}

inline void WorkspaceBarDragModel::movePointer(qreal x, qreal y)
{
    if (!m_active) {
        return;
    }
    if (m_direction == 0) {
        const qreal dx = std::abs(x - m_pressX);
        const qreal dy = std::abs(y - m_pressY);
        if (dx > m_cfg.directionLockPx || dy > m_cfg.directionLockPx) {
            m_direction = (dy > dx) ? 1 : 2;
        }
    }
    const qreal effX = (m_direction == 1) ? m_pressX : x;
    const qreal effY = (m_direction == 2) ? m_pressY : y;
    m_pointerX = effX;
    m_pointerY = effY;
    m_dragDelete = (m_pressY - effY) > m_cfg.workspaceHeight / 2.0;
    if (m_direction == 1) {
        return;
    }
    const qreal hyst = slotWidth() * m_cfg.hysteresisRatio;
    if (m_draggedSlot + 1 < m_order.size() && effX >= slotCenter(m_draggedSlot + 1) - hyst) {
        swapSlots(m_draggedSlot, m_draggedSlot + 1);
    } else if (m_draggedSlot > 0 && effX <= slotCenter(m_draggedSlot - 1) + hyst) {
        swapSlots(m_draggedSlot, m_draggedSlot - 1);
    }
}

inline void WorkspaceBarDragModel::swapSlots(int a, int b)
{
    std::swap(m_order[a], m_order[b]);
    m_draggedSlot = b;
}

}
