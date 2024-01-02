/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "switcheritem.h"
// KWin
#include "composite.h"
#include "core/output.h"
#include "tabboxhandler.h"
#include "tabbox.h"
#include "workspace.h"
#include "window.h"
#include "core/renderbackend.h"
// Qt
#include <QAbstractItemModel>
#include <QDBusMessage>
#include <QDBusConnection>

namespace KWin
{
namespace TabBox
{

SwitcherItem::SwitcherItem(QObject *parent)
    : QObject(parent)
    , m_model(nullptr)
    , m_item(nullptr)
    , m_visible(false)
    , m_allDesktops(false)
    , m_currentIndex(0)
{
    m_selectedIndexConnection = connect(tabBox, &TabBoxHandler::selectedIndexChanged, this, [this] {
        if (isVisible()) {
            setCurrentIndex(tabBox->currentIndex().row());
        }
    });
    connect(workspace(), &Workspace::outputsChanged, this, &SwitcherItem::screenGeometryChanged);
    connect(Compositor::self(), &Compositor::compositingToggled, this, &SwitcherItem::compositingChanged);
    connect(Compositor::self(), &Compositor::compositingToggled, this, &SwitcherItem::updateWindowColor);
    updateWindowColor(true);
}

SwitcherItem::~SwitcherItem()
{
    disconnect(m_selectedIndexConnection);
}

void SwitcherItem::setItem(QObject *item)
{
    if (m_item == item) {
        return;
    }
    m_item = item;
    Q_EMIT itemChanged();
}

QString SwitcherItem::windowColor() const
{
    return m_windowColor;
}

void SwitcherItem::setWindowColor(QString windowColor)
{
    m_windowColor = windowColor;
    Q_EMIT windowColorChanged(m_windowColor);
}

void SwitcherItem::setModel(QAbstractItemModel *model)
{
    m_model = model;
    Q_EMIT modelChanged();
}

void SwitcherItem::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    if (visible) {
        Q_EMIT screenGeometryChanged();
    }
    m_visible = visible;
    Q_EMIT visibleChanged();
}

QRect SwitcherItem::screenGeometry() const
{
    return workspace()->activeOutput()->geometry();
}

void SwitcherItem::setCurrentIndex(int index)
{
    if (m_currentIndex == index) {
        return;
    }
    m_currentIndex = index;
    if (m_model) {
        tabBox->setCurrentIndex(m_model->index(index, 0));
    }

    if (!Compositor::compositing()) {
        if (index != tabBox->clientList().count() - 1) {
            for (int i = 0; i < tabBox->clientList().count() - 1; i++) {
                Window *c = workspace()->tabbox()->currentClientList().at(i);
                QList<bool> minisizeClientList = workspace()->tabbox()->getAllClientIsMinisize();
                if (i != index && i < minisizeClientList.count() && tabBox->clientList().count() == minisizeClientList.count()) {
                    c->setMinimized(minisizeClientList.at(i));
                }
            }
        }
    }

    Q_EMIT currentIndexChanged(m_currentIndex);

    TabBox *tabBox = workspace()->tabbox();
    if (tabBox && tabBox->isDisplayed() && tabBox->currentClient() != nullptr) {
        Window *c = tabBox->currentClient();
        QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "ShowingDesktopStateChanged");
        if (c->isDesktop()) {
            message << bool(true);
        } else {
            message << bool(false);
        }
        QDBusConnection::sessionBus().send(message);
    }
}

void SwitcherItem::setAllDesktops(bool all)
{
    if (m_allDesktops == all) {
        return;
    }
    m_allDesktops = all;
    Q_EMIT allDesktopsChanged();
}

void SwitcherItem::setNoModifierGrab(bool set)
{
    if (m_noModifierGrab == set) {
        return;
    }
    m_noModifierGrab = set;
    Q_EMIT noModifierGrabChanged();
}

bool SwitcherItem::compositing()
{
    return Compositor::compositing();
}

void SwitcherItem::updateWindowColor(bool active)
{
    if (active) {
        if (Compositor::self() && Compositor::self()->backend() && Compositor::self()->backend()->compositingType() == XRenderCompositing) {
            setWindowColor(QString("#ffffffff"));
        } else {
            setWindowColor(QString("transparent"));
        }
    }
}

}
}
