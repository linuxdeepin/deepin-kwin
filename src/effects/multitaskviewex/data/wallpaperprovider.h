/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "model/wallpaperdata.h"

#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

class QDBusServiceWatcher;
class QTimer;

namespace KWin
{

/**
 * @brief Normalizes a wallpaper URI into a real filesystem path.
 * @param uri Wallpaper URI, possibly percent-encoded.
 * @return The decoded path with symlinks resolved.
 */
inline QString toRealPath(const QString &uri)
{
    QString path = QUrl::fromPercentEncoding(uri.toUtf8());
    if (path.startsWith(QStringLiteral("file:///"))) {
        path.remove(QStringLiteral("file://"));
    }
    const QFileInfo info(path);
    if (info.isSymLink()) {
        path = info.symLinkTarget();
    }
    return path;
}

/**
 * @brief Returns a random wallpaper path from the system wallpaper pool.
 * @return A wallpaper file path, or an empty string when none is available.
 */
QString randomWallpaperPath();

/**
 * @brief Cached, non-blocking source of the wallpapers exposed by the appearance service.
 *
 * Watches service registration, requests the workspace wallpapers asynchronously and
 * always serves the last fully received snapshot from memory.
 */
class WallpaperSource : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the source and starts watching the wallpaper services.
     * @param parent Parent object.
     */
    explicit WallpaperSource(QObject *parent = nullptr);

    /**
     * @brief Tests whether the appearance service is currently registered.
     * @return True when the appearance service is registered.
     */
    bool ready() const;

    /**
     * @brief Returns the cached clear wallpaper URI for a workspace and monitor.
     * @param desktopNum X11 workspace number.
     * @param monitor Monitor name.
     * @return The cached URI, or an empty string when the cache has no entry.
     */
    QString clearUri(int desktopNum, const QString &monitor) const;

    /**
     * @brief Starts an asynchronous refresh of the wallpaper cache.
     * @param desktops X11 workspace numbers to request.
     * @param monitors Monitor names to request.
     */
    void refresh(const QVector<int> &desktops, const QStringList &monitors);

    /**
     * @brief Stores the wallpaper URI for a workspace and monitor (fire-and-forget).
     * @param desktopNum X11 workspace number.
     * @param monitor Monitor name.
     * @param uri Wallpaper URI to store.
     */
    void setWorkspaceBackground(int desktopNum, const QString &monitor, const QString &uri);

Q_SIGNALS:
    /**
     * @brief Emitted when a refresh has completed and the caches were replaced.
     */
    void changed();

    /**
     * @brief Emitted when the registration state of the appearance service changes.
     * @param ready True when the appearance service is registered.
     */
    void serviceReadyChanged(bool ready);

private:
    void detectInitialState();
    void scheduleRefresh();
    void setAppearanceReady(bool ready);
    void commitRefresh(int generation);

    QDBusServiceWatcher *m_appearanceWatcher = nullptr;
    QTimer *m_refreshDebounce = nullptr;
    bool m_appearanceReady = false;
    bool m_refreshRequested = false;
    int m_generation = 0;
    int m_pendingClear = 0;
    QVector<int> m_desktops;
    QStringList m_monitors;
    WallpaperMap m_cachedClear;
    WallpaperMap m_nextClear;
};

}
