/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "glcontrol.h"

#include <QVector>

namespace KWin
{

/**
 * @brief Fraction of the wallpaper brightness the background mask keeps.
 *
 * The legacy %multitaskview effect modulated its wallpaper underlay to 0.6,
 * which reads as a dimming mask rather than as transparency. The paint
 * renderer only composites premultiplied alpha, so an underlay drawn at 0.6
 * over an already painted wallpaper reproduces the wallpaper unchanged. The
 * equivalent is an opaque wallpaper with a black layer of
 * `1.0 - kBackgroundBrightness` on top of it.
 */
inline constexpr qreal kBackgroundBrightness = 0.6;

/**
 * @brief Alpha of the black mask layered over the wallpaper underlay.
 *
 * The view dims the workspace wallpaper so the window grid and the workspace
 * bar keep their contrast against it. The mask only covers the Background
 * layer, so the grid and the bar thumbnails are not dimmed.
 */
inline constexpr qreal kBackgroundMaskAlpha = 1.0 - kBackgroundBrightness;

class ImageProvider;
class SceneModel;

class BackgroundControl : public GLControl
{
public:
    /**
     * @brief Constructs the background control.
     * @param parent Parent object.
     */
    explicit BackgroundControl(QObject *parent = nullptr);

    /**
     * @brief Sets the image provider used to load the background wallpaper.
     * @param p Image provider.
     */
    void setImageProvider(ImageProvider *p)
    {
        m_provider = p;
    }

    /**
     * @brief Attaches the scene model this control renders.
     * @param model Scene model.
     */
    void attachModel(SceneModel &model) override
    {
        m_model = &model;
    }

    /**
     * @brief The background never captures input.
     * @param pos Ignored.
     * @return Always false.
     */
    bool hitTest(const QPointF &pos) const override
    {
        Q_UNUSED(pos)
        return false;
    }

    /**
     * @brief Paints the background wallpaper.
     * @param ctx Paint context.
     * @param frame Current scene frame.
     */
    void paint(GLPaintContext &ctx, const SceneFrame &frame) override;

private:
    ImageProvider *m_provider = nullptr;
    SceneModel *m_model = nullptr;
};

}
