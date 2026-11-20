/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "backgroundcontrol.h"
#include "glpaintrenderer.h"
#include "imageprovider.h"
#include "model/scenemodel.h"

namespace KWin
{

BackgroundControl::BackgroundControl(QObject *parent)
    : GLControl(SceneLayer::Background, parent)
{
}

void BackgroundControl::paint(GLPaintContext &ctx, const SceneFrame &frame)
{
    const qreal bgOpacity = frame.enterProgress;
    auto &r = GLPaintRenderer::instance();
    const QString wallpaper = m_model ? m_model->backgroundItem().wallpaper : QString();
    if (m_provider && !wallpaper.isEmpty()) {
        const auto info = m_provider->textureForReady(wallpaper, geometry().size().toSize(), true);
        if (info.texture) {
            r.drawTexture(info.texture, geometry(), 0, bgOpacity, ctx.projection());
        } else {
            r.drawColor(QColor(0x1a, 0x1a, 0x2e), geometry(), 0, bgOpacity, ctx.projection());
        }
    } else {
        r.drawColor(QColor(0x1a, 0x1a, 0x2e), geometry(), 0, bgOpacity, ctx.projection());
    }
    if (m_provider && frame.bgSliding) {
        const qreal t = frame.bgSlideProgress;
        const qreal dir = frame.bgSlideDir;
        const QSize sz = geometry().size().toSize();
        const auto oldInfo = m_provider->textureForReady(frame.bgSlideFromUri, sz, true);
        const auto newInfo = m_provider->textureForReady(frame.bgSlideToUri, sz, true);
        if (oldInfo.texture) {
            r.drawTexture(oldInfo.texture,
                          geometry().translated(-dir * geometry().width() * t, 0),
                          0, bgOpacity, ctx.projection());
        }
        if (newInfo.texture) {
            r.drawTexture(newInfo.texture,
                          geometry().translated(dir * geometry().width() * (1 - t), 0),
                          0, bgOpacity, ctx.projection());
        }
    }
    r.drawColor(Qt::black, geometry(), 0, frame.enterProgress * kBackgroundMaskAlpha,
                ctx.projection());
}

}
