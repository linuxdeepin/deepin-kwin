/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright 2018 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "abstract_output.h"
#include "wayland_server.h"
#include "screens.h"

// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/outputchangeset.h>
#include <KWayland/Server/xdgoutput_interface.h>
// KF5
#include <KLocalizedString>

#include <cmath>

namespace KWin
{

AbstractOutput::AbstractOutput(QObject *parent)
    : QObject(parent)
{
}

AbstractOutput::~AbstractOutput()
{
    delete m_waylandOutputDevice.data();
    delete m_xdgOutput.data();
    delete m_waylandOutput.data();
}

QString AbstractOutput::name() const
{
    if (!m_waylandOutput) {
        return i18n("unknown");
    }
    return QStringLiteral("%1 %2").arg(m_waylandOutput->manufacturer()).arg(m_waylandOutput->model());
}

QRect AbstractOutput::geometry() const
{
    return QRect(m_globalPos, pixelSize() / scale());
}

QSize AbstractOutput::physicalSize() const
{
    return orientateSize(m_physicalSize);
}

int AbstractOutput::refreshRate() const
{
    if (!m_waylandOutput) {
        return 60000;
    }
    return m_waylandOutput->refreshRate();
}

void AbstractOutput::setGlobalPos(const QPoint &pos)
{
    m_globalPos = pos;
    if (m_waylandOutput) {
        m_waylandOutput->setGlobalPosition(pos);
    }
    if (m_waylandOutputDevice) {
        m_waylandOutputDevice->setGlobalPosition(pos);
    }
    if (m_xdgOutput) {
        m_xdgOutput->setLogicalPosition(pos);
        m_xdgOutput->done();
    }
}

void AbstractOutput::setScale(qreal scale)
{
    m_scale = scale;
    if (m_waylandOutput) {
        // this is the scale that clients will ideally use for their buffers
        // this has to be an int which is fine

        // I don't know whether we want to round or ceil
        // or maybe even set this to 3 when we're scaling to 1.5
        // don't treat this like it's chosen deliberately
        m_waylandOutput->setScale(std::ceil(scale));
    }
    if (m_waylandOutputDevice) {
        m_waylandOutputDevice->setScaleF(scale);
    }
    if (m_xdgOutput) {
        m_xdgOutput->setLogicalSize(pixelSize() / m_scale);
        m_xdgOutput->done();
    }
}

void AbstractOutput::setChanges(KWayland::Server::OutputChangeSet *changes)
{
    qCDebug(KWIN_CORE) << "Set changes in AbstractOutput.";
    Q_ASSERT(!m_waylandOutputDevice.isNull());

    bool updated = false;
    bool overallSizeCheckNeeded = false;

    if (!changes) {
        qCDebug(KWIN_CORE) << "No changes.";
        // No changes to an output is an entirely valid thing
    }
    //enabledChanged is handled by plugin code
    if (changes->modeChanged()) {
        qCDebug(KWIN_CORE) << "Setting new mode:" << changes->mode();
        m_waylandOutputDevice->setCurrentMode(changes->mode());
        updateMode(changes->mode());
        updated = true;
    }
    if (changes->transformChanged()) {
        qCDebug(KWIN_CORE) << "Server setting transform: " << (int)(changes->transform());
        transform(changes->transform());
        updated = true;
    }
    if (changes->positionChanged()) {
        qCDebug(KWIN_CORE) << "Server setting position: " << changes->position();
        setGlobalPos(changes->position());
        // may just work already!
        overallSizeCheckNeeded = true;
    }
    if (changes->scaleChanged()) {
        qCDebug(KWIN_CORE) << "Setting scale:" << changes->scale();
        setScale(changes->scaleF());
        updated = true;
    }

    overallSizeCheckNeeded |= updated;
    if (overallSizeCheckNeeded) {
        emit screens()->changed();
    }

    if (updated) {
        emit modeChanged();
    }
}

void AbstractOutput::setEnabled(bool enable)
{
    if (enable == isEnabled()) {
        return;
    }
    if (enable) {
        updateDpms(KWayland::Server::OutputInterface::DpmsMode::On);
        initWaylandOutput();
    } else {
        updateDpms(KWayland::Server::OutputInterface::DpmsMode::Off);
        delete waylandOutput().data();
    }
    waylandOutputDevice()->setEnabled(enable ? KWayland::Server::OutputDeviceInterface::Enablement::Enabled :
                                               KWayland::Server::OutputDeviceInterface::Enablement::Disabled);
}

void AbstractOutput::setWaylandMode(const QSize &size, int refreshRate)
{
    if (m_waylandOutput.isNull()) {
        return;
    }
    m_waylandOutput->setCurrentMode(size, refreshRate);
    if (m_xdgOutput) {
        m_xdgOutput->setLogicalSize(pixelSize() / scale());
        m_xdgOutput->done();
    }
}

void AbstractOutput::createXdgOutput()
{
    if (!m_waylandOutput || m_xdgOutput) {
        return;
    }
    m_xdgOutput = waylandServer()->xdgOutputManager()->createXdgOutput(m_waylandOutput, m_waylandOutput);
}

void AbstractOutput::initWaylandOutput()
{
    Q_ASSERT(m_waylandOutputDevice);

    if (!m_waylandOutput.isNull()) {
        delete m_waylandOutput.data();
        m_waylandOutput.clear();
    }
    m_waylandOutput = waylandServer()->display()->createOutput();
    createXdgOutput();

    /*
     *  add base wayland output data
     */
    m_waylandOutput->setManufacturer(m_waylandOutputDevice->manufacturer());
    m_waylandOutput->setModel(m_waylandOutputDevice->model());
    m_waylandOutput->setPhysicalSize(rawPhysicalSize());

    /*
     *  add modes
     */
    for(const auto &mode: m_waylandOutputDevice->modes()) {
        KWayland::Server::OutputInterface::ModeFlags flags;
        if (mode.flags & KWayland::Server::OutputDeviceInterface::ModeFlag::Current) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Current;
        }
        if (mode.flags & KWayland::Server::OutputDeviceInterface::ModeFlag::Preferred) {
            flags |= KWayland::Server::OutputInterface::ModeFlag::Preferred;
        }
        m_waylandOutput->addMode(mode.size, flags, mode.refreshRate);
    }
    m_waylandOutput->create();

