/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2022 MBition GmbH
    SPDX-FileContributor: Kai Uwe Broulik <kai_uwe.broulik@mbition.io>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kwin.h>

#include <sys/mman.h>
#include <unistd.h>

#include "core/output.h"
#include "core/session.h"
#include "utils/damagejournal.h"
#include "utils/ramfile.h"

#include <QtTest>

#include <memory>

using namespace KWin;

/**
 * Minimal Output subclass, so that the scale and transform dependent region mapping can be
 * tested without a backend.
 */
class TestOutput : public Output
{
public:
    RenderLoop *renderLoop() const override
    {
        return nullptr;
    }

    void configure(const QSize &modeSize, qreal scale, Transform transform)
    {
        State state;
        state.position = QPoint(0, 0);
        state.scale = scale;
        state.transform = transform;
        state.currentMode = std::make_shared<OutputMode>(modeSize, 60000);
        setState(state);
    }
};

class TestUtils : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testRamFile();
    void testSealedRamFile();
    void testDamageJournal();
    void testLogicalToNativeRegion();
    void testPauseDeviceType();
};

static const QByteArray s_testByteArray = QByteArrayLiteral("Test Data \0\1\2\3");
static const char s_writeTestArray[] = "test";

void TestUtils::testRamFile()
{
    KWin::RamFile file("test", s_testByteArray.constData(), s_testByteArray.size());
    QVERIFY(file.isValid());
    QCOMPARE(file.size(), s_testByteArray.size());

    QVERIFY(file.fd() != -1);

    char buf[20];
    int num = read(file.fd(), buf, sizeof buf);
    QCOMPARE(num, file.size());

    QCOMPARE(qstrcmp(s_testByteArray.constData(), buf), 0);
}

void TestUtils::testSealedRamFile()
{
#if HAVE_MEMFD
    KWin::RamFile file("test", s_testByteArray.constData(), s_testByteArray.size(), KWin::RamFile::Flag::SealWrite);
    QVERIFY(file.isValid());
    QVERIFY(file.effectiveFlags().testFlag(KWin::RamFile::Flag::SealWrite));

    // Writing should not work.
    auto written = write(file.fd(), s_writeTestArray, strlen(s_writeTestArray));
    QCOMPARE(written, -1);

    // Cannot use MAP_SHARED on sealed file descriptor.
    void *data = mmap(nullptr, file.size(), PROT_WRITE, MAP_SHARED, file.fd(), 0);
    QCOMPARE(data, MAP_FAILED);

    data = mmap(nullptr, file.size(), PROT_WRITE, MAP_PRIVATE, file.fd(), 0);
    QVERIFY(data != MAP_FAILED);
#else
    QSKIP("Sealing requires memfd suport.");
#endif
}

void TestUtils::testDamageJournal()
{
    const QRegion full = infiniteRegion();

    DamageJournal journal;
    journal.setCapacity(3);

    // An empty journal holds no information, so the caller has to repaint everything.
    QCOMPARE(journal.accumulate(0, full), full);
    QCOMPARE(journal.accumulate(1, full), full);
    QCOMPARE(journal.accumulate(2, full), full);

    const QRegion a(QRect(0, 0, 10, 10));
    const QRegion b(QRect(20, 20, 10, 10));
    const QRegion c(QRect(40, 40, 10, 10));
    const QRegion d(QRect(60, 60, 10, 10));

    // A buffer of age 1 was presented in the previous frame and is still up to date; older
    // ages accumulate the damage of the frames in between.
    journal.add(a);
    QVERIFY(journal.accumulate(1, full).isEmpty());
    QCOMPARE(journal.accumulate(2, full), full);

    journal.add(b);
    QVERIFY(journal.accumulate(1, full).isEmpty());
    QCOMPARE(journal.accumulate(2, full), b);
    QCOMPARE(journal.accumulate(3, full), full);

    journal.add(c);
    QCOMPARE(journal.accumulate(2, full), c);
    QCOMPARE(journal.accumulate(3, full), c.united(b));

    // Once the capacity is exceeded the oldest entry is evicted, which makes the age unknown.
    journal.add(d);
    QCOMPARE(journal.accumulate(2, full), d);
    QCOMPARE(journal.accumulate(3, full), d.united(c));

    journal.clear();
    QCOMPARE(journal.accumulate(1, full), full);
}

