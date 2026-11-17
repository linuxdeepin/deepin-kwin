/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "glpaintrenderer.h"
#include "glscene.h"
#include "model/windowmetrics.h"
#include "windowcellcontrol.h"

#include <algorithm>

namespace KWin
{

WindowCellControl::WindowCellControl(QObject *parent)
    : GLControl(SceneLayer::Cells, parent)
{
}

QRectF WindowCellControl::cellRect(int i) const
{
    if (!m_cellBase || i < 0 || i >= m_cellBase->size()) {
        return QRectF();
    }
    const qreal scroll = m_scene ? m_scene->gridScrollTarget() : 0.0;
    const qreal slideDx = m_scene ? m_scene->slideOffsetX() : 0.0;
    return m_cellBase->at(i).translated(slideDx, scroll);
}

QRectF WindowCellControl::coverRect(int i) const
{
    if (!m_cellBase || i < 0 || i >= m_cellBase->size()) {
        return QRectF();
    }
    QRectF basis = cellRect(i);
    const int placeholder = m_model ? m_model->dragPlaceholder() : -1;
    if (m_windowLayer && i != placeholder) {
        const qreal slideDx = m_scene ? m_scene->slideOffsetX() : 0.0;
        for (const WindowNode &n : m_windowLayer->nodes()) {
            if (n.cellIndex == i && !n.retiring) {
                basis = n.rect().translated(slideDx, 0.0);
                break;
            }
        }
    }
    return coverFor(basis, m_model ? m_model->rowHeight() : 0.0);
}

qreal WindowCellControl::cellRadius(int i) const
{
    if (!m_cellBase || i < 0 || i >= m_cellBase->size()) {
        return 0.0;
    }
    const QVector<qreal> rowYs = uniqueRowYs(*m_cellBase);
    return windowCornerRadius(gridRowOf(m_cellBase->at(i).y(), rowYs), m_model ? m_model->osScale() : 1.0);
}

int WindowCellControl::cellAt(const QPointF &pos) const
{
    for (int i = 0; i < cellCount(); ++i) {
        if (cellRect(i).contains(pos)) {
            return i;
        }
        const QRectF cv = coverRect(i);
        const qreal hb = kButtonSize / 2.0;
        if (QRectF(cv.left() - hb, cv.top() - hb, kButtonSize, kButtonSize).contains(pos)
            || QRectF(cv.right() - hb, cv.top() - hb, kButtonSize, kButtonSize).contains(pos)) {
            return i;
        }
    }
    return -1;
}

void WindowCellControl::paint(GLPaintContext &ctx, const SceneFrame &frame)
{
    const qreal fade = frame.enterProgress;
    auto &r = GLPaintRenderer::instance();
    const qreal s = m_model ? m_model->osScale() : 1.0;
    GLPaintRenderer::ScissorScope scissor(ctx.globalRect(m_clipRect), ctx.viewportSize.height());
    for (int i = 0; i < cellCount(); ++i) {
        const CoverVisual cv = coverVisual();
        r.drawColor(QColor(cv.r, cv.g, cv.b), coverRect(i), cellRadius(i), cv.opacity * fade, ctx.projection());
    }
    if (m_outgoing) {
        const CoverVisual cv = coverVisual();
        for (const OutgoingGrid &snap : m_outgoing->grids()) {
            for (int c = 0; c < snap.cells.size(); ++c) {
                const qreal radius = c < snap.cellRadii.size() ? snap.cellRadii[c] : 10.0 * s;
                r.drawColor(QColor(cv.r, cv.g, cv.b), snap.cells[c].rect(), radius, cv.opacity * fade, ctx.projection());
            }
        }
    }
}

void WindowCellControl::press(const QPointF &pos)
{
    m_pressed = true;
    m_dragging = false;
    m_pressCell = cellAt(pos);
    m_pressPos = pos;
}

void WindowCellControl::move(const QPointF &pos)
{
    if (dragExceeded(pos) && m_pressCell >= 0) {
        m_dragging = true;
        Q_EMIT cellDragStarted(m_pressCell);
    }
    if (m_dragging) {
        Q_EMIT cellDragMoved(pos);
    }
}

void WindowCellControl::release(const QPointF &pos)
{
    if (m_dragging) {
        Q_EMIT cellDragFinished(pos);
    } else if (m_pressCell >= 0) {
        Q_EMIT cellClicked(m_pressCell);
    }
    clearInputState();
    m_pressCell = -1;
}

void WindowCellControl::cancel()
{
    GLControl::cancel();
    m_pressCell = -1;
}

}
