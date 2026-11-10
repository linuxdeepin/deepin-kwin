/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scenemodel.h"
#include "bardragmodel.h"
#include "data/workspacebarmetrics.h"
#include "windowgrid.h"

namespace KWin
{

void SceneModel::syncScreen(const QPointF &origin, const QSizeF &screenSize,
                            const QRectF &fullRect, const QRectF &clientRect,
                            const QVector<QRectF> &absDockAreas)
{
    m_origin = origin;
    m_metrics.setSize(screenSize);
    m_layout = screenLocalLayout(screenLayoutFor(fullRect, clientRect, m_metrics.managerHeight()), fullRect);
    m_dockAreas.clear();
    m_dockAreas.reserve(absDockAreas.size());
    for (const QRectF &d : absDockAreas) {
        m_dockAreas.append(d.translated(-origin));
    }
}

void SceneModel::setHoverCell(int cell)
{
    m_hoverCell = cell;
}

BarItem *SceneModel::itemForKey(const QString &key)
{
    for (BarItem &item : m_barItems) {
        if (item.key == key) {
            return &item;
        }
    }
    return nullptr;
}

void SceneModel::syncBarItems(const QVector<QPair<QString, int>> &order, const QHash<QString, QString> &wallpapers)
{
    QVector<BarItem> next;
    next.reserve(order.size());
    for (const auto &kn : order) {
        BarItem *existing = itemForKey(kn.first);
        if (existing) {
            BarItem kept = *existing;
            kept.num = kn.second;
            kept.uri = wallpapers.value(kn.first);
            next.append(kept);
        } else {
            BarItem fresh;
            fresh.key = kn.first;
            fresh.num = kn.second;
            fresh.uri = wallpapers.value(kn.first);
            next.append(fresh);
        }
    }
    m_barItems = next;
}

void SceneModel::setBarPreview(bool active, const QString &wallpaperUri)
{
    m_barPreviewActive = active;
    m_barPreview.key = QStringLiteral("__preview__");
    m_barPreview.uri = wallpaperUri;
}

void SceneModel::resetBar()
{
    m_barDropRaiseKey.clear();
    m_barPreviewActive = false;
}

void SceneModel::syncWindows(const QVector<EffectWindow *> &windows, const QVector<QRectF> &sources)
{
    QVector<WindowItem> next;
    next.reserve(windows.size());
    for (int i = 0; i < windows.size(); ++i) {
        WindowItem *prev = windowItemFor(windows[i]);
        WindowItem item = prev ? *prev : WindowItem();
        if (prev) {
            item.retiring = false;
        }
        item.cellIndex = i;
        if (!prev) {
            item.setStatic(windows[i], sources.value(i));
        }
        next.append(item);
    }
    for (const WindowItem &item : std::as_const(m_windowItems)) {
        if (item.window && !windows.contains(item.window)) {
            WindowItem retiring = item;
            retiring.retiring = true;
            retiring.cellIndex = -1;
            next.append(retiring);
        }
    }
    m_windowItems = next;
}

void SceneModel::clearWindows()
{
    m_windowItems.clear();
    m_finishedRetiring.clear();
}

void SceneModel::dropWindow(EffectWindow *w)
{
    for (int i = m_windowItems.size() - 1; i >= 0; --i) {
        if (m_windowItems[i].window == w) {
            m_windowItems.removeAt(i);
        }
    }
}

QVector<EffectWindow *> SceneModel::takeFinishedRetiringWindows()
{
    QVector<EffectWindow *> done;
    done.swap(m_finishedRetiring);
    return done;
}

WindowItem *SceneModel::windowItemFor(EffectWindow *w)
{
    for (WindowItem &item : m_windowItems) {
        if (item.window == w) {
            return &item;
        }
    }
    return nullptr;
}

void SceneModel::syncCellItems(const QVector<QRectF> &cells, qreal rowHeight)
{
    m_cellBase = cells;
    m_rowHeight = rowHeight;
}

void SceneModel::setDragPlaceholder(int index, bool active)
{
    m_dragPlaceholder = active ? index : -1;
}

void SceneModel::beginDrag(const QRectF &cell)
{
    m_dragSession.begin(cell);
    m_lastDragPos = QPointF();
}

void SceneModel::dragTo(const QPointF &pos)
{
    m_lastDragPos = pos;
}

void SceneModel::endDrag()
{
    m_dragSession.end();
}

void SceneModel::finishGhostReturn()
{
    m_ghostReturning = false;
    m_dragPlaceholder = -1;
}

void SceneModel::finishRetiringWindow(EffectWindow *w)
{
    for (int i = m_windowItems.size() - 1; i >= 0; --i) {
        WindowItem &item = m_windowItems[i];
        if (item.window == w && item.retiring) {
            m_finishedRetiring.append(w);
            m_windowItems.removeAt(i);
            return;
        }
    }
}

void SceneModel::resetItems()
{
    resetBar();
    m_ghostReturning = false;
    m_dragPlaceholder = -1;
    m_dragSession.end();
    m_lastDragPos = QPointF();
    m_finishedRetiring.clear();
    clearWindows();
}

}
