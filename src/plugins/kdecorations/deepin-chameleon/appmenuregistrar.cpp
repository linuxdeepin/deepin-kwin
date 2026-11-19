// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "appmenuregistrar.h"
#include "utils/xcbutils.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>

#include <algorithm>

namespace
{
const QString serviceName = QStringLiteral("com.canonical.AppMenu.Registrar");
const QString objectPath = QStringLiteral("/com/canonical/AppMenu/Registrar");
}

static QDBusArgument &operator<<(QDBusArgument &argument, const RegisteredMenu &menu)
{
    argument.beginStructure();
    argument << menu.xid << menu.service << menu.path;
    argument.endStructure();
    return argument;
}

static const QDBusArgument &operator>>(const QDBusArgument &argument, RegisteredMenu &menu)
{
    argument.beginStructure();
    argument >> menu.xid >> menu.service >> menu.path;
    argument.endStructure();
    return argument;
}

AppMenuRegistrar::AppMenuRegistrar(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<RegisteredMenu>();
    qDBusRegisterMetaType<RegisteredMenus>();
    m_exporters.setConnection(QDBusConnection::sessionBus());
    m_exporters.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(&m_exporters, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &owner) {
        for (auto it = m_menus.begin(); it != m_menus.end();) {
            if (it->service == owner) {
                const uint xid = it.key();
                it = m_menus.erase(it);
                Q_EMIT WindowUnregistered(xid);
            } else {
                ++it;
            }
        }
        m_exporters.removeWatchedService(owner);
    });
}

AppMenuRegistrar::~AppMenuRegistrar()
{
    setEnabled(false);
}

void AppMenuRegistrar::setEnabled(bool enabled)
{
    if (enabled == m_ownsName) {
        return;
    }
    auto bus = QDBusConnection::sessionBus();
    if (enabled) {
        if (!bus.registerObject(objectPath, this, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
            return;
        }
        // Do not replace an existing desktop registrar or queue behind it.
        m_ownsName = bus.registerService(serviceName);
        if (!m_ownsName) {
            bus.unregisterObject(objectPath);
        }
    } else {
        bus.unregisterService(serviceName);
        bus.unregisterObject(objectPath);
        m_ownsName = false;
        m_exporters.setWatchedServices({});
        m_menus.clear();
    }
}

void AppMenuRegistrar::RegisterWindow(uint xid, const QDBusObjectPath &path)
{
    const QString owner = message().service();
    const uint pid = KWin::Xcb::getWindowPidByXRes(xid);
    if (!pid || connection().interface()->servicePid(owner).value() != pid) {
        sendErrorReply(QDBusError::AccessDenied, QStringLiteral("Window does not belong to the caller"));
        return;
    }
    const auto existing = m_menus.constFind(xid);
    if (existing != m_menus.cend()) {
        if (existing->service == owner && existing->path == path) {
            return;
        }
        removeWindow(xid);
    }
    m_menus.insert(xid, {xid, owner, path});
    m_exporters.addWatchedService(owner);
    Q_EMIT WindowRegistered(xid, owner, path);
}

void AppMenuRegistrar::UnregisterWindow(uint xid)
{
    const auto it = m_menus.constFind(xid);
    if (it != m_menus.cend() && it->service == message().service()) {
        removeWindow(xid);
    }
}

void AppMenuRegistrar::removeWindow(uint xid)
{
    auto it = m_menus.find(xid);
    if (it == m_menus.end()) {
        return;
    }
    const QString owner = it->service;
    m_menus.erase(it);
    if (std::none_of(m_menus.cbegin(), m_menus.cend(), [&owner](const RegisteredMenu &menu) {
            return menu.service == owner;
        })) {
        m_exporters.removeWatchedService(owner);
    }
    Q_EMIT WindowUnregistered(xid);
}

QString AppMenuRegistrar::GetMenuForWindow(uint xid, QDBusObjectPath &path) const
{
    const auto it = m_menus.constFind(xid);
    if (it == m_menus.cend()) {
        path = QDBusObjectPath(QStringLiteral("/"));
        return {};
    }
    path = it->path;
    return it->service;
}

RegisteredMenus AppMenuRegistrar::GetMenus() const
{
    return m_menus.values();
}
