// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "windowmenu.h"
#include "chameleontheme.h"
#include "gtkmenuimporter.h"
#include "kwinutils.h"
#include "window.h"

#include <QEvent>
#include <QtDBus>

namespace
{
const QString registrar = QStringLiteral("com.canonical.AppMenu.Registrar");
const QString registrarPath = QStringLiteral("/com/canonical/AppMenu/Registrar");
const QString dbusMenu = QStringLiteral("com.canonical.dbusmenu");

QDBusPendingCall request(const QString &service, const QString &path, const QString &interface,
                         const QString &method, const QVariantList &args)
{
    auto message = QDBusMessage::createMethodCall(service, path, interface, method);
    message.setArguments(args);
    message.setAutoStartService(false);
    return QDBusConnection::sessionBus().asyncCall(message);
}

template<typename F>
void onReply(QObject *context, const QDBusPendingCall &call, const char *signature, F callback)
{
    auto *watcher = new QDBusPendingCallWatcher(call, context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, signature, callback] {
        watcher->deleteLater();
        if (watcher->isError()) {
            qCDebug(CHAMELEON) << "Application menu request failed:" << watcher->error();
            return;
        }
        const auto reply = watcher->reply();
        if (reply.signature() != QLatin1String(signature)) {
            qCWarning(CHAMELEON) << "Unexpected application menu reply signature:" << reply.signature() << "expected" << signature;
            return;
        }
        callback(reply.arguments());
    });
}
}

WindowMenu::WindowMenu(KWin::Window *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    m_notify.setSingleShot(true);
    connect(&m_notify, &QTimer::timeout, this, &WindowMenu::changed);
    connect(window, &KWin::Window::applicationMenuChanged, this, &WindowMenu::bind);
    auto bus = QDBusConnection::sessionBus();
    for (const auto &signal : {QStringLiteral("WindowRegistered"), QStringLiteral("WindowUnregistered")}) {
        bus.connect(registrar, registrarPath, registrar, signal, this, SLOT(registrarChanged(QDBusMessage)));
    }
    bind();
}

WindowMenu::~WindowMenu()
{
    m_dbusMenu->removeEventFilter(this);
    if (m_gtkMenu) {
        m_gtkMenu->menu()->removeEventFilter(this);
    }
}

void WindowMenu::registrarChanged(const QDBusMessage &message)
{
    if (!message.arguments().isEmpty() && message.arguments().first().toUInt() == m_window->window()) {
        bind();
    }
}

void WindowMenu::bind()
{
    if (!m_service.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(m_service, m_path, dbusMenu, QStringLiteral("LayoutUpdated"), this, SLOT(layoutUpdated(uint, int)));
        m_service.clear();
    }
    m_gtkMenu.reset();
    m_dbusMenu = std::make_unique<QMenu>();
    m_dbusMenu->installEventFilter(this);
    m_notify.start(0);
    if (!m_window->applicationMenuServiceName().isEmpty() && !m_window->applicationMenuObjectPath().isEmpty()) {
        bindDBusMenu(m_window->applicationMenuServiceName(), m_window->applicationMenuObjectPath());
        return;
    }
    const auto xid = m_window->window();
    const auto property = [xid](const char *name) {
        const auto data = KWinUtils::readWindowProperty(xid, KWinUtils::internAtom(name, false), KWinUtils::internAtom("UTF8_STRING", false));
        return QString::fromUtf8(data.constData());
    };
    GtkMenuAddress gtk{property("_GTK_UNIQUE_BUS_NAME"), property("_GTK_MENUBAR_OBJECT_PATH"),
                       property("_GTK_APPLICATION_OBJECT_PATH"), property("_GTK_WINDOW_OBJECT_PATH")};
    if (!gtk.service.isEmpty() && !gtk.menubarPath.isEmpty()) {
        m_gtkMenu = std::make_unique<GtkMenuImporter>(gtk, this);
        m_gtkMenu->menu()->installEventFilter(this);
        return;
    }
    onReply(m_dbusMenu.get(), request(registrar, registrarPath, registrar, QStringLiteral("GetMenuForWindow"), {xid}), "so", [this](const QVariantList &args) {
        if (!args[0].toString().isEmpty()) {
            bindDBusMenu(args[0].toString(), args[1].value<QDBusObjectPath>().path());
        }
    });
}

