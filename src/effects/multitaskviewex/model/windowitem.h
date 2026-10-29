/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>

namespace KWin
{

inline constexpr qreal kButtonSize = 48.0;
inline constexpr qreal kButtonIcon = 24.0;

/**
 * @brief Returns the icon rectangle centered inside a button rectangle.
 * @param button Button rectangle.
 * @return The centered icon rectangle.
 */
inline QRectF buttonIconRect(const QRectF &button)
{
    return QRectF(button.center().x() - kButtonIcon / 2, button.center().y() - kButtonIcon / 2,
                  kButtonIcon, kButtonIcon);
}

class EffectWindow;

struct WindowItem
{
    EffectWindow *window = nullptr;
    QRectF source;
    int cellIndex = -1;
    bool retiring = false;

    /**
     * @brief Sets the window and its source rectangle.
     * @param w Window this item refers to.
     * @param src Source geometry of the window.
     */
    void setStatic(EffectWindow *w, const QRectF &src)
    {
        window = w;
        source = src;
    }
};

}
