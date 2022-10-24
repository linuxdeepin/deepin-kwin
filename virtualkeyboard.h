// Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_VIRTUAL_KEYBOARD_H
#define KWIN_VIRTUAL_KEYBOARD_H

#include <QObject>

#include <kwinglobals.h>
#include <kwin_export.h>

class QQuickView;
class QWindow;
class KStatusNotifierItem;

namespace KWin
{

class KWIN_EXPORT VirtualKeyboard : public QObject
{
    Q_OBJECT
public:
    virtual ~VirtualKeyboard();

    void init();

    bool event(QEvent *e) override;
    bool eventFilter(QObject *o, QEvent *event) override;

    QWindow *inputPanel() const;

Q_SIGNALS:
    void enabledChanged(bool enabled);

private:
    void show();
    void hide();
    void setEnabled(bool enable);
    void updateSni();

    bool m_enabled = false;
    KStatusNotifierItem *m_sni = nullptr;
    QScopedPointer<QQuickView> m_inputWindow;
    QMetaObject::Connection m_waylandShowConnection;
    QMetaObject::Connection m_waylandHideConnection;
    QMetaObject::Connection m_waylandHintsConnection;
    QMetaObject::Connection m_waylandSurroundingTextConnection;
    QMetaObject::Connection m_waylandResetConnection;
    QMetaObject::Connection m_waylandEnabledConnection;
    KWIN_SINGLETON(VirtualKeyboard)
};

}

#endif
