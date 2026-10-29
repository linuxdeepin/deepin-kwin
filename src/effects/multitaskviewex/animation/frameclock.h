/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <chrono>

namespace KWin
{

class FrameDeltaClock
{
public:
    /**
     * @brief Advances the clock and returns the delta since the previous frame.
     * @param present Current frame timestamp.
     * @return Frame delta in milliseconds, clamped to [0, 100]; the first frame yields 16 ms.
     */
    int advance(std::chrono::milliseconds present)
    {
        std::chrono::milliseconds delta(16);
        if (m_last.count() > 0) {
            delta = present - m_last;
            if (delta.count() < 0) {
                delta = std::chrono::milliseconds(0);
            }
            if (delta.count() > 100) {
                delta = std::chrono::milliseconds(100);
            }
        }
        m_last = present;
        return int(delta.count());
    }

    /**
     * @brief Resets the clock so the next advance() is treated as a first frame.
     */
    void reset()
    {
        m_last = std::chrono::milliseconds(0);
    }

private:
    std::chrono::milliseconds m_last{0};
};

}
