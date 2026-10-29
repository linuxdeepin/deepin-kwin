/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QGuiApplication>
#include <QLoggingCategory>

#include <xcb/xcb.h>

#include <cstdlib>

namespace KWin
{

Q_DECLARE_LOGGING_CATEGORY(KWIN_MTVEX)

/**
 * @brief Tests whether the application is running on the X11 platform.
 * @return True when the QGuiApplication platform name is "xcb".
 */
inline bool onX11Platform()
{
    return qGuiApp && qGuiApp->platformName() == QLatin1String("xcb");
}

/**
 * @brief Returns a shared X11 connection, opening it on first use.
 * @return The XCB connection, or nullptr when not on X11 or the connection failed.
 */
inline xcb_connection_t *x11Connection()
{
    if (!onX11Platform()) {
        return nullptr;
    }
    static xcb_connection_t *conn = [] {
        xcb_connection_t *c = xcb_connect(nullptr, nullptr);
        return (c && !xcb_connection_has_error(c)) ? c : nullptr;
    }();
    return conn;
}

/**
 * @brief Queries the physical state of the left mouse button.
 * @return True when the left button is pressed, or when the state cannot be determined.
 */
inline bool leftButtonPhysicallyDown()
{
    if (!onX11Platform()) {
        return true;
    }
    xcb_connection_t *conn = x11Connection();
    if (!conn) {
        return true;
    }
    const xcb_setup_t *setup = xcb_get_setup(conn);
    if (!setup) {
        return true;
    }
    const xcb_screen_t *screen = xcb_setup_roots_iterator(setup).data;
    if (!screen) {
        return true;
    }
    bool down = false;
    if (xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(
            conn, xcb_query_pointer(conn, screen->root), nullptr)) {
        down = (reply->mask & XCB_BUTTON_MASK_1) != 0;
        std::free(reply);
    }
    return down;
}

}
