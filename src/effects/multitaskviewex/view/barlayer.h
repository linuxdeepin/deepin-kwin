/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "animation/visualnode.h"
#include "model/bardraglayout.h"
#include "model/bardragmodel.h"
#include "model/baritem.h"
#include "model/motionoptions.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <optional>

namespace KWin
{

class WorkspaceBarDragModel;
class WorkspaceBarMetrics;

struct BarNode : VisualNode
{
    QString key;
    bool ghost = false;
    QRectF cell;
    bool current = false;
    bool hovered = false;
    bool raised = false;
    qreal placeholderX = 0;
    qreal placeholderW = 0;
    qreal placeholderFromX = 0;
    qreal placeholderFromW = 0;
    qreal placeholderTargetX = 0;
    qreal placeholderTargetW = 0;

    /**
     * @brief Starts a transition toward a new bar node state.
     * @param r Target rectangle.
     * @param opacity Target opacity.
     * @param phX Target placeholder x.
     * @param phW Target placeholder width.
     * @param durationMs Transition duration in milliseconds.
     * @param easing Easing curve.
     */
    void setTarget(const QRectF &r, qreal opacity, qreal phX, qreal phW,
                   int durationMs, Easing easing)
    {
        const VisualState t = {r.x(), r.y(), r.width(), r.height(), opacity};
        if (sameCore(to, t)
            && qFuzzyCompare(placeholderTargetX, phX)
            && qFuzzyCompare(placeholderTargetW, phW)) {
            return;
        }
        from = cur;
        to = t;
        placeholderFromX = placeholderX;
        placeholderFromW = placeholderW;
        placeholderTargetX = phX;
        placeholderTargetW = phW;
        timeline.configure(durationMs, easing);
        timeline.start();
    }

    /**
     * @brief Advances the geometry and placeholder transitions by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advance(int dtMs)
    {
        if (!timeline.running()) {
            return;
        }
        VisualNode::advance(dtMs);
        const qreal k = timeline.value();
        placeholderX = mix(placeholderFromX, placeholderTargetX, k);
        placeholderW = mix(placeholderFromW, placeholderTargetW, k);
        if (timeline.done()) {
            placeholderX = placeholderTargetX;
            placeholderW = placeholderTargetW;
        }
    }

    /**
     * @brief Returns the current placeholder rectangle.
     * @return The placeholder rectangle.
     */
    QRectF placeholderRect() const
    {
        const qreal cx = placeholderX + (cur.w - placeholderW) / 2.0;
        return QRectF(cx, cur.y, placeholderW, cur.h);
    }
};

class BarLayer
{
public:
    struct LayoutInput
    {
        const WorkspaceBarDragModel *drag = nullptr;
        const WorkspaceBarMetrics *metrics = nullptr;
        const QVector<int> *order = nullptr;
        int currentNum = 0;
        int hoveredSlot = -1;
        QPointF origin;
        qreal canvasWidth = 0.0;
        qreal scrollOffset = 0.0;
        bool previewActive = false;
        QString dropRaiseKey;
        int ghostFadeMs = kBarReorderDragMs;
    };

    /**
     * @brief Lays out bar nodes from the item list and drag state.
     * @param items Bar items, in order.
     * @param preview Add-preview item, or nullptr when inactive.
     * @param durationMs Transition duration in milliseconds.
     * @param easing Easing curve.
     * @param dragActive Whether a bar drag is active.
     * @param in Additional layout inputs.
     */
    void apply(const QVector<BarItem> &items, const BarItem *preview,
               int durationMs, Easing easing, bool dragActive, const LayoutInput &in);

    struct AlignInput
    {
        int currentNum = 0;
        int hoveredSlot = -1;
        int draggedId = -1;
        QString dropRaiseKey;
        qreal y = 0;
        qreal h = 0;
        bool dragActive = false;
        int ghostFadeMs = kBarReorderDragMs;
    };

    /**
     * @brief Lays out bar nodes from precomputed drag targets.
     * @param items Bar items, in order.
     * @param preview Add-preview item, or nullptr when inactive.
     * @param targets Precomputed layout targets.
     * @param durationMs Transition duration in milliseconds.
     * @param easing Easing curve.
     * @param in Additional alignment inputs.
     */
    void applyTargets(const QVector<BarItem> &items, const BarItem *preview,
                      const QVector<BarDragTarget> &targets,
                      int durationMs, Easing easing, const AlignInput &in);

    /**
     * @brief Advances all bar node transitions by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     */
    void advance(int dtMs);

    /**
     * @brief Tests whether any bar node is animating.
     * @return True while at least one node is animating.
     */
    bool animating() const;

    /**
     * @brief Returns the mutable list of bar nodes.
     * @return The bar nodes.
     */
    QVector<BarNode> &nodes()
    {
        return m_nodes;
    }

    /**
     * @brief Returns the list of bar nodes.
     * @return The bar nodes.
     */
    const QVector<BarNode> &nodes() const
    {
        return m_nodes;
    }

    /**
     * @brief Returns the add-preview node.
     * @return The preview node, or nullptr when inactive.
     */
    const BarNode *previewNode() const
    {
        return m_preview ? &*m_preview : nullptr;
    }

    /**
     * @brief Clears all nodes, ghosts and the preview.
     */
    void clear()
    {
        m_nodes.clear();
        m_ghosts.clear();
        m_preview.reset();
        m_lastPreviewCur.reset();
    }

private:
    QVector<BarNode> m_nodes;
    QVector<BarNode> m_ghosts;
    std::optional<BarNode> m_preview;
    std::optional<VisualState> m_lastPreviewCur;
};

}
