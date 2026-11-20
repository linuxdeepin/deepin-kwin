// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "appmenuregistrar.h"
#include "utils/filedescriptor.h"
#include "utils/xcbutils.h"

#include <QByteArray>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>

namespace
{
const QString serviceName = QStringLiteral("com.canonical.AppMenu.Registrar");
const QString objectPath = QStringLiteral("/com/canonical/AppMenu/Registrar");

bool isWindowBusProxy(pid_t windowPid, pid_t senderPid)
{
    const QByteArray senderExe = "/proc/" + QByteArray::number(senderPid) + "/exe";
    struct stat senderBinary;
    struct stat systemProxy;
    if (::stat(senderExe.constData(), &senderBinary) != 0 || ::stat("/usr/bin/xdg-dbus-proxy", &systemProxy) != 0
        || senderBinary.st_dev != systemProxy.st_dev || senderBinary.st_ino != systemProxy.st_ino) {
        return false;
    }

    const QByteArray windowRoot = "/proc/" + QByteArray::number(windowPid) + "/root";
    const KWin::FileDescriptor root(::open(windowRoot.constData(), O_PATH | O_DIRECTORY | O_CLOEXEC));
    struct stat flatpakInfo;
    if (!root.isValid() || ::fstatat(root.get(), ".flatpak-info", &flatpakInfo, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(flatpakInfo.st_mode)) {
        return false;
    }

    constexpr int endpointFlags = O_PATH | O_NOFOLLOW | O_CLOEXEC;
    const uid_t uid = ::getuid();
    KWin::FileDescriptor endpoint(::openat(root.get(), "run/flatpak/bus", endpointFlags));
    if (!endpoint.isValid() && errno == ENOENT) {
        const QByteArray legacyPath = "run/user/" + QByteArray::number(uid) + "/bus";
        endpoint = KWin::FileDescriptor(::openat(root.get(), legacyPath.constData(), endpointFlags));
    }
    struct stat socketInfo;
    struct statvfs mount;
    if (!endpoint.isValid() || ::fstat(endpoint.get(), &socketInfo) != 0 || !S_ISSOCK(socketInfo.st_mode)
        || ::fstatvfs(endpoint.get(), &mount) != 0 || !(mount.f_flag & ST_RDONLY)) {
        return false;
    }

    const KWin::FileDescriptor socket(::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    // Connect to the same socket inode inspected above.
    std::snprintf(address.sun_path, sizeof(address.sun_path), "/proc/self/fd/%d", endpoint.get());
    if (!socket.isValid() || ::connect(socket.get(), reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
        return false;
    }

    ucred peer{};
    socklen_t size = sizeof(peer);
    return ::getsockopt(socket.get(), SOL_SOCKET, SO_PEERCRED, &peer, &size) == 0
        && peer.pid == senderPid && peer.uid == uid;
}
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
    const pid_t windowPid = KWin::Xcb::getWindowPidByXRes(xid);
    const auto senderPid = static_cast<pid_t>(connection().interface()->servicePid(owner).value());
    if (windowPid <= 0 || senderPid <= 0 || (windowPid != senderPid && !isWindowBusProxy(windowPid, senderPid))) {
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
