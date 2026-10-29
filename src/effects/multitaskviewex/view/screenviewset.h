/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "screenview.h"

#include <memory>
#include <vector>

namespace KWin
{

class ScreenViewSet
{
public:
    /**
     * @brief Returns the view for a screen, creating it if necessary.
     * @param screen Screen to find or create a view for.
     * @return The view, or nullptr when the screen is null.
     */
    ScreenView *ensure(EffectScreen *screen)
    {
        if (!screen) {
            return nullptr;
        }
        for (const std::unique_ptr<ScreenView> &v : m_views) {
            if (v->model.screen() == screen) {
                return v.get();
            }
        }
        m_views.push_back(std::make_unique<ScreenView>(screen));
        return m_views.back().get();
    }

    /**
     * @brief Removes the view of a screen.
     * @param screen Screen whose view to remove.
     */
    void remove(EffectScreen *screen)
    {
        for (auto it = m_views.begin(); it != m_views.end(); ++it) {
            if ((*it)->model.screen() == screen) {
                m_views.erase(it);
                return;
            }
        }
    }

    /**
     * @brief Finds the view of a screen without creating one.
     * @param screen Screen to look up.
     * @return The view, or nullptr when absent or the screen is null.
     */
    ScreenView *find(EffectScreen *screen) const
    {
        if (!screen) {
            return nullptr;
        }
        for (const std::unique_ptr<ScreenView> &v : m_views) {
            if (v->model.screen() == screen) {
                return v.get();
            }
        }
        return nullptr;
    }

    /**
     * @brief Returns the view at an index.
     * @param index View index.
     * @return The view, or nullptr when out of range.
     */
    ScreenView *at(int index) const
    {
        return index >= 0 && index < count() ? m_views[size_t(index)].get() : nullptr;
    }

    /**
     * @brief Tests whether the set holds no views.
     * @return True when empty.
     */
    bool isEmpty() const
    {
        return m_views.empty();
    }

    /**
     * @brief Returns the number of views.
     * @return View count.
     */
    int count() const
    {
        return int(m_views.size());
    }

    /**
     * @brief Returns all views in insertion order.
     * @return The view list.
     */
    const std::vector<std::unique_ptr<ScreenView>> &all() const
    {
        return m_views;
    }

private:
    std::vector<std::unique_ptr<ScreenView>> m_views;
};

}
