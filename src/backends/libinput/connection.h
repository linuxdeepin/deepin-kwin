/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwinglobals.h>

#include <KSharedConfig>

#include <QObject>
#include <QPointer>
#include <QMutex>
#include <QSize>
#include <QStringList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>
#include <deque>

#include <memory>

class QSocketNotifier;
class QThread;

namespace KWin
{

class Session;
class Udev;

namespace LibInput
{

class Event;
class Device;
class Context;
class ConnectionAdaptor;

class KWIN_EXPORT Connection : public QObject
{
    Q_OBJECT

public:
    ~Connection() override;

    void setInputConfig(const KSharedConfigPtr &config)
    {
        m_config = config;
    }

    Q_INVOKABLE void setTouchDeviceToScreenId(const QString &touchDeviceSysName, const QString &screenUuid);

    QString getTouchDeviceToScreenInfo();
    void setup();
    void updateScreens();
    void deactivate();
    void processEvents();

    QStringList devicesSysNames() const;

    static std::unique_ptr<Connection> create(Session *session);

Q_SIGNALS:
    void deviceAdded(KWin::LibInput::Device *);
    void deviceRemoved(KWin::LibInput::Device *);

    void eventsRead();

private Q_SLOTS:
    void slotKGlobalSettingsNotifyChange(int type, int arg);

public Q_SLOTS:
    void slotKvmEnablePointerChange(quint32 is_enable);
    void slotKvmEnableKeyboardChange(quint32 is_enable);

private:
    Connection(std::unique_ptr<Context> &&input);
    void handleEvent();
    void applyDeviceConfig(Device *device);
    void applyScreenToDevice(Device *device);
    void doSetup();
    std::unique_ptr<QSocketNotifier> m_notifier;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QMutex m_mutex;
#else
    QRecursiveMutex m_mutex;
#endif
    std::deque<std::unique_ptr<Event>> m_eventQueue;
    QVector<Device *> m_devices;
    KSharedConfigPtr m_config;
    std::unique_ptr<ConnectionAdaptor> m_connectionAdaptor;
    std::unique_ptr<Context> m_input;
    std::unique_ptr<Udev> m_udev;
    QMap<QString, QString> m_touchDeviceToScreenMap;
    bool m_kvmEnablePointer = true;
    bool m_kvmEnableKeyboard = true;
};

}
}
