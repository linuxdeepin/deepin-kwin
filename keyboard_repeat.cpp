// Copyright (C) 2016, 2017 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keyboard_repeat.h"
#include "keyboard_input.h"
#include "input_event.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Server/seat_interface.h>

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
    if (waylandServer()->seat()->keyRepeatRate() != 0) {
        m_timer->setInterval(1000 / waylandServer()->seat()->keyRepeatRate());
    }

    if (workspace() && workspace()->isKwinDebug()) {
        qDebug() << "emit keyRepeat key" << m_key;
    }
    // TODO: better time
    // 电源按钮键值为116,屏蔽掉电源按钮的常规长按操作
    if (m_key != 116) {
    	emit keyRepeat(m_key, m_time);
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
        if (m_xkb->shouldKeyRepeat(key) && waylandServer()->seat()->keyRepeatDelay() != 0) {
            m_timer->setInterval(waylandServer()->seat()->keyRepeatDelay());
            m_key = key;
            m_time = event->timestamp();
            m_timer->start();
            if (workspace() && workspace()->isKwinDebug()) {
                qDebug() << "start repeat timer key" << key;
            }
        }
    } else if (event->type() == QEvent::KeyRelease) {
        if (key == m_key) {
            m_key = Qt::Key_unknown;
            m_timer->stop();
            if (workspace() && workspace()->isKwinDebug()) {
                qDebug() << "stop repeat timer key" << key;
            }
        }
    }
}

}
