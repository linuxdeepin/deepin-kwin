/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QRectF>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace KWin
{


inline constexpr int kAddPreviewId = 0;

inline constexpr int kDragHoverSwitchMs = 1000;

inline constexpr qreal kDragTipIconSize = 21.0;

/**
 * @brief Decides whether hovering a slot should switch the current desktop.
 * @param hoverSlot Slot index currently hovered, or a negative value when none.
 * @param currentNumber Current desktop number.
 * @param slotNumber Desktop number of the hovered slot.
 * @return True when the hovered slot is a real, different desktop.
 */
inline bool shouldSwitchDesktop(int hoverSlot, int currentNumber, int slotNumber)
{
    return hoverSlot >= 0 && slotNumber > 0 && slotNumber != currentNumber;
}

/**
 * @brief Decides whether clicking a workspace slot should leave the view.
 * @param slotNumber Desktop number of the clicked slot.
 * @param currentNumber Current desktop number.
 * @param slideAnimating Whether the background slide is still running.
 * @return True when the slot is the current desktop and no slide is running.
 */
inline bool shouldExitDesktopOnClick(int slotNumber, int currentNumber, bool slideAnimating)
{
    return slotNumber > 0 && slotNumber == currentNumber && !slideAnimating;
}

struct GhostDrop
{
    QRectF rect;
    qreal opacity = 0.0;
    int durationMs = 400;
};

/**
 * @brief Computes the ghost drop animation for a released drag.
 * @param movedAway Whether the pointer was moved far enough to delete the workspace.
 * @param ghostNow Current ghost rectangle.
 * @param cell Destination cell rectangle.
 * @return The ghost target rectangle, opacity and duration.
 */
inline GhostDrop ghostDropBehavior(bool movedAway, const QRectF &ghostNow, const QRectF &cell)
{
    if (movedAway) {
        return GhostDrop{ghostNow, 0.0, 200};
    }
    return GhostDrop{cell, 1.0, 400};
}

enum class DropTarget {
    None,
    DesktopItem,
    AddButton,
};

/**
 * @brief Resolves the drop target under the pointer.
 * @param slotAtPos Desktop slot under the pointer, or a negative value when none.
 * @param onAddButton Whether the pointer is over the add button.
 * @param canAdd Whether another workspace may still be added.
 * @return The drop target for the given position.
 */
inline DropTarget dropTargetFor(int slotAtPos, bool onAddButton, bool canAdd)
{
    if (slotAtPos >= 0) {
        return DropTarget::DesktopItem;
    }
    if (onAddButton && canAdd) {
        return DropTarget::AddButton;
    }
    return DropTarget::None;
}

/**
 * @brief Resolves the base desktop for a drag session.
 * @param sessionBase Desktop captured when the session started, or a non-positive value.
 * @param currentDesktop Desktop to fall back to.
 * @return The session base desktop when positive, otherwise the current desktop.
 */
inline int sessionBaseDesktop(int sessionBase, int currentDesktop)
{
    return sessionBase > 0 ? sessionBase : currentDesktop;
}

/**
 * @brief Tests whether the effect should accept input.
 * @param running Whether the effect is running.
 * @param exitRequested Whether an exit has already been requested.
 * @return True when running and no exit has been requested.
 */
inline bool acceptsInput(bool running, bool exitRequested)
{
    return running && !exitRequested;
}

/**
 * @brief Tests whether the add button may be shown.
 * @param count Current number of workspaces.
 * @param maxWorkspaces Maximum number of workspaces allowed.
 * @return True when another workspace may be added.
 */
inline bool showAddButton(int count, int maxWorkspaces)
{
    return count < maxWorkspaces;
}

/**
 * @brief Resolves the desktop position at which a new workspace is appended.
 * @param currentCount Current number of workspaces.
 * @return The append position, never negative.
 */
inline uint addDesktopPosition(int currentCount)
{
    return uint(currentCount < 0 ? 0 : currentCount);
}

}
