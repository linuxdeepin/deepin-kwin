/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// client
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
// server
#include "wayland/display.h"
#include "wayland/fakeinput_interface.h"

// Use the generated C protocol header directly to bypass KWayland client
// library version checks. The system libKWaylandClient silently drops
// requests for protocol messages added in newer versions (touch_down v2,
// pointer_motion_absolute v3, keyboard_key v4).
#include "wayland-fake-input-client-protocol.h"

#include <linux/input.h>

using namespace KWaylandServer;

Q_DECLARE_METATYPE(Qt::MouseButton)

class FakeInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testAuthenticate();
    void testMotion();
    void testMotionAbsolute();
    void testPointerButtonQt_data();
    void testPointerButtonQt();
    void testPointerButtonLinux_data();
    void testPointerButtonLinux();
    void testAxis_data();
    void testAxis();
    void testTouch();
    void testKeyboardKeyLinux_data();
    void testKeyboardKeyLinux();

private:
    KWaylandServer::Display *m_display = nullptr;
    FakeInputInterface *m_fakeInputInterface = nullptr;
    FakeInputDevice *m_device = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    org_kde_kwin_fake_input *m_native = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-fake-input-0");

void FakeInputTest::init()
{
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_fakeInputInterface = new FakeInputInterface(m_display);
    QSignalSpy deviceCreatedSpy(m_fakeInputInterface, &FakeInputInterface::deviceCreated);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    m_queue->setup(m_connection);

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    const quint32 name = registry.interface(KWayland::Client::Registry::Interface::FakeInput).name;
    const quint32 version = std::max<quint32>(
        registry.interface(KWayland::Client::Registry::Interface::FakeInput).version, 4);

    // Bind directly via C API with version 4 to avoid KWayland client
    // library version gating that drops newer protocol requests.
    m_native = static_cast<org_kde_kwin_fake_input *>(wl_registry_bind(
        registry.registry(),
        name,
        &org_kde_kwin_fake_input_interface,
        version));
    QVERIFY(m_native);

    QVERIFY(deviceCreatedSpy.wait());
    m_device = deviceCreatedSpy.first().first().value<FakeInputDevice *>();
    QVERIFY(m_device);
}

void FakeInputTest::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    if (m_native) {
        org_kde_kwin_fake_input_destroy(m_native);
        m_native = nullptr;
    }
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    CLEANUP(m_display)
#undef CLEANUP

    // these are the children of the display
    m_fakeInputInterface = nullptr;
}

void FakeInputTest::testAuthenticate()
{
    // this test verifies that an authenticate request is passed to the Server
    QVERIFY(!m_device->isAuthenticated());
    QSignalSpy authenticationRequestedSpy(m_device, &FakeInputDevice::authenticationRequested);

    org_kde_kwin_fake_input_authenticate(m_native, "test-case", "to test");
    QVERIFY(authenticationRequestedSpy.wait());
    QCOMPARE(authenticationRequestedSpy.count(), 1);
    QCOMPARE(authenticationRequestedSpy.first().at(0).toString(), QStringLiteral("test-case"));
    QCOMPARE(authenticationRequestedSpy.first().at(1).toString(), QStringLiteral("to test"));
    m_device->setAuthentication(true);
    QVERIFY(m_device->isAuthenticated());
}

