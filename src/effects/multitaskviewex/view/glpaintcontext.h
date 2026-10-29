/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "windowpainter.h"

#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QSize>

namespace KWin
{

class GLPaintContext
{
public:
    /**
     * @brief Constructs a paint context bound to a window painter.
     * @param painter Window painter used for window drawing.
     */
    explicit GLPaintContext(const WindowPainter *painter)
        : m_painter(painter)
    {
    }

    QSize viewportSize;
    QPointF origin;

    /**
     * @brief Translates a local rectangle into global coordinates.
     * @param local Rectangle in local coordinates.
     * @return The translated rectangle.
     */
    QRectF globalRect(const QRectF &local) const
    {
        return local.translated(origin);
    }

    /**
     * @brief Builds an orthographic projection for a local rectangle.
     * @param localRect Local rectangle to project.
     */
    void setProjection(const QRectF &localRect)
    {
        m_projection.setToIdentity();
        m_projection.ortho(localRect);
    }

    /**
     * @brief Returns the current projection matrix.
     * @return The projection matrix.
     */
    const QMatrix4x4 &projection() const
    {
        return m_projection;
    }

    /**
     * @brief Paints a window through the injected window painter.
     * @param w Window to paint.
     * @param from Source geometry.
     * @param to Destination geometry.
     * @param opacity Paint opacity.
     */
    void drawEffectWindow(EffectWindow *w, const QRectF &from, const QRectF &to, qreal opacity) const
    {
        m_painter->paintWindow(w, from, to, opacity);
    }

private:
    QMatrix4x4 m_projection;
    const WindowPainter *m_painter;
};

}
