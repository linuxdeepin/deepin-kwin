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

#include "appmenuregistrar.h"
#include "chameleon.h"
#include "chameleonconfig.h"
#include "workspace.h"
#include <QDBusConnectionInterface>

#include "main.h"
#include <KPluginFactory>

static void enableTitlebarMenu(QObject *factory)
{
    auto *workspace = KWin::Workspace::self();
    if (!workspace || KWin::kwinApp()->operationMode() != KWin::Application::OperationModeX11) {
        return;
    }
    auto *registrar = new AppMenuRegistrar(factory);

    QObject::connect(workspace, &KWin::Workspace::windowRemoved, registrar, [registrar](KWin::Window *window) {
        registrar->removeWindow(window->window());
    });
    // A separate bus owner keeps the legacy menu button's view registration independent.
    const QString connectionName = QStringLiteral("chameleon-menu-view");
    auto view = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
    auto update = [view, registrar](bool enabled) mutable {
        registrar->setEnabled(enabled);
        const QString name = QStringLiteral("org.kde.kappmenuview");
        if (enabled) {
            view.interface()->registerService(name, QDBusConnectionInterface::QueueService);
        } else {
            view.unregisterService(name);
        }
    };
    auto *config = ChameleonConfig::instance();
    QObject::connect(config, &ChameleonConfig::globalMenuEnabledChanged, registrar, update);
    update(config->globalMenuEnabled());
    QObject::connect(factory, &QObject::destroyed, [connectionName] {
        QDBusConnection::disconnectFromBus(connectionName);
    });
}

K_PLUGIN_FACTORY_WITH_JSON(
    ChameleonDecoFactory,
    "chameleon.json",
    registerPlugin<Chameleon>();
    enableTitlebarMenu(this);)

__attribute__((constructor))
static void _init_theme()
{
    // make sure atoms are initialized during the window manager startup stage
    auto global_config = ChameleonConfig::instance();
    Q_UNUSED(global_config);
}

#include "main.moc"
