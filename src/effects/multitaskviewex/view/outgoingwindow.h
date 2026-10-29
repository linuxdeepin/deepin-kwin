/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>

#include "animation/visualnode.h"

namespace KWin
{

class EffectWindow;

struct OutgoingWindow
{
    EffectWindow *window = nullptr;
    QRectF source;
    VisualNode interp;
};

}
