/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "splitprevieweffect.h"
#include "../utils/common.h"
#include <QWindow>
#include <QKeyEvent>

#define BRIGHTNESS  0.4
#define SCALE_F     1.0
#define SCALE_S     2.0
#define WINDOW_W_H  300
#define FIRST_WIN_SCALE     (float)(720.0 / 1080.0)

namespace KWin
{
SplitPreviewEffect::SplitPreviewEffect()
    : lastPresentTime(std::chrono::milliseconds::zero())
{
    connect(effects, &EffectsHandler::windowFinishUserMovedResized, this, &SplitPreviewEffect::test);
}

SplitPreviewEffect::~SplitPreviewEffect()
{

}

void SplitPreviewEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    int time;
    if (lastPresentTime.count()) {
        time = std::max(1, int((presentTime - lastPresentTime).count()));
    } else {
        time = 1;
    }
    lastPresentTime = presentTime;
    if (isActive()) {
        // if (m_effectExit.animating())
        //     m_effectExit.update(time);
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        for (auto& mm: m_motionManagers) {
            mm.calculate(time / 2.0);
        }
    }

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant(true));
    }

    effects->prePaintScreen(data, presentTime);
}

void SplitPreviewEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
}

void SplitPreviewEffect::postPaintScreen()
{
    if (m_activated)
        effects->addRepaintFull();

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
    }
    effects->postPaintScreen();
}

void SplitPreviewEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    data.mask |= PAINT_WINDOW_TRANSFORMED;

    // if (m_activated) {
    //     w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);   // Display always
    // }
    // w->enablePainting(EffectWindow::PAINT_DISABLED);

    // QString cap;
    // if (QX11Info::isPlatformX11()) {
    //     if (w->caption().isEmpty() && effectsEx->getNETWMName(w) == WATERMARK_CLASS_NAME)
    //         cap = WATERMARK_CLASS_NAME;
    // } else {
    //     cap = w->windowClass();
    // }
    // if (!cap.contains(WATERMARK_CLASS_NAME)) {
    //     if (!(w->isDock() || w->isDesktop() || isRelevantWithPresentWindows(w)) || m_unminWinlist.contains(w)) {
    //         w->disablePainting(EffectWindow::PAINT_DISABLED);
    //         w->disablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    //     }
    // }

    effects->prePaintWindow(w, data, presentTime);
}

void SplitPreviewEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!isActive()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    int desktop = effects->currentDesktop();
    WindowMotionManager& wmm = m_motionManagers[0];
    if (wmm.isManaging(w) || w->isDesktop()) {
        auto area = effects->clientArea(FullArea/*ScreenArea*/, w);
        QRegion reg(area.toRect());
        WindowPaintData d = data;
        if (w->isDesktop()) {
            d.setBrightness(BRIGHTNESS);
            
            effects->paintWindow(w, mask, reg, d);
        } else if (!w->isDesktop()) {
            auto geo = m_motionManagers[0].transformedGeometry(w);

            d += QPoint(qRound(geo.x() - w->x()), qRound(geo.y() - w->y()));
            d.setScale(QVector2D((float)(geo.width() / w->width()), (float)(geo.height() / w->height())));

            effects->paintWindow(w, mask, reg, d);

            if (m_hoverwin == w) {
                if (!m_effectFrame) {
                    m_effectFrame = effectsEx->effectFrameEx("kwin/effects/splitscreen/qml/main.qml", false);
                }
                m_effectFrame->setGeometry(geo.adjusted(-4, -4, 4, 4).toRect());
                m_effectFrame->setRadius(8);
                m_effectFrame->render(region);
            }

            
        }
    } else {
        effects->paintWindow(w, mask, region, data);
    }
}

bool SplitPreviewEffect::isActive() const
{
    return m_activated && !effects->isScreenLocked();
}

