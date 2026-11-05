/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "unmanaged.h"

#include "atoms.h"
#include "deleted.h"
#include "effects.h"
#include "composite.h"
#include "scene/surfaceitem_x11.h"
#include "scene/windowitem.h"
#include "utils/common.h"
#include "utils/xcbutils.h"
#include "wayland/surface_interface.h"
#include "workspace.h"

#include <QDebug>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <QThread>

#include <xcb/shape.h>

using namespace KWaylandServer;

namespace KWin
{

// window types that are supported as unmanaged (mainly for compositing)
const NET::WindowTypes SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK = NET::NormalMask
    | NET::DesktopMask
    | NET::DockMask
    | NET::ToolbarMask
    | NET::MenuMask
    | NET::DialogMask
    /*| NET::OverrideMask*/
    | NET::TopMenuMask
    | NET::UtilityMask
    | NET::SplashMask
    | NET::DropdownMenuMask
    | NET::PopupMenuMask
    | NET::TooltipMask
    | NET::NotificationMask
    | NET::ComboBoxMask
    | NET::DNDIconMask
    | NET::OnScreenDisplayMask
    | NET::CriticalNotificationMask;

Unmanaged::Unmanaged()
    : Window()
{
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeXwayland:
        // The wayland surface is associated with the override-redirect window asynchronously.
        connect(this, &Window::surfaceChanged, this, &Unmanaged::associate);
        break;
    case Application::OperationModeX11:
        // We have no way knowing whether the override-redirect window can be painted. Mark it
        // as ready for painting after synthetic 50ms delay.
        QTimer::singleShot(50, this, &Unmanaged::initialize);
        break;
    case Application::OperationModeWaylandOnly:
        Q_UNREACHABLE();
    }
}

Unmanaged::~Unmanaged()
{
}

std::unique_ptr<WindowItem> Unmanaged::createItem(Scene *scene)
{
    return std::make_unique<WindowItemX11>(this, scene);
}

void Unmanaged::associate()
{
    // Should initialize unmanaged window when the surface is associated.
    // Unmanaged windows are created when KWin receive MapNotify.
    // MapNotify means that the window is mapped and ready for painting.
    initialize();
}

void Unmanaged::initialize()
{
    updateIsUsingBackingPixmap(window());
    setIsUsingBackingPixmap(isUsingBackingPixmap());
}

void Unmanaged::propertyNotifyEvent(xcb_property_notify_event_t *e)
{
    if (e->window == window() && e->atom == atoms->dde_net_wm_backing_pixmap_state) {
        updateIsUsingBackingPixmap(e->window);
    }

    Window::propertyNotifyEvent(e); 
}

bool Unmanaged::isUsingBackingPixmap () const
{
    return m_isUsingBackingPixmap;
}

void Unmanaged::setIsUsingBackingPixmap(bool usingBackingPixmap)
{
    if (usingBackingPixmap != m_isUsingBackingPixmap) {
        m_isUsingBackingPixmap = usingBackingPixmap;
    }

    if (!isUsingBackingPixmap()) {
        setReadyForPainting();
    }
}

void Unmanaged::updateIsUsingBackingPixmap(xcb_window_t window)
{
    const xcb_atom_t atom = atoms->dde_net_wm_backing_pixmap_state;
    if (atom == XCB_ATOM_NONE) {
        return;
    }

    xcb_get_property_cookie_t cookie = xcb_get_property(kwinApp()->x11Connection(), false,
                                                        window, atom, XCB_ATOM_CARDINAL, 0, 1);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(kwinApp()->x11Connection(),
                                                             cookie, nullptr);
    if (!reply) {
        return;
    }

    if (reply->type == XCB_ATOM_CARDINAL && reply->format == 32 && reply->value_len >= 1) {
        setIsUsingBackingPixmap(*reinterpret_cast<const uint32_t *>(xcb_get_property_value(reply)) == 0);
    }

    free(reply);
}

bool Unmanaged::track(xcb_window_t w)
{
    XServerGrabber xserverGrabber;
    Xcb::WindowAttributes attr(w);
    Xcb::WindowGeometry geo(w);
    if (attr.isNull() || attr->map_state != XCB_MAP_STATE_VIEWABLE) {
        return false;
    }
    if (attr->_class == XCB_WINDOW_CLASS_INPUT_ONLY) {
        return false;
    }
    if (geo.isNull()) {
        return false;
    }

    setWindowHandles(w); // the window is also the frame
    Xcb::selectInput(w, attr->your_event_mask | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE);
    m_bufferGeometry = geo.rect();
    m_frameGeometry = geo.rect();
    m_clientGeometry = geo.rect();
    checkOutput();
    m_visual = attr->visual;
    bit_depth = geo->depth;
    info = new NETWinInfo(kwinApp()->x11Connection(), w, kwinApp()->x11RootWindow(),
                          NET::WMWindowType | NET::WMPid | NET::WMName,
                          NET::WM2Opacity | NET::WM2WindowRole | NET::WM2WindowClass | NET::WM2OpaqueRegion);
    workspace()->checkIfFirstWindowOfProcess(this);
    setOpacity(info->opacityF());
    getResourceClass();
    getWmClientLeader();
    getWmClientMachine();
    wm_name = readName();
    if (Xcb::Extensions::self()->isShapeAvailable()) {
        xcb_shape_select_input(kwinApp()->x11Connection(), w, true);
    }
    detectShape(w);
    getWmOpaqueRegion();
    getSkipCloseAnimation();
    setupWindowRules(false);
    setSkipZoom(false);
    setupCompositing();

    if (QWindow *internalWindow = findInternalWindow()) {
        m_outline = internalWindow->property("__kwin_outline").toBool();
        m_splitbar = wm_name.isEmpty() ? false : wm_name.contains("splitbar");
        m_splitmenu = wm_name.isEmpty() ? false : wm_name.contains("splitmenu");
        m_switcherwin = internalWindow->property("__kwin_switcherwin").toBool();
        m_windowMenu = wm_name.isEmpty() ? false : wm_name.contains("ctx-menu");
    }
    m_watermark = wm_name.isEmpty() ? false : wm_name.contains("deepin-watermark");
    if (effects) {
        static_cast<EffectsHandlerImpl *>(effects)->checkInputWindowStacking();
    }
    return true;
}

