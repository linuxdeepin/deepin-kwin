/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "glcontrol.h"

#include <QSizeF>
#include <QString>

namespace KWin
{

class ImageProvider;
class SceneModel;

class HintControl : public GLControl
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the hint control.
     * @param parent Parent object.
     */
    explicit HintControl(QObject *parent = nullptr);

    /**
     * @brief Sets the image provider used to rasterize hint text.
     * @param provider Image provider.
     */
    void setImageProvider(ImageProvider *provider);

    /**
     * @brief Returns the hint text.
     * @return Current hint text.
     */
    const QString &text() const
    {
        return m_text;
    }

    /**
     * @brief Sets the hint text.
     * @param text New hint text. An empty string hides the hint.
     */
    void setText(const QString &text);

    /**
     * @brief Returns the hint anchor in screen coordinates.
     * @return Anchor point.
     */
    QPointF anchor() const
    {
        return m_anchor;
    }

    /**
     * @brief Sets the hint anchor in screen coordinates.
     * @param anchor New anchor point.
     */
    void setAnchor(const QPointF &anchor);

    /**
     * @brief Attaches the scene model.
     * @param model Ignored, as the hint is driven by explicitly injected values.
     */
    void attachModel([[maybe_unused]] SceneModel &model) override
    {
    }

    /**
     * @brief Reports that hint controls never capture pointer input.
     * @param pos Point in screen coordinates.
     * @return Always false, so hint controls never block scene input.
     */
    bool hitTest(const QPointF &pos) const override;

    /**
     * @brief Paints the hint using its current text and anchor.
     * @param ctx Paint context.
     * @param frame Current scene frame.
     */
    void paint(GLPaintContext &ctx, const SceneFrame &frame) override;

private:
    friend class HintControlTestAccess;

    static QRectF capsuleRect(const QPointF &anchor, const QSizeF &textSize, const QRectF &bounds);

    ImageProvider *m_provider = nullptr;
    QString m_text;
    QPointF m_anchor;
};

}
