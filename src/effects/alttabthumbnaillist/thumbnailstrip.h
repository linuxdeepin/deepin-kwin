// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "alttabhelpers.h"

#include <kwineffects.h>

#include <QPointer>

#include <memory>
#include <vector>

namespace KWin
{

class ThumbnailStrip
{
public:
    ThumbnailStrip();
    ~ThumbnailStrip();

    enum class Change { None,
                        Selection,
                        Windows };
    /**
     * @brief 同步 TabBox 候选项，复用仍在列表中的窗口缓存。
     */
    Change sync(const EffectWindowList &windows, EffectWindow *selected);
    void setRect(const QRect &rect);
    void appear();
    void disappear();
    /** @brief 立即隐藏缩略图条，不播放退出动画。 */
    void hideImmediately();
    void advance(std::chrono::milliseconds time);
    bool active() const;
    bool needsRepaint() const;
    qreal opacity() const;
    int offset() const;
    void dirty(EffectWindow *window);
    void decorationChanged(EffectWindow *window);
    void geometryChanged(EffectWindow *window, bool resized);
    void remove(EffectWindow *window);

    EffectWindow *hit(const QPoint &point) const;
    /** @brief 返回缩略图条当前占用的屏幕区域，用于局部重绘。 */
    QRect repaintArea() const;
    /**
     * @brief 绘制缩略图条，并将昂贵的离屏捕获限制在单帧预算内。
     *
     * 选中项优先刷新，后台项只在首次缺少缓存时按预算逐帧生成；已存在的
     * 后台缓存即使收到 damage 也继续绘制旧内容，避免普通窗口持续重绘拖慢切换器。
     */
    void paint(ScreenPaintData &data);

private:
    /** 单个缩略图的布局与 GPU 缓存。 */
    struct Item
    {
        void release();
        void ensureContent();
        void capture(std::chrono::steady_clock::time_point now);
        void rebuildDecoration();

        QPointer<EffectWindow> window;
        QRect frame;
        QRect thumbnail;
        bool dirty = true;
        bool decorationDirty = false;
        bool needsCapture = true; // 首次显示、缓存释放或尺寸变化；后台项也要刷新。
        std::chrono::steady_clock::time_point lastCapture;
        std::unique_ptr<GLFramebuffer> fbo;
        std::unique_ptr<GLTexture> content;
        std::unique_ptr<GLTexture> decoration;
    };
    enum class State { Hidden,
                       Appearing,
                       Visible,
                       Disappearing };

    Item *findItem(EffectWindow *window);
    void rebuildItems(const EffectWindowList &windows);
    bool hasPendingCapture() const;
    void updateValues();

    /** @brief 根据候选项总宽度更新实际可视页；内容不足一页时保持水平居中。 */
    void updateViewport();
    /** @brief 根据每个窗口当前几何重算卡片宽度，并连续排布所有候选项。 */
    void updateItemGeometries();
    void layoutItems();
    int listWidth() const;
    QRect onScreen(const QRect &rect, const QRect &visible) const;
    QRect onScreen(const QRect &rect) const;
    void setStripUniforms(GLShader *shader, const QRect &rect) const;
    void select();

    /** @brief 将一个窗口捕获到缩略图 FBO；选中项可绕过后台预算。 */
    void refresh(Item &item);
    /** @brief 创建窗口内容底部圆角所需的单象限 alpha 遮罩。 */
    void ensureScissorMask();
    void drawTexture(ScreenPaintData &data, GLTexture *texture, const QRect &rect, GLShader *shader, float widthScale = 1.0f);
    void drawSelection(ScreenPaintData &data);
    void drawItem(ScreenPaintData &data, Item &item);

    std::vector<Item> m_items;
    QPointer<EffectWindow> m_selected;
    QRect m_availableRect;
    QRect m_rect;
    QRect m_visible;
    QRect m_fromVisible;
    QRect m_toVisible;
    QRect m_selectedRect;
    QRect m_fromSelectedRect;
    QRect m_toSelectedRect;
    AltTabAnimation m_animation;
    AltTabAnimation m_scrollAnimation;
    AltTabAnimation m_selectAnimation;
    State m_state = State::Hidden;
    int m_fromOffset = 0;
    int m_toOffset = 0;
    int m_offset = 0;
    qreal m_fromOpacity = 0;
    qreal m_toOpacity = 0;
    qreal m_opacity = 0;
    bool m_backgroundCaptureAvailable = false;
    bool m_layoutDirty = false;
    std::unique_ptr<GLShader> m_clip;
    std::unique_ptr<GLShader> m_scissor;
    std::unique_ptr<GLTexture> m_scissorMask;
    std::unique_ptr<GLTexture> m_selection;
};

} // namespace KWin
