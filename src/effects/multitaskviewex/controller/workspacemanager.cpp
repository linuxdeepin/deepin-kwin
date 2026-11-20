/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "workspacemanager.h"
#include "data/workspacemodel.h"
#include "helpers.h"
#include "model/bardragmodel.h"
#include "model/barpredicates.h"
#include "model/wallpaperdata.h"
#include "wallpapermanager.h"

#include "effects.h"
#include "virtualdesktops.h"
#include "window.h"
#include "workspace.h"

#include <QDBusConnection>
#include <QDebug>
#include <QLoggingCategory>
#include <QVector>

namespace KWin
{

WorkspaceManager::WorkspaceManager(WallpaperManager *wallpapers, QObject *parent)
    : QObject(parent)
    , m_model(std::make_unique<WorkspaceModel>())
    , m_wallpapers(wallpapers)
{
}

WorkspaceManager::~WorkspaceManager() = default;

QString WorkspaceManager::currentFallbackUri() const
{
    if (!m_wallpapers) {
        return QString();
    }
    auto *vds = VirtualDesktopManager::self();
    const int num = vds && vds->currentDesktop() ? int(vds->currentDesktop()->x11DesktopNumber()) : 1;
    return m_wallpapers->clearPath(num);
}

void WorkspaceManager::refresh()
{
    auto *vds = VirtualDesktopManager::self();
    if (!vds || !m_model) {
        return;
    }
    const auto desktops = vds->desktops();
    for (int i = m_model->count() - 1; i >= 0; --i) {
        const QString id = m_model->idAt(i);
        bool alive = false;
        for (const VirtualDesktop *d : desktops) {
            if (d->id() == id) {
                alive = true;
                break;
            }
        }
        if (!alive) {
            m_model->removeWorkspace(id);
        }
    }
    for (const VirtualDesktop *d : desktops) {
        const QString id = d->id();
        const int num = int(d->x11DesktopNumber());
        const QString wp = m_wallpapers ? m_wallpapers->clearUri(num) : QString();
        bool present = false;
        for (int k = 0; k < m_model->count(); ++k) {
            if (m_model->idAt(k) == id) {
                present = true;
                break;
            }
        }
        if (!present) {
            m_model->addWorkspace(id, num);
        } else {
            m_model->setNum(id, num);
        }
        m_model->setWallpaper(id, wp);
    }
    for (int target = 0; target < desktops.size(); ++target) {
        const QString id = desktops.at(target)->id();
        int cur = -1;
        for (int k = 0; k < m_model->count(); ++k) {
            if (m_model->idAt(k) == id) {
                cur = k;
                break;
            }
        }
        if (cur >= 0 && cur != target) {
            m_model->moveWorkspace(cur, target);
            m_model->setNum(id, target + 1);
        }
    }
    if (VirtualDesktop *cur = vds->currentDesktop()) {
        m_model->setCurrent(cur->id());
    }
}

void WorkspaceManager::switchToDesktop(const QString &desktopId)
{
    auto *vds = VirtualDesktopManager::self();
    if (!vds || desktopId.isEmpty()) {
        return;
    }
    if (VirtualDesktop *d = vds->desktopForId(desktopId)) {
        vds->setCurrent(d);
        refresh();
    }
}

bool WorkspaceManager::addDesktop(const QString &previewUri, int maxWorkspaces)
{
    auto *vds = VirtualDesktopManager::self();
    if (!vds || !m_model) {
        return false;
    }
    if (int(vds->count()) >= maxWorkspaces) {
        return false;
    }
    const QString uri = pickNewWorkspaceWallpaper(previewUri, currentFallbackUri());
    {
        m_inOwnMutation = true;
        VirtualDesktop *nd = vds->createVirtualDesktop(addDesktopPosition(vds->count()));
        m_inOwnMutation = false;
        if (nd) {
            const int newNum = int(nd->x11DesktopNumber());
            if (!uri.isEmpty() && m_wallpapers) {
                for (EffectScreen *s : effects->screens()) {
                    m_wallpapers->setWorkspaceBackground(newNum, s->name(), uri);
                }
            }
            refresh();
            m_model->setWallpaper(nd->id(), uri);
            qCInfo(KWIN_MTVEX) << "created workspace" << newNum << "wallpaper=" << uri
                               << "source=" << (previewUri.isEmpty() ? "current workspace fallback" : "preview selection")
                               << "screens written=" << int(effects->screens().size());
            return true;
        }
    }
    return false;
}

int WorkspaceManager::removeDesktop(int desktopNumber)
{
    auto *vds = VirtualDesktopManager::self();
    if (!vds || vds->count() == 1 || desktopNumber <= 0 || desktopNumber > int(vds->count())) {
        return 0;
    }
    VirtualDesktop *removed = vds->desktopForX11Id(desktopNumber);
    if (!removed) {
        return 0;
    }
    const int currentNumber = int(vds->current());
    const bool removingCurrent = (currentNumber == desktopNumber);
    VirtualDesktop *activateTarget = removingCurrent
        ? vds->desktopForX11Id(desktopNumber > 1 ? desktopNumber - 1 : 2)
        : vds->currentDesktop();
    const int migrateTo = (desktopNumber > 1) ? desktopNumber - 1 : 2;
    for (EffectWindow *w : effects->stackingOrder()) {
        if (!w || w->isDesktop() || w->isDock() || w->isNotification() || w->isOutline()) {
            continue;
        }
        if (w->isOnAllDesktops() || !w->isOnDesktop(desktopNumber)) {
            continue;
        }
        if (auto *impl = static_cast<EffectWindowImpl *>(w)) {
            if (Window *win = impl->window()) {
                Workspace::self()->sendWindowToDesktop(win, migrateTo, true);
            }
        }
    }
    if (m_wallpapers) {
        m_wallpapers->shiftOnRemove(desktopNumber, m_model ? m_model->count() : 0);
    }
    m_inOwnMutation = true;
    vds->removeVirtualDesktop(removed);
    if (activateTarget) {
        vds->setCurrent(activateTarget);
    }
    m_inOwnMutation = false;
    refresh();
    return int(vds->current());
}

void WorkspaceManager::applyOrder(const QVector<int> &finalOrder)
{
    auto *vds = VirtualDesktopManager::self();
    if (!vds || !m_model) {
        return;
    }

    QHash<QString, QVector<QString>> wallpapers;
    if (m_wallpapers) {
        for (EffectScreen *screen : effects->screens()) {
            const QString monitor = screen->name();
            if (wallpapers.contains(monitor)) {
                continue;
            }
            QVector<QString> uris;
            uris.reserve(m_model->count());
            for (int r = 0; r < m_model->count(); ++r) {
                uris.append(m_wallpapers->clearUri(r + 1, screen));
            }
            wallpapers.insert(monitor, uris);
        }
    }

    QVector<QString> desiredIds;
    desiredIds.reserve(finalOrder.size());
    for (int num : finalOrder) {
        QString id;
        for (int r = 0; r < m_model->count(); ++r) {
            if (m_model->numAt(r) == num) {
                id = m_model->idAt(r);
                break;
            }
        }
        desiredIds.append(id);
    }

    bool moved = false;
    for (int target = 0; target < desiredIds.size(); ++target) {
        if (desiredIds[target].isEmpty()) {
            continue;
        }
        int cur = -1;
        for (int r = 0; r < m_model->count(); ++r) {
            if (m_model->idAt(r) == desiredIds[target]) {
                cur = r;
                break;
            }
        }
        if (cur >= 0 && cur != target) {
            m_inOwnMutation = true;
            vds->moveDesktop(uint(cur), uint(target));
            m_inOwnMutation = false;
            for (auto it = wallpapers.begin(); it != wallpapers.end(); ++it) {
                QVector<QString> &uris = it.value();
                if (cur < uris.size() && target < uris.size()) {
                    uris.insert(target, uris.takeAt(cur));
                }
            }
            moved = true;
            m_model->moveWorkspace(cur, target);
            for (int i = 0; i < m_model->count(); ++i) {
                m_model->setNum(m_model->idAt(i), i + 1);
            }
        }
    }

    if (moved && m_wallpapers) {
        for (auto it = wallpapers.constBegin(); it != wallpapers.constEnd(); ++it) {
            const QString &monitor = it.key();
            const QVector<QString> &uris = it.value();
            for (int pos = 0; pos < uris.size(); ++pos) {
                if (uris.at(pos).isEmpty()) {
                    continue;
                }
                m_wallpapers->setWorkspaceBackground(pos + 1, monitor, uris.at(pos));
            }
        }
    }

    refresh();
}

void WorkspaceManager::handleVirtualDesktopsChanged()
{
    if (m_inOwnMutation || !m_model) {
        return;
    }
    refresh();
}

}
