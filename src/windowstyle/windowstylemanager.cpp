#include "windowstylemanager.h"
#include "workspace.h"
#include "unmanaged.h"
#include "configreader.h"
#include "composite.h"
#include <QScreen>

#define DBUS_APPEARANCE_SERVICE  "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ      "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF     "com.deepin.daemon.Appearance"

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
    if (m_osRadius <= 0.0)
        m_osRadius = m_configReader->getProperty().isValid() ? m_configReader->getProperty().toFloat() : 0.0;
    return m_osRadius <= 0.0 ? 0.0 : m_osRadius;
}

float WindowStyleManager::getOsScale()
{
    return m_scale;
}

}