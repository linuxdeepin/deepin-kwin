/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "glpaintrenderer.h"
#include "assetpaths.h"

#include <kwinglutils.h>

#include <QByteArray>
#include <QDebug>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QVector2D>
#include <QVector4D>

#include <epoxy/gl.h>

#include <algorithm>
#include <cmath>

namespace KWin
{

GLPaintRenderer &GLPaintRenderer::instance()
{
    static GLPaintRenderer s_instance;
    return s_instance;
}

GLPaintRenderer &GLPaintRenderer::ensure()
{
    if (m_initialized) {
        return *this;
    }
    const QString vs = shaderPath(QStringLiteral("glpaint.vert"));
    m_texShader = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::MapTexture, vs, shaderPath(QStringLiteral("glpaint_tex.frag")));
    m_colorShader = ShaderManager::instance()->generateShaderFromFile(
        ShaderTrait::UniformColor, vs, shaderPath(QStringLiteral("glpaint_color.frag")));
    if (!m_texShader->isValid() || !m_colorShader->isValid()) {
        qWarning("KwinBlur glpaint shader compilation/linking failed");
    }

    struct QuadVertex
    {
        float x, y, u, v;
    };
    static const QuadVertex quad[6] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    };
    const GLVertexAttrib attribs[] = {
        {VA_Position, 2, GL_FLOAT, offsetof(QuadVertex, x)},
        {VA_TexCoord, 2, GL_FLOAT, offsetof(QuadVertex, u)},
    };
    m_quad = std::make_unique<GLVertexBuffer>(GLVertexBuffer::Static);
    m_quad->setAttribLayout(attribs, 2, sizeof(QuadVertex));
    m_quad->setData(quad, sizeof(quad));
    m_quad->setVertexCount(6);
    m_initialized = true;
    return *this;
}

std::unique_ptr<GLTexture> GLPaintRenderer::uploadImage(const QImage &image, bool smooth)
{
    if (image.isNull() || !hasGLContext()) {
        return nullptr;
    }
    const TextureFilterPlan plan = textureFilterPlan(smooth, hasGLContext());
    const auto glFilter = [](TextureFilterMode m) -> GLenum {
        switch (m) {
        case TextureFilterMode::Nearest:
            return GL_NEAREST;
        case TextureFilterMode::Linear:
            return GL_LINEAR;
        case TextureFilterMode::Mipmap:
            return GL_LINEAR_MIPMAP_LINEAR;
        }
        return GL_LINEAR;
    };
    const int levels = plan.generateMipmap ? qMax(1, int(std::log2(qMax(image.width(), image.height()))) + 1) : 1;
    auto tex = std::make_unique<GLTexture>(GL_RGBA8, image.size(), levels);
    tex->update(image);
    tex->setWrapMode(GL_CLAMP_TO_EDGE);
    if (plan.generateMipmap) {
        tex->setFilter(GL_LINEAR);
        tex->bind();
        tex->generateMipmaps();
    }
    tex->setFilter(glFilter(plan.minFilter));
    tex->bind();
    tex->unbind();
    return tex;
}

GLPaintRenderer::ScissorScope::ScissorScope(const QRectF &rect, int viewportHeight)
{
    if (rect.isEmpty()) {
        return;
    }
    m_wasEnabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    glEnable(GL_SCISSOR_TEST);
    scissorLogical(rect, viewportHeight);
    m_active = true;
}

GLPaintRenderer::ScissorScope::~ScissorScope()
{
    if (m_active && !m_wasEnabled) {
        glDisable(GL_SCISSOR_TEST);
    }
}

void GLPaintRenderer::scissorLogical(const QRectF &rect, int viewportHeight)
{
    glScissor(int(rect.x()), int(viewportHeight - rect.y() - rect.height()),
              int(rect.width()), int(rect.height()));
}

void GLPaintRenderer::drawInternal(GLTexture *texture, const QColor &color,
                                   const QRectF &rect, qreal radius, qreal opacity,
                                   const QMatrix4x4 &projection, const QRectF &srcUV,
                                   qreal borderWidth)
{
    ensure();
    const bool textured = texture && !texture->isNull();
    GLShader *shader = textured ? m_texShader.get() : m_colorShader.get();
    ShaderManager::instance()->pushShader(shader);

    QMatrix4x4 m = projection;
    m.translate(rect.x(), rect.y(), 0);
    m.scale(float(rect.width()), float(rect.height()), 1);
    shader->setUniform("mvp", m);
    shader->setUniform("uSize", QVector2D(float(rect.width()), float(rect.height())));
    shader->setUniform("uSrcRect", QVector4D(float(srcUV.x()), float(srcUV.y()), float(srcUV.width()), float(srcUV.height())));

    shader->setUniform("box", QVector4D(float(rect.width()) / 2, float(rect.height()) / 2, float(rect.width()) / 2, float(rect.height()) / 2));
    const float rad = qBound(0.0f, float(radius), float(std::min(rect.width(), rect.height())) / 2);
    shader->setUniform("cornerRadius", QVector4D(rad, rad, rad, rad));
    shader->setUniform("opacity", float(opacity));
    shader->setUniform("borderWidth", float(borderWidth));

    if (textured) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture->texture());
        shader->setUniform("tex", 0);
    } else {
        shader->setUniform("color", color);
    }

    const GLboolean blendWas = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    m_quad->bindArrays();
    m_quad->draw(GL_TRIANGLES, 0, 6);
    m_quad->unbindArrays();
    if (blendWas) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    ShaderManager::instance()->popShader();
}

static void fitTextureGeometry(const QSizeF &srcSize, const QRectF &rect,
                               QRectF *drawRect, QRectF *srcUV)
{
    *drawRect = rect;
    *srcUV = QRectF(0, 0, 1, 1);
    if (!srcSize.isValid() || srcSize.width() <= 0 || srcSize.height() <= 0) {
        return;
    }
    const qreal srcAspect = srcSize.width() / srcSize.height();
    const qreal rectAspect = rect.width() / rect.height();
    qreal sx = 1.0, sy = 1.0;
    if (srcAspect > rectAspect) {
        sx = rectAspect / srcAspect;
    } else {
        sy = srcAspect / rectAspect;
    }
    *srcUV = QRectF((1.0 - sx) / 2, (1.0 - sy) / 2, sx, sy);
}

void GLPaintRenderer::drawTexture(GLTexture *texture, const QRectF &rect,
                                  qreal radius, qreal opacity, const QMatrix4x4 &projection)
{
    const QSize texSize = texture ? texture->size() : QSize();
    QRectF drawRect;
    QRectF srcUV;
    fitTextureGeometry(texSize, rect, &drawRect, &srcUV);
    drawInternal(texture, Qt::white, drawRect, radius, opacity, projection, srcUV, 0);
}

void GLPaintRenderer::drawColor(const QColor &color, const QRectF &rect,
                                qreal radius, qreal opacity, const QMatrix4x4 &projection,
                                qreal borderWidth)
{
    drawInternal(nullptr, color, rect, radius, opacity, projection, QRectF(0, 0, 1, 1), borderWidth);
}

}
