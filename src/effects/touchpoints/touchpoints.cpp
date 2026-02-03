/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "touchpoints.h"

#include <QAction>
#include <kwinglutils.h>

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QPainter>

#include <cmath>

namespace KWin
{

TouchPointsEffect::TouchPointsEffect()
    : Effect()
{
}

TouchPointsEffect::~TouchPointsEffect() = default;


bool TouchPointsEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    TouchPoint point;
    point.id = id;
    point.pos = pos;
    m_points << point;
    repaint();
    return false;
}

bool TouchPointsEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    auto pointIt = std::find_if(m_points.begin(), m_points.end(), [id](const TouchPoint &p) {
        return p.id == id;
    });
    if (pointIt != m_points.end()) {
        pointIt->pos = pos;
        repaint();
    } else {
        qWarning() << "Received touchMotion for unknown id" << id;
    }
    return false;
}

bool TouchPointsEffect::touchUp(qint32 id, std::chrono::microseconds time)
{
    auto pointIt = std::find_if(m_points.begin(), m_points.end(), [id](const TouchPoint &p) {
        return p.id == id;
    });
    if (pointIt != m_points.end()) {
        DyingTouchPoint dyingPoint;
        dyingPoint.pos = pointIt->pos;
        dyingPoint.expiration = PointLife + currentTime();
        m_dyingPoints << dyingPoint;
        m_points.erase(pointIt);
        repaint();
    } else {
        qWarning() << "Received touchUp for unknown id" << id;
    }
    return false;
}

void TouchPointsEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    auto it = m_dyingPoints.begin();
    while (it != m_dyingPoints.end()) {
        if (it->expiration <= currentTime()) {
            it = m_dyingPoints.erase(it);
        } else {
            it++;
        }
    }
    effects->prePaintScreen(data, presentTime);
}

void TouchPointsEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    paintScreenSetup(mask, region, data);
    for (const auto &point : m_points) {
        drawCircle(PointAlpha, point.pos.x(), point.pos.y(), PointSize);
    }
    for (const auto &dyingPoint : m_dyingPoints) {
        std::chrono::milliseconds life = dyingPoint.expiration - currentTime();
        float dyingFactor = life.count() / static_cast<float>(PointLife.count());
        float alpha = PointAlpha * dyingFactor;
        float radius = PointSize * dyingFactor;
        drawCircle(alpha, dyingPoint.pos.x(), dyingPoint.pos.y(), radius);
    }
    paintScreenFinish(mask, region, data);
}

void TouchPointsEffect::postPaintScreen()
{
    effects->postPaintScreen();
    repaint();
}

void TouchPointsEffect::repaint()
{
    QRegion dirtyRegion;
    constexpr int radius = PointSize + LineWidth;
    for (const auto &p : m_points) {
        dirtyRegion |= QRect(p.pos.x() - radius, p.pos.y() - radius, 2 * radius, 2 * radius);
    }
    for (const auto &p : m_dyingPoints) {
        dirtyRegion |= QRect(p.pos.x() - radius, p.pos.y() - radius, 2 * radius, 2 * radius);
    }
    effects->addRepaint(dirtyRegion);
}

bool TouchPointsEffect::isActive() const
{
    return !m_points.isEmpty() || !m_dyingPoints.isEmpty();
}

void TouchPointsEffect::drawCircle(float alpha, float cx, float cy, float r)
{
    if (effects->isOpenGLCompositing()) {
        drawCircleGl(alpha, cx, cy, r);
    } else if (effects->compositingType() == QPainterCompositing) {
        drawCircleQPainter(alpha, cx, cy, r);
    }
}

void TouchPointsEffect::paintScreenSetup(int mask, QRegion region, ScreenPaintData &data)
{
    if (effects->isOpenGLCompositing()) {
        paintScreenSetupGl(mask, region, data);
    }
}

void TouchPointsEffect::paintScreenFinish(int mask, QRegion region, ScreenPaintData &data)
{
    if (effects->isOpenGLCompositing()) {
        paintScreenFinishGl(mask, region, data);
    }
}

