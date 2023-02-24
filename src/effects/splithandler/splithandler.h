// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EFFECTS_SPLITHANDLER_H_
#define EFFECTS_SPLITHANDLER_H_

#include <deepin_kwineffects.h>
#include <deepin_kwineffectsex.h>
#include <utils/common.h>
#include <QTimeLine>

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

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;

    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;

    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool isActive() const override;

    void swapWinHandler(EffectWindow *c);

private:
    bool isRelevantWithPresentWindows(EffectWindow *w) const;

    void resetWinPos(EffectWindow *w);
    QPoint paintWinPos(EffectWindow *w);

public Q_SLOTS:
    void setActive(bool active);
    void toggleActive() {
        setActive(!m_activated);
    }

private Q_SLOTS:
    void onSwapWindow(/*KWin::EffectWindow **/QSet<KWin::EffectWindow *>list, int index, int direction);

private:
    bool m_activated = false;
    bool m_isSwap = true;

    QString m_screen;
    int m_desktop = 0;
    int m_index = 0;
    int m_direction = 0;
    QRect m_workarea;

    QSet<KWin::EffectWindow *> m_firstList;
    QSet<KWin::EffectWindow *> m_secondList;

    std::chrono::milliseconds m_lastPresentTime;
    std::chrono::milliseconds m_duration;
    TimeLine m_animationTime;
};
}
#endif
