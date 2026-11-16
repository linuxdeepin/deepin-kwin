/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "data/wallpaperprovider.h"
#include "data/workspacebarmetrics.h"
#include "data/workspacemodel.h"
#include "data/workspacesync.h"
#include "helpers.h"
#include "model/bardraglayout.h"
#include "model/barpredicates.h"
#include "model/keynav.h"
#include "model/screenlayout.h"
#include "model/wallpaperdata.h"
#include "model/windowgrid.h"
#include "model/windowgrouping.h"
#include "multitaskviewex.h"
#include "view/backgroundcontrol.h"
#include "view/dragghostcontrol.h"
#include "view/glpaintrenderer.h"
#include "view/hintcontrol.h"
#include "view/imageprovider.h"
#include "view/layers.h"
#include "view/windowcellcontrol.h"
#include "view/workspacebarcontrol.h"
#include "workspacemanager.h"

#include "effects.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QScreen>
#include <QUuid>
#include <QVariantMap>
#include <QWheelEvent>

#include <algorithm>

namespace KWin
{
namespace
{
constexpr qreal kWindowInfoCanvasMargin = 8.0;
}

void MultitaskViewExEffect::wireGlobalTimers()
{
    if (auto *primary = QGuiApplication::primaryScreen()) {
        connect(primary, &QScreen::logicalDotsPerInchChanged, this, [this]() {
            refreshOsScale();
        });
    }
    m_hoverSwitchTimer.setSingleShot(true);
    m_addDesktopTimer.setInterval(200);
    m_addDesktopTimer.setSingleShot(true);
    connect(&m_addDesktopTimer, &QTimer::timeout, this, [this]() {
        if (m_running) {
            addDesktop();
        }
    });
    connect(&m_hoverSwitchTimer, &QTimer::timeout, this, [this]() {
        const int slot = m_hoverSwitchSlot;
        m_hoverSwitchSlot = -1;
        ScreenView *view = nullptr;
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            if (owned->model.dragActive()) {
                view = owned.get();
                break;
            }
        }
        if (!m_running || !view) {
            return;
        }
        if (view->scene->workspaceBar()->slotAt(view->model.dragPos()) != slot) {
            return;
        }
        const int num = slot >= 0 ? workspaces()->numAt(slot) : 0;
        if (shouldSwitchDesktop(slot, workspaces()->currentNumber(), num)) {
            slideToDesktop(num);
        }
    });
}

void MultitaskViewExEffect::wireAllScreenViews()
{
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        GLScene *s = owned->scene.get();
        s->background()->setImageProvider(m_imgProvider);
        s->workspaceBar()->setImageProvider(m_imgProvider);
        s->workspaceBar()->setDragTipText(i18n("Drag upwards to remove"));
        s->windowOverlay()->setImageProvider(m_imgProvider);
        s->windowInfo()->setImageProvider(m_imgProvider);
        s->emptyHint()->setImageProvider(m_imgProvider);
    }
}

