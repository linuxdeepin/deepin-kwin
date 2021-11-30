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
#include "clipboard.h"

#include "xwayland.h"
#include "databridge.h"
#include "datasource.h"
#include "selection_source.h"
#include "transfer.h"

#include "wayland_server.h"
#include "workspace.h"
#include "client.h"

#include <KWayland/Server/seat_interface.h>

#include <xcb/xcb_event.h>
#include <xcb/xfixes.h>

#include <xwayland_logging.h>

namespace KWin {
namespace Xwl {

Clipboard::Clipboard(xcb_atom_t atom, QObject *parent)
    : Selection(atom, parent)
{
    auto *xcbConn = kwinApp()->x11Connection();

    const uint32_t clipboardValues[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                                   XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_create_window(xcbConn,
                      XCB_COPY_FROM_PARENT,
                      window(),
                      kwinApp()->x11RootWindow(),
                      0, 0,
                      10, 10,
                      0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      XCB_COPY_FROM_PARENT,
                      XCB_CW_EVENT_MASK,
                      clipboardValues);
    registerXfixes();
    xcb_flush(xcbConn);

    connect(waylandServer()->seat(), &KWayland::Server::SeatInterface::selectionChanged,
            this, &Clipboard::wlSelectionChanged);
}

void Clipboard::wlSelectionChanged(KWayland::Server::AbstractDataSource *dsi)
{
    if (m_waitingForTargets) {
        return;
    }

    if (!ownsSelection(dsi)) {
        // Wayland native client provides new selection
        if (!m_checkConnection) {
            m_checkConnection = connect(workspace(), &Workspace::clientActivated,
                                        this, &Clipboard::checkWlSource);
        }
        // remove previous source so checkWlSource() can create a new one
        setWlSource(nullptr);
    }
    checkWlSource();
}

bool Clipboard::ownsSelection(KWayland::Server::AbstractDataSource *dsi) const
{
    return dsi && dsi == m_selectionSource.get();
}

void Clipboard::checkWlSource()
{
    auto dsi = waylandServer()->seat()->selection();
    auto removeSource = [this] {
        if (wlSource()) {
            setWlSource(nullptr);
            ownSelection(false);
        }
    };

    // Wayland source gets created when:
    // - the Wl selection exists,
    // - its source is not Xwayland,
    // - a client is active,
    // - this client is an Xwayland one.
    //
    // Otherwise the Wayland source gets destroyed to shield
    // against snooping X clients.

    if (!dsi || ownsSelection(dsi)) {
        // Xwayland source or no source
        disconnect(m_checkConnection);
        m_checkConnection = QMetaObject::Connection();
        removeSource();
        return;
    }
    if (!workspace()->activeClient() || !workspace()->activeClient()->inherits("KWin::Client")) {
        // no active client or active client is Wayland native
        removeSource();
        return;
    }
    // Xwayland client is active and we need a Wayland source
    if (wlSource()) {
        // source already exists, nothing more to do
        return;
    }
    auto *wls = new WlSource(this);
    setWlSource(wls);
    if (dsi) {
        wls->setDataSourceIface(dsi);
    }
    ownSelection(true);
}

void Clipboard::doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event)
{
    createX11Source(NULL);

    const auto *ac = workspace()->activeClient();
    if (!qobject_cast<const KWin::Client *>(ac)) {
        // clipboard is only allowed to be acquired when Xwayland has focus
        // TODO: can we make this stronger (window id comparision)?
        return;
    }
    createX11Source(event);
    auto *xSrc = x11Source();
    if (xSrc) {
        xSrc->getTargets();
    }
}

void Clipboard::x11OffersChanged(const QVector<QString> &added, const QVector<QString> &removed)
{
    Q_UNUSED(added)
    Q_UNUSED(removed)
    m_waitingForTargets = false;
    X11Source *source = x11Source();
    if (!source) {
        qCWarning(KWIN_XWL) << "offers changed when not having an X11Source!?";
        return;
    }

    const Mimes offers = source->offers();

    if (!offers.isEmpty()) {
        QStringList mimeTypes;
        mimeTypes.reserve(offers.size());
        std::transform(offers.begin(), offers.end(), std::back_inserter(mimeTypes), [](const Mimes::value_type &pair) {
            return pair.first;
        });
        auto newSelection = std::make_unique<XwlDataSource>(nullptr);
        newSelection->setMimeTypes(mimeTypes);
        connect(newSelection.get(), &XwlDataSource::dataRequested, source, &X11Source::startTransfer);
        // we keep the old selection around because setSelection needs it to be still alive
        std::swap(m_selectionSource, newSelection);
        waylandServer()->seat()->setSelection(m_selectionSource.get());
    } else {
        KWayland::Server::AbstractDataSource *currentSelection = waylandServer()->seat()->selection();
        if (!ownsSelection(currentSelection)) {
            waylandServer()->seat()->setSelection(nullptr);
            m_selectionSource.reset();
        }
    }
}

}
}
