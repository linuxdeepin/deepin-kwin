// Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
// Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
// Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

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
    , xdnd_selection(QByteArrayLiteral("XdndSelection"))
    , xdnd_aware(QByteArrayLiteral("XdndAware"))
    , xdnd_enter(QByteArrayLiteral("XdndEnter"))
    , xdnd_type_list(QByteArrayLiteral("XdndTypeList"))
    , xdnd_position(QByteArrayLiteral("XdndPosition"))
    , xdnd_status(QByteArrayLiteral("XdndStatus"))
    , xdnd_action_copy(QByteArrayLiteral("XdndActionCopy"))
    , xdnd_action_move(QByteArrayLiteral("XdndActionMove"))
    , xdnd_action_ask(QByteArrayLiteral("XdndActionAsk"))
    , xdnd_drop(QByteArrayLiteral("XdndDrop"))
    , xdnd_leave(QByteArrayLiteral("XdndLeave"))
    , xdnd_finished(QByteArrayLiteral("XdndFinished"))
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
    , text(QByteArrayLiteral("TEXT"))
    , uri_list(QByteArrayLiteral("text/uri-list"))
    , netscape_url(QByteArrayLiteral("_NETSCAPE_URL"))
    , moz_url(QByteArrayLiteral("text/x-moz-url"))
    , wl_surface_id(QByteArrayLiteral("WL_SURFACE_ID"))
    , kde_net_wm_appmenu_service_name(QByteArrayLiteral("_KDE_NET_WM_APPMENU_SERVICE_NAME"))
    , kde_net_wm_appmenu_object_path(QByteArrayLiteral("_KDE_NET_WM_APPMENU_OBJECT_PATH"))
    //此原子属性是兼容dtk在触屏环境下发送的自定义XCB_CLIENT_MESSAGE事件中的_DEEPIN_MOVE_UPDATE属性
    , deepin_move_update(QByteArrayLiteral("_DEEPIN_MOVE_UPDATE"))
    //此原子属性不为0时，禁止窗口移动
    , deepin_forhibit_move(QByteArrayLiteral("_DEEPIN_FORHIBIT_MOVE"))
    , clipboard(QByteArrayLiteral("CLIPBOARD"))
    , timestamp(QByteArrayLiteral("TIMESTAMP"))
    , targets(QByteArrayLiteral("TARGETS"))
    , delete_atom(QByteArrayLiteral("DELETE"))
    , incr(QByteArrayLiteral("INCR"))
    , wl_selection(QByteArrayLiteral("WL_SELECTION"))
    , m_dtSmWindowInfo(QByteArrayLiteral("_DT_SM_WINDOW_INFO"))
    , m_motifSupport(QByteArrayLiteral("_MOTIF_WM_INFO"))
    , deepin_split_window(QByteArrayLiteral("_DEEPIN_SPLIT_WINDOW"))
    , deepin_lock_screen(QByteArrayLiteral("_DEEPIN_LOCK_SCREEN"))
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
