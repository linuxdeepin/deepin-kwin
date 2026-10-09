/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <kwin_export.h>

namespace KWin
{

/**
 * Call displayjack to enable/disable global protection of the X11 root window via DBus.
 * @param protect  true to enable global protection, false to release it.
 */
KWIN_EXPORT void dbusRootWindowProtect(bool protect);

/**
 * Whether global protection currently open was opened by KWin itself.
 * Used when releasing global protection: only close it if there are no protected windows
 * anymore, or if the global protection was opened by KWin (so KWin is responsible for closing it).
 */
KWIN_EXPORT bool protectionOpenedByKWin();

} // namespace KWin
