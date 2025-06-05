/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QImage>
#include <QObject>
#include <QSize>
#include <memory>

struct wl_resource;

namespace KWaylandServer
{
class ClientBufferPrivate;

/**
 * The ClientBuffer class represents a client buffer.
 *
 * While the ClientBuffer is referenced, it won't be destroyed. Note that the client can
 * still destroy the wl_buffer object while the ClientBuffer is referenced by the compositor.
 * You can use the isDestroyed() function to check whether the wl_buffer object has been
 * destroyed.
 */
class KWIN_EXPORT ClientBuffer : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(ClientBuffer)

public:
    ~ClientBuffer() override;

    /**
     * This enum type is used to specify the corner where the origin is. That's it, the
     * buffer corner where 0,0 is located.
     */
    enum class Origin {
        TopLeft,
        BottomLeft,
    };

    bool isReferenced() const;
    void sendRelease();
    bool isDestroyed() const;

    void ref();
    void unref();

    /**
     * Returns the wl_resource for this ClientBuffer. If the buffer is destroyed, @c null
     * will be returned.
     */
    wl_resource *resource() const;

    /**
     * Returns the size in the native pixels. The returned size is unaffected by buffer
     * scale or other surface transforms, e.g. @c wp_viewport.
     */
    virtual QSize size() const = 0;
    virtual bool hasAlphaChannel() const = 0;
    virtual Origin origin() const = 0;

    void markAsDestroyed(); ///< @internal

protected:
    ClientBuffer(ClientBufferPrivate &dd);
    ClientBuffer(wl_resource *resource, ClientBufferPrivate &dd);

    void initialize(wl_resource *resource);
    std::unique_ptr<ClientBufferPrivate> d_ptr;
};

class ClientBufferRef
{
public:
    ClientBufferRef()
        : m_buffer(nullptr)
    {
    }
    ClientBufferRef(ClientBuffer *buffer)
        : m_buffer(buffer)
    {
        Q_ASSERT(m_buffer);
        m_buffer->ref();
    }
    ~ClientBufferRef()
    {
        if (m_buffer) {
            m_buffer->unref();
        }
    }
    ClientBufferRef(const ClientBufferRef &other)
        : m_buffer(other.m_buffer)
    {
        if (m_buffer) {
            m_buffer->ref();
        }
    }
    ClientBufferRef(ClientBufferRef &&other)
        : m_buffer(std::exchange(other.m_buffer, nullptr))
    {
    }
    ClientBufferRef &operator=(ClientBufferRef &&other)
    {
        if (m_buffer) {
            m_buffer->unref();
        }
        m_buffer = std::exchange(other.m_buffer, nullptr);
        return *this;
    }
    ClientBufferRef &operator=(const ClientBufferRef &other)
    {
        if (other.m_buffer) {
            other.m_buffer->ref();
        }
        if (m_buffer) {
            m_buffer->unref();
        }
        m_buffer = other.m_buffer;
        return *this;
    }
    ClientBuffer *operator->() const
    {
        return m_buffer;
    }
    ClientBuffer *operator*() const
    {
        return m_buffer;
    }
    ClientBuffer *buffer() const
    {
        return m_buffer;
    }
    inline operator bool() const
    {
        return m_buffer != nullptr;
    }
    ClientBufferRef &operator=(ClientBuffer *buffer)
    {
        if (m_buffer != buffer) {
            if (m_buffer) {
                m_buffer->unref();
            }
            if (buffer) {
                buffer->ref();
            }
            m_buffer = buffer;
        }
        return *this;
    }

private:
    ClientBuffer *m_buffer;
};
} // namespace KWaylandServer
