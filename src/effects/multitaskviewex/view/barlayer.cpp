/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "barlayer.h"
#include "data/workspacebarmetrics.h"
#include "model/bardraglayout.h"
#include "model/bardragmodel.h"
#include "model/barpredicates.h"

#include <algorithm>

namespace KWin
{

void BarLayer::apply(const QVector<BarItem> &items, const BarItem *preview,
                     int durationMs, Easing easing, bool dragActive, const LayoutInput &in)
{
    QVector<BarDragTarget> targets;
    if (in.metrics && in.order) {
        const qreal previewX = previewXFor(in.canvasWidth, in.metrics->workspaceWidth());
        targets = layoutBarDragTargets(*in.drag, *in.order, previewX, in.previewActive, in.scrollOffset);
    }

    AlignInput ai;
    ai.currentNum = in.currentNum;
    ai.hoveredSlot = in.hoveredSlot;
    ai.draggedId = in.drag ? in.drag->draggedId() : -1;
    ai.dropRaiseKey = in.dropRaiseKey;
    ai.y = in.metrics ? in.metrics->spacingHeight() + in.origin.y() : 0.0;
    ai.h = in.metrics ? in.metrics->workspaceHeight() : 0.0;
    ai.dragActive = dragActive;
    ai.ghostFadeMs = in.ghostFadeMs;
    applyTargets(items, preview, targets, durationMs, easing, ai);
}

void BarLayer::applyTargets(const QVector<BarItem> &items, const BarItem *preview,
                            const QVector<BarDragTarget> &targets,
                            int durationMs, Easing easing, const AlignInput &in)
{
    const QVector<BarNode> oldNodes = m_nodes;

    QVector<BarNode> next;
    next.reserve(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const BarItem &item = items[i];
        const BarDragTarget *t = nullptr;
        for (const BarDragTarget &cand : targets) {
            if (cand.id == item.num) {
                t = &cand;
                break;
            }
        }
        if (!t) {
            continue;
        }
        const BarNode *old = nullptr;
        for (const BarNode &n : oldNodes) {
            if (n.key == item.key) {
                old = &n;
                break;
            }
        }
        const QRectF rect(t->x, in.y + t->y, t->w, in.h);
        BarNode n = old ? *old : BarNode{};
        n.key = item.key;
        n.cell = QRectF(t->cellX, in.y, t->w, in.h);
        n.current = (item.num == in.currentNum);
        n.hovered = (i == in.hoveredSlot);
        n.raised = (in.dragActive && t->id == in.draggedId)
            || (!in.dropRaiseKey.isEmpty() && item.key == in.dropRaiseKey);
        if (old) {
            if (in.dragActive && n.raised) {
                n.snap({rect.x(), rect.y(), rect.width(), rect.height(), t->opacity});
                n.placeholderX = t->placeholderX;
                n.placeholderW = t->placeholderW;
                n.placeholderTargetX = t->placeholderX;
                n.placeholderTargetW = t->placeholderW;
            } else {
                n.setTarget(rect, t->opacity, t->placeholderX, t->placeholderW, durationMs, easing);
            }
        } else {
            if (m_lastPreviewCur && m_preview.has_value() && preview == nullptr) {
                n.cur = n.from = *m_lastPreviewCur;
                n.to = {rect.x(), rect.y(), rect.width(), rect.height(), t->opacity};
                n.timeline.configure(durationMs, easing);
                n.timeline.start();
            } else {
                n.snap({rect.x(), rect.y(), rect.width(), rect.height(), t->opacity});
            }
            n.placeholderX = t->placeholderX;
            n.placeholderW = t->placeholderW;
        }
        next.append(n);
    }

    for (const BarNode &old : oldNodes) {
        bool alive = false;
        for (const BarItem &it : items) {
            if (it.key == old.key) {
                alive = true;
                break;
            }
        }
        if (!alive) {
            BarNode g = old;
            g.ghost = true;
            const VisualState keep = g.cur;
            g.setTarget(QRectF(keep.x, keep.y, keep.w, keep.h), 0.0, 0.0, 0.0,
                        in.ghostFadeMs, Easing::OutCubic);
            m_ghosts.append(g);
        }
    }

    m_nodes = next;

    if (preview && !preview->key.isEmpty()) {
        BarDragTarget pt{};
        for (const BarDragTarget &t : targets) {
            if (t.id == kAddPreviewId) {
                pt = t;
                break;
            }
        }
        const QRectF pre(pt.x, in.y + pt.y, pt.w, in.h);
        if (m_preview) {
            m_preview->key = preview->key;
            m_preview->setTarget(pre, pt.opacity, 0.0, 0.0, durationMs, easing);
        } else {
            BarNode n;
            n.key = preview->key;
            n.snap({pre.x(), pre.y(), pre.width(), pre.height(), pt.opacity});
            m_preview = n;
        }
    } else {
        m_preview.reset();
    }
    m_lastPreviewCur = m_preview ? std::optional<VisualState>(m_preview->cur) : std::nullopt;
}

void BarLayer::advance(int dtMs)
{
    if (dtMs <= 0) {
        return;
    }
    for (BarNode &n : m_nodes) {
        n.advance(dtMs);
    }
    for (BarNode &g : m_ghosts) {
        g.advance(dtMs);
    }
    m_ghosts.erase(std::remove_if(m_ghosts.begin(), m_ghosts.end(),
                                  [](const BarNode &g) {
                                      return !g.animating() && g.cur.opacity <= 0.01;
                                  }),
                   m_ghosts.end());
    if (m_preview) {
        m_preview->advance(dtMs);
    }
}

bool BarLayer::animating() const
{
    for (const BarNode &n : m_nodes) {
        if (n.animating()) {
            return true;
        }
    }
    for (const BarNode &g : m_ghosts) {
        if (g.animating() || g.cur.opacity > 0.01) {
            return true;
        }
    }
    if (m_preview && m_preview->animating()) {
        return true;
    }
    return false;
}

}
