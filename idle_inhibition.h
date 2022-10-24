// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QVector>
#include <QMap>

namespace KWayland
{
namespace Server
{
class IdleInterface;
}
}

using KWayland::Server::IdleInterface;

namespace KWin
{
class AbstractClient;
class ShellClient;

class IdleInhibition : public QObject
{
    Q_OBJECT
public:
    explicit IdleInhibition(IdleInterface *idle);
    ~IdleInhibition();

    void registerShellClient(ShellClient *client);

    bool isInhibited() const {
        return !m_idleInhibitors.isEmpty();
    }
    bool isInhibited(AbstractClient *client) const {
        return m_idleInhibitors.contains(client);
    }

private Q_SLOTS:
    void slotWorkspaceCreated();
    void slotDesktopChanged();

private:
    void inhibit(AbstractClient *client);
    void uninhibit(AbstractClient *client);
    void update(AbstractClient *client);

    IdleInterface *m_idle;
    QVector<AbstractClient *> m_idleInhibitors;
    QMap<AbstractClient *, QMetaObject::Connection> m_connections;
};
}
