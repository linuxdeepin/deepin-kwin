// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "alttabthumbnaillist.h"

#include "alttabhelpers.h"
#include "previewanimator.h"
#include "thumbnailstrip.h"

#include <QPainter>

#include "effects.h"
#include <kwinglutils.h>
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "window.h"
#include "workspace.h"

#define THUMBNAIL_HEIGHT        (118 * effectsEx->getOsScale())
#define THUMBNAIL_HOVER_WIDTH   (4 * effectsEx->getOsScale())
#define THUMBNAIL_HOVER_MARGIN  (7 * effectsEx->getOsScale())
#define THUMBNAIL_ICON_SIZE     (24 * effectsEx->getOsScale())
#define THUMBNAIL_RADIUS        (12 * effectsEx->getOsScale())
#define THUMBNAIL_TITLE_HEIGHT  (40 * effectsEx->getOsScale())
#define THUMBNAIL_TITLE_MARGIN  (8 * effectsEx->getOsScale())
#define THUMBNAIL_TEXT_SIZE     (14 * effectsEx->getOsScale())

Q_LOGGING_CATEGORY(KWIN_ALTTABTHUMBNAILLIST, "kwin_effect_alttabthumbnaillist", QtWarningMsg)

static void ensureResources()
{
    Q_INIT_RESOURCE(alttabthumbnaillist);
}

static QRect heightScaledRect(const QRect &r, const int height)
{
    if (r.height() < height || r.height() <= 0)
        return QRect(0, 0, r.width(), r.height());

    const int width = std::ceil(r.width() * (float(height) / r.height()));
    return QRect(0, 0, width, height);
}

