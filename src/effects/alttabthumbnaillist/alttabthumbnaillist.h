// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <kwineffects.h>

#include <chrono>
#include <memory>

namespace KWin
{

class ThumbnailStrip;
class PreviewAnimator;

class AltTabThumbnailListEffect : public Effect
{
    Q_OBJECT

public:
    AltTabThumbnailListEffect();
    ~AltTabThumbnailListEffect() override;

    bool isActive() const override;

    void prePaintWindow(EffectWindow *window, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow *window, int mask, QRegion region, WindowPaintData &data) override;
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData &data) override;
    void postPaintScreen() override;

public Q_SLOTS:
    void slotTabboxAdded(int);
    void slotTabboxClosed();
    void slotTabboxAborted();
    void slotTabboxUpdated();

    void slotMouseChanged(const QPoint &pos, const QPoint &old,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);

    void slotWindowRemoved(EffectWindow *window);

private:
    /** @brief 根据焦点输出与工作区，计算切换器和大预览的本帧布局。 */
    void updateLayout();

    std::unique_ptr<ThumbnailStrip> m_thumbnailStrip;
    std::unique_ptr<PreviewAnimator> m_previewAnimator;

    bool m_tabBoxAborted = false;
    bool m_paintingWaterMark = false;
    EffectWindow *m_waterMarkWindow = nullptr;
};

} // namespace KWin
