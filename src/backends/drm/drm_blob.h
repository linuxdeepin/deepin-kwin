/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <memory>

namespace KWin
{

class DrmGpu;

class DrmBlob
{
public:
    DrmBlob(DrmGpu *gpu, uint32_t blobId);
    virtual ~DrmBlob();

    uint32_t blobId() const;
    /**
     * @brief Create a new blob object
     * @details This factory function helps to implement composition instead of inheritance,
     * witch can allocate actual blob on demand, not on construction time.
     */
    static std::shared_ptr<DrmBlob> create(DrmGpu *gpu, const void *data, size_t dataSize);

protected:
    DrmGpu *const m_gpu;
    uint32_t m_blobId;
};

}
