// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "thumbnailstrip.h"

#include "effects.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "workspace.h"

#include <kwinglutils.h>

#include <QHash>
#include <QImage>
#include <QPainter>
#include <QVector2D>

#include <algorithm>
#include <cmath>

namespace KWin
{

namespace
{
constexpr auto SwitcherDuration = std::chrono::milliseconds(300);
constexpr auto ThumbnailCaptureInterval = std::chrono::milliseconds(40);
constexpr int THUMBNAIL_HEIGHT = 118;
constexpr int THUMBNAIL_HOVER_WIDTH = 4;
constexpr int THUMBNAIL_HOVER_MARGIN = 7;
constexpr int THUMBNAIL_ICON_SIZE = 24;
constexpr int THUMBNAIL_RADIUS = 12;
constexpr int THUMBNAIL_SPACING = 14;
constexpr int THUMBNAIL_TITLE_HEIGHT = 40;
constexpr int THUMBNAIL_TITLE_MARGIN = 8;
constexpr int THUMBNAIL_TEXT_SIZE = 14;
constexpr int THUMBNAIL_CLIP_FADE_WIDTH = 30;
}

/** @brief 初始化缩略图渲染所需的 shader。 */
ThumbnailStrip::ThumbnailStrip()
{
    m_clip = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QByteArray(), ":/effects/alttabthumbnaillist/shaders/clip.frag");
    m_scissor = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QByteArray(), ":/effects/alttabthumbnaillist/shaders/scissor.frag");

    if (m_clip && !m_clip->isValid()) {
        m_clip.reset();
    }
    if (m_scissor && !m_scissor->isValid()) {
        m_scissor.reset();
    }
}

ThumbnailStrip::~ThumbnailStrip()
{
    // Member destructors release the card textures, framebuffers and strip shaders.
    effects->makeOpenGLContextCurrent();
}

ThumbnailStrip::Change ThumbnailStrip::sync(const EffectWindowList &windows, EffectWindow *selected)
{
    const bool sameSize = windows.size() == qsizetype(m_items.size());
    const bool unchanged = sameSize && std::equal(windows.cbegin(), windows.cend(), m_items.cbegin(), [](EffectWindow *window, const Item &item) {
                               return window == item.window;
                           });
    const bool selectionChanged = m_selected != selected;

    if (!unchanged) {
        rebuildItems(windows);
    }

    m_selected = selected;

    const bool layoutChanged = !unchanged || m_layoutDirty;
    if (layoutChanged) {
        updateItemGeometries();
        layoutItems();
    }
    if (layoutChanged || selectionChanged) {
        select();
    }

    if (!unchanged) {
        return Change::Windows;
    }
    return selectionChanged ? Change::Selection : Change::None;
}

void ThumbnailStrip::setRect(const QRect &rect)
{
    if (m_availableRect == rect) {
        return;
    }

    if (m_availableRect.height() != rect.height() && !m_items.empty()) {
        // The strip height follows the system scale, as do the card textures.
        updateItemGeometries();
    }
    m_availableRect = rect;
    layoutItems();
    select();
}

void ThumbnailStrip::appear()
{
    m_fromOffset = m_offset;
    m_toOffset = 0;
    m_fromOpacity = m_opacity;
    m_toOpacity = 1;
    m_animation.start(SwitcherDuration);
    m_state = State::Appearing;
}

void ThumbnailStrip::disappear()
{
    if (m_state == State::Hidden) {
        return;
    }

    m_fromOffset = m_offset;
    m_toOffset = m_rect.height() + scale(THUMBNAIL_VIEW_BOTTOM_MARGIN);
    m_fromOpacity = m_opacity;
    m_toOpacity = 0;
    m_animation.start(SwitcherDuration);
    m_state = State::Disappearing;
}

void ThumbnailStrip::hideImmediately()
{
    m_animation.running = false;
    m_scrollAnimation.running = false;
    m_selectAnimation.running = false;
    m_offset = m_rect.height() + scale(THUMBNAIL_VIEW_BOTTOM_MARGIN);
    m_opacity = 0;
    m_state = State::Hidden;
}

