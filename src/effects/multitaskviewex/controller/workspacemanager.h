/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

namespace KWin
{

class WallpaperManager;
class WorkspaceModel;

/**
 * @brief Keeps the workspace model in sync with the virtual desktop manager.
 *
 * Every desktop mutation the view can trigger (add, remove, reorder, switch) is
 * applied here and reported through signals, so the effect only has to refresh
 * its screen views.
 */
class WorkspaceManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the manager.
     * @param wallpapers Wallpaper manager used to migrate wallpaper bindings.
     * @param parent Parent object.
     */
    explicit WorkspaceManager(WallpaperManager *wallpapers, QObject *parent = nullptr);

    /**
     * @brief Destroys the manager.
     */
    ~WorkspaceManager() override;

    /**
     * @brief Returns the workspace model.
     * @return The workspace model.
     */
    WorkspaceModel *model() const
    {
        return m_model.get();
    }

    /**
     * @brief Tests whether a desktop mutation is currently applied by this manager.
     * @return True while the manager is mutating the desktop manager itself.
     */
    bool ownsDesktopMutation() const
    {
        return m_inOwnMutation;
    }

    /**
     * @brief Reloads the model from the virtual desktop manager.
     */
    void refresh();

    /**
     * @brief Switches to the desktop with the given id.
     * @param desktopId Desktop id to activate.
     */
    void switchToDesktop(const QString &desktopId);

    /**
     * @brief Creates a workspace on the desktop manager.
     * @param previewUri Wallpaper selected in the add-preview, or an empty string.
     * @param maxWorkspaces Maximum number of workspaces allowed.
     * @return True when a desktop was created.
     */
    bool addDesktop(const QString &previewUri, int maxWorkspaces);

    /**
     * @brief Removes a workspace.
     * @param desktopNumber Desktop number to remove.
     */
    void removeDesktop(int desktopNumber);

    /**
     * @brief Reorders the desktops to match the workspace bar order.
     * @param finalOrder Desktop numbers in the requested display order.
     */
    void applyOrder(const QVector<int> &finalOrder);

    /**
     * @brief Reacts to a desktop change that was not caused by this manager.
     */
    void handleVirtualDesktopsChanged();

private:
    QString currentFallbackUri() const;

    std::unique_ptr<WorkspaceModel> m_model;
    WallpaperManager *m_wallpapers = nullptr;
    bool m_inOwnMutation = false;
};

}
