// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "multi_touch_gesture_spy.h"
#include <QPointF>
#include "multitouchgesture.h"

namespace KWin
{

void MultiTouchGestureSpy::touchDown(quint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    MultiTouchGesture::instance()->handleTouchDown(pos.x(), pos.y(), time);
}

void MultiTouchGestureSpy::touchMotion(quint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    MultiTouchGesture::instance()->handleTouchMotion(pos.x(), pos.y(), time);
}

void MultiTouchGestureSpy::touchUp(quint32 id, quint32 time)
{
    Q_UNUSED(id)
    MultiTouchGesture::instance()->handleTouchUp(time);
}

}
