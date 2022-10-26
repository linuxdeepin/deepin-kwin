/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SPLITSCREEN_H
#define KWIN_SPLITSCREEN_H

#include <deepin_kwineffects.h>
#include <deepin_kwineffectsex.h>
#include <abstract_client.h>

#include "scene.h"
#include "quit_effect.h"
#include <QHash>
//#include <Plasma/FrameSvg>

//#include <utils.h>

namespace KWin
{

class PreviewFill : public QObject
{
public:
    PreviewFill(QString screen, int maxHeight);
    ~PreviewFill();

    QRect getRect() {return m_rect;}
    void setRect(QRect rect, int mode);
    void render();
    QString getScreen() {return m_screen;}
private:
    EffectFrame *m_fillFrame;
    GLShader *m_fillShader;
    QRect m_rect;
    QString m_screen;
    int m_maxHeight;
};

class SplitScreenEffect : public Effect
{
    Q_OBJECT
public:
    SplitScreenEffect();
    ~SplitScreenEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;
    void windowInputMouseEvent(QEvent* e) override;
    void grabbedKeyboardEvent(QKeyEvent* e) override;
    void handleWheelEvent(Qt::MouseButtons btn);
    void relayDockEvent(QPoint pos, int button);

    bool isActive() const override;
    void setActive(bool active);
    bool isReceiveEvent();

    int requestedEffectChainPosition() const override {
        return 90;
    }

private Q_SLOTS:
    void slotWindowStartUserMovedResized(EffectWindow *w);
    void slotWindowQuickTileModeChanged(EffectWindow *w);
    void slotWindowFinishUserMovedResized(EffectWindow *w);
    void slotShowPreviewAlone(EffectWindow *w);

    //add masking
    void slotStartShowMasking(QString screen, bool isTopDownMove);
    void slotResizeMasking(int pos);
    void slotExitMasking();

    void onWindowAdded(EffectWindow *w);
    void onWindowDeleted(EffectWindow *w);
    void onWindowClosed(EffectWindow *w);

private:
    bool isEnterSplitMode(QuickTileMode mode);
    QRect getPreviewWindowsGeometry();
    QRect getPreviewAreaGeometry(QuickTileMode mode, QString screen, bool isRecalculate, bool isUseTmp = false);

    void cleanup();
    void preSetActive(EffectWindow *w);
    bool reLayout();
    void initData();

    bool isRelevantWithPresentWindows(EffectWindow *w) const;
    void calculateWindowTransformations(EffectWindowList windows, WindowMotionManager& wmm);
    void calculateWindowTransformationsClosest(EffectWindowList windowlist, int screen,
            WindowMotionManager& motionManager);

    void getTwoSplitQuickmatch();
    void getThreeSplitQuickmatch();
    void getFourSplitQuickmatch();
    void updateSplitLocationFromMenu(QuickTileMode mode, bool isClear = false);
    void showScroll();

private:
    void setPreviewFill(int mode, PreviewFill *fill) {
        m_previewFill[mode] = fill;
    }

    void removePreviewFill(int mode) {
        delete m_previewFill[mode];
        m_previewFill[mode] = nullptr;
        m_previewFill.remove(mode);
    }

    void createBackgroundFill(QString screen, bool isRecalculate = true, bool isUseTmp = false);

private:
    EffectWindow     *targetTouchWindow = nullptr;
    EffectWindow     *m_window = nullptr;
    EffectWindow     *m_hoverwin = nullptr;
    EffectWindow     *m_dock = nullptr;
    EffectFrame      *m_highlightFrame = nullptr;
    EffectFrame      *m_scrollFrame = nullptr;
    AbstractClient   *m_cacheClient = nullptr;
    GLShader         *m_splitthumbShader = nullptr;
    GLShader         *m_splitscrollShader = nullptr;

    QRect               m_dockRect;
    QRect               m_screenRect;
    QRect               m_backgroundRect;
    QRect               m_previewRect;
    QRect               m_scrollRect;
    QRect               m_scrollStartRect;
    QRect               m_maximizeArea;
    QPoint              m_pos;
    bool                m_activated = false;
    bool                m_hasKeyboardGrab = false;
    bool                isExitSplitScreen = false;
    bool                m_isPressScroll = false;
    bool                m_isWheelScroll = false;
    bool                isShowMasking = false;
    bool                m_isTopDownMove = false;
    bool                m_startEffect = false;
    int                 m_backgroundMode;
    int                 m_splitMode = 0;
    int                 m_scrollStartPosY = 0;
    int                 m_scrollMoveDistance = 0;
    int                 m_scrollMoveStart = 0;
    int                 m_scrollStatus = 0;     // 1 hover; 2 press;
    float                 m_scrollStep = 0;

    QString             m_screen;

    QuickTileMode        m_quickTileMode = QuickTileMode(QuickTileFlag::None);
    SplitLocationMode    m_previewLocation = SplitLocationMode(SplitLocation::None);

    QSet<KWin::EffectWindow *>      m_splitList;
    QList<EffectWindow *>           m_unminWinlist;
    QVector<WindowMotionManager>    m_motionManagers;
    QHash<EffectWindow*, QRectF>    m_previewWindowRects;
    QHash<int, PreviewFill *>       m_previewFill;

    std::chrono::milliseconds       lastPresentTime;
    SplitScreenExitEffect           m_effectExit;
    Qt::MouseButton                 m_sendDockButton = Qt::NoButton;
    QPoint                          m_cursorPos;
    int                             m_buttonType = 0;
};

} // namespace KWin

#endif
