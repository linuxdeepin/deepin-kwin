
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
            if (m_firstEffectWin)
                resetWinPos(m_firstEffectWin, m_splitFirstMode);
            if (m_secondEffectWin)
                resetWinPos(m_secondEffectWin, m_splitSecondMode);
            effectsEx->resetSplitOutlinePos(m_screen, m_desktop);
            setActive(false);
        }
    }

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
    }

    if (m_activated && m_animationTime.running())
        effects->addRepaintFull();
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
        if (w == m_firstEffectWin) {
            int pos = paintWinPos(m_firstEffectWin, m_splitFirstMode, m_firstPos);
            data += QPoint(pos - w->x(), m_workarea.y() - w->y());
        } else if (w == m_secondEffectWin) {
            int pos = paintWinPos(m_secondEffectWin, m_splitSecondMode, m_secondPos);
            data += QPoint(pos - w->x(), m_workarea.y() - w->y());
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
    }
    else {
        m_firstEffectWin = nullptr;
        m_secondEffectWin = nullptr;
        effects->setActiveFullScreenEffect(0);
    }

    effects->addRepaintFull();
}

void SplitHandlerEffect::onSwapWindow(EffectWindow *w, int index)
{
    auto c = static_cast<AbstractClient*>(static_cast<EffectWindowImpl*>(w)->window());
    if (c == nullptr)
        return;

    if (index == 1) {
        m_splitFirstMode = c->quickTileMode();
        m_firstEffectWin = w;
        m_screen = w->screen();
        m_desktop = w->desktop();
        m_secondEffectWin = nullptr;
        m_firstPos = w->x();
    } else if (index == 2) {
        m_splitSecondMode = c->quickTileMode();
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
    if (mode & QuickTileFlag::Left)
        effects->moveWindow(w, QPoint(m_workarea.x(), m_workarea.y()));
    else
        effects->moveWindow(w, QPoint(m_workarea.x() + m_workarea.width() - w->width(), m_workarea.y()));
}

int SplitHandlerEffect::paintWinPos(EffectWindow *w, QuickTileMode mode, int initpos)
{
    if (w == nullptr)
        return 0;

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

