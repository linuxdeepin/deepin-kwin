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
#include "recordeventmonitor.h"
#include <X11/Xlibint.h>

// The PROXIMITYIN and PROXIMITYOUT enumeration value is the same as the event enumeration value in the EventToCore function in xorg.
#define PROXIMITYIN     15
#define PROXIMITYOUT    16
# define TOUCHDOWN      (LASTEvent + 1)
# define TOUCHMOTION    (LASTEvent + 2)
# define TOUCHUP        (LASTEvent + 3)

RecordEventMonitor::RecordEventMonitor(QObject *parent) : QThread(parent)
{

}

void RecordEventMonitor::run()
{
    Display *pDisplay = XOpenDisplay(nullptr);
    if (pDisplay == nullptr)
        return;

    XRecordClientSpec clients = XRecordAllClients;
    XRecordRange *pRange = XRecordAllocRange();
    if (pRange == nullptr)
        return;

    // In this, we just get touch event in a range.
    memset(pRange, 0, sizeof(XRecordRange));
    pRange->device_events.first = TOUCHDOWN;
    pRange->device_events.last  = TOUCHUP;

    XRecordContext context = XRecordCreateContext(pDisplay, 0, &clients, 1, &pRange, 1);
    if (context == 0)
        return;

    XFree(pRange);
    XSync(pDisplay, True);

    Display* displayDatalink = XOpenDisplay(nullptr);
    if (displayDatalink == nullptr)
        return;

    if (!XRecordEnableContext(displayDatalink, context,  callback, (XPointer) this))
        return;
}

void RecordEventMonitor::callback(XPointer ptr, XRecordInterceptData *data)
{
    ((RecordEventMonitor *) ptr)->handleRecordEvent(data);
}

void RecordEventMonitor::handleRecordEvent(XRecordInterceptData* data)
{
    if (data->category == XRecordFromServer) {
        xEvent * event = (xEvent *)data->data;
        switch (event->u.u.type) {
        case ButtonRelease:
            if (m_bFlag) {
                emit buttonRelease();
            }
            break;
        case MotionNotify:
            if (m_bFlag) {
                emit motion();
            }
            break;
        case PROXIMITYIN:
            // When using a TabletTool device, this event is raised first
            m_bFlag = true;
            break;
        case PROXIMITYOUT:
            // This event is raised when the TabletTool device is left.
            m_bFlag = false;
            break;
        case TOUCHDOWN:
            // sometimes, xrecord extend will get repeated touch down event(maybe send to the real client).
            // but, those touch down event is belong to same ancestors.
            // so, if you need, you can use (event->u.keyButtonPointer.time)
            // to filter out the repeated touh down event.
            if (m_evnetTime != event->u.keyButtonPointer.time) {
                emit touchDown();
                m_evnetTime = event->u.keyButtonPointer.time;
            }
            break;
        case TOUCHMOTION:
            emit touchMotion();
            break;
        case TOUCHUP:
            emit touchUp();
            break;
        default:
            break;
        }
    }

    fflush(stdout);
    XRecordFreeData(data);
}
