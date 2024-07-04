/*
    SPDX-FileCopyrightText: 2010 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenshotdbusinterface2.h"
#include "composite.h"
#include "screenshot2adaptor.h"
#include "screenshotlogging.h"
#include "utils/serviceutils.h"

#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QtConcurrent>

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

namespace KWin
{

static ScreenShotFlags screenShotFlagsFromOptions(const QVariantMap &options)
{
    ScreenShotFlags flags = ScreenShotFlags();

    const QVariant includeDecoration = options.value(QStringLiteral("include-decoration"));
    if (includeDecoration.toBool()) {
        flags |= ScreenShotIncludeDecoration;
    }

    const QVariant includeCursor = options.value(QStringLiteral("include-cursor"));
    if (includeCursor.toBool()) {
        flags |= ScreenShotIncludeCursor;
    }

    const QVariant nativeResolution = options.value(QStringLiteral("native-resolution"));
    if (nativeResolution.toBool()) {
        flags |= ScreenShotNativeResolution;
    }

    return flags;
}

static void writeBufferToPipe(int fileDescriptor, const QByteArray &buffer)
{
    QFile file;
    if (!file.open(fileDescriptor, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
        close(fileDescriptor);
        qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "failed to open pipe:" << file.errorString();
        return;
    }

    qint64 remainingSize = buffer.size();

    pollfd pfds[1];
    pfds[0].fd = fileDescriptor;
    pfds[0].events = POLLOUT;

    while (true) {
        const int ready = poll(pfds, 1, 60000);
        if (ready < 0) {
            if (errno != EINTR) {
                qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "poll() failed:" << strerror(errno);
                return;
            }
        } else if (ready == 0) {
            qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "timed out writing to pipe";
            return;
        } else if (!(pfds[0].revents & POLLOUT)) {
            qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "pipe is broken";
            return;
        } else {
            const char *chunk = buffer.constData() + (buffer.size() - remainingSize);
            const qint64 writtenCount = file.write(chunk, remainingSize);

            if (writtenCount < 0) {
                qCWarning(KWIN_SCREENSHOT) << Q_FUNC_INFO << "write() failed:" << file.errorString();
                return;
            }

            remainingSize -= writtenCount;
            if (writtenCount == 0 || remainingSize == 0) {
                return;
            }
        }
    }
}

static const QString s_dbusServiceName = QStringLiteral("org.kde.KWin.ScreenShot2");
static const QString s_dbusInterface = QStringLiteral("org.kde.KWin.ScreenShot2");
static const QString s_dbusObjectPath = QStringLiteral("/org/kde/KWin/ScreenShot2");

static const QString s_errorNotAuthorized = QStringLiteral("org.kde.KWin.ScreenShot2.Error.NoAuthorized");
static const QString s_errorNotAuthorizedMessage = QStringLiteral("The process is not authorized to take a screenshot");
static const QString s_errorCancelled = QStringLiteral("org.kde.KWin.ScreenShot2.Error.Cancelled");
static const QString s_errorCancelledMessage = QStringLiteral("Screenshot got cancelled");
static const QString s_errorInvalidWindow = QStringLiteral("org.kde.KWin.ScreenShot2.Error.InvalidWindow");
static const QString s_errorInvalidWindowMessage = QStringLiteral("Invalid window requested");
static const QString s_errorInvalidArea = QStringLiteral("org.kde.KWin.ScreenShot2.Error.InvalidArea");
static const QString s_errorInvalidAreaMessage = QStringLiteral("Invalid area requested");
static const QString s_errorInvalidScreen = QStringLiteral("org.kde.KWin.ScreenShot2.Error.InvalidScreen");
static const QString s_errorInvalidScreenMessage = QStringLiteral("Invalid screen requested");
static const QString s_errorFileDescriptor = QStringLiteral("org.kde.KWin.ScreenShot2.Error.FileDescriptor");
static const QString s_errorFileDescriptorMessage = QStringLiteral("No valid file descriptor");

class ScreenShotSource2 : public QObject
{
    Q_OBJECT

public:
    explicit ScreenShotSource2(const QFuture<QImage> &future);

    bool isCancelled() const;
    bool isCompleted() const;
    void marshal(ScreenShotSinkPipe2 *sink);

Q_SIGNALS:
    void cancelled();
    void completed();

private:
    QFuture<QImage> m_future;
    QFutureWatcher<QImage> *m_watcher;
};

class ScreenShotSourceScreen2 : public ScreenShotSource2
{
    Q_OBJECT

public:
    ScreenShotSourceScreen2(ScreenShotEffect *effect, EffectScreen *screen, ScreenShotFlags flags);
};

class ScreenShotSourceArea2 : public ScreenShotSource2
{
    Q_OBJECT

public:
    ScreenShotSourceArea2(ScreenShotEffect *effect, const QRect &area, ScreenShotFlags flags);
};

class ScreenShotSourceWindow2 : public ScreenShotSource2
{
    Q_OBJECT

public:
    ScreenShotSourceWindow2(ScreenShotEffect *effect, EffectWindow *window, ScreenShotFlags flags);
};

class ScreenShotSinkPipe2 : public QObject
{
    Q_OBJECT

public:
    ScreenShotSinkPipe2(int fileDescriptor, QDBusMessage replyMessage);
    ~ScreenShotSinkPipe2();

    void cancel();
    void flush(const QImage &image);

private:
    QDBusMessage m_replyMessage;
    int m_fileDescriptor;
};

ScreenShotSource2::ScreenShotSource2(const QFuture<QImage> &future)
    : m_future(future)
{
    m_watcher = new QFutureWatcher<QImage>(this);
    connect(m_watcher, &QFutureWatcher<QImage>::finished, this, &ScreenShotSource2::completed);
    connect(m_watcher, &QFutureWatcher<QImage>::canceled, this, &ScreenShotSource2::cancelled);
    m_watcher->setFuture(m_future);
    // takeScreenShot will done in ScreenShotEffect::paintScreen
    // if all windows are minmize, we must call scheduleRepaint actively
    Compositor::self()->scheduleRepaint();
}

bool ScreenShotSource2::isCancelled() const
{
    return m_future.isCanceled();
}

bool ScreenShotSource2::isCompleted() const
{
    return m_future.isFinished();
}

void ScreenShotSource2::marshal(ScreenShotSinkPipe2 *sink)
{
    sink->flush(m_future.result());
}

ScreenShotSourceScreen2::ScreenShotSourceScreen2(ScreenShotEffect *effect,
                                                 EffectScreen *screen,
                                                 ScreenShotFlags flags)
    : ScreenShotSource2(effect->scheduleScreenShot(screen, flags))
{
}

ScreenShotSourceArea2::ScreenShotSourceArea2(ScreenShotEffect *effect,
                                             const QRect &area,
                                             ScreenShotFlags flags)
    : ScreenShotSource2(effect->scheduleScreenShot(area, flags))
{
}

ScreenShotSourceWindow2::ScreenShotSourceWindow2(ScreenShotEffect *effect,
                                                 EffectWindow *window,
                                                 ScreenShotFlags flags)
    : ScreenShotSource2(effect->scheduleScreenShot(window, flags))
{
}

ScreenShotSinkPipe2::ScreenShotSinkPipe2(int fileDescriptor, QDBusMessage replyMessage)
    : m_replyMessage(replyMessage)
    , m_fileDescriptor(fileDescriptor)
{
}

ScreenShotSinkPipe2::~ScreenShotSinkPipe2()
{
    if (m_fileDescriptor != -1) {
        close(m_fileDescriptor);
    }
}

void ScreenShotSinkPipe2::cancel()
{
    QDBusConnection::sessionBus().send(m_replyMessage.createErrorReply(s_errorCancelled,
                                                                       s_errorCancelledMessage));
}

void ScreenShotSinkPipe2::flush(const QImage &image)
{
    if (m_fileDescriptor == -1) {
        return;
    }

    // Note that the type of the data stored in the vardict matters. Be careful.
    QVariantMap results;
    results.insert(QStringLiteral("type"), QStringLiteral("raw"));
    results.insert(QStringLiteral("format"), quint32(image.format()));
    results.insert(QStringLiteral("width"), quint32(image.width()));
    results.insert(QStringLiteral("height"), quint32(image.height()));
    results.insert(QStringLiteral("stride"), quint32(image.bytesPerLine()));
    QDBusConnection::sessionBus().send(m_replyMessage.createReply(results));

    QtConcurrent::run([](int fileDescriptor, const QImage &image) {
        const QByteArray buffer(reinterpret_cast<const char *>(image.constBits()),
                                image.sizeInBytes());
        writeBufferToPipe(fileDescriptor, buffer);
    },
                      m_fileDescriptor, image);

    // The ownership of the pipe file descriptor has been moved to the worker thread.
    m_fileDescriptor = -1;
}

ScreenShotDBusInterface2::ScreenShotDBusInterface2(ScreenShotEffect *effect)
    : QObject(effect)
    , m_effect(effect)
{
    new ScreenShot2Adaptor(this);

    QDBusConnection::sessionBus().registerObject(s_dbusObjectPath, this);
    QDBusConnection::sessionBus().registerService(s_dbusServiceName);
}

ScreenShotDBusInterface2::~ScreenShotDBusInterface2()
{
    QDBusConnection::sessionBus().unregisterService(s_dbusServiceName);
    QDBusConnection::sessionBus().unregisterObject(s_dbusObjectPath);
}

int ScreenShotDBusInterface2::version() const
{
    return 2;
}

bool ScreenShotDBusInterface2::checkPermissions() const
{
    if (!calledFromDBus()) {
        return false;
    }

    static bool permissionCheckDisabled = qEnvironmentVariableIntValue("KWIN_SCREENSHOT_NO_PERMISSION_CHECKS") == 1;
    if (permissionCheckDisabled) {
        return true;
    }

    const QDBusReply<uint> reply = connection().interface()->servicePid(message().service());
    if (reply.isValid()) {
        const uint pid = reply.value();
        const auto interfaces = KWin::fetchRestrictedDBusInterfacesFromPid(pid);
        if (m_effect->checkInteface && !interfaces.contains(s_dbusInterface)) {
            sendErrorReply(s_errorNotAuthorized, s_errorNotAuthorizedMessage);
            return false;
        }
    } else {
        return false;
    }

    return true;
}

QVariantMap ScreenShotDBusInterface2::CaptureActiveWindow(const QVariantMap &options,
                                                          QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    EffectWindow *window = effects->activeWindow();
    if (!window) {
        sendErrorReply(s_errorInvalidWindow, s_errorInvalidWindowMessage);
        return QVariantMap();
    }

    const int fileDescriptor = dup(pipe.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(window, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureWindow(const QString &handle,
                                                    const QVariantMap &options,
                                                    QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    EffectWindow *window = effects->findWindow(QUuid(handle));
    if (!window) {
        bool ok;
        const int winId = handle.toInt(&ok);
        if (ok) {
            window = effects->findWindow(winId);
        } else {
            qCWarning(KWIN_SCREENSHOT) << "Invalid handle:" << handle;
        }
    }
    if (!window) {
        sendErrorReply(s_errorInvalidWindow, s_errorInvalidWindowMessage);
        return QVariantMap();
    }

    const int fileDescriptor = dup(pipe.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(window, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureArea(int x, int y, int width, int height,
                                                  const QVariantMap &options,
                                                  QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    const QRect area(x, y, width, height);
    if (area.isEmpty()) {
        sendErrorReply(s_errorInvalidArea, s_errorInvalidAreaMessage);
        return QVariantMap();
    }

    const int fileDescriptor = dup(pipe.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(area, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureScreen(const QString &name,
                                                    const QVariantMap &options,
                                                    QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    EffectScreen *screen = effects->findScreen(name);
    if (!screen) {
        sendErrorReply(s_errorInvalidScreen, s_errorInvalidScreenMessage);
        return QVariantMap();
    }

    const int fileDescriptor = dup(pipe.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(screen, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureActiveScreen(const QVariantMap &options,
                                                          QDBusUnixFileDescriptor pipe)
{
    if (!checkPermissions()) {
        return QVariantMap();
    }

    EffectScreen *screen = effects->activeScreen();
    if (!screen) {
        sendErrorReply(s_errorInvalidScreen, s_errorInvalidScreenMessage);
        return QVariantMap();
    }

    const int fileDescriptor = dup(pipe.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    takeScreenShot(screen, screenShotFlagsFromOptions(options),
                   new ScreenShotSinkPipe2(fileDescriptor, message()));

    setDelayedReply(true);
    return QVariantMap();
}

QVariantMap ScreenShotDBusInterface2::CaptureInteractive(uint kind,
                                                         const QVariantMap &options,
                                                         QDBusUnixFileDescriptor pipe)
{
    const int fileDescriptor = dup(pipe.fileDescriptor());
    if (fileDescriptor == -1) {
        sendErrorReply(s_errorFileDescriptor, s_errorFileDescriptorMessage);
        return QVariantMap();
    }

    const QDBusMessage replyMessage = message();

    if (kind == 0) {
        effects->startInteractiveWindowSelection([=, this](EffectWindow *window) {
            effects->hideOnScreenMessage(EffectsHandler::OnScreenMessageHideFlag::SkipsCloseAnimation);

            if (!window) {
                close(fileDescriptor);

                QDBusConnection bus = QDBusConnection::sessionBus();
                bus.send(replyMessage.createErrorReply(s_errorCancelled, s_errorCancelledMessage));
            } else {
                takeScreenShot(window, screenShotFlagsFromOptions(options),
                               new ScreenShotSinkPipe2(fileDescriptor, replyMessage));
            }
        });
        effects->showOnScreenMessage(i18n("Select window to screen shot with left click or enter.\n"
                                          "Escape or right click to cancel."),
                                     QStringLiteral("spectacle"));
    } else {
        effects->startInteractivePositionSelection([=, this](const QPoint &point) {
            effects->hideOnScreenMessage(EffectsHandler::OnScreenMessageHideFlag::SkipsCloseAnimation);

            if (point == QPoint(-1, -1)) {
                close(fileDescriptor);

                QDBusConnection bus = QDBusConnection::sessionBus();
                bus.send(replyMessage.createErrorReply(s_errorCancelled, s_errorCancelledMessage));
            } else {
                EffectScreen *screen = effects->screenAt(point);
                takeScreenShot(screen, screenShotFlagsFromOptions(options),
                               new ScreenShotSinkPipe2(fileDescriptor, replyMessage));
            }
        });
        effects->showOnScreenMessage(i18n("Create screen shot with left click or enter.\n"
                                          "Escape or right click to cancel."),
                                     QStringLiteral("spectacle"));
    }

    setDelayedReply(true);
    return QVariantMap();
}

void ScreenShotDBusInterface2::bind(ScreenShotSinkPipe2 *sink, ScreenShotSource2 *source)
{
    connect(source, &ScreenShotSource2::cancelled, sink, [sink, source]() {
        sink->cancel();

        sink->deleteLater();
        source->deleteLater();
    });

    connect(source, &ScreenShotSource2::completed, sink, [sink, source]() {
        source->marshal(sink);

        sink->deleteLater();
        source->deleteLater();
    });
}

void ScreenShotDBusInterface2::takeScreenShot(EffectScreen *screen, ScreenShotFlags flags,
                                              ScreenShotSinkPipe2 *sink)
{
    bind(sink, new ScreenShotSourceScreen2(m_effect, screen, flags));
}

void ScreenShotDBusInterface2::takeScreenShot(const QRect &area, ScreenShotFlags flags,
                                              ScreenShotSinkPipe2 *sink)
{
    bind(sink, new ScreenShotSourceArea2(m_effect, area, flags));
}

void ScreenShotDBusInterface2::takeScreenShot(EffectWindow *window, ScreenShotFlags flags,
                                              ScreenShotSinkPipe2 *sink)
{
    bind(sink, new ScreenShotSourceWindow2(m_effect, window, flags));
}

} // namespace KWin

#include "screenshotdbusinterface2.moc"
