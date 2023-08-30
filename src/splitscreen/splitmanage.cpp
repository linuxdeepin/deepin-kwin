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
void SplitManage::add(Window *window)
{
    if (window->isUnmanaged() || window->isAppletPopup() || window->isSpecialWindow() || window->isInternal()) {
        if (window->caption().contains("splitbar") && !m_splitBarWindows.contains(window->caption()))
            m_splitBarWindows.insert(window->caption(), window);
        return;
    }
    connect(window, &Window::quickTileModeChanged, this, &SplitManage::handleQuickTile);
    WindowData data = dataForWindow(window);
    m_data[window] = data;
}

void SplitManage::remove(Window *window)
{
    inhibit();
    if (m_data.contains(window)) {
        disconnect(window, &Window::quickTileModeChanged, this, &SplitManage::handleQuickTile);
        SplitGroup *splitgroup = getGroup(m_data[window].desktop, m_data[window].screenName);
        if (splitgroup)
            splitgroup->deleteSplitWindow(window);
        m_data.remove(window);
    }
    uninhibit();
}

bool SplitManage::isSplitWindow(Window *window)
{
    return (window->quickTileMode() == int(QuickTileFlag::Left)) || (window->quickTileMode() == int(QuickTileFlag::Right));
}

void SplitManage::handleQuickTile()
{
    inhibit();
    Window *window = qobject_cast<Window *>(QObject::sender());
    Q_ASSERT(window);
    SplitGroup *splitgroup = getGroup(m_data[window].desktop, m_data[window].screenName);
    if (splitgroup)
        splitgroup->deleteSplitWindow(window);

    int desktop = VirtualDesktopManager::self()->current();
    QString screenName = window->output()->name() + "splitbar";
    if (isSplitWindow(window)) {
        createGroup(desktop, screenName)->storeSplitWindow(window);
        createSplitBar(screenName);
        {
            m_data[window].desktop = desktop;
            m_data[window].screenName = screenName;
        }
    }
    uninhibit();
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
    if (m_inhibitCount != 0)
        return;
    int desktop = VirtualDesktopManager::self()->current();
    for (QString key : m_splitBarManage.keys()) {
        SplitGroup *splitgroup = getGroup(desktop, key);
        if (splitgroup) {
            QVector<Window *> vect;
            splitgroup->getSplitWindow(vect);
            if (vect.size() > 1) {
                hash[key] = vect;
                Q_EMIT signalSplitWindow(key, vect[0]);
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

void SplitManage::updateSplitWindowGeometry(QString name, QPointF pos, bool isfinish)
{
    if (isfinish) {
        Workspace::self()->outline()->hide();
        return;
    }
    int desktop = VirtualDesktopManager::self()->current();
    const Output *output = Workspace::self()->outputAt(pos);
    const VirtualDesktop *virtualDesktop = VirtualDesktopManager::self()->currentDesktop();
    QRectF rect = Workspace::self()->clientArea(MaximizeArea, output, virtualDesktop);

    SplitGroup *splitgroup = getGroup(desktop, name);
    if (splitgroup) {
        QVector<Window *> vect;
        splitgroup->getSplitWindow(vect);
        for (auto &w : vect) {
            QRectF geo = w->clientGeometry();
            if (w->quickTileMode() == int(QuickTileFlag::Left)) {
                w->moveResize(QRect(geo.x(), geo.y(), pos.x() - geo.x(), geo.height()));
            } else {
                w->moveResize(QRect(pos.x(), geo.y(), rect.right() - pos.x(), geo.height()));
            }
        }
    }
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