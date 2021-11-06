/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Zhang Haidong <zhanghaidong@uniontech.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

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
