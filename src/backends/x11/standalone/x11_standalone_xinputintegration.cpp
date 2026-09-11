/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "x11_standalone_xinputintegration.h"
#include "core/outputbackend.h"
#include "gestures.h"
#include "keyboard_input.h"
#include "main.h"
#include "pointer_input.h"
#include "screenedge.h"
#include "x11_standalone_cursor.h"
#include "x11_standalone_logging.h"

#include "../common/ge_event_mem_mover.h"

#include "input.h"
#include "modifier_only_shortcuts.h"
#include "workspace.h"
#include "x11eventfilter.h"
#include <kwinglobals.h>
#include "effects.h"

#include <X11/extensions/XI2proto.h>
#include <X11/extensions/XInput2.h>

#include <linux/input.h>

#include <QMatrix4x4>

namespace KWin
{

static inline qreal fixed1616ToReal(FP1616 val)
{
    return (val)*1.0 / (1 << 16);
}

class XInputEventFilter : public X11EventFilter
{
public:
    XInputEventFilter(int xi_opcode)
        : X11EventFilter(XCB_GE_GENERIC, xi_opcode, QVector<int>{XI_RawMotion, XI_RawButtonPress, XI_RawButtonRelease, XI_RawKeyPress, XI_RawKeyRelease, XI_TouchBegin, XI_TouchUpdate, XI_TouchOwnership, XI_TouchEnd, XI_PropertyEvent, XI_HierarchyChanged})
    {
    }
    ~XInputEventFilter() override = default;

