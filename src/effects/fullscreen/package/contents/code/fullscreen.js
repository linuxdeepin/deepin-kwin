/*
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var fullscreenEffect = {
    duration: animationTime(250),
    init: function () {
        effect.configChanged.connect(fullscreenEffect.loadConfig);
        effects.windowFrameGeometryChanged.connect(
            fullscreenEffect.onWindowFrameGeometryChanged);
        effects.windowFullScreenChanged.connect(
            fullscreenEffect.onWindowFullScreenChanged);
        effect.animationEnded.connect(fullscreenEffect.restoreForceBlurState);

        fullscreenEffect.loadConfig();
    },

    loadConfig: function () {
        fullscreenEffect.duration = animationTime(250);
    },

    onWindowFullScreenChanged: function (window) {
        if (!window.visible || !window.oldGeometry) {
            return;
        }
        window.setData(Effect.WindowForceBlurRole, true);
        let oldGeometry = window.oldGeometry;
        const newGeometry = window.geometry;
        if (oldGeometry.width == newGeometry.width && oldGeometry.height == newGeometry.height)
            oldGeometry = window.olderGeometry;
        window.olderGeometry = window.oldGeometry;
        window.oldGeometry = newGeometry;
        window.fullScreenAnimation1 = animate({
            window: window,
            duration: fullscreenEffect.duration,
            animations: [{
                type: Effect.Size,
                to: {
                    value1: newGeometry.width,
                    value2: newGeometry.height
                },
                from: {
                    value1: oldGeometry.width,
                    value2: oldGeometry.height
                },
                curve: QEasingCurve.OutCubic
            }, {
                type: Effect.Translation,
                to: {
                    value1: 0,
                    value2: 0
                },
                from: {
                    value1: oldGeometry.x - newGeometry.x - (newGeometry.width / 2 - oldGeometry.width / 2),
                    value2: oldGeometry.y - newGeometry.y - (newGeometry.height / 2 - oldGeometry.height / 2)
                },
                curve: QEasingCurve.OutCubic
            }]
        });
        if (!window.resize) {
            window.fullScreenAnimation2 = animate({
                window: window,
                duration: fullscreenEffect.duration,
                animations: [{
                    type: Effect.CrossFadePrevious,
                    to: 1.0,
                    from: 0.0,
                    curve: QEasingCurve.OutCubic
                }]
            });
        }
    },

    restoreForceBlurState: function (window) {
        window.setData(Effect.WindowForceBlurRole, null);
    },

    onWindowFrameGeometryChanged: function (window, oldGeometry) {
        if (window.fullScreenAnimation1) {
            if (window.geometry.width != window.oldGeometry.width ||
                window.geometry.height != window.oldGeometry.height) {
                cancel(window.fullScreenAnimation1);
                delete window.fullScreenAnimation1;
                if (window.fullScreenAnimation2) {
                    cancel(window.fullScreenAnimation2);
                    delete window.fullScreenAnimation2;
                }
            }
        }
        window.oldGeometry = window.geometry;
        window.olderGeometry = oldGeometry;
    }
};

fullscreenEffect.init();
