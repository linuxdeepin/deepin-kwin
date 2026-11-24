// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "previewanimator.h"

#include "effects.h"
#include "scene/itemrenderer.h"
#include "scene/shadowitem.h"
#include "scene/windowitem.h"
#include "window.h"

#include <kwinglutils.h>

#if QT_VERSION > QT_VERSION_CHECK(5, 11, 3)
#include <QScopeGuard>
#else
#include "utils/qscopeguard.h"
#endif

#include <algorithm>

namespace KWin
{

namespace
{
constexpr auto GatherDuration = std::chrono::milliseconds(300);
constexpr auto SwitchDuration = std::chrono::milliseconds(400);
constexpr auto RelayoutDuration = std::chrono::milliseconds(150);
constexpr auto WindowCloseDuration = std::chrono::milliseconds(400);
constexpr auto OtherCloseDelay = std::chrono::milliseconds(50);
constexpr auto CloseDuration = WindowCloseDuration + OtherCloseDelay;
constexpr auto BackgroundCloseDuration = std::chrono::milliseconds(150);

QRect fitRect(const QRect &source, const QRect &area)
{
    if (!source.isValid() || !area.isValid()) {
        return {};
    }

    QSize size = source.size();
    if (size.width() > area.width() || size.height() > area.height()) {
        const qreal factor = qMin(qreal(area.width()) / size.width(), qreal(area.height()) / size.height());
        size = {qMax(1, qRound(size.width() * factor)), qMax(1, qRound(size.height() * factor))};
    }

    QRect result({}, size);
    result.moveCenter(area.center());
    return result;
}

QRect halfRect(const QRect &rect)
{
    QRect result(QPoint(), QSize(qMax(1, rect.width() / 2), qMax(1, rect.height() / 2)));
    result.moveCenter(rect.center());
    return result;
}

const QEasingCurve s_closeEasing(QEasingCurve::OutExpo);
constexpr auto MaxPreviewFrameStep = std::chrono::milliseconds(50);

std::unique_ptr<GLTexture> captureItemTexture(ItemRenderer *renderer, qreal targetScale, Item *item, const QRectF &geometry)
{
    const QSize textureSize = scaledRect(geometry, targetScale).toAlignedRect().size();
    if (!item || !geometry.isValid() || textureSize.isEmpty()) {
        return nullptr;
    }

    auto texture = std::make_unique<GLTexture>(GL_RGBA8, textureSize);
    texture->setFilter(GL_LINEAR);
    texture->setWrapMode(GL_CLAMP_TO_EDGE);
    texture->setYInverted(true);
    GLFramebuffer framebuffer(texture.get());
    if (!framebuffer.valid()) {
        return nullptr;
    }

    GLFramebuffer::pushFramebuffer(&framebuffer);
    const auto popFramebuffer = qScopeGuard([] {
        GLFramebuffer::popFramebuffer();
    });
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    WindowPaintData paintData(captureProjection(geometry, targetScale));
    // renderItem() 将传入项视为坐标树根节点；捕获子项时需补回其祖先的场景位移。
    const QPointF parentPosition = item->rootPosition() - item->position();
    paintData.setXTranslation(parentPosition.x());
    paintData.setYTranslation(parentPosition.y());
    // renderItem() 会覆盖根 Item 的 transform；快照捕获不得污染常态场景状态。
    const QMatrix4x4 originalTransform = item->transform();
    renderer->renderItem(item, Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), paintData);
    item->setTransform(originalTransform);

    return texture;
}
}

PreviewAnimator::PreviewAnimator() = default;

PreviewAnimator::~PreviewAnimator()
{
    clear();
}

