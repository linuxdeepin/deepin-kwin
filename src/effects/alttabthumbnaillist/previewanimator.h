// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "alttabhelpers.h"

#include <kwineffects.h>

#include <QPointer>
#include <QVariant>

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
    /** @brief 立即结束预览会话，不绘制窗口回归或背景恢复动画。 */
    void cancel();
    void advance(std::chrono::milliseconds time);
    bool active() const;
    bool animating() const;

    void remove(EffectWindow *window);

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
    };
    QRect gatherTarget(const WindowTrack &track, const QRect &focusPreview) const;
    VisualState gatheringWindow(const WindowTrack &track) const;
    WindowTrack *find(EffectWindow *window);
    WindowTrack *addWindow(EffectWindow *window);
    void freeze();
    void settle();
    /** @brief 用给定进度对轨道的 from/to 与透明度插值；不推进时间线、不触碰窗口。 */
    VisualState sample(const WindowTrack &track, qreal progress) const;
    void restore(const WindowTrack &track);
    void clear();

    std::vector<WindowTrack> m_windows;
    QPointer<EffectWindow> m_selected;
    EffectScreen *m_focus = nullptr;
    QRect m_previewArea; // 保留最近布局，即使在两次 TabBox 会话之间。
    Animation m_animation;
    Animation m_gather;
    Animation m_close;
    State m_state = State::Inactive;
};

} // namespace KWin