namespace KWin
{

ItemView::ItemView(EffectWindow *w, int xInList)
    : m_window(w)
{
    if (window()) {
        const QRect scaled = heightScaledRect(window()->frameGeometry().toRect(), THUMBNAIL_HEIGHT);
        m_frameRect = QRect(xInList, 0, scaled.width(), THUMBNAIL_TITLE_HEIGHT + THUMBNAIL_HEIGHT);
        m_thumbnailRect = scaled.translated(xInList,
                THUMBNAIL_TITLE_HEIGHT + (THUMBNAIL_HEIGHT - scaled.height()) / 2);
    }
}

ItemView::ItemView(ItemView &&info)
{
    *this = std::move(info);
}

ItemView &ItemView::operator=(ItemView &&info)
{
    if (this != &info) {
        m_window = info.m_window;
        m_frameRect = info.m_frameRect;
        m_thumbnailRect = info.m_thumbnailRect;
        m_fbo = std::move(info.m_fbo);
        m_windowTexture = std::move(info.m_windowTexture);
        m_filledTexture = std::move(info.m_filledTexture);
    }
    return *this;
}

ItemView::~ItemView()
{
    clearTexture();
}

void ItemView::updateTexture()
{
    if (!window() || thumbnailRect().isEmpty())
        return;

    if (!m_windowTexture || m_windowTexture->size() != thumbnailRect().size()) {
        m_windowTexture = std::make_unique<GLTexture>(GL_RGBA8, thumbnailRect().size());
        m_windowTexture->setFilter(GL_LINEAR);
        m_windowTexture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_windowTexture->setYInverted(true);
        m_fbo = std::make_unique<GLFramebuffer>(m_windowTexture.get());
    }

    // update window thumbnail
    if (m_fbo) {
        GLFramebuffer::pushFramebuffer(m_fbo.get());

        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        ItemRenderer *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
        const qreal scale = renderer->renderTargetScale();
        QMatrix4x4 projectionMatrix;
        const QRectF geometry = window()->frameGeometry();
        projectionMatrix.ortho(geometry.x() * scale, (geometry.x() + geometry.width()) * scale,
                               geometry.y() * scale, (geometry.y() + geometry.height()) * scale, -1, 1);
        WindowPaintData data;
        data.setProjectionMatrix(projectionMatrix);
        const int mask = Scene::PAINT_WINDOW_TRANSFORMED;
        renderer->renderItem(static_cast<EffectWindowImpl *>(window())->windowItem(), mask, infiniteRegion(), data);

        if (window()->isDesktop()) {
            EffectWindow *dock = AltTabThumbnailListEffect::dockWindow();
            if (dock && geometry.contains(dock->frameGeometry()))
                renderer->renderItem(static_cast<EffectWindowImpl *>(dock)->windowItem(), mask, infiniteRegion(), data);
        }

        GLFramebuffer::popFramebuffer();
    }

    // update filled
    const QRect filled_rect(QPoint(0, 0), frameRect().size() + QSize(2, 2));  // consider border
    if (!m_filledTexture || m_filledTexture->size() != filled_rect.size()) {
        QPixmap pixmap(filled_rect.size());
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

        const float radius = THUMBNAIL_RADIUS;
        // inner rect
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#CCFFFFFF"));
        painter.drawRoundedRect(frameRect().translated(1 - frameRect().x(), 1), radius, radius);
        if (m_thumbnailRect.height() >= THUMBNAIL_HEIGHT) {
            QPainter::CompositionMode mode = painter.compositionMode();
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.setBrush(Qt::transparent);
            painter.drawRect(thumbnailRect().translated(1 - thumbnailRect().x(), 1));
            painter.setCompositionMode(mode);
        }
        // border
        painter.setPen(QPen(QColor("#19000000"), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(filled_rect, radius + 1, radius + 1);
        // split line
        painter.setPen(QPen(QColor("#0D000000"), 1));
        painter.drawLine(QPointF(0, THUMBNAIL_TITLE_HEIGHT),
                         QPointF(filled_rect.width(), THUMBNAIL_TITLE_HEIGHT));
        // icon
        const qreal verical_margin = (THUMBNAIL_TITLE_HEIGHT - THUMBNAIL_ICON_SIZE) / 2;
        const QRectF icon_rect(QPointF(THUMBNAIL_TITLE_MARGIN, verical_margin),
                               QSizeF(THUMBNAIL_ICON_SIZE, THUMBNAIL_ICON_SIZE));
        const QIcon icon = window()->isDesktop() ? QIcon::fromTheme(QStringLiteral("deepin-toggle-desktop"))
                                                 : window()->icon();
        painter.drawPixmap(icon_rect.topLeft(), icon.pixmap(THUMBNAIL_ICON_SIZE, THUMBNAIL_ICON_SIZE));
        // title
        QFont font;
        font.setPixelSize(THUMBNAIL_TEXT_SIZE);
        const QFontMetricsF metrics(font);
        const QRectF text_rect(icon_rect.x() + icon_rect.width() + THUMBNAIL_TITLE_MARGIN, 0,
                               filled_rect.width() - icon_rect.width() - 3 * THUMBNAIL_TITLE_MARGIN,
                               THUMBNAIL_TITLE_HEIGHT);
        const QString elided = metrics.elidedText(window()->caption(), Qt::ElideRight, text_rect.width());
        painter.setPen(Qt::black);
        painter.setFont(font);
        painter.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, elided);

        m_filledTexture = std::make_unique<GLTexture>(pixmap);
    }
}

void ItemView::clearTexture()
{
    m_fbo.reset();
    m_windowTexture.reset();
    m_filledTexture.reset();
}

//----------------------------thumbnail list effect-----------------------------

EffectWindow *AltTabThumbnailListEffect::s_dockWindow = nullptr;

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

void AltTabThumbnailListEffect::reconfigure(ReconfigureFlags flags)
{
    const bool enable = effects->waylandDisplay() || effectsEx->effectType() == EffectType::OpenGLComplete;

    m_scrollAnimation.enable = enable;
    m_scrollAnimation.active = false;
    m_scrollAnimation.timeLine.setDuration(std::chrono::milliseconds(static_cast<int>(animationTime(400))));
    m_scrollAnimation.timeLine.setEasingCurve(QEasingCurve::OutExpo);

    m_selectAnimation.enable = enable;
    m_selectAnimation.active = false;
    m_selectAnimation.timeLine.setDuration(std::chrono::milliseconds(static_cast<int>(animationTime(400))));
    m_selectAnimation.timeLine.setEasingCurve(QEasingCurve::OutExpo);
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

void AltTabThumbnailListEffect::setActive(bool active)
{
    m_isActive = active;

    effects->addRepaintFull();

    // inactive
    if (!active) {
        m_scrollAnimation.active = false;
        m_scrollAnimation.timeLine.reset();
        m_selectAnimation.active = false;
        m_selectAnimation.timeLine.reset();

        m_paintingWaterMark = false;

        m_visibleRect = m_prevVisibleRect = m_nextVisibleRect = QRect();
        m_selectedRect = m_prevSelectedRect = m_nextSelectedRect = QRect();

        m_selectedWindow = nullptr;
        m_waterMarkWindow = nullptr;
        s_dockWindow = nullptr;

        effects->makeOpenGLContextCurrent();
        m_itemList.clear();
        return;
    }
}

void AltTabThumbnailListEffect::renderSelectedFrame(ScreenPaintData &data)
{
    // update texture
    if (!m_selectedFrame || m_selectedFrame->size() != m_selectedRect.size()) {
        const QRect r(QPoint(0, 0), m_selectedRect.size());
        QPixmap pixmap(r.size());
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);

        const float radius = THUMBNAIL_RADIUS + THUMBNAIL_HOVER_MARGIN;
        painter.setBrush(QColor(workspace()->ActiveColor()));
        painter.drawRoundedRect(r, radius, radius);

        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setBrush(Qt::transparent);
        const float width = THUMBNAIL_HOVER_WIDTH;
        painter.drawRoundedRect(r.adjusted(width, width, -width, -width), radius - width, radius - width);

        m_selectedFrame = std::make_unique<GLTexture>(pixmap);
    }

    if (!m_selectedFrame || m_selectedFrame->isNull()) {
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to render selected frame texture";
        return;
    }

    const QRectF scaled = scaledRect(m_selectedRect, effects->renderTargetScale());

    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 mvp(data.projectionMatrix());
    mvp.translate(scaled.x(), scaled.y());
    shader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_selectedFrame->bind();
    m_selectedFrame->render(scaled.toRect(), effects->renderTargetScale());
    m_selectedFrame->unbind();

    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}

void AltTabThumbnailListEffect::renderItemView(ItemView &item, ScreenPaintData &data)
{
    const QRect trans_visible = m_visibleRect.translated(m_viewRect.left() - m_visibleRect.left(),
                                                         m_viewRect.top());

    EffectWindow *w = item.window();
    item.updateTexture();

    // render filled
    GLTexture *texture = item.filledTexture();
    if (!texture || texture->isNull()) {
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to render filled texture of " << w->caption();
    } else {
        ShaderManager::instance()->pushShader(m_clipShader.get());

        const QRect filled_rect = rectOnScreen(item.frameRect(), m_visibleRect).adjusted(-1, -1, 1, 1);  // consider border
        int clip = 0;
        if (filled_rect.left() < trans_visible.left()) {
            clip = filled_rect.left() - trans_visible.left();
        } else if (filled_rect.right() > trans_visible.right()) {
            clip = filled_rect.right() - trans_visible.right();
        }
        const float clip_factor = float(clip) / filled_rect.width();

        QMatrix4x4 mvp(data.projectionMatrix());
        mvp.translate(filled_rect.x() * effects->renderTargetScale(),
                      filled_rect.y() * effects->renderTargetScale());

        m_clipShader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
        m_clipShader->setUniform("clip_left", clip_factor < 0 ? -clip_factor : 0);
        m_clipShader->setUniform("clip_right", clip_factor > 0 ? 1 - clip_factor : 1);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        texture->bind();
        texture->render(filled_rect, effects->renderTargetScale());
        texture->unbind();

        glDisable(GL_BLEND);

        ShaderManager::instance()->popShader();
    }

    // build scissor mask
    if (!m_scissorMask) {
        QImage img(QSize(THUMBNAIL_RADIUS * 2, THUMBNAIL_RADIUS * 2), QImage::Format_RGBA8888);
        img.fill(Qt::transparent);
        QPainter painter(&img);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 255));
        painter.setRenderHint(QPainter::Antialiasing);
        painter.drawEllipse(0, 0, THUMBNAIL_RADIUS * 2, THUMBNAIL_RADIUS * 2);
        painter.end();

        m_scissorMask = std::make_unique<GLTexture>(img.copy(0, 0, THUMBNAIL_RADIUS, THUMBNAIL_RADIUS));
        m_scissorMask->setFilter(GL_LINEAR);
        m_scissorMask->setWrapMode(GL_CLAMP_TO_EDGE);
    }

