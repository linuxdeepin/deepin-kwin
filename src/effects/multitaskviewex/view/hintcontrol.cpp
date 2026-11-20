/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "hintcontrol.h"
#include "glpaintrenderer.h"
#include "imageprovider.h"
#include "textutils.h"

#include <QColor>
#include <QFont>
#include <QSizeF>

namespace KWin
{

namespace
{
const QColor kHintTextColor(Qt::black);
const QColor kHintBackgroundColor(0xE0, 0xE0, 0xE0);
constexpr int kHintFontPointSize = 14;
constexpr qreal kHintPaddingX = 16.0;
constexpr qreal kHintPaddingY = 8.0;
constexpr qreal kHintCornerRadius = 10.0;
constexpr qreal kBackgroundOpacityRatio = 0.8;

QFont hintFont()
{
    return TextUtils::systemFont(kHintFontPointSize);
}

void clampRect(QRectF &rect, const QRectF &bounds)
{
    if (bounds.isEmpty() || rect.isEmpty()) {
        return;
    }
    const qreal maxX = bounds.right() - rect.width();
    if (maxX >= bounds.left()) {
        rect.moveLeft(qBound(bounds.left(), rect.left(), maxX));
    } else {
        rect.moveLeft(bounds.center().x() - rect.width() / 2.0);
    }
    const qreal maxY = bounds.bottom() - rect.height();
    if (maxY >= bounds.top()) {
        rect.moveTop(qBound(bounds.top(), rect.top(), maxY));
    } else {
        rect.moveTop(bounds.center().y() - rect.height() / 2.0);
    }
}
}

HintControl::HintControl(QObject *parent)
    : GLControl(SceneLayer::Hint, parent)
{
}

void HintControl::setImageProvider(ImageProvider *provider)
{
    m_provider = provider;
}

void HintControl::setText(const QString &text)
{
    m_text = text;
}

void HintControl::setAnchor(const QPointF &anchor)
{
    m_anchor = anchor;
}

bool HintControl::hitTest([[maybe_unused]] const QPointF &pos) const
{
    return false;
}

QRectF HintControl::capsuleRect(const QPointF &anchor, const QSizeF &textSize, const QRectF &bounds)
{
    QRectF background(anchor.x() - textSize.width() / 2.0 - kHintPaddingX,
                      anchor.y() - textSize.height() / 2.0 - kHintPaddingY,
                      textSize.width() + 2.0 * kHintPaddingX,
                      textSize.height() + 2.0 * kHintPaddingY);
    clampRect(background, bounds);
    return background;
}

void HintControl::paint(GLPaintContext &ctx, const SceneFrame &frame)
{
    const qreal opacity = frame.enterProgress * (frame.bgSliding ? frame.bgSlideProgress : 1.0);
    if (!m_provider || m_text.isEmpty() || opacity <= 0.0) {
        return;
    }
    const QFont font = hintFont();
    const QString displayText = TextUtils::processText(m_text, font, kHintPaddingX, geometry().width());
    if (displayText.isEmpty()) {
        return;
    }
    const auto info = m_provider->textTexture(displayText, font, kHintTextColor);
    if (!info.texture) {
        return;
    }

    const QSizeF textSize(info.imageSize);
    const QRectF background = capsuleRect(m_anchor, textSize, geometry());

    const QRectF textRect(background.center().x() - textSize.width() / 2.0,
                          background.center().y() - textSize.height() / 2.0,
                          textSize.width(), textSize.height());
    auto &renderer = GLPaintRenderer::instance();
    renderer.drawColor(kHintBackgroundColor, background, kHintCornerRadius,
                       kBackgroundOpacityRatio * opacity, ctx.projection());
    renderer.drawTexture(info.texture, textRect, 0.0, opacity,
                         ctx.projection());
}

}
