/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "egl_gbm_backend.h"
// kwin
#include "composite.h"
#include "drm_backend.h"
#include "drm_output.h"
#include "gbm_surface.h"
#include "logging.h"
#include "options.h"
#include "screens.h"
// kwin libs
#include <kwinglplatform.h>
// Qt
#include <QOpenGLContext>
// system
#include <gbm.h>

namespace KWin
{

typedef EGLBoolean (*eglQueryDmaBufFormatsEXT_func) (EGLDisplay dpy, EGLint max_formats, EGLint *formats, EGLint *num_formats);
typedef EGLBoolean (*eglQueryDmaBufModifiersEXT_func) (EGLDisplay dpy, EGLint format, EGLint max_modifiers,
                                                       EGLuint64KHR *modifiers, EGLBoolean *external_only, EGLint *num_modifiers);

eglQueryDmaBufFormatsEXT_func eglQueryDmaBufFormatsEXT = nullptr;
eglQueryDmaBufModifiersEXT_func eglQueryDmaBufModifiersEXT = nullptr;


EglGbmBackend::EglGbmBackend(DrmBackend *b)
    : AbstractEglBackend()
    , m_backend(b)
{
    // Egl is always direct rendering
    setIsDirectRendering(true);
    setSyncsToVBlank(true);
    connect(m_backend, &DrmBackend::outputAdded, this, &EglGbmBackend::createOutput);
    connect(m_backend, &DrmBackend::outputRemoved, this,
        [this] (DrmOutput *output) {
            auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                [output] (const Output &o) {
                    return o.output == output;
                }
            );
            if (it == m_outputs.end()) {
                return;
            }
            cleanupOutput(*it);
            m_outputs.erase(it);
        }
    );
}

EglGbmBackend::~EglGbmBackend()
{
    cleanup();
}

void EglGbmBackend::cleanupSurfaces()
{
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        cleanupOutput(*it);
    }
    m_outputs.clear();
}

void EglGbmBackend::cleanupOutput(Output &o)
{
    cleanupPostprocess(o);
    o.output->releaseGbm();

    if (o.eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(eglDisplay(), o.eglSurface);
    }
}

bool EglGbmBackend::initializeEgl()
{
    initClientExtensions();
    EGLDisplay display = m_backend->sceneEglDisplay();

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (display == EGL_NO_DISPLAY) {
        const bool hasMesaGBM = hasClientExtension(QByteArrayLiteral("EGL_MESA_platform_gbm"));
        const bool hasKHRGBM = hasClientExtension(QByteArrayLiteral("EGL_KHR_platform_gbm"));
        const GLenum platform = hasMesaGBM ? EGL_PLATFORM_GBM_MESA : EGL_PLATFORM_GBM_KHR;

        if (!hasClientExtension(QByteArrayLiteral("EGL_EXT_platform_base")) ||
                (!hasMesaGBM && !hasKHRGBM)) {
            setFailed("missing one or more extensions between EGL_EXT_platform_base,  EGL_MESA_platform_gbm, EGL_KHR_platform_gbm");
            return false;
        }

        auto device = gbm_create_device(m_backend->fd());
        if (!device) {
            setFailed("Could not create gbm device");
            return false;
        }
        m_backend->setGbmDevice(device);

        display = eglGetPlatformDisplayEXT(platform, device, nullptr);
    }

    if (display == EGL_NO_DISPLAY)
        return false;
    setEglDisplay(display);
    return initEglAPI();
}

void EglGbmBackend::init()
{
    if (!initializeEgl()) {
        setFailed("Could not initialize egl");
        return;
    }

    initEglFormatsWithModifiers();
    //dumpFormatsWithModifiers();

    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initKWinGL();
    initBufferAge();
    initWayland();
    initRemotePresent();
}

void EglGbmBackend::dumpFormatsWithModifiers()
{
    for (auto it_h = m_eglFormatsWithModifiers.constBegin(); it_h != m_eglFormatsWithModifiers.constEnd(); ++it_h){
        uint32_t format = it_h.key();
        QVector<uint64_t> modifiers = it_h.value();
        qDebug("format = %d", format);
        for (auto it_s = modifiers.constBegin(); it_s != modifiers.constEnd(); it_s++ ) {
            qDebug("         ---------- modifier = %ld", *it_s);
        }
    }
}

