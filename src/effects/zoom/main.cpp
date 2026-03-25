/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "zoom.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(ZoomEffect,
                    "metadata.json.stripped",
                    return ZoomEffect::supported();
                    ,
                    return ZoomEffect::enabledByDefault();
                    )

} // namespace KWin

#include "main.moc"
