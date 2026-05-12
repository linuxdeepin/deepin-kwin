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

#include "multitaskview.h"
#include <QtCore>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QtMath>
#include <QAction>
#include <QScreen>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QX11Info>
#else
#include <private/qtx11extras_p.h>
#endif
#include <QDBusConnection>
#include <QFontMetrics>
#include <QSet>
#include <QTranslator>

#include "workspace.h"
#include "eventlog.h"
#include "multitaskmodel.h"
#include <QGSettings>
#include <QImageReader>
#include <QSharedPointer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <effects.h>
#include <kglobalaccel.h>
#include <qdbusconnection.h>
#include <qdbusinterface.h>
#include <qdbusreply.h>

//#include "multitouchgesture.h"       //to do

Q_GLOBAL_STATIC_WITH_ARGS(QGSettings, _gsettings_dde_dock, ("com.deepin.dde.dock"))
//Q_GLOBAL_STATIC_WITH_ARGS(QGSettings, _gsettings_dde_dock_primary, ("com.deepin.dde.dock.mainwindow"))
Q_GLOBAL_STATIC_WITH_ARGS(QGSettings, _gsettings_dde_appearance, ("com.deepin.dde.appearance"))

#define GsettingsDockPosition   "position"
//#define GsettingsDockBottom     "bottom"
//#define GsettingsDockPrimary    "only-show-primary"
#define GsettingsDockHeight     "window-size-efficient"
//#define GsettingsDockShow       "hide-mode"

#define BRIGHTNESS  0.4
#define SCALE_F     1.0
#define SCALE_S     2.0
#define WINDOW_W_H  300
#define MOUSE_MOVE_MIN_DISTANCE 2

#define MAX_DESKTOP_COUNT   6

#define FIRST_WIN_SCALE     (float)(720.0 / 1080.0)
#define WORKSPACE_SCALE     (float)(240.0 / 1920.0)
#define WORK_SPACING_SCALE  (float)(40.0 / 1920.0)
#define SPACING_H_SCALE     (float)(20.0 / 1080.0)
#define SPACING_W_SCALE     (float)(20.0 / 1920.0)
#define ADDBTN_W_SCALE      (float)(104.0 / 1920.0)
#define ADDBTN_H_SCALE      (float)(56.0 / 1080.0)
#define ADDBTN_SIZE_SCALE   (float)(64.0 / 1920.0)
#define ADDBTN_RADIUS_SCALE (float)(18.0 / 1920.0)

#ifdef BUILD_ON_V25
#define DBUS_APPEARANCE_SERVICE "org.deepin.dde.Appearance1"
#define DBUS_APPEARANCE_OBJ "/org/deepin/dde/Appearance1"
#define DBUS_APPEARANCE_INTF "org.deepin.dde.Appearance1"
#define DBUS_IMAGEEFFECT_SERVICE "org.deepin.dde.ImageEffect1"
#define DBUS_BLUR_OBJ "/org/deepin/dde/ImageBlur1"
#define DBUS_BLUR_INTF "org.deepin.dde.ImageBlur1"
#else
#define DBUS_APPEARANCE_SERVICE "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF "com.deepin.daemon.Appearance"
#define DBUS_IMAGEEFFECT_SERVICE "com.deepin.daemon.ImageEffect"
#define DBUS_BLUR_OBJ "/com/deepin/daemon/ImageBlur"
#define DBUS_BLUR_INTF "com.deepin.daemon.ImageBlur"
#endif



#define MULTITASK_CLOSE_SVG      ":/effects/multitaskview/buttons/multiview_delete.svg"
#define MULTITASK_TOP_SVG        ":/effects/multitaskview/buttons/multiview_top.svg"
#define MULTITASK_TOP_ACTIVE_SVG ":/effects/multitaskview/buttons/multiview_top_active.svg"

#define POPUP_TIME_SCALE 15.0f
#define EFFECT_DURATION_DEFAULT 300
#define SCISSOR_HOFFU 200
#define SCISSOR_HOFFD 400

// DCONFIG
#define CONFIGMANAGER_SERVICE   "org.desktopspec.ConfigManager"
#define CONFIGMANAGER_INTERFACE "org.desktopspec.ConfigManager"
#define CONFIGMANAGER_MANAGER_INTERFACE "org.desktopspec.ConfigManager.Manager"

#define GsettingsBackgroundUri "backgroundUris"
const char notification_tips[] = "dde-osd dde-osd";
const char screen_recorder[] = "deepin-screen-recorder deepin-screen-recorder";
const char split_outline[] = "kwin_x11 kwin";
const char fallback_background_name[] = "file:///usr/share/wallpapers/deepin/desktop.jpg";
const char previous_default_background_name[] = "file:///usr/share/backgrounds/default_background.jpg";
const char add_workspace_png[] = ":/effects/multitaskview/buttons/add-light.png";
const char delete_workspace_png[] = ":/effects/multitaskview/buttons/workspace_delete.svg";//":/resources/themes/add-light.svg";
const char defaultSecondBackgroundUri[] = "francesco-ungaro-1fzbUyzsHV8-unsplash";

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(multitaskview);
}

namespace KWin
{
QPoint calculateOffSet(const QRect& rect, const QRect &screenArea, const int maxHeight)
{
    QPoint point;
    if (!QX11Info::isPlatformX11()) {
        point.setX(rect.x() - screenArea.x());
        point.setY(screenArea.height() - (rect.y() - screenArea.y()) - rect.height());
        return point;
    }
    point.setX(rect.x());
    point.setY(maxHeight - rect.y() - rect.height());

    return point;
}

MultitaskViewEffect::MultitaskViewEffect()
    : m_showActions(new QAction(this))
    , m_showActionw(new QAction(this))
    , m_showActiona(new QAction(this))
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    , m_mutex(QMutex::Recursive)
#endif
    , m_timer(new QTimer(this))
{
    QAction *s = m_showActions;
    s->setObjectName(QStringLiteral("ShowMultitasking"));
    s->setText("Show Multitasking View");
    KGlobalAccel::self()->setDefaultShortcut(s, QList<QKeySequence>() << Qt::META + Qt::Key_S);
    KGlobalAccel::self()->setShortcut(s, QList<QKeySequence>() << Qt::META + Qt::Key_S);
    shortcut = KGlobalAccel::self()->shortcut(s);

    QAction *w = m_showActionw;
    w->setObjectName(QStringLiteral("ShowMultitaskingW"));
    w->setText("Show Multitasking View");
    KGlobalAccel::self()->setDefaultShortcut(w, QList<QKeySequence>() << Qt::META + Qt::Key_W);
    KGlobalAccel::self()->setShortcut(w, QList<QKeySequence>() << Qt::META + Qt::Key_W);
    shortcutw = KGlobalAccel::self()->shortcut(w);

    QAction *a = m_showActiona;
    a->setObjectName(QStringLiteral("ShowMultitaskingA"));
    a->setText("Show Multitasking View");
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_A);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_A);
    shortcuta = KGlobalAccel::self()->shortcut(a);

    connect(s, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    connect(w, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggle()));

    connect(effects, &EffectsHandler::windowAdded, this, &MultitaskViewEffect::onWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &MultitaskViewEffect::onWindowDeleted);
    connect(effects, &EffectsHandler::windowClosed, this, &MultitaskViewEffect::onWindowClosed);
    connect(effects, &EffectsHandler::screenAdded, this, [this] { onCloseEffect(true); });
    connect(effects, &EffectsHandler::screenRemoved, this, [this] { onCloseEffect(true); });
    connect(effects, &EffectsHandler::virtualScreenSizeChanged, this, [this] { onCloseEffect(true); });
    connect(effects, &EffectsHandler::virtualScreenGeometryChanged, this, [this] { onCloseEffect(true); });

    connect(_gsettings_dde_dock, &QGSettings::changed, this, &MultitaskViewEffect::onDockChange);

    ensureResources();
    reconfigure(ReconfigureAll);

    m_curDesktopIndex = effects->currentDesktop();
    m_lastDesktopIndex = m_curDesktopIndex;

    m_bgSlidingStatus = false;
    m_bgSlidingTimeLine.setEasingCurve(QEasingCurve::InQuint);
    m_bgSlidingTimeLine.setDuration(std::chrono::milliseconds(EFFECT_DURATION_DEFAULT));

    m_workspaceSlidingStatus = false;
    m_workspaceSlidingTimeline.setEasingCurve(QEasingCurve::OutQuint);
    m_workspaceSlidingTimeline.setDuration(std::chrono::milliseconds(EFFECT_DURATION_DEFAULT));

    m_popStatus = false;
    m_popTimeLine.setEasingCurve(QEasingCurve::OutQuint);
    m_popTimeLine.setDuration(std::chrono::milliseconds(EFFECT_DURATION_DEFAULT));

    m_opacityStatus = false;
    m_opacityTimeLine.setEasingCurve(QEasingCurve::OutQuint);
    m_opacityTimeLine.setDuration(std::chrono::milliseconds(2*EFFECT_DURATION_DEFAULT));

    QString qm = QString(":/effects/multitaskview/translations/multitasking_%1.qm").arg(QLocale::system().name());
    QTranslator *tran = new QTranslator(this);
    if (tran->load(qm)) {
        qApp->installTranslator(tran);
    }

    connect(m_timer, &QTimer::timeout, this, &MultitaskViewEffect::motionRepeat);

    m_model = new MultitaskViewModel(this);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &MultitaskViewEffect::onRowsInserted);
    connect(m_model, &MultitaskViewModel::ready, this, [this]() {
        m_pendingRender = false;
        for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); ++iter) {
            QList<WorkspaceThumbnail *> &list = iter.value();
            for (int j = 0; j < list.size(); j++)
                updateWorkspaceBg(iter.key()->name(), j + 1);
        }
        effects->addRepaintFull();
    });
    m_model->refresh();

    connect(m_model, &MultitaskViewModel::blurReady, this,
        [this](int workspaceIndex, const QString &screenName, QPixmap bgPixmap) {
            updateWorkspaceBg(screenName, workspaceIndex, bgPixmap);
            effects->addRepaintFull();
        });

    // Invalidate workspace cache and refresh Model when desktop count or screens change
    connect(effects, &EffectsHandler::numberDesktopsChanged, this, [this]() {
        invalidateWorkspaceCache();
    });

    initShowWholeStatusAsync();

    QDBusConnection::sessionBus().connect(DBUS_DEEPIN_WM_SERVICE, DBUS_DEEPIN_WM_OBJ, DBUS_DEEPIN_WM_INTF,
                                        "ShowWorkspaceChanged", this, SLOT(toggle()));
    QDBusConnection::sessionBus().connect("com.deepin.ScreenRecorder.time", "/com/deepin/ScreenRecorder/time", "com.deepin.ScreenRecorder.time", "start",
        this, SLOT(screenRecorderStart()));

    m_addingDesktopTimer.setInterval(200);
    m_addingDesktopTimer.setSingleShot(true);
    connect(&m_addingDesktopTimer, &QTimer::timeout, this, &MultitaskViewEffect::addNewDesktop);
}

MultitaskViewEffect::~MultitaskViewEffect()
{
    if (m_showActions) {
        delete m_showActions;
        m_showActions = nullptr;
    }
    if (m_showActionw) {
        delete m_showActionw;
        m_showActionw = nullptr;
    }
    if (m_showActiona) {
        delete m_showActiona;
        m_showActiona = nullptr;
    }
}

void MultitaskViewEffect::onRowsInserted(const QModelIndex &parent, int first, int last)
{
    if (!m_activated)
        return;

    for (int r = first; r <= last; r++) {
        QModelIndex idx = m_model->index(r);
        QString screenName = idx.data(MultitaskViewModel::ScreenNameRole).toString();
        int workspaceIndex = idx.data(MultitaskViewModel::WorkspaceIndexRole).toInt();
        QPixmap bgPix = idx.data(MultitaskViewModel::WorkspaceBgRole).value<QPixmap>();
        QPixmap wpPix = idx.data(MultitaskViewModel::WorkspaceWpRole).value<QPixmap>();

        EffectScreen *screen = effects->findScreen(screenName);
        if (!screen)
            continue;

        // Check if workspace object already exists for this screen/desktop
        bool found = false;
        QList<WorkspaceThumbnail *> &wlist = m_workspaceBackgrounds[screen];
        for (auto *wb : qAsConst(wlist)) {
            if (wb->desktop() == workspaceIndex) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        WorkspaceThumbnail *b = new WorkspaceThumbnail();
        b->setMaxHeight(m_maxHeight);

        ScreenInfo_st &info = m_screenInfoList[screenName];
        b->setArea(info.rect, info.screenrect);
        b->setImage(bgPix, wpPix, QRect());
        b->setScreenDesktop(screen, workspaceIndex);
        wlist.push_back(b);
    }

    updateWorkspacePos();
    effects->addRepaintFull();
}

void MultitaskViewEffect::reconfigure([[maybe_unused]] ReconfigureFlags flags)
{

    m_bgSlidingStatus = false;
    m_bgSlidingTimeLine.reset();

    m_popStatus = false;
    m_popTimeLine.reset();

    m_opacityStatus = false;
    m_opacityTimeLine.reset();

    m_curDesktopIndex = effects->currentDesktop();
    m_lastDesktopIndex = m_curDesktopIndex;

    m_scalingFactor = qMax(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);

    KConfigGroup config_group(KSharedConfig::openConfig("kwinrc"), "Compositing");
    if (effects->waylandDisplay()) {
        setMotionEffect(config_group.readEntry("MultitaskViewMotionEffect", true));
    } else {
        EffectType effectType = effectsEx->effectType();
        setMotionEffect(effectType == EffectType::OpenGLComplete && config_group.readEntry("MultitaskViewMotionEffect", true)
                && config_group.readEntry("window_animation", true));
    }
}

void MultitaskViewEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (isActive()) {
        if(m_effectFlyingBack.animating())
            m_effectFlyingBack.update(presentTime, motionEffectEnable());

        if(m_workspaceSlidingStatus) {
            if (m_workspaceSlidingState == 0) {
                m_workspaceSlidingState = 1;
                m_workspaceSlidingTimeline.reset();
                if (motionEffectEnable()) {
                    m_workspaceSlidingTimeline.setElapsed(std::chrono::milliseconds(30));
                } else {
                    m_workspaceSlidingTimeline.setElapsed(m_workspaceSlidingTimeline.duration());
                }
            } else {
                m_workspaceSlidingTimeline.advance(presentTime);
            }
        }

        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS
                  |  PAINT_SCREEN_BACKGROUND_FIRST;

        if(m_popStatus) {
            if (motionEffectEnable()) {
                m_popTimeLine.advance(presentTime);
            } else {
                m_popTimeLine.setElapsed(m_popTimeLine.duration());
            }
        }

        if(m_opacityStatus) {
            if (motionEffectEnable()) {
                m_opacityTimeLine.advance(presentTime);
            } else {
                m_opacityTimeLine.setElapsed(m_opacityTimeLine.duration());
            }
        }

        if(m_bgSlidingStatus) {
            if (m_bgSlidingState == 0) {
                m_bgSlidingState = 1;
                m_bgSlidingTimeLine.reset();
                if (motionEffectEnable()) {
                    m_bgSlidingTimeLine.setElapsed(std::chrono::milliseconds(30));
                } else {
                    m_bgSlidingTimeLine.setElapsed(m_bgSlidingTimeLine.duration());
                }
            } else {
                m_bgSlidingTimeLine.advance(presentTime);
            }
        }

        if (m_windowEffectState)
            m_windowEffect.update(presentTime, motionEffectEnable());

        for (auto& mm: m_motionManagers) {
            mm->calculate(POPUP_TIME_SCALE, motionEffectEnable());
        }
    }

    for (auto const &w : effects->stackingOrder()) {
        Window* t = static_cast< EffectWindowImpl* >(w)->window();
        if (w->isDock() || w->isNotification() || (t && t->isOverride()))
            w->setData(WindowForceBlurRole, QVariant(true));
    }

    effects->prePaintScreen(data, presentTime);
}

void MultitaskViewEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    m_isShowWin = false;
    effects->paintScreen(mask, region, data);

    if (!m_activated)
        return;

    QMutexLocker locker(&m_mutex);
    for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
        if (m_bgSlidingStatus) {
            WorkspaceThumbnail *lwkobj = getWorkspaceObject(iter.key(), m_lastDesktopIndex - 1);
            WorkspaceThumbnail *cwkobj = getWorkspaceObject(iter.key(), m_curDesktopIndex - 1);
            if (lwkobj && cwkobj) {
                auto area = lwkobj->getfullArea();
                if (effects->waylandDisplay()) {
                    EffectScreen *effectscreen = effectsEx->findScreen(effectsEx->getCurrentPaintingScreen());
                    if (effectscreen && iter.key() != effectscreen) {
                        if (isExtensionMode()) {
                            continue;
                        } else {
                            if (area.x() > 0) {
                                continue;
        }
    }
    m_workspaceCacheValid = true;
}
                } else {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(area.x(), 0, area.width(), m_allFullArea.height());
                }

                if (effects->numberOfDesktops() > 2) {
                    if (m_lastDesktopIndex == 1 && m_curDesktopIndex == effects->numberOfDesktops()) {
                        lwkobj->renderDesktopBackGround(-m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(1 - m_bgSlidingTimeLine.value());
                    } else if (m_lastDesktopIndex == effects->numberOfDesktops() && m_curDesktopIndex == 1) {
                        lwkobj->renderDesktopBackGround(m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(-1 + m_bgSlidingTimeLine.value());
                    } else if (m_lastDesktopIndex < m_curDesktopIndex) {
                        lwkobj->renderDesktopBackGround(-m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(1 - m_bgSlidingTimeLine.value());
                    } else if (m_lastDesktopIndex == m_curDesktopIndex && m_lastDesktopIndex == 1){
                        lwkobj->renderDesktopBackGround(-m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(1 - m_bgSlidingTimeLine.value());
                    } else {
                        lwkobj->renderDesktopBackGround(m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(-1 + m_bgSlidingTimeLine.value());
                    }
                } else {
                    if (m_lastDesktopIndex < m_curDesktopIndex) {
                        lwkobj->renderDesktopBackGround(-m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(1 - m_bgSlidingTimeLine.value());
                    } else if (m_lastDesktopIndex == m_curDesktopIndex && m_lastDesktopIndex == 1){
                        lwkobj->renderDesktopBackGround(-m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(1 - m_bgSlidingTimeLine.value());
                    } else {
                        lwkobj->renderDesktopBackGround(m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(-1 + m_bgSlidingTimeLine.value());
                    }
                }

                if (nullptr == effects->waylandDisplay()) {
                    glDisable(GL_SCISSOR_TEST);
                }
            }
        } else {
            WorkspaceThumbnail *cwkobj = getWorkspaceObject(iter.key(), effects->currentDesktop() - 1);
            if (cwkobj) {
                cwkobj->renderDesktopBackGround(0.0f);
            }
        }

        WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
        if (wmobj && wmobj->getDesktopWinNumofScreen(iter.key()) == 0) {
            if (m_tipFrames.find(iter.key()) != m_tipFrames.end() && m_tipFrames[iter.key()].size() == 2) {
                std::unique_ptr<EffectFrameEx> &bgframe = m_tipFrames[iter.key()][0];
                std::unique_ptr<EffectFrameEx> &tframe = m_tipFrames[iter.key()][1];
                QRect bgrect = tframe->geometry();
                QFontMetrics metrics(tframe->font());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                int width = metrics.width(tframe->text());
#else
                int width = metrics.horizontalAdvance(tframe->text());
#endif

                int height = metrics.height();
                bgrect.adjust(-qRound(((width - bgrect.width()) / 2.0)) - (20 * m_scalingFactor),
                              -qRound(((height - bgrect.height()) / 2.0)) - (10 * m_scalingFactor),
                               qRound(((width - bgrect.width()) / 2.0)) + (20 * m_scalingFactor),
                               qRound(((height - bgrect.height()) / 2.0)) + (11 * m_scalingFactor));
                bgframe->setGeometry(bgrect);
                bgframe->render(infiniteRegion(), 0.8, 0);
                tframe->render(infiniteRegion(), 1, 0);
            }
        }
    }

    //draw workspace background
    for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
        int actualDesktopIndex = 1;
        for (int j = 0; j < iter.value().size(); j++) {
            if (m_deleteWorkspaceDesktop - 1 == j)
                continue;
            WorkspaceThumbnail *wkobj = getWorkspaceObject(iter.key(), j);
            if (wkobj) {
                if (m_popStatus) {  //startup
                    wkobj->renderWorkspaceBackGround(m_popTimeLine.value(), actualDesktopIndex);
                } else if (m_opacityStatus) {  //new workspace
                    if (actualDesktopIndex == effects->numberOfDesktops())
                        wkobj->renderWorkspaceBackGround(m_opacityTimeLine.value(), actualDesktopIndex);
                    else
                        wkobj->renderWorkspaceBackGround(1, actualDesktopIndex);
                } else if (m_wasWorkspaceMove) {  //move workspace
                    if (m_aciveMoveDesktop != actualDesktopIndex) {
                        wkobj->renderWorkspaceBackGround(1, actualDesktopIndex);
                    }
                } else if (m_workspaceSlidingStatus) {
                    renderSlidingWorkspace(wkobj, iter.key(), actualDesktopIndex - 1, data);
                } else {
                    wkobj->renderWorkspaceBackGround(1, actualDesktopIndex);
                }
                actualDesktopIndex++;
            }
        }
    }

    if (!m_wasWindowMove && m_isShowPreview && m_screen != nullptr && !m_workspaceSlidingStatus && effects->numberOfDesktops() < MAX_DESKTOP_COUNT) {
        QRect rect = m_addWorkspaceButton[m_screen]->getRect();
        showWorkspacePreview(m_screen, rect);
    } else {
        showWorkspacePreview(m_screen, QRect(), true);
    }

    //draw button
    if (effects->numberOfDesktops() < MAX_DESKTOP_COUNT) {
        for (auto iter = m_addWorkspaceButton.begin(); iter != m_addWorkspaceButton.end(); iter++) {
            m_addWorkspaceButton[iter.key()]->render();
        }
    }

    for (int desktop = effects->numberOfDesktops(); desktop >= 0; desktop--) {
        m_isShowWin = true;
        paintingDesktop = desktop;
        if (desktop == 0) {
            if (m_hoverDesktop != -1 && m_screen != nullptr && effects->numberOfDesktops() > 1) {
                renderWorkspaceHover(m_screen);
            } else {
                m_workspaceCloseBtnArea = QRect();
            }
        }
        effects->paintScreen(mask, region, data);
    }

    if (m_windowMove && m_wasWindowMove) {
        renderWindowMove(data);
    }

    if (m_wasWorkspaceMove) {
        renderWorkspaceMove(data);
    }
}

void MultitaskViewEffect::postPaintScreen()
{
    if (m_bgSlidingTimeLine.done()) {
        m_bgSlidingStatus = false;
        m_bgSlidingTimeLine.reset();
        m_bgSlidingState = 0;
        handlerAfterTimeLine();
    }

    if (m_workspaceSlidingTimeline.done()) {
        m_workspaceSlidingStatus = false;
        m_workspaceSlidingState = 0;
        m_workspaceSlidingTimeline.reset();
    }

    if (m_popTimeLine.done()) {
        m_popStatus = false;
        m_popTimeLine.reset();
    }

    if (m_opacityTimeLine.done()) {
        m_opacityStatus = false;
        m_opacityTimeLine.reset();
    }

    if (m_windowEffect.getStatus() == 2) {
        m_windowEffectState = false;
    }

    if (m_activated && (m_popStatus || m_opacityStatus || m_bgSlidingStatus
                        || m_workspaceSlidingStatus || m_windowEffectState
                        || m_effectFlyingBack.animating()))
        effects->addRepaintFull();

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
        if (m_effectFlyingBack.animating())
            w->setScissorForce(false);
    }
    effects->postPaintScreen();

    if (m_effectFlyingBack.done()) {
        m_effectFlyingBack.end();
        setActive(false);
        if (QX11Info::isPlatformX11() && m_dockRect.contains(m_cursorPos)) {
            relayDockEvent(m_cursorPos, m_buttonType);
            m_cursorPos.setX(-1);
            m_cursorPos.setY(-1);
            m_buttonType = 0;
        } else if (!QX11Info::isPlatformX11() && m_sendButton != Qt::NoButton) {
            effectsEx->sendPointer(m_cursorPos, m_sendButton);
            m_cursorPos.setX(-1);
            m_cursorPos.setY(-1);
            m_sendButton = Qt::NoButton;
        }
    }
}

void MultitaskViewEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (!m_activated) {
        effects->prePaintWindow(w, data, presentTime);
        return;
    }
    data.mask |= PAINT_WINDOW_TRANSFORMED;

    if (m_bgSlidingStatus) {
        WindowMotionManager *mgr0 = nullptr, *mgr1 = nullptr;
        auto *lastWinManager = getWinManagerObject(m_lastDesktopIndex - 1);
        auto *curWinManager = getWinManagerObject(m_curDesktopIndex - 1);
        if (lastWinManager && curWinManager) {
            lastWinManager->getMotion(m_lastDesktopIndex, w->screen(), mgr0);
            curWinManager->getMotion(m_curDesktopIndex, w->screen(), mgr1);
        }
    }

    effects->prePaintWindow(w, data, presentTime);
}

void MultitaskViewEffect::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!m_isShowWin) {
        return;
    }

    if (!isActive()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    if (m_effectFlyingBack.animating()) {
        if (w->isWaterMark() || w->windowClass() == notification_tips) {
            effects->paintWindow(w, mask, region, data);
            return;
        }
        if (w->isOnDesktop(effects->currentDesktop()) && !w->isMinimized()) {
            m_effectFlyingBack.paintWindow(w, mask, region, data);
        }
        return;
    } else if (m_effectFlyingBack.done()) {
        if (w->isOnDesktop(effects->currentDesktop()) && !w->isMinimized()) {
            effects->paintWindow(w, mask, region, data);
        }
        return;
    }

    if (w->windowClass() == screen_recorder || w == m_screenRecorderMenu) {
        if (!m_screenRecorderMenu || w == m_screenRecorderMenu) {
            effects->setElevatedWindow(w, true);
        }
        effects->paintWindow(w, mask, region, data);
        return;
    }
    if (w->windowClass() == notification_tips && m_isCloseScreenRecorder) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    int desktop = effects->currentDesktop();
    if (0 == paintingDesktop) {
        if (w == m_windowMove && m_wasWindowMove) {
            return;
        }

        if (w->isWaterMark()) {
            effects->paintWindow(w, mask, region, data);
            return;
        }

        if (m_windowEffectState && m_flyingWinList.contains(w)) {
            m_windowEffect.paintWindow(w, mask, region, data);
            return;
        }

        if (m_bgSlidingStatus) {
            if (m_flyingWinList.contains(w))
                return;

            WindowMotionManager *mgr0 = nullptr, *mgr1 = nullptr;
            auto *lastWinManager = getWinManagerObject(m_lastDesktopIndex - 1);
            auto *curWinManager = getWinManagerObject(m_curDesktopIndex - 1);
            QRect backgroundRect = effects->clientArea(FullScreenArea, w->screen(), 1).toRect();
            int bgRectWidth = backgroundRect.width();
            if (lastWinManager) {
                lastWinManager->getMotion(m_lastDesktopIndex, w->screen(), mgr0);
                if (mgr0 && mgr0->isManaging(w)) {
                    auto area = effects->clientArea(ScreenArea, w->screen(), 0).toRect();
                    WindowPaintData d = data;
                    auto geo = mgr0->targetGeometry(w);
                    d.setScale(QVector2D((float)geo.width() / w->width(), (float)geo.height() / w->height()));

                    if (effects->numberOfDesktops() > 2) {
                        if (m_lastDesktopIndex == 1 && m_curDesktopIndex == effects->numberOfDesktops()) {
                            d.translate(geo.x() - bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        } else if (m_lastDesktopIndex == effects->numberOfDesktops() && m_curDesktopIndex == 1) {
                            d.translate(geo.x() + bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        } else if (m_lastDesktopIndex < m_curDesktopIndex) {
                            d.translate(geo.x() - bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        } else {
                            d.translate(geo.x() + bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        }
                    } else {
                        if (m_lastDesktopIndex < m_curDesktopIndex) {
                            d.translate(geo.x() - bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        } else {
                            d.translate(geo.x() + bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        }
                    }
                    if (mgr0->isWindowFill(w) && lastWinManager->isHaveWinFill(w)) {
                        // mask |= PAINT_WINDOW_LANCZOS;
                    }
                    effects->paintWindow(w, mask, area, d);     //when open, all windows flying into RegionB
                }
            }
            if (curWinManager) {
                curWinManager->getMotion(m_curDesktopIndex, w->screen(), mgr1);
                if (mgr1->isManaging(w) && !(m_windowEffectState && m_flyingWinList.contains(w))) {
                    auto area = effects->clientArea(ScreenArea, w->screen(), 0).toRect();
                    WindowPaintData d = data;
                    auto geo = mgr1->targetGeometry(w);
                    d.setScale(QVector2D((float)geo.width() / w->width(), (float)geo.height() / w->height()));

                    if (effects->numberOfDesktops() > 2) {
                        if (m_lastDesktopIndex == effects->numberOfDesktops() && m_curDesktopIndex == 1) {
                            d.translate(geo.x() - bgRectWidth + bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        } else if (m_lastDesktopIndex == 1 && m_curDesktopIndex == effects->numberOfDesktops()) {
                            d.translate(geo.x() + bgRectWidth - bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        } else if(m_lastDesktopIndex<m_curDesktopIndex) {
                            d.translate(geo.x() + bgRectWidth - bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        } else {
                            d.translate(geo.x() - bgRectWidth + bgRectWidth * m_bgSlidingTimeLine.value() - w->x(), geo.y() - w->y(), 0);
                        }
                    } else {
                        if (m_lastDesktopIndex < m_curDesktopIndex) {
                            d.translate(geo.x() + bgRectWidth * (1 - m_bgSlidingTimeLine.value()) - w->x(), geo.y() - w->y(), 0);
                        } else {
                            d.translate(geo.x() + bgRectWidth * (m_bgSlidingTimeLine.value() -1) - w->x(), geo.y() - w->y(), 0);
                        }
                    }

                    if (mgr1->isWindowFill(w) && curWinManager->isHaveWinFill(w)) {
                        // mask |= PAINT_WINDOW_LANCZOS;/
                    }
                    effects->paintWindow(w, mask, area, d);
                }
            }
        } else {
            WindowMotionManager *wmm = nullptr;
            QMutexLocker locker(&m_mutex);
            WindowLayoutManager *wmobj = getWinManagerObject(desktop - 1);
            if (wmobj && wmobj->getMotion(desktop, w->screen(), wmm)) {
                if (wmm->isManaging(w)) {
                    auto area = effects->clientArea(ScreenArea, w->screen(), 0).toRect();
                    WindowPaintData d = data;
                    auto geo = wmm->transformedGeometry(w);

                    if (wmm->isWindowFill(w) && wmobj->isHaveWinFill(w)) {
                        wmobj->getWinFill(w)->render();
                        // mask |= PAINT_WINDOW_LANCZOS;
                    }

                    float scale = geo.width() / w->width();
                    {
                        d.setScale(QVector2D(scale, (float)geo.height() / w->height()));
                        float dx = 0.0f, dy = 0.0f;
                        auto gt = wmm->targetGeometry(w);
                        float dx0 = gt.x() - w->x();
                        float dy0 = gt.y() - w->y();
                        float dx1 = geo.x() - w->x();
                        float dy1 = geo.y() - w->y();
                        if(dx0>0) {
                            if (dx1 > dx0) dx = dx0;
                            else dx = dx1;
                        } else {
                            if (dx1 < dx0) dx = dx0;
                            else dx = dx1;
                        }
                        if(dy0>0) {
                            if (dy1 > dy0) dy = dy0;
                            else dy = dy1;
                        } else {
                            if (dy1 < dy0) dy = dy0;
                            else dy = dy1;
                        }
                        d += QPoint(qRound(dx), qRound(dy));
                    }
                    effects->paintWindow(w, mask, area, d);

                    if (w == m_hoverWin) {
                        float radius = wmm->getWindowRadius(w);
                        if (wmm->isWindowFill(w) && wmobj->isHaveWinFill(w)) {
                            renderHover(w, wmobj->getWinFill(w)->getRect(), 0, radius + 1);
                        } else {
                            renderHover(w, geo.toRect(), 0, m_radius * scale + 2);
                        }
                    }

                    if (w == m_hoverWinBtn) {
                        if (wmm->isWindowFill(w) && wmobj->isHaveWinFill(w)) {
                            renderHover(w, wmobj->getWinFill(w)->getRect(), 1);
                        } else {
                            renderHover(w, geo.toRect(), 1);
                        }
                    }
                }
            }
        }

        if (w->isDock()) {
            m_dockRect = w->frameGeometry().toRect();
            effects->paintWindow(w, mask, region, data);
            return;
        }
    } else if (!m_workspaceSlidingStatus) {
        if (m_wasWorkspaceMove && paintingDesktop == m_aciveMoveDesktop)
            return;

        WindowMotionManager *wmm;
        WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(paintingDesktop - 1);
        if (wkmobj && wkmobj->getMotion(paintingDesktop, w->screen(), wmm)) {
            if (wmm->isManaging(w) && !w->isMinimized()) {
                if (workspace()->showingDesktop() && w->desktops().contains(m_curDesktopIndex))
                    return;

                WindowPaintData d = data;
                auto geo = wmm->transformedGeometry(w).toRect();

                d += QPoint(qRound(geo.x() - w->x()), qRound(geo.y() - w->y()));
                d.setScale(QVector2D((float)geo.width() / w->width(), (float)geo.height() / w->height()));
                geo &= m_workspaceBackgrounds[w->screen()][paintingDesktop - 1]->getGeometry();
                effects->paintWindow(w, mask, geo, d);
            }
        }
    }
}

void MultitaskViewEffect::handlerAfterTimeLine()
{
    if (m_isRemoveWorkspace) {
        cleanDesktopData(m_lastDesktopIndex);
    }
}

void MultitaskViewEffect::renderWindowMove(KWin::ScreenPaintData &data)
{
    QPoint diff = pointerPos() - m_windowMoveStartPos;
    QRect geo = m_windowMoveGeometry.translated(diff);
    WindowPaintData d(data.projectionMatrix());

    WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    QRect rect1 = wmobj ? wmobj->getWindowGeometry(m_windowMove, m_windowMove->screen()) : QRect();
    float K1 = (qreal)rect1.width() / (qreal)m_windowMove->width();
    d += QPoint(-m_windowMove->x(), -m_windowMove->y());
    if (pointerPos().y() < m_windowMoveStartPos.y()) {
        int Y0 = m_scale[m_windowMove->screen()].workspaceMgrRect.bottom();
        float k = (pointerPos().y() - m_windowMoveStartPos.y()) / float(Y0 - m_windowMoveStartPos.y());
        float K0 = (qreal)geo.width() / (qreal)m_windowMove->width();
        k = K1 - k * (K1 - K0);
        if (k < K0) k = K0;
        d *= QVector2D(k, k);

        float sx = (m_windowMoveStartPos.x() - rect1.x()) / float(rect1.width());
        float sy = (m_windowMoveStartPos.y() - rect1.y()) / float(rect1.height());
        float cx = pointerPos().x();
        float cy = pointerPos().y();
        float cw = k * m_windowMove->width();
        float ch = k * m_windowMove->height();
        float x0 = cx - cw * sx;
        float y0 = cy - ch * sy;
        d += QPoint(x0,y0);
    }
    else {
        d *= QVector2D(K1, K1);
        d += QPoint(rect1.left()+diff.x(), rect1.top()+diff.y());
    }
    effects->drawWindow(m_windowMove, PAINT_WINDOW_TRANSFORMED, infiniteRegion(), d);
}

void MultitaskViewEffect::renderWorkspaceMove(KWin::ScreenPaintData &data)
{
    WorkspaceThumbnail *target = getWorkspaceObject(m_screen, m_aciveMoveDesktop - 1);
    if (target) {
        QPoint cursorpos = pointerPos();
        QPoint diff = cursorpos - m_workspaceMoveStartPos;
        if (diff.y() > 1) {
            diff.setY(0);
        }
        if (m_dragWorkspacedirection == dragNone) {
            if (abs(diff.y()) > abs(diff.x())) {
                m_dragWorkspacedirection = dragUpDown;
            } else if (abs(diff.y()) < abs(diff.x())) {
                m_dragWorkspacedirection = dragLeftRight;
            }
        }

        if (m_dragWorkspacedirection == dragUpDown) {
            diff.setX(0);
        } else if (m_dragWorkspacedirection == dragLeftRight) {
            diff.setY(0);
        }

        if (diff.y() < -(target->getRect().height() / 2)) {
            m_workspaceStatus = wpDelete;
            if (effects->numberOfDesktops() != 1)
                renderDragWorkspacePrompt(m_screen);
        } else if (calculateSwitchPos(diff)) {
            m_workspaceStatus = wpSwitch;
        } else {
            m_workspaceStatus = wpRestore;
        }

        QRect wkgeo = target->getRect().translated(diff);
        target->setPosition(wkgeo.topLeft());
        float transparent = 1.0;
        if (m_workspaceStatus == wpDelete)
            transparent = 0.5;
        target->renderWorkspaceBackGround(transparent, m_aciveMoveDesktop);

        WindowMotionManager *wmm;
        EffectWindowList list;
        WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(m_aciveMoveDesktop - 1);
        if (wkmobj && wkmobj->getMotion(m_aciveMoveDesktop, m_screen, wmm)) {
            list = wmm->orderManagedWindows();
        }

        for (EffectWindow *w : list) {
            if (w->screen() == m_screen && wmm->isManaging(w) && !w->isMinimized()) {
                if (workspace()->showingDesktop() && w->desktops().contains(m_curDesktopIndex))
                    return;

                WindowPaintData d(data.projectionMatrix());
                auto geo = wmm->transformedGeometry(w);
                geo = geo.translated(diff);
                d.setOpacity(transparent);

                d *= QVector2D((qreal)geo.width() / (qreal)w->width(), (qreal)geo.height() / (qreal)w->height());
                d += QPoint(geo.left() - w->x(), geo.top() - w->y());

                effects->drawWindow(w, PAINT_WINDOW_TRANSFORMED, wkgeo, d);
            }
        }
    }
}

void MultitaskViewEffect::renderSlidingWorkspace(WorkspaceThumbnail *wkobj, EffectScreen *screen, int desktop, KWin::ScreenPaintData &data)
{
    float x0 = m_workspaceSlidingInfo[wkobj].first;
    float x1 = m_workspaceSlidingInfo[wkobj].second;
    float t = m_workspaceSlidingTimeline.value();
    float x  = (x1 - x0) * t + x0;
    QRect rect = wkobj->getRect();
    rect.moveTo(int(x), rect.y());
    wkobj->setRect(rect);
    wkobj->renderWorkspaceBackGround(1, desktop + 1);

    WindowMotionManager *wmm;
    WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(desktop);
    if (wkmobj && wkmobj->getMotion(m_aciveMoveDesktop, screen, wmm)) {
        for (EffectWindow *w : wmm->orderManagedWindows()) {
            if (wmm->isManaging(w) && !w->isMinimized()) {
                WindowPaintData d(data.projectionMatrix());
                auto geo = wmm->transformedGeometry(w);
                geo.moveTo(x + geo.x() - x1, geo.y());

                d *= QVector2D((qreal)geo.width() / (qreal)w->width(), (qreal)geo.height() / (qreal)w->height());
                d += QPoint(geo.left() - w->x(), geo.top() - w->y());

                effects->drawWindow(w, PAINT_WINDOW_TRANSFORMED, rect, d);
            }
        }
    }
}

void MultitaskViewEffect::initShowWholeStatusAsync()
{
    auto *iface = new QDBusInterface(CONFIGMANAGER_SERVICE, "/", CONFIGMANAGER_INTERFACE,
                                     QDBusConnection::systemBus(), this);
    iface->setTimeout(100);
    auto *call = new QDBusPendingCallWatcher(
        iface->asyncCall("acquireManager", "org.kde.kwin", "org.kde.kwin.multitaskview.display", ""),
        this);
    connect(call, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<QDBusObjectPath> reply = *w;
            if (reply.isError()) {
                qWarning() << "MultitaskViewEffect: DConfig acquireManager failed:"
                           << reply.error().message();
                w->deleteLater();
                return;
            }
            QString path = reply.value().path();
            QDBusConnection::systemBus().connect(CONFIGMANAGER_SERVICE, path,
                CONFIGMANAGER_MANAGER_INTERFACE, "valueChanged",
                this, SLOT(updateShowWholeStatus(QString)));
            loadShowWholeConfig(path);
            w->deleteLater();
        });
}

void MultitaskViewEffect::loadShowWholeConfig(const QString &path)
{
    auto *iface = new QDBusInterface(CONFIGMANAGER_SERVICE, path, CONFIGMANAGER_MANAGER_INTERFACE,
                                     QDBusConnection::systemBus(), this);
    iface->setTimeout(100);
    auto *call = new QDBusPendingCallWatcher(
        iface->asyncCall("value", "windowDisplay"), this);
    connect(call, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<QVariant> reply = *w;
            if (reply.isError()) {
                qWarning() << "MultitaskViewEffect: DConfig value read failed:"
                           << reply.error().message();
            } else {
                QString strValue = reply.value().toString();
                m_isShowWhole = (strValue == "Enabled" || strValue == "enabled");
            }
            w->deleteLater();
        });
}

void MultitaskViewEffect::updateShowWholeStatus(const QString& type)
{
    Q_UNUSED(type)
    // Re-acquire manager path and re-read config
    auto *iface = new QDBusInterface(CONFIGMANAGER_SERVICE, "/", CONFIGMANAGER_INTERFACE,
                                     QDBusConnection::systemBus(), this);
    iface->setTimeout(100);
    auto *call = new QDBusPendingCallWatcher(
        iface->asyncCall("acquireManager", "org.kde.kwin", "org.kde.kwin.multitaskview.display", ""),
        this);
    connect(call, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<QDBusObjectPath> reply = *w;
            if (!reply.isError()) {
                loadShowWholeConfig(reply.value().path());
            }
            w->deleteLater();
        });
}

void convertFromGLImage(QImage &img, int w, int h)
{
    // from QtOpenGL/qgl.cpp
    // Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
    // see http://qt.gitorious.org/qt/qt/blobs/master/src/opengl/qgl.cpp
    if (QSysInfo::ByteOrder == QSysInfo::BigEndian) {
        // OpenGL gives RGBA; Qt wants ARGB
        uint *p = static_cast<uint *>(static_cast<void *>(img.bits()));
        uint *end = p + w * h;
        while (p < end) {
            uint a = *p << 24;
            *p = (*p >> 8) | a;
            p++;
        }
    } else {
        // OpenGL gives ABGR (i.e. RGBA backwards); Qt wants ARGB
        for (int y = 0; y < h; y++) {
            uint *q = reinterpret_cast<uint *>(static_cast<void *>(img.scanLine(y)));
            for (int x = 0; x < w; ++x) {
                const uint pixel = *q;
                *q = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff)
                     | (pixel & 0xff00ff00);

                q++;
            }
        }

    }
    img = img.mirrored();
}

QColor pixmapMainColor(QPixmap p, double bright) //p为目标图片 bright为亮度系数，为1则表示保持不变
{
    int step=20; //步长：在图片中取点时两点之间的间隔，若为1,则取所有点，适当将此值调大有利于提高运行速度
    int t=0; //点数：记录一共取了多少个点，用于做计算平均数的分母
    QImage image=p.toImage(); //将Pixmap类型转为QImage类型
    int r=0,g=0,b=0; //三元色的值，分别用于记录各个点的rgb值的和
    for (int i=0;i<p.width();i+=step) {
        for (int j=0;j<p.height();j+=step) {
            if(image.valid(i,j)){ //判断该点是否有效
                t++; //点数加一
                QColor c=image.pixel(i,j); //获取该点的颜色
                r+=c.red(); //将获取到的各个颜色值分别累加到rgb三个变量中
                b+=c.blue();
                g+=c.green();
            }
        }
    }
        return QColor(int(bright*r/t)>255?255:int(bright*r/t),
                  int(bright*g/t)>255?255:int(bright*g/t),
                  int(bright*b/t)>255?255:int(bright*b/t)); //最后返回的值是亮度系数×平均数,若超出255则设置为255也就是最大值，防止乘与亮度系数后导致某些值大于255的情况。
}

void MultitaskViewEffect::renderHover(const EffectWindow *w, const QRect &rect, int order, float radius)
{
    if (!order) {
        QColor color = effectsEx->getActiveColor();
        QRect geoframe = rect;
        geoframe.adjust(-4, -4, 4, 4);
        m_hoverWinFrame->setGeometry(geoframe);
        m_hoverWinFrame->setColor(color);
        m_hoverWinFrame->setRadius(radius);
        m_hoverWinFrame->render(infiniteRegion(), 1, 0);
    } else {
        int width = 0;
        int height = 0;
        {
            QFont font;
            font.setPointSize(14);
            font.setFamily(m_fontFamily);
            m_textWinFrame->setFont(font);
            QFontMetrics metrics(m_textWinFrame->font());
            QString textWin = metrics.elidedText(w->caption(), Qt::ElideRight, rect.width() * 0.9);
            m_textWinFrame->setText(QString(textWin));
            if (textWin == "") {
                m_textWinFrame->setText(" ");
            }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            width = metrics.width(textWin);
#else
            width = metrics.horizontalAdvance(textWin);
#endif
            height = metrics.height();
        }

        if (w->keepAbove()) {
            m_topWinFrame->setImage(QUrl::fromLocalFile(MULTITASK_TOP_ACTIVE_SVG));
        } else {
            m_topWinFrame->setImage(QUrl::fromLocalFile(MULTITASK_TOP_SVG));
        }

        // button point 微调结果
        m_closeWinFrame->setPosition(QPoint(rect.x() + rect.width() - 12 * effectsEx->getOsScale(), rect.y() - 10 * effectsEx->getOsScale()));
        m_topWinFrame->setPosition(QPoint(rect.x() - 12 * effectsEx->getOsScale(), rect.y() - 10 * effectsEx->getOsScale()));
        m_textWinFrame->setPosition(QPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() - 40));
        if (m_winBtnArea.size() < 2)
            m_winBtnArea.resize(2);
        m_winBtnArea[0] = QRect(QPoint(rect.x() + rect.width() - 25, rect.y() - 17), QSize(48 * effectsEx->getOsScale(), 48 * effectsEx->getOsScale()));
        m_winBtnArea[1] = QRect(QPoint(rect.x() - 22, rect.y() - 17), QSize(48 * effectsEx->getOsScale(), 48 * effectsEx->getOsScale()));

        QRect geoframe = m_textWinFrame->geometry();
        geoframe.adjust(-qRound((width - geoframe.width()) / 2.0) - (16 * m_scalingFactor),
                        -qRound((height - geoframe.height()) / 2.0) - qRound((48 * m_scalingFactor - height) / 2.0),
                         qRound((width - geoframe.width()) / 2.0) + (16 * m_scalingFactor),
                         qRound((height - geoframe.height()) / 2.0) + qRound((48 * m_scalingFactor - height) / 2.0));
        m_textWinBgFrame->setGeometry(geoframe);

        m_closeWinFrame->render(infiniteRegion(), 1, 0);
        m_topWinFrame->render(infiniteRegion(), 1, 0);
        if (!(m_textWinFrame->text() == " ")) {
            m_textWinBgFrame->render(infiniteRegion(), 0.8, 0);
            m_textWinFrame->render(infiniteRegion(), 1, 0);
        }
    }
}

bool MultitaskViewEffect::isActive() const
{
    return m_activated && !effects->isScreenLocked();
}

void MultitaskViewEffect::toggle()
{
    const bool reportMultitaskView = sender() == m_showActions && !m_activated;

    if (!QX11Info::isPlatformX11()) { // MultitaskView cannot be opened without login in Wayland mode
        if(qgetenv("DISPLAY").isEmpty()) {
            return;
        }
    }

    if (m_activated) {
        m_effectFlyingBack.begin();
    } else if (m_delayDbus) {
        if (!QX11Info::isPlatformX11()) {
            QTimer::singleShot(50, [&, this]() {
                this->setActive(!this->m_activated);
            });
        } else {
            setActive(!m_activated);
        }
        if (reportMultitaskView) {
            EventLog::reportMultitaskView();
        }
    }
}

void MultitaskViewEffect::cleanup()
{
    if (m_activated)
        return;
    if (m_isCleanedUp)
        return;
    m_isCleanedUp = true;

    m_isScreenRecorder = false;
    m_isCloseScreenRecorder = false;
    m_screenRecorderMenu = nullptr;

    if (m_hasKeyboardGrab)
        effects->ungrabKeyboard();
    m_hasKeyboardGrab = false;
    effects->stopMouseInterception(this);
    effects->setActiveFullScreenEffect(0);

    while (m_motionManagers.size() > 0) {
        m_motionManagers.first()->clearMotion();
        delete m_motionManagers.first();
        m_motionManagers.removeFirst();
    }

    while (m_workspaceWinMgr.size() > 0) {
        m_workspaceWinMgr.first()->clearMotion();
        delete m_workspaceWinMgr.first();
        m_workspaceWinMgr.removeFirst();
    }

    m_maxHeight = 0;
    m_windowMove = nullptr;
    m_hoverWin = nullptr;
    m_hoverWinBtn = nullptr;

    m_motionManagers.clear();
    m_workspaceWinMgr.clear();
    m_flyingWinList.clear();
    m_notificationList.clear();
    for (EffectWindow *w : effects->stackingOrder()) {
        w->unrefVisibleEx(EffectWindow::PAINT_DISABLED_BY_MINIMIZE | EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    }
}

void MultitaskViewEffect::setActive(bool active)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;

    if (m_activated == active)
        return;

    if (active && workspace()->moveResizeWindow()) {
        return ;
    }

    m_activated = active;

    QDBusMessage message = QDBusMessage::createSignal("/KWin", "org.kde.KWin", "MultitaskStateChanged");
    message << bool(m_activated);
    QDBusConnection::sessionBus().send(message);

    if(!active) {
        m_popStatus = false;
        m_popTimeLine.reset();

        m_opacityStatus = false;
        m_opacityTimeLine.reset();

        m_bgSlidingStatus = false;
        m_bgSlidingTimeLine.reset();

        m_effectFlyingBack.reset();
        m_windowEffectState = false;
    } else {
        m_popStatus = true;   //start popup TimeLine
        m_popTimeLine.reset();

        m_opacityStatus = false;
        m_opacityTimeLine.reset();

        m_bgSlidingStatus = false;
        m_bgSlidingTimeLine.reset();

        m_effectFlyingBack.reset();

        m_isShieldEvent = false;
        m_windowEffectState = false;
    }

    cleanup();
    if (active && !m_model->isReady()) {
        m_pendingRender = true;
    }
    if (active) {
        m_isCleanedUp = false;
        initWorkspaceBackground();
        createEffectFrame();
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        m_hasKeyboardGrab = effects->grabKeyboard(this);
        effects->setActiveFullScreenEffect(this);

        m_radius = effectsEx->getOsRadius() * effectsEx->getOsScale();

        EffectWindowList windowsList = effects->stackingOrder();
        int ncurrentDesktop = effects->currentDesktop();

        m_backgroundRect = effects->clientArea(MaximizeFullArea, effects->findScreen(0), ncurrentDesktop).toRect();
        m_curDesktopIndex = ncurrentDesktop;

        for (int i = 1; i <= effects->numberOfDesktops(); i++) {        //number of desktops
            setWinLayout(i, windowsList);
        }

        for (EffectWindow *w : windowsList) {
            w->refVisibleEx(EffectWindow::PAINT_DISABLED_BY_MINIMIZE | EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        }

        effects->addRepaintFull();
    } else {
        for (auto &p: m_motionManagers) {
            p->resetWindow();
        }
    }
}

void MultitaskViewEffect::setWinLayout(int desktop, const EffectWindowList &windows)
{
    WindowLayoutManager *wmofDesktop = new WindowLayoutManager();
    WindowLayoutManager *wmofWorkspace = new WindowLayoutManager();

    wmofDesktop->setdesktop(desktop);
    m_motionManagers.append(wmofDesktop);

    WindowLayoutManager *wmobj = getWinManagerObject(desktop - 1);

    for (auto effectScreen : effects->screens()) {           //number of screens
        WindowMotionManager wmm;
        WindowMotionManager workspacewmm;
        EffectWindowList winList;
        for (const auto& w: windows) {
            if (w->isOnDesktop(desktop) && isRelevantWithPresentWindows(w) && w->screen() == effectScreen && checkConfig(w)) {
                wmm.manage(w);
                workspacewmm.manage(w);
                winList.push_back(w);
                w->setScissorForce(true);
            } else if (w->isDock()) {
                m_dockRect = w->frameGeometry().toRect();
                m_dock = w;
            }
        }

        calculateWindowTransformations(winList, wmm, desktop, effectScreen, m_scale[effectScreen], m_radius,
            wmobj, m_maxHeight, m_effectFlyingBack, m_windowEffect, m_flyingWinList);

        wmobj->setWinManager(effectScreen, wmm);

        calculateWorkSpaceWinTransformations(workspacewmm.managedWindows(), workspacewmm, desktop, m_workspaceBackgrounds);
        wmofWorkspace->setWinManager(effectScreen, workspacewmm);
    }

    wmofWorkspace->setdesktop(desktop);
    m_workspaceWinMgr.append(wmofWorkspace);
}

void MultitaskViewEffect::updateWorkspaceWinLayout(int numDesktops)
{
    int num = 0;
    if (m_workspaceWinMgr.size() >= numDesktops) {
        num = numDesktops;
    } else {
        return;
    }

    for (int i = 0; i < num; i++) {
        for (auto effectScreen : effects->screens()) {
            WindowMotionManager *wmm;
            if (!m_workspaceWinMgr[i]->getMotion(i + 1, effectScreen, wmm)) {
                continue;
            }
            calculateWorkSpaceWinTransformations(wmm->managedWindows(), *wmm, i + 1, m_workspaceBackgrounds);
        }
    }
}

void MultitaskViewEffect::updateWorkspaceWinLayout(int numDesktops, int desktop)
{
    int num = 0;
    if (m_workspaceWinMgr.size() >= numDesktops) {
        num = numDesktops;
    } else {
        return;
    }

    for (int i = 0; i < num; i++) {
        for (auto effectScreen : effects->screens()) {
            WindowMotionManager *wmm;
            if (!m_workspaceWinMgr[i]->getMotion(i + 1, effectScreen, wmm)) {
                continue;
            }
            precalculateWorkSpaceWinTransformations(wmm->managedWindows(), *wmm, desktop <= i + 1 ? i + 2 : i + 1, m_workspaceBackgrounds);
        }
    }
}

void MultitaskViewEffect::workspaceWinRelayout(int desktop, EffectScreen *screen)
{
    WindowMotionManager *wmm;
    WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(desktop - 1);
    if (wkmobj && wkmobj->getMotion(desktop, screen, wmm)) {
        calculateWorkSpaceWinTransformations(wmm->managedWindows(), *wmm, desktop, m_workspaceBackgrounds, m_deleteWorkspaceDesktop);
    }
}

bool MultitaskViewEffect::isRelevantWithPresentWindows(EffectWindow *w) const
{
    if (w->isSpecialWindow() || w->isUtility()) {
        return false;
    }

    if (w->isPopupWindow()) {
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
    if (!QX11Info::isPlatformX11()) {
        auto cl = static_cast<EffectWindowImpl *>(w)->window();
        if (w->windowClass() == notification_tips || w->windowClass() == screen_recorder || cl->isStandAlone()) {
            return false;
        }
    }

    return true;
}

bool MultitaskViewEffect::isReceiveEvent()
{
    if (!m_activated) {
        return false;
    }

    if (m_effectFlyingBack.getStatus() != -1) {
        return false;
    }

    if (m_isShieldEvent) {
        return false;
    }

    return true;
}

QRect MultitaskViewEffect::calculateWorkspaceRect(int index, EffectScreen *screen, QRect maxRect, int customDesktopCount)
{
    int spacingScale = maxRect.y() + m_scale[screen].spacingHeight;
    int workspacingScale = m_scale[screen].workspacingWidth;
    int nums = customDesktopCount ? customDesktopCount : effects->numberOfDesktops();
    int thumbWidth = m_scale[screen].workspaceWidth;
    int xfpos = maxRect.width() / 2 - ((thumbWidth * nums) / 2) - (nums / 2 * (workspacingScale/* / 2*/));
    int xpos = xfpos + (thumbWidth + (workspacingScale /*/ 2*/)) * (index - 1);
    QRect rect(xpos + maxRect.x(), spacingScale, thumbWidth, m_scale[screen].workspaceHeight);

    return rect;
}

bool MultitaskViewEffect::calculateSwitchPos(QPoint diffPoint)
{
    int index = 0, num = effects->numberOfDesktops();
    if (diffPoint.x() < 0 && m_moveWorkspacedirection != mvLeft) {
        m_moveWorkspacedirection = mvLeft;
        m_moveWorkspaceNum = -1;
        restoreWorkspacePos(index, num);
    } else if (diffPoint.x() > 0 && m_moveWorkspacedirection != mvRight) {
        m_moveWorkspacedirection = mvRight;
        m_moveWorkspaceNum = -1;
        restoreWorkspacePos(index, num);
    } else if (diffPoint.x() == 0) {
        restoreWorkspacePos(index, num);
        return false;
    }

    int restore = 1;    // 0 restore self; 1 restore other;
    bool bFlag = true;
    if (m_aciveMoveDesktop == 1 && m_moveWorkspacedirection == mvLeft) {
        bFlag = false;
        index = m_aciveMoveDesktop;
    } else if (m_aciveMoveDesktop == effects->numberOfDesktops() && m_moveWorkspacedirection == mvRight) {
        bFlag = false;
        num = m_aciveMoveDesktop - 1;
    } else {
        int frontdiff = abs(diffPoint.x()) - (m_scale[m_screen].workspaceWidth * 2 / 3 + m_scale[m_screen].workspacingWidth);
        int afterdiff = abs(diffPoint.x()) - (m_scale[m_screen].workspaceWidth / 3);

        if (frontdiff > 0) {
            int fnum = frontdiff / (m_scale[m_screen].workspaceWidth + m_scale[m_screen].workspacingWidth);
            if (m_moveWorkspacedirection == mvLeft && fnum > m_aciveMoveDesktop - 2) {
                fnum = m_aciveMoveDesktop - 2;
            } else if (m_moveWorkspacedirection == mvRight && fnum > (effects->numberOfDesktops() - m_aciveMoveDesktop - 1)) {
                fnum = effects->numberOfDesktops() - m_aciveMoveDesktop - 1;
            }

            int anum = afterdiff / (m_scale[m_screen].workspaceWidth + m_scale[m_screen].workspacingWidth);
            if (m_moveWorkspacedirection == mvLeft && anum > m_aciveMoveDesktop - 1) {
                anum = m_aciveMoveDesktop - 1;
            } else if (m_moveWorkspacedirection == mvRight && m_aciveMoveDesktop < effects->numberOfDesktops() && anum > (effects->numberOfDesktops() - m_aciveMoveDesktop)) {
                anum = effects->numberOfDesktops() - m_aciveMoveDesktop;
            }

            if (m_moveWorkspaceNum < fnum) {
                m_moveWorkspaceNum = fnum;
            }

            for (int i = 0; i <= fnum; i++) {
                if (m_moveWorkspacedirection == mvLeft) {
                    WorkspaceThumbnail *target = getWorkspaceObject(m_screen, m_aciveMoveDesktop - 2 - i);
                    WorkspaceThumbnail *pretarget = getWorkspaceObject(m_screen, m_aciveMoveDesktop - i - 1);
                    WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(m_aciveMoveDesktop - 2 - i);
                    if (target && pretarget && wkmobj && target->m_posStatus != mvRight) {
                        target->m_posStatus = mvRight;
                        target->updateGeometry(pretarget->getRect());

                        int xdiff = pretarget->getRect().x() - target->getRect().x();
                        wkmobj->updatePos(m_screen, target->getCurrentRect(), QPoint(xdiff, 0));
                    }
                } else if (m_moveWorkspacedirection == mvRight) {
                    WorkspaceThumbnail *target = getWorkspaceObject(m_screen, m_aciveMoveDesktop + i);
                    WorkspaceThumbnail *pretarget = getWorkspaceObject(m_screen, m_aciveMoveDesktop + i - 1);
                    WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(m_aciveMoveDesktop + i);
                    if (target && pretarget && wkmobj && target->m_posStatus != mvLeft) {
                        target->m_posStatus = mvLeft;
                        target->updateGeometry(pretarget->getRect());

                        int xdiff = pretarget->getRect().x() - target->getRect().x();
                        wkmobj->updatePos(m_screen, target->getCurrentRect(), QPoint(xdiff, 0));
                    }
                }
            }

            if (m_moveWorkspacedirection == mvLeft) {
                num = m_aciveMoveDesktop - 2 - anum;
            } else {
                index = m_aciveMoveDesktop + 1 + anum;
            }

            if (m_moveWorkspaceNum > anum)
                m_moveWorkspaceNum = anum;

        } else if (afterdiff < 0) {
            bFlag = false;
            m_moveWorkspaceNum = -1;
        } else if (afterdiff > 0) {
            restore = 0;
            if (m_moveWorkspaceNum == -1)
                bFlag = false;
        } else {
            bFlag = false;
        }
    }

    if (restore) {
        restoreWorkspacePos(index, num);
    }

    return bFlag;
}

void MultitaskViewEffect::restoreWorkspacePos(int index, int num)
{
    for (; index < num; index++) {
        if (index == m_aciveMoveDesktop - 1)
            continue;
        WorkspaceThumbnail *wkobj = getWorkspaceObject(m_screen, index);
        if (wkobj) {
            if (wkobj->m_posStatus == mvNone) {
                continue;
            } else {
                wkobj->m_posStatus = mvNone;
                QRect currentR = wkobj->getCurrentRect();
                wkobj->updateGeometry(wkobj->getRect());

                int xdiff = wkobj->getCurrentRect().x() - currentR.x();
                WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(index);
                if (xdiff != 0 && wkmobj) {
                    wkmobj->updatePos(m_screen, wkobj->getCurrentRect(), QPoint(xdiff, 0));
                }
            }
        }
    }
}

void MultitaskViewEffect::invalidateWorkspaceCache()
{
    m_workspaceCacheValid = false;
}

void MultitaskViewEffect::applyWorkspaceBg(WorkspaceThumbnail *b, ScreenInfo_st &info, int desktop)
{
    QRect rect = calculateWorkspaceRect(desktop, info.screen, info.rect);
    m_model->setItemThumbRect(info.name, desktop, rect);
    QModelIndex idx = m_model->findIndex(info.name, desktop);
    QPixmap bgPix = m_model->data(idx, MultitaskViewModel::WorkspaceBgRole).value<QPixmap>();
    QPixmap wpPix = m_model->data(idx, MultitaskViewModel::WorkspaceWpRole).value<QPixmap>();
    b->setArea(info.rect, info.screenrect);
    b->setMaxHeight(m_maxHeight);
    b->setScreenDesktop(info.screen, desktop);
    if (!bgPix.isNull() || !wpPix.isNull())
        b->setImage(bgPix, wpPix, rect);
}

void MultitaskViewEffect::initWorkspaceBackground()
{
    getScreenInfo();

    int count = effects->numberOfDesktops();

    if (m_workspaceCacheValid && !m_workspaceBackgrounds.isEmpty()) {
        for (auto wit = m_workspaceBackgrounds.begin(); wit != m_workspaceBackgrounds.end(); ++wit) {
            EffectScreen *screen = wit.key();
            ScreenInfo_st info;
            for (auto si = m_screenInfoList.constBegin(); si != m_screenInfoList.constEnd(); ++si) {
                if (si.value().screen == screen) {
                    info = si.value();
                    break;
                }
            }
            for (int i = 0; i < wit.value().size(); i++) {
                applyWorkspaceBg(wit.value()[i], info, i + 1);
            }
        }
        return;
    }

    for (auto &list : m_workspaceBackgrounds)
        for (auto *b : list) delete b;
    m_workspaceBackgrounds.clear();
    for (auto &btn : m_addWorkspaceButton)
        delete btn;
    m_addWorkspaceButton.clear();

    QHash<QString, ScreenInfo_st>::iterator it = m_screenInfoList.begin();
    for (; it != m_screenInfoList.end(); ++it) {
        QList<WorkspaceThumbnail *> list;
        for (int i = 1; i <= count; i++) {
            WorkspaceThumbnail *b = new WorkspaceThumbnail();
            applyWorkspaceBg(b, it.value(), i);
            list.push_back(b);
        }
        m_workspaceBackgrounds[it.value().screen] = list;

        {
            std::unique_ptr<EffectFrameEx> bgframe = effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/hoverbg.qml", false);
            bgframe->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            bgframe->setColor("#E0E0E0");
            bgframe->setRadius(5);
            std::unique_ptr<EffectFrameEx> frame = effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/text.qml", false);
            frame->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            QFont font;
            font.setPointSize(12);
            frame->setFont(font);
            frame->setText(tr("No windows"));
            frame->setPosition(QPoint(it.value().screenrect.width() / 2 + it.value().screenrect.x(), it.value().screenrect.height() / 2 + it.value().screenrect.y()));
            m_tipFrames[it.value().screen].push_back(std::move(bgframe));
            m_tipFrames[it.value().screen].push_back(std::move(frame));

            AddWorkspaceButton *addButton = new AddWorkspaceButton();
            QRect buttonRect(it.value().rect.x() + it.value().rect.width() - it.value().rect.width() * ADDBTN_W_SCALE,
                             it.value().rect.y() + it.value().rect.height() * ADDBTN_H_SCALE,
                             it.value().rect.width() * ADDBTN_SIZE_SCALE,
                             it.value().rect.width() * ADDBTN_SIZE_SCALE);
            QString buttonImage = QLatin1String(add_workspace_png);
            addButton->setArea(it.value().rect, it.value().screenrect);
            addButton->setMaxHeight(m_maxHeight);
            addButton->setImage(buttonImage, buttonRect);
            m_addWorkspaceButton[it.value().screen] = addButton;
        }
    }
}


void MultitaskViewEffect::updateWorkspacePos()
{
    for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); ++iter) {
        QList<WorkspaceThumbnail *> list = iter.value();
        for (int j = 0; j < list.size(); ++j) {
            QRect rect = calculateWorkspaceRect(j + 1, iter.key(), list[j]->getClientArea());
            list[j]->setRect(rect);
        }
    }
}

void MultitaskViewEffect::updateWorkspacePos(int removedesktop)
{
    int index = 0;
    for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); ++iter) {
        QList<WorkspaceThumbnail *> list = iter.value();
        for (int j = 0; j < list.size(); ++j) {
            if (j == removedesktop - 1)
                continue;
            QRect rect = calculateWorkspaceRect(index + 1, iter.key(), list[j]->getClientArea(), effects->numberOfDesktops() - 1);
            list[j]->setRect(rect);
            ++index;
        }
        index = 0;
    }
}

void MultitaskViewEffect::getScreenInfo()
{
    m_screenInfoList.clear();
    m_allFullArea = effects->clientArea(FullArea, effects->screens().first(), effects->currentDesktop()).toRect();

    for (Output *output : workspace()->outputs()) {
        EffectScreen *effectScreen = effectsEx->findScreen(output);
        QRect rect = effects->clientArea(FullScreenArea, effectScreen, effects->currentDesktop()).toRect();
        QRect maxRect = effects->clientArea(MaximizeArea, effectScreen, effects->currentDesktop()).toRect();
        ScreenInfo_st infost;
        infost.rect = maxRect;
        infost.screenrect = rect;
        infost.screen = effectScreen;
        //use v23 support EffectScreen get name
        infost.name = effectScreen->name();

        m_screenInfoList[infost.name] = infost;

        Scale_st scalest;
        scalest.fullArea = rect;
        scalest.spacingHeight = rect.height() * SPACING_H_SCALE;
        scalest.spacingWidth = rect.width() * SPACING_W_SCALE;
        scalest.workspacingWidth = rect.width() * WORK_SPACING_SCALE;
        scalest.workspaceWidth = rect.width() * WORKSPACE_SCALE;
        scalest.workspaceHeight = rect.height() * WORKSPACE_SCALE;
        scalest.workspaceMgrHeight = scalest.spacingHeight * 2 + rect.height() * WORKSPACE_SCALE;
        scalest.workspaceMgrRect = QRect(maxRect.x(), maxRect.y(), maxRect.width(), scalest.workspaceMgrHeight);
        scalest.windowMgrRect = QRect(maxRect.x(), maxRect.y() + scalest.workspaceMgrHeight, maxRect.width(), maxRect.height() - scalest.workspaceMgrHeight);
        m_scale[effectScreen] = scalest;

        if (m_maxHeight < rect.y() + rect.height())
            m_maxHeight = rect.y() + rect.height();
        if (!isExtensionMode()) {
            break;
        }

    }
}

int MultitaskViewEffect::getNumScreens()
{
    int num = 1;
    if (isExtensionMode()) {
        num = effects->screens().size();
    } else if (effects->screens().size() <= 0) {
        num = 0;
    }
    return num;
}

bool MultitaskViewEffect::isExtensionMode() const
{
    if (effects->screens().size() >= 2) {
        QRect rect = effects->clientArea(FullScreenArea, effects->findScreen(0), effects->currentDesktop()).toRect();
        if (effects->virtualScreenGeometry() != rect) {
            return true;
        }
    }
    return false;
}

void MultitaskViewEffect::createBackgroundFill(EffectWindow *w, QRect rect, float radius, int desktop)
{
    WindowLayoutManager *wmobj = getWinManagerObject(desktop - 1);
    if (!wmobj || wmobj->isHaveWinFill(w))
        return;

    WindowFill *fill = new WindowFill(w->screen(), rect, radius, m_maxHeight);
    wmobj->setWinFill(w, fill);
}

void MultitaskViewEffect::removeBackgroundFill(EffectWindow *w, int desktop)
{
    WindowLayoutManager *wmobj = getWinManagerObject(desktop - 1);
    if (!wmobj || !wmobj->isHaveWinFill(w))
        return;

    wmobj->removeWinFill(w);
}

void MultitaskViewEffect::addNewDesktop()
{
    int count = effects->numberOfDesktops();
    if (count >= MAX_DESKTOP_COUNT)
        return;

    m_isShieldEvent = true;
    if (!getWinManagerObject(count - 1))
        return;

    m_addingDesktop = true;
    QString bgFile = m_previewFile.isEmpty()
        ? m_model->getRandBackground() : m_previewFile;
    m_model->addDesktop(bgFile);

    QList<QString> list = m_screenInfoList.keys();
    for (int i = 0; i < list.size(); i++) {
        QString screenName = list[i];
        ScreenInfo_st &st = m_screenInfoList[screenName];
        QRect rect = calculateWorkspaceRect(count + 1, st.screen, st.rect);

        QPixmap wpPix = MultitaskViewModel::cutBackgroundPix(
            rect.size(),
            MultitaskViewModel::toRealPath(bgFile));

        WorkspaceThumbnail *b = new WorkspaceThumbnail();
        b->setMaxHeight(m_maxHeight);
        b->setArea(st.rect, st.screenrect);
        b->setImage(wpPix, wpPix, rect);
        b->setScreenDesktop(st.screen, count + 1);
        m_workspaceBackgrounds[st.screen].push_back(b);
    }

    {
        m_workspaceSlidingStatus = true;
        m_workspaceSlidingTimeline.reset();
        m_workspaceSlidingInfo.clear();
        {
            for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
                QRect maxRect = effects->clientArea(MaximizeArea, iter.key(), 1).toRect();
                QPoint pos(maxRect.x() + maxRect.width() - 20 - m_scale[iter.key()].workspaceWidth / 2, maxRect.y() + m_scale[iter.key()].spacingHeight);
                QList<WorkspaceThumbnail *> list = m_workspaceBackgrounds[iter.key()];
                for (int j = 0; j < list.size(); j++) {
                    QRect rect = list[j]->getRect();
                    m_workspaceSlidingInfo[list[j]].first = rect.x();
                    // 遍历到最后一个工作区移动的时候,使其新增的工作区的添加动画的起始坐标为要求固定位置
                    if (j == list.size() - 1) {
                        m_workspaceSlidingInfo[list[j]].first = pos.x();
                    }
                }
            }
        }
    }

    updateWorkspacePos();
    EffectWindowList windowsList = effects->stackingOrder();
    setWinLayout(count + 1, windowsList);
    updateWorkspaceWinLayout(count);

    {
        {
            for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
                QList<WorkspaceThumbnail *> list = m_workspaceBackgrounds[iter.key()];
                for (int j = 0; j < list.size(); j++) {
                    QRect rect = list[j]->getRect();
                    m_workspaceSlidingInfo[list[j]].second = rect.x();
                }
            }
        }
    }

    changeCurrentDesktop(effects->numberOfDesktops());
    m_isShieldEvent = false;
    m_addingDesktop = false;
    m_previewFile.clear();
}

void MultitaskViewEffect::removeDesktop(int desktop)
{
    m_isShieldEvent = true;
    int count = effects->numberOfDesktops();
    if (desktop <= 0 || desktop > count || count == 1) {
        WorkspaceThumbnail *wt = getWorkspaceObject(m_screen, desktop - 1);
        if (wt) {
            wt->updateGeometry(wt->getRect());
        }
        m_isShieldEvent = false;
        return;
    }
    if (m_hoverDesktop == desktop)
        m_hoverDesktop = -1;
    m_windowEffect.reset();
    m_flyingWinList.clear();
    m_windowInfo.clear();
    m_deleteWorkspaceDesktop = desktop;
     int currentDesktop = effects->currentDesktop();
    if (currentDesktop == desktop) {
        WindowLayoutManager *wmobj = getWinManagerObject(desktop - 1);
        if (wmobj) {
            m_flyingWinList = wmobj->getDesktopWinList();
            wmobj->getDesktopWinInfo(m_windowInfo);
            for (auto it = m_windowInfo.begin(); it != m_windowInfo.end(); ++it) {
                for (const auto& w : it.value().managedWindows()) {
                    // minfo[w] = it.value().transformedGeometry(w);
                    m_windowEffect.add(w, it.value().transformedGeometry(w).toRect(), QRect());
                }
            }
        }
    }

    if (m_motionManagers.size() >= desktop) {
        delete m_motionManagers[desktop - 1];
        m_motionManagers.erase(m_motionManagers.begin() + desktop - 1);
    }
    if (m_workspaceWinMgr.size() >= desktop) {
        delete m_workspaceWinMgr[desktop - 1];
        m_workspaceWinMgr.erase(m_workspaceWinMgr.begin() + desktop - 1);
    }

    QHash<EffectScreen *, EffectWindowList> winManager;
    bool isRelayout = false;
    int newd = 0;
    QSet<EffectScreen *> screens;
    for (const auto &ew : effects->stackingOrder()) {
        if (!isRelevantWithPresentWindows(ew)) {
            continue;
        }
        if (ew->isOnAllDesktops()) {
            if (winManager.contains(ew->screen())) {
                winManager[ew->screen()].push_back(ew);
            } else {
                EffectWindowList list; list.push_back(ew);
                winManager[ew->screen()] = list;
            }
            isRelayout = true;
            screens.insert(ew->screen());
            continue;
        }

        auto dl = ew->desktops();
        int ntmp = desktop == 1 ? 2 : desktop - 1;
        if (static_cast<int>(dl[0]) == ntmp) {
            if (winManager.contains(ew->screen())) {
                winManager[ew->screen()].push_back(ew);
            } else {
                EffectWindowList list; list.push_back(ew);
                winManager[ew->screen()] = list;
            }
        }
        if (dl.size() == 0 || dl[0] < static_cast<uint>(desktop)) {
            continue;
        }

        if (static_cast<int>(dl[0]) == desktop) {
            newd = static_cast<int>(dl[0]) == 1 ? 1 : static_cast<int>(dl[0]) - 1;
            WindowLayoutManager *wmobj = getWinManagerObject(newd - 1);
            WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(newd - 1);
            if (wmobj && wkmobj) {
                isRelayout = true;
                wmobj->manageWin(ew->screen(), ew);
                wkmobj->manageWin(ew->screen(), ew);
                screens.insert(ew->screen());
                if (winManager.contains(ew->screen())) {
                    winManager[ew->screen()].push_back(ew);
                } else {
                    EffectWindowList list; list.push_back(ew);
                    winManager[ew->screen()] = list;
                }
            }
        }
    }

    for (const auto &ew : effects->stackingOrder()) {
        if (ew->isOnAllDesktops()) {
            continue;
        }

        auto dl = ew->desktops();
        if (dl.isEmpty() || static_cast<int>(dl[0]) < desktop) {
            continue;
        }

        if (effectsEx->isTransientWin(ew) && !ew->isModal()) {
            continue;
        }
        newd = dl[0] == 1 ? 1 : dl[0] - 1;
        QVector<uint> desks{(uint)newd};
        effects->windowToDesktops(ew, desks);
    }

    int nrelyout = desktop == 1 ? desktop : (desktop - 1);
    if (isRelayout) {
        for (auto iter = screens.begin(); iter != screens.end(); iter++) {
            WindowMotionManager *wmm;
            WindowLayoutManager *wmobj = getWinManagerObject(nrelyout - 1);
            if (wmobj && wmobj->getMotion(nrelyout, *iter, wmm)) {
                calculateWindowTransformations(winManager.contains(*iter) ? winManager[*iter] : wmm->orderManagedWindows(), *wmm, nrelyout, *iter,
                    m_scale[*iter], m_radius, wmobj, m_maxHeight, m_effectFlyingBack, m_windowEffect, m_flyingWinList, true);
            }
            workspaceWinRelayout(nrelyout, *iter);
        }
    }

    if (currentDesktop == desktop) {
        effects->setCurrentDesktop(nrelyout);
        m_bgSlidingStatus = true;
        m_bgSlidingTimeLine.reset();
        m_curDesktopIndex = nrelyout;
        if (desktop == 1)
            m_curDesktopIndex = desktop;
        m_lastDesktopIndex = desktop;
        m_isRemoveWorkspace = true;
        startremoveDesktopEffect(desktop);
        m_windowEffectState = true;
        m_windowEffect.begin();
        effects->addRepaintFull();
    } else {
        m_curDesktopIndex = effects->currentDesktop();
        if (m_curDesktopIndex > desktop)
            effects->setCurrentDesktop(m_curDesktopIndex == 1 ? m_curDesktopIndex : (m_curDesktopIndex - 1));
        removeDesktopEx(desktop);
    }
}

void MultitaskViewEffect::startremoveDesktopEffect(int desktop)
{
    m_workspaceSlidingStatus = true;
    m_workspaceSlidingTimeline.reset();
    m_workspaceSlidingInfo.clear();
    {
        for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
            QList<WorkspaceThumbnail *> list = m_workspaceBackgrounds[iter.key()];
            for (int j = 0; j < list.size(); j++) {
                if (j == desktop - 1)
                    continue;
                QRect rect = list[j]->getRect();
                m_workspaceSlidingInfo[list[j]].first = rect.x();
            }
        }
    }

    updateWorkspacePos(desktop);
    updateWorkspaceWinLayout(effects->numberOfDesktops() - 1, desktop);

    for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
        QList<WorkspaceThumbnail *> list = m_workspaceBackgrounds[iter.key()];
        for (int j = 0; j < list.size(); j++) {
            if (j == desktop - 1)
                continue;
            QRect rect = list[j]->getRect();
            m_workspaceSlidingInfo[list[j]].second = rect.x();
        }
    }
}

