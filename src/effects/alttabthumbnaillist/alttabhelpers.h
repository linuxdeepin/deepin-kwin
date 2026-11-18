// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <kwineffects.h>
#include <kwineffectsex.h>

#include <QMatrix4x4>
#include <QRect>
#include <QtGlobal>

#include <chrono>

namespace KWin
{

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

} // namespace KWin
