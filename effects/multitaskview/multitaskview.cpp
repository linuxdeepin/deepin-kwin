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
#include <QtMath>
#include <QScreen>
#include <QX11Info>
#include <QTranslator>
#include <kwinglplatform.h>
#include <kwinglutils.h>
#include <effects.h>
#include <kglobalaccel.h>
#include <qdbusconnection.h>
#include <qdbusinterface.h>
#include <qdbusreply.h>
#include <dlfcn.h>
#include <QGSettings/qgsettings.h>
#include <QImageReader>
#include "multitouchgesture.h"
#include "kwineffectsex.h"
#include "report.h"

Q_GLOBAL_STATIC_WITH_ARGS(QGSettings, _gsettings_dde_dock, ("com.deepin.dde.dock"))
//Q_GLOBAL_STATIC_WITH_ARGS(QGSettings, _gsettings_dde_dock_primary, ("com.deepin.dde.dock.mainwindow"))
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

#define DBUS_APPEARANCE_SERVICE  "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ      "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF     "com.deepin.daemon.Appearance"

#define DBUS_DEEPIN_WM_SERVICE   "com.deepin.wm"
#define DBUS_DEEPIN_WM_OBJ       "/com/deepin/wm"
#define DBUS_DEEPIN_WM_INTF      "com.deepin.wm"

#define DBUS_DAEMON_DISPLAY_SERVICE "com.deepin.daemon.Display"
#define DBUS_DAEMON_DISPLAY_OBJ     "/com/deepin/daemon/Display"
#define DBUS_DAEMON_DISPLAY_INIF    "com.deepin.daemon.Display"

#define MULTITASK_CLOSE_SVG      ":/resources/themes/multiview_delete.svg"
#define MULTITASK_TOP_SVG        ":/resources/themes/multiview_top.svg"
#define MULTITASK_TOP_ACTIVE_SVG ":/resources/themes/multiview_top_active.svg"

#define POPUP_TIME_SCALE 15.0f
#define EFFECT_DURATION_DEFAULT 400
#define SCISSOR_HOFFU 200
#define SCISSOR_HOFFD 400

const char notification_tips[] = "dde-osd dde-osd";
const char screen_recorder[] = "deepin-screen-recorder deepin-screen-recorder";
const char fallback_background_name[] = "file:///usr/share/wallpapers/deepin/desktop.jpg";
const char previous_default_background_name[] = "file:///usr/share/backgrounds/default_background.jpg";
const char add_workspace_png[] = ":/resources/themes/add-light.png";//":/resources/themes/add-light.svg";
const char delete_workspace_png[] = ":/resources/themes/workspace_delete.png";

namespace KWin
{
QPoint calculateOffSet(const QRect& rect, const QRect& screenArea, const int maxHeight)
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

CustomThread::CustomThread(BgInfo_st &st) 
    : QThread()
    , m_st(st)
{

}

void CustomThread::run()
{
    MultiViewBackgroundManager::instance()->cacheWorkspaceBg(m_st);
}

MultiViewBackgroundManager *MultiViewBackgroundManager::_instance = new MultiViewBackgroundManager();
MultiViewBackgroundManager *MultiViewBackgroundManager::instance()
{
    return _instance;
}

MultiViewBackgroundManager::MultiViewBackgroundManager()
    : QObject()
    , m_bgmutex(QMutex::Recursive)
{
    QStringList lst = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    if (lst.size() > 0) {
        m_deepinwmrcIni = new QSettings(lst[0] + "/deepinwmrc", QSettings::IniFormat);
    }
}

MultiViewBackgroundManager::~MultiViewBackgroundManager()
{
    if (m_deepinwmrcIni) {
        delete m_deepinwmrcIni;
        m_deepinwmrcIni = nullptr;
    }
}

static QString toRealPath(const QString &path)
{
    QString res = path;
    if (res.startsWith("file:///")) {
        res.remove("file://");
    }

    QFileInfo fi(res);
    if (fi.isSymLink()) {
        res = fi.symLinkTarget();
    }
    return res;
}

QPixmap MultiViewBackgroundManager::cutBackgroundPix(const QSize &size, const QString &file)
{
    QImageReader imageReader;
    imageReader.setFileName(file);
    imageReader.setAutoTransform(true);
    auto imageSize = imageReader.size();
    auto targetScaleSize = imageSize.scaled(size, Qt::KeepAspectRatioByExpanding);

    imageReader.setScaledSize(targetScaleSize);
    QPixmap pixmap = QPixmap::fromImageReader(&imageReader);

    if(pixmap.width() > size.width() || pixmap.height() > size.height()) {
        pixmap = pixmap.copy(QRect(static_cast<int>((pixmap.width() - size.width()) / 2.0),
                                    static_cast<int>((pixmap.height() - size.height()) / 2.0), size.width(), size.height()));
    }

    return pixmap;
}

QPixmap MultiViewBackgroundManager::getCachePix(const QSize &size, QPair<QSize, QPixmap> &pair)
{
    if (pair.first == size) {
        return pair.second;
    } else if (pair.first.width() > size.width() && pair.first.height() > size.height()) {
        pair.first = size;
        pair.second = pair.second.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        pair.second = pair.second.copy(QRect(static_cast<int>((pair.second.width() - size.width()) / 2.0),
                                       static_cast<int>((pair.second.height() - size.height()) / 2.0), size.width(), size.height()));
        return pair.second;
    }
    return QPixmap();
}

void MultiViewBackgroundManager::getWorkspaceBgPath(BgInfo_st &st, QPixmap &desktopBg, QPixmap &workspaceBg)
{
    QString strBackgroundPath = QString("%1%2").arg(st.desktop).arg(st.screenName);

    QString backgroundUri;
    QFile f(QDir::homePath() + "/.config/deepinwmrc");
    if(!f.open(QFile::ReadOnly)) {
        qDebug()  << "open faild!!! " << QDir::homePath() + "/.config/deepinwmrc";
        return;
    }
    while (!f.atEnd()) {
        QString line = f.readLine();
        line.remove("\n");
        if(line.contains(QString("%1@").arg(st.desktop) + st.screenName)) {
            backgroundUri = line.split('=').last();
            break;
        }
    }
    f.close();
    m_currentBackgroundList.insert(backgroundUri);
    backgroundUri = toRealPath(backgroundUri);

    if (m_bgCachedPixmaps.contains(backgroundUri + strBackgroundPath)) {
        auto& p = m_bgCachedPixmaps[backgroundUri + strBackgroundPath];
        desktopBg = getCachePix(st.desktopSize, p);
    }
    if (desktopBg.isNull()) {
        QPixmap pixmap = cutBackgroundPix(st.desktopSize, backgroundUri);
        m_bgCachedPixmaps[backgroundUri + strBackgroundPath] = qMakePair(st.desktopSize, pixmap);
        desktopBg = pixmap;
    }

    if (m_wpCachedPixmaps.contains(backgroundUri + strBackgroundPath)) {
        auto& p = m_wpCachedPixmaps[backgroundUri + strBackgroundPath];

        workspaceBg = getCachePix(st.workspaceSize, p);
    }
    if (workspaceBg.isNull()) {
        QPixmap pixmap = cutBackgroundPix(st.workspaceSize, backgroundUri);
        m_wpCachedPixmaps[backgroundUri + strBackgroundPath] = qMakePair(st.workspaceSize, pixmap);
        workspaceBg = pixmap;
    }
}

void MultiViewBackgroundManager::cacheWorkspaceBg(BgInfo_st &st)
{
    QMutexLocker locker(&m_bgmutex);
    if (m_deepinwmrcIni) {
        QString backgroundUri;
        QString strBackgroundPath = QString("%1@%2").arg(st.desktop).arg(st.screenName);
        backgroundUri = m_deepinwmrcIni->value("WorkspaceBackground/" + strBackgroundPath).toString();
        QString strBackgroundPathkey = QString("%1%2").arg(st.desktop).arg(st.screenName);

        if (!backgroundUri.isEmpty()) {
            backgroundUri = toRealPath(backgroundUri);
            QPixmap pixmap = cutBackgroundPix(st.desktopSize, backgroundUri);
            m_bgCachedPixmaps[backgroundUri + strBackgroundPathkey] = qMakePair(st.desktopSize, pixmap);
            pixmap = cutBackgroundPix(st.workspaceSize, backgroundUri);
            m_wpCachedPixmaps[backgroundUri + strBackgroundPathkey] = qMakePair(st.workspaceSize, pixmap);
        }
    }
}

void MultiViewBackgroundManager::clearCurrentBackgroundList()
{
    m_currentBackgroundList.clear();
}

void MultiViewBackgroundManager::getBackgroundList()
{
    m_backgroundAllList.clear();
    QDBusInterface remoteApp(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ, DBUS_APPEARANCE_INTF);
    QDBusReply<QString> reply = remoteApp.call( "List", "background");

    QJsonDocument json = QJsonDocument::fromJson(reply.value().toUtf8());
    QJsonArray arr = json.array();
    if (!arr.isEmpty()) {
        auto p = arr.constBegin();
        while (p != arr.constEnd()) {
            auto o = p->toObject();
            if (!o.value("Id").isUndefined() && !o.value("Deletable").toBool()) {
                m_backgroundAllList << o.value("Id").toString();
            }
            ++p;
        }
    }

    foreach (const QString &value, m_currentBackgroundList) {
        if (m_backgroundAllList.contains(value)) {
            m_backgroundAllList.remove(value);
        }
    }

    QString oldVerisonDefaultBackground(previous_default_background_name);
    QString defaultBackground(fallback_background_name);
    if (m_currentBackgroundList.contains(oldVerisonDefaultBackground)) {
        m_backgroundAllList.remove(defaultBackground);
    }
}

void MultiViewBackgroundManager::updateBackgroundList(const QString &file)
{
    if (!m_backgroundAllList.contains(file)) {
        m_backgroundAllList.insert(file);
    }
    if (m_currentBackgroundList.contains(file)) {
        m_currentBackgroundList.remove(file);
    }
}

QString MultiViewBackgroundManager::getRandBackground()
{
    int index = 3;
    QString file;
    while (index > 0) {
        int backgroundIndex = m_backgroundAllList.count();
        if (backgroundIndex - 1 != 0) {
            qsrand((uint)QTime::currentTime().msec());
            backgroundIndex = qrand() % (backgroundIndex - 1);
        } else {
            backgroundIndex -= 1;
        }

        if (m_backgroundAllList.count() <= backgroundIndex) {
            index --;
            continue;
        }

        auto b_set = m_backgroundAllList.begin();
        file = *(b_set + backgroundIndex);
        if (m_currentBackgroundList.contains(file)) {
            m_backgroundAllList.remove(file);
            index --;
            continue;
        }
        break;
    }

    return file;
}

void MultiViewBackgroundManager::getPreviewBackground(QSize size, QPixmap &workspaceBg, int screen)
{
    m_previewScreen = screen;
    m_previewFile = getRandBackground();
    QString rfile = toRealPath(m_previewFile);
    workspaceBg = cutBackgroundPix(size, rfile);
}

void MultiViewBackgroundManager::setNewBackground(BgInfo_st &st, QPixmap &desktopBg, QPixmap &workspaceBg)
{
    QString strBackgroundPath = QString("%1%2").arg(st.desktop).arg(st.screenName);

    QDBusInterface wm(DBUS_DEEPIN_WM_SERVICE, DBUS_DEEPIN_WM_OBJ, DBUS_DEEPIN_WM_INTF);
    QString file;
    if (st.screen == m_previewScreen && !m_previewFile.isEmpty()) {
        m_previewScreen = -1;
        if (m_currentBackgroundList.contains(m_previewFile)) {
            m_previewFile = getRandBackground();
        }
        file = m_previewFile;
        m_currentBackgroundList.insert(file);
        if (m_backgroundAllList.count() - 1 > 0)
            m_backgroundAllList.remove(file);
        m_previewFile = "";
    } else {
        file = getRandBackground();
        if (m_backgroundAllList.count() - 1 > 0)
            m_backgroundAllList.remove(file);
        m_currentBackgroundList.insert(file);
    }

    wm.call( "SetWorkspaceBackgroundForMonitor", st.desktop, st.screenName, file);

    file = toRealPath(file);
    desktopBg = cutBackgroundPix(st.desktopSize, file);
    m_bgCachedPixmaps[file + strBackgroundPath] = qMakePair(st.desktopSize, desktopBg);
    workspaceBg = cutBackgroundPix(st.workspaceSize, file);
    m_wpCachedPixmaps[file + strBackgroundPath] = qMakePair(st.workspaceSize, workspaceBg);
}

void MultiViewBackgroundManager::setMonitorInfo(QList<QMap<QString,QVariant>> monitorInfoList)
{
    m_monitorInfoList = monitorInfoList;

    QList<QString> monitorNameList;
    for (int i = 0; i < m_monitorInfoList.size(); i++) {
        QMap<QString,QVariant> monitorInfo = m_monitorInfoList.at(i);
        monitorNameList.append(monitorInfo.keys());
    }
    m_screenNamelist = monitorNameList.toSet().toList();
}


MultiViewWorkspace::MultiViewWorkspace(bool flag)
    : m_backGroundFrame(nullptr)
    , m_desktop(0), m_bShader(flag)
{
    m_backGroundFrame = effects->effectFrame(EffectFrameNone, false);
    m_workspaceBgFrame = effects->effectFrame(EffectFrameNone, false);
    m_hoverFrame = effects->effectFrame(EffectFrameUnstyled, false);
    m_shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("workspacethumb.glsl"));
    m_hoverShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("workspacehover.glsl"));

    m_bkBlurShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString("bk.v"), QStringLiteral("bk.f"));
    m_backGroundFrame->setShader(m_bkBlurShader);

    m_wBGShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString("wbk.v"), QStringLiteral("wbk.f"));
    m_workspaceBgFrame->setShader(m_wBGShader);
}