    bool event(xcb_generic_event_t *event) override
    {
        GeEventMemMover ge(event);
        switch (ge->event_type) {
        case XI_RawKeyPress: {
            auto re = reinterpret_cast<xXIRawEvent *>(event);
            input()->keyboard()->processKey(re->detail - 8, InputRedirection::KeyboardKeyPressed, std::chrono::milliseconds(re->time));
            break;
        }
        case XI_RawKeyRelease: {
            auto re = reinterpret_cast<xXIRawEvent *>(event);
            input()->keyboard()->processKey(re->detail - 8, InputRedirection::KeyboardKeyReleased, std::chrono::milliseconds(re->time));
            break;
        }
        case XI_RawButtonPress: {
            auto e = reinterpret_cast<xXIRawEvent *>(event);
            switch (e->detail) {
            // TODO: this currently ignores left handed settings, for current usage not needed
            // if we want to use also for global mouse shortcuts, this needs to reflect state correctly
            case XCB_BUTTON_INDEX_1:
                input()->pointer()->processButton(BTN_LEFT, InputRedirection::PointerButtonPressed, std::chrono::milliseconds(e->time));
                break;
            case XCB_BUTTON_INDEX_2:
                input()->pointer()->processButton(BTN_MIDDLE, InputRedirection::PointerButtonPressed, std::chrono::milliseconds(e->time));
                break;
            case XCB_BUTTON_INDEX_3:
                input()->pointer()->processButton(BTN_RIGHT, InputRedirection::PointerButtonPressed, std::chrono::milliseconds(e->time));
                break;
            case XCB_BUTTON_INDEX_4:
            case XCB_BUTTON_INDEX_5:
                // vertical axis, ignore on press
                break;
                // TODO: further buttons, horizontal scrolling?
            }
        }
            if (m_x11Cursor) {
                m_x11Cursor->schedulePoll();
            }
            break;
        case XI_RawButtonRelease: {
            auto e = reinterpret_cast<xXIRawEvent *>(event);
            switch (e->detail) {
            // TODO: this currently ignores left handed settings, for current usage not needed
            // if we want to use also for global mouse shortcuts, this needs to reflect state correctly
            case XCB_BUTTON_INDEX_1:
                input()->pointer()->processButton(BTN_LEFT, InputRedirection::PointerButtonReleased, std::chrono::milliseconds(e->time));
                break;
            case XCB_BUTTON_INDEX_2:
                input()->pointer()->processButton(BTN_MIDDLE, InputRedirection::PointerButtonReleased, std::chrono::milliseconds(e->time));
                break;
            case XCB_BUTTON_INDEX_3:
                input()->pointer()->processButton(BTN_RIGHT, InputRedirection::PointerButtonReleased, std::chrono::milliseconds(e->time));
                break;
            case XCB_BUTTON_INDEX_4:
                input()->pointer()->processAxis(InputRedirection::PointerAxisVertical, 120, 1, InputRedirection::PointerAxisSourceWheel, std::chrono::milliseconds(e->time));
                break;
            case XCB_BUTTON_INDEX_5:
                input()->pointer()->processAxis(InputRedirection::PointerAxisVertical, -120, 1, InputRedirection::PointerAxisSourceWheel, std::chrono::milliseconds(e->time));
                break;
                // TODO: further buttons, horizontal scrolling?
            }
        }
            if (m_x11Cursor) {
                m_x11Cursor->schedulePoll();
            }
            break;
        case XI_TouchBegin: {
            auto e = reinterpret_cast<xXIDeviceEvent *>(event);
            m_lastTouchPositions.insert(e->detail, QPointF(fixed1616ToReal(e->event_x), fixed1616ToReal(e->event_y)));
            break;
        }
        case XI_TouchUpdate: {
            auto e = reinterpret_cast<xXIDeviceEvent *>(event);
            const QPointF touchPosition = QPointF(fixed1616ToReal(e->event_x), fixed1616ToReal(e->event_y));
            if (e->detail == m_trackingTouchId) {
                const auto last = m_lastTouchPositions.value(e->detail);
                workspace()->screenEdges()->gestureRecognizer()->updateSwipeGesture(touchPosition - last);
            }
            m_lastTouchPositions.insert(e->detail, touchPosition);
            break;
        }
        case XI_TouchEnd: {
            auto e = reinterpret_cast<xXIDeviceEvent *>(event);
            if (e->detail == m_trackingTouchId) {
                workspace()->screenEdges()->gestureRecognizer()->endSwipeGesture();
            }
            m_lastTouchPositions.remove(e->detail);
            m_trackingTouchId = 0;
            break;
        }
        case XI_TouchOwnership: {
            auto e = reinterpret_cast<xXITouchOwnershipEvent *>(event);
            auto it = m_lastTouchPositions.constFind(e->touchid);
            if (it == m_lastTouchPositions.constEnd()) {
                XIAllowTouchEvents(display(), e->deviceid, e->sourceid, e->touchid, XIRejectTouch);
            } else {
                if (workspace()->screenEdges()->gestureRecognizer()->startSwipeGesture(it.value()) > 0) {
                    m_trackingTouchId = e->touchid;
                }
                XIAllowTouchEvents(display(), e->deviceid, e->sourceid, e->touchid, m_trackingTouchId == e->touchid ? XIAcceptTouch : XIRejectTouch);
            }
            break;
        }
        case XI_PropertyEvent: {
            auto pe = reinterpret_cast<xXIPropertyEvent *>(event);
            if (m_integration && m_ctmAtom != 0 && pe->property == static_cast<Atom>(m_ctmAtom)) {
                m_integration->handleCtmPropertyEvent(pe->deviceid);
            }
            break;
        }
        case XI_HierarchyChanged: {
            auto he = reinterpret_cast<xXIHierarchyEvent *>(event);
            if (m_integration && he->num_info > 0) {
                // The xXIHierarchyInfo array is stored directly after the
                // fixed part of the event structure.
                const auto *info = reinterpret_cast<const xXIHierarchyInfo *>(he + 1);
                for (int i = 0; i < he->num_info; ++i) {
                    if (info[i].flags & (XIMasterRemoved | XISlaveRemoved)) {
                        m_integration->handleDeviceRemoved(info[i].deviceid);
                    }
                }
            }
            break;
        }
        default:
            if (m_x11Cursor) {
                m_x11Cursor->schedulePoll();
            }
            break;
        }
        return false;
    }

    void setCursor(const QPointer<X11Cursor> &cursor)
    {
        m_x11Cursor = cursor;
    }
    void setIntegration(XInputIntegration *integration)
    {
        m_integration = integration;
    }
    void setCtmAtom(unsigned long atom)
    {
        m_ctmAtom = atom;
    }
    void setDisplay(Display *display)
    {
        m_x11Display = display;
    }

private:
    Display *display() const
    {
        return m_x11Display;
    }

