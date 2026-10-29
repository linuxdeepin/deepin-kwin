/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "animation/animation.h"
#include "animation/frameclock.h"

#include <QtTest>

using namespace KWin;

static bool fuzzyEq(qreal a, qreal b)
{
    return qAbs(a - b) < 1e-12;
}

static QString animState(const Animation &a)
{
    return QStringLiteral("progress=%1 value=%2 done=%3 running=%4")
        .arg(QString::number(a.progress(), 'f', 6))
        .arg(QString::number(a.value(), 'f', 6))
        .arg(a.done() ? 1 : 0)
        .arg(a.running() ? 1 : 0);
}

class TestAnimation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void easingValues();
    void easingBoundaries();
    void inOutCubicIsSlowFastSlow();
    void linearTimeline();
    void advanceClampsAtDone();
    void resetThenAdvanceToDone();
    void frameDeltaClockIsPerViewIndependent();
    void frameDeltaClockClampsAndResets();
};

void TestAnimation::inOutCubicIsSlowFastSlow()
{
    QCOMPARE(easedValue(Easing::InOutCubic, 0.0), 0.0);
    QCOMPARE(easedValue(Easing::InOutCubic, 1.0), 1.0);
    QCOMPARE(easedValue(Easing::InOutCubic, 0.5), 0.5);
    QCOMPARE(easedValue(Easing::InOutCubic, 0.25), 0.0625);
    QCOMPARE(easedValue(Easing::InOutCubic, 0.75), 0.9375);
    const qreal d0 = easedValue(Easing::InOutCubic, 0.05) - easedValue(Easing::InOutCubic, 0.0);
    const qreal d1 = easedValue(Easing::InOutCubic, 0.55) - easedValue(Easing::InOutCubic, 0.5);
    const qreal d2 = easedValue(Easing::InOutCubic, 1.0) - easedValue(Easing::InOutCubic, 0.95);
    QVERIFY(d1 > d0);
    QVERIFY(d1 > d2);
}

void TestAnimation::easingValues()
{
    struct Case
    {
        Easing e;
        qreal t, expect;
    } cases[] = {
        {Easing::Linear, 0.25, 0.25},
        {Easing::Linear, 0.5, 0.5},
        {Easing::Linear, 0.75, 0.75},
        {Easing::InQuad, 0.25, 0.0625},
        {Easing::InQuad, 0.5, 0.25},
        {Easing::InQuad, 0.75, 0.5625},
        {Easing::OutQuad, 0.25, 0.4375},
        {Easing::OutQuad, 0.5, 0.75},
        {Easing::OutQuad, 0.75, 0.9375},
        {Easing::InCubic, 0.25, 0.015625},
        {Easing::InCubic, 0.5, 0.125},
        {Easing::InCubic, 0.75, 0.421875},
        {Easing::OutCubic, 0.25, 0.578125},
        {Easing::OutCubic, 0.5, 0.875},
        {Easing::OutCubic, 0.75, 0.984375},
        {Easing::InOutCubic, 0.25, 0.0625},
        {Easing::InOutCubic, 0.5, 0.5},
        {Easing::InOutCubic, 0.75, 0.9375},
        {Easing::InQuint, 0.25, 0.0009765625},
        {Easing::InQuint, 0.5, 0.03125},
        {Easing::InQuint, 0.75, 0.2373046875},
        {Easing::OutQuint, 0.25, 0.7626953125},
        {Easing::OutQuint, 0.5, 0.96875},
        {Easing::OutQuint, 0.75, 0.9990234375},
    };
    for (const Case &c : cases) {
        QVERIFY2(fuzzyEq(easedValue(c.e, c.t), c.expect),
                 qPrintable(QStringLiteral("easing=%1 t=%2").arg(int(c.e)).arg(c.t)));
    }
}

