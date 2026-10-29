/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "glcontrol.h"

namespace KWin
{

GLControl::GLControl(SceneLayer layer, QObject *parent)
    : QObject(parent)
    , m_layer(layer)
{
}

GLControl::~GLControl() = default;

void GLControl::setGeometry(const QRectF &g)
{
    m_geometry = g;
}

}
