/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QFileInfo>
#include <QString>
#include <QUrl>

namespace KWin
{

/**
 * @brief Normalizes an image URI into a real filesystem path.
 * @param uri Image URI, possibly percent-encoded.
 * @return The decoded path with symlinks resolved.
 *
 * Image identity is the file the bytes come from, and the fingerprint that names a
 * cached decode must therefore be built from this value: the wallpaper service
 * hands out percent-encoded URIs of symlinks, while the caller that paints asks
 * with whatever it was given. Both sides run through this one function so a cache
 * entry written by one is found by the other.
 *
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

}
