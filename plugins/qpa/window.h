// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_QPA_WINDOW_H
#define KWIN_QPA_WINDOW_H

#include <epoxy/egl.h>
#include "fixqopengl.h"

#include <fixx11h.h>
#include <qpa/qplatformwindow.h>
// wayland
#include <config-kwin.h>
#if HAVE_WAYLAND_EGL
#include <wayland-egl.h>
#endif

class QOpenGLFramebufferObject;

#if HAVE_WAYLAND_EGL
struct wl_egl_window;
#endif

namespace KWayland
{
namespace Client
{
class Surface;
class ShellSurface;
}
}

namespace KWin
{

class ShellClient;

namespace QPA
{

class Integration;

class Window : public QPlatformWindow
{
public:
    explicit Window(QWindow *window, KWayland::Client::Surface *surface, KWayland::Client::ShellSurface *shellSurface, const Integration *integration);
    virtual ~Window();

    void setVisible(bool visible) override;
    void setGeometry(const QRect &rect) override;
    WId winId() const override;

    KWayland::Client::Surface *surface() const {
        return m_surface;
    }
    EGLSurface eglSurface() const {
        return m_eglSurface;
    }
    void createEglSurface(EGLDisplay dpy, EGLConfig config);

    int scale() const;
    qreal devicePixelRatio() const override;

    void bindContentFBO();
    const QSharedPointer<QOpenGLFramebufferObject> &contentFBO() const {
        return m_contentFBO;
    }
    QSharedPointer<QOpenGLFramebufferObject> swapFBO();
    ShellClient *shellClient();

private:
    void unmap();
    void map();
    void createFBO();

    KWayland::Client::Surface *m_surface;
    KWayland::Client::ShellSurface *m_shellSurface;
    EGLSurface m_eglSurface = EGL_NO_SURFACE;
    QSharedPointer<QOpenGLFramebufferObject> m_contentFBO;
    bool m_resized = false;
    ShellClient *m_shellClient = nullptr;
#if HAVE_WAYLAND_EGL
    wl_egl_window *m_eglWaylandWindow = nullptr;
#endif
    quint32 m_windowId;
    const Integration *m_integration;
    int m_scale = 1;
};

}
}

#endif