void MultitaskViewExEffect::wireScreenView(ScreenView &view)
{
    connect(view.scene->workspaceBar(), &WorkspaceBarControl::slotHoverChanged, view.scene.get(),
            [this](int slot) {
                effects->addRepaintFull();
                effects->defineCursor(slot >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            });
    connect(view.scene->workspaceBar(), &WorkspaceBarControl::slotClicked, view.scene.get(), [this, &view](int slot) {
        const int num = workspaces()->numAt(slot);
        slideToDesktop(num);
    });
    connect(view.scene->workspaceBar(), &WorkspaceBarControl::addClicked, view.scene.get(), [this, &view]() {
        m_pendingCommitPreview = true;
        addDesktop();
        if (showAddButton(workspaces()->count(), m_config->maxWorkspaces())) {
            QString uri = randomWallpaperPath();
            if (uri.isEmpty()) {
                uri = m_wallpapers->clearPath(workspaces()->currentNumber());
            }
            m_newWorkspaceWallpaper = uri;
            if (m_imgProvider && !uri.isEmpty()) {
                m_imgProvider->predecode({uri},
                                         QSizeF(view.model.metrics().workspaceWidth(),
                                                view.model.metrics().workspaceHeight())
                                             .toSize());
            }
            m_pendingPreviewShow = true;
        } else {
            m_newWorkspaceWallpaper.clear();
            m_pendingPreviewShow = false;
        }
        effects->addRepaintFull();
    });
    connect(view.scene->workspaceBar(), &WorkspaceBarControl::deleteClicked, view.scene.get(), [this, &view](int slot) {
        if (workspaces()->count() <= 1) {
            return;
        }

        removeDesktop(workspaces()->numAt(slot));

        effects->addRepaintFull();
    });
    connect(view.scene->workspaceBar(), &WorkspaceBarControl::addHoverChanged, view.scene.get(), [this, &view](bool hovered) {
        m_addHovered = hovered;
        if (hovered && showAddButton(workspaces()->count(), m_config->maxWorkspaces())) {
            QString uri = randomWallpaperPath();
            if (uri.isEmpty()) {
                uri = m_wallpapers->clearPath(workspaces()->currentNumber());
            }
            m_newWorkspaceWallpaper = uri;
            if (m_imgProvider && !uri.isEmpty()) {
                m_imgProvider->predecode({uri},
                                         QSizeF(view.model.metrics().workspaceWidth(),
                                                view.model.metrics().workspaceHeight())
                                             .toSize());
            }
            view.model.setBarPreview(true, uri);
        } else {
            m_pendingPreviewShow = false;
            view.model.setBarPreview(false, QString());
        }
        effects->addRepaintFull();
    });
    connect(view.scene->workspaceBar(), &WorkspaceBarControl::slotDragStarted, view.scene.get(), [this, &view](int slot, const QPointF &pressPos) {
        QVector<int> order;
        for (int i = 0; i < workspaces()->count(); ++i) {
            order << workspaces()->numAt(i);
        }
        const int id = order.value(slot, -1);
        if (id < 0) {
            return;
        }
        view.model.barDragModel().begin(order, id, slot, pressPos.x(), pressPos.y());
        m_dragView = &view;
        effects->defineCursor(Qt::ClosedHandCursor);
        effects->addRepaintFull();
    });
    connect(view.scene->workspaceBar(), &WorkspaceBarControl::slotDragMoved, view.scene.get(), [this, &view](const QPointF &pos) {
        if (!view.model.barDragModel().isActive()) {
            return;
        }
        view.model.barDragModel().movePointer(pos.x(), pos.y());
        effects->addRepaintFull();
    });
    connect(view.scene->workspaceBar(), &WorkspaceBarControl::slotDragFinished, view.scene.get(), [this, &view](const QPointF &pos) {
        Q_UNUSED(pos);
        finishBarDrag();
    });
    connect(view.scene->windowCells(), &WindowCellControl::cellClicked, view.scene.get(), [this, &view](int index) {
        if (index >= 0 && index < view.model.windows().size()) {
            if (EffectWindow *w = view.model.windows().value(index)) {
                m_selectedWindow = w;
                requestExit();
            }
        }
    });
    connect(view.scene->windowOverlay(), &WindowOverlayControl::closeClicked, view.scene.get(), [this, &view](int index) {
        if (EffectWindow *w = overlayWindowAt(view, index)) {
            w->closeWindow();
            effects->addRepaintFull();
        }
    });
    connect(view.scene->windowOverlay(), &WindowOverlayControl::topClicked, view.scene.get(), [this, &view](int index) {
        if (EffectWindow *w = overlayWindowAt(view, index)) {
            if (auto *impl = static_cast<EffectWindowImpl *>(w)) {
                if (Window *win = impl->window()) {
                    win->setKeepAbove(!w->keepAbove());
                }
            }
            effects->addRepaintFull();
        }
    });
    connect(view.scene->windowCells(), &WindowCellControl::cellDragStarted, view.scene.get(), [this, &view](int index) {
        WindowItem *dragItem = (index >= 0 && index < view.model.windows().size())
            ? view.model.windowItemFor(view.model.windows().value(index))
            : nullptr;
        if (dragItem) {
            view.model.setDraggedWindow(view.model.windows().value(index));
            view.model.beginDrag(view.model.baseCells().value(index));
            m_hoverSwitchTimer.stop();
            m_hoverSwitchSlot = -1;
            const QRectF src = dragItem->source;
            view.scene->ghostLayer().setSource(view.model.draggedWindow(), src);
            const QRectF cell = view.model.baseCells().value(index);
            view.scene->ghostLayer().snap(cell, 0.6);
            view.model.setDragPlaceholder(index, true);
        }
    });
    connect(view.scene->windowCells(), &WindowCellControl::cellDragMoved, view.scene.get(), [this, &view](const QPointF &pos) {
        view.model.dragTo(pos);
        const int hoverSlot = view.scene->workspaceBar()->slotAt(pos);
        if (hoverSlot != m_hoverSwitchSlot) {
            m_hoverSwitchSlot = hoverSlot;
            const int hoverNum = hoverSlot >= 0 ? workspaces()->numAt(hoverSlot) : 0;
            if (shouldSwitchDesktop(hoverSlot, workspaces()->currentNumber(), hoverNum)) {
                m_hoverSwitchTimer.start(kDragHoverSwitchMs);
            } else {
                m_hoverSwitchTimer.stop();
            }
        }
        const QRectF cell = view.model.dragCell();
        const qreal barBottom = view.model.layout().gridArea.y();
        const qreal itemW = view.model.metrics().workspaceWidth();
        const qreal itemH = view.model.metrics().workspaceHeight();
        QRectF target;
        const int ghostDur = resolveMs(kGhostSnapMs, m_config->motion().animationSpeedMultiplier);
        int dur = ghostDur;
        const bool overEdge = pos.y() <= barBottom;
        if (overEdge) {
            const qreal k = dragGhostFitScale(cell, itemW * 0.5, itemH * 0.5);
            target = dragGhostRect(cell, pos, qMax<qreal>(0.02, k));
            dur = ghostDur;
        } else {
            const qreal k = dragGhostScale(pos.y(), barBottom);
            target = dragGhostRect(cell, pos, k);
        }
        view.scene->ghostLayer().setTarget(target, 0.6, dur, Easing::OutCubic);
        effects->addRepaintFull();
    });
    connect(view.scene->windowCells(), &WindowCellControl::cellDragFinished, view.scene.get(), [this, &view](const QPointF &pos) {
        if (!view.model.draggedWindow())
            return;
        m_hoverSwitchTimer.stop();
        m_hoverSwitchSlot = -1;
        const int slot = view.scene->workspaceBar()->slotAt(pos);
        const QRectF addBtn = view.scene->workspaceBar()->buttonRect();
        const DropTarget target = dropTargetFor(slot, addBtn.contains(pos),
                                                showAddButton(workspaces()->count(), m_config->maxWorkspaces()));
        bool movedAway = false;
        if (target == DropTarget::DesktopItem) {
            const int num = workspaces()->numAt(slot);
            if (num > 0) {
                if (auto *impl = static_cast<EffectWindowImpl *>(view.model.draggedWindow())) {
                    if (Window *win = impl->window()) {
                        win->setDesktop(num);
                        movedAway = true;
                    }
                }
            }
        } else if (target == DropTarget::AddButton) {
            EffectWindow *moved = view.model.draggedWindow();
            addDesktop();
            if (moved) {
                const int newNum = int(VirtualDesktopManager::self()->count());
                if (auto *impl = static_cast<EffectWindowImpl *>(moved)) {
                    if (Window *win = impl->window()) {
                        win->setDesktop(newNum);
                        movedAway = true;
                    }
                }
            }
        }
        if (movedAway && view.model.draggedWindow()) {
            view.model.dropWindow(view.model.draggedWindow());
        }
        syncSessionItems(0.0);
        const QRectF ghostNow = view.scene->ghostLayer().rect();
        const QRectF cell = view.model.dragCell();
        const GhostDrop drop = ghostDropBehavior(movedAway, ghostNow, cell);
        view.scene->ghostLayer().setTarget(drop.rect, drop.opacity, resolveMs(drop.durationMs, m_config->motion().animationSpeedMultiplier), Easing::OutCubic);
        view.model.setGhostReturning(true);
        view.model.setDraggedWindow(nullptr);
        view.model.endDrag();
        effects->addRepaintFull();
    });
    connect(view.scene.get(), &GLScene::blankClicked, view.scene.get(), [this, &view]() {
        requestExit();
    });
}

ScreenView *MultitaskViewExEffect::ensureScreenView(EffectScreen *screen)
{
    ScreenView *view = m_views.ensure(screen);
    if (view) {
        view->model.setMaxWorkspaces(m_config->maxWorkspaces());
        view->model.setMotionOptions(m_config->motion());
        wireScreenView(*view);
    }
    return view;
}

void MultitaskViewExEffect::syncViews()
{
    const QList<EffectScreen *> screens = effects->screens();
    for (int i = m_views.count() - 1; i >= 0; --i) {
        ScreenView *view = m_views.at(i);
        if (!view || screens.contains(view->model.screen())) {
            continue;
        }
        if (m_dockPressView == view) {
            m_dockPressButton = Qt::NoButton;
            m_dockPressView = nullptr;
        }
        if (m_dragView == view) {
            m_dragView = nullptr;
            if (m_running) {
                effects->defineCursor(Qt::ArrowCursor);
            }
        }
        if (m_running) {
            restoreWindowRoles();
            releaseBackground(*view);
        }
        m_views.remove(view->model.screen());
    }
    for (EffectScreen *screen : screens) {
        if (screen && !m_views.find(screen)) {
            ensureScreenView(screen);
        }
    }
    if (m_running) {
        syncThumbPins();
    }
}

QVector<ScreenView *> MultitaskViewExEffect::paintTargetViews(const QRectF &renderTargetRect) const
{
    QVector<QRectF> rects;
    const auto &all = m_views.all();
    rects.reserve(int(all.size()));
    for (const std::unique_ptr<ScreenView> &owned : all) {
        rects.append(owned->model.layout().fullRect.translated(owned->model.origin()));
    }
    QVector<ScreenView *> out;
    for (int index : paintTargets(rects, renderTargetRect)) {
        out.append(all.at(size_t(index)).get());
    }
    return out;
}

void MultitaskViewExEffect::applyViewGeometry(ScreenView &view)
{
    const QRectF canvas(0, 0, view.model.layout().fullRect.width(), view.model.layout().fullRect.height());
    view.scene->background()->setGeometry(canvas);
    view.scene->workspaceBar()->setGeometry(canvas);
    view.scene->windowCells()->setGeometry(canvas);
    view.scene->windowCells()->setClipRect(view.model.layout().gridArea);
    view.scene->emptyHint()->setGeometry(view.model.layout().gridArea);
    view.scene->emptyHint()->setAnchor(view.model.layout().gridArea.center());
    const qreal margin = kWindowInfoCanvasMargin * view.model.osScale();
    view.scene->windowInfo()->setGeometry(canvas.adjusted(margin, margin, -margin, -margin));
}

ScreenView *MultitaskViewExEffect::viewAtGlobal(const QPointF &globalPos) const
{
    EffectScreen *screen = effects->screenAt(globalPos.toPoint());
    return screen ? m_views.find(screen) : nullptr;
}

ScreenView *MultitaskViewExEffect::viewWithActivePress() const
{
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        if (owned->model.dragActive() || owned->model.barDragModel().isActive() || owned->scene->hasPressedControl()) {
            return owned.get();
        }
    }
    return nullptr;
}

