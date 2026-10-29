/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ghostlayer.h"
#include "glcontrol.h"

namespace KWin
{

class DragGhostControl : public GLControl
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the drag ghost control.
     * @param parent Parent object.
     */
    explicit DragGhostControl(QObject *parent = nullptr);

    /**
     * @brief The drag ghost has no model-backed state.
     * @param model Ignored.
     */
    void attachModel(SceneModel &model) override
    {
        Q_UNUSED(model)
    }

    /**
     * @brief Sets the ghost layer this control renders.
     * @param layer Ghost layer.
     */
    void setGhostLayer(GhostLayer *layer)
    {
        m_layer = layer;
    }

    /**
     * @brief Paints the drag ghost.
     * @param ctx Paint context.
     * @param frame Current scene frame.
     */
    void paint(GLPaintContext &ctx, const SceneFrame &frame) override;

private:
    GhostLayer *m_layer = nullptr;
};

}
