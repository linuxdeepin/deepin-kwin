// Copyright 2019 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 2019 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_XWL_SELECTION
#define KWIN_XWL_SELECTION

#include <QObject>
#include <QVector>

#include <xcb/xcb.h>

struct xcb_xfixes_selection_notify_event_t;

class QTimer;

namespace KWin
{
namespace Xwl
{
class TransferWltoX;
class TransferXtoWl;
class WlSource;
class X11Source;

/*
 * Base class representing generic X selections and their respective
 * Wayland counter-parts.
 *
 * The class needs to be subclassed and adjusted according to the
 * selection, but provides common fucntionality to be expected of all
 * selections.
 *
 * A selection should exist through the whole runtime of an Xwayland
 * session.
 *
 * Independently of each other the class holds the currently active
 * source instance and active transfers relative to the represented
 * selection.
 */
class Selection : public QObject
{
    Q_OBJECT
public:
    static xcb_atom_t mimeTypeToAtom(const QString &mimeType);
    static xcb_atom_t mimeTypeToAtomLiteral(const QString &mimeType);
    static QStringList atomToMimeTypes(xcb_atom_t atom);
    static QString atomName(xcb_atom_t atom);
    static void sendSelNotify(xcb_selection_request_event_t *event, bool success);

    // on selection owner changes by X clients (Xwl -> Wl)
    bool handleXfixesNotify(xcb_xfixes_selection_notify_event_t *event);
    bool filterEvent(xcb_generic_event_t *event);

    xcb_atom_t atom() const {
        return m_atom;
    }
    xcb_window_t window() const {
        return m_window;
    }
    void overwriteRequestorWindow(xcb_window_t window);

Q_SIGNALS:
    void transferFinished(xcb_timestamp_t eventTime);

protected:
    Selection(xcb_atom_t atom, QObject *parent);
    void registerXfixes();

    virtual void doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event) = 0;
    virtual void x11OffersChanged(const QVector<QString> &added, const QVector<QString> &removed) = 0;

    virtual bool handleClientMessage(xcb_client_message_event_t *event) {
        Q_UNUSED(event);
        return false;
    }
    // sets the current provider of the selection
    void setWlSource(WlSource *src);
    WlSource* wlSource() const {
        return m_wlSrc;
    }
    void createX11Source(xcb_xfixes_selection_notify_event_t *event);
    X11Source* x11Source() const {
        return m_xSrc;
    }
    // must be called in order to provide data from Wl to X
    void ownSelection(bool own);
    void setWindow(xcb_window_t window) {
        m_window = window;
    }

private:
    bool handleSelRequest(xcb_selection_request_event_t *event);
    bool handleSelNotify(xcb_selection_notify_event_t *event);
    bool handlePropNotify(xcb_property_notify_event_t *event);

    void startTransferToWayland(xcb_atom_t target, qint32 fd);
    void startTransferToX(xcb_selection_request_event_t *event, qint32 fd);

    // Timeout transfers, which have become inactive due to client errors.
    void timeoutTransfers();
    void startTimeoutTransfersTimer();
    void endTimeoutTransfersTimer();

    xcb_atom_t m_atom = XCB_ATOM_NONE;
    xcb_window_t m_window = XCB_WINDOW_NONE;
    xcb_window_t m_requestorWindow = XCB_WINDOW_NONE;
    xcb_timestamp_t m_timestamp;

    // Active source, if any. Only one of them at max can exist
    // at the same time.
    WlSource *m_wlSrc = nullptr;
    X11Source *m_xSrc = nullptr;

    // active transfers
    QVector<TransferWltoX*> m_wlToXTransfers;
    QVector<TransferXtoWl*> m_xToWlTransfers;
    QTimer *m_timeoutTransfers = nullptr;

    bool m_disownPending = false;

    friend class WlSource;
};

}
}

#endif
