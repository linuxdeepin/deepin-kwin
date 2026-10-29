/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace KWin
{

class EffectScreen;
class WallpaperSource;

/**
 * @brief Resolves the wallpaper shown for a workspace and monitor.
 *
 * Owns the appearance-service cache and the debounced refresh policy. The
 * manager only reports wallpapers; applying them to a screen view stays in the
 * effect.
 */
class WallpaperManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the manager and subscribes to the appearance services.
     * @param parent Parent object.
     */
    explicit WallpaperManager(QObject *parent = nullptr);

    /**
     * @brief Destroys the manager.
     */
    ~WallpaperManager() override;

    /**
     * @brief Requests an asynchronous refresh of the wallpaper cache.
     */
    void warm();

    /**
     * @brief Returns the raw cached clear wallpaper URI for a workspace and monitor.
     * @param desktopNum Desktop number.
     * @param monitor Monitor to query, or nullptr for the first monitor with a wallpaper.
     * @return The cached URI, or an empty string when no wallpaper is cached.
     */
    QString clearUri(int desktopNum, EffectScreen *monitor = nullptr) const;

    /**
     * @brief Returns the clear wallpaper path for a workspace and monitor.
     * @param desktopNum Desktop number.
     * @param monitor Monitor to query, or nullptr for the first monitor with a wallpaper.
     * @return The wallpaper path, falling back to the built-in wallpaper.
     */
    QString clearPath(int desktopNum, EffectScreen *monitor = nullptr) const;

    /**
     * @brief Returns the full-screen background for a workspace and monitor.
     * @param desktopNum Desktop number.
     * @param monitor Monitor to query, or nullptr for the first monitor with a wallpaper.
     * @return The clear wallpaper path.
     */
    QString backgroundFor(int desktopNum, EffectScreen *monitor = nullptr) const;

    /**
     * @brief Collects the background of every workspace for preloading.
     * @param excludeDesktop Desktop number to skip, or a non-positive value to include all.
     * @return Background paths, without duplicates.
     */
    QStringList allUris(int excludeDesktop = -1) const;

    /**
     * @brief Stores the wallpaper of a workspace on a monitor.
     * @param desktopNum Desktop number.
     * @param monitor Monitor name.
     * @param uri Wallpaper URI to store.
     */
    void setWorkspaceBackground(int desktopNum, const QString &monitor, const QString &uri);

    /**
     * @brief Migrates wallpaper bindings after a workspace was removed.
     * @param removedNum Desktop number that was removed.
     * @param desktopCount Workspace count before removal.
     */
    void shiftOnRemove(int removedNum, int desktopCount);

Q_SIGNALS:
    /**
     * @brief Emitted when the wallpaper cache was replaced by a refresh.
     */
    void refreshed();

private Q_SLOTS:
    void onConfigChanged();
    void onAppearanceChanged(const QString &ty, const QString &value);
    void onAppearanceRefreshed(const QString &type);
    void onWorkspaceBackgroundChanged(int index, const QString &newUri);
    void onWorkspaceBackgroundChangedForMonitor(int index, const QString &monitor, const QString &uri);
    void onSourceChanged();

private:
    QString wallpaperFor(int desktopNum, EffectScreen *monitor) const;
    std::unique_ptr<WallpaperSource> m_source;
    QTimer m_debounce;
};

}
