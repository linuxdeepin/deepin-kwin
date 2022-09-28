/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "drm_output.h"
#include "drm_backend.h"
#include "drm_object_plane.h"
#include "drm_object_crtc.h"
#include "drm_object_connector.h"
/*
 * generated from mutter's gen-default-modes.py
 */
#include "drm_default_modes.h"

#include <errno.h>

#include "composite.h"
#include "logind.h"
#include "logging.h"
#include "main.h"
#include "orientation_sensor.h"
#include "screens_drm.h"
#include "wayland_server.h"
// KWayland
#include <KWayland/Server/output_interface.h>
// KF5
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
// Qt
#include <QMatrix4x4>
#include <QCryptographicHash>
#include <QPainter>
// drm
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>

namespace KWin
{
DrmOutput::DrmOutput(DrmBackend *backend)
    : AbstractOutput(backend)
    , m_backend(backend)
{
}

DrmOutput::~DrmOutput()
{
    Q_ASSERT(!m_pageFlipPending);
    if (!m_deleted) {
        teardown();
    }
}

void DrmOutput::teardown()
{
    m_deleted = true;
    hideCursor();
    m_crtc->blank();

    if (m_primaryPlane) {
        // TODO: when having multiple planes, also clean up these
        m_primaryPlane->setOutput(nullptr);

        if (m_backend->deleteBufferAfterPageFlip()) {
            delete m_primaryPlane->current();
        }
        m_primaryPlane->setCurrent(nullptr);
    }

    m_crtc->setOutput(nullptr);
    m_conn->setOutput(nullptr);

    delete m_cursor[0];
    delete m_cursor[1];
    if (!m_pageFlipPending) {
        deleteLater();
    } //else will be deleted in the page flip handler
    //this is needed so that the pageflipcallback handle isn't deleted
}

void DrmOutput::releaseGbm()
{
    if (DrmBuffer *b = m_crtc->current()) {
        b->releaseGbm();
    }
    if (m_primaryPlane && m_primaryPlane->current()) {
        m_primaryPlane->current()->releaseGbm();
    }
}

bool DrmOutput::hideCursor()
{
    return drmModeSetCursor(m_backend->fd(), m_crtc->id(), 0, 0, 0) == 0;
}

bool DrmOutput::showCursor(DrmDumbBuffer *c)
{
    if (!c) return false;
    const QSize &s = c->size();
    return drmModeSetCursor(m_backend->fd(), m_crtc->id(), c->handle(), s.width(), s.height()) == 0;
}

bool DrmOutput::showCursor()
{
    const bool ret = showCursor(m_cursor[m_cursorIndex]);
    if (!ret) {
        return ret;
    }

    if (m_hasNewCursor) {
        m_cursorIndex = (m_cursorIndex + 1) % 2;
        m_hasNewCursor = false;
    }

    return ret;
}

void DrmOutput::updateCursor()
{
    QImage cursorImage = m_backend->softwareCursor();
    if (cursorImage.isNull()) {
        return;
    }
    m_hasNewCursor = true;
    QImage *c = m_cursor[m_cursorIndex]->image();
    if (!c) return;

    c->fill(Qt::transparent);
    c->setDevicePixelRatio(scale());

    QPainter p;
    p.begin(c);
    if (orientation() == Qt::InvertedLandscapeOrientation) {
        QMatrix4x4 matrix;
        matrix.translate(cursorImage.width() / 2.0, cursorImage.height() / 2.0);
        matrix.rotate(180.0f, 0.0f, 0.0f, 1.0f);
        matrix.translate(-cursorImage.width() / 2.0, -cursorImage.height() / 2.0);
        p.setWorldTransform(matrix.toTransform());
    }
    p.drawImage(QPoint(0, 0), cursorImage);
    p.end();
}

void DrmOutput::moveCursor(const QPoint &globalPos)
{
    QMatrix4x4 matrix;
    QMatrix4x4 hotspotMatrix;
    if (orientation() == Qt::InvertedLandscapeOrientation) {
        matrix.translate(pixelSize().width() /2.0, pixelSize().height() / 2.0);
        matrix.rotate(180.0f, 0.0f, 0.0f, 1.0f);
        matrix.translate(-pixelSize().width() /2.0, -pixelSize().height() / 2.0);
        const auto cursorSize = m_backend->softwareCursor().size();
        hotspotMatrix.translate(cursorSize.width()/2.0, cursorSize.height()/2.0);
        hotspotMatrix.rotate(180.0f, 0.0f, 0.0f, 1.0f);
        hotspotMatrix.translate(-cursorSize.width()/2.0, -cursorSize.height()/2.0);
    }
    hotspotMatrix.scale(scale());
    matrix.scale(scale());
    const auto outputGlobalPos = AbstractOutput::globalPos();
    matrix.translate(-outputGlobalPos.x(), -outputGlobalPos.y());
    const QPoint p = matrix.map(globalPos) - hotspotMatrix.map(m_backend->softwareCursorHotspot());
    drmModeMoveCursor(m_backend->fd(), m_crtc->id(), p.x(), p.y());
}

static QHash<int, QByteArray> s_connectorNames = {
    {DRM_MODE_CONNECTOR_Unknown, QByteArrayLiteral("Unknown")},
    {DRM_MODE_CONNECTOR_VGA, QByteArrayLiteral("VGA")},
    {DRM_MODE_CONNECTOR_DVII, QByteArrayLiteral("DVI-I")},
    {DRM_MODE_CONNECTOR_DVID, QByteArrayLiteral("DVI-D")},
    {DRM_MODE_CONNECTOR_DVIA, QByteArrayLiteral("DVI-A")},
    {DRM_MODE_CONNECTOR_Composite, QByteArrayLiteral("Composite")},
    {DRM_MODE_CONNECTOR_SVIDEO, QByteArrayLiteral("SVIDEO")},
    {DRM_MODE_CONNECTOR_LVDS, QByteArrayLiteral("LVDS")},
    {DRM_MODE_CONNECTOR_Component, QByteArrayLiteral("Component")},
    {DRM_MODE_CONNECTOR_9PinDIN, QByteArrayLiteral("DIN")},
    {DRM_MODE_CONNECTOR_DisplayPort, QByteArrayLiteral("DP")},
    {DRM_MODE_CONNECTOR_HDMIA, QByteArrayLiteral("HDMI-A")},
    {DRM_MODE_CONNECTOR_HDMIB, QByteArrayLiteral("HDMI-B")},
    {DRM_MODE_CONNECTOR_TV, QByteArrayLiteral("TV")},
    {DRM_MODE_CONNECTOR_eDP, QByteArrayLiteral("eDP")},
    {DRM_MODE_CONNECTOR_VIRTUAL, QByteArrayLiteral("Virtual")},
    {DRM_MODE_CONNECTOR_DSI, QByteArrayLiteral("DSI")}
};

namespace {
quint64 refreshRateForMode(_drmModeModeInfo *m)
{
    // Calculate higher precision (mHz) refresh rate
    // logic based on Weston, see compositor-drm.c
    quint64 refreshRate = (m->clock * 1000000LL / m->htotal + m->vtotal / 2) / m->vtotal;
    if (m->flags & DRM_MODE_FLAG_INTERLACE) {
        refreshRate *= 2;
    }
    if (m->flags & DRM_MODE_FLAG_DBLSCAN) {
        refreshRate /= 2;
    }
    if (m->vscan > 1) {
        refreshRate /= m->vscan;
    }
    return refreshRate;
}
}

bool DrmOutput::init(drmModeConnector *connector)
{
    initEdid(connector);
    initDpms(connector);
    initScaling(connector);
    initUuid();
    if (m_backend->atomicModeSetting()) {
        if (!initPrimaryPlane()) {
            return false;
        }
    } else if (!m_crtc->blank()) {
        return false;
    }

    setInternal(connector->connector_type == DRM_MODE_CONNECTOR_LVDS
            || connector->connector_type == DRM_MODE_CONNECTOR_eDP
            || connector->connector_type == DRM_MODE_CONNECTOR_DSI);
    setDpmsSupported(true);

    if (internal()) {
        connect(kwinApp(), &Application::screensCreated, this,
            [this] {
                connect(screens()->orientationSensor(), &OrientationSensor::orientationChanged, this, &DrmOutput::automaticRotation);
            }
        );
    }

    QSize physicalSize = !m_edid.physicalSize.isEmpty() ? m_edid.physicalSize : QSize(connector->mmWidth, connector->mmHeight);
    // the size might be completely borked. E.g. Samsung SyncMaster 2494HS reports 160x90 while in truth it's 520x292
    // as this information is used to calculate DPI info, it's going to result in everything being huge
    const QByteArray unknown = QByteArrayLiteral("unknown");
    KConfigGroup group = kwinApp()->config()->group("EdidOverwrite").group(m_edid.eisaId.isEmpty() ? unknown : m_edid.eisaId)
                                                       .group(m_edid.monitorName.isEmpty() ? unknown : m_edid.monitorName)
                                                       .group(m_edid.serialNumber.isEmpty() ? unknown : m_edid.serialNumber);
    if (group.hasKey("PhysicalSize")) {
        const QSize overwriteSize = group.readEntry("PhysicalSize", physicalSize);
        qCWarning(KWIN_DRM) << "Overwriting monitor physical size for" << m_edid.eisaId << "/" << m_edid.monitorName << "/" << m_edid.serialNumber << " from " << physicalSize << "to " << overwriteSize;
        physicalSize = overwriteSize;
    }
    setRawPhysicalSize(physicalSize);

    initOutputDevice(connector);

    setEnabled(true);
    return true;
}

void DrmOutput::initUuid()
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(QByteArray::number(m_conn->id()));
    hash.addData(m_edid.eisaId);
    hash.addData(m_edid.monitorName);
    hash.addData(m_edid.serialNumber);
    m_uuid = hash.result().toHex().left(10);
}