PreviewAnimator::WindowTrack *PreviewAnimator::addWindow(EffectWindow *window)
{
    if (!window || window->isDesktop() || window->isDock() || window->isWaterMark()) {
        return nullptr;
    }
    const QRect source = window->frameGeometry().toRect();
    const QRect preview = fitRect(source, m_previewArea);
    if (!preview.isValid()) {
        return nullptr;
    }

    WindowTrack &track = m_windows.emplace_back();
    track.window = window;
    track.source = source;
    track.preview = preview;
    track.from = track.to = {preview, 0.0};
    track.visibility.emplace(window, EffectWindow::PAINT_DISABLED_BY_MINIMIZE | EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    track.blur = window->data(WindowForceBlurRole);
    window->setData(WindowForceBlurRole, true);
    return &track;
}

void PreviewAnimator::begin(const EffectWindowList &windows, EffectWindow *selected, EffectScreen *focus)
{
    clear();

    m_focus = focus;
    const bool multiScreen = effects->screens().size() > 1;

    for (EffectWindow *window : windows) {
        WindowTrack *track = addWindow(window);
        if (!track) {
            continue;
        }

        const bool hidden = window->isMinimized() || !window->isOnCurrentDesktop();
        if (multiScreen) {
            // 原窗口在本屏聚拢，焦点屏的预览从半尺寸独立淡入。
            if (!hidden) {
                track->gather = gatherTarget(*track, track->preview);
            }
            track->from = {halfRect(track->preview), 0.0};
        } else {
            track->from = {track->source, hidden ? 0.0 : 1.0};
        }
        track->to.opacity = window == selected ? 1.0 : 0.0;
    }

    if (find(selected) || (selected && selected->isDesktop())) {
        m_selected = selected;
    } else if (!m_windows.empty()) {
        m_selected = m_windows.front().window;
        for (WindowTrack &track : m_windows) {
            track.to.opacity = track.window == m_selected ? 1 : 0;
        }
    }

    if (!m_windows.empty()) {
        if (multiScreen) {
            m_gather.start(SwitchDuration);
            m_animation.start(SwitchDuration);
            m_state = State::Previewing;
        } else {
            m_animation.start(GatherDuration);
            m_state = State::Gathering;
        }
    }
}

void PreviewAnimator::syncWindows(const EffectWindowList &windows)
{
    if (m_state == State::Closing) {
        return;
    }
    std::erase_if(m_windows, [&](const WindowTrack &track) {
        if (windows.contains(track.window)) {
            return false;
        }
        restore(track);
        return true;
    });
    for (EffectWindow *window : windows) {
        if (!find(window)) {
            addWindow(window);
        }
    }
    if (!windows.contains(m_selected)) {
        m_selected = nullptr;
    }
    if (m_windows.empty()) {
        clear();
    } else if (!active()) {
        m_state = State::Previewing;
    }
}

void PreviewAnimator::setLayout(EffectScreen *focus, const QRect &area)
{
    if (m_focus == focus && m_previewArea == area) {
        return;
    }

    m_focus = focus;
    m_previewArea = area;

    if (m_state == State::Inactive || m_state == State::Closing) {
        return;
    }

    freeze();

    for (WindowTrack &track : m_windows) {
        track.preview = fitRect(track.source, m_previewArea);
        if (track.gather.isValid()) {
            track.gather = gatherTarget(track, track.preview);
        }
        track.to = {track.preview, track.window == m_selected ? 1.0 : 0.0};
    }

    m_animation.start(RelayoutDuration);
    m_state = State::Previewing;
}

void PreviewAnimator::windowResized(EffectWindow *window)
{
    if (m_state == State::Inactive || m_state == State::Closing) {
        return;
    }
    WindowTrack *track = find(window);
    if (!track) {
        return;
    }
    track->source = window->frameGeometry().toRect();
    const QRect preview = fitRect(track->source, m_previewArea);
    if (track->gather.isValid()) {
        track->gather = gatherTarget(*track, preview);
    }
    if (!preview.isValid() || track->preview == preview) {
        return;
    }
    track->preview = preview;
    if (window == m_selected) {
        freeze();
        track->to.geometry = preview;
        m_animation.start(RelayoutDuration);
        effects->addRepaintFull();
    }
}

bool PreviewAnimator::select(EffectWindow *selected)
{
    if (m_state == State::Inactive || m_state == State::Closing || selected == m_selected) {
        return false;
    }

    if (!find(selected) && !(selected && selected->isDesktop())) {
        return false;
    }

    EffectWindow *old = m_selected;
    m_selected = selected;

    const bool gathering = m_state == State::Gathering;
    if (gathering) {
        // Keep fading the original windows on their opening timeline. A new
        // selection can then fade in at the preview center on its own track.
        m_gather = m_animation;
    }
    freeze();
    for (WindowTrack &track : m_windows) {
        track.to = {track.preview, track.window == selected ? 1.0 : 0.0};
        if (gathering && track.window != old && track.from.opacity > 0.0) {
            track.gather = track.preview;
            track.from.opacity = 0.0;
        }

        if (track.window == old && !gathering) {
            track.to.geometry = track.from.geometry;
        } else if (track.window == selected && track.from.opacity <= 0.001) {
            track.from = {halfRect(track.preview), 0.0};
        }
    }
    // Start the fade at the input event; the first repaint can already show progress.
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
    m_animation.start(SwitchDuration, now);
    m_state = State::Previewing;

    return true;
}

void PreviewAnimator::close()
{
    if (m_state == State::Inactive || m_state == State::Closing) {
        return;
    }

    freeze();

    for (WindowTrack &track : m_windows) {
        // Let the scene exclude other minimized windows before building the
        // closing frames' stacking order, including the final handoff frame.
        if (track.window != m_selected) {
            track.visibility.reset();
            if (track.gather.isValid() && track.from.opacity <= 0.001) {
                track.from = gatheringWindow(track);
                track.gather = {};
            }
        }
        const bool visible = track.window == m_selected || (track.window && !track.window->isMinimized() && track.window->isOnCurrentDesktop());
        track.to = {track.window ? track.window->frameGeometry().toRect() : track.source, visible ? 1.0 : 0.0};
    }

    const WindowTrack *selected = find(m_selected);
    m_snapshot.capture(selected && selected->source.isValid() ? selected->window.data() : nullptr);

    m_close.start(CloseDuration);
    m_state = State::Closing;
}

void PreviewAnimator::cancel()
{
    clear();
}

void PreviewAnimator::advance(std::chrono::milliseconds time)
{
    // Cold composition can take longer than a whole switch. Resume the fade
    // from the last painted state instead of skipping straight to its endpoint.
    const auto advancePreview = [time](AltTabAnimation &animation) {
        const auto elapsed = animation.timeline.elapsed();
        animation.advance(time);
        if (animation.timeline.elapsed() - elapsed > MaxPreviewFrameStep) {
            animation.timeline.setElapsed(elapsed + MaxPreviewFrameStep);
            animation.advance(time);
        }
    };
    advancePreview(m_animation);
    if (m_gather.running) {
        advancePreview(m_gather);
        m_gather.finishIfDone();
    }
    m_close.advance(time);
    if (m_animation.finishIfDone() && m_state != State::Closing) {
        settle();
    }

    if (m_close.finishIfDone() && m_state == State::Closing) {
        clear();
    }
}

bool PreviewAnimator::active() const
{
    return m_state != State::Inactive;
}

bool PreviewAnimator::animating() const
{
    return m_animation.running || m_gather.running || m_close.running;
}

qreal PreviewAnimator::brightness() const
{
    const qreal progress = qBound(0.0, qreal(m_close.timeline.elapsed().count()) / BackgroundCloseDuration.count(), 1.0);
    return m_state == State::Closing ? 0.4 + 0.6 * s_closeEasing.valueForProgress(progress) : 0.4;
}

void PreviewAnimator::remove(EffectWindow *window)
{
    const bool selectedRemoved = m_selected == window;
    if (m_snapshot.window == window) {
        m_snapshot.clear();
    }

    for (auto it = m_windows.begin(); it != m_windows.end();) {
        if (it->window == window) {
            restore(*it);
            it = m_windows.erase(it);
        } else {
            ++it;
        }
    }

    if (m_windows.empty()) {
        clear();
        return;
    }

    if (selectedRemoved) {
        const auto selected = std::find_if(m_windows.begin(), m_windows.end(), [](const WindowTrack &track) {
            return track.window;
        });
        if (selected == m_windows.end()) {
            clear();
            return;
        }

        if (m_state == State::Closing) {
            m_selected = selected->window;
            return;
        }

        m_selected = nullptr;
        select(selected->window);
    }
}

void PreviewAnimator::prePaint(EffectWindow *window, WindowPrePaintData &data)
{
    if (WindowTrack *track = find(window)) {
        if (!window->data(WindowRadiusRole).isValid() && !window->data(WindowClipPathRole).isValid()) {
            const qreal radius = effectsEx->getOsRadius() * effectsEx->getOsScale();
            track->temporaryRadius = QPointF(radius, radius);
            window->setData(WindowRadiusRole, track->temporaryRadius);
        }

        data.setTransformed();
        data.setTranslucent();
    }
}

bool PreviewAnimator::paint(EffectWindow *window, int mask, QRegion region, WindowPaintData &data)
{
    WindowTrack *track = find(window);
    if (!track) {
        return false;
    }

    const auto paintAt = [&](const VisualState &state, WindowPaintData &paintData) {
        if (state.opacity <= 0.001 || !state.geometry.isValid() || window->width() <= 0 || window->height() <= 0) {
            return;
        }
        paintData.setOpacity(paintData.opacity() * state.opacity);
        paintData.setScale(QVector2D(qreal(state.geometry.width()) / window->width(), qreal(state.geometry.height()) / window->height()));
        paintData += QPoint(state.geometry.x() - window->x(), state.geometry.y() - window->y());
        effects->paintWindow(window, mask, region, paintData);
    };
    const bool drawOriginal = m_gather.running && track->gather.isValid();
    const bool previewHidden = track->from.opacity == 0 && track->to.opacity == 0;
    const bool previewInSnapshot = m_state == State::Closing && m_snapshot.window == window && m_snapshot.texture;
    if (previewHidden || previewInSnapshot) {
        if (drawOriginal) {
            paintAt(gatheringWindow(*track), data);
        }
        return true;
    }
    if (drawOriginal) {
        WindowPaintData original(data);
        // WindowPaintData 的拷贝构造不保留 shader 和渲染目标缩放。
        original.shader = data.shader;
        if (const auto scale = data.renderTargetScale()) {
            original.setRenderTargetScale(*scale);
        }
        paintAt(gatheringWindow(*track), original);
    }
    const VisualState state = sample(*track, m_state == State::Closing ? closeProgress(*track) : m_animation.value());
    paintAt(state, data);
    return true;
}

PreviewAnimator::VisualState PreviewAnimator::gatheringWindow(const WindowTrack &track) const
{
    const qreal progress = m_gather.value();
    return {interpolateRect(track.source, track.gather, progress), 1.0 - progress};
}

QRect PreviewAnimator::gatherTarget(const WindowTrack &track, const QRect &focusPreview) const
{
    const EffectScreen *screen = track.window ? track.window->screen() : nullptr;
    if (!screen || screen == m_focus) {
        return focusPreview;
    }

    const int side = scale(THUMBNAIL_SIDE_MARGIN);
    const int top = scale(PREVIEW_TOP_MARGIN);
    return fitRect(track.source, screen->geometry().adjusted(side, top, -side, -top));
}

PreviewAnimator::WindowTrack *PreviewAnimator::find(EffectWindow *window)
{
    const auto it = std::find_if(m_windows.begin(), m_windows.end(), [window](const WindowTrack &track) {
        return track.window == window;
    });
    return it == m_windows.end() ? nullptr : &*it;
}

void PreviewAnimator::freeze()
{
    const qreal progress = m_animation.value();
    for (WindowTrack &track : m_windows) {
        track.from = sample(track, progress);
    }
}

void PreviewAnimator::settle()
{
    m_state = State::Previewing;

    for (WindowTrack &track : m_windows) {
        track.from = track.to = {track.preview, track.window == m_selected ? 1.0 : 0.0};
    }
}

qreal PreviewAnimator::closeProgress(const WindowTrack &track) const
{
    const auto delay = track.window == m_selected ? std::chrono::milliseconds(0) : OtherCloseDelay;
    const qreal raw = qBound(0.0, qreal((m_close.timeline.elapsed() - delay).count()) / WindowCloseDuration.count(), 1.0);
    return s_closeEasing.valueForProgress(raw);
}

PreviewAnimator::VisualState PreviewAnimator::sample(const WindowTrack &track, qreal progress) const
{
    return {interpolateRect(track.from.geometry, track.to.geometry, progress),
            track.from.opacity + (track.to.opacity - track.from.opacity) * progress};
}

void PreviewAnimator::paintClosingSnapshot(ScreenPaintData &data)
{
    if (m_state != State::Closing || !m_snapshot.window || !m_snapshot.texture) {
        return;
    }

    // Closing 中轨道保持不变，选中窗口移除时先释放快照。
    WindowTrack *track = find(m_snapshot.window);
    if (!track) {
        return;
    }

    const qreal progress = closeProgress(*track);
    const VisualState state = sample(*track, progress);
    if (!state.geometry.isValid()) {
        return;
    }

    m_snapshot.captureActiveShadowIfReady();
    m_snapshot.paintShadow(data, state, progress);
    m_snapshot.paintContent(data, state);
}

void PreviewAnimator::restore(const WindowTrack &track)
{
    if (!track.window) {
        return;
    }

    track.window->setData(WindowForceBlurRole, track.blur);
    if (track.temporaryRadius.isValid() && track.window->data(WindowRadiusRole) == track.temporaryRadius) {
        track.window->setData(WindowRadiusRole, {});
    }
}

void PreviewAnimator::clear()
{
    for (const WindowTrack &track : m_windows) {
        restore(track);
    }

    m_snapshot.clear();
    m_windows.clear();
    m_animation.running = false;
    m_gather.running = false;
    m_close.running = false;
    m_state = State::Inactive;
    m_selected = nullptr;
    m_focus = nullptr;
}

PreviewAnimator::ClosingSnapshot::~ClosingSnapshot()
{
    if (shader) {
        effects->makeOpenGLContextCurrent();
        shader.reset();
        effects->doneOpenGLContextCurrent();
    }
}

void PreviewAnimator::ClosingSnapshot::capture(EffectWindow *target)
{
    clear();
    if (!target) {
        return;
    }

    const QRectF frameGeometry = target->frameGeometry();
    const bool coversScreen = target->screen()
        && frameGeometry.toAlignedRect() == target->screen()->geometry();
    if (!target->isMinimized() && !target->isFullScreen() && !coversScreen) {
        return;
    }

    ItemRenderer *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
    const qreal targetScale = renderer->renderTargetScale();

    WindowItem *windowItem = static_cast<EffectWindowImpl *>(target)->windowItem();
    ShadowItem *shadowItem = windowItem->shadowItem();
    const QRectF shadowGeometry = shadowItem ? shadowItem->mapToGlobal(shadowItem->boundingRect()) : QRectF();

    if (shaderFailed || !effects->makeOpenGLContextCurrent()) {
        return;
    }
    if (!shader) {
        shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                   QByteArray(),
                                                                   QStringLiteral(":/effects/alttabthumbnaillist/shaders/preview.frag"));
        shaderFailed = !shader || !shader->isValid();
        if (shaderFailed) {
            shader.reset();
            effects->doneOpenGLContextCurrent();
            return;
        }
    }

    auto contentTexture = captureItemTexture(renderer, targetScale, windowItem, frameGeometry);
    auto capturedShadow = contentTexture ? captureItemTexture(renderer, targetScale, shadowItem, shadowGeometry) : nullptr;
    effects->doneOpenGLContextCurrent();

    if (!contentTexture) {
        return;
    }

    window = target;
    this->frameGeometry = frameGeometry.toAlignedRect();
    this->shadowGeometry = shadowGeometry;
    const QVariant radiusData = target->data(WindowRadiusRole);
    radius = radiusData.isValid() ? radiusData.toPointF().x() : effectsEx->getOsRadius() * effectsEx->getOsScale();
    if (target->isFullScreen() || coversScreen) {
        radius = 0;
    }
    texture = std::move(contentTexture);
    awaitingActiveShadow = target->isMinimized() && capturedShadow;
    shadowTexture = std::move(capturedShadow);
}

