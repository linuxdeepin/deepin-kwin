// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_EGL_GBM_BACKEND_H
#define KWIN_EGL_GBM_BACKEND_H

#include "abstract_egl_backend.h"
#include "remoteaccess_manager.h"

#include <memory>

struct gbm_surface;

namespace KWin
{
class DrmBackend;
class DrmBuffer;
class DrmDumbBuffer;
class DrmOutput;
class GbmSurface;
class DrmGpu;

/**
 * @brief OpenGL Backend using Egl on a GBM surface.
 **/
class EglGbmBackend : public AbstractEglBackend
{
    Q_OBJECT
public:
    EglGbmBackend(DrmBackend *b, DrmGpu *gpu);
    virtual ~EglGbmBackend();
    void screenGeometryChanged(const QSize &size) override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    QRegion prepareRenderingFrame() override;
    void endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void endRenderingFrameForScreen(int screenId, const QRegion &damage, const QRegion &damagedRegion) override;
    bool setDamageRegion(const QRegion region) override;
    bool usesOverlayWindow() const override;
    bool perScreenRendering() const override;
    QRegion prepareRenderingForScreen(int screenId) override;
    void init() override;
    unsigned int getFrameFd() {
        return m_dmaFd;
    }

    void connectToGpu(DrmGpu *gpu);

protected:
    void present() override;
    void cleanupSurfaces() override;
    void aboutToStartPainting(const QRegion &damage) override;

private:
    bool initializeEgl();
    bool initBufferConfigs();
    bool initRenderingContext();
    void initRemotePresent();
    struct Output {
        DrmOutput *output = nullptr;
        DrmBuffer *buffer = nullptr;
        DrmDumbBuffer *dumbBuffer = nullptr;
        std::shared_ptr<GbmSurface> gbmSurface;
        EGLSurface eglSurface = EGL_NO_SURFACE;
        int bufferAge = 0;
        /**
        * @brief The damage history for the past 10 frames.
        */
        QList<QRegion> damageHistory;

        struct {
            std::shared_ptr<GLTexture> texture;
            std::shared_ptr<GLVertexBuffer> vbo;
            std::shared_ptr<GLRenderTarget> fbo;
            std::shared_ptr<GLShader> shader;
        } rotation;

        /**
        * @brief  Hisilicon libmali platform.
        *           Get modifiers by two ways: 1st by drm* api, 2ed by egl* api.
        *           compare modifiers vector of the same format,
        *           if there have one equal modifier of these two modifiers vector,
        *           formats&modifiers have been supported. UMMMMMMMMM :(
        */
        bool m_modifiersEnabled = false;
        QVector<uint64_t> m_drmModifiers;
        QVector<uint64_t> m_eglModifiers;
    };
    bool resetOutput(Output &output, DrmOutput *drmOutput);
    bool makeContextCurrent(const Output &output);
    void setupViewport(const Output& output);
    void presentOnOutput(Output &output, const QRegion &damagedRegion);
    void cleanupOutput(Output &output);
    void createOutput(DrmOutput *output);

    void cleanupPostprocess(Output& output);
    void resetPostprocess(Output& output);
    void preparePostprocess(const Output& output) const;
    void renderPostprocess(Output& output);

    DrmBackend *m_backend;
    DrmGpu *m_gpu;
    QVector<Output> m_outputs;
    QScopedPointer<RemoteAccessManager> m_remoteaccessManager;
    friend class EglGbmTexture;

    QHash<uint32_t, QVector<uint64_t>> m_eglFormatsWithModifiers;
    void initEglFormatsWithModifiers();
    void dumpFormatsWithModifiers();
    void initEglPartialUpateExt();

    unsigned int m_dmaFd;
    unsigned int m_dumpOutputBufferCount;
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
