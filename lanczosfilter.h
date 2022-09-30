// Copyright (C) 2010 Martin Gräßlin <mgraesslin@kde.org>
// Copyright (C) 2010 by Fredrik Höglund <fredrik@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_LANCZOSFILTER_P_H
#define KWIN_LANCZOSFILTER_P_H

#include <QObject>
#include <QBasicTimer>
#include <QVector>
#include <QVector2D>
#include <QVector4D>

#include <kwinconfig.h>
#include <kwin_export.h>

namespace KWin
{

class EffectWindow;
class EffectWindowImpl;
class WindowPaintData;
class GLTexture;
class GLRenderTarget;
class GLShader;

class KWIN_EXPORT LanczosFilter
    : public QObject
{
    Q_OBJECT

public:
    explicit LanczosFilter(QObject* parent = 0);
    ~LanczosFilter();
    void performPaint(EffectWindowImpl* w, int mask, QRegion region, WindowPaintData& data);

protected:
    virtual void timerEvent(QTimerEvent*);
private:
    void init();
    void updateOffscreenSurfaces();
    void setUniforms();
    void discardCacheTexture(EffectWindow *w);

    void createKernel(float delta, int *kernelSize);
    void createOffsets(int count, float width, Qt::Orientation direction);
    GLTexture *m_offscreenTex;
    GLRenderTarget *m_offscreenTarget;
    QBasicTimer m_timer;
    bool m_inited;
    QScopedPointer<GLShader> m_shader;
    int m_uOffsets;
    int m_uKernel;
    QVector2D m_offsets[16];
    QVector4D m_kernel[16];
};

} // namespace

#endif // KWIN_LANCZOSFILTER_P_H