void DrmOutput::initOutputDevice(drmModeConnector *connector)
{
    QString manufacturer;
    if (!m_edid.eisaId.isEmpty()) {
        manufacturer = QString::fromLatin1(m_edid.eisaId);
    }

    QString connectorName = s_connectorNames.value(connector->connector_type, QByteArrayLiteral("Unknown"));
    QString modelName;

    if (!m_edid.monitorName.isEmpty()) {
        QString m = QString::fromLatin1(m_edid.monitorName);
        if (!m_edid.serialNumber.isEmpty()) {
            m.append('/');
            m.append(QString::fromLatin1(m_edid.serialNumber));
        }
        modelName = m;
    } else if (!m_edid.serialNumber.isEmpty()) {
        modelName = QString::fromLatin1(m_edid.serialNumber);
    } else {
        modelName = i18n("unknown");
    }

    const QString model = connectorName + QStringLiteral("-") + QString::number(connector->connector_type_id) + QStringLiteral("-") + modelName;

    // read in mode information
    QVector<KWayland::Server::OutputDeviceInterface::Mode> modes;
    for (int i = 0; i < connector->count_modes; ++i) {
        // TODO: in AMS here we could read and store for later every mode's blob_id
        // would simplify isCurrentMode(..) and presentAtomically(..) in case of mode set
        auto *m = &connector->modes[i];
        KWayland::Server::OutputDeviceInterface::ModeFlags deviceflags;
        if (isCurrentMode(m)) {
            deviceflags |= KWayland::Server::OutputDeviceInterface::ModeFlag::Current;
        }
        if (m->type & DRM_MODE_TYPE_PREFERRED) {
            deviceflags |= KWayland::Server::OutputDeviceInterface::ModeFlag::Preferred;
        }

        KWayland::Server::OutputDeviceInterface::Mode mode;
        mode.id = i;
        mode.size = QSize(m->hdisplay, m->vdisplay);
        mode.flags = deviceflags;
        mode.refreshRate = refreshRateForMode(m);
        modes << mode;
    }

    // if hardware support upscaling and internal panel only presents one physical mode,
    // we extend the list with some default modes
    if (isInternal() && modes.size() == 1 && m_scalingCapable) {
        auto& default_mode = modes[0];
        if (!(default_mode.flags & KWayland::Server::OutputDeviceInterface::ModeFlag::Preferred)) {
            default_mode.flags = KWayland::Server::OutputDeviceInterface::ModeFlag::Preferred;
        }
        bool landscape = default_mode.size.width() > default_mode.size.height();
        const drmModeModeInfo* drm_modes;
        if (landscape) {
            drm_modes = &s_default_landscape_drm_mode_infos[0];
        } else {
            drm_modes = &s_default_portrait_drm_mode_infos[0];
        }
        int sz = sizeof(s_default_landscape_drm_mode_infos)/sizeof(drm_modes[0]);

        int modeid = 1;
        for (int i = 0; i < sz; ++i) {
            auto drm_mode = drm_modes[i];
            if (drm_modes[i].hdisplay > default_mode.size.width() ||
                    drm_modes[i].vdisplay > default_mode.size.height() ||
                    refreshRateForMode(const_cast<drmModeModeInfo*>(drm_modes+i)) > default_mode.refreshRate)
                continue;

            KWayland::Server::OutputDeviceInterface::ModeFlags deviceflags;
            if (isCurrentMode(&drm_mode)) {
                deviceflags |= KWayland::Server::OutputDeviceInterface::ModeFlag::Current;
            }
            if (drm_mode.type & DRM_MODE_TYPE_PREFERRED) {
                deviceflags |= KWayland::Server::OutputDeviceInterface::ModeFlag::Preferred;
            }

            KWayland::Server::OutputDeviceInterface::Mode mode;
            mode.id = modeid++;
            mode.size = QSize(drm_mode.hdisplay, drm_mode.vdisplay);
            mode.flags = deviceflags;
            mode.refreshRate = refreshRateForMode(&drm_mode);
            modes << mode;
        }
    }

    AbstractOutput::initWaylandOutputDevice(model, manufacturer, m_uuid, modes);
}

