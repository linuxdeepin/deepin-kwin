// Copyright (C) 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_OVERLAYWINDOW_H
#define KWIN_OVERLAYWINDOW_H

#include <QRegion>
// xcb
#include <xcb/xcb.h>

#include <kwin_export.h>

namespace KWin {
class KWIN_EXPORT OverlayWindow {
public:
    virtual ~OverlayWindow();
    /// Creates XComposite overlay window, call initOverlay() afterwards
    virtual bool create() = 0;
    /// Init overlay and the destination window in it
    virtual void setup(xcb_window_t window) = 0;
    virtual void show() = 0;
    virtual void hide() = 0; // hides and resets overlay window
    virtual void setShape(const QRegion& reg) = 0;
    virtual void resize(const QSize &size) = 0;
    /// Destroys XComposite overlay window
    virtual void destroy() = 0;
    virtual xcb_window_t window() const = 0;
    virtual bool isVisible() const = 0;
    virtual void setVisibility(bool visible) = 0;
protected:
    OverlayWindow();
};
} // namespace

#endif //KWIN_OVERLAYWINDOW_H
