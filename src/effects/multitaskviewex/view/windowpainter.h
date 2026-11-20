/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>

namespace KWin
{

class EffectWindow;

/**
 * @brief Canvas a control paints into.
 *
 * Every screen owns one canvas, projected onto its own viewport of the render target. A painter has
 * to know both to place a window, because KWin draws a window at the window's own screen geometry.
 */
struct WindowCanvas
{
    QMatrix4x4 projection; ///< Projection that maps the canvas onto the screen viewport.
    QPointF origin; ///< Canvas origin in screen coordinates.
};

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

    /**
     * @brief Paints a window inside a canvas.
     *
     * Unlike paintWindow(), which draws in screen coordinates, this call places the window in the
     * canvas of one screen. The painter has to combine the canvas projection with the placement of
     * the window, otherwise the window lands in the projection's coordinates while only the canvas
     * viewport covers a part of the render target.
     *
     * @param w Window to paint.
     * @param from Source geometry in canvas coordinates.
     * @param to Destination geometry in canvas coordinates.
     * @param opacity Paint opacity.
     * @param canvas Canvas the window is painted into.
     */
    virtual void paintWindowInCanvas(EffectWindow *w, const QRectF &from, const QRectF &to, qreal opacity,
                                     const WindowCanvas &canvas) const = 0;

    /**
     * @brief Returns whether a window is kept above.
     * @param w Window to query.
     * @return True when the window is kept above.
     */
    virtual bool isKeepAbove(EffectWindow *w) const = 0;
};

}
