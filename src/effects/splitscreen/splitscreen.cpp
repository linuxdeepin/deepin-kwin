/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "splitscreen.h"

#include <QtCore>
#include <QMouseEvent>
#include <QtMath>
#include <QX11Info>
#include <deepin_kwinglutils.h>
#include <deepin_kwinglplatform.h>
#include <effects.h>

#include <qdbusconnection.h>

#define BRIGHTNESS  0.4
#define SCALE_F     1.0
#define SCALE_S     2.0
#define WINDOW_W_H  300
#define FIRST_WIN_SCALE     (float)(340.0 / 1080.0)
#define SECOND_WIN_SCALE    (float)(220.0 / 1080.0)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(splitscreen);
}

namespace KWin
{
QPoint splitScreenCalculateOffSet(const QRect& rect, const QSize screenArea, const int maxHeight)
{
    QPoint point;
    point.setX(rect.x());
    point.setY(maxHeight - rect.y() - rect.height());
    /*if (!QX11Info::isPlatformX11()) {             //for support wayland
        if (point.x() > screenArea.width()) {
            point.setX(point.x() - screenArea.width());
        }

        if (point.y() > screenArea.height()) {
            point.setY(point.y() - screenArea.height());
        }
    }*/
    return point;
}

SplitScreenEffect::SplitScreenEffect()
    : lastPresentTime(std::chrono::milliseconds::zero())
{
    reconfigure(ReconfigureAll);

    m_backgroundMode = int(QuickTileFlag::None);

    connect(effects, &EffectsHandler::windowStartUserMovedResized, this, &SplitScreenEffect::slotWindowStartUserMovedResized);
    connect(effects, &EffectsHandler::windowFinishUserMovedResized, this, &SplitScreenEffect::slotWindowFinishUserMovedResized);
    connect(effectsEx, &EffectsHandlerEx::windowQuickTileModeChanged, this, &SplitScreenEffect::slotWindowQuickTileModeChanged);
    connect(effectsEx, &EffectsHandlerEx::showSplitScreenPreview, this, &SplitScreenEffect::slotShowPreviewAlone);
    connect(effects, &EffectsHandler::windowAdded, this, &SplitScreenEffect::onWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &SplitScreenEffect::onWindowDeleted);
    connect(effects, &EffectsHandler::windowClosed, this, &SplitScreenEffect::onWindowClosed);

    //add masking single slot
    connect(effectsEx, &EffectsHandlerEx::signalSplitScreenStartShowMasking, this, &SplitScreenEffect::slotStartShowMasking);
    connect(effectsEx, &EffectsHandlerEx::signalSplitScreenResizeMasking, this, &SplitScreenEffect::slotResizeMasking);
    connect(effectsEx, &EffectsHandlerEx::signalSplitScreenExitMasking, this, &SplitScreenEffect::slotExitMasking);
    ensureResources();
    m_splitthumbShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QString(), QString(":/effects/splitscreen/shaders/splitthumb.frag"));
    //m_splitscrollShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture, QString(), QStringLiteral(":/effects/splitscreen/shaders/scrollthumb.frag"));
}

SplitScreenEffect::~SplitScreenEffect()
{
}

void SplitScreenEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
}

void SplitScreenEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    // The animation code assumes that the time diff cannot be 0, let's work around it.
    int time;
    if (lastPresentTime.count()) {
        time = std::max(1, int((presentTime - lastPresentTime).count()));
    } else {
        time = 1;
    }
    lastPresentTime = presentTime;

    if (isActive()) {
        if (m_effectExit.animating())
            m_effectExit.update(time);

        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

        for (auto& mm: m_motionManagers) {
            mm.calculate(time / 3.0/*15.0*/);
        }
    }

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant(true));
    }

    effects->prePaintScreen(data, presentTime);
}

void SplitScreenEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);
    if (m_previewRect.height() > m_backgroundRect.height())
        showScroll();
}

void SplitScreenEffect::postPaintScreen()
{
    bool resetLastPresentTime = true;

    if (m_activated) {
        if (m_effectExit.done()) {
            m_effectExit.end();
            if (isExitSplitScreen) {
                setActive(false);
                if (QX11Info::isPlatformX11() && m_dockRect.contains(m_cursorPos) && m_dockRect.contains(QCursor::pos())) {
                    relayDockEvent(m_cursorPos, m_buttonType);
                    m_cursorPos.setX(0);
                    m_cursorPos.setY(0);
                    m_buttonType = 0;
                } else if (!QX11Info::isPlatformX11() && m_sendDockButton != Qt::NoButton) {
                    effectsEx->sendPointer(m_sendDockButton);
                    m_sendDockButton = Qt::NoButton;
                }
            }
        }
        effects->addRepaintFull();
        resetLastPresentTime = false;
    }

    if (resetLastPresentTime) {
        lastPresentTime = std::chrono::milliseconds::zero();
    }

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
    }
    effects->postPaintScreen();
}

void SplitScreenEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    data.mask |= PAINT_WINDOW_TRANSFORMED;

    if (m_activated) {
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);   // Display always
    }
    w->enablePainting(EffectWindow::PAINT_DISABLED);
    if (!(w->isDock() || w->isDesktop() || isRelevantWithPresentWindows(w)) || m_unminWinlist.contains(w)) {
        w->disablePainting(EffectWindow::PAINT_DISABLED);
        w->disablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    } else if (isShowMasking && w->isMinimized()) {
        w->disablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    }
    if ((m_effectExit.animating() || m_effectExit.done()) && w->isMinimized()) {
        w->disablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    }

    effects->prePaintWindow(w, data, presentTime);
}

void SplitScreenEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (isShowMasking) {
        auto paintClient = static_cast<AbstractClient*>(static_cast<EffectWindowImpl*>(w)->window());
        if (!paintClient)
            return;

        int mode = paintClient->quickTileMode();
        if (mode != (int)QuickTileFlag::None && m_splitList.contains(w)) {
            auto iter = m_previewFill.find(mode);
            if (iter != m_previewFill.end() && w->screen()->name() == iter.value()->getScreen()) {
                iter.value()->render();
                return;
            }
        }

        effects->paintWindow(w, mask, region, data);
        return;
    }
    if (!isActive() || (isExitSplitScreen && m_effectExit.done())) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (m_effectExit.animating()) {
        if (w->isOnDesktop(effects->currentDesktop())) {
            m_effectExit.paintWindow(w, mask, region, data);
            if (w->isDesktop()) {
                for (auto iter = m_previewFill.begin(); iter != m_previewFill.end(); iter++) {
                    iter.value()->render();
                }
            }
        }
        return;
    }

    int desktop = effects->currentDesktop();
    WindowMotionManager& wmm = m_motionManagers[0];
    if (wmm.isManaging(w) || w->isDesktop()) {
        auto area = effects->clientArea(FullArea, w);
        WindowPaintData d = data;
        if (w->isDesktop()) {
            d.setBrightness(BRIGHTNESS);
            effects->paintWindow(w, mask, area, d);
            for (auto iter = m_previewFill.begin(); iter != m_previewFill.end(); iter++) {
                iter.value()->render();
            }
        } else {
            QRectF geo;
            if (m_startEffect)
                geo = wmm.transformedGeometry(w);
            else
                geo = m_previewWindowRects[w];

            d += QPoint(qRound(geo.x() - w->x()), qRound(geo.y() - w->y()));
            d.setScale(QVector2D((float)(geo.width() / w->width()), (float)(geo.height() / w->height())));

            if (m_hoverwin == w) {
                if (!m_highlightFrame) {
                    m_highlightFrame = effects->effectFrame(EffectFrameUnstyled, false);
                }
                QRect geo_frame = geo.toRect();
                geo_frame.adjust(-1, -1, 1, 1);
                m_highlightFrame->setGeometry(geo_frame);


                glEnable(GL_SCISSOR_TEST);
                glScissor(m_backgroundRect.x(), area.height() - m_backgroundRect.y() - m_backgroundRect.height(), m_backgroundRect.width(), m_backgroundRect.height());

                ShaderBinder binder(m_splitthumbShader);

                QColor color= effectsEx->getActiveColor();
                m_splitthumbShader->setUniform(GLShader::Color, color);
                geo_frame.adjust(-3, -3, 3, 3);
                m_splitthumbShader->setUniform("iResolution", QVector3D(geo_frame.width(), geo_frame.height(), 0));
                m_splitthumbShader->setUniform("iOffset", QVector2D(splitScreenCalculateOffSet(geo_frame, area.size(), area.height())));

                m_highlightFrame->render(infiniteRegion(), 1, 0);
                m_highlightFrame->setShader(m_splitthumbShader);

                glDisable(GL_SCISSOR_TEST);
            }

            effects->paintWindow(w, mask, /*area*/m_backgroundRect, d);
        }
    } else {
        effects->paintWindow(w, mask, region, data);
    }
}

void SplitScreenEffect::showScroll()
{
    if (!m_scrollFrame) {
        m_scrollFrame = effects->effectFrame(EffectFrameUnstyled, false);
        m_scrollFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    }

    float s;
    QSize size(15, 39);
    QIcon icon(":/effects/splitscreen/icon/scrollbar.svg");
    if (m_scrollStatus == 1) {
        icon = QIcon(":/effects/splitscreen/icon/scrollbar_hover.svg");
    } else if (m_scrollStatus == 2) {
        icon = QIcon(":/effects/splitscreen/icon/scrollbar_active.svg");
    }
    m_scrollFrame->setIcon(icon);
    m_scrollFrame->setIconSize(size);

    int previewstep = 0;
    if (!m_scrollStartRect.isEmpty()) {
        s = (float)(m_scrollRect.y() - m_scrollStartRect.y()) / (float)(m_backgroundRect.height() - 39);
        m_scrollMoveDistance = m_scrollMoveStart - (m_previewRect.height() - m_backgroundRect.height()) * s;
        previewstep = m_scrollMoveDistance - m_previewRect.y();
    }

    if (m_isWheelScroll) {
        if (previewstep != 0) {
            for (auto iter = m_previewWindowRects.begin(); iter != m_previewWindowRects.end(); iter++) {
                QRectF geo = iter.value();
                geo.moveTop(geo.y() + previewstep);
                m_previewWindowRects[iter.key()] = geo;
            }
        }
        m_previewRect.moveTop(m_scrollMoveDistance);
    }

    if (m_isPressScroll) {
        if (previewstep != 0) {
            for (auto iter = m_previewWindowRects.begin(); iter != m_previewWindowRects.end(); iter++) {
                QRectF geo = iter.value();
                geo.moveTop(geo.y() + previewstep);
                m_previewWindowRects[iter.key()] = geo;
            }
        }
        m_previewRect.moveTop(m_scrollMoveDistance);
    }

    m_scrollFrame->setPosition(m_scrollRect.topLeft());
    m_scrollFrame->render(infiniteRegion(), 1, 0);
}

bool SplitScreenEffect::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    if (!m_activated || isShowMasking)
        return false;
    targetTouchWindow = nullptr;
    WindowMotionManager& wm = m_motionManagers[0];
    for (const auto& w : wm.managedWindows()) {
        auto geo = wm.transformedGeometry(w);
        if (geo.contains(pos)) {
            targetTouchWindow = w;
            break;
        }
    }
    if (targetTouchWindow) {
        effects->setElevatedWindow(targetTouchWindow, true);
        effects->addRepaintFull();
    }
    return true;
}

bool SplitScreenEffect::touchUp(qint32 id, quint32 time)
{
    if (!m_activated || isShowMasking)
        return false;
    if (targetTouchWindow) {
        effects->defineCursor(Qt::PointingHandCursor);
        effects->setElevatedWindow(targetTouchWindow, true);

        m_quickTileMode = QuickTileMode(m_backgroundMode);

        m_splitList.insert(targetTouchWindow);
        m_effectExit.setWinPreviewRectsBegin(m_previewWindowRects);
        effectsEx->setSplitWindow(targetTouchWindow, m_backgroundMode, false);
        m_startEffect = false;

        if (reLayout())
            m_effectExit.setWinPreviewRectsEnd(m_previewWindowRects);

        m_effectExit.begin();
    } else {
        setActive(false);
    }

    return true;
}

