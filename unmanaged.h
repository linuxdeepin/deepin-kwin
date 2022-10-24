// Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_UNMANAGED_H
#define KWIN_UNMANAGED_H

#include <netwm.h>

#include "toplevel.h"

namespace KWin
{

class Unmanaged
    : public Toplevel
{
    Q_OBJECT
public:
    explicit Unmanaged();
    bool windowEvent(xcb_generic_event_t *e);
    bool track(Window w);
    bool hasScheduledRelease() const;
    static void deleteUnmanaged(Unmanaged* c);
    virtual int desktop() const;
    virtual QStringList activities() const;
    virtual QVector<VirtualDesktop *> desktops() const override;
    virtual QPoint clientPos() const;
    virtual QSize clientSize() const;
    virtual QRect transparentRect() const;
    virtual Layer layer() const {
        return UnmanagedLayer;
    }
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const;

    bool fetchWindowForLockScreen();

    bool isKeepAbove();

public Q_SLOTS:
    void release(ReleaseReason releaseReason = ReleaseReason::Release);
protected:
    virtual void debug(QDebug& stream) const;
    void addDamage(const QRegion &damage) override;
private:
    virtual ~Unmanaged(); // use release()
    // handlers for X11 events
    void configureNotifyEvent(xcb_configure_notify_event_t *e);

    bool m_scheduledRelease;
};

} // namespace

#endif
