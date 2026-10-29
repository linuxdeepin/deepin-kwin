/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

namespace KWin
{

class EffectWindow;

class ExitElevationState
{
public:
    /**
     * @brief Requests a new exit elevation.
     * @param selected Window selected when leaving, if any.
     * @param selectedInView Whether the selected window is part of the view.
     * @param active Currently active window, if any.
     * @param activeInView Whether the active window is part of the view.
     */
    void request(EffectWindow *selected, bool selectedInView, EffectWindow *active, bool activeInView)
    {
        m_selected = selected;
        m_elevated = (selected && selectedInView) ? selected
                                                  : ((active && activeInView) ? active : nullptr);
    }

    /**
     * @brief Cancels the elevation without finishing it.
     */
    void interrupt()
    {
        m_elevated = nullptr;
    }

    /**
     * @brief Finishes the elevation and clears the elevated window.
     */
    void finish()
    {
        m_elevated = nullptr;
    }

    /**
     * @brief Clears all state at a session boundary.
     */
    void sessionBoundary()
    {
        m_elevated = nullptr;
        m_selected = nullptr;
    }

    /**
     * @brief Returns the currently elevated window.
     * @return The elevated window, or nullptr when none.
     */
    EffectWindow *elevated() const
    {
        return m_elevated;
    }

    /**
     * @brief Returns the window selected when leaving.
     * @return The selected window, or nullptr when none.
     */
    EffectWindow *selected() const
    {
        return m_selected;
    }

private:
    EffectWindow *m_elevated = nullptr;
    EffectWindow *m_selected = nullptr;
};

}
