/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "assetpaths.h"
#include "data/workspacebarmetrics.h"
#include "glpaintrenderer.h"
#include "imageprovider.h"
#include "model/bardraglayout.h"
#include "model/barpredicates.h"
#include "workspacebarcontrol.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QSizeF>

namespace KWin
{

namespace
{
constexpr qreal kDragTipIconWidth = 24.0;
constexpr qreal kDragTipIconHeight = 25.0;
constexpr qreal kDragTipSpacing = 8.0;
constexpr qreal kDragTipTopMargin = 10.0;
constexpr qreal kDashStep = 10.0;
constexpr qreal kDashSegWidth = 6.0;
constexpr qreal kDashSegHeight = 2.0;
}

WorkspaceBarControl::WorkspaceBarControl()
    : GLControl(SceneLayer::Bar)
{
}

WorkspaceBarControl::~WorkspaceBarControl() = default;

QRectF WorkspaceBarControl::itemRect(int row) const
{
    if (!m_barLayer || row < 0 || row >= m_barLayer->nodes().size()) {
        return QRectF();
    }
    return m_barLayer->nodes()[row].rect();
}

QRectF WorkspaceBarControl::deleteBtnRect(int row) const
{
    const QRectF r = itemRect(row);
    if (r.isEmpty()) {
        return QRectF();
    }
    const qreal s = m_model ? m_model->osScale() : 1.0;
    return QRectF(r.x() + r.width() - 30 * s, r.y() - 13 * s, 48 * s, 48 * s);
}

QRectF WorkspaceBarControl::itemHitRect(int row) const
{
    const QRectF r = itemRect(row);
    if (r.isEmpty() || !m_items || m_items->size() <= 1) {
        return r;
    }
    return r.united(deleteBtnRect(row));
}

QRectF WorkspaceBarControl::buttonRect() const
{
    return m_metrics ? m_metrics->addBtnRect().translated(m_layout ? QPointF(m_layout->offX, m_layout->offY) : QPointF())
                     : QRectF();
}

QSize WorkspaceBarControl::thumbCellSize() const
{
    return m_metrics ? QSizeF(m_metrics->workspaceWidth(), m_metrics->workspaceHeight()).toSize() : QSize();
}

int WorkspaceBarControl::slotAt(const QPointF &pos) const
{
    if (!m_items) {
        return -1;
    }
    for (int i = 0; i < m_items->size(); ++i) {
        if (itemHitRect(i).contains(pos)) {
            return i;
        }
    }
    return -1;
}

int WorkspaceBarControl::deleteBtnAt(const QPointF &pos) const
{
    if (!m_items || m_items->size() <= 1) {
        return -1;
    }
    for (int i = 0; i < m_items->size(); ++i) {
        if (deleteBtnRect(i).contains(pos)) {
            return i;
        }
    }
    return -1;
}

bool WorkspaceBarControl::hitTest(const QPointF &pos) const
{
    if (slotAt(pos) >= 0) {
        return true;
    }
    if (!m_items || !showAddButton(m_items->size(), m_model ? m_model->maxWorkspaces() : kDefaultMaxWorkspaces)) {
        return false;
    }
    return buttonRect().contains(pos);
}

void WorkspaceBarControl::paint(GLPaintContext &ctx, const SceneFrame &frame)
{
    if (!m_items || m_items->isEmpty()) {
        return;
    }
    auto &r = GLPaintRenderer::instance();
    const qreal opacity = frame.barOpacity;
    const qreal barShiftY = frame.barShiftY;
    const qreal s = m_model ? m_model->osScale() : 1.0;
    const int draggedId = m_dragModel ? m_dragModel->draggedId() : -1;
    const QVector<BarNode> &nodes = m_barLayer ? m_barLayer->nodes() : m_emptyNodes;

    const QSize thumbCell = thumbCellSize();
    QVector<GLTexture *> itemTex;
    if (m_provider && m_items) {
        itemTex.resize(m_items->size());
        for (int i = 0; i < m_items->size(); ++i) {
            const QString &uri = (*m_items)[i].uri;
            if (!uri.isEmpty()) {
                itemTex[i] = m_provider->textureFor(uri, thumbCell).texture;
            }
        }
    }

    const auto drawItem = [&](const BarNode &node, const BarItem &item, int index) {
        const QRectF rect = node.rect().translated(0, barShiftY);
        if (node.cur.opacity <= 0.01 || rect.isEmpty()) {
            return;
        }
        const qreal itemOpacity = opacity * node.cur.opacity;
        if (index >= 0 && index < itemTex.size() && itemTex[index]) {
            r.drawTexture(itemTex[index], rect, 6 * s, itemOpacity, ctx.projection());
        }
        if (node.current) {
            r.drawColor(QColor(0x00, 0x81, 0xff), rect.adjusted(-3 * s, -3 * s, 3 * s, 3 * s), 10 * s, itemOpacity,
                        ctx.projection(), 4.0 * s);
        }
        if (node.hovered && m_items->size() > 1) {
            const QRectF ico(rect.x() + rect.width() - 14 * s, rect.y() - 9 * s, 24 * s, 24 * s);
            const auto ic = m_provider->textureFor(deleteIconPath(), QSize(int(24 * s), int(24 * s)));
            if (ic.texture) {
                r.drawTexture(ic.texture, ico, 0, itemOpacity, ctx.projection());
            }
        }
    };

    const BarNode *previewNode = m_barLayer ? m_barLayer->previewNode() : nullptr;
    const BarItem *previewItem = m_model ? m_model->barPreviewItem() : nullptr;
    if (previewNode && previewNode->cur.opacity > 0.01) {
        const QRectF pre = previewNode->rect().translated(0, barShiftY);
        if (!pre.isEmpty() && previewItem && !previewItem->uri.isEmpty()) {
            const QSize previewSize = thumbCellSize();
            const auto pi = m_provider->textureForReady(previewItem->uri, previewSize,
                                                        false);
            if (pi.texture) {
                r.drawTexture(pi.texture, pre, 6 * s,
                              opacity * previewNode->cur.opacity, ctx.projection());
            }
        }
    }

    {
        const QRectF clipBar(0, 0, buttonRect().left(), m_metrics ? m_metrics->managerHeight() + 40.0 * s : 0.0);
        GLPaintRenderer::ScissorScope scissor(ctx.globalRect(clipBar), ctx.viewportSize.height());
        for (int i = 0; i < nodes.size() && i < m_items->size(); ++i) {
            if (!nodes[i].raised) {
                drawItem(nodes[i], (*m_items)[i], i);
            }
        }
        for (int i = 0; i < nodes.size() && i < m_items->size(); ++i) {
            if (nodes[i].raised) {
                drawItem(nodes[i], (*m_items)[i], i);
            }
        }
    }

    const int count = m_items->size();
    if (showAddButton(count, m_model ? m_model->maxWorkspaces() : kDefaultMaxWorkspaces) && m_metrics) {
        const QRectF btn = buttonRect().translated(0, barShiftY);
        const auto ic = m_provider->textureFor(addIconPath(), btn.size().toSize());
        if (ic.texture) {
            r.drawTexture(ic.texture, btn, btn.width() / 2, opacity, ctx.projection());
        }
    }

    if (m_dragModel && m_provider && !m_dragTipText.isEmpty() && m_metrics
        && shouldShowDragTip(m_dragModel->dragDelete(), m_items->size())) {
        const BarItem *draggedItem = nullptr;
        for (const BarItem &item : std::as_const(*m_items)) {
            if (item.num == draggedId) {
                draggedItem = &item;
                break;
            }
        }
        if (draggedItem) {
            const BarNode *dn = nullptr;
            if (m_barLayer) {
                for (const BarNode &n : m_barLayer->nodes()) {
                    if (n.key == draggedItem->key) {
                        dn = &n;
                        break;
                    }
                }
            }
            if (!dn) {
                return;
            }
            const QRectF box = dn->cell.translated(0, barShiftY);
            QFont tipFont;
            tipFont.setPointSize(13);
            const auto textInfo = m_provider->textTexture(m_dragTipText, tipFont, QColor(Qt::white));
            const qreal textW = textInfo.imageSize.width();
            const qreal textH = textInfo.imageSize.height();
            const qreal icon = kDragTipIconWidth * s;
            const qreal iconHeight = kDragTipIconHeight * s;
            const qreal spacing = kDragTipSpacing * s;
            auto tip = dragTipLayout(box, icon, textW, textH, spacing, iconHeight);
            const qreal underlineY = qRound((dn->cell.y() + dn->cell.height()) / 2.0 + barShiftY);
            tip.rect.moveTop(underlineY + kDragTipTopMargin * s);
            for (qreal x = box.left(); x < box.right(); x += kDashStep * s) {
                r.drawColor(QColor(Qt::white),
                            QRectF(qRound(x), underlineY, qMin(kDashSegWidth * s, box.right() - x), kDashSegHeight * s),
                            0, opacity, ctx.projection());
            }
            const auto ic = m_provider->textureFor(workspaceDeleteIconPath(), QSize(qRound(icon), qRound(iconHeight)));
            if (ic.texture) {
                r.drawTexture(ic.texture,
                              QRectF(qRound(tip.rect.x()), qRound(tip.rect.center().y() - iconHeight / 2.0), icon, iconHeight),
                              0, opacity, ctx.projection());
            }
            if (tip.showText && textInfo.texture) {
                const QRectF textRect(qRound(tip.rect.x() + icon + spacing),
                                      qRound(tip.rect.center().y() - textH / 2.0), textW, textH);
                r.drawTexture(textInfo.texture, textRect, 0, opacity, ctx.projection());
            }
        }
    }
}

void WorkspaceBarControl::press(const QPointF &pos)
{
    m_pressed = true;
    m_dragging = false;
    m_pressPos = pos;
    m_pressSlot = deleteBtnAt(pos) < 0 ? slotAt(pos) : -1;
}

void WorkspaceBarControl::move(const QPointF &pos)
{
    if (!m_pressed) {
        if (m_metrics) {
            const bool hover = buttonRect().contains(pos);
            if (hover != m_addHovered) {
                m_addHovered = hover;
                Q_EMIT addHoverChanged(hover);
            }
        }
        const int slot = slotAt(pos);
        if (slot != m_hoveredSlot) {
            m_hoveredSlot = slot;
            Q_EMIT slotHoverChanged(slot);
        }
        return;
    }
    if (dragExceeded(pos) && m_pressSlot >= 0) {
        m_dragging = true;
        Q_EMIT slotDragStarted(m_pressSlot, m_pressPos);
    }
    if (m_dragging) {
        Q_EMIT slotDragMoved(pos);
    }
}

void WorkspaceBarControl::release(const QPointF &pos)
{
    const bool wasPressed = m_pressed;
    const int pressSlot = m_pressSlot;
    const bool dragging = m_dragging;
    clearInputState();
    m_pressSlot = -1;
    if (!wasPressed) {
        return;
    }
    if (dragging) {
        Q_EMIT slotDragFinished(pos);
        return;
    }
    const int delBtn = deleteBtnAt(m_pressPos);
    if (delBtn >= 0 && deleteBtnRect(delBtn).contains(pos)) {
        Q_EMIT deleteClicked(delBtn);
        return;
    }
    if (buttonRect().contains(m_pressPos) && buttonRect().contains(pos)) {
        Q_EMIT addClicked();
        return;
    }
    if (pressSlot >= 0 && slotAt(pos) == pressSlot) {
        Q_EMIT slotClicked(pressSlot);
    }
}

void WorkspaceBarControl::cancel()
{
    GLControl::cancel();
    m_pressSlot = -1;
}

}
