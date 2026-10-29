/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "animation/visualnode.h"
#include "model/windowitem.h"

#include <QRectF>

namespace KWin
{

class EffectWindow;

struct WindowNode
{
    VisualNode interp;
    Animation overlayTimeline;
    qreal overlayFrom = 0.0;
    qreal overlayTarget = 0.0;
    qreal overlay = 0.0;

    EffectWindow *window = nullptr;
    QRectF source;
    int cellIndex = -1;
    bool retiring = false;

    /**
     * @brief Starts a transition toward a new geometry and opacity.
     * @param r Target rectangle.
     * @param opacity Target opacity.
     * @param durationMs Transition duration in milliseconds.
     * @param easing Easing curve.
     */
    void setTarget(const QRectF &r, qreal opacity, int durationMs, Easing easing)
    {
        interp.setTarget(VisualState{r.x(), r.y(), r.width(), r.height(), opacity}, durationMs, easing);
    }

    /**
     * @brief Jumps immediately to a visual state without animating.
     * @param s State to snap to.
     */
    void snap(const VisualState &s)
    {
        interp.snap(s);
    }

    /**
     * @brief Starts a transition of the overlay (button) opacity.
     * @param alpha Target overlay alpha.
     * @param durationMs Transition duration in milliseconds.
     * @param easing Easing curve.
     */
    void setOverlayTarget(qreal alpha, int durationMs, Easing easing)
    {
        if (qFuzzyCompare(overlayTarget, alpha)) {
            return;
        }
        overlayFrom = overlay;
        overlayTarget = alpha;
        overlayTimeline.configure(durationMs, easing);
        overlayTimeline.start();
    }

    /**
     * @brief Advances the overlay transition by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advanceOverlay(int dtMs)
    {
        if (!overlayTimeline.running()) {
            return;
        }
        overlayTimeline.advance(dtMs);
        overlay = overlayFrom + (overlayTarget - overlayFrom) * overlayTimeline.value();
        if (overlayTimeline.done()) {
            overlay = overlayTarget;
        }
    }

    /**
     * @brief Advances both the geometry and the overlay transitions.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advance(int dtMs)
    {
        interp.advance(dtMs);
        advanceOverlay(dtMs);
    }

    /**
     * @brief Tests whether the geometry transition is running.
     * @return True while the geometry is animating.
     */
    bool animating() const
    {
        return interp.animating();
    }

    /**
     * @brief Tests whether the overlay transition is running.
     * @return True while the overlay is animating.
     */
    bool overlayAnimating() const
    {
        return overlayTimeline.running();
    }

    /**
     * @brief Returns the current window rectangle.
     * @return The current geometry.
     */
    QRectF rect() const
    {
        return interp.rect();
    }

    /**
     * @brief Returns the current window opacity.
     * @return The current opacity.
     */
    qreal opacity() const
    {
        return interp.cur.opacity;
    }

    /**
     * @brief Returns the current overlay opacity.
     * @return The current overlay alpha.
     */
    qreal overlayOpacity() const
    {
        return overlay;
    }

    /**
     * @brief Returns the window rectangle expanded for the overlay outline.
     * @param s Scale factor.
     * @return The outline rectangle.
     */
    QRectF overlayRect(qreal s = 1.0) const
    {
        return rect().adjusted(-3 * s, -3 * s, 3 * s, 3 * s);
    }

    /**
     * @brief Returns the close button rectangle for the window.
     * @param s Scale factor.
     * @return The close button rectangle.
     */
    QRectF closeButtonRect(qreal s = 1.0) const
    {
        const QRectF c = rect();
        return QRectF(c.right() - 24 * s, c.top() - 24 * s, kButtonSize * s, kButtonSize * s);
    }

    /**
     * @brief Returns the "move to top" button rectangle for the window.
     * @param s Scale factor.
     * @return The top button rectangle.
     */
    QRectF topButtonRect(qreal s = 1.0) const
    {
        const QRectF c = rect();
        return QRectF(c.left() - 24 * s, c.top() - 24 * s, kButtonSize * s, kButtonSize * s);
    }
};

}