bool DrmOutput::isCurrentMode(const drmModeModeInfo *mode) const
{
    return mode->clock       == m_mode.clock
        && mode->hdisplay    == m_mode.hdisplay
        && mode->hsync_start == m_mode.hsync_start
        && mode->hsync_end   == m_mode.hsync_end
        && mode->htotal      == m_mode.htotal
        && mode->hskew       == m_mode.hskew
        && mode->vdisplay    == m_mode.vdisplay
        && mode->vsync_start == m_mode.vsync_start
        && mode->vsync_end   == m_mode.vsync_end
        && mode->vtotal      == m_mode.vtotal
        && mode->vscan       == m_mode.vscan
        && mode->vrefresh    == m_mode.vrefresh
        && mode->flags       == m_mode.flags
        && mode->type        == m_mode.type
        && qstrcmp(mode->name, m_mode.name) == 0;
}

static bool verifyEdidHeader(drmModePropertyBlobPtr edid)
{
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    if (data[0] != 0x00) {
        return false;
    }
    for (int i = 1; i < 7; ++i) {
        if (data[i] != 0xFF) {
            return false;
        }
    }
    if (data[7] != 0x00) {
        return false;
    }
    return true;
}

static QByteArray extractEisaId(drmModePropertyBlobPtr edid)
{
    /*
     * From EDID standard section 3.4:
     * The ID Manufacturer Name field, shown in Table 3.5, contains a 2-byte representation of the monitor's
     * manufacturer. This is the same as the EISA ID. It is based on compressed ASCII, “0001=A” ... “11010=Z”.
     *
     * The table:
     * | Byte |        Bit                    |
     * |      | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
     * ----------------------------------------
     * |  1   | 0)| (4| 3 | 2 | 1 | 0)| (4| 3 |
     * |      | * |    Character 1    | Char 2|
     * ----------------------------------------
     * |  2   | 2 | 1 | 0)| (4| 3 | 2 | 1 | 0)|
     * |      | Character2|      Character 3  |
     * ----------------------------------------
     **/
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    static const uint offset = 0x8;
    char id[4];
    if (data[offset] >> 7) {
        // bit at position 7 is not a 0
        return QByteArray();
    }
    // shift two bits to right, and with 7 right most bits
    id[0] = 'A' + ((data[offset] >> 2) & 0x1f) -1;
    // for first byte: take last two bits and shift them 3 to left (000xx000)
    // for second byte: shift 5 bits to right and take 3 right most bits (00000xxx)
    // or both together
    id[1] = 'A' + (((data[offset] & 0x3) << 3) | ((data[offset + 1] >> 5) & 0x7)) - 1;
    // take five right most bits
    id[2] = 'A' + (data[offset + 1] & 0x1f) - 1;
    id[3] = '\0';
    return QByteArray(id);
}

