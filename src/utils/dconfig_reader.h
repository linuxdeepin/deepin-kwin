// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusError>
#include <QDebug>
#include <QDBusObjectPath>
#include <type_traits>
namespace KWin
{

#define CONFIGMANAGER_SERVICE   "org.desktopspec.ConfigManager"
#define CONFIGMANAGER_INTERFACE "org.desktopspec.ConfigManager"
#define CONFIGMANAGER_MANAGER_INTERFACE "org.desktopspec.ConfigManager.Manager"

#define DBUS_TIMEOUT 100

enum class DconfigReaderRetCode {
    DCONF_SUCCESS = 0,
    DCONF_NOT_FOUNT,
    DCONF_NOT_REPLY,
    DCONF_TYPE_NOT_SUPPORT,
};

template <typename T>
inline DconfigReaderRetCode DconfigRead(const QString& dconfItem, const QString& dconfKey, T& dconfValue)
{
    QDBusInterface interfaceRequire(CONFIGMANAGER_SERVICE, "/", CONFIGMANAGER_INTERFACE, QDBusConnection::systemBus());
    interfaceRequire.setTimeout(DBUS_TIMEOUT);
    QDBusReply<QDBusObjectPath> reply = interfaceRequire.call("acquireManager", "org.kde.kwin", dconfItem, "");
    if (reply.isValid()) {
        QString path = reply.value().path();
        QDBusInterface interfaceValue(CONFIGMANAGER_SERVICE, path, CONFIGMANAGER_MANAGER_INTERFACE, QDBusConnection::systemBus());
        interfaceValue.setTimeout(DBUS_TIMEOUT);
        QDBusReply<QVariant> replyValue = interfaceValue.call("value", dconfKey);
        if constexpr (std::is_same_v<T, bool>) {
            dconfValue = replyValue.value().toBool();
        } else if constexpr (std::is_same_v<T, int>) {
            dconfValue = replyValue.value().toInt();
        } else if constexpr (std::is_same_v<T, double>) {
            dconfValue = replyValue.value().toDouble();
        } else if constexpr (std::is_same_v<T, QString>) {
            dconfValue = replyValue.value().toString();
        } else {
            return DconfigReaderRetCode::DCONF_TYPE_NOT_SUPPORT;
        }
        return DconfigReaderRetCode::DCONF_SUCCESS;
    } else {
        qWarning() << "Error in DConfig reply:" << reply.error();
        return DconfigReaderRetCode::DCONF_NOT_REPLY;
    }
    return DconfigReaderRetCode::DCONF_NOT_FOUNT;
}

} // namespace KWin
