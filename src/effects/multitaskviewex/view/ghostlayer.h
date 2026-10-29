/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "animation/visualnode.h"

namespace KWin
{

class EffectWindow;

class GhostLayer
{
public:
    /**
     * @brief Sets the window the ghost is drawn from.
     * @param w Source window.
     * @param src Source geometry.
     */
    void setSource(EffectWindow *w, const QRectF &src)
    {
        m_window = w;
        m_source = src;
    }

    /**
     * @brief Starts a transition toward a new ghost geometry.
     * @param r Target rectangle.
     * @param opacity Target opacity.
     * @param durationMs Transition duration in milliseconds.
     * @param easing Easing curve.
     */
    void setTarget(const QRectF &r, qreal opacity, int durationMs, Easing easing)
    {
        m_node.setTarget(VisualState{r.x(), r.y(), r.width(), r.height(), opacity}, durationMs, easing);
    }

    /**
     * @brief Jumps immediately to a ghost geometry without animating.
     * @param r Target rectangle.
     * @param opacity Target opacity.
     */
    void snap(const QRectF &r, qreal opacity)
    {
        m_node.snap(VisualState{r.x(), r.y(), r.width(), r.height(), opacity});
    }

    /**
     * @brief Clears the ghost and its source window.
     */
    void clear()
    {
        m_node.snap(VisualState{0, 0, 0, 0, 0});
        m_window = nullptr;
        m_source = QRectF();
    }

    /**
     * @brief Advances the ghost transition by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advance(int dtMs)
    {
        m_node.advance(dtMs);
    }

    /**
     * @brief Tests whether the ghost is animating.
     * @return True while the ghost transition is running.
     */
    bool animating() const
    {
        return m_node.animating();
    }

    /**
     * @brief Returns the source window of the ghost.
     * @return The source window, or nullptr when inactive.
     */
    EffectWindow *window() const
    {
        return m_window;
    }

    /**
     * @brief Returns the source geometry of the ghost.
     * @return The source rectangle.
     */
    QRectF sourceGeometry() const
    {
        return m_source;
    }

    /**
     * @brief Returns the current ghost rectangle.
     * @return The ghost geometry.
     */
    QRectF rect() const
    {
        return m_node.rect();
    }

    /**
     * @brief Returns the current ghost opacity.
     * @return The ghost opacity.
     */
    qreal opacity() const
    {
        return m_node.cur.opacity;
    }

    /**
     * @brief Tests whether the ghost has a source window.
     * @return True when a source window is set.
     */
    bool active() const
    {
        return m_window != nullptr;
    }

    /**
     * @brief Returns the underlying interpolation node.
     * @return The visual node.
     */
    const VisualNode &node() const
    {
        return m_node;
    }

private:
    VisualNode m_node;
    EffectWindow *m_window = nullptr;
    QRectF m_source;
};

}