void FakeInputTest::testMotion()
{
    // this test verifies that motion is properly passed to the server
    QVERIFY(!m_device->isAuthenticated());
    QSignalSpy motionSpy(m_device, &FakeInputDevice::pointerMotionRequested);

    // without an authentication we shouldn't get the signals
    org_kde_kwin_fake_input_pointer_motion(m_native, wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(!motionSpy.wait(100));

    // now let's authenticate the interface
    m_device->setAuthentication(true);
    org_kde_kwin_fake_input_pointer_motion(m_native, wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(motionSpy.wait());
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(motionSpy.last().first().toPointF(), QPointF(1, 2));

    // just for the fun: once more
    org_kde_kwin_fake_input_pointer_motion(m_native, wl_fixed_from_double(0), wl_fixed_from_double(0));
    QVERIFY(motionSpy.wait());
    QCOMPARE(motionSpy.count(), 2);
    QCOMPARE(motionSpy.last().first().toPointF(), QPointF(0, 0));
}

void FakeInputTest::testMotionAbsolute()
{
    // this test verifies that motion is properly passed to the server
    QVERIFY(!m_device->isAuthenticated());
    QSignalSpy motionSpy(m_device, &FakeInputDevice::pointerMotionAbsoluteRequested);

    // without an authentication we shouldn't get the signals
    org_kde_kwin_fake_input_pointer_motion_absolute(m_native, wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(!motionSpy.wait(100));

    // now let's authenticate the interface
    m_device->setAuthentication(true);
    org_kde_kwin_fake_input_pointer_motion_absolute(m_native, wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(motionSpy.wait());
    QCOMPARE(motionSpy.count(), 1);
    QCOMPARE(motionSpy.last().first().toPointF(), QPointF(1, 2));

    // just for the fun: once more
    org_kde_kwin_fake_input_pointer_motion_absolute(m_native, wl_fixed_from_double(0), wl_fixed_from_double(0));
    QVERIFY(motionSpy.wait());
    QCOMPARE(motionSpy.count(), 2);
    QCOMPARE(motionSpy.last().first().toPointF(), QPointF(0, 0));
}

void FakeInputTest::testPointerButtonQt_data()
{
    QTest::addColumn<Qt::MouseButton>("qtButton");
    QTest::addColumn<quint32>("linuxButton");

    QTest::newRow("left") << Qt::LeftButton << quint32(BTN_LEFT);
    QTest::newRow("right") << Qt::RightButton << quint32(BTN_RIGHT);
    QTest::newRow("middle") << Qt::MiddleButton << quint32(BTN_MIDDLE);
}

void FakeInputTest::testPointerButtonQt()
{
    // this test verifies that pointer button events are properly passed to the server with Qt button codes
    QVERIFY(!m_device->isAuthenticated());
    QSignalSpy pressedSpy(m_device, &FakeInputDevice::pointerButtonPressRequested);
    QSignalSpy releasedSpy(m_device, &FakeInputDevice::pointerButtonReleaseRequested);

    // without an authentication we shouldn't get the signals
    QFETCH(Qt::MouseButton, qtButton);
    QFETCH(quint32, linuxButton);
    Q_UNUSED(qtButton);
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_RELEASED);
    QVERIFY(!pressedSpy.wait(100));
    QVERIFY(pressedSpy.isEmpty());
    QVERIFY(releasedSpy.isEmpty());

    // now authenticate
    m_device->setAuthentication(true);
    // now our click should work
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_RELEASED);
    QVERIFY(releasedSpy.wait());
    QCOMPARE(pressedSpy.count(), 1);
    QCOMPARE(releasedSpy.count(), 1);
    QTEST(pressedSpy.last().first().value<quint32>(), "linuxButton");
    QTEST(releasedSpy.last().first().value<quint32>(), "linuxButton");

    // and a press/release "manually"
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
    QVERIFY(pressedSpy.wait());
    QCOMPARE(pressedSpy.count(), 2);
    QCOMPARE(releasedSpy.count(), 1);
    QTEST(pressedSpy.last().first().value<quint32>(), "linuxButton");
    // and release
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_RELEASED);
    QVERIFY(releasedSpy.wait());
    QCOMPARE(pressedSpy.count(), 2);
    QCOMPARE(releasedSpy.count(), 2);
    QTEST(releasedSpy.last().first().value<quint32>(), "linuxButton");
}

void FakeInputTest::testPointerButtonLinux_data()
{
    QTest::addColumn<quint32>("linuxButton");

    QTest::newRow("left") << quint32(BTN_LEFT);
    QTest::newRow("right") << quint32(BTN_RIGHT);
    QTest::newRow("middle") << quint32(BTN_MIDDLE);
    QTest::newRow("side") << quint32(BTN_SIDE);
    QTest::newRow("extra") << quint32(BTN_EXTRA);
    QTest::newRow("forward") << quint32(BTN_FORWARD);
    QTest::newRow("back") << quint32(BTN_BACK);
    QTest::newRow("task") << quint32(BTN_TASK);
}

void FakeInputTest::testPointerButtonLinux()
{
    // this test verifies that pointer button events are properly passed to the server with Linux button codes
    QVERIFY(!m_device->isAuthenticated());
    QSignalSpy pressedSpy(m_device, &FakeInputDevice::pointerButtonPressRequested);
    QSignalSpy releasedSpy(m_device, &FakeInputDevice::pointerButtonReleaseRequested);

    // without an authentication we shouldn't get the signals
    QFETCH(quint32, linuxButton);
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_RELEASED);
    QVERIFY(!pressedSpy.wait(100));
    QVERIFY(pressedSpy.isEmpty());
    QVERIFY(releasedSpy.isEmpty());

    // now authenticate
    m_device->setAuthentication(true);
    // now our click should work
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_RELEASED);
    QVERIFY(releasedSpy.wait());
    QCOMPARE(pressedSpy.count(), 1);
    QCOMPARE(releasedSpy.count(), 1);
    QTEST(pressedSpy.last().first().value<quint32>(), "linuxButton");
    QTEST(releasedSpy.last().first().value<quint32>(), "linuxButton");

    // and a press/release "manually"
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_PRESSED);
    QVERIFY(pressedSpy.wait());
    QCOMPARE(pressedSpy.count(), 2);
    QCOMPARE(releasedSpy.count(), 1);
    QTEST(pressedSpy.last().first().value<quint32>(), "linuxButton");
    // and release
    org_kde_kwin_fake_input_button(m_native, linuxButton, WL_POINTER_BUTTON_STATE_RELEASED);
    QVERIFY(releasedSpy.wait());
    QCOMPARE(pressedSpy.count(), 2);
    QCOMPARE(releasedSpy.count(), 2);
    QTEST(releasedSpy.last().first().value<quint32>(), "linuxButton");
}

