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

#ifndef KWIN_MULTITASKMODEL_H
#define KWIN_MULTITASKMODEL_H

#include <QAbstractListModel>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QHash>
#include <QPixmap>
#include <QRect>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace KWin
{

class MultitaskViewModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ isReady NOTIFY ready)
    Q_PROPERTY(QStringList wallpaperList READ wallpaperList NOTIFY ready)
public:
    enum DataRoles {
        ScreenNameRole    = Qt::UserRole + 1,
        ScreenRectRole    = Qt::UserRole + 2,
        WorkspaceIndexRole = Qt::UserRole + 3,
        WorkspaceBgRole   = Qt::UserRole + 4,
        WorkspaceWpRole   = Qt::UserRole + 5,
        WallpaperListRole  = Qt::UserRole + 6,
        WorkspaceRectRole  = Qt::UserRole + 7,
        WorkspaceUriRole   = Qt::UserRole + 8,
    };
    Q_ENUM(DataRoles)

    explicit MultitaskViewModel(QObject *parent = nullptr);
    ~MultitaskViewModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isReady() const { return m_ready; }

    QStringList wallpaperList() const { return m_wallpaperList; }
    QModelIndex findIndex(const QString &screenName, int workspaceIndex) const;
    QString getWorkspaceBackgroundUri(int workspaceIndex, const QString &screenName) const;
    void setWorkspaceBackgroundForMonitor(int index, const QString &strMonitorName, const QString &uri);
    void setItemThumbRect(const QString &screenName, int workspaceIndex, const QRect &thumbRect);
    static QString toRealPath(const QString &path);
    static QPixmap cutBackgroundPix(const QSize &size, const QString &file);
    QString getRandBackground() const;
    void updateBackgroundList(const QString &file);

    Q_INVOKABLE void addDesktop(const QString &bgFile = QString());
    Q_INVOKABLE void removeDesktop(int desktop);
    Q_INVOKABLE void switchDesktop(int to, int from);

public Q_SLOTS:
    void refresh();
    void onBackgroundChanged();

Q_SIGNALS:
    void ready();
    void blurReady(int workspaceIndex, const QString &screenName, QPixmap bgPixmap);

private:
    struct Item {
        QString screenName;
        QRect screenRect;
        int workspaceIndex = 0;
        QPixmap bgPixmap;
        QPixmap wpPixmap;
        QString uri;
        QRect thumbRect;
    };

    void connectDBusSignals();
    void fetchWorkspaceBackgrounds();
    void fetchWallpaperList();
    void handleBgUriReply(int workspaceIndex, const QString &screenName, const QString &uri, int generation);
    void handlePixmapReady(int workspaceIndex, const QString &screenName,
                           const QRect &screenRect, const QRect &thumbRect,
                           const QPixmap &bgPix, const QPixmap &wpPix,
                           const QString &uri);
    void updateBlurPixmap(int workspaceIndex, const QString &screenName,
                          const QPixmap &bgPixmap);
    void handleWallpaperListReady(const QStringList &list);
    void checkAllLoaded();

    static QPixmap getCachePix(const QSize &size, QPair<QSize, QPixmap> &pair);

    QDBusInterface *m_appearance = nullptr;
    QDBusInterface *m_imageBlur = nullptr;
    QVector<Item> m_items;
    QStringList m_wallpaperList;
    bool m_wallpaperListReady = false;
    bool m_signalsConnected = false;
    bool m_ready = false;
    int m_pendingLoads = 0;
    int m_refreshGeneration = 0;

    QHash<QString, QPair<QSize, QPixmap>> m_wpCachedPixmaps;
    QHash<QString, QPair<QSize, QPixmap>> m_bgCachedPixmaps;
    QHash<QPair<QString, int>, int> m_itemIndex;
    QSet<QString> m_backgroundAllList;
    QSet<QString> m_currentBackgroundList;

    Q_DISABLE_COPY(MultitaskViewModel)
};

} // namespace KWin

#endif
