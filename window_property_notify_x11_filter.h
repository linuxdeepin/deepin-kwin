// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_WINDOW_PROPERTY_NOTIFY_X11_FILTER_H
#define KWIN_WINDOW_PROPERTY_NOTIFY_X11_FILTER_H

#include "x11eventfilter.h"

namespace KWin
{
class EffectsHandlerImpl;

class WindowPropertyNotifyX11Filter : public X11EventFilter
{
public:
    explicit WindowPropertyNotifyX11Filter(EffectsHandlerImpl *effects);

    bool event(xcb_generic_event_t *event) override;

private:
    EffectsHandlerImpl *m_effects;
};

}

#endif
