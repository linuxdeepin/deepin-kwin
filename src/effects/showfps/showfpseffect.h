/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>
#include <kwinoffscreenquickview.h>

#include <QElapsedTimer>
#include <QHash>

namespace KWin
{

// Per-screen FPS statistics exposed to the overlay QML scene of one screen. The
// compositor runs a separate paint pass per screen, so all state lives here
// instead of on the effect itself; otherwise the reported value is the sum over
// all screens and can be several times higher than the frame rate of the screen
// the overlay is shown on.
class ScreenFps : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(int maximumFps READ maximumFps NOTIFY maximumFpsChanged)
    Q_PROPERTY(int paintDuration READ paintDuration NOTIFY paintChanged)
    Q_PROPERTY(int paintAmount READ paintAmount NOTIFY paintChanged)
    Q_PROPERTY(QColor paintColor READ paintColor NOTIFY paintChanged)

public:
    explicit ScreenFps(QObject *parent = nullptr);

    int fps() const;
    int maximumFps() const;
    int paintDuration() const;
    int paintAmount() const;
    QColor paintColor() const;

    void countFrame();
    void updateFps(int maximumFps);
    void beginPaint();
    void endPaint();
    void addPaintAmount(int amount);

Q_SIGNALS:
    void fpsChanged();
    void maximumFpsChanged();
    void paintChanged();

private:
    int m_frames = 0;
    int m_fps = 0;
    int m_maximumFps = 0;
    std::chrono::steady_clock::time_point m_lastUpdate;

    int m_paintDuration = 0;
    int m_paintAmount = 0;
    QElapsedTimer m_paintDurationTimer;
};

class ShowFpsEffect : public Effect
{
    Q_OBJECT

public:
    ShowFpsEffect();
    ~ShowFpsEffect() override;

    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void paintWindow(KWin::EffectWindow *w, int mask, QRegion region, KWin::WindowPaintData &data) override;
    void postPaintScreen() override;

    static bool supported();

private:
    // One overlay scene and one data object per screen. On X11 all screens are
    // painted at once and data.screen() is always the first output, so only
    // that entry is counted and rendered, matching the old single-overlay
    // behavior.
    struct ScreenEntry
    {
        std::unique_ptr<ScreenFps> data;
        std::unique_ptr<OffscreenQuickScene> scene;
    };

    ScreenEntry *ensureScreenEntry(EffectScreen *screen);

    std::unique_ptr<QWindow> m_window;
    QHash<EffectScreen *, ScreenEntry *> m_screenEntries;
    EffectScreen *m_paintedScreen = nullptr;
};

} // namespace KWin
