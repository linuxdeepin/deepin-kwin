// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RECORDEVENTMONITOR_H
#define RECORDEVENTMONITOR_H

#include <QObject>
#include <QThread>

class RecordEventMonitor : public QThread
{
    Q_OBJECT
public:
    explicit RecordEventMonitor(QObject *parent = nullptr);
    static RecordEventMonitor *instance();
    void stopRecord();

Q_SIGNALS:
    void touchDown();
    void touchMotion();
    void touchUp();

protected:
    void run();

private:
    static RecordEventMonitor *m_pRecordEventMonitor;
};

#endif // RECORDEVENTMONITOR_H
