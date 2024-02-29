/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "windowshadow.h"
#include "window.h"
#include "decorationstyle.h"
#include <QPainter>

namespace KWin
{
QMap<QString, QVector<QImage>> WindowShadow::m_cacheShadow;

WindowShadow::WindowShadow(Window *window)
    : m_window(window)
{
}

WindowShadow::~WindowShadow()
{
}

QString WindowShadow::buildShadowCacheKey(shadowConfig &config)
{
    const QPointF shadow_overlap(qMax(config.windowRadius.x(), 3.0), qMax(config.windowRadius.y(), 3.0));
    const QMargins &paddings = QMargins(config.shadowRadius - config.shadowOffset.x() - shadow_overlap.x(),
                                        config.shadowRadius - config.shadowOffset.y() - shadow_overlap.y(),
                                        config.shadowRadius - shadow_overlap.x(),
                                        config.shadowRadius - shadow_overlap.y());

    return QString("%1_%2.%3_%4_%5_%6.%7.%8.%9").arg(qRound(config.windowRadius.x())).arg(qRound(config.windowRadius.y()))
                                                .arg(paddings.left()).arg(paddings.top()).arg(paddings.right()).arg(paddings.bottom())
                                                .arg(config.shadowColor.name(QColor::HexArgb))
                                                .arg(config.borderWidth).arg(config.borderColor.name());
}

void WindowShadow::updateWindowShadow()
{
    if (!m_window || !m_window->windowStyleObj())
        return;
    if (m_window->hasAlpha()
        && !m_window->windowStyleObj()->propertyIsValid(DecorationStyle::WindowRadiusProperty)
        && !m_window->isWindowMenu()) {
        m_key = "";
        return;
    }
    getShadow();
}

QPointF WindowShadow::getWindowRadius()
{
    QPointF radius(0.0, 0.0);
    if (m_window->windowRadiusObj())
        radius = m_window->windowRadiusObj()->windowRadius();
    return radius;
}

void WindowShadow::getShadow()
{
    shadowConfig st;
    st.windowRadius = getWindowRadius();

    if (m_window->windowStyleObj()->propertyIsValid(DecorationStyle::ShadowOffsetProperty)) {
        st.shadowOffset = m_window->windowStyleObj()->shadowOffset();
    } else {
        st.shadowOffset = QPointF(0, 22);
    }

    if (m_window->windowStyleObj()->propertyIsValid(DecorationStyle::ShadowColorProperty)) {
        st.shadowColor = m_window->windowStyleObj()->shadowColor();
    } else {
        if (m_window->isActive())
            st.shadowColor = QColor("#80000000");
        else
            st.shadowColor = QColor("#40000000");
    }

    if (m_window->windowStyleObj()->propertyIsValid(DecorationStyle::ShadowRadiusProperty)) {
        st.shadowRadius = m_window->windowStyleObj()->shadowRadius();
    } else {
        st.shadowRadius = 50;
    }

    if (m_window->windowStyleObj()->propertyIsValid(DecorationStyle::BorderWidthProperty)) {
        st.borderWidth = m_window->windowStyleObj()->borderWidth();
    } else {
        st.borderWidth = 1;
    }

    if (m_window->windowStyleObj()->propertyIsValid(DecorationStyle::BorderColorProperty)) {
        st.borderColor = m_window->windowStyleObj()->borderColor();
    } else {
        st.borderColor = QColor("#10000000");
    }

    int shadow_size = st.shadowRadius + st.windowRadius.x() + st.windowRadius.y();

    const QPointF shadow_overlap(qMax(st.windowRadius.x(), 3.0), qMax(st.windowRadius.y(), 3.0));
    m_padding = QMargins(shadow_size - st.shadowOffset.x() - shadow_overlap.x(),
                                        shadow_size - st.shadowOffset.y() - shadow_overlap.y(),
                                        shadow_size - shadow_overlap.x(),
                                        shadow_size - shadow_overlap.y());
    m_key = buildShadowCacheKey(st);
    if (m_cacheShadow.contains(m_key))
        return;

    auto shadow = false;//m_shadowCache.value(key);
    bool no_shadow = st.shadowColor.alpha() == 0 || qIsNull(st.shadowRadius);

    if (!shadow) {
        // create image
        qreal shadowStrength = st.shadowColor.alpha();
        QImage image(2 * shadow_size, 2 * shadow_size, QImage::Format_ARGB32);
        image.fill(Qt::transparent);

        if (!no_shadow) {
            // create gradient
            // gaussian delta function
            auto alpha = [](qreal x) { return std::exp(-x * x / 0.15); };

            // color calculation delta function
            auto gradientStopColor = [] (QColor color, int alpha) {
                color.setAlpha(alpha);
                return color;
            };

            QRadialGradient radialGradient(shadow_size, shadow_size, shadow_size);

            for(int i = 0; i < 10; ++i) {
                const qreal x(qreal(i) / 9);
                radialGradient.setColorAt(x, gradientStopColor(st.shadowColor, alpha(x) * shadowStrength * 0.6));
            }

            radialGradient.setColorAt(1, gradientStopColor(st.shadowColor, 0));

            // fill
            QPainter painter(&image);
            painter.setRenderHint( QPainter::Antialiasing, true);
            painter.fillRect(image.rect(), radialGradient);
        }

        // contrast pixel
        QRectF innerRect = QRectF(shadow_size - st.shadowOffset.x() - shadow_overlap.x(),
                                  shadow_size - st.shadowOffset.y() - shadow_overlap.y(),
                                  st.shadowOffset.x() + 2 * shadow_overlap.x(),
                                  st.shadowOffset.y() + 2 * shadow_overlap.y());

        QPainter painter(&image);

        if (st.windowRadius.x() > 0 && st.windowRadius.y() > 0) {
            painter.setRenderHint(QPainter::Antialiasing, true);
        }

        if (st.borderWidth > 0 && st.borderColor.alpha() != 0) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setPen(QPen(st.borderColor, st.borderWidth + 1));
            painter.setBrush(Qt::NoBrush);
            if (st.windowRadius.x() > 0 && st.windowRadius.y() > 0) {
                painter.drawRoundedRect(innerRect, st.windowRadius.x() - 0.5, st.windowRadius.y() - 0.5);
            } else {
                painter.drawRect(innerRect);
            }
        }

        if (!no_shadow) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::black);
            painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
            if (st.windowRadius.x() > 0 && st.windowRadius.y() > 0) {
                painter.drawRoundedRect(innerRect, 0.5 + st.windowRadius.x(), 0.5 + st.windowRadius.y());
            } else {
                painter.drawRect(innerRect);
            }
        }

        QRect innerShadowRect(shadow_size, shadow_size, 1, 1);
        QRect top(innerShadowRect.left(), 0, innerShadowRect.width(), innerShadowRect.top());
        kwin_popup_shadow_top = image.copy(top);
        QRect topright(innerShadowRect.left() + innerShadowRect.width(), 0,
                 image.width() - innerShadowRect.width() - innerShadowRect.left(),
                 innerShadowRect.top());
        kwin_popup_shadow_top_right = image.copy(topright);
        QRect right(innerShadowRect.left() + innerShadowRect.width(),
                 innerShadowRect.top(),
                 image.width() - innerShadowRect.width() - innerShadowRect.left(),
                 innerShadowRect.height());
        kwin_popup_shadow_right = image.copy(right);
        QRect bottomright(innerShadowRect.left() + innerShadowRect.width(),
                 innerShadowRect.top() + innerShadowRect.height(),
                 image.width() - innerShadowRect.width() - innerShadowRect.left(),
                 image.height() - innerShadowRect.top() - innerShadowRect.height());
        kwin_popup_shadow_bottom_right = image.copy(bottomright);
        QRect bottom(innerShadowRect.left(),
                 innerShadowRect.top() + innerShadowRect.height(),
                 innerShadowRect.width(),
                 image.height() - innerShadowRect.top() - innerShadowRect.height());
        kwin_popup_shadow_bottom = image.copy(bottom);
        QRect bottomleft(0, innerShadowRect.top() + innerShadowRect.height(),
                 innerShadowRect.left(),
                 image.height() - innerShadowRect.top() - innerShadowRect.height());
        kwin_popup_shadow_bottom_left = image.copy(bottomleft);
        QRect left(0, innerShadowRect.top(), innerShadowRect.left(), innerShadowRect.height());
        kwin_popup_shadow_left = image.copy(left);
        QRect topleft(0, 0, innerShadowRect.left(), innerShadowRect.top());
        kwin_popup_shadow_top_left = image.copy(topleft);

        WindowShadow::m_cacheShadow[m_key].append(kwin_popup_shadow_top);
        WindowShadow::m_cacheShadow[m_key].append(kwin_popup_shadow_top_right);
        WindowShadow::m_cacheShadow[m_key].append(kwin_popup_shadow_right);
        WindowShadow::m_cacheShadow[m_key].append(kwin_popup_shadow_bottom_right);
        WindowShadow::m_cacheShadow[m_key].append(kwin_popup_shadow_bottom);
        WindowShadow::m_cacheShadow[m_key].append(kwin_popup_shadow_bottom_left);
        WindowShadow::m_cacheShadow[m_key].append(kwin_popup_shadow_left);
        WindowShadow::m_cacheShadow[m_key].append(kwin_popup_shadow_top_left);
    }
}
    
}