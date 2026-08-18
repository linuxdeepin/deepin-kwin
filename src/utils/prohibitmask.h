/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

#include <QImage>
#include <QRegion>

class QPainter;

namespace KWin
{

/**
 * Per-window prohibit mask region calculation result.
 * Coordinates are in local physical pixels relative to the viewport origin.
 */
struct KWIN_EXPORT ProtectedRegions {
    bool maskAll = false;
    QRegion rects;
};

/**
 * Compute the protected window regions that need to be masked.
 *
 * @param viewport  The capture area in global logical coordinates.
 * @param scale     Output scale factor (logical to physical pixels).
 * @return ProtectedRegions with maskAll flag and per-window rects.
 */
KWIN_EXPORT ProtectedRegions computeProtectedRegions(const QRect &viewport, qreal scale);

/**
 * Apply grey fill + lock icon overlay on the current framebuffer using OpenGL.
 * The caller must have the target FBO bound.
 *
 * @param regions  Pre-computed protected regions from computeProtectedRegions().
 * @param fboSize  Size of the framebuffer in physical pixels.
 * @param flipped  If true, the image in the FBO is upside-down (screen top at GL bottom).
 *                 glScissor Y is flipped accordingly.
 */
KWIN_EXPORT void applyProhibitMaskGL(const ProtectedRegions &regions, const QSize &fboSize, bool flipped = false, bool drawIcon = true);

/**
 * Apply grey fill + lock icon overlay on a QImage using QPainter.
 */
KWIN_EXPORT void applyProhibitMaskCPU(QImage &image, const ProtectedRegions &regions);

/**
 * Call displayjack to enable/disable global protection of the root window via DBus.
 * @param protect  true to enable global protection, false to release it.
 */
KWIN_EXPORT void dbusRootWindowProtect(bool protect);

/**
 * Whether global protection currently open was opened by KWin itself.
 * Used when releasing global protection: only close it if there are no protected windows
 * anymore, or if the global protection was opened by KWin (so KWin is responsible for closing it).
 */
KWIN_EXPORT bool protectionOpenedByKWin();

} // namespace KWin
