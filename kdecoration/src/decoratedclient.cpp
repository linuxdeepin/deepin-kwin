/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#include "decoratedclient.h"
#include "private/decoratedclientprivate.h"
#include "private/decorationbridge.h"

#include <QColor>

namespace KDecoration2
{
DecoratedClient::DecoratedClient(Decoration *parent, DecorationBridge *bridge)
    : QObject()
    , d(bridge->createClient(this, parent))
{
}

DecoratedClient::~DecoratedClient() = default;

#define DELEGATE(type, method)           \
    type DecoratedClient::method() const \
    {                                    \
        return d->method();              \
    }

DELEGATE(bool, isActive)
DELEGATE(QString, caption)
DELEGATE(int, desktop)
DELEGATE(bool, isOnAllDesktops)
DELEGATE(bool, isShaded)
DELEGATE(QIcon, icon)
DELEGATE(bool, isMaximized)
DELEGATE(bool, isMaximizedHorizontally)
DELEGATE(bool, isMaximizedVertically)
DELEGATE(bool, isKeepAbove)
DELEGATE(bool, isKeepBelow)
DELEGATE(bool, isCloseable)
DELEGATE(bool, isMaximizeable)
DELEGATE(bool, isMinimizeable)
DELEGATE(bool, providesContextHelp)
DELEGATE(bool, isModal)
DELEGATE(bool, isShadeable)
DELEGATE(bool, isMoveable)
DELEGATE(bool, isResizeable)
DELEGATE(WId, windowId)
DELEGATE(WId, decorationId)
DELEGATE(int, width)
DELEGATE(int, height)
DELEGATE(QSize, size)
DELEGATE(QPalette, palette)
DELEGATE(Qt::Edges, adjacentScreenEdges)
DELEGATE(QString, windowClass)

#undef DELEGATE

bool DecoratedClient::hasApplicationMenu() const
{
    if (const auto *appMenuEnabledPrivate = dynamic_cast<ApplicationMenuEnabledDecoratedClientPrivate *>(d.get())) {
        return appMenuEnabledPrivate->hasApplicationMenu();
    }
    return false;
}

bool DecoratedClient::isApplicationMenuActive() const
{
    if (const auto *appMenuEnabledPrivate = dynamic_cast<ApplicationMenuEnabledDecoratedClientPrivate *>(d.get())) {
        return appMenuEnabledPrivate->isApplicationMenuActive();
    }
    return false;
}

QPointer<Decoration> DecoratedClient::decoration() const
{
    return QPointer<Decoration>(d->decoration());
}

QColor DecoratedClient::color(QPalette::ColorGroup group, QPalette::ColorRole role) const
{
    return d->palette().color(group, role);
}

QColor DecoratedClient::color(ColorGroup group, ColorRole role) const
{
    return d->color(group, role);
}

void DecoratedClient::showApplicationMenu(int actionId)
{
    if (auto *appMenuEnabledPrivate = dynamic_cast<ApplicationMenuEnabledDecoratedClientPrivate *>(d.get())) {
        appMenuEnabledPrivate->showApplicationMenu(actionId);
    }
}

} // namespace
