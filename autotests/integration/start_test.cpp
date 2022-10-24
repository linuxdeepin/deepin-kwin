// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwin_wayland_test.h"
#include "platform.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/surface.h>
#include "../testprintasanbase.h"
namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_start_test-0");

class StartTest : public TestPrintAsanBase
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testScreens();
    void testNoWindowsAtStart();
    void testCreateWindow();
    void testHideShowCursor();
};

void StartTest::initTestCase()
{
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QVERIFY(waylandServer()->hasGlobalShortcutSupport());
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
}

void StartTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void StartTest::testScreens()
{
    QCOMPARE(screens()->count(), 1);
    QCOMPARE(screens()->size(), QSize(1280, 1024));
    QCOMPARE(screens()->geometry(), QRect(0, 0, 1280, 1024));
}

void StartTest::testNoWindowsAtStart()
{
    QVERIFY(workspace()->clientList().isEmpty());
    QVERIFY(workspace()->desktopList().isEmpty());
    QVERIFY(workspace()->allClientList().isEmpty());
    QVERIFY(workspace()->deletedList().isEmpty());
    QVERIFY(workspace()->unmanagedList().isEmpty());
    QVERIFY(waylandServer()->clients().isEmpty());
}

void StartTest::testCreateWindow()
{
    // first we need to connect to the server
    using namespace KWayland::Client;
    QVERIFY(Test::setupWaylandConnection());

    QSignalSpy shellClientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(shellClientAddedSpy.isValid());
    QSignalSpy shellClientRemovedSpy(waylandServer(), &WaylandServer::shellClientRemoved);
    QVERIFY(shellClientRemovedSpy.isValid());

    {
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QSignalSpy surfaceRenderedSpy(surface.data(), &Surface::frameRendered);
    QVERIFY(surfaceRenderedSpy.isValid());

    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    Test::flushWaylandConnection();
    QVERIFY(waylandServer()->clients().isEmpty());
    // now dispatch should give us the client
    waylandServer()->dispatch();
    QTRY_COMPARE(waylandServer()->clients().count(), 1);
    // but still not yet in workspace
    QVERIFY(workspace()->allClientList().isEmpty());

    // icon geometry accesses windowManagementInterface which only exists after window became visible
    // verify that accessing doesnt't crash
    QVERIFY(waylandServer()->clients().first()->iconGeometry().isNull());

    // let's render
    Test::render(surface.data(), QSize(100, 50), Qt::blue);
    surface->commit();

    Test::flushWaylandConnection();
    QVERIFY(shellClientAddedSpy.wait());
    QCOMPARE(workspace()->allClientList().count(), 1);
    QCOMPARE(workspace()->allClientList().first(), waylandServer()->clients().first());
    QVERIFY(workspace()->activeClient());
    QCOMPARE(workspace()->activeClient()->pos(), QPoint(0, 0));
    QCOMPARE(workspace()->activeClient()->size(), QSize(100, 50));
    QCOMPARE(workspace()->activeClient()->geometry(), QRect(0, 0, 100, 50));

    // and kwin will render it
    QVERIFY(surfaceRenderedSpy.wait());
    }
    // this should tear down everything again
    QVERIFY(shellClientRemovedSpy.wait());
    QVERIFY(waylandServer()->clients().isEmpty());
}


void StartTest::testHideShowCursor()
{
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);
    kwinApp()->platform()->hideCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);


    kwinApp()->platform()->hideCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->hideCursor();
    kwinApp()->platform()->hideCursor();
    kwinApp()->platform()->hideCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);

    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);
    testPrintlog();
}

}

WAYLANDTEST_MAIN(KWin::StartTest)
#include "start_test.moc"
