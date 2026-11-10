/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

namespace KWin
{

/**
 * @brief Minimum progress or opacity for a hint to be considered visible.
 */
inline constexpr qreal kHintVisibilityThreshold = 0.01;

/**
 * @brief Returns whether the empty-state hint should be visible.
 * @param hasWindows Whether any windows are present.
 * @param hasRetiringWindows Whether any windows are still retiring.
 * @param dragActive Whether a window drag is active.
 * @param ghostReturning Whether a ghost return animation is active.
 * @return True when the empty-state hint should be shown.
 */
inline bool shouldShowEmptyHint(bool hasWindows, bool hasRetiringWindows, bool dragActive, bool ghostReturning)
{
    return !hasWindows && !hasRetiringWindows && !dragActive && !ghostReturning;
}

/**
 * @brief Returns whether the hover window-info hint should be visible.
 * @param enterProgress View enter progress.
 * @param bgSliding Whether the background is sliding.
 * @param dragActive Whether a window drag is active.
 * @param barDragActive Whether a workspace-bar drag is active.
 * @param ghostReturning Whether a ghost return animation is active.
 * @param hasHovered Whether a hovered window exists.
 * @param overlayOpacity Hovered window overlay opacity.
 * @return True when the window-info hint should be shown.
 */
inline bool shouldShowWindowInfoHint(qreal enterProgress, bool bgSliding, bool dragActive, bool barDragActive,
                                     bool ghostReturning, bool hasHovered, qreal overlayOpacity)
{
    return enterProgress > kHintVisibilityThreshold && !bgSliding && !dragActive && !barDragActive && !ghostReturning
        && hasHovered && overlayOpacity > kHintVisibilityThreshold;
}

}
