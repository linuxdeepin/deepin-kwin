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
