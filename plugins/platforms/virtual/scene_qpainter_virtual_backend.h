// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SCENE_QPAINTER_VIRTUAL_BACKEND_H
#define KWIN_SCENE_QPAINTER_VIRTUAL_BACKEND_H

#include <platformsupport/scenes/qpainter/backend.h>

#include <QObject>
#include <QVector>

namespace KWin
{

class VirtualBackend;

class VirtualQPainterBackend : public QObject, public QPainterBackend
{
    Q_OBJECT
public:
    VirtualQPainterBackend(VirtualBackend *backend);
    virtual ~VirtualQPainterBackend();

    QImage *buffer() override;
    QImage *bufferForScreen(int screenId) override;
    bool needsFullRepaint() const override;
    bool usesOverlayWindow() const override;
    void prepareRenderingFrame() override;
    void present(int mask, const QRegion &damage) override;
    bool perScreenRendering() const override;

private:
    void createOutputs();

    QVector<QImage> m_backBuffers;
    VirtualBackend *m_backend;
    int m_frameCounter = 0;
};

}

#endif
