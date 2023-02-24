// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REMOTEACCESSMANAGER_H
#define REMOTEACCESSMANAGER_H

// DWayland
#include <DWayland/Server/remote_access_interface.h>

// Qt
#include <QObject>

struct gbm_bo;
struct gbm_surface;

namespace KWin
{

class DrmOutput;
class DrmBuffer;
class DrmGbmBuffer;
class Display;
class WaylandOutput;

using KWaylandServer::RemoteAccessManagerInterface;
using KWaylandServer::BufferHandle;

class RemoteAccessManager : public QObject
{
    Q_OBJECT
public:
    explicit RemoteAccessManager(QObject *parent = nullptr);
    ~RemoteAccessManager() override;

    void passBuffer(WaylandOutput *output, DrmBuffer *buffer);
    void incrementRenderSequence();

Q_SIGNALS:
    void bufferNoLongerNeeded(qint32 gbm_handle);
    void screenRecordStatusChanged(bool isScreenRecording);

private:
    void releaseBuffer(const BufferHandle *buf);

    RemoteAccessManagerInterface *m_interface = nullptr;
};

} // KWin namespace

#endif // REMOTEACCESSMANAGER_H
