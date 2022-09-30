// Copyright 2019 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 2019 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_XWL_DRAG_WL
#define KWIN_XWL_DRAG_WL

#include "drag.h"

#include <KWayland/Client/dataoffer.h>

#include <QPoint>
#include <QPointer>
#include <QVector>

namespace KWayland
{
namespace Client
{
class Surface;
}
namespace Server
{
class DataDeviceInterface;
class DataSourceInterface;
class SurfaceInterface;
class AbstractDataSource;
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
class Xvisit;

using DnDActions = KWayland::Client::DataDeviceManager::DnDActions;

class WlToXDrag : public Drag
{
    Q_OBJECT
public:
    explicit WlToXDrag();

    DragEventReply moveFilter(Toplevel *target, QPoint pos) override;
    bool handleClientMessage(xcb_client_message_event_t *event) override;

    bool end() override;

    KWayland::Server::AbstractDataSource *dataSourceIface() const {
        return m_dsi;
    }

private:
    KWayland::Server::AbstractDataSource *m_dsi;
    Xvisit *m_visit = nullptr;
};

// visit to an X window
class Xvisit : public QObject
{
    Q_OBJECT
public:
    // TODO: handle ask action

    Xvisit(WlToXDrag *drag, AbstractClient *target);

    bool handleClientMessage(xcb_client_message_event_t *event);
    bool handleStatus(xcb_client_message_event_t *ev);
    bool handleFinished(xcb_client_message_event_t *ev);

    void sendPosition(const QPointF &globalPos);
    void leave();

    bool finished() const {
        return m_state.finished;
    }
    AbstractClient *target() const {
        return m_target;
    }

Q_SIGNALS:
    void finish(Xvisit *self);

private:
    void sendEnter();
    void sendDrop(uint32_t time);
    void sendLeave();

    void receiveOffer();
    void enter();

    void retrieveSupportedActions();
    void determineProposedAction();
    void requestDragAndDropAction();
    void setProposedAction();

    void drop();

    void doFinish();
    void stopConnections();

    WlToXDrag *m_drag;
    AbstractClient *m_target;
    uint32_t m_version = 0;

    QMetaObject::Connection m_enterCon;
    QMetaObject::Connection m_motionCon;
    QMetaObject::Connection m_actionCon;
    QMetaObject::Connection m_dropCon;

    struct {
        bool pending = false;
        bool cached = false;
        QPoint cache;
    } m_pos;

    // Must be QPointer, because KWayland::Client::DataDevice
    // might delete it.
    QPointer<KWayland::Client::DataOffer> m_dataOffer;

    // supported by the Wl source
    DnDActions m_supportedActions = DnDAction::None;
    // preferred by the X client
    DnDAction m_preferredAction = DnDAction::None;
    // decided upon by the compositor
    DnDAction m_proposedAction = DnDAction::None;

    struct {
        bool entered = false;
        bool dropped = false;
        bool finished = false;
    } m_state;

    bool m_accepts = false;
};

}
}

#endif
