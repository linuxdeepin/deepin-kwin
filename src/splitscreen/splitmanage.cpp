/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "splitmanage.h"
#include "window.h"
#include "virtualdesktops.h"
#include "../core/output.h"
#include "workspace.h"
#include "outline.h"
#include "wayland_server.h"
#include <QThread>

namespace KWin {

SplitGroup::SplitGroup(int d)
    : m_desktop(d)
{
}

void SplitGroup::setDesktop(int d)
{
    m_desktop = d;
}

void SplitGroup::storeSplitWindow(Window *w)
{
    m_splitGroup.push_back(w);
}

void SplitGroup::deleteSplitWindow(Window *w)
{
    if (m_splitGroup.contains(w))
        m_splitGroup.removeOne(w);
}

void SplitGroup::getSplitWindow(QVector<Window *> &vec)
{
    vec = m_splitGroup;
}

/*********************************************/
// split manage
/*********************************************/
SplitManage::SplitManage()
    : m_mutex(QMutex::Recursive)
{
}

void SplitManage::add(Window *window)
{
    if (window->isUnmanaged() || window->isAppletPopup() || window->isSpecialWindow() || window->isInternal()) {
        if (window->caption().contains("splitbar") && !m_splitBarWindows.contains(window->caption())) {
            m_pause = false;
            m_splitBarWindows.insert(window->caption(), window);
        }
        return;
    }
    connect(window, &Window::quickTileModeChanged, this, &SplitManage::handleQuickTile);
    connect(window, &Window::screenChanged, this, &SplitManage::windowScreenChange);
    connect(window, &Window::desktopChanged, this, &SplitManage::windowDesktopChange);
    connect(window, &Window::minimizedChanged, this, &SplitManage::updateSplitWindowsGroup);
    connect(window, &Window::fullScreenChanged, this, &SplitManage::updateSplitWindowsGroup);
    connect(window, &Window::frameGeometryChanged, this, &SplitManage::windowFrameSizeChange);
    WindowData data = dataForWindow(window);
    m_data[window] = data;
}

void SplitManage::remove(Window *window)
{
    inhibit();
    if (m_data.contains(window)) {
        disconnect(window, &Window::quickTileModeChanged, this, &SplitManage::handleQuickTile);
        disconnect(window, &Window::screenChanged, this, &SplitManage::windowScreenChange);
        disconnect(window, &Window::desktopChanged, this, &SplitManage::windowDesktopChange);
        disconnect(window, &Window::minimizedChanged, this, &SplitManage::updateSplitWindowsGroup);
        disconnect(window, &Window::fullScreenChanged, this, &SplitManage::updateSplitWindowsGroup);
        disconnect(window, &Window::frameGeometryChanged, this, &SplitManage::windowFrameSizeChange);
        removeQuickTile(window);
        m_data.remove(window);
    }
    uninhibit();
}

void SplitManage::removeInternal(Window *window)
{
    QMutexLocker locker(&m_mutex);
    inhibit();
    if (window->isUnmanaged() || window->isAppletPopup() || window->isSpecialWindow() || window->isInternal()) {
        if (window->caption().contains("splitbar") && m_splitBarWindows.contains(window->caption())) {
            m_pause = true;
            m_splitBarWindows.remove(window->caption());
            if (m_splitBarManage.contains(window->caption())) {
                m_splitBarManage.remove(window->caption());
            }
        }
    }
    uninhibit();
}

bool SplitManage::isSplitWindow(Window *window)
{
    return (window->quickTileMode() == int(QuickTileFlag::Left)) || (window->quickTileMode() == int(QuickTileFlag::Right));
}

void SplitManage::handleQuickTile()
{
    QMutexLocker locker(&m_mutex);
    inhibit();
    Window *window = qobject_cast<Window *>(QObject::sender());
    Q_ASSERT(window);
    removeQuickTile(window);

    int desktop = VirtualDesktopManager::self()->current();
    if (window->desktop() == -1)
        desktop = -1;
    QString screenName = window->output()->name() + "splitbar";
    if (isSplitWindow(window)) {
        addQuickTile(desktop, screenName, window);
        createSplitBar(screenName);
        {
            m_data[window].desktop = desktop;
            m_data[window].screenName = screenName;
        }
    }
    m_lastWin = window;
    uninhibit();
}

void SplitManage::windowFrameSizeChange()
{
    Window *window = qobject_cast<Window *>(QObject::sender());
    if (waylandServer() && m_lastWin == window) {
        workspace()->updateStackingOrder();
        m_lastWin = nullptr;
    }
}

void SplitManage::windowScreenChange()
{
    QMutexLocker locker(&m_mutex);
    Window *window = qobject_cast<Window *>(QObject::sender());
    if (window->output()->name().isEmpty())
        return;
    updateStorage(window);
}

void SplitManage::windowDesktopChange()
{
    QMutexLocker locker(&m_mutex);
    Window *window = qobject_cast<Window *>(QObject::sender());
    updateStorage(window);
}

void SplitManage::updateStorage(Window *window)
{
    inhibit();
    removeQuickTile(window);
    int desktop = window->desktop();
    QString screenName = window->output()->name() + "splitbar";
    if (isSplitWindow(window)) {
        addQuickTile(desktop, screenName, window);
        createSplitBar(screenName);
    }
    {
        m_data[window].desktop = desktop;
        m_data[window].screenName = screenName;
    }
    uninhibit();
}

void SplitManage::updateSplitWindowsGroup()
{
    QMutexLocker locker(&m_mutex);
    inhibit();
    Window *window = qobject_cast<Window *>(QObject::sender());
    if (isSplitWindow(window)) {
        if (window->isMinimized() || window->isFullScreen()) {
            removeQuickTile(window);
        } else {
            addQuickTile(m_data[window].desktop, m_data[window].screenName, window);
        }
    }
    uninhibit();
}

void SplitManage::addQuickTile(int desktop, QString screenName, Window *window)
{
    if (desktop == -1) {
        for (int i = 1; i <= VirtualDesktopManager::self()->count(); ++i) {
            createGroup(i, screenName)->storeSplitWindow(window);
        }
    } else {
        createGroup(desktop, screenName)->storeSplitWindow(window);
    }
}

void SplitManage::removeQuickTile(Window *window)
{
    if (m_data[window].desktop == -1) {
        for (int i = 1; i <= VirtualDesktopManager::self()->count(); ++i) {
            SplitGroup *splitgroup = getGroup(i, m_data[window].screenName);
            if (splitgroup)
                splitgroup->deleteSplitWindow(window);
        }
    } else {
        SplitGroup *splitgroup = getGroup(m_data[window].desktop, m_data[window].screenName);
        if (splitgroup)
            splitgroup->deleteSplitWindow(window);
    }
}

SplitManage::WindowData SplitManage::dataForWindow(Window *window) const
{
    return WindowData {
        .geometry = window->moveResizeGeometry(),
        .quickTile = window->quickTileMode(),
        .screenName = window->output()->name() + "splitbar",
        .desktop = window->desktop(),
    };
}

SplitGroup *SplitManage::getGroup(int &desktop, QString &name)
{
    SplitGroup *splitgroup = nullptr;
    if (m_splitGroupManage.contains(name)) {
        for (const auto &sg : m_splitGroupManage[name]) {
            if (sg->desktop() == desktop) {
                splitgroup = sg;
                break;
            }
        }
    }
    return splitgroup;
}

SplitGroup *SplitManage::createGroup(int &desktop, QString &name)
{
    SplitGroup *splitgroup = nullptr;
    if (!m_splitGroupManage.contains(name)) {
        SplitGroup *sg = new SplitGroup(desktop);
        m_splitGroupManage.insert(name, QSet<SplitGroup *>() << sg);
        splitgroup = sg;
    } else {
        for (const auto &sg : m_splitGroupManage[name]) {
            if (sg->desktop() == desktop) {
                splitgroup = sg;
                break;
            }
        }
        if (!splitgroup) {
            SplitGroup *sg = new SplitGroup(desktop);
            m_splitGroupManage[name].insert(sg);
            splitgroup = sg;
        }
    }
    return splitgroup;
}

void SplitManage::createSplitBar(QString &name)
{
    if (!m_splitBarManage.contains(name)) {
        SplitBar *splitbar = new SplitBar(name);
        m_splitBarManage.insert(name, splitbar);
        connect(splitbar, &SplitBar::splitbarPosChanged, this, &SplitManage::updateSplitWindowGeometry);
    }
}

void SplitManage::getSplitWindows(QHash<QString, QVector<Window *>> &hash)
{
    if (m_inhibitCount != 0 || m_pause)
        return;
    int desktop = VirtualDesktopManager::self()->current();
    for (QString key : m_splitBarManage.keys()) {
        SplitGroup *splitgroup = getGroup(desktop, key);
        if (splitgroup) {
            QVector<Window *> vect;
            splitgroup->getSplitWindow(vect);
            if (vect.size() > 1) {
                hash[key] = vect;
                Q_EMIT signalSplitWindow(key, vect.contains(workspace()->activeWindow()) ? workspace()->activeWindow() : vect[0]);
            } else {
                Q_EMIT signalSplitWindow(key, nullptr);
            }
        }
    }
}

void SplitManage::getSplitBarWindow(QHash<QString, Window *> &hash)
{
    if (m_inhibitCount != 0)
        return;
    hash = m_splitBarWindows;
}

void SplitManage::updateSplitWindowGeometry(QString name, QPointF pos, Window *w, bool isfinish)
{
    if (isfinish || w == nullptr) {
        return;
    }

    if (w != workspace()->activeWindow()) {
        QList<Window *> list = workspace()->stackingOrder();
        std::reverse(list.begin(), list.end());
        for (auto it = list.constBegin(); it != list.constEnd(); ++it) {
            if ((*it) && !(*it)->isClient())
                continue;
            if ((*it)->isSplitWindow() && (*it)->desktop() == w->desktop() && (*it)->screen() == w->screen()) {
                Workspace::self()->raiseWindow((*it));
                break;
            }
        }
    }

    w->resizeSplitWindow(pos);
}

void SplitManage::inhibit()
{
    m_inhibitCount++;
}

void SplitManage::uninhibit()
{
    m_inhibitCount--;
}

}