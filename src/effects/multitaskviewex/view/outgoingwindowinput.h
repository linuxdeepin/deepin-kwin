/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>

namespace KWin
{

class EffectWindow;

struct OutgoingWindowInput
{
    EffectWindow *window = nullptr;
    QRectF source;
    QRectF cur;
    qreal op = 1.0;
};

}
