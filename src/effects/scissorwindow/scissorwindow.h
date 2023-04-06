// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SCISSORWINDOW_H
#define SCISSORWINDOW_H

#include <deepin_kwineffects.h>

#include <QPainterPath>

#include <map>
#include <memory>

namespace KWin { class GLTexture; }

namespace KWin {

class ScissorWindow : public Effect
{
    Q_OBJECT

    struct WindowMaskCache {
        QPainterPath maskPath;
        std::shared_ptr<GLTexture> maskTexture;
    };

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
    static bool isMaximized(EffectWindow *w, const PaintData& data);

    int requestedEffectChainPosition() const override { return 99; }

    void reconfigure(ReconfigureFlags flags) override;

    void setRoundedCornerRadius(int radius);

    void buildTextureMask();

    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, std::chrono::milliseconds time) override;

    void drawWindow(EffectWindow* w, int mask, const QRegion& region, WindowPaintData& data) override;

protected Q_SLOTS:
    void windowAdded(EffectWindow *window);
    void windowDeleted(EffectWindow *window);

private:
    enum { TopLeft = 0, TopRight, BottomRight, BottomLeft, NCorners };
    int m_radius;
    QSize m_cornerSize;

    GLTexture *m_texMask[NCorners];
    //GLTexture *m_maskTexture;
    GLShader *m_maskShader, *m_filletOptimizeShader;
    std::map<int, GLTexture*> m_texMaskMap;
    std::map<EffectWindow*, WindowMaskCache> m_clipMaskMap;
};

}

#endif
