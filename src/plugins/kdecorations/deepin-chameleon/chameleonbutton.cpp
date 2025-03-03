/*
 * Copyright (C) 2017 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     zccrs <zccrs@live.com>
 *
 * Maintainer: zccrs <zhangjide@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "chameleonbutton.h"
#include "chameleon.h"
#include "kwinutils.h"
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <QDebug>
#include <QHoverEvent>
#include <QPainter>
#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <private/qtx11extras_p.h>
#endif
#include "workspace.h"

#define LONG_PRESS_TIME     300
#define OUT_RELEASE_EVENT   100

ChameleonButton::ChameleonButton(KDecoration3::DecorationButtonType type, const QPointer<KDecoration3::Decoration> &decoration, QObject *parent)
    : KDecoration3::DecorationButton(type, decoration, parent)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto c = decoration->client().data();
#else
    auto c = decoration->window();
#endif
    // qDebug()<<__FUNCTION__<<__LINE__<<"windowId: "<<c->windowId();

    m_type = type;

    switch (type) {
    case KDecoration3::DecorationButtonType::Menu:
        break;
    case KDecoration3::DecorationButtonType::Minimize:
        setVisible(c->isMinimizeable());
        connect(c, &KDecoration3::DecoratedWindow::minimizeableChanged, this, &ChameleonButton::setVisible);
        break;
    case KDecoration3::DecorationButtonType::Maximize:
        setVisible(c->isMaximizeable());
        connect(c, &KDecoration3::DecoratedWindow::maximizeableChanged, this, &ChameleonButton::setVisible);
        break;
    case KDecoration3::DecorationButtonType::Close:
        setVisible(c->isCloseable());
        connect(c, &KDecoration3::DecoratedWindow::closeableChanged, this, &ChameleonButton::setVisible);
        break;
    default: // 隐藏不支持的按钮
        setVisible(false);
        break;
    }
    if (m_type == KDecoration3::DecorationButtonType::Maximize) {
        connect(KWinUtils::compositor(), SIGNAL(compositingToggled(bool)), this, SLOT(onCompositorChanged(bool)));
    }
}

ChameleonButton::~ChameleonButton()
{
    KWinUtils::setSplitMenuKeepShowing(false);
    KWinUtils::hideSplitMenu(false);
}

KDecoration3::DecorationButton *ChameleonButton::create(KDecoration3::DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent)
{
    return new ChameleonButton(type, decoration, parent);
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void ChameleonButton::paint(QPainter *painter, const QRect &repaintRegion)
#else
void ChameleonButton::paint(QPainter *painter, const QRectF &repaintRegion)
#endif
{
    Q_UNUSED(repaintRegion)

    Chameleon *decoration = qobject_cast<Chameleon*>(this->decoration());

    if (!decoration)
        return;

    const int length = std::max(geometry().width(), geometry().height());
    QRect rect(0, 0, length, length);
    {
        // move to center
        QRectF rf(rect);
        rf.moveCenter(geometry().center());
        rect = rf.toRect();
    }

    painter->save();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto c = decoration->client().data();
#else
    auto c = decoration->window();
#endif

    QIcon::Mode state = QIcon::Normal;

    if (!isEnabled()) {
        state = QIcon::Disabled;
    } else if (isPressed()) {
        state = QIcon::Selected;
    } else if (isHovered()) {
        state = QIcon::Active;
    }

    switch (type()) {
    case KDecoration3::DecorationButtonType::Menu: {
        c->icon().paint(painter, rect);
        break;
    }
    case KDecoration3::DecorationButtonType::ApplicationMenu: {
        decoration->menuIcon().paint(painter, rect, Qt::AlignCenter, state);
        break;
    }
    case KDecoration3::DecorationButtonType::Minimize: {
        decoration->minimizeIcon().paint(painter, rect, Qt::AlignCenter, state);
        break;
    }
    case KDecoration3::DecorationButtonType::Maximize: {
        if (isChecked())
            decoration->unmaximizeIcon().paint(painter, rect, Qt::AlignCenter, state);
        else
            decoration->maximizeIcon().paint(painter, rect, Qt::AlignCenter, state);
        break;
    }
    case KDecoration3::DecorationButtonType::Close: {
        decoration->closeIcon().paint(painter, rect, Qt::AlignCenter, state);
        break;
    }
    default:
        break;
    }

    painter->restore();
}

void ChameleonButton::hoverEnterEvent(QHoverEvent *event)
{
    if (!m_isMaxAvailble)
        return;

    if (KWinUtils::instance()->isCompositing()) {
        Chameleon *decoration = qobject_cast<Chameleon*>(this->decoration());
        if (decoration) {
            effect = decoration->effect();
            if (effect && !effect->isUserMove()) {
                KDecoration3::DecorationButton::hoverEnterEvent(event);

                if (!contains(event->posF()) || !isVisible() || !isEnabled()) {
                    return;
                }
                if (m_type == KDecoration3::DecorationButtonType::Maximize) {
                    if (KWinUtils::instance()->isCompositing()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                        auto c = decoration->client().data();
#else
                        auto c = decoration->window();
#endif
                        if (c) {
                            // FIXME
                            uint32_t wid = 0;
                            // uint32_t wid = effect->isWaylandClient() ? c->decorationId() : c->windowId();
                            QRect button_rect(QPoint(geometry().x() + effect->pos().x(), effect->pos().y()),
                                              QSize(geometry().width(), decoration->titleBarHeight()));
                            KWinUtils::showSplitMenu(button_rect, wid);
                        }
                    }
                    decoration->requestHideToolTip();
                }
            }
        }
    } else {
        KDecoration3::DecorationButton::hoverEnterEvent(event);
    }
}

void ChameleonButton::hoverLeaveEvent(QHoverEvent *event)
{
    if (KWinUtils::instance()->isCompositing()) {
        Chameleon *decoration = qobject_cast<Chameleon*>(this->decoration());
        if (decoration) {
            effect = decoration->effect();
            if (max_hover_timer && m_type == KDecoration3::DecorationButtonType::Maximize) {
                max_hover_timer->stop();
            }
            if (effect && !effect->isUserMove()) {
                KDecoration3::DecorationButton::hoverLeaveEvent(event);
                if (m_type == KDecoration3::DecorationButtonType::Maximize) {
                    KWinUtils::hideSplitMenu(true);
                }
            }
        }
    } else {
        KDecoration3::DecorationButton::hoverLeaveEvent(event);
    }
}

void ChameleonButton::mousePressEvent(QMouseEvent *event)
{
    KDecoration3::DecorationButton::mousePressEvent(event);
    if (m_type == KDecoration3::DecorationButtonType::Maximize) {
        if (!max_timer) {
            max_timer = new QTimer();
            max_timer->setSingleShot(true);
            connect(max_timer, &QTimer::timeout, [this] {
                if (m_isMaxAvailble) {
                    m_isMaxAvailble = false;
                    Chameleon *decoration = qobject_cast<Chameleon*>(this->decoration());
                    if (decoration) {
                        effect = decoration->effect();
                        if (effect) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                            auto c = decoration->client().data();
#else
                            auto c = decoration->window();
#endif
                            if (c) {
                                // FIXME
                                uint32_t wid = 0;
                                // uint32_t wid = effect->isWaylandClient() ? c->decorationId() : c->windowId();
                                KWinUtils::setSplitMenuKeepShowing(true);
                                QRect button_rect(QPoint(geometry().x() + effect->pos().x(), effect->pos().y()),
                                                  QSize(geometry().width(), decoration->titleBarHeight()));
                                KWinUtils::showSplitMenu(button_rect, wid);
                            }
                        }
                    }
                }
            });
            max_timer->start(LONG_PRESS_TIME);
            m_mousePosX = event->pos().x();
        } else {
            max_timer->start(LONG_PRESS_TIME);
            m_mousePosX = event->pos().x();
        }
    }
}

void ChameleonButton::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_type == KDecoration3::DecorationButtonType::Maximize) {
        if (max_timer) {
            max_timer->stop();
        }
        if (!geometry().contains(event->localPos()))
            KWinUtils::setSplitMenuKeepShowing(false);
        if (!m_isMaxAvailble) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            event->setLocalPos(QPointF(event->localPos().x() - OUT_RELEASE_EVENT, event->localPos().y()));
#endif
        }
        KWinUtils::hideSplitMenu(false);
        KWinUtils::setSplitMenuKeepShowing(false);
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    KDecoration3::DecorationButton::mouseReleaseEvent(event);
#else
    QMouseEvent newEvent(
        event->type(),
        QPointF(event->position().x() - OUT_RELEASE_EVENT, event->position().y()), // 新的局部位置
        event->scenePosition(),
        event->globalPosition(),
        event->button(),
        event->buttons(),
        event->modifiers());
    KDecoration3::DecorationButton::mouseReleaseEvent(&newEvent);
#endif
    m_isMaxAvailble = true;
}

void ChameleonButton::onCompositorChanged(bool active)
{
    if (!active) {
        KWinUtils::hideSplitMenu(false);
    }
}
