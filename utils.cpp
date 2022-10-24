// Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
// Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

/*

 This file is for (very) small utility functions/classes.

*/

#include "utils.h"

#include <QWidget>
#include <kkeyserver.h>

#ifndef KCMRULES
#include <assert.h>
#include <QApplication>
#include <QDebug>

#include <X11/Xlib.h>

#include <stdio.h>

#include "atoms.h"
#include "platform.h"
#include "workspace.h"
#include <signal.h>

#endif

Q_LOGGING_CATEGORY(KWIN_CORE, "kwin_core", QtCriticalMsg)
Q_LOGGING_CATEGORY(KWIN_VIRTUALKEYBOARD, "kwin_virtualkeyboard", QtCriticalMsg)
namespace KWin
{

#ifndef KCMRULES

//************************************
// StrutRect
//************************************

StrutRect::StrutRect(QRect rect, StrutArea area)
    : QRect(rect)
    , m_area(area)
{
}

StrutRect::StrutRect(const StrutRect& other)
    : QRect(other)
    , m_area(other.area())
{
}

#endif

#ifndef KCMRULES
void updateXTime()
{
    kwinApp()->platform()->updateXTime();
}

static int server_grab_count = 0;

void grabXServer()
{
    if (++server_grab_count == 1)
        xcb_grab_server(connection());
}

void ungrabXServer()
{
    assert(server_grab_count > 0);
    if (--server_grab_count == 0) {
        xcb_ungrab_server(connection());
        xcb_flush(connection());
    }
}

static bool keyboard_grabbed = false;

bool grabXKeyboard(xcb_window_t w)
{
    if (QWidget::keyboardGrabber() != NULL)
        return false;
    if (keyboard_grabbed)
        return false;
    if (qApp->activePopupWidget() != NULL)
        return false;
    if (w == XCB_WINDOW_NONE)
        w = rootWindow();
    const xcb_grab_keyboard_cookie_t c = xcb_grab_keyboard_unchecked(connection(), false, w, xTime(),
                                                                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    ScopedCPointer<xcb_grab_keyboard_reply_t> grab(xcb_grab_keyboard_reply(connection(), c, NULL));
    if (grab.isNull()) {
        return false;
    }
    if (grab->status != XCB_GRAB_STATUS_SUCCESS) {
        return false;
    }
    keyboard_grabbed = true;
    return true;
}

void ungrabXKeyboard()
{
    if (!keyboard_grabbed) {
        // grabXKeyboard() may fail sometimes, so don't fail, but at least warn anyway
        qCDebug(KWIN_CORE) << "ungrabXKeyboard() called but keyboard not grabbed!";
    }
    keyboard_grabbed = false;
    xcb_ungrab_keyboard(connection(), XCB_TIME_CURRENT_TIME);
}

Process::Process(QObject *parent)
    : QProcess(parent)
{
}

Process::~Process() = default;

void Process::setupChildProcess()
{
    sigset_t userSiganls;
    sigemptyset(&userSiganls);
    sigaddset(&userSiganls, SIGUSR1);
    sigaddset(&userSiganls, SIGUSR2);
    pthread_sigmask(SIG_UNBLOCK, &userSiganls, nullptr);
}

#endif

// converting between X11 mouse/keyboard state mask and Qt button/keyboard states

Qt::MouseButton x11ToQtMouseButton(int button)
{
    if (button == XCB_BUTTON_INDEX_1)
        return Qt::LeftButton;
    if (button == XCB_BUTTON_INDEX_2)
        return Qt::MidButton;
    if (button == XCB_BUTTON_INDEX_3)
        return Qt::RightButton;
    if (button == XCB_BUTTON_INDEX_4)
        return Qt::XButton1;
    if (button == XCB_BUTTON_INDEX_5)
        return Qt::XButton2;
    return Qt::NoButton;
}

Qt::MouseButtons x11ToQtMouseButtons(int state)
{
    Qt::MouseButtons ret = 0;
    if (state & XCB_KEY_BUT_MASK_BUTTON_1)
        ret |= Qt::LeftButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_2)
        ret |= Qt::MidButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_3)
        ret |= Qt::RightButton;
    if (state & XCB_KEY_BUT_MASK_BUTTON_4)
        ret |= Qt::XButton1;
    if (state & XCB_KEY_BUT_MASK_BUTTON_5)
        ret |= Qt::XButton2;
    return ret;
}

