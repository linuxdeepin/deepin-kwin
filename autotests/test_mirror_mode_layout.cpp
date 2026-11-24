/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mirrormodelayout.h"

#include <QtTest>

using namespace KWin;
using namespace KWin::MirrorModeLayout;

class TestMirrorModeLayout : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testRealMachineLayout();
    void testAnchorInTheMiddle();
    void testDisabledDisplayIsLeftAlone();
    void testNoPrimaryFallsBackToFirstEnabled();
    void testNoEnabledDisplay();
    void testEmptyLayout();
    void testClientGeometry();
    void testClampToClientGeometry();
};

static MirrorModeDisplay display(const QRect &geometry, bool enabled = true, bool primary = false)
{
    MirrorModeDisplay ret;
    ret.enabled = enabled;
    ret.primary = primary;
    ret.geometry = geometry;
    ret.scanoutSize = geometry.size();
    return ret;
}

// The layout of the machine the bug was reported on: a 1920x1080 primary sitting
// below a 3840x2160 secondary. Clients are told the screen is the primary alone.
void TestMirrorModeLayout::testRealMachineLayout()
{
    const QVector<MirrorModeDisplay> displays = {
        display(QRect(0, 1473, 1920, 1080), true, true),
        display(QRect(1920, 0, 3840, 2160)),
    };

    QCOMPARE(anchorIndex(displays), 0);
    QCOMPARE(normalize(displays), (QVector<QRect>{
                                      QRect(0, 0, 1920, 1080),
                                      QRect(1920, 0, 3840, 2160),
                                  }));
    QCOMPARE(clientGeometry(displays[0].scanoutSize), QRect(0, 0, 1920, 1080));

    // The work area the X server used to hand out for the same layout, before the
    // primary was moved to the origin: it belongs to the area no output scans out.
    QCOMPARE(clampToClientGeometry(QRect(0, 1473, 1920, 1034), clientGeometry(displays[0].scanoutSize)), QRect());
}

// The anchor keeps its size but loses its position, and the display that used to be
// to its left is packed to its right.
void TestMirrorModeLayout::testAnchorInTheMiddle()
{
    const QVector<MirrorModeDisplay> displays = {
        display(QRect(0, 0, 1920, 1080)),
        display(QRect(1920, 0, 2560, 1440), true, true),
        display(QRect(4480, 0, 1920, 1080)),
    };

    QCOMPARE(anchorIndex(displays), 1);
    QCOMPARE(normalize(displays), (QVector<QRect>{
                                      QRect(2560, 0, 1920, 1080),
                                      QRect(0, 0, 2560, 1440),
                                      QRect(4480, 0, 1920, 1080),
                                  }));
}

// A display that is not scanning out has no CRTC, so its position cannot overlap
// anything and is left for the snapshot to restore.
void TestMirrorModeLayout::testDisabledDisplayIsLeftAlone()
{
    const QVector<MirrorModeDisplay> displays = {
        display(QRect(0, 1473, 1920, 1080), true, true),
        display(QRect(1920, 0, 3840, 2160), false),
    };

    QCOMPARE(normalize(displays), (QVector<QRect>{
                                      QRect(0, 0, 1920, 1080),
                                      QRect(1920, 0, 3840, 2160),
                                  }));
}

// Without a primary the X server falls back to the first enabled display, and so does
// the model, so both agree on the anchor.
void TestMirrorModeLayout::testNoPrimaryFallsBackToFirstEnabled()
{
    const QVector<MirrorModeDisplay> displays = {
        display(QRect(0, 1080, 1280, 1024), false),
        display(QRect(1920, 0, 1920, 1080)),
        display(QRect(0, 0, 1280, 1024)),
    };

    QCOMPARE(anchorIndex(displays), 1);
    QCOMPARE(normalize(displays), (QVector<QRect>{
                                      QRect(0, 1080, 1280, 1024),
                                      QRect(0, 0, 1920, 1080),
                                      QRect(1920, 0, 1280, 1024),
                                  }));
}

void TestMirrorModeLayout::testNoEnabledDisplay()
{
    const QVector<MirrorModeDisplay> displays = {
        display(QRect(0, 1473, 1920, 1080), false, true),
        display(QRect(1920, 0, 3840, 2160), false),
    };

    QCOMPARE(anchorIndex(displays), -1);
    QCOMPARE(normalize(displays), (QVector<QRect>{
                                      QRect(0, 1473, 1920, 1080),
                                      QRect(1920, 0, 3840, 2160),
                                  }));
}

void TestMirrorModeLayout::testEmptyLayout()
{
    const QVector<MirrorModeDisplay> displays;

    QCOMPARE(anchorIndex(displays), -1);
    QCOMPARE(normalize(displays), QVector<QRect>());
}

void TestMirrorModeLayout::testClientGeometry()
{
    QCOMPARE(clientGeometry(QSize(1920, 1080)), QRect(0, 0, 1920, 1080));
    QCOMPARE(clientGeometry(QSize(2560, 1440)), QRect(0, 0, 2560, 1440));
    QCOMPARE(clientGeometry(QSize()), QRect());
    QCOMPARE(clientGeometry(QSize(0, 1080)), QRect());
}

// The areas the X server hands out: what KWin wrote (the screen minus the struts it
// kept) is returned untouched, everything else is clipped to the screen clients see.
void TestMirrorModeLayout::testClampToClientGeometry()
{
    const QRect screen(0, 0, 1920, 1080);

    // A bottom panel: the strut is already inside the screen.
    QCOMPARE(clampToClientGeometry(QRect(0, 0, 1920, 1034), screen), QRect(0, 0, 1920, 1034));
    // A left panel: non-zero origin, the extent must not grow.
    QCOMPARE(clampToClientGeometry(QRect(50, 0, 1870, 1080), screen), QRect(50, 0, 1870, 1080));
    // A top panel.
    QCOMPARE(clampToClientGeometry(QRect(0, 50, 1920, 1030), screen), QRect(0, 50, 1920, 1030));
    // The work area of the real, unnormalized layout: clipped to the screen.
    QCOMPARE(clampToClientGeometry(QRect(0, 0, 5760, 2507), screen), QRect(0, 0, 1920, 1080));
    // Off screen struts and negative origins.
    QCOMPARE(clampToClientGeometry(QRect(1920, 0, 3840, 2124), screen), QRect());
    QCOMPARE(clampToClientGeometry(QRect(-20, -20, 2000, 1120), screen), QRect(0, 0, 1920, 1080));
    // Nothing to clip.
    QCOMPARE(clampToClientGeometry(QRect(), screen), QRect());
    QCOMPARE(clampToClientGeometry(QRect(0, 0, 1920, 1034), QRect()), QRect());
}

QTEST_GUILESS_MAIN(TestMirrorModeLayout)

#include "test_mirror_mode_layout.moc"
