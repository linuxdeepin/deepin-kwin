/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "integration.h"
#include "backingstore.h"
#include "eglplatformcontext.h"
#include "logging.h"
#include "offscreensurface.h"
#include "screen.h"
#include "window.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "main.h"
#include "workspace.h"

#include <QCoreApplication>
#include <QTimer>
#include <QtConcurrentRun>

#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtEventDispatcherSupport/private/qunixeventdispatcher_qpa_p.h>
#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtThemeSupport/private/qgenericunixthemes_p.h>
#else
#include <QtGui/private/qgenericunixeventdispatcher_p.h>
#include <QtGui/private/qgenericunixfontdatabase_p.h>
#include <QtGui/private/qgenericunixthemes_p.h>
#include <QtGui/private/qunixeventdispatcher_qpa_p.h>
#endif

namespace KWin
{

namespace QPA
{
class DDEWaylandTheme: public QGenericUnixTheme {
public:
    QVariant themeHint(ThemeHint hint) const {
        if (hint == QPlatformTheme::SystemIconThemeName) {
            return "bloom";
        }

        return QGenericUnixTheme::themeHint(hint);
    }
};

Integration::Integration()
    : QObject()
    , QPlatformIntegration()
    , m_fontDb(new QGenericUnixFontDatabase())
    , m_nativeInterface(new QPlatformNativeInterface())
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
    , m_services(new QGenericUnixServices())
#else
    , m_services(new QDesktopUnixServices())
#endif
{
}

Integration::~Integration()
{
    for (QPlatformScreen *platformScreen : std::as_const(m_screens)) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
        QWindowSystemInterface::handleScreenRemoved(platformScreen);
#else
        destroyScreen(platformScreen);
#endif
    }
    if (m_dummyScreen) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
        QWindowSystemInterface::handleScreenRemoved(m_dummyScreen);
#else
        destroyScreen(m_dummyScreen);
#endif
    }
}

QHash<Output *, Screen *> Integration::screens() const
{
    return m_screens;
}

bool Integration::hasCapability(Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps:
        return true;
    case OpenGL:
        return true;
    case ThreadedOpenGL:
        return false;
    case BufferQueueingOpenGL:
        return false;
    case MultipleWindows:
    case NonFullScreenWindows:
        return true;
    case RasterGLSurface:
        return false;
    default:
        return QPlatformIntegration::hasCapability(cap);
    }
}

void Integration::initialize()
{
    // This method is called from QGuiApplication's constructor, before kwinApp is built
    QTimer::singleShot(0, this, [this] {
        // The QPA is initialized before the workspace is created.
        if (workspace()) {
            handleWorkspaceCreated();
        } else {
            connect(kwinApp(), &Application::workspaceCreated, this, &Integration::handleWorkspaceCreated);
        }
    });

    QPlatformIntegration::initialize();

    m_dummyScreen = new PlaceholderScreen();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
    QWindowSystemInterface::handleScreenAdded(m_dummyScreen);
#else
    screenAdded(m_dummyScreen);
#endif
}

QAbstractEventDispatcher *Integration::createEventDispatcher() const
{
    return new QUnixEventDispatcherQPA;
}

QPlatformBackingStore *Integration::createPlatformBackingStore(QWindow *window) const
{
    return new BackingStore(window);
}

QPlatformWindow *Integration::createPlatformWindow(QWindow *window) const
{
    return new Window(window);
}

QPlatformOffscreenSurface *Integration::createPlatformOffscreenSurface(QOffscreenSurface *surface) const
{
    return new OffscreenSurface(surface);
}

QPlatformFontDatabase *Integration::fontDatabase() const
{
    return m_fontDb.get();
}

QPlatformTheme *Integration::createPlatformTheme(const QString &name) const
{
    return new DDEWaylandTheme;
}

QStringList Integration::themeNames() const
{
    if (qEnvironmentVariableIsSet("KDE_FULL_SESSION")) {
        return QStringList({QStringLiteral("kde")});
    }
    return QStringList({QLatin1String(QGenericUnixTheme::name)});
}

QPlatformOpenGLContext *Integration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    if (kwinApp()->outputBackend()->sceneEglGlobalShareContext() == EGL_NO_CONTEXT) {
        qCWarning(KWIN_QPA) << "Attempting to create a QOpenGLContext before the scene is initialized";
        return nullptr;
    }
    const EGLDisplay eglDisplay = kwinApp()->outputBackend()->sceneEglDisplay();
    if (eglDisplay != EGL_NO_DISPLAY) {
        EGLPlatformContext *platformContext = new EGLPlatformContext(context, eglDisplay);
        return platformContext;
    }
    return nullptr;
}

void Integration::handleWorkspaceCreated()
{
    connect(workspace(), &Workspace::outputAdded,
            this, &Integration::handleOutputEnabled);
    connect(workspace(), &Workspace::outputRemoved,
            this, &Integration::handleOutputDisabled);

    const QList<Output *> outputs = workspace()->outputs();
    for (Output *output : outputs) {
        handleOutputEnabled(output);
    }
}

void Integration::handleOutputEnabled(Output *output)
{
    Screen *platformScreen = new Screen(output, this);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
        QWindowSystemInterface::handleScreenAdded(platformScreen);
#else
        screenAdded(platformScreen);
#endif
    m_screens.insert(output, platformScreen);

    if (m_dummyScreen) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
        QWindowSystemInterface::handleScreenAdded(m_dummyScreen);
#else
        screenAdded(m_dummyScreen);
#endif
        m_dummyScreen = nullptr;
    }
}

void Integration::handleOutputDisabled(Output *output)
{
    Screen *platformScreen = m_screens.take(output);
    if (!platformScreen) {
        qCWarning(KWIN_QPA) << "Unknown output" << output;
        return;
    }

    if (m_screens.isEmpty()) {
        m_dummyScreen = new PlaceholderScreen();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
        QWindowSystemInterface::handleScreenAdded(m_dummyScreen);
#else
        screenAdded(m_dummyScreen);
#endif
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 13, 0))
        QWindowSystemInterface::handleScreenRemoved(platformScreen);
#else
        destroyScreen(platformScreen);
#endif
}

QPlatformNativeInterface *Integration::nativeInterface() const
{
    return m_nativeInterface.get();
}

QPlatformServices *Integration::services() const
{
    return m_services.get();
}

}
}
