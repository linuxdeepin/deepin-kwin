// Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "orientation_sensor.h"
#include <orientationsensoradaptor.h>

#include <QOrientationSensor>
#include <QOrientationReading>

#include <KConfigGroup>
#include <KStatusNotifierItem>
#include <KLocalizedString>

namespace KWin
{

OrientationSensor::OrientationSensor(QObject *parent)
    : QObject(parent)
    , m_sensor(new QOrientationSensor(this))
{
    connect(m_sensor, &QOrientationSensor::readingChanged, this,
        [this] {
            auto toOrientation = [] (auto reading) {
                switch (reading->orientation()) {
                case QOrientationReading::Undefined:
                    return OrientationSensor::Orientation::Undefined;
                case QOrientationReading::TopUp:
                    return OrientationSensor::Orientation::TopUp;
                case QOrientationReading::TopDown:
                    return OrientationSensor::Orientation::TopDown;
                case QOrientationReading::LeftUp:
                    return OrientationSensor::Orientation::LeftUp;
                case QOrientationReading::RightUp:
                    return OrientationSensor::Orientation::RightUp;
                case QOrientationReading::FaceUp:
                    return OrientationSensor::Orientation::FaceUp;
                case QOrientationReading::FaceDown:
                    return OrientationSensor::Orientation::FaceDown;
                default:
                    Q_UNREACHABLE();
                }
            };
            const auto orientation = toOrientation(m_sensor->reading());
            if (m_orientation != orientation) {
                m_orientation = orientation;
                emit orientationChanged();
            }
        }
    );
    connect(m_sensor, &QOrientationSensor::activeChanged, this,
        [this] {
            if (!m_sni) {
                return;
            }
            if (m_sensor->isActive()) {
                m_sni->setToolTipTitle(i18n("Automatic screen rotation is enabled"));
            } else {
                m_sni->setToolTipTitle(i18n("Automatic screen rotation is disabled"));
            }
        }
    );
}

OrientationSensor::~OrientationSensor() = default;

void OrientationSensor::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    if (m_enabled) {
        loadConfig();
        setupStatusNotifier();
        m_adaptor = new OrientationSensorAdaptor(this);
    } else {
        delete m_sni;
        m_sni = nullptr;
        delete m_adaptor;
        m_adaptor = nullptr;
    }
    startStopSensor();
}

void OrientationSensor::loadConfig()
{
    if (!m_config) {
        return;
    }
    m_userEnabled = m_config->group("OrientationSensor").readEntry("Enabled", true);
}

void OrientationSensor::setupStatusNotifier()
{
    if (m_sni) {
        return;
    }
    m_sni = new KStatusNotifierItem(QStringLiteral("kwin-automatic-rotation"), this);
    m_sni->setStandardActionsEnabled(false);
    m_sni->setCategory(KStatusNotifierItem::Hardware);
    m_sni->setStatus(KStatusNotifierItem::Passive);
    m_sni->setTitle(i18n("Automatic Screen Rotation"));
    // TODO: proper icon with state
    m_sni->setIconByName(QStringLiteral("preferences-desktop-display"));
    // we start disabled, it gets updated when the sensor becomes active
    m_sni->setToolTipTitle(i18n("Automatic screen rotation is disabled"));
    connect(m_sni, &KStatusNotifierItem::activateRequested, this,
        [this] {
            m_userEnabled = !m_userEnabled;
            startStopSensor();
            emit userEnabledChanged(m_userEnabled);
        }
    );
}

void OrientationSensor::startStopSensor()
{
    if (m_enabled && m_userEnabled) {
        m_sensor->start();
    } else {
        m_sensor->stop();
    }
}

void OrientationSensor::setUserEnabled(bool enabled)
{
    if (m_userEnabled == enabled) {
        return;
    }
    m_userEnabled = enabled;
    if (m_config) {
        m_config->group("OrientationSensor").writeEntry("Enabled", m_userEnabled);
    }
    emit userEnabledChanged(m_userEnabled);
}

}
