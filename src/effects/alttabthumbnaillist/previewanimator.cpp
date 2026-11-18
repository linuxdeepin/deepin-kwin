// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "previewanimator.h"

#include "window.h"

#include <algorithm>

namespace KWin
{

namespace
{
constexpr auto GatherDuration = std::chrono::milliseconds(300);
constexpr auto SwitchDuration = std::chrono::milliseconds(400);
constexpr auto RelayoutDuration = std::chrono::milliseconds(150);

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

constexpr auto MaxPreviewFrameStep = std::chrono::milliseconds(50);

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

void PreviewAnimator::cancel()
{
    clear();
}

void PreviewAnimator::advance(std::chrono::milliseconds time)
{
    // Cold composition can take longer than a whole switch. Resume the fade
    // from the last painted state instead of skipping straight to its endpoint.
    const auto advancePreview = [time](Animation &animation) {
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

void PreviewAnimator::remove(EffectWindow *window)
{
    const bool selectedRemoved = m_selected == window;
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

PreviewAnimator::VisualState PreviewAnimator::sample(const WindowTrack &track, qreal progress) const
{
    return {interpolateRect(track.from.geometry, track.to.geometry, progress),
            track.from.opacity + (track.to.opacity - track.from.opacity) * progress};
}

void PreviewAnimator::restore(const WindowTrack &track)
{
    if (!track.window) {
        return;
    }

    track.window->setData(WindowForceBlurRole, track.blur);
}

void PreviewAnimator::clear()
{
    for (const WindowTrack &track : m_windows) {
        restore(track);
    }

    m_windows.clear();
    m_animation.running = false;
    m_gather.running = false;
    m_close.running = false;
    m_state = State::Inactive;
    m_selected = nullptr;
    m_focus = nullptr;
}

} // namespace KWin
