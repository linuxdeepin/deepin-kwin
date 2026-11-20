/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "wallpaperdata.h"

#include <QString>
#include <QVector>

namespace KWin
{

/**
 * @brief Per-screen input of a workspace-switch slide.
 */
struct SlideScreenInput
{
    QString monitor; ///< Monitor name of the screen, as reported by the appearance service.
    qreal width; ///< Layout width of the screen, that is the distance its content slides.
};

/**
 * @brief Per-screen data of a workspace-switch slide.
 */
struct ScreenSlide
{
    QString fromWallpaper; ///< Wallpaper the screen shows for the workspace being left.
    QString toWallpaper; ///< Wallpaper the screen shows for the workspace being entered.
    qreal outgoingOffset; ///< Signed horizontal offset of the screen's outgoing grid.
};

/**
 * @brief Composes the workspace-switch slide of every screen.
 * @param screens Per-screen inputs, in the order the screens are painted.
 * @param direction 1 when the target workspace is to the right, -1 otherwise.
 * @param wallpapers Cached wallpaper URIs, indexed by workspace number and monitor name.
 * @param fromDesktop Workspace number being left.
 * @param toDesktop Workspace number being entered.
 * @param fallbackUri Wallpaper used for a screen the cache has no wallpaper for.
 * @return One slide per input screen, in the same order.
 */
inline QVector<ScreenSlide> composeSlide(const QVector<SlideScreenInput> &screens,
                                         int direction,
                                         const WallpaperMap &wallpapers,
                                         int fromDesktop,
                                         int toDesktop,
                                         const QString &fallbackUri)
{
    QVector<ScreenSlide> slides;
    slides.reserve(screens.size());
    for (const SlideScreenInput &screen : screens) {
        const QString fromCached = wallpaperForMonitor(wallpapers, fromDesktop, screen.monitor);
        const QString toCached = wallpaperForMonitor(wallpapers, toDesktop, screen.monitor);
        ScreenSlide slide;
        slide.fromWallpaper = fromCached.isEmpty() ? fallbackUri : fromCached;
        slide.toWallpaper = toCached.isEmpty() ? fallbackUri : toCached;
        slide.outgoingOffset = direction * screen.width;
        slides.append(slide);
    }
    return slides;
}

} // namespace KWin
