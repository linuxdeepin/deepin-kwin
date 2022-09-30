// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_ROOTINFO_FILTER_H
#define KWIN_ROOTINFO_FILTER_H

#include "x11eventfilter.h"

namespace KWin
{
class RootInfo;

class RootInfoFilter : public X11EventFilter
{
public:
    explicit RootInfoFilter(RootInfo *parent);

    bool event(xcb_generic_event_t *event) override;

private:
    RootInfo *m_rootInfo;
};

}

#endif
