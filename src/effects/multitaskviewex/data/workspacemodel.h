/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>

namespace KWin
{

class WorkspaceModel
{
public:
    /**
     * @brief Returns the number of workspaces in the model.
     * @return Workspace count.
     */
    int count() const
    {
        return m_workspaces.size();
    }

    /**
     * @brief Returns the id of the workspace at a row.
     * @param row Row index.
     * @return The workspace id, or an empty string when the row is out of range.
     */
    QString idAt(int row) const;

    /**
     * @brief Returns the desktop number of the workspace at a row.
     * @param row Row index.
     * @return The desktop number, or 0 when the row is out of range.
     */
    int numAt(int row) const;

    /**
     * @brief Appends a workspace, ignoring ids that already exist.
     * @param id Workspace id.
     * @param num Desktop number.
     */
    void addWorkspace(const QString &id, int num);

    /**
     * @brief Removes a workspace by id, moving the current marker when needed.
     * @param id Workspace id to remove.
     */
    void removeWorkspace(const QString &id);

    /**
     * @brief Moves a workspace between two row positions.
     * @param from Source row index.
     * @param to Destination row index.
     */
    void moveWorkspace(int from, int to);

    /**
     * @brief Marks the workspace with the given id as current.
     * @param id Workspace id to make current.
     */
    void setCurrent(const QString &id);

    /**
     * @brief Sets the wallpaper URI of a workspace.
     * @param id Workspace id.
     * @param uri Wallpaper URI.
     */
    void setWallpaper(const QString &id, const QString &uri);

    /**
     * @brief Sets the desktop number of a workspace.
     * @param id Workspace id.
     * @param num New desktop number.
     */
    void setNum(const QString &id, int num);

    /**
     * @brief Returns the desktop number of the current workspace.
     * @return The current desktop number.
     */
    int currentNumber() const
    {
        return m_currentNumber;
    }

private:
    struct Workspace
    {
        QString id;
        int num = 0;
        QString wallpaper;
    };

    int indexOfId(const QString &id) const;

    QVector<Workspace> m_workspaces;
    QString m_currentId;
    int m_currentNumber = 1;
};

}
