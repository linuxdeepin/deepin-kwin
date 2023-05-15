#include "chameleonwindowglobaltheme.h"

#include <qobjectdefs.h>
#include <qvariant.h>

#include <QPointF>
#include <QMetaProperty>
#include <QDynamicPropertyChangeEvent>

#include <unordered_map>

#include "kwinutils.h"

#include <QDebug>

ChameleonWindowGlobalTheme::ChameleonWindowGlobalTheme()
    : QObject()
{
}

ChameleonWindowGlobalTheme::PropertyFlags ChameleonWindowGlobalTheme::validProperties() const
{
    return m_validProperties;
}

bool ChameleonWindowGlobalTheme::propertyIsValid(ChameleonWindowGlobalTheme::PropertyFlag p) const
{
    return m_validProperties.testFlag(p);
}

void ChameleonWindowGlobalTheme::setValidProperties(qint64 validProperties)
{
    if (m_validProperties == validProperties)
        return;

    PropertyFlags p = PropertyFlag(validProperties);

    m_validProperties = p;
    Q_EMIT validPropertiesChanged(m_validProperties);
}

ChameleonWindowGlobalTheme* ChameleonWindowGlobalTheme::Instance()
{
    static ChameleonWindowGlobalTheme* global = nullptr;

    if (!global) {
        ChameleonWindowGlobalTheme* theme = new ChameleonWindowGlobalTheme;
        // NOTE: bind to root window.
        bool ok = KWinUtils::instance()->buildNativeSettings(theme, 0);

        if (ok) {
            global = theme;
        }
    }

    return global;
}

ChameleonWindowGlobalTheme::~ChameleonWindowGlobalTheme() = default;

QVariant ChameleonWindowGlobalTheme::theme() const {
    const QVariant& themeVariant{property("Net/ThemeName")};
    if (themeVariant.isValid()) {
        auto tmp = themeVariant.toString().split("-");
        if (tmp.size() > 1) {
            return QString("%1/%2").arg(tmp[1].contains("dark") ? "dark" : "light").arg(tmp[0]);
        }
    }

    return {};
}

QVariant ChameleonWindowGlobalTheme::radius() const {
    const QVariant& radiusVariant = property("DTK/WindowRadius");
    if (radiusVariant.isValid()) {
        const qreal radius = radiusVariant.toDouble();
        return QPointF{radius, radius};
    }

    return {};
}