    /*
     *  set dpms
     */
    m_waylandOutput->setDpmsSupported(m_supportsDpms);
    // set to last known mode
    m_waylandOutput->setDpmsMode(m_dpms);
    connect(m_waylandOutput.data(), &KWayland::Server::OutputInterface::dpmsModeRequested, this,
        [this] (KWayland::Server::OutputInterface::DpmsMode mode) {
            updateDpms(mode);
        }, Qt::QueuedConnection
    );
}

void AbstractOutput::initWaylandOutputDevice(const QString &model,
                                             const QString &manufacturer,
                                             const QByteArray &uuid,
                                             const QVector<KWayland::Server::OutputDeviceInterface::Mode> &modes)
{
    if (!m_waylandOutputDevice.isNull()) {
        delete m_waylandOutputDevice.data();
        m_waylandOutputDevice.clear();
    }
    m_waylandOutputDevice = waylandServer()->display()->createOutputDevice();
    m_waylandOutputDevice->setUuid(uuid);

    if (!manufacturer.isEmpty()) {
        m_waylandOutputDevice->setManufacturer(manufacturer);
    } else {
        m_waylandOutputDevice->setManufacturer(i18n("unknown"));
    }

    m_waylandOutputDevice->setModel(model);
    m_waylandOutputDevice->setPhysicalSize(m_physicalSize);

    int i = 0;
    for (auto mode : modes) {
        QString flags;
        if (mode.flags & KWayland::Server::OutputDeviceInterface::ModeFlag::Preferred) {
            flags += " preferred";
        }
        if (mode.flags & KWayland::Server::OutputDeviceInterface::ModeFlag::Current) {
            flags += " current";
        }
        qCDebug(KWIN_CORE).nospace() << "Adding mode " << ++i << ": " << mode.size << " [" << mode.refreshRate << "]" << flags;
        m_waylandOutputDevice->addMode(mode);
    }
    m_waylandOutputDevice->create();
}

QSize AbstractOutput::orientateSize(const QSize &size) const
{
    if (m_orientation == Qt::PortraitOrientation || m_orientation == Qt::InvertedPortraitOrientation) {
        return size.transposed();
    }
    return size;
}

}
