// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "screens_hwcomposer.h"
#include "hwcomposer_backend.h"

namespace KWin
{

HwcomposerScreens::HwcomposerScreens(HwcomposerBackend *backend, QObject *parent)
    : BasicScreens(backend, parent)
    , m_backend(backend)
{
}

HwcomposerScreens::~HwcomposerScreens() = default;

float HwcomposerScreens::refreshRate(int screen) const
{
    Q_UNUSED(screen)
    return m_backend->refreshRate() / 1000.0f;
}

QSizeF HwcomposerScreens::physicalSize(int screen) const
{
    const QSizeF size = m_backend->physicalSize();
    if (size.isValid()) {
        return size;
    }
    return Screens::physicalSize(screen);
}

}
