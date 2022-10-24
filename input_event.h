// Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_INPUT_EVENT_H
#define KWIN_INPUT_EVENT_H
#include <QInputEvent>

namespace KWin
{

namespace LibInput
{
class Device;
}

class MouseEvent : public QMouseEvent
{
public:
    explicit MouseEvent(QEvent::Type type, const QPointF &pos, Qt::MouseButton button, Qt::MouseButtons buttons,
                        Qt::KeyboardModifiers modifiers, quint32 timestamp,
                        const QSizeF &delta, const QSizeF &deltaNonAccelerated, quint64 timestampMicroseconds,
                        LibInput::Device *device);

    QSizeF delta() const {
        return m_delta;
    }

    QSizeF deltaUnaccelerated() const {
        return m_deltaUnccelerated;
    }

    quint64 timestampMicroseconds() const {
        return m_timestampMicroseconds;
    }

    LibInput::Device *device() const {
        return m_device;
    }

    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const {
        return m_modifiersRelevantForShortcuts;
    }

    void setModifiersRelevantForGlobalShortcuts(const Qt::KeyboardModifiers &mods) {
        m_modifiersRelevantForShortcuts = mods;
    }

    quint32 nativeButton() const {
        return m_nativeButton;
    }

    void setNativeButton(quint32 button) {
        m_nativeButton = button;
    }

private:
    QSizeF m_delta;
    QSizeF m_deltaUnccelerated;
    quint64 m_timestampMicroseconds;
    LibInput::Device *m_device;
    Qt::KeyboardModifiers m_modifiersRelevantForShortcuts = Qt::KeyboardModifiers();
    quint32 m_nativeButton = 0;
};

class WheelEvent : public QWheelEvent
{
public:
    explicit WheelEvent(const QPointF &pos, qreal delta, Qt::Orientation orientation, Qt::MouseButtons buttons,
                        Qt::KeyboardModifiers modifiers, quint32 timestamp, LibInput::Device *device);

    LibInput::Device *device() const {
        return m_device;
    }

    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const {
        return m_modifiersRelevantForShortcuts;
    }

    void setModifiersRelevantForGlobalShortcuts(const Qt::KeyboardModifiers &mods) {
        m_modifiersRelevantForShortcuts = mods;
    }

private:
    LibInput::Device *m_device;
    Qt::KeyboardModifiers m_modifiersRelevantForShortcuts = Qt::KeyboardModifiers();
};

class KeyEvent : public QKeyEvent
{
public:
    explicit KeyEvent(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers, quint32 code, quint32 keysym,
                      const QString &text, bool autorepeat, quint32 timestamp, LibInput::Device *device);

    LibInput::Device *device() const {
        return m_device;
    }

    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const {
        return m_modifiersRelevantForShortcuts;
    }

    void setModifiersRelevantForGlobalShortcuts(const Qt::KeyboardModifiers &mods) {
        m_modifiersRelevantForShortcuts = mods;
    }

private:
    LibInput::Device *m_device;
    Qt::KeyboardModifiers m_modifiersRelevantForShortcuts = Qt::KeyboardModifiers();
};

class SwitchEvent : public QInputEvent
{
public:
    enum class State {
        Off,
        On
    };
    explicit SwitchEvent(State state, quint32 timestamp, quint64 timestampMicroseconds, LibInput::Device *device);

    State state() const {
        return m_state;
    }

    quint64 timestampMicroseconds() const {
        return m_timestampMicroseconds;
    }

    LibInput::Device *device() const {
        return m_device;
    }

private:
    State m_state;
    quint64 m_timestampMicroseconds;
    LibInput::Device *m_device;
};

}

#endif
