/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#define WL_EGL_PLATFORM 1
#include "integration.h"
#include "window.h"
#include "screens.h"
#include "../../shell_client.h"
#include "../../wayland_server.h"
#include <logging.h>

#include <QOpenGLFramebufferObject>
#include <qpa/qwindowsysteminterface.h>

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/surface.h>

namespace KWin
{
namespace QPA
{
static quint32 s_windowId = 0;

Window::Window(QWindow *window, KWayland::Client::Surface *surface, KWayland::Client::ShellSurface *shellSurface, const Integration *integration)
    : QPlatformWindow(window)
    , m_surface(surface)
    , m_shellSurface(shellSurface)
    , m_windowId(++s_windowId)
    , m_integration(integration)
    , m_scale(screens()->maxScale())
{
    m_surface->setScale(m_scale);

    QObject::connect(m_surface, &QObject::destroyed, window, [this] { m_surface = nullptr;});
    QObject::connect(m_shellSurface, &QObject::destroyed, window, [this] { m_shellSurface = nullptr;});
    waylandServer()->internalClientConection()->flush();
}

Window::~Window()
{
    unmap();
    if (m_eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(m_integration->eglDisplay(), m_eglSurface);
    }
#if HAVE_WAYLAND_EGL
    if (m_eglWaylandWindow) {
        wl_egl_window_destroy(m_eglWaylandWindow);
    }
#endif
    delete m_shellSurface;
    delete m_surface;
}

WId Window::winId() const
{
    return m_windowId;
}

void Window::setVisible(bool visible)
{
    if (!visible) {
        unmap();
    } else {
        map();
    }
    QPlatformWindow::setVisible(visible);
}

void Window::setGeometry(const QRect &rect)
{
    const QRect &oldRect = geometry();
    QPlatformWindow::setGeometry(rect);
    if (rect.x() != oldRect.x()) {
        emit window()->xChanged(rect.x());
    }
    if (rect.y() != oldRect.y()) {
        emit window()->yChanged(rect.y());
    }
    if (rect.width() != oldRect.width()) {
        emit window()->widthChanged(rect.width());
    }
    if (rect.height() != oldRect.height()) {
        emit window()->heightChanged(rect.height());
    }

    const QSize nativeSize = rect.size() * m_scale;

    if (m_contentFBO) {
        if (m_contentFBO->size() != nativeSize) {
            m_resized = true;
        }
    }
#if HAVE_WAYLAND_EGL
    if (m_eglWaylandWindow) {
        wl_egl_window_resize(m_eglWaylandWindow, nativeSize.width(), nativeSize.height(), 0, 0);
    }
#endif
    QWindowSystemInterface::handleGeometryChange(window(), geometry());
}

void Window::map()
{
    if (!m_shellClient) {
        m_shellClient = waylandServer()->findClient(window());
    }
}

void Window::unmap()
{
    if (m_shellClient) {
        m_shellClient->setInternalFramebufferObject(QSharedPointer<QOpenGLFramebufferObject>());
    }
    if (m_surface) {
        m_surface->attachBuffer(KWayland::Client::Buffer::Ptr());
        m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    }
    if (waylandServer()->internalClientConection()) {
        waylandServer()->internalClientConection()->flush();
    }
}

void Window::createEglSurface(EGLDisplay dpy, EGLConfig config)
{
#if HAVE_WAYLAND_EGL
    const QSize size = window()->size() * m_scale;
    m_eglWaylandWindow = wl_egl_window_create(*m_surface, size.width(), size.height());
    if (!m_eglWaylandWindow) {
        return;
    }
    m_eglSurface = eglCreateWindowSurface(dpy, config, m_eglWaylandWindow, nullptr);
#else
    Q_UNUSED(dpy)
    Q_UNUSED(config)
#endif
}

void Window::bindContentFBO()
{
    if (m_resized || !m_contentFBO) {
        createFBO();
    }
    m_contentFBO->bind();
}

QSharedPointer<QOpenGLFramebufferObject> Window::swapFBO()
{
    auto fbo = m_contentFBO;
    m_contentFBO.clear();
    m_surface->commit(KWayland::Client::Surface::CommitFlag::None);
    return fbo;
}

void Window::createFBO()
{
    const QRect &r = geometry();
    if (m_contentFBO && r.size().isEmpty()) {
        return;
    }
    const QSize nativeSize = r.size() * m_scale;
    m_contentFBO.reset(new QOpenGLFramebufferObject(nativeSize.width(), nativeSize.height(), QOpenGLFramebufferObject::CombinedDepthStencil));
    if (!m_contentFBO->isValid()) {
        qCWarning(KWIN_QPA) << "Content FBO is not valid";
    }
    m_resized = false;
}

ShellClient *Window::shellClient()
{
    map();
    return m_shellClient;
}

int Window::scale() const
{
    return m_scale;
}

qreal Window::devicePixelRatio() const
{
    return m_scale;
}

}
}
