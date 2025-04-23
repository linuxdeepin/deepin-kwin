/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_adaptor.h"
#include "drm_connector.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_pointer.h"

#include <xf86drmMode.h>

#include <cerrno>
#include <cmath>
#include <cstring>
#include <libxcvt/libxcvt.h>

#include <QProcess>

namespace KWin
{

static bool checkIfEqual(const drmModeModeInfo *one, const drmModeModeInfo *two)
{
    return std::memcmp(one, two, sizeof(drmModeModeInfo)) == 0;
}

static QSize resolutionForMode(const drmModeModeInfo *info)
{
    return QSize(info->hdisplay, info->vdisplay);
}

static quint64 refreshRateForMode(_drmModeModeInfo *m)
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

static OutputMode::Flags flagsForMode(const drmModeModeInfo *info)
{
    OutputMode::Flags flags;
    if (info->type & DRM_MODE_TYPE_PREFERRED) {
        flags |= OutputMode::Flag::Preferred;
    }
    return flags;
}

DrmConnectorMode::DrmConnectorMode(DrmConnector *connector, drmModeModeInfo nativeMode)
    : OutputMode(resolutionForMode(&nativeMode), refreshRateForMode(&nativeMode), flagsForMode(&nativeMode))
    , m_connector(connector)
    , m_nativeMode(nativeMode)
{
}

std::shared_ptr<DrmBlobFactory> DrmConnectorMode::blob()
{
    if (!m_blob) {
        m_blob = DrmBlobFactory::create(m_connector->gpu(), &m_nativeMode, sizeof(m_nativeMode));
    }
    return m_blob;
}

drmModeModeInfo *DrmConnectorMode::nativeMode()
{
    return &m_nativeMode;
}

bool DrmConnectorMode::operator==(const DrmConnectorMode &otherMode) const
{
    return checkIfEqual(&m_nativeMode, &otherMode.m_nativeMode);
}

DrmConnector::DrmConnector(DrmGpu *gpu, uint32_t connectorId)
    : DrmObject(gpu,
                connectorId,
                {PropertyDefinition(QByteArrayLiteral("CRTC_ID"), Requirement::Required),
                 PropertyDefinition(QByteArrayLiteral("non-desktop"), Requirement::Optional),
                 PropertyDefinition(QByteArrayLiteral("DPMS"), Requirement::RequiredForLegacy),
                 PropertyDefinition(QByteArrayLiteral("EDID"), Requirement::Optional),
                 PropertyDefinition(QByteArrayLiteral("overscan"), Requirement::Optional),
                 PropertyDefinition(QByteArrayLiteral("vrr_capable"), Requirement::Optional),
                 PropertyDefinition(QByteArrayLiteral("underscan"), Requirement::Optional,
                                    {QByteArrayLiteral("off"), QByteArrayLiteral("on"), QByteArrayLiteral("auto")}),
                 PropertyDefinition(QByteArrayLiteral("underscan vborder"), Requirement::Optional),
                 PropertyDefinition(QByteArrayLiteral("underscan hborder"), Requirement::Optional),
                 PropertyDefinition(QByteArrayLiteral("Broadcast RGB"), Requirement::Optional,
                                    {QByteArrayLiteral("Automatic"), QByteArrayLiteral("Full"), QByteArrayLiteral("Limited 16:235")}),
                 PropertyDefinition(QByteArrayLiteral("max bpc"), Requirement::Optional),
                 PropertyDefinition(QByteArrayLiteral("link-status"), Requirement::Optional,
                                    {QByteArrayLiteral("Good"), QByteArrayLiteral("Bad")}),
                 PropertyDefinition(QByteArrayLiteral("content type"), Requirement::Optional,
                                    {QByteArrayLiteral("No Data"), QByteArrayLiteral("Graphics"), QByteArrayLiteral("Photo"), QByteArrayLiteral("Cinema"), QByteArrayLiteral("Game")}),
                 PropertyDefinition(QByteArrayLiteral("panel orientation"), Requirement::Optional, {QByteArrayLiteral("Normal"), QByteArrayLiteral("Upside Down"), QByteArrayLiteral("Left Side Up"), QByteArrayLiteral("Right Side Up")}),
                 PropertyDefinition(QByteArrayLiteral("HDR_OUTPUT_METADATA"), Requirement::Optional),
                 PropertyDefinition(QByteArrayLiteral("brightness"), Requirement::Optional)},
                DRM_MODE_OBJECT_CONNECTOR)
    , m_pipeline(std::make_unique<DrmPipeline>(this))
    , m_conn(drmModeGetConnector(gpu->fd(), connectorId))
{
    if (m_conn) {
        for (int i = 0; i < m_conn->count_encoders; ++i) {
            DrmUniquePtr<drmModeEncoder> enc(drmModeGetEncoder(gpu->fd(), m_conn->encoders[i]));
            if (!enc) {
                qCWarning(KWIN_DRM) << "failed to get encoder" << m_conn->encoders[i];
                continue;
            }
            m_possibleCrtcs |= enc->possible_crtcs;
        }
        for (int i = 0; i < m_conn->count_props; ++i) {
            ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> property(drmModeGetProperty(gpu->fd(), m_conn->props[i]));
            if (!property) {
                continue;
            }
            if (qstrcmp(property->name, "scaling mode") == 0) {
                qCDebug(KWIN_DRM) << "connector support scaling mode";
                m_scalingCapable = true;
                break;
            }
        }
    } else {
        qCWarning(KWIN_DRM) << "drmModeGetConnector failed!" << strerror(errno);
    }
}

bool DrmConnector::init()
{
    return m_conn && initProps();
}

bool DrmConnector::isConnected() const
{
    return !m_driverModes.empty() && m_conn && m_conn->connection == DRM_MODE_CONNECTED;
}

QString DrmConnector::connectorName() const
{
    const char *connectorName = drmModeGetConnectorTypeName(m_conn->connector_type);
    if (!connectorName) {
        connectorName = "Unknown";
    }
    return QStringLiteral("%1-%2").arg(connectorName).arg(m_conn->connector_type_id);
}

QString DrmConnector::modelName() const
{
    if (m_edid.serialNumber().isEmpty()) {
        return connectorName() + QLatin1Char('-') + m_edid.nameString();
    } else {
        return m_edid.nameString();
    }
}

bool DrmConnector::isInternal() const
{
    return m_conn->connector_type == DRM_MODE_CONNECTOR_LVDS || m_conn->connector_type == DRM_MODE_CONNECTOR_eDP
        || m_conn->connector_type == DRM_MODE_CONNECTOR_DSI;
}

QSize DrmConnector::physicalSize() const
{
    return m_physicalSize;
}

QList<std::shared_ptr<DrmConnectorMode>> DrmConnector::modes() const
{
    return m_modes;
}

std::shared_ptr<DrmConnectorMode> DrmConnector::findMode(const drmModeModeInfo &modeInfo) const
{
    const auto it = std::find_if(m_modes.constBegin(), m_modes.constEnd(), [&modeInfo](const auto &mode) {
        return checkIfEqual(mode->nativeMode(), &modeInfo);
    });
    return it == m_modes.constEnd() ? nullptr : *it;
}

Output::SubPixel DrmConnector::subpixel() const
{
    switch (m_conn->subpixel) {
    case DRM_MODE_SUBPIXEL_UNKNOWN:
        return Output::SubPixel::Unknown;
    case DRM_MODE_SUBPIXEL_NONE:
        return Output::SubPixel::None;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
        return Output::SubPixel::Horizontal_RGB;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
        return Output::SubPixel::Horizontal_BGR;
    case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
        return Output::SubPixel::Vertical_RGB;
    case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
        return Output::SubPixel::Vertical_BGR;
    default:
        Q_UNREACHABLE();
    }
}

bool DrmConnector::hasBrightness() const
{
    return getProp(PropertyIndex::BrightnessId);
}

int32_t DrmConnector::brightness() const
{
    if (const auto &prop = getProp(PropertyIndex::BrightnessId)) {
        return prop->pending();
    }
    return 60;
}

bool DrmConnector::hasOverscan() const
{
    return getProp(PropertyIndex::Overscan) || getProp(PropertyIndex::Underscan);
}

uint32_t DrmConnector::overscan() const
{
    if (const auto &prop = getProp(PropertyIndex::Overscan)) {
        return prop->pending();
    } else if (const auto &prop = getProp(PropertyIndex::Underscan_vborder)) {
        return prop->pending();
    }
    return 0;
}

bool DrmConnector::vrrCapable() const
{
    const auto prop = getProp(PropertyIndex::VrrCapable);
    return prop && prop->current() == 1;
}

bool DrmConnector::hasRgbRange() const
{
    const auto &rgb = getProp(PropertyIndex::Broadcast_RGB);
    return rgb && rgb->hasAllEnums();
}

Output::RgbRange DrmConnector::rgbRange() const
{
    const auto &rgb = getProp(PropertyIndex::Broadcast_RGB);
    return rgb->enumForValue<Output::RgbRange>(rgb->pending());
}

bool DrmConnector::updateProperties()
{
    if (auto connector = drmModeGetConnector(gpu()->fd(), id())) {
        m_conn.reset(connector);
    } else if (!m_conn) {
        return false;
    }
    if (!DrmObject::updateProperties()) {
        return false;
    }
    if (const auto &dpms = getProp(PropertyIndex::Dpms)) {
        dpms->setLegacy();
    }

    auto &underscan = m_props[static_cast<uint32_t>(PropertyIndex::Underscan)];
    auto &vborder = m_props[static_cast<uint32_t>(PropertyIndex::Underscan_vborder)];
    auto &hborder = m_props[static_cast<uint32_t>(PropertyIndex::Underscan_hborder)];
    if (underscan && vborder && hborder) {
        underscan->setEnum(vborder->current() > 0 ? UnderscanOptions::On : UnderscanOptions::Off);
    } else {
        underscan.reset();
        vborder.reset();
        hborder.reset();
    }

    // parse edid
    if (const auto edidProp = getProp(PropertyIndex::Edid); edidProp && edidProp->immutableBlob()) {
        m_edid = Edid(edidProp->immutableBlob()->data, edidProp->immutableBlob()->length);
        if (!m_edid.isValid()) {
            qCWarning(KWIN_DRM) << "Couldn't parse EDID for connector" << this;
        }
    } else if (m_conn->connection == DRM_MODE_CONNECTED) {
        qCDebug(KWIN_DRM) << "Could not find edid for connector" << this;
    }

    // check the physical size
    if (m_edid.physicalSize().isEmpty()) {
        m_physicalSize = QSize(m_conn->mmWidth, m_conn->mmHeight);
    } else {
        m_physicalSize = m_edid.physicalSize();
    }

    // Huawei KLVU 410 and KLVV420 devices use mipi screens and do not provide
    // edid, so the screen size cannot be obtained through drm. The screen size
    // obtained from the external screen is 571mm x 381mm. Set a fixed screen
    // size for these two devices.
    if (m_physicalSize.isEmpty()) {
        QStringList strArg;
        strArg << "-s" << "system-product-name";
        QProcess proc;
        proc.setProgram("/usr/sbin/dmidecode");
        proc.setArguments(strArg);
        proc.start(QIODevice::ReadWrite);
        proc.waitForFinished(-1);
        QString oputput = proc.readAllStandardOutput().data();
        if (oputput.contains("KLVU") || oputput.contains("KLVV")) {
            m_physicalSize = QSize(571, 381);
        }
    }

    // update modes
    bool equal = m_conn->count_modes == m_driverModes.count();
    for (int i = 0; equal && i < m_conn->count_modes; i++) {
        equal &= checkIfEqual(m_driverModes[i]->nativeMode(), &m_conn->modes[i]);
    }
    if (!equal && m_conn->count_modes > 0) {
        // reload modes
        m_driverModes.clear();
        for (int i = 0; i < m_conn->count_modes; i++) {
            m_driverModes.append(std::make_shared<DrmConnectorMode>(this, m_conn->modes[i]));
        }
        m_modes.clear();
        m_modes.append(m_driverModes);
        // if hardware support upscaling and internal panel only presents one physical mode,
        // we extend the list with some default modes
        if (isInternal() && m_modes.size() == 1 && m_scalingCapable) {
            m_modes.append(generateCommonModes());
        }
        if (m_pipeline->mode()) {
            if (const auto mode = findMode(*m_pipeline->mode()->nativeMode())) {
                m_pipeline->setMode(mode);
            } else {
                m_pipeline->setMode(m_modes.constFirst());
            }
        } else {
            m_pipeline->setMode(m_modes.constFirst());
        }
        m_pipeline->applyPendingChanges();
        if (m_pipeline->output()) {
            m_pipeline->output()->updateModes();
        }
    }

    return true;
}

bool DrmConnector::isCrtcSupported(DrmCrtc *crtc) const
{
    return (m_possibleCrtcs & (1 << crtc->pipeIndex()));
}

bool DrmConnector::isNonDesktop() const
{
    const auto &prop = getProp(PropertyIndex::NonDesktop);
    return prop && prop->current();
}

const Edid *DrmConnector::edid() const
{
    return &m_edid;
}

DrmPipeline *DrmConnector::pipeline() const
{
    return m_pipeline.get();
}

void DrmConnector::disable()
{
    setPending(PropertyIndex::CrtcId, 0);
}

DrmConnector::LinkStatus DrmConnector::linkStatus() const
{
    if (const auto &property = getProp(PropertyIndex::LinkStatus)) {
        return property->enumForValue<LinkStatus>(property->current());
    }
    return LinkStatus::Good;
}

static const QVector<QSize> s_commonModes = {
    /* 4:3 (1.33) */
    QSize(1600, 1200),
    QSize(1280, 1024), /* 5:4 (1.25) */
    QSize(1024, 768),
    /* 16:10 (1.6) */
    QSize(2560, 1600),
    QSize(1920, 1200),
    QSize(1280, 800),
    /* 16:9 (1.77) */
    QSize(5120, 2880),
    QSize(3840, 2160),
    QSize(3200, 1800),
    QSize(2880, 1620),
    QSize(2560, 1440),
    QSize(1920, 1080),
    QSize(1600, 900),
    QSize(1368, 768),
    QSize(1280, 720),
};

QList<std::shared_ptr<DrmConnectorMode>> DrmConnector::generateCommonModes()
{
    QList<std::shared_ptr<DrmConnectorMode>> ret;
    QSize maxSize;
    uint32_t maxSizeRefreshRate = 0;
    for (const auto &mode : std::as_const(m_driverModes)) {
        if (mode->size().width() >= maxSize.width() && mode->size().height() >= maxSize.height() && mode->refreshRate() >= maxSizeRefreshRate) {
            maxSize = mode->size();
            maxSizeRefreshRate = mode->refreshRate();
        }
    }
    const uint64_t maxBandwidthEstimation = maxSize.width() * maxSize.height() * uint64_t(maxSizeRefreshRate);
    for (const auto &size : s_commonModes) {
        const uint64_t bandwidthEstimation = size.width() * size.height() * 60000ull;
        if (size.width() > maxSize.width() || size.height() > maxSize.height() || bandwidthEstimation > maxBandwidthEstimation) {
            continue;
        }
        const auto generatedMode = generateMode(size, 60);
        if (std::any_of(m_driverModes.cbegin(), m_driverModes.cend(), [generatedMode](const auto &mode) {
                return mode->size() == generatedMode->size() && mode->refreshRate() == generatedMode->refreshRate();
            })) {
            continue;
        }
        ret << generatedMode;
    }
    return ret;
}

std::shared_ptr<DrmConnectorMode> DrmConnector::generateMode(const QSize &size, float refreshRate)
{
    auto modeInfo = libxcvt_gen_mode_info(size.width(), size.height(), refreshRate, false, false);

    drmModeModeInfo mode{
        .clock = uint32_t(modeInfo->dot_clock),
        .hdisplay = uint16_t(modeInfo->hdisplay),
        .hsync_start = modeInfo->hsync_start,
        .hsync_end = modeInfo->hsync_end,
        .htotal = modeInfo->htotal,
        .vdisplay = uint16_t(modeInfo->vdisplay),
        .vsync_start = modeInfo->vsync_start,
        .vsync_end = modeInfo->vsync_end,
        .vtotal = modeInfo->vtotal,
        .vscan = 1,
        .vrefresh = static_cast<uint32_t>(std::ceil(modeInfo->vrefresh)),
        .flags = modeInfo->mode_flags,
        .type = DRM_MODE_TYPE_USERDEF,
    };

    sprintf(mode.name, "%dx%d@%d", size.width(), size.height(), mode.vrefresh);

    free(modeInfo);
    return std::make_shared<DrmConnectorMode>(this, mode);
}

DrmConnector::PanelOrientation DrmConnector::panelOrientation() const
{
    if (const auto &property = getProp(PropertyIndex::PanelOrientation)) {
        return property->enumForValue<PanelOrientation>(property->current());
    } else {
        return PanelOrientation::Normal;
    }
}

QDebug &operator<<(QDebug &s, const KWin::DrmConnector *obj)
{
    QDebugStateSaver saver(s);
    if (obj) {

        QString connState = QStringLiteral("Disconnected");
        if (!obj->m_conn || obj->m_conn->connection == DRM_MODE_UNKNOWNCONNECTION) {
            connState = QStringLiteral("Unknown Connection");
        } else if (obj->m_conn->connection == DRM_MODE_CONNECTED) {
            connState = QStringLiteral("Connected");
        }

        s.nospace() << "DrmConnector(id=" << obj->id() << ", gpu=" << obj->gpu() << ", name=" << obj->modelName() << ", connection=" << connState << ", countMode=" << (obj->m_conn ? obj->m_conn->count_modes : 0)
                    << ')';
    } else {
        s << "DrmConnector(0x0)";
    }
    return s;
}

DrmConnector::DrmContentType DrmConnector::kwinToDrmContentType(ContentType type)
{
    switch (type) {
    case ContentType::None:
        return DrmContentType::Graphics;
    case ContentType::Photo:
        return DrmContentType::Photo;
    case ContentType::Video:
        return DrmContentType::Cinema;
    case ContentType::Game:
        return DrmContentType::Game;
    default:
        Q_UNREACHABLE();
    }
}

Output::Transform DrmConnector::toKWinTransform(PanelOrientation orientation)
{
    switch (orientation) {
    case PanelOrientation::Normal:
        return KWin::Output::Transform::Normal;
    case PanelOrientation::RightUp:
        return KWin::Output::Transform::Rotated270;
    case PanelOrientation::LeftUp:
        return KWin::Output::Transform::Rotated90;
    case PanelOrientation::UpsideDown:
        return KWin::Output::Transform::Rotated180;
    default:
        Q_UNREACHABLE();
    }
}
}
