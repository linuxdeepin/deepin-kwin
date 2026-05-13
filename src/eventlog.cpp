// Copyright (C) 2026 Uniontech Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "eventlog.h"

#include "config-kwin.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include <unistd.h>

namespace KWin
{

static constexpr const char *EVENTLOG_SERVICE = "org.deepin.EventLog1";
static constexpr const char *EVENTLOG_PATH = "/org/deepin/EventLog1";
static constexpr const char *EVENTLOG_INTERFACE = "org.deepin.EventLog1";
static constexpr const char *EVENTLOG_PACKAGE = "kwin";

void EventLog::reportMultitaskView()
{
    QJsonObject info;
    info["tid"] = 1000300000;
    info["launch_type"] = 1;
    info["kwin_version"] = QString::fromLatin1(KWIN_EVENTLOG_VERSION);
    info["target"] = QStringLiteral("kwin_multitask_view");

    sendEventLog(info);
}

void EventLog::reportSplitScreen(int launchType, Window *window)
{
    Q_UNUSED(window)
    QJsonObject info;
    info["tid"] = 1000300004;
    info["launch_type"] = launchType;
    info["kwin_version"] = QString::fromLatin1(KWIN_EVENTLOG_VERSION);
    info["target"] = QStringLiteral("kwin_split_screen");

    sendEventLog(info);
}

void EventLog::sendEventLog(const QJsonObject &info)
{
    QJsonObject payload;
    QJsonObject message;
    QString target;
    const int tid = info[QStringLiteral("tid")].toInt();

    for (auto it = info.begin(); it != info.end(); ++it) {
        if (it.key() == QLatin1String("tid")) {
            continue;
        }
        if (it.key() == QLatin1String("target")) {
            target = it.value().toString();
            continue;
        }
        message.insert(it.key(), it.value());
    }

    payload["tid"] = tid;
    payload["message"] = message;
    if (!target.isEmpty()) {
        payload["target"] = target;
    }

    QJsonObject eventData;
    eventData["info"] = payload;
    eventData["package"] = QString::fromLatin1(EVENTLOG_PACKAGE);
    eventData["pid"] = static_cast<int>(getpid());
    eventData["sys_uid"] = static_cast<int>(getuid());
    eventData["time"] = QDateTime::currentMSecsSinceEpoch();

    const QString json = QString::fromUtf8(QJsonDocument(eventData).toJson(QJsonDocument::Compact));

    QDBusInterface iface(QString::fromLatin1(EVENTLOG_SERVICE),
                         QString::fromLatin1(EVENTLOG_PATH),
                         QString::fromLatin1(EVENTLOG_INTERFACE),
                         QDBusConnection::systemBus());
    iface.setTimeout(500);
    iface.asyncCall(QStringLiteral("ReportEventLog"), QString::fromLatin1(EVENTLOG_PACKAGE), json);
}

} // namespace KWin
