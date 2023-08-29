// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "chameleon.h"
#include "chameleonshadow.h"
#include "chameleonbutton.h"
#include "chameleonconfig.h"
#include "chameleontheme.h"
#include "chameleonwindowtheme.h"
#include "kwinutils.h"

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationSettings>
#include <KDecoration2/DecorationButtonGroup>

#include <KConfigCore/KConfig>
#include <KConfigCore/KConfigGroup>

#include <QObject>
#include <QPainter>
#include <QDebug>
#include <QScreen>
#include <QGuiApplication>
#include <QtDBus>
#include <QX11Info>
#include <QMap>

#include "abstract_client.h"

Q_DECLARE_METATYPE(QPainterPath)

Chameleon::Chameleon(QObject *parent, const QVariantList &args)
    : KDecoration2::Decoration(parent, args)
    , m_client(parent)
{
}

Chameleon::~Chameleon()
{
    if (KWin::EffectWindow *effect = this->effect()) {
        // 清理窗口特效的数据
        effect->setData(ChameleonConfig::WindowRadiusRole, QVariant());
        effect->setData(ChameleonConfig::WindowMaskTextureRole, QVariant());
    }
}

void Chameleon::init()
{
    if (m_initialized)
        return;

    auto c = client().data();

    if (!m_client)
        m_client = KWinUtils::findClient(KWinUtils::Predicate::WindowMatch, c->windowId());
    else
        KWinUtils::insertChameleon(c, m_client);

    initButtons();

    // 要放到updateTheme调用之前初始化此对象
    auto global_config = ChameleonConfig::instance();
    m_theme = new ChameleonWindowTheme(m_client, this);

    m_font = qGuiApp->font();
    updateTheme();

    if (!QX11Info::isPlatformX11() && m_client) {
        m_ddeShellSurface = KWinUtils::getDDEShellSurface(m_client);
        if (m_ddeShellSurface) {
            connect(m_ddeShellSurface, &KWaylandServer::DDEShellSurfaceInterface::noTitleBarPropertyRequested, this,
                [this, c] (qint32 value) {
                    if (value != m_noTitleBar) {
                        m_noTitleBar = value;
                        updateTitleBarArea();
                    }
                }
            );
            connect(m_ddeShellSurface, &KWaylandServer::DDEShellSurfaceInterface::windowRadiusPropertyRequested, this,
                [this] (QPointF windowRadius) {
                    m_theme->setValidProperties(ChameleonWindowTheme::WindowRadiusProperty);
                    if (m_theme->propertyIsValid(ChameleonWindowTheme::WindowRadiusProperty)) {
                        if (windowRadius != m_theme->windowRadius()) {
                            m_theme->setProperty("windowRadius",windowRadius);
                            updateBorderPath();
                        }
                    }
                }
            );
        }
    }

    connect(global_config, &ChameleonConfig::themeChanged, this, &Chameleon::updateTheme);
    connect(global_config, &ChameleonConfig::windowNoTitlebarPropertyChanged, this, &Chameleon::onNoTitlebarPropertyChanged);
    connect(settings().data(), &KDecoration2::DecorationSettings::alphaChannelSupportedChanged, this, &Chameleon::updateConfig);
    connect(c, &KDecoration2::DecoratedClient::activeChanged, this, &Chameleon::updateConfig);
    connect(c, &KDecoration2::DecoratedClient::widthChanged, this, &Chameleon::onClientWidthChanged);
    connect(c, &KDecoration2::DecoratedClient::heightChanged, this, &Chameleon::onClientHeightChanged);
    if (QX11Info::isPlatformX11()) {
        connect(c, &KDecoration2::DecoratedClient::maximizedChanged, this, &Chameleon::updateTitleBarArea);
    } else {
        connect(c, &KDecoration2::DecoratedClient::maximizedChanged, this, &Chameleon::updateTitleBarArea, Qt::QueuedConnection);
    }
    connect(c, &KDecoration2::DecoratedClient::adjacentScreenEdgesChanged, this, &Chameleon::updateBorderPath);
    connect(c, &KDecoration2::DecoratedClient::maximizedHorizontallyChanged, this, &Chameleon::updateBorderPath);
    connect(c, &KDecoration2::DecoratedClient::maximizedVerticallyChanged, this, &Chameleon::updateBorderPath);
    connect(c, &KDecoration2::DecoratedClient::captionChanged, this, &Chameleon::updateTitleGeometry);
    connect(c, &KDecoration2::DecoratedClient::maximizeableChanged, this, &Chameleon::updateTitleBarArea);
    connect(this, &Chameleon::noTitleBarChanged, this, &Chameleon::updateTitleBarArea, Qt::QueuedConnection);
    connect(m_theme, &ChameleonWindowTheme::themeChanged, this, &Chameleon::updateTheme);
    connect(m_theme, &ChameleonWindowTheme::windowRadiusChanged, this, &Chameleon::updateBorderPath);
    connect(m_theme, &ChameleonWindowTheme::windowRadiusChanged, this, &Chameleon::updateShadow);
    connect(m_theme, &ChameleonWindowTheme::borderWidthChanged, this, &Chameleon::updateShadow);
    connect(m_theme, &ChameleonWindowTheme::borderColorChanged, this, &Chameleon::updateShadow);
    connect(m_theme, &ChameleonWindowTheme::shadowRadiusChanged, this, &Chameleon::updateShadow);
    connect(m_theme, &ChameleonWindowTheme::shadowOffectChanged, this, &Chameleon::updateShadow);
    connect(m_theme, &ChameleonWindowTheme::shadowColorChanged, this, &Chameleon::updateShadow);
    connect(m_theme, &ChameleonWindowTheme::mouseInputAreaMarginsChanged, this, &Chameleon::updateMouseInputAreaMargins);
    connect(m_theme, &ChameleonWindowTheme::windowPixelRatioChanged, this, &Chameleon::updateShadow);
    connect(m_theme, &ChameleonWindowTheme::windowPixelRatioChanged, this, &Chameleon::updateTitleBarArea);

    connect(global_config, &ChameleonConfig::appearanceChanged, this, &Chameleon::onAppearanceChanged);
    connect(global_config, &ChameleonConfig::globalWindowRadiusChanged, this, [=]() {
        updateShadow();
        updateBorderPath();
        updateTitleBarArea();
    });

    QTimer::singleShot(0, this, [=] {
        if (QDBusInterface interface("org.deepin.dde.Appearance1",
                                     "/org/deepin/dde/Appearance1",
                                     "org.freedesktop.DBus.Properties");
            interface.isValid())
        {
            QDBusPendingCall pcall = interface.asyncCall(QLatin1String("Get"), "FontSize");
            QDBusPendingCallWatcher *watcherFontSize = new QDBusPendingCallWatcher(pcall, this);
            connect(watcherFontSize, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<QString> reply = *watcher;
                if (!reply.isError()) {
                    onAppearanceChanged("fontsize", reply.argumentAt<0>());
                }
                watcher->deleteLater();
            });
            onAppearanceChanged("fontsize", interface.property("FontSize").value<QString>());
            QDBusPendingCall pcallstandardfont = interface.asyncCall(QLatin1String("Get"), "StandardFont");
            QDBusPendingCallWatcher *watcherStandardFont = new QDBusPendingCallWatcher(pcallstandardfont, this);
            connect(watcherStandardFont, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<QString> reply = *watcher;
                if (!reply.isError()) {
                    onAppearanceChanged("standardfont", reply.argumentAt<0>());
                }
                watcher->deleteLater();
            });
        }
    });

    m_initialized = true;

    KWinUtils::instance()->setInitialized();
}