bool MultitaskViewExEffect::inDockArea(const QPointF &pos) const
{
    ScreenView *view = viewAtGlobal(pos);
    return view && pointerBelongsToDock(view->model.dockAreas(), pos - view->model.origin());
}

void MultitaskViewExEffect::acquireBackground(const QString &uri)
{
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        acquireBackground(*owned, uri);
    }
}

void MultitaskViewExEffect::acquireBackground(ScreenView &view, const QString &uri)
{
    if (uri.isEmpty()) {
        releaseBackground(view);
        return;
    }
    const QSize bgSize = view.scene->background()->geometry().size().toSize();
    if (uri == view.acquiredBgUri && bgSize == view.acquiredBgSize) {
        return;
    }
    releaseBackground(view);
    if (m_imgProvider) {
        m_imgProvider->acquireTexture(uri, bgSize);
    }
    view.acquiredBgUri = uri;
    view.acquiredBgSize = bgSize;
}

void MultitaskViewExEffect::releaseBackground(ScreenView &view)
{
    if (view.acquiredBgUri.isEmpty()) {
        return;
    }
    if (m_imgProvider) {
        m_imgProvider->releaseTexture(view.acquiredBgUri, view.acquiredBgSize);
    }
    view.acquiredBgUri.clear();
    view.acquiredBgSize = QSize();
}

