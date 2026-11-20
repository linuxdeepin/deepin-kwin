/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "data/wallpaperprovider.h"
#include "data/workspacemodel.h"
#include "effectconfig.h"
#include "helpers.h"
#include "multitaskviewex.h"
#include "view/backgroundcontrol.h"
#include "view/imageprovider.h"
#include "view/screenview.h"
#include "wallpapermanager.h"

#include "effects.h"
#include "virtualdesktops.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QSizeF>

namespace KWin
{

void MultitaskViewExEffect::warmWallpapers()
{
    m_wallpapers->warm();
}

void MultitaskViewExEffect::onWallpapersRefreshed()
{
    prewarmTextures();
    if (!m_running) {
        return;
    }
    refreshModel();
    syncBarModel();
    reapplyBackgrounds();
    effects->addRepaintFull();
}

void MultitaskViewExEffect::reapplyBackgrounds()
{
    const int cur = workspaces() ? workspaces()->currentNumber() : 1;
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        const QString wp = m_wallpapers->backgroundFor(cur, owned->model.screen());
        acquireBackground(*owned, wp);
        owned->model.backgroundItem().setWallpaper(wp);
    }
}

void MultitaskViewExEffect::prewarmTextures()
{
    if (!m_imgProvider) {
        return;
    }
    QStringList clears;
    QStringList backgrounds;
    for (const QString &uri : m_wallpapers->allUris()) {
        const QString path = toRealPath(uri);
        if (!path.isEmpty() && !backgrounds.contains(path)) {
            backgrounds.append(path);
        }
    }
    auto *vds = VirtualDesktopManager::self();
    if (vds) {
        for (const VirtualDesktop *d : vds->desktops()) {
            const int num = int(d->x11DesktopNumber());
            for (EffectScreen *s : effects->screens()) {
                const QString clear = m_wallpapers->clearUri(num, s);
                if (clear.isEmpty()) {
                    continue;
                }
                const QString clearPath = toRealPath(clear);
                if (!clears.contains(clearPath)) {
                    clears.append(clearPath);
                }
            }
        }
    }
    for (const std::unique_ptr<ScreenView> &owned : m_views.all()) {
        const WorkspaceBarMetrics &metrics = owned->model.metrics();
        const QSize bgSize = owned->scene->background()->geometry().size().toSize();
        const QSize thumbSize = QSizeF(metrics.workspaceWidth(), metrics.workspaceHeight()).toSize();
        m_imgProvider->predecode(backgrounds, bgSize);
        m_imgProvider->predecode(clears, thumbSize);
    }
}

}
