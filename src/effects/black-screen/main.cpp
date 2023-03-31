// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "black-screen.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(BlackScreenEffect,
                              "metadata.json.stripped",
                              return BlackScreenEffect::supported();)

} // namespace KWin

#include "main.moc"
