/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "windowradius.h"
#include "window.h"
#include "effects.h"
#include "workspace.h"
#include "windowstylemanager.h"
#include "decorationstyle.h"
#include "composite.h"
#include <xcb/xcb.h>
#include <QWindow>
#include <QX11Info>

#define _DEEPIN_SCISSOR_WINDOW "_DEEPIN_SCISSOR_WINDOW"

Q_DECLARE_METATYPE(QPainterPath)

namespace KWin
{
static xcb_atom_t internAtom(const char *name, bool only_if_exists)
{
    if (!name || *name == 0)
        return XCB_NONE;

    if (!QX11Info::isPlatformX11())
        return XCB_NONE;

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(QX11Info::connection(), only_if_exists, strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(QX11Info::connection(), cookie, 0);

    if (!reply)
        return XCB_NONE;

    xcb_atom_t atom = reply->atom;
    free(reply);

    return atom;
}

WindowRadius::WindowRadius(Window *window)
    : m_window(window)
{
    m_atom_deepin_scissor_window = internAtom(_DEEPIN_SCISSOR_WINDOW, false);

    connect(m_window->windowStyleObj(), &DecorationStyle::windowRadiusChanged, this, &WindowRadius::onUpdateWindowRadiusChanged);
}

WindowRadius::~WindowRadius()
{
}

int WindowRadius::updateWindowRadius()
{
    if (!m_window || !m_window->effectWindow() || !m_window->windowStyleObj())
        return 0;

    KWin::EffectWindowImpl *effect = m_window->effectWindow();
    if (m_window->isMaximized()) {
        effect->setData(WindowClipPathRole, QVariant());
        effect->setData(WindowRadiusRole, QVariant());
        m_radius = QPointF(0.0, 0.0);
        return 1;
    }

    int ret = -1;
    QPainterPath path;
    const QByteArray &clip_data = effect->readProperty(m_atom_deepin_scissor_window, m_atom_deepin_scissor_window, 8);
    if (!clip_data.isEmpty()) {
        QDataStream ds(clip_data);
        ds >> path;
    }
    if (path.isEmpty()) {
        effect->setData(WindowClipPathRole, QVariant());
        QPointF radius = getWindowRadius();
        auto effect_window_radius = effect->data(WindowRadiusRole);
        if (effect_window_radius != radius) {
            effect->setData(WindowRadiusRole, QVariant::fromValue(radius));
            m_radius = radius;
            ret = 1;
        }
    } else {
        effect->setData(WindowRadiusRole, QVariant());
        effect->setData(WindowClipPathRole, QVariant::fromValue(path));
        ret = 1;
    }

    return ret;
}

QPointF WindowRadius::getWindowRadius()
{
    QPointF radius;
    if (m_window->windowStyleObj()->propertyIsValid(DecorationStyle::WindowRadiusProperty)) {
        radius = m_window->windowStyleObj()->windowRadius();
    } else {
        radius.setX(Workspace::self()->getWindowStyleMgr()->getOsRadius() * Workspace::self()->getWindowStyleMgr()->getOsScale());
        radius.setY(Workspace::self()->getWindowStyleMgr()->getOsRadius() * Workspace::self()->getWindowStyleMgr()->getOsScale());
    }
    m_radius = radius;
    return radius;
}

QString WindowRadius::theme() const
{
    return property("theme").toString();
}

void WindowRadius::onUpdateWindowRadiusChanged()
{
    m_window->updateWindowRadius();
}

}
