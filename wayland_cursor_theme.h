// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_WAYLAND_CURSOR_THEME_H
#define KWIN_WAYLAND_CURSOR_THEME_H

#include <kwin_export.h>

#include <QObject>
#include "cursor.h"

struct wl_cursor_image;
struct wl_cursor_theme;

namespace KWayland
{
namespace Client
{
class ShmPool;
}
}

namespace KWin
{

class KWIN_EXPORT WaylandCursorTheme : public QObject
{
    Q_OBJECT
public:
    explicit WaylandCursorTheme(KWayland::Client::ShmPool *shm, QObject *parent = nullptr);
    virtual ~WaylandCursorTheme();

    wl_cursor_image *get(CursorShape shape);
    wl_cursor_image *get(const QByteArray &name);

Q_SIGNALS:
    void themeChanged();

private:
    void loadTheme();
    void destroyTheme();
    wl_cursor_theme *m_theme;
    KWayland::Client::ShmPool *m_shm = nullptr;
};

}

#endif