void Chameleon::updateFont(FontType updateType, const QString &val)
{
    switch (updateType) {
        case FontType::StandardFont:
            m_font.setFamily(val);
            break;
        case FontType::FontSize:
            double value = val.toDouble();
            if (value <= 0)
                return;
            m_font.setPointSizeF(value * getScaleFactor());
            break;
    }

    updateTitleGeometry();
}

void Chameleon::paint(QPainter *painter, const QRect &repaintArea)
{
    auto s = settings().data();

    if (!noTitleBar()) {
        if (windowNeedRadius()) {
            painter->setClipPath(m_borderPath);
        }
        painter->setFont(m_font);

        painter->fillRect(titleBar() & repaintArea, getBackgroundColor());
        painter->setPen(getTextColor());
        painter->drawText(m_titleArea, Qt::AlignCenter, m_title);

        // draw all buttons
        m_leftButtons->paint(painter, repaintArea);
        m_rightButtons->paint(painter, repaintArea);
    }

    if (windowNeedBorder()) {
        qreal border_width = borderWidth();

        // 支持alpha通道时在阴影上绘制border
        if (!qIsNull(border_width)) {
            if (noTitleBar()) {
                painter->fillPath(m_borderPath, borderColor());
            } else {
                // 绘制path是沿着路径外圈绘制，所以此处应该+1才能把border绘制到窗口边缘
                painter->strokePath(m_borderPath, QPen(borderColor(), border_width + 1));
            }
        }
    }
}

