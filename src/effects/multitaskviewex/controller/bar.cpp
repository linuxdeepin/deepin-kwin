/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "data/workspacebarmetrics.h"
#include "data/workspacemodel.h"
#include "effectconfig.h"
#include "helpers.h"
#include "model/bardragmodel.h"
#include "model/scenemodel.h"
#include "model/windowitem.h"
#include "multitaskviewex.h"
#include "view/imageprovider.h"
#include "view/screenview.h"
#include "view/workspacebarcontrol.h"
#include "wallpapermanager.h"
#include "workspacemanager.h"

#include "effects.h"
#include "virtualdesktops.h"

#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

namespace KWin
{

void MultitaskViewExEffect::finishBarDragIfButtonReleased()
{
    if (dragView() && dragView()->model.barDragModel().isActive() && !leftButtonPhysicallyDown()
        && !inDockArea(effects->cursorPos())) {
        finishBarDrag();
    }
}

void MultitaskViewExEffect::cancelBarDrag(ScreenView &view)
{
    if (!view.model.barDragModel().isActive()) {
        return;
    }
    view.model.barDragModel().end();
    view.scene->workspaceBar()->resetInput();
    view.scene->cancelPress();
    if (m_dragView == &view) {
        m_dragView = nullptr;
    }
    effects->defineCursor(Qt::ArrowCursor);
    effects->addRepaintFull();
}

void MultitaskViewExEffect::finishBarDrag()
{
    if (!dragView() || !dragView()->model.barDragModel().isActive()) {
        return;
    }
    ScreenView *dv = dragView();
    const int id = dv->model.barDragModel().draggedId();
    const bool remove = dv->model.barDragModel().dragDelete() && workspaces()->count() > 1;
    dv->model.barDragModel().end();
    dv->scene->workspaceBar()->resetInput();
    m_dragView = nullptr;

    if (remove) {
        removeDesktop(id);
    } else {
        for (const BarItem &it : dv->model.barItems()) {
            if (it.num == id) {
                dv->model.noteBarDropFinished(it.key);
                break;
            }
        }
        applyBarOrder(*dv);
    }
    effects->defineCursor(Qt::ArrowCursor);
    effects->addRepaintFull();
}

QVector<int> MultitaskViewExEffect::barOrder() const
{
    if (dragView() && dragView()->model.barDragModel().isActive()) {
        return dragView()->model.barDragModel().order();
    }
    QVector<int> order;
    order.reserve(workspaces()->count());
    for (int i = 0; i < workspaces()->count(); ++i) {
        order.append(workspaces()->numAt(i));
    }
    return order;
}

void MultitaskViewExEffect::syncBarModel()
{
    if (m_pendingCommitPreview) {
        m_pendingCommitPreview = false;
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            owned->model.setBarPreview(false, QString());
        }
    }
    QVector<QPair<QString, int>> order;
    for (int i = 0; i < workspaces()->count(); ++i) {
        order.append({workspaces()->idAt(i), workspaces()->numAt(i)});
    }
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        ScreenView &v = *owned;
        QHash<QString, QString> uris;
        for (int i = 0; i < workspaces()->count(); ++i) {
            const QString key = workspaces()->idAt(i);
            const int num = workspaces()->numAt(i);
            const QString uri = m_wallpapers->clearUri(num, v.model.screen());
            uris.insert(key, uri);
        }
        WorkspaceBarDragModel::Config dc;
        dc.workspaceWidth = v.model.metrics().workspaceWidth();
        dc.workspaceHeight = v.model.metrics().workspaceHeight();
        dc.spacing = v.model.metrics().workspacingWidth();
        dc.listX = v.model.metrics().itemX(0, workspaces()->count()) + v.model.layout().offX;
        v.model.barDragModel().setConfig(dc);
        v.model.syncBarItems(order, uris);
        v.scene->resetBarScroll();
        v.scene->syncBarLayer(barOrder(), workspaces()->currentNumber(),
                              v.scene->workspaceBar()->hoveredSlot());
    }
    syncThumbPins();
}

void MultitaskViewExEffect::syncThumbPins()
{
    if (!m_imgProvider) {
        return;
    }
    QHash<QString, QPair<QString, QSize>> want;
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        const QSize cell = owned->scene->workspaceBar()->thumbCellSize();
        for (const BarItem &it : owned->model.barItems()) {
            if (!it.uri.isEmpty()) {
                want.insert(ImageProvider::textureKeyFor(it.uri, cell), {it.uri, cell});
            }
        }
    }
    for (auto it = m_thumbPins.constBegin(); it != m_thumbPins.constEnd(); ++it) {
        if (!want.contains(it.key())) {
            m_imgProvider->unpin(it.value().first, it.value().second);
        }
    }
    for (auto it = want.constBegin(); it != want.constEnd(); ++it) {
        if (!m_thumbPins.contains(it.key())) {
            m_imgProvider->pin(it.value().first, it.value().second);
        }
    }
    m_thumbPins = want;
}

void MultitaskViewExEffect::applyBarOrder(ScreenView &view)
{
    m_workspaces->applyOrder(view.model.barDragModel().order());
    if (m_running) {
        reapplyBackgrounds();
    }
    syncBarModel();
}

void MultitaskViewExEffect::addDesktop()
{
    if (!m_workspaces->addDesktop(m_newWorkspaceWallpaper, m_config->maxWorkspaces())) {
        return;
    }
    m_newWorkspaceWallpaper.clear();
    syncBarModel();
}

void MultitaskViewExEffect::removeDesktop(int desktopNumber)
{
    m_workspaces->removeDesktop(desktopNumber);
    if (m_running) {
        syncSessionItems();
    }
    syncBarModel();
    effects->addRepaintFull();
}

void MultitaskViewExEffect::onVirtualDesktopsChanged()
{
    if (m_workspaces->ownsDesktopMutation() || !workspaces()) {
        return;
    }
    refreshModel();
    if (m_running) {
        syncBarModel();
        effects->addRepaintFull();
    }
    warmWallpapers();
}

void MultitaskViewExEffect::refreshModel()
{
    m_workspaces->refresh();
}

}
