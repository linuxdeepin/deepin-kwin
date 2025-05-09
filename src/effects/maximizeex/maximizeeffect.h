/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MAXIMIZEEFFCT_H
#define KWIN_MAXIMIZEEFFCT_H

#include <kwineffects.h>
#include <kwineffectsex.h>

namespace KWin
{

class MaximizeEffect : public Effect
{
    Q_OBJECT
public:
    MaximizeEffect();
    ~MaximizeEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;
    void drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;

    bool isActive() const override;

private:
    void setActive(bool);
    void cleanup();

public Q_SLOTS:
    void slotWindowMaximizedStateChanged(EffectWindow *window, bool horizontal, bool vertical);
    void slotWindowMaximizedStateAboutToChange(EffectWindow *window, bool horizontal, bool vertical);

private:
    bool                        m_activated = false;
    EffectWindow                *m_maxiWin = nullptr;
    QRectF                      m_oldGeo, m_newGeo;
    QRectF                      m_nowGeo, m_lastGeo;
    TimeLine                    m_animationTime;
    bool                        m_isMaximized = false;

    std::unique_ptr<GLFramebuffer> m_fbo;
    std::unique_ptr<GLTexture> m_texture;
};
}
#endif
