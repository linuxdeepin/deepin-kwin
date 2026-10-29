/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "barlayer.h"
#include "glcontrol.h"
#include "model/bardragmodel.h"
#include "model/baritem.h"
#include "model/scenemodel.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>

namespace KWin
{

class ImageProvider;
class WorkspaceBarMetrics;

class WorkspaceBarControl : public GLControl
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the workspace bar control.
     */
    WorkspaceBarControl();

    /**
     * @brief Destroys the workspace bar control.
     */
    ~WorkspaceBarControl() override;

    /**
     * @brief Attaches the scene model and caches its bar data pointers.
     * @param model Scene model.
     */
    void attachModel(SceneModel &model) override
    {
        m_items = &model.barItems();
        m_model = &model;
        m_dragModel = &model.barDragModel();
        m_metrics = &model.metrics();
        m_layout = &model.layout();
    }

    /**
     * @brief Sets the bar layer used for painting.
     * @param layer Bar layer.
     */
    void setBarLayer(const BarLayer *layer)
    {
        m_barLayer = layer;
    }

    /**
     * @brief Sets the image provider used to load bar thumbnails.
     * @param provider Image provider.
     */
    void setImageProvider(ImageProvider *provider)
    {
        m_provider = provider;
    }

    /**
     * @brief Tests whether the control has a bar item list attached.
     * @return True when attached.
     */
    bool hasItems() const
    {
        return m_items != nullptr;
    }

    /**
     * @brief Sets the text shown in the delete-drag tip.
     * @param text Tip text.
     */
    void setDragTipText(const QString &text)
    {
        m_dragTipText = text;
    }

    /**
     * @brief Returns the hovered slot index.
     * @return The hovered slot, or -1 when none.
     */
    int hoveredSlot() const
    {
        return m_hoveredSlot;
    }

    /**
     * @brief Tests whether the add button is hovered.
     * @return True when hovered.
     */
    bool addHovered() const
    {
        return m_addHovered;
    }

    /**
     * @brief Returns the delete-drag tip text.
     * @return The tip text.
     */
    QString dragTipText() const
    {
        return m_dragTipText;
    }

    /**
     * @brief Returns the slot captured at press time.
     * @return The pressed slot, or -1 when none.
     */
    int pressedSlot() const
    {
        return m_pressSlot;
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
     * @brief Clears the input and hover state.
     */
    void resetInput()
    {
        clearInputState();
        m_pressSlot = -1;
        m_hoveredSlot = -1;
        m_addHovered = false;
    }

    /**
     * @brief Returns the bar slot at a point.
     * @param pos Point in screen coordinates.
     * @return The slot index, or -1 when none.
     */
    int slotAt(const QPointF &pos) const;

    /**
     * @brief Returns the add button rectangle.
     * @return The add button rectangle.
     */
    QRectF buttonRect() const;

    /**
     * @brief Returns the slot whose delete button is at a point.
     * @param pos Point in screen coordinates.
     * @return The slot index, or -1 when none.
     */
    int deleteBtnAt(const QPointF &pos) const;

    /**
     * @brief Returns the size of a bar thumbnail cell.
     * @return The thumbnail cell size.
     */
    QSize thumbCellSize() const;

    /**
     * @brief Tests whether a point hits a bar item or the add button.
     * @param pos Point in screen coordinates.
     * @return True when a bar element is hit.
     */
    bool hitTest(const QPointF &pos) const override;

    /**
     * @brief Paints the workspace bar.
     * @param ctx Paint context.
     * @param frame Current scene frame.
     */
    void paint(GLPaintContext &ctx, const SceneFrame &frame) override;

    /**
     * @brief Handles a press on a bar item or the add button.
     * @param pos Press position.
     */
    void press(const QPointF &pos) override;

    /**
     * @brief Handles a pointer move, updating hover or drag.
     * @param pos Pointer position.
     */
    void move(const QPointF &pos) override;

    /**
     * @brief Handles a release, resolving click or drop.
     * @param pos Release position.
     */
    void release(const QPointF &pos) override;

Q_SIGNALS:
    /**
     * @brief Emitted when the hovered slot changes.
     * @param slot Newly hovered slot, or -1.
     */
    void slotHoverChanged(int slot);

    /**
     * @brief Emitted when a slot is clicked.
     * @param slot Clicked slot.
     */
    void slotClicked(int slot);

    /**
     * @brief Emitted when the add button is clicked.
     */
    void addClicked();

    /**
     * @brief Emitted when the add button hover state changes.
     * @param hovered New hover state.
     */
    void addHoverChanged(bool hovered);

    /**
     * @brief Emitted when a slot's delete button is clicked.
     * @param slot Slot to delete.
     */
    void deleteClicked(int slot);

    /**
     * @brief Emitted when a slot drag starts.
     * @param slot Dragged slot.
     * @param pressPos Position where the drag started.
     */
    void slotDragStarted(int slot, const QPointF &pressPos);

    /**
     * @brief Emitted while a slot drag moves.
     * @param pos Current drag position.
     */
    void slotDragMoved(const QPointF &pos);

    /**
     * @brief Emitted when a slot drag finishes.
     * @param pos Final drag position.
     */
    void slotDragFinished(const QPointF &pos);

private:
    QRectF itemRect(int row) const;
    QRectF deleteBtnRect(int row) const;
    QRectF itemHitRect(int row) const;

    const WorkspaceBarMetrics *m_metrics = nullptr;
    const ScreenLayout *m_layout = nullptr;
    ImageProvider *m_provider = nullptr;
    const QVector<BarItem> *m_items = nullptr;
    const WorkspaceBarDragModel *m_dragModel = nullptr;
    SceneModel *m_model = nullptr;
    const BarLayer *m_barLayer = nullptr;
    QVector<BarNode> m_emptyNodes;
    int m_hoveredSlot = -1;
    bool m_addHovered = false;
    QString m_dragTipText;
    int m_pressSlot = -1;
};

}
