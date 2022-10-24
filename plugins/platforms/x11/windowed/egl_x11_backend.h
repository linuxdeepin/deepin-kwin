// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_EGL_X11_BACKEND_H
#define KWIN_EGL_X11_BACKEND_H
#include "eglonxbackend.h"

namespace KWin
{

class X11WindowedBackend;

/**
 * @brief OpenGL Backend using Egl windowing system over an X overlay window.
 **/
class EglX11Backend : public EglOnXBackend
{
public:
    explicit EglX11Backend(X11WindowedBackend *backend);
    virtual ~EglX11Backend();
    virtual QRegion prepareRenderingFrame();
    virtual void endRenderingFrame(const QRegion &damage, const QRegion &damagedRegion);
    virtual bool usesOverlayWindow() const override;
    bool perScreenRendering() const override;
    QRegion prepareRenderingForScreen(int screenId) override;
    void endRenderingFrameForScreen(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;

protected:
    virtual void present();
    void cleanupSurfaces() override;
    bool createSurfaces() override;

private:
    void setupViewport(int screenId);
    QVector<EGLSurface> m_surfaces;
    X11WindowedBackend *m_backend;
};

} // namespace

#endif
