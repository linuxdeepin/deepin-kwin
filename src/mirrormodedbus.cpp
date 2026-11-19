/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mirrormodedbus.h"

#include "window.h"
#include "workspace.h"

#include <QDBusConnection>

namespace KWin
{

MirrorModeDBus::MirrorModeDBus(Workspace *workspace)
    : QObject(workspace)
    , m_workspace(workspace)
{
    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/MirrorMode"), this,
                                                     QDBusConnection::ExportAllSlots
                                                         | QDBusConnection::ExportAllProperties
                                                         | QDBusConnection::ExportAllSignals)) {
        qWarning() << "Failed to export the mirror mode object at /MirrorMode";
    }
    connect(workspace, &Workspace::windowAdded, this, &MirrorModeDBus::windowsChanged);
    connect(workspace, &Workspace::windowRemoved, this, &MirrorModeDBus::windowsChanged);
    connect(workspace, &Workspace::mirrorModeListChanged, this, &MirrorModeDBus::windowsChanged);
}

QStringList MirrorModeDBus::primaryWindows() const
{
    QStringList ids;
    const Output *primary = m_workspace->mirrorModePrimaryOutput();
    if (!primary) {
        primary = m_workspace->preferredOutput();
    }
    if (!primary) {
        return ids;
    }
    const auto windows = m_workspace->stackingOrder();
    for (const Window *window : windows) {
        if (Workspace::isMirrorModeListable(window) && window->output() == primary) {
            ids.append(QString::number(window->window()));
        }
    }
    return ids;
}

QStringList MirrorModeDBus::mirrorWindows() const
{
    QStringList ids;
    const auto windows = m_workspace->stackingOrder();
    for (const Window *window : windows) {
        if (window->isMirrorModeListed()) {
            ids.append(QString::number(window->window()));
        }
    }
    return ids;
}

Window *MirrorModeDBus::findWindow(qulonglong winId) const
{
    const auto windows = m_workspace->stackingOrder();
    for (Window *window : windows) {
        if (window->window() == winId) {
            return window;
        }
    }
    return nullptr;
}

bool MirrorModeDBus::approveWindow(qulonglong winId)
{
    Window *window = findWindow(winId);
    if (!window) {
        return false;
    }
    window->setMirrorModeListed(true);
    return window->isMirrorModeListed();
}

bool MirrorModeDBus::removeWindow(qulonglong winId)
{
    Window *window = findWindow(winId);
    if (!window) {
        return false;
    }
    window->setMirrorModeListed(false);
    return !window->isMirrorModeListed();
}

} // namespace KWin