void EglGbmBackend::initEglFormatsWithModifiers()
{
    EGLint count = 0;

    if (!hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import"))) {
        qDebug("Formats&Modifiers haven't supported: EGL_EXT_image_dma_buf_import isn't supported!");
        return;
    }
    if (hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import_modifiers"))) {
        eglQueryDmaBufFormatsEXT = (eglQueryDmaBufFormatsEXT_func)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        eglQueryDmaBufModifiersEXT = (eglQueryDmaBufModifiersEXT_func)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    }
    if (eglQueryDmaBufFormatsEXT == nullptr) {
        qDebug("Formats&Modifiers haven't supported: eglQueryDmaBufFormatsEXT failed!");
        return;
    }

    EGLBoolean success = eglQueryDmaBufFormatsEXT(eglDisplay(), 0, nullptr, &count);
    QVector<uint32_t> formats(count);
    if (!success || count == 0) {
        qDebug("Formats&Modifiers haven't supported: eglQueryDmaBufFormatsEXT Failed to get count! 1st call.");
        return;
    }
    if (!eglQueryDmaBufFormatsEXT(eglDisplay(), count, (EGLint*)formats.data(), &count)) {
        qDebug("Formats&Modifiers haven't supported: eglQueryDmaBufFormatsEXT Failed to get formats! 2nd call.");
        return;
    }
    for (auto format : qAsConst(formats)) {
        if (eglQueryDmaBufModifiersEXT != nullptr) {
            count = 0;
            success = eglQueryDmaBufModifiersEXT(eglDisplay(), format, 0, nullptr, nullptr, &count);

            if (success && count > 0) {
                QVector<uint64_t> modifiers(count);
                if (eglQueryDmaBufModifiersEXT(eglDisplay(), format, count, modifiers.data(), nullptr, &count)) {
                    m_eglFormatsWithModifiers.insert(format, modifiers);
                    continue;
                }
            }
        }
        m_eglFormatsWithModifiers.insert(format, QVector<uint64_t>());
    }
}

bool EglGbmBackend::initRenderingContext()
{
    initBufferConfigs();

    if (!createContext()) {
        return false;
    }

    const auto outputs = m_backend->drmOutputs();
    for (DrmOutput *drmOutput: outputs) {
        createOutput(drmOutput);
    }
    if (m_outputs.isEmpty()) {
        qCCritical(KWIN_DRM) << "Create Window Surfaces failed";
        return false;
    }
    // set our first surface as the one for the abstract backend, just to make it happy
    setSurface(m_outputs.first().eglSurface);

    return makeContextCurrent(m_outputs.first());
}

void EglGbmBackend::initRemotePresent()
{
    if (qEnvironmentVariableIsSet("KWIN_NO_REMOTE")) {
        return;
    }

    qCDebug(KWIN_DRM) << "Support for remote access enabled";
    m_remoteaccessManager.reset(new RemoteAccessManager);
}

void EglGbmBackend::cleanupPostprocess(Output& output)
{
    output.rotation.vbo.reset();
    output.rotation.fbo.reset();
    output.rotation.texture.reset();
    qDebug() << "---------" << __func__;
}

void EglGbmBackend::resetPostprocess(Output& output)
{
    cleanupPostprocess(output);

    if (output.output->hardwareTransformed()) {
        return;
    }

    makeContextCurrent(output);

    auto sz = output.output->pixelSize();
    auto* back = new GLTexture(GL_RGB8, sz.width(), sz.height());
    back->setFilter(GL_LINEAR);
    back->setWrapMode(GL_CLAMP_TO_EDGE);

    output.rotation.texture.reset(back);

    GLRenderTarget *fbo = new GLRenderTarget(*back);
    output.rotation.fbo.reset(fbo);
    qDebug() << "---------!!!!!!" << __func__;
}

void EglGbmBackend::preparePostprocess(const Output& output) const
{
    if (output.rotation.fbo) {
        GLRenderTarget::pushRenderTarget(output.rotation.fbo.get());
        GLRenderTarget::setKWinFramebuffer(output.rotation.fbo->id());
        qDebug() << "---------!!!!!!" << __func__ << output.output->geometry();
    }
}

