// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef MULTI_TOUCH_GESTURE_SPY_H
#define MULTI_TOUCH_GESTURE_SPY_H

#include "input_event_spy.h"

namespace KWin
{

class MultiTouchGestureSpy : public InputEventSpy
{
public:
    void touchDown(quint32 id, const QPointF &pos, quint32 time) override;
    void touchMotion(quint32 id, const QPointF &pos, quint32 time) override;
    void touchUp(quint32 id, quint32 time) override;
};

}

#endif // MULTI_TOUCH_GESTURE_SPY_H
