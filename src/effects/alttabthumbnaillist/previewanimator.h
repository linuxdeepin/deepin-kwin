// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "alttabhelpers.h"

#include <kwineffects.h>

#include <QPointer>
#include <QVariant>
#include <QVector4D>

#include <memory>
#include <optional>
#include <vector>

namespace KWin
{

class PreviewAnimator
{
public:
    PreviewAnimator();
    ~PreviewAnimator();

    /**
     * @brief 创建一次 Alt+Tab 预览会话，并开始所有候选窗口的首次聚拢。
     *
     * 在这里冻结真实几何与临时 data role，后续动画只读取会话快照，避免 TabBox
     * 选中变化改变首次聚拢的起点。
     */
    void begin(const EffectWindowList &windows, EffectWindow *selected, EffectScreen *focus);
    void syncWindows(const EffectWindowList &windows);
    /** @brief 从当前即时状态重定向到新的输出布局，避免任务栏变化时跳帧。 */
    void setLayout(EffectScreen *focus, const QRect &area);
    /**
     * @brief 在稳定预览中同步执行旧窗口淡出与新窗口缩放淡入。
     *
     * @return 选中窗口实际变化并启动动画时返回 true。
     */
    bool select(EffectWindow *selected);
    void windowResized(EffectWindow *window);
    /** @brief 以当前插值结果为起点，让所有候选窗口回归各自最新真实几何。 */
    void close();
    /** @brief 立即结束预览会话，不绘制窗口回归或背景恢复动画。 */
    void cancel();
    void advance(std::chrono::milliseconds time);
    bool active() const;
    bool animating() const;

    /** @brief 返回预览会话需要应用到场景的背景亮度。 */
    qreal brightness() const;
    void remove(EffectWindow *window);
    void prePaint(EffectWindow *window, WindowPrePaintData &data);
    /** @brief 以即时矩形和透明度变换候选窗口；true 表示已接管绘制。 */
    bool paint(EffectWindow *window, int mask, QRegion region, WindowPaintData &data);
    /** @brief 在 Scene 完成普通窗口绘制后叠加关闭快照。 */
    void paintClosingSnapshot(ScreenPaintData &data);

private:
    /** Previewing 包括稳定、切换和重排；是否还在插值由时间线记录。 */
    enum class State { Inactive,
                       Gathering,
                       Previewing,
                       Closing };
    /** @brief 窗口轨道在给定进度下应绘制的矩形与透明度。 */
    struct VisualState
    {
        QRect geometry;
        qreal opacity = 1;
    };
    /** 预览窗口在本次会话中的渲染快照。 */
    struct WindowTrack
    {
        QPointer<EffectWindow> window;
        std::optional<EffectWindowVisibleRef> visibility;
        QRect source;
        QRect preview;
        QRect gather; // 原窗口在所属屏幕的聚拢终点；无效时不单独绘制原窗口。
        VisualState from;
        VisualState to;
        QVariant blur;
        QVariant temporaryRadius;
    };
    /** 最小化或覆盖输出窗口关闭时用于连续回归的内容与阴影快照。 */
    struct ClosingSnapshot
    {
        ~ClosingSnapshot();
        void capture(EffectWindow *target);
        void clear();
        void captureActiveShadowIfReady();
        void paintShadow(const ScreenPaintData &screen, const VisualState &state, qreal progress) const;
        void paintContent(const ScreenPaintData &screen, const VisualState &state) const;

        QPointer<EffectWindow> window;
        QRect frameGeometry;
        QRectF shadowGeometry;
        QVector4D inactiveShadowUV; // 混合区域纹理坐标到原始阴影纹理的缩放与偏移。
        qreal radius = 0;
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLTexture> shadowTexture;
        std::unique_ptr<GLTexture> activeShadowTexture;
        bool awaitingActiveShadow = false;
        std::unique_ptr<GLShader> shader;
        bool shaderFailed = false;
    };

    QRect gatherTarget(const WindowTrack &track, const QRect &focusPreview) const;
    VisualState gatheringWindow(const WindowTrack &track) const;
    WindowTrack *find(EffectWindow *window);
    WindowTrack *addWindow(EffectWindow *window);
    void freeze();
    void settle();
    qreal closeProgress(const WindowTrack &track) const;
    /** @brief 用给定进度对轨道的 from/to 与透明度插值；不推进时间线、不触碰窗口。 */
    VisualState sample(const WindowTrack &track, qreal progress) const;
    void restore(const WindowTrack &track);
    void clear();

    std::vector<WindowTrack> m_windows;
    QPointer<EffectWindow> m_selected;
    EffectScreen *m_focus = nullptr;
    QRect m_previewArea; // 保留最近布局，即使在两次 TabBox 会话之间。
    AltTabAnimation m_animation;
    AltTabAnimation m_gather;
    AltTabAnimation m_close;
    ClosingSnapshot m_snapshot;
    State m_state = State::Inactive;
};

} // namespace KWin
