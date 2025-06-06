/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2012 Philipp Knechtges <philipp-dev@knechtges.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinconfig.h" // KWIN_HAVE_OPENGL
#include "kwineffects.h"
#include "kwinglplatform.h"
#include "kwinglutils.h"
#include "kwinglutils_funcs.h"

#include "kwingltexture_p.h"

#include <QImage>
#include <QPixmap>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

namespace KWin
{

//****************************************
// GLTexture
//****************************************

bool GLTexturePrivate::s_supportsFramebufferObjects = false;
bool GLTexturePrivate::s_supportsARGB32 = false;
bool GLTexturePrivate::s_supportsUnpack = false;
bool GLTexturePrivate::s_supportsTextureStorage = false;
bool GLTexturePrivate::s_supportsTextureSwizzle = false;
bool GLTexturePrivate::s_supportsTextureFormatRG = false;
bool GLTexturePrivate::s_supportsTexture16Bit = false;
uint GLTexturePrivate::s_fbo = 0;

// Table of GL formats/types associated with different values of QImage::Format.
// Zero values indicate a direct upload is not feasible.
//
// Note: Blending is set up to expect premultiplied data, so the non-premultiplied
// Format_ARGB32 must be converted to Format_ARGB32_Premultiplied ahead of time.
struct
{
    GLenum internalFormat;
    GLenum format;
    GLenum type;
} static const formatTable[] = {
    {0, 0, 0}, // QImage::Format_Invalid
    {0, 0, 0}, // QImage::Format_Mono
    {0, 0, 0}, // QImage::Format_MonoLSB
    {0, 0, 0}, // QImage::Format_Indexed8
    {GL_RGB8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV}, // QImage::Format_RGB32
    {0, 0, 0}, // QImage::Format_ARGB32
    {GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV}, // QImage::Format_ARGB32_Premultiplied
    {GL_RGB8, GL_BGR, GL_UNSIGNED_SHORT_5_6_5_REV}, // QImage::Format_RGB16
    {0, 0, 0}, // QImage::Format_ARGB8565_Premultiplied
    {0, 0, 0}, // QImage::Format_RGB666
    {0, 0, 0}, // QImage::Format_ARGB6666_Premultiplied
    {GL_RGB5, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV}, // QImage::Format_RGB555
    {0, 0, 0}, // QImage::Format_ARGB8555_Premultiplied
    {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE}, // QImage::Format_RGB888
    {GL_RGB4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV}, // QImage::Format_RGB444
    {GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV}, // QImage::Format_ARGB4444_Premultiplied
    {GL_RGB8, GL_RGBA, GL_UNSIGNED_BYTE}, // QImage::Format_RGBX8888
    {0, 0, 0}, // QImage::Format_RGBA8888
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, // QImage::Format_RGBA8888_Premultiplied
    {GL_RGB10, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_BGR30
    {GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_A2BGR30_Premultiplied
    {GL_RGB10, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_RGB30
    {GL_RGB10_A2, GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV}, // QImage::Format_A2RGB30_Premultiplied
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE}, // QImage::Format_Alpha8
    {GL_R8, GL_RED, GL_UNSIGNED_BYTE}, // QImage::Format_Grayscale8
    {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT}, // QImage::Format_RGBX64
    {0, 0, 0}, // QImage::Format_RGBA64
    {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT}, // QImage::Format_RGBA64_Premultiplied
    {GL_R16, GL_RED, GL_UNSIGNED_SHORT}, // QImage::Format_Grayscale16
    {0, 0, 0}, // QImage::Format_BGR888
};

GLTexture::GLTexture(GLenum target)
    : d_ptr(new GLTexturePrivate())
{
    Q_D(GLTexture);
    d->m_target = target;
}

GLTexture::GLTexture(GLTexturePrivate &dd)
    : d_ptr(&dd)
{
}

GLTexture::GLTexture(const GLTexture &tex)
    : d_ptr(tex.d_ptr)
{
}

GLTexture::GLTexture(const QImage &image, GLenum target)
    : d_ptr(new GLTexturePrivate())
{
    Q_D(GLTexture);

    if (image.isNull()) {
        return;
    }

    d->m_target = target;

    if (d->m_target != GL_TEXTURE_RECTANGLE_ARB) {
        d->m_scale.setWidth(1.0 / image.width());
        d->m_scale.setHeight(1.0 / image.height());
    } else {
        d->m_scale.setWidth(1.0);
        d->m_scale.setHeight(1.0);
    }

    d->m_size = image.size();
    d->m_yInverted = true;
    d->m_canUseMipmaps = false;
    d->m_mipLevels = 1;

    d->updateMatrix();

    const bool created = create();
    Q_ASSERT(created);
    bind();

    if (!GLPlatform::instance()->isGLES()) {
        QImage im;
        GLenum internalFormat;
        GLenum format;
        GLenum type;

        const QImage::Format index = image.format();

        if (index < sizeof(formatTable) / sizeof(formatTable[0]) && formatTable[index].internalFormat
            && !(formatTable[index].type == GL_UNSIGNED_SHORT && !d->s_supportsTexture16Bit)) {
            internalFormat = formatTable[index].internalFormat;
            format = formatTable[index].format;
            type = formatTable[index].type;
            im = image;
        } else {
            im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            internalFormat = GL_RGBA8;
            format = GL_BGRA;
            type = GL_UNSIGNED_INT_8_8_8_8_REV;
        }

        d->m_internalFormat = internalFormat;

        if (d->s_supportsTextureStorage) {
            glTexStorage2D(d->m_target, 1, internalFormat, im.width(), im.height());
            glTexSubImage2D(d->m_target, 0, 0, 0, im.width(), im.height(),
                            format, type, im.constBits());
            d->m_immutable = true;
        } else {
            glTexParameteri(d->m_target, GL_TEXTURE_MAX_LEVEL, d->m_mipLevels - 1);
            glTexImage2D(d->m_target, 0, internalFormat, im.width(), im.height(), 0,
                         format, type, im.constBits());
        }
    } else {
        d->m_internalFormat = GL_RGBA8;

        if (d->s_supportsARGB32) {
            const QImage im = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            glTexImage2D(d->m_target, 0, GL_BGRA_EXT, im.width(), im.height(),
                         0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, im.constBits());
        } else {
            const QImage im = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
            glTexImage2D(d->m_target, 0, GL_RGBA, im.width(), im.height(),
                         0, GL_RGBA, GL_UNSIGNED_BYTE, im.constBits());
        }
    }

    unbind();
    setFilter(GL_LINEAR);
}

GLTexture::GLTexture(const QPixmap &pixmap, GLenum target)
    : GLTexture(pixmap.toImage(), target)
{
}

GLTexture::GLTexture(const QString &fileName)
    : GLTexture(QImage(fileName))
{
}

GLTexture::GLTexture(GLenum internalFormat, int width, int height, int levels, bool needsMutability)
    : d_ptr(new GLTexturePrivate())
{
    Q_D(GLTexture);

    d->m_target = GL_TEXTURE_2D;
    d->m_scale.setWidth(1.0 / width);
    d->m_scale.setHeight(1.0 / height);
    d->m_size = QSize(width, height);
    d->m_canUseMipmaps = levels > 1;
    d->m_mipLevels = levels;
    d->m_filter = levels > 1 ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST;

    d->updateMatrix();

    create();
    bind();

    if (!GLPlatform::instance()->isGLES()) {
        if (d->s_supportsTextureStorage && !needsMutability) {
            glTexStorage2D(d->m_target, levels, internalFormat, width, height);
            d->m_immutable = true;
        } else {
            glTexParameteri(d->m_target, GL_TEXTURE_MAX_LEVEL, levels - 1);
            glTexImage2D(d->m_target, 0, internalFormat, width, height, 0,
                         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
        }
        d->m_internalFormat = internalFormat;
    } else {
        // The format parameter in glTexSubImage() must match the internal format
        // of the texture, so it's important that we allocate the texture with
        // the format that will be used in update() and clear().
        const GLenum format = d->s_supportsARGB32 ? GL_BGRA_EXT : GL_RGBA;
        glTexImage2D(d->m_target, 0, format, width, height, 0,
                     format, GL_UNSIGNED_BYTE, nullptr);

        // This is technically not true, but it means that code that calls
        // internalFormat() won't need to be specialized for GLES2.
        d->m_internalFormat = GL_RGBA8;
    }

    unbind();
}

GLTexture::GLTexture(GLenum internalFormat, const QSize &size, int levels, bool needsMutability)
    : GLTexture(internalFormat, size.width(), size.height(), levels, needsMutability)
{
}

GLTexture::GLTexture(GLuint textureId, GLenum internalFormat, const QSize &size, int levels)
    : d_ptr(new GLTexturePrivate())
{
    Q_D(GLTexture);
    d->m_foreign = true;
    d->m_texture = textureId;
    d->m_target = GL_TEXTURE_2D;
    d->m_scale.setWidth(1.0 / size.width());
    d->m_scale.setHeight(1.0 / size.height());
    d->m_size = size;
    d->m_canUseMipmaps = levels > 1;
    d->m_mipLevels = levels;
    d->m_filter = levels > 1 ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST;
    d->m_internalFormat = internalFormat;

    d->updateMatrix();
}

GLTexture::~GLTexture()
{
}

bool GLTexture::create()
{
    Q_D(GLTexture);
    if (!isNull()) {
        return true;
    }
    glGenTextures(1, &d->m_texture);
    return d->m_texture != GL_NONE;
}

GLTexture &GLTexture::operator=(const GLTexture &tex)
{
    d_ptr = tex.d_ptr;
    return *this;
}

GLTexturePrivate::GLTexturePrivate()
    : m_texture(0)
    , m_target(0)
    , m_internalFormat(0)
    , m_filter(GL_NEAREST)
    , m_wrapMode(GL_REPEAT)
    , m_yInverted(false)
    , m_canUseMipmaps(false)
    , m_markedDirty(false)
    , m_filterChanged(true)
    , m_wrapModeChanged(false)
    , m_immutable(false)
    , m_foreign(false)
    , m_mipLevels(1)
    , m_unnormalizeActive(0)
    , m_normalizeActive(0)
{
}

GLTexturePrivate::~GLTexturePrivate()
{
    if (m_texture != 0 && !m_foreign) {
        glDeleteTextures(1, &m_texture);
    }
}

void GLTexturePrivate::initStatic()
{
    if (!GLPlatform::instance()->isGLES()) {
        s_supportsFramebufferObjects = hasGLVersion(3, 0) || hasGLExtension("GL_ARB_framebuffer_object") || hasGLExtension(QByteArrayLiteral("GL_EXT_framebuffer_object"));
        s_supportsTextureStorage = hasGLVersion(4, 2) || hasGLExtension(QByteArrayLiteral("GL_ARB_texture_storage"));
        s_supportsTextureSwizzle = hasGLVersion(3, 3) || hasGLExtension(QByteArrayLiteral("GL_ARB_texture_swizzle"));
        // see https://www.opengl.org/registry/specs/ARB/texture_rg.txt
        s_supportsTextureFormatRG = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_ARB_texture_rg"));
        s_supportsTexture16Bit = true;
        s_supportsARGB32 = true;
        s_supportsUnpack = true;
    } else {
        s_supportsFramebufferObjects = true;
        s_supportsTextureStorage = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_EXT_texture_storage"));
        s_supportsTextureSwizzle = hasGLVersion(3, 0);
        // see https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_rg.txt
        s_supportsTextureFormatRG = hasGLVersion(3, 0) || hasGLExtension(QByteArrayLiteral("GL_EXT_texture_rg"));
        s_supportsTexture16Bit = hasGLExtension(QByteArrayLiteral("GL_EXT_texture_norm16"));

        // QImage::Format_ARGB32_Premultiplied is a packed-pixel format, so it's only
        // equivalent to GL_BGRA/GL_UNSIGNED_BYTE on little-endian systems.
        s_supportsARGB32 = QSysInfo::ByteOrder == QSysInfo::LittleEndian && hasGLExtension(QByteArrayLiteral("GL_EXT_texture_format_BGRA8888"));

        s_supportsUnpack = hasGLExtension(QByteArrayLiteral("GL_EXT_unpack_subimage"));
    }
}

void GLTexturePrivate::cleanup()
{
    s_supportsFramebufferObjects = false;
    s_supportsARGB32 = false;
    if (s_fbo) {
        glDeleteFramebuffers(1, &s_fbo);
        s_fbo = 0;
    }
}

bool GLTexture::isNull() const
{
    Q_D(const GLTexture);
    return GL_NONE == d->m_texture;
}

QSize GLTexture::size() const
{
    Q_D(const GLTexture);
    return d->m_size;
}

void GLTexture::setSize(const QSize &size)
{
    Q_D(GLTexture);
    if (!isNull()) {
        return;
    }
    d->m_size = size;
    d->updateMatrix();
}

void GLTexture::update(const QImage &image, const QPoint &offset, const QRect &src)
{
    if (image.isNull() || isNull()) {
        return;
    }

    Q_D(GLTexture);
    Q_ASSERT(!d->m_foreign);

    GLenum glFormat;
    GLenum type;
    QImage::Format uploadFormat;
    if (!GLPlatform::instance()->isGLES()) {
        const QImage::Format index = image.format();

        if (index < sizeof(formatTable) / sizeof(formatTable[0]) && formatTable[index].internalFormat
            && !(formatTable[index].type == GL_UNSIGNED_SHORT && !d->s_supportsTexture16Bit)) {
            glFormat = formatTable[index].format;
            type = formatTable[index].type;
            uploadFormat = index;
        } else {
            glFormat = GL_BGRA;
            type = GL_UNSIGNED_INT_8_8_8_8_REV;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        }
    } else {
        if (d->s_supportsARGB32) {
            glFormat = GL_BGRA_EXT;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_ARGB32_Premultiplied;
        } else {
            glFormat = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            uploadFormat = QImage::Format_RGBA8888_Premultiplied;
        }
    }
    bool useUnpack = d->s_supportsUnpack && image.format() == uploadFormat && !src.isNull();

    QImage im;
    if (useUnpack) {
        im = image;
        Q_ASSERT(im.depth() % 8 == 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, im.bytesPerLine() / (im.depth() / 8));
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, src.x());
        glPixelStorei(GL_UNPACK_SKIP_ROWS, src.y());
    } else {
        if (src.isNull()) {
            im = image;
        } else {
            im = image.copy(src);
        }
        if (im.format() != uploadFormat) {
            im = im.convertToFormat(uploadFormat);
        }
    }

    int width = image.width();
    int height = image.height();
    if (!src.isNull()) {
        width = src.width();
        height = src.height();
    }

    bind();

    glTexSubImage2D(d->m_target, 0, offset.x(), offset.y(), width, height, glFormat, type, im.constBits());

    unbind();

    if (useUnpack) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }
}

void GLTexture::discard()
{
    d_ptr = new GLTexturePrivate();
}

void GLTexture::bind()
{
    Q_D(GLTexture);
    Q_ASSERT(d->m_texture);

    glBindTexture(d->m_target, d->m_texture);

    if (d->m_markedDirty) {
        d->onDamage();
    }
    if (d->m_filterChanged) {
        GLenum minFilter = GL_NEAREST;
        GLenum magFilter = GL_NEAREST;

        switch (d->m_filter) {
        case GL_NEAREST:
            minFilter = magFilter = GL_NEAREST;
            break;

        case GL_LINEAR:
            minFilter = magFilter = GL_LINEAR;
            break;

        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
            magFilter = GL_NEAREST;
            minFilter = d->m_canUseMipmaps ? d->m_filter : GL_NEAREST;
            break;

        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_LINEAR:
            magFilter = GL_LINEAR;
            minFilter = d->m_canUseMipmaps ? d->m_filter : GL_LINEAR;
            break;
        }

        glTexParameteri(d->m_target, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(d->m_target, GL_TEXTURE_MAG_FILTER, magFilter);

        d->m_filterChanged = false;
    }
    if (d->m_wrapModeChanged) {
        glTexParameteri(d->m_target, GL_TEXTURE_WRAP_S, d->m_wrapMode);
        glTexParameteri(d->m_target, GL_TEXTURE_WRAP_T, d->m_wrapMode);
        d->m_wrapModeChanged = false;
    }
}

void GLTexture::generateMipmaps()
{
    Q_D(GLTexture);

    if (d->m_canUseMipmaps && d->s_supportsFramebufferObjects) {
        glGenerateMipmap(d->m_target);
    }
}

void GLTexture::unbind()
{
    Q_D(GLTexture);
    glBindTexture(d->m_target, 0);
}

void GLTexture::render(const QRect &rect, qreal scale)
{
    render(infiniteRegion(), rect, scale, false);
}

void GLTexture::render(const QRegion &region, const QRect &rect, qreal scale, bool hardwareClipping)
{
    Q_D(GLTexture);
    if (rect.isEmpty()) {
        return; // nothing to paint and m_vbo is likely nullptr and d->m_cachedSize empty as well, #337090
    }

    QRect destinationRect = scaledRect(rect, scale).toRect();
    if (destinationRect.size() != d->m_cachedSize) {
        d->m_cachedSize = destinationRect.size();
        QRect r(destinationRect);
        r.moveTo(0, 0);
        if (!d->m_vbo) {
            d->m_vbo = std::make_unique<GLVertexBuffer>(KWin::GLVertexBuffer::Static);
        }

        const float verts[4 * 2] = {
            // NOTICE: r.x/y could be replaced by "0", but that would make it unreadable...
            static_cast<float>(r.x()), static_cast<float>(r.y()),
            static_cast<float>(r.x()), static_cast<float>(r.y() + destinationRect.height()),
            static_cast<float>(r.x() + destinationRect.width()), static_cast<float>(r.y()),
            static_cast<float>(r.x() + destinationRect.width()), static_cast<float>(r.y() + destinationRect.height())};

        const float texWidth = (target() == GL_TEXTURE_RECTANGLE_ARB) ? width() : 1.0f;
        const float texHeight = (target() == GL_TEXTURE_RECTANGLE_ARB) ? height() : 1.0f;

        const float texcoords[4 * 2] = {
            0.0f, d->m_yInverted ? 0.0f : texHeight, // y needs to be swapped (normalized coords)
            0.0f, d->m_yInverted ? texHeight : 0.0f,
            texWidth, d->m_yInverted ? 0.0f : texHeight,
            texWidth, d->m_yInverted ? texHeight : 0.0f};

        d->m_vbo->setData(4, 2, verts, texcoords);
    }
    d->m_vbo->render(region, GL_TRIANGLE_STRIP, hardwareClipping);
}

GLuint GLTexture::texture() const
{
    Q_D(const GLTexture);
    return d->m_texture;
}

GLenum GLTexture::target() const
{
    Q_D(const GLTexture);
    return d->m_target;
}

void GLTexture::setTarget(const GLenum &target)
{
    Q_D(GLTexture);
    d->m_target = target;
}

GLenum GLTexture::filter() const
{
    Q_D(const GLTexture);
    return d->m_filter;
}

GLenum GLTexture::internalFormat() const
{
    Q_D(const GLTexture);
    return d->m_internalFormat;
}

void GLTexture::clear()
{
    Q_D(GLTexture);
    Q_ASSERT(!d->m_foreign);
    if (!GLTexturePrivate::s_fbo && GLFramebuffer::supported() && GLPlatform::instance()->driver() != Driver_Catalyst) { // fail. -> bug #323065
        glGenFramebuffers(1, &GLTexturePrivate::s_fbo);
    }

    if (GLTexturePrivate::s_fbo) {
        // Clear the texture
        GLuint previousFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<GLint *>(&previousFramebuffer));
        if (GLTexturePrivate::s_fbo != previousFramebuffer) {
            glBindFramebuffer(GL_FRAMEBUFFER, GLTexturePrivate::s_fbo);
        }
        glClearColor(0, 0, 0, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, d->m_texture, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        if (GLTexturePrivate::s_fbo != previousFramebuffer) {
            glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
        }
    } else {
        if (const int size = width() * height()) {
            std::vector<uint32_t> buffer(size, 0);
            bind();
            if (!GLPlatform::instance()->isGLES()) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width(), height(),
                                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer.data());
            } else {
                const GLenum format = d->s_supportsARGB32 ? GL_BGRA_EXT : GL_RGBA;
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width(), height(),
                                format, GL_UNSIGNED_BYTE, buffer.data());
            }
            unbind();
        }
    }
}