void SplitScreenEffect::handleWheelEvent(Qt::MouseButtons btn)
{
    if (m_isWheelScroll)
        return;
    m_startEffect = false;
    m_isWheelScroll = true;
    if (btn == Qt::ForwardButton) {
        if (m_scrollRect.y() > m_backgroundRect.y()) {
            int y = m_scrollRect.y() - 15;
            QPoint pos;
            if (y <= m_backgroundRect.y())
                pos = QPoint(m_backgroundRect.x() + m_backgroundRect.width() - 16, m_backgroundRect.y() + 2);
            else
                pos = QPoint(m_backgroundRect.x() + m_backgroundRect.width() - 16, y);
            m_scrollRect = QRect(pos, QSize(15, 39));
        }
    } else if (btn == Qt::BackButton) {
        if (m_scrollRect.bottom() < m_backgroundRect.bottom()) {
            int y = m_scrollRect.bottom() + 15;
            QPoint pos;
            if (y >= m_backgroundRect.bottom())
                pos = QPoint(m_backgroundRect.x() + m_backgroundRect.width() - 16, m_scrollRect.y() + 15 - y + m_backgroundRect.bottom() - 2);
            else
                pos = QPoint(m_backgroundRect.x() + m_backgroundRect.width() - 16, m_scrollRect.y() + 15);
            m_scrollRect = QRect(pos, QSize(15, 39));
        }
    }
    QTimer::singleShot(300, [&]() { m_isWheelScroll = false; });
}

void SplitScreenEffect::relayDockEvent(QPoint pos, int button)
{
    if (m_dock) {
        auto cl = static_cast<EffectWindowImpl *>(m_dock)->window();
        xcb_button_release_event_t c;
        memset(&c, 0, sizeof(c));

        c.response_type = XCB_BUTTON_PRESS;
        c.event = cl->window();
        c.event_x = pos.x() - m_dockRect.x();
        c.event_y = pos.y() - m_dockRect.y();
        c.detail = (button == 1 ? 1 : 3);  // 1 左键 3 右键
        xcb_send_event(connection(), false, c.event, XCB_EVENT_MASK_BUTTON_PRESS, reinterpret_cast<const char*>(&c));
        xcb_flush(connection());

        memset(&c, 0, sizeof(c));
        c.response_type = XCB_BUTTON_RELEASE;
        c.event = cl->window();
        c.event_x = pos.x() - m_dockRect.x();
        c.event_y = pos.y() - m_dockRect.y();
        c.detail = (button == 1 ? 1 : 3);
        xcb_send_event(connection(), false, c.event, XCB_EVENT_MASK_BUTTON_RELEASE, reinterpret_cast<const char*>(&c));
        xcb_flush(connection());
    } // button release
}

void SplitScreenEffect::windowInputMouseEvent(QEvent* e)
{
    if (!isReceiveEvent()) {
        return;
    }
    if (!m_activated)
        return;

    switch (e->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::Wheel:
            break;
        default:
            return;
    }

    auto me = static_cast<QMouseEvent*>(e);
    if (e->type() == QEvent::Wheel) {
        auto wheelEvent = static_cast<QWheelEvent*>(e);
        if (wheelEvent->delta() > 0) {
            if (QX11Info::isPlatformX11())
                handleWheelEvent(Qt::ForwardButton);
            else
                handleWheelEvent(Qt::BackButton);
        } else if (wheelEvent->delta() < 0) {
            if (QX11Info::isPlatformX11())
                handleWheelEvent(Qt::BackButton);
            else
                handleWheelEvent(Qt::ForwardButton);
        }
        return;
    }

    EffectWindow* target = nullptr;
    WindowMotionManager& wm = m_motionManagers[0];
    for (const auto& w : wm.managedWindows()) {
        if (m_previewWindowRects.find(w) == m_previewWindowRects.end()) {
            continue;
        }
        auto geo = m_previewWindowRects[w];
        if (geo.contains(me->pos()) && m_backgroundRect.contains(me->pos())) {
            target = w;
            break;
        }
    }

    if (!m_isPressScroll) {
        if (m_scrollRect.contains(me->pos())) {
            m_scrollStatus = 1;
        } else {
            m_scrollStatus = 0;
        }
    }


    switch (me->type()) {
    case QEvent::MouseMove:
        if (m_scrollStatus == 2) {
            if (me->pos().y() - m_scrollStartPosY + m_scrollStartRect.y() > m_backgroundRect.y() &&
                me->pos().y() + m_scrollStartRect.bottom() - m_scrollStartPosY < m_backgroundRect.bottom()) {
               QPoint pos(m_backgroundRect.x() + m_backgroundRect.width() - 16, me->pos().y() - m_scrollStartPosY + m_scrollStartRect.y());
               m_scrollRect = QRect(pos, QSize(15, 39));
            }
        } else if (target) {
            m_hoverwin = target;
        } else {
            m_hoverwin = nullptr;
        }
        break;
    case QEvent::MouseButtonPress:
        if (target) {
            effects->addRepaintFull();
        } else if (m_scrollStatus == 1) {
            m_scrollStatus = 2;
            m_isPressScroll = true;
            m_scrollStartPosY = me->pos().y();
            m_scrollStartRect = m_scrollRect;
            m_scrollMoveStart = m_previewRect.y();
            m_startEffect = false;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (m_dockRect.contains(me->pos())) {
            isExitSplitScreen = true;
            if (QX11Info::isPlatformX11()) {
                isExitSplitScreen = true;
                m_effectExit.setWinPreviewRectsBegin(m_previewWindowRects);
                m_effectExit.begin();
                m_cursorPos = me->pos();
                m_buttonType = me->button();
            } else {
                m_effectExit.setWinPreviewRectsBegin(m_previewWindowRects);
                m_effectExit.begin();
                m_sendDockButton = me->button();
            }
        } else if (m_scrollStatus == 2) {
            m_isPressScroll = false;
            m_scrollMoveDistance = 0;
            m_scrollStartPosY = 0;
            m_scrollStatus = 0;
        } else if (target) {
            effects->defineCursor(Qt::PointingHandCursor);
            effects->setElevatedWindow(target, true);
            // effects->activateWindow(target);

            m_quickTileMode = QuickTileMode(m_backgroundMode);

            m_splitList.insert(target);
            m_effectExit.setWinPreviewRectsBegin(m_previewWindowRects);
            effectsEx->setSplitWindow(target, m_backgroundMode, false);
            m_startEffect = false;

            if (reLayout())
                m_effectExit.setWinPreviewRectsEnd(m_previewWindowRects);

            m_effectExit.begin();
        } else {
            isExitSplitScreen = true;
            m_effectExit.setWinPreviewRectsBegin(m_previewWindowRects);
            m_effectExit.begin();
        }

        break;
    default:
        return;
    }
}

void SplitScreenEffect::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        switch (e->key())
        {
        case Qt::Key_Escape:
            setActive(false);
            break;
        default:
            break;
        }
    }
}

