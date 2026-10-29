/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "data/workspacemodel.h"
#include "multitaskviewex.h"
#include "windownavigator.h"
#include "workspacemanager.h"

#include "effects.h"
#include "window.h"

#include <QVector>

namespace KWin
{

EffectWindow *MultitaskViewExEffect::hoverWindow(ScreenView &view) const
{
    return WindowNavigator::hoverWindow(view);
}

void MultitaskViewExEffect::setHoverWindow(ScreenView &view, EffectWindow *w)
{
    view.model.setHoverCell(view.model.windows().indexOf(w));
    effects->addRepaintFull();
}

EffectWindow *MultitaskViewExEffect::stepWindow(ScreenView &view, EffectWindow *w, int step) const
{
    return WindowNavigator::stepWindow(view, w, step);
}

EffectWindow *MultitaskViewExEffect::firstOrLastWindow(ScreenView &view, bool last) const
{
    return WindowNavigator::firstOrLastWindow(view, last);
}

EffectWindow *MultitaskViewExEffect::overlayWindowAt(ScreenView &view, int index) const
{
    return WindowNavigator::overlayWindowAt(view, index);
}

EffectWindow *MultitaskViewExEffect::sameTypeWindow(ScreenView &view, EffectWindow *w, int step) const
{
    return WindowNavigator::sameTypeWindow(view, w, step);
}

bool MultitaskViewExEffect::cycleHoverWindow(ScreenView &view, int step)
{
    EffectWindow *cur = hoverWindow(view);
    if (!cur) {
        return false;
    }
    setHoverWindow(view, stepWindow(view, cur, step));
    return true;
}

void MultitaskViewExEffect::moveHoverToDesktopIfAvailable(ScreenView &view, int num)
{
    if (hoverWindow(view) && num > 0 && num <= workspaces()->count()
        && num != workspaces()->currentNumber()) {
        moveHoverWindowToDesktop(num);
    }
}

void MultitaskViewExEffect::moveHoverWindowToDesktop(int num)
{
    ScreenView *view = viewAtGlobal(effects->cursorPos());
    if (!view) {
        return;
    }
    EffectWindow *w = hoverWindow(*view);
    if (!w || w->isOnAllDesktops() || num <= 0 || num > workspaces()->count()
        || num == workspaces()->currentNumber()) {
        return;
    }
    if (auto *impl = static_cast<EffectWindowImpl *>(w)) {
        if (Window *win = impl->window()) {
            win->setDesktop(num);
        }
    }
    setHoverWindow(*view, nullptr);
    syncSessionItems();
    effects->addRepaintFull();
}

}
