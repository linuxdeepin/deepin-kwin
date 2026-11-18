// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "alttabthumbnaillist.h"

#include "alttabhelpers.h"
#include "previewanimator.h"
#include "thumbnailstrip.h"

#include "effects.h"
#include "scene/itemrenderer.h"
#include "window.h"
#include "workspace.h"

static void ensureResources()
{
    Q_INIT_RESOURCE(alttabthumbnaillist);
}

namespace KWin
{

AltTabThumbnailListEffect::AltTabThumbnailListEffect()
    : m_thumbnailStrip(std::make_unique<ThumbnailStrip>())
    , m_previewAnimator(std::make_unique<PreviewAnimator>())
{
    ensureResources();

    connect(effects, &EffectsHandler::tabBoxAdded, this, &AltTabThumbnailListEffect::slotTabboxAdded);
    connect(effects, &EffectsHandler::tabBoxClosed, this, &AltTabThumbnailListEffect::slotTabboxClosed);
    connect(effects, &EffectsHandler::tabBoxAborted, this, &AltTabThumbnailListEffect::slotTabboxAborted);
    connect(effects, &EffectsHandler::tabBoxUpdated, this, &AltTabThumbnailListEffect::slotTabboxUpdated);
    connect(effects, &EffectsHandler::mouseChanged, this, &AltTabThumbnailListEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::windowClosed, this, &AltTabThumbnailListEffect::slotWindowRemoved);
    connect(effects, &EffectsHandler::windowDeleted, this, &AltTabThumbnailListEffect::slotWindowRemoved);

    connect(effects, &EffectsHandler::windowDamaged, this, [this](EffectWindow *window) {
        m_thumbnailStrip->dirty(window);
    });
    connect(effects, &EffectsHandler::windowGeometryShapeChanged, this, [this](EffectWindow *window, const QRectF &old) {
        if (old == window->frameGeometry()) {
            m_thumbnailStrip->geometryChanged(window, false);
        }
    });
    connect(effects, &EffectsHandler::windowFrameGeometryChanged, this, [this](EffectWindow *window, const QRectF &old) {
        if (old.size() != window->frameGeometry().size()) {
            m_thumbnailStrip->geometryChanged(window, true);
            m_previewAnimator->windowResized(window);
        }
    });
    const auto watchDecoration = [this](EffectWindow *window) {
        Window *client = static_cast<EffectWindowImpl *>(window)->window();
        const auto invalidate = [this, window] {
            m_thumbnailStrip->decorationChanged(window);
        };
        connect(client, &Window::captionChanged, this, invalidate);
        connect(client, &Window::iconChanged, this, invalidate);
    };
    connect(effects, &EffectsHandler::windowAdded, this, watchDecoration);
    for (EffectWindow *window : effects->stackingOrder()) {
        watchDecoration(window);
    }
    connect(Workspace::self(), &Workspace::activeColorChanged, this, [this] {
        m_thumbnailStrip->dirty(nullptr);
    });
}

AltTabThumbnailListEffect::~AltTabThumbnailListEffect() = default;

bool AltTabThumbnailListEffect::isActive() const
{
    return m_thumbnailStrip->active() || m_previewAnimator->active();
}

/**
 * @brief 计算焦点屏及任务栏避让后的切换器和预览区。
 *
 * 切换器垂直位置只受底部任务栏影响，侧边任务栏仅缩减横向 PlacementArea。
 */
void AltTabThumbnailListEffect::updateLayout()
{
    EffectScreen *screen = effects->activeScreen();
    if (!screen) {
        return;
    }

    const QRect screenGeometry = screen->geometry();
    const QRect placementArea = effects->clientArea(PlacementArea, screen, effects->currentDesktop()).toRect();

    const int bottom = qMin(placementArea.bottom(), screenGeometry.bottom()) - scale(THUMBNAIL_VIEW_BOTTOM_MARGIN);
    const int height = scale(THUMBNAIL_VIEW_HEIGHT);

    const QRect view(placementArea.left() + scale(THUMBNAIL_VIEW_SIDE_MARGIN), bottom - height + 1,
                     placementArea.width() - scale(2 * THUMBNAIL_VIEW_SIDE_MARGIN), height);

    const int previewTop = screenGeometry.top() + scale(PREVIEW_TOP_MARGIN);
    const int previewBottom = view.top() - scale(PREVIEW_TOP_MARGIN);

    const QRect previewArea{placementArea.left() + scale(THUMBNAIL_SIDE_MARGIN), previewTop,
                            placementArea.width() - scale(2 * THUMBNAIL_SIDE_MARGIN),
                            qMax(0, previewBottom - previewTop)};

    m_thumbnailStrip->setRect(view);
    m_previewAnimator->setLayout(screen, previewArea);
}

void AltTabThumbnailListEffect::prePaintWindow(EffectWindow *window, WindowPrePaintData &data, std::chrono::milliseconds time)
{
    m_previewAnimator->prePaint(window, data);
    effects->prePaintWindow(window, data, time);
}

void AltTabThumbnailListEffect::paintWindow(EffectWindow *window, int mask, QRegion region, WindowPaintData &data)
{
    if (window->isDesktop()) {
        data.setBrightness(m_previewAnimator->active() ? m_previewAnimator->brightness() : 1);
    } else if (window->isWaterMark() && !m_paintingWaterMark) {
        m_waterMarkWindow = window;
        return;
    }

    if (!m_previewAnimator->paint(window, mask, region, data)) {
        effects->paintWindow(window, mask, region, data);
    }
}

void AltTabThumbnailListEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds time)
{
    if (isActive()) {
        updateLayout();
    }

    m_thumbnailStrip->advance(time);
    m_previewAnimator->advance(time);
    if (m_previewAnimator->active()) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }

    if (m_thumbnailStrip->active()) {
        data.paint += m_thumbnailStrip->repaintArea().adjusted(-1, -1, 1, 1);
    }

    effects->prePaintScreen(data, time);
}

void AltTabThumbnailListEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
    m_previewAnimator->paintClosingSnapshot(data);
    m_thumbnailStrip->paint(data);
    if (m_waterMarkWindow && m_thumbnailStrip->active()) {
        m_paintingWaterMark = true;
        ItemRenderer *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
        WindowPaintData watermark(renderer->renderTargetProjectionMatrix());

        watermark.setOpacity(m_thumbnailStrip->opacity());
        watermark.translate(0, m_thumbnailStrip->offset());

        effects->paintWindow(m_waterMarkWindow, 0, region, watermark);
        m_paintingWaterMark = false;
    }
}

void AltTabThumbnailListEffect::postPaintScreen()
{
    if (m_previewAnimator->animating()) {
        effects->addRepaintFull();
    } else if (m_thumbnailStrip->needsRepaint()) {
        effects->addRepaint(m_thumbnailStrip->repaintArea());
    }

    if (!isActive()) {
        m_waterMarkWindow = nullptr;
        m_paintingWaterMark = false;
    }
    effects->postPaintScreen();
}

void AltTabThumbnailListEffect::slotTabboxAdded(int)
{
    m_tabBoxAborted = false;
    const EffectWindowList windows = effects->currentTabBoxWindowList();

    if (windows.empty() || (windows.size() == 1 && windows.front()->isDesktop())) {
        effects->closeTabBox();
        return;
    }

    updateLayout();
    EffectWindow *selected = effects->currentTabBoxWindow();

    m_thumbnailStrip->sync(windows, selected);
    m_previewAnimator->begin(windows, selected, effects->activeScreen());
    m_thumbnailStrip->appear();

    effects->addRepaintFull();
}

void AltTabThumbnailListEffect::slotTabboxClosed()
{
    EffectWindow *selected = effects->currentTabBoxWindow();
    const bool selectedDesktop = selected && selected->isDesktop();
    if (m_tabBoxAborted || selectedDesktop) {
        m_tabBoxAborted = false;
        m_thumbnailStrip->hideImmediately();
        m_previewAnimator->cancel();
    } else {
        m_thumbnailStrip->disappear();
        m_previewAnimator->close();
    }
    effects->addRepaintFull();
}

void AltTabThumbnailListEffect::slotTabboxAborted()
{
    m_tabBoxAborted = true;
}

void AltTabThumbnailListEffect::slotTabboxUpdated()
{
    if (!isActive()) {
        return;
    }

    const EffectWindowList windows = effects->currentTabBoxWindowList();
    EffectWindow *selected = effects->currentTabBoxWindow();
    const auto change = m_thumbnailStrip->sync(windows, selected);
    if (change == ThumbnailStrip::Change::Windows) {
        m_previewAnimator->syncWindows(windows);
    }
    if (m_previewAnimator->select(selected) || change == ThumbnailStrip::Change::Windows) {
        effects->addRepaintFull();
    } else if (change == ThumbnailStrip::Change::Selection) {
        effects->addRepaint(m_thumbnailStrip->repaintArea());
    }
}

void AltTabThumbnailListEffect::slotMouseChanged(const QPoint &pos, const QPoint &, Qt::MouseButtons buttons, Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (buttons == Qt::LeftButton) {
        if (EffectWindow *window = m_thumbnailStrip->hit(pos)) {
            effects->setTabBoxWindow(window);
        }
    }
}

void AltTabThumbnailListEffect::slotWindowRemoved(EffectWindow *window)
{
    const bool wasActive = isActive();
    if (window == m_waterMarkWindow) {
        m_waterMarkWindow = nullptr;
        m_paintingWaterMark = false;
    }

    m_thumbnailStrip->remove(window);
    m_previewAnimator->remove(window);
    if (wasActive) {
        effects->addRepaintFull();
    }
}

} // namespace KWin
