// Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "main_wayland.h"
#include "composite.h"
#include "virtualkeyboard.h"
#include "workspace.h"
#include "log.h"
#include <config-kwin.h>
// kwin
#include "platform.h"
#include "effects.h"
#include "tabletmodemanager.h"
#include "report.h"

#ifdef PipeWire_FOUND
#include "screencast/screencastmanager.h"
#endif
#include "wayland_server.h"
#include "xcbutils.h"
#include "xwl/xwayland.h"

// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/seat_interface.h>
// KDE
#include <KLocalizedString>
#include <KPluginLoader>
#include <KPluginMetaData>
#include <KCrash>
#include <KQuickAddons/QtQuickSettings>

// Qt
#include <qplatformdefs.h>
#include <QAbstractEventDispatcher>
#include <QCommandLineParser>
#include <QtConcurrentRun>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QProcess>
#include <QSocketNotifier>
#include <QStyle>
#include <QThread>
#include <QDebug>
#include <QWindow>

// system
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif // HAVE_UNISTD_H

#if HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#if HAVE_SYS_PROCCTL_H
#include <unistd.h>
#include <sys/procctl.h>
#endif

#if HAVE_LIBCAP
#include <sys/capability.h>
#endif

#include <sched.h>

#include <iostream>
#include <iomanip>
#include <xkb.h>

namespace KWin
{

static void sighandler(int)
{
    QApplication::exit();
}

void disableDrKonqi()
{
    KCrash::setDrKonqiEnabled(false);
}
// run immediately, before Q_CORE_STARTUP functions
// that would enable drkonqi
Q_CONSTRUCTOR_FUNCTION(disableDrKonqi)

static void readDisplay(int pipe);

enum class RealTimeFlags
{
    DontReset,
    ResetOnFork
};

namespace {
void gainRealTime(RealTimeFlags flags = RealTimeFlags::DontReset)
{
#if HAVE_SCHED_RESET_ON_FORK
    const int minPriority = sched_get_priority_min(SCHED_RR);
    struct sched_param sp;
    sp.sched_priority = minPriority;
    int policy = SCHED_RR;
    if (flags == RealTimeFlags::ResetOnFork) {
        policy |= SCHED_RESET_ON_FORK;
    }
    sched_setscheduler(0, policy, &sp);
#else
    Q_UNUSED(flags);
#endif
}
}

//************************************
// ApplicationWayland
//************************************

ApplicationWayland::ApplicationWayland(int &argc, char **argv)
    : ApplicationWaylandAbstract(OperationModeWaylandOnly, argc, argv)
{
}

ApplicationWayland::~ApplicationWayland()
{
    setTerminating();
    if (!waylandServer()) {
        return;
    }

    if (kwinApp()->platform()) {
        kwinApp()->platform()->setOutputsEnabled(false);
    }
    // need to unload all effects prior to destroying X connection as they might do X calls
    if (effects) {
        static_cast<EffectsHandlerImpl*>(effects)->unloadAllEffects();
    }
    if (m_xwayland) {
        // needs to be done before workspace gets destroyed
        m_xwayland->prepareDestroy();
    }
    destroyWorkspace();
    waylandServer()->dispatch();

    if (QStyle *s = style()) {
        s->unpolish(this);
    }
    // kill Xwayland before terminating its connection
    delete m_xwayland;
    m_xwayland = nullptr;
    waylandServer()->terminateClientConnections();
    destroyCompositor();
    qDebug() << QDateTime::currentDateTime().toString(QString::fromLatin1("hh:mm:ss.zzz")) << Q_FUNC_INFO << "finish";
}

void ApplicationWayland::performStartup()
{
    if (m_startXWayland) {
        setOperationMode(OperationModeXwayland);
    }
    // first load options - done internally by a different thread
    createOptions();
    waylandServer()->createInternalConnection();

    // try creating the Wayland Backend
    createInput();
    // now libinput thread has been created, adjust scheduler to not leak into other processes
    gainRealTime(RealTimeFlags::ResetOnFork);

    VirtualKeyboard::create(this);
    createBackend();
    TabletModeManager::create(this);
#ifdef PipeWire_FOUND
    new ScreencastManager(this);
#endif
}

void ApplicationWayland::createBackend()
{
    connect(platform(), &Platform::screensQueried, this, &ApplicationWayland::continueStartupWithScreens);
    connect(platform(), &Platform::initFailed, this,
        [] () {
            std::cerr <<  "FATAL ERROR: backend failed to initialize, exiting now" << std::endl;
            QCoreApplication::exit(1);
        }
    );
    connect(platform(), &Platform::startWithoutScreen, this, &ApplicationWayland::continueStartupWithoutScreens);
    if (m_disableMultiScreens) {
        platform()->disableMultiScreens();
    }
    platform()->init();
}

void ApplicationWayland::continueStartupWithScreens()
{
    disconnect(kwinApp()->platform(), &Platform::screensQueried, this, &ApplicationWayland::continueStartupWithScreens);
    disconnect(kwinApp()->platform(), &Platform::startWithoutScreen, this, &ApplicationWayland::continueStartupWithoutScreens);
    createScreens();

    if (operationMode() == OperationModeWaylandOnly) {
        createCompositor();
        connect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::continueStartupWithScene);
        return;
    }
    createCompositor();
    connect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::continueStartupWithXwayland);
}

