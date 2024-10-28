/*
    SPDX-FileCopyrightText: 2016 Oleg Chernovskiy <kanedias@xaker.ru>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "remote_access_interface.h"
#include "remote_access_interface_p.h"

#include <qwayland-server-remote-access.h>

#include "display.h"
#include "output_interface.h"
#include "utils/common.h"

#include <QDebug>
#include <QHash>
#include <QMutableHashIterator>
#include <QPointer>

#include <fcntl.h>
#include <functional>
#include <sys/time.h>
#include <unistd.h>

namespace KWaylandServer
{

using namespace std::chrono;
using namespace std::chrono_literals;

static const QString SCREEN_RECORDING_START = QStringLiteral("screenRecordingStart");
static const QString SCREEN_RECORDING_FINISHED = QStringLiteral("screenRecordingStop");
static QObject gsScreenRecord;

static constexpr int MAX_BUFFERS_SIZE = 100;
static constexpr auto KEEP_ALIVE_SECOND = 5s;

struct BufferHandlePrivate // @see gbm_import_fd_data
{
    ~BufferHandlePrivate() {
        if (fd != -1) {
            if (Q_UNLIKELY(close(fd))) {
                qCWarning(KWIN_CORE) << "Couldn't close released fd:" << strerror(errno);
            }
        }
    }
    // Note that on client side received fd number will be different
    // and meaningful only for client process!
    // Thus we can use server-side fd as an implicit unique id
    qint32 fd = -1; ///< also internal buffer id for client
    quint32 width = 0;
    quint32 height = 0;
    quint32 stride = 0;
    quint32 format = 0;
    qint32 frame = -1;
};

BufferHandle::BufferHandle()
    : QObject(nullptr)
    ,d(new BufferHandlePrivate)
{
}

BufferHandle::~BufferHandle()
{
}

BufferHandle &BufferHandle::setFd(qint32 fd)
{
    if (d == nullptr) {
        return *this;
    }
    d->fd = fd;
    return *this;
}

qint32 BufferHandle::fd() const
{
    if (!d) {
        return -1;
    }
    return d->fd;
}

BufferHandle &BufferHandle::setSize(quint32 width, quint32 height)
{
    d->width = width;
    d->height = height;
    return *this;
}

quint32 BufferHandle::width() const
{
    return d->width;
}

quint32 BufferHandle::height() const
{
    return d->height;
}

BufferHandle &BufferHandle::setStride(quint32 stride)
{
    d->stride = stride;
    return *this;
}

quint32 BufferHandle::stride() const
{
    return d->stride;
}

BufferHandle &BufferHandle::setFormat(quint32 format)
{
    d->format = format;
    return *this;
}

quint32 BufferHandle::format() const
{
    return d->format;
}

BufferHandle &BufferHandle::setFrame(qint32 frame)
{
    d->frame = frame;
    return *this;
}

qint32 BufferHandle::frame() const
{
    return d->frame;
}

/**
 * @brief helper struct for manual reference counting.
 * automatic counting via QSharedPointer is no-go here as we hold strong reference in sentBuffers.
 * @note must be copyable
 */
struct BufferHolder
{
    QPointer<BufferHandle> buf = nullptr; ///< this pointer use to strong reference the allocated buf in heap. should only be destroy by org_kde_kwin_remote_access_manager_release request.
    quint64 counter = 0;
    seconds lifetime = 0s;
};

class RemoteAccessManagerInterfacePrivate : public QtWaylandServer::org_kde_kwin_remote_access_manager
{
public:
    RemoteAccessManagerInterfacePrivate(RemoteAccessManagerInterface *q, Display *display);
    ~RemoteAccessManagerInterfacePrivate() override;

    /**
     * @brief Send buffer ready notification to all connected clients
     * @param output wl_output interface to determine which screen sent this buf
     * @param buf buffer containing GBM-related params
     */
    void sendBufferReady(const OutputInterface *output, QPointer<BufferHandle> buf);
    /**
     * @brief Release all bound buffers associated with this resource
     * @param resource one of bound clients
     */
    void release(wl_resource *resource);

    void incrementRenderSequence();

    Display *display;
    int renderSequence = 0;

private:
    virtual void org_kde_kwin_remote_access_manager_get_buffer(Resource *resource, uint32_t buffer, int32_t internal_buffer_id) override;
    virtual void org_kde_kwin_remote_access_manager_release(Resource *resource) override;
    virtual void org_kde_kwin_remote_access_manager_record(Resource *resource, int32_t frame) override;
    virtual void org_kde_kwin_remote_access_manager_get_rendersequence(Resource *resource) override;

    /**
     * @brief Unreferences counter and frees buffer when it reaches zero
     * @param bh holder to decrease reference counter on
     * @return true if buffer was released, false otherwise
     */
    bool unref(BufferHolder &bh);
    /**
     * @brief Clear sent buffer, regardless of whether its refence count is 0 or not
     * @param bh holder to be destroy
     */
    void remove(const BufferHolder &bh);

