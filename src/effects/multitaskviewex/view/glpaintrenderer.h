/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwingltexture.h>

#include "texturefilter.h"

#include <QColor>
#include <QHash>
#include <QMatrix4x4>
#include <QRectF>
#include <QString>

#include <memory>

namespace KWin
{

class GLShader;
class GLVertexBuffer;

class GLPaintRenderer
{
public:
    /**
     * @brief Returns the process-wide renderer instance.
     * @return The singleton renderer.
     */
    static GLPaintRenderer &instance();

    /**
     * @brief Initializes the renderer's shaders and buffers on first use.
     * @return This renderer.
     */
    GLPaintRenderer &ensure();

    /**
     * @brief Uploads an image into a GPU texture.
     * @param image Source image.
     * @param smooth Whether to request smooth (mipmapped) filtering.
     * @return The uploaded texture.
     */
    std::unique_ptr<GLTexture> uploadImage(const QImage &image, bool smooth = false);

    /**
     * @brief Draws a texture as a rounded rectangle.
     * @param texture Texture to draw.
     * @param rect Destination rectangle.
     * @param radius Corner radius.
     * @param opacity Paint opacity.
     * @param projection Projection matrix.
     */
    void drawTexture(GLTexture *texture, const QRectF &rect,
                     qreal radius, qreal opacity, const QMatrix4x4 &projection);

    /**
     * @brief Draws a solid rounded rectangle.
     * @param color Fill color.
     * @param rect Destination rectangle.
     * @param radius Corner radius.
     * @param opacity Paint opacity.
     * @param projection Projection matrix.
     * @param borderWidth Border width in pixels, zero for no border.
     */
    void drawColor(const QColor &color, const QRectF &rect,
                   qreal radius, qreal opacity, const QMatrix4x4 &projection,
                   qreal borderWidth = 0);

    /**
     * @brief Applies a scissor rectangle given in logical top-left coordinates.
     * @param rect Scissor rectangle.
     * @param viewportHeight Viewport height used to flip the y axis.
     */
    static void scissorLogical(const QRectF &rect, int viewportHeight);

    class ScissorScope
    {
    public:
        /**
         * @brief Pushes a scissor rectangle.
         * @param logicalRect Scissor rectangle in logical coordinates.
         * @param viewportHeight Viewport height used to flip the y axis.
         */
        ScissorScope(const QRectF &logicalRect, int viewportHeight);

        /**
         * @brief Restores the previous scissor state.
         */
        ~ScissorScope();

        /**
         * @brief Deleted copy constructor.
         */
        ScissorScope(const ScissorScope &) = delete;

        /**
         * @brief Deleted copy assignment.
         * @return This scope.
         */
        ScissorScope &operator=(const ScissorScope &) = delete;

    private:
        bool m_active = false;
        bool m_wasEnabled = false;
    };

private:
    GLPaintRenderer() = default;
    void drawInternal(GLTexture *texture, const QColor &color,
                      const QRectF &rect, qreal radius, qreal opacity,
                      const QMatrix4x4 &projection, const QRectF &srcUV,
                      qreal borderWidth);

    std::unique_ptr<GLShader> m_texShader;
    std::unique_ptr<GLShader> m_colorShader;
    std::unique_ptr<GLVertexBuffer> m_quad;
    bool m_initialized = false;
};

}
