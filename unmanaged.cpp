// Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "unmanaged.h"

#include "workspace.h"
#include "effects.h"
#include "deleted.h"
#include "utils.h"
#include "xcbutils.h"
#include "atoms.h"
#include <QTimer>
#include <QDebug>

#include <xcb/shape.h>

namespace KWin
{

Unmanaged::Unmanaged()
    : Toplevel()
    , m_scheduledRelease(false)
{
    ready_for_painting = false;
    connect(this, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), SIGNAL(geometryChanged()));
    QTimer::singleShot(50, this, SLOT(setReadyForPainting()));
}

Unmanaged::~Unmanaged()
{
}

bool Unmanaged::track(Window w)
{
    GRAB_SERVER_DURING_CONTEXT
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
    setWindowHandles(w);   // the window is also the frame
    Xcb::selectInput(w, attr->your_event_mask | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE);
    geom = geo.rect();
    checkScreen();
    m_visual = attr->visual;
    bit_depth = geo->depth;
    info = new NETWinInfo(connection(), w, rootWindow(),
                          NET::WMWindowType | NET::WMPid | NET::WMState,
                          NET::WM2Opacity |
                          NET::WM2WindowRole |
                          NET::WM2WindowClass |
                          NET::WM2OpaqueRegion);
    getResourceClass();
    getWmClientLeader();
    getWmClientMachine();
    if (Xcb::Extensions::self()->isShapeAvailable())
        xcb_shape_select_input(connection(), w, true);
    detectShape(w);
    getWmOpaqueRegion();
    getSkipCloseAnimation();
    setupCompositing();
    if (effects)
        static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowStacking();
    return true;
}

void Unmanaged::release(ReleaseReason releaseReason)
{
    Deleted* del = NULL;
    if (releaseReason != ReleaseReason::KWinShutsDown) {
        del = Deleted::create(this);
    }
    emit windowClosed(this, del);
    finishCompositing(releaseReason);
    if (!QWidget::find(window()) && releaseReason != ReleaseReason::Destroyed) { // don't affect our own windows
        if (Xcb::Extensions::self()->isShapeAvailable())
            xcb_shape_select_input(connection(), window(), false);
        Xcb::selectInput(window(), XCB_EVENT_MASK_NO_EVENT);
    }
    if (releaseReason != ReleaseReason::KWinShutsDown) {
        workspace()->removeUnmanaged(this);
        addWorkspaceRepaint(del->visibleRect());
        disownDataPassedToDeleted();
        del->unrefWindow();
    }
    deleteUnmanaged(this);
}

void Unmanaged::deleteUnmanaged(Unmanaged* c)
{
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

QPoint Unmanaged::clientPos() const
{
    return QPoint(0, 0);   // unmanaged windows don't have decorations
}

QSize Unmanaged::clientSize() const
{
    return size();
}

QRect Unmanaged::transparentRect() const
{
    return QRect(clientPos(), clientSize());
}

void Unmanaged::debug(QDebug& stream) const
{
    stream << "\'ID:" << window() << "\'";
}

NET::WindowType Unmanaged::windowType(bool direct, int supportedTypes) const
{
    // for unmanaged windows the direct does not make any difference
    // as there are no rules to check and no hacks to apply
    Q_UNUSED(direct)
    if (supportedTypes == 0) {
        supportedTypes = SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK;
    }
    return info->windowType(NET::WindowTypes(supportedTypes));
}

bool Unmanaged::fetchWindowForLockScreen()
{
    auto cookie = xcb_get_property(connection(),0,this->window(),atoms->deepin_lock_screen,
                         atoms->deepin_lock_screen,0,0);
    xcb_get_property_reply_t *reply;
    xcb_generic_error_t *error;
    if ((reply = xcb_get_property_reply(connection(), cookie, &error))) {
        if (reply->type == XCB_ATOM_ATOM && reply->format == 32 && reply->bytes_after == 1) {
            free(reply);
            return true;
        }
    }

    return false;
}

bool Unmanaged::isKeepAbove()
{
    return bool(info->state() & NET::KeepAbove);
}

void Unmanaged::addDamage(const QRegion &damage)
{
    repaints_region += damage;
    Toplevel::addDamage(damage);
}

} // namespace

