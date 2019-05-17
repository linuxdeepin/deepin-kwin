/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "rootinfo_filter.h"
#include "netinfo.h"
#include "virtualdesktops.h"
#include "../utils/xcbutils.h"
#include "atoms.h"
namespace KWin
{

RootInfoFilter::RootInfoFilter(RootInfo *parent)
    : X11EventFilter(QVector<int>{XCB_PROPERTY_NOTIFY, XCB_CLIENT_MESSAGE})
    , m_rootInfo(parent)
{
}

static QVector<xcb_atom_t> getNetWMAtoms(xcb_atom_t property)
{
    QVector<xcb_atom_t> net_wm_atoms;

    xcb_window_t root = rootWindow();
    int offset = 0;
    int remaining = 0;
    xcb_connection_t *xcb_connection = connection();

    do {
        xcb_get_property_cookie_t cookie = xcb_get_property(xcb_connection, false, root, property, XCB_ATOM_ATOM, offset, 1024);
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
