#include "windowradiusmanager.h"
#include "workspace.h"
#include "unmanaged.h"
#include "configreader.h"
#include <QScreen>

#define DBUS_APPEARANCE_SERVICE  "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ      "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF     "com.deepin.daemon.Appearance"

namespace KWin
{
WindowRadiusManager::WindowRadiusManager(/* args */)
{
    m_configReader = new ConfigReader(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ,
                                      DBUS_APPEARANCE_INTF, "WindowRadius");
    connect(m_configReader, &ConfigReader::sigRadiusChanged, this, &WindowRadiusManager::onRadiusChange);
    m_scale = qMax(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);
}

WindowRadiusManager::~WindowRadiusManager()
{
    if (m_configReader) {
        delete m_configReader;
        m_configReader = nullptr;
    }
}

void WindowRadiusManager::onWindowAdded(Window *window)
{
    window->updateWindowRadius(m_scale);
    connect(window, static_cast<void (Window::*)(Window *, bool, bool)>(&Window::clientMaximizedStateChanged), this, &WindowRadiusManager::onWindowMaxiChanged);
}

void WindowRadiusManager::onRadiusChange(QVariant property)
{
    float r = property.toFloat();
    if (m_radius != r)
        m_radius = r;
    Q_EMIT sigRadiusChanged(r, m_scale);
}

void WindowRadiusManager::onWindowMaxiChanged(Window *window, bool h, bool v)
{
    window->setMaximized(h && v);
    window->updateWindowRadius(m_scale);
}

float WindowRadiusManager::getOsRadius()
{
    if (m_radius <= 0.0)
        m_radius = m_configReader->getProperty().isValid() ? m_configReader->getProperty().toFloat() : 0.0;
    return m_radius <= 0.0 ? 0.0 : m_radius;
}

}