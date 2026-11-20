/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QString>
#include <QTextBoundaryFinder>

namespace KWin
{
namespace TextUtils
{

/**
 * @brief Maximum number of grapheme clusters kept before an ellipsis is appended.
 */
inline constexpr int kMaxTitleLength = 25;

/**
 * @brief Point size the design values assume for the system font.
 */
inline constexpr qreal kDefaultFontPointSize = 10.5;

/**
 * @brief Builds a UI font that tracks the system font family and size.
 * @param designPointSize Point size at the default system font size.
 * @return System font scaled from kDefaultFontPointSize to designPointSize.
 */
inline QFont systemFont(qreal designPointSize)
{
    QFont font = QGuiApplication::font();
    const qreal systemPointSize = font.pointSizeF();
    const qreal scale = systemPointSize > 0.0 ? systemPointSize / kDefaultFontPointSize : 1.0;
    font.setPointSize(qRound(designPointSize * scale));
    return font;
}

/**
 * @brief Selects a non-empty single-line text, falling back when the caption is blank.
 * @param caption Window caption.
 * @param fallback Text used when the caption is blank.
 * @return Simplified caption, or simplified fallback when the caption is blank.
 */
inline QString effectiveText(const QString &caption, const QString &fallback)
{
    const QString title = caption.simplified();
    return title.isEmpty() ? fallback.simplified() : title;
}

/**
 * @brief Normalizes, truncates, and elides text for a hint capsule.
 * @param text Raw text to process.
 * @param font Font used to measure the text.
 * @param paddingX Horizontal capsule padding in pixels.
 * @param maxCapsuleWidth Maximum capsule width in pixels.
 * @return Single-line text elided to the capsule, or an empty string when no text fits.
 */
inline QString processText(const QString &text, const QFont &font, qreal paddingX, qreal maxCapsuleWidth)
{
    QString title = text.simplified();
    if (title.isEmpty() || maxCapsuleWidth <= 0.0) {
        return QString();
    }
    if (title.size() > kMaxTitleLength) {
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, title);
        int count = 0;
        int cut = 0;
        while (count < kMaxTitleLength) {
            const int next = finder.toNextBoundary();
            if (next < 0) {
                break;
            }
            cut = next;
            ++count;
        }
        if (cut > 0 && cut < title.size()) {
            title = title.left(cut) + QChar(0x2026);
        }
    }
    const qreal maxTextWidth = qMax<qreal>(1.0, maxCapsuleWidth - 2.0 * paddingX);
    return QFontMetricsF(font).elidedText(title, Qt::ElideRight, maxTextWidth);
}

}
}