bool GLTexture::isDirty() const
{
    Q_D(const GLTexture);
    return d->m_markedDirty;
}

void GLTexture::setFilter(GLenum filter)
{
    Q_D(GLTexture);
    if (filter != d->m_filter) {
        d->m_filter = filter;
        d->m_filterChanged = true;
    }
}

void GLTexture::setWrapMode(GLenum mode)
{
    Q_D(GLTexture);
    if (mode != d->m_wrapMode) {
        d->m_wrapMode = mode;
        d->m_wrapModeChanged = true;
    }
}

void GLTexturePrivate::onDamage()
{
    // No-op
}

void GLTexture::setDirty()
{
    Q_D(GLTexture);
    d->m_markedDirty = true;
}

void GLTexturePrivate::updateMatrix()
{
    m_matrix[NormalizedCoordinates].setToIdentity();
    m_matrix[UnnormalizedCoordinates].setToIdentity();

    if (m_target == GL_TEXTURE_RECTANGLE_ARB) {
        m_matrix[NormalizedCoordinates].scale(m_size.width(), m_size.height());
    } else {
        m_matrix[UnnormalizedCoordinates].scale(1.0 / m_size.width(), 1.0 / m_size.height());
    }

    if (!m_yInverted) {
        m_matrix[NormalizedCoordinates].translate(0.0, 1.0);
        m_matrix[NormalizedCoordinates].scale(1.0, -1.0);

        m_matrix[UnnormalizedCoordinates].translate(0.0, m_size.height());
        m_matrix[UnnormalizedCoordinates].scale(1.0, -1.0);
    }
}