KWin::EffectWindow *Chameleon::effect() const
{
    if (m_effect)
        return m_effect.data();

    if (!m_client)
        return nullptr;

    Chameleon *self = const_cast<Chameleon*>(this);
    self->m_effect = m_client->findChild<KWin::EffectWindow*>(QString(), Qt::FindDirectChildrenOnly);
    Q_EMIT self->effectInitialized(m_effect.data());

    return m_effect.data();
}

bool Chameleon::noTitleBar() const
{
    if (m_noTitleBar < 0) {
        // 需要初始化
        const QByteArray &data = KWinUtils::instance()->readWindowProperty(client().data()->windowId(),
                                                                           ChameleonConfig::instance()->atomDeepinNoTitlebar(),
                                                                           XCB_ATOM_CARDINAL);

        qint8 no_titlebar = !data.isEmpty() && data.at(0);

        if (no_titlebar != m_noTitleBar) {
            const_cast<Chameleon*>(this)->m_noTitleBar = no_titlebar;

            Q_EMIT const_cast<Chameleon*>(this)->noTitleBarChanged(m_noTitleBar);
        }
    }

    return m_noTitleBar;
}

qreal Chameleon::borderWidth() const
{
    if (m_theme->propertyIsValid(ChameleonWindowTheme::BorderWidthProperty)) {
        return m_theme->borderWidth();
    }

    return m_config.borderConfig.borderWidth;
}

qreal Chameleon::titleBarHeight() const
{
    return m_config.titlebarConfig.height * getScaleFactor();
}

qreal Chameleon::shadowRadius() const
{
    if (m_theme->propertyIsValid(ChameleonWindowTheme::ShadowRadiusProperty)) {
        return m_theme->shadowRadius();
    }

    return m_config.shadowConfig.shadowRadius;
}

QPointF Chameleon::shadowOffset() const
{
    if (m_theme->propertyIsValid(ChameleonWindowTheme::ShadowOffsetProperty)) {
        return m_theme->shadowOffset();
    }

    return m_config.shadowConfig.shadowOffset;
}

QPointF Chameleon::windowRadius() const
{
    if (m_theme->propertyIsValid(ChameleonWindowTheme::WindowRadiusProperty)) {
        return m_theme->windowRadius();
    }

    auto globalWindowRadius = ChameleonConfig::instance()->globalWindowRadius();
    if (globalWindowRadius != ChameleonConfig::InvalidRadius)
        return globalWindowRadius;

    return m_config.radius * getScaleFactor();
}

QMarginsF Chameleon::mouseInputAreaMargins() const
{
    if (m_theme->propertyIsValid(ChameleonWindowTheme::MouseInputAreaMargins)) {
        return m_theme->mouseInputAreaMargins();
    }

    return m_config.mouseInputAreaMargins;
}

