/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "mirrormodelayout.h"

#include <QRect>
#include <QStringList>
#include <QVector>

#include <xcb/randr.h>
#include <xcb/xcb.h>

namespace KWin
{

/**
 * Applies the mirror mode layout to the X server and puts it back.
 *
 * While the mode is on, the X server keeps scanning the real layout while clients are
 * shown the anchor display alone, anchored at the origin. The two views only agree
 * while the real anchor sits at the origin, which is what apply() produces: the anchor
 * is moved there and the other outputs are packed left to right.
 *
 * The layout that was replaced is written to the configuration, so a run that ends
 * while the mode is on can be undone on the next start.
 */
class X11MirrorModeLayout
{
public:
    explicit X11MirrorModeLayout(xcb_connection_t *connection);

    /**
     * Moves the anchor output to the origin and packs the other outputs left to right.
     *
     * Returns @c false when the layout cannot be read or written, in which case nothing
     * was changed and the caller must keep the mode off.
     */
    bool apply();

    /**
     * Puts the layout apply() replaced back and forgets it.
     */
    void restore();


    /**
     * Puts the layout an earlier run saved back, without applying the mode again.
     */
    static void restoreSnapshot(xcb_connection_t *connection);

private:
    struct CrtcState
    {
        xcb_randr_crtc_t crtc = XCB_NONE;
        xcb_randr_mode_t mode = XCB_NONE;
        uint16_t rotation = XCB_RANDR_ROTATION_ROTATE_0;
        QRect geometry;
        QVector<xcb_randr_output_t> outputs;
    };

    QVector<CrtcState> readCrtcs() const;
    bool writeCrtcs(const QVector<CrtcState> &crtcs) const;

    static QStringList serialize(const QVector<CrtcState> &crtcs);
    static QVector<CrtcState> deserialize(const QStringList &entries);

    xcb_connection_t *m_connection = nullptr;
    QVector<CrtcState> m_snapshot;
};

} // namespace KWin
