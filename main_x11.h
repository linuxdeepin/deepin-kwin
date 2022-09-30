// Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_MAIN_X11_H
#define KWIN_MAIN_X11_H
#include "main.h"

namespace KWin
{

class KWinSelectionOwner
    : public KSelectionOwner
{
    Q_OBJECT
public:
    explicit KWinSelectionOwner(int screen);
protected:
    virtual bool genericReply(xcb_atom_t target, xcb_atom_t property, xcb_window_t requestor);
    virtual void replyTargets(xcb_atom_t property, xcb_window_t requestor);
    virtual void getAtoms();
private:
    xcb_atom_t make_selection_atom(int screen);
    static xcb_atom_t xa_version;
};

class ApplicationX11 : public Application
{
    Q_OBJECT
public:
    ApplicationX11(int &argc, char **argv);
    virtual ~ApplicationX11();

    void setReplace(bool replace);

protected:
    void performStartup() override;
    bool notify(QObject *o, QEvent *e) override;

private Q_SLOTS:
    void lostSelection();

private:
    void crashChecking();
    void setupCrashHandler();

    static void crashHandler(int signal);

    QScopedPointer<KWinSelectionOwner> owner;
    bool m_replace;
};

}

#endif