void MultitaskViewExEffect::syncSessionItems(qreal windowEnterOffsetX)
{
    auto *vds = VirtualDesktopManager::self();
    const int currentNum = vds && vds->currentDesktop() ? int(vds->currentDesktop()->x11DesktopNumber()) : 1;
    m_sessionDesktop = sessionBaseDesktop(m_sessionDesktop, currentNum);

    const QList<EffectScreen *> screens = effects->screens();
    QVector<EffectWindow *> candidates;
    QVector<int> candidateScreen;
    QHash<EffectScreen *, QVector<QRectF>> docks;
    for (EffectWindow *w : effects->stackingOrder()) {
        if (windowLayerOf(w->isDesktop(), w->isDock(), w->isNotification(), w->isOutline()) == WindowLayer::Dock) {
            docks[w->screen()].append(w->frameGeometry());
        }
        if (w->isDock() || w->isDesktop() || w->isNotification() || w->isOutline()
            || w->isSplash() || w->isSpecialWindow()) {
            continue;
        }
        if (!w->isOnCurrentActivity()) {
            continue;
        }
        if (m_closing.contains(w)) {
            continue;
        }
        if (w->desktop() != currentNum) {
            continue;
        }
        const int si = screens.indexOf(w->screen());
        if (si < 0) {
            continue;
        }
        candidates.append(w);
        candidateScreen.append(si);
    }
    const QVector<QVector<int>> perScreen = groupIndicesByScreen(screens.size(), candidateScreen);

    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        const int si = screens.indexOf(owned->model.screen());
        const QVector<int> &group = (si >= 0 && si < perScreen.size()) ? perScreen.at(si) : QVector<int>();
        QVector<EffectWindow *> wins;
        wins.reserve(group.size());
        for (int index : group) {
            wins.append(candidates.at(index));
        }
        syncScreenItems(*owned, wins, docks.value(owned->model.screen()), windowEnterOffsetX);
    }
}

