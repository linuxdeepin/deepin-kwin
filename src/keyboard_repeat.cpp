/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "keyboard_repeat.h"
#include "keyboard_input.h"
#include "input_event.h"
#include "wayland_server.h"

#include <KWaylandServer/keyboard_interface.h>
#include <KWaylandServer/seat_interface.h>

#include <QTimer>

namespace KWin
{

KeyboardRepeat::KeyboardRepeat(Xkb *xkb)
    : QObject()
    , m_timer(new QTimer(this))
    , m_xkb(xkb)
{
    connect(m_timer, &QTimer::timeout, this, &KeyboardRepeat::handleKeyRepeat);
}

KeyboardRepeat::~KeyboardRepeat() = default;

void KeyboardRepeat::handleKeyRepeat()
{
    // TODO: don't depend on WaylandServer
    if (waylandServer()->seat()->keyboard()->keyRepeatRate() != 0) {
        m_timer->setInterval(1000 / waylandServer()->seat()->keyboard()->keyRepeatRate());
    }
    // TODO: better time
    // 电源按钮键值为116,屏蔽掉电源按钮的常规长按操作
    if (m_key != 116) {
        Q_EMIT keyRepeat(m_key, m_time);
    }
}

void KeyboardRepeat::keyEvent(KeyEvent *event)
{
    if (event->isAutoRepeat() && event->type() == QEvent::KeyPress) {
        return;
    }
    const quint32 key = event->nativeScanCode();
    if (event->type() == QEvent::KeyPress) {
        // TODO: don't get these values from WaylandServer
        if (m_xkb->shouldKeyRepeat(key) && waylandServer()->seat()->keyboard()->keyRepeatDelay() != 0) {
            m_timer->setInterval(waylandServer()->seat()->keyboard()->keyRepeatDelay());
            m_key = key;
            m_time = event->timestamp();
            m_timer->start();
        }
    } else if (event->type() == QEvent::KeyRelease) {
        if (key == m_key) {
            m_key = Qt::Key_unknown;
            m_timer->stop();
        }
    }
}

}