static void extractMonitorDescriptorDescription(drmModePropertyBlobPtr blob, DrmOutput::Edid &edid)
{
    // see section 3.10.3
    const uint8_t *data = reinterpret_cast<uint8_t*>(blob->data);
    static const uint offset = 0x36;
    static const uint blockLength = 18;
    for (int i = 0; i < 5; ++i) {
        const uint co = offset + i * blockLength;
        // Flag = 0000h when block used as descriptor
        if (data[co] != 0) {
            continue;
        }
        if (data[co + 1] != 0) {
            continue;
        }
        // Reserved = 00h when block used as descriptor
        if (data[co + 2] != 0) {
            continue;
        }
        /*
         * FFh: Monitor Serial Number - Stored as ASCII, code page # 437, ≤ 13 bytes.
         * FEh: ASCII String - Stored as ASCII, code page # 437, ≤ 13 bytes.
         * FDh: Monitor range limits, binary coded
         * FCh: Monitor name, stored as ASCII, code page # 437
         * FBh: Descriptor contains additional color point data
         * FAh: Descriptor contains additional Standard Timing Identifications
         * F9h - 11h: Currently undefined
         * 10h: Dummy descriptor, used to indicate that the descriptor space is unused
         * 0Fh - 00h: Descriptor defined by manufacturer.
         */
        if (data[co + 3] == 0xfc && edid.monitorName.isEmpty()) {
            edid.monitorName = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
        }
        if (data[co + 3] == 0xfe) {
            const QByteArray id = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
            if (!id.isEmpty()) {
                edid.eisaId = id;
            }
        }
        if (data[co + 3] == 0xff) {
            edid.serialNumber = QByteArray((const char *)(&data[co + 5]), 12).trimmed();
        }
    }
}

static QByteArray extractSerialNumber(drmModePropertyBlobPtr edid)
{
    // see section 3.4
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    static const uint offset = 0x0C;
    /*
     * The ID serial number is a 32-bit serial number used to differentiate between individual instances of the same model
     * of monitor. Its use is optional. When used, the bit order for this field follows that shown in Table 3.6. The EDID
     * structure Version 1 Revision 1 and later offer a way to represent the serial number of the monitor as an ASCII string
     * in a separate descriptor block.
     */
    uint32_t serialNumber = 0;
    serialNumber  = (uint32_t) data[offset + 0];
    serialNumber |= (uint32_t) data[offset + 1] << 8;
    serialNumber |= (uint32_t) data[offset + 2] << 16;
    serialNumber |= (uint32_t) data[offset + 3] << 24;
    if (serialNumber == 0) {
        return QByteArray();
    }
    return QByteArray::number(serialNumber);
}

static QSize extractPhysicalSize(drmModePropertyBlobPtr edid)
{
    const uint8_t *data = reinterpret_cast<uint8_t*>(edid->data);
    return QSize(data[0x15], data[0x16]) * 10;
}

void DrmOutput::initEdid(drmModeConnector *connector)
{
    ScopedDrmPointer<_drmModePropertyBlob, &drmModeFreePropertyBlob> edid;
    for (int i = 0; i < connector->count_props; ++i) {
        ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> property(drmModeGetProperty(m_backend->fd(), connector->props[i]));
        if (!property) {
            continue;
        }
        if ((property->flags & DRM_MODE_PROP_BLOB) && qstrcmp(property->name, "EDID") == 0) {
            edid.reset(drmModeGetPropertyBlob(m_backend->fd(), connector->prop_values[i]));
        }
    }
    if (!edid) {
        return;
    }

    // for documentation see: http://read.pudn.com/downloads110/ebook/456020/E-EDID%20Standard.pdf
    if (edid->length < 128) {
        return;
    }
    if (!verifyEdidHeader(edid.data())) {
        return;
    }
    m_edid.eisaId = extractEisaId(edid.data());
    m_edid.serialNumber = extractSerialNumber(edid.data());

    // parse monitor descriptor description
    extractMonitorDescriptorDescription(edid.data(), m_edid);

    m_edid.physicalSize = extractPhysicalSize(edid.data());
}

bool DrmOutput::initPrimaryPlane()
{
    for (int i = 0; i < m_backend->planes().size(); ++i) {
        DrmPlane* p = m_backend->planes()[i];
        if (!p) {
            continue;
        }
        if (p->type() != DrmPlane::TypeIndex::Primary) {
            continue;
        }
        if (p->output()) {     // Plane already has an output
            continue;
        }
        if (m_primaryPlane) {     // Output already has a primary plane
            continue;
        }
        if (!p->isCrtcSupported(m_crtc->resIndex())) {
            continue;
        }
        p->setOutput(this);
        m_primaryPlane = p;
        qCDebug(KWIN_DRM) << "Initialized primary plane" << p->id() << "on CRTC" << m_crtc->id();
        return true;
    }
    qCCritical(KWIN_DRM) << "Failed to initialize primary plane.";
    return false;
}

bool DrmOutput::initCursorPlane()       // TODO: Add call in init (but needs layer support in general first)
{
    for (int i = 0; i < m_backend->planes().size(); ++i) {
        DrmPlane* p = m_backend->planes()[i];
        if (!p) {
            continue;
        }
        if (p->type() != DrmPlane::TypeIndex::Cursor) {
            continue;
        }
        if (p->output()) {     // Plane already has an output
            continue;
        }
        if (m_cursorPlane) {     // Output already has a cursor plane
            continue;
        }
        if (!p->isCrtcSupported(m_crtc->resIndex())) {
            continue;
        }
        p->setOutput(this);
        m_cursorPlane = p;
        qCDebug(KWIN_DRM) << "Initialized cursor plane" << p->id() << "on CRTC" << m_crtc->id();
        return true;
    }
    return false;
}

