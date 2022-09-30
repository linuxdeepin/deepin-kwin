// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "scene_qpainter_fb_backend.h"
#include "fb_backend.h"
#include "composite.h"
#include "logind.h"
#include "cursor.h"
#include "virtual_terminal.h"
// Qt
#include <QPainter>

namespace KWin
{
FramebufferQPainterBackend::FramebufferQPainterBackend(FramebufferBackend *backend)
    : QObject()
    , QPainterBackend()
    , m_renderBuffer(backend->size(), QImage::Format_RGB32)
    , m_backend(backend)
{
    m_renderBuffer.fill(Qt::black);

    m_backend->map();

    m_backBuffer = QImage((uchar*)backend->mappedMemory(),
                          backend->bytesPerLine() / (backend->bitsPerPixel() / 8),
                          backend->bufferSize() / backend->bytesPerLine(),
                          backend->bytesPerLine(), backend->imageFormat());

    m_backBuffer.fill(Qt::black);
    connect(VirtualTerminal::self(), &VirtualTerminal::activeChanged, this,
        [this] (bool active) {
            if (active) {
                Compositor::self()->bufferSwapComplete();
                Compositor::self()->addRepaintFull();
            } else {
                Compositor::self()->aboutToSwapBuffers();
            }
        }
    );
}

FramebufferQPainterBackend::~FramebufferQPainterBackend() = default;

QImage *FramebufferQPainterBackend::buffer()
{
    return &m_renderBuffer;
}

bool FramebufferQPainterBackend::needsFullRepaint() const
{
    return false;
}

void FramebufferQPainterBackend::prepareRenderingFrame()
{
}

void FramebufferQPainterBackend::present(int mask, const QRegion &damage)
{
    Q_UNUSED(mask)
    Q_UNUSED(damage)
    if (!LogindIntegration::self()->isActiveSession()) {
        return;
    }
    QPainter p(&m_backBuffer);
    p.drawImage(QPoint(0, 0), m_backend->isBGR() ? m_renderBuffer.rgbSwapped() : m_renderBuffer);
}

bool FramebufferQPainterBackend::usesOverlayWindow() const
{
    return false;
}

}