void ThumbnailStrip::advance(std::chrono::milliseconds time)
{
    m_backgroundCaptureAvailable = true;
    m_animation.advance(time);
    m_scrollAnimation.advance(time);
    m_selectAnimation.advance(time);

    updateValues();

    if (m_animation.finishIfDone()) {
        if (m_state == State::Appearing) {
            m_state = State::Visible;
            m_offset = 0;
            m_opacity = 1;
        } else if (m_state == State::Disappearing) {
            m_state = State::Hidden;
        }
    }
    if (m_selectAnimation.finishIfDone()) {
        m_selectedRect = m_toSelectedRect;
    }

    if (m_scrollAnimation.finishIfDone()) {
        m_visible = m_toVisible;
    }
}

bool ThumbnailStrip::active() const
{
    return m_state != State::Hidden;
}

bool ThumbnailStrip::needsRepaint() const
{
    return m_animation.running || m_scrollAnimation.running || m_selectAnimation.running || hasPendingCapture();
}

qreal ThumbnailStrip::opacity() const
{
    return m_opacity;
}

int ThumbnailStrip::offset() const
{
    return m_offset;
}

void ThumbnailStrip::dirty(EffectWindow *window)
{
    for (Item &item : m_items) {
        if (!window || item.window == window) {
            item.dirty = true;
        }
    }

    if (!window) {
        m_selection.reset();
    }
}

void ThumbnailStrip::geometryChanged(EffectWindow *window, bool resized)
{
    Item *item = findItem(window);
    if (!item) {
        return;
    }

    item->needsCapture = true;
    item->dirty = true;
    if (!active()) {
        m_layoutDirty |= resized;
        return;
    }
    if (resized) {
        updateItemGeometries();
        layoutItems();
        select();
    }
    effects->addRepaint(repaintArea());
}

void ThumbnailStrip::decorationChanged(EffectWindow *window)
{
    if (Item *item = findItem(window)) {
        item->decorationDirty = true;
        if (active() && item->frame.intersects(m_visible)) {
            effects->addRepaint(repaintArea());
        }
    }
}

void ThumbnailStrip::remove(EffectWindow *window)
{
    if (!findItem(window)) {
        return;
    }

    const bool contextCurrent = effects->makeOpenGLContextCurrent();
    const auto firstRemoved = std::remove_if(m_items.begin(), m_items.end(), [window](const Item &item) {
        return item.window == window;
    });
    m_items.erase(firstRemoved, m_items.end());
    if (m_selected == window) {
        m_selected = nullptr;
        m_selection.reset();
        m_selectedRect = {};
        m_fromSelectedRect = {};
        m_toSelectedRect = {};
    }

    updateItemGeometries();
    layoutItems();
    select();

    if (contextCurrent) {
        effects->doneOpenGLContextCurrent();
    }
}

EffectWindow *ThumbnailStrip::hit(const QPoint &point) const
{
    if (m_state != State::Visible) {
        return nullptr;
    }

    for (const Item &item : m_items) {
        if (onScreen(item.frame).translated(0, m_offset).contains(point)) {
            return item.window;
        }
    }

    return nullptr;
}

void ThumbnailStrip::paint(ScreenPaintData &data)
{
    if (!active() || m_opacity <= 0) {
        return;
    }

    drawSelection(data);

    Item *selected = findItem(m_selected);
    if (selected && selected->frame.intersects(m_visible)) {
        if (m_state != State::Disappearing) {
            refresh(*selected);
        }
        drawItem(data, *selected);
    }

    for (Item &item : m_items) {
        if (&item == selected) {
            continue;
        }

        if (!item.frame.intersects(m_visible)) {
            item.release();
            continue;
        }

        if (m_state != State::Disappearing && (item.needsCapture || item.decorationDirty)) {
            refresh(item);
        }

        drawItem(data, item);
    }
}

QRect ThumbnailStrip::repaintArea() const
{
    if (!active() || !m_rect.isValid()) {
        return {};
    }

    // 条带会沿 y 方向滑动，覆盖从静止位置到当前位置之间的范围，避免动画残影。
    const int startY = qMin(0, m_offset);
    const int endY = qMax(0, m_offset);
    return m_rect.adjusted(0, startY, 0, endY);
}