void SplitScreenEffect::initData()
{
    m_scrollMoveDistance = 0;
    m_splitList.clear();
    isExitSplitScreen = false;
    m_splitMode = 0;
    m_previewLocation = SplitLocationMode(SplitLocation::None);
    QHash<int, PreviewFill *>::iterator it;
    for(it = m_previewFill.begin(); it != m_previewFill.end(); ++it) {
        delete it.value();
        it.value() = nullptr;
    }
    m_previewFill.clear();
}

void SplitScreenEffect::slotWindowStartUserMovedResized(EffectWindow *w)
{
    if (!w->isMovable())
        return;

    m_window = w;

    auto wImpl = static_cast<EffectWindowImpl*>(w);
    if (wImpl)
        m_cacheClient = static_cast<AbstractClient*>(wImpl->window());

    setActive(false);
}

void SplitScreenEffect::slotWindowQuickTileModeChanged(EffectWindow *w)
{
    auto c = static_cast<AbstractClient*>(static_cast<EffectWindowImpl*>(w)->window());
    if (c != m_cacheClient && !m_cacheClient)
        return;

    m_quickTileMode = m_cacheClient->quickTileMode();
}

void SplitScreenEffect::slotWindowFinishUserMovedResized(EffectWindow *w)
{
    if (!m_cacheClient) {
        return;
    }

    if (m_cacheClient->isSwapHandle() || !isEnterSplitMode(m_quickTileMode)) {
        m_cacheClient = nullptr;
        m_quickTileMode = QuickTileMode(QuickTileFlag::None);
        return;
    }
    initData();
    if (m_quickTileMode & QuickTileFlag::Left && m_quickTileMode & QuickTileFlag::Right) {
        return;
    }

    int desktop = effects->currentDesktop();
    m_screen = effectsEx->getScreenWithSplit();
    if (m_screen.isEmpty())
        m_screen = w->screen()->name();
    effectsEx->getSplitList(m_splitList, m_splitMode, desktop, m_screen);  //borrow m_splitMode

    if (m_splitMode == int(SplitLocation::AllShow)) { 
        return;
    }
    m_previewLocation = (SplitLocationMode)m_splitMode;
    if (m_splitList.size() == 0) {
        m_splitList.insert(w);
    }

    m_splitMode = effectsEx->getSplitMode(desktop, m_screen);
    m_screenRect = effects->clientArea(FullArea, w);
    m_pos = w->pos();
    if (!QX11Info::isPlatformX11()) {
        m_pos = cursorPos();
    }

    createBackgroundFill(m_screen, true, true);
    m_backgroundRect = getPreviewWindowsGeometry();
    m_scrollRect = QRect(m_backgroundRect.x() + m_backgroundRect.width() - 16, m_backgroundRect.y(), 15, 39);
    m_scrollStartRect = m_scrollRect;
    preSetActive(w);
    m_scrollMoveStart = m_previewRect.y();
}

void SplitScreenEffect::slotShowPreviewAlone(EffectWindow *w)
{
    m_cacheClient = static_cast<AbstractClient*>(static_cast<EffectWindowImpl*>(w)->window());
    if (!m_cacheClient)
        return;

    initData();
    if (m_screen.isEmpty())
        m_screen = w->screen()->name();

    int desktop = effects->currentDesktop();
    m_splitList.insert(w);
    m_window = w;
    m_screenRect = effects->clientArea(FullArea, m_window);
    m_pos = w->pos();
    if (!QX11Info::isPlatformX11()) {
        m_pos = cursorPos();
    }
    m_quickTileMode = m_cacheClient->quickTileMode();
    m_splitMode = effectsEx->getSplitMode(desktop, m_screen);
    updateSplitLocationFromMenu(m_quickTileMode);

    createBackgroundFill(m_screen, false);
    m_backgroundRect = getPreviewWindowsGeometry();
    m_scrollRect = QRect(m_backgroundRect.x() + m_backgroundRect.width() - 16, m_backgroundRect.y(), 15, 39);
    m_scrollStartRect = m_scrollRect;
    preSetActive(w);
    m_scrollMoveStart = m_previewRect.y();
}

void SplitScreenEffect::onWindowClosed(EffectWindow *w)
{
    if (!m_activated)
        return;

    if (w->isDock()) {
        m_dock = nullptr;
        m_dockRect.setSize(QSize(0, 0));
    }
}

void SplitScreenEffect::onWindowDeleted(EffectWindow *w)
{
    if (!m_activated)
        return;

    if (w->isDock()) {
        m_dock = nullptr;
        m_dockRect.setSize(QSize(0, 0));
    }
}

void SplitScreenEffect::onWindowAdded(EffectWindow *w)
{
    if (!m_activated)
        return;

    if (w->isDock()) {
        m_dockRect = w->geometry();
        m_dock = w;
    }
}

void SplitScreenEffect::updateSplitLocationFromMenu(QuickTileMode m, bool isClear)
{
    if (m & QuickTileFlag::Left) {
        if (m & QuickTileFlag::Top) {
            m_previewLocation = isClear ? m_previewLocation & ~SplitLocationMode(SplitLocation::leftTop)
                                         : m_previewLocation | SplitLocation::leftTop;
        } else if (m & QuickTileFlag::Bottom) {
            m_previewLocation = isClear ? m_previewLocation & ~SplitLocationMode(SplitLocation::leftBottom)
                                         : m_previewLocation | SplitLocation::leftBottom;
        } else {
            m_previewLocation = isClear ? m_previewLocation & ~SplitLocationMode(SplitLocation::leftTop)
                                         : m_previewLocation | SplitLocation::leftTop;
            m_previewLocation = isClear ? m_previewLocation & ~SplitLocationMode(SplitLocation::leftBottom)
                                         : m_previewLocation | SplitLocation::leftBottom;
        }
    } else if (m & QuickTileFlag::Right) {
        if (m & QuickTileFlag::Top) {
            m_previewLocation = isClear ? m_previewLocation & ~SplitLocationMode(SplitLocation::rightTop)
                                         : m_previewLocation | SplitLocation::rightTop;
        } else if (m & QuickTileFlag::Bottom) {
            m_previewLocation = isClear ? m_previewLocation & ~SplitLocationMode(SplitLocation::rightBottom)
                                         : m_previewLocation | SplitLocation::rightBottom;
        } else {
            m_previewLocation = isClear ? m_previewLocation & ~SplitLocationMode(SplitLocation::rightTop)
                                         : m_previewLocation | SplitLocation::rightTop;
            m_previewLocation = isClear ? m_previewLocation & ~SplitLocationMode(SplitLocation::rightBottom)
                                         : m_previewLocation | SplitLocation::rightBottom;
        }
    }
}

