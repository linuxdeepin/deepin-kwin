/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QVariant>

namespace KWin
{

class EffectWindow;
struct ScreenView;

/**
 * @brief Applies and restores the window roles used while the view is active.
 *
 * The view rounds window corners per grid row and forces scissoring so the
 * scaled window contents stay inside their cell. The previous role values are
 * remembered per window and restored when the window leaves the view.
 */
class WindowRoles
{
public:
    /**
     * @brief Constructs the role manager.
     * @param osScale Initial OS scale factor.
     */
    explicit WindowRoles(qreal osScale = 1.0);

    /**
     * @brief Sets the OS scale factor used for row corner radii.
     * @param scale New scale factor.
     */
    void setOsScale(qreal scale)
    {
        m_osScale = scale;
    }

    /**
     * @brief Applies the view roles to every window of a screen view.
     * @param view Screen view whose windows are shown.
     */
    void apply(ScreenView &view);

    /**
     * @brief Restores the roles of a window.
     * @param w Window whose roles are restored.
     */
    void restore(EffectWindow &w);

    /**
     * @brief Forgets every saved role value.
     */
    void clear();

private:
    qreal m_osScale = 1.0;
    QHash<EffectWindow *, QVariant> m_savedRadius;
    QHash<EffectWindow *, bool> m_savedScissor;
};

}
