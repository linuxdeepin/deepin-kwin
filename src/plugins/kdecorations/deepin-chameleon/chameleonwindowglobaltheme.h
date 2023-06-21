#ifndef CHAMELEONWINDOWGLOBALTHEME_H
#define CHAMELEONWINDOWGLOBALTHEME_H

#include <qobjectdefs.h>
#include <qvariant.h>
#include <QVariant>
#include <QObject>

class ChameleonWindowGlobalTheme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 validProperties READ validProperties WRITE setValidProperties NOTIFY validPropertiesChanged)

public:
    enum PropertyFlag {
        ThemeProperty = 0x02,
        WindowRadiusProperty = 0x04,
        BorderWidthProperty = 0x08,
        BorderColorProperty = 0x10,
        ShadowRadiusProperty = 0x20,
        ShadowOffsetProperty = 0x40,
        ShadowColorProperty = 0x80,
        MouseInputAreaMargins = 0x100,
        WindowPixelRatioProperty = 0x200
    };
    Q_DECLARE_FLAGS(PropertyFlags, PropertyFlag)
    Q_FLAG(PropertyFlags)

    static ChameleonWindowGlobalTheme* Instance();

    PropertyFlags validProperties() const;
    bool propertyIsValid(PropertyFlag p) const;

    QVariant theme() const;
    QVariant radius() const;

Q_SIGNALS:
    void validPropertiesChanged(qint64 validProperties);

public Q_SLOTS:
    void setValidProperties(qint64 validProperties);

private:
    ChameleonWindowGlobalTheme();
    ~ChameleonWindowGlobalTheme();

    PropertyFlags m_validProperties;
};

#endif /* CHAMELEONWINDOWGLOBALTHEME_H */