void PreviewAnimator::ClosingSnapshot::captureActiveShadowIfReady()
{
    // TabBox 先关闭再激活窗口。保留原阴影，在激活后只补抓一次。
    if (!awaitingActiveShadow || effects->activeWindow() != window || window->isMinimized()) {
        return;
    }
    awaitingActiveShadow = false;
    auto *windowItem = static_cast<EffectWindowImpl *>(window.data())->windowItem();
    ShadowItem *shadow = windowItem->shadowItem();
    if (!shadow) {
        return;
    }
    auto *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
    const QPoint moved = window->frameGeometry().toAlignedRect().topLeft() - frameGeometry.topLeft();
    const QRectF inactive = shadowGeometry;
    // 两态边距可能不同。在同一窗口坐标系混合，避免把新阴影拉伸到旧边界。
    const QRectF bounds = inactive.united(shadow->mapToGlobal(shadow->boundingRect()).translated(-moved));
    activeShadowTexture = captureItemTexture(renderer, renderer->renderTargetScale(), shadow, bounds.translated(moved));
    if (activeShadowTexture) {
        inactiveShadowUV = QVector4D(bounds.width() / inactive.width(), bounds.height() / inactive.height(),
                                     (bounds.x() - inactive.x()) / inactive.width(), (bounds.y() - inactive.y()) / inactive.height());
        shadowGeometry = bounds;
    }
}

