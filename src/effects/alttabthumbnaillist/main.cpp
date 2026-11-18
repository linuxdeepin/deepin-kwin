// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "alttabthumbnaillist.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(AltTabThumbnailListEffect,
                              "metadata.json.stripped",
                              return effects->isOpenGLCompositing();)

} // namespace KWin

#include "main.moc"