void ApplicationWayland::continueStartupWithoutScreens()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss") << Q_FUNC_INFO
            << " ut-gfx-start: operationMode " << operationMode() << " withoutScreen " << m_runWithoutScreen;

    disconnect(kwinApp()->platform(), &Platform::startWithoutScreen, this, &ApplicationWayland::continueStartupWithoutScreens);
    if (!m_runWithoutScreen) {
        return;
    }
    disconnect(kwinApp()->platform(), &Platform::screensQueried, this, &ApplicationWayland::continueStartupWithScreens);

    platform()->installDefaultDisplay();

    if (operationMode() == OperationModeWaylandOnly) {
        createCompositor();
        connect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::continueStartupWithScene);
        return;
    }
    createCompositor();
}

void ApplicationWayland::continueStartupWithScene()
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss") << Q_FUNC_INFO << " ut-gfx-start";
    disconnect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::continueStartupWithScene);
    startSession();
    createWorkspace();
    notifyKSplash();
}

void ApplicationWayland::continueStartupWithXwayland()
{
    disconnect(Compositor::self(), &Compositor::sceneCreated, this, &ApplicationWayland::continueStartupWithXwayland);

    m_xwayland = new Xwl::Xwayland(this);
    connect(m_xwayland, &Xwl::Xwayland::criticalError, this, [](int code) {
        // we currently exit on Xwayland errors always directly
        // TODO: restart Xwayland
        std::cerr << "Xwayland had a critical error. Going to exit now." << std::endl;
        exit(code);
    });
    m_xwayland->init();
}

void ApplicationWayland::startSession()
{
    if (!m_inputMethodServerToStart.isEmpty()) {
        int socket = dup(waylandServer()->createInputMethodConnection());
        if (socket >= 0) {
            QProcessEnvironment environment = processStartupEnvironment();
            environment.insert(QStringLiteral("WAYLAND_SOCKET"), QByteArray::number(socket));
            environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
            environment.remove("DISPLAY");
            environment.remove("WAYLAND_DISPLAY");
            QProcess *p = new Process(this);
            p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
            auto finishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
            connect(p, finishedSignal, this,
                [this, p] {
                    if (waylandServer()) {
                        waylandServer()->destroyInputMethodConnection();
                    }
                    p->deleteLater();
                }
            );
            p->setProcessEnvironment(environment);
            p->start(m_inputMethodServerToStart);
            p->waitForStarted();
        }
    }

    // start session
    if (!m_sessionArgument.isEmpty()) {
        QProcess *p = new Process(this);
        p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        QProcessEnvironment environment = processStartupEnvironment();
        //sonald: remove dde-kwin-wayland preload
        environment.remove("LD_PRELOAD");
        p->setProcessEnvironment(environment);
        auto finishedSignal = static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished);
        connect(p, finishedSignal, this, &ApplicationWayland::quit);
        p->start(m_sessionArgument);
    }
    // start the applications passed to us as command line arguments
    if (!m_applicationsToStart.isEmpty()) {
        for (const QString &application: m_applicationsToStart) {
            // note: this will kill the started process when we exit
            // this is going to happen anyway as we are the wayland and X server the app connects to
            QProcess *p = new Process(this);
            QProcessEnvironment environment = processStartupEnvironment();
            //sonald: remove dde-kwin-wayland preload
            environment.remove("LD_PRELOAD");
            environment.remove("QT_MESSAGE_PATTERN");
            p->setProcessEnvironment(environment);
            p->setProcessChannelMode(QProcess::ForwardedErrorChannel);
            p->start(application);
        }
    }
}