void EglGbmBackend::renderPostprocess(Output& output)
{
    if (!output.rotation.fbo) return;

    GLRenderTarget::popRenderTarget();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLRenderTarget::setKWinFramebuffer(0);

    const auto& sz = output.output->modeSize();
    glViewport(0, 0, sz.width(), sz.height());

    if (!output.rotation.vbo) {
        const bool gles = GLPlatform::instance()->isGLES();
        const bool glsl_140 = !gles && GLPlatform::instance()->glslVersion() >= kVersionNumber(1, 40);
        const bool core = glsl_140 || (gles && GLPlatform::instance()->glslVersion() >= kVersionNumber(3, 0));

        const QByteArray attribute = core ? "in"        : "attribute";
        const QByteArray texture2D = core ? "texture"   : "texture2D";
        const QByteArray fragColor = core ? "fragColor" : "gl_FragColor";

        QString glHeaderString;

        if (gles) {
            if (core) {
                glHeaderString += "#version 300 es\n\n";
            }

            glHeaderString += "precision highp float;\n";
        } else if (glsl_140) {
            glHeaderString += "#version 140\n\n";
        }

        QByteArray vertexSource;
        QByteArray fragSource;

        QTextStream streamVert(&vertexSource);

        streamVert << glHeaderString;

        streamVert << "uniform mat4 rotateMatrix;\n";
        streamVert << attribute << " vec4 vertex;\n\n";
        streamVert << attribute << " vec4 texcoord;\n\n";
        streamVert << "out vec2 texCoord;\n";
        streamVert << "void main(void)\n";
        streamVert << "{\n";
        streamVert << "    gl_Position = vertex;\n";
        streamVert << "    vec4 t = rotateMatrix * texcoord;\n";
        streamVert << "    texCoord = t.xy;\n";
        streamVert << "}\n";
        streamVert.flush();

        QTextStream streamFrag(&fragSource);
        streamFrag << glHeaderString;

        streamFrag << "uniform sampler2D texUnit;\n";
        streamFrag << attribute << " vec2 texCoord;\n\n";
        if (core) {
            streamFrag << "out vec4 fragColor;\n\n";
        }
        streamFrag << "void main(void)\n";
        streamFrag << "{\n";
        streamFrag << "    " << fragColor << " = " << texture2D << "(texUnit, texCoord);\n";
        streamFrag << "}\n";
        streamFrag.flush();

        GLShader* shaderRotate = ShaderManager::instance()->loadShaderFromCode(vertexSource, fragSource);
        if (!shaderRotate->isValid()) {
            abort();
        }
        output.rotation.shader.reset(shaderRotate);

        GLVertexBuffer *vbo = new GLVertexBuffer(KWin::GLVertexBuffer::Static);

        QVector<float> verts;
        verts 
            << 1  << -1
            << -1 << -1
            << -1 << 1
            << -1 << 1
            << 1 << 1
            << 1  << -1;
        QVector<float> texcoords;
        texcoords
            << 1.0 << 0.0
            << 0.0 << 0.0
            << 0.0 << 1.0
            << 0.0 << 1.0
            << 1.0 << 1.0
            << 1.0 << 0.0;
        vbo->setData(verts.count()/2, 2, verts.data(), texcoords.data());
        output.rotation.vbo.reset(vbo);
        qDebug() << "--------- init vbo";
    }

#if 1
    auto* shaderRotate = output.rotation.shader.get();
    auto rotateLoc = shaderRotate->uniformLocation("rotateMatrix");

    ShaderManager::instance()->pushShader(shaderRotate);
    QMatrix4x4 m;
    m.ortho(0, sz.width(), sz.height(), 0, 0, 65535);

    QMatrix4x4 rm;
    rm.translate(0.5, 0.5);
    rm.rotate(-output.output->rotation(), 0, 0, 1);
    rm.translate(-0.5, -0.5);

    shaderRotate->setUniform(rotateLoc, rm);
#else
    // use builtin shader
    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);
    QMatrix4x4 rm;
    rm.rotate(output.output->rotation(), 0, 0, 1);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, rm);
