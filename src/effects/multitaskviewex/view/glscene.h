/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "barlayer.h"
#include "ghostlayer.h"
#include "glcontrol.h"
#include "glpaintcontext.h"
#include "hintcontrol.h"
#include "model/backgrounditem.h"
#include "model/bardragmodel.h"
#include "model/baritem.h"
#include "model/scenemodel.h"
#include "model/windowdragsession.h"
#include "model/windowitem.h"
#include "outgoinglayer.h"
#include "windowitemlayer.h"
#include "windowoverlaycontrol.h"

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

namespace KWin
{

class GLControl;
class BackgroundControl;
class WorkspaceBarControl;
class WindowCellControl;
class DragGhostControl;
class WorkspaceBarMetrics;

class GLScene : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the scene and its control tree.
     * @param parent Parent object.
     */
    explicit GLScene(QObject *parent = nullptr);

    /**
     * @brief Destroys the scene.
     */
    ~GLScene() override;

    /**
     * @brief Binds the scene to a model.
     * @param model Scene model to read from.
     */
    void setModel(SceneModel *model);

    /**
     * @brief Returns the bound model.
     * @return The scene model, or nullptr when unbound.
     */
    SceneModel *model() const
    {
        return m_model;
    }

    /**
     * @brief Returns the background control.
     * @return The background control.
     */
    BackgroundControl *background() const
    {
        return m_background.get();
    }

    /**
     * @brief Returns the workspace bar control.
     * @return The workspace bar control.
     */
    WorkspaceBarControl *workspaceBar() const
    {
        return m_bar.get();
    }

    /**
     * @brief Returns the window cell control.
     * @return The window cell control.
     */
    WindowCellControl *windowCells() const
    {
        return m_cells.get();
    }

    /**
     * @brief Returns the empty-workspace hint control.
     * @return The empty-workspace hint control.
     */
    HintControl *emptyHint() const
    {
        return m_emptyHint.get();
    }

    /**
     * @brief Returns the window overlay control.
     * @return The window overlay control.
     */
    WindowOverlayControl *windowOverlay() const
    {
        return m_overlay.get();
    }

    /**
     * @brief Returns the hover window information hint control.
     * @return The hover window information hint control.
     */
    HintControl *windowInfo() const
    {
        return m_windowInfo.get();
    }

    /**
     * @brief Paints a single scene layer.
     * @param ctx Paint context.
     * @param layer Layer to paint.
     */
    void paintLayer(GLPaintContext &ctx, SceneLayer layer);

    /**
     * @brief Resets all session-scoped animation state.
     */
    void resetSession();

    /**
     * @brief Advances every animation by a frame delta.
     * @param dtMs Elapsed time in milliseconds.
     * @return True when any animation is still running.
     */
    bool advance(int dtMs);

    /**
     * @brief Tests whether the scene is animating.
     * @return True while any animation is running.
     */
    bool animating() const;

    /**
     * @brief Returns the mutable bar layer.
     * @return The bar layer.
     */
    BarLayer &barLayer()
    {
        return m_barLayer;
    }

    /**
     * @brief Returns the bar layer.
     * @return The bar layer.
     */
    const BarLayer &barLayer() const
    {
        return m_barLayer;
    }

    /**
     * @brief Rebuilds the bar layer nodes from the workspace order.
     * @param order Workspace ids in display order.
     * @param currentNum Current desktop number.
     * @param hoveredSlot Hovered slot index, or -1 when none.
     */
    void syncBarLayer(const QVector<int> &order, int currentNum, int hoveredSlot);

    /**
     * @brief Returns the mutable ghost layer.
     * @return The ghost layer.
     */
    GhostLayer &ghostLayer()
    {
        return m_ghostLayer;
    }

    /**
     * @brief Returns the ghost layer.
     * @return The ghost layer.
     */
    const GhostLayer &ghostLayer() const
    {
        return m_ghostLayer;
    }

    /**
     * @brief Starts the background wallpaper slide transition.
     * @param fromUri Wallpaper URI sliding out.
     * @param toUri Wallpaper URI sliding in.
     * @param dir Slide direction, positive or negative.
     */
    void startSlide(const QString &fromUri, const QString &toUri, int dir);

    /**
     * @brief Stops the background slide transition.
     */
    void resetSlide();

    /**
     * @brief Tests whether the background slide is running.
     * @return True while the slide is animating.
     */
    bool slideAnimating() const
    {
        return m_slideAnim.running();
    }

    /**
     * @brief Returns the background slide progress.
     * @return Progress in [0, 1].
     */
    qreal slideProgress() const
    {
        return m_slide;
    }

    /**
     * @brief Starts entering or leaving the view.
     * @param entered True to enter, false to leave.
     */
    void setEntered(bool entered);

    /**
     * @brief Resets the enter animation to its initial state.
     */
    void resetEnter();

    /**
     * @brief Returns the enter/exit progress.
     * @return Progress in [0, 1].
     */
    qreal enterProgress() const
    {
        return m_enterValue;
    }

    /**
     * @brief Tests whether the view is in the entered state.
     * @return True when entered.
     */
    bool entered() const
    {
        return m_entered;
    }

    /**
     * @brief Tests whether the exit animation has fully finished.
     * @return True when not entered and the value has reached zero.
     */
    bool exitFinished() const
    {
        return !m_entered && m_enterValue <= 0.0;
    }

