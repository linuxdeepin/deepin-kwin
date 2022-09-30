// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "was_user_interaction_x11_filter.h"
#include "workspace.h"
#include <xcb/xcb.h>

namespace KWin
{
WasUserInteractionX11Filter::WasUserInteractionX11Filter()
    : X11EventFilter(QVector<int>{XCB_KEY_PRESS, XCB_KEY_RELEASE, XCB_BUTTON_PRESS, XCB_BUTTON_RELEASE})
{
}

bool WasUserInteractionX11Filter::event(xcb_generic_event_t *event)
{
    Q_UNUSED(event);
    workspace()->setWasUserInteraction();
    return false;
}

}
