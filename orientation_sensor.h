// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

#include <KSharedConfig>

#include <kwin_export.h>

class QOrientationSensor;
class OrientationSensorAdaptor;
class KStatusNotifierItem;

namespace KWin
{

class KWIN_EXPORT OrientationSensor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kwin.OrientationSensor")
    Q_PROPERTY(bool userEnabled READ isUserEnabled WRITE setUserEnabled NOTIFY userEnabledChanged)
public:
    explicit OrientationSensor(QObject *parent = nullptr);
    ~OrientationSensor();

    void setEnabled(bool enabled);

    /**
     * Just like QOrientationReading::Orientation,
     * copied to not leak the QSensors API into internal API.
     **/
    enum class Orientation {
        Undefined,
        TopUp,
        TopDown,
        LeftUp,
        RightUp,
        FaceUp,
        FaceDown
    };

    Orientation orientation() const {
        return m_orientation;
    }

    void setConfig(KSharedConfig::Ptr config) {
        m_config = config;
    }

    bool isUserEnabled() const {
        return m_userEnabled;
    }
    void setUserEnabled(bool enabled);

Q_SIGNALS:
    void orientationChanged();
    void userEnabledChanged(bool);

private:
    void setupStatusNotifier();
    void startStopSensor();
    void loadConfig();
    QOrientationSensor *m_sensor;
    bool m_enabled = false;
    bool m_userEnabled = true;
    Orientation m_orientation = Orientation::Undefined;
    KStatusNotifierItem *m_sni = nullptr;
    KSharedConfig::Ptr m_config;
    OrientationSensorAdaptor *m_adaptor = nullptr;

};

}
