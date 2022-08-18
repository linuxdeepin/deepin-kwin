/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SPLITEVENT_H
#define KWIN_SPLITEVENT_H

#include <deepin_kwineffects.h>
#include <deepin_kwineffectsex.h>
#include <abstract_client.h>

#include "scene.h"
#include <QHash>
//#include <Plasma/FrameSvg>

//#include <utils.h>

namespace KWin
{

class SplitEventEffect : public Effect
{
    Q_OBJECT
public:
    SplitEventEffect();
    ~SplitEventEffect() override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    bool isActive() const override;

private Q_SLOTS:
    void slotWindowFinishUserMovedResized(EffectWindow *w);
    void slotActiveEvent();

private:
    bool m_activated = false;
    bool m_hasKeyboardGrab = false;
};

} // namespace KWin

#endif
