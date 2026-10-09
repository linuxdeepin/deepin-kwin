/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "regionscreencastsource.h"
#include "screencastutils.h"

#include <composite.h>
#include <core/output.h>
#include <kwingltexture.h>
#include <kwinglutils.h>
#include <scene/workspacescene.h>
#include <workspace.h>

#include <QPainter>

namespace KWin
{

RegionScreenCastSource::RegionScreenCastSource(const QRect &region, qreal scale, QObject *parent)
    : ScreenCastSource(parent)
    , m_region(region)
    , m_scale(scale)
{
    Q_ASSERT(m_region.isValid());
    Q_ASSERT(m_scale > 0);

    if (Compositor::self() && workspace()) {
        connect(workspace(), &Workspace::outputsChanged, this, &RegionScreenCastSource::updateOutputs);
        updateOutputs();
    }
}

RegionScreenCastSource::~RegionScreenCastSource()
{
    if (Compositor::self()) {
        for (const QPointer<Output> &output : std::as_const(m_capturedOutputs)) {
            if (output) {
                Compositor::self()->removeCaptureClient(output);
            }
        }
    }
}

void RegionScreenCastSource::updateOutputs()
{
    auto *compositor = Compositor::self();
    if (!compositor) {
        return;
    }
    for (const QPointer<Output> &output : std::as_const(m_capturedOutputs)) {
        if (output) {
            compositor->removeCaptureClient(output);
        }
    }
    m_capturedOutputs.clear();
    const auto allOutputs = workspace()->outputs();
    for (Output *output : allOutputs) {
        if (output->geometry().intersects(m_region)) {
            m_capturedOutputs.append(output);
            compositor->addCaptureClient(output);
        }
    }
    m_outputsDirty = true;
    compositor->scene()->addRepaint(m_region);
}

QSize RegionScreenCastSource::textureSize() const
{
    return m_region.size() * m_scale;
}

bool RegionScreenCastSource::hasAlphaChannel() const
{
    return true;
}

bool RegionScreenCastSource::isFrameReady() const
{
    const auto *compositor = Compositor::self();
    if (!compositor || m_capturedOutputs.isEmpty()) {
        return false;
    }
    for (const QPointer<Output> &output : m_capturedOutputs) {
        if (!output || !compositor->captureFrameForOutput(output)) {
            return false;
        }
    }
    return true;
}

void RegionScreenCastSource::updateOutput(Output *output)
{
    m_last = output->renderLoop()->lastPresentationTimestamp();

    if (m_renderedTexture) {
        Compositor *compositor = Compositor::self();
        if (!compositor) {
            return;
        }
        const std::shared_ptr<GLTexture> outputTexture = compositor->captureFrameForOutput(output);
        const auto outputGeometry = output->geometry();
        if (!outputTexture || !m_region.intersects(output->geometry())) {
            return;
        }

        GLFramebuffer::pushFramebuffer(m_target.get());

        ShaderBinder shaderBinder(ShaderTrait::MapTexture);
        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(m_region);
        projectionMatrix.translate(outputGeometry.left() / m_scale, (m_region.bottom() - outputGeometry.bottom()) / m_scale);
        projectionMatrix.translate(0, m_region.top() / m_scale);

        shaderBinder.shader()->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

        outputTexture->bind();
        outputTexture->render(output->geometry(), 1 / m_scale);
        outputTexture->unbind();
        GLFramebuffer::popFramebuffer();
    }
}

std::chrono::nanoseconds RegionScreenCastSource::clock() const
{
    return m_last;
}

void RegionScreenCastSource::render(GLFramebuffer *target)
{
    if (!m_renderedTexture) {
        m_renderedTexture.reset(new GLTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize()));
        m_target.reset(new GLFramebuffer(m_renderedTexture.get()));
    }
    if (m_outputsDirty) {
        GLFramebuffer::pushFramebuffer(m_target.get());
        const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        if (scissorEnabled) {
            glEnable(GL_SCISSOR_TEST);
        }
        GLFramebuffer::popFramebuffer();
        for (const QPointer<Output> &output : std::as_const(m_capturedOutputs)) {
            updateOutput(output);
        }
        m_outputsDirty = false;
    }

    GLFramebuffer::pushFramebuffer(target);
    QRect r(QPoint(), target->size());
    auto shader = ShaderManager::instance()->pushShader(ShaderTrait::MapTexture);

    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(r);
    shader->setUniform(GLShader::ModelViewProjectionMatrix, projectionMatrix);

    m_renderedTexture->bind();
    m_renderedTexture->render(r, m_scale);
    m_renderedTexture->unbind();

    ShaderManager::instance()->popShader();
    GLFramebuffer::popFramebuffer();
}

void RegionScreenCastSource::render(spa_data *spa, spa_video_format format)
{
    GLTexture offscreenTexture(hasAlphaChannel() ? GL_RGBA8 : GL_RGB8, textureSize());
    GLFramebuffer offscreenTarget(&offscreenTexture);

    render(&offscreenTarget);
    grabTexture(&offscreenTexture, spa, format);
}

}
