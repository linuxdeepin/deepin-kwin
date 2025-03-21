/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <kglobalaccel_interface.h>
#else
#include <KGlobalAccel/private/kglobalaccel_interface.h>
#endif

#include <QObject>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
class KGlobalAccelImpl : public KGlobalAccelInterfaceV2
#else
class KGlobalAccelImpl : public KGlobalAccelInterface
#endif
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID KGlobalAccelInterface_iid FILE "kwin.json")
    Q_INTERFACES(KGlobalAccelInterface)

public:
    KGlobalAccelImpl(QObject *parent = nullptr);
    ~KGlobalAccelImpl() override;

    bool grabKey(int key, bool grab) override;
    void setEnabled(bool) override;

public Q_SLOTS:
    bool checkKeyPressed(int keyQt);
    bool checkKeyReleased(int keyQt);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool checkPointerPressed(Qt::MouseButtons buttons);
    bool checkAxisTriggered(int axis);
#endif

private:
    bool m_shuttingDown = false;
    QMetaObject::Connection m_inputDestroyedConnection;
};