void MultitaskViewEffect::removeDesktopEx(int desktop)
{
    QMutexLocker locker(&m_mutex);
    m_model->removeDesktop(desktop);
    m_isRemoveWorkspace = false;
    m_deleteWorkspaceDesktop = -1;
    for (auto &workspace_list : m_workspaceBackgrounds) {
        delete workspace_list[desktop - 1];
        workspace_list.removeAt(desktop - 1);
        for (int j = desktop - 1; j < workspace_list.size(); j++) {
            workspace_list[j]->setScreenDesktop(workspace_list[j]->screen(), j + 1);
        }
    }

    {
        m_workspaceSlidingStatus = true;
        m_workspaceSlidingTimeline.reset();
        m_workspaceSlidingInfo.clear();
        {
            for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
                QList<WorkspaceThumbnail *> list = m_workspaceBackgrounds[iter.key()];
                for (int j = 0; j < list.size(); j++) {
                    QRect rect = list[j]->getRect();
                    m_workspaceSlidingInfo[list[j]].first = rect.x();
                }
            }
        }
    }

    updateWorkspacePos();
    updateWorkspaceWinLayout(effects->numberOfDesktops());

    {
        for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
            QList<WorkspaceThumbnail *> list = m_workspaceBackgrounds[iter.key()];
            for (int j = 0; j < list.size(); j++) {
                QRect rect = list[j]->getRect();
                m_workspaceSlidingInfo[list[j]].second = rect.x();
            }
        }
    }

    m_isShieldEvent = false;
}

