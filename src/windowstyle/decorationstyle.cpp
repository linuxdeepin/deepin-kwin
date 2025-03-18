/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "decorationstyle.h"
#include "main.h"
#include "utils/xcbutils.h"
#include "window.h"
#include "workspace.h"
#include "windowstylemanager.h"
#include "atoms.h"

namespace KWin
{
DecorationStyle::DecorationStyle(Window *window)
{
    static QFunctionPointer build_function = qApp->platformFunction("_d_buildNativeSettings");
    if (build_function) {
        reinterpret_cast<bool(*)(QObject*, quint32)>(build_function)(this, window->window());
    } else {
        qCWarning(KWIN_CORE) << "Load DPlatformIntegration::buildNativeSettings failed!";
    }
}

QPointF DecorationStyle::variant2Point(QVariant value, QPointF defaultV)
{
    QStringList l = value.toString().split(",");

    if (l.count() < 2) {
        return defaultV;
    }

    QPoint p;
    p.setX(l.first().toDouble());
    p.setY(l.at(1).toDouble());

    return p;
}

QMarginsF DecorationStyle::variant2Margins(QVariant value, QMarginsF defaultV)
{
    if (!value.isValid()) {
        return defaultV;
    }

    QStringList l = value.toStringList();

    if (l.isEmpty()) {
        l = value.toString().split(",");
    }

    if (l.count() < 4) {
        return defaultV;
    }

    return QMarginsF(l.at(0).toDouble(), l.at(1).toDouble(),
                     l.at(2).toDouble(), l.at(3).toDouble());
}

void DecorationStyle::setValidProperties(qint64 validProperties)
{
    if (m_validProperties == validProperties)
        return;

    PropertyFlags p = PropertyFlag(validProperties);

    m_validProperties = p;

    Q_EMIT validPropertiesChanged(m_validProperties);
}

/******************************************************/

X11DecorationStyle::X11DecorationStyle(Window *window)
    : DecorationStyle(window)
    , m_window(window)
    , m_isX11Only(kwinApp()->operationMode() == Application::OperationModeX11)
{
}

X11DecorationStyle::~X11DecorationStyle()
{
}

X11DecorationStyle::PropertyFlags X11DecorationStyle::validProperties()
{
    return m_validProperties;
}

bool X11DecorationStyle::propertyIsValid(PropertyFlag p)
{
    return m_validProperties.testFlag(p);
}

QString X11DecorationStyle::theme()
{
    return m_isX11Only ? property("theme").toString() : m_theme;
}

QPointF X11DecorationStyle::windowRadius()
{
    return m_isX11Only ? variant2Point(property("windowRadius")) : m_windowRadius;
}

void X11DecorationStyle::setWindowRadius(const QPointF value)
{
    if (m_isX11Only) {
        QString point = QString::number(value.x()) + "," + QString::number(value.y());
        setProperty("windowRadius", point);
    } else {
        m_windowRadius = value;
    }
}

qreal X11DecorationStyle::borderWidth()
{
    return m_isX11Only ? property("borderWidth").toDouble() : m_borderWidth;
}

void X11DecorationStyle::setBorderWidth(qreal width)
{
    if (m_isX11Only)
        setProperty("borderWidth", width);
    else
        m_borderWidth = width;
}

QColor X11DecorationStyle::borderColor()
{
    return m_isX11Only ? qvariant_cast<QColor>(property("borderColor")) : m_borderColor;
}

void X11DecorationStyle::setBorderColor(QColor color)
{
    if (m_isX11Only)
        setProperty("borderColor", QVariant::fromValue(color));
    else
        m_borderColor = color;
}

qreal X11DecorationStyle::shadowRadius()
{
    return m_isX11Only ? property("shadowRadius").toDouble() : m_shadowRadius;
}

QPointF X11DecorationStyle::shadowOffset()
{
    return m_isX11Only ? variant2Point(property("shadowOffset")) : m_shadowOffset;
}

QColor X11DecorationStyle::shadowColor()
{
    return m_isX11Only ? qvariant_cast<QColor>(property("shadowColor")) : m_shadowColor;
}

void X11DecorationStyle::setShadowColor(QColor color)
{
    if (m_isX11Only)
        setProperty("shadowColor", QVariant::fromValue(color));
    else
        m_shadowColor = color;
}

QMarginsF X11DecorationStyle::mouseInputAreaMargins()
{
    return m_isX11Only ? variant2Margins(property("mouseInputAreaMargins")) : m_mouseInputAreaMargins;
}

qreal X11DecorationStyle::windowPixelRatio()
{
    qreal ratio = m_isX11Only ? property("windowPixelRatio").toDouble() : m_windowPixelRatio;
    return m_validProperties.testFlag(WindowPixelRatioProperty) ? ratio : 1.0;
}

X11DecorationStyle::effectScenes X11DecorationStyle::windowEffect()
{
    qreal effect = m_isX11Only ? property("windowEffect").toDouble() : m_windowEffect;
    return effectScene(effect);
}

qreal X11DecorationStyle::windowStartUpEffect()
{
    return m_isX11Only ? property("windowStartUpEffect").toDouble() : m_windowStartUpEffect;
}

X11DecorationStyle::effectScenes X11DecorationStyle::getWindowEffectScene()
{
    effectScenes validSce;
    if (m_validProperties.testFlag(WindowEffectProperty)) {
        validSce = windowEffect();
    } else {
        Xcb::Property property(false, m_window->window(), atoms->deepin_net_effect, XCB_ATOM_CARDINAL, 0, 256);
        const QByteArray data = property.toByteArray(32, XCB_ATOM_CARDINAL);
        if (!data.isEmpty()) {
            validSce = effectScene(*(reinterpret_cast<const quint32 *>(data.constData())));
        }
    }
    return validSce;
}

void X11DecorationStyle::parseWinCustomRadius()
{
    Xcb::StringProperty property(m_window->window(), atoms->deepin_net_radius);
    const QByteArray data = property.toByteArray();
    if (!data.isEmpty()) {
        setValidProperties(validProperties() | DecorationStyle::WindowRadiusProperty);
        setWindowRadius(variant2Point(data.constData()));
    }
}

void X11DecorationStyle::parseWinCustomShadow()
{
    Xcb::StringProperty property(m_window->window(), atoms->deepin_net_shadow);
    const QByteArray data = property.toByteArray();
    if (!data.isEmpty()) {
        setValidProperties(validProperties() | DecorationStyle::ShadowColorProperty);
        setShadowColor(data.constData());
    }
}

void X11DecorationStyle::parseWinStartUpEffect()
{
    if (m_validProperties.testFlag(WindowStartUpEffectProperty)) {
        m_window->setStartUpEffectType(windowStartUpEffect());
    } else {
        Xcb::Property property(false, m_window->window(), atoms->deepin_net_startup, XCB_ATOM_CARDINAL, 0, 256);
        const QByteArray data = property.toByteArray(32, XCB_ATOM_CARDINAL);
        if (!data.isEmpty()) {
            m_window->setStartUpEffectType(*(reinterpret_cast<const quint32 *>(data.constData())));
        }
    }
}

/***********************************************************/

WaylandDecorationStyle::WaylandDecorationStyle(Window *window)
    : DecorationStyle(window)
    , m_window(window)
{
    connect(window, &Window::waylandWindowRadiusChanged, this, &WaylandDecorationStyle::onUpdateWindowRadiusByWayland);
    connect(window, &Window::waylandShadowColorChanged, this, &WaylandDecorationStyle::onUpdateShadowColorByWayland);
    connect(window, &Window::waylandBorderWidthChanged, this, &WaylandDecorationStyle::onUpdateBorderWidthByWayland);
    connect(window, &Window::waylandBorderColorChanged, this, &WaylandDecorationStyle::onUpdateBorderColorByWayland);
}

WaylandDecorationStyle::PropertyFlags WaylandDecorationStyle::validProperties()
{
    return m_validProperties;
}

bool WaylandDecorationStyle::propertyIsValid(PropertyFlag p)
{
    return m_validProperties.testFlag(p);
}

QPointF WaylandDecorationStyle::windowRadius()
{
    return m_radius;
}

void WaylandDecorationStyle::setWindowRadius(const QPointF value)
{
    m_radius = value;
}

qreal WaylandDecorationStyle::borderWidth()
{
    return m_borderWidth;
}

void WaylandDecorationStyle::setBorderWidth(qreal width)
{
    m_borderWidth = width;
}

QColor WaylandDecorationStyle::borderColor()
{
    return m_borderColor;
}

void WaylandDecorationStyle::setBorderColor(QColor color)
{
    m_borderColor = color;
}


QColor WaylandDecorationStyle::shadowColor()
{
    return m_shadowColor;
}

void WaylandDecorationStyle::setShadowColor(QColor color)
{
    m_shadowColor = color;
}

void WaylandDecorationStyle::onUpdateWindowRadiusByWayland(QPointF radius)
{
    setValidProperties(validProperties() | DecorationStyle::WindowRadiusProperty);
    m_radius = radius * Workspace::self()->getWindowStyleMgr()->getOsScale();
    Q_EMIT windowRadiusChanged();
}

void WaylandDecorationStyle::onUpdateShadowColorByWayland(QString color)
{
    setValidProperties(validProperties() | DecorationStyle::ShadowColorProperty);
    setShadowColor(QColor(color));
    Q_EMIT shadowColorChanged();
}

void WaylandDecorationStyle::onUpdateBorderWidthByWayland(qint32 width)
{
    setValidProperties(validProperties() | DecorationStyle::BorderWidthProperty);
    setBorderWidth(width);
    Q_EMIT borderWidthChanged();
}

void WaylandDecorationStyle::onUpdateBorderColorByWayland(QString color)
{
    setValidProperties(validProperties() | DecorationStyle::BorderColorProperty);
    setBorderColor(QColor(color));
    Q_EMIT borderColorChanged();
}

}