    /**
     * @brief Tests whether the enter animation is running.
     * @return True while the enter animation is running.
     */
    bool enterAnimRunning() const
    {
        return m_enterAnim.running();
    }

    /**
     * @brief Returns the duration of the window change animation.
     * @return Duration in milliseconds.
     */
    int windowAnimDuration() const;

    /**
     * @brief Returns the mutable outgoing layer.
     * @return The outgoing layer.
     */
    OutgoingLayer &outgoingLayer()
    {
        return m_outgoingLayer;
    }

    /**
     * @brief Returns the outgoing layer.
     * @return The outgoing layer.
     */
    const OutgoingLayer &outgoingLayer() const
    {
        return m_outgoingLayer;
    }

    /**
     * @brief Returns the workspace bar horizontal scroll offset.
     * @return The scroll offset.
     */
    qreal barScrollOffset() const
    {
        return m_barScrollOffset;
    }

    /**
     * @brief Scrolls the workspace bar by a delta, clamped to its content.
     * @param delta Scroll delta.
     * @param viewportWidth Visible bar viewport width.
     * @return True when the offset actually changed.
     */
    bool scrollBarBy(qreal delta, qreal viewportWidth);

    /**
     * @brief Resets the workspace bar scroll offset to zero.
     */
    void resetBarScroll()
    {
        m_barScrollOffset = 0.0;
    }

    /**
     * @brief Returns the window grid scroll target.
     * @return The grid scroll target.
     */
    qreal gridScrollTarget() const
    {
        return m_gridScrollTarget;
    }

    /**
     * @brief Sets the window grid scroll target.
     * @param target New grid scroll target.
     */
    void setGridScrollTarget(qreal target)
    {
        m_gridScrollTarget = target;
    }

    /**
     * @brief Returns the maximum window grid scroll offset.
     * @return The grid max scroll.
     */
    qreal gridMaxScroll() const
    {
        return m_gridMaxScroll;
    }

    /**
     * @brief Sets the maximum window grid scroll offset.
     * @param maxScroll New grid max scroll.
     */
    void setGridMaxScroll(qreal maxScroll)
    {
        m_gridMaxScroll = maxScroll;
    }

    /**
     * @brief Returns the mutable window item layer.
     * @return The window item layer.
     */
    WindowItemLayer &windowItemLayer()
    {
        return m_windowItemLayer;
    }

    /**
     * @brief Returns the window item layer.
     * @return The window item layer.
     */
    const WindowItemLayer &windowItemLayer() const
    {
        return m_windowItemLayer;
    }

    /**
     * @brief Rebuilds the window item layer from the model.
     * @param enterOffsetX Horizontal offset used while entering.
     */
    void syncWindowLayer(qreal enterOffsetX = 0.0);

    /**
     * @brief Returns the topmost control containing a point.
     * @param pos Point in screen coordinates.
     * @return The hit control, or nullptr when none is hit.
     */
    GLControl *hitTest(const QPointF &pos) const;

    /**
     * @brief Routes a press to the hit control.
     * @param pos Press position.
     */
    void dispatchPress(const QPointF &pos);

    /**
     * @brief Routes a pointer move to the pressed control.
     * @param pos Pointer position.
     */
    void dispatchMove(const QPointF &pos);

    /**
     * @brief Routes a release to the pressed control.
     * @param pos Release position.
     */
    void dispatchRelease(const QPointF &pos);

    /**
     * @brief Tests whether a control is currently pressed.
     * @return True when a pressed control exists.
     */
    bool hasPressedControl() const;

    /**
     * @brief Cancels the active press without emitting any click.
     */
    void cancelPress();

public:
    /**
     * @brief Tests whether another frame is needed.
     * @return True while the scene is animating.
     */
    bool needsFrame() const
    {
        return animating();
    }

public:
    /**
     * @brief Returns the frame snapshot consumed by controls while painting.
     * @return The current scene frame.
     */
    SceneFrame currentFrame() const;

Q_SIGNALS:
    /**
     * @brief Emitted when the blank area of the scene is clicked.
     */
    void blankClicked();

private:
    std::vector<GLControl *> renderQueue() const;

    std::unique_ptr<BackgroundControl> m_background;
    std::unique_ptr<WindowCellControl> m_cells;
    std::unique_ptr<HintControl> m_emptyHint;
    std::unique_ptr<WorkspaceBarControl> m_bar;
    std::unique_ptr<DragGhostControl> m_ghost;
    std::unique_ptr<WindowOverlayControl> m_overlay;
    std::unique_ptr<HintControl> m_windowInfo;
    SceneModel *m_model = nullptr;
    BarLayer m_barLayer;
    GhostLayer m_ghostLayer;
    Animation m_slideAnim;
    qreal m_slideFrom = 1.0;
    qreal m_slide = 1.0;
    QString m_slideFromUri;
    QString m_slideToUri;
    qreal m_slideDir = 1.0;
    bool m_sliding = false;
    Animation m_enterAnim;
    qreal m_enterFrom = 0.0;
    qreal m_enterTo = 0.0;
    qreal m_enterValue = 0.0;
    bool m_entered = false;
    WindowItemLayer m_windowItemLayer;
    OutgoingLayer m_outgoingLayer;
    qreal m_barScrollOffset = 0.0;
    qreal m_gridScrollTarget = 0.0;
    qreal m_gridMaxScroll = 0.0;
    GLControl *m_pressedControl = nullptr;
    GLControl *m_hoveredControl = nullptr;
    QPointF m_pressPos;
};

}
