/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <KWindowSystem/private/kwindowshadow_p.h>
#else
#include <private/kwindowshadow_p.h>
#endif

namespace KWin
{

class WindowShadowTile final : public KWindowShadowTilePrivate
{
public:
    bool create() override;
    void destroy() override;
};

class WindowShadow final : public KWindowShadowPrivate
{
public:
    bool create() override;
    void destroy() override;
};

} // namespace KWin