void MultitaskViewEffect::cleanDesktopData(int desktop)
{
    QMutexLocker locker(&m_mutex);
    m_model->removeDesktop(desktop);
    m_isRemoveWorkspace = false;
    m_deleteWorkspaceDesktop = -1;
    for (auto iter = m_workspaceBackgrounds.begin(); iter != m_workspaceBackgrounds.end(); iter++) {
        delete m_workspaceBackgrounds[iter.key()][desktop - 1];
        m_workspaceBackgrounds[iter.key()].removeAt(desktop - 1);
        for (int j = desktop - 1; j < m_workspaceBackgrounds[iter.key()].size(); j++) {
            m_workspaceBackgrounds[iter.key()][j]->setScreenDesktop(iter.key(), j + 1);
        }
    }
    m_isShieldEvent = false;
}

void MultitaskViewEffect::switchDesktop()
{
    for (auto screenInfo : m_screenInfoList) {
        EffectScreen *effectScreen = screenInfo.screen;
        for (int j = 0; j <= m_moveWorkspaceNum; j++) {
            WorkspaceThumbnail *target = getWorkspaceObject(effectScreen, m_aciveMoveDesktop - 1);
            WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(m_aciveMoveDesktop - 1);
            if (target && wkmobj && m_moveWorkspacedirection == mvLeft) {
                WorkspaceThumbnail *nexttarget = getWorkspaceObject(effectScreen, m_aciveMoveDesktop - 2 -j);
                if (nexttarget) {
                    QRect tmpRect = target->getRect();
                    target->setRect(nexttarget->getRect());
                    nexttarget->setRect(tmpRect);

                    if (effectScreen != m_screen) {
                        WindowLayoutManager *wkmobj1 = getWorkspaceWinManagerObject(m_aciveMoveDesktop - 2 - j);
                        if (wkmobj1) {
                            int xdiff = target->getRect().x() - tmpRect.x();
                            wkmobj->updatePos(effectScreen, target->getRect(), QPoint(xdiff, 0));
                            wkmobj1->updatePos(effectScreen, tmpRect, QPoint(-xdiff, 0));
                        }
                    }
                    if (nexttarget->m_posStatus != mvNone) {
                        nexttarget->m_posStatus = mvNone;
                    }
                }
            } else if (target && wkmobj && m_moveWorkspacedirection == mvRight) {
                QRect tmpRect = target->getRect();

                WorkspaceThumbnail *pretarget = getWorkspaceObject(effectScreen, m_aciveMoveDesktop + j);
                if (pretarget) {
                    target->setRect(pretarget->getRect());
                    pretarget->setRect(tmpRect);

                    if (effectScreen != m_screen) {
                        WindowLayoutManager *wkmobj1 = getWorkspaceWinManagerObject(m_aciveMoveDesktop + j);
                        if (wkmobj1) {
                            int xdiff = target->getRect().x() - tmpRect.x();
                            wkmobj->updatePos(effectScreen, target->getRect(), QPoint(xdiff, 0));
                            wkmobj1->updatePos(effectScreen, tmpRect, QPoint(-xdiff, 0));
                        }
                    }
                    if (pretarget->m_posStatus != mvNone) {
                        pretarget->m_posStatus = mvNone;
                    }
                }
            }
        }

        if (m_moveWorkspacedirection == mvLeft) {
            WorkspaceThumbnail *wp = m_workspaceBackgrounds[effectScreen].takeAt(m_aciveMoveDesktop - 1);
            m_workspaceBackgrounds[effectScreen].insert(m_aciveMoveDesktop - 2 - m_moveWorkspaceNum, wp);
        } else if (m_moveWorkspacedirection == mvRight) {
            WorkspaceThumbnail *wp = m_workspaceBackgrounds[effectScreen].takeAt(m_aciveMoveDesktop - 1);
            m_workspaceBackgrounds[effectScreen].insert(m_aciveMoveDesktop + m_moveWorkspaceNum, wp);
        }
    }

    int ncurrent = effects->currentDesktop();
    int index = m_aciveMoveDesktop - 1;
    WindowLayoutManager *wmobj = getWinManagerObject(index);
    WorkspaceThumbnail *target = getWorkspaceObject(m_screen, index);
    WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(index);
    if (wmobj && target && wkmobj && m_moveWorkspacedirection == mvLeft) {
        WorkspaceThumbnail *nexttarget = getWorkspaceObject(m_screen, index - 1 - m_moveWorkspaceNum);
        if (nexttarget) {
            m_motionManagers.insert(index - 1 - m_moveWorkspaceNum, wmobj);
            m_motionManagers.erase(m_motionManagers.begin() + index + 1);

            wkmobj->updatePos(m_screen, nexttarget->getRect(), QPoint(nexttarget->getRect().x() - target->getRect().x(), 0));
            m_workspaceWinMgr.insert(index - 1 - m_moveWorkspaceNum, wkmobj);
            m_workspaceWinMgr.erase(m_workspaceWinMgr.begin() + index + 1);
        }
        desktopSwitchPosition(m_aciveMoveDesktop - 1 - m_moveWorkspaceNum, m_aciveMoveDesktop);
        if (m_aciveMoveDesktop == ncurrent) {
            ncurrent = m_aciveMoveDesktop - 1 - m_moveWorkspaceNum;
        } else if (m_aciveMoveDesktop > ncurrent && m_aciveMoveDesktop - 1 - m_moveWorkspaceNum <= ncurrent) {
            ncurrent += 1;
        }
    } else if (wmobj && target && wkmobj && m_moveWorkspacedirection == mvRight) {
        WorkspaceThumbnail *pretarget = getWorkspaceObject(m_screen, index + 1 + m_moveWorkspaceNum);
        if (pretarget) {
            m_motionManagers.insert(index + 2 + m_moveWorkspaceNum, wmobj);
            m_motionManagers.erase(m_motionManagers.begin() + index);
            wkmobj->updatePos(m_screen, pretarget->getRect(), QPoint(pretarget->getRect().x() - target->getRect().x(), 0));
            m_workspaceWinMgr.insert(index + 2 + m_moveWorkspaceNum, wkmobj);
            m_workspaceWinMgr.erase(m_workspaceWinMgr.begin() + index);

        }
        desktopSwitchPosition(m_aciveMoveDesktop + 1 + m_moveWorkspaceNum, m_aciveMoveDesktop);
        if (m_aciveMoveDesktop == ncurrent) {
            ncurrent = m_aciveMoveDesktop + 1 + m_moveWorkspaceNum;
        } else if (m_aciveMoveDesktop < ncurrent && m_aciveMoveDesktop + 1 + m_moveWorkspaceNum >= ncurrent) {
            ncurrent -= 1;
        }
    }

    int dir = m_moveWorkspacedirection == 1 ? 1 : -1;
    for (const auto& ew: effects->stackingOrder()) {
        if (ew->isOnAllDesktops())
            continue;
        if (effectsEx->isTransientWin(ew) && !ew->isModal() && !ew->isDialog())
            continue;

        auto dl = ew->desktops();
        if (static_cast<int>(dl[0]) == m_aciveMoveDesktop && !ew->isModal()) {
            QVector<uint> desks {static_cast<uint>(m_aciveMoveDesktop - dir - m_moveWorkspaceNum * dir)};
            effects->windowToDesktops(ew, desks);
        } else if (static_cast<int>(dl[0]) >= (m_aciveMoveDesktop - dir - (dir > 0 ? m_moveWorkspaceNum : 0)) && static_cast<int>(dl[0]) <= (m_aciveMoveDesktop - dir + (dir > 0 ? 0 : m_moveWorkspaceNum)) && !ew->isModal()) {
            int newd = dl[0] + dir;
            QVector<uint> desks {(uint)newd};
            effects->windowToDesktops(ew, desks);
        }
    }
    effects->setCurrentDesktop(ncurrent);
}