ThumbnailStrip::Item *ThumbnailStrip::findItem(EffectWindow *window)
{
    const auto it = std::find_if(m_items.begin(), m_items.end(), [window](const Item &item) {
        return item.window == window;
    });
    return it == m_items.end() ? nullptr : &*it;
}

void ThumbnailStrip::rebuildItems(const EffectWindowList &windows)
{
    QHash<EffectWindow *, Item *> cachedItems;
    cachedItems.reserve(m_items.size());
    for (Item &item : m_items) {
        cachedItems.insert(item.window.data(), &item);
    }

    std::vector<Item> items;
    items.reserve(windows.size());
    auto remaining = m_items.size();
    for (EffectWindow *window : windows) {
        if (!window) {
            continue;
        }
        if (Item *cached = cachedItems.take(window)) {
            Item &item = items.emplace_back(std::move(*cached));
            // 窗口仍在列表中，补刷此前延后的后台内容。
            item.needsCapture |= item.dirty;
            --remaining;
        } else {
            items.emplace_back().window = window;
        }
    }

    const bool contextCurrent = remaining != 0 && effects->makeOpenGLContextCurrent();
    m_items = std::move(items);
    m_visible = {};
    m_fromVisible = {};
    m_toVisible = {};
    m_selectedRect = {};
    m_fromSelectedRect = {};
    m_toSelectedRect = {};

    if (contextCurrent) {
        effects->doneOpenGLContextCurrent();
    }
}

bool ThumbnailStrip::hasPendingCapture() const
{
    return m_state != State::Hidden && m_state != State::Disappearing && std::any_of(m_items.cbegin(), m_items.cend(), [this](const Item &item) {
               return item.frame.intersects(m_visible) && !item.thumbnail.isEmpty() && item.needsCapture;
           });
}

void ThumbnailStrip::updateValues()
{
    if (m_animation.running) {
        const qreal t = m_animation.value();
        m_offset = qRound(m_fromOffset + (m_toOffset - m_fromOffset) * t);
        m_opacity = m_fromOpacity + (m_toOpacity - m_fromOpacity) * t;
    }

    if (m_selectAnimation.running) {
        const qreal progress = m_selectAnimation.value();
        m_selectedRect = interpolateRect(m_fromSelectedRect, m_toSelectedRect, progress);
    }

    if (m_scrollAnimation.running) {
        const qreal progress = m_scrollAnimation.value();
        m_visible = interpolateRect(m_fromVisible, m_toVisible, progress);
    }
}

void ThumbnailStrip::layoutItems()
{
    updateViewport();
    if (m_items.empty() || !m_rect.isValid()) {
        return;
    }

    const int width = listWidth();
    const int visibleWidth = qMin(width, m_rect.width());
    if (!m_visible.isValid()) {
        m_visible = {0, 0, visibleWidth, m_rect.height()};
    } else {
        m_visible.setSize({visibleWidth, m_rect.height()});
        m_visible.moveLeft(qBound(0, m_visible.left(), qMax(0, width - visibleWidth)));
    }

    m_fromVisible = m_visible;
    m_toVisible = m_visible;
    effects->setTabBoxViewRect(m_rect.adjusted(-1, -1, 1, 1));
}

void ThumbnailStrip::updateViewport()
{
    if (!m_availableRect.isValid()) {
        m_rect = {};
        return;
    }

    const int width = m_items.empty()
        ? m_availableRect.width()
        : qMin(listWidth(), m_availableRect.width());
    m_rect = {m_availableRect.left() + (m_availableRect.width() - width) / 2,
              m_availableRect.top(),
              width,
              m_availableRect.height()};
}

