/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace KWin
{

/**
 * What the compositor knows about one candidate lock surface.
 *
 * The fields are plain observations so the policy below can be reasoned about,
 * and tested, without a compositor.
 */
struct LockSurfaceState {
    bool kwinLockScreen = false;
    bool ddeLockWindow = false;
    bool shown = false;
};

/**
 * @brief Tests whether a window class names the dde-lock greeter.
 * @param windowClass Window class of a stacking-order candidate.
 * @return True for the greeter's own window class.
 *
 * The name is compared in full: a window that merely contains it ("dde-lock-helper")
 * belongs to something else and must not lock the session.
 */
inline bool ddeLockWindowClass(const QString &windowClass)
{
    return windowClass == QLatin1String("dde-lock org.deepin.dde.lock");
}

/**
 * @brief Tests whether a window caption names the dde-lock greeter.
 * @param caption Normal (unmodified) window caption.
 * @return True for the greeter's caption.
 */
inline bool ddeLockWindowCaption(const QString &caption)
{
    return caption == QLatin1String("org.deepin.dde.lock");
}

/**
 * Whether a candidate window means "the screen is locked right now".
 *
 * dde-lock unmaps its window instead of destroying it, so a window being listed
 * in the stacking order says nothing about the session state; only a window that
 * is still shown locks the session.
 *
 * This mirrors KWin core's own stacking rule (src/workspace.cpp, isLockScreenWindow)
 * with one deliberate difference: core can ask whether a window is mapped, while the
 * effect API only offers isVisible() (not minimized, on the current desktop and
 * activity). A greeter that is minimized or pinned to another virtual desktop would
 * therefore read as "not locked" here. That cannot make the view unusable, and the
 * alternative — treating a listed window as locked forever — is what left the view
 * refusing to open after a normal unlock.
 */
inline bool lockSurfaceLocksSession(const LockSurfaceState &state)
{
    return (state.kwinLockScreen || state.ddeLockWindow) && state.shown;
}

} // namespace KWin
