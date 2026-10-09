/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencastmanager.h"
#include "composite.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "deleted.h"
#include "kwingltexture.h"
#include "outputscreencastsource.h"
#include "regionscreencastsource.h"
#include "scene/workspacescene.h"
#include "screencaststream.h"
#include "wayland/display.h"
#include "wayland/output_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "windowscreencastsource.h"
#include "workspace.h"
#include "wayland/dderestrict_interface.h"

#include <KLocalizedString>

namespace KWin
{

ScreencastManager::ScreencastManager()
    : m_screencast(new KWaylandServer::ScreencastV1Interface(waylandServer()->display(), this))
{
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::windowScreencastRequested, this, &ScreencastManager::streamWindow);
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::outputScreencastRequested, this, &ScreencastManager::streamWaylandOutput);
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::virtualOutputScreencastRequested, this, &ScreencastManager::streamVirtualOutput);
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::regionScreencastRequested, this, &ScreencastManager::streamRegion);
}

static QRegion scaleRegion(const QRegion &_region, qreal scale)
{
    if (scale == 1.) {
        return _region;
    }

    QRegion region;
    for (auto it = _region.begin(), itEnd = _region.end(); it != itEnd; ++it) {
        region += QRect(std::floor(it->x() * scale),
                        std::floor(it->y() * scale),
                        std::ceil(it->width() * scale),
                        std::ceil(it->height() * scale));
    }

    return region;
}

class WindowStream : public ScreenCastStream
{
public:
    WindowStream(Window *window, WindowScreenCastSource *source, QObject *parent)
        : ScreenCastStream(source, parent)
        , m_window(window)
    {
        m_timer.setInterval(0);
        m_timer.setSingleShot(true);
        setObjectName(window->desktopFileName());
        connect(&m_timer, &QTimer::timeout, this, &WindowStream::bufferToStream);
        connect(this, &ScreenCastStream::startStreaming, this, &WindowStream::startFeeding);
        connect(this, &ScreenCastStream::stopStreaming, this, &WindowStream::stopFeeding);
        connect(source, &WindowScreenCastSource::frameRendered, this, [this] {
            m_timer.stop();
            m_timer.setSingleShot(true);
            m_timer.setInterval(0);
        });
    }

private:
    void updateProtection()
    {
        // Retry if rate limiting or a pending PipeWire buffer prevents the refresh.
        m_timer.setSingleShot(false);
        m_timer.start(16);
        bufferToStream();
    }

    void startFeeding()
    {
        connect(m_window, &Window::damaged, this, &WindowStream::markDirty);
        if (auto *ddeRestrict = waylandServer()->ddeRestrict()) {
            connect(ddeRestrict, &KWaylandServer::DDERestrictInterface::protectionChanged,
                    this, &WindowStream::updateProtection);
        }
        markDirty();
    }

    void stopFeeding()
    {
        disconnect(m_window, &Window::damaged, this, &WindowStream::markDirty);
        if (auto *ddeRestrict = waylandServer()->ddeRestrict()) {
            disconnect(ddeRestrict, &KWaylandServer::DDERestrictInterface::protectionChanged,
                       this, &WindowStream::updateProtection);
        }
        m_timer.stop();
    }

    void markDirty()
    {
        if (!m_timer.isActive()) {
            m_timer.start();
        }
    }

    void bufferToStream()
    {
        recordFrame(QRegion(0, 0, m_window->width(), m_window->height()));
    }

    Window *m_window;
    QTimer m_timer;
};

void ScreencastManager::streamWindow(KWaylandServer::ScreencastStreamV1Interface *waylandStream,
                                     const QString &winid,
                                     KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    auto window = Workspace::self()->findToplevel(QUuid(winid));
    if (!window) {
        waylandStream->sendFailed(i18n("Could not find window id %1", winid));
        return;
    }

    auto source = new WindowScreenCastSource(window);
    auto stream = new WindowStream(window, source, this);
    stream->setCursorMode(mode, 1, window->clientGeometry().toRect());
    if (mode != KWaylandServer::ScreencastV1Interface::CursorMode::Hidden) {
        connect(window, &Window::clientGeometryChanged, stream, [window, stream, mode]() {
            stream->setCursorMode(mode, 1, window->clientGeometry().toRect());
        });
    }

    integrateStreams(waylandStream, stream);
}