void FakeInputTest::testAxis_data()
{
    QTest::addColumn<Qt::Orientation>("orientation");
    QTest::addColumn<qreal>("delta");

    QTest::newRow("horizontal/1") << Qt::Horizontal << 1.0;
    QTest::newRow("horizontal/-2") << Qt::Horizontal << -2.0;
    QTest::newRow("vertical/10") << Qt::Vertical << 10.0;
    QTest::newRow("vertical/-20") << Qt::Vertical << -22.0;
}

void FakeInputTest::testAxis()
{
    // this test verifies that pointer axis events are properly passed to the server
    QVERIFY(!m_device->isAuthenticated());
    QSignalSpy axisSpy(m_device, &FakeInputDevice::pointerAxisRequested);

    QFETCH(Qt::Orientation, orientation);
    QFETCH(qreal, delta);
    const uint32_t wlAxis = (orientation == Qt::Horizontal) ? WL_POINTER_AXIS_HORIZONTAL_SCROLL : WL_POINTER_AXIS_VERTICAL_SCROLL;
    // without an authentication we shouldn't get the signals
    org_kde_kwin_fake_input_axis(m_native, wlAxis, wl_fixed_from_double(delta));
    QVERIFY(!axisSpy.wait(100));

    // now authenticate
    m_device->setAuthentication(true);

    // now we can properly test
    org_kde_kwin_fake_input_axis(m_native, wlAxis, wl_fixed_from_double(delta));
    QVERIFY(axisSpy.wait());
    QCOMPARE(axisSpy.count(), 1);
    QCOMPARE(axisSpy.first().at(0).value<Qt::Orientation>(), orientation);
    QCOMPARE(axisSpy.first().at(1).value<qreal>(), delta);
}

