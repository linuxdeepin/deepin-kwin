// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SCENE_QPAINTER_DRM_BACKEND_H
#define KWIN_SCENE_QPAINTER_DRM_BACKEND_H
#include <platformsupport/scenes/qpainter/backend.h>
#include <QObject>
#include <QVector>

namespace KWin
{

class DrmBackend;
class DrmDumbBuffer;
class DrmOutput;

class DrmQPainterBackend : public QObject, public QPainterBackend
{
    Q_OBJECT
public:
    DrmQPainterBackend(DrmBackend *backend);
    virtual ~DrmQPainterBackend();

    QImage *buffer() override;
    QImage *bufferForScreen(int screenId) override;
    bool needsFullRepaint() const override;
    bool usesOverlayWindow() const override;
    void prepareRenderingFrame() override;
    void present(int mask, const QRegion &damage) override;
    bool perScreenRendering() const override;

private:
    void initOutput(DrmOutput *output);
    struct Output {
        DrmDumbBuffer *buffer[2];
        DrmOutput *output;
        int index = 0;
    };
    QVector<Output> m_outputs;
    DrmBackend *m_backend;
};
}

#endif