bool SplitScreenEffect::isEnterSplitMode(QuickTileMode mode)
{
    return ((mode & QuickTileFlag::Left) || (mode & QuickTileFlag::Right));
            //&& !((mode & QuickTileFlag::Top) || (mode & QuickTileFlag::Bottom)); //to support 1/4 split show review
}

void SplitScreenEffect::getTwoSplitQuickmatch()
{
    if (!(m_previewLocation & SplitLocation::leftTop) && !(m_previewLocation & SplitLocation::leftBottom)) {
        m_previewLocation |= SplitLocation::leftTop;
        m_previewLocation |= SplitLocation::leftBottom;
        m_backgroundMode = int(QuickTileFlag::Left);
    } else if (!(m_previewLocation & SplitLocation::rightTop) && !(m_previewLocation & SplitLocation::rightBottom)) {
        m_previewLocation |= SplitLocation::rightTop;
        m_previewLocation |= SplitLocation::rightBottom;
        m_backgroundMode = int(QuickTileFlag::Right);
    }
}

void SplitScreenEffect::getThreeSplitQuickmatch()
{
    if (m_quickTileMode & QuickTileFlag::Top || m_quickTileMode & QuickTileFlag::Bottom) {
        if (m_quickTileMode & QuickTileFlag::Right) {
            if (!(m_previewLocation & SplitLocation::leftTop)) {
                m_previewLocation |= SplitLocation::leftTop;
                m_previewLocation |= SplitLocation::leftBottom;
                m_backgroundMode = int(QuickTileFlag::Left);
            } else if (!(m_previewLocation & SplitLocation::rightTop)) {
                m_previewLocation |= SplitLocation::rightTop;
                m_backgroundMode = QuickTileFlag::Top | QuickTileFlag::Right;
            } else if (!(m_previewLocation & SplitLocation::rightBottom)) {
                m_previewLocation |= SplitLocation::rightBottom;
                m_backgroundMode = QuickTileFlag::Bottom | QuickTileFlag::Right;
            }
        } else if (m_quickTileMode & QuickTileFlag::Left) {
            if (!(m_previewLocation & SplitLocation::leftTop)) {
                m_previewLocation |= SplitLocation::leftTop;
                m_backgroundMode = QuickTileFlag::Top | QuickTileFlag::Left;
            } else if (!(m_previewLocation & SplitLocation::leftBottom)) {
                m_previewLocation |= SplitLocation::leftBottom;
                m_backgroundMode = QuickTileFlag::Bottom | QuickTileFlag::Left;
            } else if (!(m_previewLocation & SplitLocation::rightTop)) {
                m_previewLocation |= SplitLocation::rightTop;
                m_previewLocation |= SplitLocation::rightBottom;
                m_backgroundMode = int(QuickTileFlag::Right);
            }
        }
    } else {
        if (m_quickTileMode & QuickTileFlag::Left) {
            if (!(m_previewLocation & SplitLocation::rightTop)) {
                m_previewLocation |= SplitLocation::rightTop;
                m_backgroundMode = QuickTileFlag::Top | QuickTileFlag::Right;
            } else if (!(m_previewLocation & SplitLocation::rightBottom)) {
                m_previewLocation |= SplitLocation::rightBottom;
                m_backgroundMode = QuickTileFlag::Bottom | QuickTileFlag::Right;
            }
        } else {
            if (!(m_previewLocation & SplitLocation::leftTop)) {
                m_previewLocation |= SplitLocation::leftTop;
                m_backgroundMode = QuickTileFlag::Top | QuickTileFlag::Left;
            } else if (!(m_previewLocation & SplitLocation::leftBottom)) {
                m_previewLocation |= SplitLocation::leftBottom;
                m_backgroundMode = QuickTileFlag::Bottom | QuickTileFlag::Left;
            }
        }
    }
}

void SplitScreenEffect::getFourSplitQuickmatch()
{
    if (!(m_previewLocation & SplitLocation::leftTop)) {
        m_previewLocation |= SplitLocation::leftTop;
        m_backgroundMode = QuickTileFlag::Top | QuickTileFlag::Left;
    } else if (!(m_previewLocation & SplitLocation::rightTop)) {
        m_previewLocation |= SplitLocation::rightTop;
        m_backgroundMode = QuickTileFlag::Top | QuickTileFlag::Right;
    } else if (!(m_previewLocation & SplitLocation::leftBottom)) {
        m_previewLocation |= SplitLocation::leftBottom;
        m_backgroundMode = QuickTileFlag::Bottom | QuickTileFlag::Left;
    } else if (!(m_previewLocation & SplitLocation::rightBottom)) {
        m_previewLocation |= SplitLocation::rightBottom;
        m_backgroundMode = QuickTileFlag::Bottom | QuickTileFlag::Right;
    }
}

QRect SplitScreenEffect::getPreviewWindowsGeometry()
{
    if (m_splitMode == int(SplitMode::Two)) {
        getTwoSplitQuickmatch();
    } else if (m_splitMode == int(SplitMode::Three)) {
        getThreeSplitQuickmatch();
    } else if (m_splitMode == int(SplitMode::Four)) {
        getFourSplitQuickmatch();
    }
    if (m_previewFill.find((int)m_backgroundMode) == m_previewFill.end())
        return QRect();
    QRect rect = m_previewFill[(int)m_backgroundMode]->getRect();
    return rect;
}

