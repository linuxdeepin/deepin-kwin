/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_blob.h"
#include "drm_gpu.h"

namespace KWin
{

DrmBlobFactory::DrmBlobFactory(DrmGpu *gpu, uint32_t blobId)
    : m_gpu(gpu)
    , m_blobId(blobId)
{
}

DrmBlobFactory::~DrmBlobFactory()
{
    if (m_blobId) {
        drmModeDestroyPropertyBlob(m_gpu->fd(), m_blobId);
        m_blobId = 0;
    }
}

uint32_t DrmBlobFactory::blobId() const
{
    return m_blobId;
}

std::shared_ptr<DrmBlobFactory> DrmBlobFactory::create(DrmGpu *gpu, const void *data, size_t dataSize)
{
    uint32_t id = 0;
    if (drmModeCreatePropertyBlob(gpu->fd(), data, dataSize, &id) == 0) {
        return std::make_shared<DrmBlobFactory>(gpu, id);
    } else {
        return nullptr;
    }
}

}