bool DrmOutput::initCursor(const QSize &cursorSize)
{
    auto createCursor = [this, cursorSize] (int index) {
        m_cursor[index] = m_backend->createBuffer(cursorSize);
        if (!m_cursor[index]->map(QImage::Format_ARGB32_Premultiplied)) {
            return false;
        }
        return true;
    };
    if (!createCursor(0) || !createCursor(1)) {
        return false;
    }
    return true;
}

void DrmOutput::initDpms(drmModeConnector *connector)
{
    for (int i = 0; i < connector->count_props; ++i) {
        ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> property(drmModeGetProperty(m_backend->fd(), connector->props[i]));
        if (!property) {
            continue;
        }
        if (qstrcmp(property->name, "DPMS") == 0) {
            m_dpms.swap(property);
            break;
        }
    }
}

void DrmOutput::initScaling(drmModeConnector *connector)
{
    for (int i = 0; i < connector->count_props; ++i) {
        ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> property(drmModeGetProperty(m_backend->fd(), connector->props[i]));
        if (!property) {
            continue;
        }
        if (qstrcmp(property->name, "scaling mode") == 0) {
            qCDebug(KWIN_DRM) << "connector support scaling mode";
            m_scalingCapable = true;
            break;
        }
    }
}

static DrmOutput::DpmsMode fromWaylandDpmsMode(KWayland::Server::OutputInterface::DpmsMode wlMode)
{
    using namespace KWayland::Server;
    switch (wlMode) {
    case OutputInterface::DpmsMode::On:
        return DrmOutput::DpmsMode::On;
    case OutputInterface::DpmsMode::Standby:
        return DrmOutput::DpmsMode::Standby;
    case OutputInterface::DpmsMode::Suspend:
        return DrmOutput::DpmsMode::Suspend;
    case OutputInterface::DpmsMode::Off:
        return DrmOutput::DpmsMode::Off;
    default:
        Q_UNREACHABLE();
    }
}

static KWayland::Server::OutputInterface::DpmsMode toWaylandDpmsMode(DrmOutput::DpmsMode mode)
{
    using namespace KWayland::Server;
    switch (mode) {
    case DrmOutput::DpmsMode::On:
        return OutputInterface::DpmsMode::On;
    case DrmOutput::DpmsMode::Standby:
        return OutputInterface::DpmsMode::Standby;
    case DrmOutput::DpmsMode::Suspend:
        return OutputInterface::DpmsMode::Suspend;
    case DrmOutput::DpmsMode::Off:
        return OutputInterface::DpmsMode::Off;
    default:
        Q_UNREACHABLE();
    }
}

void DrmOutput::updateDpms(KWayland::Server::OutputInterface::DpmsMode mode)
{
    if (m_dpms.isNull()) {
        return;
    }

    const auto drmMode = fromWaylandDpmsMode(mode);
    if (drmMode == m_dpmsModePending) {
        qCDebug(KWIN_DRM) << "New DPMS mode equals old mode. DPMS unchanged.";
        return;
    }

    m_dpmsModePending = drmMode;

    if (m_backend->atomicModeSetting()) {
        m_modesetRequested = true;
        if (drmMode == DpmsMode::On) {
            if (m_pageFlipPending) {
                m_pageFlipPending = false;
                Compositor::self()->bufferSwapComplete();
            }
            dpmsOnHandler();
        } else {
            m_dpmsAtomicOffPending = true;
            if (!m_pageFlipPending) {
                dpmsAtomicOff();
            }
        }
    } else {
        if (drmModeConnectorSetProperty(m_backend->fd(), m_conn->id(), m_dpms->prop_id, uint64_t(drmMode)) < 0) {
            m_dpmsModePending = m_dpmsMode;
            qCWarning(KWIN_DRM) << "Setting DPMS failed";
            return;
        }
        if (drmMode == DpmsMode::On) {
            dpmsOnHandler();
        } else {
            dpmsOffHandler();
        }
        m_dpmsMode = m_dpmsModePending;
    }
}

void DrmOutput::dpmsOnHandler()
{
    qCDebug(KWIN_DRM) << "DPMS mode set for output" << m_crtc->id() << "to On.";

    auto wlOutput = waylandOutput();
    if (wlOutput) {
        wlOutput->setDpmsMode(toWaylandDpmsMode(m_dpmsModePending));
    }
    emit dpmsChanged();

    m_backend->checkOutputsAreOn();
    if (!m_backend->atomicModeSetting()) {
        m_crtc->blank();
    }
    if (Compositor *compositor = Compositor::self()) {
        compositor->addRepaintFull();
    }
}

void DrmOutput::dpmsOffHandler()
{
    qCDebug(KWIN_DRM) << "DPMS mode set for output" << m_crtc->id() << "to Off.";

    auto wlOutput = waylandOutput();
    if (wlOutput) {
        wlOutput->setDpmsMode(toWaylandDpmsMode(m_dpmsModePending));
    }
    emit dpmsChanged();

    m_backend->outputWentOff();
}

