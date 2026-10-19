/*
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

"use strict";

var fullscreenEffect = {
    duration: animationTime(400),
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
        fullscreenEffect.duration = animationTime(400);
    },

    copyGeometry: function (geometry) {
        return {
            x: geometry.x,
            y: geometry.y,
            width: geometry.width,
            height: geometry.height
        };
    },

    canUsePosition: function (oldGeometry, newGeometry) {
        return oldGeometry.x + oldGeometry.width / 2 >= 0 &&
            oldGeometry.y + oldGeometry.height / 2 >= 0 &&
            newGeometry.x + newGeometry.width / 2 >= 0 &&
            newGeometry.y + newGeometry.height / 2 >= 0;
    },

    onWindowFullScreenChanged: function (window) {
        if (!window.visible || !window.oldGeometry) {
            return;
        }
        window.setData(Effect.WindowForceBlurRole, true);

        var oldGeometry = fullscreenEffect.copyGeometry(window.oldGeometry);
        var newGeometry = fullscreenEffect.copyGeometry(window.geometry);
        if (oldGeometry.width === newGeometry.width &&
            oldGeometry.height === newGeometry.height && window.olderGeometry) {
            oldGeometry = fullscreenEffect.copyGeometry(window.olderGeometry);
        }
        window.olderGeometry = fullscreenEffect.copyGeometry(window.oldGeometry);
        window.oldGeometry = fullscreenEffect.copyGeometry(newGeometry);

        var couldRetarget = false;
        if (window.fullScreenAnimation1 && window.fullScreenAnimation1.length >= 2) {
            var sizeRetargeted = retarget(window.fullScreenAnimation1[0], {
                value1: newGeometry.width,
                value2: newGeometry.height
            }, fullscreenEffect.duration);
            var movementRetargeted = false;
            if (sizeRetargeted) {
                if (window.fullScreenAnimation1UsesPosition &&
                    fullscreenEffect.canUsePosition(oldGeometry, newGeometry)) {
                    movementRetargeted = retarget(window.fullScreenAnimation1[1], {
                        value1: newGeometry.x + newGeometry.width / 2,
                        value2: newGeometry.y + newGeometry.height / 2
                    }, fullscreenEffect.duration);
                } else if (!window.fullScreenAnimation1UsesPosition) {
                    movementRetargeted = retarget(window.fullScreenAnimation1[1], {
                        value1: 0,
                        value2: 0
                    }, fullscreenEffect.duration);
                }
            }
            couldRetarget = sizeRetargeted && movementRetargeted;
        }

        if (!couldRetarget) {
            if (window.fullScreenAnimation1) {
                cancel(window.fullScreenAnimation1);
                delete window.fullScreenAnimation1;
            }

            var animations = [{
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
            }];

            window.fullScreenAnimation1UsesPosition =
                fullscreenEffect.canUsePosition(oldGeometry, newGeometry);
            if (window.fullScreenAnimation1UsesPosition) {
                animations.push({
                    type: Effect.Position,
                    to: {
                        value1: newGeometry.x + newGeometry.width / 2,
                        value2: newGeometry.y + newGeometry.height / 2
                    },
                    from: {
                        value1: oldGeometry.x + oldGeometry.width / 2,
                        value2: oldGeometry.y + oldGeometry.height / 2
                    },
                    curve: QEasingCurve.OutCubic
                });
            } else {
                animations.push({
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
                });
            }

            window.fullScreenAnimation1 = animate({
                window: window,
                duration: fullscreenEffect.duration,
                animations: animations
            });
        }

        var crossFadeRetargeted = false;
        if (!window.resize &&
            window.fullScreenAnimation2 &&
            window.fullScreenAnimation2.length > 0) {
            if (typeof window.fullScreenAnimation2SourceFullScreen !== "boolean") {
                window.fullScreenAnimation2SourceFullScreen = !window.fullScreen;
            }
            // Progress 0 is the captured frame; 1 is the live frame.
            var crossFadeTarget = window.fullScreenAnimation2SourceFullScreen === window.fullScreen
                ? 0.0 : 1.0;
            crossFadeRetargeted = retarget(
                window.fullScreenAnimation2[0], crossFadeTarget, fullscreenEffect.duration);
        }

        if (!window.resize && !crossFadeRetargeted) {
            window.fullScreenAnimation2SourceFullScreen = !window.fullScreen;
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
                if (window.fullScreenAnimation2) {
                    cancel(window.fullScreenAnimation2);
                    delete window.fullScreenAnimation2;
                }
            }
        }
        window.oldGeometry = fullscreenEffect.copyGeometry(window.geometry);
        window.olderGeometry = fullscreenEffect.copyGeometry(oldGeometry);
    }
};

fullscreenEffect.init();