void MultitaskViewEffect::desktopSwitchPosition(int to, int from)
{
    QList<QString> list = m_screenInfoList.keys();
    for (int i = 0; i < list.size(); i++) {
        QString monitorName = list[i];

        QString strFromUri = m_model->getWorkspaceBackgroundUri(from, monitorName);

        if (from < to) {
            for (int j = from - 1; j < to; j++) {
                int desktopIndex = j + 1; //desktop index
                if ( desktopIndex == to) {
                    m_model->setWorkspaceBackgroundForMonitor(desktopIndex, monitorName, strFromUri);
                } else {
                    QString backgroundUri = m_model->getWorkspaceBackgroundUri(desktopIndex + 1, monitorName);
                    m_model->setWorkspaceBackgroundForMonitor(desktopIndex, monitorName, backgroundUri);
                }
            }
        } else {
            for (int j = from; j > to - 1; j--) {
                if (j == to) {
                    m_model->setWorkspaceBackgroundForMonitor(to, monitorName, strFromUri);
                } else {
                    QString backgroundUri = m_model->getWorkspaceBackgroundUri(j - 1, monitorName);
                    m_model->setWorkspaceBackgroundForMonitor(j, monitorName, backgroundUri);
                }
            }
        }
    }
}

void MultitaskViewEffect::removeWinAndRelayout(EffectWindow *w)
{
    QMutexLocker locker(&m_mutex);

    // Remove window from ALL managers unconditionally to prevent dangling pointers
    for (int wi = 0; wi < m_workspaceWinMgr.size(); wi++) {
        for (auto *s : effects->screens())
            m_workspaceWinMgr[wi]->removeWin(s, w);
    }
    for (int i = 0; i < m_motionManagers.size(); i++) {
        for (auto *s : effects->screens())
            m_motionManagers[i]->removeWin(s, w);
    }

    EffectScreen *screen = w->screen();
    if (w->desktops().isEmpty() || !screen) {
        // Relayout all desktops
        for (int i = 0; i < m_motionManagers.size(); i++) {
            WindowMotionManager *wmm;
            for (auto *s : effects->screens()) {
                if (m_motionManagers[i]->getMotion(i + 1, s, wmm)) {
                    calculateWindowTransformations(wmm->orderManagedWindows(), *wmm, i + 1, s,
                        m_scale[s], m_radius, m_motionManagers[i], m_maxHeight,
                        m_effectFlyingBack, m_windowEffect, m_flyingWinList, true);
                }
            }
        }
    } else {
        int desktop = effects->currentDesktop();
        WindowLayoutManager *wmobj = getWinManagerObject(desktop - 1);
        if (wmobj) {
            WindowMotionManager *wmm;
            if (wmobj->getMotion(desktop, screen, wmm)) {
                calculateWindowTransformations(wmm->orderManagedWindows(), *wmm, desktop, screen,
                    m_scale[screen], m_radius, wmobj, m_maxHeight,
                    m_effectFlyingBack, m_windowEffect, m_flyingWinList, true);
            }
        }
    }

    for (int i = 0; i < effects->numberOfDesktops(); i++) {
        WindowLayoutManager *wmobj = getWinManagerObject(i);
        EffectWindowList list;
        if (wmobj) {
            list = wmobj->getDesktopWinList();
        }
        if (list.size() != 0) {
            return;
        }
    }

}

void MultitaskViewEffect::moveWindowChangeDesktop(EffectWindow *w, int todesktop, EffectScreen *toscreen, bool isSwitch)
{
    if (w->isOnAllDesktops()) {
        return;
    }

    if (effects->numberOfDesktops() < todesktop) {
        return;
    }

    int fromdesktop = w->desktops().first();
    QMutexLocker locker(&m_mutex);
    EffectScreen *screen = w->screen();
    effects->windowToScreen(w, toscreen);
    QVector<uint> desks{(uint)todesktop};
    effects->windowToDesktops(w, desks);

    EffectWindowList mainWinList;
    mainWinList = w->mainWindows();
    for (EffectWindow * mainWin : mainWinList) {
        effects->windowToScreen(mainWin, toscreen);
        QVector<uint> desks{(uint)todesktop};
        effects->windowToDesktops(mainWin, desks);
    }

    EffectWindowList childWinList;
    childWinList = effectsEx->getChildWinList(w);

    if (isSwitch) {
        effects->activateWindow(w);
    }

    WindowLayoutManager *wkmobj = getWorkspaceWinManagerObject(fromdesktop - 1);
    WindowLayoutManager *wkmobj1 = getWorkspaceWinManagerObject(todesktop - 1);
    if (wkmobj && wkmobj1) {
        for (EffectWindow * mainWin : mainWinList) {
            wkmobj->removeWin(screen, mainWin);
            wkmobj1->manageWin(toscreen, mainWin);
        }

        for (EffectWindow *childWin : childWinList) {
            wkmobj->removeWin(screen, childWin);
            wkmobj1->manageWin(toscreen, childWin);
        }

        wkmobj->removeWin(screen, w);
        wkmobj1->manageWin(toscreen, w);
        workspaceWinRelayout(fromdesktop, screen);
        workspaceWinRelayout(todesktop, toscreen);
    }

    WindowLayoutManager *wmobj = getWinManagerObject(fromdesktop - 1);
    if (wmobj) {
        for (EffectWindow * mainWin : mainWinList) {
            wmobj->removeWin(screen, mainWin);
        }

        for (EffectWindow *childWin : childWinList) {
            wmobj->removeWin(screen, childWin);
        }

        wmobj->removeWin(screen, w);
        WindowMotionManager *wmm;
        if (wmobj->getMotion(fromdesktop, screen, wmm)) {
            calculateWindowTransformations(wmm->orderManagedWindows(), *wmm, fromdesktop, screen,
                m_scale[screen], m_radius, wmobj, m_maxHeight,
                m_effectFlyingBack, m_windowEffect, m_flyingWinList, true);
        }
    }

    wmobj = getWinManagerObject(todesktop - 1);
    if (wmobj) {
        for (EffectWindow * mainWin : mainWinList) {
            wmobj->manageWin(toscreen, mainWin);
        }
        for (EffectWindow *childWin : childWinList) {
            wmobj->manageWin(toscreen, childWin);
        }

        wmobj->manageWin(toscreen, w);
        WindowMotionManager *wmmto;
        if (wmobj->getMotion(todesktop, toscreen, wmmto)) {
            calculateWindowTransformations(wmmto->orderManagedWindows(), *wmmto, todesktop, toscreen,
                m_scale[toscreen], m_radius, wmobj, m_maxHeight,
                m_effectFlyingBack, m_windowEffect, m_flyingWinList, true);
        }
    }
    m_curDesktopIndex = effects->currentDesktop();
}

bool MultitaskViewEffect::closeWindow(EffectWindow *w)
{
    if (effectsEx->getChildWinList(w).size() == 0) {
        w->closeWindow();
        removeWinAndRelayout(w);
        effects->addRepaintFull();
        return true;
    }
    return false;
}

void MultitaskViewEffect::setWinKeepAbove(EffectWindow *w)
{
    if (w->keepAbove()) {
        effectsEx->setKeepAbove(w, false);
    } else {
        effectsEx->setKeepAbove(w, true);
    }

    effects->setElevatedWindow(w, false);
    effects->activateWindow(w);
}

void MultitaskViewEffect::changeCurrentDesktop(int desktop)
{
    m_curDesktopIndex = static_cast<int>(effects->currentDesktop());
    effects->setCurrentDesktop(desktop);
    m_hoverWin = nullptr;
    m_flyingWinList.clear();

    if (effects->numberOfDesktops() == 1)
        return;

    //sliding
    m_bgSlidingStatus = true;
    m_bgSlidingTimeLine.reset();
    m_lastDesktopIndex = m_curDesktopIndex;
    m_curDesktopIndex = desktop;
}

bool MultitaskViewEffect::checkConfig(EffectWindow *w)
{
    bool flag = true;
    if (!m_isShowWhole) {
        if (w->isMinimized()) {
            flag = false;
        }
    }
    return flag;
}

QVector<int> MultitaskViewEffect::desktopList(const EffectWindow *w) const
{
    if (w->isOnAllDesktops()) {
        static QVector<int> allDesktops;
        if (allDesktops.count() != effects->numberOfDesktops()) {
            allDesktops.resize(effects->numberOfDesktops());
            for (int i = 0; i < effects->numberOfDesktops(); ++i)
                allDesktops[i] = i+1;
        }
        return allDesktops;
    }

    QVector<int> desks;
    desks.resize(w->desktops().count());
    int i = 0;
    for (const int desk : w->desktops()) {
        desks[i++] = desk;
    }
    return desks;
}

EffectWindow *MultitaskViewEffect::getNextWindow(EffectWindow *w)
{
    EffectWindow *target = nullptr;
    EffectWindowList list;
    WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    if (wmobj) {
        list = wmobj->getDesktopWinList();
    }

    int index = 0;
    for (int i = list.size() - 1; i >= 0; i--) {
        if (w == list[i]) {
            index = i - 1;
            if (index < 0) {
                target = list[list.size() - 1];
            } else {
                target = list[index];
            }
            break;
        }
    }
    return target;
}

EffectWindow *MultitaskViewEffect::getPreWindow(EffectWindow *w)
{
    EffectWindow *target = nullptr;
    EffectWindowList list;
    WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    if (wmobj) {
        list = wmobj->getDesktopWinList();
    }

    int index = 0;
    for (int i = list.size() - 1; i >= 0; i--) {
        if (w == list[i]) {
            index = i + 1;
            if (index >= list.size()) {
                target = list[0];
            } else {
                target = list[index];
            }
            break;
        }
    }
    return target;
}

EffectWindow *MultitaskViewEffect::getHomeOrEndWindow(bool flag)
{
    EffectWindow *target = nullptr;
    EffectWindowList list;
    WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    if (wmobj) {
        list = wmobj->getDesktopWinList();
    }

    if (list.size() <= 0)
        return nullptr;

    if (flag) {
        target = list[list.size() - 1];
    } else {
        target = list[0];
    }
    return target;
}

