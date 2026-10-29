/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "multitaskviewex.h"
#include "data/workspacemodel.h"
#include "effectconfig.h"
#include "helpers.h"
#include "model/bardraglayout.h"
#include "model/barpredicates.h"
#include "model/keynav.h"
#include "view/backgroundcontrol.h"
#include "view/glpaintrenderer.h"
#include "view/imageprovider.h"
#include "view/layers.h"
#include "view/windowcellcontrol.h"
#include "view/workspacebarcontrol.h"
#include "wallpapermanager.h"
#include "workspacemanager.h"

#include "effects.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QDBusConnection>
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

static void ensureResources()
{
    Q_INIT_RESOURCE(multitaskviewex);
}

namespace KWin
{

Q_LOGGING_CATEGORY(KWIN_MTVEX, "kwin_effects.multitaskviewex", QtInfoMsg)

MultitaskViewExEffect::MultitaskViewExEffect()
{
    ensureResources();
    m_toggleAction = new QAction(this);
    m_toggleAction->setObjectName(QStringLiteral("ShowMultitasking"));
    m_toggleAction->setText(i18n("Show Multitasking View"));
    m_toggleShortcut = QList<QKeySequence>() << (Qt::META + Qt::Key_S);
    KGlobalAccel::self()->setDefaultShortcut(m_toggleAction, m_toggleShortcut);
    KGlobalAccel::self()->setShortcut(m_toggleAction, m_toggleShortcut);
    m_toggleShortcut = KGlobalAccel::self()->shortcut(m_toggleAction);
    connect(m_toggleAction, &QAction::triggered, this, &MultitaskViewExEffect::toggle);
    m_borderActivate = QList<ElectricBorder>{ElectricTop};
    m_config = new EffectConfig(this);
    m_config->init();
    m_config->reload();
    connect(m_config, &EffectConfig::changed, this, &MultitaskViewExEffect::applyConfigToViews);

    m_wallpapers = new WallpaperManager(this);
    connect(m_wallpapers, &WallpaperManager::refreshed, this, &MultitaskViewExEffect::onWallpapersRefreshed);

    m_workspaces = new WorkspaceManager(m_wallpapers, this);
    m_imgProvider = new ImageProvider(this);
    connect(m_imgProvider, &ImageProvider::predecoded, this, [this](const QString &) {
        if (m_running) {
            effects->addRepaintFull();
        }
    });
    syncViews();
    wireAllScreenViews();
    wireGlobalTimers();

    connect(effects, &EffectsHandler::windowClosed, this, &MultitaskViewExEffect::onWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &MultitaskViewExEffect::onWindowDeleted);

    if (auto *vds = VirtualDesktopManager::self()) {
        connect(vds, &VirtualDesktopManager::desktopCreated, this,
                [this](VirtualDesktop *) {
                    onVirtualDesktopsChanged();
                });
        connect(vds, &VirtualDesktopManager::desktopRemoved, this,
                [this](VirtualDesktop *) {
                    onVirtualDesktopsChanged();
                });
        connect(vds, &VirtualDesktopManager::currentChanged, this,
                [this](uint from, uint to) {
                    if (m_running && !m_inSlideTransition && !m_workspaces->ownsDesktopMutation()) {
                        slideToDesktop(int(to), int(from));
                    } else {
                        onVirtualDesktopsChanged();
                    }
                });
        connect(vds, &VirtualDesktopManager::countChanged, this,
                [this](uint, uint) {
                    onVirtualDesktopsChanged();
                });
    }

    QDBusConnection::sessionBus().registerObject("/MultitaskViewEx", this,
                                                 QDBusConnection::ExportScriptableContents);
    QTimer::singleShot(0, this, [this]() {
        warmWallpapers();
    });
}

MultitaskViewExEffect::~MultitaskViewExEffect()
{
    if (m_running) {
        restoreWindowRoles();
        effects->setActiveFullScreenEffect(nullptr);
        effects->stopMouseInterception(this);
    }
    m_hasKeyboardGrab = false;

    if (m_imgProvider) {
        effects->makeOpenGLContextCurrent();
        m_imgProvider->invalidate();
    }
}

void MultitaskViewExEffect::activate()
{
    setRunning(true);
}

void MultitaskViewExEffect::requestExit()
{
    if (m_exitRequested) {
        return;
    }
    m_exitRequested = true;
    elevateForExit();
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        owned->scene->setEntered(false);
    }
    effects->addRepaintFull();
}

