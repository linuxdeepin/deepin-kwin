/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mirrormodedbus.h"

#include "window.h"
#include "workspace.h"
#include "mirrormode_logging.h"

#include <QDBusConnection>

namespace KWin
{

static QString windowIdString(qulonglong winId)
{
    return QStringLiteral("0x%1 (%2)").arg(QString::number(winId, 16), QString::number(winId));
}

MirrorModeDBus::MirrorModeDBus(Workspace *workspace)
    : QObject(workspace)
    , m_workspace(workspace)
{
    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/MirrorMode"), this,
                                                     QDBusConnection::ExportAllSlots
                                                         | QDBusConnection::ExportAllProperties
                                                         | QDBusConnection::ExportAllSignals)) {
        qCWarning(KWIN_MIRRORMODE) << "Failed to export the object at /MirrorMode";
    }
    connect(workspace, &Workspace::windowAdded, this, &MirrorModeDBus::windowsChanged);
    connect(workspace, &Workspace::windowRemoved, this, &MirrorModeDBus::windowsChanged);
    connect(workspace, &Workspace::mirrorModeListChanged, this, &MirrorModeDBus::windowsChanged);
    // primaryWindows() is anchored on the primary screen, which entering or leaving the
    // mode replaces, and on the output each window sits on, which a hotplug redoes. The
    // properties have to be re-read then too, not only when the stacking order changes.
    connect(workspace, &Workspace::mirrorModeChanged, this, &MirrorModeDBus::windowsChanged);
    connect(workspace, &Workspace::outputsChanged, this, &MirrorModeDBus::windowsChanged);
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
    qCDebug(KWIN_MIRRORMODE) << "Approve requested for window" << windowIdString(winId);
    Window *window = findWindow(winId);
    if (!window) {
        qCDebug(KWIN_MIRRORMODE) << "Approve: no window for" << windowIdString(winId);
        return false;
    }
    qCDebug(KWIN_MIRRORMODE) << "Approve: matched" << window->resourceName() << window->caption();
    window->setMirrorModeListed(true);
    if (!window->isMirrorModeListed()) {
        qCDebug(KWIN_MIRRORMODE) << "Approve: window refused to be listed" << window->resourceName();
        return false;
    }
    qCDebug(KWIN_MIRRORMODE) << "Approve: listed, activating" << window->resourceName();
    // The request comes from the shell that drives the mode, so the approved window is
    // the one the user is meant to work in: it has to take the focus on the primary
    // output, not just be mirrored onto the others. The shell is not the window itself,
    // so the focus stealing prevention cannot vouch for it: activate with force.
    m_workspace->activateWindow(window, true);
    // The focus change is asynchronous: what it settles on is reported by the active
    // window change that follows, so only the request can be logged here.
    qCDebug(KWIN_MIRRORMODE) << "Approve: activation requested for" << window->resourceName();
    return true;
}

bool MirrorModeDBus::removeWindow(qulonglong winId)
{
    qCDebug(KWIN_MIRRORMODE) << "Remove requested for window" << windowIdString(winId);
    Window *window = findWindow(winId);
    if (!window) {
        return false;
    }
    window->setMirrorModeListed(false);
    qCDebug(KWIN_MIRRORMODE) << "Remove:" << window->resourceName() << "listed"
                       << window->isMirrorModeListed();
    return !window->isMirrorModeListed();
}

} // namespace KWin
