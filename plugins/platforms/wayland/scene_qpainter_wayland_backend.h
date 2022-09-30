// Copyright (C) 2013, 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SCENE_QPAINTER_WAYLAND_BACKEND_H
#define KWIN_SCENE_QPAINTER_WAYLAND_BACKEND_H

#include <platformsupport/scenes/qpainter/backend.h>

#include <QObject>
#include <QImage>
#include <QWeakPointer>

namespace KWayland
{
namespace Client
{
class Buffer;
}
}

namespace KWin
{
namespace Wayland
{
class WaylandBackend;
}

class WaylandQPainterBackend : public QObject, public QPainterBackend
{
    Q_OBJECT
public:
    explicit WaylandQPainterBackend(Wayland::WaylandBackend *b);
    virtual ~WaylandQPainterBackend();

    virtual void present(int mask, const QRegion& damage) override;
    virtual bool usesOverlayWindow() const override;
    virtual void screenGeometryChanged(const QSize &size) override;
    virtual QImage *buffer() override;
    virtual void prepareRenderingFrame() override;
    virtual bool needsFullRepaint() const override;
private Q_SLOTS:
    void remapBuffer();
private:
    Wayland::WaylandBackend *m_backend;
    bool m_needsFullRepaint;
    QImage m_backBuffer;
    QWeakPointer<KWayland::Client::Buffer> m_buffer;
};

}

#endif