    QPointer<X11Cursor> m_x11Cursor;
    XInputIntegration *m_integration = nullptr;
    unsigned long m_ctmAtom = 0;
    Display *m_x11Display = nullptr;
    uint32_t m_trackingTouchId = 0;
    QHash<uint32_t, QPointF> m_lastTouchPositions;
};

class XKeyPressReleaseEventFilter : public X11EventFilter
{
public:
    XKeyPressReleaseEventFilter(uint32_t type)
        : X11EventFilter(type)
    {
    }
    ~XKeyPressReleaseEventFilter() override = default;

    bool event(xcb_generic_event_t *event) override
    {
        if (effects && static_cast<EffectsHandlerImpl *>(effects)->hasKeyboardGrab()) {
            return false;
        }
        xcb_key_press_event_t *ke = reinterpret_cast<xcb_key_press_event_t *>(event);
        if (ke->event == ke->root) {
            const uint8_t eventType = event->response_type & ~0x80;
            if (eventType == XCB_KEY_PRESS) {
                input()->keyboard()->processKey(ke->detail - 8, InputRedirection::KeyboardKeyPressed, std::chrono::milliseconds(ke->time));
            } else {
                input()->keyboard()->processKey(ke->detail - 8, InputRedirection::KeyboardKeyReleased, std::chrono::milliseconds(ke->time));
            }
        }
        return false;
    }
};

XInputIntegration::XInputIntegration(Display *display, QObject *parent)
    : QObject(parent)
    , m_x11Display(display)
{
}

XInputIntegration::~XInputIntegration() = default;

void XInputIntegration::init()
{
    Display *dpy = display();
    int xi_opcode, event, error;
    // init XInput extension
    if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error)) {
        qCDebug(KWIN_X11STANDALONE) << "XInputExtension not present";
        return;
    }

    // verify that the XInput extension is at at least version 2.0
    int major = 2, minor = 2;
    int result = XIQueryVersion(dpy, &major, &minor);
    if (result != Success) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to init XInput 2.2, trying 2.0";
        minor = 0;
        if (XIQueryVersion(dpy, &major, &minor) != Success) {
            qCDebug(KWIN_X11STANDALONE) << "Failed to init XInput";
            return;
        }
    }
    m_hasXInput = true;
    KWin::Workspace::setDelayedRaisingClientMode(KWin::Workspace::DRCM_XInputDriven);
    m_xiOpcode = xi_opcode;
    m_majorVersion = major;
    m_minorVersion = minor;
    qCDebug(KWIN_X11STANDALONE) << "Has XInput support" << m_majorVersion << "." << m_minorVersion;
}

void XInputIntegration::setCursor(X11Cursor *cursor)
{
    m_x11Cursor = QPointer<X11Cursor>(cursor);
}

void XInputIntegration::startListening()
{
    // this assumes KWin is the only one setting events on the root window
    // given Qt's source code this seems to be true. If it breaks, we need to change
    XIEventMask evmasks[1];
    unsigned char mask1[XIMaskLen(XI_LASTEVENT)];

    memset(mask1, 0, sizeof(mask1));

    XISetMask(mask1, XI_RawMotion);
    XISetMask(mask1, XI_RawButtonPress);
    XISetMask(mask1, XI_RawButtonRelease);
    if (m_majorVersion >= 2 && m_minorVersion >= 1) {
        // we need to listen to all events, which is only available with XInput 2.1
        XISetMask(mask1, XI_RawKeyPress);
        XISetMask(mask1, XI_RawKeyRelease);
    }
    if (m_majorVersion >= 2 && m_minorVersion >= 2) {
        // touch events since 2.2
        XISetMask(mask1, XI_TouchBegin);
        XISetMask(mask1, XI_TouchUpdate);
        XISetMask(mask1, XI_TouchOwnership);
        XISetMask(mask1, XI_TouchEnd);
    }

    evmasks[0].deviceid = XIAllMasterDevices;
    evmasks[0].mask_len = sizeof(mask1);
    evmasks[0].mask = mask1;
    XISelectEvents(display(), rootWindow(), evmasks, 1);

    // Select XI_PropertyEvent on all devices to monitor external CTM changes.
    // CTM lives on slave/hardware devices, not master devices, so we must
    // select on XIAllDevices rather than XIAllMasterDevices.
    if (m_ctmAtom == 0) {
        m_ctmAtom = XInternAtom(display(), "Coordinate Transformation Matrix", False);
    }
    if (m_ctmAtom != 0) {
        XIEventMask propMask[1];
        unsigned char propMaskBits[XIMaskLen(XI_LASTEVENT)];
        memset(propMaskBits, 0, sizeof(propMaskBits));
        XISetMask(propMaskBits, XI_PropertyEvent);
        // Device add/remove notifications are needed to drop per-device CTM
        // state; without selecting the event the handler below never runs.
        XISetMask(propMaskBits, XI_HierarchyChanged);
        propMask[0].deviceid = XIAllDevices;
        propMask[0].mask_len = sizeof(propMaskBits);
        propMask[0].mask = propMaskBits;
        XISelectEvents(display(), rootWindow(), propMask, 1);
    }

    m_xiEventFilter = std::make_unique<XInputEventFilter>(m_xiOpcode);
    m_xiEventFilter->setCursor(m_x11Cursor);
    m_xiEventFilter->setIntegration(this);
    m_xiEventFilter->setCtmAtom(m_ctmAtom);
    m_xiEventFilter->setDisplay(display());
    m_keyPressFilter = std::make_unique<XKeyPressReleaseEventFilter>(XCB_KEY_PRESS);
    m_keyReleaseFilter = std::make_unique<XKeyPressReleaseEventFilter>(XCB_KEY_RELEASE);

    // install the input event spies also relevant for X11 platform
    input()->installInputEventSpy(new ModifierOnlyShortcuts);
}