void ThumbnailStrip::updateItemGeometries()
{
    m_layoutDirty = false;
    int x = scale(THUMBNAIL_SIDE_MARGIN);
    const int imageHeight = scale(THUMBNAIL_HEIGHT);

    for (Item &item : m_items) {
        const QRect source = item.window ? item.window->frameGeometry().toRect() : QRect();
        const int sourceHeight = qMax(1, source.height());
        const int contentHeight = qMin(imageHeight, sourceHeight);
        const int width = sourceHeight > imageHeight
            ? qCeil(source.width() * qreal(imageHeight) / sourceHeight)
            : source.width();

        if (item.thumbnail.size() != QSize(width, contentHeight)) {
            item.decorationDirty = true;
            item.needsCapture = true;
            item.dirty = true;
        }
        item.frame = {x, 0, width, scale(THUMBNAIL_TITLE_HEIGHT) + imageHeight};
        item.thumbnail = {x,
                          scale(THUMBNAIL_TITLE_HEIGHT) + (imageHeight - contentHeight) / 2,
                          width,
                          contentHeight};
        x += width + scale(THUMBNAIL_SPACING);
    }
}

int ThumbnailStrip::listWidth() const
{
    return m_items.empty() ? 0 : m_items.back().frame.right() + 1 + scale(THUMBNAIL_SIDE_MARGIN);
}

QRect ThumbnailStrip::onScreen(const QRect &rect, const QRect &visible) const
{
    if (!rect.isValid() || !rect.intersects(visible)) {
        return {};
    }

    return rect.translated(m_rect.left() - visible.left(), m_rect.top() + (m_rect.height() - scale(THUMBNAIL_TITLE_HEIGHT + THUMBNAIL_HEIGHT)) / 2);
}

QRect ThumbnailStrip::onScreen(const QRect &rect) const
{
    return onScreen(rect, m_visible);
}

void ThumbnailStrip::setStripUniforms(GLShader *shader, const QRect &rect) const
{
    const qreal left = qBound(0.0, qreal(m_rect.left() - rect.left()) / rect.width(), 1.0);
    const qreal right = qBound(0.0, qreal(m_rect.right() - rect.left() + 1) / rect.width(), 1.0);
    shader->setUniform("clip_left", float(left));
    shader->setUniform("clip_right", float(right));
    shader->setUniform("clip_fade", qMin(1.0f, float(scale(THUMBNAIL_CLIP_FADE_WIDTH)) / rect.width()));
    shader->setUniform("opacity", float(m_opacity));
}

void ThumbnailStrip::select()
{
    const Item *selected = findItem(m_selected);
    if (!selected) {
        m_selectedRect = {};
        m_fromSelectedRect = {};
        m_toSelectedRect = {};
        return;
    }

    updateValues();
    QRect targetVisible = m_visible;
    const int spacing = scale(THUMBNAIL_SPACING);
    if (selected->frame.left() - spacing < targetVisible.left()) {
        targetVisible.moveLeft(selected->frame.left() - spacing);
    } else if (selected->frame.right() + spacing > targetVisible.right()) {
        targetVisible.moveLeft(selected->frame.right() + spacing - targetVisible.width() + 1);
    }

    targetVisible.moveLeft(qBound(0, targetVisible.left(), qMax(0, listWidth() - targetVisible.width())));

    m_fromVisible = m_visible;
    m_toVisible = targetVisible;
    if (m_fromVisible != m_toVisible) {
        m_scrollAnimation.start(SwitcherDuration);
    }

    m_fromSelectedRect = m_selectedRect;
    const int margin = scale(THUMBNAIL_HOVER_MARGIN);
    m_toSelectedRect = onScreen(selected->frame, targetVisible).adjusted(-margin, -margin, margin, margin);
    if (!m_selectedRect.isValid()) {
        m_selectedRect = m_toSelectedRect;
        m_fromSelectedRect = m_toSelectedRect;
    } else if (m_toSelectedRect.isValid() && m_fromSelectedRect != m_toSelectedRect) {
        m_selectAnimation.start(SwitcherDuration);
    }
}