void TestAnimation::easingBoundaries()
{
    const Easing all[] = {Easing::Linear, Easing::InQuad, Easing::OutQuad, Easing::InCubic, Easing::OutCubic,
                          Easing::InOutCubic, Easing::InQuint, Easing::OutQuint};
    for (Easing e : all) {
        QVERIFY2(fuzzyEq(easedValue(e, 0.0), 0.0), qPrintable(QStringLiteral("f(0)!=0 easing=%1").arg(int(e))));
        QVERIFY2(fuzzyEq(easedValue(e, 1.0), 1.0), qPrintable(QStringLiteral("f(1)!=1 easing=%1").arg(int(e))));
        QVERIFY2(fuzzyEq(easedValue(e, -0.5), 0.0), qPrintable(QStringLiteral("f(-0.5)!=0 easing=%1").arg(int(e))));
        QVERIFY2(fuzzyEq(easedValue(e, 1.5), 1.0), qPrintable(QStringLiteral("f(1.5)!=1 easing=%1").arg(int(e))));
        QVERIFY2(easedValue(e, 0.6) >= easedValue(e, 0.4), qPrintable(QStringLiteral("non-monotonic easing=%1").arg(int(e))));
    }
}

void TestAnimation::linearTimeline()
{
    Animation a;
    a.configure(1000, Easing::Linear);
    QCOMPARE(animState(a), QStringLiteral("progress=0.000000 value=0.000000 done=0 running=0"));

    a.start();
    QCOMPARE(animState(a), QStringLiteral("progress=0.000000 value=0.000000 done=0 running=1"));

    a.advance(0);
    QCOMPARE(animState(a), QStringLiteral("progress=0.000000 value=0.000000 done=0 running=1"));

    a.advance(500);
    QCOMPARE(animState(a), QStringLiteral("progress=0.500000 value=0.500000 done=0 running=1"));

    a.advance(500);
    QCOMPARE(animState(a), QStringLiteral("progress=1.000000 value=1.000000 done=1 running=0"));
}

void TestAnimation::advanceClampsAtDone()
{
    Animation a;
    a.configure(600, Easing::OutQuint);
    a.start();
    a.advance(2000);
    QCOMPARE(animState(a), QStringLiteral("progress=1.000000 value=1.000000 done=1 running=0"));

    a.advance(500);
    QCOMPARE(animState(a), QStringLiteral("progress=1.000000 value=1.000000 done=1 running=0"));
}

void TestAnimation::resetThenAdvanceToDone()
{
    Animation a;
    a.configure(1000, Easing::Linear);
    a.start();
    a.advance(300);
    QCOMPARE(animState(a), QStringLiteral("progress=0.300000 value=0.300000 done=0 running=1"));

    a.reset();
    QCOMPARE(animState(a), QStringLiteral("progress=0.000000 value=0.000000 done=0 running=0"));

    a.start();
    a.advance(1000);
    QCOMPARE(animState(a), QStringLiteral("progress=1.000000 value=1.000000 done=1 running=0"));
}

void TestAnimation::frameDeltaClockIsPerViewIndependent()
{
    using namespace std::chrono_literals;

    FrameDeltaClock clockA;
    FrameDeltaClock clockB;
    QVector<int> deltasA;
    QVector<int> deltasB;
    for (int frame = 0; frame < 3; ++frame) {
        const auto present = 1000ms + frame * 16ms;
        deltasA.append(clockA.advance(present));
        deltasB.append(clockB.advance(present));
    }
    QCOMPARE(deltasA, (QVector<int>{16, 16, 16}));
    QCOMPARE(deltasB, (QVector<int>{16, 16, 16}));

    FrameDeltaClock shared;
    QCOMPARE(shared.advance(1000ms), 16);
    QCOMPARE(shared.advance(1000ms), 0);
}

void TestAnimation::frameDeltaClockClampsAndResets()
{
    using namespace std::chrono_literals;

    FrameDeltaClock c;
    QCOMPARE(c.advance(1000ms), 16);
    QCOMPARE(c.advance(1050ms), 50);
    QCOMPARE(c.advance(1050ms), 0);
    QCOMPARE(c.advance(1040ms), 0);
    QCOMPARE(c.advance(1290ms), 100);

    c.reset();
    QCOMPARE(c.advance(9000ms), 16);
}

QTEST_MAIN(TestAnimation)
#include "tst_animation.moc"
