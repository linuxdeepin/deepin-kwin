/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace KWin
{

class EffectWindow;
struct ScreenView;

/**
 * @brief Queries over the windows shown by a screen view.
 *
 * The navigator is stateless: it reads the window list of a view and returns the
 * window a navigation step resolves to. Applying the result (hover cell, repaint
 * or desktop move) stays with the caller.
 */
class WindowNavigator
{
public:
    /**
     * @brief Returns the hovered window.
     * @param view Screen view to query.
     * @return The hovered window, or nullptr when no cell is hovered.
     */
    static EffectWindow *hoverWindow(const ScreenView &view);

    /**
     * @brief Returns the window reached by a cyclic step.
     * @param view Screen view to query.
     * @param w Window to start from.
     * @param step Number of positions to advance.
     * @return The next window, or nullptr when the view has no window or w is absent.
     */
    static EffectWindow *stepWindow(const ScreenView &view, EffectWindow *w, int step);

    /**
     * @brief Returns the first or last window of a view.
     * @param view Screen view to query.
     * @param last True for the last window, false for the first.
     * @return The selected window, or nullptr when the view has no window.
     */
    static EffectWindow *firstOrLastWindow(const ScreenView &view, bool last);

    /**
     * @brief Returns the window of a retiring-aware overlay index.
     * @param view Screen view to query.
     * @param index Overlay index.
     * @return The window, or nullptr when the index is invalid or retiring.
     */
    static EffectWindow *overlayWindowAt(const ScreenView &view, int index);

    /**
     * @brief Returns the next window of the same window class.
     * @param view Screen view to query.
     * @param w Window to start from.
     * @param step Number of positions to advance.
     * @return The matching window; w itself when it is the only one of its class.
     */
    static EffectWindow *sameTypeWindow(const ScreenView &view, EffectWindow *w, int step);
};

}
