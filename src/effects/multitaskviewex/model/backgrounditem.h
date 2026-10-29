/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace KWin
{

struct BackgroundItem
{
    QString wallpaper;
    /**
     * @brief Sets the wallpaper URI of the background.
     * @param uri Wallpaper URI.
     */
    void setWallpaper(const QString &uri)
    {
        wallpaper = uri;
    }
};

}
