/*
    SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KCModule>
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationThemeProvider>
#include <KPluginMetaData>
#include <QElapsedTimer>
#include <QVariant>

class QQmlComponent;
class QQmlContext;
class QQmlEngine;
class QQuickItem;

class KConfigLoader;

namespace KWin
{
class Borders;
class OffscreenQuickView;
}

namespace Aurorae
{

class Decoration : public KDecoration3::Decoration
{
    Q_OBJECT
    Q_PROPERTY(KDecoration3::DecoratedWindow *client READ clientPointer CONSTANT)
    Q_PROPERTY(QQuickItem *item READ item)
public:
    explicit Decoration(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~Decoration() override;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void paint(QPainter *painter, const QRect &repaintRegion) override;
#else
    void paint(QPainter *painter, const QRectF &repaintRegion) override;
#endif

    Q_INVOKABLE QVariant readConfig(const QString &key, const QVariant &defaultValue = QVariant());

    KDecoration3::DecoratedWindow *clientPointer() const;
    QQuickItem *item() const;

public Q_SLOTS:

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void init() override;
#else
    bool init() override;
#endif

    void installTitleItem(QQuickItem *item);

    void updateShadow();
    void updateBlur();

Q_SIGNALS:
    void configChanged();

protected:
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setupBorders(QQuickItem *item);
    void updateBorders();
    void updateBuffer();
    void updateExtendedBorders();

    bool m_supportsMask{false};
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QRect m_contentRect; // the geometry of the part of the buffer that is not a shadow when buffer was created.
#else
    QRectF m_contentRect;
#endif
    std::unique_ptr<QQuickItem> m_item;
    std::unique_ptr<QQmlContext> m_qmlContext;
    KWin::Borders *m_borders;
    KWin::Borders *m_maximizedBorders;
    KWin::Borders *m_extendedBorders;
    KWin::Borders *m_padding;
    QString m_themeName;

    std::unique_ptr<QWindow> m_dummyWindow;
    std::unique_ptr<KWin::OffscreenQuickView> m_view;
};

class ThemeProvider : public KDecoration3::DecorationThemeProvider
{
    Q_OBJECT
public:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    explicit ThemeProvider(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
#else
    explicit ThemeProvider(QObject *parent, const KPluginMetaData &data);
#endif
    QList<KDecoration3::DecorationThemeMetaData> themes() const override
    {
        return m_themes;
    }

private:
    void init();
    void findAllQmlThemes();
    void findAllSvgThemes();
    bool hasConfiguration(const QString &theme);
    QList<KDecoration3::DecorationThemeMetaData> m_themes;
    const KPluginMetaData m_data;
};

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
class ConfigurationModule : public KCModule
{
    Q_OBJECT
public:
    ConfigurationModule(QWidget *parent, const QVariantList &args);

private:
    void init();
    void initSvg();
    void initQml();
    QString m_theme;
    KConfigLoader *m_skeleton = nullptr;
    int m_buttonSize;
};
#endif
}