QRect SplitScreenEffect::getPreviewAreaGeometry(QuickTileMode mode, QString screen, bool isRecalculate, bool isUseTmp)
{
    m_maximizeArea = effects->clientArea(MaximizeArea, m_pos, effects->currentDesktop());
    QRect ret = m_maximizeArea;

    if (mode & QuickTileFlag::Left)
        ret.setRight(ret.left()+ret.width()/2 - 2);
    else if (mode & QuickTileFlag::Right)
        ret.setLeft(ret.right()-(ret.width()-ret.width()/2) + 2);
    if (mode & QuickTileFlag::Top)
        ret.setBottom(ret.top()+ret.height()/2 - 2);
    else if (mode & QuickTileFlag::Bottom)
        ret.setTop(ret.bottom()-(ret.height()-ret.height()/2) + 2);

    if (!isRecalculate)
        return ret;

    return effectsEx->getSplitArea((int)mode, ret, m_maximizeArea, screen, effects->currentDesktop(), isUseTmp);
}

bool SplitScreenEffect::isActive() const
{
    return m_activated && !effects->isScreenLocked();
}

void SplitScreenEffect::cleanup()
{
    if (m_activated)
        return;

    if (m_hasKeyboardGrab)
        effects->ungrabKeyboard();
    m_hasKeyboardGrab = false;
    lastPresentTime = std::chrono::milliseconds::zero();
    effects->stopMouseInterception(this);
    effects->setActiveFullScreenEffect(nullptr);

    while (m_motionManagers.size() > 0) {
        m_motionManagers.first().unmanageAll();
        m_motionManagers.removeFirst();
    }

    m_scrollMoveDistance = 0;
    m_splitMode = 0;
    m_splitList.clear();
    m_unminWinlist.clear();
    m_previewFill.clear();
    m_previewWindowRects.clear();
    m_quickTileMode = QuickTileMode(QuickTileFlag::None);
    m_previewLocation = SplitLocationMode(SplitLocation::None);
    m_cacheClient = nullptr;
    isExitSplitScreen = false;
    m_screen = "";
    m_startEffect = false;
}

bool SplitScreenEffect::reLayout()
{
    if (m_previewLocation == SplitLocationMode(SplitLocation::AllShow)) {
        isExitSplitScreen = true;
        return false;
    }

    m_unminWinlist.clear();
    EffectWindowList windows = effects->stackingOrder();
    int currentDesktop = effects->currentDesktop();
    EffectWindowList winList;
    WindowMotionManager wmm;
    for (const auto& w: windows) {
        if (w->isOnDesktop(currentDesktop) && isRelevantWithPresentWindows(w)) {
            if (w == m_window || m_splitList.contains(w)) {
                continue;
            }

            if (!effectsEx->checkWindowAllowToSplit(w)) {
                m_unminWinlist.append(w);
                continue;
            }
            //preview的窗口到这里
            wmm.manage(w);
            winList.push_back(w);
        } else if (w->isDock()) {
            m_dockRect = w->geometry();
            m_dock = w;
        }
    }
    if (wmm.managedWindows().size() == 0) {
        isExitSplitScreen = true;
        return false;
    }

    while (m_motionManagers.size() > 0) {
        m_motionManagers.first().unmanageAll();
        m_motionManagers.removeFirst();
    }
    m_previewWindowRects.clear();
    m_backgroundRect = getPreviewWindowsGeometry();
    calculateWindowTransformations(winList, wmm);
    m_motionManagers.append(wmm);
    m_scrollRect = QRect(m_backgroundRect.x() + m_backgroundRect.width() - 16, m_backgroundRect.y(), 15, 39);
    m_scrollStartRect = m_scrollRect;
    m_scrollMoveStart = m_previewRect.y();
    return true;
}

void SplitScreenEffect::preSetActive(EffectWindow *w)
{
    m_unminWinlist.clear();
    EffectWindowList windows = effects->stackingOrder();
    int currentDesktop = effects->currentDesktop();
    EffectWindowList winList;
    WindowMotionManager wmm;
    for (const auto& w: windows) {
        if (w->isOnDesktop(currentDesktop) && isRelevantWithPresentWindows(w)) {
            if (w == m_window || m_splitList.contains(w)) {
                continue;
            }

            if (!effectsEx->checkWindowAllowToSplit(w)) {
                m_unminWinlist.append(w);
                continue;
            }

            //preview的窗口到这里
            wmm.manage(w);
            winList.push_back(w);
        } else if (w->isDock()) {
            m_dockRect = w->geometry();
            m_dock = w;
        }
    }

    if (wmm.managedWindows().size() != 0) {
        calculateWindowTransformations(winList, wmm);
        m_motionManagers.append(wmm);
        setActive(true);
    }

    m_cacheClient = nullptr;
    m_quickTileMode = QuickTileMode(QuickTileFlag::None);
}

void SplitScreenEffect::setActive(bool active)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;

    if (m_activated == active)
        return;

    m_activated = active;

    m_effectExit.reset();
    if (active) {
        if (!isShowMasking) {
            effects->startMouseInterception(this, Qt::ArrowCursor);
            m_hasKeyboardGrab = effects->grabKeyboard(this);
            effects->setActiveFullScreenEffect(this);
            m_startEffect = true;
        }
    } else {
        cleanup();
        auto p = m_motionManagers.begin();
        while (p != m_motionManagers.end()) {
            for (EffectWindow *w : p->managedWindows()) {
                p->moveWindow(w, w->geometry());
            }
            ++p;
        }
    }

    effects->addRepaintFull();
}

bool SplitScreenEffect::isRelevantWithPresentWindows(EffectWindow *w) const
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

void SplitScreenEffect::calculateWindowTransformations(EffectWindowList windows, WindowMotionManager& wmm)
{
    if (windows.size() == 0)
        return;

    calculateWindowTransformationsClosest(windows, /*m_screen*/0, wmm);
}

bool SplitScreenEffect::isReceiveEvent()
{
    if (isShowMasking) {
        return false;
    }
    if (!m_activated) {
        return false;
    }

    if (m_effectExit.getStatus() != -1) {
        return false;
    }

    return true;
}

void SplitScreenEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen,
        WindowMotionManager& motionManager)
{
    Q_UNUSED(screen)

    QList<int> centerList;
    QHash<int, QList<EffectWindow*>> winLayout;
    QHash<EffectWindow*, QRect> targets;
    for (int i = 0; i < windowlist.size(); i++) {
        QRect rect = windowlist[i]->geometry();
        targets[windowlist[i]] = rect;
    }

    QRect clientRect = m_backgroundRect;
    QRect screenRect = effects->clientArea(MaximizeFullArea, windowlist[0]);
    float scaleHeight = screenRect.height() * FIRST_WIN_SCALE;
    float scaleHeightEx = screenRect.height() * SECOND_WIN_SCALE;

    int index = 1;
    int xpos = 0, ypos = clientRect.y() + 20;
    int totalh = 20, totalw = 20;
    int totalWidth = m_backgroundRect.width() - 40;

    bool isMin = false;

    if (clientRect.height() - 40 < scaleHeight) {
        if (clientRect.height() - 40 < scaleHeightEx)
            scaleHeight = clientRect.height() - 40;
        else
            scaleHeight = scaleHeightEx;
        isMin = true;
    }

    bool overlap;
    do {
        overlap = false;
        float height = 0;
        for (int i = windowlist.size() - 1; i >= 0; i--) {
            EffectWindow *w = windowlist[i];
            QRect *target = &targets[w];
            float width = target->width();
            height = target->height();
            if (target->height() > scaleHeight) {
                float scale = (float)(scaleHeight / target->height());
                height = scaleHeight;
                width = target->width() * scale;
                if (width > totalWidth) {
                    scale = (float)(totalWidth) / (float)(target->width());
                    height = target->height() * scale;
                    width = totalWidth;
                }
            }

            totalw += width + 20;
            if (!isMin && totalw > clientRect.width()) {
                winLayout.clear();
                centerList.clear();
                scaleHeight = scaleHeightEx;
                overlap = true;
                isMin = true;
                totalw = 20;
                totalh = 20;
                index = 1;
                break;
            } else if (totalw > clientRect.width()) {
                xpos = ((clientRect.width() - totalw + width + 20) / 2) + 20 + clientRect.x();
                centerList.push_back(xpos);
                index ++;
                totalh += height + 20;
                totalw = 20;
                totalw += width + 20;
                winLayout[index].push_back(w);
            } else {
                winLayout[index].push_back(w);
            }
        }
        if (!overlap)
            totalh += height + 20;
    } while (overlap);  //calculation layout row

    xpos = ((clientRect.width() - totalw) / 2) + 20 + clientRect.x();
    centerList.push_back(xpos);
    totalw = 0;

    if (totalh < clientRect.height()) {
        ypos = (clientRect.height() - totalh) / 2 + clientRect.y();
    }

    for (int j = 0; j < winLayout.size(); j++) {
        float height = 0;
        for (int i = 0; i < winLayout[j + 1].size(); i++) {
            EffectWindow *w = winLayout[j + 1][i];
            QRect *target = &targets[w];
            height = target->height();
            float width = target->width();
            if (height > scaleHeight) {
                float scale = (float)(scaleHeight / height);
                width = width * scale;
                if (width > totalWidth) {
                    scale = (float)(totalWidth) / (float)(target->width());
                    height = height * scale;
                    width = totalWidth;
                } else {
                    height = scaleHeight;
                }
            } else if (width > totalWidth) {
                float scale = (float)(totalWidth) / (float)(target->width());
                height = height * scale;
                width = totalWidth;
            }

            target->setRect(centerList[j] + totalw, ypos, width, height);
            totalw += width + 20;
            m_previewWindowRects[w] = *target;
            motionManager.moveWindow(w, targets.value(w));
        }
        totalw = 0;
        ypos += height + 20;
    }
    m_previewRect = m_backgroundRect;
    m_previewRect.setHeight(totalh);
}

void SplitScreenEffect::createBackgroundFill(QString screen, bool isRecalculate, bool isUseTmp)
{
    QRect rect;
    QList<QuickTileMode> tileModes;
    if (m_splitMode == int(SplitMode::Two)) {
        tileModes.append(QuickTileFlag::Right);
        tileModes.append(QuickTileFlag::Left);
    } else if (m_splitMode == int(SplitMode::Three)) {
        bool isLeftHalf = false;
        if (m_quickTileMode == QuickTileMode(QuickTileFlag::Left)) {
            isLeftHalf = true;
        } else if (m_quickTileMode == QuickTileMode(QuickTileFlag::Right | QuickTileFlag::Top)) {
            isLeftHalf = true;
        } else if (m_quickTileMode == QuickTileMode(QuickTileFlag::Right | QuickTileFlag::Bottom)) {
            isLeftHalf = true;
        }
        if (isLeftHalf) {
            tileModes.append(QuickTileFlag::Left);
            tileModes.append(QuickTileFlag::Right | QuickTileFlag::Top);
            tileModes.append(QuickTileFlag::Right | QuickTileFlag::Bottom);
        } else {
            tileModes.append(QuickTileFlag::Left | QuickTileFlag::Top);
            tileModes.append(QuickTileFlag::Left | QuickTileFlag::Bottom);
            tileModes.append(QuickTileFlag::Right);
        }
    } else if (m_splitMode == int(SplitMode::Four)) {
        tileModes.append(QuickTileFlag::Left | QuickTileFlag::Top);
        tileModes.append(QuickTileFlag::Left | QuickTileFlag::Bottom);
        tileModes.append(QuickTileFlag::Right | QuickTileFlag::Top);
        tileModes.append(QuickTileFlag::Right | QuickTileFlag::Bottom);
    }

    for (auto mode : tileModes) {
        rect = getPreviewAreaGeometry(mode, screen, isRecalculate, isUseTmp);
        PreviewFill *fill = new PreviewFill(screen, m_screenRect.height());
        fill->setRect(rect, (int)mode);
        setPreviewFill((int)mode, fill);
    }
}

