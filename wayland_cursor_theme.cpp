// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wayland_cursor_theme.h"
#include "cursor.h"
#include "wayland_server.h"
#include "screens.h"
#include "workspace.h"
// Qt
#include <QVector>
#include <QDebug>
// KWayland
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
// Wayland
#include <wayland-cursor.h>

namespace KWin
{

WaylandCursorTheme::WaylandCursorTheme(KWayland::Client::ShmPool *shm, QObject *parent)
    : QObject(parent)
    , m_theme(nullptr)
    , m_shm(shm)
{
    connect(screens(), &Screens::maxScaleChanged, this, &WaylandCursorTheme::loadTheme);
}

WaylandCursorTheme::~WaylandCursorTheme()
{
    destroyTheme();
}

void WaylandCursorTheme::loadTheme()
{
    if (!m_shm->isValid()) {
        return;
    }
    Cursor *c = Cursor::self();
    int size = c->themeSize();
    if (size == 0) {
        //set a default size
        size = 24;
    }

    size *= screens()->maxScale();

    auto theme = wl_cursor_theme_load(c->themeName().toUtf8().constData(),
                                   size, m_shm->shm());
    if (theme) {
        if (!m_theme) {
            // so far the theme had not been created, this means we need to start tracking theme changes
            connect(c, &Cursor::themeChanged, this, &WaylandCursorTheme::loadTheme);
        } else {
            destroyTheme();
        }
        m_theme = theme;
        if (workspace() && workspace()->isKwinDebug()) {
            qDebug()<<"emit themeChanged"<<c->themeName();
        }
        emit themeChanged();
    }
}

void WaylandCursorTheme::destroyTheme()
{
    if (!m_theme) {
        return;
    }
    wl_cursor_theme_destroy(m_theme);
    m_theme = nullptr;
}

wl_cursor_image *WaylandCursorTheme::get(CursorShape shape)
{
    return get(shape.name());
}

wl_cursor_image *WaylandCursorTheme::get(const QByteArray &name)
{
    if (!m_theme) {
        loadTheme();
    }
    if (!m_theme) {
        // loading cursor failed
        return nullptr;
    }
    wl_cursor *c = wl_cursor_theme_get_cursor(m_theme, name.constData());
    if (!c || c->image_count <= 0) {
        const auto &names = Cursor::self()->cursorAlternativeNames(name);
        for (auto it = names.begin(), end = names.end(); it != end; it++) {
            c = wl_cursor_theme_get_cursor(m_theme, (*it).constData());
            if (c && c->image_count > 0) {
                break;
            }
        }
    }
    if (!c || c->image_count <= 0) {
        return nullptr;
    }
    // TODO: who deletes c?
    return c->images[0];
}

}
