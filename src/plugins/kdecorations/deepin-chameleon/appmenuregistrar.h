// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QHash>

struct RegisteredMenu
{
    uint xid;
    QString service;
    QDBusObjectPath path;
};
using RegisteredMenus = QList<RegisteredMenu>;
Q_DECLARE_METATYPE(RegisteredMenu)

class AppMenuRegistrar : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")

public:
    explicit AppMenuRegistrar(QObject *parent);
    ~AppMenuRegistrar() override;
    void setEnabled(bool enabled);
    void removeWindow(uint xid);

public Q_SLOTS:
    void RegisterWindow(uint xid, const QDBusObjectPath &path);
    void UnregisterWindow(uint xid);
    QString GetMenuForWindow(uint xid, QDBusObjectPath &path) const;
    RegisteredMenus GetMenus() const;

Q_SIGNALS:
    void WindowRegistered(uint xid, const QString &service, const QDBusObjectPath &path);
    void WindowUnregistered(uint xid);

private:
    QDBusServiceWatcher m_exporters;
    QHash<uint, RegisteredMenu> m_menus;
    bool m_ownsName = false;
};
