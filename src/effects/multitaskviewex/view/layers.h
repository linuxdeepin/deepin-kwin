/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector>

#include <array>

namespace KWin
{

enum class SceneLayer {
    Background = 0,
    Cells = 5,
    Windows = 10,
    Hint = 15,
    Bar = 60,
    Overlay = 70,
    Ghost = 100,
};

/**
 * @brief Returns the z value of a scene layer.
 * @param layer Scene layer.
 * @return The layer z ordering value.
 */
inline constexpr int layerZ(SceneLayer layer)
{
    return static_cast<int>(layer);
}

/**
 * @brief Returns the scene layers ordered back to front.
 * @return The layer order used for painting.
 */
inline constexpr std::array<SceneLayer, 7> sceneLayerOrder()
{
    return {SceneLayer::Background, SceneLayer::Cells, SceneLayer::Windows,
            SceneLayer::Hint, SceneLayer::Bar, SceneLayer::Overlay, SceneLayer::Ghost};
}

enum class WindowLayer {
    Desktop,
    Normal,
    Dock,
    Skip,
};

/**
 * @brief Classifies a window into a paint layer.
 * @param isDesktop Whether the window is a desktop.
 * @param isDock Whether the window is a dock.
 * @param isNotification Whether the window is a notification.
 * @param isOutline Whether the window is an outline.
 * @return The window layer the window belongs to.
 */
inline WindowLayer windowLayerOf(bool isDesktop, bool isDock, bool isNotification, bool isOutline)
{
    if (isDesktop) {
        return WindowLayer::Desktop;
    }
    if (isDock) {
        return WindowLayer::Dock;
    }
    if (isNotification || isOutline) {
        return WindowLayer::Skip;
    }
    return WindowLayer::Normal;
}

/**
 * @brief Tests whether a point lies inside any dock area.
 * @param dockAreas Dock rectangles.
 * @param pos Point to test.
 * @return True when the point is inside a dock area.
 */
inline bool pointerBelongsToDock(const QVector<QRectF> &dockAreas, const QPointF &pos)
{
    for (const QRectF &r : dockAreas) {
        if (r.contains(pos)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Tests whether a mouse button triggers a dock click.
 * @param button Mouse button to test.
 * @return True when the button is relayed to the dock, that is, the left or
 *         the right one. The middle button and every other button are ignored.
 */
inline bool isDockClickButton(Qt::MouseButton button)
{
    return button == Qt::LeftButton || button == Qt::RightButton;
}

/**
 * @brief Tests whether a button release completes a dock click.
 * @param pressButton Button recorded when the pointer was pressed inside the dock.
 * @param releaseButton Button of the release event.
 * @param sameView Whether the release happened on the screen that received the press.
 * @param releaseInDock Whether the release happened inside a dock area.
 * @return True when the view must exit and relay the click to the dock.
 */
inline bool isDockClickRelease(Qt::MouseButton pressButton, Qt::MouseButton releaseButton, bool sameView, bool releaseInDock)
{
    return pressButton != Qt::NoButton && pressButton == releaseButton && sameView && releaseInDock
        && isDockClickButton(pressButton);
}

}
