// Copyright 2019 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 2019 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_XWL_CLIPBOARD
#define KWIN_XWL_CLIPBOARD

#include <memory>

#include "selection.h"

namespace KWayland
{
namespace Server
{
class DataDeviceInterface;
class AbstractDataSource;
}
}

namespace KWin
{
namespace Xwl
{
class XwlDataSource;
/**
 * Represents the X clipboard, which is on Wayland side just called
 * @e selection.
 */
class Clipboard : public Selection
{
    Q_OBJECT
public:
    Clipboard(xcb_atom_t atom, QObject *parent);

private:
    void doHandleXfixesNotify(xcb_xfixes_selection_notify_event_t *event) override;
    void x11OffersChanged(const QVector<QString> &added, const QVector<QString> &removed) override;
    /**
     * React to Wl selection change.
     */
    void wlSelectionChanged(KWayland::Server::AbstractDataSource *ddi);
    /**
     * Check the current state of the selection and if a source needs
     * to be created or destroyed.
     */
    void checkWlSource();

    /**
     * Returns if dsi is managed by our data bridge
     */
    bool ownsSelection(KWayland::Server::AbstractDataSource *dsi) const;

    QMetaObject::Connection m_checkConnection;

    Q_DISABLE_COPY(Clipboard)
    bool m_waitingForTargets = false;
    std::unique_ptr<XwlDataSource> m_selectionSource;
};

}
}

#endif
