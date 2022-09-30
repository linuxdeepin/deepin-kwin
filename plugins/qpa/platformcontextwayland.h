// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_QPA_PLATFORMCONTEXTWAYLAND_H
#define KWIN_QPA_PLATFORMCONTEXTWAYLAND_H

#include "abstractplatformcontext.h"

namespace KWin
{
namespace QPA
{
class Integration;

class PlatformContextWayland : public AbstractPlatformContext
{
public:
    explicit PlatformContextWayland(QOpenGLContext *context, Integration *integration);

    void swapBuffers(QPlatformSurface *surface) override;

    bool makeCurrent(QPlatformSurface *surface) override;

    bool isSharing() const override;

private:
    void create();
};

}
}

#endif
