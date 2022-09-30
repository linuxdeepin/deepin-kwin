// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_WAS_USER_INTERACTION_X11_FILTER_H
#define KWIN_WAS_USER_INTERACTION_X11_FILTER_H
#include "x11eventfilter.h"

namespace KWin
{

class WasUserInteractionX11Filter : public X11EventFilter
{
public:
    explicit WasUserInteractionX11Filter();

    bool event(xcb_generic_event_t *event) override;
};

}

#endif
