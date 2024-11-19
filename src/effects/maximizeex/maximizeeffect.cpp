/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "maximizeeffect.h"
#include "../utils/common.h"

#include "effects.h"
#include "scene/itemrenderer.h"
#include "scene/windowitem.h"

namespace MaxiConsts {
    const QEasingCurve TOGGLE_MODE =  QEasingCurve::OutQuart;// AnimationMode.EASE_OUT_Expo;
    static const int FADE_DURATION = 350;
}

namespace KWin
{
MaximizeEffect::MaximizeEffect()
{
    connect(effects, &EffectsHandler::windowMaximizedStateChanged, this, &MaximizeEffect::slotWindowMaximizedStateChanged);
    connect(effects, &EffectsHandler::windowMaximizedStateAboutToChange, this, &MaximizeEffect::slotWindowMaximizedStateAboutToChange);

    m_animationTime.setDuration(std::chrono::milliseconds(static_cast<int>(animationTime(MaxiConsts::FADE_DURATION))));
    m_animationTime.setDirection(TimeLine::Forward);
    m_animationTime.setEasingCurve(MaxiConsts::TOGGLE_MODE);
}

MaximizeEffect::~MaximizeEffect()
{
}

void MaximizeEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (isActive()) {
        m_animationTime.advance(presentTime);
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant(true));
    }

    effects->prePaintScreen(data, presentTime);
}

void MaximizeEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
}

void MaximizeEffect::postPaintScreen()
{
    if (m_activated && m_animationTime.done()) {
        setActive(false);
    }
    if (m_activated)
        effects->addRepaintFull();

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
    }
    effects->postPaintScreen();
}

void MaximizeEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    data.mask |= PAINT_WINDOW_TRANSFORMED;

    effects->prePaintWindow(w, data, presentTime);
}

void MaximizeEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!isActive()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (w->isWaterMark()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    auto area = effects->clientArea(MaximizeFullArea, w);
    QRegion reg(area.toRect());
    if (m_maxiWin == w) {
        float t = m_animationTime.value();
        QRect r = m_newGeo.toRect();
        QRect r1 = m_oldGeo.toRect();
        QPoint p0 = r.topLeft();
        QPoint p1 = QPoint(r1.x(), r1.y());

        float w0 = r.width();
        float w1 = r1.width();
        float wt = w0 - (w0 - w1)*(1-t);
        float k = wt/w0;

        float h0 = r.height();
        float h1 = r1.height();
        float ht = h0 - (h0 - h1)*(1-t);
        float k1 = ht/h0;

        QPoint p = (p1-p0) * (1-t) + p0;
        data.setScale(QVector2D(k,k1));
        data += QPoint(qRound(p.x() - w->x()), qRound(p.y() - w->y()));
    }
    effects->paintWindow(w, mask, reg, data);
}

void MaximizeEffect::drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    effects->drawWindow(w, mask, region, data);

    float t = m_animationTime.value();
    if (m_maxiWin != w || !m_texture || m_texture->isNull() || t > (effects->waylandDisplay() ? 0.2 : 0.7) || !m_isMaximized)
        return;

    QPoint p0 = QPoint(w->x(), w->y());
    QPoint p1 = m_oldGeo.toRect().topLeft();
    QPoint p = p1 - (p1-p0) * t;

    float w0 = w->width();
    float w1 = m_oldGeo.width();
    float wt = (w0-w1) * t + w1;

    float h0 = w->height();
    float h1 = m_oldGeo.height();
    float ht = (h0 - h1) * t + h1;

    m_texture->bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    auto s = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
    QMatrix4x4 mvp = data.projectionMatrix();
    mvp.translate(p.x(), p.y());
    s->setUniform(GLShader::ModelViewProjectionMatrix, mvp);
    const qreal scale = 1;
    const QRectF rect = QRect(p, QSize(wt, ht));
    m_texture->render(rect.toRect(), scale);
    ShaderManager::instance()->popShader();
    m_texture->unbind();
    glDisable(GL_BLEND);
}

bool MaximizeEffect::isActive() const
{
    return m_activated && !effects->isScreenLocked();
}

void MaximizeEffect::setActive(bool active)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;

    m_activated = active;

    if (active) {
        effects->setActiveFullScreenEffect(this);
    } else {
        cleanup();
    }

    effects->addRepaintFull();
}

void MaximizeEffect::cleanup()
{
    if (m_activated)
        return;

    m_maxiWin = nullptr;
    m_texture.reset();
    m_fbo.reset();
    effects->setActiveFullScreenEffect(nullptr);
}

void MaximizeEffect::slotWindowMaximizedStateChanged(EffectWindow *window, bool horizontal, bool vertical)
{
    if (window != m_maxiWin || m_oldGeo == window->frameGeometry() || horizontal != vertical) {
        setActive(false);
        return;
    }

    m_newGeo = window->frameGeometry();
    m_animationTime.reset();
    m_isMaximized = horizontal && vertical;
    setActive(true);
}

void MaximizeEffect::slotWindowMaximizedStateAboutToChange(EffectWindow *window, bool horizontal, bool vertical)
{
    Window *w = window ? static_cast<EffectWindowImpl *>(window)->window() : nullptr;
    if (!w || w->isInteractiveMove() || horizontal != vertical) {
        setActive(false);
        return;
    }

    m_oldGeo = window->frameGeometry();
    m_maxiWin = window;

    effects->makeOpenGLContextCurrent();

    m_texture = std::make_unique<GLTexture>(GL_RGBA8, w->size().toSize());
    m_texture->setFilter(GL_LINEAR);
    m_texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_texture->setYInverted(true);

    if (m_fbo = std::make_unique<GLFramebuffer>(m_texture.get())) {
        GLFramebuffer::pushFramebuffer(m_fbo.get());

        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        ItemRenderer *renderer = static_cast<EffectsHandlerImpl *>(effects)->scene()->renderer();
        const qreal scale = renderer->renderTargetScale();
        QMatrix4x4 projectionMatrix;
        const QRectF geometry = w->frameGeometry();
        projectionMatrix.ortho(geometry.x() * scale, (geometry.x() + geometry.width()) * scale,
                               geometry.y() * scale, (geometry.y() + geometry.height()) * scale, -1, 1);
        WindowPaintData data;
        data.setProjectionMatrix(projectionMatrix);
        const int mask = Scene::PAINT_WINDOW_TRANSFORMED;
        renderer->renderItem(w->windowItem(), mask, infiniteRegion(), data);

        GLFramebuffer::popFramebuffer();
    } else {
        setActive(false);
    }

    effects->doneOpenGLContextCurrent();
}

}
