/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef WINDOWSTYLE_H
#define WINDOWSTYLE_H

#include <QObject>
#include <QMarginsF>
#include <QPointF>

namespace KWin
{
class Window;

class DecorationStyle : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 validProperties READ validProperties WRITE setValidProperties NOTIFY validPropertiesChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QPointF windowRadius READ windowRadius WRITE setWindowRadius NOTIFY windowRadiusChanged)
    Q_PROPERTY(qreal borderWidth READ borderWidth NOTIFY borderWidthChanged)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY borderColorChanged)
    Q_PROPERTY(qreal shadowRadius READ shadowRadius NOTIFY shadowRadiusChanged)
    Q_PROPERTY(QPointF shadowOffset READ shadowOffset  NOTIFY shadowOffectChanged)
    Q_PROPERTY(QColor shadowColor READ shadowColor NOTIFY shadowColorChanged)
    Q_PROPERTY(QMarginsF mouseInputAreaMargins READ mouseInputAreaMargins NOTIFY mouseInputAreaMarginsChanged)
    Q_PROPERTY(qreal windowPixelRatio READ windowPixelRatio NOTIFY windowPixelRatioChanged)
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

    explicit DecorationStyle(Window *window);
    ~DecorationStyle(){};
    QPointF variant2Point(QVariant value, QPointF defaultV = QPointF(0.0, 0.0));
    QMarginsF variant2Margins(QVariant value, QMarginsF defaultV = QMarginsF(0, 0, 0, 0));

    virtual PropertyFlags validProperties() = 0;
    virtual bool propertyIsValid(PropertyFlag p) = 0;

    virtual QString theme() = 0;
    virtual QPointF windowRadius() = 0;
    virtual void setWindowRadius(const QPointF value) = 0;
    virtual qreal borderWidth() = 0;
    virtual void setBorderWidth(int width) = 0;
    virtual QColor borderColor() = 0;
    virtual qreal shadowRadius() = 0;
    virtual QPointF shadowOffset() = 0;
    virtual QColor shadowColor() = 0;
    virtual QMarginsF mouseInputAreaMargins() = 0;
    virtual qreal windowPixelRatio() = 0;
 public Q_SLOTS:
    void setValidProperties(qint64 validProperties);

Q_SIGNALS:
    void validPropertiesChanged(qint64 validProperties);
    void themeChanged();
    void windowRadiusChanged();
    void borderWidthChanged();
    void borderColorChanged();
    void shadowRadiusChanged();
    void shadowOffectChanged();
    void shadowColorChanged();
    void mouseInputAreaMarginsChanged();
    void windowPixelRatioChanged();


protected:
    PropertyFlags   m_validProperties;
};


class X11DecorationStyle : public DecorationStyle
{
    Q_OBJECT
public:

    explicit X11DecorationStyle(Window *window);
    ~X11DecorationStyle();

    PropertyFlags validProperties() override;
    bool propertyIsValid(PropertyFlag p) override;

    QString theme() override;
    QPointF windowRadius() override;
    void setWindowRadius(const QPointF value) override;
    qreal borderWidth() override;
    void setBorderWidth(int width) override;
    QColor borderColor() override;
    qreal shadowRadius() override;
    QPointF shadowOffset() override;
    QColor shadowColor() override;
    QMarginsF mouseInputAreaMargins() override;
    qreal windowPixelRatio() override;

private:
    Window          *m_window;
};

class WaylandDecorationStyle : public DecorationStyle
{
    Q_OBJECT
public:
    explicit WaylandDecorationStyle(Window *window);
    ~WaylandDecorationStyle(){};

    PropertyFlags validProperties() override;
    bool propertyIsValid(PropertyFlag p) override;

    QString theme() {return "";};
    QPointF windowRadius();
    void setWindowRadius(const QPointF value);
    qreal borderWidth();
    void setBorderWidth(int width) override;
    QColor borderColor();
    qreal shadowRadius() {return 50;};
    QPointF shadowOffset() {return QPointF(0,0);};
    QColor shadowColor();
    QMarginsF mouseInputAreaMargins() {return QMarginsF(0,0,0,0);};
    qreal windowPixelRatio() {return 1;};
public Q_SLOT:
    void onUpdateWindowRadiusByWayland(QPointF);

private:
    QPointF m_radius = QPointF(-1, 0);
    qreal   m_border = 1;
    Window  *m_window;
};

}

Q_DECLARE_METATYPE(QMarginsF)
#endif