void TouchPointsEffect::drawCircleGl(float alpha, float cx, float cy, float r)
{
    // Draw filled translucent white circle with 1px inner white border (30% opacity)
    // and 1px outer black border (8% opacity). No shadow.
    static const int num_segments = 80;
    static const float theta = 2 * 3.1415926f / float(num_segments);
    const auto scale = effects->renderTargetScale();

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);

    // Clamp radii
    const float pixelThickness = std::max(1.0 / scale, 1.0 / 4.0);
    const float innerOuterR = r;
    const float innerInnerR = std::max(0.0f, r - pixelThickness);
    const float outerInnerR = r;
    const float outerOuterR = r + pixelThickness;

    // Filled circle (triangle fan)
    {
        QVector<float> verts;
        verts.reserve((num_segments + 2) * 2);
        verts << cx * scale << cy * scale;
        for (int i = 0; i <= num_segments; ++i) {
            const float ang = theta * i;
            const float vx = (cx + r * cosf(ang)) * scale;
            const float vy = (cy + r * sinf(ang)) * scale;
            verts << vx << vy;
        }
        QColor fill(232, 232, 232);
        fill.setAlphaF(std::max(0.0f, std::min(1.0f, alpha)));
        vbo->setColor(fill);
        vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
        vbo->render(GL_TRIANGLE_FAN);
    }

    // Inner 1px white border (draw as a thin ring using triangle strip)
    if (innerInnerR < innerOuterR) {
        QVector<float> verts;
        verts.reserve((num_segments + 1) * 4);
        for (int i = 0; i <= num_segments; ++i) {
            const float ang = theta * i;
            const float ox = (cx + innerOuterR * cosf(ang)) * scale;
            const float oy = (cy + innerOuterR * sinf(ang)) * scale;
            const float ix = (cx + innerInnerR * cosf(ang)) * scale;
            const float iy = (cy + innerInnerR * sinf(ang)) * scale;
            verts << ox << oy;
            verts << ix << iy;
        }
        QColor innerCol(255, 255, 255);
        innerCol.setAlphaF(0.30f);
        vbo->setColor(innerCol);
        vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
        vbo->render(GL_TRIANGLE_STRIP);
    }

    // Outer 1px black border
    {
        QVector<float> verts;
        verts.reserve((num_segments + 1) * 4);
        for (int i = 0; i <= num_segments; ++i) {
            const float ang = theta * i;
            const float ix = (cx + outerInnerR * cosf(ang)) * scale;
            const float iy = (cy + outerInnerR * sinf(ang)) * scale;
            const float ox = (cx + outerOuterR * cosf(ang)) * scale;
            const float oy = (cy + outerOuterR * sinf(ang)) * scale;
            verts << ix << iy;
            verts << ox << oy;
        }
        QColor outerCol(0, 0, 0);
        outerCol.setAlphaF(0.08f);
        vbo->setColor(outerCol);
        vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
        vbo->render(GL_TRIANGLE_STRIP);
    }
}

void TouchPointsEffect::drawCircleQPainter(float alpha, float cx, float cy, float r)
{
    QPainter *painter = effects->scenePainter();
    painter->save();
    // Filled translucent (RGB 232,232,232)
    QColor fill(232, 232, 232);
    fill.setAlphaF(std::max(0.0f, std::min(1.0f, alpha)));
    painter->setBrush(fill);
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(cx - r, cy - r, r * 2, r * 2);

    // Outer 1px black border (centered on edge)
    QColor outer(0, 0, 0, static_cast<int>(0.08f * 255));
    QPen penOuter(outer);
    penOuter.setWidthF(1.0);
    painter->setPen(penOuter);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(cx - r - 0.5, cy - r - 0.5, (r + 0.5) * 2, (r + 0.5) * 2);

    // Inner 1px white border (inside edge)
    QColor inner(255, 255, 255, static_cast<int>(0.30f * 255));
    QPen penInner(inner);
    penInner.setWidthF(1.0);
    painter->setPen(penInner);
    painter->drawEllipse(cx - r + 0.5, cy - r + 0.5, (r - 0.5) * 2, (r - 0.5) * 2);
    painter->restore();
}

void TouchPointsEffect::paintScreenSetupGl(int, QRegion, ScreenPaintData &data)
{
    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::UniformColor);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());

    glLineWidth(2);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TouchPointsEffect::paintScreenFinishGl(int, QRegion, ScreenPaintData &)
{
    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}
std::chrono::milliseconds TouchPointsEffect::currentTime() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}
} // namespace
