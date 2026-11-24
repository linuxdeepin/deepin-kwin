/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_logging.h"

#include "mirrormode_logging.h"

Q_LOGGING_CATEGORY(KWIN_X11STANDALONE, "kwin_platform_x11_standalone", QtWarningMsg)
// libkwin is built with hidden visibility, so the definition it carries is not importable
// from here. The backend runs in the executable and keeps its own copy, like KWIN_CORE in
// main_x11.cpp; the rules match on the name, so both copies behave the same.
Q_LOGGING_CATEGORY(KWIN_MIRRORMODE, "kwin_mirrormode", QtWarningMsg)
