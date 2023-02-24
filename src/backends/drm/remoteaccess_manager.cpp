// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "drm_output.h"
#include "remoteaccess_manager.h"
#include "logging.h"
#include "drm_backend.h"
#include "wayland_server.h"
#include "drm_buffer_gbm.h"
#include "waylandoutput.h"

// system
#include <DWayland/Server/output_interface.h>
#include <unistd.h>
#include <gbm.h>

#include <cerrno>

namespace KWin
{

RemoteAccessManager::RemoteAccessManager(QObject *parent)
    : QObject(parent)
{
    if (waylandServer()) {
        m_interface = new RemoteAccessManagerInterface(waylandServer()->display());

        connect(m_interface, &RemoteAccessManagerInterface::screenRecordStatusChanged, this, [=](bool isScreenRecording) {
                Q_EMIT screenRecordStatusChanged(isScreenRecording);
            });

        connect(m_interface, &RemoteAccessManagerInterface::bufferReleased,
                this, &RemoteAccessManager::releaseBuffer);
    }
}

RemoteAccessManager::~RemoteAccessManager()
{
    if (m_interface) {
        delete m_interface;
    }
}

void RemoteAccessManager::releaseBuffer(const BufferHandle *buf)
{
    int ret = close(buf->fd());
    if (Q_UNLIKELY(ret)) {
        qCWarning(KWIN_DRM) << "Couldn't close released GBM fd:" << strerror(errno);
    }
    delete buf;
}

void RemoteAccessManager::passBuffer(WaylandOutput *output, DrmBuffer *buffer)
{
    DrmGbmBuffer* gbmbuf = static_cast<DrmGbmBuffer *>(buffer);

    // no connected RemoteAccess instance
    if (!m_interface && m_interface->isBound()) {
        return;
    }

    // first buffer may be null
    if (!gbmbuf || !gbmbuf->hasBo()) {
        return;
    }

    auto buf = new BufferHandle;
    auto bo = gbmbuf->getBo();
    buf->setFd(gbm_bo_get_fd(bo));
    buf->setSize(gbm_bo_get_width(bo), gbm_bo_get_height(bo));
    buf->setStride(gbm_bo_get_stride(bo));
    buf->setFormat(gbm_bo_get_format(bo));

    m_interface->sendBufferReady(output->waylandOutput(), buf);
}

void RemoteAccessManager::incrementRenderSequence()
{
    m_interface->incrementRenderSequence();
}

} // KWin namespace