void DrmOutput::transform(KWayland::Server::OutputDeviceInterface::Transform transform)
{
    waylandOutputDevice()->setTransform(transform);
    using KWayland::Server::OutputDeviceInterface;
    using KWayland::Server::OutputInterface;
    auto wlOutput = waylandOutput();

    switch (transform) {
    case OutputDeviceInterface::Transform::Normal:
        if (m_primaryPlane) {
            m_primaryPlane->setTransformation(DrmPlane::Transformation::Rotate0);
        }
        if (wlOutput) {
            wlOutput->setTransform(OutputInterface::Transform::Normal);
        }
        setOrientation(Qt::PrimaryOrientation);
        break;
    case OutputDeviceInterface::Transform::Rotated90:
        if (m_primaryPlane) {
            m_primaryPlane->setTransformation(DrmPlane::Transformation::Rotate90);
        }
        if (wlOutput) {
            wlOutput->setTransform(OutputInterface::Transform::Rotated90);
        }
        setOrientation(Qt::PortraitOrientation);
        break;
    case OutputDeviceInterface::Transform::Rotated180:
        if (m_primaryPlane) {
            m_primaryPlane->setTransformation(DrmPlane::Transformation::Rotate180);
        }
        if (wlOutput) {
            wlOutput->setTransform(OutputInterface::Transform::Rotated180);
        }
        setOrientation(Qt::InvertedLandscapeOrientation);
        break;
    case OutputDeviceInterface::Transform::Rotated270:
        if (m_primaryPlane) {
            m_primaryPlane->setTransformation(DrmPlane::Transformation::Rotate270);
        }
        if (wlOutput) {
            wlOutput->setTransform(OutputInterface::Transform::Rotated270);
        }
        setOrientation(Qt::InvertedPortraitOrientation);
        break;
    case OutputDeviceInterface::Transform::Flipped:
        // TODO: what is this exactly?
        if (wlOutput) {
            wlOutput->setTransform(OutputInterface::Transform::Flipped);
        }
        break;
    case OutputDeviceInterface::Transform::Flipped90:
        // TODO: what is this exactly?
        if (wlOutput) {
            wlOutput->setTransform(OutputInterface::Transform::Flipped90);
        }
        break;
    case OutputDeviceInterface::Transform::Flipped180:
        // TODO: what is this exactly?
        if (wlOutput) {
            wlOutput->setTransform(OutputInterface::Transform::Flipped180);
        }
        break;
    case OutputDeviceInterface::Transform::Flipped270:
        // TODO: what is this exactly?
        if (wlOutput) {
            wlOutput->setTransform(OutputInterface::Transform::Flipped270);
        }
        break;
    }
    m_modesetRequested = true;
    // the cursor might need to get rotated
    updateCursor();
    showCursor();

    // TODO: are these calls not enough in updateMode already?
    setWaylandMode();
}

void DrmOutput::updateMode(int modeIndex)
{
    // get all modes on the connector
    ScopedDrmPointer<_drmModeConnector, &drmModeFreeConnector> connector(drmModeGetConnector(m_backend->fd(), m_conn->id()));

    if (connector->count_modes <= modeIndex) {
        if (!isInternal() || !m_scalingCapable) {
            return;
        }
        if (modeIndex < waylandOutput()->modes().size()) {
            auto m = waylandOutput()->modes().at(modeIndex);
            if (m.size.width() > m.size.height()) {
                for (const auto& dm: s_default_landscape_drm_mode_infos) {
                    if (dm.hdisplay == m.size.width() &&
                            dm.vdisplay == m.size.height() &&
                            m.refreshRate == refreshRateForMode((drmModeModeInfo*)&dm)) {
                        m_mode = dm;
                    }
                }
            } else {
                for (const auto& dm: s_default_portrait_drm_mode_infos) {
                    if (dm.hdisplay == m.size.width() &&
                            dm.vdisplay == m.size.height() &&
                            m.refreshRate == refreshRateForMode((drmModeModeInfo*)&dm)) {
                        m_mode = dm;
                    }
                }
            }
        } else {
            return;
        }
    } else {
        if (isCurrentMode(&connector->modes[modeIndex])) {
            // nothing to do
            return;
        }
        m_mode = connector->modes[modeIndex];
    }

    QString connectorName = s_connectorNames.value(connector->connector_type, QByteArrayLiteral("Unknown"));
    qCDebug(KWIN_DRM) << __func__ << connectorName << " mid" << modeIndex
        <<"total modes " << waylandOutput()->modes().size()
        << m_mode.hdisplay << m_mode.vdisplay;
    m_modesetRequested = true;
    setWaylandMode();
}

QSize DrmOutput::pixelSize() const
{
    return orientateSize(QSize(m_mode.hdisplay, m_mode.vdisplay));
}

void DrmOutput::setWaylandMode()
{
    AbstractOutput::setWaylandMode(QSize(m_mode.hdisplay, m_mode.vdisplay),
                                   refreshRateForMode(&m_mode));
}

void DrmOutput::pageFlipped()
{
    m_pageFlipPending = false;
    if (m_deleted) {
        deleteLater();
        return;
    }

    if (!m_crtc) {
        return;
    }
    // Egl based surface buffers get destroyed, QPainter based dumb buffers not
    // TODO: split up DrmOutput in two for dumb and egl/gbm surface buffer compatible subclasses completely?
    if (m_backend->deleteBufferAfterPageFlip()) {
        if (m_backend->atomicModeSetting()) {
            if (!m_primaryPlane->next()) {
                // on manual vt switch
                // TODO: when we later use overlay planes it might happen, that we have a page flip with only
                //       damage on one of these, and therefore the primary plane has no next buffer
                //       -> Then we don't want to return here!
                if (m_primaryPlane->current()) {
                    m_primaryPlane->current()->releaseGbm();
                }
                return;
            }
            for (DrmPlane *p : m_nextPlanesFlipList) {
                p->flipBufferWithDelete();
            }
            m_nextPlanesFlipList.clear();
        } else {
            if (!m_crtc->next()) {
                // on manual vt switch
                if (DrmBuffer *b = m_crtc->current()) {
                    b->releaseGbm();
                }
            }
            m_crtc->flipBuffer();
        }
    } else {
        if (m_backend->atomicModeSetting()){
            for (DrmPlane *p : m_nextPlanesFlipList) {
                p->flipBuffer();
            }
            m_nextPlanesFlipList.clear();
        } else {
            m_crtc->flipBuffer();
        }
        m_crtc->flipBuffer();
    }
}