EffectWindow *MultitaskViewEffect::getNextSameTypeWindow(EffectWindow *w)
{
    EffectWindow *target = nullptr;
    EffectWindowList list;
    WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    if (wmobj) {
        list = wmobj->getDesktopWinList();
    }

    EffectWindowList sameList;
    for (int i = list.size() - 1; i >= 0; i--) {
        if (w->windowClass() == list[i]->windowClass()) {
            sameList.push_back(list[i]);
        }
    }
    if (sameList.size() == 1) {
        target = w;
    } else {
        int index = 0;
        for (int j = 0; j < sameList.size(); j++) {
            if (w == sameList[j]) {
                index = j + 1;
                if (index >= sameList.size()) {
                    target = sameList[0];
                } else {
                    target = sameList[index];
                }
                break;
            }
        }
    }

    return target;
}

EffectWindow *MultitaskViewEffect::getPreSameTypeWindow(EffectWindow *w)
{
    EffectWindow *target = nullptr;
    EffectWindowList list;
    WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    if (wmobj) {
        list = wmobj->getDesktopWinList();
    }

    EffectWindowList sameList;
    for (int i = list.size() - 1; i >= 0; i--) {
        if (w->windowClass() == list[i]->windowClass()) {
            sameList.push_back(list[i]);
        }
    }
    if (sameList.size() == 1) {
        target = w;
    } else {
        int index = 0;
        for (int j = 0; j < sameList.size(); j++) {
            if (w == sameList[j]) {
                index = j - 1;
                if (index < 0) {
                    target = sameList[sameList.size() - 1];
                } else {
                    target = sameList[index];
                }
                break;
            }
        }
    }

    return target;
}

WorkspaceThumbnail *MultitaskViewEffect::getWorkspaceObject(EffectScreen *screen, int secindex)
{
    WorkspaceThumbnail *target = nullptr;
    if (m_workspaceBackgrounds.contains(screen) && secindex >= 0) {
        if (m_workspaceBackgrounds[screen].size() > secindex) {
            target = m_workspaceBackgrounds[screen][secindex];
        }
    }
    return target;
}

WindowLayoutManager *MultitaskViewEffect::getWinManagerObject(int index)
{
    WindowLayoutManager *target = nullptr;
    if (m_motionManagers.size() > index && index >= 0) {
        target = m_motionManagers[index];
    }
    return target;
}

WindowLayoutManager *MultitaskViewEffect::getWorkspaceWinManagerObject(int index)
{
    WindowLayoutManager *target = nullptr;
    if (m_workspaceWinMgr.size() > index && index >= 0) {
        target = m_workspaceWinMgr[index];
    }
    return target;
}

bool MultitaskViewEffect::checkHandlerWorkspace(QPoint pos, EffectScreen *screen, int &desktop)
{
    bool isChecked = false;
    QList<WorkspaceThumbnail *> list = m_workspaceBackgrounds[screen];
    for (int d = 0; d < list.size(); d++) {
        QRect wrect = list[d]->getRect().adjusted(-1, -1, 4, 4);
        QPoint p(list[d]->getRect().topRight().x() - 14 * effectsEx->getOsScale(), list[d]->getRect().y() - 9 * effectsEx->getOsScale());
        QRect brect(p, m_closeWorkspaceFrame->geometry().size());
        if (wrect.contains(pos) || brect.contains(pos)) {
            desktop = d + 1;
            isChecked = true;
            break;
        }
    }

    return isChecked;
}

bool MultitaskViewEffect::touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    Q_UNUSED(time)

    if (!m_activated) {
        return false;
    }
    // only if we don't track a touch id yet
    if (!m_touch.active) {
        m_touch.active = true;
        m_touch.id = id;
        m_touch.pos = pos.toPoint();
        QMouseEvent event(QEvent::MouseButtonPress, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        windowInputMouseEvent(&event);
        m_touch.isPress = true;
    }
    return true;
}

bool MultitaskViewEffect::touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)

    if (!m_activated) {
        return false;
    }
    if (m_touch.active && m_touch.id == id) {
        // only update for the touch id we track
        QMouseEvent event(QEvent::MouseMove, pos, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        m_touch.isMotion = true;
        m_touch.pos = pos.toPoint();
        windowInputMouseEvent(&event);
        m_touch.isMotion = false;
    }
    return true;
}

bool MultitaskViewEffect::touchUp(qint32 id, std::chrono::microseconds time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)

    if (!m_activated) {
        return false;
    }
    if (m_touch.active && m_touch.id == id) {
        m_touch.active = false;
        m_touch.id = 0;
        m_touch.isPress = false;
        QMouseEvent event(QEvent::MouseButtonRelease, m_touch.pos, m_touch.pos, Qt::LeftButton, Qt::LeftButton ,Qt::NoModifier);
        windowInputMouseEvent(&event);
    }
    return true;
}

QPoint MultitaskViewEffect::pointerPos()
{
    if (m_touch.active) {
        return m_touch.pos.toPoint();
    }
    return Effect::cursorPos();
}

void MultitaskViewEffect::motionRepeat()
{
    m_longPressTouch = true;
}

} // namespace KWin


// ---- Merged component implementations follow ----

namespace KWin
{

WorkspaceThumbnail::WorkspaceThumbnail(bool flag)
    : m_backGroundFrame(nullptr)
    , m_desktop(0)
{
    m_backGroundFrame = effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/icon.qml", true);
    m_backGroundFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_workspaceBgFrame = effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/workspacebg.qml", true);
    m_workspaceBgFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_workspaceBgFrame->setRadius(10);
    m_hoverFrame = effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/hover.qml", true);
    m_hoverFrame->setRadius(10);
}

WorkspaceThumbnail::~WorkspaceThumbnail()
{

}

void WorkspaceThumbnail::renderDesktopBackGround(float k)
{
    m_backGroundFrame->setPosition(QPoint(m_fullArea.x() + k * m_fullArea.width(), m_fullArea.y()), true);
    m_backGroundFrame->renderPixmap(infiniteRegion(), 0.6);
}

void WorkspaceThumbnail::renderWorkspaceBackGround(float t, int desktop)
{
    int ncurrentDesktop = static_cast<int>(effects->currentDesktop());
    QRect rect = m_workspaceBgFrame->geometry();

    if (ncurrentDesktop == desktop) {
        QColor color = effectsEx->getActiveColor();
        QRect geoframe = rect;
        geoframe.adjust(-3, -3, 3, 3);
        m_hoverFrame->setGeometry(geoframe);
        m_hoverFrame->setColor(color);
        m_hoverFrame->render(infiniteRegion(), 1, 0);
    }

    m_workspaceBgFrame->render(infiniteRegion(), t, 1);
}

void WorkspaceThumbnail::setPosition(QPoint pos)
{
    m_workspaceBgFrame->setPosition(pos, true);
}

void WorkspaceThumbnail::setImage(const QPixmap &bgPix, const QPixmap &wpPix, const QRect &rect)
{
    m_rect = rect;
    m_currentRect = rect;

    m_backGroundFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_backGroundFrame->setGeometry(m_fullArea);
    m_backGroundFrame->setPosition(m_fullArea.topLeft());
    m_backGroundFrame->setPixmap(bgPix);

    m_workspaceBgFrame->setGeometry(rect);
    m_workspaceBgFrame->setImage(wpPix);
}

void WorkspaceThumbnail::setRect(const QRect rect)
{
    m_rect = rect;
    updateGeometry(rect);
}

QRect WorkspaceThumbnail::getGeometry()
{
    return m_workspaceBgFrame->geometry();
}

void WorkspaceThumbnail::updateGeometry(QRect rect)
{
    m_currentRect = rect;
    m_workspaceBgFrame->setGeometry(rect);
}

WindowFill::WindowFill(EffectScreen *screen, QRect rect, float radius, int maxHeight)
    : m_rect(rect)
    , m_screen(screen)
    , m_maxHeight(maxHeight)
{
    m_fillFrame = effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/hoverbg.qml", false);
    m_fillFrame->setGeometry(rect);
    m_fillFrame->setColor("#A0A0A0");
    m_fillFrame->setRadius(radius);
}

WindowFill::~WindowFill()
{

}

void WindowFill::render()
{
    m_fillFrame->render(infiniteRegion(), 0.5, 0);
}

WindowLayoutManager::WindowLayoutManager()
    : m_desktop(1)
{
}

WindowLayoutManager::~WindowLayoutManager()
{
    for (auto it = m_winManager.begin(); it != m_winManager.end(); it++) {
        it.value()->unmanageAll();
        delete it.value();
    }
    m_winManager.clear();

    QHash<EffectWindow*, WindowFill *>::iterator itf = m_windowFill.begin();
    for (; itf != m_windowFill.end(); ++itf) {
        if (itf.value()) {
            delete itf.value();
            itf.value() = nullptr;
        }
    }
    m_windowFill.clear();
}

void WindowLayoutManager::clearMotion()
{
    QHash<EffectScreen *, WindowMotionManager *>::iterator it;
    for (it = m_winManager.begin(); it != m_winManager.end();) {
        it.value()->unmanageAll();
        ++it;
    }
}

EffectWindowList WindowLayoutManager::orderManagedWindows(int desktop) const
{
    Q_UNUSED(desktop)
    EffectWindowList list;
    for (auto it = m_winManager.begin(); it != m_winManager.end(); it++) {
        list.append(it.value()->orderManagedWindows());
    }
    return list;
}

EffectWindowList WindowLayoutManager::orderManagedWindows(int desktop, EffectScreen *screen) const
{
    Q_UNUSED(desktop)
    if (m_winManager.contains(screen)) {
        return m_winManager[screen]->orderManagedWindows();
    }
    return EffectWindowList();
}

void WindowLayoutManager::manageWin(EffectScreen *screen, EffectWindow *w, int desktop)
{
    m_desktop = desktop;
    if (m_winManager.contains(screen)) {
        m_winManager[screen]->manage(w);
    } else {
        WindowMotionManager *wm = new WindowMotionManager();
        wm->manage(w);
        m_winManager[screen] = wm;
    }
}

void WindowLayoutManager::removeWin(EffectScreen *screen, EffectWindow *w)
{
    if (m_winManager.contains(screen)) {
        m_winManager[screen]->unmanage(w);
    }
}

bool WindowLayoutManager::getMotion(int desktop, EffectScreen *screen, WindowMotionManager *&mgr)
{
    Q_UNUSED(desktop)
    if (m_winManager.contains(screen)) {
        mgr = m_winManager[screen];
        return true;
    }
    return false;
}

int WindowLayoutManager::getDesktopWinNum(int desktop) const
{
    Q_UNUSED(desktop)
    return orderManagedWindows(desktop).size();
}

int WindowLayoutManager::getDesktopWinNumofScreen(EffectScreen *screen) const
{
    if (m_winManager.contains(screen)) {
        return m_winManager[screen]->orderManagedWindows().size();
    }
    return 0;
}

void WindowLayoutManager::resetWindow()
{
    QHash<EffectScreen *, WindowMotionManager *>::iterator it;
    for (it = m_winManager.begin(); it != m_winManager.end();) {
        for (EffectWindow* w : it.value()->managedWindows()) {
            it.value()->moveWindow(w, w->frameGeometry().toRect());
        }
        ++it;
    }
}

void WindowLayoutManager::endMotion()
{
    clearMotion();
}

bool WindowLayoutManager::animating() const
{
    for (auto it = m_winManager.begin(); it != m_winManager.end(); it++) {
        if (it.value()->managedWindows().size() > 0)
            return true;
    }
    return false;
}

void WindowLayoutManager::setWinManager(EffectScreen *screen, WindowMotionManager &wm)
{
    if (m_winManager.contains(screen)) {
        *(m_winManager[screen]) = wm;
    } else {
        m_winManager[screen] = new WindowMotionManager(wm);
    }
}

void WindowLayoutManager::calculate(int tm, bool motionEffectEnable)
{
    for (auto it = m_winManager.begin(); it != m_winManager.end(); it++) {
        it.value()->calculate(tm, motionEffectEnable);
    }
}

void WindowLayoutManager::updatePos(EffectScreen *screen, QRect rect, QPoint diff)
{
    Q_UNUSED(rect)
    if (m_winManager.contains(screen)) {
        for (EffectWindow* w : m_winManager[screen]->orderManagedWindows()) {
            QRectF rect = m_winManager[screen]->transformedGeometry(w);
            rect.translate(diff);
            m_winManager[screen]->setTransformedGeometry(w, rect);
        }
    }
}

EffectWindow *WindowLayoutManager::getHoverWin(QPoint pos, EffectScreen *screen)
{
    if (screen) {
        if (m_winManager.contains(screen)) {
            WindowMotionManager *wm = m_winManager[screen];
            for (const auto& w : wm->managedWindows()) {
                if (!effects->stackingOrder().contains(w)) {
                    const_cast<WindowMotionManager*>(wm)->unmanage(w);
                    continue;
                }
                auto geo = wm->transformedGeometry(w);
                QSize btnSize(24 * effectsEx->getOsScale(), 24 * effectsEx->getOsScale());
                QRect closeRect(QPoint(geo.topRight().x() - 12 * effectsEx->getOsScale(), geo.y() - 10 * effectsEx->getOsScale()), btnSize);
                QRect topRect(QPoint(geo.x() - 12 * effectsEx->getOsScale(), geo.y() - 10 * effectsEx->getOsScale()), btnSize);
                geo.adjust(-5, -5, 5, 5);
                if (geo.contains(pos)
                    || closeRect.contains(pos)
                    || topRect.contains(pos)) {
                    return w;
                } else if (wm->isWindowFill(w) && isHaveWinFill(w)) {
                    auto fillgeo = getWinFill(w)->getRect();
                    closeRect = QRect(QPoint(fillgeo.topRight().x() - 12 * effectsEx->getOsScale(), fillgeo.y() - 10 * effectsEx->getOsScale()), btnSize);
                    topRect = QRect(QPoint(fillgeo.x() - 12 * effectsEx->getOsScale(), fillgeo.y() - 10 * effectsEx->getOsScale()), btnSize);
                    fillgeo.adjust(-5, -5, 5, 5);
                    if (fillgeo.contains(pos)
                        || closeRect.contains(pos)
                        || topRect.contains(pos)) {
                        return w;
                    }
                }
            }
        }
    } else {
        for (auto it = m_winManager.begin(); it != m_winManager.end();) {
            for (const auto& w : it.value()->managedWindows()) {
                if (!effects->stackingOrder().contains(w)) {
                    const_cast<WindowMotionManager*>(it.value())->unmanage(w);
                    continue;
                }
                auto geo = it.value()->transformedGeometry(w);
                geo.adjust(-5, -5, 5, 5);
                if (geo.contains(pos)) {
                    return w;
                }
            }
            ++it;
        }
    }
    return nullptr;
}

QRect WindowLayoutManager::getWindowGeometry(EffectWindow *w, EffectScreen *screen)
{
    QRect rect;
    if (m_winManager.contains(screen)) {
        rect = m_winManager[screen]->targetGeometry(w).toRect();
    }
    return rect;
}

EffectWindowList WindowLayoutManager::getDesktopWinList()
{
    EffectWindowList list;
    for (auto it = m_winManager.begin(); it != m_winManager.end(); it++) {
        list.append(it.value()->orderManagedWindows());
    }

    return list;
}

void WindowLayoutManager::getDesktopWinInfo(QHash<EffectScreen *, WindowMotionManager> &minfo)
{
    for (auto it = m_winManager.begin(); it != m_winManager.end(); it++) {
        minfo[it.key()] = *(it.value());
    }
}

bool WindowLayoutManager::isHaveWinFill(EffectWindow *w)
{
    if (m_windowFill.contains(w)) {
        return true;
    }
    return false;
}

void WindowLayoutManager::setWinFill(EffectWindow *w, WindowFill *fill)
{
    m_windowFill[w] = fill;
}

WindowFill *WindowLayoutManager::getWinFill(EffectWindow *w)
{
    return m_windowFill[w];
}

void WindowLayoutManager::removeWinFill(EffectWindow *w)
{
    delete m_windowFill[w];
    m_windowFill[w] = nullptr;
    m_windowFill.remove(w);
}

void calculateWindowTransformations(EffectWindowList windows, WindowMotionManager& wmm, int desktop,
    EffectScreen *screen, const Scale_st &scale, float radius,
    WindowLayoutManager *layoutManager, int maxHeight,
    MultiTaskEffectFlyingBack &effectFlyingBack, MultiTaskWindowEffect &windowEffect,
    const EffectWindowList &flyingWinList, bool isReLayout)
{
    if (windows.size() == 0)
        return;
    calculateWindowTransformationsClosest(windows, desktop, screen, wmm, scale, radius, layoutManager, maxHeight,
                                          effectFlyingBack, windowEffect, flyingWinList, isReLayout);
}

void calculateWindowTransformationsClosest(EffectWindowList windowlist, int desktop, EffectScreen *screen,
    WindowMotionManager& motionManager, const Scale_st &scale, float radius,
    WindowLayoutManager *layoutManager, int maxHeight,
    MultiTaskEffectFlyingBack &effectFlyingBack, MultiTaskWindowEffect &windowEffect,
    const EffectWindowList &flyingWinList, bool isReLayout)
{
    QHash<EffectWindow*, QRect> targets;

    for (EffectWindow *w : windowlist) {
        QRect rect = w->frameGeometry().toRect();
        targets[w] = rect;
    }

    QRect screenRect = effects->clientArea(MaximizeFullArea, screen, 1).toRect();
    QRect desktopRect = effects->clientArea(MaximizeArea, screen, 1).toRect();
    float scaleHeight = screenRect.height() * FIRST_WIN_SCALE;
    int minSpacingH = scale.spacingHeight;
    int totalw = scale.spacingWidth;
    QRect clientRect = desktopRect;
    clientRect.setY(clientRect.y() + scale.workspaceMgrHeight);

    QList<int> centerList;
    int row = 1;
    int index = 1;
    int xpos = 0;
    float winRadius = 0.0;
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
                float s = (float)(scaleHeight / target->height());
                width = target->width() * s;
            }
            totalw += width;
            totalw += scale.spacingWidth;
            if (width + scale.spacingWidth * 2 > clientRect.width())
                break;

            if (totalw > clientRect.width()) {
                index ++;
                if (index > row)
                    break;
                xpos = ((clientRect.width() - totalw + width + scale.spacingWidth) / 2) + scale.spacingWidth + clientRect.x();
                centerList.push_back(xpos);
                totalw = scale.spacingWidth;
                totalw += width;
                totalw += scale.spacingWidth;
            }
        }
        xpos = ((clientRect.width() - totalw) / 2.0) + scale.spacingWidth + clientRect.x();
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
            totalw = scale.spacingWidth;
        }
    } while (overlap);

    {
        if (radius > 0) {
            winRadius = row <= 2 ? 18.0 : 10.0;
        }
    }

    float winYPos = (clientRect.height() - (index - 1) * minSpacingH - index * scaleHeight) / 2 + clientRect.y();
    row = 1;
    int x = centerList[row - 1];
    totalw = scale.spacingWidth;
    for (int i = windowlist.size() - 1; i >= 0; i--) {
        EffectWindow *w = windowlist[i];
        if (!motionManager.isManaging(w))
            continue;

        QRect *target = &targets[w];
        float width = 0.0, height = 0.0;
        bool isFill = false;
        if (target->height() > scaleHeight) {
            float s = (float)(scaleHeight / target->height());
            width = target->width() * s;
            height = scaleHeight;
        } else {
            width = target->width();
            height = target->height();
            isFill = true;
        }
        totalw += width;
        totalw += scale.spacingWidth;
        if (totalw > clientRect.width()) {
            row++;
            totalw = scale.spacingWidth;
            totalw += width;
            totalw += scale.spacingWidth;
            x = centerList[row - 1];
            winYPos += minSpacingH;
            winYPos += scaleHeight;
        }

        if (isReLayout) {
            motionManager.resetWindowFill(w);
            if (layoutManager && layoutManager->isHaveWinFill(w))
                layoutManager->removeWinFill(w);
        }

        target->setRect(x, winYPos + (scaleHeight - height) / 2, width, height);
        if (isFill) {
            QRect rect(x, winYPos, width, scaleHeight);
            motionManager.setWindowFill(w, true, rect);
            if (layoutManager && !layoutManager->isHaveWinFill(w)) {
                WindowFill *fill = new WindowFill(w->screen(), rect, winRadius, maxHeight);
                layoutManager->setWinFill(w, fill);
            }
        }
        motionManager.setWindowRadius(w, winRadius);
        x += width;
        x += scale.spacingWidth;

        motionManager.moveWindow(w, targets.value(w));

        effectFlyingBack.add(w, targets.value(w));
        if (flyingWinList.contains(const_cast<EffectWindow*>(w)))
            windowEffect.add(w, QRect(), targets.value(w));
    }
}

