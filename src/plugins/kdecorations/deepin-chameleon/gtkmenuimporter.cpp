// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gtkmenuimporter.h"

#include <QActionGroup>
#include <QPointer>

#include <gio/gio.h>

#include <vector>

namespace
{
QByteArray attribute(GMenuModel *model, int index, const char *name)
{
    g_autoptr(GVariant) value = g_menu_model_get_item_attribute_value(model, index, name, G_VARIANT_TYPE_STRING);
    return value ? QByteArray(g_variant_get_string(value, nullptr)) : QByteArray();
}

QString menuLabel(const QByteArray &label)
{
    const QString source = QString::fromUtf8(label);
    QString text;
    for (qsizetype i = 0; i < source.size(); ++i) {
        QChar ch = source.at(i);
        if (ch == QLatin1Char('_')) {
            if (++i == source.size()) {
                break;
            }
            ch = source.at(i);
            if (ch != QLatin1Char('_')) {
                text += QLatin1Char('&');
            }
        }
        if (ch == QLatin1Char('&')) {
            text += ch;
        }
        text += ch;
    }
    return text;
}

QKeySequence shortcut(const QString &accel)
{
    const auto keyStart = accel.lastIndexOf(QLatin1Char('>')) + 1;
    QString modifiers = accel.left(keyStart);
    QString keyName = accel.mid(keyStart);
    modifiers.replace(QLatin1String("<Primary>"), QLatin1String("Ctrl+"), Qt::CaseInsensitive);
    modifiers.replace(QLatin1String("<Control>"), QLatin1String("Ctrl+"), Qt::CaseInsensitive);
    modifiers.replace(QLatin1String("<Super>"), QLatin1String("Meta+"), Qt::CaseInsensitive);
    modifiers.remove(QLatin1Char('<')).replace(QLatin1Char('>'), QLatin1Char('+'));
    return QKeySequence::fromString(modifiers + keyName, QKeySequence::PortableText);
}
}

// A section inserts its rows directly into the enclosing QMenu. Its invisible
// end marker keeps late insertions before the following section or sibling.
class GtkMenuItems
{
public:
    GtkMenuItems(GtkMenuImporter &importer, GMenuModel *model, QMenu *menu, QAction *before, int depth, const QByteArray &prefix)
        : m_importer(importer)
        , m_model(G_MENU_MODEL(g_object_ref(model)))
        , m_menu(menu)
        , m_depth(depth)
        , m_prefix(prefix)
    {
        m_end.setVisible(false);
        menu->insertAction(before, &m_end);
        g_signal_connect(model, "items-changed", G_CALLBACK(+[](GMenuModel *, int position, int removed, int added, gpointer data) {
                             static_cast<GtkMenuItems *>(data)->itemsChanged(position, removed, added);
                         }),
                         this);
        itemsChanged(0, 0, g_menu_model_get_n_items(model));
    }

    ~GtkMenuItems()
    {
        g_signal_handlers_disconnect_by_data(m_model, this);
        g_object_unref(m_model);
        m_importer.m_itemCount -= m_rows.size();
    }

private:
    struct Row
    {
        std::unique_ptr<QAction> action;
        std::unique_ptr<QMenu> submenu;
        std::unique_ptr<GtkMenuItems> children;
    };

    void itemsChanged(int position, int removed, int added)
    {
        const int size = static_cast<int>(m_rows.size());
        if (position > size) {
            return;
        }
        const int eraseCount = std::min(removed, size - position);
        m_rows.erase(m_rows.begin() + position, m_rows.begin() + position + eraseCount);
        m_importer.m_itemCount -= eraseCount;
        // Bound the amount of application-controlled data allocated in KWin.
        for (int i = position; i < position + added && m_importer.m_itemCount < 5000; ++i) {
            QAction *before = i < int(m_rows.size()) ? m_rows[i]->action.get() : &m_end;
            ++m_importer.m_itemCount;
            m_rows.insert(m_rows.begin() + i, createRow(i, before));
        }
    }