#endif

    glActiveTexture(GL_TEXTURE0);
    output.rotation.texture->bind();

    GLint ss[4];
    glGetIntegerv(GL_VIEWPORT, ss);
    qDebug() << "-----------!!!!!!- " << __func__ << sz << ss[0] << ss[1] << ss[2] << ss[3];
    output.rotation.vbo->render(GL_TRIANGLES);

    output.rotation.texture->unbind();
    ShaderManager::instance()->popShader();
}


bool EglGbmBackend::resetOutput(Output &o, DrmOutput *drmOutput)
{
    o.output = drmOutput;
    auto size = o.output->hardwareTransformed() ? drmOutput->pixelSize() : drmOutput->modeSize();

    qDebug() << "-----------" << __func__ << "size" << size << drmOutput->geometry();

    std::shared_ptr<GbmSurface> gbmSurface;

    if (o.m_modifiersEnabled) {
        qDebug("---------- formats&modifiers have been enabled!");
        gbmSurface = std::make_shared<GbmSurface>();
        gbm_surface *gbmS = gbm_surface_create_with_modifiers(m_backend->gbmDevice(),
                                                              size.width(), size.height(),
                                                              drmOutput->getPrimaryPlane()->getCurrentFormat(),
                                                              o.m_eglModifiers.data(), o.m_eglModifiers.count());
        gbmSurface->setSurface(gbmS);
    } else {
        gbmSurface = std::make_shared<GbmSurface>(m_backend->gbmDevice(),
                                                  size.width(), size.height(),
                                                  drmOutput->getPrimaryPlane()->getCurrentFormat(),
                                                  GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    }

    if (!gbmSurface) {
        qCCritical(KWIN_DRM) << "Create gbm surface failed";
        return false;
    }
    auto eglSurface = eglCreatePlatformWindowSurfaceEXT(eglDisplay(), config(), (void *)(gbmSurface->surface()), nullptr);
    if (eglSurface == EGL_NO_SURFACE) {
        qCCritical(KWIN_DRM) << "Create Window Surface failed";
        return false;
    } else {
        // destroy previous surface
        if (o.eglSurface != EGL_NO_SURFACE) {
            if (surface() == o.eglSurface) {
                setSurface(eglSurface);
            }
            eglDestroySurface(eglDisplay(), o.eglSurface);
        }
        o.eglSurface = eglSurface;
        o.gbmSurface = gbmSurface;
    }

    resetPostprocess(o);
    return true;
}

void EglGbmBackend::createOutput(DrmOutput *drmOutput)
{
    Output o;
    auto drmFormatsWithModifiers = drmOutput->getPrimaryPlane()->getFormatsWithModifiers();
    auto itDrmH = drmFormatsWithModifiers.find(drmOutput->getPrimaryPlane()->getCurrentFormat());
    auto itEglH = m_eglFormatsWithModifiers.find(drmOutput->getPrimaryPlane()->getCurrentFormat());

    int envModifiersSupport = qEnvironmentVariableIntValue("KWIN_WAYLAND_MODIFIERS_SUPPORT");

    if (itDrmH != drmFormatsWithModifiers.end() && itEglH != m_eglFormatsWithModifiers.end()) {
        QVector<uint64_t> drmModifiers = itDrmH.value();
        QVector<uint64_t> eglModifiers = itEglH.value();


        for (auto itDrmV = drmModifiers.constBegin(); itDrmV != drmModifiers.constEnd();
             itDrmV++) {
            if (*itDrmV == 0)
                continue;

            for (auto itEglV = eglModifiers.constBegin(); itEglV!= eglModifiers.constEnd();
                 itEglV++) {
                if (*itEglV == *itDrmV && envModifiersSupport) {
                    o.m_modifiersEnabled = true;
                    break;
                }
            }
            if (o.m_modifiersEnabled) {
                o.m_drmModifiers = drmModifiers;
                o.m_eglModifiers = eglModifiers;
                break;
            }
        }

    }

    if (resetOutput(o, drmOutput)) {
        connect(drmOutput, &DrmOutput::modeChanged, this,
            [drmOutput, this] {
                auto it = std::find_if(m_outputs.begin(), m_outputs.end(),
                    [drmOutput] (const auto &o) {
                        return o.output == drmOutput;
                    }
                );
                if (it == m_outputs.end()) {
                    return;
                }
                resetOutput(*it, drmOutput);
            }
        );
        m_outputs << o;
    }
}

void EglGbmBackend::setupViewport(const Output& output)
{
    // TODO: ensure the viewport is set correctly each time
    const QSize &overall = screens()->size();
    const QRect &v = output.output->geometry();
    // TODO: are the values correct?

    qreal scale = output.output->scale();

    glViewport(-v.x() * scale, (v.height() - overall.height() + v.y()) * scale,
               overall.width() * scale, overall.height() * scale);

    //qInfo() << Q_FUNC_INFO << "width:" << overall.width() * scale
            //<< "height:" << overall.height() * scale
            //<< "scale:" << scale;
}

bool EglGbmBackend::makeContextCurrent(const Output &output)
{
    const EGLSurface surface = output.eglSurface;
    if (surface == EGL_NO_SURFACE) {
        return false;
    }
    if (eglMakeCurrent(eglDisplay(), surface, surface, context()) == EGL_FALSE) {
        qCCritical(KWIN_DRM) << "Make Context Current failed";
        return false;
    }

    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        qCWarning(KWIN_DRM) << "Error occurred while creating context " << error;
        return false;
    }

    return true;
}

