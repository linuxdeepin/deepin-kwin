/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

namespace KWin
{

enum class Easing {
    Linear,
    InQuad,
    OutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
    InQuint,
    OutQuint,
};

/**
 * @brief Evaluates an easing curve at a normalized time.
 * @param easing Easing curve to evaluate.
 * @param t Normalized time in [0, 1].
 * @return The eased progress value.
 */
qreal easedValue(Easing easing, qreal t);

class Animation
{
public:
    /**
     * @brief Configures the timeline duration and easing curve.
     * @param durationMs Total duration in milliseconds.
     * @param easing Easing curve to apply.
     */
    void configure(int durationMs, Easing easing);

    /**
     * @brief Starts the timeline from the beginning.
     */
    void start();

    /**
     * @brief Advances the timeline by a frame delta.
     * @param msDelta Elapsed time in milliseconds.
     */
    void advance(int msDelta);

    /**
     * @brief Stops the timeline and resets elapsed time.
     */
    void reset();

    /**
     * @brief Returns linear progress in [0, 1].
     * @return Elapsed fraction of the duration.
     */
    qreal progress() const;

    /**
     * @brief Returns the eased progress value.
     * @return The easing curve evaluated at progress().
     */
    qreal value() const;

    /**
     * @brief Tests whether the timeline has reached the end.
     * @return True when progress is complete.
     */
    bool done() const;

    /**
     * @brief Tests whether the timeline is currently running.
     * @return True when started and not yet done.
     */
    bool running() const
    {
        return !done() && m_started;
    }

private:
    int m_durationMs = 0;
    int m_elapsedMs = 0;
    Easing m_easing = Easing::Linear;
    bool m_started = false;
};

}
