// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SCENE_QPAINTER_FB_BACKEND_H
#define KWIN_SCENE_QPAINTER_FB_BACKEND_H
#include <platformsupport/scenes/qpainter/backend.h>

#include <QObject>
#include <QImage>

namespace KWin
{
class FramebufferBackend;

class FramebufferQPainterBackend : public QObject, public QPainterBackend
{
    Q_OBJECT
public:
    FramebufferQPainterBackend(FramebufferBackend *backend);
    virtual ~FramebufferQPainterBackend();

    QImage *buffer() override;
    bool needsFullRepaint() const override;
    bool usesOverlayWindow() const override;
    void prepareRenderingFrame() override;
    void present(int mask, const QRegion &damage) override;

private:
    QImage m_renderBuffer;
    QImage m_backBuffer;
    FramebufferBackend *m_backend;
};

}

#endif
