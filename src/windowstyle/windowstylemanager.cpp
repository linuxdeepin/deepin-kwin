/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "windowstylemanager.h"
#include "workspace.h"
#include "unmanaged.h"
#include "configreader.h"
#include "composite.h"
#include <QScreen>
#include <QGSettings/qgsettings.h>

#define DBUS_APPEARANCE_SERVICE  "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ      "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF     "com.deepin.daemon.Appearance"

Q_GLOBAL_STATIC_WITH_ARGS(QGSettings, _gsettings_deepin_xsetting, ("com.deepin.xsettings"))
#define GsettingsDtkRadius     "dtk-window-radius"

namespace KWin
{
WindowStyleManager::WindowStyleManager()
{
    m_configReader = new ConfigReader(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ,
                                      DBUS_APPEARANCE_INTF, "WindowRadius");
    connect(m_configReader, &ConfigReader::sigRadiusChanged, this, &WindowStyleManager::onRadiusChange);
    connect(Compositor::self(), &Compositor::compositingToggled, this, &WindowStyleManager::onCompositingChanged);
    m_scale = qMax(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);
}

WindowStyleManager::~WindowStyleManager()
{
    if (m_configReader) {
        delete m_configReader;
        m_configReader = nullptr;
    }
}

void WindowStyleManager::onWindowAdded(Window *window)
{
    window->createWinStyle();
    window->updateWindowRadius();
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
        w->updateWindowShadow();
    }
}

void WindowStyleManager::onCompositingChanged(bool acitve)
{
    if (acitve) {
        QList<Window*> windows = workspace()->allClientList();
        for (Window *w : windows) {
            w->updateWindowRadius();
        }
    }
}

float WindowStyleManager::getOsRadius()
{
    if (m_osRadius <= 0.0) {
        if (m_configReader->getProperty().isValid()) {
            m_osRadius = m_configReader->getProperty().toFloat();
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

}