void ThumbnailStrip::refresh(Item &item)
{
    if (!item.window || item.thumbnail.isEmpty()) {
        return;
    }

    // 装饰（标题栏、图标、文字）与窗口内容纹理相互独立。
    // 装饰只依赖布局尺寸与窗口标题/图标，内容纹理刷新时无需重建。
    if (item.decorationDirty || !item.decoration || item.decoration->size() != item.frame.size() + QSize(2, 2)) {
        item.rebuildDecoration();
        item.decorationDirty = false;
    }

    if (item.window != m_selected && !item.needsCapture) {
        return;
    }
    if (!item.dirty && item.content && item.content->size() == item.thumbnail.size()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    // 持续 damage 的窗口按时间节流；选中项不消耗每帧唯一的后台捕获机会。
    if (!item.needsCapture && now - item.lastCapture < ThumbnailCaptureInterval) {
        return;
    }
    if (item.window != m_selected) {
        if (!m_backgroundCaptureAvailable) {
            return;
        }
        m_backgroundCaptureAvailable = false;
    }

    item.ensureContent();
    if (!item.fbo || !item.fbo->valid()) {
        item.release();
        return;
    }
    item.capture(now);
}

void ThumbnailStrip::Item::release()
{
    fbo.reset();
    content.reset();
    decoration.reset();
    needsCapture = true;
    dirty = true;
}

void ThumbnailStrip::Item::ensureContent()
{
    if (!content || content->size() != thumbnail.size()) {
        content = std::make_unique<GLTexture>(GL_RGBA8, thumbnail.size());
        content->setFilter(GL_LINEAR);
        content->setWrapMode(GL_CLAMP_TO_EDGE);
        content->setYInverted(true);
        fbo = std::make_unique<GLFramebuffer>(content.get());
    }
}

void ThumbnailStrip::Item::capture(std::chrono::steady_clock::time_point now)
{
    const QRectF geometry = window->frameGeometry();
    if (!geometry.isValid()) {
        return;
    }

    lastCapture = now;
    GLFramebuffer::pushFramebuffer(fbo.get());
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    ItemRenderer *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
    const qreal targetScale = renderer->renderTargetScale();
    WindowPaintData paintData(captureProjection(geometry, targetScale));
    WindowItem *windowItem = static_cast<EffectWindowImpl *>(window.data())->windowItem();
    // renderItem() 会覆盖根 Item 的 transform；缩略图捕获不得污染常态场景状态。
    const QMatrix4x4 originalTransform = windowItem->transform();
    renderer->renderItem(windowItem, Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), paintData);
    windowItem->setTransform(originalTransform);
    GLFramebuffer::popFramebuffer();
    dirty = false;
    needsCapture = false;
}

