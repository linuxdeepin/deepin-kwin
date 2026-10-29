/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowroles.h"
#include "model/bardragmodel.h"
#include "model/scenemodel.h"
#include "model/windowmetrics.h"
#include "view/screenview.h"

#include "effects.h"
#include "kwineffects.h"
namespace KWin
{

WindowRoles::WindowRoles(qreal osScale)
    : m_osScale(osScale)
{
}

void WindowRoles::apply(ScreenView &view)
{
    const QVector<qreal> rowYs = uniqueRowYs(view.model.baseCells());
    for (int i = 0; i < view.model.windows().size(); ++i) {
        EffectWindow *w = view.model.windows().at(i);
        if (!w) {
            continue;
        }
        m_savedRadius.insert(w, w->data(WindowRadiusRole));
        m_savedScissor.insert(w, w->isScissorForce());
        w->setScissorForce(true);
        const int row = gridRowOf(view.model.baseCells().value(i).y(), rowYs);
        const qreal radius = windowCornerRadius(row, m_osScale);
        w->setData(WindowRadiusRole, QVariant::fromValue(QPointF(radius, radius)));
    }
}

void WindowRoles::restore(EffectWindow &w)
{
    auto it = m_savedRadius.find(&w);
    if (it == m_savedRadius.end()) {
        return;
    }
    w.setData(WindowRadiusRole, it.value());
    m_savedRadius.erase(it);
    w.setScissorForce(m_savedScissor.take(&w));
}

void WindowRoles::clear()
{
    m_savedRadius.clear();
    m_savedScissor.clear();
}

}
