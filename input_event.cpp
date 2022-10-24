// Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "input_event.h"

namespace KWin
{

MouseEvent::MouseEvent(QEvent::Type type, const QPointF &pos, Qt::MouseButton button,
                       Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers,
                       quint32 timestamp, const QSizeF &delta, const QSizeF &deltaNonAccelerated,
                       quint64 timestampMicroseconds, LibInput::Device *device)
            : QMouseEvent(type, pos, pos, button, buttons, modifiers)
            , m_delta(delta)
            , m_deltaUnccelerated(deltaNonAccelerated)
            , m_timestampMicroseconds(timestampMicroseconds)
            , m_device(device)
{
    setTimestamp(timestamp);
}

WheelEvent::WheelEvent(const QPointF &pos, qreal delta, Qt::Orientation orientation, Qt::MouseButtons buttons,
                        Qt::KeyboardModifiers modifiers, quint32 timestamp, LibInput::Device *device)
            : QWheelEvent(pos, pos, QPoint(), (orientation == Qt::Horizontal) ? QPoint(delta, 0) : QPoint(0, delta), delta, orientation, buttons, modifiers)
            , m_device(device)
{
    setTimestamp(timestamp);
}

KeyEvent::KeyEvent(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers, quint32 code, quint32 keysym,
                   const QString &text, bool autorepeat, quint32 timestamp, LibInput::Device *device)
         : QKeyEvent(type, key, modifiers, code, keysym, 0, text, autorepeat)
         , m_device(device)
{
    setTimestamp(timestamp);
}

SwitchEvent::SwitchEvent(State state, quint32 timestamp, quint64 timestampMicroseconds, LibInput::Device* device)
    : QInputEvent(QEvent::User)
    , m_state(state)
    , m_timestampMicroseconds(timestampMicroseconds)
    , m_device(device)
{
    setTimestamp(timestamp);
}

}