MultiViewWorkspace::~MultiViewWorkspace()
{
    if (m_backGroundFrame) {
        delete m_backGroundFrame;
        m_backGroundFrame = nullptr;
    }
    if (m_workspaceBgFrame) {
        delete m_workspaceBgFrame;
        m_workspaceBgFrame = nullptr;
    }
    if (m_hoverFrame) {
        delete m_hoverFrame;
        m_hoverFrame = nullptr;
    }
    if (m_shader) {
        delete m_shader;
        m_shader = nullptr;
    }
    if (m_hoverShader) {
        delete m_hoverShader;
        m_hoverShader = nullptr;
    }

    if(m_bkBlurShader){
        delete m_bkBlurShader;
        m_bkBlurShader = nullptr;
    }
    if(m_wBGShader){
        delete m_wBGShader;
        m_wBGShader = nullptr;
    }
}

void MultiViewWorkspace::renderDesktopBackGround(float k)
{
    m_backGroundFrame->setShader(m_bkBlurShader);
    ShaderManager::instance()->pushShader(m_bkBlurShader);
    m_bkBlurShader->setUniform("dx", m_fullArea.width() * k);
    m_bkBlurShader->setUniform("iResolution", QVector2D(m_fullArea.width(), m_fullArea.height()));
    ShaderManager::instance()->popShader();
    m_backGroundFrame->render(infiniteRegion(), 1, 1);
}

void MultiViewWorkspace::renderWorkspaceBackGround(float t, int desktop)
{
    int ncurrentDesktop = effects->currentDesktop();
    QRect rect = m_workspaceBgFrame->geometry();

    if (ncurrentDesktop == desktop) {
        QColor color = effectsEx->getActiveColor();
        QRect geoframe = rect;
        geoframe.adjust(-3, -3, 3, 3);
        m_hoverFrame->setGeometry(geoframe);
        ShaderBinder binder(m_hoverShader);
        m_hoverShader->setUniform(GLShader::Color, color);
        m_hoverShader->setUniform("iResolution", QVector3D(geoframe.width(), geoframe.height(), 0));
        m_hoverShader->setUniform("iOffset", QVector2D(calculateOffSet(geoframe, m_fullArea, m_maxHeight)));
        m_hoverFrame->setShader(m_hoverShader);
        m_hoverFrame->render(infiniteRegion(), 1, 0);
    }

    m_workspaceBgFrame->setShader(m_wBGShader);
    ShaderManager::instance()->pushShader(m_wBGShader);
    m_wBGShader->setUniform("iResolution", QVector3D(rect.width(), rect.height(), 0));
    m_wBGShader->setUniform("iOffset", QVector2D(calculateOffSet(rect, m_fullArea, m_maxHeight)));
    m_wBGShader->setUniform("t", t);
    ShaderManager::instance()->popShader();
    m_workspaceBgFrame->render(infiniteRegion(), 1, 1);
}

void MultiViewWorkspace::render(bool isDrawBg)
{
    if (isDrawBg)
        m_backGroundFrame->render(infiniteRegion(), 1, 0.8);
    else
        m_workspaceBgFrame->render(infiniteRegion(), 1, 0.8);
}

void MultiViewWorkspace::setPosition(QPoint pos)
{
    m_workspaceBgFrame->setPosition(pos);
    QRect rect = m_workspaceBgFrame->geometry();

    ShaderBinder bind(m_shader);
    m_shader->setUniform("iResolution", QVector3D(rect.width(), rect.height(), 0));
    m_shader->setUniform("iOffset", QVector2D(calculateOffSet(rect, m_fullArea, m_maxHeight)));
    if(m_bShader)
        m_workspaceBgFrame->setShader(m_shader);
}

void MultiViewWorkspace::setImage(const QPixmap &bgPix, const QPixmap &wpPix, const QRect &rect)
{
    m_rect = rect;
    m_currentRect = rect;
    QIcon icon(bgPix);
    m_backGroundFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_backGroundFrame->setGeometry(m_fullArea);
    m_backGroundFrame->setPosition(m_fullArea.topLeft());
    m_backGroundFrame->setIcon(icon);
    m_backGroundFrame->setIconSize(QSize(m_fullArea.width(), m_fullArea.height()));

    m_workspaceBgFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_workspaceBgFrame->setGeometry(rect);
    m_workspaceBgFrame->setPosition(rect.topLeft());

    icon = wpPix;
    m_workspaceBgFrame->setIcon(icon);
    m_workspaceBgFrame->setIconSize(QSize(rect.width(), rect.height()));

    ShaderBinder bind(m_shader);
    m_shader->setUniform("iResolution", QVector3D(rect.width(), rect.height(), 0));
    m_shader->setUniform("iOffset", QVector2D(calculateOffSet(rect, m_fullArea, m_maxHeight)));
    if(m_bShader)
        m_workspaceBgFrame->setShader(m_shader);
}

void MultiViewWorkspace::setImage(const QString &btf, const QRect &rect)
{
    m_rect = rect;
    m_currentRect = rect;
    m_workspaceBgFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_workspaceBgFrame->setGeometry(rect);
    m_workspaceBgFrame->setPosition(rect.topLeft());

    QIcon icon(btf);
    m_workspaceBgFrame->setIcon(icon);
    m_workspaceBgFrame->setIconSize(QSize(rect.width(), rect.height()));

    ShaderBinder bind(m_shader);
    m_shader->setUniform("iResolution", QVector3D(rect.width(), rect.height(), 0));
    m_shader->setUniform("iOffset", QVector2D(calculateOffSet(rect, m_fullArea, m_maxHeight)));
    if(m_bShader)
        m_workspaceBgFrame->setShader(m_shader);
}

void MultiViewWorkspace::setRect(const QRect rect)
{
    m_rect = rect;
    updateGeometry(rect);
}

QRect MultiViewWorkspace::getGeometry()
{
    return m_workspaceBgFrame->geometry();
}

void MultiViewWorkspace::updateGeometry(QRect rect)
{
    m_currentRect = rect;
    m_workspaceBgFrame->setGeometry(rect);
    m_workspaceBgFrame->setIconSize(QSize(rect.width(), rect.height()));
    ShaderBinder bind(m_shader);
    m_shader->setUniform("iResolution", QVector3D(rect.width(), rect.height(), 0));
    m_shader->setUniform("iOffset", QVector2D(calculateOffSet(rect, m_fullArea, m_maxHeight)));
    if(m_bShader)
        m_workspaceBgFrame->setShader(m_shader);
}

MultiViewWinFill::MultiViewWinFill(int screen, QRect rect, int maxHeight)
    : m_rect(rect)
    , m_screen(screen)
    , m_maxHeight(maxHeight)
{
    m_fillShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture | ShaderTrait::Modulate, QString(), QStringLiteral("windowfill.glsl"));
    m_fillFrame = effects->effectFrame(EffectFrameUnstyled, false);
    m_fillFrame->setGeometry(rect);
    ShaderBinder binder(m_fillShader);
    m_fillShader->setUniform("iResolution", QVector3D(rect.width(), rect.height(), 0));
    m_fillShader->setUniform("iOffset", QVector2D(calculateOffSet(rect, effects->clientArea(ScreenArea, screen, 0), m_maxHeight)));
    m_fillFrame->setShader(m_fillShader);
}

MultiViewWinFill::~MultiViewWinFill()
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

void MultiViewWinFill::render()
{
    m_fillFrame->render(infiniteRegion(), 1, 0);
}

MultitaskViewEffect::MultitaskViewEffect()
    : m_showAction(new QAction(this))
    , m_mutex(QMutex::Recursive)
    , m_timer(new QTimer(this))
{
    QAction *a = m_showAction;
    a->setObjectName(QStringLiteral("ShowMultitasking"));
    a->setText("Show Multitasking View");
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_S);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_S);
    shortcut = KGlobalAccel::self()->shortcut(a);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_S, a);
    connect(a, SIGNAL(triggered(bool)), this, SLOT(toggle()));

    connect(effects, &EffectsHandler::windowAdded, this, &MultitaskViewEffect::onWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &MultitaskViewEffect::onWindowDeleted);
    connect(effects, &EffectsHandler::windowClosed, this, &MultitaskViewEffect::onWindowClosed);
    connect(effects, &EffectsHandler::closeEffect, this, &MultitaskViewEffect::onCloseEffect);
    connect(effects, &EffectsHandler::numberScreensChanged, this, [this] {
        onCloseEffect(true);
    });

    connect(effectsEx, &EffectsHandlerEx::showMultitasking, this, [=]() {
        this->toggle();
    });

    connect(_gsettings_dde_dock, &QGSettings::changed, this, &MultitaskViewEffect::onDockChange);

    m_hoverWinShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture | ShaderTrait::Modulate, QString(), QStringLiteral("windowhover.glsl"));
    m_previewShader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("workspacethumb.glsl"));

    reconfigure(ReconfigureAll);


    m_curDesktopIndex = effects->currentDesktop();
    m_lastDesktopIndex = m_curDesktopIndex;

    m_bgSlidingStatus = false;
    m_bgSlidingTimeLine.setEasingCurve(QEasingCurve::InQuint);
    m_bgSlidingTimeLine.setDuration(std::chrono::milliseconds(EFFECT_DURATION_DEFAULT));

    m_workspaceSlidingStatus = false;
    m_workspaceSlidingTimeline.setEasingCurve(QEasingCurve::Linear);
    m_workspaceSlidingTimeline.setDuration(std::chrono::milliseconds(EFFECT_DURATION_DEFAULT));

    m_popStatus = false;
    m_popTimeLine.setEasingCurve(QEasingCurve::OutQuint);
    m_popTimeLine.setDuration(std::chrono::milliseconds(EFFECT_DURATION_DEFAULT));

    m_opacityStatus = false;
    m_opacityTimeLine.setEasingCurve(QEasingCurve::OutQuint);
    m_opacityTimeLine.setDuration(std::chrono::milliseconds(2*EFFECT_DURATION_DEFAULT));

    QString qm = QString(":/multitasking/multitaskview/translations/multitasking_%1.qm").arg(QLocale::system().name());
    QTranslator *tran = new QTranslator();
    if (tran->load(qm)) {
        qApp->installTranslator(tran);
    }

    connect(m_timer, &QTimer::timeout, this, &MultitaskViewEffect::motionRepeat);

    cacheWorkspaceBackground();

    QDBusInterface interfaceRequire("org.desktopspec.ConfigManager", "/", "org.desktopspec.ConfigManager", QDBusConnection::systemBus());
    QDBusPendingReply<QDBusObjectPath> reply = interfaceRequire.call("acquireManager", "org.kde.kwin", "org.kde.kwin.multitaskview.display", "");
    reply.waitForFinished();
    QDBusInterface interfaceValue("org.desktopspec.ConfigManager", reply.value().path(), "org.desktopspec.ConfigManager.Manager", QDBusConnection::systemBus());
    QDBusReply<QVariant> replyValue = interfaceValue.call("value", "windowDisplay");
    QString strValue = replyValue.value().toString();
    if (strValue == "Enabled" || strValue == "enabled") {
        m_isShowWhole = true;
    } else {
        m_isShowWhole = false;
    }
    QDBusConnection::sessionBus().connect("com.deepin.ScreenRecorder.time", "/com/deepin/ScreenRecorder/time", "com.deepin.ScreenRecorder.time", "start", this, SLOT(screenRecorderStart()));
    // 监听控制中心字体变化
    QDBusConnection::sessionBus().connect("com.deepin.daemon.Appearance", "/com/deepin/daemon/Appearance", "com.deepin.daemon.Appearance", "Changed", this, SLOT(fontChanged(QString, QString)));
}

void MultitaskViewEffect::fontChanged(const QString &fontType, const QString &fontName)
{
    if (fontType == "standardfont" || fontType == "monospacefont") {
        m_fontFamily = fontName;
    }
}

MultitaskViewEffect::~MultitaskViewEffect()
{
    if (m_showAction) {
        delete m_showAction;
        m_showAction = nullptr;
    }
    if (m_hoverWinFrame) {
        delete m_hoverWinFrame;
        m_hoverWinFrame = nullptr;
    }
    if (m_closeWinFrame) {
        delete m_closeWinFrame;
        m_closeWinFrame = nullptr;
    }
    if (m_topWinFrame) {
        delete m_topWinFrame;
        m_topWinFrame = nullptr;
    }
    if (m_textWinFrame) {
        delete m_textWinFrame;
        m_textWinFrame = nullptr;
    }
    if (m_hoverWinShader) {
        delete m_hoverWinShader;
        m_hoverWinShader = nullptr;
    }
    if (m_previewFrame) {
        delete m_previewFrame;
        m_previewFrame = nullptr;
    }
    if (m_closeWorkspaceFrame) {
        delete m_closeWorkspaceFrame;
        m_closeWorkspaceFrame = nullptr;
    }
    if (m_dragTipsFrame) {
        delete m_dragTipsFrame;
        m_dragTipsFrame = nullptr;
    }
}

void MultitaskViewEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    m_bgSlidingStatus = false;
    m_bgSlidingTimeLine.reset();

    m_popStatus = false;
    m_popTimeLine.reset();

    m_opacityStatus = false;
    m_opacityTimeLine.reset();

    m_curDesktopIndex = effects->currentDesktop();
    m_lastDesktopIndex = m_curDesktopIndex;

    m_scalingFactor = qMax(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);
}

void MultitaskViewEffect::prePaintScreen(ScreenPrePaintData &data, int time)
{
    if (isActive()) {
        if(m_effectFlyingBack.animating())
            m_effectFlyingBack.update(time);

        if(m_workspaceSlidingStatus)
            m_workspaceSlidingTimeline.update(std::chrono::milliseconds(time));

        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS
                  |  PAINT_SCREEN_BACKGROUND_FIRST;

        if(m_popStatus)
            m_popTimeLine.update(std::chrono::milliseconds(time));

        if(m_opacityStatus)
            m_opacityTimeLine.update(std::chrono::milliseconds(time));

        if(m_bgSlidingStatus)
            m_bgSlidingTimeLine.update(std::chrono::milliseconds(time));

        for (auto& mm: m_motionManagers) {
            mm->calculate(POPUP_TIME_SCALE);
        }
    }

    effects->prePaintScreen(data, time);
}