static const QString s_waylandPlugin = QStringLiteral("KWinWaylandWaylandBackend");
static const QString s_x11Plugin = QStringLiteral("KWinWaylandX11Backend");
static const QString s_fbdevPlugin = QStringLiteral("KWinWaylandFbdevBackend");
#if HAVE_DRM
static const QString s_drmPlugin = QStringLiteral("KWinWaylandDrmBackend");
#endif
#if HAVE_LIBHYBRIS
static const QString s_hwcomposerPlugin = QStringLiteral("KWinWaylandHwcomposerBackend");
#endif
static const QString s_virtualPlugin = QStringLiteral("KWinWaylandVirtualBackend");

static QString automaticBackendSelection()
{
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
        return s_waylandPlugin;
    }
    if (qEnvironmentVariableIsSet("DISPLAY")) {
        return s_x11Plugin;
    }
#if HAVE_LIBHYBRIS
    if (qEnvironmentVariableIsSet("ANDROID_ROOT")) {
        return s_hwcomposerPlugin;
    }
#endif
#if HAVE_DRM
    return s_drmPlugin;
#endif
    return s_fbdevPlugin;
}

static void disablePtrace()
{
#if HAVE_PR_SET_DUMPABLE
    // check whether we are running under a debugger
    const QFileInfo parent(QStringLiteral("/proc/%1/exe").arg(getppid()));
    if (parent.isSymLink() &&
            (parent.symLinkTarget().endsWith(QLatin1String("/gdb")) ||
             parent.symLinkTarget().endsWith(QLatin1String("/gdbserver")))) {
        // debugger, don't adjust
        return;
    }

    // disable ptrace in kwin_wayland
    prctl(PR_SET_DUMPABLE, 0);
#endif
#if HAVE_PROC_TRACE_CTL
    // FreeBSD's rudimentary procfs does not support /proc/<pid>/exe
    // We could use the P_TRACED flag of the process to find out
    // if the process is being debugged ond FreeBSD.
    int mode = PROC_TRACE_CTL_DISABLE;
    procctl(P_PID, getpid(), PROC_TRACE_CTL, &mode);
#endif

}

static void unsetDumpable(int sig)
{
#if HAVE_PR_SET_DUMPABLE
    prctl(PR_SET_DUMPABLE, 1);
#endif
    signal(sig, SIG_IGN);
    raise(sig);
    return;
}

