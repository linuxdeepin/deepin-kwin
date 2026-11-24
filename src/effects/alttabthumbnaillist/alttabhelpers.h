// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <kwineffects.h>
#include <kwineffectsex.h>

#include <QEasingCurve>
#include <QMatrix4x4>
#include <QRect>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <optional>

namespace KWin
{

constexpr int THUMBNAIL_SIDE_MARGIN = 20;
constexpr int THUMBNAIL_VIEW_BOTTOM_MARGIN = 60;
constexpr int THUMBNAIL_VIEW_HEIGHT = 198;
constexpr int THUMBNAIL_VIEW_SIDE_MARGIN = 10;
constexpr int PREVIEW_TOP_MARGIN = 30;

inline int scale(int value)
{
    return qRound(effectsEx->getOsScale() * value);
}

inline QMatrix4x4 captureProjection(const QRectF &geometry, qreal targetScale)
{
    QMatrix4x4 projection;
    projection.ortho(geometry.left() * targetScale, geometry.right() * targetScale,
                     geometry.top() * targetScale, geometry.bottom() * targetScale, -1, 1);
    return projection;
}

inline QMatrix4x4 textureProjection(const ScreenPaintData &screen, const QRect &rect)
{
    QMatrix4x4 projection(screen.projectionMatrix());
    const qreal targetScale = effects->renderTargetScale();
    projection.translate(rect.x() * targetScale, rect.y() * targetScale);
    return projection;
}

inline QRect interpolateRect(const QRect &from, const QRect &to, qreal t)
{
    const int x = qRound(from.x() + (to.x() - from.x()) * t);
    const int y = qRound(from.y() + (to.y() - from.y()) * t);
    const int width = qRound(from.width() + (to.width() - from.width()) * t);
    const int height = qRound(from.height() + (to.height() - from.height()) * t);

    return {x, y, width, height};
}

struct AltTabAnimation
{
    TimeLine timeline;
    bool running = false;

    void start(std::chrono::milliseconds duration, std::optional<std::chrono::milliseconds> timestamp = std::nullopt)
    {
        m_startTimestamp = timestamp;
        timeline.reset();
        timeline.setDuration(duration);
        timeline.setEasingCurve(QEasingCurve::OutExpo);
        running = true;
    }

    void advance(std::chrono::milliseconds time)
    {
        if (running) {
            if (m_startTimestamp) {
                // A late frame can still carry a presentation prediction from before the input.
                timeline.advance(std::min(*m_startTimestamp, time));
                m_startTimestamp.reset();
            }
            timeline.advance(time);
        }
    }

    qreal value() const
    {
        return timeline.value();
    }

    bool finishIfDone()
    {
        if (!running || !timeline.done()) {
            return false;
        }
        running = false;
        return true;
    }

private:
    std::optional<std::chrono::milliseconds> m_startTimestamp;
};

} // namespace KWin
