/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "kwinglplatform.h"
#include "kwingltexture.h"
#include "kwinglutils.h"

#include <spa/buffer/buffer.h>
#include <spa/param/video/raw.h>
#include "../../backends/drm/drm_egl_backend.h"
#include "composite.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>


namespace KWin
{

// in-place vertical mirroring
static void mirrorVertically(uchar *data, int height, int stride)
{
    const int halfHeight = height / 2;
    std::vector<uchar> temp(stride);
    for (int y = 0; y < halfHeight; ++y) {
        auto cur = &data[y * stride], dest = &data[(height - y - 1) * stride];
        memcpy(temp.data(), cur, stride);
        memcpy(cur, dest, stride);
        memcpy(dest, temp.data(), stride);
    }
}
static GLenum closestGLType(spa_video_format format)
{
    switch (format) {
    case SPA_VIDEO_FORMAT_RGB:
        return GL_RGB;
    case SPA_VIDEO_FORMAT_BGR:
        return GL_BGR;
    case SPA_VIDEO_FORMAT_RGBx:
    case SPA_VIDEO_FORMAT_RGBA:
        return GL_RGBA;
    case SPA_VIDEO_FORMAT_BGRA:
    case SPA_VIDEO_FORMAT_BGRx:
        return GL_BGRA;
    default:
        qDebug() << "unknown format" << format;
        return GL_RGBA;
    }
}

static void grabTexture(GLTexture *texture, spa_data *spa, spa_video_format format)
{
    const QSize size = texture->size();
    const bool isGLES = GLPlatform::instance()->isGLES();
    const bool invertNeeded = isGLES ^ texture->isYInverted();
    const bool invertNeededAndSupported = invertNeeded && GLPlatform::instance()->supports(PackInvert);
    GLboolean prev;
    if (invertNeededAndSupported) {
        glGetBooleanv(GL_PACK_INVERT_MESA, &prev);
        glPixelStorei(GL_PACK_INVERT_MESA, GL_TRUE);
    }
    const uint stride = SPA_ROUND_UP_N (size.width() * 4, 4);
    spa->chunk->stride = stride;
    spa->chunk->size = spa->maxsize;
    EglGbmBackend * eglGbmBackend = static_cast<EglGbmBackend *>(Compositor::self()->backend());
    spa->fd = eglGbmBackend ? eglGbmBackend->getFrameFd() : 0;

    if (spa->fd) {
        unsigned char *map_data = static_cast<unsigned char *>((mmap(NULL, stride * size.height(), PROT_READ, MAP_SHARED, spa->fd, 0)));

        if (map_data != MAP_FAILED) {
            memcpy(spa->data, map_data, stride * size.height());
            munmap(map_data, stride * size.height());
            map_data = NULL;
        } else {
            texture->bind();
            if (isGLES) {
                GLFramebuffer fbo(texture);
                GLFramebuffer::pushFramebuffer(&fbo);
                glReadPixels(0, 0, size.width(), size.height(), closestGLType(format), GL_UNSIGNED_BYTE, spa->data);
                GLFramebuffer::popFramebuffer();
            } else if (GLPlatform::instance()->glVersion() >= kVersionNumber(4, 5)) {
                glGetTextureImage(texture->texture(), 0, closestGLType(format), GL_UNSIGNED_BYTE, spa->chunk->size, spa->data);
            } else {
                glGetTexImage(texture->target(), 0, closestGLType(format), GL_UNSIGNED_BYTE, spa->data);
            }
        }

    } else {
        texture->bind();
        if (isGLES) {
            GLFramebuffer fbo(texture);
            GLFramebuffer::pushFramebuffer(&fbo);
            glReadPixels(0, 0, size.width(), size.height(), closestGLType(format), GL_UNSIGNED_BYTE, spa->data);
            GLFramebuffer::popFramebuffer();
        } else if (GLPlatform::instance()->glVersion() >= kVersionNumber(4, 5)) {
            glGetTextureImage(texture->texture(), 0, closestGLType(format), GL_UNSIGNED_BYTE, spa->chunk->size, spa->data);
        } else {
            glGetTexImage(texture->target(), 0, closestGLType(format), GL_UNSIGNED_BYTE, spa->data);
        }
    }

    if (invertNeededAndSupported) {
        if (!prev) {
            glPixelStorei(GL_PACK_INVERT_MESA, prev);
        }
    } else if (invertNeeded) {
        mirrorVertically(static_cast<uchar *>(spa->data), size.height(), spa->chunk->stride);
    }
}

} // namespace KWin
