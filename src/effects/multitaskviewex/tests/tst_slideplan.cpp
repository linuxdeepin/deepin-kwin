/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "model/slideplan.h"

#include <QtTest>

using namespace KWin;

namespace
{

const QString leftMonitor = QStringLiteral("DP-1");
const QString rightMonitor = QStringLiteral("HDMI-1");
const QString fallback = QStringLiteral("/usr/share/wallpapers/deepin/desktop.jpg");

QVector<SlideScreenInput> twoScreens()
{
    return {
        SlideScreenInput{leftMonitor, 1920.0},
        SlideScreenInput{rightMonitor, 2560.0},
    };
}

WallpaperMap perMonitorWallpapers()
{
    WallpaperMap cache;
    cache[1][leftMonitor] = QStringLiteral("file:///left-workspace1.jpg");
    cache[1][rightMonitor] = QStringLiteral("file:///right-workspace1.jpg");
    cache[2][leftMonitor] = QStringLiteral("file:///left-workspace2.jpg");
    cache[2][rightMonitor] = QStringLiteral("file:///right-workspace2.jpg");
    return cache;
}

} // namespace

class TestSlidePlan : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void wallpapersAreResolvedPerScreen();
    void offsetsUseEachScreenWidth();
    void screensWithoutWallpaperUseFallbackOnly();
    void missingDesktopUsesFallbackOnEveryScreen();
};

void TestSlidePlan::wallpapersAreResolvedPerScreen()
{
    const QVector<ScreenSlide> slides = composeSlide(twoScreens(), 1, perMonitorWallpapers(), 1, 2,
                                                     fallback);

    QCOMPARE(slides.size(), 2);
    QCOMPARE(slides.at(0).fromWallpaper, QStringLiteral("file:///left-workspace1.jpg"));
    QCOMPARE(slides.at(0).toWallpaper, QStringLiteral("file:///left-workspace2.jpg"));
    QCOMPARE(slides.at(1).fromWallpaper, QStringLiteral("file:///right-workspace1.jpg"));
    QCOMPARE(slides.at(1).toWallpaper, QStringLiteral("file:///right-workspace2.jpg"));

    // A screen must never show the wallpaper cached for another screen.
    QVERIFY(slides.at(0).fromWallpaper != slides.at(1).fromWallpaper);
    QVERIFY(slides.at(0).toWallpaper != slides.at(1).toWallpaper);
}

void TestSlidePlan::offsetsUseEachScreenWidth()
{
    const QVector<ScreenSlide> forward = composeSlide(twoScreens(), 1, perMonitorWallpapers(), 1, 2,
                                                      fallback);
    QCOMPARE(forward.at(0).outgoingOffset, 1920.0);
    QCOMPARE(forward.at(1).outgoingOffset, 2560.0);

    const QVector<ScreenSlide> backward = composeSlide(twoScreens(), -1, perMonitorWallpapers(), 2, 1,
                                                       fallback);
    QCOMPARE(backward.at(0).outgoingOffset, -1920.0);
    QCOMPARE(backward.at(1).outgoingOffset, -2560.0);
    QCOMPARE(backward.at(1).fromWallpaper, QStringLiteral("file:///right-workspace2.jpg"));
    QCOMPARE(backward.at(1).toWallpaper, QStringLiteral("file:///right-workspace1.jpg"));
}

void TestSlidePlan::screensWithoutWallpaperUseFallbackOnly()
{
    WallpaperMap cache = perMonitorWallpapers();
    cache[1].remove(rightMonitor);
    cache[2].remove(rightMonitor);

    const QVector<ScreenSlide> slides = composeSlide(twoScreens(), 1, cache, 1, 2, fallback);

    QCOMPARE(slides.at(0).fromWallpaper, QStringLiteral("file:///left-workspace1.jpg"));
    QCOMPARE(slides.at(0).toWallpaper, QStringLiteral("file:///left-workspace2.jpg"));
    QCOMPARE(slides.at(1).fromWallpaper, fallback);
    QCOMPARE(slides.at(1).toWallpaper, fallback);
}

void TestSlidePlan::missingDesktopUsesFallbackOnEveryScreen()
{
    const QVector<ScreenSlide> slides = composeSlide(twoScreens(), 1, perMonitorWallpapers(), 3, 4,
                                                     fallback);

    for (const ScreenSlide &slide : slides) {
        QCOMPARE(slide.fromWallpaper, fallback);
        QCOMPARE(slide.toWallpaper, fallback);
    }
}

QTEST_MAIN(TestSlidePlan)
#include "tst_slideplan.moc"
