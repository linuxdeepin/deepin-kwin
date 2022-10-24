// Copyright (C) 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "scene_qpainter_wayland_backend.h"
#include "composite.h"
#include "logging.h"
#include "wayland_backend.h"
#include <KWayland/Client/buffer.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

namespace KWin
{

WaylandQPainterBackend::WaylandQPainterBackend(Wayland::WaylandBackend *b)
    : QPainterBackend()
    , m_backend(b)
    , m_needsFullRepaint(true)
    , m_backBuffer(QImage(QSize(), QImage::Format_RGB32))
    , m_buffer()
{
    connect(b->shmPool(), SIGNAL(poolResized()), SLOT(remapBuffer()));
    connect(b, &Wayland::WaylandBackend::shellSurfaceSizeChanged,
            this, &WaylandQPainterBackend::screenGeometryChanged);
    connect(b->surface(), &KWayland::Client::Surface::frameRendered,
            Compositor::self(), &Compositor::bufferSwapComplete);
}

WaylandQPainterBackend::~WaylandQPainterBackend()
{
    if (m_buffer) {
        m_buffer.toStrongRef()->setUsed(false);
    }
}

bool WaylandQPainterBackend::usesOverlayWindow() const
{
    return false;
}

void WaylandQPainterBackend::present(int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    if (m_backBuffer.isNull()) {
        return;
    }
    Compositor::self()->aboutToSwapBuffers();
    m_needsFullRepaint = false;
    auto s = m_backend->surface();
    s->attachBuffer(m_buffer);
    s->damage(damage);
    s->commit();
}

void WaylandQPainterBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    if (!m_buffer) {
        return;
    }
    m_buffer.toStrongRef()->setUsed(false);
    m_buffer.clear();
}

QImage *WaylandQPainterBackend::buffer()
{
    return &m_backBuffer;
}

void WaylandQPainterBackend::prepareRenderingFrame()
{
    if (m_buffer) {
        auto b = m_buffer.toStrongRef();
        if (b->isReleased()) {
            // we can re-use this buffer
            b->setReleased(false);
            return;
        } else {
            // buffer is still in use, get a new one
            b->setUsed(false);
        }
    }
    m_buffer.clear();
    const QSize size(m_backend->shellSurfaceSize());
    m_buffer = m_backend->shmPool()->getBuffer(size, size.width() * 4);
    if (!m_buffer) {
        qCDebug(KWIN_WAYLAND_BACKEND) << "Did not get a new Buffer from Shm Pool";
        m_backBuffer = QImage();
        return;
    }
    auto b = m_buffer.toStrongRef();
    b->setUsed(true);
    m_backBuffer = QImage(b->address(), size.width(), size.height(), QImage::Format_RGB32);
    m_backBuffer.fill(Qt::transparent);
    m_needsFullRepaint = true;
    qCDebug(KWIN_WAYLAND_BACKEND) << "Created a new back buffer";
}

void WaylandQPainterBackend::remapBuffer()
{
    if (!m_buffer) {
        return;
    }
    auto b = m_buffer.toStrongRef();
    if (!b->isUsed()){
        return;
    }
    const QSize size = m_backBuffer.size();
    m_backBuffer = QImage(b->address(), size.width(), size.height(), QImage::Format_RGB32);
    qCDebug(KWIN_WAYLAND_BACKEND) << "Remapped our back buffer";
}

bool WaylandQPainterBackend::needsFullRepaint() const
{
    return m_needsFullRepaint;
}

}
