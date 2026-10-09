/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowscreencastsource.h"
#include "screencastutils.h"

#include "composite.h"
#include "core/output.h"
#include "core/renderloop.h"
#include "deleted.h"
#include "kwineffects.h"
#include "kwingltexture.h"
#include "kwinglutils.h"
#include "scene/itemrenderer_opengl.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "wayland/dderestrict_interface.h"
#include "wayland_server.h"

namespace KWin
{

WindowScreenCastSource::WindowScreenCastSource(Window *window, QObject *parent)
    : ScreenCastSource(parent)
    , m_window(window)
    , m_offscreenRef(window)
{
    connect(m_window, &Window::windowClosed, this, &ScreenCastSource::closed);
}

bool WindowScreenCastSource::hasAlphaChannel() const
{
    return true;
}

QSize WindowScreenCastSource::textureSize() const
{
    return m_window->clientGeometry().size().toSize();
}

void WindowScreenCastSource::render(spa_data *spa, spa_video_format format)
{
    GLTexture offscreenTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize());
    GLFramebuffer offscreenTarget(&offscreenTexture);

    render(&offscreenTarget);
    grabTexture(&offscreenTexture, spa, format);
}

void WindowScreenCastSource::render(GLFramebuffer *target)
{
    const QRectF geometry = m_window->clientGeometry();
    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(geometry.x(), geometry.x() + geometry.width(),
                           geometry.y(), geometry.y() + geometry.height(), -1, 1);

    WindowPaintData data;
    data.setProjectionMatrix(projectionMatrix);
    data.setRenderTargetScale(1.0);

    auto *ddeRestrict = waylandServer()->ddeRestrict();
    const bool protectedContent = !ddeRestrict || ddeRestrict->prohibitScreencast()
        || m_window->isProhibitScreenshotWindow();
    auto *renderer = Compositor::self()->scene()->renderer();

    GLFramebuffer::pushFramebuffer(target);
    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    const float background = protectedContent ? 0xD4 / 255.f : 0.f;
    glClearColor(background, background, background, protectedContent ? 1.f : 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (protectedContent) {
        if (auto *glRenderer = dynamic_cast<ItemRendererOpenGL *>(renderer)) {
            QMatrix4x4 projection;
            projection.ortho(0, target->size().width(), 0, target->size().height(), -1, 1);
            glRenderer->renderProhibitMask(target->size(), projection);
        }
    } else {
        renderer->renderItem(m_window->windowItem(), Scene::PAINT_WINDOW_TRANSFORMED, infiniteRegion(), data);
    }
    if (scissorEnabled) {
        glEnable(GL_SCISSOR_TEST);
    }
    GLFramebuffer::popFramebuffer();
    Q_EMIT frameRendered();
}

std::chrono::nanoseconds WindowScreenCastSource::clock() const
{
    return m_window->output()->renderLoop()->lastPresentationTimestamp();
}

} // namespace KWin