void TestUtils::testLogicalToNativeRegion()
{
    const QSize modeSize(1920, 1080);

    {
        TestOutput output;
        output.configure(modeSize, 1.0, Output::Transform::Normal);
        QCOMPARE(output.logicalToNativeRegion(QRect(10, 20, 30, 40)), QRegion(QRect(10, 20, 30, 40)));
    }
    {
        TestOutput output;
        output.configure(modeSize, 2.0, Output::Transform::Normal);
        QCOMPARE(output.logicalToNativeRegion(QRect(10, 20, 30, 40)), QRegion(QRect(20, 40, 60, 80)));
    }
    {
        // Multiple rectangles have to be mapped individually.
        TestOutput output;
        output.configure(modeSize, 2.0, Output::Transform::Normal);
        const QRegion logical = QRect(0, 0, 10, 10).united(QRect(50, 50, 10, 10));
        const QRegion expected = QRect(0, 0, 20, 20).united(QRect(100, 100, 20, 20));
        QCOMPARE(output.logicalToNativeRegion(logical), expected);
    }
    {
        // The infinite region can't be scaled or rotated and has to be passed through.
        TestOutput output;
        output.configure(modeSize, 2.0, Output::Transform::Rotated270);
        QCOMPARE(output.logicalToNativeRegion(infiniteRegion()), infiniteRegion());
    }

    // A whole-output rectangle maps to the whole native buffer under every transform, so it can
    // not catch a wrong rotation or flip. The top-left 100x50 rectangle below can; its expected
    // native rectangles are derived by hand from logicalToNativeMatrix() for a 1920x1080 output
    // at scale 1 (note that rect() is transposed for 90 and 270 degrees).
    const QRect logicalRect(0, 0, 100, 50);
    const struct {
        Output::Transform transform;
        QRect nativeRect;
    } partialCases[] = {
        {Output::Transform::Normal, QRect(0, 0, 100, 50)},
        {Output::Transform::Rotated90, QRect(0, 980, 50, 100)},
        {Output::Transform::Rotated180, QRect(1820, 1030, 100, 50)},
        {Output::Transform::Rotated270, QRect(1870, 0, 50, 100)},
        {Output::Transform::Flipped, QRect(1820, 0, 100, 50)},
        {Output::Transform::Flipped90, QRect(0, 0, 50, 100)},
        {Output::Transform::Flipped180, QRect(0, 1030, 100, 50)},
        {Output::Transform::Flipped270, QRect(1870, 980, 50, 100)},
    };
    for (const auto &testCase : partialCases) {
        TestOutput output;
        output.configure(modeSize, 1.0, testCase.transform);
        QCOMPARE(output.logicalToNativeRegion(logicalRect), QRegion(testCase.nativeRect));
    }

    const Output::Transform transforms[] = {
        Output::Transform::Normal,
        Output::Transform::Rotated90,
        Output::Transform::Rotated180,
        Output::Transform::Rotated270,
        Output::Transform::Flipped,
        Output::Transform::Flipped90,
        Output::Transform::Flipped180,
        Output::Transform::Flipped270,
    };
    for (const Output::Transform transform : transforms) {
        const bool transposed = transform == Output::Transform::Rotated90
            || transform == Output::Transform::Rotated270
            || transform == Output::Transform::Flipped90
            || transform == Output::Transform::Flipped270;
        for (const qreal scale : {1.0, 2.0}) {
            // The native size is independent of the rotation: rotating the full output covers
            // the whole native buffer, just with the axes swapped for 90 and 270 degrees.
            TestOutput output;
            output.configure(modeSize, scale, transform);
            const QSize logicalSize = output.rect().size();
            QSize nativeSize = transposed ? logicalSize.transposed() : logicalSize;
            nativeSize *= scale;
            QCOMPARE(output.logicalToNativeRegion(output.rect()), QRegion(QRect(QPoint(0, 0), nativeSize)));
        }
    }
}

void TestUtils::testPauseDeviceType()
{
    // devicePaused() only skips deactivating the gpu for "gone", since that one is never
    // resumed. Everything that is not recognized has to stay on the resumable path.
    QVERIFY(Session::pauseDeviceTypeFromString(QStringLiteral("gone")) == Session::PauseDeviceType::Gone);
    QVERIFY(Session::pauseDeviceTypeFromString(QStringLiteral("force")) == Session::PauseDeviceType::Force);
    QVERIFY(Session::pauseDeviceTypeFromString(QStringLiteral("pause")) == Session::PauseDeviceType::Pause);
    QVERIFY(Session::pauseDeviceTypeFromString(QString()) == Session::PauseDeviceType::Pause);
    QVERIFY(Session::pauseDeviceTypeFromString(QStringLiteral("bogus")) == Session::PauseDeviceType::Pause);
}

QTEST_MAIN(TestUtils)
#include "test_utils.moc"
