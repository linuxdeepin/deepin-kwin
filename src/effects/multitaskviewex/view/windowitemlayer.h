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
     * @brief Sets the horizontal offset applied to every node while a workspace slide runs.
     *
     * The offset belongs to the layer, not to individual nodes: a workspace switch must
     * translate the whole grid as one block, so every window is painted at its resting
     * rect plus this single offset.
     *
     * @param dx Offset in canvas coordinates.
     */
    void setSlideOffset(qreal dx)
    {
        m_slideOffset = dx;
    }

    /**
     * @brief Returns the slide offset applied to the whole grid this frame.
     * @return Offset in canvas coordinates; 0 when no slide is running.
     */
    qreal slideOffset() const
    {
        return m_slideOffset;
    }

    /**
     * @brief Returns the rect where a window is actually painted this frame.
     * @param n Window node.
     * @return Painted rect: the resting rect translated by the layer slide offset.
     */
    QRectF paintedRect(const WindowNode &n) const
    {
        return n.rect().translated(m_slideOffset, 0.0);
    }

private:
    QVector<WindowNode> m_nodes;
    qreal m_slideOffset = 0.0;
};

}