static WorkspaceThumbnail *getWorkspaceObjectForScreen(
    const QHash<EffectScreen *, QList<WorkspaceThumbnail *>> &workspaceBackgrounds,
    EffectScreen *screen, int secindex)
{
    if (workspaceBackgrounds.contains(screen) && secindex >= 0) {
        const QList<WorkspaceThumbnail *> &list = workspaceBackgrounds[screen];
        if (list.size() > secindex) {
            return list[secindex];
        }
    }
    return nullptr;
}

void calculateWorkSpaceWinTransformations(EffectWindowList windows, WindowMotionManager &wm, int desktop,
    const QHash<EffectScreen *, QList<WorkspaceThumbnail *>> &workspaceBackgrounds,
    int deleteWorkspaceDesktop)
{
    if (0 == windows.size() || 0 == workspaceBackgrounds.size()) {
        return;
    }

    for (int i = 0; i < windows.size(); i++) {
        EffectWindow *w = windows[i];
        if (!w->screen()) continue;
        int idx = (deleteWorkspaceDesktop == 1) ? deleteWorkspaceDesktop : (desktop - 1);
        WorkspaceThumbnail *wkobj = getWorkspaceObjectForScreen(workspaceBackgrounds, w->screen(), idx);
        if (!wkobj)
            continue;
        QPoint pos((w->x() - wkobj->getfullArea().x()) * WORKSPACE_SCALE + wkobj->getRect().x(),
                   (w->y() - wkobj->getfullArea().y()) * WORKSPACE_SCALE + wkobj->getRect().y());
        QSize size(w->width() * WORKSPACE_SCALE, w->height() * WORKSPACE_SCALE);
        QRect rect(pos, size);
        wm.setTransformedGeometry(w, rect);
    }
}

void precalculateWorkSpaceWinTransformations(EffectWindowList windows, WindowMotionManager &wm, int desktop,
    const QHash<EffectScreen *, QList<WorkspaceThumbnail *>> &workspaceBackgrounds)
{
    if (0 == windows.size() || 0 == workspaceBackgrounds.size()) {
        return;
    }

    for (int i = 0; i < windows.size(); i++) {
        EffectWindow *w = windows[i];
        WorkspaceThumbnail *wkobj = getWorkspaceObjectForScreen(workspaceBackgrounds, w->screen(), desktop - 1);
        if (!wkobj)
            continue;
        QPoint pos((w->x() - wkobj->getfullArea().x()) * WORKSPACE_SCALE + wkobj->getRect().x(),
                   (w->y() - wkobj->getfullArea().y()) * WORKSPACE_SCALE + wkobj->getRect().y());
        QSize size(w->width() * WORKSPACE_SCALE, w->height() * WORKSPACE_SCALE);
        QRect rect(pos, size);
        wm.setTransformedGeometry(w, rect);
    }
}

AddWorkspaceButton::AddWorkspaceButton()
{
    m_button = effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/icon.qml", true);
}

AddWorkspaceButton::~AddWorkspaceButton()
{

}

void AddWorkspaceButton::render()
{
    m_button->setRadius(m_fullArea.width() * ADDBTN_RADIUS_SCALE);
    m_button->render(infiniteRegion(), 1, 0);
}

void AddWorkspaceButton::setImage(const QString &btf, const QRect &rect)
{
    m_rect = rect;
    m_button->setImage(QUrl::fromLocalFile(btf));
    m_button->setGeometry(rect);
}

void AddWorkspaceButton::setRect(const QRect &rect)
{
    m_rect = rect;
    m_button->setGeometry(rect);
}

void MultitaskViewEffect::createEffectFrame()
{
    if (!hoverWinFrame()) {
        setHoverWinFrame(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/hover.qml", false));
    }

    if (!closeWinFrame()) {
        setCloseWinFrame(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/icon.qml", true));
        closeWinFrame()->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        closeWinFrame()->setImage(QUrl::fromLocalFile(MULTITASK_CLOSE_SVG));
        closeWinFrame()->setGeometry(QRect(0, 0, 24 * effectsEx->getOsScale(), 24 * effectsEx->getOsScale()));
    }

    if (!topWinFrame()) {
        setTopWinFrame(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/icon.qml", true));
        topWinFrame()->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        topWinFrame()->setGeometry(QRect(0, 0, 24 * effectsEx->getOsScale(), 24 * effectsEx->getOsScale()));
    }

    if (!textWinFrame()) {
        setTextWinFrame(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/text.qml", false));
    }

    if (!textWinBgFrame()) {
        setTextWinBgFrame(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/hoverbg.qml", false));
        textWinBgFrame()->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        textWinBgFrame()->setRadius(10);
        textWinBgFrame()->setColor("#E0E0E0");
    }

    if (!previewFrame()) {
        setPreviewFrame(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/workspacebg.qml", true));
        previewFrame()->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        previewFrame()->setRadius(10);
    }

    if (!closeWorkspaceFrame()) {
        setCloseWorkspaceFrame(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/icon.qml", true));
        closeWorkspaceFrame()->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        closeWorkspaceFrame()->setImage(QUrl::fromLocalFile(MULTITASK_CLOSE_SVG));
        closeWorkspaceFrame()->setGeometry(QRect(0, 0, 24 * effectsEx->getOsScale(), 24 * effectsEx->getOsScale()));
    }

    if (!dragTipsFrame()) {
        setDragTipsFrame(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/dragtips.qml", false));
        dragTipsFrame()->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        dragTipsFrame()->setImage(QUrl::fromLocalFile(delete_workspace_png));
        dragTipsFrame()->setIconSize(QSize(21 * scalingFactor(), 21 * scalingFactor()));
        dragTipsFrame()->setText(tr("Drag upwards to remove"));
        dragTipsFrame()->setColor(QColor(Qt::white));
    }

    if (!dragTipsFrameShadow()) {
        setDragTipsFrameShadow(effectsEx->effectFrameEx("kwin/effects/multitaskview/qml/dragtips.qml", false));
        dragTipsFrameShadow()->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        dragTipsFrameShadow()->setImage(QUrl::fromLocalFile(delete_workspace_png));
        dragTipsFrameShadow()->setIconSize(QSize(22 * scalingFactor(), 21 * scalingFactor()));
        dragTipsFrameShadow()->setText(tr("Drag upwards to remove"));
        dragTipsFrameShadow()->setColor(QColor(Qt::black));
    }
}

void MultitaskViewEffect::showWorkspacePreview(EffectScreen *screen, QRect addButtonRect, bool isClear)
{
    if (!isClear) {
        if (previewFrame()->image().isEmpty() && !previewLoading()) {
            setPreviewLoading(true);
            QPixmap wpPix;
            QSize size(scale()[screen].workspaceWidth, scale()[screen].workspaceHeight);
            QString file = model()->getRandBackground();
            setPreviewFile(file);
            QString realPath = MultitaskViewModel::toRealPath(file);

            auto *fw = new QFutureWatcher<QPixmap>(this);
            connect(fw, &QFutureWatcher<QPixmap>::finished, this, [this, fw, screen, size]() {
                setPreviewLoading(false);
                QPixmap wp = fw->result();
                if (!wp.isNull())
                    previewFrame()->setImage(wp);

                QRect maxRect = effects->clientArea(MaximizeArea, screen, 1).toRect();
                QPoint pos1(maxRect.x() + maxRect.width() - 20 - scale()[screen].workspaceWidth / 2,
                            maxRect.y() + scale()[screen].spacingHeight);
                setPreviewFramePosX(pos1.x());
                previewFrame()->setGeometry(QRect(QPoint(pos1.x(), pos1.y()), size));
                effects->addRepaintFull();
                fw->deleteLater();
            });
            fw->setFuture(QtConcurrent::run(MultitaskViewModel::cutBackgroundPix, size, realPath));
            return;
        }

        QRect rect = previewFrame()->geometry();
        if (effects->waylandDisplay()) {
            EffectScreen *effectscreen = effectsEx->findScreen(effectsEx->getCurrentPaintingScreen());
            if (effectscreen && screen != effectscreen) {
                return;
            }
        } else {
            glEnable(GL_SCISSOR_TEST);
            glScissor(rect.x(), allFullArea().height() - rect.y() - rect.height(), rect.width() / 2 + 20, rect.height());
        }

        previewFrame()->render(infiniteRegion(), 1, 0.8);

        if (nullptr == effects->waylandDisplay()) {
            glDisable(GL_SCISSOR_TEST);
        }
    } else {
        if (previewFrame() && !previewFrame()->image().isEmpty()) {
            previewFrame()->setImage(QUrl());
            setPreviewLoading(false);
        }
    }
}

void MultitaskViewEffect::renderWorkspaceHover(EffectScreen *screen)
{
    WorkspaceThumbnail *wkobj = getWorkspaceObject(screen, hoverDesktop() - 1);
    if (wkobj) {
        QRect rect = wkobj->getRect();
        setWorkspaceCloseBtnArea(QRect(QPoint(rect.x() + rect.width() - 30, rect.y() - 13), QSize(48, 48)));
        closeWorkspaceFrame()->setPosition(QPoint(rect.x() + rect.width() - 14 * effectsEx->getOsScale(), rect.y() - 9 * effectsEx->getOsScale()));
        closeWorkspaceFrame()->render(infiniteRegion(), 1, 0);
    }
}

void MultitaskViewEffect::renderDragWorkspacePrompt(EffectScreen *screen)
{
    if (effects->waylandDisplay()) {
        EffectScreen *effectscreen = effectsEx->findScreen(effectsEx->getCurrentPaintingScreen());
        if (effectscreen && screen != effectscreen) {
            if (isExtensionMode()) {
                return;
            }
        }
    }
    WorkspaceThumbnail *wkobj = getWorkspaceObject(screen, aciveMoveDesktop() - 1);
    if (wkobj) {
        QRect rect = wkobj->getRect();

        QFont font;
        font.setFamily(fontFamily());
        font.setPointSize(13);
        QFontMetrics metrics(font);
        auto metricsRect = metrics.boundingRect(tr("Drag upwards to remove"));
        const int spacing = 13;
        int width = dragTipsFrameShadow()->iconSize().width() + metricsRect.width() + spacing;
        int height = qMax(qMax(dragTipsFrameShadow()->iconSize().height(), metricsRect.height()), 28);

        dragTipsFrameShadow()->setFont(font);
        dragTipsFrameShadow()->setGeometry(QRect(rect.x() + (rect.width() - width) / 2, rect.y() + (rect.height() / 2), width, height));
        dragTipsFrame()->setFont(font);
        dragTipsFrame()->setGeometry(QRect(rect.x() + (rect.width() - width) / 2, rect.y() + (rect.height() / 2), width, height));

        dragTipsFrameShadow()->render(infiniteRegion(), 1, 0);
        dragTipsFrame()->render(infiniteRegion(), 1, 0);

        drawDottedLine(rect, screen);
    }
}

void MultitaskViewEffect::drawDottedLine(const QRect &geo, EffectScreen *screen)
{
    static std::unique_ptr<GLShader> shader = ShaderManager::instance()->generateShaderFromFile(
            ShaderTrait::UniformColor,
            QStringLiteral(":/effects/multitaskview/shaders/dottedline.vert"),
            QStringLiteral(":/effects/multitaskview/shaders/dottedline.frag"));
    GLVertexBuffer* vbo = GLVertexBuffer::streamingBuffer();
    auto area = effects->clientArea(effects->waylandDisplay() ? ScreenArea : FullArea, screen, 0);
    vbo->reset();
    vbo->setColor(QColor(255, 255, 255));
    vbo->setUseColor(true);
    QVector<float> verts;
    verts.reserve(44);
    float x1 = (float)(geo.left() - (effects->waylandDisplay() ? area.left() : 0)) * 2 / area.width() - 1.0;
    float x2 = (float)(geo.right()  - (effects->waylandDisplay() ? area.left() : 0)) * 2 / area.width() - 1.0;
    float y1 = 1.0 - (float)(geo.y() + geo.height() - (effects->waylandDisplay() ? area.top() : 0)) / 2 * 2 / area.height();

    for (int i = 0; i < 22; i++) {
        verts << x1 + (x2 - x1) / 21.0f * i << y1;
    }
    ShaderBinder bind(shader.get());
    vbo->setData(verts.size() / 2, 2, verts.data(), NULL);
    vbo->render(GL_LINES);
}

void MultitaskViewEffect::updateWorkspaceBg(const QString &screenName, int desktop, const QPixmap &bgOverride)
{
    for (auto iter = workspaceBackgrounds().begin(); iter != workspaceBackgrounds().end(); ++iter) {
        if (iter.key()->name() != screenName)
            continue;
        if (desktop < 1 || desktop > iter.value().size())
            continue;
        WorkspaceThumbnail *b = iter.value()[desktop - 1];
        QModelIndex idx = model()->findIndex(screenName, desktop);
        QPixmap bg = bgOverride.isNull()
            ? model()->data(idx, MultitaskViewModel::WorkspaceBgRole).value<QPixmap>()
            : bgOverride;
        QPixmap wpPix = model()->data(idx, MultitaskViewModel::WorkspaceWpRole).value<QPixmap>();
        if (!bg.isNull() || !wpPix.isNull())
            b->setImage(bg, wpPix, b->getRect());
        break;
    }
}

void MultitaskViewEffect::windowInputMouseEvent(QEvent *e)
{
    if (!isReceiveEvent()) {
        return;
    }

    switch (e->type()) {
    case QEvent::MouseMove:
        if (touch().active && !touch().isMotion) {
            return;
        }
        break;
    case QEvent::MouseButtonPress:
        if(touch().active && touch().isPress) {
            return;
        }
        break;
    case QEvent::MouseButtonRelease:
        if(touch().active && touch().isPress) {
            return;
        }
        break;
    case QEvent::Wheel:
        break;
    default:
        return;
    }

    if (e->type() == QEvent::Wheel) {
        auto wheelEvent = static_cast<QWheelEvent *>(e);
        if (wheelEvent->angleDelta().y() < 0) {
            handlerWheelEvent(QX11Info::isPlatformX11() ? Qt::ForwardButton : Qt::BackButton);
        } else if (wheelEvent->angleDelta().y() > 0) {
            handlerWheelEvent(QX11Info::isPlatformX11() ? Qt::BackButton : Qt::ForwardButton);
        }
        return;
    }

    auto mouseEvent = static_cast<QMouseEvent *>(e);
    EffectWindow *target = nullptr;
    bool isAddWorkspace = false;
    bool isHoverWorkspace = false;
    int desktop = -1;
    EffectScreen *screen = nullptr;
    if (!wasWorkspaceMove()) {
        for (auto effectScreen : effects->screens()) {
            if (scale()[effectScreen].workspaceMgrRect.contains(mouseEvent->pos())) {
                isHoverWorkspace = true;
                screen = effectScreen;
                setScreen(effectScreen);
                if (effects->numberOfDesktops() < MAX_DESKTOP_COUNT) {
                    if (addWorkspaceButton()[effectScreen]->getRect().contains(mouseEvent->pos())) {
                        isAddWorkspace = true;
                    }
                }
                break;
            } else if (scale()[effectScreen].windowMgrRect.contains(mouseEvent->pos())) {
                WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
                if (wmobj) {
                    target = wmobj->getHoverWin(mouseEvent->pos(), effectScreen);
                }
                break;
            }
        }
    }

    auto hoverOnNotification = [this] (const QPoint pos)->bool {
        for (EffectWindow *notification : notificationList()) {
            if (notification->frameGeometry().contains(pos))
                return true;
        }
        return false;
    };

    QPoint diff = mouseEvent->pos() - lastWorkspaceMovePos();
    switch (mouseEvent->type()) {
    case QEvent::MouseMove:
    {
        if (!QX11Info::isPlatformX11() && hoverOnNotification(mouseEvent->pos())) {
            setHoverWinBtn(nullptr);
            setHoverDesktop(-1);
            return;
        }

        if (!dockRect().contains(mouseEvent->pos())) {
            if (target) {
                setHoverWin(target);
                setHoverWinBtn(target);
                setHoverDesktop(-1);
            } else if (!wasWorkspaceMove() && (abs(diff.x()) > MOUSE_MOVE_MIN_DISTANCE || (abs(diff.y()) > MOUSE_MOVE_MIN_DISTANCE)) && aciveMoveDesktop() != -1 && m_screen != nullptr) {
                setWorkspaceMoveStartPos(mouseEvent->pos());
                setWasWorkspaceMove(true);
                setHoverDesktop(-1);
            } else if (isHoverWorkspace) {
                if (!checkHandlerWorkspace(mouseEvent->pos(), m_screen, hoverDesktop()))
                    setHoverDesktop(-1);
                setHoverWinBtn(nullptr);
            } else {
                setHoverWinBtn(nullptr);
                setHoverDesktop(-1);
            }
        } else {
            setHoverWinBtn(nullptr);
            setHoverDesktop(-1);
        }

        if (windowMove() && (abs(diff.x()) > MOUSE_MOVE_MIN_DISTANCE || (abs(diff.y()) > MOUSE_MOVE_MIN_DISTANCE))) {
            if (!wasWindowMove()) {
                setWindowMoveStartPos(mouseEvent->pos());
                WindowLayoutManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
                setWindowMoveGeometry(wmobj ? wmobj->getWindowGeometry(windowMove(), windowMove()->screen()) : QRect());

                int width = scale()[windowMove()->screen()].workspaceWidth / 2;
                float scale = (float)width / windowMove()->width();
                int height = windowMove()->height() * scale;
                windowMoveGeometry().setSize(QSize(width, height));
            }
            setWasWindowMove(true);
        }

        if (abs(diff.x()) > MOUSE_MOVE_MIN_DISTANCE || abs(diff.y()) > MOUSE_MOVE_MIN_DISTANCE) {
            setIsShowPreview(isAddWorkspace);
        } else {
            setIsShowPreview(false);
        }

        return;
    }
    case QEvent::MouseButtonPress:
        setIsShowPreview(isAddWorkspace);
        timer()->setSingleShot(true);
        timer()->start(250);
        if (!dockRect().contains(mouseEvent->pos())) {
            if (target) {
                setWindowMove(target);
                setWindowMoveDiff(mouseEvent->pos() - target->pos().toPoint());
            } else if (isHoverWorkspace && !isAddWorkspace) {
                if (checkHandlerWorkspace(mouseEvent->pos(), screen, aciveMoveDesktop())) {
                    setScreen(screen);
                }
            }
            setLastWorkspaceMovePos(mouseEvent->pos());
        }

        break;
    case QEvent::MouseButtonRelease:
        timer()->stop();
        if (!wasWindowMove() && !wasWorkspaceMove() && dockRect().contains(mouseEvent->pos())) {
            setDelayDbus(false);
            effectFlyingBack().begin();
            effects->addRepaintFull();
            setCursorPos(mouseEvent->pos());
            if (QX11Info::isPlatformX11()) {
                setButtonType(mouseEvent->button());
            } else {
                setSendButton(mouseEvent->button());
            }
        } else if (!wasWindowMove() && !QX11Info::isPlatformX11() && hoverOnNotification(mouseEvent->pos())) {
            effectFlyingBack().begin();
            effects->addRepaintFull();
            setCursorPos(mouseEvent->pos());
            setSendButton(mouseEvent->button());
        } else if (target) {
            bool isPressBtn = false;
            int btn = 0;
            for (; btn < winBtnArea().size(); ++btn) {
                if (winBtnArea()[btn].contains(mouseEvent->pos())) {
                    isPressBtn = true;
                    break;
                }
            }
            if (isPressBtn) {
                if (btn == 0) {
                    closeWindow(target);
                    setHoverWin(nullptr);
                    winBtnArea().clear();
                } else if (btn == 1) {
                    setWinKeepAbove(target);
                }
            } else if (!wasWindowMove() && !longPressTouch()) {
                effects->defineCursor(Qt::PointingHandCursor);
                effects->setElevatedWindow(target, false);
                effects->activateWindow(target);

                effectFlyingBack().setSelecedWindow(target);
                effectFlyingBack().begin();
            }
        } else if (wasWindowMove() && isHoverWorkspace) {
            checkHandlerWorkspace(mouseEvent->pos(), screen, desktop);
            if (desktop != -1 && !windowMove()->isOnDesktop(desktop)) {
                moveWindowChangeDesktop(windowMove(), desktop, screen);
            } else {
                WorkspaceThumbnail *wkobj = getWorkspaceObject(screen, effects->numberOfDesktops() - 1);
                if (wkobj) {
                    QRect bgrect = QRect(wkobj->getRect().x() + wkobj->getRect().width(), scale()[screen].workspaceMgrRect.y(),
                                         scale()[screen].workspaceMgrRect.x() + scale()[screen].workspaceMgrRect.width() - wkobj->getRect().x() - wkobj->getRect().width(), scale()[screen].workspaceMgrRect.height());
                    if (bgrect.contains(mouseEvent->pos()) && effects->numberOfDesktops() < 6) {
                        addNewDesktop();
                        moveWindowChangeDesktop(windowMove(), effects->numberOfDesktops(), screen, true);
                        setOpacityStatus(true);
                    }
                }
            }
        } else if (isHoverWorkspace && !wasWorkspaceMove() && !wasWindowMove()) {
            if (isAddWorkspace) {
                addNewDesktop();
            } else if (workspaceCloseBtnArea().contains(mouseEvent->pos())) {
                removeDesktop(aciveMoveDesktop());
            } else {
                checkHandlerWorkspace(mouseEvent->pos(), screen, desktop);

                if (desktop == effects->currentDesktop() || desktop == -1) {
                    effectFlyingBack().begin();
                    effects->addRepaintFull();
                } else {
                    changeCurrentDesktop(desktop);
                }
            }
        } else if (workspaceStatusVal() == MultitaskViewEffect::wpSwitch) {
            switchDesktop();
        } else if (workspaceStatusVal() == MultitaskViewEffect::wpDelete) {
            removeDesktop(aciveMoveDesktop());
        } else if (workspaceStatusVal() == MultitaskViewEffect::wpRestore) {
            WorkspaceThumbnail *wt = getWorkspaceObject(m_screen, aciveMoveDesktop() - 1);
            if (wt)
                wt->updateGeometry(wt->getRect());
        } else if (!wasWorkspaceMove() && !wasWindowMove()) {
            effectFlyingBack().begin();
            effects->addRepaintFull();
        }

        setWorkspaceStatus(MultitaskViewEffect::wpNone);
        setAciveMoveDesktop(-1);
        setWasWorkspaceMove(false);
        setMoveWorkspaceNum(-1);
        setMoveWorkspacedirection(mvNone);
        setDragWorkspacedirection(dragNone);
        setWindowMove(nullptr);
        setWasWindowMove(false);
        setLongPressTouch(false);

        break;
    default:
        return;
    }
}

void MultitaskViewEffect::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (!isReceiveEvent()) {
        return;
    }

    if (e->type() == QEvent::KeyPress) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (shortcutKeys().contains(e->key() + e->modifiers()) ||
            shortcutAKeys().contains(e->key() + e->modifiers()) ||
            shortcutWKeys().contains(e->key() + e->modifiers())) {
            toggle();
            return;
        }
#else
        if (shortcutKeys().contains(e->key() | e->modifiers()) || shortcutAKeys().contains(e->key() | e->modifiers()) || shortcutWKeys().contains(e->key() | e->modifiers())) {
            toggle();
            return;
        }
#endif
        switch (e->key()) {
        case Qt::Key_Escape:
            effectFlyingBack().begin();
            effects->addRepaintFull();
            break;
        case Qt::Key_1:
        case Qt::Key_2:
        case Qt::Key_3:
        case Qt::Key_4:
        case Qt::Key_5:
        case Qt::Key_6:
            if (e->modifiers() == Qt::MetaModifier ||
                    e->modifiers() == (Qt::MetaModifier|Qt::KeypadModifier)) {
                int index = e->key() - Qt::Key_0;
                if (effects->numberOfDesktops() >= index) {
                    if (effects->currentDesktop() != index) {
                        changeCurrentDesktop(index);
                    } else {
                        effectFlyingBack().begin();
                        effects->addRepaintFull();
                    }
                }
            }
            break;
        case Qt::Key_Exclam:
        case Qt::Key_At:
        case Qt::Key_NumberSign:
        case Qt::Key_Dollar:
        case Qt::Key_Percent:
        case Qt::Key_AsciiCircum:
            if (e->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier)) {
                int target_desktop = 1;
                switch(e->key()) {
                case Qt::Key_Exclam:      target_desktop = 1; break;
                case Qt::Key_At:          target_desktop = 2; break;
                case Qt::Key_NumberSign:  target_desktop = 3; break;
                case Qt::Key_Dollar:      target_desktop = 4; break;
                case Qt::Key_Percent:     target_desktop = 5; break;
                case Qt::Key_AsciiCircum: target_desktop = 6; break;
                default: break;
                }
                if (hoverWin() && !(effects->numberOfDesktops() < target_desktop) && effects->currentDesktop() != target_desktop) {
                    moveWindowChangeDesktop(hoverWin(), target_desktop, hoverWin()->screen());
                    setHoverWin(nullptr);
                }
            }
            break;
        case Qt::Key_Home:
            if (e->modifiers() == Qt::NoModifier) {
                setHoverWin(getHomeOrEndWindow(true));
            }
            break;
        case Qt::Key_End:
            if (e->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier)) {
                if (hoverWin() && effects->currentDesktop() != 1) {
                    moveWindowChangeDesktop(hoverWin(), 1, hoverWin()->screen());
                    setHoverWin(nullptr);
                }
            } else if (e->modifiers() == Qt::NoModifier) {
                setHoverWin(getHomeOrEndWindow(false));
            }
            break;
        case Qt::Key_PageDown:
        case Qt::Key_Clear:
            if (e->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier)) {
                int target_desktop = 3;
                switch (e->key())
                {
                case Qt::Key_PageDown:  target_desktop = 3; break;
                case Qt::Key_Clear:     target_desktop = 5; break;
                default: break;
                }
                if (hoverWin() && !(effects->numberOfDesktops() < target_desktop) && effects->currentDesktop() != target_desktop) {
                    moveWindowChangeDesktop(hoverWin(), target_desktop, hoverWin()->screen());
                    setHoverWin(nullptr);
                }
            }
            break;
        case Qt::Key_Down:
            if (e->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier)) {
                if (hoverWin() && !(effects->numberOfDesktops() < 2) && effects->currentDesktop() != 2) {
                    moveWindowChangeDesktop(hoverWin(), 2, hoverWin()->screen());
                    setHoverWin(nullptr);
                }
            } else if (e->modifiers() == Qt::NoModifier) {
                if (hoverWin()) {
                    setHoverWin(getNextWindow(hoverWin()));
                }
            }
            break;
        case Qt::Key_Up:
            if (e->modifiers() == Qt::NoModifier) {
                if (hoverWin()) {
                    setHoverWin(getPreWindow(hoverWin()));
                }
            }
            break;
        case Qt::Key_Equal:
            if (e->modifiers() == Qt::AltModifier) {
                if(!addingDesktopTimer().isActive())
                    addingDesktopTimer().start();
            }
            break;
        case Qt::Key_Plus:
            if (e->modifiers() == (Qt::AltModifier|Qt::KeypadModifier)) {
                if(!addingDesktopTimer().isActive())
                    addingDesktopTimer().start();
            }
            break;
        case Qt::Key_Minus:
            if (e->modifiers() == Qt::AltModifier ||
                e->modifiers() == (Qt::AltModifier|Qt::KeypadModifier)) {
                removeDesktop(effects->currentDesktop());
            }
            break;
        case Qt::Key_Tab:
            if (e->modifiers() == Qt::NoModifier) {
                if (hoverWin()) {
                    setHoverWin(getNextWindow(hoverWin()));
                } else {
                    setHoverWin(getHomeOrEndWindow(true));
                }
            }
            break;
        case Qt::Key_Backtab:
            if (e->modifiers() == Qt::ShiftModifier) {
                if (hoverWin()) {
                    setHoverWin(getPreWindow(hoverWin()));
                }
            }
            break;
        case Qt::Key_Right:
            if (e->modifiers() == Qt::MetaModifier) {
                int index = effects->currentDesktop() + 1;
                if (index > effects->numberOfDesktops()) {
                    changeCurrentDesktop(1);
                } else {
                    changeCurrentDesktop(index);
                }
            } else if (e->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier)) {
                if (hoverWin() && !(effects->numberOfDesktops() < 6) && effects->currentDesktop() != 6) {
                    moveWindowChangeDesktop(hoverWin(), 6, hoverWin()->screen());
                    setHoverWin(nullptr);
                }
            } else if (e->modifiers() == Qt::NoModifier) {
                if (hoverWin()) {
                    setHoverWin(getNextWindow(hoverWin()));
                }
            }
            break;
        case Qt::Key_Left:
            if (e->modifiers() == Qt::MetaModifier) {
                int index = effects->currentDesktop() - 1;
                if (index <= 0) {
                    changeCurrentDesktop(effects->numberOfDesktops());
                } else {
                    changeCurrentDesktop(index);
                }
            } else if (e->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier)) {
                if (hoverWin()) {
                    moveWindowChangeDesktop(hoverWin(), 4, hoverWin()->screen());
                }
            } else if (e->modifiers() == Qt::NoModifier) {
                if (hoverWin()) {
                    setHoverWin(getPreWindow(hoverWin()));
                }
            }
            break;
        case Qt::Key_Return:
            if (hoverWin()) {
                effects->defineCursor(Qt::PointingHandCursor);
                effects->setElevatedWindow(hoverWin(), false);
                effects->activateWindow(hoverWin());

                effectFlyingBack().setSelecedWindow(hoverWin());
                effectFlyingBack().begin();
            }
            break;
        case Qt::Key_Delete:
            if (e->modifiers() == Qt::NoModifier) {
                if (hoverWin() && closeWindow(hoverWin())) {
                    setHoverWin(nullptr);
                }
            }
            break;
        case Qt::Key_L:
            if (e->modifiers() == Qt::MetaModifier) {
                setActive(false);
                QString sessionPath = workspace()->getProcessSessionPath();
                if (!sessionPath.isEmpty()) {
                    QDBusMessage message = QDBusMessage::createMethodCall(DBUS_LOGIN_SERVICE, sessionPath,
                            DBUS_SESSION_INTF, "Lock");
                    QDBusConnection::systemBus().asyncCall(message);
                }
            }
            break;
        case Qt::Key_QuoteLeft:
            if (e->modifiers() == Qt::AltModifier) {
                if (hoverWin()) {
                    setHoverWin(getNextSameTypeWindow(hoverWin()));
                }
            }
            break;
        case Qt::Key_AsciiTilde:
            if (e->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)) {
                if (hoverWin()) {
                    setHoverWin(getPreSameTypeWindow(hoverWin()));
                }
            }
            break;
        case Qt::Key_Space:
            if (e->modifiers() == (Qt::AltModifier)) {
                break;
            }
            [[fallthrough]];
        default:
            if (!QX11Info::isPlatformX11() && effectsEx->isShortcuts(e)) {
                QTimer::singleShot(20, [this]() {
                    effects->ungrabKeyboard();
                    setHasKeyboardGrab(false);
                    });
            }
            break;
        }
    }
}

