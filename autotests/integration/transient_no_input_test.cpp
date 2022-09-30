// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kwin_wayland_test.h"
#include "platform.h"
#include "abstract_client.h"
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

static const QString s_socketName = QStringLiteral("wayland_test_kwin_transient_no_input-0");

class TransientNoInputTest : public TestPrintAsanBase
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testTransientNoFocus();
};

void TransientNoInputTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
}

void TransientNoInputTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void TransientNoInputTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void TransientNoInputTest::testTransientNoFocus()
{
    using namespace KWayland::Client;

    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());

    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());

    QScopedPointer<ShellSurface> shellSurface(Test::createShellSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    // let's render
    Test::render(surface.data(), QSize(100, 50), Qt::blue);

    Test::flushWaylandConnection();
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *c = workspace()->activeClient();
    QVERIFY(c);
    QCOMPARE(clientAddedSpy.first().first().value<ShellClient*>(), c);

    // let's create a transient with no input
    QScopedPointer<Surface> transientSurface(Test::createSurface());
    QVERIFY(!transientSurface.isNull());
    QScopedPointer<ShellSurface> transientShellSurface(Test::createShellSurface(transientSurface.data()));
    QVERIFY(!transientShellSurface.isNull());
    transientShellSurface->setTransient(surface.data(), QPoint(10, 20), ShellSurface::TransientFlag::NoFocus);
    Test::flushWaylandConnection();
    // let's render
    Test::render(transientSurface.data(), QSize(200, 20), Qt::red);
    Test::flushWaylandConnection();
    QVERIFY(clientAddedSpy.wait());
    // get the latest ShellClient
    auto transientClient = clientAddedSpy.last().first().value<ShellClient*>();
    QVERIFY(transientClient != c);
    QCOMPARE(transientClient->geometry(), QRect(c->x() + 10, c->y() + 20, 200, 20));
    QVERIFY(transientClient->isTransient());
    QVERIFY(!transientClient->wantsInput());

    // workspace's active window should not have changed
    QCOMPARE(workspace()->activeClient(), c);
    testPrintlog();
}

}

WAYLANDTEST_MAIN(KWin::TransientNoInputTest)
#include "transient_no_input_test.moc"