bool EglGbmBackend::initBufferConfigs()
{
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE,         EGL_WINDOW_BIT,
        EGL_RED_SIZE,             1,
        EGL_GREEN_SIZE,           1,
        EGL_BLUE_SIZE,            1,
        EGL_ALPHA_SIZE,           0,
        EGL_RENDERABLE_TYPE,      isOpenGLES() ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
        EGL_CONFIG_CAVEAT,        EGL_NONE,
        EGL_NONE,
    };

    EGLint count;
    EGLConfig configs[1024];
    if (!eglChooseConfig(eglDisplay(), config_attribs, configs, sizeof(configs)/sizeof(EGLConfig), &count)) {
        qCCritical(KWIN_DRM) << "choose config failed";
        return false;
    }

    qCDebug(KWIN_DRM) << "EGL buffer configs count:" << count;

    // loop through all configs, chosing the first one that has suitable format
    for (EGLint i = 0; i < count; i++) {
        EGLint gbmFormat;
        // query some configuration parameters, to show in debug log
        eglGetConfigAttrib(eglDisplay(), configs[i], EGL_NATIVE_VISUAL_ID, &gbmFormat);

        if (KWIN_DRM().isDebugEnabled()) {
            // GBM formats are declared as FOURCC code (integer from ASCII chars, so use this fact)
            char gbmFormatStr[sizeof(EGLint) + 1] = {0};
            memcpy(gbmFormatStr, &gbmFormat, sizeof(EGLint));
            // query number of bits for color channel
            EGLint blueSize, redSize, greenSize, alphaSize;
            eglGetConfigAttrib(eglDisplay(), configs[i], EGL_RED_SIZE, &redSize);
            eglGetConfigAttrib(eglDisplay(), configs[i], EGL_GREEN_SIZE, &greenSize);
            eglGetConfigAttrib(eglDisplay(), configs[i], EGL_BLUE_SIZE, &blueSize);
            eglGetConfigAttrib(eglDisplay(), configs[i], EGL_ALPHA_SIZE, &alphaSize);
            qCDebug(KWIN_DRM) << "  EGL config #" << i << " has GBM FOURCC format:" << gbmFormatStr
                              << "; color sizes (RGBA order):" << redSize << greenSize << blueSize << alphaSize;
        }

        if ((gbmFormat == GBM_FORMAT_XRGB8888) || (gbmFormat == GBM_FORMAT_ARGB8888)) {
            setConfig(configs[i]);
            return true;
        }
    }

    qCCritical(KWIN_DRM) << "choose EGL config did not return a suitable config" << count;
    return false;
}

void EglGbmBackend::present()
{
    for (auto &o: m_outputs) {
        makeContextCurrent(o);
        presentOnOutput(o);
    }
}

