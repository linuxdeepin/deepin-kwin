// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright (C) 2018 Vlad Zagorodniy <vladzzag@gmail.com>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "idle_inhibition.h"
#include "deleted.h"
#include "shell_client.h"
#include "workspace.h"

#include <KWayland/Server/idle_interface.h>
#include <KWayland/Server/surface_interface.h>

#include <algorithm>
#include <functional>

using KWayland::Server::SurfaceInterface;

namespace KWin
{

IdleInhibition::IdleInhibition(IdleInterface *idle)
    : QObject(idle)
    , m_idle(idle)
{
    // Workspace is created after the wayland server is initialized.
    connect(kwinApp(), &Application::workspaceCreated, this, &IdleInhibition::slotWorkspaceCreated);
}

IdleInhibition::~IdleInhibition() = default;

void IdleInhibition::registerShellClient(ShellClient *client)
{
    auto updateInhibit = [this, client] {
        update(client);
    };

    m_connections[client] = connect(client->surface(), &SurfaceInterface::inhibitsIdleChanged, this, updateInhibit);
    connect(client, &ShellClient::desktopChanged, this, updateInhibit);
    connect(client, &ShellClient::clientMinimized, this, updateInhibit);
    connect(client, &ShellClient::clientUnminimized, this, updateInhibit);
    connect(client, &ShellClient::windowHidden, this, updateInhibit);
    connect(client, &ShellClient::windowShown, this, updateInhibit);
    connect(client, &ShellClient::windowClosed, this,
        [this, client] {
            uninhibit(client);
            auto it = m_connections.find(client);
            if (it != m_connections.end()) {
                disconnect(it.value());
                m_connections.erase(it);
            }
        }
    );

    updateInhibit();
}

void IdleInhibition::inhibit(AbstractClient *client)
{
    if (isInhibited(client)) {
        // already inhibited
        return;
    }
    m_idleInhibitors << client;
    m_idle->inhibit();
    // TODO: notify powerdevil?
}

void IdleInhibition::uninhibit(AbstractClient *client)
{
    auto it = std::find(m_idleInhibitors.begin(), m_idleInhibitors.end(), client);
    if (it == m_idleInhibitors.end()) {
        // not inhibited
        return;
    }
    m_idleInhibitors.erase(it);
    m_idle->uninhibit();
}

void IdleInhibition::update(AbstractClient *client)
{
    // TODO: Don't honor the idle inhibitor object if the shell client is not
    // on the current activity (currently, activities are not supported).
    const bool visible = client->isShown(true) && client->isOnCurrentDesktop();
    if (visible && client->surface()->inhibitsIdle()) {
        inhibit(client);
    } else {
        uninhibit(client);
    }
}

void IdleInhibition::slotWorkspaceCreated()
{
    connect(workspace(), &Workspace::currentDesktopChanged, this, &IdleInhibition::slotDesktopChanged);
}

void IdleInhibition::slotDesktopChanged()
{
    workspace()->forEachAbstractClient([this] (AbstractClient *c) { update(c); });
}

}
