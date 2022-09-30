// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H
#include "abstract_egl_backend.h"

namespace KWin
{
class VirtualBackend;
class GLTexture;
class GLRenderTarget;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 **/
class EglGbmBackend : public AbstractEglBackend
{
public:
    EglGbmBackend(VirtualBackend *b);
    virtual ~EglGbmBackend();
    void screenGeometryChanged(const QSize &size) override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion prepareRenderingFrame() override;
    void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    bool usesOverlayWindow() const override;
    void init() override;

protected:
    void present() override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    VirtualBackend *m_backend;
    GLTexture *m_backBuffer = nullptr;
    GLRenderTarget *m_fbo = nullptr;
    int m_frameCounter = 0;
    friend class EglGbmTexture;
};

/**
 * @brief Texture using an EGLImageKHR.
 **/
class EglGbmTexture : public AbstractEglTexture
{
public:
    virtual ~EglGbmTexture();

private:
    friend class EglGbmBackend;
    EglGbmTexture(SceneOpenGLTexture *texture, EglGbmBackend *backend);
};

} // namespace

#endif
