/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "glcontrol.h"
#include "windowitemlayer.h"

#include <QVector>

namespace KWin
{

class ImageProvider;

class WindowOverlayControl : public GLControl
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the window overlay control.
     * @param parent Parent object.
     */
    explicit WindowOverlayControl(QObject *parent = nullptr);

    /**
     * @brief Sets the image provider used for button icons.
     * @param p Image provider.
     */
    void setImageProvider(ImageProvider *p)
    {
        m_provider = p;
    }

    /**
     * @brief Sets the window item layer used to locate buttons.
     * @param layer Window item layer.
     */
    void setWindowItemLayer(WindowItemLayer *layer)
    {
        m_layer = layer;
    }

    /**
     * @brief Attaches the scene model.
     * @param model Scene model.
     */
    void attachModel(SceneModel &model) override
    {
        m_model = &model;
    }

    /**
     * @brief Resolves which overlay button is under a point.
     * @param pos Point in screen coordinates.
     * @param topIndex Output window index for the top button, or -1.
     * @param closeIndex Output window index for the close button, or -1.
     * @return The hit button code, or 0 when none.
     */
    /**
     * @brief Returns the rect where a retiring window is painted this frame.
     * @param n Window node.
     * @return Painted rect, including the grid slide offset.
     */
    QRectF retiringRect(const WindowNode &n) const;

    int buttonAt(const QPointF &pos, int &topIndex, int &closeIndex) const;

    /**
     * @brief Tests whether a point hits an overlay button.
     * @param pos Point in screen coordinates.
     * @return True when a button is hit.
     */
    bool hitTest(const QPointF &pos) const override;

    /**
     * @brief Clears the pressed button indices.
     */
    void resetInput()
    {
        m_pressTop = m_pressClose = -1;
    }

    /**
     * @brief Tests whether there are overlay nodes to draw.
     * @return True when the layer has nodes.
     */
    bool hasItems() const
    {
        return m_layer && !m_layer->nodes().isEmpty();
    }

    /**
     * @brief Returns the top button index captured at press time.
     * @return The pressed top index, or -1 when none.
     */
    int pressTop() const
    {
        return m_pressTop;
    }

    /**
     * @brief Returns the close button index captured at press time.
     * @return The pressed close index, or -1 when none.
     */
    int pressClose() const
    {
        return m_pressClose;
    }

    /**
     * @brief Paints the window overlay buttons.
     * @param ctx Paint context.
     * @param frame Current scene frame.
     */
    void paint(GLPaintContext &ctx, const SceneFrame &frame) override;

    /**
     * @brief Handles a press on an overlay button.
     * @param pos Press position.
     */
    void press(const QPointF &pos) override;

    /**
     * @brief Handles a release, resolving button clicks.
     * @param pos Release position.
     */
    void release(const QPointF &pos) override;

    /**
     * @brief Cancels the pending press without clicking an overlay button.
     */
    void cancel() override;

Q_SIGNALS:
    /**
     * @brief Emitted when a window's top button is clicked.
     * @param index Window cell index.
     */
    void topClicked(int index);

    /**
     * @brief Emitted when a window's close button is clicked.
     * @param index Window cell index.
     */
    void closeClicked(int index);

private:
    ImageProvider *m_provider = nullptr;
    WindowItemLayer *m_layer = nullptr;
    SceneModel *m_model = nullptr;
    int m_pressTop = -1;
    int m_pressClose = -1;
};

}