bool DrmOutput::present(DrmBuffer *buffer)
{
    if (m_backend->atomicModeSetting()) {
        return presentAtomically(buffer);
    } else {
        return presentLegacy(buffer);
    }
}

bool DrmOutput::dpmsAtomicOff()
{
    m_dpmsAtomicOffPending = false;

    // TODO: With multiple planes: deactivate all of them here
    delete m_primaryPlane->next();
    m_primaryPlane->setNext(nullptr);
    m_nextPlanesFlipList << m_primaryPlane;

    if (!doAtomicCommit(AtomicCommitMode::Test)) {
        qCDebug(KWIN_DRM) << "Atomic test commit to Dpms Off failed. Aborting.";
        return false;
    }
    if (!doAtomicCommit(AtomicCommitMode::Real)) {
        qCDebug(KWIN_DRM) << "Atomic commit to Dpms Off failed. This should have never happened! Aborting.";
        return false;
    }
    m_nextPlanesFlipList.clear();
    dpmsOffHandler();

    return true;

}

bool DrmOutput::presentAtomically(DrmBuffer *buffer)
{
    if (!LogindIntegration::self()->isActiveSession()) {
        qCWarning(KWIN_DRM) << "Logind session not active.";
        return false;
    }

    if (m_pageFlipPending) {
        qCWarning(KWIN_DRM) << "Page not yet flipped.";
        return false;
    }

    m_primaryPlane->setNext(buffer);
    m_nextPlanesFlipList << m_primaryPlane;

    if (!doAtomicCommit(AtomicCommitMode::Test)) {
        //TODO: When we use planes for layered rendering, fallback to renderer instead. Also for direct scanout?
        //TODO: Probably should undo setNext and reset the flip list
        qCDebug(KWIN_DRM) << "Atomic test commit failed. Aborting present.";
        // go back to previous state
        if (m_lastWorkingState.valid) {
            m_mode = m_lastWorkingState.mode;
            setOrientation(m_lastWorkingState.orientation);
            setGlobalPos(m_lastWorkingState.globalPos);
            if (m_primaryPlane) {
                m_primaryPlane->setTransformation(m_lastWorkingState.planeTransformations);
            }
            m_modesetRequested = true;
            // the cursor might need to get rotated
            updateCursor();
            showCursor();
            // TODO: forward to OutputInterface and OutputDeviceInterface
            setWaylandMode();
            emit screens()->changed();
        }
        return false;
    }

    const bool wasModeset = m_modesetRequested;
    if (!doAtomicCommit(AtomicCommitMode::Real)) {
        qCDebug(KWIN_DRM) << "Atomic commit failed. This should have never happened! Aborting present.";
        //TODO: Probably should undo setNext and reset the flip list
        return false;
    }
    if (wasModeset) {
        // store current mode set as new good state
        m_lastWorkingState.mode = m_mode;
        m_lastWorkingState.orientation = orientation();
        m_lastWorkingState.globalPos = globalPos();
        if (m_primaryPlane) {
            m_lastWorkingState.planeTransformations = m_primaryPlane->transformation();
        }
        m_lastWorkingState.valid = true;
    }
    m_pageFlipPending = true;
    return true;
}

bool DrmOutput::presentLegacy(DrmBuffer *buffer)
{
    if (m_crtc->next()) {
        return false;
    }
    if (!LogindIntegration::self()->isActiveSession()) {
        m_crtc->setNext(buffer);
        return false;
    }
    if (m_dpmsMode != DpmsMode::On) {
        return false;
    }

    // Do we need to set a new mode first?
    if (!m_crtc->current() || m_crtc->current()->needsModeChange(buffer)) {
        if (!setModeLegacy(buffer)) {
            return false;
        }
    }
    const bool ok = drmModePageFlip(m_backend->fd(), m_crtc->id(), buffer->bufferId(), DRM_MODE_PAGE_FLIP_EVENT, this) == 0;
    if (ok) {
        m_crtc->setNext(buffer);
    } else {
        qCWarning(KWIN_DRM) << "Page flip failed:" << strerror(errno);
    }
    return ok;
}

bool DrmOutput::setModeLegacy(DrmBuffer *buffer)
{
    uint32_t connId = m_conn->id();
    if (drmModeSetCrtc(m_backend->fd(), m_crtc->id(), buffer->bufferId(), 0, 0, &connId, 1, &m_mode) == 0) {
        return true;
    } else {
        qCWarning(KWIN_DRM) << "Mode setting failed";
        return false;
    }
}

