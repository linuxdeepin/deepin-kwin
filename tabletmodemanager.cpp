// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2018 Marco Martin <mart@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tabletmodemanager.h"
#include "input.h"
#include "input_event.h"
#include "input_event_spy.h"

#include "libinput/device.h"

#include <QDBusConnection>

using namespace KWin;

KWIN_SINGLETON_FACTORY_VARIABLE(TabletModeManager, s_manager)

class KWin::TabletModeInputEventSpy : public InputEventSpy
{
public:
    explicit TabletModeInputEventSpy(TabletModeManager *parent);

    void switchEvent(SwitchEvent *event) override;
private:
    TabletModeManager *m_parent;
};

TabletModeInputEventSpy::TabletModeInputEventSpy(TabletModeManager *parent)
    : m_parent(parent)
{
}

void TabletModeInputEventSpy::switchEvent(SwitchEvent *event)
{
    if (!event->device()->isTabletModeSwitch()) {
        return;
    }

    switch (event->state()) {
    case SwitchEvent::State::Off:
        m_parent->setIsTablet(false);
        break;
    case SwitchEvent::State::On:
        m_parent->setIsTablet(true);
        break;
    default:
        Q_UNREACHABLE();
    }
}



TabletModeManager::TabletModeManager(QObject *parent)
    : QObject(parent),
      m_spy(new TabletModeInputEventSpy(this))
{
    input()->installInputEventSpy(m_spy);

    QDBusConnection::sessionBus().registerObject(QStringLiteral("/org/kde/KWin"),
                                                 QStringLiteral("org.kde.KWin.TabletModeManager"),
                                                 this,
                                                 QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals
    );

    connect(input(), &InputRedirection::hasTabletModeSwitchChanged, this, &TabletModeManager::tabletModeAvailableChanged);
}

bool TabletModeManager::isTabletModeAvailable() const
{
    return input()->hasTabletModeSwitch();
}

bool TabletModeManager::isTablet() const
{
    return m_isTabletMode;
}

void TabletModeManager::setIsTablet(bool tablet)
{
    if (m_isTabletMode == tablet) {
        return;
    }

    m_isTabletMode = tablet;
    emit tabletModeChanged(tablet);
}
