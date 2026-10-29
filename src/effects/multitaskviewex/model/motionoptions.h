/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <algorithm>
#include <cmath>

namespace KWin
{

inline constexpr int kDefaultMaxWorkspaces = 6;

inline constexpr int kDefaultWorkspaceMoveDurationMs = 400;
inline constexpr int kDefaultWindowChangeDurationMs = 300;
inline constexpr int kDefaultViewEnterDurationMs = 400;

inline constexpr double kDefaultAnimationSpeedMultiplier = 1.0;

inline constexpr double kMinAnimationSpeedMultiplier = 0.1;
inline constexpr double kMaxAnimationSpeedMultiplier = 5.0;

inline constexpr int kMinAnimationDurationMs = 16;

inline constexpr int kOutgoingSlideMs = 300;
inline constexpr int kWindowQuickMs = 120;
inline constexpr int kOverlayFadeMs = 120;
inline constexpr int kGhostSnapMs = 400;
inline constexpr int kBarReorderDragMs = 150;

struct MotionOptions
{
    int workspaceMoveDurationMs = kDefaultWorkspaceMoveDurationMs;
    int windowChangeDurationMs = kDefaultWindowChangeDurationMs;
    int viewEnterDurationMs = kDefaultViewEnterDurationMs;
    double animationSpeedMultiplier = kDefaultAnimationSpeedMultiplier;
};

/**
 * @brief Clamps a speed multiplier into the supported range.
 * @param multiplier Requested speed multiplier.
 * @return The multiplier clamped to [kMinAnimationSpeedMultiplier, kMaxAnimationSpeedMultiplier].
 */
inline double clampAnimationSpeedMultiplier(double multiplier)
{
    return std::clamp(multiplier, kMinAnimationSpeedMultiplier, kMaxAnimationSpeedMultiplier);
}

/**
 * @brief Resolves a base duration under a speed multiplier.
 * @param baseMs Base duration in milliseconds.
 * @param multiplier Speed multiplier; a higher value means a faster (shorter) animation.
 * @return The scaled duration in milliseconds, clamped to at least kMinAnimationDurationMs.
 */
inline int resolveMs(int baseMs, double multiplier)
{
    const double clamped = clampAnimationSpeedMultiplier(multiplier);
    const int scaled = int(std::lround(double(baseMs) / clamped));
    return std::max(kMinAnimationDurationMs, scaled);
}

}
