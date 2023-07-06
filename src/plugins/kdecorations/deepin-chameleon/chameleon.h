// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CHAMELEON_H
#define CHAMELEON_H

#include "chameleontheme.h"

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationButtonGroup>

#include <deepin_kwineffects.h>

#include <QVariant>
#include <QDir>
#include <QSettings>
#include <QScreen>
#include <QPainterPath>
#include <QFont>
#include <DWayland/Server/ddeshell_interface.h>

class Settings;
class ChameleonWindowTheme;
class Chameleon : public KDecoration2::Decoration
{
    Q_OBJECT

public:
    explicit Chameleon(QObject *parent = nullptr, const QVariantList &args = QVariantList());
    ~Chameleon();

    enum class FontType {
        StandardFont,
        FontSize,
    };

    void paint(QPainter *painter, const QRect &repaintArea) override;

    KWin::EffectWindow *effect() const;
    bool noTitleBar() const;

    qreal borderWidth() const;
    qreal titleBarHeight() const;
    qreal shadowRadius() const;
    QPointF shadowOffset() const;
    QPointF windowRadius() const;
    QMarginsF mouseInputAreaMargins() const;

    QColor shadowColor() const;
    QColor borderColor() const;
    QColor getBackgroundColor() const;

    QIcon menuIcon() const;
    QIcon minimizeIcon() const;
    QIcon maximizeIcon() const;
    QIcon unmaximizeIcon() const;
    QIcon closeIcon() const;

    QPointF menuIconPos() const;
    qint32 menuIconWidth() const;
    qint32 menuIconHeight() const;

Q_SIGNALS:
    void noTitleBarChanged(bool noTitleBar);
    void effectInitialized(KWin::EffectWindow *effect);

protected:
    void init() override;

private Q_SLOTS:
    void updateFont(FontType updateType, const QString &val);
    void onAppearanceChanged(const QString &key, const QString &value);

private:
    void initButtons();
    void updateButtonsGeometry();

    void updateTitleGeometry();

    void updateTheme();
    void updateConfig();
    void updateTitleBarArea();
    void updateBorderPath();
    void updateShadow();
    void updateMouseInputAreaMargins();

    void onClientWidthChanged();
    void onClientHeightChanged();
    void onNoTitlebarPropertyChanged(quint32 windowId);

    void onThemeWindowRadiusChanged();
    void onThemeBorderWidthChanged();
    void onThemeBorderColorChanged();
    void onThemeShadowRadiusChanged();
    void onThemeShadowOffsetChanged();

    bool windowNeedRadius() const;
    bool windowNeedBorder() const;

    QColor getTextColor() const;
    qreal getScaleFactor() const;

    bool m_initialized = false;
    qint8 m_noTitleBar = -1;
    qreal m_scaleFactor = 1;
    QObject *m_client = nullptr;

    QMarginsF m_titleBarAreaMargins;
    QPainterPath m_borderPath;
    ChameleonTheme::ConfigGroup* m_baseConfigGroup = nullptr;
    ChameleonTheme::ThemeConfig m_config;
    ChameleonWindowTheme *m_theme = nullptr;

    QString m_title;
    QRect m_titleArea;

    KDecoration2::DecorationButtonGroup *m_leftButtons = nullptr;
    KDecoration2::DecorationButtonGroup *m_rightButtons = nullptr;

    QPointer<KWin::EffectWindow> m_effect;
    QFont m_font;
    KWaylandServer::DDEShellSurfaceInterface * m_ddeShellSurface = nullptr;
};

#endif // CHAMELEON_H
