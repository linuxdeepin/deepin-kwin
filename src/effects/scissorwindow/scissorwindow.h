/*
 * Copyright (C) 2020 ~ 2025 Uniontech Technology Co., Ltd.
 *
 * Author:     zjq <zhaojunqing@uniontech.com>
 *
 * Maintainer: zjq <zhaojunqing@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCISSORWINDOW_H
#define SCISSORWINDOW_H

#include <deepin_kwineffects.h>

namespace KWin { class GLTexture; }

namespace KWin {

class ScissorWindow : public Effect
{
    Q_OBJECT

public:
    ScissorWindow();
    ~ScissorWindow() override;

    enum EffectDataRole {
        BaseRole = KWin::DataRole::LanczosCacheRole + 100,
        WindowRadiusRole = BaseRole + 1,
        WindowClipPathRole = BaseRole + 2,
        WindowMaskTextureRole = BaseRole + 3,
        ShadowMaskRole = KWin::DataRole::LanczosCacheRole + 201,
        ShadowOffsetRole
    };

    static bool supported();
    static bool enabledByDefault();
    static bool isMaximized(EffectWindow *w);
    
    int requestedEffectChainPosition() const override { return 99; }

    void reconfigure(ReconfigureFlags flags) override;

    void setRoundedCornerRadius(int radius);

    void buildTextureMask();

    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, std::chrono::milliseconds time) override;

    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;

protected Q_SLOTS:
    void windowAdded(EffectWindow *window);

private:
    enum { TopLeft = 0, TopRight, BottomRight, BottomLeft, NCorners };
    int m_radius;
    QSize m_cornerSize;

    GLTexture *m_texMask[NCorners];
    GLShader *m_shader, *m_shader1, *m_shader2;
};

}

#endif
