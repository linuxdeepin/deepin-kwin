// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2016  Martin Graesslin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_ONSCREENNOTIFICATION_H
#define KWIN_ONSCREENNOTIFICATION_H

#include <QObject>

#include <KSharedConfig>

class QPropertyAnimation;
class QTimer;
class QQmlContext;
class QQmlComponent;
class QQmlEngine;

namespace KWin {

class OnScreenNotificationInputEventSpy;

class OnScreenNotification : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(QString iconName READ iconName WRITE setIconName NOTIFY iconNameChanged)
    Q_PROPERTY(int timeout READ timeout WRITE setTimeout NOTIFY timeoutChanged)

public:
    explicit OnScreenNotification(QObject *parent = nullptr);
    ~OnScreenNotification() override;
    bool isVisible() const;
    QString message() const;
    QString iconName() const;
    int timeout() const;

    QRect geometry() const;

    void setVisible(bool m_visible);
    void setMessage(const QString &message);
    void setIconName(const QString &iconName);
    void setTimeout(int timeout);

    void setConfig(KSharedConfigPtr config);
    void setEngine(QQmlEngine *engine);

    void setContainsPointer(bool contains);
    void setSkipCloseAnimation(bool skip);

Q_SIGNALS:
    void visibleChanged();
    void messageChanged();
    void iconNameChanged();
    void timeoutChanged();

private:
    void show();
    void ensureQmlContext();
    void ensureQmlComponent();
    void createInputSpy();
    bool m_visible = false;
    QString m_message;
    QString m_iconName;
    QTimer *m_timer;
    KSharedConfigPtr m_config;
    QScopedPointer<QQmlContext> m_qmlContext;
    QScopedPointer<QQmlComponent> m_qmlComponent;
    QQmlEngine *m_qmlEngine = nullptr;
    QScopedPointer<QObject> m_mainItem;
    QScopedPointer<OnScreenNotificationInputEventSpy> m_spy;
    QPropertyAnimation *m_animation = nullptr;
    bool m_containsPointer = false;
};
}

#endif // KWIN_ONSCREENNOTIFICATION_H
