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
#include <QDBusMessage>
#include <QDBusConnection>
#include "configreader.h"

#define BRIGHTNESS          0.4
#define FIRST_WIN_SCALE     (float)(720.0 / 1080.0)
#define SPACING_H           (float)(20.0 / 1080.0)
#define SPACING_W           (float)(20.0 / 1920.0)

#define DBUS_APPEARANCE_SERVICE  "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ      "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF     "com.deepin.daemon.Appearance"

namespace KWin
{
SplitPreviewEffect::SplitPreviewEffect()
    : lastPresentTime(std::chrono::milliseconds::zero())
{
    connect(effectsEx, &EffectsHandlerEx::triggerSplitPreview, this, &SplitPreviewEffect::toggle);
    if (!m_effectFrame) {
        m_effectFrame = effectsEx->effectFrameEx("kwin/effects/splitscreen/qml/main.qml", false);
    }
    m_configReader = new ConfigReader(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ,
                                      DBUS_APPEARANCE_INTF, "WindowRadius");
}

SplitPreviewEffect::~SplitPreviewEffect()
{
    if (m_configReader) {
        delete m_configReader;
        m_configReader = nullptr;
    }
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

    effects->prePaintWindow(w, data, presentTime);
}

void SplitPreviewEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!isActive()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (w->isWaterMark()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (m_unPreviewWin.contains(w))
        return;

    int desktop = effects->currentDesktop();
    WindowMotionManager& wmm = m_motionManagers[0];
    if (wmm.isManaging(w) || w->isDesktop()) {
        auto area = effects->clientArea(FullArea, w);
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
                m_effectFrame->setGeometry(geo.adjusted(-4, -4, 4, 4).toRect());
                m_effectFrame->setRadius(m_radius);
                QColor color(effectsEx->getActiveColor());
                m_effectFrame->setColor(color);
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

void SplitPreviewEffect::toggle(KWin::EffectWindow *w)
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
            if (!effectsEx->isWinAllowSplit(w)) {
                m_unPreviewWin.append(w);
                continue;
            }
            if (w->isMinimized()) {
                w->refVisibleEx(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
            }
            wmm.manage(w);
        }
    }
    m_backgroundRect = getPreviewWindowsGeometry(w);
    m_screenRect = effects->clientArea(ScreenArea, w->screen(), currentDesktop);

    if (m_configReader->getProperty().isValid()) {
        m_radius = m_configReader->getProperty().toFloat() ? 10.0 : 0.0;
    }

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

    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "SplitScreenStateChanged");
    message << bool(m_activated);
    QDBusConnection::sessionBus().send(message);

    if (active) {
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        m_hasKeyboardGrab = effects->grabKeyboard(this);
        effects->setActiveFullScreenEffect(this);
    } else {
        cleanup();
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
    int currentDesktop = effects->currentDesktop();
    while (m_motionManagers.size() > 0) {
        for (const auto& w: m_motionManagers.first().managedWindows()) {
            if (w->isOnDesktop(currentDesktop) && isRelevantWithPresentWindows(w)) {
                if (w->isMinimized()) {
                    w->unrefVisibleEx(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
                }
            }
        }
        m_motionManagers.first().unmanageAll();
        m_motionManagers.removeFirst();
    }
    m_unPreviewWin.clear();
}

QRect SplitPreviewEffect::getPreviewWindowsGeometry(EffectWindow *w)
{
    int mode = effectsEx->getQuickTileMode(w);
    QRectF ret = effectsEx->getQuickTileGeometry(w, mode^0b11, effects->cursorPos());
    m_backgroundMode = mode ^ 0b11;
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
                effects->activateWindow(target);
                effectsEx->setQuickTileWindow(target, m_backgroundMode);
            }
            setActive(false);
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

    calculateWindowTransformationsClosest(windows, wmm);
}


void SplitPreviewEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist, WindowMotionManager& motionManager)
{
    QHash<EffectWindow*, QRect> targets;
    for (auto &w : windowlist) {
        QRect rect = w->clientGeometry().toRect();
        targets[w] = rect;
    }

    QRect clientRect = m_backgroundRect;
    float scaleHeight = clientRect.height() * FIRST_WIN_SCALE;
    int minSpacingH = SPACING_H * m_screenRect.height();
    int minSpacingW = SPACING_W * m_screenRect.width();
    int totalw = minSpacingW;
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
