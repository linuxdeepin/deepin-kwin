/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 jccKevin <luochaojiang@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gbmloader.h"

#include <dlfcn.h>
#include <unistd.h>
#include <memory>
#include <algorithm>

#include <QString>
#include <QStringList>

GbmLoader *GbmLoader::g_gbmLoader = new GbmLoader();

GbmLoader *GbmLoader::loader()
{
    return g_gbmLoader;
}

void GbmLoader::release()
{
    if (g_gbmLoader) {
        delete g_gbmLoader;
        g_gbmLoader = nullptr;
    }
}

GbmLoader::GbmLoader()
{
    const QStringList libraries{ "libgbm.so", "libgbm.so.1" };
    std::any_of(libraries.cbegin(), libraries.cend(), [this](const QString &library) {
        if (auto *gbmHandle = dlopen(library.toUtf8(), RTLD_LAZY | RTLD_LOCAL)) {
            m_gbmHandle = gbmHandle;
            *(void **)(&createWithModifiers)    = dlsym(m_gbmHandle, "gbm_bo_create_with_modifiers");
            *(void **)(&createWithModifiers2)   = dlsym(m_gbmHandle, "gbm_bo_create_with_modifiers2");
            *(void **)(&gbmBoGetFdForPlane)     = dlsym(m_gbmHandle, "gbm_bo_get_fd_for_plane");
        }
        return m_gbmHandle;
    });
}

GbmLoader::~GbmLoader()
{
    if (m_gbmHandle) {
        dlclose(m_gbmHandle);
        m_gbmHandle = nullptr;
    }
}