Qt::KeyboardModifiers x11ToQtKeyboardModifiers(int state)
{
    Qt::KeyboardModifiers ret = 0;
    if (state & XCB_KEY_BUT_MASK_SHIFT)
        ret |= Qt::ShiftModifier;
    if (state & XCB_KEY_BUT_MASK_CONTROL)
        ret |= Qt::ControlModifier;
    if (state & KKeyServer::modXAlt())
        ret |= Qt::AltModifier;
    if (state & KKeyServer::modXMeta())
        ret |= Qt::MetaModifier;
    return ret;
}

} // namespace

#include <cmath>
#include <qcolor.h>

float RgbToHsv::minValue(float a,float b)
{
    float temp = b;
    if(a < temp)
        temp = a;
    return temp;
}

float RgbToHsv::maxValue(float a,float b)
{
    float temp = b;
    if(a > temp)
        temp = a;
    return temp;
}

void RgbToHsv::RGB_TO_HSV(const COLOR_RGB* input,COLOR_HSV* output)
{
    float r,g,b,minRGB,maxRGB,deltaRGB;

    r = input->R/255.0f;
    g = input->G/255.0f;
    b = input->B/255.0f;
    minRGB = minValue(r,minValue(g,b));
    maxRGB = maxValue(r,maxValue(g,b));
    deltaRGB = maxRGB - minRGB;

    output->V = maxRGB;
    if(maxRGB != 0.0f)
        output->S = deltaRGB / maxRGB;
    else
        output->S = 0.0f;
    if (output->S <= 0.0f) {
        output->H = 0.0f;
    } else {
        if (r == maxRGB) {
            output->H = (g-b)/deltaRGB;
        } else {
            if (g == maxRGB) {
                output->H = 2.0f + (b-r)/deltaRGB;
            } else {
                if (b == maxRGB) {
                    output->H = 4.0f + (r-g)/deltaRGB;
                }
            }
        }
        output->H = output->H * 60.0f;
        if (output->H < 0.0f) {
            output->H += 360;
        }
        output->H /= 360;
    }
}

void RgbToHsv::HSV_TO_RGB(COLOR_HSV* input,COLOR_RGB* output)
{
    float R,G,B;
    int k;
    float aa,bb,cc,f;
    if (input->S <= 0.0f)
        R = G = B = input->V;
    else {
        if (input->H == 1.0f)
            input->H = 0.0f;
        input->H *= 6.0f;
        k = (int)floor(input->H);
        f = input->H - k;
        aa = input->V * (1.0f - input->S);
        bb = input->V * (1.0f - input->S * f);
        cc = input->V * (1.0f -(input->S * (1.0f - f)));
        switch(k)
        {
        case 0:
            R = input->V;
            G = cc;
            B =aa;
            break;
        case 1:
            R = bb;
            G = input->V;
            B = aa;
            break;
        case 2:
            R =aa;
            G = input->V;
            B = cc;
            break;
        case 3:
            R = aa;
            G = bb;
            B = input->V;
            break;
        case 4:
            R = cc;
            G = aa;
            B = input->V;
            break;
        case 5:
            R = input->V;
            G = aa;
            B = bb;
            break;
        }
    }
    output->R = (unsigned char)(R * 255);
    output->G = (unsigned char)(G * 255);
    output->B = (unsigned char)(B * 255);
}

QString RgbToHsv::adjustBrightness(QString rgb, int step)
{
    rgb = rgb.mid(1);
    QColor color(rgb.toUInt(NULL,16));
    rgb_v.R = color.red();
    rgb_v.G = color.green();
    rgb_v.B = color.blue();
    rgb_v.l = 0x64;

    COLOR_HSV hsv_v;

    RGB_TO_HSV(&rgb_v,&hsv_v);
    rgb_v.l += step;
    if(rgb_v.l <= 0) {
        rgb_v.l = 1;
    } else if (rgb_v.l >= 100) {
        rgb_v.l = 100;
    }

    hsv_v.V = rgb_v.l / 100.0;
    HSV_TO_RGB(&hsv_v,&rgb_v);
    char c[8] = {0};
    sprintf(c, "#%02x%02x%02x", rgb_v.R, rgb_v.G, rgb_v.B);
    QString str(c);
    return str;
}

#ifndef KCMRULES
#endif
