// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "thumbnailstrip.h"

#include "effects.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"
#include "workspace.h"

#include <kwinglutils.h>

#include <QImage>
#include <QPainter>
#include <QVector2D>

#include <algorithm>
#include <cmath>

namespace KWin
{

namespace
{
constexpr auto ThumbnailCaptureInterval = std::chrono::milliseconds(40);
constexpr int THUMBNAIL_HEIGHT = 118;
constexpr int THUMBNAIL_HOVER_WIDTH = 4;
constexpr int THUMBNAIL_HOVER_MARGIN = 7;
constexpr int THUMBNAIL_ICON_SIZE = 24;
constexpr int THUMBNAIL_RADIUS = 12;
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

bool ThumbnailStrip::active() const
{
    return m_state != State::Hidden;
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

ThumbnailStrip::Item *ThumbnailStrip::findItem(EffectWindow *window)
{
    const auto it = std::find_if(m_items.begin(), m_items.end(), [window](const Item &item) {
        return item.window == window;
    });
    return it == m_items.end() ? nullptr : &*it;
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