void PreviewAnimator::ClosingSnapshot::paintShadow(const ScreenPaintData &screen, const VisualState &state, qreal progress) const
{
    if (!shadowTexture || !frameGeometry.isValid() || !shadowGeometry.isValid()) {
        return;
    }
    const QRect &rect = state.geometry;
    const qreal opacity = state.opacity;
    const qreal xScale = qreal(rect.width()) / frameGeometry.width();
    const qreal yScale = qreal(rect.height()) / frameGeometry.height();
    const QRect shadowRect = shadowGeometry.toAlignedRect().translated(-frameGeometry.topLeft());
    const QRect targetShadowRect(qRound(rect.x() + shadowRect.x() * xScale), qRound(rect.y() + shadowRect.y() * yScale),
                                 qRound(shadowRect.width() * xScale), qRound(shadowRect.height() * yScale));
    GLShader *shadowShader = activeShadowTexture ? shader.get()
                                                 : ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::Modulate);
    const ShaderBinder binder(shadowShader);
    if (activeShadowTexture) {
        shadowShader->setUniform("activeShadow", 1);
        shadowShader->setUniform("inactiveShadowUV", inactiveShadowUV);
        shadowShader->setUniform("shadowProgress", float(progress));
        shadowShader->setUniform("opacity", float(opacity));
        glActiveTexture(GL_TEXTURE1);
        activeShadowTexture->bind();
        glActiveTexture(GL_TEXTURE0);
    } else {
        shadowShader->setUniform(GLShader::ModulationConstant, QVector4D(opacity, opacity, opacity, opacity));
    }
    shadowShader->setUniform(GLShader::ModelViewProjectionMatrix, textureProjection(screen, targetShadowRect));
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    shadowTexture->bind();
    shadowTexture->render(targetShadowRect, effects->renderTargetScale());
    shadowTexture->unbind();
    if (activeShadowTexture) {
        glActiveTexture(GL_TEXTURE1);
        activeShadowTexture->unbind();
        glActiveTexture(GL_TEXTURE0);
    }
    glDisable(GL_BLEND);
}