void MultitaskViewEffect::handlerWheelEvent(Qt::MouseButtons btn)
{
    static bool is_smooth_scrolling = false;
    if (is_smooth_scrolling)
        return;
    if (btn == Qt::ForwardButton) {
        is_smooth_scrolling = true;
        if (effects->currentDesktop() + 1 <= effects->numberOfDesktops()) {
            changeCurrentDesktop(effects->currentDesktop() + 1);
        } else {
            changeCurrentDesktop(1);
        }
    } else if (btn == Qt::BackButton) {
        is_smooth_scrolling = true;
        if (effects->currentDesktop() - 1 >= 1) {
            changeCurrentDesktop(effects->currentDesktop() - 1);
        } else {
            changeCurrentDesktop(effects->numberOfDesktops());
        }
    }
    QTimer::singleShot(400, [&]() { is_smooth_scrolling = false; });
}

void MultitaskViewEffect::relayDockEvent(QPoint pos, int button)
{
    if (dock()) {
        auto cl = static_cast<EffectWindowImpl *>(dock())->window();
        xcb_button_release_event_t c;
        memset(&c, 0, sizeof(c));

        c.response_type = XCB_BUTTON_PRESS;
        c.event = cl->window();
        c.event_x = pos.x() - dockRect().x();
        c.event_y = pos.y() - dockRect().y();
        c.detail = (button == 1 ? 1 : 3);
        xcb_send_event(connection(), false, c.event, XCB_EVENT_MASK_BUTTON_PRESS, reinterpret_cast<const char *>(&c));
        xcb_flush(connection());

        memset(&c, 0, sizeof(c));
        c.response_type = XCB_BUTTON_RELEASE;
        c.event = cl->window();
        c.event_x = pos.x() - dockRect().x();
        c.event_y = pos.y() - dockRect().y();
        c.detail = (button == 1 ? 1 : 3);
        xcb_send_event(connection(), false, c.event, XCB_EVENT_MASK_BUTTON_RELEASE, reinterpret_cast<const char *>(&c));
        xcb_flush(connection());
    }
}

void MultitaskViewEffect::onWindowAdded(EffectWindow *w)
{
    if (!isActivated())
        return;

    w->refVisibleEx(EffectWindow::PAINT_DISABLED_BY_MINIMIZE | EffectWindow::PAINT_DISABLED_BY_DESKTOP);

    if (w->isDock()) {
        setDockRect(w->frameGeometry().toRect());
        setDock(w);
        onDockChange("");
    } else if (!QX11Info::isPlatformX11() && w->windowClass() == notification_tips) {
        notificationList().push_back(w);
    } else if (!QX11Info::isPlatformX11() && w->caption() == "org.deepin.dde.lock") {
        setActive(false);
    } else if (QX11Info::isPlatformX11()) {
        if (w->windowClass() != screen_recorder
            && w->windowClass() != notification_tips
            && w->windowClass() != split_outline
            && !w->isUtility()) {
            effectFlyingBack().begin();
            effects->addRepaintFull();
        } else if (w->windowClass() == screen_recorder) {
            {
                setWasWorkspaceMove(false);
                if (WorkspaceThumbnail *wt = getWorkspaceObject(screen(), aciveMoveDesktop() - 1))
                    wt->updateGeometry(wt->getRect());
                setAciveMoveDesktop(-1);
                setMoveWorkspaceNum(-1);
                setMoveWorkspacedirection(mvNone);
                setDragWorkspacedirection(dragNone);
                setWorkspaceStatus(MultitaskViewEffect::wpNone);
                setWindowMove(nullptr);
                setWasWindowMove(false);
            }
            {
                if (hasKeyboardGrab())
                    effects->ungrabKeyboard();
                setHasKeyboardGrab(false);
            }
            if (isScreenRecorder()) {
                setScreenRecorderMenu(w);
            } else {
                effects->stopMouseInterception(this);
                setIsScreenRecorder(true);
            }
        }
    } else if (!QX11Info::isPlatformX11()) {
        if (isScreenRecorder()) {
            setScreenRecorderMenu(w);
        } else if (w->windowClass() == screen_recorder) {
            {
                setWasWorkspaceMove(false);
                if (WorkspaceThumbnail *wt = getWorkspaceObject(screen(), aciveMoveDesktop() - 1))
                    wt->updateGeometry(wt->getRect());
                setAciveMoveDesktop(-1);
                setMoveWorkspaceNum(-1);
                setMoveWorkspacedirection(mvNone);
                setDragWorkspacedirection(dragNone);
                setWorkspaceStatus(MultitaskViewEffect::wpNone);
                setWindowMove(nullptr);
                setWasWindowMove(false);
            }
            effects->stopMouseInterception(this);
            setIsScreenRecorder(true);
        } else if (w->windowClass() != notification_tips && w->caption() != "deepin-splitoutline") {
            setActive(false);
        }
    }
}

void MultitaskViewEffect::onWindowDeleted(EffectWindow *w)
{
    if (!isActivated())
        return;

    if (w->isDock()) {
        setDock(nullptr);
        dockRect().setSize(QSize(0, 0));
    } else if (!QX11Info::isPlatformX11() && w->windowClass() == notification_tips) {
        auto it = std::find(notificationList().begin(), notificationList().end(), w);
        if (it != notificationList().end())
            notificationList().erase(it);
        if (notificationList().empty())
            setIsCloseScreenRecorder(false);
        return;
    } else if (w->windowClass() == screen_recorder && w != screenRecorderMenu()) {
        setIsScreenRecorder(false);
        if (!QX11Info::isPlatformX11())
            setIsCloseScreenRecorder(true);
        return;
    } else if (w == screenRecorderMenu()) {
        setScreenRecorderMenu(nullptr);
        return;
    } else {
        removeWinAndRelayout(w);
        setIsShieldEvent(false);
        effects->addRepaintFull();
    }
}

void MultitaskViewEffect::onWindowClosed(EffectWindow *w)
{
    if (!isActivated())
        return;

    if (w->windowClass() == screen_recorder && w != screenRecorderMenu()) {
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        if (QX11Info::isPlatformX11()) {
            setHasKeyboardGrab(effects->grabKeyboard(this));
        }
    } else if (w->isDock()) {
        setDock(nullptr);
        dockRect().setSize(QSize(0, 0));
    } else if (!QX11Info::isPlatformX11() && w->windowClass() == notification_tips) {
        auto it = std::find(notificationList().begin(), notificationList().end(), w);
        if (it != notificationList().end())
            notificationList().erase(it);
    } else {
        removeWinAndRelayout(w);
        setIsShieldEvent(false);
        effects->addRepaintFull();
    }
}

void MultitaskViewEffect::onCloseEffect(bool isSleepBefore)
{
    if (isSleepBefore && m_activated) {
        invalidateWorkspaceCache();
        setActive(false);
    }
}

void MultitaskViewEffect::onDockChange(const QString &key)
{
    QString position = _gsettings_dde_dock->get(GsettingsDockPosition).toString();
    int height = _gsettings_dde_dock->get(GsettingsDockHeight).toInt();
    if (position == "bottom") {
        if (dockRect().height() < height) {
            dockRect().setY(dockRect().height() + dockRect().y() - height);
            dockRect().setHeight(height);
        }
    } else if (position == "top") {
        if (dockRect().height() < height) {
            dockRect().setHeight(height);
        }
    } else if (position == "left") {
        if (dockRect().width() < height) {
            dockRect().setWidth(height);
        }
    } else if (position == "right") {
        if (dockRect().width() < height) {
            dockRect().setX(dockRect().width() + dockRect().x() - height);
            dockRect().setWidth(height);
        }
    }
}

void MultitaskViewEffect::screenRecorderStart()
{
    if (isScreenRecorder()) {
        effects->startMouseInterception(this, Qt::PointingHandCursor);
    }
}

void MultitaskViewEffect::fontChanged(const QString &fontType, const QString &fontName)
{
    if (fontType == "standardfont" || fontType == "monospacefont") {
        setFontFamily(fontName);
    }
}


} // namespace KWin
