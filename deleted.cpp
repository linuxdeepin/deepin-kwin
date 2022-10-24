// Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "deleted.h"

#include "workspace.h"
#include "client.h"
#include "group.h"
#include "netinfo.h"
#include "shadow.h"
#include "shell_client.h"
#include "decorations/decoratedclient.h"
#include "decorations/decorationrenderer.h"

#include <QDebug>

namespace KWin
{

Deleted::Deleted()
    : Toplevel()
    , delete_refcount(1)
    , m_frame(XCB_WINDOW_NONE)
    , no_border(true)
    , m_layer(UnknownLayer)
    , m_minimized(false)
    , m_modal(false)
    , m_wasClient(false)
    , m_wasCurrentTab(true)
    , m_decorationRenderer(nullptr)
    , m_fullscreen(false)
    , m_keepAbove(false)
    , m_keepBelow(false)
    , m_wasActive(false)
    , m_wasX11Client(false)
    , m_wasWaylandClient(false)
    , m_wasGroupTransient(false)
    , m_wasPopupWindow(false)
{
}

Deleted::~Deleted()
{
    if (delete_refcount != 0)
        qCCritical(KWIN_CORE) << "Deleted client has non-zero reference count (" << delete_refcount << ")";
    assert(delete_refcount == 0);
    if (workspace()) {
        workspace()->removeDeleted(this);
    }
    for (Toplevel *toplevel : qAsConst(m_transientFor)) {
        if (auto *deleted = qobject_cast<Deleted *>(toplevel)) {
            deleted->removeTransient(this);
        }
    }
    for (Deleted *transient : qAsConst(m_transients)) {
        transient->removeTransientFor(this);
    }
    deleteEffectWindow();
}

Deleted* Deleted::create(Toplevel* c)
{
    Deleted* d = new Deleted();
    d->copyToDeleted(c);
    workspace()->addDeleted(d, c);
    return d;
}

// to be used only from Workspace::finishCompositing()
void Deleted::discard()
{
    delete_refcount = 0;
    delete this;
}

void Deleted::copyToDeleted(Toplevel* c)
{
    assert(dynamic_cast< Deleted* >(c) == NULL);
    Toplevel::copyToDeleted(c);
    desk = c->desktop();
    m_desktops = c->desktops();
    activityList = c->activities();
    contentsRect = QRect(c->clientPos(), c->clientSize());
    m_contentPos = c->clientContentPos();
    transparent_rect = c->transparentRect();
    m_layer = c->layer();
    m_frame = c->frameId();
    m_opacity = c->opacity();
    m_type = c->windowType();
    m_windowRole = c->windowRole();
    if (WinInfo* cinfo = dynamic_cast< WinInfo* >(info))
        cinfo->disable();
    if (AbstractClient *client = dynamic_cast<AbstractClient*>(c)) {
        no_border = client->noBorder();
        if (!no_border) {
            client->layoutDecorationRects(decoration_left,
                                          decoration_top,
                                          decoration_right,
                                          decoration_bottom);
            if (client->isDecorated()) {
                if (Decoration::Renderer *renderer = client->decoratedClient()->renderer()) {
                    m_decorationRenderer = renderer;
                    m_decorationRenderer->reparent(this);
                }
            }
        }
        m_wasClient = true;
        m_minimized = client->isMinimized();
        m_modal = client->isModal();
        m_mainClients = client->mainClients();
        foreach (AbstractClient *c, m_mainClients) {
            addTransientFor(c);
            connect(c, &AbstractClient::windowClosed, this, &Deleted::mainClientClosed);
        }
        m_fullscreen = client->isFullScreen();
        m_wasCurrentTab = client->isCurrentTab();
        m_keepAbove = client->keepAbove();
        m_keepBelow = client->keepBelow();
        m_caption = client->caption();

        m_wasActive = client->isActive();

        m_wasGroupTransient = client->groupTransient();
    }

    for (auto vd : m_desktops) {
        connect(vd, &QObject::destroyed, this, [=] {
            m_desktops.removeOne(vd);
        });
    }

    m_wasWaylandClient = qobject_cast<ShellClient *>(c) != nullptr;
    m_wasX11Client = !m_wasWaylandClient;
    m_wasPopupWindow = c->isPopupWindow();
}

void Deleted::unrefWindow()
{
    if (--delete_refcount > 0)
        return;
    // needs to be delayed
    // a) when calling from effects, otherwise it'd be rather complicated to handle the case of the
    // window going away during a painting pass
    // b) to prevent dangeling pointers in the stacking order, see bug #317765
    deleteLater();
}

int Deleted::desktop() const
{
    return desk;
}

QStringList Deleted::activities() const
{
    return activityList;
}

QVector<VirtualDesktop *> Deleted::desktops() const
{
    return m_desktops;
}

QPoint Deleted::clientPos() const
{
    return contentsRect.topLeft();
}

QSize Deleted::clientSize() const
{
    return contentsRect.size();
}

void Deleted::debug(QDebug& stream) const
{
    stream << "\'ID:" << window() << "\' (deleted)";
}

void Deleted::layoutDecorationRects(QRect& left, QRect& top, QRect& right, QRect& bottom) const
{
    left = decoration_left;
    top = decoration_top;
    right = decoration_right;
    bottom = decoration_bottom;
}

QRect Deleted::decorationRect() const
{
    return rect();
}

QRect Deleted::transparentRect() const
{
    return transparent_rect;
}

bool Deleted::isDeleted() const
{
    return true;
}

NET::WindowType Deleted::windowType(bool direct, int supportedTypes) const
{
    Q_UNUSED(direct)
    Q_UNUSED(supportedTypes)
    return m_type;
}

void Deleted::mainClientClosed(Toplevel *client)
{
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(client))
        m_mainClients.removeAll(c);
}

void Deleted::transientForClosed(Toplevel *toplevel, Deleted *deleted)
{
    if (deleted == nullptr) {
        m_transientFor.removeAll(toplevel);
        return;
    }

    const int index = m_transientFor.indexOf(toplevel);
    if (index == -1) {
        return;
    }

    m_transientFor[index] = deleted;
    deleted->addTransient(this);
}

xcb_window_t Deleted::frameId() const
{
    return m_frame;
}

double Deleted::opacity() const
{
    return m_opacity;
}

QByteArray Deleted::windowRole() const
{
    return m_windowRole;
}

QVector<uint> Deleted::x11DesktopIds() const
{
    const auto desks = desktops();
    QVector<uint> x11Ids;
    x11Ids.reserve(desks.count());
    std::transform(desks.constBegin(), desks.constEnd(),
        std::back_inserter(x11Ids),
        [] (const VirtualDesktop *vd) {
            return vd->x11DesktopNumber();
        }
    );
    return x11Ids;
}

void Deleted::addTransient(Deleted *transient)
{
    m_transients.append(transient);
}

void Deleted::removeTransient(Deleted *transient)
{
    m_transients.removeAll(transient);
}

void Deleted::addTransientFor(AbstractClient *parent)
{
    m_transientFor.append(parent);
    connect(parent, &AbstractClient::windowClosed, this, &Deleted::transientForClosed);
}

void Deleted::removeTransientFor(Deleted *parent)
{
    m_transientFor.removeAll(parent);
}

} // namespace

