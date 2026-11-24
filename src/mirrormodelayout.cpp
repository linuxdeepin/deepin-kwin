/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mirrormodelayout.h"

#include <algorithm>

namespace KWin
{

namespace MirrorModeLayout
{

int anchorIndex(const QVector<MirrorModeDisplay> &displays)
{
    int firstEnabled = -1;
    for (int i = 0; i < displays.size(); ++i) {
        if (!displays[i].enabled) {
            continue;
        }
        if (displays[i].primary) {
            return i;
        }
        if (firstEnabled < 0) {
            firstEnabled = i;
        }
    }
    return firstEnabled;
}

QRect clientGeometry(const QSize &scanoutSize)
{
    if (scanoutSize.isEmpty()) {
        return QRect();
    }
    return QRect(QPoint(0, 0), scanoutSize);
}

QVector<QRect> normalize(const QVector<MirrorModeDisplay> &displays)
{
    QVector<QRect> geometries;
    geometries.reserve(displays.size());
    for (const MirrorModeDisplay &display : displays) {
        geometries.append(display.geometry);
    }

    const int anchor = anchorIndex(displays);
    if (anchor < 0) {
        return geometries;
    }

    const QRect anchorGeometry(QPoint(0, 0), displays[anchor].geometry.size());
    geometries[anchor] = anchorGeometry;

    QVector<int> packed;
    packed.reserve(displays.size());
    for (int i = 0; i < displays.size(); ++i) {
        if (i != anchor && displays[i].enabled) {
            packed.append(i);
        }
    }
    std::sort(packed.begin(), packed.end(), [&displays](int lhs, int rhs) {
        const QPoint l = displays[lhs].geometry.topLeft();
        const QPoint r = displays[rhs].geometry.topLeft();
        if (l.x() != r.x()) {
            return l.x() < r.x();
        }
        if (l.y() != r.y()) {
            return l.y() < r.y();
        }
        return lhs < rhs;
    });

    int x = anchorGeometry.width();
    for (const int index : std::as_const(packed)) {
        const QSize size = displays[index].geometry.size();
        geometries[index] = QRect(x, 0, size.width(), size.height());
        x += size.width();
    }
    return geometries;
}

QRect clampToClientGeometry(const QRect &area, const QRect &clientGeometry)
{
    if (area.isEmpty() || clientGeometry.isEmpty()) {
        return QRect();
    }
    return area.intersected(clientGeometry);
}

} // namespace MirrorModeLayout
} // namespace KWin
