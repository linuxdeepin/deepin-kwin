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

class DrmBlobFactory
{
public:
    DrmBlobFactory(DrmGpu *gpu, uint32_t blobId);
    virtual ~DrmBlobFactory();

    uint32_t blobId() const;
    /**
     * @brief Create a new blob object
     * @details This factory function helps to implement composition instead of inheritance,
     * which can allocate actual blob on demand, not on construction time.
     * @note don't abuse this function in DrmBlob's subclass, otherwise will cause blob leaks.
     */
    static std::shared_ptr<DrmBlobFactory> create(DrmGpu *gpu, const void *data, size_t dataSize);

protected:
    DrmGpu *const m_gpu;
    uint32_t m_blobId;
};

class DrmObject;

/*
 * Base class for in-place-constructed blobs
 */
template<typename Object, typename Object::PropertyIndex Index>
class DrmBlob
{
    static_assert(std::is_base_of_v<KWin::DrmObject, Object>,
        "Object must derive from KWin::DrmObject");
public:
    explicit DrmBlob(Object *obj)
        : m_object(obj)
    {
    }

    std::shared_ptr<DrmBlobFactory> blob() const
    {
        return m_blob;
    }

protected:
    Object *m_object{nullptr};
    std::shared_ptr<DrmBlobFactory> m_blob;
};

}
