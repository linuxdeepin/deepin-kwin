/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QPair>

namespace KWin
{

/**
 * @brief Computes the workspace-number shifts applied to wallpaper bindings after a workspace is removed.
 * @param removedNumber Desktop number that was removed.
 * @param count Workspace count before removal.
 * @return Pairs of (old desktop number, new desktop number) to move.
 */
inline QList<QPair<int, int>> computeWallpaperShiftMoves(int removedNumber, int count)
{
    QList<QPair<int, int>> moves;
    for (int i = removedNumber; i < count; ++i) {
        moves.append(qMakePair(i + 1, i));
    }
    return moves;
}

}
