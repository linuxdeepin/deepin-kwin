/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QString>

namespace KWin
{

/**
 * @brief Compile-time deepin wallpaper DBus endpoints, identical to the legacy multitaskview effect.
 */
namespace WallpaperDBus
{
#ifdef BUILD_ON_V25
inline constexpr char appearanceService[] = "org.deepin.dde.Appearance1";
inline constexpr char appearanceObject[] = "/org/deepin/dde/Appearance1";
inline constexpr char appearanceInterface[] = "org.deepin.dde.Appearance1";
#else
inline constexpr char appearanceService[] = "com.deepin.daemon.Appearance";
inline constexpr char appearanceObject[] = "/com/deepin/daemon/Appearance";
inline constexpr char appearanceInterface[] = "com.deepin.daemon.Appearance";
#endif
}

/**
 * @brief Wallpaper URI used when the appearance service returns nothing.
 */
inline constexpr char fallback_background_name[] = "file:///usr/share/wallpapers/deepin/desktop.jpg";

/**
 * @brief Returns a wallpaper URI, substituting the built-in fallback when none is available.
 * @param uri URI returned by the appearance service, possibly empty.
 * @return The URI when non-empty, otherwise the built-in fallback wallpaper URI.
 */
inline QString workspaceBackgroundOrFallback(const QString &uri)
{
    return uri.isEmpty() ? QLatin1String(fallback_background_name) : uri;
}

using WallpaperMap = QHash<int, QHash<QString, QString>>;

/**
 * @brief Tests whether two wallpaper snapshots are identical.
 * @param before Earlier wallpaper map.
 * @param after Later wallpaper map.
 * @return True if both maps compare equal, false otherwise.
 */
inline bool wallpaperSnapshotUnchanged(const WallpaperMap &before,
                                       const WallpaperMap &after)
{
    return before == after;
}

/**
 * @brief Merges freshly fetched wallpaper URIs into the cache in place.
 * @param cache Cache to update; existing entries are overwritten by fetched ones.
 * @param fetched Wallpaper URIs to merge in.
 */
inline void mergeWallpaperCache(WallpaperMap &cache, const WallpaperMap &fetched)
{
    for (auto it = fetched.constBegin(); it != fetched.constEnd(); ++it) {
        for (auto m = it.value().constBegin(); m != it.value().constEnd(); ++m) {
            cache[it.key()][m.key()] = m.value();
        }
    }
}

/**
 * @brief Picks the wallpaper URI for a newly created workspace.
 * @param previewUri Wallpaper selected in the add-preview, if any.
 * @param currentFallbackUri Fallback URI used when no preview is available.
 * @return The preview URI when non-empty, otherwise the fallback URI.
 */
inline QString pickNewWorkspaceWallpaper(const QString &previewUri, const QString &currentFallbackUri)
{
    return previewUri.isEmpty() ? currentFallbackUri : previewUri;
}

}
