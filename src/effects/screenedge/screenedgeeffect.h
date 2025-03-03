/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include <kwineffects.h>

class QTimer;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
namespace Plasma
{
class Svg;
}
#else
namespace KSvg
{
class Svg;
}
#endif

namespace KWin
{
class Glow;
class GLTexture;

class ScreenEdgeEffect : public Effect
{
    Q_OBJECT
public:
    ScreenEdgeEffect();
    ~ScreenEdgeEffect() override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 10;
    }

private Q_SLOTS:
    void edgeApproaching(ElectricBorder border, qreal factor, const QRect &geometry);
    void cleanup();

private:
    void ensureGlowSvg();
    std::unique_ptr<Glow> createGlow(ElectricBorder border, qreal factor, const QRect &geometry);
    template<typename T>
    T *createCornerGlow(ElectricBorder border);
    template<typename T>
    T *createEdgeGlow(ElectricBorder border, const QSize &size);
    QSize cornerGlowSize(ElectricBorder border);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Plasma::Svg *m_glow = nullptr;
#else
    KSvg::Svg *m_glow = nullptr;
#endif
    std::map<ElectricBorder, std::unique_ptr<Glow>> m_borders;
    QTimer *m_cleanupTimer;
};

class Glow
{
public:
    std::unique_ptr<GLTexture> texture;
    std::unique_ptr<QImage> image;
    QSize pictureSize;
    qreal strength;
    QRect geometry;
    ElectricBorder border;
};

}