void ThumbnailStrip::Item::rebuildDecoration()
{
    if (!window) {
        return;
    }

    QPixmap pixmap(frame.size() + QSize(2, 2));
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#CCFFFFFF"));
    painter.drawRoundedRect(QRect(1, 1, frame.width(), frame.height()), scale(THUMBNAIL_RADIUS), scale(THUMBNAIL_RADIUS));
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.setBrush(Qt::transparent);
    painter.drawRect(thumbnail.translated(1 - thumbnail.x(), 1));
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(QPen(QColor("#19000000"), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(pixmap.rect(), scale(THUMBNAIL_RADIUS + 1), scale(THUMBNAIL_RADIUS + 1));
    painter.setPen(QPen(QColor("#0D000000"), 1));
    painter.drawLine(0, scale(THUMBNAIL_TITLE_HEIGHT), pixmap.width(), scale(THUMBNAIL_TITLE_HEIGHT));

    const int iconSize = scale(THUMBNAIL_ICON_SIZE);
    const QRect iconRect(scale(THUMBNAIL_TITLE_MARGIN), (scale(THUMBNAIL_TITLE_HEIGHT) - iconSize) / 2, iconSize, iconSize);
    painter.drawPixmap(iconRect.topLeft(), window->icon().pixmap(iconSize, iconSize));
    QFont font;
    font.setPixelSize(scale(THUMBNAIL_TEXT_SIZE));
    painter.setFont(font);
    painter.setPen(Qt::black);
    const int textLeft = iconRect.right() + scale(THUMBNAIL_TITLE_MARGIN);
    const int textWidth = pixmap.width() - iconRect.right() - scale(2 * THUMBNAIL_TITLE_MARGIN);
    const QRect textRect(textLeft, 0, textWidth, scale(THUMBNAIL_TITLE_HEIGHT));

    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(font).elidedText(window->caption(), Qt::ElideRight, textRect.width()));
    decoration = std::make_unique<GLTexture>(pixmap);
}

void ThumbnailStrip::ensureScissorMask()
{
    const int radius = scale(THUMBNAIL_RADIUS);
    if (radius <= 0) {
        return;
    }

    const QSize size(radius, radius);
    if (m_scissorMask && m_scissorMask->size() == size) {
        return;
    }

    m_scissorMask.reset();

    QImage image(QSize(radius * 2, radius * 2), QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawEllipse(image.rect());

    m_scissorMask = std::make_unique<GLTexture>(image.copy(0, 0, radius, radius));
    m_scissorMask->setFilter(GL_LINEAR);
    m_scissorMask->setWrapMode(GL_CLAMP_TO_EDGE);
}

void ThumbnailStrip::drawTexture(ScreenPaintData &data, GLTexture *texture, const QRect &rect, GLShader *shader, float widthScale)
{
    if (!texture || texture->isNull() || !rect.isValid()) {
        return;
    }

    GLShader *current = shader ? shader : ShaderManager::instance()->shader(ShaderTrait::MapTexture);
    const ShaderBinder binder(current);
    current->setUniform(GLShader::ModelViewProjectionMatrix, textureProjection(data, rect));
    if (shader) {
        current->setUniform("widthScale", widthScale);
        setStripUniforms(current, rect);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    texture->bind();
    texture->render(rect, effects->renderTargetScale());
    texture->unbind();
    glDisable(GL_BLEND);
}

void ThumbnailStrip::drawSelection(ScreenPaintData &data)
{
    if (!m_selectedRect.isValid()) {
        return;
    }

    const int radius = scale(THUMBNAIL_RADIUS + THUMBNAIL_HOVER_MARGIN);
    // 两端保留原像素，中间一列可伸展。窄于两端的选中框仍按实际宽度绘制。
    const QSize textureSize(m_clip ? qMin(m_selectedRect.width(), 2 * radius + 1) : m_selectedRect.width(), m_selectedRect.height());
    if (!m_selection || m_selection->size() != textureSize) {
        QPixmap pixmap(textureSize);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(workspace()->ActiveColor()));
        painter.drawRoundedRect(pixmap.rect(), radius, radius);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setBrush(Qt::transparent);
        const int borderWidth = scale(THUMBNAIL_HOVER_WIDTH);
        painter.drawRoundedRect(pixmap.rect().adjusted(borderWidth, borderWidth, -borderWidth, -borderWidth),
                                radius - borderWidth,
                                radius - borderWidth);
        m_selection = std::make_unique<GLTexture>(pixmap);
    }

    const float widthScale = float(m_selectedRect.width()) / textureSize.width();
    drawTexture(data, m_selection.get(), m_selectedRect.translated(0, m_offset), m_clip.get(), widthScale);
}

void ThumbnailStrip::drawItem(ScreenPaintData &data, Item &item)
{
    const QRect frame = onScreen(item.frame).adjusted(-1, -1, 1, 1).translated(0, m_offset);
    const QRect thumbnail = onScreen(item.thumbnail).translated(0, m_offset);
    drawTexture(data, item.decoration.get(), frame, m_clip.get());
    if (!item.content || !thumbnail.isValid()) {
        return;
    }

    ensureScissorMask();
    if (!item.window || item.window->width() <= 0 || item.window->height() <= 0) {
        return;
    }

    if (!m_scissor || !m_scissor->isValid() || !m_scissorMask) {
        drawTexture(data, item.content.get(), thumbnail, nullptr);
        return;
    }

    const qreal radius = scale(THUMBNAIL_RADIUS);
    const ShaderBinder binder(m_scissor.get());
    m_scissor->setUniform(GLShader::ModelViewProjectionMatrix, textureProjection(data, thumbnail));
    m_scissor->setUniform("sampler", 0);
    m_scissor->setUniform("msk1", 1);
    m_scissor->setUniform("k", QVector2D(thumbnail.width() / radius, thumbnail.height() / radius));
    setStripUniforms(m_scissor.get(), thumbnail);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE1);
    m_scissorMask->bind();
    glActiveTexture(GL_TEXTURE0);
    item.content->bind();
    item.content->render(thumbnail, effects->renderTargetScale());
    item.content->unbind();
    glActiveTexture(GL_TEXTURE1);
    m_scissorMask->unbind();
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_BLEND);
}

} // namespace KWin
