/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "decorationstyle.h"
#include "window.h"
#include "workspace.h"
#include "windowstylemanager.h"

namespace KWin
{
DecorationStyle::DecorationStyle(Window *window)
{
    static QFunctionPointer build_function = qApp->platformFunction("_d_buildNativeSettings");
    if (build_function) {
        reinterpret_cast<bool(*)(QObject*, quint32)>(build_function)(this, window->window());
    }
}

QPointF DecorationStyle::variant2Point(QVariant value, QPointF defaultV)
{
    QStringList l = value.toString().split(",");

    if (l.count() < 2) {
        return defaultV;
    }

    QPointF p;
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
    return property("theme").toString();
}

QPointF X11DecorationStyle::windowRadius()
{
    return variant2Point(property("windowRadius"));
}

void X11DecorationStyle::setWindowRadius(const QPointF value)
{
    QString point = QString::number(value.x()) + "," + QString::number(value.y());
    setProperty("windowRadius", point);
}

qreal X11DecorationStyle::borderWidth()
{
    return property("borderWidth").toDouble();
}

void X11DecorationStyle::setBorderWidth(int width)
{
    setProperty("borderWidth", width);
}

QColor X11DecorationStyle::borderColor()
{
    return qvariant_cast<QColor>(property("borderColor"));
}

qreal X11DecorationStyle::shadowRadius()
{
    return property("shadowRadius").toDouble();
}

QPointF X11DecorationStyle::shadowOffset()
{
    return variant2Point(property("shadowOffset"));
}

QColor X11DecorationStyle::shadowColor()
{
    return qvariant_cast<QColor>(property("shadowColor"));
}

QMarginsF X11DecorationStyle::mouseInputAreaMargins()
{
    return variant2Margins(property("mouseInputAreaMargins"));
}

qreal X11DecorationStyle::windowPixelRatio()
{
    return m_validProperties.testFlag(WindowPixelRatioProperty) ? property("windowPixelRatio").toDouble() : 1.0;
}

/***********************************************************/

WaylandDecorationStyle::WaylandDecorationStyle(Window *window)
    : DecorationStyle(window)
    , m_window(window)
{
    connect(window, &Window::waylandWindowRadiusChanged, this, &WaylandDecorationStyle::onUpdateWindowRadiusByWayland);
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
    return m_border;
}

void WaylandDecorationStyle::setBorderWidth(int width)
{
    m_border = width;
}

QColor WaylandDecorationStyle::borderColor()
{
    return QColor();
}

QColor WaylandDecorationStyle::shadowColor()
{
    return QColor();
}

void WaylandDecorationStyle::onUpdateWindowRadiusByWayland(QPointF radius)
{
    setValidProperties(DecorationStyle::WindowRadiusProperty);
    m_radius = radius * Workspace::self()->getWindowStyleMgr()->getOsScale();
    Q_EMIT windowRadiusChanged();
}

}
