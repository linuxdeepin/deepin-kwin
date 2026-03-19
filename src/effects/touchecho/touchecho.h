/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Filip Wieladek <wattos@gmail.com>
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwineffects.h>

namespace KWin
{

class TouchEchoEffect
    : public Effect
{
    Q_OBJECT
public:
    TouchEchoEffect();
    ~TouchEchoEffect() override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;
    bool isActive() const override;
    bool touchDown(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchMotion(qint32 id, const QPointF &pos, std::chrono::microseconds time) override;
    bool touchUp(qint32 id, std::chrono::microseconds time) override;

private:
    inline void drawCircle(float alpha, float cx, float cy, float r);
    inline void paintScreenSetup(int mask, QRegion region, ScreenPaintData &data);
    inline void paintScreenFinish(int mask, QRegion region, ScreenPaintData &data);

    void repaint();

    void drawCircleGl(float alpha, float cx, float cy, float r);
    void drawCircleQPainter(float alpha, float cx, float cy, float r);
    void paintScreenSetupGl(int mask, QRegion region, ScreenPaintData &data);
    void paintScreenFinishGl(int mask, QRegion region, ScreenPaintData &data);
    std::chrono::milliseconds currentTime() const;
    static constexpr float PointSize = 12.0;
    static constexpr float LineWidth = 1.0;
    static constexpr float PointAlpha = 0.2;
    static constexpr std::chrono::milliseconds PointLife = std::chrono::milliseconds(300);

    struct TouchPoint
    {
        qint32 id;
        QPointF pos;
    };

    struct DyingTouchPoint
    {
        QPointF pos;
        std::chrono::milliseconds expiration;
    };

    QVector<TouchPoint> m_points;
    QVector<DyingTouchPoint> m_dyingPoints;
};

} // namespace
