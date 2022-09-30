// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sharingplatformcontext.h"
#include "integration.h"
#include "window.h"
#include "../../platform.h"
#include "../../wayland_server.h"
#include "../../shell_client.h"
#include <logging.h>

#include <QOpenGLFramebufferObject>
#include <private/qopenglcontext_p.h>

namespace KWin
{

namespace QPA
{

SharingPlatformContext::SharingPlatformContext(QOpenGLContext *context)
    : SharingPlatformContext(context, EGL_NO_SURFACE)
{
}

SharingPlatformContext::SharingPlatformContext(QOpenGLContext *context, const EGLSurface &surface, EGLConfig config)
    : AbstractPlatformContext(context, kwinApp()->platform()->sceneEglDisplay(), config)
    , m_surface(surface)
{
    create();
}

bool SharingPlatformContext::makeCurrent(QPlatformSurface *surface)
{
    Window *window = static_cast<Window*>(surface);

    // QOpenGLContext::makeCurrent in Qt5.12 calls platfrom->setContext before setCurrentContext
    // but binding the content FBO looks up the format from the current context, so we need // to make sure sure Qt knows what the correct one is already
    QOpenGLContextPrivate::setCurrentContext(context());
    if (eglMakeCurrent(eglDisplay(), m_surface, m_surface, eglContext())) {
        if (m_needBindFbo) {
            window->bindContentFBO();
        }
        return true;
    }
    qCWarning(KWIN_QPA) << "Failed to make context current";
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_QPA) << "EGL error code: " << error;
    }

    return false;
}

bool SharingPlatformContext::isSharing() const
{
    return false;
}

void SharingPlatformContext::swapBuffers(QPlatformSurface *surface)
{
    Window *window = static_cast<Window*>(surface);
    auto c = window->shellClient();
    if (!c) {
        qCDebug(KWIN_QPA) << "SwapBuffers called but there is no ShellClient";
        return;
    }
    // makeCurrent will result into calling SharingPlatformContext::makeCurrent,
    // which unconditionally binds fbo, but may be replace promptly by upcoming swapFBO
    m_needBindFbo = false;
    context()->doneCurrent();
    context()->makeCurrent(surface->surface());
    glFlush();
    c->setInternalFramebufferObject(window->swapFBO());
    window->bindContentFBO();
    m_needBindFbo = true;
}

GLuint SharingPlatformContext::defaultFramebufferObject(QPlatformSurface *surface) const
{
    if (Window *window = dynamic_cast<Window*>(surface)) {
        const auto &fbo = window->contentFBO();
        if (!fbo.isNull()) {
            return fbo->handle();
        }
    }
    qCDebug(KWIN_QPA) << "No default framebuffer object for internal window";
    return 0;
}

void SharingPlatformContext::create()
{
    if (config() == 0) {
        qCWarning(KWIN_QPA) << "Did not get an EGL config";
        return;
    }
    if (!bindApi()) {
        qCWarning(KWIN_QPA) << "Could not bind API.";
        return;
    }
    createContext(kwinApp()->platform()->sceneEglContext());
}

}
}
