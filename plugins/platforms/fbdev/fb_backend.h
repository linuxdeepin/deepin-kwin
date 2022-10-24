// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_FB_BACKEND_H
#define KWIN_FB_BACKEND_H
#include "platform.h"

#include <QImage>
#include <QSize>

namespace KWin
{

class KWIN_EXPORT FramebufferBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "fbdev.json")
public:
    explicit FramebufferBackend(QObject *parent = nullptr);
    virtual ~FramebufferBackend();

    Screens *createScreens(QObject *parent = nullptr) override;
    QPainterBackend *createQPainterBackend() override;

    QSize screenSize() const override {
        return m_resolution;
    }

    void init() override;

    bool isValid() const {
        return m_fd >= 0;
    }

    QSize size() const {
        return m_resolution;
    }
    QSize physicalSize() const {
        return m_physicalSize;
    }

    void map();
    void unmap();
    void *mappedMemory() const {
        return m_memory;
    }
    int bytesPerLine() const {
        return m_bytesPerLine;
    }
    int bufferSize() const {
        return m_bufferLength;
    }
    quint32 bitsPerPixel() const {
        return m_bitsPerPixel;
    }
    QImage::Format imageFormat() const;
    /**
     * @returns whether the imageFormat is BGR instead of RGB.
     **/
    bool isBGR() const {
        return m_bgr;
    }

    QVector<CompositingType> supportedCompositors() const override {
        return QVector<CompositingType>{QPainterCompositing};
    }

private:
    void openFrameBuffer();
    bool handleScreenInfo();
    void initImageFormat();
    QSize m_resolution;
    QSize m_physicalSize;
    QByteArray m_id;
    struct Color {
        quint32 offset;
        quint32 length;
    };
    Color m_red;
    Color m_green;
    Color m_blue;
    Color m_alpha;
    quint32 m_bitsPerPixel = 0;
    int m_fd = -1;
    quint32 m_bufferLength = 0;
    int m_bytesPerLine = 0;
    void *m_memory = nullptr;
    QImage::Format m_imageFormat = QImage::Format_Invalid;
    bool m_bgr = false;
};

}

#endif
