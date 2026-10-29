/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dragghostcontrol.h"

namespace KWin
{

DragGhostControl::DragGhostControl(QObject *parent)
    : GLControl(SceneLayer::Ghost, parent)
{
}

void DragGhostControl::paint(GLPaintContext &ctx, const SceneFrame &frame)
{
    Q_UNUSED(frame)
    if (!m_layer || !m_layer->active() || m_layer->rect().isEmpty()) {
        return;
    }
    ctx.drawEffectWindow(m_layer->window(), m_layer->sourceGeometry(), m_layer->rect(), m_layer->opacity());
}

}
