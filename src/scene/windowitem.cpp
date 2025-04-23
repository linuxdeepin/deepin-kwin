/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/windowitem.h"
#include "deleted.h"
#include "internalwindow.h"
#include "scene/decorationitem.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem_internal.h"
#include "scene/surfaceitem_wayland.h"
#include "scene/surfaceitem_x11.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KDecoration2/Decoration>

namespace KWin
{

WindowItem::WindowItem(Window *window, Scene *scene, Item *parent)
    : Item(scene, parent)
    , m_window(window)
{
    m_decorationChangedConnection = connect(window, &Window::decorationChanged, this, &WindowItem::updateDecorationItem);
    updateDecorationItem();

    connect(window, &Window::shadowChanged, this, &WindowItem::updateShadowItem);
    updateShadowItem();

    connect(window, &Window::frameGeometryChanged, this, &WindowItem::updatePosition);
    updatePosition();

    if (waylandServer()) {
        connect(waylandServer(), &WaylandServer::lockStateChanged, this, &WindowItem::updateVisibility);
    }
    connect(window, &Window::lockScreenOverlayChanged, this, &WindowItem::updateVisibility);
    connect(window, &Window::minimizedChanged, this, &WindowItem::updateVisibility);
    connect(window, &Window::hiddenChanged, this, &WindowItem::updateVisibility);
    connect(window, &Window::activitiesChanged, this, &WindowItem::updateVisibility);
    connect(window, &Window::desktopChanged, this, &WindowItem::updateVisibility);
    connect(workspace(), &Workspace::currentActivityChanged, this, &WindowItem::updateVisibility);
    connect(workspace(), &Workspace::currentDesktopChanged, this, &WindowItem::updateVisibility);
    updateVisibility();

    connect(window, &Window::opacityChanged, this, &WindowItem::updateOpacity);
    updateOpacity();

    connect(window, &Window::windowClosed, this, &WindowItem::handleWindowClosed);
}

WindowItem::~WindowItem()
{
    disconnect(m_decorationChangedConnection);
}

SurfaceItem *WindowItem::surfaceItem() const
{
    return m_surfaceItem.get();
}

DecorationItem *WindowItem::decorationItem() const
{
    return m_decorationItem.get();
}

ShadowItem *WindowItem::shadowItem() const
{
    return m_shadowItem.get();
}

Window *WindowItem::window() const
{
    return m_window;
}

void WindowItem::refVisible(int reason)
{
    if (reason & PAINT_DISABLED_BY_HIDDEN) {
        m_forceVisibleByHiddenCount++;
    }
    if (reason & PAINT_DISABLED_BY_DELETE) {
        m_forceVisibleByDeleteCount++;
    }
    if (reason & PAINT_DISABLED_BY_DESKTOP) {
        m_forceVisibleByDesktopCount++;
    }
    if (reason & PAINT_DISABLED_BY_MINIMIZE) {
        m_forceVisibleByMinimizeCount++;
    }
    if (reason & PAINT_DISABLED_BY_ACTIVITY) {
        m_forceVisibleByActivityCount++;
    }
    updateVisibility();
}

void WindowItem::unrefVisible(int reason)
{
    if (reason & PAINT_DISABLED_BY_HIDDEN) {
        Q_ASSERT(m_forceVisibleByHiddenCount > 0);
        m_forceVisibleByHiddenCount--;
    }
    if (reason & PAINT_DISABLED_BY_DELETE) {
        Q_ASSERT(m_forceVisibleByDeleteCount > 0);
        m_forceVisibleByDeleteCount--;
    }
    if (reason & PAINT_DISABLED_BY_DESKTOP) {
        Q_ASSERT(m_forceVisibleByDesktopCount > 0);
        m_forceVisibleByDesktopCount--;
    }
    if (reason & PAINT_DISABLED_BY_MINIMIZE) {
        Q_ASSERT(m_forceVisibleByMinimizeCount > 0);
        m_forceVisibleByMinimizeCount--;
    }
    if (reason & PAINT_DISABLED_BY_ACTIVITY) {
        Q_ASSERT(m_forceVisibleByActivityCount > 0);
        m_forceVisibleByActivityCount--;
    }
    updateVisibility();
}

void WindowItem::handleWindowClosed(Window *original, Deleted *deleted)
{
    m_window = deleted;
}

bool WindowItem::computeVisibility() const
{
    if (!m_window || !m_window->readyForPainting()) {
        return false;
    }
    if (waylandServer() && waylandServer()->isScreenLocked()) {
        return m_window->isLockScreen() || m_window->isInputMethod() || m_window->isLockScreenOverlay();
    }
    if (m_window->isDeleted()) {
        if (m_forceVisibleByDeleteCount == 0) {
            return false;
        }
    }
    if (!m_window->isOnCurrentDesktop()) {
        if (m_forceVisibleByDesktopCount == 0) {
            return false;
        }
    }
    if (!m_window->isOnCurrentActivity()) {
        if (m_forceVisibleByActivityCount == 0) {
            return false;
        }
    }
    if (m_window->isMinimized()) {
        if (m_forceVisibleByMinimizeCount == 0) {
            return false;
        }
    }
    if (m_window->isHiddenInternal()) {
        if (m_forceVisibleByHiddenCount == 0) {
            return false;
        }
    }
    return true;
}

void WindowItem::updateVisibility()
{
    setVisible(computeVisibility());
}

void WindowItem::updatePosition()
{
    if (!m_window)
        return;
    setPosition(m_window->pos());
}

void WindowItem::updateSurfaceItem(SurfaceItem *surfaceItem)
{
    if (!m_window)
        return;

    m_surfaceItem.reset(surfaceItem);

    if (m_surfaceItem) {
        connect(m_window, &Window::shadeChanged, this, &WindowItem::updateSurfaceVisibility);
        connect(m_window, &Window::bufferGeometryChanged, this, &WindowItem::updateSurfacePosition);
        connect(m_window, &Window::frameGeometryChanged, this, &WindowItem::updateSurfacePosition);

        connect(surfaceItem, &SurfaceItem::damaged, this, &WindowItem::markDamaged);
        connect(surfaceItem, &SurfaceItem::childAdded, this, [this](Item *item) {
            auto surfaceItem = static_cast<SurfaceItem *>(item);
            connect(surfaceItem, &SurfaceItem::damaged, this, &WindowItem::markDamaged);
        });

        updateSurfacePosition();
        updateSurfaceVisibility();
    } else {
        disconnect(m_window, &Window::shadeChanged, this, &WindowItem::updateSurfaceVisibility);
        disconnect(m_window, &Window::bufferGeometryChanged, this, &WindowItem::updateSurfacePosition);
        disconnect(m_window, &Window::frameGeometryChanged, this, &WindowItem::updateSurfacePosition);
    }
}

void WindowItem::updateSurfacePosition()
{
    if (!m_window)
        return;

    const QRectF bufferGeometry = m_window->bufferGeometry();
    const QRectF frameGeometry = m_window->frameGeometry();

    m_surfaceItem->setPosition(bufferGeometry.topLeft() - frameGeometry.topLeft());
}

void WindowItem::updateSurfaceVisibility()
{
    if (!m_window)
        return;
    m_surfaceItem->setVisible(!m_window->isShade());
}

void WindowItem::updateShadowItem()
{
    if (!m_window)
        return;

    Shadow *shadow = m_window->shadow();
    if (shadow) {
        if (!m_shadowItem || m_shadowItem->shadow() != shadow) {
            m_shadowItem.reset(new ShadowItem(shadow, m_window, scene(), this));
        }
        if (m_decorationItem) {
            m_shadowItem->stackBefore(m_decorationItem.get());
        } else if (m_surfaceItem) {
            m_shadowItem->stackBefore(m_surfaceItem.get());
        }
        markDamaged();
    } else {
        m_shadowItem.reset();
    }
}

void WindowItem::updateDecorationItem()
{
    if (!m_window || m_window->isDeleted() || m_window->isZombie()) {
        return;
    }

    if (m_decorationItem) {
        if (auto oldDeco = m_window->decoration()) {
            disconnect(oldDeco, &KDecoration2::Decoration::damaged, this, &WindowItem::markDamaged);
        }
        m_decorationItem.reset();
    }

    if (const auto decoration = m_window->decoration()) {
        m_decorationItem.reset(new DecorationItem(decoration, m_window, scene(), this));
        if (m_decorationItem) {
            if (m_shadowItem) {
                m_decorationItem->stackAfter(m_shadowItem.get());
            } else if (m_surfaceItem) {
                m_decorationItem->stackBefore(m_surfaceItem.get());
            }
            connect(decoration, &KDecoration2::Decoration::damaged,
                    this, &WindowItem::markDamaged,
                    Qt::QueuedConnection);
            markDamaged();
        }
    }
}

void WindowItem::updateOpacity()
{
    if (!m_window)
        return;
    setOpacity(m_window->opacity());
}

void WindowItem::markDamaged()
{
    if (!m_window)
        return;
    Q_EMIT m_window->damaged(m_window);
}

WindowItemX11::WindowItemX11(Window *window, Scene *scene, Item *parent)
    : WindowItem(window, scene, parent)
{
    initialize();

    // Xwayland windows and Wayland surfaces are associated asynchronously.
    connect(window, &Window::surfaceChanged, this, &WindowItemX11::initialize);
}

void WindowItemX11::initialize()
{
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeX11:
        updateSurfaceItem(new SurfaceItemX11(window(), scene(), this));
        break;
    case Application::OperationModeXwayland:
        if (!window()->surface()) {
            updateSurfaceItem(nullptr);
        } else {
            updateSurfaceItem(new SurfaceItemXwayland(window(), scene(), this));
        }
        break;
    case Application::OperationModeWaylandOnly:
        Q_UNREACHABLE();
    }
}

WindowItemWayland::WindowItemWayland(Window *window, Scene *scene, Item *parent)
    : WindowItem(window, scene, parent)
{
    updateSurfaceItem(new SurfaceItemWayland(window->surface(), scene, this));
}

WindowItemInternal::WindowItemInternal(InternalWindow *window, Scene *scene, Item *parent)
    : WindowItem(window, scene, parent)
{
    updateSurfaceItem(new SurfaceItemInternal(window, scene, this));
}

} // namespace KWin