void FakeInputTest::testTouch()
{
    QVERIFY(!m_device->isAuthenticated());
    QSignalSpy touchDownSpy(m_device, &FakeInputDevice::touchDownRequested);
    QSignalSpy touchMotionSpy(m_device, &FakeInputDevice::touchMotionRequested);
    QSignalSpy touchUpSpy(m_device, &FakeInputDevice::touchUpRequested);
    QSignalSpy touchFrameSpy(m_device, &FakeInputDevice::touchFrameRequested);
    QSignalSpy touchCancelSpy(m_device, &FakeInputDevice::touchCancelRequested);

    // without an authentication we shouldn't get the signals
    org_kde_kwin_fake_input_touch_down(m_native, 0, wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(!touchDownSpy.wait(100));

    org_kde_kwin_fake_input_touch_motion(m_native, 0, wl_fixed_from_double(3), wl_fixed_from_double(4));
    QVERIFY(!touchMotionSpy.wait(100));

    org_kde_kwin_fake_input_touch_up(m_native, 0);
    QVERIFY(!touchUpSpy.wait(100));

    org_kde_kwin_fake_input_touch_down(m_native, 1, wl_fixed_from_double(5), wl_fixed_from_double(6));
    QVERIFY(!touchDownSpy.wait(100));

    org_kde_kwin_fake_input_touch_frame(m_native);
    QVERIFY(!touchFrameSpy.wait(100));

    org_kde_kwin_fake_input_touch_cancel(m_native);
    QVERIFY(!touchCancelSpy.wait(100));

    // now let's authenticate the interface
    m_device->setAuthentication(true);
    org_kde_kwin_fake_input_touch_down(m_native, 0, wl_fixed_from_double(1), wl_fixed_from_double(2));
    QVERIFY(touchDownSpy.wait());
    QCOMPARE(touchDownSpy.count(), 1);
    QCOMPARE(touchDownSpy.last().at(0).value<quint32>(), quint32(0));
    QCOMPARE(touchDownSpy.last().at(1).toPointF(), QPointF(1, 2));

    // Same id should not trigger another touchDown.
    org_kde_kwin_fake_input_touch_down(m_native, 0, wl_fixed_from_double(5), wl_fixed_from_double(6));
    QVERIFY(!touchDownSpy.wait(100));

    org_kde_kwin_fake_input_touch_motion(m_native, 0, wl_fixed_from_double(3), wl_fixed_from_double(4));
    QVERIFY(touchMotionSpy.wait());
    QCOMPARE(touchMotionSpy.count(), 1);
    QCOMPARE(touchMotionSpy.last().at(0).value<quint32>(), quint32(0));
    QCOMPARE(touchMotionSpy.last().at(1).toPointF(), QPointF(3, 4));

    // touchMotion with bogus id should not trigger signal.
    org_kde_kwin_fake_input_touch_motion(m_native, 1, wl_fixed_from_double(3), wl_fixed_from_double(4));
    QVERIFY(!touchMotionSpy.wait(100));

    org_kde_kwin_fake_input_touch_up(m_native, 0);
    QVERIFY(touchUpSpy.wait());
    QCOMPARE(touchUpSpy.count(), 1);
    QCOMPARE(touchUpSpy.last().at(0).value<quint32>(), quint32(0));

    // touchUp with bogus id should not trigger signal.
    org_kde_kwin_fake_input_touch_up(m_native, 1);
    QVERIFY(!touchUpSpy.wait(100));

    org_kde_kwin_fake_input_touch_down(m_native, 1, wl_fixed_from_double(5), wl_fixed_from_double(6));
    QVERIFY(touchDownSpy.wait());
    QCOMPARE(touchDownSpy.count(), 2);
    QCOMPARE(touchDownSpy.last().at(0).value<quint32>(), quint32(1));
    QCOMPARE(touchDownSpy.last().at(1).toPointF(), QPointF(5, 6));

    org_kde_kwin_fake_input_touch_frame(m_native);
    QVERIFY(touchFrameSpy.wait());
    QCOMPARE(touchFrameSpy.count(), 1);

    org_kde_kwin_fake_input_touch_cancel(m_native);
    QVERIFY(touchCancelSpy.wait());
    QCOMPARE(touchCancelSpy.count(), 1);
}

void FakeInputTest::testKeyboardKeyLinux_data()
{
    QTest::addColumn<quint32>("linuxKey");

    QTest::newRow("A") << quint32(KEY_A);
    QTest::newRow("S") << quint32(KEY_S);
    QTest::newRow("D") << quint32(KEY_D);
    QTest::newRow("F") << quint32(KEY_F);
}

void FakeInputTest::testKeyboardKeyLinux()
{
    // this test verifies that keyboard key events are properly passed to the server with Linux key codes
    QVERIFY(!m_device->isAuthenticated());
    QSignalSpy pressedSpy(m_device, &FakeInputDevice::keyboardKeyPressRequested);
    QSignalSpy releasedSpy(m_device, &FakeInputDevice::keyboardKeyReleaseRequested);

    // without an authentication we shouldn't get the signals
    QFETCH(quint32, linuxKey);
    org_kde_kwin_fake_input_keyboard_key(m_native, linuxKey, WL_KEYBOARD_KEY_STATE_PRESSED);
    org_kde_kwin_fake_input_keyboard_key(m_native, linuxKey, WL_KEYBOARD_KEY_STATE_RELEASED);
    QVERIFY(!pressedSpy.wait(100));
    QVERIFY(pressedSpy.isEmpty());
    QVERIFY(releasedSpy.isEmpty());

    // now authenticate
    m_device->setAuthentication(true);
    // now our click should work
    org_kde_kwin_fake_input_keyboard_key(m_native, linuxKey, WL_KEYBOARD_KEY_STATE_PRESSED);
    org_kde_kwin_fake_input_keyboard_key(m_native, linuxKey, WL_KEYBOARD_KEY_STATE_RELEASED);
    QVERIFY(releasedSpy.wait());
    QCOMPARE(pressedSpy.count(), 1);
    QCOMPARE(releasedSpy.count(), 1);
    QTEST(pressedSpy.last().first().value<quint32>(), "linuxKey");
    QTEST(releasedSpy.last().first().value<quint32>(), "linuxKey");

    // and a press/release "manually"
    org_kde_kwin_fake_input_keyboard_key(m_native, linuxKey, WL_KEYBOARD_KEY_STATE_PRESSED);
    QVERIFY(pressedSpy.wait());
    QCOMPARE(pressedSpy.count(), 2);
    QCOMPARE(releasedSpy.count(), 1);
    QTEST(pressedSpy.last().first().value<quint32>(), "linuxKey");
    // and release
    org_kde_kwin_fake_input_keyboard_key(m_native, linuxKey, WL_KEYBOARD_KEY_STATE_RELEASED);
    QVERIFY(releasedSpy.wait());
    QCOMPARE(pressedSpy.count(), 2);
    QCOMPARE(releasedSpy.count(), 2);
    QTEST(releasedSpy.last().first().value<quint32>(), "linuxKey");
}

QTEST_GUILESS_MAIN(FakeInputTest)
#include "test_fake_input.moc"
