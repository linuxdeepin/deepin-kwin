/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>

namespace KWin
{

class EffectWindow;

class WindowPainter
{
public:
    /**
     * @brief Destroys the painter.
     */
    virtual ~WindowPainter() = default;

    /**
     * @brief Paints a window from a source geometry into a destination geometry.
     * @param w Window to paint.
     * @param from Source geometry.
     * @param to Destination geometry.
     * @param opacity Paint opacity.
     */
    virtual void paintWindow(EffectWindow *w, const QRectF &from, const QRectF &to,
                             qreal opacity) const = 0;
};

}
