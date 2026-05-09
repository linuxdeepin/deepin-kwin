/*
 * Copyright (C) 2024 ~ 2024 Deepin Technology Co., Ltd.
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

#include "multitaskmodel.h"

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>
#include <QGuiApplication>
#include <QScreen>
#include <QtConcurrent>

#include "effects.h"
#include "workspace.h"

#define MAX_DESKTOP_COUNT 6

namespace KWin
{

#ifdef BUILD_ON_V25
#define DBUS_APPEARANCE_SERVICE "org.deepin.dde.Appearance1"
#define DBUS_APPEARANCE_OBJ     "/org/deepin/dde/Appearance1"
#define DBUS_APPEARANCE_INTF    "org.deepin.dde.Appearance1"
#define DBUS_IMAGEEFFECT_SERVICE "org.deepin.dde.ImageEffect1"
#define DBUS_BLUR_OBJ           "/org/deepin/dde/ImageBlur1"
#define DBUS_BLUR_INTF          "org.deepin.dde.ImageBlur1"
#else
#define DBUS_APPEARANCE_SERVICE  "com.deepin.daemon.Appearance"
#define DBUS_APPEARANCE_OBJ      "/com/deepin/daemon/Appearance"
#define DBUS_APPEARANCE_INTF     "com.deepin.daemon.Appearance"
#define DBUS_IMAGEEFFECT_SERVICE "com.deepin.daemon.ImageEffect"
#define DBUS_BLUR_OBJ            "/com/deepin/daemon/ImageBlur"
#define DBUS_BLUR_INTF           "com.deepin.daemon.ImageBlur"
#endif

static const char fallback_background_name[] = "file:///usr/share/wallpapers/deepin/desktop.jpg";

MultitaskViewModel::MultitaskViewModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

MultitaskViewModel::~MultitaskViewModel()
{
    if (m_appearance)
        delete m_appearance;
    if (m_imageBlur)
        delete m_imageBlur;
}

int MultitaskViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant MultitaskViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};

    const Item &item = m_items.at(index.row());

    switch (role) {
    case ScreenNameRole:
        return item.screenName;
    case ScreenRectRole:
        return item.screenRect;
    case WorkspaceIndexRole:
        return item.workspaceIndex;
    case WorkspaceBgRole:
        return item.bgPixmap;
    case WorkspaceWpRole:
        return item.wpPixmap;
    case WallpaperListRole:
        return QVariant::fromValue(m_wallpaperList);
    case WorkspaceRectRole:
        return item.thumbRect;
    case WorkspaceUriRole:
        return item.uri;
    default:
        return {};
    }
}

QHash<int, QByteArray> MultitaskViewModel::roleNames() const
{
    return {
        { ScreenNameRole, "screenName" },
        { ScreenRectRole, "screenRect" },
        { WorkspaceIndexRole, "workspaceIndex" },
        { WorkspaceBgRole, "workspaceBg" },
        { WorkspaceWpRole, "workspaceWp" },
        { WallpaperListRole, "wallpaperList" },
        { WorkspaceRectRole, "workspaceRect" },
        { WorkspaceUriRole, "workspaceUri" },
    };
}

QString MultitaskViewModel::toRealPath(const QString &path)
{
    QString res = QUrl::fromPercentEncoding(path.toUtf8());
    if (res.startsWith("file:///"))
        res.remove("file://");

    QFileInfo fi(res);
    if (fi.isSymLink())
        res = fi.symLinkTarget();
    return res;
}

QPixmap MultitaskViewModel::cutBackgroundPix(const QSize &size, const QString &file)
{
    QImageReader imageReader;
    imageReader.setFileName(file);
    imageReader.setAutoTransform(true);
    auto imageSize = imageReader.size();
    auto targetScaleSize = imageSize.scaled(size, Qt::KeepAspectRatioByExpanding);

    imageReader.setScaledSize(targetScaleSize);
    QPixmap pixmap = QPixmap::fromImageReader(&imageReader);

    if (pixmap.width() > size.width() || pixmap.height() > size.height()) {
        pixmap = pixmap.copy(QRect(static_cast<int>((pixmap.width() - size.width()) / 2.0),
                                    static_cast<int>((pixmap.height() - size.height()) / 2.0),
                                    size.width(), size.height()));
    }
    return pixmap;
}

QPixmap MultitaskViewModel::getCachePix(const QSize &size, QPair<QSize, QPixmap> &pair)
{
    if (pair.first == size) {
        return pair.second;
    } else if (pair.first.width() > size.width() && pair.first.height() > size.height()) {
        pair.first = size;
        pair.second = pair.second.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        pair.second = pair.second.copy(QRect(static_cast<int>((pair.second.width() - size.width()) / 2.0),
                                       static_cast<int>((pair.second.height() - size.height()) / 2.0),
                                       size.width(), size.height()));
        return pair.second;
    }
    return QPixmap();
}

void MultitaskViewModel::refresh()
{
    if (!m_appearance) {
        m_appearance = new QDBusInterface(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ,
                                           DBUS_APPEARANCE_INTF, QDBusConnection::sessionBus(), this);
    }
    if (!m_imageBlur) {
        m_imageBlur = new QDBusInterface(DBUS_IMAGEEFFECT_SERVICE, DBUS_BLUR_OBJ,
                                          DBUS_BLUR_INTF, QDBusConnection::systemBus(), this);
        m_imageBlur->setTimeout(5000);
    }

    beginResetModel();
    m_items.clear();
    m_itemIndex.clear();
    endResetModel();
    m_pendingLoads = 0;
    m_ready = false;

    connectDBusSignals();
    fetchWorkspaceBackgrounds();
    fetchWallpaperList();
}

void MultitaskViewModel::addDesktop(const QString &bgFile)
{
    int count = effects->numberOfDesktops();
    if (count >= MAX_DESKTOP_COUNT)
        return;

    for (Output *output : workspace()->outputs()) {
        EffectScreen *screen = effectsEx->findScreen(output);
        if (!screen)
            continue;
        QString uri = bgFile.isEmpty() ? getRandBackground() : bgFile;
        setWorkspaceBackgroundForMonitor(count + 1, screen->name(), uri);
    }

    effects->setNumberOfDesktops(count + 1);
    // KWin emits numberDesktopsChanged → connected to refresh() in later task
}

void MultitaskViewModel::removeDesktop(int desktop)
{
    int count = effects->numberOfDesktops();
    if (desktop <= 0 || desktop > count || count <= 1)
        return;

    // Recycle removed desktop's wallpaper back to available pool
    for (Output *output : workspace()->outputs()) {
        EffectScreen *screen = effectsEx->findScreen(output);
        if (!screen)
            continue;
        QString uri = getWorkspaceBackgroundUri(desktop, screen->name());
        updateBackgroundList(uri);
    }

    // Shuffle wallpaper URIs down
    for (int i = desktop + 1; i <= count; i++) {
        for (Output *output : workspace()->outputs()) {
            EffectScreen *screen = effectsEx->findScreen(output);
            if (!screen)
                continue;
            QString uri = getWorkspaceBackgroundUri(i, screen->name());
            setWorkspaceBackgroundForMonitor(i - 1, screen->name(), uri);
        }
    }

    effects->setNumberOfDesktops(count - 1);
}

void MultitaskViewModel::switchDesktop(int to, int from)
{
    effects->setCurrentDesktop(from);
    effects->setCurrentDesktop(to);
}

void MultitaskViewModel::connectDBusSignals()
{
    if (m_signalsConnected)
        return;
    m_signalsConnected = true;

    QDBusConnection::sessionBus().connect(
        DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ,
        "org.freedesktop.DBus.Properties", "PropertiesChanged",
        this, SLOT(onBackgroundChanged()));

    QDBusConnection::sessionBus().connect(
        DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ,
        DBUS_APPEARANCE_INTF, "Changed",
        this, SLOT(onBackgroundChanged()));

    connect(effects, &EffectsHandler::numberDesktopsChanged, this, [this](uint old) {
        refresh();
    });
}

void MultitaskViewModel::fetchWorkspaceBackgrounds()
{
    m_ready = false;
    int gen = ++m_refreshGeneration;
    int count = effects->numberOfDesktops();
    for (Output *output : workspace()->outputs()) {
        EffectScreen *screen = effectsEx->findScreen(output);
        QString screenName = screen->name();
        for (int i = 1; i <= count; i++) {
            QDBusPendingCall call = m_appearance->asyncCall(
                "GetWorkspaceBackgroundForMonitor", i, screenName);
            auto *watcher = new QDBusPendingCallWatcher(call, this);

            connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, i, screenName, gen](QDBusPendingCallWatcher *w) {
                    if (m_refreshGeneration != gen) {
                        w->deleteLater();
                        return;
                    }
                    QDBusPendingReply<QString> reply = *w;
                    QString uri;
                    if (reply.isError()) {
                        qWarning() << "MultitaskViewModel: GetWorkspaceBackgroundForMonitor failed for"
                                   << screenName << "workspace" << i << ":" << reply.error().message();
                        uri = QLatin1String(fallback_background_name);
                    } else if (reply.value().isEmpty()) {
                        uri = QLatin1String(fallback_background_name);
                    } else {
                        uri = reply.value();
                    }
                    handleBgUriReply(i, screenName, uri, gen);
                    w->deleteLater();
                });
            m_pendingLoads++;
        }
    }
}

void MultitaskViewModel::fetchWallpaperList()
{
    QDBusPendingCall call = m_appearance->asyncCall("List", "background");
    auto *watcher = new QDBusPendingCallWatcher(call, this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<QString> reply = *w;
            if (reply.isError()) {
                qWarning() << "MultitaskViewModel: wallpaper List call failed:"
                           << reply.error().message();
                handleWallpaperListReady({});
                w->deleteLater();
                return;
            }
            QStringList list;
            QJsonDocument json = QJsonDocument::fromJson(reply.value().toUtf8());
            QJsonArray arr = json.array();
            if (!arr.isEmpty()) {
                for (auto p = arr.constBegin(); p != arr.constEnd(); ++p) {
                    auto o = p->toObject();
                    if (!o.value("Id").isUndefined() && !o.value("Deletable").toBool()) {
                        list << o.value("Id").toString();
                    }
                }
            }
            handleWallpaperListReady(list);
            w->deleteLater();
        });
}

void MultitaskViewModel::handleBgUriReply(int workspaceIndex, const QString &screenName, const QString &uri, int generation)
{
    QRect screenRect;
    for (Output *output : workspace()->outputs()) {
        EffectScreen *screen = effectsEx->findScreen(output);
        if (screen->name() == screenName) {
            screenRect = effects->clientArea(FullScreenArea, screen, effects->currentDesktop()).toRect();
            break;
        }
    }

    int w = screenRect.width() * (240.0 / 1920.0);
    int h = screenRect.height() * (240.0 / 1920.0);
    QSize workspaceSize(w, h);
    QSize bgSize = screenRect.size();

    QString realUri = toRealPath(uri);
    QString wpCacheKey = realUri + QString::number(workspaceIndex) + screenName;
    QString bgCacheKey = realUri + QString::number(workspaceIndex) + screenName;

    QPixmap wpPix;
    if (m_wpCachedPixmaps.contains(wpCacheKey)) {
        auto &p = m_wpCachedPixmaps[wpCacheKey];
        wpPix = getCachePix(workspaceSize, p);
    }

    // Try bg cache first — if hit, use cached blur directly, skip async
    QPixmap cachedBg;
    if (m_bgCachedPixmaps.contains(bgCacheKey)) {
        auto &p = m_bgCachedPixmaps[bgCacheKey];
        cachedBg = getCachePix(bgSize, p);
    }

    if (!cachedBg.isNull() && !wpPix.isNull()) {
        handlePixmapReady(workspaceIndex, screenName, screenRect, QRect(), cachedBg, wpPix, uri);
        return;
    }

    auto startBlurLoad = [this, workspaceIndex, screenName, screenRect, realUri, bgCacheKey, bgSize, generation]() {
        auto *blurCall = new QDBusPendingCallWatcher(
            m_imageBlur->asyncCall("Get", realUri), this);
        connect(blurCall, &QDBusPendingCallWatcher::finished, this,
            [this, workspaceIndex, screenName, bgCacheKey, bgSize, generation](QDBusPendingCallWatcher *w) {
                if (m_refreshGeneration != generation) { w->deleteLater(); return; }
                QDBusPendingReply<QString> reply = *w;
                QString bgUri = reply.value().isEmpty()
                    ? QLatin1String(fallback_background_name)
                    : toRealPath(reply.value());
                auto *fw = new QFutureWatcher<QPixmap>(this);
                connect(fw, &QFutureWatcher<QPixmap>::finished, this,
                    [this, fw, workspaceIndex, screenName, bgCacheKey, bgSize, generation]() {
                        if (m_refreshGeneration != generation) { fw->deleteLater(); return; }
                        QPixmap bg = fw->result();
                        if (!bg.isNull())
                            m_bgCachedPixmaps[bgCacheKey] = qMakePair(bgSize, bg);
                        updateBlurPixmap(workspaceIndex, screenName, bg);
                        fw->deleteLater();
                    });
                fw->setFuture(QtConcurrent::run(cutBackgroundPix, bgSize, bgUri));
                w->deleteLater();
            });
    };

    if (!wpPix.isNull()) {
        // wp cached: register with wp-as-bg, async blur
        if (!cachedBg.isNull())
            handlePixmapReady(workspaceIndex, screenName, screenRect, QRect(), cachedBg, wpPix, uri);
        else {
            handlePixmapReady(workspaceIndex, screenName, screenRect, QRect(), wpPix, wpPix, uri);
            startBlurLoad();
        }
    } else {
        auto *wpFw = new QFutureWatcher<QPixmap>(this);
        connect(wpFw, &QFutureWatcher<QPixmap>::finished, this,
            [this, wpFw, workspaceIndex, screenName, screenRect, realUri, wpCacheKey, bgCacheKey,
             bgSize, uri, generation, startBlurLoad]() {
                if (m_refreshGeneration != generation) { wpFw->deleteLater(); return; }
                QPixmap wp = wpFw->result();
                QSize wsSize(bgSize.width() * (240.0 / 1920.0),
                             bgSize.height() * (240.0 / 1920.0));
                if (!wp.isNull())
                    m_wpCachedPixmaps[wpCacheKey] = qMakePair(wsSize, wp);
                wpFw->deleteLater();

                QPixmap cachedBg;
                if (m_bgCachedPixmaps.contains(bgCacheKey)) {
                    auto &p = m_bgCachedPixmaps[bgCacheKey];
                    cachedBg = getCachePix(bgSize, p);
                }
                if (!cachedBg.isNull())
                    handlePixmapReady(workspaceIndex, screenName, screenRect, QRect(), cachedBg, wp, uri);
                else {
                    handlePixmapReady(workspaceIndex, screenName, screenRect, QRect(), wp, wp, uri);
                    startBlurLoad();
                }
            });
        wpFw->setFuture(QtConcurrent::run(cutBackgroundPix, workspaceSize, realUri));
    }
}

void MultitaskViewModel::handlePixmapReady(int workspaceIndex, const QString &screenName,
                                            const QRect &screenRect, const QRect &thumbRect,
                                            const QPixmap &bgPix, const QPixmap &wpPix,
                                            const QString &uri)
{
    // Find existing item or add new one
    int foundIdx = -1;
    for (int i = 0; i < m_items.size(); i++) {
        if (m_items[i].screenName == screenName && m_items[i].workspaceIndex == workspaceIndex) {
            foundIdx = i;
            break;
        }
    }

    Item item;
    item.screenName = screenName;
    item.screenRect = screenRect;
    item.workspaceIndex = workspaceIndex;
    item.bgPixmap = bgPix;
    item.wpPixmap = wpPix;
    item.uri = uri;
    item.thumbRect = thumbRect;

    if (foundIdx >= 0) {
        m_items[foundIdx] = item;
        Q_EMIT dataChanged(index(foundIdx), index(foundIdx));
    } else {
        beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
        m_items.append(item);
        endInsertRows();
        m_itemIndex.insert(qMakePair(screenName, workspaceIndex), m_items.size() - 1);
    }

    m_pendingLoads--;
    checkAllLoaded();
}

void MultitaskViewModel::updateBlurPixmap(int workspaceIndex, const QString &screenName,
                                           const QPixmap &bgPixmap)
{
    int foundIdx = -1;
    for (int i = 0; i < m_items.size(); i++) {
        if (m_items[i].screenName == screenName && m_items[i].workspaceIndex == workspaceIndex) {
            foundIdx = i;
            break;
        }
    }
    if (foundIdx >= 0 && !bgPixmap.isNull()) {
        m_items[foundIdx].bgPixmap = bgPixmap;
        Q_EMIT dataChanged(index(foundIdx), index(foundIdx));
        Q_EMIT blurReady(workspaceIndex, screenName, bgPixmap);
    }
}

void MultitaskViewModel::handleWallpaperListReady(const QStringList &list)
{
    m_wallpaperList = list;
    m_backgroundAllList.clear();
    for (const auto &s : list)
        m_backgroundAllList.insert(s);
    for (const auto &s : m_currentBackgroundList) {
        if (m_backgroundAllList.contains(s))
            m_backgroundAllList.remove(s);
    }
    m_wallpaperListReady = true;
    checkAllLoaded();
}

void MultitaskViewModel::checkAllLoaded()
{
    if (m_ready)
        return;
    if (m_pendingLoads <= 0 && m_wallpaperListReady) {
        m_ready = true;
        Q_EMIT ready();
    }
}

void MultitaskViewModel::onBackgroundChanged()
{
    // Re-fetch all workspace backgrounds and wallpaper list on change
    fetchWorkspaceBackgrounds();
    fetchWallpaperList();
}

QModelIndex MultitaskViewModel::findIndex(const QString &screenName, int workspaceIndex) const
{
    auto it = m_itemIndex.constFind(qMakePair(screenName, workspaceIndex));
    if (it != m_itemIndex.constEnd())
        return index(*it);
    return {};
}

QString MultitaskViewModel::getWorkspaceBackgroundUri(int workspaceIndex, const QString &screenName) const
{
    // Query from cache first
    for (const auto &item : m_items) {
        if (item.screenName == screenName && item.workspaceIndex == workspaceIndex)
            return item.uri;
    }
    return QLatin1String(fallback_background_name);
}

void MultitaskViewModel::setItemThumbRect(const QString &screenName, int workspaceIndex, const QRect &thumbRect)
{
    QModelIndex idx = findIndex(screenName, workspaceIndex);
    if (!idx.isValid())
        return;
    m_items[idx.row()].thumbRect = thumbRect;
    Q_EMIT dataChanged(idx, idx, {WorkspaceRectRole});
}

void MultitaskViewModel::setWorkspaceBackgroundForMonitor(int index, const QString &strMonitorName, const QString &uri)
{
    if (!m_appearance) {
        m_appearance = new QDBusInterface(DBUS_APPEARANCE_SERVICE, DBUS_APPEARANCE_OBJ,
                                           DBUS_APPEARANCE_INTF, QDBusConnection::sessionBus(), this);
    }
    m_appearance->asyncCall("SetWorkspaceBackgroundForMonitor", index, strMonitorName, uri);
}

QString MultitaskViewModel::getRandBackground() const
{
    if (m_backgroundAllList.isEmpty())
        return QLatin1String(fallback_background_name);

    int maxAttempts = 3;
    QSet<QString> allSet = m_backgroundAllList;
    while (maxAttempts > 0) {
        int idx = QRandomGenerator::global()->bounded(allSet.size());
        auto it = allSet.begin();
        for (int i = 0; i < idx; i++) ++it;
        QString file = *it;
        if (!m_currentBackgroundList.contains(file))
            return file;
        maxAttempts--;
    }
    return QLatin1String(fallback_background_name);
}

void MultitaskViewModel::updateBackgroundList(const QString &file)
{
    if (!m_backgroundAllList.contains(file))
        m_backgroundAllList.insert(file);
    if (m_currentBackgroundList.contains(file))
        m_currentBackgroundList.remove(file);
}

} // namespace KWin
