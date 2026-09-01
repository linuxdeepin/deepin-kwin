/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/backgroundeffectitem.h"

namespace KWin
{

BackgroundEffectItem::BackgroundEffectItem(Scene *scene, Item *parent)
    : Item(scene, parent)
{
    setZ(-1);
}

BackgroundEffectItem::~BackgroundEffectItem() = default;

void BackgroundEffectItem::setRegion(const QRegion &region)
{
    m_region = region;
    updateGeometry();
}

QRegion BackgroundEffectItem::region() const
{
    return m_region;
}

void BackgroundEffectItem::setExpandSize(int expandSize)
{
    m_expandSize = expandSize;
    updateGeometry();
}

int BackgroundEffectItem::expandSize() const
{
    return m_expandSize;
}

QRegion BackgroundEffectItem::expandedRegion() const
{
    QRegion expanded;
    if (m_region.isEmpty()) {
        return expanded;
    }
    for (const QRect &rect : m_region) {
        expanded |= rect.adjusted(-m_expandSize, -m_expandSize, m_expandSize, m_expandSize);
    }
    // Return coordinates relative to this item's position so that
    // mapToGlobal(expandedRegion()) does not double-count the position
    // offset set in updateGeometry().
    return expanded.translated(-position().toPoint());
}

void BackgroundEffectItem::updateGeometry()
{
    if (m_region.isEmpty()) {
        setPosition(QPointF(0, 0));
        setSize(QSizeF(0, 0));
        return;
    }

    QRegion expanded;
    for (const QRect &rect : m_region) {
        expanded |= rect.adjusted(-m_expandSize, -m_expandSize, m_expandSize, m_expandSize);
    }

    const QRect boundingRect = expanded.boundingRect();
    setPosition(boundingRect.topLeft());
    setSize(boundingRect.size());
}

} // namespace KWin
