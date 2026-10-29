/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "glpaintcontext.h"
#include "sceneframe.h"
#include "layers.h"

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace KWin
{

class SceneModel;

class GLControl : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a control on a scene layer.
     * @param layer Scene layer this control belongs to.
     * @param parent Parent object.
     */
    explicit GLControl(SceneLayer layer, QObject *parent = nullptr);

    /**
     * @brief Destroys the control.
     */
    ~GLControl() override;

    /**
     * @brief Returns the scene layer of this control.
     * @return The scene layer.
     */
    SceneLayer layer() const
    {
        return m_layer;
    }

    /**
     * @brief Returns the paint z ordering value of this control.
     * @return The z value derived from the scene layer.
     */
    int z() const
    {
        return layerZ(m_layer);
    }

    /**
     * @brief Returns the control geometry in screen coordinates.
     * @return The control rectangle.
     */
    QRectF geometry() const
    {
        return m_geometry;
    }

    /**
     * @brief Sets the control geometry.
     * @param g New geometry.
     */
    void setGeometry(const QRectF &g);

    /**
     * @brief Attaches the scene model the control reads from.
     * @param model Scene model.
     */
    virtual void attachModel(SceneModel &model) = 0;

    /**
     * @brief Tests whether a point hits this control.
     * @param pos Point in screen coordinates.
     * @return True when the point lies inside the geometry.
     */
    virtual bool hitTest(const QPointF &pos) const
    {
        return m_geometry.contains(pos);
    }

    /**
     * @brief Paints the control.
     * @param ctx Paint context.
     * @param frame Current scene frame.
     */
    virtual void paint(GLPaintContext &ctx, const SceneFrame &frame) = 0;

    /**
     * @brief Handles a press.
     * @param pos Press position.
     */
    virtual void press(const QPointF &pos)
    {
        Q_UNUSED(pos)
    }

    /**
     * @brief Handles a pointer move.
     * @param pos Pointer position.
     */
    virtual void move(const QPointF &pos)
    {
        Q_UNUSED(pos)
    }

    /**
     * @brief Handles a release.
     * @param pos Release position.
     */
    virtual void release(const QPointF &pos)
    {
        Q_UNUSED(pos)
    }

protected:
    bool m_pressed = false;
    bool m_dragging = false;
    QPointF m_pressPos;
    static constexpr qreal kDragThreshold = 6.0;
    bool dragExceeded(const QPointF &pos) const
    {
        return m_pressed && !m_dragging && (pos - m_pressPos).manhattanLength() > kDragThreshold;
    }

    void clearInputState()
    {
        m_pressed = false;
        m_dragging = false;
    }

    QRectF m_geometry;
    SceneLayer m_layer = SceneLayer::Background;
};

}
