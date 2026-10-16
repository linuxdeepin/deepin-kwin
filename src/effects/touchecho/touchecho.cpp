/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "touchecho.h"

#include <QAction>
#include <QGuiApplication>
#include <QScreen>
#include <kwinglutils.h>

#include <KConfigGroup>
#include <KGlobalAccel>

#include <QPainter>

#include <cmath>

namespace KWin
{

TouchEchoEffect::TouchEchoEffect()
    : Effect()
{
}

TouchEchoEffect::~TouchEchoEffect() = default;


void TouchEchoEffect::touchDownNotify(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    TouchPoint point;
    point.id = id;
    point.pos = pos;
    m_points << point;
    repaint();
}

void TouchEchoEffect::touchMotionNotify(qint32 id, const QPointF &pos, std::chrono::microseconds time)
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
}

void TouchEchoEffect::touchUpNotify(qint32 id, std::chrono::microseconds time)
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
}

void TouchEchoEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
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

void TouchEchoEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    paintScreenSetup(mask, region, data);
    for (const auto &point : m_points) {
        const QPointF pos = mapToDisplay(point.pos);
        drawCircle(PointAlpha, pos.x(), pos.y(), PointSize);
    }
    for (const auto &dyingPoint : m_dyingPoints) {
        std::chrono::milliseconds life = dyingPoint.expiration - currentTime();
        float dyingFactor = life.count() / static_cast<float>(PointLife.count());
        float alpha = PointAlpha * dyingFactor;
        float radius = PointSize * dyingFactor;
        const QPointF pos = mapToDisplay(dyingPoint.pos);
        drawCircle(alpha, pos.x(), pos.y(), radius);
    }
    paintScreenFinish(mask, region, data);
}

void TouchEchoEffect::postPaintScreen()
{
    effects->postPaintScreen();
    repaint();
}

QPointF TouchEchoEffect::mapToDisplay(const QPointF &logicalPos) const
{
    const QMatrix4x4 transform = effects->inputTransform();
    if (transform.isIdentity()) {
        return logicalPos;
    }
    // inputTransform is the CTM in normalized [0,1] space.  Touch coordinates
    // arrive in logical pixel space; normalize, apply the inverse (logical→display),
    // then denormalize back to pixels.
    const QSize screenSize = effects->virtualScreenSize();
    if (screenSize.isEmpty()) {
        // Nothing to normalize against; fall back to the unmodified position.
        return logicalPos;
    }
    bool invertible = false;
    const QMatrix4x4 inverse = transform.inverted(&invertible);
    if (!invertible) {
        // A singular transform has no meaningful logical→display mapping.
        return logicalPos;
    }
    const QPointF normalized(logicalPos.x() / screenSize.width(),
                             logicalPos.y() / screenSize.height());
    const QPointF displayNorm = inverse.map(normalized);
    const QPointF display(displayNorm.x() * screenSize.width(),
                          displayNorm.y() * screenSize.height());
    return display;
}

void TouchEchoEffect::repaint()
{
    QRegion dirtyRegion;
    // QPainter's outer border extends half a pen width beyond its radius.
    // Scale the full painted bounds and round outwards.
    const qreal radius = (PointSize + 1.5 * LineWidth) * effectScaleFactor();
    for (const auto &p : m_points) {
        // The circle is painted at its display position, so the damage has to
        // use the same mapping; the bounds keep the display scale factor.
        const QPointF pos = mapToDisplay(p.pos);
        dirtyRegion |= QRectF(pos.x() - radius, pos.y() - radius, 2 * radius, 2 * radius).toAlignedRect();
    }
    for (const auto &p : m_dyingPoints) {
        const QPointF pos = mapToDisplay(p.pos);
        dirtyRegion |= QRectF(pos.x() - radius, pos.y() - radius, 2 * radius, 2 * radius).toAlignedRect();
    }
    effects->addRepaint(dirtyRegion);
}

bool TouchEchoEffect::isActive() const
{
    return !m_points.isEmpty() || !m_dyingPoints.isEmpty();
}

void TouchEchoEffect::drawCircle(float alpha, float cx, float cy, float r)
{
    if (effects->isOpenGLCompositing()) {
        drawCircleGl(alpha, cx, cy, r);
    } else if (effects->compositingType() == QPainterCompositing) {
        drawCircleQPainter(alpha, cx, cy, r);
    }
}

void TouchEchoEffect::paintScreenSetup(int mask, QRegion region, ScreenPaintData &data)
{
    if (effects->isOpenGLCompositing()) {
        paintScreenSetupGl(mask, region, data);
    }
}

void TouchEchoEffect::paintScreenFinish(int mask, QRegion region, ScreenPaintData &data)
{
    if (effects->isOpenGLCompositing()) {
        paintScreenFinishGl(mask, region, data);
    }
}

