// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "platformcontextwayland.h"
#include "integration.h"
#include "window.h"

namespace KWin
{

namespace QPA
{

PlatformContextWayland::PlatformContextWayland(QOpenGLContext *context, Integration *integration)
    : AbstractPlatformContext(context, integration, integration->eglDisplay())
{
    create();
}

bool PlatformContextWayland::makeCurrent(QPlatformSurface *surface)
{
    Window *window = static_cast<Window*>(surface);
    EGLSurface s = window->eglSurface();
    if (s == EGL_NO_SURFACE) {
        window->createEglSurface(eglDisplay(), config());
        s = window->eglSurface();
        if (s == EGL_NO_SURFACE) {
            return false;
        }
    }
    return eglMakeCurrent(eglDisplay(), s, s, eglContext());
}

bool PlatformContextWayland::isSharing() const
{
    return false;
}

void PlatformContextWayland::swapBuffers(QPlatformSurface *surface)
{
    Window *window = static_cast<Window*>(surface);
    EGLSurface s = window->eglSurface();
    if (s == EGL_NO_SURFACE) {
        return;
    }
    eglSwapBuffers(eglDisplay(), s);
}

void PlatformContextWayland::create()
{
    if (config() == 0) {
        return;
    }
    if (!bindApi()) {
        return;
    }
    createContext();
}

}
}