void SplitPreviewEffect::test(KWin::EffectWindow *w)
{
    if (!effectsEx->getQuickTileMode(w))
        return;
    m_window = w;
    EffectWindowList windows = effects->stackingOrder();
    int currentDesktop = effects->currentDesktop();
    WindowMotionManager wmm;
    for (const auto& w: windows) {
        if (w->isOnDesktop(currentDesktop) && isRelevantWithPresentWindows(w)) {
            if (w == m_window)
                continue;

            // if (!effectsEx->checkWindowAllowToSplit(w)) {
            //     m_unminWinlist.append(w);
            //     continue;
            // }

            wmm.manage(w);
        }
    }
    m_backgroundRect = getPreviewWindowsGeometry(w);

    if (wmm.managedWindows().size() != 0) {
        calculateWindowTransformations(wmm.managedWindows(), wmm);
        m_motionManagers.append(wmm);
        setActive(true);
    }
}

void SplitPreviewEffect::setActive(bool active)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;

    if (m_activated == active)
        return;

    m_activated = active;

    // QDBusMessage message =QDBusMessage::createSignal("/KWin", "org.kde.KWin", "SplitScreenStateChanged");
    // message << bool(m_activated);
    // QDBusConnection::sessionBus().send(message);

    if (active) {
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        m_hasKeyboardGrab = effects->grabKeyboard(this);
        effects->setActiveFullScreenEffect(this);

    } else {
        cleanup();

        // auto p = m_motionManagers.begin();
        // while (p != m_motionManagers.end()) {
        //     for (auto &w : p->managedWindows()) {
        //         p->moveWindow(w, w->clientGeometry());
        //     }
        //     ++p;
        // }
    }

    effects->addRepaintFull();
}

void SplitPreviewEffect::cleanup()
{
    if (m_activated)
        return;

    if (m_hasKeyboardGrab)
        effects->ungrabKeyboard();
    m_hasKeyboardGrab = false;
    effects->stopMouseInterception(this);
    effects->setActiveFullScreenEffect(nullptr);
    lastPresentTime = std::chrono::milliseconds::zero();

    while (m_motionManagers.size() > 0) {
        m_motionManagers.first().unmanageAll();
        m_motionManagers.removeFirst();
    }

    // m_unminWinlist.clear();
}

QRect SplitPreviewEffect::getPreviewWindowsGeometry(EffectWindow *w)
{
    int mode = effectsEx->getQuickTileMode(w);
    QRectF ret = effects->clientArea(MaximizeArea, w);
    if (mode == int(QuickTileFlag::Left)) {
        ret.setLeft(ret.right() - (ret.width() - ret.width() / 2) + 1);
        m_backgroundMode = int(QuickTileFlag::Right);
    } else if (mode == int(QuickTileFlag::Right)) {
        ret.setRight(ret.left() + ret.width() / 2 - 1);
        m_backgroundMode = int(QuickTileFlag::Left);
    }

    return ret.toRect();
}

void SplitPreviewEffect::windowInputMouseEvent(QEvent* e)
{
    if (!m_activated)
        return;

    switch (e->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            break;
        default:
            return;
    }

    auto me = static_cast<QMouseEvent*>(e);

    EffectWindow* target = nullptr;
    WindowMotionManager& wm = m_motionManagers[0];
    for (const auto& w : wm.managedWindows()) {
        auto geo = wm.transformedGeometry(w);
        if (geo.contains(me->pos())) {
            target = w;
            break;
        }
    }

    switch (me->type()) {
        case QEvent::MouseMove:
            if (target) {
                m_hoverwin = target;
            } else {
                m_hoverwin = nullptr;
            }
            break;
        case QEvent::MouseButtonPress:
            if (target) {
                effects->addRepaintFull();
            }
            break;
        case QEvent::MouseButtonRelease:
            if (target) {
                effects->defineCursor(Qt::PointingHandCursor);
                // effects->setElevatedWindow(target, true);
                effects->activateWindow(target);
                effectsEx->setQuickTileWindow(target, m_backgroundMode);
                // effectsEx->setSplitWindow(target, m_backgroundMode);
            } else {
                // effectsEx->resetSplitGeometry();
            }

            // temp code
            // the client will reset to half screen area after setActive(false) on wayland,
            // because the client damage to new geometry will cost some time.
            // setActive(false) directly will cause client blink.
            // if (!QX11Info::isPlatformX11()) {
            //     QTimer::singleShot(50, [this]{
            //         setActive(false);
            //             });
            // } else {
                setActive(false);
            // }
            break;
        default:
            return;
    }
}

void SplitPreviewEffect::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        switch (e->key()) {
        case Qt::Key_Escape:
            setActive(false);
            break;
        }
    }
}

