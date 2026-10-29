/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>
#include <QSizeF>

#include <cmath>

namespace KWin
{

class WorkspaceBarMetrics
{
public:
    /**
     * @brief Sets the reference screen size used to scale every metric.
     * @param size Reference size.
     */
    void setSize(const QSizeF &size)
    {
        m_size = size;
    }

    /**
     * @brief Returns the reference screen size.
     * @return The size set through setSize().
     */
    QSizeF size() const
    {
        return m_size;
    }

    /**
     * @brief Returns the width of a single workspace item.
     * @return Scaled item width.
     */
    qreal workspaceWidth() const
    {
        return m_size.width() * 240.0 / 1920.0;
    }

    /**
     * @brief Returns the height of a single workspace item.
     * @return Scaled item height.
     */
    qreal workspaceHeight() const
    {
        return m_size.height() * 240.0 / 1920.0;
    }

    /**
     * @brief Returns the horizontal gap between items within a row.
     * @return Scaled horizontal spacing.
     */
    qreal spacingWidth() const
    {
        return m_size.width() * 20.0 / 1920.0;
    }

    /**
     * @brief Returns the vertical gap below the bar.
     * @return Scaled vertical spacing.
     */
    qreal spacingHeight() const
    {
        return m_size.height() * 20.0 / 1080.0;
    }

    /**
     * @brief Returns the horizontal gap between adjacent workspace slots.
     * @return Scaled slot spacing.
     */
    qreal workspacingWidth() const
    {
        return m_size.width() * 40.0 / 1920.0;
    }

    /**
     * @brief Returns the full width of a workspace slot including its spacing.
     * @return Slot width.
     */
    qreal slotWidth() const
    {
        return workspaceWidth() + workspacingWidth();
    }

    /**
     * @brief Returns the total height reserved for the workspace bar.
     * @return Bar height.
     */
    qreal managerHeight() const
    {
        return spacingHeight() * 2.0 + workspaceHeight();
    }

    /**
     * @brief Returns the x position at which the first item of a row starts.
     * @param nums Number of items in the row.
     * @return The centered start x position.
     */
    qreal xfposFor(int nums) const
    {
        return m_size.width() / 2.0 - (workspaceWidth() * nums) / 2.0
            - std::floor(nums / 2.0) * workspacingWidth();
    }

    /**
     * @brief Returns the x position of one item within a row.
     * @param index Item index in the row.
     * @param nums Total number of items in the row.
     * @return The item x position.
     */
    qreal itemX(int index, int nums) const
    {
        return xfposFor(nums) + index * slotWidth();
    }

    /**
     * @brief Returns the total width of a row of items.
     * @param nums Number of items.
     * @return The content width, zero when nums is not positive.
     */
    qreal contentWidth(int nums) const
    {
        return nums <= 0 ? 0.0 : nums * workspaceWidth() + (nums - 1) * workspacingWidth();
    }

    /**
     * @brief Returns the rectangle of the add-workspace button.
     * @return The add button rectangle.
     */
    QRectF addBtnRect() const
    {
        return QRectF(m_size.width() - m_size.width() * 104 / 1920, m_size.height() * 56 / 1080,
                      m_size.width() * 64 / 1920, m_size.width() * 64 / 1920);
    }

private:
    QSizeF m_size;
};

}
