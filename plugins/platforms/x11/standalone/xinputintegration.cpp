/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "xinputintegration.h"
#include "main.h"
#include "logging.h"
#include "platform.h"
#include "x11cursor.h"

#include "keyboard_input.h"
#include "x11eventfilter.h"
#include <kwinglobals.h>

#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2proto.h>

#include <linux/input.h>

namespace KWin
{

class XInputEventFilter : public X11EventFilter
{
public:
    XInputEventFilter(int xi_opcode)
        : X11EventFilter(XCB_GE_GENERIC, xi_opcode, QVector<int>{XI_RawMotion, XI_RawButtonPress, XI_RawButtonRelease, XI_RawKeyPress, XI_RawKeyRelease,
                                                                 XI_RawTouchBegin, XI_RawTouchUpdate, XI_RawTouchEnd})
        {}
    virtual ~XInputEventFilter() = default;

    bool event(xcb_generic_event_t *event) override {
        xcb_ge_generic_event_t *ge = reinterpret_cast<xcb_ge_generic_event_t *>(event);
        switch (ge->event_type) {
        case XI_RawKeyPress:
            if (m_xkb) {
                m_xkb->updateKey(reinterpret_cast<xXIRawEvent*>(event)->detail - 8, InputRedirection::KeyboardKeyPressed);
            }
            break;
        case XI_RawKeyRelease:
            if (m_xkb) {
                m_xkb->updateKey(reinterpret_cast<xXIRawEvent*>(event)->detail - 8, InputRedirection::KeyboardKeyReleased);
            }
            break;
        case XI_RawButtonPress:
            if (m_xkb) {
                auto e = reinterpret_cast<xXIRawEvent*>(event);
                switch (e->detail) {
                // TODO: this currently ignores left handed settings, for current usage not needed
                // if we want to use also for global mouse shortcuts, this needs to reflect state correctly
                case XCB_BUTTON_INDEX_1:
                    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, e->time);
                    break;
                case XCB_BUTTON_INDEX_2:
                    kwinApp()->platform()->pointerButtonPressed(BTN_MIDDLE, e->time);
                    break;
                case XCB_BUTTON_INDEX_3:
                    kwinApp()->platform()->pointerButtonPressed(BTN_RIGHT, e->time);
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
        case XI_RawMotion: {
            auto e = reinterpret_cast<xXIRawEvent*>(event);
            kwinApp()->platform()->pointerMotion(QCursor::pos(), e->time);
            break;
        }
        case XI_RawButtonRelease:
            if (m_xkb) {
                auto e = reinterpret_cast<xXIRawEvent*>(event);
                switch (e->detail) {
                // TODO: this currently ignores left handed settings, for current usage not needed
                // if we want to use also for global mouse shortcuts, this needs to reflect state correctly
                case XCB_BUTTON_INDEX_1:
                    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, e->time);
                    break;
                case XCB_BUTTON_INDEX_2:
                    kwinApp()->platform()->pointerButtonReleased(BTN_MIDDLE, e->time);
                    break;
                case XCB_BUTTON_INDEX_3:
                    kwinApp()->platform()->pointerButtonReleased(BTN_RIGHT, e->time);
                    break;
                case XCB_BUTTON_INDEX_4:
                    kwinApp()->platform()->pointerAxisVertical(120, e->time);
                    break;
                case XCB_BUTTON_INDEX_5:
                    kwinApp()->platform()->pointerAxisVertical(-120, e->time);
                    break;
                // TODO: further buttons, horizontal scrolling?
                }
            }
            if (m_x11Cursor) {
                m_x11Cursor->schedulePoll();
            }
            break;
        // 在xorg中，一个触摸点的touchUpdate和touchEnd事件只会发送给touchBegin事件的接收者，
        // 也就是说，当手指已经按在触屏上后，在此之后kwin成功grab了触屏和鼠标事件，但在grab之前
        // 已经按下的触摸点的后续事件kwin中也无法接收。这样情况发生于：一个client收到touchBegin
        // 后请求_NET_WM_MOVERESIZE，之后手指移动时无法移动窗口，因此，此处监听原始的触屏事件，
        // 用于处理此问题。
        case XI_RawTouchBegin: {
            auto e = reinterpret_cast<xXIRawEvent*>(event);

            // 鼠标只会跟随第一个触摸点，此处只处理第一个触摸点
            if (m_first_touch_point  == UINT_MAX) {
                m_first_touch_point = e->detail;
            } else {
                break;
            }

            kwinApp()->platform()->touchDown(e->detail, QCursor::pos(), e->time);
            break;
        }
        case XI_RawTouchUpdate: {
            auto e = reinterpret_cast<xXIRawEvent*>(event);

            if (m_first_touch_point != e->detail) {
                break;
            }

            kwinApp()->platform()->touchMotion(e->detail, QCursor::pos(), e->time);
            break;
        }
        case XI_RawTouchEnd: {
            auto e = reinterpret_cast<xXIRawEvent*>(event);

            if (m_first_touch_point != e->detail) {
                break;
            }

            // 清除已按下的第一个点
            m_first_touch_point = UINT_MAX;
            kwinApp()->platform()->touchUp(e->detail, e->time);
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

    void setCursor(const QPointer<X11Cursor> &cursor) {
        m_x11Cursor = cursor;
    }
    void setXkb(Xkb *xkb) {
        m_xkb = xkb;
    }

private:
    QPointer<X11Cursor> m_x11Cursor;
    // TODO: QPointer
    Xkb *m_xkb = nullptr;
    quint32 m_first_touch_point = UINT_MAX;
};

class XKeyPressReleaseEventFilter : public X11EventFilter
{
public:
    XKeyPressReleaseEventFilter(uint32_t type)
        : X11EventFilter(type)
    {}
    ~XKeyPressReleaseEventFilter() = default;

    bool event(xcb_generic_event_t *event) override {
        xcb_key_press_event_t *ke = reinterpret_cast<xcb_key_press_event_t *>(event);
        if (m_xkb && ke->event == ke->root) {
            const uint8_t eventType = event->response_type & ~0x80;
            if (eventType == XCB_KEY_PRESS) {
                m_xkb->updateKey(ke->detail - 8, InputRedirection::KeyboardKeyPressed);
            } else {
                m_xkb->updateKey(ke->detail - 8, InputRedirection::KeyboardKeyReleased);
            }
        }
        return false;
    }

    void setXkb(Xkb *xkb) {
        m_xkb = xkb;
    }

private:
    // TODO: QPointer
    Xkb *m_xkb = nullptr;
};

XInputIntegration::XInputIntegration(QObject *parent)
    : QObject(parent)
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
    int major = 2, minor = 0;
    int result = XIQueryVersion(dpy, &major, &minor);
    if (result == BadImplementation) {
        // Xinput 2.2 returns BadImplementation if checked against 2.0
        major = 2;
        minor = 2;
        if (XIQueryVersion(dpy, &major, &minor) != Success) {
            qCDebug(KWIN_X11STANDALONE) << "Failed to init XInput";
            return;
        }
    } else if (result != Success) {
        qCDebug(KWIN_X11STANDALONE) << "Failed to init XInput";
        return;
    }
    m_hasXInput = true;
    m_xiOpcode = xi_opcode;
    m_majorVersion = major;
    m_minorVersion = minor;
    qCDebug(KWIN_X11STANDALONE) << "Has XInput support" << m_majorVersion << "." << m_minorVersion;
}

void XInputIntegration::setCursor(X11Cursor *cursor)
{
    m_x11Cursor = QPointer<X11Cursor>(cursor);
}

void XInputIntegration::setXkb(Xkb *xkb)
{
    m_xkb = xkb;
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

    if (m_majorVersion >= 2) {
        if (m_minorVersion >= 1) {
            // we need to listen to all events, which is only available with XInput 2.1
            XISetMask(mask1, XI_RawKeyPress);
            XISetMask(mask1, XI_RawKeyRelease);
        }

        // 在xinput>=2.2版本时监听触摸事件，支持触摸屏下窗口移动/resize
        if (m_minorVersion >= 2) {
            XISetMask(mask1, XI_RawTouchBegin);
            XISetMask(mask1, XI_RawTouchUpdate);
            XISetMask(mask1, XI_RawTouchEnd);
        }
    }

    evmasks[0].deviceid = XIAllMasterDevices;
    evmasks[0].mask_len = sizeof(mask1);
    evmasks[0].mask = mask1;
    XISelectEvents(display(), rootWindow(), evmasks, 1);
    m_xiEventFilter.reset(new XInputEventFilter(m_xiOpcode));
    m_xiEventFilter->setCursor(m_x11Cursor);
    m_xiEventFilter->setXkb(m_xkb);
    m_keyPressFilter.reset(new XKeyPressReleaseEventFilter(XCB_KEY_PRESS));
    m_keyPressFilter->setXkb(m_xkb);
    m_keyReleaseFilter.reset(new XKeyPressReleaseEventFilter(XCB_KEY_RELEASE));
    m_keyReleaseFilter->setXkb(m_xkb);
}

}