bool GLTexture::isYInverted() const
{
    Q_D(const GLTexture);
    return d->m_yInverted;
}

void GLTexture::setYInverted(bool inverted)
{
    Q_D(GLTexture);
    if (d->m_yInverted == inverted) {
        return;
    }

    d->m_yInverted = inverted;
    d->updateMatrix();
}

void GLTexture::setSwizzle(GLenum red, GLenum green, GLenum blue, GLenum alpha)
{
    Q_D(GLTexture);

    if (!GLPlatform::instance()->isGLES()) {
        const GLuint swizzle[] = {red, green, blue, alpha};
        glTexParameteriv(d->m_target, GL_TEXTURE_SWIZZLE_RGBA, (const GLint *)swizzle);
    } else {
        glTexParameteri(d->m_target, GL_TEXTURE_SWIZZLE_R, red);
        glTexParameteri(d->m_target, GL_TEXTURE_SWIZZLE_G, green);
        glTexParameteri(d->m_target, GL_TEXTURE_SWIZZLE_B, blue);
        glTexParameteri(d->m_target, GL_TEXTURE_SWIZZLE_A, alpha);
    }
}

int GLTexture::width() const
{
    Q_D(const GLTexture);
    return d->m_size.width();
}

int GLTexture::height() const
{
    Q_D(const GLTexture);
    return d->m_size.height();
}

