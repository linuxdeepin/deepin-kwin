/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windownavigator.h"
#include "model/scenemodel.h"
#include "view/screenview.h"

#include "kwineffects.h"
namespace KWin
{

EffectWindow *WindowNavigator::hoverWindow(const ScreenView &view)
{
    const int cell = view.model.hoverCell();
    return (cell >= 0 && cell < view.model.windows().size()) ? view.model.windows().value(cell) : nullptr;
}

EffectWindow *WindowNavigator::stepWindow(const ScreenView &view, EffectWindow *w, int step)
{
    const int idx = view.model.windows().indexOf(w);
    if (idx < 0 || view.model.windows().isEmpty()) {
        return nullptr;
    }
    const int count = view.model.windows().size();
    return view.model.windows().value(((idx + step) % count + count) % count);
}

EffectWindow *WindowNavigator::firstOrLastWindow(const ScreenView &view, bool last)
{
    return view.model.windows().isEmpty() ? nullptr : (last ? view.model.windows().last() : view.model.windows().first());
}

EffectWindow *WindowNavigator::overlayWindowAt(const ScreenView &view, int index)
{
    const auto &items = view.model.windowItems();
    if (index < 0 || index >= items.size() || items.at(index).retiring) {
        return nullptr;
    }
    return items.at(index).window;
}

EffectWindow *WindowNavigator::sameTypeWindow(const ScreenView &view, EffectWindow *w, int step)
{
    if (!w) {
        return nullptr;
    }
    const QString cls = w->windowClass();
    QVector<EffectWindow *> same;
    for (EffectWindow *c : view.model.windows()) {
        if (c && c->windowClass() == cls) {
            same.append(c);
        }
    }
    if (same.size() <= 1) {
        return w;
    }
    return same.value(((same.indexOf(w) + step) % same.size() + same.size()) % same.size());
}

}