    std::unique_ptr<Row> createRow(int index, QAction *before)
    {
        auto row = std::make_unique<Row>();
        row->action = std::make_unique<QAction>(m_menu);
        QAction *action = row->action.get();
        m_menu->insertAction(before, action);
        action->setText(menuLabel(attribute(m_model, index, G_MENU_ATTRIBUTE_LABEL)));
        auto accel = attribute(m_model, index, "accel");
        if (!accel.isEmpty()) {
            action->setShortcut(shortcut(QString::fromUtf8(accel)));
            action->setShortcutVisibleInContextMenu(true);
        }

        g_autoptr(GMenuModel) section = g_menu_model_get_item_link(m_model, index, G_MENU_LINK_SECTION);
        g_autoptr(GMenuModel) submenu = g_menu_model_get_item_link(m_model, index, G_MENU_LINK_SUBMENU);
        if (section || submenu) {
            QByteArray prefix = m_prefix;
            const QByteArray ns = attribute(m_model, index, G_MENU_ATTRIBUTE_ACTION_NAMESPACE);
            if (!ns.isEmpty()) {
                prefix += ns + '.';
            }
            if (section) {
                action->setSeparator(true);
            } else {
                row->submenu = std::make_unique<QMenu>(m_menu);
                action->setMenu(row->submenu.get());
                bindSubmenu(row->submenu.get(), index);
            }
            if (m_depth < 32) {
                row->children = std::make_unique<GtkMenuItems>(m_importer, section ? section : submenu,
                                                               section ? m_menu : row->submenu.get(),
                                                               section ? before : nullptr, m_depth + 1, prefix);
            }
        } else {
            bindAction(action, index);
        }
        return row;
    }

    void bindSubmenu(QMenu *menu, int index)
    {
        const auto [group, name] = m_importer.resolveAction(m_prefix + attribute(m_model, index, "submenu-action"));
        if (!group || name.isEmpty()) {
            return;
        }
        const auto setOpen = [group, name](bool open) {
            g_action_group_change_action_state(group, name.constData(), g_variant_new_boolean(open));
        };
        QObject::connect(menu, &QMenu::aboutToShow, menu, [setOpen] {
            setOpen(true);
        });
        QObject::connect(menu, &QMenu::aboutToHide, menu, [setOpen] {
            setOpen(false);
        });
    }

    void bindAction(QAction *action, int index)
    {
        const auto [group, name] = m_importer.resolveAction(m_prefix + attribute(m_model, index, G_MENU_ATTRIBUTE_ACTION));
        const QByteArray hiddenWhen = attribute(m_model, index, "hidden-when");
        std::shared_ptr<GVariant> target(g_menu_model_get_item_attribute_value(m_model, index, G_MENU_ATTRIBUTE_TARGET, nullptr), [](GVariant *v) {
            if (v) {
                g_variant_unref(v);
            }
        });

        const auto update = [action, group, name, hiddenWhen, target] {
            gboolean enabled = false;
            const GVariantType *type = nullptr;
            g_autoptr(GVariant) state = nullptr;
            const bool exists = group && g_action_group_query_action(group, name.constData(), &enabled, &type, nullptr, nullptr, &state);
            const bool matches = type ? target && g_variant_is_of_type(target.get(), type) : !target;
            action->setEnabled(exists && enabled && matches);
            action->setVisible(!(hiddenWhen == "action-missing" && !exists) && !(hiddenWhen == "action-disabled" && !action->isEnabled()));
            const bool radio = state && target && matches;
            const bool toggle = state && !target && g_variant_is_of_type(state, G_VARIANT_TYPE_BOOLEAN);
            if (radio && !action->actionGroup()) {
                action->setActionGroup(new QActionGroup(action));
            }
            action->setCheckable(radio || toggle);
            action->setChecked(radio ? g_variant_equal(state, target.get()) : toggle && g_variant_get_boolean(state));
        };
        QObject::connect(&m_importer.m_actionUpdates, &QTimer::timeout, action, update);
        update();
        if (group) {
            QObject::connect(action, &QAction::triggered, action, [action, group, name, target, update] {
                update();
                if (action->isEnabled()) {
                    g_action_group_activate_action(group, name.constData(), target.get());
                }
            });
        }
    }

