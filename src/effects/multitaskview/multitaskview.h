/*
 * Copyright (C) 2022 ~ 2022 Deepin Technology Co., Ltd.
 *
 * Author:     zhangyu <zhangyud@uniontech.com>
 *
 * Maintainer: zhangyu <zhangyud@uniontech.com>
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

#ifndef KWIN_MULTITASKVIEW_H
#define KWIN_MULTITASKVIEW_H

#include "window.h"
#include "kwinglutils.h"
#include "scene/scene.h"
#include "multitask_effect.h"
#include "kwineffectsex.h"
#include <QHash>
#include <QMutex>
#include <map>
#include <QSettings>
#include "multitaskmodel.h"

namespace KWin
{

class EffectFrameEx;
class WindowMotionManager;

enum workspaceMoveDirection {
    mvNone      = 0x00,
    mvLeft      = 0x01,
    mvRight     = 0x02,
};

enum workspaceDragDirection {
    dragNone      = 0x00,
    dragUpDown    = 0x01,
    dragLeftRight = 0x02,
};

class WorkspaceThumbnail : public QObject
{
    Q_OBJECT
public:
    WorkspaceThumbnail(bool flag=false);
    ~WorkspaceThumbnail();

    void renderDesktopBackGround(float k);
    void renderWorkspaceBackGround(float t, int desktop = 1);

    void setImage(const QPixmap &bgPix, const QPixmap &wpPix, const QRect &rect);
    void setRect(const QRect rect);
    QRect getRect() {return m_rect;}
    QRect getCurrentRect() {return m_currentRect;}
    QRect getGeometry();
    void updateGeometry(QRect rect);
    void setMaxHeight(int height) {m_maxHeight = height;}
    void setArea(QRect &rect, QRect &fullrect) {m_clientArea = rect; m_fullArea = fullrect;}
    QRect getClientArea() {return m_clientArea;}
    QRect getfullArea() {return m_fullArea;}
    void setScreenDesktop(EffectScreen *screen, int desktop) { m_screen = screen; m_desktop = desktop;}
    EffectScreen *screen() {return m_screen;}

    int desktop() {return m_desktop;}
    void setPosition(QPoint pos);

private:
    std::unique_ptr<EffectFrameEx> m_backGroundFrame;
    std::unique_ptr<EffectFrameEx> m_workspaceBgFrame;
    std::unique_ptr<EffectFrameEx> m_hoverFrame;
    int m_desktop;
    EffectScreen *m_screen;
    int m_maxHeight;
    QRect m_rect;
    QRect m_currentRect;
    QRect m_clientArea;
    QRect m_fullArea;
    QString m_image;

public:
    workspaceMoveDirection m_posStatus = mvNone;
};

class WindowFill : public QObject
{
public:
    WindowFill(EffectScreen *screen, QRect rect, float radius, int maxHeight);
    ~WindowFill();

    QRect getRect() {return m_rect;}
    void render();
private:
    std::unique_ptr<EffectFrameEx> m_fillFrame;
    QRect m_rect;
    EffectScreen *m_screen;
    int m_maxHeight;
};

typedef struct scaleSt {
    float spacingHeight;
    float spacingWidth;
    float workspacingWidth;
    float workspaceMgrHeight;
    float workspaceWidth;
    float workspaceHeight;
    QRect workspaceMgrRect;
    QRect windowMgrRect;
    QRect fullArea;
} Scale_st;

class WindowLayoutManager : public QObject
{
    Q_OBJECT
public:
    explicit WindowLayoutManager();
    ~WindowLayoutManager() override;

    void clearMotion();

    EffectWindowList orderManagedWindows(int desktop) const;
    EffectWindowList orderManagedWindows(int desktop, EffectScreen *screen) const;

    void manageWin(EffectScreen *screen, EffectWindow *w, int desktop = 1);
    void removeWin(EffectScreen *screen, EffectWindow *w);
    bool getMotion(int desktop, EffectScreen *screen, WindowMotionManager *&mgr);

    int getDesktopWinNum(int desktop) const;
    int getDesktopWinNumofScreen(EffectScreen *screen) const;
    void resetWindow();
    void endMotion();
    bool animating() const;

    void setdesktop(int desktop) { m_desktop = desktop; }
    void setWinManager(EffectScreen *screen, WindowMotionManager &wm);
    void calculate(int tm, bool motionEffectEnable);

    void updatePos(EffectScreen *screen, QRect rect, QPoint diff);
    EffectWindow *getHoverWin(QPoint pos, EffectScreen *screen = nullptr);
    QRect getWindowGeometry(EffectWindow *w, EffectScreen *screen);
    EffectWindowList getDesktopWinList();
    void getDesktopWinInfo(QHash<EffectScreen *, WindowMotionManager> &minfo);

    bool isHaveWinFill(EffectWindow *w);
    void setWinFill(EffectWindow *w, WindowFill *fill);
    WindowFill *getWinFill(EffectWindow *w);
    void removeWinFill(EffectWindow *w);

private:
    QHash<EffectScreen *, WindowMotionManager *> m_winManager;
    QHash<EffectWindow *, WindowFill *> m_windowFill;
    int m_desktop;
    QRect m_currentShowRect;
};

typedef QHash<EffectScreen *, WindowMotionManager *> WindowManagerHash;

void calculateWindowTransformations(EffectWindowList windows, WindowMotionManager& wmm, int desktop,
    EffectScreen *screen, const Scale_st &scale, float radius,
    WindowLayoutManager *layoutManager, int maxHeight,
    MultiTaskEffectFlyingBack &effectFlyingBack, MultiTaskWindowEffect &windowEffect,
    const EffectWindowList &flyingWinList, bool isReLayout = false);
void calculateWindowTransformationsClosest(EffectWindowList windowlist, int desktop,
    EffectScreen *screen, WindowMotionManager& motionManager,
    const Scale_st &scale, float radius,
    WindowLayoutManager *layoutManager, int maxHeight,
    MultiTaskEffectFlyingBack &effectFlyingBack, MultiTaskWindowEffect &windowEffect,
    const EffectWindowList &flyingWinList, bool isReLayout = false);
void calculateWorkSpaceWinTransformations(EffectWindowList windows, WindowMotionManager &wm, int desktop,
    const QHash<EffectScreen *, QList<WorkspaceThumbnail *>> &workspaceBackgrounds,
    int deleteWorkspaceDesktop = -1);
void precalculateWorkSpaceWinTransformations(EffectWindowList windows, WindowMotionManager &wm, int desktop,
    const QHash<EffectScreen *, QList<WorkspaceThumbnail *>> &workspaceBackgrounds);

class AddWorkspaceButton : public QObject
{
    Q_OBJECT
public:
    AddWorkspaceButton();
    ~AddWorkspaceButton();

    void render();
    void setImage(const QString &btf, const QRect &rect);
    void setMaxHeight(int height) { m_maxHeight = height; }
    void setArea(QRect &rect, QRect &fullrect) { m_clientArea = rect; m_fullArea = fullrect; }
    void setRect(const QRect &rect);
    const QRect &getRect() const { return m_rect; }

private:
    std::unique_ptr<EffectFrameEx> m_button;
    int m_maxHeight;
    int m_scale;
    QRect m_rect;
    QRect m_clientArea;
    QRect m_fullArea;
};

typedef struct screenNameSt {
    QString       name;
    QRect         rect;
    QRect         screenrect;
    EffectScreen *screen;
} ScreenInfo_st;

class MultitaskViewEffect : public Effect
{
    Q_OBJECT
public:
    MultitaskViewEffect();
    virtual ~MultitaskViewEffect() override;

    void reconfigure(ReconfigureFlags flags) override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;

    void postPaintScreen() override;

    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data) override;

    void windowInputMouseEvent(QEvent* e) override;
    void grabbedKeyboardEvent(QKeyEvent* e) override;

    bool isActive() const override;
    void setActive(bool active);
    bool isReceiveEvent();
    QPoint pointerPos();

    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;

    void setMotionEffect(bool enable) { m_motionEffectEnable = enable; }
    bool motionEffectEnable() const { return m_motionEffectEnable; }

public:
    enum workspaceStatus {
        wpNone      = 0x00,
        wpSwitch    = 0x01,
        wpRestore   = 0x02,
        wpDelete    = 0x04,
    };

    struct TouchState {
        qint32 id = 0;
        bool active = false;
        bool isMotion = false;
        QPointF pos;
        bool isPress = false;
    };

    QPoint cursorPos() const { return m_cursorPos; }
    void setCursorPos(QPoint p) { m_cursorPos = p; }
    int buttonType() const { return m_buttonType; }
    void setButtonType(int v) { m_buttonType = v; }
    QList<EffectWindow *> &notificationList() { return m_notificationList; }
    EffectWindow *windowMove() const { return m_windowMove; }
    void setWindowMove(EffectWindow *w) { m_windowMove = w; }
    EffectWindow *dock() const { return m_dock; }
    void setDock(EffectWindow *w) { m_dock = w; }
    EffectWindow *hoverWinBtn() const { return m_hoverWinBtn; }
    void setHoverWinBtn(EffectWindow *w) { m_hoverWinBtn = w; }
    EffectWindow *hoverWin() const { return m_hoverWin; }
    void setHoverWin(EffectWindow *w) { m_hoverWin = w; }
    EffectFrameEx *hoverWinFrame() const { return m_hoverWinFrame.get(); }
    void setHoverWinFrame(std::unique_ptr<EffectFrameEx> f) { m_hoverWinFrame = std::move(f); }
    EffectFrameEx *closeWinFrame() const { return m_closeWinFrame.get(); }
    void setCloseWinFrame(std::unique_ptr<EffectFrameEx> f) { m_closeWinFrame = std::move(f); }
    EffectFrameEx *topWinFrame() const { return m_topWinFrame.get(); }
    void setTopWinFrame(std::unique_ptr<EffectFrameEx> f) { m_topWinFrame = std::move(f); }
    EffectFrameEx *textWinFrame() const { return m_textWinFrame.get(); }
    void setTextWinFrame(std::unique_ptr<EffectFrameEx> f) { m_textWinFrame = std::move(f); }
    EffectFrameEx *textWinBgFrame() const { return m_textWinBgFrame.get(); }
    void setTextWinBgFrame(std::unique_ptr<EffectFrameEx> f) { m_textWinBgFrame = std::move(f); }
    EffectFrameEx *previewFrame() const { return m_previewFrame.get(); }
    void setPreviewFrame(std::unique_ptr<EffectFrameEx> f) { m_previewFrame = std::move(f); }
    EffectFrameEx *closeWorkspaceFrame() const { return m_closeWorkspaceFrame.get(); }
    void setCloseWorkspaceFrame(std::unique_ptr<EffectFrameEx> f) { m_closeWorkspaceFrame = std::move(f); }
    EffectFrameEx *dragTipsFrame() const { return m_dragTipsFrame.get(); }
    void setDragTipsFrame(std::unique_ptr<EffectFrameEx> f) { m_dragTipsFrame = std::move(f); }
    EffectFrameEx *dragTipsFrameShadow() const { return m_dragTipsFrameShadow.get(); }
    void setDragTipsFrameShadow(std::unique_ptr<EffectFrameEx> f) { m_dragTipsFrameShadow = std::move(f); }
    bool isActivated() const { return m_activated; }
    void setActivated(bool v) { m_activated = v; }
    bool isShieldEvent() const { return m_isShieldEvent; }
    void setIsShieldEvent(bool v) { m_isShieldEvent = v; }
    bool hasKeyboardGrab() const { return m_hasKeyboardGrab; }
    void setHasKeyboardGrab(bool v) { m_hasKeyboardGrab = v; }
    bool wasWorkspaceMove() const { return m_wasWorkspaceMove; }
    void setWasWorkspaceMove(bool v) { m_wasWorkspaceMove = v; }
    bool isShowPreview() const { return m_isShowPreview; }
    void setIsShowPreview(bool v) { m_isShowPreview = v; }
    bool wasWindowMove() const { return m_wasWindowMove; }
    void setWasWindowMove(bool v) { m_wasWindowMove = v; }
    bool delayDbus() const { return m_delayDbus; }
    void setDelayDbus(bool v) { m_delayDbus = v; }
    bool longPressTouch() const { return m_longPressTouch; }
    void setLongPressTouch(bool v) { m_longPressTouch = v; }
    QPoint workspaceMoveStartPos() const { return m_workspaceMoveStartPos; }
    void setWorkspaceMoveStartPos(QPoint p) { m_workspaceMoveStartPos = p; }
    QPoint windowMoveStartPos() const { return m_windowMoveStartPos; }
    void setWindowMoveStartPos(QPoint p) { m_windowMoveStartPos = p; }
    QPoint windowMoveDiff() const { return m_windowMoveDiff; }
    void setWindowMoveDiff(QPoint p) { m_windowMoveDiff = p; }
    QPoint lastWorkspaceMovePos() const { return m_lastWorkspaceMovePos; }
    void setLastWorkspaceMovePos(QPoint p) { m_lastWorkspaceMovePos = p; }
    QHash<EffectScreen *, AddWorkspaceButton *> &addWorkspaceButton() { return m_addWorkspaceButton; }
    QHash<EffectScreen *, QList<WorkspaceThumbnail *>> &workspaceBackgrounds() { return m_workspaceBackgrounds; }
    QRect &dockRect() { return m_dockRect; }
    const QRect &dockRect() const { return m_dockRect; }
    void setDockRect(QRect r) { m_dockRect = r; }
    QRect &windowMoveGeometry() { return m_windowMoveGeometry; }
    const QRect &windowMoveGeometry() const { return m_windowMoveGeometry; }
    void setWindowMoveGeometry(QRect r) { m_windowMoveGeometry = r; }
    QRect allFullArea() const { return m_allFullArea; }
    void setAllFullArea(QRect r) { m_allFullArea = r; }
    EffectScreen *screen() const { return m_screen; }
    void setScreen(EffectScreen *s) { m_screen = s; }
    int &aciveMoveDesktop() { return m_aciveMoveDesktop; }
    int aciveMoveDesktop() const { return m_aciveMoveDesktop; }
    void setAciveMoveDesktop(int v) { m_aciveMoveDesktop = v; }
    int &hoverDesktop() { return m_hoverDesktop; }
    int hoverDesktop() const { return m_hoverDesktop; }
    void setHoverDesktop(int v) { m_hoverDesktop = v; }
    int moveWorkspaceNum() const { return m_moveWorkspaceNum; }
    void setMoveWorkspaceNum(int v) { m_moveWorkspaceNum = v; }
    qreal scalingFactor() const { return m_scalingFactor; }
    void setScalingFactor(qreal v) { m_scalingFactor = v; }
    QHash<EffectScreen *, Scale_st> &scale() { return m_scale; }
    QVector<QRect> &winBtnArea() { return m_winBtnArea; }
    QRect workspaceCloseBtnArea() const { return m_workspaceCloseBtnArea; }
    void setWorkspaceCloseBtnArea(QRect r) { m_workspaceCloseBtnArea = r; }
    workspaceStatus workspaceStatusVal() const { return m_workspaceStatus; }
    void setWorkspaceStatus(workspaceStatus v) { m_workspaceStatus = v; }
    workspaceDragDirection dragWorkspacedirection() const { return m_dragWorkspacedirection; }
    void setDragWorkspacedirection(workspaceDragDirection v) { m_dragWorkspacedirection = v; }
    workspaceMoveDirection moveWorkspacedirection() const { return m_moveWorkspacedirection; }
    void setMoveWorkspacedirection(workspaceMoveDirection v) { m_moveWorkspacedirection = v; }
    TouchState &touch() { return m_touch; }
    QTimer *timer() const { return m_timer; }
    bool isScreenRecorder() const { return m_isScreenRecorder; }
    void setIsScreenRecorder(bool v) { m_isScreenRecorder = v; }
    bool isCloseScreenRecorder() const { return m_isCloseScreenRecorder; }
    void setIsCloseScreenRecorder(bool v) { m_isCloseScreenRecorder = v; }
    EffectWindow *screenRecorderMenu() const { return m_screenRecorderMenu; }
    void setScreenRecorderMenu(EffectWindow *w) { m_screenRecorderMenu = w; }
    MultitaskViewModel *model() const { return m_model; }
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(QString s) { m_fontFamily = s; }
    Qt::MouseButton sendButton() const { return m_sendButton; }
    void setSendButton(Qt::MouseButton b) { m_sendButton = b; }
    MultiTaskEffectFlyingBack &effectFlyingBack() { return m_effectFlyingBack; }
    QString previewFile() const { return m_previewFile; }
    void setPreviewFile(QString s) { m_previewFile = s; }
    bool previewLoading() const { return m_previewLoading; }
    void setPreviewLoading(bool v) { m_previewLoading = v; }
    int previewFramePosX() const { return m_previewFramePosX; }
    void setPreviewFramePosX(int v) { m_previewFramePosX = v; }
    bool opacityStatus() const { return m_opacityStatus; }
    void setOpacityStatus(bool v) { m_opacityStatus = v; }
    QTimer &addingDesktopTimer() { return m_addingDesktopTimer; }
    const QList<QKeySequence> &shortcutKeys() const { return shortcut; }
    const QList<QKeySequence> &shortcutAKeys() const { return shortcuta; }
    const QList<QKeySequence> &shortcutWKeys() const { return shortcutw; }

public Q_SLOTS:
    void toggle();
    void screenRecorderStart();

public:
    void cleanup();
    bool isRelevantWithPresentWindows(EffectWindow *w) const;
    QRect calculateWorkspaceRect(int index, EffectScreen *screen, QRect maxRect, int customDesktopCount = 0);
    bool calculateSwitchPos(QPoint diffPoint);
    void restoreWorkspacePos(int index, int num);

    void initWorkspaceBackground();
    void applyWorkspaceBg(WorkspaceThumbnail *b, ScreenInfo_st &info, int desktop);
    void invalidateWorkspaceCache();
    void updateWorkspacePos();
    void updateWorkspacePos(int removedesktop);
    void getScreenInfo();
    void setWinLayout(int desktop, const EffectWindowList &windows);
    void updateWorkspaceWinLayout(int numDesktops);
    void updateWorkspaceWinLayout(int numDesktops, int desktop);
    void workspaceWinRelayout(int desktop, EffectScreen *screen = nullptr);
    void removeWinAndRelayout(EffectWindow *w);
    void createBackgroundFill(EffectWindow *w, QRect rect, float radius, int desktop);
    void removeBackgroundFill(EffectWindow *w, int desktop);
    void addNewDesktop();
    void removeDesktop(int desktop);
    void removeDesktopEx(int desktop);
    void startremoveDesktopEffect(int desktop);
    void cleanDesktopData(int desktop);
    void switchDesktop();
    void desktopSwitchPosition(int to, int from);
    bool checkConfig(EffectWindow *w);
    bool isExtensionMode() const;
    int getNumScreens();
    QVector<int>desktopList(const EffectWindow *w) const;

    void renderWorkspaceMove(KWin::ScreenPaintData &data);
    void renderWindowMove(KWin::ScreenPaintData &data);
    void renderSlidingWorkspace(WorkspaceThumbnail *wkobj, EffectScreen *screen, int desktop, KWin::ScreenPaintData &data);
    void renderHover(const EffectWindow *w, const QRect &rect, int order = 0, float radius = 0.0);
    void handlerAfterTimeLine();
    void setWinKeepAbove(EffectWindow *w);
    void changeCurrentDesktop(int desktop);

    bool checkHandlerWorkspace(QPoint pos, EffectScreen *screen, int &desktop);
    void moveWindowChangeDesktop(EffectWindow *w, int todesktop, EffectScreen *toscreen, bool isSwitch = false);
    bool closeWindow(EffectWindow *w);
    void initShowWholeStatusAsync();
    void loadShowWholeConfig(const QString &path);
    void updateShowWholeStatus(const QString& type);

    // Input handling methods (merged from InputHandling)
    void handlerWheelEvent(Qt::MouseButtons buttons);
    void relayDockEvent(QPoint pos, int button);
    void onWindowAdded(EffectWindow *w);
    void onWindowDeleted(EffectWindow *w);
    void onWindowClosed(EffectWindow *w);
    void onCloseEffect(bool isSleepBefore);
    void onDockChange(const QString &key);
    void fontChanged(const QString &fontType, const QString &fontName);

    // UI controls methods (merged from UIControls)
    void createEffectFrame();
    void showWorkspacePreview(EffectScreen *screen, QRect addButtonRect, bool isClear = false);
    void renderWorkspaceHover(EffectScreen *screen);
    void renderDragWorkspacePrompt(EffectScreen *screen);
    void updateWorkspaceBg(const QString &screenName, int desktop, const QPixmap &bgOverride = {});

public:
    WorkspaceThumbnail *getWorkspaceObject(EffectScreen *screen, int secindex);
    WindowLayoutManager *getWinManagerObject(int index);
    WindowLayoutManager *getWorkspaceWinManagerObject(int index);
    EffectWindow *getNextWindow(EffectWindow *w);
    EffectWindow *getPreWindow(EffectWindow *w);
    EffectWindow *getHomeOrEndWindow(bool);
    EffectWindow *getNextSameTypeWindow(EffectWindow *w);
    EffectWindow *getPreSameTypeWindow(EffectWindow *w);
    void motionRepeat();

private:
    void onRowsInserted(const QModelIndex &parent, int first, int last);
    void drawDottedLine(const QRect &geo, EffectScreen *screen);
    QPoint m_cursorPos{-1, -1};
    int m_buttonType{0};

    QAction *m_showActions = nullptr;
    QAction *m_showActionw = nullptr;
    QAction *m_showActiona = nullptr;
    QList<QKeySequence> shortcut;
    QList<QKeySequence> shortcutw;
    QList<QKeySequence> shortcuta;
    QList<EffectWindow *> m_notificationList;

    EffectWindow *m_windowMove = nullptr;
    EffectWindow *m_dock = nullptr;
    EffectWindow *m_hoverWinBtn = nullptr;
    EffectWindow *m_hoverWin = nullptr;
    std::unique_ptr<EffectFrameEx> m_hoverWinFrame = nullptr;
    std::unique_ptr<EffectFrameEx> m_closeWinFrame = nullptr;
    std::unique_ptr<EffectFrameEx> m_topWinFrame = nullptr;
    std::unique_ptr<EffectFrameEx> m_textWinFrame = nullptr;
    std::unique_ptr<EffectFrameEx> m_textWinBgFrame = nullptr;
    std::unique_ptr<EffectFrameEx> m_previewFrame = nullptr;
    std::unique_ptr<EffectFrameEx> m_closeWorkspaceFrame = nullptr;
    std::unique_ptr<EffectFrameEx> m_dragTipsFrame = nullptr;
    std::unique_ptr<EffectFrameEx> m_dragTipsFrameShadow = nullptr;

    bool m_activated = false;
    bool m_isShieldEvent = false;
    bool m_hasKeyboardGrab = false;
    bool m_wasWorkspaceMove = false;
    bool m_isShowWhole = true;
    bool m_isShowWin = true;
    bool m_isShowPreview = false;
    bool m_wasWindowMove = false;

    bool m_delayDbus = true;
    bool m_longPressTouch = false;
    bool m_motionEffectEnable = false;

    QPoint m_workspaceMoveStartPos;
    QPoint m_windowMoveStartPos;
    QPoint m_windowMoveDiff;
    QPoint m_lastWorkspaceMovePos;

    EffectWindowList                     m_flyingWinList;
    QHash<EffectScreen *, WindowMotionManager>      m_windowInfo;
    std::map<EffectScreen *, std::vector<std::unique_ptr<EffectFrameEx>>> m_tipFrames;
    QHash<EffectScreen *, AddWorkspaceButton *>      m_addWorkspaceButton;
    QHash<QString, ScreenInfo_st>        m_screenInfoList;
    QHash<EffectScreen *, QList<WorkspaceThumbnail *>> m_workspaceBackgrounds;
    QVector<WindowLayoutManager *>       m_motionManagers;
    QVector<WindowLayoutManager *>       m_workspaceWinMgr;
    QRect m_backgroundRect;
    QRect m_dockRect;
    QRect m_windowMoveGeometry;
    QRect m_allFullArea;

    EffectScreen *m_screen = nullptr;
    int m_aciveMoveDesktop = -1;
    int m_deleteWorkspaceDesktop = -1;
    int m_hoverDesktop = -1;
    int m_passiveMoveDesktop = -1;
    int m_moveWorkspaceNum = -1;
    int m_maxHeight = 0;
    qreal m_scalingFactor = 1;

    QHash<EffectScreen *, Scale_st> m_scale;
    QVector<QRect>    m_winBtnArea;
    QRect                m_workspaceCloseBtnArea;

    int             paintingDesktop = 0;
    workspaceStatus m_workspaceStatus = wpNone;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QMutex          m_mutex;
#else
    QRecursiveMutex m_mutex;
#endif

    TimeLine     m_bgSlidingTimeLine;
    bool         m_bgSlidingStatus;
    int          m_curDesktopIndex;
    int          m_lastDesktopIndex;
    int          m_bgSlidingState = 0;
    bool         m_isRemoveWorkspace = false;
    bool         m_windowEffectState = false;

    TimeLine     m_popTimeLine;
    bool         m_popStatus;

    TimeLine     m_opacityTimeLine;
    bool         m_opacityStatus;

    bool m_workspaceSlidingStatus;
    TimeLine m_workspaceSlidingTimeline;
    std::map <WorkspaceThumbnail*, std::pair<int,int> > m_workspaceSlidingInfo;
    int m_previewFramePosX;
    int m_workspaceSlidingState = 0;

    MultiTaskEffectFlyingBack m_effectFlyingBack;
    MultiTaskWindowEffect     m_windowEffect;

    workspaceMoveDirection m_moveWorkspacedirection = mvNone;
    workspaceDragDirection m_dragWorkspacedirection = dragNone;
    float                  m_radius = 0.0;

    QTimer m_addingDesktopTimer;

    TouchState m_touch;
    QTimer *m_timer;
    bool m_isScreenRecorder = false;
    bool m_isCloseScreenRecorder = false;
    EffectWindow *m_screenRecorderMenu = nullptr;

    MultitaskViewModel *m_model = nullptr;
    bool m_pendingRender = false;
    bool m_workspaceCacheValid = false;
    bool m_isCleanedUp = false;
    bool m_addingDesktop = false;
    bool m_previewLoading = false;
    QString m_previewFile;

    QString m_fontFamily;
    Qt::MouseButton m_sendButton = Qt::NoButton;
};

} // namespace KWin

#endif
