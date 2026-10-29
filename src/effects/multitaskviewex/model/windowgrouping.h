/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QVector>

namespace KWin
{

/**
 * @brief Groups item indices by the screen each item belongs to.
 * @param screenCount Number of screens.
 * @param screenIndexOfItem Screen index of each item.
 * @return One list of item indices per screen, preserving item order.
 */
inline QVector<QVector<int>> groupIndicesByScreen(int screenCount, const QVector<int> &screenIndexOfItem)
{
    QVector<QVector<int>> out;
    if (screenCount <= 0) {
        return out;
    }
    out.resize(screenCount);
    for (int i = 0; i < screenIndexOfItem.size(); ++i) {
        const int s = screenIndexOfItem.at(i);
        if (s >= 0 && s < screenCount) {
            out[s].append(i);
        }
    }
    return out;
}

}
