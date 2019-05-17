/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

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

#include "atoms.h"

namespace KWin
{

Atoms::Atoms()
    : kwin_running(QByteArrayLiteral("KWIN_RUNNING"))
    , activities(QByteArrayLiteral("_KDE_NET_WM_ACTIVITIES"))
    , wm_protocols(QByteArrayLiteral("WM_PROTOCOLS"))
    , wm_delete_window(QByteArrayLiteral("WM_DELETE_WINDOW"))
    , wm_take_focus(QByteArrayLiteral("WM_TAKE_FOCUS"))
    , wm_change_state(QByteArrayLiteral("WM_CHANGE_STATE"))
    , wm_client_leader(QByteArrayLiteral("WM_CLIENT_LEADER"))
    , wm_window_role(QByteArrayLiteral("WM_WINDOW_ROLE"))
    , wm_state(QByteArrayLiteral("WM_STATE"))
    , sm_client_id(QByteArrayLiteral("SM_CLIENT_ID"))
    , motif_wm_hints(QByteArrayLiteral("_MOTIF_WM_HINTS"))
    , net_wm_context_help(QByteArrayLiteral("_NET_WM_CONTEXT_HELP"))
    , net_wm_ping(QByteArrayLiteral("_NET_WM_PING"))
    , net_wm_user_time(QByteArrayLiteral("_NET_WM_USER_TIME"))
    , kde_net_wm_user_creation_time(QByteArrayLiteral("_KDE_NET_WM_USER_CREATION_TIME"))
    , net_wm_take_activity(QByteArrayLiteral("_NET_WM_TAKE_ACTIVITY"))
    , net_wm_window_opacity(QByteArrayLiteral("_NET_WM_WINDOW_OPACITY"))
    , xdnd_aware(QByteArrayLiteral("XdndAware"))
    , xdnd_position(QByteArrayLiteral("XdndPosition"))
    , net_frame_extents(QByteArrayLiteral("_NET_FRAME_EXTENTS"))
    , kde_net_wm_frame_strut(QByteArrayLiteral("_KDE_NET_WM_FRAME_STRUT"))
    , net_wm_sync_request_counter(QByteArrayLiteral("_NET_WM_SYNC_REQUEST_COUNTER"))
    , net_wm_sync_request(QByteArrayLiteral("_NET_WM_SYNC_REQUEST"))
    , kde_net_wm_shadow(QByteArrayLiteral("_KDE_NET_WM_SHADOW"))
    , kde_net_wm_tab_group(QByteArrayLiteral("_KDE_NET_WM_TAB_GROUP"))
    , kde_first_in_window_list(QByteArrayLiteral("_KDE_FIRST_IN_WINDOWLIST"))
    , kde_color_sheme(QByteArrayLiteral("_KDE_NET_WM_COLOR_SCHEME"))
    , kde_skip_close_animation(QByteArrayLiteral("_KDE_NET_WM_SKIP_CLOSE_ANIMATION"))
    , kde_screen_edge_show(QByteArrayLiteral("_KDE_NET_WM_SCREEN_EDGE_SHOW"))
    , gtk_frame_extents(QByteArrayLiteral("_GTK_FRAME_EXTENTS"))
    , gtk_show_window_menu(QByteArrayLiteral("_GTK_SHOW_WINDOW_MENU"))
    , kwin_dbus_service(QByteArrayLiteral("_ORG_KDE_KWIN_DBUS_SERVICE"))
    , utf8_string(QByteArrayLiteral("UTF8_STRING"))
    , wl_surface_id(QByteArrayLiteral("WL_SURFACE_ID"))
    , kde_net_wm_appmenu_service_name(QByteArrayLiteral("_KDE_NET_WM_APPMENU_SERVICE_NAME"))
    , kde_net_wm_appmenu_object_path(QByteArrayLiteral("_KDE_NET_WM_APPMENU_OBJECT_PATH"))
    , m_dtSmWindowInfo(QByteArrayLiteral("_DT_SM_WINDOW_INFO"))
    , m_motifSupport(QByteArrayLiteral("_MOTIF_WM_INFO"))
    , m_helpersRetrieved(false)
{
}

void Atoms::retrieveHelpers()
{
    if (m_helpersRetrieved) {
        return;
    }
    // just retrieve the atoms once, all others are retrieved when being accessed
    // Q_UNUSED is used in the hope that the compiler doesn't optimize the operations away
    xcb_atom_t atom = m_dtSmWindowInfo;
    Q_UNUSED(atom)
    atom = m_motifSupport;
    Q_UNUSED(atom)
    m_helpersRetrieved = true;
}

} // namespace