void PreviewAnimator::ClosingSnapshot::paintContent(const ScreenPaintData &screen, const VisualState &state) const
{
    const QRect &rect = state.geometry;
    const ShaderBinder binder(shader.get());
    shader->setUniform(GLShader::ModelViewProjectionMatrix, textureProjection(screen, rect));
    const qreal scaledRadius = frameGeometry.width() > 0
        ? radius * qreal(rect.width()) / frameGeometry.width() * effects->renderTargetScale()
        : 0;
    shader->setUniform("size", QVector2D(rect.width() * effects->renderTargetScale(), rect.height() * effects->renderTargetScale()));
    shader->setUniform("radius", float(scaledRadius));
    shader->setUniform("shadowProgress", -1.0f);
    shader->setUniform("opacity", float(state.opacity));

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    texture->bind();
    texture->render(rect, effects->renderTargetScale());
    texture->unbind();
    glDisable(GL_BLEND);
}

void PreviewAnimator::ClosingSnapshot::clear()
{
    if (texture || shadowTexture) {
        effects->makeOpenGLContextCurrent();
        texture.reset();
        shadowTexture.reset();
        activeShadowTexture.reset();
        effects->doneOpenGLContextCurrent();
    }

    window = nullptr;
    frameGeometry = {};
    shadowGeometry = {};
    inactiveShadowUV = {};
    radius = 0;
    awaitingActiveShadow = false;
}

} // namespace KWin