void MultitaskViewEffect::paintScreen(int mask, QRegion region, ScreenPaintData &data)
{
    m_isShowWin = false;
    effects->paintScreen(mask, region, data);

    QMutexLocker locker(&m_mutex);
    for (int i = 0; i < m_workspaceBackgrounds.size(); i++) {
        if (m_bgSlidingStatus) {
            MultiViewWorkspace *lwkobj = getWorkspaceObject(i, m_lastDesktopIndex - 1);
            MultiViewWorkspace *cwkobj = getWorkspaceObject(i, m_curDesktopIndex - 1);
            if (lwkobj && cwkobj) {
                auto area = lwkobj->getfullArea();
                if (effects->waylandDisplay()) {
                    if (i != effectsEx->getCurrentPaintingScreen()) {
                        if (area.x() > 0)
                            continue;
                    }
                } else {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(area.x(), area.y() - SCISSOR_HOFFU, area.width(), area.height() + SCISSOR_HOFFD);
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
                    } else {
                        lwkobj->renderDesktopBackGround(m_bgSlidingTimeLine.value());
                        cwkobj->renderDesktopBackGround(-1 + m_bgSlidingTimeLine.value());
                    }
                } else {
                    if (m_lastDesktopIndex < m_curDesktopIndex) {
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
            MultiViewWorkspace *cwkobj = getWorkspaceObject(i, effects->currentDesktop() - 1);
            if (cwkobj) {
                cwkobj->renderDesktopBackGround(0.0f);
            }
        }

        MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
        if (wmobj && wmobj->getDesktopWinNumofScreen(i) == 0) {
            static GLShader*  shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture | ShaderTrait::Modulate, QString(), QStringLiteral("windowtext.glsl"));
            if (m_tipFrames.contains(i) && m_tipFrames[i].size() == 2) {
                EffectFrame *bgframe = m_tipFrames[i][0];
                EffectFrame *tframe = m_tipFrames[i][1];
                QRect bgrect = tframe->geometry();
                QFontMetrics* metrics = NULL;
                if (!metrics)
                    metrics = new QFontMetrics(tframe->font());
                int width = metrics->width(tframe->text());
                int height = metrics->height();
                delete metrics;
                bgrect.adjust(-(width - bgrect.width()) / 2 - (6 * m_scalingFactor),
                              -(height - bgrect.height()) / 2 - (3 * m_scalingFactor),
                              (width - bgrect.width()) / 2 + (6 * m_scalingFactor),
                              (height - bgrect.height()) / 2 + (4 * m_scalingFactor));

                bgframe->setGeometry(bgrect);
                ShaderBinder binder(shader);
                shader->setUniform("iResolution", QVector3D(bgframe->geometry().width(), bgframe->geometry().height(), 0));
                shader->setUniform("iOffset", QVector2D(bgframe->geometry().x(), m_maxHeight - bgframe->geometry().y() - bgframe->geometry().height()));
                bgframe->setShader(shader);
                bgframe->render(infiniteRegion(), 1, 0);
                tframe->render(infiniteRegion(), 1, 0);
            }
        }
    }

    //draw workspace background
    for (int wi = 0; wi < m_workspaceBackgrounds.size(); wi++) {
        for (int j = 0; j < effects->numberOfDesktops(); j++) {
            if (m_deleteWorkspaceDesktop - 1 == j)
                continue;
            MultiViewWorkspace *wkobj = getWorkspaceObject(wi, j);
            if (wkobj) {
                if (m_popStatus) {  //startup
                    wkobj->renderWorkspaceBackGround(m_popTimeLine.value(), j + 1);
                } else if (m_opacityStatus) {  //new workspace
                    if (j == effects->numberOfDesktops() - 1)
                        wkobj->renderWorkspaceBackGround(m_opacityTimeLine.value(), j + 1);
                    else
                        wkobj->renderWorkspaceBackGround(1, j + 1);
                } else if (m_wasWorkspaceMove) {  //move workspace
                    if (m_aciveMoveDesktop - 1 != j) {
                        wkobj->renderWorkspaceBackGround(1, j + 1);
                    }
                } else if (m_workspaceSlidingStatus) {
                    renderSlidingWorkspace(wkobj, wi, j, data);
                } else {
                    wkobj->renderWorkspaceBackGround(1, j + 1);
                }
            }
        }
    }

    if (!m_wasWindowMove && m_isShowPreview && m_screen != -1) {
        QPoint pos = m_addWorkspaceButton[m_screen]->getRect().topLeft();
        showWorkspacePreview(m_screen, pos);
    } else {
        showWorkspacePreview(m_screen, QPoint(), true);
    }

    //draw button
    if (effects->numberOfDesktops() < MAX_DESKTOP_COUNT) {
        for (int i = 0; i < m_addWorkspaceButton.size(); i++) {
            m_addWorkspaceButton[i]->render();
        }
    }

    for (int desktop = effects->numberOfDesktops(); desktop >= 0; desktop--) {
        m_isShowWin = true;
        paintingDesktop = desktop;
        if (desktop == 0) {
            if (m_hoverDesktop != -1 && m_screen != -1 && effects->numberOfDesktops() > 1) {
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

        handlerAfterTimeLine();
    }

    if (m_workspaceSlidingTimeline.done()) {
        m_workspaceSlidingStatus = false;
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

    if (m_activated)
        effects->addRepaintFull();

    for (auto const& w: effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
    }
    effects->postPaintScreen();

    if (m_effectFlyingBack.done()) {
        m_effectFlyingBack.end();
        setActive(false);
        QTimer::singleShot(400, [&]() { m_delayDbus = true; });
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

void MultitaskViewEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, int time)
{
    data.mask |= PAINT_WINDOW_TRANSFORMED;

    if (m_activated && checkConfig(w)) {
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    }
    if (w->isOnDesktop(paintingDesktop))
        w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    else if (w->isDesktop())
        w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    if (0 != paintingDesktop) {
        if (w->isMinimized())
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    }

    if (m_windowMove && m_wasWindowMove && m_windowMove->findModal() == w)
        w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);

    if (m_bgSlidingStatus) {
        WindowMotionManager *mgr0 = nullptr, *mgr1 = nullptr;
        auto *lastWinManager = getWinManagerObject(m_lastDesktopIndex - 1);
        auto *curWinManager = getWinManagerObject(m_curDesktopIndex - 1);
        if (lastWinManager && curWinManager) {
            lastWinManager->getMotion(m_lastDesktopIndex, w->screen(), mgr0);
            curWinManager->getMotion(m_curDesktopIndex, w->screen(), mgr1);
            if (mgr0->isManaging(w) || mgr1->isManaging(w))
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            if (w->isDesktop())
                w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        }
    }

    effects->prePaintWindow(w, data, time);
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
        if (w->isOnDesktop(effects->currentDesktop())) {
            m_effectFlyingBack.paintWindow(w, mask, region, data);
        }
        return;
    } else if (m_effectFlyingBack.done()) {
        if (w->isOnDesktop(effects->currentDesktop())) {
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
    if (w->caption() == "dde-osd" && m_isCloseScreenRecorder) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    int desktop = effects->currentDesktop();
    if (0 == paintingDesktop) {
        if (w == m_windowMove && m_wasWindowMove) {
            return;
        }

        if (m_bgSlidingStatus) {
            if (m_flyingWinList.contains(w))
                return;

            WindowMotionManager *mgr0 = nullptr, *mgr1 = nullptr;
            auto *lastWinManager = getWinManagerObject(m_lastDesktopIndex - 1);
            auto *curWinManager = getWinManagerObject(m_curDesktopIndex - 1);
            QRect backgroundRect = effects->clientArea(FullScreenArea, w->screen(), 1);
            int bgRectWidth = backgroundRect.width();
            if (lastWinManager) {
                lastWinManager->getMotion(m_lastDesktopIndex, w->screen(), mgr0);
                if (mgr0->isManaging(w)) {
                    auto area = effects->clientArea(ScreenArea, w->screen(), 0);
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
                        mask |= PAINT_WINDOW_LANCZOS;
                    }
                    effects->paintWindow(w, mask, area, d);     //when open, all windows flying into RegionB
                }
            }
            if (curWinManager) {
                curWinManager->getMotion(m_curDesktopIndex, w->screen(), mgr1);
                if (mgr1->isManaging(w)) {
                    auto area = effects->clientArea(ScreenArea, w->screen(), 0);
                    WindowPaintData d = data;
                    auto geo0 = mgr1->transformedGeometry(w);
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
                        mask |= PAINT_WINDOW_LANCZOS;
                    }
                    effects->paintWindow(w, mask, area, d);
                }
            }
        } else {
            WindowMotionManager *wmm = nullptr;
            QMutexLocker locker(&m_mutex);
            MultiViewWinManager *wmobj = getWinManagerObject(desktop - 1);
            if (wmobj && wmobj->getMotion(desktop, w->screen(), wmm)) {
                if (wmm->isManaging(w)) {
                    auto area = effects->clientArea(ScreenArea, w->screen(), 0);
                    WindowPaintData d = data;
                    auto geo = wmm->transformedGeometry(w);

                    WindowQuadList quads;
                    foreach (const WindowQuad &quad, d.quads) {
                        switch (quad.type()) {
                        case WindowQuadDecoration:
                            quads.append(quad);
                            continue;
                        case WindowQuadContents:
                            quads.append(quad);
                            continue;
                        default:
                            continue;
                        }
                    }
                    d.quads = quads;

                    if (w == m_hoverWin) {
                        if (wmm->isWindowFill(w) && wmobj->isHaveWinFill(w)) {
                            renderHover(w, wmobj->getWinFill(w)->getRect());
                        } else {
                            renderHover(w, geo.toRect());
                        }
                    }

                    if (wmm->isWindowFill(w) && wmobj->isHaveWinFill(w)) {
                        wmobj->getWinFill(w)->render();
                        mask |= PAINT_WINDOW_LANCZOS;
                    }

                    {
                        d.setScale(QVector2D((float)geo.width() / w->width(), (float)geo.height() / w->height()));
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
            effects->paintWindow(w, mask, region, data);
            return;
        }
    } else if (!m_workspaceSlidingStatus) {
        if (m_wasWorkspaceMove && paintingDesktop == m_aciveMoveDesktop)
            return;

        WindowMotionManager *wmm;
        MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(paintingDesktop - 1);
        if (wkmobj && wkmobj->getMotion(paintingDesktop, w->screen(), wmm)) {
            if (wmm->isManaging(w)) {
                auto area = effects->clientArea(ScreenArea, w->screen(), 0);
                WindowPaintData d = data;
                auto geo = wmm->transformedGeometry(w);

                d += QPoint(qRound(geo.x() - w->x()), qRound(geo.y() - w->y()));
                d.setScale(QVector2D((float)geo.width() / w->width(), (float)geo.height() / w->height()));
                mask |= PAINT_SCREEN_TRANSFORMED;
                effects->paintWindow(w, mask, area, d);
            }
        }
    }
}

void MultitaskViewEffect::handlerAfterTimeLine()
{
    if (m_isRemoveWorkspace) {
        removeDesktopEx(m_lastDesktopIndex);
    }
}

void MultitaskViewEffect::renderWindowMove(KWin::ScreenPaintData &data)
{
    QPoint diff = cursorPos() - m_windowMoveStartPos;
    QRect geo = m_windowMoveGeometry.translated(diff);
    WindowPaintData d(m_windowMove, data.projectionMatrix());

    MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    QRect rect1 = wmobj ? wmobj->getWindowGeometry(m_windowMove, m_windowMove->screen()) : QRect();
    float K1 = (qreal)rect1.width() / (qreal)m_windowMove->width();
    d += QPoint(-m_windowMove->x(), -m_windowMove->y());
    if (cursorPos().y() < m_windowMoveStartPos.y()) {
        int Y0 = m_scale[m_windowMove->screen()].workspaceMgrRect.bottom();
        float k = (cursorPos().y() - m_windowMoveStartPos.y()) / float(Y0 - m_windowMoveStartPos.y());
        float K0 = (qreal)geo.width() / (qreal)m_windowMove->width();
        k = K1 - k * (K1 - K0);
        if (k < K0) k = K0;
        d *= QVector2D(k, k);

        float sx = (m_windowMoveStartPos.x() - rect1.x()) / float(rect1.width());
        float sy = (m_windowMoveStartPos.y() - rect1.y()) / float(rect1.height());
        float cx = cursorPos().x();
        float cy = cursorPos().y();
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
    MultiViewWorkspace *target = getWorkspaceObject(m_screen, m_aciveMoveDesktop - 1);
    if (target) {
        QPoint cursorpos = cursorPos();
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
        MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(m_aciveMoveDesktop - 1);
        if (wkmobj && wkmobj->getMotion(m_aciveMoveDesktop, m_screen, wmm)) {
            list = wmm->orderManagedWindows();
        }

        foreach (EffectWindow *w, list) {
            if (w->screen() == m_screen && wmm->isManaging(w) && !w->isMinimized()) {
                WindowPaintData d(w, data.projectionMatrix());
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

void MultitaskViewEffect::renderSlidingWorkspace(MultiViewWorkspace *wkobj, int screen, int desktop, KWin::ScreenPaintData &data)
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
    MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(desktop);
    if (wkmobj && wkmobj->getMotion(m_aciveMoveDesktop, screen, wmm)) {
        foreach (EffectWindow *w, wmm->orderManagedWindows()) {
            if (wmm->isManaging(w) && !w->isMinimized()) {
                WindowPaintData d(w, data.projectionMatrix());
                auto geo = wmm->transformedGeometry(w);
                geo.moveTo(x + geo.x() - x1, geo.y());

                d *= QVector2D((qreal)geo.width() / (qreal)w->width(), (qreal)geo.height() / (qreal)w->height());
                d += QPoint(geo.left() - w->x(), geo.top() - w->y());

                effects->drawWindow(w, PAINT_WINDOW_TRANSFORMED, rect, d);
            }
        }
    }
}

void MultitaskViewEffect::renderHover(const EffectWindow *w, const QRect &rect, int order)
{
    if (!order) {
        if (!m_hoverWinFrame) {
            m_hoverWinFrame = effects->effectFrame(EffectFrameUnstyled, false);
        }

        QColor color = effectsEx->getActiveColor();
        QRect geoframe = rect;
        geoframe.adjust(-5, -5, 5, 5);
        m_hoverWinFrame->setGeometry(geoframe);
        ShaderBinder binder(m_hoverWinShader);
        m_hoverWinShader->setUniform(GLShader::Color, color);
        m_hoverWinShader->setUniform("iResolution", QVector3D(geoframe.width(), geoframe.height(), 0));
        m_hoverWinShader->setUniform("iOffset", QVector2D(calculateOffSet(geoframe, m_scale[w->screen()].fullArea, m_maxHeight)));
        m_hoverWinFrame->setShader(m_hoverWinShader);
        m_hoverWinFrame->render(infiniteRegion(), 1, 0);
    } else {
        if (!m_closeWinFrame) {
            m_closeWinFrame = effects->effectFrame(EffectFrameNone, false);
            m_closeWinFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);

            QIcon icon(MULTITASK_CLOSE_SVG);
            m_closeWinFrame->setIcon(icon);
            m_closeWinFrame->setIconSize(QSize(48, 48));
        }
        if (!m_topWinFrame) {
            m_topWinFrame = effects->effectFrame(EffectFrameUnstyled, false);
            m_topWinFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        }
        if (!m_textWinFrame) {
            m_textWinFrame = effects->effectFrame(EffectFrameStyled, false);
            m_textWinFrame->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        }
        if (!m_textWinBgFrame) {
            m_textWinBgFrame = effects->effectFrame(EffectFrameStyled, false);
            m_textWinBgFrame->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        }

        int width = 0;
        int height = 0;
        {
            QFont font;
            font.setFamily(m_fontFamily);
            font.setPointSize(14);
            m_textWinFrame->setFont(font);
            QFontMetrics* metrics = NULL;
            if (!metrics)
                metrics = new QFontMetrics(m_textWinFrame->font());
            QString string = metrics->elidedText(w->caption(), Qt::ElideRight, rect.width() * 0.9);
            if (string != m_textWinFrame->text())
                m_textWinFrame->setText(string);
            width = metrics->width(string);
            height = metrics->height();
            delete metrics;
        }

        if (w->keepAbove() && m_topFrameIcon != MULTITASK_TOP_ACTIVE_SVG) {
            QIcon icon(MULTITASK_TOP_ACTIVE_SVG);
            m_topFrameIcon = MULTITASK_TOP_ACTIVE_SVG;
            m_topWinFrame->setIcon(icon);
            m_topWinFrame->setIconSize(QSize(48, 48));
        } else if (!w->keepAbove() && m_topFrameIcon != MULTITASK_TOP_SVG) {
            QIcon icon(MULTITASK_TOP_SVG);
            m_topFrameIcon = MULTITASK_TOP_SVG;
            m_topWinFrame->setIcon(icon);
            m_topWinFrame->setIconSize(QSize(48, 48));
        }

        //button point 微调结果
        m_closeWinFrame->setPosition(QPoint(rect.x() + rect.width() - 25, rect.y() - 17));
        m_topWinFrame->setPosition(QPoint(rect.x() - 22, rect.y() - 17));
        m_textWinFrame->setPosition(QPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() - 40));
        m_winBtnArea[0] = QRect(QPoint(rect.x() + rect.width() - 25, rect.y() - 17), QSize(48, 48));
        m_winBtnArea[1] = QRect(QPoint(rect.x() - 22, rect.y() - 17), QSize(48, 48));

        m_textWinBgFrame->setPosition(QPoint(rect.x() + rect.width() / 2, rect.y() + rect.height() - 40));
        static GLShader*  shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture | ShaderTrait::Modulate, QString(), QStringLiteral("windowtext.glsl"));
        QRect geoframe = m_textWinFrame->geometry();
        geoframe.adjust(-(width - geoframe.width()) / 2 - (16 * m_scalingFactor),
                        -(height - geoframe.height()) / 2 - ((48 * m_scalingFactor - height) / 2),
                        (width - geoframe.width()) / 2 + (16 * m_scalingFactor),
                        (height - geoframe.height()) / 2 + (48 * m_scalingFactor - height) / 2);
        m_textWinBgFrame->setGeometry(geoframe);
        ShaderBinder binder(shader);
        shader->setUniform("iResolution", QVector3D(geoframe.width(), geoframe.height(), 0));
        shader->setUniform("iOffset", QVector2D(calculateOffSet(geoframe, m_scale[m_screen].fullArea, m_maxHeight)));
        m_textWinBgFrame->setShader(shader);
        m_textWinBgFrame->render(infiniteRegion(), 1, 0);

        m_closeWinFrame->render(infiniteRegion(), 1, 0);
        m_topWinFrame->render(infiniteRegion(), 1, 0);
        m_textWinFrame->render(infiniteRegion(), 1, 0);
    }
}

void MultitaskViewEffect::renderWorkspaceHover(int screen)
{
    MultiViewWorkspace *wkobj = getWorkspaceObject(screen, m_hoverDesktop - 1);
    if (wkobj) {
        QRect rect = wkobj->getRect();
        if (!m_closeWorkspaceFrame) {
            m_closeWorkspaceFrame = effects->effectFrame(EffectFrameNone, false);
            m_closeWorkspaceFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);

            QIcon icon(MULTITASK_CLOSE_SVG);
            m_closeWorkspaceFrame->setIcon(icon);
            m_closeWorkspaceFrame->setIconSize(QSize(48, 48));
        }
        m_workspaceCloseBtnArea = QRect(QPoint(rect.x() + rect.width() - 30, rect.y() - 13), QSize(48, 48));
        m_closeWorkspaceFrame->setPosition(QPoint(rect.x() + rect.width() - 30, rect.y() - 13));    //point 微调结果
        m_closeWorkspaceFrame->render(infiniteRegion(), 1, 0);
    }
}

void MultitaskViewEffect::renderDragWorkspacePrompt(int screen)
{
    MultiViewWorkspace *wkobj = getWorkspaceObject(screen, m_aciveMoveDesktop - 1);
    if (wkobj) {
        QRect rect = wkobj->getRect();
        if (!m_dragTipsFrame) {
            m_dragTipsFrame = effects->effectFrame(EffectFrameUnstyled, false);
            m_dragTipsFrame->setAlignment(Qt::AlignHCenter);
            if (m_scalingFactor == 1)
                m_dragTipsFrame->setSpacing(10);
            m_dragTipsFrame->setGeometry(QRect(rect.x(), rect.y() + (rect.height() / 2) + 10, rect.width(), 30));

            QIcon icon(delete_workspace_png);
            m_dragTipsFrame->setIcon(icon);
            m_dragTipsFrame->setIconSize(QSize(19 * m_scalingFactor, 20 * m_scalingFactor));
        }

        QFont font;
        font.setFamily(m_fontFamily);
        font.setPointSize(13);
        m_dragTipsFrame->setFont(font);
        m_dragTipsFrame->setText(tr("Drag upwards to remove"));
        m_dragTipsFrame->setPosition(QPoint(rect.x() + (rect.width() / 2), rect.y() + (rect.height() / 2) + 10));
        m_dragTipsFrame->render(infiniteRegion(), 1, 0);

        drawDottedLine(rect, screen);
    }
}

void MultitaskViewEffect::drawDottedLine(const QRect &geo, int screen)
{
    if (!GLPlatform::instance()->isGLES()) {
        glLineStipple(1, 0x0f0f);
        glEnable(GL_LINE_STIPPLE);
    }
    glLineWidth(1.0);
    
    static GLShader* shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::UniformColor, QString("dottedline.vert"), QStringLiteral("dottedline.frag"));
    GLVertexBuffer* vbo = GLVertexBuffer::streamingBuffer();
    auto area = effects->clientArea(FullArea, screen, 0);
    vbo->reset();
    vbo->setColor(QColor(255, 255, 255));
    vbo->setUseColor(true);
    QVector<float> verts;
    verts.reserve(4);
    float x1 = (float)(geo.left()) * 2 / area.width() - 1.0;
    float x2 = (float)(geo.right()) * 2 / area.width() - 1.0;
    float y1 = 1.0 - (float)(geo.y() + geo.height()) / 2 * 2 / area.height();
    verts << x1 << y1;
    verts << x2 << y1;

    ShaderBinder bind(shader);
    vbo->setData(verts.size() / 2, 2, verts.data(), NULL);
    glEnable(GL_BLEND);
    vbo->render(GL_LINES);
    if (!GLPlatform::instance()->isGLES()) {
        glDisable(GL_LINE_STIPPLE);
    }
    glDisable(GL_BLEND);
}

void MultitaskViewEffect::onWindowClosed(EffectWindow *w)
{
    if (!m_activated)
        return;

    if (w->windowClass() == screen_recorder && w != m_screenRecorderMenu) {
        effects->startMouseInterception(this, Qt::PointingHandCursor);
    } else if (w->isDock()) {
        m_dock = nullptr;
        m_dockRect.setSize(QSize(0, 0));
    }
}

void MultitaskViewEffect::onWindowDeleted(EffectWindow *w)
{
    if (!m_activated)
        return;

    if (w->isDock()) {
        m_dock = nullptr;
        m_dockRect.setSize(QSize(0, 0));
    } else if (!QX11Info::isPlatformX11() && w->caption() == "dde-osd") {
        m_isCloseScreenRecorder = false;
        return;
    } else if (w->windowClass() == screen_recorder && w != m_screenRecorderMenu) {
        m_isScreenRecorder = false;
        if (!QX11Info::isPlatformX11())
            m_isCloseScreenRecorder = true;
        return;
    } else if (w == m_screenRecorderMenu) {
        m_screenRecorderMenu = nullptr;
        return;
    } else {
        removeWinAndRelayout(w);
        m_isShieldEvent = false;
    }
}

void MultitaskViewEffect::onWindowAdded(EffectWindow *w)
{
    if (!m_activated)
        return;

    if (w->isDock()) {
        m_dockRect = w->geometry();
        m_dock = w;
        onDockChange("");
    } else if (!QX11Info::isPlatformX11() && w->caption() == "org.deepin.dde.lock") {
        setActive(false);
    } else if (QX11Info::isPlatformX11()) {
        if (w->windowClass() != screen_recorder && w->windowClass() != notification_tips) {
            m_effectFlyingBack.begin();
            effects->addRepaintFull();
        } else if (w->windowClass() == screen_recorder) {
            if (m_isScreenRecorder) {
                m_screenRecorderMenu = w;
            } else {
                effects->stopMouseInterception(this);
                m_isScreenRecorder = true;
            }
        }
    } else if (!QX11Info::isPlatformX11()) {
        if (m_isScreenRecorder) {
            m_screenRecorderMenu = w;
        } else if (w->windowClass() == screen_recorder) {
            effects->stopMouseInterception(this);
            m_isScreenRecorder = true;
        } else if (w->windowClass() != notification_tips) {
            setActive(false);
        }
    }
}

void MultitaskViewEffect::onCloseEffect(bool isSleepBefore)
{
    if (isSleepBefore && isActive()) {
        setActive(false);
    }
}

void MultitaskViewEffect::onDockChange(const QString &key)
{

    QString position = _gsettings_dde_dock->get(GsettingsDockPosition).toString();
    int height = _gsettings_dde_dock->get(GsettingsDockHeight).toInt();
    if (position == "bottom") {
        if (m_dockRect.height() < height) {
            m_dockRect.setY(m_dockRect.height() + m_dockRect.y() - height);
            m_dockRect.setHeight(height);
        }
    } else if (position == "top") {
        if (m_dockRect.height() < height) {
            m_dockRect.setHeight(height);
        }
    } else if (position == "left") {
        if (m_dockRect.width() < height) {
            m_dockRect.setWidth(height);
        }
    } else if (position == "right") {
        if (m_dockRect.width() < height) {
            m_dockRect.setX(m_dockRect.width() + m_dockRect.x() - height);
            m_dockRect.setWidth(height);
        }
    }
}

void MultitaskViewEffect::screenRecorderStart()
{
    if (m_isScreenRecorder && !QX11Info::isPlatformX11()) {
        effects->startMouseInterception(this, Qt::PointingHandCursor);
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

void MultitaskViewEffect::windowInputMouseEvent(QEvent* e)
{
    if (!isReceiveEvent()) {
        return;
    }

    switch (e->type()) {
    case QEvent::MouseMove:
        if (m_touch.active && !m_touch.isMotion) {
            return;
        }
        break;
    case QEvent::MouseButtonPress:
        if(m_touch.active && m_touch.isPress) {
            return;
        }
    case QEvent::MouseButtonRelease:
        if(m_touch.active && m_touch.isPress) {
            return;
        }
    case QEvent::Wheel:
        break;
    default:
        return;
    }

    if (!QX11Info::isPlatformX11() && e->type() == QEvent::Wheel) {
        auto wheelEvent = static_cast<QWheelEvent*>(e);
        if (wheelEvent->delta() > 0) {
            handlerWheelEvent(Qt::ForwardButton);
        } else if (wheelEvent->delta() < 0) {
            handlerWheelEvent(Qt::BackButton);
        }
        return;
    }
    
    auto mouseEvent = static_cast<QMouseEvent*>(e);
    if (mouseEvent->button() == Qt::ForwardButton || mouseEvent->button() == Qt::BackButton) {
        if (mouseEvent->buttons() == Qt::ForwardButton) {
            handlerWheelEvent(Qt::ForwardButton);
        } else if (mouseEvent->buttons() == Qt::BackButton) {
            handlerWheelEvent(Qt::BackButton);
        }
        return;
    }

    EffectWindow* target = nullptr;
    bool isAddWorkspace = false;
    bool isHoverWorkspace = false;
    int desktop = -1, screen = -1;
    if (!m_wasWorkspaceMove) {
        for (int i = 0; i < getNumScreens(); i++) {
            if (m_scale[i].workspaceMgrRect.contains(mouseEvent->pos())) {
                isHoverWorkspace = true;
                screen = i;
                m_screen = i;
                if (effects->numberOfDesktops() < MAX_DESKTOP_COUNT) {
                    if (m_addWorkspaceButton[i]->getRect().contains(mouseEvent->pos())) {
                        isAddWorkspace = true;
                    }
                }
                break;
            } else if (m_scale[i].windowMgrRect.contains(mouseEvent->pos())) {
                MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
                if (wmobj) {
                    target = wmobj->getHoverWin(mouseEvent->pos(), i);
                }
                break;
            }
        }
    }


    switch (mouseEvent->type()) {
    case QEvent::MouseMove:
    {
        QPoint diff = mouseEvent->pos() - m_lastWorkspaceMovePos;
        if (!m_dockRect.contains(mouseEvent->pos())) {
            if (target) {   // window hover
                m_hoverWin = target;
                m_hoverWinBtn = target;
                m_hoverDesktop = -1;
            } else if (!m_wasWorkspaceMove && (abs(diff.x()) > MOUSE_MOVE_MIN_DISTANCE || (abs(diff.y()) > MOUSE_MOVE_MIN_DISTANCE)) && m_aciveMoveDesktop != -1 && m_screen != -1) {     //workspace move
                m_workspaceMoveStartPos = mouseEvent->pos();
                m_wasWorkspaceMove = true;
                m_hoverDesktop = -1;
            } else if (isHoverWorkspace) {  // workspace hover
                if (!checkHandlerWorkspace(mouseEvent->pos(), m_screen, m_hoverDesktop))
                    m_hoverDesktop = -1;
                m_hoverWinBtn = nullptr;
            } else {
                m_hoverWinBtn = nullptr;
                m_hoverDesktop = -1;
            }
        } else {
            m_hoverWinBtn = nullptr;
            m_hoverDesktop = -1;
        }

        if (m_windowMove && (abs(diff.x()) > MOUSE_MOVE_MIN_DISTANCE || (abs(diff.y()) > MOUSE_MOVE_MIN_DISTANCE))) {    // window move
            if (!m_wasWindowMove) {
                m_windowMoveStartPos = mouseEvent->pos();
                MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
                m_windowMoveGeometry = wmobj ? wmobj->getWindowGeometry(m_windowMove, m_windowMove->screen()) : QRect();

                int width = m_scale[m_windowMove->screen()].workspaceWidth / 2;
                float scale = (float)width / m_windowMove->width();
                int height = m_windowMove->height() * scale;
                m_windowMoveGeometry.setSize(QSize(width, height));
            }
            m_wasWindowMove = true;
        }
        m_isShowPreview = isAddWorkspace;

        return;
    }
    case QEvent::MouseButtonPress:
        m_timer->setSingleShot(true);
        m_timer->start(250);
        if (!m_dockRect.contains(mouseEvent->pos())) {
            if (target) {       // window press
                m_windowMove = target;
                m_windowMoveDiff = mouseEvent->pos() - target->pos();
            } else if (isHoverWorkspace && !isAddWorkspace) {   // workspace press
                if (checkHandlerWorkspace(mouseEvent->pos(), screen, m_aciveMoveDesktop)) {
                    m_screen = screen;
                }
            }
            m_lastWorkspaceMovePos = mouseEvent->pos();
        }

        break;
    case QEvent::MouseButtonRelease:
        m_timer->stop();
        if (!m_wasWindowMove && m_dockRect.contains(mouseEvent->pos())) {
            m_delayDbus = false;
            if (QX11Info::isPlatformX11()) {
                m_effectFlyingBack.begin();
                effects->addRepaintFull();
                m_cursorPos = mouseEvent->pos();
                m_buttonType = mouseEvent->button();
            } else {
                m_effectFlyingBack.begin();
                effects->addRepaintFull();
                // fix bug 128331, move sendPointer to flyingback down
                //effectsEx->sendPointer(mouseEvent->button());
                m_sendDockButton = mouseEvent->button();
            }
        } else if (target) {
            bool isPressBtn = false;
            int i = 0;
            for (; i < m_winBtnArea.size(); i++) {
                if (m_winBtnArea[i].contains(mouseEvent->pos())) {
                    isPressBtn = true;
                    break;
                }
            }
            if (isPressBtn) {
                if (i == 0) {   // close btn
                    if (closeWindow(target)) {
                        m_hoverWin = nullptr;
                        m_isShieldEvent = true;
                    }
                } else if (i == 1) {    //top btn
                    setWinKeepAbove(target);
                }
            } else if (!m_wasWindowMove && !m_longPressTouch) {
                effects->defineCursor(Qt::PointingHandCursor);
                effects->setElevatedWindow(target, false);
                effects->activateWindow(target);

                m_effectFlyingBack.setSelecedWindow(target);
                m_effectFlyingBack.begin();
            }
        } else if (m_wasWindowMove && isHoverWorkspace) {
            checkHandlerWorkspace(mouseEvent->pos(), screen, desktop);
            if (desktop != -1 && !m_windowMove->isOnDesktop(desktop)) {
                moveWindowChangeDesktop(m_windowMove, desktop, screen);
            } else {
                MultiViewWorkspace *wkobj = getWorkspaceObject(screen, effects->numberOfDesktops() - 1);
                if (wkobj) {
                    QRect bgrect = QRect(wkobj->getRect().x() + wkobj->getRect().width(), m_scale[screen].workspaceMgrRect.y(),
                                         m_scale[screen].workspaceMgrRect.x() + m_scale[screen].workspaceMgrRect.width() - wkobj->getRect().x() - wkobj->getRect().width(), m_scale[screen].workspaceMgrRect.height());
                    if (bgrect.contains(mouseEvent->pos()) && effects->numberOfDesktops() < 6) {
                        addNewDesktop();
                        moveWindowChangeDesktop(m_windowMove, effects->numberOfDesktops(), screen, true);
                        m_opacityStatus = true;  //start opacity and sliding effects
                    }
                }
            }
        } else if (isHoverWorkspace && !m_wasWorkspaceMove && !m_wasWindowMove) {
            if (isAddWorkspace) {
                addNewDesktop();
            } else if (m_workspaceCloseBtnArea.contains(mouseEvent->pos())) {
                removeDesktop(m_aciveMoveDesktop);
            } else {
                checkHandlerWorkspace(mouseEvent->pos(), screen, desktop);

                if (desktop == effects->currentDesktop() || desktop == -1) {
                    m_effectFlyingBack.begin();
                    effects->addRepaintFull();
                } else {
                    changeCurrentDesktop(desktop);
                }
            }
        } else if (m_workspaceStatus == wpSwitch) {
            switchDesktop();
        } else if (m_workspaceStatus == wpDelete) {
            removeDesktop(m_aciveMoveDesktop);
        } else if (m_workspaceStatus == wpRestore) {
            MultiViewWorkspace *wt = getWorkspaceObject(m_screen, m_aciveMoveDesktop - 1);
            if (wt)
                wt->updateGeometry(wt->getRect());
        } else if (!m_wasWorkspaceMove && !m_wasWindowMove) {
            m_effectFlyingBack.begin();
            effects->addRepaintFull();
        }

        m_workspaceStatus = wpNone;
        m_hoverDesktop = -1;
        m_aciveMoveDesktop = -1;
        m_screen = -1;
        m_wasWorkspaceMove = false;
        m_moveWorkspaceNum = -1;
        m_moveWorkspacedirection = mvNone;
        m_dragWorkspacedirection = dragNone;
        m_windowMove = nullptr;
        m_wasWindowMove = false;
        m_longPressTouch = false;

        break;
    default:
        return;
    }
}

void MultitaskViewEffect::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (!isReceiveEvent()) {
        return;
    }

    if (e->type() == QEvent::KeyPress) {
        if (shortcut.contains(e->key() + e->modifiers())) {
            toggle();
            return;
        }
        switch (e->key()) {
        case Qt::Key_Escape:
            m_effectFlyingBack.begin();
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
                        m_effectFlyingBack.begin();
                        effects->addRepaintFull();
                    }
                }
            }
            break;
        case Qt::Key_Exclam:
        case Qt::Key_At: // shift+2
        case Qt::Key_NumberSign: // shift+3
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
                if (m_hoverWin) {
                    moveWindowChangeDesktop(m_hoverWin, target_desktop, m_hoverWin->screen());
                }
            }
            break;
        case Qt::Key_Home:
            if (e->modifiers() == Qt::NoModifier) {
                m_hoverWin = getHomeOrEndWindow(true);
            }
            break;
        case Qt::Key_End:
            if (e->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier)) {
                if (m_hoverWin) {
                    moveWindowChangeDesktop(m_hoverWin, 1, m_hoverWin->screen());
                }
            } else if (e->modifiers() == Qt::NoModifier) {
                m_hoverWin = getHomeOrEndWindow(false);
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
                if (m_hoverWin) {
                    moveWindowChangeDesktop(m_hoverWin, target_desktop, m_hoverWin->screen());
                }
            }
            break;
        case Qt::Key_Down:
            if (e->modifiers() == (Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier)) {
                if (m_hoverWin) {
                    moveWindowChangeDesktop(m_hoverWin, 2, m_hoverWin->screen());
                }
            } else if (e->modifiers() == Qt::NoModifier) {
                if (m_hoverWin) {
                    m_hoverWin = getNextWindow(m_hoverWin);
                }
            }
            break;
        case Qt::Key_Up:
            if (e->modifiers() == Qt::NoModifier) {
                if (m_hoverWin) {
                    m_hoverWin = getPreWindow(m_hoverWin);
                }
            }
            break;
        case Qt::Key_Equal:
            if (e->modifiers() == Qt::AltModifier) {
                addNewDesktop();
            }
            break;
        case Qt::Key_Plus:
            if (e->modifiers() == (Qt::AltModifier|Qt::KeypadModifier)) {
                addNewDesktop();
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
                if (m_hoverWin) {
                    m_hoverWin = getNextWindow(m_hoverWin);
                } else {
                    m_hoverWin = getHomeOrEndWindow(true);
                }
            }
            break;
        case Qt::Key_Backtab:
            if (e->modifiers() == Qt::ShiftModifier) {
                if (m_hoverWin) {
                    m_hoverWin = getPreWindow(m_hoverWin);
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
                if (m_hoverWin) {
                    moveWindowChangeDesktop(m_hoverWin, 6, m_hoverWin->screen());
                }
            } else if (e->modifiers() == Qt::NoModifier) {
                if (m_hoverWin) {
                    m_hoverWin = getNextWindow(m_hoverWin);
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
                if (m_hoverWin) {
                    moveWindowChangeDesktop(m_hoverWin, 4, m_hoverWin->screen());
                }
            } else if (e->modifiers() == Qt::NoModifier) {
                if (m_hoverWin) {
                    m_hoverWin = getPreWindow(m_hoverWin);
                }
            }
            break;
        case Qt::Key_Return:
            if (m_hoverWin) {
                effects->defineCursor(Qt::PointingHandCursor);
                effects->setElevatedWindow(m_hoverWin, false);
                effects->activateWindow(m_hoverWin);

                m_effectFlyingBack.setSelecedWindow(m_hoverWin);
                m_effectFlyingBack.begin();
            }
            break;
        case Qt::Key_Delete:
            if (e->modifiers() == Qt::NoModifier) {
                if (m_hoverWin && closeWindow(m_hoverWin)) {
                    m_hoverWin = nullptr;
                    m_isShieldEvent = true;
                }
            }
            break;
        case Qt::Key_L:
            if (e->modifiers() == Qt::MetaModifier) {
                setActive(false);
                effectsEx->requestLock();
            }
            break;
        case Qt::Key_QuoteLeft:
            if (e->modifiers() == Qt::AltModifier) {
                if (m_hoverWin) {
                    m_hoverWin = getNextSameTypeWindow(m_hoverWin);
                }
            }
            break;
        case Qt::Key_AsciiTilde:
            if (e->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)) {
                if (m_hoverWin) {
                    m_hoverWin = getPreSameTypeWindow(m_hoverWin);
                }
            }
            break;
        default:
            if (!QX11Info::isPlatformX11() && effectsEx->isShortcuts(e)) {
                effects->ungrabKeyboard();
                m_hasKeyboardGrab = false;
            }
            break;
        }
    }
}

void MultitaskViewEffect::relayDockEvent(QPoint pos, int button)
{
    if (m_dock) {
        auto cl = static_cast<EffectWindowImpl *>(m_dock)->window();
        xcb_button_release_event_t c;
        memset(&c, 0, sizeof(c));

        c.response_type = ButtonPress;
        c.event = cl->window();
        c.event_x = pos.x() - m_dockRect.x();
        c.event_y = pos.y() - m_dockRect.y();
        c.detail = (button == 1 ? 1 : 3);  // 1 左键 3 右键
        xcb_send_event(connection(), false, c.event, XCB_EVENT_MASK_BUTTON_PRESS, reinterpret_cast<const char*>(&c));
        xcb_flush(connection());

        memset(&c, 0, sizeof(c));
        c.response_type = ButtonRelease;
        c.event = cl->window();
        c.event_x = pos.x() - m_dockRect.x();
        c.event_y = pos.y() - m_dockRect.y();
        c.detail = (button == 1 ? 1 : 3);
        xcb_send_event(connection(), false, c.event, XCB_EVENT_MASK_BUTTON_RELEASE, reinterpret_cast<const char*>(&c));
        xcb_flush(connection());
    } // button release
}

bool MultitaskViewEffect::isActive() const
{
    return m_activated && !effects->isScreenLocked();
}

void MultitaskViewEffect::toggle()
{
    if (!QX11Info::isPlatformX11()) { // MultitaskView cannot be opened without login in Wayland mode
        if(qgetenv("DISPLAY").isEmpty()) {
            return;
        }
    }

    Dl_info dli;
    dladdr(__builtin_return_address(1), &dli);

    QString name =  dli.dli_sname;
    if (name.contains("activate")) {
        KWin::Report::writeEventLog(KWin::Report::TriggerMutitaskview, "Shortcut key");
    } else {
        KWin::Report::writeEventLog(KWin::Report::TriggerMutitaskview, "button");
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
    }
}

void MultitaskViewEffect::cleanup()
{
    if (m_activated) {
        return;
    }

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

    for (int i = 0; i < m_addWorkspaceButton.size(); i++) {
        if (m_addWorkspaceButton[i]) {
            delete m_addWorkspaceButton[i];
            m_addWorkspaceButton[i] = nullptr;
        }
    }

    for (int i = 0; i < m_tipFrames.size(); i++) {
        for (int j = 0; j < m_tipFrames[i].size(); j++) {
            if (m_tipFrames[i][j]) {
                delete m_tipFrames[i][j];
                m_tipFrames[i][j] = nullptr;
            }
        }
    }

    for (int i = 0; i < m_workspaceBackgrounds.size(); i++) {
        QList<MultiViewWorkspace *> list = m_workspaceBackgrounds[i];
        for (int j = 0; j < list.size(); j++) {
            delete list[j];
            list[j] = nullptr;
        }
    }

    m_maxHeight = 0;
    m_windowMove = nullptr;
    m_hoverWin = nullptr;
    m_motionManagers.clear();
    m_workspaceWinMgr.clear();
    m_addWorkspaceButton.clear();
    m_workspaceBackgrounds.clear();
    m_tipFrames.clear();
    m_flyingWinList.clear();
    MultiViewBackgroundManager::instance()->clearCurrentBackgroundList();
}

void MultitaskViewEffect::setActive(bool active)
{
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return;

    if (m_activated == active)
        return;

    m_activated = active;

    QTimer::singleShot(400, [&, active]() {
        QDBusInterface wm(DBUS_DEEPIN_WM_SERVICE, DBUS_DEEPIN_WM_OBJ, DBUS_DEEPIN_WM_INTF);
        wm.asyncCall("SetMultiTaskingStatus", active);
    });

    if (!QX11Info::isPlatformX11()) {
        effectsEx->changeBlurState(active);
    }

    if(!active) {
        m_popStatus = false;
        m_popTimeLine.reset();

        m_opacityStatus = false;
        m_opacityTimeLine.reset();

        m_bgSlidingStatus = false;
        m_bgSlidingTimeLine.reset();

        m_effectFlyingBack.reset();
    } else {
        m_popStatus = true;   //start popup TimeLine
        m_popTimeLine.reset();

        m_opacityStatus = false;
        m_opacityTimeLine.reset();

        m_bgSlidingStatus = false;
        m_bgSlidingTimeLine.reset();

        m_effectFlyingBack.reset();

        m_isShieldEvent = false;
    }


    cleanup();
    if (active) {
        initWorkspaceBackground();
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        m_hasKeyboardGrab = effects->grabKeyboard(this);
        effects->setActiveFullScreenEffect(this);

        EffectWindowList windowsList = effects->stackingOrder();
        int ncurrentDesktop = effects->currentDesktop();
        m_backgroundRect = effects->clientArea(MaximizeFullArea, 0, ncurrentDesktop);
        m_curDesktopIndex = ncurrentDesktop;

        for (int i = 1; i <= effects->numberOfDesktops(); i++) {        //number of desktops
            setWinLayout(i, windowsList);
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
    MultiViewWinManager *wmofDesktop = new MultiViewWinManager();
    MultiViewWinManager *wmofWorkspace = new MultiViewWinManager();

    wmofDesktop->setdesktop(desktop);
    m_motionManagers.append(wmofDesktop);

    MultiViewWinManager *wmobj = getWinManagerObject(desktop - 1);

    for (int j = 0; j < getNumScreens(); j++) {           //number of screens
        WindowMotionManager wmm;
        WindowMotionManager workspacewmm;
        EffectWindowList winList;
        for (const auto& w: windows) {
            if (w->isOnDesktop(desktop) && isRelevantWithPresentWindows(w) && w->screen() == j && checkConfig(w)) {
                wmm.manage(w);
                workspacewmm.manage(w);
                winList.push_back(w);
            } else if (w->isDock()) {
                m_dockRect = w->geometry();
                m_dock = w;
            }
        }

        calculateWindowTransformations(winList, wmm, desktop, j);

        wmobj->setWinManager(j, wmm);

        calculateWorkSpaceWinTransformations(workspacewmm.managedWindows(), workspacewmm, desktop);
        wmofWorkspace->setWinManager(j, workspacewmm);
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
        for (int s = 0; s < getNumScreens(); s++) {
            WindowMotionManager *wmm;
            if (!m_workspaceWinMgr[i]->getMotion(i + 1, s, wmm)) {
                continue;
            }
            calculateWorkSpaceWinTransformations(wmm->managedWindows(), *wmm, i + 1);
        }
    }
}

void MultitaskViewEffect::workspaceWinRelayout(int desktop, int screen)
{
    WindowMotionManager *wmm;
    MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(desktop - 1);
    if (wkmobj && wkmobj->getMotion(desktop, screen, wmm)) {
        calculateWorkSpaceWinTransformations(wmm->managedWindows(), *wmm, desktop);
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
        if (w->caption() == "dde-osd" || cl->isStandAlone()) {
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

void MultitaskViewEffect::calculateWorkSpaceWinTransformations(EffectWindowList windows, WindowMotionManager &wm, int desktop)
{
    if (0 == windows.size() || 0 == m_workspaceBackgrounds.size()) {
        return;
    }

    for (int i = 0; i < windows.size(); i++) {
        EffectWindow *w = windows[i];
        MultiViewWorkspace *wkobj = getWorkspaceObject(w->screen(), m_deleteWorkspaceDesktop == 1 ? m_deleteWorkspaceDesktop : desktop - 1);
        if (!wkobj)
            continue;
        QPoint pos((w->x() - wkobj->getfullArea().x()) * WORKSPACE_SCALE + wkobj->getRect().x(), (w->y() - wkobj->getfullArea().y()) * WORKSPACE_SCALE + wkobj->getRect().y());
        QSize size(w->width() * WORKSPACE_SCALE, w->height() * WORKSPACE_SCALE);
        QRect rect(pos, size);
        wm.setTransformedGeometry(w, rect);
    }
}

void MultitaskViewEffect::calculateWindowTransformations(EffectWindowList windows, WindowMotionManager& wmm, int desktop, int screen, bool isReLayout)
{
    if (windows.size() == 0)
        return;
    calculateWindowTransformationsClosest(windows, desktop, screen, wmm, isReLayout);
}

void MultitaskViewEffect::calculateWindowTransformationsClosest(EffectWindowList windowlist, int desktop, int screen,
        WindowMotionManager& motionManager, bool isReLayout)
{
    QHash<EffectWindow*, QRect> targets;
    foreach (EffectWindow *w, windowlist) {
        QRect rect = w->geometry();
        targets[w] = rect;
    }

    QRect screenRect = effects->clientArea(MaximizeFullArea, screen, 1);
    QRect desktopRect = effects->clientArea(MaximizeArea, screen, 1);
    float scaleHeight = screenRect.height() * FIRST_WIN_SCALE;
    int minSpacingH = m_scale[screen].spacingHeight;
    int totalw = m_scale[screen].spacingWidth;
    QRect clientRect = desktopRect;
    clientRect.setY(clientRect.y() + m_scale[screen].workspaceMgrHeight);

    QList<int> centerList;
    int row = 1;
    int index = 1;
    int xpos = 0;
    bool overlap;
    do {
        overlap = false;
        for (int i = windowlist.size() - 1; i >= 0; i--) {
            EffectWindow *w = windowlist[i];
            QRect *target = &targets[w];
            float width = target->width();
            if (target->height() > scaleHeight) {
                float scale = (float)(scaleHeight / target->height());
                width = target->width() * scale;
            }
            totalw += width;
            totalw += m_scale[screen].spacingWidth;

            if (totalw > clientRect.width()) {
                index ++;
                if (index > row)
                    break;
                xpos = ((clientRect.width() - totalw + width + m_scale[screen].spacingWidth) / 2) + m_scale[screen].spacingWidth + clientRect.x();
                centerList.push_back(xpos);
                totalw = m_scale[screen].spacingWidth;
                totalw += width;
                totalw += m_scale[screen].spacingWidth;
            }
        }
        xpos = ((clientRect.width() - totalw) / 2) + m_scale[screen].spacingWidth + clientRect.x();
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
            totalw = m_scale[screen].spacingWidth;
        }
    } while (overlap);  //calculation layout row

    float winYPos = (clientRect.height() - (index - 1) * minSpacingH - index * scaleHeight) / 2 + clientRect.y();
    row = 1;
    int x = centerList[row - 1];
    totalw = m_scale[screen].spacingWidth;
    for (int i = windowlist.size() - 1; i >= 0; i--) {
        EffectWindow *w = windowlist[i];
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
        totalw += m_scale[screen].spacingWidth;
        if (totalw > clientRect.width()) {
            row++;
            totalw = m_scale[screen].spacingWidth;
            totalw += width;
            totalw += m_scale[screen].spacingWidth;
            x = centerList[row - 1];
            winYPos += minSpacingH;
            winYPos += scaleHeight;
        }

        if (isReLayout) {
            motionManager.resetWindowFill(w);
            removeBackgroundFill(w, desktop);
        }

        target->setRect(x, winYPos + (scaleHeight - height) / 2, width, height);
        if (isFill) {
            QRect rect(x, winYPos, width, scaleHeight);
            motionManager.setWindowFill(w, true, rect);
            createBackgroundFill(w, rect, desktop);
        }
        x += width;
        x += m_scale[screen].spacingWidth;

        motionManager.moveWindow(w, targets.value(w));

        //store window grids infomation
        m_effectFlyingBack.add(w, targets.value(w));
    }
}

QRect MultitaskViewEffect::calculateWorkspaceRect(int index, int screen, QRect maxRect)
{
    int spacingScale = maxRect.y() + m_scale[screen].spacingHeight;
    int workspacingScale = m_scale[screen].workspacingWidth;
    int nums = effects->numberOfDesktops();
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
            } else if (m_moveWorkspacedirection == mvRight && anum > (effects->numberOfDesktops() - m_aciveMoveDesktop)) {
                anum = effects->numberOfDesktops() - m_aciveMoveDesktop;
            }

            if (m_moveWorkspaceNum < fnum) {
                m_moveWorkspaceNum = fnum;
            }

            for (int i = 0; i <= fnum; i++) {
                if (m_moveWorkspacedirection == mvLeft) {
                    MultiViewWorkspace *target = getWorkspaceObject(m_screen, m_aciveMoveDesktop - 2 - i);
                    MultiViewWorkspace *pretarget = getWorkspaceObject(m_screen, m_aciveMoveDesktop - i - 1);
                    MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(m_aciveMoveDesktop - 2 - i);
                    if (target && pretarget && wkmobj && target->m_posStatus != mvRight) {
                        target->m_posStatus = mvRight;
                        target->updateGeometry(pretarget->getRect());

                        int xdiff = pretarget->getRect().x() - target->getRect().x();
                        wkmobj->updatePos(m_screen, target->getCurrentRect(), QPoint(xdiff, 0));
                    }
                } else if (m_moveWorkspacedirection == mvRight) {
                    MultiViewWorkspace *target = getWorkspaceObject(m_screen, m_aciveMoveDesktop + i);
                    MultiViewWorkspace *pretarget = getWorkspaceObject(m_screen, m_aciveMoveDesktop + i - 1);
                    MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(m_aciveMoveDesktop + i);
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
        MultiViewWorkspace *wkobj = getWorkspaceObject(m_screen, index);
        if (wkobj) {
            if (wkobj->m_posStatus == mvNone) {
                continue;
            } else {
                wkobj->m_posStatus = mvNone;
                QRect currentR = wkobj->getCurrentRect();
                wkobj->updateGeometry(wkobj->getRect());

                int xdiff = wkobj->getCurrentRect().x() - currentR.x();
                MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(index);
                if (xdiff != 0 && wkmobj) {
                    wkmobj->updatePos(m_screen, wkobj->getCurrentRect(), QPoint(xdiff, 0));
                }
            }
        }
    }
}

void MultitaskViewEffect::initWorkspaceBackground()
{
    getScreenInfo();

    int count = effects->numberOfDesktops();
    QHash<QString, ScreenInfo_st>::iterator it = m_screenInfoList.begin();
    for (; it != m_screenInfoList.end(); it++) {
        QList<MultiViewWorkspace *> list;
        for (int i = 1; i <= count; i++) {
            MultiViewWorkspace *b = new MultiViewWorkspace();
            QRect rect = calculateWorkspaceRect(i, it.value().screen, it.value().rect);
            BgInfo_st st;
            st.desktop = i;
            st.screenName = it.value().name;
            st.desktopSize = it.value().screenrect.size();
            st.workspaceSize = rect.size();
            QPixmap bgPix, wpPix;
            MultiViewBackgroundManager::instance()->getWorkspaceBgPath(st, bgPix, wpPix);
            b->setArea(it.value().rect, it.value().screenrect);
            b->setMaxHeight(m_maxHeight);
            b->setImage(bgPix, wpPix, rect);
            b->setScreenDesktop(it.value().screen, i);
            list.push_back(b);
        }
        m_workspaceBackgrounds[it.value().screen] = list;

        {
            EffectFrame *bgframe = effects->effectFrame(EffectFrameStyled, false);
            bgframe->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            EffectFrame *frame = effects->effectFrame(EffectFrameStyled, false);
            frame->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            QFont font;
            font.setFamily(m_fontFamily);
            font.setPointSize(12);
            frame->setFont(font);
            frame->setText(tr("No windows"));
            frame->setPosition(QPoint(it.value().screenrect.width() / 2 + it.value().screenrect.x(), it.value().screenrect.height() / 2 + it.value().screenrect.y()));
            m_tipFrames[it.value().screen].push_back(bgframe);
            m_tipFrames[it.value().screen].push_back(frame);

            MultiViewWorkspace *addButton = new MultiViewWorkspace(true);
            QRect buttonRect(it.value().rect.x() + it.value().rect.width() - 104, it.value().rect.y() + 56, 64, 64);
            QString buttonImage = QLatin1String(add_workspace_png);
            addButton->setArea(it.value().rect, it.value().screenrect);
            addButton->setMaxHeight(m_maxHeight);
            addButton->setImage(buttonImage, buttonRect);
            m_addWorkspaceButton[it.value().screen] = addButton;
        }
    }

    MultiViewBackgroundManager::instance()->getBackgroundList();
}

void MultitaskViewEffect::cacheWorkspaceBackground()
{
    getScreenInfo();
    int count = effects->numberOfDesktops();
    QHash<QString, ScreenInfo_st>::iterator it = m_screenInfoList.begin();
    for (; it != m_screenInfoList.end(); it++) {
        for (int i = 1; i <= count; i++) {
            QRect rect = calculateWorkspaceRect(i, it.value().screen, it.value().rect);
            BgInfo_st st;
            st.desktop = i;
            st.screenName = it.value().name;
            st.desktopSize = it.value().screenrect.size();
            st.workspaceSize = rect.size();
            CustomThread *thread = new CustomThread(st);
            connect(thread, &QThread::finished, thread, &QObject::deleteLater);
            thread->start();
        }
    }
}

void MultitaskViewEffect::updateWorkspacePos()
{
    for (int i = 0; i < m_workspaceBackgrounds.size(); i++) {
        QList<MultiViewWorkspace *> list = m_workspaceBackgrounds[i];
        for (int j = 0; j < list.size(); j++) {
            QRect rect = calculateWorkspaceRect(j + 1, i, list[j]->getClientArea());
            list[j]->setRect(rect);
        }
    }
}

void MultitaskViewEffect::getScreenInfo()
{
    m_screenInfoList.clear();
    for (int i = 0; i < getNumScreens(); i++) {
        QRect rect = effects->clientArea(FullScreenArea, i, effects->currentDesktop());
        QRect maxRect = effects->clientArea(MaximizeArea, i, effects->currentDesktop());
        QPoint pos(rect.x() + 2, rect.y() + 2);
        QScreen *pScreen = QGuiApplication::screenAt(pos);

        ScreenInfo_st infost;
        infost.rect = maxRect;
        infost.screenrect = rect;
        infost.screen = i;
        if (!QX11Info::isPlatformX11()) {
            if (!isExtensionMode()) {
                //get primary screen in copy mode
                QDBusInterface interface(DBUS_DAEMON_DISPLAY_SERVICE, DBUS_DAEMON_DISPLAY_OBJ, DBUS_DAEMON_DISPLAY_INIF);
                infost.name = interface.property("Primary").toString();
            } else {
                infost.name = effectsEx->getScreenNameForWayland(i);
            }
        } else {
            infost.name = pScreen->name();
        }

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
        m_scale[i] = scalest;

        if (m_maxHeight < rect.y() + rect.height())
            m_maxHeight = rect.y() + rect.height();
    }
}

int MultitaskViewEffect::getNumScreens()
{
    int num = 1;
    if (isExtensionMode()) {
        num = effects->numScreens();
    } else if (effects->numScreens() <= 0) {
        num = 0;
    }
    return num;
}

bool MultitaskViewEffect::isExtensionMode() const
{
    if (effects->numScreens() >= 2) {
        QRect rect = effects->clientArea(FullScreenArea, 0, effects->currentDesktop());
        if (effects->virtualScreenGeometry() != rect) {
            return true;
        }
    }
    return false;
}

void MultitaskViewEffect::createBackgroundFill(EffectWindow *w, QRect rect, int desktop)
{
    MultiViewWinManager *wmobj = getWinManagerObject(desktop - 1);
    if (wmobj && wmobj->isHaveWinFill(w))
        return;

    MultiViewWinFill *fill = new MultiViewWinFill(w->screen(), rect, m_maxHeight);
    wmobj->setWinFill(w, fill);
}

void MultitaskViewEffect::removeBackgroundFill(EffectWindow *w, int desktop)
{
    MultiViewWinManager *wmobj = getWinManagerObject(desktop - 1);
    if (wmobj && !wmobj->isHaveWinFill(w)) {
        return;
    }
    wmobj->removeWinFill(w);
}

void MultitaskViewEffect::addNewDesktop()
{
    KWin::Report::writeEventLog(KWin::Report::TriggerMoveWorkspace);
    int count = effects->numberOfDesktops();
    if (count >= MAX_DESKTOP_COUNT)
        return;

    m_isShieldEvent = true;
    effects->setNumberOfDesktops(count + 1);
    QList<QString> list = m_screenInfoList.keys();
    for (int i = 0; i < list.size(); i++) {
        QString screenName = list[i];
        ScreenInfo_st &st = m_screenInfoList[screenName];
        QRect rect = calculateWorkspaceRect(count + 1, st.screen, st.rect);
        BgInfo_st bgst;
        bgst.desktop = count + 1;
        bgst.screen = st.screen;
        bgst.screenName = screenName;
        bgst.desktopSize = st.screenrect.size();
        bgst.workspaceSize = rect.size();
        QPixmap bgPix, wpPix;
        MultiViewBackgroundManager::instance()->setNewBackground(bgst, bgPix, wpPix);
        MultiViewWorkspace *b = new MultiViewWorkspace();
        b->setMaxHeight(m_maxHeight);
        b->setArea(st.rect, st.screenrect);
        b->setImage(bgPix, wpPix, rect);
        b->setScreenDesktop(st.screen, count + 1);
        m_workspaceBackgrounds[st.screen].push_back(b);
    }

    {
        m_workspaceSlidingStatus = true;
        m_workspaceSlidingTimeline.reset();
        m_workspaceSlidingInfo.clear();
        {
            for (int i = 0; i < m_workspaceBackgrounds.size(); i++) {
                QList<MultiViewWorkspace *> list = m_workspaceBackgrounds[i];
                for (int j = 0; j < list.size(); j++) {
                    QRect rect = list[j]->getRect();
                    m_workspaceSlidingInfo[list[j]].first = rect.x();
                    if (j == list.size() - 1 && m_previewFrame) {
                        //m_workspaceSlidingInfo[list[j]].first = m_previewFrame->geometry().x();
                        m_workspaceSlidingInfo[list[j]].first = m_previewFramePosX;                    
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
            for (int i = 0; i < m_workspaceBackgrounds.size(); i++) {
                QList<MultiViewWorkspace *> list = m_workspaceBackgrounds[i];
                for (int j = 0; j < list.size(); j++) {
                    QRect rect = list[j]->getRect();
                    m_workspaceSlidingInfo[list[j]].second = rect.x();
                }
            }
        }
    }

    changeCurrentDesktop(effects->numberOfDesktops());
    m_isShieldEvent = false;
}

void MultitaskViewEffect::removeDesktop(int desktop)
{
    KWin::Report::writeEventLog(KWin::Report::TriggerAddWorkspace);
    m_isShieldEvent = true;
    int count = effects->numberOfDesktops();
    if (desktop <= 0 || desktop > count || count == 1) {
        MultiViewWorkspace *wt = getWorkspaceObject(m_screen, desktop - 1);
        if (wt) {
            wt->updateGeometry(wt->getRect());
        }
        m_isShieldEvent = false;
        return;
    }
    m_flyingWinList.clear();
    m_deleteWorkspaceDesktop = desktop;
    int currentDesktop = effects->currentDesktop();
    if (currentDesktop == desktop) {
        MultiViewWinManager *wmobj = getWinManagerObject(desktop - 1);
        if (wmobj)
            m_flyingWinList = wmobj->getDesktopWinList();
    }

    if (m_motionManagers.size() >= desktop) {
        delete m_motionManagers[desktop - 1];
        m_motionManagers.erase(m_motionManagers.begin() + desktop - 1);
    }
    if (m_workspaceWinMgr.size() >= desktop) {
        delete m_workspaceWinMgr[desktop - 1];
        m_workspaceWinMgr.erase(m_workspaceWinMgr.begin() + desktop - 1);
    }

    bool isRelayout = false;
    int newd = 0;
    QSet<int> screens;
    for (const auto &ew : effects->stackingOrder()) {
        if (ew->isOnAllDesktops()) {
            continue;
        }

        auto dl = ew->desktops();
        if (dl.size() == 0 || dl[0] < desktop) {
            continue;
        }

        if (dl[0] == desktop) {
            newd = dl[0] == 1 ? 1 : dl[0] - 1;
            MultiViewWinManager *wmobj = getWinManagerObject(newd - 1);
            MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(newd - 1);
            if (wmobj && wkmobj) {
                isRelayout = true;
                wmobj->manageWin(ew->screen(), ew);
                wkmobj->manageWin(ew->screen(), ew);
                screens.insert(ew->screen());
            }
        }
    }

    for (const auto &ew : effects->stackingOrder()) {
        if (ew->isOnAllDesktops()) {
            continue;
        }

        auto dl = ew->desktops();
        if (dl.size() == 0 || dl[0] < desktop) {
            continue;
        }

        if (effectsEx->isTransientWin(ew) && !ew->isModal()) {
            continue;
        }
        newd = dl[0] == 1 ? 1 : dl[0] - 1;
        QVector<uint> desks{(uint)newd};
        effects->windowToDesktops(ew, desks);
    }

    desktopAboutToRemoved(desktop);

    int nrelyout = desktop == 1 ? desktop : (desktop - 1);
    if (isRelayout) {
        QSet<int>::iterator iter;
        for (iter = screens.begin(); iter != screens.end(); iter++) {
            WindowMotionManager *wmm;
            MultiViewWinManager *wmobj = getWinManagerObject(nrelyout - 1);
            if (wmobj && wmobj->getMotion(nrelyout, *iter, wmm)) {
                calculateWindowTransformations(wmm->orderManagedWindows(), *wmm, nrelyout, *iter, true);
            }
            workspaceWinRelayout(nrelyout, *iter);
        }
    }

    if (currentDesktop == desktop) {
        effects->setCurrentDesktop(nrelyout);
        m_bgSlidingStatus = true;
        m_bgSlidingTimeLine.reset();
        if (m_curDesktopIndex == count)
            m_curDesktopIndex = desktop - 1;
	else
            m_curDesktopIndex = desktop + 1;
        m_lastDesktopIndex = desktop;
        m_isRemoveWorkspace = true;
    } else {
        m_curDesktopIndex = effects->currentDesktop();
        if (m_curDesktopIndex > desktop)
            effects->setCurrentDesktop(m_curDesktopIndex == 1 ? m_curDesktopIndex : (m_curDesktopIndex - 1));
        removeDesktopEx(desktop);
    }
}

void MultitaskViewEffect::removeDesktopEx(int desktop)
{
    QMutexLocker locker(&m_mutex);
    effects->setNumberOfDesktops(effects->numberOfDesktops() - 1);
    m_isRemoveWorkspace = false;
    m_deleteWorkspaceDesktop = -1;
    for (int i = 0; i < getNumScreens(); i++) {
        delete m_workspaceBackgrounds[i][desktop - 1];
        m_workspaceBackgrounds[i].removeAt(desktop - 1);
    }

    {
        m_workspaceSlidingStatus = true;
        m_workspaceSlidingTimeline.reset();
        m_workspaceSlidingInfo.clear();
        {
            for (int i = 0; i < m_workspaceBackgrounds.size(); i++) {
                QList<MultiViewWorkspace *> list = m_workspaceBackgrounds[i];
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
        for (int i = 0; i < m_workspaceBackgrounds.size(); i++) {
            QList<MultiViewWorkspace *> list = m_workspaceBackgrounds[i];
            for (int j = 0; j < list.size(); j++) {
                QRect rect = list[j]->getRect();
                m_workspaceSlidingInfo[list[j]].second = rect.x();
            }
        }
    }

    m_isShieldEvent = false;
}

void MultitaskViewEffect::switchDesktop()
{
    for (int i = 0; i < getNumScreens(); i++) {
        for (int j = 0; j <= m_moveWorkspaceNum; j++) {
            MultiViewWorkspace *target = getWorkspaceObject(i, m_aciveMoveDesktop - 1);
            MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(m_aciveMoveDesktop - 1);
            if (target && wkmobj && m_moveWorkspacedirection == mvLeft) {
                MultiViewWorkspace *nexttarget = getWorkspaceObject(i, m_aciveMoveDesktop - 2 -j);
                if (nexttarget) {
                    QRect tmpRect = target->getRect();
                    target->setRect(nexttarget->getRect());
                    nexttarget->setRect(tmpRect);

                    if (i != m_screen) {
                        MultiViewWinManager *wkmobj1 = getWorkspaceWinManagerObject(m_aciveMoveDesktop - 2 - j);
                        if (wkmobj1) {
                            int xdiff = target->getRect().x() - tmpRect.x();
                            wkmobj->updatePos(i, target->getRect(), QPoint(xdiff, 0));
                            wkmobj1->updatePos(i, tmpRect, QPoint(-xdiff, 0));
                        }
                    }
                    if (nexttarget->m_posStatus != mvNone) {
                        nexttarget->m_posStatus = mvNone;
                    }
                }
            } else if (target && wkmobj && m_moveWorkspacedirection == mvRight) {
                QRect tmpRect = target->getRect();
                MultiViewWorkspace *pretarget = getWorkspaceObject(i, m_aciveMoveDesktop + j);
                if (pretarget) {
                    target->setRect(pretarget->getRect());
                    pretarget->setRect(tmpRect);

                    if (i != m_screen) {
                        MultiViewWinManager *wkmobj1 = getWorkspaceWinManagerObject(m_aciveMoveDesktop + j);
                        if (wkmobj1) {
                            int xdiff = target->getRect().x() - tmpRect.x();
                            wkmobj->updatePos(i, target->getRect(), QPoint(xdiff, 0));
                            wkmobj1->updatePos(i, tmpRect, QPoint(-xdiff, 0));
                        }
                    }
                    if (pretarget->m_posStatus != mvNone) {
                        pretarget->m_posStatus = mvNone;
                    }
                }
            }
        }

        if (m_moveWorkspacedirection == mvLeft) {
            MultiViewWorkspace *wp = m_workspaceBackgrounds[i].takeAt(m_aciveMoveDesktop - 1);
            m_workspaceBackgrounds[i].insert(m_aciveMoveDesktop - 2 - m_moveWorkspaceNum, wp);
        } else if (m_moveWorkspacedirection == mvRight) {
            MultiViewWorkspace *wp = m_workspaceBackgrounds[i].takeAt(m_aciveMoveDesktop - 1);
            m_workspaceBackgrounds[i].insert(m_aciveMoveDesktop + m_moveWorkspaceNum, wp);
        }
    }

    int ncurrent = effects->currentDesktop();
    int index = m_aciveMoveDesktop - 1;
    MultiViewWinManager *wmobj = getWinManagerObject(index);
    MultiViewWorkspace *target = getWorkspaceObject(m_screen, index);
    MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(index);
    if (wmobj && target && wkmobj && m_moveWorkspacedirection == mvLeft) {
        MultiViewWorkspace *nexttarget = getWorkspaceObject(m_screen, index - 1 - m_moveWorkspaceNum);
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
        MultiViewWorkspace *pretarget = getWorkspaceObject(m_screen, index + 1 + m_moveWorkspaceNum);
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
        if (dl[0] == m_aciveMoveDesktop && !ew->isModal()) {
            QVector<uint> desks {(uint)(m_aciveMoveDesktop - dir - m_moveWorkspaceNum * dir)};
            effects->windowToDesktops(ew, desks);
        } else if (dl[0] >= (m_aciveMoveDesktop - dir - (dir > 0 ? m_moveWorkspaceNum : 0)) && dl[0] <= (m_aciveMoveDesktop - dir + (dir > 0 ? 0 : m_moveWorkspaceNum)) && !ew->isModal()) {
            int newd = dl[0] + dir;
            QVector<uint> desks {(uint)newd};
            effects->windowToDesktops(ew, desks);
        }
    }
    effects->setCurrentDesktop(ncurrent);
}

void MultitaskViewEffect::desktopSwitchPosition(int to, int from)
{
    KWin::Report::writeEventLog(KWin::Report::TriggerDeleteWorkspace);
    QDBusInterface wm(DBUS_DEEPIN_WM_SERVICE, DBUS_DEEPIN_WM_OBJ, DBUS_DEEPIN_WM_INTF);

    QList<QString> list = m_screenInfoList.keys();
    for (int i = 0; i < list.size(); i++) {
        QString monitorName = list[i];

        QDBusReply<QString> getReply = wm.call( "GetWorkspaceBackgroundForMonitor", from, monitorName);
        QString strFromUri;
        if (!getReply.value().isEmpty()) {
            strFromUri = getReply.value();
        } else {
            strFromUri = QLatin1String(fallback_background_name);
        }

        if (from < to) {
            for (int j = from - 1; j < to; j++) {
                int desktopIndex = j + 1; //desktop index
                if ( desktopIndex == to) {
                    wm.call( "SetWorkspaceBackgroundForMonitor", desktopIndex, monitorName, strFromUri);
                } else {
                    QDBusReply<QString> getReply = wm.call( "GetWorkspaceBackgroundForMonitor", desktopIndex + 1, monitorName);
                    QString backgroundUri;
                    if (!getReply.value().isEmpty()) {
                        backgroundUri = getReply.value();
                    } else {
                        backgroundUri = QLatin1String(fallback_background_name);
                    }
                    wm.call( "SetWorkspaceBackgroundForMonitor", desktopIndex, monitorName, backgroundUri);
                }
            }
        } else {
            for (int j = from; j > to - 1; j--) {
                if (j == to) {
                    QDBusReply<QString> setReply = wm.call( "SetWorkspaceBackgroundForMonitor", to, monitorName, strFromUri);
                } else {
                    QDBusReply<QString> getReply = wm.call( "GetWorkspaceBackgroundForMonitor", j - 1, monitorName);
                    QString backgroundUri;
                    if(!getReply.value().isEmpty()) {
                        backgroundUri = getReply.value();
                    } else {
                        backgroundUri = QLatin1String(fallback_background_name);
                    }
                    QDBusReply<QString> setReply = wm.call( "SetWorkspaceBackgroundForMonitor", j, monitorName, backgroundUri);
                }
            }
        }
    }
}

void MultitaskViewEffect::desktopAboutToRemoved(int d)
{
    QDBusInterface wm(DBUS_DEEPIN_WM_SERVICE, DBUS_DEEPIN_WM_OBJ, DBUS_DEEPIN_WM_INTF);

    QList<QString> list = m_screenInfoList.keys();
    for (int i = 0; i < list.count(); i++) {
        QString monitorName = list.at(i);

        QDBusReply<QString> getReply = wm.call( "GetWorkspaceBackgroundForMonitor", d, monitorName);
        if (!getReply.value().isEmpty()) {
            QString backgroundUri = getReply.value();
            MultiViewBackgroundManager::instance()->updateBackgroundList(backgroundUri);
        }

        for (int i = d; i < effects->numberOfDesktops(); i++) {
            QString backgrounduri;
            QDBusReply<QString> getReply = wm.call( "GetWorkspaceBackgroundForMonitor", i + 1, monitorName);
            if (!getReply.value().isEmpty()) {
                backgrounduri = getReply.value();
            } else {
                backgrounduri = QLatin1String(fallback_background_name);
            }
            wm.call( "SetWorkspaceBackgroundForMonitor", i, monitorName, backgrounduri);
        }
    }
}

void MultitaskViewEffect::removeWinAndRelayout(EffectWindow *w)
{
    QMutexLocker locker(&m_mutex);
    if (w->desktop() == -1) {
        for (int wi = 0; wi < m_workspaceWinMgr.size(); wi++) {
            m_workspaceWinMgr[wi]->removeWin(w->screen(), w);
        }
        for (int i = 0; i < m_motionManagers.size(); i++) {
            m_motionManagers[i]->removeWin(w->screen(), w);
            WindowMotionManager *wmm;
            if (m_motionManagers[i]->getMotion(i + 1, w->screen(), wmm)) {
                calculateWindowTransformations(wmm->orderManagedWindows(), *wmm, i + 1, w->screen(), true);
            }
        }

    } else {
        int desktop = w->desktop();
        MultiViewWinManager *wmobj = getWinManagerObject(desktop - 1);
        MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(desktop - 1);
        if (wmobj && wkmobj) {
            wkmobj->removeWin(w->screen(), w);
            wmobj->removeWin(w->screen(), w);
            WindowMotionManager *wmm;
            if (wmobj->getMotion(desktop, w->screen(), wmm)) {
                calculateWindowTransformations(wmm->orderManagedWindows(), *wmm, desktop, w->screen(), true);
            }
        }
    }

    for (int i = 0; i < effects->numberOfDesktops(); i++) {
        MultiViewWinManager *wmobj = getWinManagerObject(i);
        EffectWindowList list;
        if (wmobj) {
            list = wmobj->getDesktopWinList();
        }
        if (list.size() != 0) {
            return;
        }
    }

    if (isRelevantWithPresentWindows(w))
        setActive(false);
}

void MultitaskViewEffect::moveWindowChangeDesktop(EffectWindow *w, int todesktop, int toscreen, bool isSwitch)
{
    if (w->isOnAllDesktops()) {
        return;
    }

    if (effects->numberOfDesktops() < todesktop) {
        return;
    }

    int fromdesktop = w->desktop();
    QMutexLocker locker(&m_mutex);
    int screen = w->screen();
    effects->windowToScreen(w, toscreen);
    QVector<uint> desks{(uint)todesktop};
    effects->windowToDesktops(w, desks);

    EffectWindowList mainWinList;
    mainWinList = w->mainWindows();
    foreach (EffectWindow * mainWin, mainWinList) {
        effects->windowToScreen(mainWin, toscreen);
        QVector<uint> desks{(uint)todesktop};
        effects->windowToDesktops(mainWin, desks);
    }

    EffectWindowList childWinList;
    childWinList = effectsEx->getChildWinList(w);

    if (isSwitch) {
        effects->activateWindow(w);
    }

    MultiViewWinManager *wkmobj = getWorkspaceWinManagerObject(fromdesktop - 1);
    MultiViewWinManager *wkmobj1 = getWorkspaceWinManagerObject(todesktop - 1);
    if (wkmobj && wkmobj1) {
        foreach (EffectWindow * mainWin, mainWinList) {
            wkmobj->removeWin(screen, mainWin);
            wkmobj1->manageWin(toscreen, mainWin);
        }

        foreach (EffectWindow *childWin, childWinList) {
            wkmobj->removeWin(screen, childWin);
            wkmobj1->manageWin(toscreen, childWin);
        }

        wkmobj->removeWin(screen, w);
        wkmobj1->manageWin(toscreen, w);
        workspaceWinRelayout(fromdesktop, screen);
        workspaceWinRelayout(todesktop, toscreen);
    }

    MultiViewWinManager *wmobj = getWinManagerObject(fromdesktop - 1);
    if (wmobj) {
        foreach (EffectWindow * mainWin, mainWinList) {
            wmobj->removeWin(screen, mainWin);
        }

        foreach (EffectWindow *childWin, childWinList) {
            wmobj->removeWin(screen, childWin);
        }

        wmobj->removeWin(screen, w);
        WindowMotionManager *wmm;
        if (wmobj->getMotion(fromdesktop, screen, wmm)) {
            calculateWindowTransformations(wmm->orderManagedWindows(), *wmm, fromdesktop, screen, true);
        }
    }

    wmobj = getWinManagerObject(todesktop - 1);
    if (wmobj) {
        foreach (EffectWindow * mainWin, mainWinList) {
            wmobj->manageWin(toscreen, mainWin);
        }
        foreach (EffectWindow *childWin, childWinList) {
            wmobj->manageWin(toscreen, childWin);
        }

        wmobj->manageWin(toscreen, w);
        WindowMotionManager *wmmto;
        if (wmobj->getMotion(todesktop, toscreen, wmmto)) {
            calculateWindowTransformations(wmmto->orderManagedWindows(), *wmmto, todesktop, toscreen, true);
        }
    }
    m_curDesktopIndex = effects->currentDesktop();
}

bool MultitaskViewEffect::closeWindow(EffectWindow *w)
{
    if (effectsEx->getChildWinList(w).size() == 0) {
        w->closeWindow();
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
    effects->setCurrentDesktop(desktop);
    m_hoverWin = nullptr;
    m_flyingWinList.clear();

    //sliding
    m_bgSlidingStatus = true;
    m_bgSlidingTimeLine.reset();
    m_lastDesktopIndex = m_curDesktopIndex;
    m_curDesktopIndex = desktop;
}

void MultitaskViewEffect::showWorkspacePreview(int screen, QPoint pos, bool isClear)
{
    if (!isClear) {
        if (!m_previewFrame) {
            m_previewFrame = effects->effectFrame(EffectFrameNone, false);
            m_previewFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        }
        if (m_previewFrame->icon().isNull()) {
            QPixmap wpPix;
            QSize size(m_scale[screen].workspaceWidth, m_scale[screen].workspaceHeight);
            MultiViewBackgroundManager::instance()->getPreviewBackground(size, wpPix, screen);

            QRect maxRect = effects->clientArea(MaximizeArea, screen, 1);
            QPoint pos1(maxRect.x() + maxRect.width() - 20 - m_scale[screen].workspaceWidth / 2, maxRect.y() + m_scale[screen].spacingHeight);

            m_previewFramePosX = pos1.x();

            QIcon icon(wpPix);
            m_previewFrame->setIcon(icon);
            m_previewFrame->setIconSize(size);
            m_previewFrame->setPosition(pos1);
            m_previewFrame->setGeometry(QRect(QPoint(pos1.x(), pos1.y()), size));
        }

        QRect rect = m_previewFrame->geometry();
        if (effects->waylandDisplay()) {
            if (screen != effectsEx->getCurrentPaintingScreen()) {
                if (m_scale[screen].fullArea.x() > 0)
                    return;
            }
        } else {
            glEnable(GL_SCISSOR_TEST);
            glScissor(rect.x(), m_scale[m_screen].fullArea.height() - rect.y() - rect.height(), rect.width() / 2 + 20, rect.height());
        }

        ShaderBinder bind(m_previewShader);
        m_previewShader->setUniform("iResolution", QVector3D(rect.width(), rect.height(), 0));
        m_previewShader->setUniform("iOffset", QVector2D(calculateOffSet(rect, m_scale[m_screen].fullArea, m_maxHeight)));
        m_previewFrame->setShader(m_previewShader);
        m_previewFrame->render(infiniteRegion(), 1, 0.8);

        if (nullptr == effects->waylandDisplay()) {
            glDisable(GL_SCISSOR_TEST);
        }
    } else {
        if (m_previewFrame && !m_previewFrame->icon().isNull()) {
            m_previewFrame->setIcon(QIcon());
        }
    }
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
    MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
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
    MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
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
    MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
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
    MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    if (wmobj) {
        list = wmobj->getDesktopWinList();
    }

    EffectWindowList sameList;
    int index = 0;
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
    MultiViewWinManager *wmobj = getWinManagerObject(effects->currentDesktop() - 1);
    if (wmobj) {
        list = wmobj->getDesktopWinList();
    }

    EffectWindowList sameList;
    int index = 0;
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

MultiViewWorkspace *MultitaskViewEffect::getWorkspaceObject(int index, int secindex)
{
    MultiViewWorkspace *target = nullptr;
    if (m_workspaceBackgrounds.contains(index) && secindex >= 0) {
        if (m_workspaceBackgrounds[index].size() > secindex) {
            target = m_workspaceBackgrounds[index][secindex];
        }
    }
    return target;
}

MultiViewWinManager *MultitaskViewEffect::getWinManagerObject(int index)
{
    MultiViewWinManager *target = nullptr;
    if (m_motionManagers.size() > index && index >= 0) {
        target = m_motionManagers[index];
    }
    return target;
}

MultiViewWinManager *MultitaskViewEffect::getWorkspaceWinManagerObject(int index)
{
    MultiViewWinManager *target = nullptr;
    if (m_workspaceWinMgr.size() > index && index >= 0) {
        target = m_workspaceWinMgr[index];
    }
    return target;
}

bool MultitaskViewEffect::checkHandlerWorkspace(QPoint pos, int screen, int &desktop)
{
    bool isChecked = false;
    QList<MultiViewWorkspace *> list = m_workspaceBackgrounds[screen];
    for (int d = 0; d < list.size(); d++) {
        if (list[d]->getRect().contains(pos)) {
            desktop = d + 1;
            isChecked = true;
            break;
        }
    }

    return isChecked;
}

bool MultitaskViewEffect::touchDown(quint32 id, const QPointF &pos, quint32 time)
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

bool MultitaskViewEffect::touchMotion(quint32 id, const QPointF &pos, quint32 time)
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

bool MultitaskViewEffect::touchUp(quint32 id, quint32 time)
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

void MultitaskViewEffect::motionRepeat()
{
    m_longPressTouch = true;
}

} // namespace KWin
