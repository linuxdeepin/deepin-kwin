/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xwaylandlauncher.h"

#include <config-kwin.h>

#include "utils/dconfig_reader.h"
#include "xwayland_logging.h"
#include "xwaylandsocket.h"

#include "options.h"
#include "wayland_server.h"

#if KWIN_BUILD_NOTIFICATIONS
#include <KLocalizedString>
#include <KNotification>
#endif

#include <QAbstractEventDispatcher>
#include <QDataStream>
#include <QFile>
#include <QHostInfo>
#include <QRandomGenerator>
#if (QT_VERSION > QT_VERSION_CHECK(5, 11, 3))
#include <QScopeGuard>
#else
#include "utils/qscopeguard.h"
#endif
#include <QTimer>
#include <QtConcurrent>

// system
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace KWin
{
namespace Xwl
{

XwaylandLauncher::XwaylandLauncher(QObject *parent)
    : QObject(parent)
{
    m_launcherThread = new QThread();
    // Important: Move object to thread before connecting its own slots
    // or creating child QObjects like timers that should live in this thread.
    this->moveToThread(m_launcherThread);
    m_launcherThread->start();

    m_resetCrashCountTimer = new QTimer(this); // Parented, so will move to m_launcherThread
    m_resetCrashCountTimer->setSingleShot(true);
    connect(m_resetCrashCountTimer, &QTimer::timeout, this, &XwaylandLauncher::resetCrashCount);

    // Connections for signal/slot based public API
}

XwaylandLauncher::~XwaylandLauncher()
{
    // Stop and cleanup the launcher's own thread first
    if (m_launcherThread) {
        // If stop() was not called explicitly, ensure resources are cleaned up.
        // This needs to be invoked on the launcher's thread.
        // QMetaObject::invokeMethod is used for safety if destructor is called from another thread.
        QMetaObject::invokeMethod(this, "onStopRequested", Qt::BlockingQueuedConnection);
        m_launcherThread->quit();
        m_launcherThread->wait(); // Wait for the launcher thread to finish
        m_launcherThread->deleteLater();
    }
}

// Public methods use QMetaObject::invokeMethod to call slots on the launcher thread

void XwaylandLauncher::setListenFDs(const QVector<int> &listenFds)
{
    QMetaObject::invokeMethod(this, "onSetListenFDsRequested", Qt::QueuedConnection, Q_ARG(QVector<int>, listenFds));
}

void XwaylandLauncher::setDisplayName(const QString &displayName)
{
    QMetaObject::invokeMethod(this, "onSetDisplayNameRequested", Qt::QueuedConnection, Q_ARG(QString, displayName));
}

void XwaylandLauncher::setXauthority(const QString &xauthority)
{
    QMetaObject::invokeMethod(this, "onSetXauthorityRequested", Qt::QueuedConnection, Q_ARG(QString, xauthority));
}

void XwaylandLauncher::start()
{
    QMetaObject::invokeMethod(this, "onStartRequested", Qt::QueuedConnection);
}

void XwaylandLauncher::stop()
{
    QMetaObject::invokeMethod(this, "onStopRequested", Qt::QueuedConnection);
}

// Slots that implement the actual logic, executed in XwaylandLauncher's thread

void XwaylandLauncher::onSetListenFDsRequested(const QVector<int> &listenFds)
{
    m_listenFds = listenFds;
}

void XwaylandLauncher::onSetDisplayNameRequested(const QString &displayName)
{
    m_displayName = displayName;
}

void XwaylandLauncher::onSetXauthorityRequested(const QString &xauthority)
{
    m_xAuthority = xauthority;
}

void XwaylandLauncher::onStartRequested()
{
    if (m_xwaylandProcess) {
        qCDebug(KWIN_XWL) << "Xwayland already running or start requested.";
        return;
    }

    if (!m_listenFds.isEmpty()) {
        Q_ASSERT(!m_displayName.isEmpty());
    } else {
        m_socket.reset(new XwaylandSocket(XwaylandSocket::OperationMode::CloseFdsOnExec));
        if (!m_socket->isValid()) {
            // qFatal terminates, which is harsh. Emit error and return.
            qCWarning(KWIN_XWL, "Failed to establish X11 socket for Xwayland.");
            Q_EMIT errorOccurred(); // Ensure this signal is emitted from the correct thread
            return;
        }
        m_displayName = m_socket->name();
        m_listenFds = m_socket->fileDescriptors();
    }

    if (!startInternal()) {
        // errorOccurred should have been emitted by startInternal
        qCWarning(KWIN_XWL, "Xwayland startInternal failed.");
    }
}

void XwaylandLauncher::onStopRequested()
{
    if (!m_xwaylandProcess) {
        qCDebug(KWIN_XWL) << "Xwayland not running or stop already requested.";
        return;
    }
    stopInternal();
}

// Internal methods (called by slots, so they run in m_launcherThread)

bool XwaylandLauncher::startInternal()
{
    Q_ASSERT(!m_xwaylandProcess); // Should be true due to check in onStartRequested

    QVector<int> fdsToClose;
    auto cleanup = qScopeGuard([&fdsToClose] {
        for (const int fd : std::as_const(fdsToClose)) {
            close(fd);
        }
    });

    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        qCWarning(KWIN_XWL, "Failed to create pipe to start Xwayland: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }
    fdsToClose << pipeFds[1];

    int sx[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sx) < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }
    int fd = dup(sx[1]);
    if (fd < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for XCB connection: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }

    const int waylandSocket = waylandServer()->createXWaylandConnection();
    if (waylandSocket == -1) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }
    const int wlfd = dup(waylandSocket);
    if (wlfd < 0) {
        qCWarning(KWIN_XWL, "Failed to open socket for Xwayland server: %s", strerror(errno));
        Q_EMIT errorOccurred();
        return false;
    }

    m_xcbConnectionFd = sx[0];

    QStringList arguments;

    arguments << m_displayName;

    if (!m_listenFds.isEmpty()) {
        // xauthority externally set and managed
        if (!m_xAuthority.isEmpty()) {
            arguments << QStringLiteral("-auth") << m_xAuthority;
        }

        for (int socket : std::as_const(m_listenFds)) {
            int dupSocket = dup(socket);
            fdsToClose << dupSocket;
#if HAVE_XWAYLAND_LISTENFD
            arguments << QStringLiteral("-listenfd") << QString::number(dupSocket);
#else
            arguments << QStringLiteral("-listen") << QString::number(dupSocket);
#endif
        }
    }

    arguments << QStringLiteral("-displayfd") << QString::number(pipeFds[1]);
    arguments << QStringLiteral("-rootless");
    arguments << QStringLiteral("-wm") << QString::number(fd);
    arguments << QStringLiteral("-verbose") << QString::number(4);

    m_xwaylandProcess = new QProcess(this); // Parented, so will move to m_launcherThread
    m_xwaylandProcess->setProgram(QStringLiteral("Xwayland"));
    m_xwaylandProcess->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("WAYLAND_DEBUG"));
    env.insert("WAYLAND_SOCKET", QByteArray::number(wlfd));
    if (qEnvironmentVariableIsSet("KWIN_XWAYLAND_DEBUG")) {
        env.insert("WAYLAND_DEBUG", QByteArrayLiteral("1"));
    }
    bool forceGlamorSupport = false;
    if (DconfigRead(QStringLiteral("org.kde.kwin.xwayland"), QStringLiteral("forceGlamorSupport"), forceGlamorSupport) == DconfigReaderRetCode::DCONF_SUCCESS) {
        if (forceGlamorSupport) {
            env.insert("XWAYLAND_FORCE_SUPPORT_GLAMOR", QByteArrayLiteral("1"));
        }
    }
    m_xwaylandProcess->setProcessEnvironment(env);
    m_xwaylandProcess->setArguments(arguments);
    connect(m_xwaylandProcess, &QProcess::errorOccurred, this, &XwaylandLauncher::handleXwaylandError);
    connect(m_xwaylandProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &XwaylandLauncher::handleXwaylandFinished);
    connect(m_xwaylandProcess, &QProcess::readyReadStandardOutput, this, [this] {
        QByteArray buffer;
        buffer.append(m_xwaylandProcess->readAllStandardOutput());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QtConcurrent::run(this, &XwaylandLauncher::processXwaylandOutput, buffer);
#else
        QtConcurrent::run(&XwaylandLauncher::processXwaylandOutput, this, buffer);
#endif
    });

    // When Xwayland starts writing the display name to displayfd, it is ready. Alternatively,
    // the Xwayland can send us the SIGUSR1 signal, but it's already reserved for VT hand-off.
    m_readyNotifier = new QSocketNotifier(pipeFds[0], QSocketNotifier::Read, this);
    connect(m_readyNotifier, &QSocketNotifier::activated, this, [this]() {
        // This lambda will execute in m_launcherThread
        maybeDestroyReadyNotifier();
        Q_EMIT started(m_displayName, m_xAuthority, m_xcbConnectionFd);
    });

    m_xwaylandProcess->start();

    return true;
}

