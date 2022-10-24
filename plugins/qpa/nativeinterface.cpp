// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nativeinterface.h"
#include "integration.h"
#include "window.h"
#include "../../wayland_server.h"

#include <QWindow>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/surface.h>

namespace KWin
{
namespace QPA
{

static const QByteArray s_displayKey = QByteArrayLiteral("display");
static const QByteArray s_wlDisplayKey = QByteArrayLiteral("wl_display");
static const QByteArray s_compositorKey = QByteArrayLiteral("compositor");
static const QByteArray s_surfaceKey = QByteArrayLiteral("surface");

NativeInterface::NativeInterface(Integration *integration)
    : QPlatformNativeInterface()
    , m_integration(integration)
{
}

void *NativeInterface::nativeResourceForIntegration(const QByteArray &resource)
{
    const QByteArray r = resource.toLower();
    if (r == s_displayKey || r == s_wlDisplayKey) {
        if (!waylandServer() || !waylandServer()->internalClientConection()) {
            return nullptr;
        }
        return waylandServer()->internalClientConection()->display();
    }
    if (r == s_compositorKey) {
        return static_cast<wl_compositor*>(*m_integration->compositor());
    }
    return nullptr;
}

void *NativeInterface::nativeResourceForWindow(const QByteArray &resource, QWindow *window)
{
    const QByteArray r = resource.toLower();
    if (r == s_displayKey || r == s_wlDisplayKey) {
        if (!waylandServer() || !waylandServer()->internalClientConection()) {
            return nullptr;
        }
        return waylandServer()->internalClientConection()->display();
    }
    if (r == s_compositorKey) {
        return static_cast<wl_compositor*>(*m_integration->compositor());
    }
    if (r == s_surfaceKey && window) {
        if (auto handle = window->handle()) {
            if (auto surface = static_cast<Window*>(handle)->surface()) {
                return static_cast<wl_surface*>(*surface);
            }
        }
    }
    return nullptr;
}

static void roundtrip()
{
    if (!waylandServer()) {
        return;
    }
    auto c = waylandServer()->internalClientConection();
    if (!c) {
        return;
    }
    c->flush();
    waylandServer()->dispatch();
}

QFunctionPointer NativeInterface::platformFunction(const QByteArray &function) const
{
    if (qstrcmp(function.toLower(), "roundtrip") == 0) {
        return &roundtrip;
    }
    return nullptr;
}

}
}
