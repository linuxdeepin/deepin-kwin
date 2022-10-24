// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2017  Martin Graesslin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "popup_input_filter.h"
#include "deleted.h"
#include "shell_client.h"
#include "wayland_server.h"

#include <QMouseEvent>

namespace KWin
{

PopupInputFilter::PopupInputFilter()
    : QObject()
{
    connect(waylandServer(), &WaylandServer::shellClientAdded, this, &PopupInputFilter::handleClientAdded);
}

void PopupInputFilter::handleClientAdded(Toplevel *client)
{
    if (m_popupClients.contains(client)) {
        return;
    }
    if (client->hasPopupGrab()) {
        // TODO: verify that the Toplevel is allowed as a popup
        connect(client, &Toplevel::windowShown, this, &PopupInputFilter::handleClientAdded, Qt::UniqueConnection);
        connect(client, &Toplevel::windowClosed, this, &PopupInputFilter::handleClientRemoved, Qt::UniqueConnection);
        m_popupClients << client;
    }
}

void PopupInputFilter::handleClientRemoved(Toplevel *client)
{
    m_popupClients.removeOne(client);
}
bool PopupInputFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(nativeButton)
    if (m_popupClients.isEmpty()) {
        return false;
    }
    if (event->type() == QMouseEvent::MouseButtonPress) {
        auto pointerFocus = qobject_cast<AbstractClient*>(input()->findToplevel(event->globalPos()));
        if (!pointerFocus || !AbstractClient::belongToSameApplication(pointerFocus, qobject_cast<AbstractClient*>(m_popupClients.constLast()))) {
            // a press on a window (or no window) not belonging to the popup window
            cancelPopups();
            // filter out this press
            return true;
        }
        if (pointerFocus && pointerFocus->isDecorated()) {
            // test whether it is on the decoration
            const QRect clientRect = QRect(pointerFocus->clientPos(), pointerFocus->clientSize()).translated(pointerFocus->pos());
            if (!clientRect.contains(event->globalPos())) {
                cancelPopups();
                return true;
            }
        }
    }
    return false;
}

void PopupInputFilter::cancelPopups()
{
    while (!m_popupClients.isEmpty()) {
        auto c = m_popupClients.takeLast();
        c->popupDone();
    }
}

}
