/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     zhangyu <zhangyu@uniontech.com>
 *
 * Maintainer: zhangyu <zhangyu@uniontech.com>
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

#ifndef EFFECTS_SPLITHANDLER_H_
#define EFFECTS_SPLITHANDLER_H_

#include <kwineffects.h>
#include <kwineffectsex.h>
#include <utils.h>

namespace KWin
{
class SplitHandlerEffect : public Effect
{
    Q_OBJECT
public:
    enum SwapClientIndex { First = 1, Second };

    SplitHandlerEffect();
    virtual ~SplitHandlerEffect();

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, int time) override;
    void paintScreen(int mask, QRegion region, ScreenPaintData &data) override;
    void postPaintScreen() override;

    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool isActive() const override;

    void swapWinHandler(EffectWindow *c);

private:
    bool isRelevantWithPresentWindows(EffectWindow *w) const;

    void resetWinPos(EffectWindow *w, QuickTileMode mode);
    int paintWinPos(EffectWindow *w, QuickTileMode mode, int initpos);

public Q_SLOTS:
    void setActive(bool active);
    void toggleActive() {
        setActive(!m_activated);
    }

private slots:
    void onSwapWindow(KWin::EffectWindow *, int index);

private:
    bool m_activated = false;
    bool m_isSwap = true;
    int m_pos = 0;
    int m_firstPos = 0;
    int m_secondPos = 0;
    int m_screen = 0;
    int m_desktop = 0;
    QRect m_workarea;

    EffectWindow *m_firstEffectWin = nullptr;
    EffectWindow *m_secondEffectWin = nullptr;
    QuickTileMode m_splitFirstMode = QuickTileMode(QuickTileFlag::None);
    QuickTileMode m_splitSecondMode = QuickTileMode(QuickTileFlag::None);

    std::chrono::milliseconds m_duration;
    TimeLine m_animationTime;
};
}
#endif
