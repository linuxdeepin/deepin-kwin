// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_DRM_BACKEND_H
#define KWIN_DRM_BACKEND_H
#include "platform.h"
#include "input.h"

#include "drm_buffer.h"
#if HAVE_GBM
#include "drm_buffer_gbm.h"
#endif
#include "drm_inputeventfilter.h"
#include "drm_pointer.h"

#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QSize>
#include <QVector>
#include <xf86drmMode.h>

#include <memory>

struct gbm_bo;
struct gbm_device;
struct gbm_surface;

namespace KWin
{

class Udev;
class UdevMonitor;

class DrmOutput;
class DrmPlane;
class DrmCrtc;
class DrmConnector;
class GbmSurface;
class EglGbmBackend;
class Cursor;
class DrmGpu;
class UdevDevice;

class KWIN_EXPORT DrmBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "drm.json")
public:
    explicit DrmBackend(QObject *parent = nullptr);
    ~DrmBackend() override;

    void configurationChangeRequested(KWayland::Server::OutputConfigurationInterface *config) override;
    Screens *createScreens(QObject *parent = nullptr) override;
    QPainterBackend *createQPainterBackend() override;
    OpenGLBackend* createOpenGLBackend() override;
    OpenGLBackend* getOpenGLBackend() override;

    bool requiresCompositing() const override;
    DmaBufTexture *createDmaBufTexture(const QSize &size) override;

    void init() override;
    
    bool present(DrmBuffer *buffer, DrmOutput *output);

    Outputs outputs() const override;
    Outputs enabledOutputs() const override;
    QVector<DrmOutput*> drmOutputs() const {
        return m_outputs;
    }
    QVector<DrmOutput*> drmEnabledOutputs() const {
        return m_enabledOutputs;
    }

    void enableOutput(DrmOutput *output, bool enable);

    void outputWentOff();
    void checkOutputsAreOn();

    // QPainter reuses buffers
    bool deleteBufferAfterPageFlip() const {
        return m_deleteBufferAfterPageFlip;
    }

#if HAVE_EGL_STREAMS
    bool useEglStreams() const {
        return m_useEglStreams;
    }
#endif

    QVector<CompositingType> supportedCompositors() const override;

    QString supportInformation() const override;

    bool isCursorEnabled() const {
        return m_cursorEnabled;
    };

    enum CursorType {
        HardwareCursor, ///< 硬鼠
        SoftwareCursor  ///< 软鼠
    };
    ///
    /// \brief changeCursorType 切换鼠标类型
    /// \param cursorType 鼠标类型
    ///
    void changeCursorType(CursorType cursorType = HardwareCursor);

    DrmGpu *primaryGpu() const;
    DrmGpu *findGpu(int sysNum) const;
    DrmGpu *addGpu(UdevDevice *device);

public Q_SLOTS:
    void turnOutputsOn();

protected:
    void doHideCursor() override;
    void doShowCursor() override;
private:
    friend class DrmGpu;
    void addOutput(DrmOutput* output);
    void removeOutput(DrmOutput* output);
    static void pageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
    void openDrm();
    void activate(bool active);
    void reactivate();
    void deactivate();
    bool updateOutputs();
    void setCursor();
    void updateCursor();
    void moveCursor();
    void initCursor();
    void readOutputsConfiguration();
    void writeOutputsConfiguration();
    QByteArray generateOutputConfigurationUuid() const;
    DrmOutput *findOutput(quint32 connector);
    DrmOutput *findOutput(const QByteArray &uuid);
    void updateOutputsEnabled();
    QScopedPointer<Udev> m_udev;
    QScopedPointer<UdevMonitor> m_udevMonitor;
    
    // active output pipelines (planes + crtc + encoder + connector)
    QVector<DrmOutput*> m_outputs;
    // active and enabled pipelines (above + wl_output)
    QVector<DrmOutput*> m_enabledOutputs;

    bool m_deleteBufferAfterPageFlip;
    bool m_cursorEnabled = false;
    QSize m_cursorSize;
    int m_pageFlipsPending = 0;
    bool m_active = false;
#if HAVE_EGL_STREAMS
    bool m_useEglStreams = false;
#endif
    QVector<DrmGpu*> m_gpus;
    QScopedPointer<DpmsInputEventFilter> m_dpmsFilter;
    EglGbmBackend *m_eglGbmBackend = nullptr;
};


}

#endif