void XInputIntegration::updateDeviceCtm(int deviceId, const QMatrix4x4 &transform, unsigned long ctmAtom)
{
    Display *dpy = display();

    Atom typeReturn;
    int formatReturn;
    unsigned long numItemsReturn;
    unsigned long bytesAfterReturn;
    unsigned char *data = nullptr;

    // The CTM property is specified as a FLOAT array. Request that type
    // explicitly so a property carrying another 32-bit type (e.g. INTEGER) is
    // not reinterpreted as float data.
    const unsigned long expectedType = floatAtom();
    if (expectedType == 0) {
        return;
    }
    if (XIGetProperty(dpy, deviceId, ctmAtom, 0, 9, False,
                      expectedType, &typeReturn, &formatReturn,
                      &numItemsReturn, &bytesAfterReturn, &data) != Success) {
        return;
    }

    // Skip devices without a well-formed FLOAT CTM property.
    if (typeReturn == None || typeReturn != expectedType || numItemsReturn != 9 || formatReturn != 32) {
        if (data) {
            XFree(data);
        }
        return;
    }

    // Cache the original CTM on first access for each device.
    if (!m_originalCtms.contains(deviceId)) {
        QVector<float> originalCtm(9);
        const float *ctm = reinterpret_cast<const float *>(data);
        for (int j = 0; j < 9; j++) {
            originalCtm[j] = ctm[j];
        }
        m_originalCtms[deviceId] = originalCtm;
    }

    const QVector<float> &original = m_originalCtms[deviceId];

    float newCtm[9];
    if (transform.isIdentity()) {
        // Restore the exact original CTM.
        for (int j = 0; j < 9; j++) {
            newCtm[j] = original[j];
        }
    } else {
        // CTM and QMatrix4x4 both use v' = M * v (column-vector) convention.
        // CTM 3x3: [c1 c2 c3; c4 c5 c6; 0 0 1] → QMatrix4x4 with m03=c3, m13=c6.
        QMatrix4x4 origMatrix(
            original[0], original[1], 0, original[2],
            original[3], original[4], 0, original[5],
            0, 0, 1, 0,
            0, 0, 0, 1);

        // Compose: new_ctm = inverse_zoom * original_ctm.
        // CTM maps raw→screen; inverse zoom maps screen→logical.
        // So new_ctm = inverse_zoom * original_ctm maps raw→logical.
        QMatrix4x4 newMatrix = transform * origMatrix;

        // Convert back to CTM 9 floats: c1=m00, c2=m01, c3=m03, c4=m10, c5=m11, c6=m13.
        newCtm[0] = newMatrix(0, 0);
        newCtm[1] = newMatrix(0, 1);
        newCtm[2] = newMatrix(0, 3);
        newCtm[3] = newMatrix(1, 0);
        newCtm[4] = newMatrix(1, 1);
        newCtm[5] = newMatrix(1, 3);
        newCtm[6] = 0;
        newCtm[7] = 0;
        newCtm[8] = 1;
    }
    // Mark this as a self-caused change so the resulting XI_PropertyEvent
    // is not treated as an external CTM update (avoids feedback loop).
    m_pendingSelfChangeCount[deviceId]++;
    XIChangeProperty(dpy, deviceId, ctmAtom, typeReturn, 32,
                     PropModeReplace, reinterpret_cast<unsigned char *>(newCtm), 9);

    XFree(data);
}