void XwaylandLauncher::stopInternal()
{
    Q_EMIT finished();

    maybeDestroyReadyNotifier();
    waylandServer()->destroyXWaylandConnection();

    // When the Xwayland process is finally terminated, the finished() signal will be emitted,
    // however we don't actually want to process it anymore. Furthermore, we also don't really
    // want to handle any errors that may occur during the teardown.
    if (m_xwaylandProcess->state() != QProcess::NotRunning) {
        disconnect(m_xwaylandProcess, nullptr, this, nullptr);
        m_xwaylandProcess->terminate();
        m_xwaylandProcess->waitForFinished(5000);
    }
    delete m_xwaylandProcess;
    m_xwaylandProcess = nullptr;
}

void XwaylandLauncher::restartInternal()
{
    if (m_xwaylandProcess) {
        stopInternal();
    }
    startInternal();
}

void XwaylandLauncher::maybeDestroyReadyNotifier()
{
    if (m_readyNotifier) {
        close(m_readyNotifier->socket());

        delete m_readyNotifier;
        m_readyNotifier = nullptr;
    }
}

void XwaylandLauncher::handleXwaylandFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qCDebug(KWIN_XWL) << "Xwayland process has quit with exit status:" << exitStatus << "exit code:" << exitCode;

#if KWIN_BUILD_NOTIFICATIONS
    KNotification::event(QStringLiteral("xwaylandcrash"), i18n("Xwayland has crashed"));