void MultitaskViewExEffect::syncScreenItems(ScreenView &view, const QVector<EffectWindow *> &wins,
                                                const QVector<QRectF> &absDocks, qreal windowEnterOffsetX)
{
    const QRectF fullRect = effects->clientArea(MaximizeFullArea, view.model.screen(), m_sessionDesktop);
    const QRectF screenGeo = effects->clientArea(MaximizeArea, view.model.screen(), m_sessionDesktop);
    const QPointF origin = fullRect.topLeft();
    view.model.syncScreen(origin, screenGeo.size(), fullRect, screenGeo, absDocks);

    for (EffectWindow *w : wins) {
        if (w && !m_gridMinimizeRefs.contains(w)) {
            m_gridMinimizeRefs.insert(w, EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_MINIMIZE));
        }
    }
    for (auto it = m_gridMinimizeRefs.begin(); it != m_gridMinimizeRefs.end();) {
        if (!wins.contains(it.key())) {
            it = m_gridMinimizeRefs.erase(it);
        } else {
            ++it;
        }
    }

    const QRectF gridArea = view.model.layout().gridArea;
    QVector<QSize> naturals;
    naturals.reserve(wins.size());
    for (EffectWindow *w : wins) {
        naturals.append(QSize(w->width(), w->height()));
    }
    qreal scaleH = 0;
    const QVector<QRectF> cells = layoutWindowGrid(naturals, gridArea, view.model.layout().fullRect.height(),
                                                   view.model.metrics().spacingHeight(), view.model.metrics().spacingWidth(),
                                                   &scaleH);
    qreal maxScroll = 0;
    if (!cells.isEmpty()) {
        qreal maxY = cells.first().bottom();
        for (const QRectF &c : cells) {
            maxY = qMax(maxY, c.bottom());
        }
        maxScroll = qMax<qreal>(0.0, maxY - gridArea.y() - gridArea.height());
    }
    view.scene->setGridMaxScroll(maxScroll);
    view.scene->setGridScrollTarget(qBound<qreal>(0.0, view.scene->gridScrollTarget(), maxScroll));

    view.model.windows().clear();
    QVector<QRectF> sources;
    sources.reserve(wins.size());
    for (int i = 0; i < wins.size(); ++i) {
        EffectWindow *w = wins[i];
        sources.append(QRectF(w->x(), w->y(), w->width(), w->height()).translated(-origin));
        view.model.windows().append(w);
    }
    view.model.syncCellItems(cells, scaleH);
    m_roles.apply(view);
    view.model.syncWindows(view.model.windows(), sources);
    view.scene->syncWindowLayer(windowEnterOffsetX);
    applyViewGeometry(view);
}

