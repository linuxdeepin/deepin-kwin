/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "x11window.h"

namespace KWin
{

/**
 * The XwaylandWindow class represents a managed Xwayland window.
 */
class XwaylandWindow : public X11Window
{
    Q_OBJECT

public:
    explicit XwaylandWindow();

    bool wantsSyncCounter() const override;

    void recordShape(xcb_window_t id, xcb_shape_kind_t kind) override;

    bool hitTest(const QPointF &point) const override;

private:
    void associate();
    void initialize();

    QVector<QRectF> m_shapeInputRegion;
    QVector<QRectF> m_shapeBoundingRegion;
};

} // namespace KWin