void ScreencastManager::streamVirtualOutput(KWaylandServer::ScreencastStreamV1Interface *stream,
                                            const QString &name,
                                            const QSize &size,
                                            double scale,
                                            KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    auto output = kwinApp()->outputBackend()->createVirtualOutput(name, size, scale);
    streamOutput(stream, output, mode);
    connect(stream, &KWaylandServer::ScreencastStreamV1Interface::finished, output, [output] {
        kwinApp()->outputBackend()->removeVirtualOutput(output);
    });
}

void ScreencastManager::streamWaylandOutput(KWaylandServer::ScreencastStreamV1Interface *waylandStream,
                                            KWaylandServer::OutputInterface *output,
                                            KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    streamOutput(waylandStream, output->handle(), mode);
}

void ScreencastManager::streamOutput(KWaylandServer::ScreencastStreamV1Interface *waylandStream,
                                     Output *streamOutput,
                                     KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    if (!streamOutput) {
        waylandStream->sendFailed(i18n("Could not find output"));
        return;
    }

    auto stream = new ScreenCastStream(new OutputScreenCastSource(streamOutput), this);
    stream->setObjectName(streamOutput->name());

    stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());

    auto bufferToStream = [stream, streamOutput](Output *output, const QRegion &damagedRegion) {
        if (output == streamOutput) {
            stream->recordFrame(scaleRegion(damagedRegion.translated(-streamOutput->geometry().topLeft()), streamOutput->scale()));
        }
    };
    connect(stream, &ScreenCastStream::startStreaming, waylandStream, [streamOutput, stream, bufferToStream] {
        Compositor::self()->scene()->addRepaint(streamOutput->geometry());
        connect(Compositor::self(), &Compositor::captureFrameReady, stream, bufferToStream);
    });
    integrateStreams(waylandStream, stream);
}

static QString rectToString(const QRect &rect)
{
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

void ScreencastManager::streamRegion(KWaylandServer::ScreencastStreamV1Interface *waylandStream, const QRect &geometry, qreal scale, KWaylandServer::ScreencastV1Interface::CursorMode mode)
{

    if (!geometry.isValid()) {
        waylandStream->sendFailed(i18n("Invalid region"));
        return;
    }

    auto source = new RegionScreenCastSource(geometry, scale);
    auto stream = new ScreenCastStream(source, this);
    stream->setObjectName(rectToString(geometry));

    stream->setCursorMode(mode, scale, geometry);

    connect(stream, &ScreenCastStream::startStreaming, waylandStream, [geometry, stream, source] {
        Compositor::self()->scene()->addRepaint(geometry);

        connect(Compositor::self(), &Compositor::captureFrameReady, stream,
                [stream, source](Output *output, const QRegion &damagedRegion) {
            const QRect streamRegion = source->region();
            const QRegion region = damagedRegion & streamRegion;
            if (!output->geometry().intersects(streamRegion) || region.isEmpty()) {
                return;
            }
            source->updateOutput(output);
            stream->recordFrame(scaleRegion(region.translated(-streamRegion.topLeft()), source->scale()));
        });
    });
    integrateStreams(waylandStream, stream);
}

void ScreencastManager::integrateStreams(KWaylandServer::ScreencastStreamV1Interface *waylandStream, ScreenCastStream *stream)
{
    connect(waylandStream, &KWaylandServer::ScreencastStreamV1Interface::finished, stream, &ScreenCastStream::stop);
    connect(stream, &ScreenCastStream::stopStreaming, waylandStream, [stream, waylandStream] {
        waylandStream->sendClosed();
        stream->deleteLater();
    });
    connect(stream, &ScreenCastStream::streamReady, stream, [waylandStream](uint nodeid) {
        waylandStream->sendCreated(nodeid);
    });
    if (!stream->init()) {
        waylandStream->sendFailed(stream->error());
        delete stream;
    }
}

} // namespace KWin
