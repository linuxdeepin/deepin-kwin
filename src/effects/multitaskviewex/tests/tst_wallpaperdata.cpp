/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "model/wallpaperdata.h"

#include <QtTest>

using namespace KWin;

class TestWallpaperData : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void compileTimeEndpointsMatchBuild();
    void pickNewWorkspaceWallpaperPrefersPreview();
};

void TestWallpaperData::compileTimeEndpointsMatchBuild()
{
#ifdef BUILD_ON_V25
    QCOMPARE(QLatin1String(WallpaperDBus::appearanceService), QStringLiteral("org.deepin.dde.Appearance1"));
    QCOMPARE(QLatin1String(WallpaperDBus::appearanceObject), QStringLiteral("/org/deepin/dde/Appearance1"));
    QCOMPARE(QLatin1String(WallpaperDBus::appearanceInterface), QStringLiteral("org.deepin.dde.Appearance1"));
#else
    QCOMPARE(QLatin1String(WallpaperDBus::appearanceService), QStringLiteral("com.deepin.daemon.Appearance"));
    QCOMPARE(QLatin1String(WallpaperDBus::appearanceObject), QStringLiteral("/com/deepin/daemon/Appearance"));
    QCOMPARE(QLatin1String(WallpaperDBus::appearanceInterface), QStringLiteral("com.deepin.daemon.Appearance"));
#endif
    QCOMPARE(QLatin1String(fallback_background_name),
             QStringLiteral("file:///usr/share/wallpapers/deepin/desktop.jpg"));
}

void TestWallpaperData::pickNewWorkspaceWallpaperPrefersPreview()
{
    QCOMPARE(pickNewWorkspaceWallpaper(QStringLiteral("file:///preview.jpg"),
                                       QStringLiteral("file:///current.jpg")),
             QStringLiteral("file:///preview.jpg"));
    QCOMPARE(pickNewWorkspaceWallpaper(QString(),
                                       QStringLiteral("file:///current.jpg")),
             QStringLiteral("file:///current.jpg"));
    QCOMPARE(pickNewWorkspaceWallpaper(QString(), QString()), QString());
}

QTEST_MAIN(TestWallpaperData)
#include "tst_wallpaperdata.moc"