    GtkMenuImporter &m_importer;
    GMenuModel *m_model;
    QMenu *m_menu;
    int m_depth;
    QByteArray m_prefix;
    QAction m_end;
    // Moving row pointers preserves child-before-parent destruction on erase.
    std::vector<std::unique_ptr<Row>> m_rows;
};

GtkMenuImporter::GtkMenuImporter(const GtkMenuAddress &address, QObject *parent)
    : QObject(parent)
    , m_address(address)
{
    m_actionUpdates.setSingleShot(true);
    // GIO dispatches the callback on the thread-default context of this GUI thread.
    g_bus_get(
        G_BUS_TYPE_SESSION, nullptr, [](GObject *, GAsyncResult *result, gpointer data) {
            std::unique_ptr<QPointer<GtkMenuImporter>> self(static_cast<QPointer<GtkMenuImporter> *>(data));
            g_autoptr(GDBusConnection) bus = g_bus_get_finish(result, nullptr);
            if (*self && bus) {
                (*self)->attach(bus);
            }
        },
        new QPointer<GtkMenuImporter>(this));
}

GtkMenuImporter::~GtkMenuImporter()
{
    m_items.reset();
    for (auto *group : {m_appActions, m_windowActions}) {
        if (group) {
            g_signal_handlers_disconnect_by_data(group, this);
            g_object_unref(group);
        }
    }
}

std::pair<GActionGroup *, QByteArray> GtkMenuImporter::resolveAction(const QByteArray &qualifiedName) const
{
    if (qualifiedName.startsWith("app.")) {
        return {m_appActions, qualifiedName.mid(4)};
    }
    if (qualifiedName.startsWith("win.")) {
        return {m_windowActions, qualifiedName.mid(4)};
    }
    return {};
}

void GtkMenuImporter::attach(GDBusConnection *bus)
{
    const auto service = m_address.service.toUtf8();
    const auto groupAt = [&](const QString &path) -> GActionGroup * {
        const auto objectPath = path.toUtf8();
        if (path.contains(QChar::Null) || !g_variant_is_object_path(objectPath.constData())) {
            return nullptr;
        }
        auto *group = g_dbus_action_group_get(bus, service.constData(), objectPath.constData());
        const auto changed = +[](GActionGroup *, const char *, gpointer data) {
            static_cast<GtkMenuImporter *>(data)->m_actionUpdates.start(0);
        };
        g_signal_connect(group, "action-added", G_CALLBACK(changed), this);
        g_signal_connect(group, "action-removed", G_CALLBACK(changed), this);
        g_signal_connect(group, "action-enabled-changed", G_CALLBACK(+[](GActionGroup *, const char *, gboolean, gpointer data) {
                             static_cast<GtkMenuImporter *>(data)->m_actionUpdates.start(0);
                         }),
                         this);
        g_signal_connect(group, "action-state-changed", G_CALLBACK(+[](GActionGroup *, const char *, GVariant *, gpointer data) {
                             static_cast<GtkMenuImporter *>(data)->m_actionUpdates.start(0);
                         }),
                         this);
        return G_ACTION_GROUP(group);
    };
    m_appActions = groupAt(m_address.applicationPath);
    m_windowActions = groupAt(m_address.windowPath);
    g_autoptr(GDBusMenuModel) model = g_dbus_menu_model_get(bus, service.constData(), m_address.menubarPath.toUtf8().constData());
    m_items = std::make_unique<GtkMenuItems>(*this, G_MENU_MODEL(model), &m_root, nullptr, 0, QByteArray());
}