QMatrix4x4 GLTexture::matrix(TextureCoordinateType type) const
{
    Q_D(const GLTexture);
    return d->m_matrix[type];
}

bool GLTexture::framebufferObjectSupported()
{
    return GLTexturePrivate::s_supportsFramebufferObjects;
}

bool GLTexture::supportsSwizzle()
{
    return GLTexturePrivate::s_supportsTextureSwizzle;
}

bool GLTexture::supportsFormatRG()
{
    return GLTexturePrivate::s_supportsTextureFormatRG;
}

QImage GLTexture::toImage() const
{
    if (target() != GL_TEXTURE_2D) {
        return QImage();
    }

    if (GLPlatform::instance()->isGLES()) {
        QImage ret(size(), QImage::Format_ARGB32_Premultiplied);
        GLFramebuffer fbo(const_cast<GLTexture*>(this));
        GLFramebuffer::pushFramebuffer(&fbo);
        glReadPixels(0, 0, width(), height(), GL_BGRA_EXT, GL_UNSIGNED_BYTE, ret.bits());
        GLFramebuffer::popFramebuffer();
        return ret;
    } else {
        QImage ret(size(), QImage::Format_RGBA8888_Premultiplied);
        GLint currentTextureBinding;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTextureBinding);

        if (GLuint(currentTextureBinding) != texture()) {
            glBindTexture(GL_TEXTURE_2D, texture());
        }
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, ret.bits());
        if (GLuint(currentTextureBinding) != texture()) {
            glBindTexture(GL_TEXTURE_2D, currentTextureBinding);
        }
        return ret;
    }
}

} // namespace KWin