void SplitScreenEffect::slotStartShowMasking(QString screen, bool isTopDownMove)
{
    initData();
    int desktop = effects->currentDesktop();
    m_isTopDownMove = isTopDownMove;
    m_window = effects->activeWindow();
    effectsEx->getActiveSplitList(m_splitList, desktop, screen);

    if (!m_splitList.contains(m_window))
        m_window = effectsEx->findSplitGroupSecondaryClient(desktop, screen);

    m_screenRect = effects->clientArea(FullArea, m_window);
    m_pos = m_window->pos();
    m_cacheClient = static_cast<AbstractClient*>(static_cast<EffectWindowImpl*>(m_window)->window());
    m_quickTileMode = m_cacheClient->quickTileMode();
    m_splitMode = effectsEx->getSplitMode(desktop, screen);
    createBackgroundFill(screen);
    isShowMasking = true;
    removePreviewFill((int)m_quickTileMode);

    if (isTopDownMove && m_splitMode == int(SplitMode::Three)) {
        if (m_quickTileMode & QuickTileFlag::Top || m_quickTileMode & QuickTileFlag::Bottom) {
            if (m_quickTileMode & QuickTileFlag::Left) {
                removePreviewFill((int)QuickTileFlag::Right);
            } else {
                removePreviewFill((int)QuickTileFlag::Left);
            }
        }
        if (m_quickTileMode == (QuickTileMode)QuickTileFlag::Left || m_quickTileMode == (QuickTileMode)QuickTileFlag::Right) {
            m_window = effectsEx->findSplitGroupSecondaryClient(desktop, screen);
            if (m_window) {
                m_cacheClient = static_cast<AbstractClient*>(static_cast<EffectWindowImpl*>(m_window)->window());
                if (m_cacheClient)
                    removePreviewFill((int)m_cacheClient->quickTileMode());
            }
        }
    }
    effectsEx->raiseActiveWindow(desktop, screen);
    setActive(true);
}

void SplitScreenEffect::slotResizeMasking(int pos)
{
    bool isResizable = true;
    QRect activeClientResizeGeom;
    //adjust masking area
    QHash<int, QRect> resizeInfo;
    for (auto splitClient : m_splitList) {
        auto client = static_cast<AbstractClient*>(static_cast<EffectWindowImpl*>(splitClient)->window());
        if (!client)
            continue;
        QRect resizeGeom = client->moveResizeGeometry();
        QuickTileMode mode = client->quickTileMode();

        if (m_isTopDownMove) {
            if (mode & QuickTileFlag::Top) {
                resizeGeom = QRect(resizeGeom.x(), resizeGeom.y(), resizeGeom.width(), pos - 1 - m_maximizeArea.y());
            } else if (mode & QuickTileFlag::Bottom) {
                resizeGeom = QRect(resizeGeom.x(), pos + 1, resizeGeom.width(), m_maximizeArea.y() + m_maximizeArea.height() - (pos + 1));
            }
        } else {
            if (mode & QuickTileFlag::Left) {
                resizeGeom = QRect(resizeGeom.x(), resizeGeom.y(), pos - 1 - m_maximizeArea.x(), resizeGeom.height());
            } else if (mode & QuickTileFlag::Right) {
                resizeGeom = QRect(pos + 1, resizeGeom.y(), m_maximizeArea.x() + m_maximizeArea.width() - (pos + 1), resizeGeom.height());
            }
        }
        if (!client->checkResizable(resizeGeom.size())) {
            isResizable = false;
            resizeInfo.clear();
            break;
        }
        if (splitClient == m_window) {
            activeClientResizeGeom = resizeGeom;
        } else {
            resizeInfo[int(mode)] = resizeGeom;            
        }
    }
    if (!isResizable) {
        effectsEx->setSplitOutlinePos(pos, false);
        return;
    }
    effectsEx->setSplitOutlinePos(pos);
    if (m_cacheClient) {
        m_cacheClient->moveResize(activeClientResizeGeom);
        m_cacheClient->palette();
    }

    for (auto iter = resizeInfo.begin(); iter != resizeInfo.end(); iter++) {
        auto findIter = m_previewFill.find(iter.key());
        if (findIter == m_previewFill.end())
            continue;
        m_previewFill[iter.key()]->setRect(iter.value(), iter.key());
    }
}

void SplitScreenEffect::slotExitMasking()
{
    for (auto splitClient : m_splitList) {
        if (splitClient == m_window)
            continue;

        auto client = static_cast<AbstractClient*>(static_cast<EffectWindowImpl*>(splitClient)->window());
        QuickTileMode mode = client->quickTileMode();
        auto iter = m_previewFill.find(mode);
        if (iter == m_previewFill.end())
            continue;
        client->moveResize(iter.value()->getRect());
        client->palette();
    }

    setActive(false);
    lastPresentTime = std::chrono::milliseconds::zero();
    m_splitMode = 0;
    m_splitList.clear();
    m_previewFill.clear();
    m_quickTileMode = QuickTileMode(QuickTileFlag::None);
    m_cacheClient = nullptr;
    isShowMasking = false;
}

PreviewFill::PreviewFill(QString screen, int maxHeight)
    : m_screen(screen)
    , m_maxHeight(maxHeight)
{
    m_fillShader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture | ShaderTrait::Modulate, QString(), QStringLiteral(":/effects/splitscreen/shaders/previewfill.frag"));
    m_fillFrame = effects->effectFrame(EffectFrameUnstyled, false);

    ShaderBinder binder(m_fillShader);
    QColor color = effectsEx->getActiveColor();
    m_fillShader->setUniform(GLShader::Color, color);
    m_fillFrame->setShader(m_fillShader);
}

PreviewFill::~PreviewFill()
{
    if (m_fillFrame) {
        delete m_fillFrame;
        m_fillFrame = nullptr;
    }
    if (m_fillShader) {
        delete m_fillShader;
        m_fillShader = nullptr;
    }
}

void PreviewFill::render()
{
    m_fillFrame->render(infiniteRegion(), 1, 0);
}

void PreviewFill::setRect(QRect rect, int mode)
{
    m_rect = rect;
    if ((QuickTileMode)mode & QuickTileFlag::Top) {
        rect.adjust(0, 0, 0, -4);
    } else if ((QuickTileMode)mode & QuickTileFlag::Bottom) {
        rect.adjust(0, 5, 0, -4);
    }
    if ((QuickTileMode)mode & QuickTileFlag::Left) {
        rect.adjust(0, 0, -4, 0);
    } else if ((QuickTileMode)mode & QuickTileFlag::Right) {
        rect.adjust(5, 0, 0, 0);
    }
    m_fillFrame->setGeometry(rect);
}

} // namespace KWin