void WindowMenu::bindDBusMenu(const QString &owner, const QString &path)
{
    m_service = owner;
    m_path = path;
    QDBusConnection::sessionBus().connect(owner, path, dbusMenu, QStringLiteral("LayoutUpdated"), this, SLOT(layoutUpdated(uint, int)));
    readLevel(m_dbusMenu.get(), 0);
}

void WindowMenu::layoutUpdated(uint, int parentId)
{
    if (!parentId) {
        readLevel(m_dbusMenu.get(), 0);
    }
}

void WindowMenu::popup(QMenu *menu, const QPoint &position)
{
    const QVariant id = menu->property("dbusMenuId");
    if (!id.isValid()) {
        menu->popup(position);
        return;
    }
    prepareSubmenu(menu, id.toInt(), [this, menu, position] {
        if (m_window->isActive() && !menu->isEmpty()) {
            menu->setProperty("prepared", true);
            menu->popup(position);
        }
    });
}

void WindowMenu::prepareSubmenu(QMenu *menu, int id, std::function<void()> ready)
{
    onReply(menu, request(m_service, m_path, dbusMenu, QStringLiteral("AboutToShow"), {id}), "b",
            [this, menu, id, ready](const QVariantList &) {
                readLevel(menu, id, ready);
            });
}

void WindowMenu::readLevel(QMenu *menu, int id, std::function<void()> ready)
{
    onReply(menu, request(m_service, m_path, dbusMenu, QStringLiteral("GetLayout"), {id, 1, QStringList()}), "u(ia{sv}av)",
            [this, menu, ready](const QVariantList &args) {
                renderLevel(menu, args[1].value<QDBusArgument>());
                if (ready) {
                    ready();
                }
            });
}

void WindowMenu::renderLevel(QMenu *menu, const QDBusArgument &layout)
{
    int parentId;
    QVariantMap props;
    QList<QDBusVariant> rows;
    layout.beginStructure();
    layout >> parentId >> props >> rows;
    layout.endStructure();
    // QMenu::clear() removes the actions but leaves child QMenus alive.
    qDeleteAll(menu->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly));
    menu->clear();
    for (const auto &row : rows) {
        int id;
        QVariantMap item;
        const auto node = row.variant().value<QDBusArgument>();
        if (node.currentSignature() != QLatin1String("(ia{sv}av)")) {
            continue;
        }
        node.beginStructure();
        QList<QDBusVariant> children;
        node >> id >> item >> children;
        node.endStructure();
        if (!item.value(QStringLiteral("visible"), true).toBool()) {
            continue;
        }
        if (item.value(QStringLiteral("type")).toString() == QLatin1String("separator")) {
            menu->addSeparator();
            continue;
        }
        QString label = item.value(QStringLiteral("label")).toString();
        label.replace(QLatin1Char('&'), QStringLiteral("&&")).replace(QLatin1Char('_'), QLatin1Char('&'));
        QAction *action;
        if (item.value(QStringLiteral("children-display")).toString() == QLatin1String("submenu")) {
            auto *sub = menu->addMenu(label);
            sub->setProperty("dbusMenuId", id);
            sub->addAction(QStringLiteral("…"))->setEnabled(false);
            connect(sub, &QMenu::aboutToShow, this, [this, sub, id] {
                if (sub->property("prepared").toBool()) {
                    sub->setProperty("prepared", false);
                    return;
                }
                prepareSubmenu(sub, id);
            });
            action = sub->menuAction();
        } else {
            action = menu->addAction(label);
            connect(action, &QAction::triggered, this, [this, id] {
                request(m_service, m_path, dbusMenu, QStringLiteral("Event"),
                        {id, QStringLiteral("clicked"), QVariant::fromValue(QDBusVariant(QString())), 0u});
            });
        }
        action->setEnabled(item.value(QStringLiteral("enabled"), true).toBool());
        if (item.contains(QStringLiteral("toggle-type"))) {
            action->setCheckable(true);
            action->setChecked(item.value(QStringLiteral("toggle-state")).toInt() == 1);
        }
    }
    if (menu->isVisible()) {
        menu->adjustSize();
    }
}

bool WindowMenu::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved || event->type() == QEvent::ActionChanged) {
        m_notify.start(0);
    }
    return false;
}

QList<QAction *> WindowMenu::actions() const
{
    const auto actions = (m_gtkMenu ? m_gtkMenu->menu() : m_dbusMenu.get())->actions();
    QList<QAction *> result;
    for (auto *action : actions) {
        if (action->isVisible() && !action->isSeparator() && !action->text().isEmpty()) {
            result.append(action);
        }
    }
    return result;
}
