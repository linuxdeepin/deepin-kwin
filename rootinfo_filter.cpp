/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

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
#include "rootinfo_filter.h"
#include "netinfo.h"
#include "virtualdesktops.h"
#include "xcbutils.h"
#include "atoms.h"

namespace KWin
{
class Filter : public X11EventFilter
{
public:
    Filter(RootInfoFilter *parent, int eventType)
        : X11EventFilter(eventType, 0, QVector<int>{})
        , m_parent(parent) {}

    bool event(xcb_generic_event_t *event) override
    {
        return m_parent->event(event);
    }

private:
    RootInfoFilter *m_parent;
};

RootInfoFilter::RootInfoFilter(RootInfo *parent)
    : m_rootInfo(parent)
{
    m_filters << new Filter(this, XCB_PROPERTY_NOTIFY)
              << new Filter(this, XCB_CLIENT_MESSAGE);
}

RootInfoFilter::~RootInfoFilter()
{
    qDeleteAll(m_filters);
}

static QVector<xcb_atom_t> getNetWMAtoms(xcb_atom_t property)
{
    QVector<xcb_atom_t> net_wm_atoms;

    xcb_window_t root = rootWindow();
    int offset = 0;
    int remaining = 0;
    xcb_connection_t *xcb_connection = connection();

    do {
        xcb_get_property_cookie_t cookie = xcb_get_property(xcb_connection, false, root,
                                                            property,
                                                            XCB_ATOM_ATOM, offset, 1024);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(xcb_connection, cookie, NULL);
        if (!reply)
            break;

        remaining = 0;

        if (reply->type == XCB_ATOM_ATOM && reply->format == 32) {
            int len = xcb_get_property_value_length(reply)/sizeof(xcb_atom_t);
            xcb_atom_t *atoms = (xcb_atom_t *)xcb_get_property_value(reply);
            int s = net_wm_atoms.size();
            net_wm_atoms.resize(s + len);
            memcpy(net_wm_atoms.data() + s, atoms, len*sizeof(xcb_atom_t));

            remaining = reply->bytes_after;
            offset += len;
        }

        free(reply);
    } while (remaining > 0);

    return net_wm_atoms;
}

bool RootInfoFilter::event(xcb_generic_event_t *event)
{
    NET::Properties dirtyProtocols;
    NET::Properties2 dirtyProtocols2;
    m_rootInfo->event(event, &dirtyProtocols, &dirtyProtocols2);
    if (dirtyProtocols & NET::DesktopNames)
        VirtualDesktopManager::self()->save();
    if (dirtyProtocols2 & NET::WM2DesktopLayout)
        VirtualDesktopManager::self()->updateLayout();

    // TODO(zccrs): 应该在 kwindowsystem 项目的 NETRootInfo 中添加
    if ((event->response_type & ~0x80) == XCB_PROPERTY_NOTIFY) {
        xcb_property_notify_event_t *pe = reinterpret_cast<xcb_property_notify_event_t*>(event);
        Xcb::Atom net_support(QByteArrayLiteral("_NET_SUPPORTED"));
        xcb_atom_t gtk_frame_extents = atoms->gtk_frame_extents;

        if (pe->atom == net_support) {
            auto old_atoms = getNetWMAtoms(net_support);
            QVector<xcb_atom_t> new_atoms;

            if (!old_atoms.contains(gtk_frame_extents)) {
                // Append _GTK_FRAME_EXTENTS atom to _NET_SUPPORTED
                new_atoms << gtk_frame_extents;
            }

            if (!old_atoms.contains(atoms->gtk_show_window_menu)) {
                // Support _GTK_SHOW_WINDOW_MENU
                new_atoms << atoms->gtk_show_window_menu;
            }

            if (!new_atoms.isEmpty()) {
                xcb_change_property(connection(), XCB_PROP_MODE_APPEND, rootWindow(),
                                    net_support, XCB_ATOM_ATOM, 32, new_atoms.length(), new_atoms.constData());
            }
        }
    }

    return false;
}

}
