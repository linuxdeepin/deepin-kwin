/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006-2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_GLTEXTURE_H
#define KWIN_GLTEXTURE_H

#include <kwinglutils_export.h>

#include <QSize>
#include <QRegion>
#include <QSharedPointer>
#include <QExplicitlySharedDataPointer>
#include <QMatrix4x4>

#include <epoxy/gl.h>

class QImage;
class QPixmap;

/** @addtogroup kwineffects */
/** @{ */

namespace KWin
{

class GLVertexBuffer;
class GLTexturePrivate;

enum TextureCoordinateType {
    NormalizedCoordinates = 0,
    UnnormalizedCoordinates
};

class KWINGLUTILS_EXPORT GLTexture
{
public:
    GLTexture();
    GLTexture(const GLTexture& tex);
    explicit GLTexture(const QImage& image, GLenum target = GL_TEXTURE_2D);
    explicit GLTexture(const QPixmap& pixmap, GLenum target = GL_TEXTURE_2D);
    explicit GLTexture(const QString& fileName);
    GLTexture(GLenum internalFormat, int width, int height, int levels = 1);
    explicit GLTexture(GLenum internalFormat, const QSize &size, int levels = 1);
    virtual ~GLTexture();

    GLTexture & operator = (const GLTexture& tex);

    bool isNull() const;
    QSize size() const;
    int width() const;
    int height() const;
    /**
     * @since 4.7
     **/
    bool isYInverted() const;
    /**
     * @since 4.8
     **/
    void setYInverted(bool inverted);

    /**
     * Specifies which component of a texel is placed in each respective
     * component of the vector returned to the shader.
     *
     * Valid values are GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ONE and GL_ZERO.
     *
     * @see swizzleSupported()
     * @since 5.2
     */
    void setSwizzle(GLenum red, GLenum green, GLenum blue, GLenum alpha);

    /**
     * Returns a matrix that transforms texture coordinates of the given type,
     * taking the texture target and the y-inversion flag into account.
     *
     * @since 4.11
     */
    QMatrix4x4 matrix(TextureCoordinateType type) const;

    void update(const QImage& image, const QPoint &offset = QPoint(0, 0), const QRect &src = QRect());
    virtual void discard();
    void bind();
    void unbind();
    void render(QRegion region, const QRect& rect, bool hardwareClipping = false);

    GLuint texture() const;
    GLenum target() const;
    GLenum filter() const;
    GLenum internalFormat() const;

    /** @short
     * Make the texture fully transparent
     * Warning: this clobbers the current framebuffer binding except on fglrx
     */
    void clear();
    bool isDirty() const;
    void setFilter(GLenum filter);
    void setWrapMode(GLenum mode);
    void setDirty();

    void generateMipmaps();

    static bool framebufferObjectSupported();

    /**
     * Returns true if texture swizzle is supported, and false otherwise
     *
     * Texture swizzle requires OpenGL 3.3, GL_ARB_texture_swizzle, or OpenGL ES 3.0.
     *
     * @since 5.2
     */
    static bool supportsSwizzle();

    /**
     * Returns @c true if texture formats R* are supported, and @c false otherwise.
     *
     * This requires OpenGL 3.0, GL_ARB_texture_rg or OpenGL ES 3.0 or GL_EXT_texture_rg.
     *
     * @since 5.2.1
     **/
    static bool supportsFormatRG();

protected:
    QExplicitlySharedDataPointer<GLTexturePrivate> d_ptr;
    GLTexture(GLTexturePrivate& dd);

private:
    Q_DECLARE_PRIVATE(GLTexture)
};

} // namespace

/** @} */

#endif
