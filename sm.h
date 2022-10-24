// Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
// Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SM_H
#define KWIN_SM_H

#include <QDataStream>
#include <kwinglobals.h>
#include <QStringList>
#include <netwm_def.h>
#include <QRect>

#include <X11/SM/SMlib.h>
#include <fixx11h.h>

class QSocketNotifier;

namespace KWin
{

class Client;

struct SessionInfo {
    QByteArray sessionId;
    QByteArray windowRole;
    QByteArray wmCommand;
    QByteArray wmClientMachine;
    QByteArray resourceName;
    QByteArray resourceClass;

    QRect geometry;
    QRect restore;
    QRect fsrestore;
    int maximized;
    int fullscreen;
    int desktop;
    bool minimized;
    bool onAllDesktops;
    bool shaded;
    bool keepAbove;
    bool keepBelow;
    bool skipTaskbar;
    bool skipPager;
    bool skipSwitcher;
    bool noBorder;
    NET::WindowType windowType;
    QString shortcut;
    bool active; // means 'was active in the saved session'
    int stackingOrder;
    float opacity;
    int tabGroup; // Unique identifier for the client group that this window is in

    Client* tabGroupClient; // The first client created that has an identical identifier
    QStringList activities;
};


enum SMSavePhase {
    SMSavePhase0,     // saving global state in "phase 0"
    SMSavePhase2,     // saving window state in phase 2
    SMSavePhase2Full  // complete saving in phase2, there was no phase 0
};

class KWIN_EXPORT SessionSaveDoneHelper
    : public QObject
{
    Q_OBJECT
public:
    SessionSaveDoneHelper();
    virtual ~SessionSaveDoneHelper();
    SmcConn connection() const {
        return conn;
    }
    void saveDone();
    void close();
private Q_SLOTS:
    void processData();
private:
    QSocketNotifier* notifier;
    SmcConn conn;
};

} // namespace

#endif