bool SplitPreviewEffect::isRelevantWithPresentWindows(EffectWindow *w) const
{
    if (w->isSpecialWindow() || w->isUtility()) {
        return false;
    }

    if (w->isDock()) {
        return false;
    }

    if (w->isSkipSwitcher()) {
        return false;
    }

    if (w->isDeleted()) {
        return false;
    }

    if (!w->acceptsFocus()) {
        return false;
    }

    if (!w->isCurrentTab()) {
        return false;
    }

    if (!w->isOnCurrentActivity()) {
        return false;
    }

    return true;
}

void SplitPreviewEffect::calculateWindowTransformations(EffectWindowList windows, WindowMotionManager& wmm)
{
    if (windows.size() == 0)
        return;

    calculateWindowTransformationsClosest(windows, /*m_screen*/0, wmm);
}


void SplitPreviewEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager/*, bool isReLayout*/)
{
    QHash<EffectWindow*, QRect> targets;
    for (auto &w : windowlist) {
        QRect rect = w->clientGeometry().toRect();
        targets[w] = rect;
    }

    QRect clientRect = m_backgroundRect;
    float scaleHeight = clientRect.height() * FIRST_WIN_SCALE;
    int minSpacingH = 20;//m_scale[screen].spacingHeight;
    int minSpacingW = 20;//m_scale[screen].spacingWidth;
    int totalw = 20;//m_scale[screen].spacingWidth;
    
    QList<int> centerList;
    int row = 1;
    int index = 1;
    int xpos = 0;
    bool overlap;
    do {
        overlap = false;
        for (int i = windowlist.size() - 1; i >= 0; i--) {
            EffectWindow *w = windowlist[i];
            if (!motionManager.isManaging(w))
                continue;
            QRect *target = &targets[w];
            float width = target->width();
            if (target->height() > scaleHeight) {
                float scale = (float)(scaleHeight / target->height());
                width = target->width() * scale;
            }
            totalw += width;
            totalw += minSpacingW;
            int singlew = width + minSpacingW * 2;
            if (singlew > clientRect.width())
                break;

            if (totalw > clientRect.width()) {
                index ++;
                if (index > row)
                    break;
                xpos = ((clientRect.width() - totalw + width + minSpacingW) / 2) + minSpacingW + clientRect.x();
                centerList.push_back(xpos);
                totalw = minSpacingW;
                totalw += width;
                totalw += minSpacingW;
            }
        }
        xpos = ((clientRect.width() - totalw) / 2) + minSpacingW + clientRect.x();
        centerList.push_back(xpos);

        if (totalw > clientRect.width()) {
            centerList.clear();
            overlap = true;
            scaleHeight -= 15;
            float critical = (float)(clientRect.height() - (row + 2) * minSpacingH) / (float)(row + 1);
            if (scaleHeight <= critical) {
                row++;
            }
            index = 1;
            totalw = minSpacingW;
        }
    } while (overlap);  //calculation layout row

    float winYPos = (clientRect.height() - (index - 1) * minSpacingH - index * scaleHeight) / 2 + clientRect.y();
    row = 1;
    
    int x = centerList.size() >= row ? centerList[row - 1] : 0;
    totalw = minSpacingW;
    for (int i = windowlist.size() - 1; i >= 0; i--) {
        EffectWindow *w = windowlist[i];
        if (!motionManager.isManaging(w))
            continue;

        QRect *target = &targets[w];
        float width = 0.0, height = 0.0;
        bool isFill = false;
        if (target->height() > scaleHeight) {
            float scale = (float)(scaleHeight / target->height());
            width = target->width() * scale;
            height = scaleHeight;
        } else {
            width = target->width();
            height = target->height();
            isFill = true;
        }
        totalw += width;
        totalw += minSpacingW;
        if (totalw > clientRect.width()) {
            row++;
            totalw = minSpacingW;
            totalw += width;
            totalw += minSpacingW;
            x = centerList.size() >= row ? centerList[row - 1] : 0;
            winYPos += minSpacingH;
            winYPos += scaleHeight;
        }

        target->setRect(x, winYPos + (scaleHeight - height) / 2, width, height);

        x += width;
        x += minSpacingW;

        motionManager.moveWindow(w, targets.value(w));
    }
}

}