qreal TouchEchoEffect::effectScaleFactor()
{
    return qMax(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);
}

void TouchEchoEffect::drawCircleGl(float alpha, float cx, float cy, float r)
{
    static const int num_segments = 80;
    static const float theta = 2 * 3.1415926f / float(num_segments);
    const float scaledR = r * effectScaleFactor();
    const float penWidth = 1.0 * effectScaleFactor();
    const float innerOuterR = scaledR;
    const float innerInnerR = std::max(0.0f, scaledR - penWidth);
    const float outerInnerR = scaledR;
    const float outerOuterR = scaledR + penWidth;

    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setUseColor(true);

    // Filled circle
    {
        QVector<float> verts;
        verts.reserve((num_segments + 2) * 2);
        verts << cx << cy;
        for (int i = 0; i <= num_segments; ++i) {
            const float ang = theta * i;
            const float vx = cx + scaledR * cosf(ang);
            const float vy = cy + scaledR * sinf(ang);
            verts << vx << vy;
        }
        QColor fill(CircleFillColor);
        fill.setAlphaF(std::max(0.0f, std::min(1.0f, alpha)));
        vbo->setColor(fill);
        vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
        vbo->render(GL_TRIANGLE_FAN);
    }

    // Inner white border
    if (innerInnerR < innerOuterR) {
        QVector<float> verts;
        verts.reserve((num_segments + 1) * 4);
        for (int i = 0; i <= num_segments; ++i) {
            const float ang = theta * i;
            const float ox = cx + innerOuterR * cosf(ang);
            const float oy = cy + innerOuterR * sinf(ang);
            const float ix = cx + innerInnerR * cosf(ang);
            const float iy = cy + innerInnerR * sinf(ang);
            verts << ox << oy;
            verts << ix << iy;
        }
        QColor innerCol(InnerBorderColor);
        innerCol.setAlphaF(std::max(0.0f, std::min(1.0f, alpha * InnerBorderAlpha)));
        vbo->setColor(innerCol);
        vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
        vbo->render(GL_TRIANGLE_STRIP);
    }

    // Outer black border
    {
        QVector<float> verts;
        verts.reserve((num_segments + 1) * 4);
        for (int i = 0; i <= num_segments; ++i) {
            const float ang = theta * i;
            const float ix = cx + outerInnerR * cosf(ang);
            const float iy = cy + outerInnerR * sinf(ang);
            const float ox = cx + outerOuterR * cosf(ang);
            const float oy = cy + outerOuterR * sinf(ang);
            verts << ix << iy;
            verts << ox << oy;
        }
        QColor outerCol(OuterBorderColor);
        outerCol.setAlphaF(std::max(0.0f, std::min(1.0f, alpha * OuterBorderAlpha)));
        vbo->setColor(outerCol);
        vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
        vbo->render(GL_TRIANGLE_STRIP);
    }
}

void TouchEchoEffect::drawCircleQPainter(float alpha, float cx, float cy, float r)
{
    QPainter *painter = effects->scenePainter();
    const float scaledR = r * effectScaleFactor();
    const float penWidth = 1.0 * effectScaleFactor();
    const float innerR = scaledR;
    const float outerR = scaledR + penWidth;

    painter->save();

    // Filled circle
    {
        QColor fill(CircleFillColor);
        fill.setAlphaF(std::max(0.0f, std::min(1.0f, alpha)));
        painter->setBrush(fill);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(QPointF(cx, cy), scaledR, scaledR);
    }

    // Inner white border
    {
        QColor inner(InnerBorderColor);
        inner.setAlphaF(std::max(0.0f, std::min(1.0f, alpha * InnerBorderAlpha)));
        QPen penInner(inner);
        penInner.setWidthF(1.0 * effectScaleFactor());
        painter->setPen(penInner);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), innerR, innerR);
    }

    // Outer black border
    {
        QColor outer(OuterBorderColor);
        outer.setAlphaF(std::max(0.0f, std::min(1.0f, alpha * OuterBorderAlpha)));
        QPen penOuter(outer);
        penOuter.setWidthF(1.0 * effectScaleFactor());
        painter->setPen(penOuter);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(cx, cy), outerR, outerR);
    }

    painter->restore();
}

void TouchEchoEffect::paintScreenSetupGl(int, QRegion, ScreenPaintData &data)
{
    GLShader *shader = ShaderManager::instance()->pushShader(ShaderTrait::UniformColor);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, data.projectionMatrix());

    glLineWidth(1.0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TouchEchoEffect::paintScreenFinishGl(int, QRegion, ScreenPaintData &)
{
    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();
}
std::chrono::milliseconds TouchEchoEffect::currentTime() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}
} // namespace
