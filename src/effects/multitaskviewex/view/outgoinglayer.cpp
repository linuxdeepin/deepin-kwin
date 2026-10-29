/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "model/bardragmodel.h"
#include "model/windowgrid.h"
#include "model/windowmetrics.h"
#include "outgoinglayer.h"

#include <algorithm>

namespace KWin
{

bool OutgoingGrid::animating() const
{
    for (const OutgoingWindow &w : windows) {
        if (w.interp.animating()) {
            return true;
        }
    }
    for (const VisualNode &c : cells) {
        if (c.animating()) {
            return true;
        }
    }
    return false;
}

void OutgoingGrid::advance(int dtMs)
{
    for (OutgoingWindow &w : windows) {
        w.interp.advance(dtMs);
    }
    for (VisualNode &c : cells) {
        c.advance(dtMs);
    }
}

void OutgoingLayer::snapshot(qreal slideOffsetX, const QVector<OutgoingWindowInput> &wins,
                             const QVector<QRectF> &cellBase, qreal rowHeight, qreal osScale, int durationMs)
{
    OutgoingGrid snap;
    snap.slideDx = -slideOffsetX;

    snap.windows.reserve(wins.size());
    for (const OutgoingWindowInput &wi : wins) {
        if (!wi.window) {
            continue;
        }
        OutgoingWindow gw;
        gw.window = wi.window;
        gw.source = wi.source;
        gw.interp.snap(VisualState{wi.cur.x(), wi.cur.y(), wi.cur.width(), wi.cur.height(), wi.op});
        gw.interp.setTarget(VisualState{wi.cur.x() + snap.slideDx, wi.cur.y(),
                                        wi.cur.width(), wi.cur.height(), 1.0},
                            durationMs, Easing::OutCubic);
        snap.windows.append(gw);
    }

    const QVector<qreal> rowYs = uniqueRowYs(cellBase);
    snap.cells.reserve(cellBase.size());
    snap.cellRadii.reserve(cellBase.size());
    for (const QRectF &base : cellBase) {
        const QRectF cover = coverFor(base, rowHeight);
        VisualNode n;
        n.snap(VisualState{cover.x(), cover.y(), cover.width(), cover.height(), 1.0});
        n.setTarget(VisualState{cover.x() + snap.slideDx, cover.y(), cover.width(), cover.height(), 1.0},
                    durationMs, Easing::OutCubic);
        snap.cells.append(n);
        snap.cellRadii.append(windowCornerRadius(gridRowOf(base.y(), rowYs), osScale));
    }

    if (!snap.empty()) {
        m_grids.append(snap);
    }
}

void OutgoingLayer::advance(int dtMs)
{
    if (dtMs <= 0) {
        return;
    }
    for (int i = m_grids.size() - 1; i >= 0; --i) {
        OutgoingGrid &snap = m_grids[i];
        snap.advance(dtMs);
        if (!snap.animating()) {
            for (const OutgoingWindow &w : std::as_const(snap.windows)) {
                if (w.window) {
                    m_finished.append(w.window);
                }
            }
            m_grids.removeAt(i);
        }
    }
}

bool OutgoingLayer::animating() const
{
    for (const OutgoingGrid &snap : m_grids) {
        if (snap.animating()) {
            return true;
        }
    }
    return false;
}

QVector<EffectWindow *> OutgoingLayer::takeFinished()
{
    QVector<EffectWindow *> done;
    done.swap(m_finished);
    return done;
}

void OutgoingLayer::removeWindow(EffectWindow *w)
{
    for (OutgoingGrid &snap : m_grids) {
        for (int i = snap.windows.size() - 1; i >= 0; --i) {
            if (snap.windows[i].window == w) {
                snap.windows.removeAt(i);
            }
        }
    }
    m_finished.removeAll(w);
}

}
