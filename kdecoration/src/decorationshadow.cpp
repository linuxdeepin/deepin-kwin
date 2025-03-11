/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#include "decorationshadow.h"
#include "decorationshadow_p.h"

namespace KDecoration2
{
DecorationShadow::Private::Private(DecorationShadow *parent)
    : q(parent)
{
}

DecorationShadow::Private::~Private() = default;

DecorationShadow::DecorationShadow()
    : QObject()
    , d(new Private(this))
{
}

DecorationShadow::~DecorationShadow() = default;

QRect DecorationShadow::topLeftGeometry() const
{
    if (d->innerShadowRect.isNull() || d->shadow.isNull()) {
        return QRect();
    }
    return QRect(0, 0, d->innerShadowRect.left(), d->innerShadowRect.top());
}

QRect DecorationShadow::topGeometry() const
{
    if (d->innerShadowRect.isNull() || d->shadow.isNull()) {
        return QRect();
    }
    return QRect(d->innerShadowRect.left(), 0, d->innerShadowRect.width(), d->innerShadowRect.top());
}

QRect DecorationShadow::topRightGeometry() const
{
    if (d->innerShadowRect.isNull() || d->shadow.isNull()) {
        return QRect();
    }
    return QRect(d->innerShadowRect.left() + d->innerShadowRect.width(),
                 0,
                 d->shadow.width() - d->innerShadowRect.width() - d->innerShadowRect.left(),
                 d->innerShadowRect.top());
}

QRect DecorationShadow::rightGeometry() const
{
    if (d->innerShadowRect.isNull() || d->shadow.isNull()) {
        return QRect();
    }
    return QRect(d->innerShadowRect.left() + d->innerShadowRect.width(),
                 d->innerShadowRect.top(),
                 d->shadow.width() - d->innerShadowRect.width() - d->innerShadowRect.left(),
                 d->innerShadowRect.height());
}

QRect DecorationShadow::bottomRightGeometry() const
{
    if (d->innerShadowRect.isNull() || d->shadow.isNull()) {
        return QRect();
    }
    return QRect(d->innerShadowRect.left() + d->innerShadowRect.width(),
                 d->innerShadowRect.top() + d->innerShadowRect.height(),
                 d->shadow.width() - d->innerShadowRect.width() - d->innerShadowRect.left(),
                 d->shadow.height() - d->innerShadowRect.top() - d->innerShadowRect.height());
}

QRect DecorationShadow::bottomGeometry() const
{
    if (d->innerShadowRect.isNull() || d->shadow.isNull()) {
        return QRect();
    }
    return QRect(d->innerShadowRect.left(),
                 d->innerShadowRect.top() + d->innerShadowRect.height(),
                 d->innerShadowRect.width(),
                 d->shadow.height() - d->innerShadowRect.top() - d->innerShadowRect.height());
}

QRect DecorationShadow::bottomLeftGeometry() const
{
    if (d->innerShadowRect.isNull() || d->shadow.isNull()) {
        return QRect();
    }
    return QRect(0,
                 d->innerShadowRect.top() + d->innerShadowRect.height(),
                 d->innerShadowRect.left(),
                 d->shadow.height() - d->innerShadowRect.top() - d->innerShadowRect.height());
}

QRect DecorationShadow::leftGeometry() const
{
    if (d->innerShadowRect.isNull() || d->shadow.isNull()) {
        return QRect();
    }
    return QRect(0, d->innerShadowRect.top(), d->innerShadowRect.left(), d->innerShadowRect.height());
}

#ifndef K_DOXYGEN

#define DELEGATE(type, name)            \
    type DecorationShadow::name() const \
    {                                   \
        return d->name;                 \
    }

DELEGATE(QImage, shadow)
DELEGATE(QMargins, padding)
DELEGATE(QRect, innerShadowRect)

#define I(name, Name)                           \
    int DecorationShadow::padding##Name() const \
    {                                           \
        return d->padding.name();               \
    }
I(top, Top)
I(bottom, Bottom)
I(right, Right)
I(left, Left)
#undef I

#undef DELEGATE

#define SETTER(type, setName, name)          \
    void DecorationShadow::setName(type arg) \
    {                                        \
        if (d->name == arg) {                \
            return;                          \
        }                                    \
        d->name = arg;                       \
        Q_EMIT name##Changed(d->name);       \
    }

SETTER(const QImage &, setShadow, shadow)

#undef SETTER
#endif

void DecorationShadow::setPadding(const QMargins &margins)
{
    if (d->padding == margins) {
        return;
    }
    d->padding = margins;
    Q_EMIT paddingChanged();
}

void DecorationShadow::setInnerShadowRect(const QRect &rect)
{
    if (d->innerShadowRect == rect) {
        return;
    }
    d->innerShadowRect = rect;
    Q_EMIT innerShadowRectChanged();
}

}