QColor Chameleon::shadowColor() const
{
    if (m_theme->propertyIsValid(ChameleonWindowTheme::ShadowColorProperty)) {
        return m_theme->shadowColor();
    }

    return m_config.shadowConfig.shadowColor;
}

QColor Chameleon::borderColor() const
{
    if (m_theme->propertyIsValid(ChameleonWindowTheme::BorderColorProperty)) {
        return m_theme->borderColor();
    }

    return m_config.borderConfig.borderColor;
}

QIcon Chameleon::menuIcon() const
{
    return m_config.titlebarConfig.menuBtn.btnIcon;
}

QIcon Chameleon::minimizeIcon() const
{
    return m_config.titlebarConfig.minimizeBtn.btnIcon;
}

QIcon Chameleon::maximizeIcon() const
{
    return m_config.titlebarConfig.maximizeBtn.btnIcon;
}

QIcon Chameleon::unmaximizeIcon() const
{
    return m_config.titlebarConfig.unmaximizeBtn.btnIcon;
}

QIcon Chameleon::closeIcon() const
{
    return m_config.titlebarConfig.closeBtn.btnIcon;
}

QPointF Chameleon::menuIconPos() const
{
    return m_config.titlebarConfig.menuBtn.pos * getScaleFactor();
}

qint32 Chameleon::menuIconWidth() const
{
    return m_config.titlebarConfig.menuBtn.width * getScaleFactor();
}

qint32 Chameleon::menuIconHeight() const
{
    return m_config.titlebarConfig.menuBtn.height *getScaleFactor();
}

ChameleonTheme::ThemeConfig Chameleon::theme() const
{
    return m_config;
}

void Chameleon::initButtons()
{
    m_leftButtons = new KDecoration2::DecorationButtonGroup(KDecoration2::DecorationButtonGroup::Position::Left, this, &ChameleonButton::create);
    m_rightButtons = new KDecoration2::DecorationButtonGroup(KDecoration2::DecorationButtonGroup::Position::Right, this, &ChameleonButton::create);
    connect(m_rightButtons, &KDecoration2::DecorationButtonGroup::geometryChanged,
            this, &Chameleon::updateTitleBarArea, Qt::QueuedConnection);
}

void Chameleon::updateButtonsGeometry()
{
    auto s = settings();
    auto c = client().data();

    // adjust button position
    const int bHeight = noTitleBar() ? 0 : titleBarHeight();
    KWinUtils::Window::setTitleBarHeight(m_client, bHeight);

    const int bWidth = m_config.titlebarConfig.width * getScaleFactor();

    for (const QPointer<KDecoration2::DecorationButton> &button : m_leftButtons->buttons() + m_rightButtons->buttons()) {
        button.data()->setGeometry(QRectF(QPoint(0, 0), QSizeF(bWidth, bHeight)));
    }

    // left buttons
    if (!m_leftButtons->buttons().isEmpty()) {
        // spacing
        m_leftButtons->setSpacing(0);

        // padding
        const int vPadding = 0;
        const int hPadding = s->smallSpacing();

        m_leftButtons->buttons().front()->setGeometry(QRectF(menuIconPos(), QSizeF(menuIconWidth(), menuIconHeight())));
        if (c->isMaximizedHorizontally()) {
            // add offsets on the side buttons, to preserve padding, but satisfy Fitts law
            m_leftButtons->setPos(QPointF(menuIconPos().x(), menuIconPos().y() + vPadding));
        } else {
            m_leftButtons->setPos(QPointF(menuIconPos().x() + hPadding + borderLeft(), menuIconPos().y() + vPadding));
        }
    }

    // right buttons
    if (!m_rightButtons->buttons().isEmpty()) {
        // spacing
        m_rightButtons->setSpacing(s->smallSpacing());

        // padding
        const int vPadding = 0;
        const int hPadding = 0;

        if (c->isMaximizedHorizontally()) {
            m_rightButtons->buttons().back()->setGeometry(QRectF(QPoint(0, 0), QSizeF(bWidth + hPadding, bHeight)));
            m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width(), vPadding));
        } else {
            m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width() - hPadding - borderRight(), vPadding));
        }
    }

    updateTitleGeometry();
}

