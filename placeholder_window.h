// Copyright (C) 2011 Zhang Haidong <zhanghaidong@uniontech.com>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_PLACEHOLDER_WINDDOW_H
#define KWIN_PLACEHOLDER_WINDDOW_H

#include "x11eventfilter.h"
#include "xcbutils.h"
#include <xcb/xcb.h>

namespace KWin
{
    class WaylandServer;

    class KWIN_EXPORT PlaceholderWindow : public X11EventFilter
    {
    public:
        PlaceholderWindow();
        ~PlaceholderWindow();

    public:
        quint32 window() const;

        bool create(const QRect &rc, WaylandServer *ws = nullptr);
        void destroy();

        bool event(xcb_generic_event_t *event);
        void move(uint32_t x, uint32_t y);

        void setGeometry(const QRect &rc);
        QRect getGeometry() const;

    private:
        void setShape();

    private:
        Xcb::Window m_window;
        xcb_rectangle_t *m_shapeXRects;
        int m_shapeXRectsCount;
        xcb_gcontext_t foreground;

        WaylandServer *m_waylandServer;
    };
} // namespace KWin

#endif //KWIN_PLACEHOLDER_WINDDOW_H