void MultitaskViewExEffect::toggle()
{
    if (!m_running) {
        setRunning(true);
        return;
    }
    if (m_exitRequested) {
        m_exitRequested = false;
        clearElevatedWindow();
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            owned->scene->setEntered(true);
        }
    } else {
        m_exitRequested = true;
        elevateForExit();
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            owned->scene->setEntered(false);
        }
    }
    effects->addRepaintFull();
}

void MultitaskViewExEffect::elevateForExit()
{
    clearElevatedWindow();
    EffectWindow *selected = m_selectedWindow;
    EffectWindow *active = effects->activeWindow();
    const auto inAnyView = [this](EffectWindow *w) {
        if (!w) {
            return false;
        }
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            if (owned->model.windows().contains(w)) {
                return true;
            }
        }
        return false;
    };
    m_elevation.request(selected, inAnyView(selected), active, inAnyView(active));
    if (EffectWindow *w = m_elevation.elevated()) {
        effects->setElevatedWindow(w, true);
    }
}

void MultitaskViewExEffect::clearElevatedWindow()
{
    if (EffectWindow *w = m_elevation.elevated()) {
        effects->setElevatedWindow(w, false);
    }
    m_elevation.finish();
}

bool MultitaskViewExEffect::viewActive() const
{
    return m_running;
}

void MultitaskViewExEffect::stop()
{
    setRunning(false);
}

void MultitaskViewExEffect::setRunning(bool run)
{
    clearElevatedWindow();
    m_hoverSwitchTimer.stop();
    m_hoverSwitchSlot = -1;
    m_selectedWindow = nullptr;
    if (m_running == run) {
        return;
    }
    m_running = run;
    if (run) {
        syncViews();
        m_exitRequested = false;
        m_newWorkspaceWallpaper.clear();
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            owned->scene->resetSession();
        }
        refreshModel();
        setupSession();
        warmWallpapers();
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            owned->scene->setEntered(true);
            owned->frameClock.reset();
        }
        effects->setActiveFullScreenEffect(this);
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        m_hasKeyboardGrab = effects->grabKeyboard(this);
    } else {
        m_exitRequested = false;
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            owned->scene->resetSession();
        }
        if (m_hasKeyboardGrab) {
            effects->ungrabKeyboard();
            m_hasKeyboardGrab = false;
        }
        effects->setActiveFullScreenEffect(nullptr);
        effects->stopMouseInterception(this);
        m_teardownPending = true;
        syncViews();
    }
    Q_EMIT stateChanged();
    effects->addRepaintFull();
}

void MultitaskViewExEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (!m_running) {
        effects->prePaintScreen(data, presentTime);
        return;
    }
    {
        const auto now = presentTime;
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            ScreenView &v = *owned;
            const int delta = v.frameClock.advance(now);
            v.scene->syncBarLayer(barOrder(), workspaces()->currentNumber(),
                                  v.scene->workspaceBar()->hoveredSlot());
            v.scene->syncWindowLayer();
            v.scene->advance(delta);
            for (const WindowNode &n : v.scene->windowItemLayer().nodes()) {
                if (n.retiring && !n.animating() && n.opacity() <= 0.01) {
                    v.model.finishRetiringWindow(n.window);
                }
            }
            for (EffectWindow *w : v.model.takeFinishedRetiringWindows()) {
                m_closing.remove(w);
            }
            for (EffectWindow *w : v.scene->outgoingLayer().takeFinished()) {
                m_visibleRefs.remove(w);
                m_roles.restore(*w);
            }
        }
    }
    data.mask |= Effect::PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    data.mask |= Effect::PAINT_SCREEN_BACKGROUND_FIRST;
    const auto &views = m_views.all();
    bool opaqueBackground = !views.empty() && !m_exitRequested;
    for (const std::unique_ptr<ScreenView> &owned : views) {
        if (!owned->scene->entered() || owned->scene->enterAnimRunning()) {
            opaqueBackground = false;
            break;
        }
    }
    if (opaqueBackground) {
        data.mask |= Effect::PAINT_SCREEN_NO_BACKGROUND;
    }
    const EffectWindowList stacking = effects->stackingOrder();
    if (m_config->useBlurEffect()) {
        for (EffectWindow *w : stacking) {
            w->setData(WindowForceBlurRole, QVariant(true));
        }
    }
    effects->prePaintScreen(data, presentTime);
}

