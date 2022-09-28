/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2019 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_XWL_DRAG_X
#define KWIN_XWL_DRAG_X

#include "drag.h"

#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/dataoffer.h>

#include <KWayland/Server/datadevicemanager_interface.h>

#include <QPoint>
#include <QPointer>
#include <QVector>

namespace KWayland {
namespace Client {
class DataSource;
}
}

namespace KWin
{
class Toplevel;
class AbstractClient;

namespace Xwl
{
class X11Source;
enum class DragEventReply;
class WlVisit;

using Mimes = QVector<QPair<QString, xcb_atom_t> >;

class XToWlDrag : public Drag
{
    Q_OBJECT
public:
    explicit XToWlDrag(X11Source *source);
    ~XToWlDrag() override;

    DragEventReply moveFilter(Toplevel *target, QPoint pos) override;
    bool handleClientMessage(xcb_client_message_event_t *event) override;

    void setDragAndDropAction(DnDAction action);
    DnDAction selectedDragAndDropAction();

    bool end() override {
        return false;
    }
    X11Source* x11Source() const {
        return m_src;
    }

private:
    void setOffers(const Mimes &offers);
    void offerCallback(const QString &mime);
    void setDragTarget();

    bool checkForFinished();

    KWayland::Client::DataSource *m_dataSource;

    Mimes m_offers;
    Mimes m_offersPending;

    X11Source *m_src;
    QVector<QPair<xcb_timestamp_t, bool> > m_dataRequests;

    WlVisit *m_visit = nullptr;
    QVector<WlVisit*> m_oldVisits;

    bool m_performed = false;
    DnDAction m_lastSelectedDragAndDropAction = DnDAction::None;
};

class WlVisit : public QObject
{
    Q_OBJECT
public:
    WlVisit(AbstractClient *target, XToWlDrag *drag);
    ~WlVisit();

    bool handleClientMessage(xcb_client_message_event_t *event);
    bool leave();

    AbstractClient *target() const {
        return m_target;
    }
    xcb_window_t window() const {
        return m_window;
    }
    bool entered() const {
        return m_entered;
    }
    bool dropHandled() const {
        return m_dropHandled;
    }
    bool finished() const {
        return m_finished;
    }
    void sendFinished();

Q_SIGNALS:
    void offersReceived(const Mimes &offers);
    void finish(WlVisit *self);

private:
    bool handleEnter(xcb_client_message_event_t *ev);
    bool handlePosition(xcb_client_message_event_t *ev);
    bool handleDrop(xcb_client_message_event_t *ev);
    bool handleLeave(xcb_client_message_event_t *ev);

    void sendStatus();

    void getMimesFromWinProperty(Mimes &offers);

    bool targetAcceptsAction() const;

    void doFinish();
    void unmapProxyWindow();

    AbstractClient *m_target;
    xcb_window_t m_window;

    xcb_window_t m_srcWindow = XCB_WINDOW_NONE;
    XToWlDrag *m_drag;

    uint32_t m_version = 0;

    xcb_atom_t m_actionAtom;
    DnDAction m_action = DnDAction::None;

    bool m_mapped = false;
    bool m_entered = false;
    bool m_dropHandled = false;
    bool m_finished = false;

};

}
}

#endif
