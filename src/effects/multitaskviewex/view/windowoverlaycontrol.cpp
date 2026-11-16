/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowoverlaycontrol.h"
#include "assetpaths.h"
#include "glpaintrenderer.h"
#include "imageprovider.h"
#include "model/scenemodel.h"

namespace KWin
{

WindowOverlayControl::WindowOverlayControl(QObject *parent)
    : GLControl(SceneLayer::Overlay, parent)
{
}

int WindowOverlayControl::buttonAt(const QPointF &pos, int &topIndex, int &closeIndex) const
{
    topIndex = closeIndex = -1;
    if (!m_layer) {
        return -1;
    }
    const qreal s = m_model ? m_model->osScale() : 1.0;
    const auto &nodes = m_layer->nodes();
    for (int i = 0; i < nodes.size(); ++i) {
        const WindowNode &n = nodes.at(i);
        if (n.retiring) {
            continue;
        }
        if (n.overlayOpacity() <= 0.5) {
            continue;
        }
        if (n.topButtonRect(s).contains(pos))
            topIndex = i;
        if (n.closeButtonRect(s).contains(pos))
            closeIndex = i;
    }
    return (topIndex >= 0 || closeIndex >= 0) ? qMax(topIndex, closeIndex) : -1;
}

bool WindowOverlayControl::hitTest(const QPointF &pos) const
{
    int t, c;
    return buttonAt(pos, t, c) >= 0;
}

void WindowOverlayControl::press(const QPointF &pos)
{
    buttonAt(pos, m_pressTop, m_pressClose);
}

void WindowOverlayControl::release(const QPointF &pos)
{
    int t, c;
    buttonAt(pos, t, c);
    if (m_pressTop >= 0 && t == m_pressTop) {
        Q_EMIT topClicked(m_pressTop);
    } else if (m_pressClose >= 0 && c == m_pressClose) {
        Q_EMIT closeClicked(m_pressClose);
    }
    m_pressTop = m_pressClose = -1;
}

void WindowOverlayControl::cancel()
{
    GLControl::cancel();
    m_pressTop = m_pressClose = -1;
}

void WindowOverlayControl::paint(GLPaintContext &ctx, const SceneFrame &frame)
{
    const qreal fade = frame.enterProgress;
    if (!m_layer) {
        return;
    }
    auto &r = GLPaintRenderer::instance();
    const qreal s = m_model ? m_model->osScale() : 1.0;
    for (const WindowNode &n : m_layer->nodes()) {
        if (n.retiring) {
            if (n.window && n.opacity() > 0.01) {
                ctx.drawEffectWindow(n.window, n.source, n.rect(), n.opacity() * fade);
            }
            continue;
        }
        const qreal alpha = n.overlayOpacity() * fade;
        if (alpha <= 0.01) {
            continue;
        }
        if (!m_provider) {
            continue;
        }
        const auto top = m_provider->textureFor(topIconPath(), QSize(int(kButtonIcon), int(kButtonIcon)));
        const auto close = m_provider->textureFor(deleteIconPath(), QSize(int(kButtonIcon), int(kButtonIcon)));
        const QRectF topBtn = n.topButtonRect(s);
        const QRectF closeBtn = n.closeButtonRect(s);
        const QRectF topIcon = buttonIconRect(topBtn);
        const QRectF closeIcon = buttonIconRect(closeBtn);
        if (top.texture) {
            r.drawTexture(top.texture, topIcon, 0, alpha, ctx.projection());
        }
        if (close.texture) {
            r.drawTexture(close.texture, closeIcon, 0, alpha, ctx.projection());
        }
    }
}

}