void XInputIntegration::setInputTransform(const QMatrix4x4 &transform)
{
    if (!m_hasXInput) {
        return;
    }

    // Cache the CTM atom to avoid a round trip on every call.
    if (m_ctmAtom == None) {
        m_ctmAtom = XInternAtom(display(), "Coordinate Transformation Matrix", False);
    }
    if (m_xiEventFilter) {
        m_xiEventFilter->setCtmAtom(m_ctmAtom);
    }
    if (m_ctmAtom == None) {
        return;
    }
    m_lastInputTransform = transform;

    int numDevices = 0;
    XIDeviceInfo *devices = XIQueryDevice(display(), XIAllDevices, &numDevices);
    if (!devices) {
        return;
    }

    for (int i = 0; i < numDevices; i++) {
        updateDeviceCtm(devices[i].deviceid, transform, m_ctmAtom);
    }

    XIFreeDeviceInfo(devices);
}

void XInputIntegration::handleCtmPropertyEvent(int deviceId)
{
    // If this event was caused by kwin's own XIChangeProperty, consume one
    // count and skip cache update to avoid a feedback loop.
    auto it = m_pendingSelfChangeCount.find(deviceId);
    if (it != m_pendingSelfChangeCount.end() && it.value() > 0) {
        if (--it.value() == 0) {
            m_pendingSelfChangeCount.erase(it);
        }
        return;
    }
    qCDebug(KWIN_X11STANDALONE) << "External CTM change for device" << deviceId << ", refreshing cached baseline";

    // External component changed the CTM. Re-read and update the cached
    // original so the next setInputTransform composes against the correct
    // baseline instead of the stale value captured on first access.
    if (m_ctmAtom == 0) {
        return;
    }

    Display *dpy = display();
    Atom typeReturn;
    int formatReturn;
    unsigned long numItemsReturn;
    unsigned long bytesAfterReturn;
    unsigned char *data = nullptr;

    const unsigned long expectedType = floatAtom();
    if (expectedType == 0) {
        return;
    }
    if (XIGetProperty(dpy, deviceId, m_ctmAtom, 0, 9, False,
                      expectedType, &typeReturn, &formatReturn,
                      &numItemsReturn, &bytesAfterReturn, &data) != Success) {
        return;
    }

    if (typeReturn == None || typeReturn != expectedType || numItemsReturn != 9 || formatReturn != 32) {
        if (data) {
            XFree(data);
        }
        return;
    }

    QVector<float> originalCtm(9);
    const float *ctm = reinterpret_cast<const float *>(data);
    for (int j = 0; j < 9; j++) {
        originalCtm[j] = ctm[j];
    }
    qCDebug(KWIN_X11STANDALONE) << "Refreshed cached CTM for device" << deviceId;

    m_originalCtms[deviceId] = originalCtm;

    XFree(data);

    // Re-apply the current zoom transform so the device CTM reflects the
    // updated baseline composed with zoom correction. An external CTM change
    // overwrites the zoom-adjusted CTM with the raw baseline; without this
    // re-apply, touch input loses zoom correction until the next zoom change.
    if (!m_lastInputTransform.isIdentity()) {
        updateDeviceCtm(deviceId, m_lastInputTransform, m_ctmAtom);
    }
}

unsigned long XInputIntegration::floatAtom()
{
    if (m_floatAtom == 0) {
        m_floatAtom = XInternAtom(display(), "FLOAT", False);
    }
    return m_floatAtom;
}

void XInputIntegration::handleDeviceRemoved(int deviceId)
{
    // Drop per-device caches: a device that later reuses this id must start
    // from a clean baseline instead of a stale CTM and a stale pending counter
    // that would swallow the next legitimate property event.
    const int droppedCtms = m_originalCtms.remove(deviceId);
    const int droppedPending = m_pendingSelfChangeCount.remove(deviceId);
    if (droppedCtms > 0 || droppedPending > 0) {
        qCDebug(KWIN_X11STANDALONE) << "Dropped cached CTM state for removed device" << deviceId;
    }
}

}
