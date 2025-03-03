/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QHash>
#include <QTimer>
#include <QtPlugin>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <KIdleTime/private/abstractsystempoller.h>
#else
#include <private/kabstractidletimepoller_p.h>
#endif

namespace KWin
{

class IdleDetector;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
class KWinIdleTimePoller : public AbstractSystemPoller
#else
class KWinIdleTimePoller : public KAbstractIdleTimePoller
#endif
{
    Q_OBJECT
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Q_PLUGIN_METADATA(IID "org.kde.kidletime.AbstractSystemPoller" FILE "kwin.json")
    Q_INTERFACES(AbstractSystemPoller)
#else
    Q_PLUGIN_METADATA(IID KAbstractIdleTimePoller_iid FILE "kwin.json")
    Q_INTERFACES(KAbstractIdleTimePoller)
#endif

public:
    KWinIdleTimePoller(QObject *parent = nullptr);

    bool isAvailable() override;
    bool setUpPoller() override;
    void unloadPoller() override;

public Q_SLOTS:
    void addTimeout(int nextTimeout) override;
    void removeTimeout(int nextTimeout) override;
    QList<int> timeouts() const override;
    int forcePollRequest() override;
    void catchIdleEvent() override;
    void stopCatchingIdleEvents() override;
    void simulateUserActivity() override;

private:
    IdleDetector *m_catchResumeTimeout = nullptr;
    QHash<int, IdleDetector *> m_timeouts;
};

}
