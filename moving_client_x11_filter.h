// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_MOVING_CLIENT_X11_FILTER_H
#define KWIN_MOVING_CLIENT_X11_FILTER_H
#include "x11eventfilter.h"

namespace KWin
{

class MovingClientX11Filter : public X11EventFilter
{
public:
    explicit MovingClientX11Filter();

    bool event(xcb_generic_event_t *event) override;
};

}

#endif