void MultitaskViewExEffect::refreshOsScale()
{
    const qreal s = effectsEx ? qreal(effectsEx->getOsScale()) : 1.0;
    if (qFuzzyCompare(s, m_osScale)) {
        return;
    }
    m_osScale = s;
    m_roles.setOsScale(s);
    qCDebug(KWIN_MTVEX) << "osScale changed:" << s;
    effects->addRepaintFull();
}

void MultitaskViewExEffect::setupSession(qreal windowEnterOffsetX)
{
    refreshOsScale();
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        owned->model.setOsScale(m_osScale);
    }
    syncSessionItems(windowEnterOffsetX);
    syncBarModel();
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        applyViewGeometry(*owned);
    }
    if (m_imgProvider) {
        const QStringList allUris = m_wallpapers->allUris();
        QStringList preload;
        preload.reserve(allUris.size());
        for (const QString &uri : allUris) {
            preload.append(uri);
            for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
                m_imgProvider->pin(uri, owned->scene->background()->geometry().size().toSize());
            }
        }
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            m_imgProvider->predecode(preload, owned->scene->background()->geometry().size().toSize());
        }
    }
    const int currentNum = workspaces() ? workspaces()->currentNumber() : 1;
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        const QString wp = m_wallpapers->backgroundFor(currentNum, owned->model.screen());
        acquireBackground(*owned, wp);
        owned->model.backgroundItem().setWallpaper(wp);
    }
}

void MultitaskViewExEffect::teardownSession()
{
    if (m_imgProvider) {
        const int currentNum = workspaces() ? workspaces()->currentNumber() : -1;
        for (const QString &uri : m_wallpapers->allUris(currentNum)) {
            for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
                m_imgProvider->unpin(uri, owned->scene->background()->geometry().size().toSize());
            }
        }
        m_trimPending = true;
    }
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        releaseBackground(*owned);
    }
    m_closing.clear();
    m_visibleRefs.clear();
    m_gridMinimizeRefs.clear();
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        owned->model.syncWindows({}, {});
        owned->model.windows().clear();
        owned->model.setDraggedWindow(nullptr);
        owned->model.setHoverCell(-1);
    }
}

