// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "moving_client_x11_filter.h"
#include "client.h"
#include "workspace.h"
#include <KKeyServer>
#include <xcb/xcb.h>

namespace KWin
{
volatile bool MovingClientX11Filter::isMove = false;
MovingClientX11Filter::MovingClientX11Filter()
    : X11EventFilter(QVector<int>{XCB_KEY_PRESS, XCB_MOTION_NOTIFY, XCB_BUTTON_PRESS, XCB_BUTTON_RELEASE})
{
}

bool MovingClientX11Filter::getMoveStatus()
{
    return isMove;
}

void MovingClientX11Filter::setMoveStatus(const bool &status)
{
    isMove = status;
}

bool MovingClientX11Filter::event(xcb_generic_event_t *event)
{
    auto client = dynamic_cast<Client*>(workspace()->getMovingClient());
    if (!client) {
        return false;
    }
    auto testWindow = [client, event] (xcb_window_t window) {
        isMove = true;
        return client->moveResizeGrabWindow() == window && client->windowEvent(event);
    };

    const uint8_t eventType = event->response_type & ~0x80;
    switch (eventType) {
    case XCB_KEY_PRESS: {
        int keyQt;
        xcb_key_press_event_t *keyEvent = reinterpret_cast<xcb_key_press_event_t*>(event);
        KKeyServer::xcbKeyPressEventToQt(keyEvent, &keyQt);
        client->keyPressEvent(keyQt, keyEvent->time);
        return true;
    }
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        return testWindow(reinterpret_cast<xcb_button_press_event_t*>(event)->event);
    case XCB_MOTION_NOTIFY:
        return testWindow(reinterpret_cast<xcb_motion_notify_event_t*>(event)->event);
    }
    return false;
}

}
