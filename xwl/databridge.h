// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2018 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_XWL_DATABRIDGE
#define KWIN_XWL_DATABRIDGE

#include <QObject>
#include <QPoint>

#include <xcb/xcb.h>

class xcb_xfixes_selection_notify_event_t;

namespace KWayland {
namespace Client {
class DataDevice;
}
namespace Server {
class DataDeviceInterface;
class SurfaceInterface;
}
}

namespace KWin
{
class Toplevel;

namespace Xwl
{
class Xwayland;
class Clipboard;
class Dnd;
enum class DragEventReply;

/*
 * Interface class for all data sharing in the context of X selections
 * and Wayland's internal mechanism.
 *
 * Exists only once per Xwayland session.
 */
class DataBridge : public QObject
{
    Q_OBJECT
public:
    static DataBridge* self();

    explicit DataBridge(QObject *parent = nullptr);
    ~DataBridge();

    bool filterEvent(xcb_generic_event_t *event);
    DragEventReply dragMoveFilter(Toplevel *target, QPoint pos);

    KWayland::Client::DataDevice *dataDevice() const
    {
        return m_dd;
    }
    KWayland::Server::DataDeviceInterface *dataDeviceIface() const
    {
        return m_ddi;
    }
    Dnd* dnd() const
    {
        return m_dnd;
    }

private:
    void init();

    bool handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event);

    Clipboard *m_clipboard = nullptr;
    Dnd *m_dnd = nullptr;

    /* Internal data device interface */
    KWayland::Client::DataDevice *m_dd = nullptr;
    KWayland::Server::DataDeviceInterface *m_ddi = nullptr;
};

}
}

#endif
