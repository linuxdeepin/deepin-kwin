/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "effectconfig.h"
#include "helpers.h"

#include "utils/dconfig_reader.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QLoggingCategory>

#include <algorithm>

namespace KWin
{

EffectConfig::EffectConfig(QObject *parent)
    : QObject(parent)
{
}

void EffectConfig::init()
{
    QDBusInterface interfaceRequire(CONFIGMANAGER_SERVICE, "/", CONFIGMANAGER_INTERFACE, QDBusConnection::systemBus());
    interfaceRequire.setTimeout(DBUS_TIMEOUT);
    QDBusReply<QDBusObjectPath> reply = interfaceRequire.call("acquireManager", "org.kde.kwin", "org.kde.kwin.effects.multitaskviewex", "");
    if (!reply.isValid()) {
        qCWarning(KWIN_MTVEX) << "Failed to acquire DConfig manager for multitaskviewex:" << reply.error();
        return;
    }
    m_path = reply.value();
    QDBusConnection::systemBus().connect(CONFIGMANAGER_SERVICE, m_path.path(),
                                         CONFIGMANAGER_MANAGER_INTERFACE, "valueChanged", this, SLOT(onDConfigChanged(QString)));
}

void EffectConfig::reload()
{
    m_useBlurEffect = read<bool>(QStringLiteral("useBlurEffect"), false);
    m_maxWorkspaces = std::clamp(read<int>(QStringLiteral("maxWorkspaces"), kDefaultMaxWorkspaces), 1, 64);
    m_motion.workspaceMoveDurationMs = std::clamp(read<int>(QStringLiteral("workspaceMoveDurationMs"), kDefaultWorkspaceMoveDurationMs), kMinAnimationDurationMs, 5000);
    m_motion.windowChangeDurationMs = std::clamp(read<int>(QStringLiteral("windowChangeDurationMs"), kDefaultWindowChangeDurationMs), kMinAnimationDurationMs, 5000);
    m_motion.viewEnterDurationMs = std::clamp(read<int>(QStringLiteral("viewEnterDurationMs"), kDefaultViewEnterDurationMs), kMinAnimationDurationMs, 5000);
    m_motion.animationSpeedMultiplier = clampAnimationSpeedMultiplier(read<double>(QStringLiteral("animationSpeedMultiplier"), kDefaultAnimationSpeedMultiplier));
    Q_EMIT changed();
}

void EffectConfig::onDConfigChanged(const QString &key)
{
    qCDebug(KWIN_MTVEX) << "DConfig changed:" << key;
    reload();
}

template<typename T>
T EffectConfig::read(const QString &key, const T &defaultValue)
{
    T value = defaultValue;
    const DconfigReaderRetCode ret = DconfigRead<T>(QStringLiteral("org.kde.kwin.effects.multitaskviewex"), key, value);
    if (ret != DconfigReaderRetCode::DCONF_SUCCESS) {
        qCDebug(KWIN_MTVEX) << "DConfig read failed for" << key << "ret=" << static_cast<int>(ret);
    }
    return value;
}

template int EffectConfig::read<int>(const QString &, const int &);
template double EffectConfig::read<double>(const QString &, const double &);
template bool EffectConfig::read<bool>(const QString &, const bool &);

}
