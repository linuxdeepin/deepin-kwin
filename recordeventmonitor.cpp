// Copyright (C) 2022 Uniontech Technology Co., Ltd.
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "recordeventmonitor.h"
#include <X11/Xlibint.h>
#include "multitouchgesture.h"

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
    m_d0 = pDisplay;
    if (pDisplay == nullptr)
        return;

    XRecordClientSpec clients = XRecordAllClients;
    XRecordRange *pRange = XRecordAllocRange();
    if (pRange == nullptr)
        return;

    // In this, we just get touch event in a range.
    memset(pRange, 0, sizeof(XRecordRange));
    pRange->device_events.first = KeyPress;
    pRange->device_events.last  = TOUCHUP;

    XRecordContext context = XRecordCreateContext(pDisplay, 0, &clients, 1, &pRange, 1);
    m_context = context;
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
            if (m_eventTime != event->u.keyButtonPointer.time) {
                emit touchDown(event->u.keyButtonPointer.rootX, event->u.keyButtonPointer.rootY, event->u.keyButtonPointer.time);
                m_eventTime = event->u.keyButtonPointer.time;
            }
            break;
        case TOUCHMOTION:
            emit touchMotion(event->u.keyButtonPointer.rootX, event->u.keyButtonPointer.rootY, event->u.keyButtonPointer.time);
            break;
        case TOUCHUP:
            emit touchUp(event->u.keyButtonPointer.time);
            break;
        default:
            break;
        }
    }

    fflush(stdout);
    XRecordFreeData(data);
}

void RecordEventMonitor::stopRecord()
{
    XRecordDisableContext(m_d0, m_context);
    XRecordFreeContext(m_d0, m_context);
    XSync(m_d0, True);
    XCloseDisplay(m_d0);
}
