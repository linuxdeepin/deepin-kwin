/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "animation/visualnode.h"
#include "model/motionoptions.h"
#include "model/windowitem.h"
#include "windownode.h"

#include <QVector>

namespace KWin
{

class WindowItemLayer
{
public:
    struct AlignInput
    {
        const QVector<QRectF> *cellBase = nullptr;
        qreal scrollOffset = 0.0;
        int hoveredIndex = -1;
        bool entered = false;
        int placeholderCell = -1;
        int retireMs = kDefaultWindowChangeDurationMs;
        int overlayMs = kOverlayFadeMs;
    };

    /**
     * @brief Lays out window nodes from the item list.
     * @param items Window items, in order.
     * @param durationMs Transition duration in milliseconds.
     * @param easing Easing curve.
     * @param in Alignment inputs.
     */
    void apply(const QVector<WindowItem> &items, int durationMs, Easing easing, const AlignInput &in)
    {
        const QVector<WindowNode> oldNodes = m_nodes;
        QVector<WindowNode> next;
        next.reserve(items.size());
        for (int i = 0; i < items.size(); ++i) {
            const WindowItem &item = items[i];
            const WindowNode *old = nullptr;
            for (const WindowNode &n : oldNodes) {
                if (n.window == item.window) {
                    old = &n;
                    break;
                }
            }
            WindowNode n = old ? *old : WindowNode{};
            n.window = item.window;
            n.source = item.source;
            n.cellIndex = item.cellIndex;
            n.retiring = item.retiring;
            if (item.retiring) {
                n.setTarget(n.rect(), 0.0, in.retireMs, easing);
                n.setOverlayTarget(0.0, in.overlayMs, Easing::OutCubic);
                next.append(n);
                continue;
            }
            const QRectF base = (item.cellIndex >= 0 && in.cellBase && item.cellIndex < in.cellBase->size())
                ? in.cellBase->at(item.cellIndex)
                : item.source;
            const QRectF target = in.entered ? base.translated(0, in.scrollOffset) : item.source;
            const bool placeholder = (item.cellIndex == in.placeholderCell);
            const qreal op = placeholder ? 0.0 : 1.0;
            if (old) {
                n.setTarget(target, op, durationMs, easing);
            } else {
                const QRectF start = in.entered ? base : item.source;
                n.interp.snap(VisualState{start.x(), start.y(), start.width(), start.height(), op});
                n.overlay = 0.0;
                n.setTarget(target, op, durationMs, easing);
            }
            n.setOverlayTarget((!placeholder && i == in.hoveredIndex) ? 1.0 : 0.0, in.overlayMs, Easing::OutCubic);
            next.append(n);
        }
        m_nodes = next;
    }

    /**
     * @brief Advances every window node by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advance(int dtMs)
    {
        for (WindowNode &n : m_nodes) {
            n.advance(dtMs);
        }
    }

    /**
     * @brief Tests whether any window node is animating.
     * @return True while at least one node is animating.
     */
    bool animating() const
    {
        for (const WindowNode &n : m_nodes) {
            if (n.animating() || n.overlayAnimating()) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Returns the node of a window.
     * @param w Window to look up.
     * @return The matching node, or nullptr when absent.
     */
    WindowNode *nodeFor(EffectWindow *w)
    {
        for (WindowNode &n : m_nodes) {
            if (n.window == w) {
                return &n;
            }
        }
        return nullptr;
    }

    /**
     * @brief Returns the node of a window.
     * @param w Window to look up.
     * @return The matching node, or nullptr when absent.
     */
    const WindowNode *nodeFor(EffectWindow *w) const
    {
        for (const WindowNode &n : m_nodes) {
            if (n.window == w) {
                return &n;
            }
        }
        return nullptr;
    }

    /**
     * @brief Returns the mutable list of window nodes.
     * @return The window nodes.
     */
    QVector<WindowNode> &nodes()
    {
        return m_nodes;
    }

    /**
     * @brief Returns the list of window nodes.
     * @return The window nodes.
     */
    const QVector<WindowNode> &nodes() const
    {
        return m_nodes;
    }

    /**
     * @brief Clears every window node.
     */
    void clear()
    {
        m_nodes.clear();
    }

    /**
     * @brief Horizontal offsets of the two grids a workspace slide moves, in canvas coordinates.
     *
     * A workspace switch slides two grids past each other: the grid of the incoming workspace
     * enters from one side while the grid of the outgoing workspace leaves on the other side.
     * Expressing both in the same space - the canvas of this screen - keeps them exactly one
     * canvas width apart, so no consumer has to know how far the animation has progressed.
     */
    struct GridSlideOffsets
    {
        qreal incoming = 0.0; ///< Offset of the grid that is entering; 0 when nothing slides.
        qreal outgoing = 0.0; ///< Offset of the grid that is leaving; 0 when nothing slides.
    };

    /**
     * @brief Sets the offsets of both grids for this frame.
     * @param offsets Offsets in canvas coordinates.
     */
    void setSlideOffsets(const GridSlideOffsets &offsets)
    {
        m_slideOffsets = offsets;
    }

    /**
     * @brief Returns the offsets of both grids for this frame.
     * @return Offsets in canvas coordinates; both 0 when no slide is running.
     */
    GridSlideOffsets slideOffsets() const
    {
        return m_slideOffsets;
    }

    /**
     * @brief Returns the rect where a window is actually painted this frame.
     *
     * A retiring window is still part of the workspace that is leaving, every other window
     * already belongs to the workspace that is entering. Which of the two offsets applies is
     * therefore decided by the node itself, not by the layer.
     *
     * @param n Window node.
     * @return Painted rect: the resting rect translated by the offset of the node's own grid.
     */
    QRectF paintedRect(const WindowNode &n) const
    {
        return n.rect().translated(n.retiring ? m_slideOffsets.outgoing : m_slideOffsets.incoming, 0.0);
    }

private:
    QVector<WindowNode> m_nodes;
    GridSlideOffsets m_slideOffsets;
};

}
