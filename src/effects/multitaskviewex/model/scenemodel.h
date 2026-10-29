/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "animation/animation.h"
#include "backgrounditem.h"
#include "baritem.h"
#include "data/workspacebarmetrics.h"
#include "model/bardragmodel.h"
#include "model/motionoptions.h"
#include "screenlayout.h"
#include "windowdragsession.h"
#include "windowitem.h"

#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace KWin
{

class EffectScreen;
class EffectWindow;

class SceneModel
{
public:
    /**
     * @brief Returns the screen origin used for layout.
     * @return The layout origin.
     */
    QPointF origin() const
    {
        return m_origin;
    }

    /**
     * @brief Returns the workspace bar metrics.
     * @return The bar metrics.
     */
    const WorkspaceBarMetrics &metrics() const
    {
        return m_metrics;
    }

    /**
     * @brief Returns the screen layout.
     * @return The layout computed by syncScreen().
     */
    const ScreenLayout &layout() const
    {
        return m_layout;
    }

    /**
     * @brief Returns the absolute dock areas of the screen.
     * @return The dock areas.
     */
    const QVector<QRectF> &dockAreas() const
    {
        return m_dockAreas;
    }

    /**
     * @brief Returns the base cell rectangles before animation.
     * @return The base cells.
     */
    const QVector<QRectF> &baseCells() const
    {
        return m_cellBase;
    }

    /**
     * @brief Returns the currently hovered cell index.
     * @return The hovered cell index, or -1 when none.
     */
    int hoverCell() const
    {
        return m_hoverCell;
    }

    /**
     * @brief Recomputes the layout for a screen.
     * @param origin Screen origin.
     * @param screenSize Screen size.
     * @param fullRect Full screen rectangle.
     * @param clientRect Usable client rectangle.
     * @param absDockAreas Absolute dock areas.
     */
    void syncScreen(const QPointF &origin, const QSizeF &screenSize,
                    const QRectF &fullRect, const QRectF &clientRect,
                    const QVector<QRectF> &absDockAreas);

    /**
     * @brief Returns the OS scale factor applied to the layout.
     * @return The scale factor.
     */
    qreal osScale() const
    {
        return m_osScale;
    }

    /**
     * @brief Sets the OS scale factor applied to the layout.
     * @param s Scale factor.
     */
    void setOsScale(qreal s)
    {
        m_osScale = s;
    }

    /**
     * @brief Returns the animation timing options.
     * @return The current motion options.
     */
    const MotionOptions &motion() const
    {
        return m_motion;
    }

    /**
     * @brief Sets the animation timing options.
     * @param options Options to apply.
     */
    void setMotionOptions(const MotionOptions &options)
    {
        m_motion = options;
    }

    /**
     * @brief Returns the maximum number of workspaces allowed.
     * @return The workspace limit.
     */
    int maxWorkspaces() const
    {
        return m_maxWorkspaces;
    }

    /**
     * @brief Sets the maximum number of workspaces allowed.
     * @param maxWorkspaces Workspace limit.
     */
    void setMaxWorkspaces(int maxWorkspaces)
    {
        m_maxWorkspaces = maxWorkspaces;
    }

    /**
     * @brief Sets the hovered cell index.
     * @param cell Cell index, or -1 to clear the hover.
     */
    void setHoverCell(int cell);

    /**
     * @brief Returns the mutable list of bar items.
     * @return The bar items.
     */
    QVector<BarItem> &barItems()
    {
        return m_barItems;
    }

    /**
     * @brief Returns the list of bar items.
     * @return The bar items.
     */
    const QVector<BarItem> &barItems() const
    {
        return m_barItems;
    }

    /**
     * @brief Rebuilds the bar items from workspace order and wallpapers.
     * @param order Workspace keys paired with their desktop numbers, in display order.
     * @param wallpapers Wallpaper URI keyed by workspace key.
     */
    void syncBarItems(const QVector<QPair<QString, int>> &order, const QHash<QString, QString> &wallpapers);

    /**
     * @brief Clears all bar items and the drop-raise marker.
     */
    void resetItems();

    /**
     * @brief Sets the add-preview state of the bar.
     * @param active Whether the preview is shown.
     * @param wallpaperUri Wallpaper URI shown by the preview.
     */
    void setBarPreview(bool active, const QString &wallpaperUri);

    /**
     * @brief Returns the add-preview item.
     * @return The preview item, or nullptr when inactive.
     */
    const BarItem *barPreviewItem() const
    {
        return m_barPreviewActive ? &m_barPreview : nullptr;
    }

    /**
     * @brief Tests whether the add-preview is active.
     * @return True when the preview is shown.
     */
    bool barPreviewActive() const
    {
        return m_barPreviewActive;
    }

    /**
     * @brief Records the key of the item raised by the last bar drop.
     * @param key Workspace key raised after a drop.
     */
    void noteBarDropFinished(const QString &key)
    {
        m_barDropRaiseKey = key;
    }

    /**
     * @brief Returns the key of the item raised by the last bar drop.
     * @return The raised workspace key.
     */
    QString barDropRaiseKey() const
    {
        return m_barDropRaiseKey;
    }

    /**
     * @brief Resets the bar drag and preview state.
     */
    void resetBar();

    /**
     * @brief Rebuilds the window items from a window list.
     * @param windows Windows to show, in order.
     * @param sources Source geometry of each window.
     */
    void syncWindows(const QVector<EffectWindow *> &windows, const QVector<QRectF> &sources);

    /**
     * @brief Clears all window items.
     */
    void clearWindows();

    /**
     * @brief Returns the window item for a window.
     * @param w Window to look up.
     * @return The matching item, or nullptr when absent.
     */
    WindowItem *windowItemFor(EffectWindow *w);

    /**
     * @brief Returns the mutable list of window items.
     * @return The window items.
     */
    QVector<WindowItem> &windowItems()
    {
        return m_windowItems;
    }

    /**
     * @brief Returns the list of window items.
     * @return The window items.
     */
    const QVector<WindowItem> &windowItems() const
    {
        return m_windowItems;
    }

    /**
     * @brief Drops the item of a window, marking it as retiring.
     * @param w Window to drop.
     */
    void dropWindow(EffectWindow *w);

    /**
     * @brief Takes the windows whose retire animation has finished.
     * @return Windows to release, and clears the finished list.
     */
    QVector<EffectWindow *> takeFinishedRetiringWindows();

    /**
     * @brief Marks a retiring window as finished.
     * @param w Window whose retire animation completed.
     */
    void finishRetiringWindow(EffectWindow *w);

    /**
     * @brief Rebuilds the cell rectangles used to place windows.
     * @param cells Cell rectangles.
     * @param rowHeight Height of a grid row.
     */
    void syncCellItems(const QVector<QRectF> &cells, qreal rowHeight);

    /**
     * @brief Returns the height of a grid row.
     * @return The row height.
     */
    qreal rowHeight() const
    {
        return m_rowHeight;
    }

    /**
     * @brief Returns the base cell rectangles before animation.
     * @return The base cells.
     */
    const QVector<QRectF> &cellBase() const
    {
        return m_cellBase;
    }

    /**
     * @brief Starts a window drag anchored to a cell.
     * @param cell Source cell rectangle.
     */
    void beginDrag(const QRectF &cell);

    /**
     * @brief Updates the drag pointer position.
     * @param pos Current pointer position.
     */
    void dragTo(const QPointF &pos);

    /**
     * @brief Ends the window drag.
     */
    void endDrag();

    /**
     * @brief Tests whether a window drag is active.
     * @return True while a drag is in progress.
     */
    bool dragActive() const
    {
        return m_dragSession.active();
    }

    /**
     * @brief Returns the source cell of the window drag.
     * @return The drag source cell.
     */
    QRectF dragCell() const
    {
        return m_dragSession.cell();
    }

    /**
     * @brief Returns the last drag pointer position.
     * @return The last drag position.
     */
    QPointF dragPos() const
    {
        return m_lastDragPos;
    }

    /**
     * @brief Sets or clears the drag placeholder cell.
     * @param index Cell index acting as placeholder.
     * @param active Whether the placeholder is active.
     */
    void setDragPlaceholder(int index, bool active);

    /**
     * @brief Returns the drag placeholder cell index.
     * @return The placeholder index, or -1 when none.
     */
    int dragPlaceholder() const
    {
        return m_dragPlaceholder;
    }

    /**
     * @brief Returns the mutable background item.
     * @return The background item.
     */
    BackgroundItem &backgroundItem()
    {
        return m_backgroundItem;
    }

    /**
     * @brief Returns the background item.
     * @return The background item.
     */
    const BackgroundItem &backgroundItem() const
    {
        return m_backgroundItem;
    }

    /**
     * @brief Sets whether the ghost is animating back to its slot.
     * @param returning Whether the ghost is returning.
     */
    void setGhostReturning(bool returning)
    {
        m_ghostReturning = returning;
    }

    /**
     * @brief Tests whether the ghost is animating back to its slot.
     * @return True while the ghost is returning.
     */
    bool ghostReturning() const
    {
        return m_ghostReturning;
    }

    /**
     * @brief Returns the screen this model is bound to.
     * @return The bound screen, or nullptr when unbound.
     */
    EffectScreen *screen() const
    {
        return m_screen;
    }

    /**
     * @brief Binds the model to a screen.
     * @param s Screen to bind, or nullptr to unbind.
     */
    void setScreen(EffectScreen *s)
    {
        m_screen = s;
    }

    /**
     * @brief Returns the mutable list of tracked windows.
     * @return The tracked windows.
     */
    QVector<EffectWindow *> &windows()
    {
        return m_windows;
    }

    /**
     * @brief Returns the list of tracked windows.
     * @return The tracked windows.
     */
    const QVector<EffectWindow *> &windows() const
    {
        return m_windows;
    }

    /**
     * @brief Returns the window currently being dragged.
     * @return The dragged window, or nullptr when none.
     */
    EffectWindow *draggedWindow() const
    {
        return m_draggedWindow;
    }

    /**
     * @brief Sets the window currently being dragged.
     * @param w Dragged window, or nullptr to clear.
     */
    void setDraggedWindow(EffectWindow *w)
    {
        m_draggedWindow = w;
    }

    /**
     * @brief Returns the mutable bar drag model.
     * @return The bar drag model.
     */
    WorkspaceBarDragModel &barDragModel()
    {
        return m_barDragModel;
    }

    /**
     * @brief Returns the bar drag model.
     * @return The bar drag model.
     */
    const WorkspaceBarDragModel &barDragModel() const
    {
        return m_barDragModel;
    }

private:
    BarItem *itemForKey(const QString &key);

    QPointF m_origin;
    WorkspaceBarMetrics m_metrics;
    qreal m_osScale = 1.0;
    MotionOptions m_motion;
    int m_maxWorkspaces = kDefaultMaxWorkspaces;
    ScreenLayout m_layout;
    QVector<QRectF> m_dockAreas;
    int m_hoverCell = -1;

    QVector<BarItem> m_barItems;
    QString m_barDropRaiseKey;
    BarItem m_barPreview;
    bool m_barPreviewActive = false;

    QVector<QRectF> m_cellBase;
    qreal m_rowHeight = 0;
    QVector<WindowItem> m_windowItems;
    QVector<EffectWindow *> m_finishedRetiring;
    int m_dragPlaceholder = -1;
    WindowDragSession m_dragSession;
    QPointF m_lastDragPos;

    BackgroundItem m_backgroundItem;
    bool m_ghostReturning = false;

    EffectScreen *m_screen = nullptr;
    QVector<EffectWindow *> m_windows;
    EffectWindow *m_draggedWindow = nullptr;
    WorkspaceBarDragModel m_barDragModel;
};

}