void Chameleon::updateTitleGeometry()
{
    auto s = settings();

    m_titleArea = titleBar();

    m_title = client().data()->caption();
    // 使用系统字体，不要使用 settings() 中的字体
    const QFontMetricsF fontMetrics(m_font);
    int full_width = fontMetrics.width(m_title) * getScaleFactor();

    if (m_config.titlebarConfig.area == Qt::TopEdge || m_config.titlebarConfig.area == Qt::BottomEdge) {
        int buttons_width = m_leftButtons->geometry().width()
            + m_rightButtons->geometry().width() + 2 * s->smallSpacing();

        m_titleArea.setWidth(m_titleArea.width() - buttons_width);
        m_titleArea.moveLeft(m_leftButtons->geometry().right() + s->smallSpacing());

        if (full_width < (m_titleArea.right() - titleBar().center().x()) * 2) {
            m_titleArea.setWidth(full_width);
            m_titleArea.moveCenter(titleBar().center());

        } else if (full_width > m_titleArea.width()) {
            m_title = fontMetrics.elidedText(m_title,
                    Qt::ElideRight, qMax(m_titleArea.width(), m_titleArea.height()));
            m_titleArea.moveRight(m_rightButtons->geometry().left() + s->smallSpacing());

        } else {
            m_titleArea.setWidth(full_width);
            m_titleArea.moveRight(m_rightButtons->geometry().left() + s->smallSpacing());
        }

    } else  {
        int buttons_height = m_leftButtons->geometry().height()
            + m_rightButtons->geometry().height() + 2 * s->smallSpacing();

        m_titleArea.setHeight(m_titleArea.height() - buttons_height);
        m_titleArea.moveTop(m_leftButtons->geometry().bottom() + s->smallSpacing());

        if (full_width < (m_titleArea.bottom() - titleBar().center().y()) * 2) {
            m_titleArea.setHeight(full_width);
            m_titleArea.moveCenter(titleBar().center());

        } else if (full_width > m_titleArea.height()) {
            m_title = fontMetrics.elidedText(m_title,
                    Qt::ElideRight, qMax(m_titleArea.width(), m_titleArea.height()));
            m_titleArea.moveBottom(m_rightButtons->geometry().top() + s->smallSpacing());

        } else {
            m_titleArea.setWidth(full_width);
            m_titleArea.moveBottom(m_rightButtons->geometry().top() + s->smallSpacing());
        }
    }

    update();
}

void Chameleon::updateTheme()
{
    QString theme_name;

    if (m_theme->propertyIsValid(ChameleonWindowTheme::ThemeProperty)) {
        theme_name = m_theme->theme();
    }

    KWin::AbstractClient* client = dynamic_cast<KWin::AbstractClient*>(m_client);
    if (!client) {
        qCCritical(CHAMELEON) << "The AbstractClient corresponding to chameleon is nullptr";
    }

    qCDebug(CHAMELEON) << "windowId: " << QString("0x%1").arg(client->property("windowId").toULongLong(), 0, 16) << " windowType: " << client->windowType();

    ChameleonTheme::ConfigGroup* configGroup;

    if (!theme_name.isEmpty()) {
        ChameleonTheme::instance()->loadTheme(theme_name);
    }

    configGroup = ChameleonTheme::instance()->themeConfig(client->windowType());

    if (m_baseConfigGroup == configGroup) {
        return;
    }

    m_baseConfigGroup = configGroup;
    updateConfig();
}

