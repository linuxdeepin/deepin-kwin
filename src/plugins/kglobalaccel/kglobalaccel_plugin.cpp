/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kglobalaccel_plugin.h"

#include "input.h"

#include <QDebug>

KGlobalAccelImpl::KGlobalAccelImpl(QObject *parent)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    : KGlobalAccelInterfaceV2(parent)
#else
    : KGlobalAccelInterface(parent)
#endif
{
}

KGlobalAccelImpl::~KGlobalAccelImpl() = default;

bool KGlobalAccelImpl::grabKey(int key, bool grab)
{
    return true;
}

void KGlobalAccelImpl::setEnabled(bool enabled)
{
    if (m_shuttingDown) {
        return;
    }
    static KWin::InputRedirection *s_input = KWin::InputRedirection::self();
    if (!s_input) {
        qFatal("This plugin is intended to be used with KWin and this is not KWin, exiting now");
    } else {
        if (!m_inputDestroyedConnection) {
            m_inputDestroyedConnection = connect(s_input, &QObject::destroyed, this, [this] {
                m_shuttingDown = true;
            });
        }
    }
    s_input->registerGlobalAccel(enabled ? this : nullptr);
}

bool KGlobalAccelImpl::checkKeyPressed(int keyQt)
{
    return keyPressed(keyQt);
}

bool KGlobalAccelImpl::checkKeyReleased(int keyQt)
{
    return keyReleased(keyQt);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool KGlobalAccelImpl::checkPointerPressed(Qt::MouseButtons buttons)
{
    return pointerPressed(buttons);
}

bool KGlobalAccelImpl::checkAxisTriggered(int axis)
{
    return axisTriggered(axis);
}
#endif
