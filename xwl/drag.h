// Copyright 2019 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 2019 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_XWL_DRAG
#define KWIN_XWL_DRAG

#include <KWayland/Client/datadevicemanager.h>

#include <QPoint>

#include <xcb/xcb.h>

namespace KWin
{
class Toplevel;

namespace Xwl
{
enum class DragEventReply;

using DnDAction = KWayland::Client::DataDeviceManager::DnDAction;

/**
 * An ongoing drag operation.
 */
class Drag : public QObject
{
    Q_OBJECT
public:
    static void sendClientMessage(xcb_window_t target, xcb_atom_t type, xcb_client_message_data_t *data);
    static DnDAction atomToClientAction(xcb_atom_t atom);
    static xcb_atom_t clientActionToAtom(DnDAction action);

    virtual ~Drag() = default;
    virtual bool handleClientMessage(xcb_client_message_event_t *event) = 0;
    virtual DragEventReply moveFilter(Toplevel *target, QPoint pos) = 0;

    virtual bool end() = 0;

Q_SIGNALS:
    void finish(Drag *self);
};

}
}

#endif
