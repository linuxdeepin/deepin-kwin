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
#include <deepin_kwinglutils.h>
#include <effects.h>

namespace SplitConsts {
    const QEasingCurve TOGGLE_MODE =  QEasingCurve::OutExpo;// AnimationMode.EASE_OUT_Expo;
    static const int FADE_DURATION = 300;
    static const qreal REBOUND_COEF = 0.85;
}

namespace KWin
{

SplitHandlerEffect::SplitHandlerEffect()
    :Effect()
    , m_lastPresentTime(std::chrono::milliseconds::zero())
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
    m_animationTime.setDuration(m_duration/*SplitConsts::FADE_DURATION*/);
    m_animationTime.setDirection(TimeLine::Forward);
    m_animationTime.setEasingCurve(SplitConsts::TOGGLE_MODE);
    m_animationTime.reset();
}

void SplitHandlerEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (isActive()) {
        std::chrono::milliseconds delta(0);
        if (m_lastPresentTime.count()) {
            delta = presentTime - m_lastPresentTime;
        }
        m_lastPresentTime = presentTime;

        m_animationTime.update(delta);
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }

    // for (auto const& w: effects->stackingOrder()) {
    //     w->setData(WindowForceBlurRole, QVariant(true));
    // }

    effects->prePaintScreen(data, presentTime);
}

void SplitHandlerEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
}

void SplitHandlerEffect::postPaintScreen()
{
    effects->postPaintScreen();
    if (m_isSwap) {
        if (m_activated && m_animationTime.done()) {
            QSet<KWin::EffectWindow *>::iterator iter;
            for (iter = m_firstList.begin(); iter != m_firstList.end(); iter++) {
                resetWinPos(*iter);
            }

            for (iter = m_secondList.begin(); iter != m_secondList.end(); iter++) {
                resetWinPos(*iter);
            }

            setActive(false);
        }
    }

    // for (auto const& w: effects->stackingOrder()) {
    //     w->setData(WindowForceBlurRole, QVariant());
    // }

    if (m_activated && m_animationTime.running()) {
        effects->addRepaintFull();
    }

    effects->postPaintScreen();
}

void SplitHandlerEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
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

    effects->prePaintWindow(w, data, presentTime);
}

void SplitHandlerEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!isActive()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (w->isDesktop()) {
        // data.setBrightness(0.8);
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (m_isSwap) {
        QPoint pos;
        if (m_firstList.contains(w)) {
            pos = paintWinPos(w);
            data += QPoint(pos.x() - w->x(), pos.y() - w->y());
        } else if (m_secondList.contains(w)) {
            pos = paintWinPos(w);
            data += QPoint(pos.x() - w->x(), pos.y() - w->y());
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
        m_lastPresentTime = std::chrono::milliseconds::zero();
        effects->setActiveFullScreenEffect(this);
    } else {
        m_animationTime.reset();
        m_firstList.clear();
        m_secondList.clear();
        if (m_index == SwapClientIndex::Second)
            effectsEx->resetSplitOutlinePos(m_screen, m_desktop);
        effects->setActiveFullScreenEffect(0);
        m_desktop = 0;
        m_screen = "";
        m_isSwap = false;
    }

    effects->addRepaintFull();
}

void SplitHandlerEffect::onSwapWindow(QSet<KWin::EffectWindow *>list, int index, int direction)
{
    if (list.size() == 0) {
        return;
    }

    QSet<KWin::EffectWindow *>::iterator iter = list.begin();

    EffectWindow *w = *iter;
    auto effectWindow = static_cast<EffectWindowImpl*>(w);
    if (nullptr == effectWindow) {
        return;
    }
    auto client = static_cast<AbstractClient*>(effectWindow->window());
    if (nullptr == client) {
        return;
    }

    m_desktop = client->desktop();
    m_screen = effectWindow->screen()->name();
    m_direction = direction;

    m_index = index;
    if (SwapClientIndex::First == index) {
        m_firstList = list;
    } else if (SwapClientIndex::Second == index) {
        m_secondList = list;
    }
    m_isSwap = true;
    // m_animationTime.reset();
    // m_lastPresentTime = std::chrono::milliseconds::zero();

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

void SplitHandlerEffect::resetWinPos(EffectWindow *w)
{
    auto effectWindow = static_cast<EffectWindowImpl*>(w);
    if (nullptr == effectWindow) {
        return;
    }
    auto client = static_cast<AbstractClient*>(effectWindow->window());
    if (nullptr == client) {
        return;
    }
    QuickTileMode mode = client->quickTileMode();

    QPoint pos(m_workarea.topLeft());
    if (mode & QuickTileFlag::Bottom) {
        pos.setY(m_workarea.bottom() - w->height());
    }
    if (mode & QuickTileFlag::Right) {
        pos.setX(m_workarea.right() - w->width());
    }
    effects->moveWindow(w, pos);
}

QPoint SplitHandlerEffect::paintWinPos(EffectWindow *w)
{
    auto effectWindow = static_cast<EffectWindowImpl*>(w);
    if (nullptr == effectWindow) {
        return QPoint();
    }
    auto client = static_cast<AbstractClient*>(effectWindow->window());
    if (nullptr == client) {
        return QPoint();
    }
    QuickTileMode mode = client->quickTileMode();

    QPoint pos(m_workarea.topLeft());
    if (mode & QuickTileFlag::Bottom) {
        pos.setY(m_workarea.bottom() - w->height());
    }
    if (mode & QuickTileFlag::Right) {
        pos.setX(m_workarea.right() - w->width());
    }

    qreal coef = m_animationTime.value();
    QPoint tmppos;
    tmppos.setX(w->x() - (w->x() - pos.x()) * coef);
    tmppos.setY(w->y() - (w->y() - pos.y()) * coef);

    return tmppos;
}

}