#endif
    m_resetCrashCountTimer->stop();

    switch (options->xwaylandCrashPolicy()) {
    case XwaylandCrashPolicy::Restart:
        if (++m_crashCount <= options->xwaylandMaxCrashCount()) {
            restartInternal();
            m_resetCrashCountTimer->start(std::chrono::minutes(10));
        } else {
            qCWarning(KWIN_XWL, "Stopping Xwayland server because it has crashed %d times "
                                "over the past 10 minutes",
                      m_crashCount);
            stop();
        }
        break;
    case XwaylandCrashPolicy::Stop:
        stop();
        break;
    }
}

void XwaylandLauncher::resetCrashCount()
{
    qCDebug(KWIN_XWL) << "Resetting the crash counter, its current value is" << m_crashCount;
    m_crashCount = 0;
}

void XwaylandLauncher::handleXwaylandError(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart:
        qCWarning(KWIN_XWL) << "Xwayland process failed to start";
        return;
    case QProcess::Crashed:
        qCWarning(KWIN_XWL) << "Xwayland process crashed";
        break;
    case QProcess::Timedout:
        qCWarning(KWIN_XWL) << "Xwayland operation timed out";
        break;
    case QProcess::WriteError:
    case QProcess::ReadError:
        qCWarning(KWIN_XWL) << "An error occurred while communicating with Xwayland";
        break;
    case QProcess::UnknownError:
        qCWarning(KWIN_XWL) << "An unknown error has occurred in Xwayland";
        break;
    }
    Q_EMIT errorOccurred();
}

void XwaylandLauncher::processXwaylandOutput(QByteArray buffer)
{
    while (true) {

        // Find the position of the newline character
        int newlinePos = buffer.indexOf('\n');
        if (newlinePos == -1) {
            break;
        }

        // Extract a line of data
        QByteArray line = buffer.left(newlinePos);
        // Remove the processed data (including the newline character) from the buffer
        buffer.remove(0, newlinePos + 1);

        // Process and log the output
        qCInfo(KWIN_XWL).noquote() << "[xwayland]" << line.trimmed();
    }
}
}
}