bool DrmOutput::doAtomicCommit(AtomicCommitMode mode)
{
    drmModeAtomicReq *req = drmModeAtomicAlloc();

    auto errorHandler = [this, mode, req] () {
        if (mode == AtomicCommitMode::Test) {
            // TODO: when we later test overlay planes, make sure we change only the right stuff back
        }
        if (req) {
            drmModeAtomicFree(req);
        }

        if (m_dpmsMode != m_dpmsModePending) {
            qCWarning(KWIN_DRM) << "Setting DPMS failed";
            m_dpmsModePending = m_dpmsMode;
            if (m_dpmsMode != DpmsMode::On) {
                dpmsOffHandler();
            }
        }

        // TODO: see above, rework later for overlay planes!
        for (DrmPlane *p : m_nextPlanesFlipList) {
            p->setNext(nullptr);
        }
        m_nextPlanesFlipList.clear();

    };

    if (!req) {
            qCWarning(KWIN_DRM) << "DRM: couldn't allocate atomic request";
            errorHandler();
            return false;
    }

    uint32_t flags = 0;

    // Do we need to set a new mode?
    if (m_modesetRequested) {
        if (m_dpmsModePending == DpmsMode::On) {
            if (drmModeCreatePropertyBlob(m_backend->fd(), &m_mode, sizeof(m_mode), &m_blobId) != 0) {
                qCWarning(KWIN_DRM) << "Failed to create property blob";
                errorHandler();
                return false;
            }
        }
        if (!atomicReqModesetPopulate(req, m_dpmsModePending == DpmsMode::On)){
            qCWarning(KWIN_DRM) << "Failed to populate Atomic Modeset";
            errorHandler();
            return false;
        }
        flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
    }

    if (mode == AtomicCommitMode::Real) {
        if (m_dpmsModePending == DpmsMode::On) {
            if (!(flags & DRM_MODE_ATOMIC_ALLOW_MODESET)) {
                // TODO: Evaluating this condition should only be necessary, as long as we expect older kernels than 4.10.
                flags |= DRM_MODE_ATOMIC_NONBLOCK;
            }
            flags |= DRM_MODE_PAGE_FLIP_EVENT;
        }
    } else {
        flags |= DRM_MODE_ATOMIC_TEST_ONLY;
    }

    bool ret = true;
    // TODO: Make sure when we use more than one plane at a time, that we go through this list in the right order.
    for (int i = m_nextPlanesFlipList.size() - 1; 0 <= i; i-- ) {
        DrmPlane *p = m_nextPlanesFlipList[i];
        ret &= p->atomicPopulate(req);
    }

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic planes. Abort atomic commit!";
        errorHandler();
        return false;
    }

    if (drmModeAtomicCommit(m_backend->fd(), req, flags, this)) {
        qCWarning(KWIN_DRM) << "Atomic request failed to commit:" << strerror(errno);
        errorHandler();
        return false;
    }

    if (mode == AtomicCommitMode::Real && (flags & DRM_MODE_ATOMIC_ALLOW_MODESET)) {
        qCDebug(KWIN_DRM) << "Atomic Modeset successful.";
        m_modesetRequested = false;
        m_dpmsMode = m_dpmsModePending;
    }

    drmModeAtomicFree(req);
    return true;
}

bool DrmOutput::atomicReqModesetPopulate(drmModeAtomicReq *req, bool enable)
{
    if (enable) {
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::SrcX), 0);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::SrcY), 0);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::SrcW), m_mode.hdisplay << 16);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::SrcH), m_mode.vdisplay << 16);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::CrtcW), m_mode.hdisplay);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::CrtcH), m_mode.vdisplay);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::CrtcId), m_crtc->id());
    } else {
        if (m_backend->deleteBufferAfterPageFlip()) {
            delete m_primaryPlane->current();
            delete m_primaryPlane->next();
        }
        m_primaryPlane->setCurrent(nullptr);
        m_primaryPlane->setNext(nullptr);

        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::SrcX), 0);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::SrcY), 0);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::SrcW), 0);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::SrcH), 0);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::CrtcW), 0);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::CrtcH), 0);
        m_primaryPlane->setValue(int(DrmPlane::PropertyIndex::CrtcId), 0);
    }
    m_conn->setValue(int(DrmConnector::PropertyIndex::CrtcId), enable ? m_crtc->id() : 0);
    m_crtc->setValue(int(DrmCrtc::PropertyIndex::ModeId), enable ? m_blobId : 0);
    m_crtc->setValue(int(DrmCrtc::PropertyIndex::Active), enable);

    bool ret = true;
    ret &= m_conn->atomicPopulate(req);
    ret &= m_crtc->atomicPopulate(req);

    return ret;
}

bool DrmOutput::supportsTransformations() const
{
    if (!m_primaryPlane) {
        return false;
    }
    const auto transformations = m_primaryPlane->supportedTransformations();
    return transformations.testFlag(DrmPlane::Transformation::Rotate90)
        || transformations.testFlag(DrmPlane::Transformation::Rotate180)
        || transformations.testFlag(DrmPlane::Transformation::Rotate270);
}

void DrmOutput::automaticRotation()
{
    if (!m_primaryPlane) {
        return;
    }
    const auto supportedTransformations = m_primaryPlane->supportedTransformations();
    const auto requestedTransformation = screens()->orientationSensor()->orientation();
    using KWayland::Server::OutputDeviceInterface;
    OutputDeviceInterface::Transform newTransformation = OutputDeviceInterface::Transform::Normal;
    switch (requestedTransformation) {
    case OrientationSensor::Orientation::TopUp:
        newTransformation = OutputDeviceInterface::Transform::Normal;
        break;
    case OrientationSensor::Orientation::TopDown:
        if (!supportedTransformations.testFlag(DrmPlane::Transformation::Rotate180)) {
            return;
        }
        newTransformation = OutputDeviceInterface::Transform::Rotated180;
        break;
    case OrientationSensor::Orientation::LeftUp:
        if (!supportedTransformations.testFlag(DrmPlane::Transformation::Rotate90)) {
            return;
        }
        newTransformation = OutputDeviceInterface::Transform::Rotated90;
        break;
    case OrientationSensor::Orientation::RightUp:
        if (!supportedTransformations.testFlag(DrmPlane::Transformation::Rotate270)) {
            return;
        }
        newTransformation = OutputDeviceInterface::Transform::Rotated270;
        break;
    case OrientationSensor::Orientation::FaceUp:
    case OrientationSensor::Orientation::FaceDown:
    case OrientationSensor::Orientation::Undefined:
        // unsupported
        return;
    }
    transform(newTransformation);
    emit screens()->changed();
}

int DrmOutput::getGammaRampSize() const
{
    return m_crtc->getGammaRampSize();
}

bool DrmOutput::setGammaRamp(const ColorCorrect::GammaRamp &gamma)
{
    return m_crtc->setGammaRamp(gamma);
}

}
