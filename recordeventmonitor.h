/*
 * Copyright (C) 2022 Uniontech Technology Co., Ltd.
 *
 * Author:     xinbo wang <wangxinbo@uniontech.com>
 *
 * Maintainer: xinbo wang <wangxinbo@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
