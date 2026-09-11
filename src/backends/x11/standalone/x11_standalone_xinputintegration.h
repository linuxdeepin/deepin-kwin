/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QPointer>
#include <memory>
#include <QHash>
#include <QVector>
typedef struct _XDisplay Display;
#include <QMatrix4x4>

namespace KWin
{

class XInputEventFilter;
class XKeyPressReleaseEventFilter;
class X11Cursor;

class XInputIntegration : public QObject
{
    Q_OBJECT
public:
    explicit XInputIntegration(Display *display, QObject *parent);
    ~XInputIntegration() override;

    void init();
    void startListening();

    bool hasXinput() const
    {
        return m_hasXInput;
    }
    void setCursor(X11Cursor *cursor);
    void setInputTransform(const QMatrix4x4 &transform);
    void updateDeviceCtm(int deviceId, const QMatrix4x4 &transform, unsigned long ctmAtom);
    void handleCtmPropertyEvent(int deviceId);
    void handleDeviceRemoved(int deviceId);

private:
    Display *display() const
    {
        return m_x11Display;
    }

    bool m_hasXInput = false;
    int m_xiOpcode = 0;
    int m_majorVersion = 0;
    int m_minorVersion = 0;
    QPointer<X11Cursor> m_x11Cursor;
    Display *m_x11Display;

    std::unique_ptr<XInputEventFilter> m_xiEventFilter;
    std::unique_ptr<XKeyPressReleaseEventFilter> m_keyPressFilter;
    std::unique_ptr<XKeyPressReleaseEventFilter> m_keyReleaseFilter;
    // Cached original Coordinate Transformation Matrix per device, to restore on reset.
    QHash<int, QVector<float>> m_originalCtms;
    // Per-device counter of pending CTM property events caused by kwin's own
    // XIChangeProperty, used to distinguish self-caused events from external
    // changes and avoid a feedback loop.
    QHash<int, int> m_pendingSelfChangeCount;
    unsigned long m_ctmAtom = 0;
    // Atom of the FLOAT type used by the CTM property, interned lazily.
    unsigned long m_floatAtom = 0;
    unsigned long floatAtom();
    // Last input transform applied via setInputTransform(), used to re-apply
    // zoom correction after an external CTM change updates the cached baseline.
    QMatrix4x4 m_lastInputTransform;
};

}
