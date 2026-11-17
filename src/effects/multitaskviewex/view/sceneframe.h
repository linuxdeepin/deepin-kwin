/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace KWin
{

struct SceneFrame
{
    qreal enterProgress = 1.0;
    qreal barOpacity = 1.0;
    qreal barShiftY = 0.0;
    bool bgSliding = false;
    qreal bgSlideProgress = 1.0;
    QString bgSlideFromUri;
    QString bgSlideToUri;
    qreal bgSlideDir = 1.0;
};

}
