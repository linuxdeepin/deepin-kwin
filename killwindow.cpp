// Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
// Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
// Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "killwindow.h"
#include "abstract_client.h"
#include "main.h"
#include "platform.h"
#include "osd.h"
#include "unmanaged.h"

#include <KLocalizedString>

namespace KWin
{

KillWindow::KillWindow()
{
}

KillWindow::~KillWindow()
{
}

void KillWindow::start()
{
    OSD::show(i18n("Select window to force close with left click or enter.\nEscape or right click to cancel."),
              QStringLiteral("window-close"));
    kwinApp()->platform()->startInteractiveWindowSelection(
        [] (KWin::Toplevel *t) {
            OSD::hide();
            if (!t) {
                return;
            }
            if (AbstractClient *c = qobject_cast<AbstractClient*>(t)) {
                c->killWindow();
            } else if (Unmanaged *u = qobject_cast<Unmanaged*>(t)) {
                xcb_kill_client(connection(), u->window());
            }
        }, QByteArrayLiteral("pirate")
    );
}

} // namespace
