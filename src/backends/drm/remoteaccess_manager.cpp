/********************************************************************
 *
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Oleg Chernovskiy <kanedias@xaker.ru>

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

#include "drm_output.h"
#include "remoteaccess_manager.h"
#include "drm_backend.h"
#include "wayland_server.h"
#include "drm_buffer_gbm.h"
#include "workspace.h"

// system
#include "wayland/output_interface.h"
#include "wayland/dderestrict_interface.h"
#include "core/output.h"
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
    if (!buf) {
        qCWarning(KWIN_DRM) << "buf is already released.";
        return;
    }
    if (buf->fd() != -1) {
        int ret = close(buf->fd());
        if (Q_UNLIKELY(ret)) {
            qCWarning(KWIN_DRM) << "Couldn't close released GBM fd:" << strerror(errno);
        }
    }
    delete buf;
}

void RemoteAccessManager::passBuffer(Output *output, DrmGpuBuffer *buffer)
{
    auto dde_restrict = waylandServer()->ddeRestrict();
    if (dde_restrict && dde_restrict->prohibitScreencast() && waylandServer()->hasProhibitWindows()) {
        return;
    }

    bool hasProtectedWindow = false;
    if (workspace() && workspace()->hasProtectedWindow())
        hasProtectedWindow = true;

    GbmBuffer* gbmbuf = static_cast<GbmBuffer *>(buffer);
    // no connected RemoteAccess instance
    if (!m_interface && m_interface->isBound()) {
        return;
    }

    // first buffer may be null
    if (!gbmbuf || !gbmbuf->bo()) {
        return;
    }

    auto buf = new BufferHandle;
    auto bo = gbmbuf->bo();
    buf->setSize(gbm_bo_get_width(bo), gbm_bo_get_height(bo));
    buf->setFormat(gbm_bo_get_format(bo));

    if (hasProtectedWindow) {
        if (output->shmRemoteProhibitBufferFd() == -1) {
            output->creatShmRemoteProhibitBuffer();
        } else if (output->modeSize() != output->shmRemoteProhibitBufferSize()) {
            output->freeShmRemoteProhibitBuffer();
            output->creatShmRemoteProhibitBuffer();
        }
        buf->setFd(output->dupShmRemoteProhibitBufferFd());
        buf->setStride(output->modeSize().width() * 4);
    } else {
        buf->setFd(gbm_bo_get_fd(bo));
        buf->setStride(gbm_bo_get_stride(bo));
    }

    if (buf->fd() == -1) {
        delete buf;
        return;
    }

    m_interface->sendBufferReady(waylandServer()->findWaylandOutput(output), buf);
}

void RemoteAccessManager::incrementRenderSequence()
{
    m_interface->incrementRenderSequence();
}

} // KWin namespace