void Unmanaged::moveResizeInternal(const QRectF &rect, MoveResizeMode mode, bool immediateResize)
{
    Q_UNUSED(mode);
    Q_UNUSED(immediateResize);
    // Override-redirect windows are positioned by their client, so a normal
    // moveResize() is a no-op. Mirror mode still has to move them onto the
    // primary screen to keep every window inside it, and the restore runs after the
    // mode has been switched off, so the pending restore counts as well.
    if (!workspace()->isMirrorMode() && !isMirrorModeRestorePending()) {
        return;
    }
    Xcb::moveResizeWindow(window(), rect.toAlignedRect());
}

void Unmanaged::release(ReleaseReason releaseReason)
{
    FUNC_DEBUG_LOG(Q_FUNC_INFO, window());
    if (SurfaceItemX11 *item = qobject_cast<SurfaceItemX11 *>(surfaceItem())) {
        if (releaseReason == ReleaseReason::Destroyed) {
            item->forgetDamage();
        } else {
            item->destroyDamage();
        }
    }
    Deleted *del = nullptr;
    if (releaseReason != ReleaseReason::KWinShutsDown) {
        del = Deleted::create(this);
    }
    if (releaseReason != ReleaseReason::Release) {
        Q_EMIT workspace()->preRemoveInternalWindow(this);
    }
    Q_EMIT windowClosed(this, del);
    if (!QWidget::find(window()) && releaseReason != ReleaseReason::Destroyed) { // don't affect our own windows
        if (Xcb::Extensions::self()->isShapeAvailable()) {
            xcb_shape_select_input(kwinApp()->x11Connection(), window(), false);
        }
        Xcb::selectInput(window(), XCB_EVENT_MASK_NO_EVENT);
    }
    workspace()->removeUnmanaged(this);
    if (releaseReason != ReleaseReason::KWinShutsDown) {
        disownDataPassedToDeleted();
        del->unrefWindow();
    }
    deleteUnmanaged(this);
}

void Unmanaged::deleteUnmanaged(Unmanaged *c)
{
    FUNC_DEBUG_LOG(Q_FUNC_INFO, c->window());
    delete c;
}

bool Unmanaged::hasScheduledRelease() const
{
    return m_scheduledRelease;
}

int Unmanaged::desktop() const
{
    return NET::OnAllDesktops; // TODO for some window types should be the current desktop?
}

QStringList Unmanaged::activities() const
{
    return QStringList();
}

QVector<VirtualDesktop *> Unmanaged::desktops() const
{
    return QVector<VirtualDesktop *>();
}

QPointF Unmanaged::clientPos() const
{
    return QPoint(0, 0); // unmanaged windows don't have decorations
}

NET::WindowType Unmanaged::windowType(bool direct, int supportedTypes) const
{
    // for unmanaged windows the direct does not make any difference
    // as there are no rules to check and no hacks to apply
    if (supportedTypes == 0) {
        supportedTypes = SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK;
    }
    return info->windowType(NET::WindowTypes(supportedTypes));
}

bool Unmanaged::isOutline() const
{
    return m_outline;
}

bool Unmanaged::isSplitBar() const
{
    return m_splitbar;
}

bool Unmanaged::isWaterMark() const
{
    return m_watermark;
}

bool Unmanaged::isUnmanaged() const
{
    return true;
}

bool Unmanaged::isSplitMenu() const
{
    return m_splitmenu;
}

bool Unmanaged::isSwitcherWin() const
{
    return m_switcherwin;
}

bool Unmanaged::isWindowMenu() const
{
    return m_windowMenu;
}

QWindow *Unmanaged::findInternalWindow() const
{
    const QWindowList windows = kwinApp()->topLevelWindows();
    for (QWindow *w : windows) {
        if (w->winId() == window()) {
            return w;
        }
    }
    return nullptr;
}

void Unmanaged::checkOutput()
{
    setOutput(workspace()->outputAt(frameGeometry().center()));
}

void Unmanaged::damageNotifyEvent()
{
    Q_ASSERT(kwinApp()->operationMode() == Application::OperationModeX11);
    SurfaceItemX11 *item = static_cast<SurfaceItemX11 *>(surfaceItem());
    if (item) {
        item->processDamage();
    }
}

QString Unmanaged::readName()
{
    QString str = QString::fromUtf8(info->name());
    return str.mid(0, str.indexOf("—")).simplified();
}

} // namespace