void MultitaskViewExEffect::slideToDesktop(int num, int fromOverride)
{
    const int from = (fromOverride > 0) ? fromOverride : workspaces()->currentNumber();
    if (num <= 0 || num == from) {
        return;
    }
    m_inSlideTransition = true;
    const QString fromWall = m_wallpapers->backgroundFor(from);
    const QString toWall = m_wallpapers->backgroundFor(num);
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        owned->scene->startSlide(fromWall, toWall, num > from ? 1 : -1);
    }
    auto *vds = VirtualDesktopManager::self();
    if (vds) {
        if (VirtualDesktop *d = vds->desktopForX11Id(num)) {
            qreal slideOffset = 0.0;
            for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
                slideOffset = (num > from ? 1.0 : -1.0) * owned->model.metrics().size().width();
                break;
            }
            for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
                QVector<OutgoingWindowInput> wins;
                wins.reserve(owned->model.windowItems().size());
                for (const WindowItem &it : owned->model.windowItems()) {
                    if (it.retiring || !it.window) {
                        continue;
                    }
                    const WindowNode *n = owned->scene->windowItemLayer().nodeFor(it.window);
                    OutgoingWindowInput wi;
                    wi.window = it.window;
                    wi.source = it.source;
                    wi.cur = n ? n->rect() : it.source;
                    wi.op = n ? n->opacity() : 1.0;
                    wins.append(wi);
                }
                owned->scene->outgoingLayer().snapshot(slideOffset, wins, owned->model.cellBase(),
                                                       owned->model.rowHeight(), owned->model.osScale(),
                                                       resolveMs(kOutgoingSlideMs, m_config->motion().animationSpeedMultiplier));
                for (const OutgoingGrid &snap : owned->scene->outgoingLayer().grids()) {
                    for (const OutgoingWindow &gw : snap.windows) {
                        EffectWindow *w = gw.window;
                        if (w && !m_visibleRefs.contains(w)) {
                            m_visibleRefs.insert(w, EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_MINIMIZE | EffectWindow::PAINT_DISABLED_BY_DESKTOP));
                        }
                    }
                }
            }
            m_workspaces->switchToDesktop(d->id());
            for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
                if (owned->model.dragActive()) {
                    owned->model.setDragPlaceholder(-1, false);
                }
            }
            syncSessionItems(slideOffset);
            syncBarModel();
            for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
                const QString wp = m_wallpapers->backgroundFor(num, owned->model.screen());
                acquireBackground(*owned, wp);
                owned->model.backgroundItem().setWallpaper(wp);
            }
        } else {
        }
    }
    m_inSlideTransition = false;
    effects->addRepaintFull();
}

void MultitaskViewExEffect::slideByWheel(int wheelY)
{
    if (wheelY == 0) {
        return;
    }
    static QElapsedTimer wheelTimer;
    static bool wheelStarted = false;
    if (wheelStarted && wheelTimer.elapsed() < 400) {
        return;
    }
    if (!wheelStarted) {
        wheelTimer.start();
        wheelStarted = true;
    }
    const int count = workspaces()->count();
    const int cur = workspaces()->currentNumber();
    int target = (wheelY < 0) ? cur + 1 : cur - 1;
    if (target < 1) {
        target = count;
    }
    if (target > count) {
        target = 1;
    }
    slideToDesktop(target);
    wheelTimer.restart();
}

void MultitaskViewExEffect::gridScrollBy(ScreenView &view, qreal angleDeltaY)
{
    view.scene->setGridScrollTarget(qBound<qreal>(0.0, view.scene->gridScrollTarget() - angleDeltaY / 120.0 * 60.0, view.scene->gridMaxScroll()));
    effects->addRepaintFull();
}

void MultitaskViewExEffect::onWindowClosed(EffectWindow *w)
{
    if (!m_running || !w || m_closing.contains(w)) {
        return;
    }
    ScreenView *view = nullptr;
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        if (owned->model.windowItemFor(w)) {
            view = owned.get();
            break;
        }
    }
    if (!view) {
        return;
    }
    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void *>(this)));
    ClosingWindow closing;
    closing.window = w;
    closing.deletedRef = EffectWindowDeletedRef(w);
    closing.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_DELETE);
    m_closing.insert(w, closing);
    EffectWindow *hovered = view->model.windows().value(view->model.hoverCell(), nullptr);
    view->model.windows().removeAll(w);
    if (view->model.draggedWindow() == w) {
        view->model.setDraggedWindow(nullptr);
    }
    if (m_selectedWindow == w) {
        m_selectedWindow = nullptr;
    }
    syncSessionItems();
    view->model.setHoverCell(view->model.windows().indexOf(hovered));
    view->scene->syncWindowLayer();
    effects->addRepaintFull();
}

void MultitaskViewExEffect::onWindowDeleted(EffectWindow *w)
{
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        owned->model.dropWindow(w);
        owned->scene->outgoingLayer().removeWindow(w);
        EffectWindow *hovered = owned->model.windows().value(owned->model.hoverCell(), nullptr);
        if (owned->model.windows().removeAll(w) > 0) {
            if (owned->model.draggedWindow() == w) {
                owned->model.setDraggedWindow(nullptr);
            }
            owned->model.setHoverCell(owned->model.windows().indexOf(hovered));
            owned->scene->syncWindowLayer();
        }
    }
    m_closing.remove(w);
}

}
