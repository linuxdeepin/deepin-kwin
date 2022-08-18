/*
 * Copyright (C) 2022 ~ 2022 Deepin Technology Co., Ltd.
 *
 * Author:     zjq <zhaojunqing@uniontech.com>
 *
 * Maintainer: zjq <zhaojunqing@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <deepin_kwineffects.h>
#include <map>

#include <QRect>

namespace KWin
{

#define DURATION_FLYING_BACK 300

class SplitScreenExitEffect
{

public:
    SplitScreenExitEffect() {
        m_status = -1;

        m_timeLine.setEasingCurve(QEasingCurve::OutQuint);
        m_timeLine.setDuration(std::chrono::milliseconds(DURATION_FLYING_BACK));
        m_timeLine.reset();

        m_why = -1;
    }

    ~SplitScreenExitEffect() {
        reset();
    }

    void reset() {
        m_status = -1;

        m_timeLine.reset();

        m_why = -1;
        m_winMotionMgrBegin.clear();
        m_winMotionMgrEnd.clear();
    }

    void add(EffectWindow* w, QRectF p) {
        m_winMotionMgrBegin[w] = p;
    }
    void setWinPreviewRectsBegin(QHash<EffectWindow*, QRectF>& winPreviewRects) {
        m_winMotionMgrBegin = winPreviewRects;
    }
    void setWinPreviewRectsEnd(QHash<EffectWindow*, QRectF>& winPreviewRects) {
        m_winMotionMgrEnd = winPreviewRects;
    }

    void because(int why) {
        m_why = why;
    }

    void begin() {
        m_status = 0;
        m_timeLine.reset();
    }

    bool animating() {
        return m_status == 1 || m_status == 0;
    }

    void update(int time) {
        if (m_status == 0) {
            m_timeLine.update(std::chrono::milliseconds(time));
            m_status = 1;
        } else if (m_status == 1) {
            m_timeLine.update(std::chrono::milliseconds(time));
            if (m_timeLine.done()) {
                m_status = 2;
            }
        }
    }

    bool done() {
        if (m_status == 2) {
            return true;
        } else {
            return false;
        }
    }

    void end() {
        reset();
    }

    int getStatus() {
        return m_status;
    }

    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) {
        if (m_status == 1) {
            if (w->isDesktop() /*|| w->isDock()*/) {
                data.setBrightness(0.4);
                effects->paintWindow(w, mask, region, data);
            } else if (m_winMotionMgrBegin.contains(w)) {
                float t = m_timeLine.value();

                QRect r = m_winMotionMgrBegin[w].toRect();
                QPoint p0 = r.topLeft();
                QRect d;
                if (m_winMotionMgrEnd.contains(w)) {
                    d = m_winMotionMgrEnd[w].toRect();
                }
                else {
                    d = w->geometry();
                }
                QPoint p1 = d.topLeft();

                float w0 = r.width();
                float w1 = d.width();
                float wt = (w1-w0) * t + w0;
                float ht = (d.height() - r.height()) * t + r.height();

                QPoint p = (p1 - p0) * t + p0;
                data.setScale(QVector2D(float(wt/w->width()), float(ht / w->height())));

                data += QPoint(p.x() - w->x(), p.y() - w->y());
                effects->paintWindow(w, mask, region, data);
            } else {
                effects->paintWindow(w, mask, region, data);
            }
        }
    }

private:
    QHash<EffectWindow*, QRectF> m_winMotionMgrBegin;
    QHash<EffectWindow*, QRectF> m_winMotionMgrEnd;

    TimeLine m_timeLine;

    int m_status;  // -1:reset  0:start,  1:doing   2:end

    /*
     * what cause this effect
     * 0: Esc pressed, or mouse hit blank area
     * 1: mouse hitted one window
     */
    int m_why;
};

}
