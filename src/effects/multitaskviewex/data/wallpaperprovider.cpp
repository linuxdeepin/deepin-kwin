/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wallpaperprovider.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>

namespace KWin
{

static const char *s_directory = "/usr/share/wallpapers/deepin";

QString randomWallpaperPath()
{
    QStringList backgrounds;
    QDir dir(QString::fromLatin1(s_directory));
    if (dir.exists()) {
        const QStringList filters = {
            QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
            QStringLiteral("*.png"), QStringLiteral("*.bmp"),
            QStringLiteral("*.webp")};
        const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
        for (const QFileInfo &fi : files) {
            backgrounds.append(fi.absoluteFilePath());
        }
    }
    if (backgrounds.isEmpty()) {
        return QString();
    }
    const int idx = QRandomGenerator::global()->bounded(backgrounds.size());
    return backgrounds.at(idx);
}

WallpaperSource::WallpaperSource(QObject *parent)
    : QObject(parent)
    , m_appearanceWatcher(new QDBusServiceWatcher(QString::fromLatin1(WallpaperDBus::appearanceService),
                                                  QDBusConnection::sessionBus(),
                                                  QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
                                                  this))
    , m_refreshDebounce(new QTimer(this))
{
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(300);
    connect(m_refreshDebounce, &QTimer::timeout, this, [this]() {
        refresh(m_desktops, m_monitors);
    });
    connect(m_appearanceWatcher, &QDBusServiceWatcher::serviceRegistered, this, [this]() {
        setAppearanceReady(true);
    });
    connect(m_appearanceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this]() {
        setAppearanceReady(false);
    });
    detectInitialState();
}

bool WallpaperSource::ready() const
{
    return m_appearanceReady;
}

QString WallpaperSource::clearUri(int desktopNum, const QString &monitor) const
{
    const auto it = m_cachedClear.constFind(desktopNum);
    if (it == m_cachedClear.constEnd()) {
        return QString();
    }
    return it.value().value(monitor);
}

void WallpaperSource::refresh(const QVector<int> &desktops, const QStringList &monitors)
{
    m_desktops = desktops;
    m_monitors = monitors;
    m_refreshRequested = true;
    m_refreshDebounce->stop();
    if (!m_appearanceReady) {
        return;
    }
    const int generation = ++m_generation;
    m_nextClear.clear();
    const int total = int(desktops.size()) * int(monitors.size());
    m_pendingClear = total;
    if (total == 0) {
        commitRefresh(generation);
        return;
    }
    for (int num : desktops) {
        for (const QString &monitor : monitors) {
            QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(WallpaperDBus::appearanceService),
                                                              QLatin1String(WallpaperDBus::appearanceObject),
                                                              QLatin1String(WallpaperDBus::appearanceInterface),
                                                              QStringLiteral("GetWorkspaceBackgroundForMonitor"));
            msg << num << monitor;
            auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
            connect(watcher, &QDBusPendingCallWatcher::finished, this,
                    [this, generation, num, monitor](QDBusPendingCallWatcher *w) {
                        w->deleteLater();
                        if (generation != m_generation) {
                            return;
                        }
                        const QDBusReply<QString> reply = *w;
                        m_nextClear[num][monitor] = workspaceBackgroundOrFallback(reply.value());
                        if (--m_pendingClear == 0) {
                            commitRefresh(generation);
                        }
                    });
        }
    }
}

void WallpaperSource::setWorkspaceBackground(int desktopNum, const QString &monitor, const QString &uri)
{
    m_cachedClear[desktopNum][monitor] = uri;
    if (!m_appearanceReady) {
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(WallpaperDBus::appearanceService),
                                                      QLatin1String(WallpaperDBus::appearanceObject),
                                                      QLatin1String(WallpaperDBus::appearanceInterface),
                                                      QStringLiteral("SetWorkspaceBackgroundForMonitor"));
    msg << desktopNum << monitor << uri;
    QDBusConnection::sessionBus().asyncCall(msg);
}

void WallpaperSource::detectInitialState()
{
    if (auto *iface = QDBusConnection::sessionBus().interface()) {
        const QDBusReply<bool> reply =
            iface->isServiceRegistered(QString::fromLatin1(WallpaperDBus::appearanceService));
        m_appearanceReady = reply.isValid() && reply.value();
    }
}

void WallpaperSource::scheduleRefresh()
{
    if (!m_refreshRequested || !m_appearanceReady) {
        return;
    }
    m_refreshDebounce->start();
}

void WallpaperSource::setAppearanceReady(bool ready)
{
    if (m_appearanceReady == ready) {
        return;
    }
    m_appearanceReady = ready;
    if (ready) {
        scheduleRefresh();
    } else {
        ++m_generation;
        m_refreshDebounce->stop();
    }
    Q_EMIT serviceReadyChanged(ready);
}

void WallpaperSource::commitRefresh(int generation)
{
    if (generation != m_generation) {
        return;
    }
    m_cachedClear = m_nextClear;
    Q_EMIT changed();
}

}
