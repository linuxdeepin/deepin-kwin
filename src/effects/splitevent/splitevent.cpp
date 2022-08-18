/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "splitevent.h"

// #include <QtCore>
// #include <QMouseEvent>
// #include <QtMath>

// #include <deepin_kwinglutils.h>
// #include <deepin_kwinglplatform.h>
#include <effects.h>

//#include <qdbusconnection.h>

#define BRIGHTNESS  0.4
#define SCALE_F     1.0
#define SCALE_S     2.0
#define WINDOW_W_H  300


namespace KWin
{

SplitEventEffect::SplitEventEffect()
{
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowFinishUserMovedResized, this, &SplitEventEffect::slotWindowFinishUserMovedResized);
    connect(effectsEx, &EffectsHandlerEx::splitEventActive, this, &SplitEventEffect::slotActiveEvent);
}

SplitEventEffect::~SplitEventEffect()
{
}

// bool SplitEventEffect::supported()
// {
//     return effects->compositingType() == OpenGLCompositing && !GLPlatform::instance()->supports(LimitedNPOT);
// }


void SplitEventEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
}

void SplitEventEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    effects->prePaintScreen(data, presentTime);
}

void SplitEventEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    // if (!m_activated) {
    //     effects->paintScreen(mask, region, data);
    //     return;
    // }

    // QRect rectv;
    // QRect recth;

    // if (effectsEx->getOutlineState()) {
    //     effectsEx->getOutlineRect(effects->screenAt(cursorPos())->name(), recth, rectv);
    //     // qCritical() << "=================================" << cursorPos() << recth << rectv;
    //     if (!rectv.isEmpty() && rectv.contains(cursorPos())) {
    //         effectsEx->setShowLine(true, rectv);
    //     }
        
    //     if (!recth.isEmpty() && recth.contains(cursorPos())) {
    //         effectsEx->setShowLine(true, recth);
    //     }

    //     if (recth.isEmpty() && rectv.isEmpty()) {
    //         effectsEx->setShowLine(false);
    //     }
    // } else {
    //     effectsEx->setShowLine(false);
    // }

    effects->paintScreen(mask, region, data);
}

bool SplitEventEffect::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    // if (!m_activated)
    //     return false;
    // targetTouchWindow = nullptr;
    // WindowMotionManager& wm = m_motionManagers[0];
    // for (const auto& w : wm.managedWindows()) {
    //     auto geo = wm.transformedGeometry(w);
    //     if (geo.contains(pos)) {
    //         targetTouchWindow = w;
    //         break;
    //     }
    // }
    // if (targetTouchWindow) {
    //     effects->setElevatedWindow(targetTouchWindow, true);
    //     effects->addRepaintFull();
    // }
    return true;
}

bool SplitEventEffect::touchUp(qint32 id, quint32 time)
{
    // if (!m_activated)
    //     return false;
    // if (targetTouchWindow) {
    //     effects->defineCursor(Qt::PointingHandCursor);
    //     effects->setElevatedWindow(targetTouchWindow, false);
    //     effects->activateWindow(targetTouchWindow);
    //     effectsEx->setSplitWindow(targetTouchWindow, m_backgroundMode);
    // }
    // setActive(false);
    return true;
}

void SplitEventEffect::slotWindowFinishUserMovedResized(EffectWindow *w)
{
    auto effectWindow = static_cast<EffectWindowImpl*>(w);
    if (nullptr == effectWindow) {
        return;
    }
    auto client = static_cast<AbstractClient*>(effectWindow->window());
    if (nullptr == client) {
        return;
    }
    if (client->quickTileMode() != int(QuickTileFlag::None))
        m_activated = true;
}

void SplitEventEffect::slotActiveEvent()
{
    m_activated = true;
}

bool SplitEventEffect::isActive() const
{
    return false;
    return m_activated;
}

} // namespace KWin