    static const quint32 s_version;

    RemoteAccessManagerInterface *q;

    /**
     * Buffers that were sent but still not acked by server
     * Keys are fd numbers as they are unique
     **/
    QHash<qint32, BufferHolder> sentBuffers;
    QHash<wl_resource *, qint32> requestFrames;
    QHash<wl_resource*, qint32> lastFrames;


    /**
     * Clients of this interface.
     * This may be screenshot app, video capture app,
     * remote control app etc.
     */
    QList<Resource *> clientResources;

protected:
    void org_kde_kwin_remote_access_manager_bind_resource(Resource *resource) override;
};

const quint32 RemoteAccessManagerInterfacePrivate::s_version = 2;

RemoteAccessManagerInterfacePrivate::RemoteAccessManagerInterfacePrivate(RemoteAccessManagerInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_remote_access_manager(*display, s_version)
    , display(display)
    , q(_q)
{
}

void RemoteAccessManagerInterfacePrivate::sendBufferReady(const OutputInterface *output, QPointer<BufferHandle> buf)
{
    BufferHolder holder{buf, 0, duration_cast<seconds>(steady_clock::now().time_since_epoch())};
    // notify clients
    for (auto res : resourceMap()) {
        auto client = wl_resource_get_client(res->handle);
        auto boundScreens = output->clientResources(display->getConnection(client));

        // clients don't necessarily bind outputs
        if (boundScreens.isEmpty()) {
            continue;
        }

        int frame = -1;
        if (requestFrames.contains(res->handle)) {
            frame = requestFrames[res->handle];
        }

        if (frame) {
            // no reason for client to bind wl_output multiple times, send only to first one
            send_buffer_ready(res->handle, buf->fd(), boundScreens[0]);
            holder.counter++;
            if (frame == 1) {
                lastFrames[res->handle] = buf->fd();
            }
        }
        if (frame > 0) {
            requestFrames[res->handle] = frame - 1;
        }
    }
    if (holder.counter == 0) {
        // buffer was not requested by any client
        Q_EMIT q->bufferReleased(buf);
        return;
    }

    // store buffer locally, clients will ask it later
    sentBuffers[buf->fd()] = holder;

    // NOTE: for applications which consume buffers too slow, simply close too old buffers to ensure kwin not quit.
    //       In this circumstances, applications should handle the potential exception: fd has been closed by compositor.
    if (Q_UNLIKELY(sentBuffers.size() > MAX_BUFFERS_SIZE)) {
        const auto &fds = sentBuffers.keys();
        int fdNums = fds.size();
        for (qint32 fd : fds) {
            if (fdNums <= MAX_BUFFERS_SIZE) {
                break;
            }
            // Remove buffers which are so old that they are unlikely to used by clients forcely.
            // Clients still have buffer and will send buffer_release request in the future.
            if (holder.lifetime - sentBuffers[fd].lifetime > KEEP_ALIVE_SECOND) {
                remove(sentBuffers[fd]);
                fdNums--;
            }
        }
        qCDebug(KWIN_CORE) << "Current buffer size over" << MAX_BUFFERS_SIZE <<", released forcely. Now buffer size is" << sentBuffers.size();
    }
}

void RemoteAccessManagerInterfacePrivate::incrementRenderSequence()
{
    renderSequence++;
}

void RemoteAccessManagerInterfacePrivate::org_kde_kwin_remote_access_manager_bind_resource(Resource *resource)
{
    // add newly created client resource to the list
    clientResources << resource;
    Q_EMIT q->addedClient();
}

bool RemoteAccessManagerInterfacePrivate::unref(BufferHolder &bh)
{
    if (bh.counter != 0) {
        bh.counter--;
        if (bh.counter == 0) {
            // no more clients using this buffer
            remove(bh);
            return true;
        }
        return false;
    }
    return false;
}

void RemoteAccessManagerInterfacePrivate::remove(const BufferHolder &bh)
{
    if (!bh.buf) {
        return;
    }
    int fd = bh.buf->fd();
    Q_EMIT q->bufferReleased(bh.buf);
    sentBuffers.remove(fd);
}

void RemoteAccessManagerInterfacePrivate::org_kde_kwin_remote_access_manager_get_buffer(Resource *resource, uint32_t buffer, int32_t internal_buffer_id)
{
    // client asks for buffer we earlier announced, we must have it
    if (Q_UNLIKELY(!this->sentBuffers.contains(internal_buffer_id))) { // no such buffer (?)
        wl_resource_post_no_memory(resource->handle);
        return;
    }

    BufferHolder &bh = sentBuffers[internal_buffer_id];
    if (requestFrames.contains(resource->handle)) {
        if (lastFrames[resource->handle] == bh.buf->fd()) {
            bh.buf->setFrame(0);
        }
    }
    wl_resource *RbiResource = wl_resource_create(resource->client(), &org_kde_kwin_remote_buffer_interface, resource->version(), buffer);

    if (Q_UNLIKELY(!RbiResource)) {
        qCDebug(KWIN_CORE) << resource->client() << buffer << internal_buffer_id;
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto rbuf = new RemoteBufferInterface(bh.buf, RbiResource);

    QObject::connect(rbuf, &QObject::destroyed, [resource, internal_buffer_id, this] {
        if (!clientResources.contains(resource) || !sentBuffers.contains(internal_buffer_id)) {
            // remote buffer destroy confirmed after client is already gone
            // all relevant buffers are already unreferenced
            return;
        }
        unref(sentBuffers[internal_buffer_id]);
        gsScreenRecord.setObjectName(SCREEN_RECORDING_FINISHED);
    });

    // unref resource when fd return -1
    if (rbuf->sendGbmHandle() == -1) {
        delete rbuf;
    }

    gsScreenRecord.setObjectName(SCREEN_RECORDING_START);
}

void RemoteAccessManagerInterfacePrivate::org_kde_kwin_remote_access_manager_release(Resource *resource)
{
    requestFrames.remove(resource->handle);
    lastFrames.remove(resource->handle);
    clientResources.removeAll(resource);

    // erese all fds which are no longer used by clients and close them
    // std::for_each(sentBuffers.begin(), sentBuffers.end(), [this](BufferHolder& bh) { remove(bh); }); // client is responsable for deletion

    wl_resource_destroy(resource->handle);
}

void RemoteAccessManagerInterfacePrivate::org_kde_kwin_remote_access_manager_record(Resource *resource, int32_t frame)
{
    requestFrames[resource->handle] = frame;
    Q_EMIT q->startRecord(frame);
}

void RemoteAccessManagerInterfacePrivate::org_kde_kwin_remote_access_manager_get_rendersequence(Resource *resource)
{
    send_rendersequence(resource->handle, renderSequence);
}

RemoteAccessManagerInterfacePrivate::~RemoteAccessManagerInterfacePrivate()
{
}

RemoteAccessManagerInterface::RemoteAccessManagerInterface(Display *display)
    : QObject(nullptr)
    , d(new RemoteAccessManagerInterfacePrivate(this, display))
{
    connect(&gsScreenRecord, &QObject::objectNameChanged, this, [=](const QString &name) {
        Q_EMIT screenRecordStatusChanged(name == SCREEN_RECORDING_START);
    });
}

RemoteAccessManagerInterface::~RemoteAccessManagerInterface()
{
}

void RemoteAccessManagerInterface::sendBufferReady(const OutputInterface *output, QPointer<BufferHandle> buf)
{
    d->sendBufferReady(output, buf);
}

void RemoteAccessManagerInterface::incrementRenderSequence()
{
    d->incrementRenderSequence();
}

bool RemoteAccessManagerInterface::isBound() const
{
    return !d->resourceMap().isEmpty();
}

class RemoteBufferInterfacePrivate : public QtWaylandServer::org_kde_kwin_remote_buffer
{
public:
    RemoteBufferInterfacePrivate(RemoteBufferInterface *q, QPointer<BufferHandle> buf, wl_resource *resource);
    ~RemoteBufferInterfacePrivate() override;

    int sendGbmHandle();

    virtual void org_kde_kwin_remote_buffer_release(Resource *resource) override;
    virtual void org_kde_kwin_remote_buffer_destroy_resource(Resource *resource) override;

private:
    RemoteBufferInterface *q;
    QPointer<BufferHandle> wrapped;
};

RemoteBufferInterfacePrivate::RemoteBufferInterfacePrivate(RemoteBufferInterface *q, QPointer<BufferHandle> buf, wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_remote_buffer(resource)
    , q(q)
    , wrapped(buf)
{
}

RemoteBufferInterfacePrivate::~RemoteBufferInterfacePrivate()
{
}

int RemoteBufferInterfacePrivate::sendGbmHandle()
{
    if (!wrapped) {
        return -1;
    }
    if (fcntl(wrapped->fd(), F_GETFL) == -1) {
        return -1;
    }
    send_gbm_handle(wrapped->fd(), wrapped->width(), wrapped->height(), wrapped->stride(), wrapped->format(), wrapped->frame());
    return 0;
}

void RemoteBufferInterfacePrivate::org_kde_kwin_remote_buffer_release(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void RemoteBufferInterfacePrivate::org_kde_kwin_remote_buffer_destroy_resource(Resource *resource)
{
    delete q;
}

RemoteBufferInterface::RemoteBufferInterface(QPointer<BufferHandle> buf, wl_resource *resource)
    : QObject()
    , d(new RemoteBufferInterfacePrivate(this, buf, resource))
{
}

RemoteBufferInterface::~RemoteBufferInterface()
{
}

int RemoteBufferInterface::sendGbmHandle()
{
    return d->sendGbmHandle();
}

}