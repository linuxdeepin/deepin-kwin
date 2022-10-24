// Copyright (C) 2017 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_KEYBOARD_LAYOUT_SWITCHING_H
#define KWIN_KEYBOARD_LAYOUT_SWITCHING_H

#include <QObject>
#include <QHash>

namespace KWin
{

class AbstractClient;
class KeyboardLayout;
class Xkb;
class VirtualDesktop;

namespace KeyboardLayoutSwitching
{

class Policy : public QObject
{
    Q_OBJECT
public:
    virtual ~Policy();

    virtual QString name() const = 0;

    static Policy *create(Xkb *xkb, KeyboardLayout *layout, const QString &policy);

protected:
    explicit Policy(Xkb *xkb, KeyboardLayout *layout);
    virtual void clearCache() = 0;
    virtual void layoutChanged() = 0;

    void setLayout(quint32 layout);
    quint32 layout() const;

private:
    Xkb *m_xkb;
    KeyboardLayout *m_layout;
};

class GlobalPolicy : public Policy
{
    Q_OBJECT
public:
    explicit GlobalPolicy(Xkb *xkb, KeyboardLayout *layout);
    ~GlobalPolicy() override;

    QString name() const override {
        return QStringLiteral("Global");
    }

protected:
    void clearCache() override {}
    void layoutChanged() override {}
};

class VirtualDesktopPolicy : public Policy
{
    Q_OBJECT
public:
    explicit VirtualDesktopPolicy(Xkb *xkb, KeyboardLayout *layout);
    ~VirtualDesktopPolicy() override;

    QString name() const override {
        return QStringLiteral("Desktop");
    }

protected:
    void clearCache() override;
    void layoutChanged() override;

private:
    void desktopChanged();
    QHash<VirtualDesktop *, quint32> m_layouts;
};

class WindowPolicy : public Policy
{
    Q_OBJECT
public:
    explicit WindowPolicy(Xkb *xkb, KeyboardLayout *layout);
    ~WindowPolicy() override;

    QString name() const override {
        return QStringLiteral("Window");
    }

protected:
    void clearCache() override;
    void layoutChanged() override;

private:
    QHash<AbstractClient*, quint32> m_layouts;
};

class ApplicationPolicy : public Policy
{
    Q_OBJECT
public:
    explicit ApplicationPolicy(Xkb *xkb, KeyboardLayout *layout);
    ~ApplicationPolicy() override;

    QString name() const override {
        return QStringLiteral("WinClass");
    }

protected:
    void clearCache() override;
    void layoutChanged() override;

private:
    void clientActivated(AbstractClient *c);
    QHash<AbstractClient*, quint32> m_layouts;
};

}
}

#endif
