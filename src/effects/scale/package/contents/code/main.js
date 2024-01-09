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
    "ksplashqml ksplashqml"
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
        // We don't want to animate most of plasmashell's windows, yet, some
        // of them we want to, for example, Task Manager Settings window.
        // The problem is that all those window share single window class.
        // So, the only way to decide whether a window should be animated is
        // to use a heuristic: if a window has decoration, then it's most
        // likely a dialog or a settings window so we have to animate it.
        if (window.windowClass == "plasmashell plasmashell"
                || window.windowClass == "plasmashell org.kde.plasmashell") {
            return window.hasDecoration;
        }

        if (blacklist.indexOf(window.windowClass) != -1) {
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

        // Override-redirect windows are usually used for user interface
        // concepts that are not expected to be animated by this effect.
        if (!window.managed) {
            return false;
        }

        if (window.windowClass == "dde-launcher dde-launcher")
            return true;

        return true;
    },
    setupForcedRoles: function (window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, true);
        window.setData(Effect.WindowForceBlurRole, true);
    },
    cleanupForcedRoles: function (window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
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
        if (!effect.grab(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        scaleEffect.setupForcedRoles(window);
        var windowRect = window.geometry;
        var scaleSize = scaleEffect.addSize;
        var scaleDuration = scaleEffect.addDuration;
        if (window.windowClass == "dde-launcher dde-launcher") {
            scaleSize = scaleEffect.addLauncherSize;
            scaleDuration = scaleEffect.addLauncherDuration;
        }
        window.scaleInAnimation = animate({
            window: window,
            curve: scaleEffect.addCurve,
            duration: animationTime(scaleDuration),
            animations: [
                {
                    type: Effect.Size,
                    from: {
                        value1: windowRect.width * scaleSize / 100,
                        value2: windowRect.height * scaleSize / 100
                    },
                    to: {
                        value1: windowRect.width,
                        value2: windowRect.height
                    }
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
        if (!effect.grab(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        if (window.scaleInAnimation) {
            cancel(window.scaleInAnimation);
            delete window.scaleInAnimation;
        }
        scaleEffect.setupForcedRoles(window);
        var windowRect = window.geometry;
        var scaleSize = scaleEffect.closedSize
        var scaleDuration = scaleEffect.closedDuration;
        if (window.windowClass == "dde-launcher dde-launcher") {
            scaleSize = scaleEffect.closedLauncherSize;
            scaleDuration = scaleEffect.closedLauncherDuration;
        }
        window.scaleOutAnimation = animate({
            window: window,
            curve: scaleEffect.closedCurve,
            duration: animationTime(scaleDuration),
            animations: [
                {
                    type: Effect.Size,
                    to: {
                        value1: windowRect.width * scaleSize / 100,
                        value2: windowRect.height * scaleSize / 100
                    },
                    from: {
                        value1: windowRect.width,
                        value2: windowRect.height
                    }
                },
                {
                    type: Effect.Opacity,
                    to: 0.0,
                    from: 1.0
                },
            ]
        });
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
                scaleEffect.cleanupForcedRoles(window);
            }
        }
    },
    init: function () {
        scaleEffect.loadConfig();

        effect.configChanged.connect(scaleEffect.loadConfig);
        effect.animationEnded.connect(scaleEffect.cleanupForcedRoles);
        effects.windowAdded.connect(scaleEffect.slotWindowAdded);
        effects.windowClosed.connect(scaleEffect.slotWindowClosed);
        effects.windowDataChanged.connect(scaleEffect.slotWindowDataChanged);
    }
};

scaleEffect.init();