void dropNiceCapability()
{
#if HAVE_LIBCAP
    cap_t caps = cap_get_proc();
    if (!caps) {
        return;
    }
    cap_value_t capList[] = { CAP_SYS_NICE };
    if (cap_set_flag(caps, CAP_PERMITTED, 1, capList, CAP_CLEAR) == -1) {
        cap_free(caps);
        return;
    }
    if (cap_set_flag(caps, CAP_EFFECTIVE, 1, capList, CAP_CLEAR) == -1) {
        cap_free(caps);
        return;
    }
    cap_set_proc(caps);
    cap_free(caps);
#endif
}

} // namespace
void customLogMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    QString kwinCategory = QString::fromUtf8(qgetenv("KWIN_LOG_CATEGORY"));
    if (!kwinCategory.isEmpty() && kwinCategory != ctx.category) {
        return;
    }

    bool timeFlag = true;
    QString kwinTime = QString::fromUtf8(qgetenv("KWIN_LOG_TIME"));
    if (!kwinTime.isEmpty() && kwinTime.toLower() == "false") {
        timeFlag = false;
    }

    QString logInfo = "Default";
    switch (type) {
    case QtDebugMsg:
        logInfo = QString("Debug");
        break;
    case QtWarningMsg:
        logInfo = QString("Warning");
        break;
    case QtCriticalMsg:
        logInfo = QString("Critical");
        break;
    case QtFatalMsg:
        logInfo = QString("Fatal");
        break;
    case QtInfoMsg:
        logInfo = QString("Info");
        break;
    }

    pid_t pid = getpid();
    QString stdmessage = "";
    QString d_message = QString("[%1][%2] %3").arg(logInfo).arg(ctx.category).arg(msg);
    if (timeFlag) {
        QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        stdmessage = QString("%1 [%2][%3][%4] %5").arg(currentDateTime).arg(pid).arg(logInfo).arg(ctx.category).arg(msg);
    } else {
        stdmessage = QString("[%1][%2][%3] %4").arg(pid).arg(logInfo).arg(ctx.category).arg(msg);
    }
    std::cout << stdmessage.toStdString() << std::endl;

    const char * cMsg = d_message.toLatin1().data();
    switch (type) {
    case QtDebugMsg:
        DLOGD("%s", cMsg);
        break;
    case QtWarningMsg:
        DLOGW("%s", cMsg);
        break;
    case QtCriticalMsg:
        DLOGC("%s", cMsg);
        break;
    case QtInfoMsg:
        DLOGI("%s", cMsg);
        break;
    }
}

