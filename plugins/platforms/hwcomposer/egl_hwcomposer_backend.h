// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_EGL_HWCOMPOSER_BACKEND_H
#define KWIN_EGL_HWCOMPOSER_BACKEND_H
#include "abstract_egl_backend.h"

namespace KWin
{

class HwcomposerBackend;
class HwcomposerWindow;

class EglHwcomposerBackend : public AbstractEglBackend
{
public:
    EglHwcomposerBackend(HwcomposerBackend *backend);
    virtual ~EglHwcomposerBackend();
    bool usesOverlayWindow() const override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    void screenGeometryChanged(const QSize &size) override;
    QRegion prepareRenderingFrame() override;
    void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void init() override;

protected:
    void present() override;

private:
    bool initializeEgl();
    bool initRenderingContext();
    bool initBufferConfigs();
    bool makeContextCurrent();
    HwcomposerBackend *m_backend;
    HwcomposerWindow *m_nativeSurface = nullptr;
};

class EglHwcomposerTexture : public AbstractEglTexture
{
public:
    virtual ~EglHwcomposerTexture();

private:
    friend class EglHwcomposerBackend;
    EglHwcomposerTexture(SceneOpenGLTexture *texture, EglHwcomposerBackend *backend);
};

}

#endif
