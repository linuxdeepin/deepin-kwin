/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <KWayland/Server/screencast_interface.h>

namespace KWin
{

class PipeWireStream;

class ScreencastManager : public QObject
{
    Q_OBJECT

public:
    explicit ScreencastManager(QObject *parent = nullptr);

    void streamWindow(KWayland::Server::ScreencastStreamV1Interface *stream, const QString &winid);
    void streamOutput(KWayland::Server::ScreencastStreamV1Interface *stream,
                      KWayland::Server::OutputInterface *output,
                      KWayland::Server::ScreencastV1Interface::CursorMode mode);

private:
    void integrateStreams(KWayland::Server::ScreencastStreamV1Interface *waylandStream, PipeWireStream *pipewireStream);

    KWayland::Server::ScreencastV1Interface *m_screencast;
};

} // namespace KWin