int main(int argc, char * argv[])
{
    if (getuid() == 0) {
        std::cerr << "kwin_wayland does not support running as root." << std::endl;
        return 1;
    }
    QString kwinLog = QString::fromUtf8(qgetenv("KWIN_LOG"));
    // we reorient print kwin log to syslog only if we set KWIN_LOG = true
    if (kwinLog.isEmpty() || kwinLog.toLower() == "false") {
        qDebug() << QDateTime::currentDateTime().toString("yyyy-mm-dd hh:mm:ss") << Q_FUNC_INFO \
            << " do not print kwin log to syslog";
    } else {
        qInstallMessageHandler(customLogMessageHandler);
    }

    KWin::Report::eventLog::instance()->init();

    KWin::disablePtrace();
    KWin::Application::setupMalloc();
    KWin::Application::setupLocalizedString();
    KWin::gainRealTime();
    KWin::dropNiceCapability();

    if (signal(SIGTERM, KWin::sighandler) == SIG_IGN)
        signal(SIGTERM, SIG_IGN);
    if (signal(SIGINT, KWin::sighandler) == SIG_IGN)
        signal(SIGINT, SIG_IGN);
    if (signal(SIGHUP, KWin::sighandler) == SIG_IGN)
        signal(SIGHUP, SIG_IGN);
    signal(SIGABRT, KWin::unsetDumpable);
    signal(SIGSEGV, KWin::unsetDumpable);
    signal(SIGPIPE, SIG_IGN);
    // ensure that no thread takes SIGUSR
    sigset_t userSignals;
    sigemptyset(&userSignals);
    sigaddset(&userSignals, SIGUSR1);
    sigaddset(&userSignals, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &userSignals, nullptr);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // enforce our internal qpa plugin, unfortunately command line switch has precedence
    setenv("QT_QPA_PLATFORM", "wayland-org.kde.kwin.qpa", true);

    qunsetenv("QT_DEVICE_PIXEL_RATIO");
    qputenv("QT_IM_MODULE", "qtvirtualkeyboard");
    qputenv("QSG_RENDER_LOOP", "basic");
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    KWin::ApplicationWayland a(argc, argv);
    a.setupTranslator();
    // reset QT_QPA_PLATFORM to a sane value for any processes started from KWin
    setenv("QT_QPA_PLATFORM", "wayland", true);

    KWin::Application::createAboutData();
    KQuickAddons::QtQuickSettings::init();

    const auto availablePlugins = KPluginLoader::findPlugins(QStringLiteral("org.kde.kwin.waylandbackends"));
    auto hasPlugin = [&availablePlugins] (const QString &name) {
        return std::any_of(availablePlugins.begin(), availablePlugins.end(),
            [name] (const KPluginMetaData &plugin) {
                return plugin.pluginId() == name;
            }
        );
    };
    const bool hasSizeOption = hasPlugin(KWin::s_x11Plugin) || hasPlugin(KWin::s_virtualPlugin);
    const bool hasOutputCountOption = hasPlugin(KWin::s_x11Plugin);
    const bool hasX11Option = hasPlugin(KWin::s_x11Plugin);
    const bool hasVirtualOption = hasPlugin(KWin::s_virtualPlugin);
    const bool hasWaylandOption = hasPlugin(KWin::s_waylandPlugin);
    const bool hasFramebufferOption = hasPlugin(KWin::s_fbdevPlugin);
#if HAVE_DRM
    const bool hasDrmOption = hasPlugin(KWin::s_drmPlugin);
#endif
#if HAVE_LIBHYBRIS
    const bool hasHwcomposerOption = hasPlugin(KWin::s_hwcomposerPlugin);
#endif

    QCommandLineOption xwaylandOption(QStringLiteral("xwayland"),
                                      i18n("Start a rootless Xwayland server."));
    QCommandLineOption waylandSocketOption(QStringList{QStringLiteral("s"), QStringLiteral("socket")},
                                           i18n("Name of the Wayland socket to listen on. If not set \"wayland-0\" is used."),
                                           QStringLiteral("socket"));
    QCommandLineOption framebufferOption(QStringLiteral("framebuffer"),
                                         i18n("Render to framebuffer."));
    QCommandLineOption framebufferDeviceOption(QStringLiteral("fb-device"),
                                               i18n("The framebuffer device to render to."),
                                               QStringLiteral("fbdev"));
    QCommandLineOption x11DisplayOption(QStringLiteral("x11-display"),
                                        i18n("The X11 Display to use in windowed mode on platform X11."),
                                        QStringLiteral("display"));
    QCommandLineOption waylandDisplayOption(QStringLiteral("wayland-display"),
                                            i18n("The Wayland Display to use in windowed mode on platform Wayland."),
                                            QStringLiteral("display"));
    QCommandLineOption virtualFbOption(QStringLiteral("virtual"), i18n("Render to a virtual framebuffer."));
    QCommandLineOption widthOption(QStringLiteral("width"),
                                   i18n("The width for windowed mode. Default width is 1024."),
                                   QStringLiteral("width"));
    widthOption.setDefaultValue(QString::number(1024));
    QCommandLineOption heightOption(QStringLiteral("height"),
                                    i18n("The height for windowed mode. Default height is 768."),
                                    QStringLiteral("height"));
    heightOption.setDefaultValue(QString::number(768));

    QCommandLineOption scaleOption(QStringLiteral("scale"),
                                    i18n("The scale for windowed mode. Default value is 1."),
                                    QStringLiteral("scale"));
    scaleOption.setDefaultValue(QString::number(1));

    QCommandLineOption outputCountOption(QStringLiteral("output-count"),
                                    i18n("The number of windows to open as outputs in windowed mode. Default value is 1"),
                                    QStringLiteral("height"));
    outputCountOption.setDefaultValue(QString::number(1));

    QCommandLineOption withoutscreenOption(QStringLiteral("withoutscreen"),
                                      i18n("Start kwin without screen."));

    QCommandLineOption disableMultiScreens(QStringLiteral("disable-multiscreens"),
                                      i18n("Disable multi screens"));

    QCommandLineParser parser;
    a.setupCommandLine(&parser);
    parser.addOption(xwaylandOption);
    parser.addOption(withoutscreenOption);
    parser.addOption(disableMultiScreens);
    parser.addOption(waylandSocketOption);
    if (hasX11Option) {
        parser.addOption(x11DisplayOption);
    }
    if (hasWaylandOption) {
        parser.addOption(waylandDisplayOption);
    }
    if (hasFramebufferOption) {
        parser.addOption(framebufferOption);
        parser.addOption(framebufferDeviceOption);
    }
    if (hasVirtualOption) {
        parser.addOption(virtualFbOption);
    }
    if (hasSizeOption) {
        parser.addOption(widthOption);
        parser.addOption(heightOption);
        parser.addOption(scaleOption);
    }
    if (hasOutputCountOption) {
        parser.addOption(outputCountOption);
    }
#if HAVE_LIBHYBRIS
    QCommandLineOption hwcomposerOption(QStringLiteral("hwcomposer"), i18n("Use libhybris hwcomposer"));
    if (hasHwcomposerOption) {
        parser.addOption(hwcomposerOption);
    }
#endif
    QCommandLineOption libinputOption(QStringLiteral("libinput"),
                                      i18n("Enable libinput support for input events processing. Note: never use in a nested session."));
    parser.addOption(libinputOption);
#if HAVE_DRM
    QCommandLineOption drmOption(QStringLiteral("drm"), i18n("Render through drm node."));
    if (hasDrmOption) {
        parser.addOption(drmOption);
    }
#endif

    QCommandLineOption inputMethodOption(QStringLiteral("inputmethod"),
                                         i18n("Input method that KWin starts."),
                                         QStringLiteral("path/to/imserver"));
    parser.addOption(inputMethodOption);

    QCommandLineOption listBackendsOption(QStringLiteral("list-backends"),
                                           i18n("List all available backends and quit."));
    parser.addOption(listBackendsOption);

    QCommandLineOption screenLockerOption(QStringLiteral("lockscreen"),
                                          i18n("Starts the session in locked mode."));
    parser.addOption(screenLockerOption);

    QCommandLineOption noScreenLockerOption(QStringLiteral("no-lockscreen"),
                                            i18n("Starts the session without lock screen support."));
    parser.addOption(noScreenLockerOption);

    QCommandLineOption noGlobalShortcutsOption(QStringLiteral("no-global-shortcuts"),
                                               i18n("Starts the session without global shortcuts support."));
    parser.addOption(noGlobalShortcutsOption);

    QCommandLineOption exitWithSessionOption(QStringLiteral("exit-with-session"),
                                             i18n("Exit after the session application, which is started by KWin, closed."),
                                             QStringLiteral("/path/to/session"));
    parser.addOption(exitWithSessionOption);

    parser.addPositionalArgument(QStringLiteral("applications"),
                                 i18n("Applications to start once Wayland and Xwayland server are started"),
                                 QStringLiteral("[/path/to/application...]"));

    parser.process(a);
    a.processCommandLine(&parser);

#ifdef KWIN_BUILD_ACTIVITIES
    a.setUseKActivities(false);
#endif

    if (parser.isSet(listBackendsOption)) {
        for (const auto &plugin: availablePlugins) {
            std::cout << std::setw(40) << std::left << qPrintable(plugin.name()) << qPrintable(plugin.description()) << std::endl;
        }
        return 0;
    }

    if (parser.isSet(exitWithSessionOption)) {
        a.setSessionArgument(parser.value(exitWithSessionOption));
    }

    KWin::Application::setUseLibinput(parser.isSet(libinputOption));

    QString pluginName;
    QSize initialWindowSize;
    QByteArray deviceIdentifier;
    int outputCount = 1;
    qreal outputScale = 1;

#if HAVE_DRM
    if (hasDrmOption && parser.isSet(drmOption)) {
        pluginName = KWin::s_drmPlugin;
    }
#endif

    if (hasSizeOption) {
        bool ok = false;
        const int width = parser.value(widthOption).toInt(&ok);
        if (!ok) {
            std::cerr << "FATAL ERROR incorrect value for width" << std::endl;
            return 1;
        }
        const int height = parser.value(heightOption).toInt(&ok);
        if (!ok) {
            std::cerr << "FATAL ERROR incorrect value for height" << std::endl;
            return 1;
        }
        const qreal scale = parser.value(scaleOption).toDouble(&ok);
        if (!ok || scale < 1) {
            std::cerr << "FATAL ERROR incorrect value for scale" << std::endl;
            return 1;
        }

        outputScale = scale;
        initialWindowSize = QSize(width, height);
    }

    if (hasOutputCountOption) {
        bool ok = false;
        const int count = parser.value(outputCountOption).toInt(&ok);
        if (ok) {
            outputCount = qMax(1, count);
        }
    }

    if (hasX11Option && parser.isSet(x11DisplayOption)) {
        deviceIdentifier = parser.value(x11DisplayOption).toUtf8();
        pluginName = KWin::s_x11Plugin;
    } else if (hasWaylandOption && parser.isSet(waylandDisplayOption)) {
        deviceIdentifier = parser.value(waylandDisplayOption).toUtf8();
        pluginName = KWin::s_waylandPlugin;
    }

    if (hasFramebufferOption && parser.isSet(framebufferOption)) {
        pluginName = KWin::s_fbdevPlugin;
        deviceIdentifier = parser.value(framebufferDeviceOption).toUtf8();
    }
#if HAVE_LIBHYBRIS
    if (hasHwcomposerOption && parser.isSet(hwcomposerOption)) {
        pluginName = KWin::s_hwcomposerPlugin;
    }
#endif
    if (hasVirtualOption && parser.isSet(virtualFbOption)) {
        pluginName = KWin::s_virtualPlugin;
    }

    if (pluginName.isEmpty()) {
        std::cerr << "No backend specified through command line argument, trying auto resolution" << std::endl;
        pluginName = KWin::automaticBackendSelection();
    }

    auto pluginIt = std::find_if(availablePlugins.begin(), availablePlugins.end(),
        [&pluginName] (const KPluginMetaData &plugin) {
            return plugin.pluginId() == pluginName;
        }
    );
    if (pluginIt == availablePlugins.end()) {
        std::cerr << "FATAL ERROR: could not find a backend" << std::endl;
        return 1;
    }

    // TODO: create backend without having the server running
    KWin::WaylandServer *server = KWin::WaylandServer::create(&a);

    KWin::WaylandServer::InitalizationFlags flags;
    if (parser.isSet(screenLockerOption)) {
        flags = KWin::WaylandServer::InitalizationFlag::LockScreen;
    } else if (parser.isSet(noScreenLockerOption)) {
        flags = KWin::WaylandServer::InitalizationFlag::NoLockScreenIntegration;
    }
    if (parser.isSet(noGlobalShortcutsOption)) {
        flags |= KWin::WaylandServer::InitalizationFlag::NoGlobalShortcuts;
    }
    if (!server->init(parser.value(waylandSocketOption).toUtf8(), flags)) {
        std::cerr << "FATAL ERROR: could not create Wayland server" << std::endl;
        return 1;
    }

    a.initPlatform(*pluginIt);
    if (!a.platform()) {
        std::cerr << "FATAL ERROR: could not instantiate a backend" << std::endl;
        return 1;
    }
    if (!deviceIdentifier.isEmpty()) {
        a.platform()->setDeviceIdentifier(deviceIdentifier);
    }
    if (initialWindowSize.isValid()) {
        a.platform()->setInitialWindowSize(initialWindowSize);
    }
    a.platform()->setInitialOutputScale(outputScale);
    a.platform()->setInitialOutputCount(outputCount);

    QObject::connect(&a, &KWin::Application::workspaceCreated, server, &KWin::WaylandServer::initWorkspace);
    environment.insert(QStringLiteral("WAYLAND_DISPLAY"), server->display()->socketName());
    a.setProcessStartupEnvironment(environment);
    a.setStartXwayland(parser.isSet(xwaylandOption));
    a.setWithoutScreen(parser.isSet(withoutscreenOption));
    a.setDisableMultiScreens(parser.isSet(disableMultiScreens));
    a.setApplicationsToStart(parser.positionalArguments());
    a.setInputMethodServerToStart(parser.value(inputMethodOption));
    a.start();

    return a.exec();
}
