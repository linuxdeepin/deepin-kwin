/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene/item.h"

namespace KWin
{

/**
 * The BackgroundEffectItem class represents an area where a background effect
 * (e.g. blur) is active. It does not paint anything itself; its purpose is to
 * track the semantic region and sampling expansion radius so that the scene
 * can correctly extend damage to cover the full sampling area when partial
 * damage intersects it.
 *
 * The item is positioned and sized to cover the expanded region, establishing
 * proper Item geometry so that setPosition()/setSize() and removeChild()
 * automatically schedule repaints for old and new areas.
 */
class KWIN_EXPORT BackgroundEffectItem : public Item
{
    Q_OBJECT

public:
    explicit BackgroundEffectItem(Scene *scene, Item *parent = nullptr);
    ~BackgroundEffectItem() override;

    void setRegion(const QRegion &region);
    QRegion region() const;

    void setExpandSize(int expandSize);
    int expandSize() const;

    /**
     * Returns the blur region expanded by the sampling radius, in coordinates
     * relative to this item's position. This allows mapToGlobal() to produce
     * the correct global expanded region without double-counting the position
     * offset that was set via setPosition().
     */
    QRegion expandedRegion() const;

private:
    void updateGeometry();

    QRegion m_region;
    int m_expandSize = 0;
};

} // namespace KWin
