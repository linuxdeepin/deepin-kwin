/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "workspacemodel.h"

namespace KWin
{

QString WorkspaceModel::idAt(int row) const
{
    return (row >= 0 && row < m_workspaces.size()) ? m_workspaces.at(row).id : QString();
}

int WorkspaceModel::numAt(int row) const
{
    return (row >= 0 && row < m_workspaces.size()) ? m_workspaces.at(row).num : 0;
}

int WorkspaceModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

void WorkspaceModel::addWorkspace(const QString &id, int num)
{
    if (indexOfId(id) >= 0) {
        return;
    }
    Workspace ws;
    ws.id = id;
    ws.num = num;
    m_workspaces.append(ws);
}

void WorkspaceModel::removeWorkspace(const QString &id)
{
    const int row = indexOfId(id);
    if (row < 0) {
        return;
    }
    if (m_currentId == id && m_workspaces.size() > 1) {
        const int fallback = qMax(0, row - 1);
        m_currentId = m_workspaces.at(fallback).id;
        m_currentNumber = m_workspaces.at(fallback).num;
    } else if (m_currentId == id) {
        m_currentId.clear();
        m_currentNumber = 1;
    }
    m_workspaces.removeAt(row);
}

void WorkspaceModel::moveWorkspace(int from, int to)
{
    if (from < 0 || from >= m_workspaces.size() || to < 0 || to >= m_workspaces.size() || from == to) {
        return;
    }
    m_workspaces.move(from, to);
}

void WorkspaceModel::setCurrent(const QString &id)
{
    const int row = indexOfId(id);
    if (row < 0 || m_currentId == id) {
        return;
    }
    m_currentId = id;
    m_currentNumber = m_workspaces.at(row).num;
}

void WorkspaceModel::setNum(const QString &id, int num)
{
    const int row = indexOfId(id);
    if (row < 0 || m_workspaces.at(row).num == num) {
        return;
    }
    m_workspaces[row].num = num;
}

void WorkspaceModel::setWallpaper(const QString &id, const QString &uri)
{
    const int row = indexOfId(id);
    if (row < 0) {
        return;
    }
    m_workspaces[row].wallpaper = uri;
}

}
