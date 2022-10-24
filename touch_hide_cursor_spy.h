// Copyright (C) 2018 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "input_event_spy.h"

namespace KWin
{

class TouchHideCursorSpy : public InputEventSpy
{
public:
    void pointerEvent(KWin::MouseEvent *event) override;
    void wheelEvent(KWin::WheelEvent *event) override;
    void touchDown(quint32 id, const QPointF &pos, quint32 time) override;

private:
    void showCursor();
    void hideCursor();

    bool m_cursorHidden = false;
};

}
