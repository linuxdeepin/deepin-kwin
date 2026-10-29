/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "animation.h"

#include <QtMath>

namespace KWin
{

qreal easedValue(Easing easing, qreal t)
{
    t = qBound(0.0, t, 1.0);
    switch (easing) {
    case Easing::Linear:
        return t;
    case Easing::InQuad:
        return t * t;
    case Easing::OutQuad:
        return t * (2.0 - t);
    case Easing::InCubic:
        return t * t * t;
    case Easing::OutCubic: {
        const qreal u = 1.0 - t;
        return 1.0 - u * u * u;
    }
    case Easing::InOutCubic: {
        if (t < 0.5) {
            return 4.0 * t * t * t;
        }
        const qreal u = -2.0 * t + 2.0;
        return 1.0 - (u * u * u) / 2.0;
    }
    case Easing::InQuint:
        return t * t * t * t * t;
    case Easing::OutQuint: {
        const qreal u = 1.0 - t;
        return 1.0 - u * u * u * u * u;
    }
    }
    return t;
}

void Animation::configure(int durationMs, Easing easing)
{
    m_durationMs = durationMs;
    m_easing = easing;
    m_elapsedMs = 0;
    m_started = false;
}

void Animation::start()
{
    m_elapsedMs = 0;
    m_started = true;
}

void Animation::advance(int msDelta)
{
    if (!m_started || done()) {
        return;
    }
    if (msDelta < 0) {
        msDelta = 0;
    }
    m_elapsedMs += msDelta;
    if (m_elapsedMs > m_durationMs) {
        m_elapsedMs = m_durationMs;
    }
}

void Animation::reset()
{
    m_elapsedMs = 0;
    m_started = false;
}

qreal Animation::progress() const
{
    if (m_durationMs <= 0) {
        return done() ? 1.0 : 0.0;
    }
    return qBound(0.0, qreal(m_elapsedMs) / qreal(m_durationMs), 1.0);
}

qreal Animation::value() const
{
    return easedValue(m_easing, progress());
}

bool Animation::done() const
{
    return m_durationMs > 0 ? m_elapsedMs >= m_durationMs : m_started;
}

}
