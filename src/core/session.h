/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinglobals.h>

#include <QObject>
#include <QString>

#include <memory>
#include <sys/types.h>

namespace KWin
{

/**
 * The Session class represents the session controlled by the compositor.
 *
 * The Session class provides information about the virtual terminal where the compositor
 * is running and a way to open files that require special privileges, e.g. DRM devices or
 * input devices.
 */
class KWIN_EXPORT Session : public QObject
{
    Q_OBJECT

public:
    /**
     * This enum type is used to specify the type of the session.
     */
    enum class Type {
        Noop,
        ConsoleKit,
        Logind,
    };

    /**
     * This enum type is used to specify optional capabilities of the session.
     */
    enum class Capability : uint {
        SwitchTerminal = 0x1,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    /**
     * This enum type is used to specify why a device has been paused.
     */
    enum class PauseDeviceType {
        /**
         * The device is temporarily unavailable, for example when switching between
         * virtual terminals. It will be resumed later.
         */
        Pause,
        /**
         * Like @c Pause, but the device was taken away because it wasn't released in time.
         */
        Force,
        /**
         * The device has been removed. It will not be resumed.
         */
        Gone,
    };

    static PauseDeviceType pauseDeviceTypeFromString(const QString &type);

    static std::unique_ptr<Session> create();
    static std::unique_ptr<Session> create(Type type);

    /**
     * Returns @c true if the session is active; otherwise returns @c false.
     */
    virtual bool isActive() const = 0;

    /**
     * Returns the capabilities supported by the session.
     */
    virtual Capabilities capabilities() const = 0;

    /**
     * Returns the seat name for the Session.
     */
    virtual QString seat() const = 0;

    /**
     * Returns the terminal controlled by the Session.
     */
    virtual uint terminal() const = 0;

    /**
     * Opens the file with the specified @a fileName. Returns the file descriptor
     * of the file or @a -1 if an error has occurred.
     */
    virtual int openRestricted(const QString &fileName) = 0;

    /**
     * Closes a file that has been opened using the openRestricted() function.
     */
    virtual void closeRestricted(int fileDescriptor) = 0;

    /**
     * Switches to the specified virtual @a terminal. This function does nothing if the
     * Capability::SwitchTerminal capability is unsupported.
     */
    virtual void switchTo(uint terminal) = 0;

    virtual void updateDisplay(QString displayNumber){}

Q_SIGNALS:
    /**
     * This signal is emitted when the session is resuming from suspend.
     */
    void awoke();
    /**
     * This signal is emitted when the session is suspend.
     */
    void suspend();
    /**
     * This signal is emitted when the active state of the session has changed.
     */
    void activeChanged(bool active);

    /**
     * This signal is emitted when the specified device can be used again.
     */
    void deviceResumed(dev_t deviceId);

    /**
     * This signal is emitted when the given device cannot be used by the compositor
     * anymore. For example, this normally occurs when switching between VTs.
     *
     * Note that when this signal is emitted for a DRM device, master permissions can
     * be already revoked.
     */
    void devicePaused(dev_t deviceId, PauseDeviceType type);

protected:
    explicit Session() = default;
};

} // namespace KWin

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::Session::Capabilities)