    // render thumbnail
    texture = item.windowTexture();
    if (!texture || texture->isNull()) {
        qWarning(KWIN_ALTTABTHUMBNAILLIST) << "Failed to load thumbnail texture of " << w->caption();
    } else {
        ShaderManager::instance()->pushShader(m_scissorShader.get());

        const QRect thumbnail_rect = rectOnScreen(item.thumbnailRect(), m_visibleRect);
        const float width_factor = float(thumbnail_rect.width()) / w->width();
        const float height_factor = float(thumbnail_rect.height()) / w->height();
        int clip = 0;
        if (thumbnail_rect.left() < trans_visible.left()) {
            clip = thumbnail_rect.left() - trans_visible.left();
        } else if (thumbnail_rect.right() > trans_visible.right()) {
            clip = thumbnail_rect.right() - trans_visible.right();
        }
        const float clip_factor = float(clip) / thumbnail_rect.width();

        QMatrix4x4 mvp(data.projectionMatrix());
        mvp.translate(thumbnail_rect.x() * effects->renderTargetScale(),
                      thumbnail_rect.y() * effects->renderTargetScale());

        m_scissorShader->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
        m_scissorShader->setUniform("sampler", 0);
        m_scissorShader->setUniform("msk1", 1);
        m_scissorShader->setUniform("k",
                                    QVector2D(w->width() * width_factor / THUMBNAIL_RADIUS,
                                              w->height() * height_factor / THUMBNAIL_RADIUS));
        m_scissorShader->setUniform("clip_left", clip_factor < 0 ? -clip_factor : 0);
        m_scissorShader->setUniform("clip_right", clip_factor > 0 ? 1 - clip_factor : 1);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        glActiveTexture(GL_TEXTURE1);
        m_scissorMask->bind();
        glActiveTexture(GL_TEXTURE0);
        texture->bind();
        texture->render(thumbnail_rect, effects->renderTargetScale());
        glActiveTexture(GL_TEXTURE1);
        m_scissorMask->unbind();
        glActiveTexture(GL_TEXTURE0);
        texture->unbind();

        glDisable(GL_BLEND);

        ShaderManager::instance()->popShader();
    }
}

QRect AltTabThumbnailListEffect::rectOnScreen(const QRect &rectInList, const QRect &visible) const
{
    if (!rectInList.isValid() || !visible.isValid() || !rectInList.intersects(visible))
        return QRect();

    const int item_top_margin = (m_viewRect.height() - THUMBNAIL_TITLE_HEIGHT - THUMBNAIL_HEIGHT) / 2;
    const int item_y = m_viewRect.top() + item_top_margin;
    QRect item_rect = rectInList.translated(-visible.left() + m_viewRect.left(), item_y);
    return item_rect;
}

} // namespace KWin
