#ifndef WINDOWRADIUS_H
#define WINDOWRADIUS_H

#include <QObject>
#include <QPointF>

namespace KWin
{
class Window;

class WindowRadius : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 validProperties READ validProperties WRITE setValidProperties NOTIFY validPropertiesChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QPointF windowRadius READ windowRadius WRITE setWindowRadius NOTIFY windowRadiusChanged)

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

    explicit WindowRadius(Window *window);
    ~WindowRadius();

    bool updateWindowRadius();

    void setWindowRadius(QPointF);
    QPointF windowRadius();
    QString theme() const;

    PropertyFlags validProperties() const;
    bool propertyIsValid(PropertyFlag p) const;

Q_SIGNALS:
    void windowRadiusChanged();
    void validPropertiesChanged(qint64 validProperties);
    void themeChanged();

public Q_SLOTS:
    void onUpdateWindowRadiusChanged();
    void onUpdateWindowRadiusByWayland(QPointF);
    void onCompoistorChanged(bool);

    void setValidProperties(qint64 validProperties);

public:
    Window *m_window;
    quint32 m_atom_deepin_scissor_window;
    QPointF m_radius = QPointF(-1, 0);
    bool    m_isMaximized = false;
    float   m_scale = 1.0;
    PropertyFlags m_validProperties;
};
}

#endif
