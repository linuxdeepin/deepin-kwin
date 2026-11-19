// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QMenu>
#include <QTimer>

#include <memory>
#include <utility>

struct _GActionGroup;
struct _GDBusConnection;
class GtkMenuItems;

struct GtkMenuAddress
{
    QString service;
    QString menubarPath;
    QString applicationPath;
    QString windowPath;
};

class GtkMenuImporter : public QObject
{
public:
    GtkMenuImporter(const GtkMenuAddress &address, QObject *parent);
    ~GtkMenuImporter() override;
    QMenu *menu()
    {
        return &m_root;
    }

private:
    friend class GtkMenuItems;
    void attach(_GDBusConnection *bus);
    std::pair<_GActionGroup *, QByteArray> resolveAction(const QByteArray &qualifiedName) const;

    GtkMenuAddress m_address;
    QMenu m_root;
    _GActionGroup *m_appActions = nullptr;
    _GActionGroup *m_windowActions = nullptr;
    std::unique_ptr<GtkMenuItems> m_items;
    QTimer m_actionUpdates;
    int m_itemCount = 0;
};
