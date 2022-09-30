// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "screen.h"
#include "platformcursor.h"
#include "screens.h"
#include "wayland_server.h"
#include "integration.h"

namespace KWin
{
namespace QPA
{

Screen::Screen(int screen, Integration *integration)
    : QPlatformScreen()
    , m_screen(screen)
    , m_cursor(new PlatformCursor)
    , m_integration(integration)
{
}

Screen::~Screen() = default;

QList<QPlatformScreen *> Screen::virtualSiblings() const
{
    const auto screens = m_integration->getScreens();

    QList<QPlatformScreen *> siblings;
    siblings.reserve(siblings.size());

    for (Screen *screen : screens) {
        siblings << screen;
    }

    return siblings;
}


int Screen::depth() const
{
    return 32;
}

QImage::Format Screen::format() const
{
    return QImage::Format_ARGB32_Premultiplied;
}

QRect Screen::geometry() const
{
    return m_screen != -1 ? screens()->geometry(m_screen) : QRect(0, 0, 1, 1);
}

QSizeF Screen::physicalSize() const
{
    QSizeF ps;
    if (m_screen != -1) {
        ps = screens()->physicalSize(m_screen);
    }

    if (ps.isEmpty()) {
        ps = QPlatformScreen::physicalSize();
    }

    return ps;
}

QPlatformCursor *Screen::cursor() const
{
    return m_cursor.data();
}

QDpi Screen::logicalDpi() const
{
    static int force_dpi = qEnvironmentVariableIsSet("QT_WAYLAND_FORCE_DPI") ? qEnvironmentVariableIntValue("QT_WAYLAND_FORCE_DPI") : -1;

    if (force_dpi > 0) {
        return QDpi(force_dpi, force_dpi);
    }

    return QPlatformScreen::logicalDpi();
}

qreal Screen::devicePixelRatio() const
{
    return m_screen != -1 ? screens()->scale(m_screen) : 1.0;
}

}
}
