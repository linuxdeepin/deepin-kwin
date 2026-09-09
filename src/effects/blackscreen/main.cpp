/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blackscreen.h"

namespace KWin
{
KWIN_EFFECT_FACTORY_SUPPORTED(BlackScreenEffect,
                              "metadata.json.stripped",
                              return BlackScreenEffect::supported();)
} // namespace KWin

#include "main.moc"
