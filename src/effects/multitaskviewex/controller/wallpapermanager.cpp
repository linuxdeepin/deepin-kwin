/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wallpapermanager.h"
#include "data/wallpaperprovider.h"
#include "data/workspacesync.h"
#include "helpers.h"
#include "model/wallpaperdata.h"

#include "effects.h"
#include "virtualdesktops.h"

#include <QDBusConnection>
#include <QDebug>
#include <QLoggingCategory>

namespace KWin
{

WallpaperManager::WallpaperManager(QObject *parent)
    : QObject(parent)
    , m_source(std::make_unique<WallpaperSource>())
{
    connect(m_source.get(), &WallpaperSource::changed, this, &WallpaperManager::onSourceChanged);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &WallpaperManager::warm);

    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.deepin.dde.Appearance1"),
        QStringLiteral("/org/deepin/dde/Appearance1"),
        QStringLiteral("org.deepin.dde.Appearance1"),
        QStringLiteral("Changed"), this,
        SLOT(onAppearanceChanged(QString, QString)));
    QDBusConnection::sessionBus().connect(
        QStringLiteral("org.deepin.dde.Appearance1"),
        QStringLiteral("/org/deepin/dde/Appearance1"),
        QStringLiteral("org.deepin.dde.Appearance1"),
        QStringLiteral("Refreshed"), this,
        SLOT(onAppearanceRefreshed(QString)));
    QDBusConnection::sessionBus().connect(
        QStringLiteral("com.deepin.wm"),
        QStringLiteral("/com/deepin/wm"),
        QStringLiteral("com.deepin.wm"),
        QStringLiteral("WorkspaceBackgroundChanged"), this,
        SLOT(onWorkspaceBackgroundChanged(int, QString)));
    QDBusConnection::sessionBus().connect(
        QStringLiteral("com.deepin.wm"),
        QStringLiteral("/com/deepin/wm"),
        QStringLiteral("com.deepin.wm"),
        QStringLiteral("WorkspaceBackgroundChangedForMonitor"), this,
        SLOT(onWorkspaceBackgroundChangedForMonitor(int, QString, QString)));
}

WallpaperManager::~WallpaperManager() = default;

QString WallpaperManager::wallpaperFor(int desktopNum, EffectScreen *monitor) const
{
    if (!m_source) {
        return QString();
    }
    const QString monName = monitor ? monitor->name() : QString();
    if (monName.isEmpty()) {
        for (EffectScreen *s : effects->screens()) {
            const QString uri = wallpaperFor(desktopNum, s);
            if (!uri.isEmpty()) {
                return uri;
            }
        }
        return QString();
    }
    return m_source->clearUri(desktopNum, monName);
}

QString WallpaperManager::clearUri(int desktopNum, EffectScreen *monitor) const
{
    return wallpaperFor(desktopNum, monitor);
}

QString WallpaperManager::clearPath(int desktopNum, EffectScreen *monitor) const
{
    const QString uri = wallpaperFor(desktopNum, monitor);
    return toRealPath(uri.isEmpty() ? QLatin1String(fallback_background_name) : uri);
}

QString WallpaperManager::backgroundFor(int desktopNum, EffectScreen *monitor) const
{
    const QString clear = wallpaperFor(desktopNum, monitor);
    if (clear.isEmpty()) {
        return toRealPath(QLatin1String(fallback_background_name));
    }
    return toRealPath(clear);
}

QStringList WallpaperManager::allUris(int excludeDesktop) const
{
    QStringList out;
    auto *vds = VirtualDesktopManager::self();
    if (!vds || !m_source) {
        return out;
    }
    for (const VirtualDesktop *d : vds->desktops()) {
        const int num = int(d->x11DesktopNumber());
        if (num == excludeDesktop) {
            continue;
        }
        for (EffectScreen *s : effects->screens()) {
            const QString uri = backgroundFor(num, s);
            if (!uri.isEmpty() && !out.contains(uri)) {
                out.append(uri);
            }
        }
    }
    return out;
}

void WallpaperManager::setWorkspaceBackground(int desktopNum, const QString &monitor, const QString &uri)
{
    if (m_source) {
        m_source->setWorkspaceBackground(desktopNum, monitor, uri);
    }
}

void WallpaperManager::onConfigChanged()
{
    qCInfo(KWIN_MTVEX) << "appearance changed signal -> debounce timer started";
    m_debounce.start();
}

void WallpaperManager::onAppearanceChanged(const QString &ty, const QString &value)
{
    Q_UNUSED(ty);
    Q_UNUSED(value);
    onConfigChanged();
}

void WallpaperManager::onAppearanceRefreshed(const QString &type)
{
    Q_UNUSED(type);
    onConfigChanged();
}

void WallpaperManager::onWorkspaceBackgroundChanged(int index, const QString &newUri)
{
    Q_UNUSED(index);
    Q_UNUSED(newUri);
    onConfigChanged();
}

void WallpaperManager::onWorkspaceBackgroundChangedForMonitor(int index, const QString &monitor, const QString &uri)
{
    Q_UNUSED(index);
    Q_UNUSED(monitor);
    Q_UNUSED(uri);
    onConfigChanged();
}

void WallpaperManager::onSourceChanged()
{
    Q_EMIT refreshed();
}

void WallpaperManager::warm()
{
    auto *vds = VirtualDesktopManager::self();
    if (!vds || !m_source) {
        return;
    }
    QVector<int> desktops;
    const auto vdsDesktops = vds->desktops();
    desktops.reserve(int(vdsDesktops.size()));
    for (const VirtualDesktop *d : vdsDesktops) {
        desktops.append(int(d->x11DesktopNumber()));
    }
    QStringList monitors;
    for (EffectScreen *s : effects->screens()) {
        monitors.append(s->name());
    }
    m_source->refresh(desktops, monitors);
}

void WallpaperManager::shiftOnRemove(int removedNum, int desktopCount)
{
    const auto moves = computeWallpaperShiftMoves(removedNum, desktopCount);
    if (moves.isEmpty() || !m_source) {
        return;
    }
    const auto cachedClearFor = [this](int num, const QString &mon) {
        QString uri = m_source->clearUri(num, mon);
        if (!uri.isEmpty() || !mon.isEmpty()) {
            return uri;
        }
        for (EffectScreen *other : effects->screens()) {
            uri = m_source->clearUri(num, other->name());
            if (!uri.isEmpty()) {
                break;
            }
        }
        return uri;
    };
    for (EffectScreen *s : effects->screens()) {
        const QString mon = s->name();
        for (const auto &mv : moves) {
            const QString uri = cachedClearFor(mv.first, mon);
            if (uri.isEmpty()) {
                continue;
            }
            m_source->setWorkspaceBackground(mv.second, mon, uri);
        }
    }
}

}
