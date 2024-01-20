#include "windowradius.h"
#include "window.h"
#include "effects.h"
#include "workspace.h"
#include "windowradiusmanager.h"
#include "composite.h"
#include <xcb/xcb.h>
#include <QWindow>
#include <QX11Info>

#define _DEEPIN_SCISSOR_WINDOW "_DEEPIN_SCISSOR_WINDOW"

Q_DECLARE_METATYPE(QPainterPath)

namespace KWin
{
static xcb_atom_t internAtom(const char *name, bool only_if_exists)
{
    if (!name || *name == 0)
        return XCB_NONE;

    if (!QX11Info::isPlatformX11())
        return XCB_NONE;

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(QX11Info::connection(), only_if_exists, strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(QX11Info::connection(), cookie, 0);

    if (!reply)
        return XCB_NONE;

    xcb_atom_t atom = reply->atom;
    free(reply);

    return atom;
}

WindowRadius::WindowRadius(Window *window)
    : m_window(window)
{
    m_atom_deepin_scissor_window = internAtom(_DEEPIN_SCISSOR_WINDOW, false);

    static QFunctionPointer build_function = qApp->platformFunction("_d_buildNativeSettings");
    if (build_function) {
        reinterpret_cast<bool(*)(QObject*, quint32)>(build_function)(this, window->window());
    }
    connect(this, &WindowRadius::windowRadiusChanged, this, &WindowRadius::onUpdateWindowRadiusChanged);
    connect(Compositor::self(), &Compositor::compositingToggled, this, &WindowRadius::onCompoistorChanged);
    connect(window, &Window::waylandWindowRadiusChanged, this, &WindowRadius::onUpdateWindowRadiusByWayland);
}

WindowRadius::~WindowRadius()
{
}

bool WindowRadius::updateWindowRadius(float scale)
{
    if (!m_window || !m_window->effectWindow())
        return false;
    m_scale = scale;
    KWin::EffectWindowImpl *effect = m_window->effectWindow();
    if (m_window->isMaximized()) {
        effect->setData(WindowClipPathRole, QVariant());
        effect->setData(WindowRadiusRole, QVariant());
        return true;
    }

    QPainterPath path;
    const QByteArray &clip_data = effect->readProperty(m_atom_deepin_scissor_window, m_atom_deepin_scissor_window, 8);
    if (!clip_data.isEmpty()) {
        QDataStream ds(clip_data);
        ds >> path;
    }
    if (path.isEmpty()) {
        effect->setData(WindowClipPathRole, QVariant());
        QPointF radius = windowRadius();
        effect->setData(WindowRadiusRole, QVariant::fromValue(radius * scale));
    } else {
        effect->setData(WindowRadiusRole, QVariant());
        effect->setData(WindowClipPathRole, QVariant::fromValue(path));
    }

    return true;
}

void WindowRadius::setWindowRadius(QPointF radius)
{
    m_radius = radius;
}

QPointF WindowRadius::windowRadius()
{
    if(!QX11Info::isPlatformX11()) {
        if (m_radius.x() < 0) {
            m_radius.setX(Workspace::self()->getWindowRadiusMgr()->getOsRadius());
            m_radius.setY(Workspace::self()->getWindowRadiusMgr()->getOsRadius());
        }
        return m_radius;
    }
    QStringList l;
    if (m_window->isDock() || propertyIsValid(WindowRadius::WindowRadiusProperty)) {
        QVariant value = property("windowRadius");
        l = value.toString().split(",");
    }
    QPointF radius;
    if (l.count() < 2) {
        radius.setX(Workspace::self()->getWindowRadiusMgr()->getOsRadius());
        radius.setY(Workspace::self()->getWindowRadiusMgr()->getOsRadius());
    } else {
        radius.setX(l.first().toDouble());
        radius.setY(l.at(1).toDouble());
    }
    return radius;
}

void WindowRadius::onUpdateWindowRadiusChanged()
{
    m_window->updateWindowRadius();
}

void WindowRadius::onUpdateWindowRadiusByWayland(QPointF windowRadius)
{
    setProperty("windowRadius",windowRadius);
    m_window->updateWindowRadius();
}

void WindowRadius::onCompoistorChanged(bool status)
{
    if (status)
        m_window->updateWindowRadius();
}

WindowRadius::PropertyFlags WindowRadius::validProperties() const
{
    return m_validProperties;
}

bool WindowRadius::propertyIsValid(PropertyFlag p) const
{
    return m_validProperties.testFlag(p);
}

void WindowRadius::setValidProperties(qint64 validProperties)
{
    if (m_validProperties == validProperties)
        return;

    PropertyFlags p = PropertyFlag(validProperties);

    m_validProperties = p;
    Q_EMIT validPropertiesChanged(m_validProperties);
}

}
