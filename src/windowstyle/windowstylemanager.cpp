/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "windowstylemanager.h"
#include "composite.h"
#include "configreader.h"
#include "decorationstyle.h"
#include "effects.h"
#include "unmanaged.h"
#include "workspace.h"
#include <QGSettings>
#include <QScreen>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <private/qtx11extras_p.h>
#endif
#include <QDebug>

#ifdef BUILD_ON_V25
#define DBUS_APPEARANCE_SERVICE "org.deepin.dde.Appearance1"
#define DBUS_APPEARANCE_OBJ "/org/deepin/dde/Appearance1"
#define DBUS_APPEARANCE_INTF "org.deepin.dde.Appearance1"
#else
#define DBUS_APPEARANCE_SERVICE "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF "com.deepin.daemon.Appearance"
#endif

Q_GLOBAL_STATIC_WITH_ARGS(QGSettings, _gsettings_deepin_xsetting, ("com.deepin.xsettings"))
#define GsettingsDtkRadius     "dtk-window-radius"

namespace KWin
{
WindowStyleManager::WindowStyleManager()
{
    m_radiusConfig = std::make_unique<ConfigReader>(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ, DBUS_APPEARANCE_INTF, "WindowRadius");
    m_themeConfig = std::make_unique<ConfigReader>(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ, DBUS_APPEARANCE_INTF, "GtkTheme");
    connect(m_radiusConfig.get(), &ConfigReader::sigPropertyChanged, this, &WindowStyleManager::onRadiusChange);
    connect(m_themeConfig.get(), &ConfigReader::sigPropertyChanged, this, &WindowStyleManager::onThemeChange);
    connect(Compositor::self(), &Compositor::compositingToggled, this, &WindowStyleManager::onCompositingChanged);
    auto screen = QGuiApplication::primaryScreen();
    m_scale = qMax(1.0, screen->logicalDotsPerInch() / 96.0);
    // NOTE: xsettings daemon not setup in time, may not get correct `Xft.dpi`, add listener to it
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, [this]() {
        m_scale = qMax(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);
        Q_EMIT sigScaleChanged();
    });
}

WindowStyleManager::~WindowStyleManager()
{
}

void WindowStyleManager::onWindowAdded(Window *window)
{
    window->createWinStyle();
    parseWinCustomEffect(window);
    handleSpecialWindowStyle(window);
    window->updateWindowRadius();
    window->updateWinProperty();
    connect(window, static_cast<void (Window::*)(Window *, bool, bool)>(&Window::clientMaximizedStateChanged), this, &WindowStyleManager::onWindowMaxiChanged);
    connect(window, &Window::activeChanged, this, &WindowStyleManager::onWindowActiveChanged);
    connect(window, &Window::geometryShapeChanged, this, &WindowStyleManager::onGeometryShapeChanged);
    connect(window, &Window::hasAlphaChanged, this, &WindowStyleManager::onWindowActiveChanged);
}

void WindowStyleManager::onRadiusChange(QVariant property)
{
    float r = property.toFloat();
    if (m_osRadius != r)
        m_osRadius = r;
    Q_EMIT sigRadiusChanged(r);
    Q_EMIT workspace()->osRadiusChanged();
}

void WindowStyleManager::onThemeChange(QVariant property)
{
    QString str = property.toString();
    bool isDark = (str == "deepin-dark") ? true : false;
    workspace()->setDarkTheme(isDark);
    Q_EMIT sigThemeChanged(isDark);
    Q_EMIT workspace()->osThemeChanged();
}

void WindowStyleManager::onWindowMaxiChanged(Window *window, bool h, bool v)
{
    window->updateWindowRadius();
}

void WindowStyleManager::onWindowActiveChanged()
{
    Window *window = qobject_cast<Window *>(QObject::sender());
    window->updateWindowShadow();
}

void WindowStyleManager::onGeometryShapeChanged(Window *w, QRectF rectF)
{
    if (w->cacheShapeGeometry().size() != rectF.size()) {
        w->setCacheShapeGeometry(rectF);
        w->updateWindowRadius();
    }
}

void WindowStyleManager::onCompositingChanged(bool acitve)
{
    if (acitve) {
        QList<Window*> windows = workspace()->allClientList();
        for (Window *w : windows) {
            w->updateWindowRadius(true);
        }
        QTimer::singleShot(50, [&] {
            if (Compositor::self() && Compositor::self()->scene()) {
                Compositor::self()->scene()->addRepaintFull();
            }
        });
    }
}

void WindowStyleManager::onWaylandWindowCustomEffect(uint32_t type)
{
    Window *window = qobject_cast<Window *>(QObject::sender());
    window->windowStyleObj()->setWindowEffectScene(type);
    parseWinCustomEffect(window);
}

void WindowStyleManager::onWaylandWindowStartUpEffect(uint32_t type)
{
    Window *window = qobject_cast<Window *>(QObject::sender());
    window->setStartUpEffectType(type);
}

float WindowStyleManager::getOsRadius()
{
    if (m_osRadius <= 0.0) {
        if (m_radiusConfig.get()->getProperty().isValid()) {
            m_osRadius = m_radiusConfig.get()->getProperty().toFloat();
        } else {
            m_osRadius = _gsettings_deepin_xsetting->get(GsettingsDtkRadius).toInt();
        }
    }

    return m_osRadius <= 0.0 ? 0.0 : m_osRadius;
}

float WindowStyleManager::getOsScale()
{
    return m_scale;
}

void WindowStyleManager::handleSpecialWindowStyle(Window *window)
{
    if (window->isSwitcherWin()) {
        window->windowStyleObj()->setValidProperties(DecorationStyle::WindowRadiusProperty | DecorationStyle::BorderWidthProperty);
        QPointF r(getOsRadius() * m_scale, getOsRadius() * m_scale);
        window->windowStyleObj()->setWindowRadius(r);
        window->windowStyleObj()->setBorderWidth(0);
    }
    if (window->isWindowMenu()) {
        window->windowStyleObj()->setValidProperties(DecorationStyle::WindowRadiusProperty);
        QPointF r(getOsRadius() * m_scale, getOsRadius() * m_scale);
        window->windowStyleObj()->setWindowRadius(r);
    }
}

void WindowStyleManager::parseWinCustomEffect(Window *window)
{
    DecorationStyle::effectScenes validSce = window->windowStyleObj()->getWindowEffectScene();

    if (validSce.testFlag(DecorationStyle::effectNoRadius)) {
        window->windowStyleObj()->cancelRadiusByUser(true);
    } else {
        window->windowStyleObj()->parseWinCustomRadius();
    }

    if (validSce.testFlag(DecorationStyle::effectNoShadow)) {
        window->windowStyleObj()->cancelShadowByUser(true);
    } else {
        window->windowStyleObj()->parseWinCustomShadow();
    }

    if (validSce.testFlag(DecorationStyle::effectNoBorder)) {
        window->windowStyleObj()->setValidProperties(window->windowStyleObj()->validProperties() | DecorationStyle::BorderWidthProperty);
        window->windowStyleObj()->setBorderWidth(0);
    }

    if (validSce.testFlag(DecorationStyle::effectNoStart)) {
        window->setStartUpEffectType(effectType::effectNone);
    } else {
        window->windowStyleObj()->parseWinStartUpEffect();
    }

    // if (validSce.testFlag(effectScene::effectNoClose))

    // if (validSce.testFlag(effectScene::effectNoMax))

    // if (validSce.testFlag(effectScene::effectNoMin))
}

}