void EglGbmBackend::presentOnOutput(EglGbmBackend::Output &o)
{
    eglSwapBuffers(eglDisplay(), o.eglSurface);

    if (o.m_modifiersEnabled) {
        o.buffer = m_backend->createBuffer(o.gbmSurface,
                                           o.output->getPrimaryPlane()->getCurrentFormat(),
                                           o.m_drmModifiers);
    } else {
        o.buffer = m_backend->createBuffer(o.gbmSurface);
    }

    if(m_remoteaccessManager && gbm_surface_has_free_buffers(o.gbmSurface->surface())) {
        // GBM surface is released on page flip so
        // we should pass the buffer before it's presented
        m_remoteaccessManager->passBuffer(o.output, o.buffer);
    }
    m_backend->present(o.buffer, o.output);

    if (supportsBufferAge()) {
        eglQuerySurface(eglDisplay(), o.eglSurface, EGL_BUFFER_AGE_EXT, &o.bufferAge);
    }

}

void EglGbmBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
    // TODO, create new buffer?
}

SceneOpenGLTexturePrivate *EglGbmBackend::createBackendTexture(SceneOpenGLTexture *texture)
{
    return new EglGbmTexture(texture, this);
}

QRegion EglGbmBackend::prepareRenderingFrame()
{
    startRenderTimer();
    return QRegion();
}

QRegion EglGbmBackend::prepareRenderingForScreen(int screenId)
{
    if (screenId >= m_outputs.size()) {
        return QRegion();
    }

    const Output &o = m_outputs.at(screenId);
    doneCurrent();
    makeContextCurrent(o);
    preparePostprocess(o);
    setupViewport(o);

    if (supportsBufferAge()) {
        QRegion region;

        // Note: An age of zero means the buffer contents are undefined
        if (o.bufferAge > 0 && o.bufferAge <= o.damageHistory.count()) {
            for (int i = 0; i < o.bufferAge - 1; i++)
                region |= o.damageHistory[i];
        } else {
            region = o.output->geometry();
        }

        return region;
    }
    return QRegion();
}

void EglGbmBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(renderedRegion)
    Q_UNUSED(damagedRegion)
}

void EglGbmBackend::endRenderingFrameForScreen(int screenId, const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    if (screenId >= m_outputs.size()) {
        return;
    }
    Output &o = m_outputs[screenId];
    renderPostprocess(o);

    if (damagedRegion.intersected(o.output->geometry()).isEmpty() && screenId == 0) {

        // If the damaged region of a window is fully occluded, the only
        // rendering done, if any, will have been to repair a reused back
        // buffer, making it identical to the front buffer.
        //
        // In this case we won't post the back buffer. Instead we'll just
        // set the buffer age to 1, so the repaired regions won't be
        // rendered again in the next frame.
        if (!renderedRegion.intersected(o.output->geometry()).isEmpty())
            glFlush();

        for (auto &o: m_outputs) {
            o.bufferAge = 1;
        }
        return;
    }
    presentOnOutput(o);

    // Save the damaged region to history
    // Note: damage history is only collected for the first screen. For any other screen full repaints
    // are triggered. This is due to a limitation in Scene::paintGenericScreen which resets the Toplevel's
    // repaint. So multiple calls to Scene::paintScreen as it's done in multi-output rendering only
    // have correct damage information for the first screen. If we try to track damage nevertheless,
    // it creates artifacts. So for the time being we work around the problem by only supporting buffer
    // age on the first output. To properly support buffer age on all outputs the rendering needs to
    // be refactored in general.
    if (supportsBufferAge() && screenId == 0) {
        if (o.damageHistory.count() > 10) {
            o.damageHistory.removeLast();
        }

        o.damageHistory.prepend(damagedRegion.intersected(o.output->geometry()));
    }
}

bool EglGbmBackend::usesOverlayWindow() const
{
    return false;
}

bool EglGbmBackend::perScreenRendering() const
{
    return true;
}

/************************************************
 * EglTexture
 ************************************************/

EglGbmTexture::EglGbmTexture(KWin::SceneOpenGLTexture *texture, EglGbmBackend *backend)
    : AbstractEglTexture(texture, backend)
{
}

EglGbmTexture::~EglGbmTexture() = default;

} // namespace
