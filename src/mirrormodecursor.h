/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QRegion>
#include <QTimer>

#include <xcb/xfixes.h>

#include <vector>

namespace KWin
{

class Workspace;

/**
 * Everything the mirror mode does to the pointer, kept out of the cursor
 * backend and the scene: confining the pointer to the primary output and tracking the
 * X cursor image the mirror outputs have to draw.
 *
 * Both are X11 only. The instance exists for the whole lifetime of the Workspace and is
 * idle while the mode is off.
 */
class MirrorModeCursor : public QObject
{
    Q_OBJECT

public:
    explicit MirrorModeCursor(Workspace *workspace);
    ~MirrorModeCursor() override;

    /**
     * Cursor image as currently displayed by the X server, or a null image when the
     * mode is off or the cursor is hidden.
     */
    QImage image() const;
    /**
     * Rectangle the cursor occupies on \a mirrorGeometry once mirrored, empty when the
     * mode is off or the cursor is hidden.
     */
    QRectF mirroredCursorRect(const QRectF &mirrorGeometry) const;

private Q_SLOTS:
    void applyMode(bool enabled);
    void poll();

private:
    void createBarriers();
    void destroyBarriers();
    // Whether XFixes 5.0 pointer barriers are available. The version cannot change while
    // the connection lives, so it is queried once and cached.
    bool supportsPointerBarriers();
    // Queries the X cursor image; returns true when the image or the hotspot changed.
    bool queryCursor();
    // Repaints the mirrored cursor at its new place; \a force also repaints when only the
    // image changed and the rect stayed the same.
    void updateMirrorRegion(bool force = false);

    Workspace *m_workspace;
    QTimer m_pollTimer;
    std::vector<xcb_xfixes_barrier_t> m_barriers;
    QImage m_image;
    QPoint m_hotspot;
    QRegion m_mirrorRegion;
    // -1 until queried, then 0 when barriers are unavailable and 1 when they are.
    int m_pointerBarriersAvailable = -1;
};

} // namespace KWin
