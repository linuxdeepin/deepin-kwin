/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "glcontrol.h"
#include "model/bardragmodel.h"
#include "model/scenemodel.h"
#include "model/windowgrid.h"
#include "outgoinglayer.h"
#include "windowitemlayer.h"

#include <QVector>

namespace KWin
{

class GLScene;

class WindowCellControl : public GLControl
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the window cell control.
     * @param parent Parent object.
     */
    explicit WindowCellControl(QObject *parent = nullptr);

    /**
     * @brief Attaches the scene model and caches its base cell list.
     * @param model Scene model.
     */
    void attachModel(SceneModel &model) override
    {
        m_model = &model;
        m_cellBase = &model.cellBase();
    }

    /**
     * @brief Sets the window item layer used for painting.
     * @param layer Window item layer.
     */
    void setWindowItemLayer(const WindowItemLayer *layer)
    {
        m_windowLayer = layer;
    }

    /**
     * @brief Sets the owning scene.
     * @param s Owning scene.
     */
    void setScene(GLScene *s)
    {
        m_scene = s;
    }

    /**
     * @brief Sets the outgoing layer used for painting.
     * @param layer Outgoing layer.
     */
    void setOutgoingLayer(const OutgoingLayer *layer)
    {
        m_outgoing = layer;
    }

    /**
     * @brief Tests whether the control has a cell list attached.
     * @return True when attached.
     */
    bool hasItems() const
    {
        return m_cellBase != nullptr;
    }

    /**
     * @brief Returns the cell index at a point.
     * @param pos Point in screen coordinates.
     * @return The hit cell index, or -1 when none.
     */
    int cellAt(const QPointF &pos) const;

    /**
     * @brief Sets the clip rectangle used while painting.
     * @param r Clip rectangle.
     */
    void setClipRect(const QRectF &r)
    {
        m_clipRect = r;
    }

    /**
     * @brief Clears the input state and the pressed cell.
     */
    void resetInput()
    {
        clearInputState();
        m_pressCell = -1;
    }

    /**
     * @brief Returns the cell captured at press time.
     * @return The pressed cell index, or -1 when none.
     */
    int pressedCell() const
    {
        return m_pressCell;
    }

    /**
     * @brief Tests whether the control is pressed.
     * @return True when pressed.
     */
    bool pressed() const
    {
        return m_pressed;
    }

    /**
     * @brief Tests whether the control is dragging.
     * @return True when dragging.
     */
    bool dragging() const
    {
        return m_dragging;
    }

    /**
     * @brief Returns the number of cells.
     * @return Cell count.
     */
    int cellCountPublic() const
    {
        return cellCount();
    }

    /**
     * @brief Returns the rectangle of a cell.
     * @param i Cell index.
     * @return The cell rectangle.
     */
    QRectF cellRectAt(int i) const
    {
        return cellRect(i);
    }

    /**
     * @brief Returns the cover rectangle of a cell.
     * @param i Cell index.
     * @return The cover rectangle.
     */
    QRectF coverRectAt(int i) const
    {
        return coverRect(i);
    }

    /**
     * @brief Returns the corner radius of a cell.
     * @param i Cell index.
     * @return The corner radius.
     */
    qreal cellRadiusAt(int i) const
    {
        return cellRadius(i);
    }

    /**
     * @brief Tests whether a point hits a cell.
     * @param pos Point in screen coordinates.
     * @return True when a cell is hit.
     */
    bool hitTest(const QPointF &pos) const override
    {
        return cellAt(pos) >= 0;
    }

    /**
     * @brief Paints the window cells.
     * @param ctx Paint context.
     * @param frame Current scene frame.
     */
    void paint(GLPaintContext &ctx, const SceneFrame &frame) override;

    /**
     * @brief Handles a press on a cell.
     * @param pos Press position.
     */
    void press(const QPointF &pos) override;

    /**
     * @brief Handles a pointer move, updating selection or drag.
     * @param pos Pointer position.
     */
    void move(const QPointF &pos) override;

    /**
     * @brief Handles a release, resolving click or drop.
     * @param pos Release position.
     */
    void release(const QPointF &pos) override;

    /**
     * @brief Cancels the pending press without selecting a window.
     */
    void cancel() override;

Q_SIGNALS:
    /**
     * @brief Emitted when a cell is clicked.
     * @param index Clicked cell index.
     */
    void cellClicked(int index);

    /**
     * @brief Emitted when a cell drag starts.
     * @param index Dragged cell index.
     */
    void cellDragStarted(int index);

    /**
     * @brief Emitted while a cell drag moves.
     * @param pos Current drag position.
     */
    void cellDragMoved(const QPointF &pos);

    /**
     * @brief Emitted when a cell drag finishes.
     * @param pos Final drag position.
     */
    void cellDragFinished(const QPointF &pos);

private:
    int cellCount() const
    {
        return m_cellBase ? m_cellBase->size() : 0;
    }

    QRectF cellRect(int i) const;
    QRectF coverRect(int i) const;
    qreal cellRadius(int i) const;

    const QVector<QRectF> *m_cellBase = nullptr;
    const WindowItemLayer *m_windowLayer = nullptr;
    const OutgoingLayer *m_outgoing = nullptr;
    GLScene *m_scene = nullptr;
    SceneModel *m_model = nullptr;
    QRectF m_clipRect;
    int m_pressCell = -1;
};

}
