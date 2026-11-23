/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "data/wallpaperprovider.h"
#include "data/workspacebarmetrics.h"
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
#include <QSet>
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
    if (!m_imgProvider || effects->screens().isEmpty()) {
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
    // The decode sizes come from the screens, not from the views: warm-up runs when
    // the effect is created, before any view knows its geometry, and a view only
    // learns it when it is first shown. Both sides read the same screen rectangle,
    // so the paint side asks for exactly what was decoded here.
    QSet<QString> liveKeys;
    for (EffectScreen *screen : effects->screens()) {
        // MaximizeFullArea is the same rectangle on every desktop, so the desktop argument
        // only documents intent: the full screen, not the client area.
        const QRectF fullRect = effects->clientArea(MaximizeFullArea, screen, effects->currentDesktop());
        const QSize bgSize = fullRect.size().toSize();
        const QSize thumbSize = barThumbSizeFor(fullRect.size());
        if (bgSize.isEmpty() || thumbSize.isEmpty()) {
            continue;
        }
        for (const QString &source : backgrounds) {
            liveKeys.insert(ImageProvider::decodeKey(source, bgSize));
        }
        for (const QString &source : clears) {
            liveKeys.insert(ImageProvider::decodeKey(source, thumbSize));
        }
        m_imgProvider->predecode(backgrounds, bgSize);
        m_imgProvider->predecode(clears, thumbSize);
    }
    // A decode no screen can ask for is dead memory; everything a screen can still ask for
    // stays, so this cannot repeat the bug of evicting the warm-up before it is painted.
    m_imgProvider->retainDecoded(liveKeys);
}

}
