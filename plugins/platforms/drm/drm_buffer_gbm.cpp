// Copyright 2015 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2017 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "drm_buffer_gbm.h"
#include "gbm_surface.h"

#include "logging.h"

// system
#include <sys/mman.h>
#include <errno.h>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <sys/sdt.h>
#include <unistd.h>

namespace KWin
{

// DrmSurfaceBuffer
DrmSurfaceBuffer::DrmSurfaceBuffer(int fd, const std::shared_ptr<GbmSurface> &surface)
    : DrmBuffer(fd)
    , m_surface(surface)
{
    m_bo = m_surface->lockFrontBuffer();
    if (!m_bo) {
        qCWarning(KWIN_DRM) << "Locking front buffer failed";
        return;
    }
    m_dmaFd = gbm_bo_get_fd(m_bo);
    m_size = QSize(gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo));
    if (drmModeAddFB(fd, m_size.width(), m_size.height(), 24, 32, gbm_bo_get_stride(m_bo),
                     gbm_bo_get_handle(m_bo).u32, &m_bufferId) != 0) {
        qCWarning(KWIN_DRM) << "drmModeAddFB failed";
    }
    gbm_bo_set_user_data(m_bo, this, nullptr);
}

DrmSurfaceBuffer::DrmSurfaceBuffer(int fd, const std::shared_ptr<GbmSurface> &surface,
                                   uint32_t format, QVector<uint64_t> &modifiers)
    : DrmBuffer(fd)
    , m_surface(surface)
{
    m_bo = m_surface->lockFrontBuffer();
    if (!m_bo) {
        qCWarning(KWIN_DRM) << "Locking front buffer failed";
        return;
    }
    m_size = QSize(gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo));
    uint32_t strides[4] = { };
    uint32_t handles[4] = { };
    uint32_t offsets[4] = { };
    uint64_t mods[4] = { };

    handles[0] = gbm_bo_get_handle(m_bo).u32;
    strides[0] = gbm_bo_get_stride(m_bo);
    mods[0] = modifiers.data()[0];
    m_dmaFd = gbm_bo_get_fd(m_bo);
    if (drmModeAddFB2WithModifiers(fd, m_size.width(), m_size.height(), format,
                                   handles, strides,
                                   offsets, mods, &m_bufferId, DRM_MODE_FB_MODIFIERS) != 0) {
        qCWarning(KWIN_DRM) << "drmModeAddFB2WithModifiers failed";
    }

    gbm_bo_set_user_data(m_bo, this, nullptr);
}

DrmSurfaceBuffer::~DrmSurfaceBuffer()
{
    if (m_bufferId) {
        drmModeRmFB(fd(), m_bufferId);
    }
    releaseGbm();
}

void DrmSurfaceBuffer::releaseGbm()
{
    if (m_dmaFd > 0) {
        close(m_dmaFd);
        m_dmaFd = 0;
    }
    m_surface->releaseBuffer(m_bo);
    DTRACE_PROBE(DrmSurfaceBuffer, releaseGbm);
    m_bo = nullptr;
}

}
