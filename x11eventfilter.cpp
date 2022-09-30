// Copyright (C) 2014 Fredrik Höglund <fredrik@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "x11eventfilter.h"
#include <workspace.h>

namespace KWin
{

X11EventFilter::X11EventFilter(const QVector<int> &eventTypes)
    : m_eventTypes(eventTypes)
    , m_extension(0)
{
    Workspace::self()->registerEventFilter(this);
}


X11EventFilter::X11EventFilter(int eventType, int opcode, int genericEventType)
    : X11EventFilter(eventType, opcode, QVector<int>{genericEventType})
{
}

X11EventFilter::X11EventFilter(int eventType, int opcode, const QVector< int > &genericEventTypes)
    : m_eventTypes(QVector<int>{eventType}), m_extension(opcode), m_genericEventTypes(genericEventTypes)
{
    Workspace::self()->registerEventFilter(this);
}

X11EventFilter::~X11EventFilter()
{
    if (auto w = Workspace::self()) {
        w->unregisterEventFilter(this);
    }
}

bool X11EventFilter::isGenericEvent() const
{
    if (m_eventTypes.count() != 1) {
        return false;
    }
    return m_eventTypes.first() == XCB_GE_GENERIC;
}

}
