// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chameleon.h"
#include "chameleonconfig.h"

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(
    ChameleonDecoFactory,
    "chameleon.json",
    {
        auto global_config = ChameleonConfig::instance();
        Q_UNUSED(global_config)
        registerPlugin<Chameleon>();
    }
)

#include "main.moc"
