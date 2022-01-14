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

#include "splithandler.h"
#include <abstract_client.h>
#include <kwinglutils.h>
#include <effects.h>

namespace SplitConsts {
    const QEasingCurve TOGGLE_MODE =  QEasingCurve::OutExpo;// AnimationMode.EASE_OUT_Expo;
    static const int FADE_DURATION = 600;
    static const qreal REBOUND_COEF = 0.85;
}

namespace KWin
{

SplitHandlerEffect::SplitHandlerEffect()
    :Effect()
{
    reconfigure(ReconfigureAll);
    connect(effectsEx, &EffectsHandlerEx::swapSplitWin, this, &SplitHandlerEffect::onSwapWindow);
}
SplitHandlerEffect::~SplitHandlerEffect()
{

}

void SplitHandlerEffect::reconfigure(ReconfigureFlags flags)
{
    m_duration = std::chrono::milliseconds(static_cast<int>(animationTime(SplitConsts::FADE_DURATION)));
    m_animationTime.setDuration(m_duration);
    m_animationTime.setDirection(TimeLine::Forward);
    m_animationTime.setEasingCurve(SplitConsts::TOGGLE_MODE);
}

void SplitHandlerEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    if (isActive()) {
        std::chrono::milliseconds delta(time);
        m_animationTime.update(delta);
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant(true));
    }

    effects->prePaintScreen(data, time);
}

void SplitHandlerEffect::paintScreen(int mask, QRegion region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
}

void SplitHandlerEffect::postPaintScreen()
{
    effects->postPaintScreen();

    if (m_isSwap) {
        if (m_activated && m_animationTime.done()) {
            if (m_firstEffectWin) {
                resetWinPos(m_firstEffectWin, m_splitFirstMode);
            }
            if (m_secondEffectWin) {
                resetWinPos(m_secondEffectWin, m_splitSecondMode);
            }
            effectsEx->resetSplitOutlinePos(m_screen, m_desktop);
            setActive(false);
        }
    }

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
    }

    if (m_activated && m_animationTime.running()) {
        effects->addRepaintFull();
    }
}

void SplitHandlerEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    data.setTransformed();

    if (m_activated) {
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);   // Display always
    }

    w->enablePainting(EffectWindow::PAINT_DISABLED);
    if (!(w->isDock() || w->isDesktop() || isRelevantWithPresentWindows(w)) || w->isMinimized()) {
        w->disablePainting(EffectWindow::PAINT_DISABLED);
        w->disablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    }

    effects->prePaintWindow(w, data, time);
}

void SplitHandlerEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!isActive()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (w->isDesktop()) {
        data.setBrightness(0.8);
    }

    if (m_isSwap) {
        int xPos = 0;
        if (w == m_firstEffectWin) {
            xPos = paintWinPos(m_firstEffectWin, m_splitFirstMode, m_firstPos);
            data += QPoint(xPos - w->x(), m_workarea.y() - w->y());
        } else if (w == m_secondEffectWin) {
            xPos = paintWinPos(m_secondEffectWin, m_splitSecondMode, m_secondPos);
            data += QPoint(xPos - w->x(), m_workarea.y() - w->y());
        }
    }

    effects->paintWindow(w, mask, region, data);
}

bool SplitHandlerEffect::isActive() const
{
    return m_activated;
}

void SplitHandlerEffect::setActive(bool active)
{
    if (m_activated == active)
        return;

    m_activated = active;

    if (active) {
        m_animationTime.reset();
        effects->setActiveFullScreenEffect(this);
    } else {
        m_firstEffectWin = nullptr;
        m_secondEffectWin = nullptr;
        effects->setActiveFullScreenEffect(0);
    }

    effects->addRepaintFull();
}

void SplitHandlerEffect::onSwapWindow(EffectWindow *w, int index)
{
    auto effectWindow = static_cast<EffectWindowImpl*>(w);
    if (nullptr == effectWindow) {
        return;
    }

    auto client = static_cast<AbstractClient*>(effectWindow->window());
    if (nullptr == client) {
        return;
    }

    if (SwapClientIndex::First == index) {
        m_splitFirstMode = client->quickTileMode();
        m_firstEffectWin = w;
        m_screen = w->screen();
        m_desktop = w->desktop();
        m_secondEffectWin = nullptr;
        m_firstPos = w->x();
    } else if (SwapClientIndex::Second == index) {
        m_splitSecondMode = client->quickTileMode();
        m_secondEffectWin = w;
        m_secondPos = w->x();
    }
    m_isSwap = true;

    m_workarea = effects->clientArea(MaximizeArea, w->screen(), w->desktop());
    setActive(true);
}

bool SplitHandlerEffect::isRelevantWithPresentWindows(EffectWindow *w) const
{
    if (w->isSpecialWindow() || w->isUtility()) {
        return false;
    }

    if (w->isDock()) {
        return false;
    }

    if (w->isSkipSwitcher()) {
        return false;
    }

    if (w->isDeleted()) {
        return false;
    }

    if (!w->acceptsFocus()) {
        return false;
    }

    if (!w->isCurrentTab()) {
        return false;
    }

    if (!w->isOnCurrentActivity()) {
        return false;
    }

    return true;
}

void SplitHandlerEffect::resetWinPos(EffectWindow *w, QuickTileMode mode)
{
    if (mode & QuickTileFlag::Left) {
        effects->moveWindow(w, QPoint(m_workarea.x(), m_workarea.y()));
    } else {
        effects->moveWindow(w, QPoint(m_workarea.x() + m_workarea.width() - w->width(), m_workarea.y()));
    }
}

int SplitHandlerEffect::paintWinPos(EffectWindow *w, QuickTileMode mode, int initpos)
{
    if (nullptr == w) {
        return 0;
    }

    qreal coef = m_animationTime.value();
    int tpos = 0;
    if (mode & QuickTileFlag::Left) {
        tpos = initpos - (initpos - m_workarea.x()) * coef;
    } else {
        int epos = m_workarea.x() + m_workarea.width() - w->width() - initpos;
        tpos = initpos + epos * coef;
    }

    return  tpos;
}

}

