/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "workspace.h"

#include <QtTest>

using namespace KWin;

class TestMirrorModeMapping : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testSameAspect();
    void testLetterbox();
    void testItemOffset();
    void testEmptyGeometries();
    void testPolicyParsing();
    void testShouldMirrorDecision();
};

void TestMirrorModeMapping::testSameAspect()
{
    const QRectF primary(0, 0, 1920, 1080);
    const QRectF mirror(1920, 0, 1920, 1080);
    QCOMPARE(Workspace::mirrorModeMirrorRect(QRectF(0, 0, 100, 100), primary, mirror), QRectF(1920, 0, 100, 100));
    QCOMPARE(Workspace::mirrorModeMirrorRect(primary, primary, mirror), mirror);
}

void TestMirrorModeMapping::testLetterbox()
{
    const QRectF primary(0, 0, 1920, 1080);
    // 4:3 mirror: scale 5/6, content centered vertically with black bands.
    const QRectF mirror(1920, 0, 1600, 1200);
    QCOMPARE(Workspace::mirrorModeMirrorRect(primary, primary, mirror), QRectF(1920, 150, 1600, 900));
    // Primary not at the origin.
    const QRectF offsetPrimary(1920, 0, 1920, 1080);
    const QRectF offsetMirror(0, 0, 1920, 1080);
    QCOMPARE(Workspace::mirrorModeMirrorRect(offsetPrimary, offsetPrimary, offsetMirror), offsetMirror);
}

void TestMirrorModeMapping::testItemOffset()
{
    const QRectF primary(0, 0, 1920, 1080);
    const QPointF item(301, 251);
    // The item position is applied before the paint transform, so the offset must make
    // item position + offset scale with the mirror.
    QCOMPARE(item + Workspace::mirrorModeMirrorOffset(item, primary, QRectF(1920, 0, 3840, 2160)),
             QPointF(1920, 0) + item * 2);
    QCOMPARE(item + Workspace::mirrorModeMirrorOffset(item, primary, QRectF(1920, 0, 1920, 1080)),
             QPointF(1920, 0) + item);
    // Primary not at the origin.
    const QRectF offsetPrimary(1920, 0, 1920, 1080);
    QCOMPARE(QPointF(2021, 251) + Workspace::mirrorModeMirrorOffset(QPointF(2021, 251), offsetPrimary, QRectF(0, 0, 1920, 1080)),
             QPointF(101, 251));
}

void TestMirrorModeMapping::testEmptyGeometries()
{
    QVERIFY(Workspace::mirrorModeMirrorRect(QRectF(0, 0, 10, 10), QRectF(), QRectF(0, 0, 10, 10)).isEmpty());
    QVERIFY(Workspace::mirrorModeMirrorRect(QRectF(0, 0, 10, 10), QRectF(0, 0, 10, 10), QRectF()).isEmpty());
}

void TestMirrorModeMapping::testPolicyParsing()
{
    using Policy = Workspace::MirrorModePolicy;
    QCOMPARE(Workspace::mirrorModePolicyFromString(QStringLiteral("blacklist")), Policy::Blacklist);
    QCOMPARE(Workspace::mirrorModePolicyFromString(QStringLiteral("whitelist")), Policy::Whitelist);
    QCOMPARE(Workspace::mirrorModePolicyFromString(QStringLiteral("passthrough")), Policy::Passthrough);
    QCOMPARE(Workspace::mirrorModePolicyFromString(QString()), Policy::Whitelist);
    QCOMPARE(Workspace::mirrorModePolicyFromString(QStringLiteral("bogus")), Policy::Whitelist);
}

void TestMirrorModeMapping::testShouldMirrorDecision()
{
    using Policy = Workspace::MirrorModePolicy;
    QVERIFY(!Workspace::mirrorModeShouldMirror(Policy::Blacklist, true));
    QVERIFY(Workspace::mirrorModeShouldMirror(Policy::Blacklist, false));
    QVERIFY(Workspace::mirrorModeShouldMirror(Policy::Whitelist, true));
    QVERIFY(!Workspace::mirrorModeShouldMirror(Policy::Whitelist, false));
    QVERIFY(Workspace::mirrorModeShouldMirror(Policy::Passthrough, true));
    QVERIFY(Workspace::mirrorModeShouldMirror(Policy::Passthrough, false));

}

QTEST_GUILESS_MAIN(TestMirrorModeMapping)

#include "test_mirror_mode_mapping.moc"
