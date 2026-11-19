// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <QDBusObjectPath>
#include <QMenu>
#include <QTimer>
#include <functional>
#include <memory>

namespace KWin
{
class Window;
}
class QDBusArgument;
class QDBusMessage;
class GtkMenuImporter;
class WindowMenu : public QObject
{
    Q_OBJECT
public:
    explicit WindowMenu(KWin::Window *window, QObject *parent);
    ~WindowMenu() override;
    QList<QAction *> actions() const;
    void popup(QMenu *menu, const QPoint &position);
Q_SIGNALS:
    void changed();
private Q_SLOTS:
    void bind();
    void registrarChanged(const QDBusMessage &message);
    void layoutUpdated(uint revision, int parentId);

private:
    bool eventFilter(QObject *object, QEvent *event) override;
    void bindDBusMenu(const QString &owner, const QString &path);
    void prepareSubmenu(QMenu *menu, int id, std::function<void()> ready = {});
    void readLevel(QMenu *menu, int id, std::function<void()> ready = {});
    void renderLevel(QMenu *menu, const QDBusArgument &layout);
    KWin::Window *m_window;
    QString m_service;
    QString m_path;
    std::unique_ptr<QMenu> m_dbusMenu;
    std::unique_ptr<GtkMenuImporter> m_gtkMenu;
    QTimer m_notify;
};
