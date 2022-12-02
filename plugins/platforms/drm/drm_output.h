// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_DRM_OUTPUT_H
#define KWIN_DRM_OUTPUT_H

#include "abstract_output.h"
#include "drm_pointer.h"
#include "drm_object.h"
#include "drm_object_plane.h"

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QVector>
#include <xf86drmMode.h>

namespace KWin
{

class DrmBackend;
class DrmBuffer;
class DrmDumbBuffer;
class DrmPlane;
class DrmConnector;
class DrmCrtc;

class KWIN_EXPORT DrmOutput : public AbstractOutput
{
    Q_OBJECT
public:
    struct Edid {
        QByteArray eisaId;
        QByteArray monitorName;
        QByteArray serialNumber;
        QSize physicalSize;
    };
    ///deletes the output, calling this whilst a page flip is pending will result in an error
    ~DrmOutput() override;
    ///queues deleting the output after a page flip has completed.
    void teardown();
    void releaseGbm();
    bool showCursor(DrmDumbBuffer *buffer);
    bool showCursor();
    bool hideCursor();
    void updateCursor();
    void moveCursor(const QPoint &globalPos);
    void setShowCursor(bool flag) {
        m_showCursor = flag;
    }
    bool isNeedShowCursor() {
        return m_showCursor;
    }
    void setHideCursor(bool flag) {
        m_hideCursor = flag;
    }
    bool isNeedHideCursor() {
        return m_hideCursor;
    }
    bool init(drmModeConnector *connector);
    bool present(DrmBuffer *buffer);
    void pageFlipped();

    QSize pixelSize() const override;
    QSize modeSize() const override;

    // These values are defined by the kernel
    enum class DpmsMode {
        On = DRM_MODE_DPMS_ON,
        Standby = DRM_MODE_DPMS_STANDBY,
        Suspend = DRM_MODE_DPMS_SUSPEND,
        Off = DRM_MODE_DPMS_OFF
    };
    bool isDpmsEnabled() const {
        // We care for current as well as pending mode in order to allow first present in AMS.
        return m_dpmsModePending == DpmsMode::On;
    }

    QByteArray uuid() const {
        return m_uuid;
    }

    bool initCursor(const QSize &cursorSize);

    bool supportsTransformations() const;

    bool hardwareTransformed();

    int rotation() const; // rotation degrees
    /**
     * Returns a matrix that can translate into the display's coordinates system
     */
    QMatrix4x4 transformation() const;

    void advertiseLastState();

    void updateEnablement(bool enable);

    void atomicEnable();
    void atomicDisable();

    DrmPlane* getPrimaryPlane() {
        return m_primaryPlane;
    }

    bool m_isVirtual = false;

Q_SIGNALS:
    void dpmsChanged();

private:
    friend class DrmBackend;
    friend class DrmCrtc;   // TODO: For use of setModeLegacy. Remove later when we allow multiple connectors per crtc
                            //       and save the connector ids in the DrmCrtc instance.
    DrmOutput(DrmBackend *backend);

    bool presentAtomically(DrmBuffer *buffer);

    enum class AtomicCommitMode {
        Test,
        Real
    };
    bool doAtomicCommit(AtomicCommitMode mode);

    bool presentLegacy(DrmBuffer *buffer);
    bool setModeLegacy(DrmBuffer *buffer);
    void initEdid(drmModeConnector *connector);
    void initDpms(drmModeConnector *connector);
    void initScaling(drmModeConnector *connector);
    void initOutputDevice(drmModeConnector *connector);

    bool isCurrentMode(const drmModeModeInfo *mode) const;
    void initUuid();
    bool initPrimaryPlane();
    bool initCursorPlane();

    bool dpmsLegacyApply();
    void dpmsOnHandler();
    void dpmsOffHandler();
    bool dpmsAtomicOff();
    bool atomicReqModesetPopulate(drmModeAtomicReq *req, bool enable);
    void updateDpms(KWayland::Server::OutputInterface::DpmsMode mode) override;
    void updateMode(int modeIndex) override;
    void updateColorCurves(KWayland::Server::OutputDeviceInterface::ColorCurves colorCurves);
    void setWaylandMode();

    void transform(KWayland::Server::OutputDeviceInterface::Transform transform) override;
    void automaticRotation();

    int getGammaRampSize() const override;
    bool setGammaRamp(const ColorCorrect::GammaRamp &gamma) override;
    const ColorCorrect::GammaRamp* getGammaRamp() override;

    DrmBackend *m_backend;
    DrmConnector *m_conn = nullptr;
    DrmCrtc *m_crtc = nullptr;
    bool m_lastGbm = false;
    drmModeModeInfo m_mode;
    Edid m_edid;
    QByteArray m_originEdid;
    KWin::ScopedDrmPointer<_drmModeProperty, &drmModeFreeProperty> m_dpms;
    DpmsMode m_dpmsMode = DpmsMode::On;
    DpmsMode m_dpmsModePending = DpmsMode::On;
    QByteArray m_uuid;
    bool m_scalingCapable = false;

    uint32_t m_blobId = 0;
    DrmPlane* m_primaryPlane = nullptr;
    DrmPlane* m_cursorPlane = nullptr;
    QVector<DrmPlane*> m_nextPlanesFlipList;
    bool m_pageFlipPending = false;
    bool m_dpmsAtomicOffPending = false;
    bool m_modesetRequested = true;

    struct {
        Qt::ScreenOrientation orientation;
        drmModeModeInfo mode;
        DrmPlane::Transformations planeTransformations;
        QPoint globalPos;
        bool valid = false;
    } m_lastWorkingState;
    QScopedPointer<DrmDumbBuffer> m_cursor[2];
    int m_cursorIndex = 0;
    bool m_hasNewCursor = false;
    bool m_showCursor = false;
    bool m_hideCursor = false;
    bool m_internal = false;
    bool m_deleted = false;
    bool m_teardown = false;
    QPoint m_oldPos = QPoint(32767, 32767);
    uint8_t m_tryTimes = 0;
};

}

Q_DECLARE_METATYPE(KWin::DrmOutput*)

#endif

