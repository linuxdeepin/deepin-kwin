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

#include "remoteaccess_manager.h"
#include "drm_backend.h"
#include "drm_buffer_gbm.h"
#include "drm_dumb_buffer.h"
#include "drm_output.h"
#include "wayland_server.h"
#include "workspace.h"

// system
#include "core/output.h"
#include "wayland/output_interface.h"
#include <gbm.h>

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
        connect(m_interface, &RemoteAccessManagerInterface::addedClient, this, [=]() {
            Q_EMIT addedClient();
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

void RemoteAccessManager::releaseBuffer(QPointer<BufferHandle> buf)
{
    if (!buf) {
        qCWarning(KWIN_CORE) << "buf is already released.";
        return;
    }
    m_gbmBufferList.erase(buf);
    delete buf;
}

void RemoteAccessManager::passBuffer(Output *output, std::shared_ptr<DrmGpuBuffer> buffer)
{
    if (!buffer) {
        qWarning(KWIN_CORE) << "Original buffer has been destroyed!";
        return;
    }

    if (!hasConnections()) {
        return;
    }

    if (std::shared_ptr<GbmBuffer> gbmbuf = std::dynamic_pointer_cast<GbmBuffer>(buffer)) {
        passGbmBuffer(output, gbmbuf);
    } else if (std::shared_ptr<DrmDumbBuffer> dumbuf = std::dynamic_pointer_cast<DrmDumbBuffer>(buffer)) {
        passDumBuffer(output, dumbuf);
    }
}

bool RemoteAccessManager::hasConnections() const
{
    return m_interface && m_interface->isBound();
}

void RemoteAccessManager::passGbmBuffer(Output *output, std::shared_ptr<GbmBuffer> gbmbuf)
{
    auto bo = gbmbuf->bo();
    if (!bo) {
        return;
    }

    auto buf = new BufferHandle;
    buf->setSize(gbm_bo_get_width(bo), gbm_bo_get_height(bo));
    buf->setFormat(gbm_bo_get_format(bo));
    buf->setFd(gbm_bo_get_fd(bo));
    buf->setStride(gbm_bo_get_stride(bo));

    if (buf->fd() == -1) {
        delete buf;
        return;
    }
    m_gbmBufferList[buf] = gbmbuf;

    m_interface->sendBufferReady(waylandServer()->findWaylandOutput(output), buf);
}

void RemoteAccessManager::passDumBuffer(Output *output, std::shared_ptr<DrmDumbBuffer> dumbuf)
{
    auto buf = new BufferHandle;
    uint32_t stride = dumbuf->strides()[0];


    buf->setSize(dumbuf->size().width(), dumbuf->size().height());
    buf->setFormat(dumbuf->format());
    buf->setStride(stride);
    Output::shm_rp_buffer *shmbuf = output->creatForDumpBuffer(dumbuf->size(), dumbuf->data());
    if (!shmbuf) {
        delete buf;
        return;
    }
    buf->setFd(shmbuf->fd);

    if (buf->fd() == -1) {
        delete buf;
        return;
    }

    connect(buf, &QObject::destroyed, this, [this, output, shmbuf]() {
        if (output && output != m_removedOutput) {
            output->destroyForDumpBuffer(shmbuf);
        }
    });

    connect(workspace(), &Workspace::outputRemoved, this, [this, output](Output *o) {
        if (output == o) {
            m_removedOutput = output;
        }
    });

    m_interface->sendBufferReady(waylandServer()->findWaylandOutput(output), buf);
}

void RemoteAccessManager::incrementRenderSequence()
{
    m_interface->incrementRenderSequence();
}

} // KWin namespace
