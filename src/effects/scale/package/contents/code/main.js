/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018, 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

const blacklist = [
    // The logout screen has to be animated only by the logout effect.
    "ksmserver ksmserver",
    "ksmserver-logout-greeter ksmserver-logout-greeter",

    // KDE Plasma splash screen has to be animated only by the login effect.
    "ksplashqml ksplashqml",
    "dde-blackwidget dde-blackwidget",
    "dde-desktop dde-desktop",
    "dde-shell/launchpad",
    "deepin-screen-recorder deepin-screen-recorder",
    "plasmashell plasmashell",
    "plasmashell org.kde.plasmashell"
];

const whitelist = [
    "dde-lock org.deepin.dde.lock",
    "dde-launcher dde-launcher",
    "org.deepin.ds.launchpad.fullscreen",
    "dde-polkit-agent dde-polkit-agent"
];

var scaleEffect = {
    loadConfig: function (window) {
        scaleEffect.addSize = effect.readConfig("addSize", 30);
        scaleEffect.addLauncherSize = effect.readConfig("addLauncherSize", 120);
        scaleEffect.addDuration = effect.readConfig("addDuration", 360) / 2;
        scaleEffect.addLauncherDuration = effect.readConfig("addLauncherDuration", 360) / 2;
        scaleEffect.addCurve = effect.readConfig("addCurve", 10);
        scaleEffect.closedSize = effect.readConfig("closedSize", 30);
        scaleEffect.closedLauncherSize = effect.readConfig("closedLauncherSize", 120);
        scaleEffect.closedDuration = effect.readConfig("closedDuration", 360) / 2;
        scaleEffect.closedLauncherDuration = effect.readConfig("closedLauncherDuration", 800) / 2;
        scaleEffect.closedCurve = effect.readConfig("closedCurve", 10);
    },
    isScaleWindow: function (window) {
        if ((blacklist.indexOf(window.windowClass) != -1) || (blacklist.indexOf(window.caption) !== -1)) {
            return false;
        }

        if (window.hasDecoration) {
            return true;
        }

        // Don't animate combobox popups, tooltips, popup menus, etc.
        if (window.popupWindow) {
            return false;
        }

        // Dont't animate the outline and the screenlocker as it looks bad.
        if (window.lockScreen || window.outline) {
            return false;
        }

        // Don't animate the tabbox switcher window.
        if (window.switcherwin) {
            return false;
        }

        // Override-redirect windows are usually used for user interface
        // concepts that are not expected to be animated by this effect.
        /*if (!window.managed) {
            return false;
        }*/

        if (whitelist.indexOf(window.windowClass) != -1) {
            return true;
        }

        return window.normalWindow || window.dialog || window.utility || window.splash || window.onScreenDisplay;
    },
    setupForcedRoles: function (window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, true);
        window.setData(Effect.WindowForceBlurRole, true);
    },
    cleanupForcedRoles: function (window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
    },
    // Tracks Deleted windows currently running their scale-out (close) animation,
    // keyed by windowClass. Populated only from slotWindowClosed (the close path,
    // which receives the Deleted), so minimize/unminimize is never recorded here.
    scaleOutWindows: {},
    registerScaleOut: function (window, animation) {
        var windowClass = window.windowClass;
        if (!scaleEffect.scaleOutWindows[windowClass]) {
            scaleEffect.scaleOutWindows[windowClass] = [];
        }
        scaleEffect.scaleOutWindows[windowClass].push({
            window: window,
            animation: animation
        });
    },
    unregisterScaleOut: function (window) {
        var windowClass = window.windowClass;
        var entries = scaleEffect.scaleOutWindows[windowClass];
        if (!entries) {
            return;
        }
        scaleEffect.scaleOutWindows[windowClass] = entries.filter(function (entry) {
            return entry.window !== window;
        });
        if (scaleEffect.scaleOutWindows[windowClass].length === 0) {
            delete scaleEffect.scaleOutWindows[windowClass];
        }
    },
    // When a window is added while a Deleted window of the same windowClass is
    // still running its scale-out animation (close+reopen race, e.g. quickly
    // toggling the control center icon), cancel the pending scale-out so the old
    // render object stops being painted and is released early instead of
    // overlapping the freshly scaling-in window. Only Deleted windows recorded
    // via slotWindowClosed are ever cancelled here.
    cancelPendingScaleOut: function (windowClass) {
        var entries = scaleEffect.scaleOutWindows[windowClass];
        if (!entries || entries.length === 0) {
            return;
        }
        for (var i = 0; i < entries.length; i++) {
            var entry = entries[i];
            cancel(entry.animation);
            delete entry.window.scaleOutAnimation;
            scaleEffect.cleanupForcedRoles(entry.window);
        }
        delete scaleEffect.scaleOutWindows[windowClass];
    },
    slotWindowAdded: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!scaleEffect.isScaleWindow(window)) {
            return;
        }
        if (!window.visible) {
            return;
        }
        if (window.startEffectType != 1
            && window.startEffectType != 16) {
            return;
        }
        if (!effect.grab(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        if (window.dock || window.splash || window.toolbar
            || window.notification
            || window.criticalNotification
            || window.appletPopup) {
            return ;
        }
        // Drop any still-running scale-out of a Deleted window sharing the same
        // windowClass so the old render object is not painted alongside this one
        // when an application is closed and reopened quickly (WM-91).
        scaleEffect.cancelPendingScaleOut(window.windowClass);
        scaleEffect.setupForcedRoles(window);
        var windowRect = window.geometry;
        var scaleSize = scaleEffect.addSize;
        var scaleDuration = scaleEffect.addDuration;
        if (window.windowClass == "dde-launcher dde-launcher" ||
            window.startEffectType == 16 ||
            window.caption == "org.deepin.ds.launchpad.fullscreen") {
            scaleSize = scaleEffect.addLauncherSize;
            scaleDuration = scaleEffect.addLauncherDuration;
        }
        window.scaleInAnimation = animate({
            window: window,
            curve: scaleEffect.addCurve,
            duration: animationTime(scaleDuration),
            animations: [
                {
                    type: Effect.Scale,
                    from: scaleSize / 100
                },
                {
                    type: Effect.Opacity,
                    from: 0.0,
                    to: 1.0
                },
            ]
        });
    },
    slotWindowClosed: function (window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        if (!scaleEffect.isScaleWindow(window)) {
            return;
        }
        if (!window.visible) {
            return;
        }
        if (window.startEffectType != 1 && window.startEffectType != 16) {
            return;
        }
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        if (window.dock || window.splash || window.toolbar
            || window.notification || window.onScreenDisplay
            || window.criticalNotification
            || window.appletPopup) {
            return ;
        }
        if (window.scaleInAnimation) {
            cancel(window.scaleInAnimation);
            delete window.scaleInAnimation;
        }
        scaleEffect.setupForcedRoles(window);
        var windowRect = window.geometry;
        var scaleSize = scaleEffect.closedSize
        var scaleDuration = scaleEffect.closedDuration;
        if (window.windowClass == "dde-launcher dde-launcher" ||
            window.windowClass == "dde-lock org.deepin.dde.lock" ||
            window.caption == "org.deepin.ds.launchpad.fullscreen") {
            scaleSize = scaleEffect.closedLauncherSize;
            scaleDuration = scaleEffect.closedLauncherDuration;
        }
        window.scaleOutAnimation = animate({
            window: window,
            curve: scaleEffect.closedCurve,
            duration: animationTime(scaleDuration),
            animations: [
                {
                    type: Effect.Scale,
                    to: scaleSize / 100
                },
                {
                    type: Effect.Opacity,
                    to: 0.0,
                    from: 1.0
                },
            ]
        });
        scaleEffect.registerScaleOut(window, window.scaleOutAnimation);
    },
    slotWindowDataChanged: function (window, role) {
        if (role == Effect.WindowAddedGrabRole) {
            if (window.scaleInAnimation && effect.isGrabbed(window, role)) {
                cancel(window.scaleInAnimation);
                delete window.scaleInAnimation;
                scaleEffect.cleanupForcedRoles(window);
            }
        } else if (role == Effect.WindowClosedGrabRole) {
            if (window.scaleOutAnimation && effect.isGrabbed(window, role)) {
                cancel(window.scaleOutAnimation);
                delete window.scaleOutAnimation;
                scaleEffect.unregisterScaleOut(window);
                scaleEffect.cleanupForcedRoles(window);
            }
        }
    },
    slotAnimationEnded: function (window) {
        // Fires for both scale-in and scale-out animations reaching their natural
        // end. Drop any tracked scale-out entry for this window (a no-op for the
        // scale-in of a freshly added window) and release the forced roles as the
        // original handler did.
        scaleEffect.unregisterScaleOut(window);
        scaleEffect.cleanupForcedRoles(window);
    },
    init: function () {
        scaleEffect.loadConfig();

        effect.configChanged.connect(scaleEffect.loadConfig);
        effect.animationEnded.connect(scaleEffect.slotAnimationEnded);
        effects.windowAdded.connect(scaleEffect.slotWindowAdded);
        effects.windowClosed.connect(scaleEffect.slotWindowClosed);
        effects.windowDataChanged.connect(scaleEffect.slotWindowDataChanged);
    }
};

scaleEffect.init();
