/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QRect>
#include <QSize>
#include <QVector>

namespace KWin
{

/**
 * One display, described the way the mirror mode layout cares about it.
 */
struct MirrorModeDisplay
{
    /// @c true while the display is scanning out.
    bool enabled = false;
    /// @c true for the display the X server uses as the anchor of the layout.
    bool primary = false;
    /// Area the display occupies, in scanout coordinates.
    QRect geometry;
    /// Size clients are told the screen has when this display is the anchor.
    QSize scanoutSize;
};

/**
 * The layout model of the mirror mode, shared with the X server.
 *
 * While the mode is on, clients only ever see the anchor display, anchored at the
 * origin, while the X server keeps scanning the real layout. Both views agree only
 * when the real anchor sits at the origin, which is what normalize() produces and
 * what the rest of this namespace assumes.
 *
 * The functions are pure: they describe what the layout has to look like instead of
 * touching the platform, so the transaction that pushes the result into RandR and the
 * tests that pin the arithmetic share them. The X server side of the contract lives in
 * the privacy view of xorg-server, which reports exactly one screen, anchored at the
 * origin, and only ever clips the areas struts reserved.
 */
namespace MirrorModeLayout
{

/**
 * Index of the display that anchors the layout, or -1 while none of them is enabled.
 *
 * A marked primary wins; otherwise the first enabled display is used, which is the
 * same fallback the X server takes.
 */
KWIN_EXPORT int anchorIndex(const QVector<MirrorModeDisplay> &displays);

/**
 * The screen clients are shown for a display scanning out at @p scanoutSize.
 */
KWIN_EXPORT QRect clientGeometry(const QSize &scanoutSize);

/**
 * Returns one geometry per display, in the order the displays were given: the anchor
 * moved to the origin, the other scanning displays packed left to right in the order
 * they are laid out right now, and the disabled ones left where they are.
 *
 * Packing left to right keeps the new layout free of overlaps, which RandR rejects,
 * and keeps the choice independent of what the user meant by their layout - the
 * snapshot restore brings that back.
 */
KWIN_EXPORT QVector<QRect> normalize(const QVector<MirrorModeDisplay> &displays);

/**
 * Clips an area a strut reserved to the screen clients are shown.
 *
 * Returns an empty rectangle when the strut lives outside of that screen, which is
 * what happens while the real layout is still the one clients are not shown.
 */
KWIN_EXPORT QRect clampToClientGeometry(const QRect &area, const QRect &clientGeometry);

} // namespace MirrorModeLayout
} // namespace KWin