void Chameleon::updateConfig()
{
    auto c = client().data();

    bool active = c->isActive();
    bool hasAlpha = settings()->isAlphaChannelSupported();

    // NOTE: base config is read from preset configuration, override it.
    m_config = active ? m_baseConfigGroup->normal : m_baseConfigGroup->inactive;

    updateMouseInputAreaMargins();
    updateTitleBarArea();
    // 解决关闭应用更新shadow闪屏的问题(bug87758)
    // if ((c == sender()) && !active) {
    //     return;
    // }
    // 窗口边框特效-阴影
    KConfig config("deepin-kwinrc", KConfig::CascadeConfig);
    KConfigGroup group_shadow(&config, "Compositing");
    if (group_shadow.hasKey("window_border_effect")) {
        if (group_shadow.readEntry("window_border_effect") == "true") {
            updateShadow();
        }
    } else {
        updateShadow();
    }
    update();
}

void Chameleon::updateTitleBarArea()
{
    auto c = client().data();

    m_titleBarAreaMargins.setLeft(0);
    m_titleBarAreaMargins.setTop(0);
    m_titleBarAreaMargins.setRight(0);
    m_titleBarAreaMargins.setBottom(0);

    qreal border_width = windowNeedBorder() ? borderWidth() : 0;
    qreal titlebar_height = noTitleBar() ? 0 : titleBarHeight();

    switch (m_config.titlebarConfig.area) {
    case Qt::LeftEdge:
        m_titleBarAreaMargins.setLeft(titlebar_height);
        setTitleBar(QRect(border_width, border_width, titlebar_height, c->height()));
        setBorders(QMargins(border_width + titlebar_height, border_width,
                            border_width, border_width));
        break;
    case Qt::RightEdge:
        m_titleBarAreaMargins.setRight(titlebar_height);
        setTitleBar(QRect(border_width + c->width() - titlebar_height, border_width,
                          titlebar_height, c->height()));
        setBorders(QMargins(border_width, border_width,
                            border_width + titlebar_height, border_width));
        break;
    case Qt::TopEdge:
        m_titleBarAreaMargins.setTop(titlebar_height);
        setTitleBar(QRect(border_width, border_width,
                          c->width(), titlebar_height));
        setBorders(QMargins(border_width, border_width + titlebar_height,
                            border_width, border_width));
        break;
    case Qt::BottomEdge:
        m_titleBarAreaMargins.setBottom(titlebar_height);
        setTitleBar(QRect(border_width, border_width + c->height() - titlebar_height,
                          c->width(), titlebar_height));
        setBorders(QMargins(border_width, border_width,
                            border_width, border_width + titlebar_height));
        break;
    default:
        return;
    }

    updateBorderPath();
    updateButtonsGeometry();
}

void Chameleon::updateBorderPath()
{
    auto c = client().data();
    QRectF client_rect(0, 0, c->width(), c->height());
    client_rect += borders();
    client_rect.moveTopLeft(QPointF(0, 0));

    QPainterPath path;
    KWin::EffectWindow *effect = this->effect();

    if (windowNeedRadius()) {
        auto window_radius = windowRadius();
        path.addRoundedRect(client_rect, window_radius.x(), window_radius.y());

        if (effect) {
            const QVariant &effect_window_radius = effect->data(ChameleonConfig::WindowRadiusRole);
            bool need_update = true;

            if (effect_window_radius.isValid()) {
                auto old_window_radius = effect_window_radius.toPointF();

                if (old_window_radius == window_radius) {
                    need_update = false;
                }
            }

            if (need_update) {
                // 清理已缓存的旧的窗口mask材质
                effect->setData(ChameleonConfig::WindowMaskTextureRole, QVariant());
                // 设置新的窗口圆角
                if (window_radius.isNull()) {
                    effect->setData(ChameleonConfig::WindowRadiusRole, QVariant());
                } else {
                    effect->setData(ChameleonConfig::WindowRadiusRole, QVariant::fromValue(window_radius));
                }
            }
        }
    } else {
        path.addRect(client_rect);

        if (effect) {
            // 清理已缓存的旧的窗口mask材质
            effect->setData(ChameleonConfig::WindowMaskTextureRole, QVariant());
            // 清理窗口圆角的设置
            effect->setData(ChameleonConfig::WindowRadiusRole, QVariant());
        }
    }

    m_borderPath = path;

    update();
}

