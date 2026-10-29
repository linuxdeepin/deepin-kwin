/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "animation.h"
#include "visualstate.h"

#include <QRectF>
#include <QtGlobal>

namespace KWin
{

struct VisualNode
{
    VisualState cur;
    VisualState from;
    VisualState to;
    Animation timeline;

    /**
     * @brief Starts a transition toward a new visual state.
     * @param t Target visual state.
     * @param durationMs Transition duration in milliseconds.
     * @param easing Easing curve.
     */
    void setTarget(const VisualState &t, int durationMs, Easing easing)
    {
        if (sameCore(to, t)) {
            return;
        }
        from = cur;
        to = t;
        timeline.configure(durationMs, easing);
        timeline.start();
    }

    /**
     * @brief Jumps immediately to a visual state without animating.
     * @param s State to snap to.
     */
    void snap(const VisualState &s)
    {
        timeline.reset();
        cur = from = to = s;
    }

    /**
     * @brief Advances the transition by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advance(int dtMs)
    {
        if (!timeline.running()) {
            return;
        }
        timeline.advance(dtMs);
        const qreal k = timeline.value();
        cur.x = mix(from.x, to.x, k);
        cur.y = mix(from.y, to.y, k);
        cur.w = mix(from.w, to.w, k);
        cur.h = mix(from.h, to.h, k);
        cur.opacity = mix(from.opacity, to.opacity, k);
        if (timeline.done()) {
            cur = to;
        }
    }

    /**
     * @brief Tests whether a transition is in progress.
     * @return True while the timeline is running.
     */
    bool animating() const
    {
        return timeline.running();
    }

    /**
     * @brief Returns the current interpolated rectangle.
     * @return The current geometry.
     */
    QRectF rect() const
    {
        return QRectF(cur.x, cur.y, cur.w, cur.h);
    }

    /**
     * @brief Tests whether two visual states have equal position, size and opacity.
     * @param a First state.
     * @param b Second state.
     * @return True when all components compare fuzzy-equal.
     */
    static bool sameCore(const VisualState &a, const VisualState &b)
    {
        return qFuzzyCompare(a.x, b.x) && qFuzzyCompare(a.y, b.y) && qFuzzyCompare(a.w, b.w)
            && qFuzzyCompare(a.h, b.h) && qFuzzyCompare(a.opacity, b.opacity);
    }

    /**
     * @brief Linearly interpolates between two values.
     * @param a Value at k = 0.
     * @param b Value at k = 1.
     * @param k Interpolation factor.
     * @return The interpolated value.
     */
    static qreal mix(qreal a, qreal b, qreal k)
    {
        return a + (b - a) * k;
    }
};

}
