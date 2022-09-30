// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_QPA_INTEGRATION_H
#define KWIN_QPA_INTEGRATION_H

#include <epoxy/egl.h>
#include "fixqopengl.h"

#include <fixx11h.h>
#include <qpa/qplatformintegration.h>
#include <QObject>

namespace KWayland
{
namespace Client
{
class Registry;
class Compositor;
class Shell;
}
}

namespace KWin
{
namespace QPA
{

class Screen;

class Integration : public QObject, public QPlatformIntegration
{
    Q_OBJECT
public:
    explicit Integration();
    virtual ~Integration();

    bool hasCapability(Capability cap) const override;
    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QAbstractEventDispatcher *createEventDispatcher() const override;
    QPlatformFontDatabase *fontDatabase() const override;
    QStringList themeNames() const override;
    QPlatformTheme *createPlatformTheme(const QString &name) const override;
    QPlatformNativeInterface *nativeInterface() const override;
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override;

    void initialize() override;
    QPlatformInputContext *inputContext() const override;

    KWayland::Client::Compositor *compositor() const;
    EGLDisplay eglDisplay() const;

    QVector<Screen*> getScreens() const;

private:
    void initScreens();
    void initEgl();
    KWayland::Client::Shell *shell() const;

    QPlatformFontDatabase *m_fontDb;
    QPlatformNativeInterface *m_nativeInterface;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
    Screen *m_dummyScreen = nullptr;
    QScopedPointer<QPlatformInputContext> m_inputContext;
    QVector<Screen*> m_screens;
};

}
}

#endif
