// Copyright (C) 2022 Uniontech Technology Co., Ltd.
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef RECORDEVENTMONITOR_H
#define RECORDEVENTMONITOR_H

#include <QObject>
#include <QThread>
#include <X11/Xlib.h>
#include <X11/extensions/record.h>

class RecordEventMonitor : public QThread
{
    Q_OBJECT
public:
    explicit RecordEventMonitor(QObject *parent = nullptr);
    void stopRecord();

signals:
    void buttonRelease();
    void motion();
    void touchDown(double x, double y, unsigned int time);
    void touchMotion(double x, double y, unsigned int time);
    void touchUp(unsigned int time);

public slots:

protected:
    static void callback(XPointer trash, XRecordInterceptData *data);
    void handleRecordEvent(XRecordInterceptData *);
    void run();
    bool m_bFlag = false;
    int m_eventTime = 0;
    XRecordContext m_context;
    Display *m_d0;

};

#endif // RECORDEVENTMONITOR_H
