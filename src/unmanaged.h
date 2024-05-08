/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_UNMANAGED_H
#define KWIN_UNMANAGED_H

#include <netwm.h>

#include "toplevel.h"

namespace KWin
{

class KWIN_EXPORT Unmanaged : public Toplevel
{
    Q_OBJECT
public:
    explicit Unmanaged();
    bool windowEvent(xcb_generic_event_t *e);
    bool track(xcb_window_t w);
    bool hasScheduledRelease() const;
    static void deleteUnmanaged(Unmanaged* c);
    int desktop() const override;
    QStringList activities() const override;
    QVector<VirtualDesktop *> desktops() const override;
    QPoint clientPos() const override;
    Layer layer() const override {
        return UnmanagedLayer;
    }
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    bool isOutline() const override;
    bool isSwitcherWin() const override;

    bool fetchWindowForLockScreen();

    bool isKeepAbove();

public Q_SLOTS:
    void release(ReleaseReason releaseReason = ReleaseReason::Release);

private:
    ~Unmanaged() override; // use release()
    // handlers for X11 events
    void configureNotifyEvent(xcb_configure_notify_event_t *e);
    void damageNotifyEvent();
    QWindow *findInternalWindow() const;
    void associate();
    void initialize();
    bool m_outline = false;
    bool m_scheduledRelease = false;
    bool m_switcherwin = false;
};

} // namespace

#endif