void Chameleon::updateShadow()
{
    // TODO: should use std::options to check m_config valid?
    if (settings()->isAlphaChannelSupported()) {
        // 优先使用窗口自己设置的属性
        m_config.radius = windowRadius();
        auto w = effect();
        QPointF maxWindowRadius = QPointF(w->width() / 2.0, w->height() / 2.0);

        if (m_theme->propertyIsValid(ChameleonWindowTheme::BorderWidthProperty)) {
            m_config.borderConfig.borderWidth = m_theme->borderWidth();
        }

        if (m_theme->propertyIsValid(ChameleonWindowTheme::BorderColorProperty)) {
            m_config.borderConfig.borderColor = m_theme->borderColor();
        }

        if (m_theme->propertyIsValid(ChameleonWindowTheme::ShadowRadiusProperty)) {
            m_config.shadowConfig.shadowRadius = m_theme->shadowRadius();
        }

        if (m_theme->propertyIsValid(ChameleonWindowTheme::ShadowOffsetProperty)) {
            m_config.shadowConfig.shadowOffset = m_theme->shadowOffset();
        }

        if (m_theme->propertyIsValid(ChameleonWindowTheme::ShadowColorProperty)) {
            m_config.shadowConfig.shadowColor = m_theme->shadowColor();
        }

        // 这里的数据是已经缩放过的，因此scale值需要为1
        setShadow(ChameleonShadow::instance()->getShadow(m_config, 1.0, maxWindowRadius));
    }
}

void Chameleon::updateMouseInputAreaMargins()
{
    setResizeOnlyBorders(mouseInputAreaMargins().toMargins());
}

void Chameleon::onClientWidthChanged()
{
    updateTitleBarArea();
}

void Chameleon::onClientHeightChanged()
{
    updateTitleBarArea();
}

void Chameleon::onNoTitlebarPropertyChanged(quint32 windowId)
{
    if (client().data()->windowId() != windowId)
        return;

    // 标记为未初始化状态
    m_noTitleBar = -1;
}

void Chameleon::onAppearanceChanged(const QString &key, const QString &value)
{
    if (key.toLower() == "fontsize") {
        updateFont(FontType::FontSize, value);
    }

    if (key.toLower() == "standardfont") {
        updateFont(FontType::StandardFont, value);
    }
}

bool Chameleon::windowNeedRadius() const
{
    auto c = client().data();
    return KWinUtils::instance()->isCompositing() && c->adjacentScreenEdges() == Qt::Edges();
}

bool Chameleon::windowNeedBorder() const
{
    if (client().data()->isMaximized())
        return false;

    // 开启窗口混成时可以在阴影图片中绘制窗口边框
    if (settings()->isAlphaChannelSupported()) {
        return false;
    }

    return true;
}

QColor Chameleon::getTextColor() const
{
    if (m_config.titlebarConfig.font.textColor.isValid())
        return m_config.titlebarConfig.font.textColor;

    auto c = client().data();

    return  c->color(c->isActive() ? KDecoration2::ColorGroup::Active : KDecoration2::ColorGroup::Inactive, KDecoration2::ColorRole::Foreground);
}

qreal Chameleon::getScaleFactor() const
{
    if (m_theme->propertyIsValid(ChameleonWindowTheme::WindowPixelRatioProperty)) {
        return m_theme->windowPixelRatio();
    }

    return ChameleonConfig::instance()->screenScaleFactor();
}

QColor Chameleon::getBackgroundColor() const
{
    if (m_config.titlebarConfig.backgroundColor.isValid())
        return m_config.titlebarConfig.backgroundColor;

    auto c = client().data();

    return c->color(c->isActive() ? KDecoration2::ColorGroup::Active : KDecoration2::ColorGroup::Inactive, KDecoration2::ColorRole::TitleBar);
}
