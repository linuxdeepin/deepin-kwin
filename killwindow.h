// Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
// Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
// Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_KILLWINDOW_H
#define KWIN_KILLWINDOW_H

namespace KWin
{

class KillWindow
{
public:

    KillWindow();
    ~KillWindow();

    void start();
};

} // namespace

#endif
