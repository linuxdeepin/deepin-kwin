// Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SCREENLOCKERWATCHER_H
#define KWIN_SCREENLOCKERWATCHER_H

#include <QObject>

#include <kwinglobals.h>

class OrgFreedesktopScreenSaverInterface;
class QDBusServiceWatcher;
class QDBusPendingCallWatcher;

namespace KWin
{

class KWIN_EXPORT ScreenLockerWatcher : public QObject
{
    Q_OBJECT
public:
    virtual ~ScreenLockerWatcher();
    bool isLocked() const {
        return m_locked;
    }
Q_SIGNALS:
    void locked(bool locked);
private Q_SLOTS:
    void setLocked(bool activated);
    void activeQueried(QDBusPendingCallWatcher *watcher);
    void serviceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner);
    void serviceRegisteredQueried();
    void serviceOwnerQueried();
private:
    void initialize();
    OrgFreedesktopScreenSaverInterface *m_interface;
    QDBusServiceWatcher *m_serviceWatcher;
    bool m_locked;

    KWIN_SINGLETON(ScreenLockerWatcher)
};
}

#endif
