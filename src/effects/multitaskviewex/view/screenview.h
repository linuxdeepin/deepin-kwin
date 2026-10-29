/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "animation/frameclock.h"
#include "glscene.h"

namespace KWin
{

class EffectScreen;
class EffectWindow;

struct ScreenView
{
    SceneModel model;
    std::unique_ptr<GLScene> scene;
    QString acquiredBgUri;
    QSize acquiredBgSize;
    FrameDeltaClock frameClock;

    /**
     * @brief Constructs a view for a screen and binds its scene to the model.
     * @param s Screen this view belongs to.
     */
    explicit ScreenView(EffectScreen *s)
        : scene(std::make_unique<GLScene>())
    {
        model.setScreen(s);
        scene->setModel(&model);
    }
};

}