namespace
{
class KWinWindowPainter : public WindowPainter
{
public:
    KWinWindowPainter(int mask, const QRegion &region, const ScreenPaintData &data)
        : m_mask(mask)
        , m_region(region)
        , m_data(data)
    {
    }

    void paintWindow(EffectWindow *w, const QRectF &from, const QRectF &to, qreal opacity) const override
    {
        WindowPaintData d(m_data.projectionMatrix());
        d.setOpacity(opacity);
        d *= QVector2D(float(to.width() / from.width()), float(to.height() / from.height()));
        d += QPoint(int(to.x() - from.x()), int(to.y() - from.y()));
        effects->drawWindow(w, m_mask | Effect::PAINT_WINDOW_TRANSFORMED,
                            QRegion(QRect(-1, -1, 0x7FFFFFFF, 0x7FFFFFFF)), d);
    }

private:
    int m_mask;
    QRegion m_region;
    ScreenPaintData m_data;
};
}

void MultitaskViewExEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    if (m_teardownPending) {
        m_teardownPending = false;
        restoreWindowRoles();
        teardownSession();
    }
    if (m_trimPending) {
        m_trimPending = false;
        if (m_imgProvider) {
            m_imgProvider->trimUnused();
        }
    }
    if (!m_running) {
        effects->paintScreen(mask, region, data);
        return;
    }
    GLint savedViewport[4] = {0, 0, 0, 0};
    GLboolean savedDepth = GL_FALSE, savedCull = GL_FALSE, savedScissor = GL_FALSE, savedBlend = GL_FALSE;
    GLint savedFbo = 0;
    glGetIntegerv(GL_VIEWPORT, savedViewport);
    savedDepth = glIsEnabled(GL_DEPTH_TEST);
    savedCull = glIsEnabled(GL_CULL_FACE);
    savedScissor = glIsEnabled(GL_SCISSOR_TEST);
    savedBlend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedFbo);

    m_consumeBackgroundPass = true;
    effects->paintScreen(mask, region, data);
    m_consumeBackgroundPass = false;
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
    if (savedDepth)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (savedCull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (savedScissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    if (savedBlend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, savedFbo);
    const QRectF renderTarget(effects->renderTargetRect());
    const QVector<ScreenView *> targets = paintTargetViews(renderTarget);

    const KWinWindowPainter painter(mask, region, data);
    GLPaintContext ctx(&painter);

    for (EffectWindow *w : effects->stackingOrder()) {
        if (windowLayerOf(w->isDesktop(), w->isDock(), w->isNotification(), w->isOutline()) == WindowLayer::Desktop) {
            painter.paintWindow(w, w->frameGeometry(), w->frameGeometry(), 1.0);
        }
    }
    for (SceneLayer layer : sceneLayerOrder()) {
        if (layer == SceneLayer::Windows) {
            glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, savedFbo);
            effects->paintScreen(mask, region, data);
            continue;
        }
        const bool afterWindowLayer = layerZ(layer) > layerZ(SceneLayer::Windows);
        for (ScreenView *view : targets) {
            const QRectF canvas = view->scene->background()->geometry();
            ctx.setProjection(QRectF(0, 0, canvas.width(), canvas.height()));
            ctx.origin = view->model.origin();
            ctx.viewportSize = renderTarget.size().toSize();
            glViewport(int(ctx.origin.x()),
                       int(ctx.viewportSize.height() - (ctx.origin.y() + canvas.height())),
                       int(canvas.width()), int(canvas.height()));
            if (afterWindowLayer) {
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glDisable(GL_SCISSOR_TEST);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            }
            view->scene->paintLayer(ctx, layer);
        }
    }
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
    if (savedDepth)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (savedCull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (savedScissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    if (savedBlend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, savedFbo);
}

void MultitaskViewExEffect::postPaintScreen()
{
    finishBarDragIfButtonReleased();
    if (m_running && m_config->useBlurEffect()) {
        const EffectWindowList stacking = effects->stackingOrder();
        for (EffectWindow *w : stacking) {
            w->setData(WindowForceBlurRole, QVariant());
        }
    }
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        if (m_running && owned->model.ghostReturning() && !owned->scene->ghostLayer().animating()) {
            const int idx = owned->model.dragPlaceholder();
            owned->scene->ghostLayer().clear();
            owned->model.setDragPlaceholder(-1, false);
            effects->addRepaintFull();
        }
        if (m_running && m_pendingPreviewShow) {
            const auto &nodes = owned->scene->barLayer().nodes();
            if (!nodes.isEmpty() && !nodes.last().animating()) {
                m_pendingPreviewShow = false;
                if (m_addHovered && showAddButton(owned->model.barItems().size(), owned->model.maxWorkspaces())) {
                    owned->model.setBarPreview(true, m_newWorkspaceWallpaper);
                    effects->addRepaintFull();
                }
            }
        }
    }
    const bool exitFinished = !m_views.isEmpty() && [this] {
        for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
            if (!owned->scene->exitFinished()) {
                return false;
            }
        }
        return true;
    }();
    if (m_running && m_exitRequested && exitFinished) {
        EffectWindow *toActivate = m_selectedWindow;
        clearElevatedWindow();
        setRunning(false);
        relayDockClick();
        if (toActivate && effects->stackingOrder().contains(toActivate)) {
            QTimer::singleShot(0, this, [this, toActivate] {
                if (!effects->stackingOrder().contains(toActivate)) {
                    return;
                }
                if (auto *impl = static_cast<EffectWindowImpl *>(toActivate)) {
                    if (Window *win = impl->window()) {
                        win->setActive(true);
                    }
                }
                effects->activateWindow(toActivate);
            });
        }
        effects->postPaintScreen();
        return;
    }
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        if (owned->scene->needsFrame() || owned->model.barDragModel().isActive()) {
            effects->addRepaintFull();
            break;
        }
    }
    effects->postPaintScreen();
}

void MultitaskViewExEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!m_running) {
        effects->paintWindow(w, mask, region, data);
        return;
    }
    if (m_consumeBackgroundPass) {
        return;
    }
    switch (windowLayerOf(w->isDesktop(), w->isDock(), w->isNotification(), w->isOutline())) {
    case WindowLayer::Skip:
    case WindowLayer::Desktop:
        return;
    case WindowLayer::Dock:
        effects->paintWindow(w, mask, region, data);
        return;
    case WindowLayer::Normal:
        break;
    }
    ScreenView *view = m_views.find(w->screen());
    if (!view) {
        return;
    }
    QRectF src, cur;
    qreal op = 0.0;
    if (WindowNode *n = view->scene->windowItemLayer().nodeFor(w)) {
        if (n->retiring || n->opacity() <= 0.01) {
            return;
        }
        src = n->source;
        cur = n->rect();
        op = n->opacity();
    } else {
        bool found = false;
        for (const OutgoingGrid &snap : view->scene->outgoingLayer().grids()) {
            for (const OutgoingWindow &gw : snap.windows) {
                if (gw.window == w) {
                    src = gw.source;
                    cur = gw.interp.rect();
                    op = gw.interp.cur.opacity;
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
        if (!found || op <= 0.01) {
            return;
        }
    }
    if (src.width() <= 0 || src.height() <= 0) {
        return;
    }
    WindowPaintData d(data);
    d.multiplyOpacity(op);
    d *= QVector2D(float(cur.width() / src.width()), float(cur.height() / src.height()));
    d += QPoint(int(cur.x() - src.x()), int(cur.y() - src.y()));
    const QRectF canvas = view->scene->background()->geometry();
    const QRectF frameGeo = w->frameGeometry();
    const QRectF expandedGeo = w->expandedGeometry();
    const qreal sx = src.width() > 0.0 ? cur.width() / src.width() : 1.0;
    const qreal sy = src.height() > 0.0 ? cur.height() / src.height() : 1.0;
    const qreal padL = qMax<qreal>(0.0, (frameGeo.left() - expandedGeo.left()) * sx);
    const qreal padT = qMax<qreal>(0.0, (frameGeo.top() - expandedGeo.top()) * sy);
    const qreal padR = qMax<qreal>(0.0, (expandedGeo.right() - frameGeo.right()) * sx);
    const qreal padB = qMax<qreal>(0.0, (expandedGeo.bottom() - frameGeo.bottom()) * sy);
    const QRectF shadowRect = cur.adjusted(-padL, -padT, padR, padB);
    const QRegion clip = (QRegion(shadowRect.toRect()) & QRegion(canvas.toRect())).translated(view->model.origin().toPoint());
    if (!clip.isEmpty()) {
        effects->paintWindow(w, mask | Effect::PAINT_WINDOW_TRANSFORMED, clip, d);
    }
}

void MultitaskViewExEffect::drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    effects->drawWindow(w, mask, region, data);
}

void MultitaskViewExEffect::windowInputMouseEvent(QEvent *event)
{
    finishBarDragIfButtonReleased();
    if (!acceptsInput(m_running, m_exitRequested)) {
        event->accept();
        return;
    }
    const QPointF globalPos = static_cast<QMouseEvent *>(event)->pos();
    ScreenView *view = viewAtGlobal(globalPos);
    if (!view) {
        event->accept();
        return;
    }
    const QPointF pos = globalPos - view->model.origin();
    if (pointerBelongsToDock(view->model.dockAreas(), pos)) {
        if (event->type() == QEvent::MouseButtonPress && !view->model.dragActive()) {
            m_dockRelay.remember(globalPos, static_cast<QMouseEvent *>(event)->button());
            requestExit();
        }
        event->accept();
        return;
    }
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        finishBarDrag();
        view->scene->dispatchPress(pos);
        event->accept();
        break;
    case QEvent::MouseMove: {
        view->scene->dispatchMove(pos);
        const int cell = view->scene->windowCells()->cellAt(pos);
        if (cell != view->model.hoverCell()) {
            view->model.setHoverCell(cell);
            effects->addRepaintFull();
        }
        event->accept();
        break;
    }
    case QEvent::MouseButtonRelease:
        view->scene->dispatchRelease(pos);
        event->accept();
        break;
    case QEvent::Wheel: {
        QWheelEvent *we = static_cast<QWheelEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPointF wheelGlobal = we->position();
#else
        const QPointF wheelGlobal = we->posF();
#endif
        ScreenView *wv = viewAtGlobal(wheelGlobal);
        if (!wv) {
            break;
        }
        const QPointF wpos = wheelGlobal - wv->model.origin();
        const qreal dy = we->angleDelta().y();
        if (qFuzzyIsNull(dy)) {
            break;
        }
        if (wv->model.layout().gridArea.contains(wpos)) {
            gridScrollBy(*wv, dy);
        } else if (wpos.y() < wv->model.layout().gridArea.y()) {
            const qreal content = wv->model.metrics().contentWidth(wv->model.barItems().size());
            const qreal viewportWidth = wv->model.metrics().size().width();
            if (barMaxScroll(content, viewportWidth) > 0.0) {
                if (wv->scene->scrollBarBy(-dy, viewportWidth)) {
                    effects->addRepaintFull();
                }
            } else {
                slideByWheel(int(dy));
            }
        } else {
        }
        event->accept();
        break;
    }
    default:
        break;
    }
}

