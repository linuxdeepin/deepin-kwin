/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "view/textutils.h"

#include <QFontMetricsF>
#include <QGuiApplication>
#include <QtTest>

using namespace KWin;

namespace
{
bool isWellFormedUtf16(const QString &text)
{
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i).isHighSurrogate()) {
            if (i + 1 >= text.size() || !text.at(i + 1).isLowSurrogate()) {
                return false;
            }
            ++i;
        } else if (text.at(i).isLowSurrogate()) {
            return false;
        }
    }
    return true;
}

QString emoji()
{
    return QString::fromUtf8("\xF0\x9F\x98\x80");
}

constexpr int kTestFontPointSize = 14;
constexpr qreal kTestPaddingX = 16.0;
constexpr qreal kTestMaxCapsuleWidth = 80.0;
constexpr qreal kHugeWidth = 100000.0;

QFont testFont()
{
    QFont font;
    font.setPointSize(kTestFontPointSize);
    return font;
}

QString normalized(const QString &text)
{
    return TextUtils::processText(text, testFont(), kTestPaddingX, kHugeWidth);
}
}

class TestTextUtils : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void truncatesLongTitles();
    void truncatesChineseTitles();
    void keepsGraphemeBoundaries();
    void truncatesEmoji();
    void truncatesCombiningMarks();
    void normalizesText();
    void selectsEffectiveText();
    void elidesToCapsuleWidth();
    void scalesDesignSizeToSystemFont();
};

void TestTextUtils::truncatesLongTitles()
{
    const QString title = normalized(QString(40, QLatin1Char('a')));
    QCOMPARE(title.size(), TextUtils::kMaxTitleLength + 1);
    QCOMPARE(title.left(TextUtils::kMaxTitleLength), QString(TextUtils::kMaxTitleLength, QLatin1Char('a')));
    QCOMPARE(title.back(), QChar(0x2026));

    const QString exact(TextUtils::kMaxTitleLength, QLatin1Char('a'));
    QCOMPARE(normalized(exact), exact);
    QCOMPARE(normalized(QStringLiteral("short")), QStringLiteral("short"));
}

void TestTextUtils::truncatesChineseTitles()
{
    const QString title = normalized(QString(30, QChar(0x4E2D)));
    QCOMPARE(title.size(), TextUtils::kMaxTitleLength + 1);
    QCOMPARE(title.left(TextUtils::kMaxTitleLength), QString(TextUtils::kMaxTitleLength, QChar(0x4E2D)));
    QCOMPARE(title.back(), QChar(0x2026));
}

void TestTextUtils::keepsGraphemeBoundaries()
{
    const QString title = QString(24, QLatin1Char('a')) + emoji();
    const QString result = normalized(title);
    QCOMPARE(result, title);
    QVERIFY(isWellFormedUtf16(result));
}

void TestTextUtils::truncatesEmoji()
{
    QString title;
    for (int i = 0; i < TextUtils::kMaxTitleLength + 1; ++i) {
        title += emoji();
    }
    const QString truncated = normalized(title);
    QVERIFY(isWellFormedUtf16(truncated));
    QCOMPARE(truncated.size(), TextUtils::kMaxTitleLength * 2 + 1);
    QVERIFY(truncated.endsWith(QChar(0x2026)));
}

void TestTextUtils::truncatesCombiningMarks()
{
    const QString combining = QStringLiteral("e") + QChar(0x0301);
    const QString exact = QString(24, QLatin1Char('a')) + combining;
    QCOMPARE(normalized(exact), exact);

    const QString tooLong = QString(25, QLatin1Char('a')) + combining;
    const QString truncated = normalized(tooLong);
    QCOMPARE(truncated, QString(25, QLatin1Char('a')) + QChar(0x2026));
    QVERIFY(isWellFormedUtf16(truncated));
}

void TestTextUtils::normalizesText()
{
    QCOMPARE(normalized(QStringLiteral("  Window\nTitle  ")), QStringLiteral("Window Title"));
    QVERIFY(normalized(QStringLiteral("   ")).isEmpty());
}

void TestTextUtils::selectsEffectiveText()
{
    QCOMPARE(TextUtils::effectiveText(QStringLiteral("  Window\nTitle  "), QStringLiteral("app")),
             QStringLiteral("Window Title"));
    QCOMPARE(TextUtils::effectiveText(QString(), QStringLiteral("  app.fallback  ")),
             QStringLiteral("app.fallback"));
    QCOMPARE(TextUtils::effectiveText(QStringLiteral("   "), QStringLiteral("app.fallback")),
             QStringLiteral("app.fallback"));
    QCOMPARE(TextUtils::effectiveText(QStringLiteral("title"), QStringLiteral("app")),
             QStringLiteral("title"));
    QVERIFY(TextUtils::effectiveText(QString(), QString()).isEmpty());
}

void TestTextUtils::elidesToCapsuleWidth()
{
    const QFont font = testFont();
    const qreal paddingX = kTestPaddingX;

    QCOMPARE(TextUtils::processText(QStringLiteral("Hi"), font, paddingX, 100.0), QStringLiteral("Hi"));
    QVERIFY(TextUtils::processText(QString(), font, paddingX, kHugeWidth).isEmpty());
    QVERIFY(TextUtils::processText(QStringLiteral("text"), font, paddingX, 0.0).isEmpty());

    const QString longText(100, QLatin1Char('a'));
    const QString elided = TextUtils::processText(longText, font, paddingX, kTestMaxCapsuleWidth);
    QVERIFY(!elided.isEmpty());
    QVERIFY(elided.size() < longText.size());
    QVERIFY(elided.endsWith(QChar(0x2026)));
    QVERIFY(!elided.endsWith(QString(QChar(0x2026)) + QChar(0x2026)));
    QVERIFY(QFontMetricsF(font).horizontalAdvance(elided) <= kTestMaxCapsuleWidth - 2.0 * paddingX);

    const QString chinese(30, QChar(0x4E2D));
    const QString chineseElided = TextUtils::processText(chinese, font, paddingX, kTestMaxCapsuleWidth);
    QVERIFY(!chineseElided.isEmpty());
    QVERIFY(chineseElided.endsWith(QChar(0x2026)));
    QVERIFY(QFontMetricsF(font).horizontalAdvance(chineseElided) <= kTestMaxCapsuleWidth - 2.0 * paddingX);
}

void TestTextUtils::scalesDesignSizeToSystemFont()
{
    const QFont systemFont = QGuiApplication::font();
    const QFont hintFont = TextUtils::systemFont(kTestFontPointSize);
    QCOMPARE(hintFont.family(), systemFont.family());

    const qreal systemPointSize = systemFont.pointSizeF() > 0.0
        ? systemFont.pointSizeF()
        : TextUtils::kDefaultFontPointSize;
    QCOMPARE(hintFont.pointSize(),
             qRound(kTestFontPointSize * systemPointSize / TextUtils::kDefaultFontPointSize));
}

QTEST_MAIN(TestTextUtils)
#include "tst_textutils.moc"
