// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "window_property_notify_x11_filter.h"
#include "client.h"
#include "effects.h"
#include "unmanaged.h"
#include "workspace.h"

namespace KWin
{

WindowPropertyNotifyX11Filter::WindowPropertyNotifyX11Filter(EffectsHandlerImpl *effects)
    : X11EventFilter(QVector<int>{XCB_PROPERTY_NOTIFY})
    , m_effects(effects)
{
}

bool WindowPropertyNotifyX11Filter::event(xcb_generic_event_t *event)
{
    const auto *pe = reinterpret_cast<xcb_property_notify_event_t*>(event);
    if (!m_effects->isPropertyTypeRegistered(pe->atom)) {
        return false;
    }
    if (pe->window == kwinApp()->x11RootWindow()) {
        emit m_effects->propertyNotify(nullptr, pe->atom);
    } else if (const auto c = workspace()->findClient(Predicate::WindowMatch, pe->window)) {
        emit m_effects->propertyNotify(c->effectWindow(), pe->atom);
    } else if (const auto c = workspace()->findUnmanaged(pe->window)) {
        emit m_effects->propertyNotify(c->effectWindow(), pe->atom);
    }
    return false;
}

}