void MultitaskViewExEffect::relayDockClick()
{
    if (!m_dockRelay.pending()) {
        return;
    }
    const QPointF pos = m_dockRelay.pos();
    const Qt::MouseButton button = m_dockRelay.button();
    m_dockRelay.clear();
    if (effectsEx) {
        effectsEx->sendPointer(pos, button);
    }
}

void MultitaskViewExEffect::grabbedKeyboardEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->type() != QEvent::KeyPress) {
        return;
    }
    const int key = keyEvent->key() | keyEvent->modifiers();
    for (const QKeySequence &seq : m_toggleShortcut) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (seq[0].toCombined() == key) {
#else
        if (seq[0] == key) {
#endif
            toggle();
            return;
        }
    }

    ScreenView *kv = viewAtGlobal(effects->cursorPos());
    if (!kv) {
        return;
    }

    const NavAction action = navActionFor(keyEvent->key(), keyEvent->modifiers());
    switch (action.type) {
    case NavActionType::None:
        if (m_hasKeyboardGrab && !onX11Platform() && effectsEx->isShortcuts(keyEvent)) {
            QTimer::singleShot(20, this, [this]() {
                if (m_hasKeyboardGrab) {
                    effects->ungrabKeyboard();
                    m_hasKeyboardGrab = false;
                }
            });
        }
        break;
    case NavActionType::Consume:
        break;
    case NavActionType::Exit:
        requestExit();
        break;
    case NavActionType::SwitchDesktop:
        if (workspaces()->count() >= action.arg) {
            if (workspaces()->currentNumber() != action.arg) {
                setHoverWindow(*kv, nullptr);
                slideToDesktop(action.arg);
            } else {
                requestExit();
            }
        }
        break;
    case NavActionType::SwitchDesktopStep: {
        int index = workspaces()->currentNumber() + action.arg;
        if (index > workspaces()->count()) {
            index = 1;
        } else if (index <= 0) {
            index = workspaces()->count();
        }
        setHoverWindow(*kv, nullptr);
        slideToDesktop(index);
        break;
    }
    case NavActionType::MoveHoverToDesktop:
        moveHoverToDesktopIfAvailable(*kv, action.arg);
        break;
    case NavActionType::HoverFirst:
        setHoverWindow(*kv, firstOrLastWindow(*kv, true));
        break;
    case NavActionType::HoverLast:
        setHoverWindow(*kv, firstOrLastWindow(*kv, false));
        break;
    case NavActionType::CycleHover:
        cycleHoverWindow(*kv, action.arg);
        break;
    case NavActionType::CycleHoverOrFirst:
        if (!cycleHoverWindow(*kv, action.arg)) {
            setHoverWindow(*kv, firstOrLastWindow(*kv, true));
        }
        break;
    case NavActionType::CycleSameType:
        if (EffectWindow *w = hoverWindow(*kv)) {
            setHoverWindow(*kv, sameTypeWindow(*kv, w, action.arg));
        }
        break;
    case NavActionType::AddDesktop:
        if (!m_addDesktopTimer.isActive()) {
            m_addDesktopTimer.start();
        }
        break;
    case NavActionType::RemoveDesktop:
        removeDesktop(workspaces()->currentNumber());
        break;
    case NavActionType::ActivateHover:
        if (EffectWindow *w = hoverWindow(*kv)) {
            effects->defineCursor(Qt::PointingHandCursor);
            effects->setElevatedWindow(w, false);
            m_selectedWindow = w;
            requestExit();
        }
        break;
    case NavActionType::CloseHover:
        if (EffectWindow *w = hoverWindow(*kv)) {
            w->closeWindow();
            setHoverWindow(*kv, nullptr);
        }
        break;
    case NavActionType::Lock:
        setRunning(false);
        {
            const QString sessionPath = workspace()->getProcessSessionPath();
            if (!sessionPath.isEmpty()) {
                QDBusMessage message = QDBusMessage::createMethodCall(DBUS_LOGIN_SERVICE, sessionPath,
                                                                      DBUS_SESSION_INTF, "Lock");
                QDBusConnection::systemBus().asyncCall(message);
            }
        }
        break;
    }
}

void MultitaskViewExEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags);
    m_config->reload();
}

void MultitaskViewExEffect::applyConfigToViews()
{
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        owned->model.setMaxWorkspaces(m_config->maxWorkspaces());
        owned->model.setMotionOptions(m_config->motion());
    }
    if (m_running) {
        effects->addRepaintFull();
    }
}

WorkspaceModel *MultitaskViewExEffect::workspaces() const
{
    return m_workspaces ? m_workspaces->model() : nullptr;
}

void MultitaskViewExEffect::restoreWindowRoles()
{
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        for (EffectWindow *w : owned->model.windows()) {
            if (w) {
                m_roles.restore(*w);
            }
        }
    }
    m_roles.clear();
}

int MultitaskViewExEffect::requestedEffectChainPosition() const
{
    return 60;
}

bool MultitaskViewExEffect::borderActivated(ElectricBorder border)
{
    if (!m_borderActivate.contains(border)) {
        return false;
    }
    toggle();
    return true;
}

}

#include "multitaskviewex.moc"
