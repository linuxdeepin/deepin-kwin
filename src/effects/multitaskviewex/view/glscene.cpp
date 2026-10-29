/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "backgroundcontrol.h"
#include "data/workspacebarmetrics.h"
#include "dragghostcontrol.h"
#include "glscene.h"
#include "model/bardraglayout.h"
#include "model/bardragmodel.h"
#include "windowcellcontrol.h"
#include "workspacebarcontrol.h"

#include <algorithm>

namespace KWin
{

static MotionOptions motionOf(const SceneModel *model)
{
    return model ? model->motion() : MotionOptions{};
}

GLScene::GLScene(QObject *parent)
    : QObject(parent)
    , m_background(new BackgroundControl)
    , m_cells(new WindowCellControl)
    , m_bar(new WorkspaceBarControl)
    , m_overlay(new WindowOverlayControl)
    , m_ghost(new DragGhostControl)
{
    m_background->setParent(this);
    m_cells->setParent(this);
    m_overlay->setParent(this);
    m_bar->setParent(this);
    m_ghost->setParent(this);
}

void GLScene::setModel(SceneModel *model)
{
    m_model = model;
    if (!model) {
        return;
    }
    for (GLControl *c : renderQueue()) {
        c->attachModel(*model);
    }
    m_bar->setBarLayer(&m_barLayer);
    m_ghost->setGhostLayer(&m_ghostLayer);
    m_overlay->setWindowItemLayer(&m_windowItemLayer);
    m_cells->setWindowItemLayer(&m_windowItemLayer);
    m_cells->setOutgoingLayer(&m_outgoingLayer);
    m_cells->setScene(this);
}

GLScene::~GLScene() = default;

std::vector<GLControl *> GLScene::renderQueue() const
{
    std::vector<GLControl *> queue = {m_background.get(), m_cells.get(), m_bar.get(), m_overlay.get(), m_ghost.get()};
    std::sort(queue.begin(), queue.end(), [](GLControl *a, GLControl *b) {
        return a->z() < b->z();
    });
    return queue;
}

void GLScene::resetSession()
{
    if (m_model) {
        m_model->resetItems();
    }
    m_barLayer.clear();
    m_ghostLayer.clear();
    resetSlide();
    resetEnter();
    m_windowItemLayer.clear();
    m_outgoingLayer.clear();
    m_bar->resetInput();
    m_cells->resetInput();
    m_overlay->resetInput();
    m_pressedControl = nullptr;
    m_hoveredControl = nullptr;
}

void GLScene::paintLayer(GLPaintContext &ctx, SceneLayer layer)
{
    const SceneFrame frame = currentFrame();
    for (GLControl *c : renderQueue()) {
        if (c->layer() == layer) {
            c->paint(ctx, frame);
        }
    }
}

SceneFrame GLScene::currentFrame() const
{
    SceneFrame frame;
    frame.enterProgress = m_enterValue;
    frame.barShiftY = m_model ? -m_model->metrics().workspaceHeight() * (1.0 - frame.enterProgress) : 0.0;
    frame.bgSliding = m_sliding;
    frame.bgSlideProgress = m_slide;
    frame.bgSlideFromUri = m_slideFromUri;
    frame.bgSlideToUri = m_slideToUri;
    frame.bgSlideDir = m_slideDir;
    return frame;
}

GLControl *GLScene::hitTest(const QPointF &pos) const
{
    auto queue = renderQueue();
    for (auto it = queue.rbegin(); it != queue.rend(); ++it) {
        GLControl *c = *it;
        if (c->hitTest(pos)) {
            return c;
        }
    }
    return nullptr;
}

void GLScene::dispatchPress(const QPointF &pos)
{
    GLControl *c = hitTest(pos);
    m_pressPos = pos;
    m_pressedControl = c;
    if (c) {
        c->press(pos);
    }
}

void GLScene::dispatchMove(const QPointF &pos)
{
    if (m_pressedControl) {
        m_pressedControl->move(pos);
        return;
    }
    GLControl *c = hitTest(pos);
    if (c != m_hoveredControl) {
        if (m_hoveredControl) {
            m_hoveredControl->move(pos);
        }
        m_hoveredControl = c;
    }
    if (c) {
        c->move(pos);
    }
}

void GLScene::dispatchRelease(const QPointF &pos)
{
    if (m_pressedControl) {
        m_pressedControl->release(pos);
        m_pressedControl = nullptr;
        return;
    }
    if (!hitTest(pos)) {
        Q_EMIT blankClicked();
    }
}

bool GLScene::advance(int dtMs)
{
    if (m_enterAnim.running()) {
        m_enterAnim.advance(dtMs);
        m_enterValue = m_enterFrom + (m_enterTo - m_enterFrom) * m_enterAnim.value();
        if (m_enterAnim.done()) {
            m_enterValue = m_enterTo;
        }
    }
    if (m_model) {
        m_barLayer.advance(dtMs);
        m_ghostLayer.advance(dtMs);
        m_windowItemLayer.advance(dtMs);
        if (m_slideAnim.running()) {
            m_slideAnim.advance(dtMs);
            m_slide = m_slideFrom + (1.0 - m_slideFrom) * m_slideAnim.value();
            if (m_slideAnim.done()) {
                m_slide = 1.0;
                m_sliding = false;
            }
        }
        m_outgoingLayer.advance(dtMs);
    }
    return animating();
}

void GLScene::startSlide(const QString &fromUri, const QString &toUri, int dir)
{
    m_sliding = true;
    m_slideFromUri = fromUri;
    m_slideToUri = toUri;
    m_slideDir = dir;
    m_slideFrom = 0.0;
    m_slide = 0.0;
    const MotionOptions motion = motionOf(m_model);
    m_slideAnim.configure(resolveMs(motion.workspaceMoveDurationMs, motion.animationSpeedMultiplier), Easing::OutCubic);
    m_slideAnim.start();
}

void GLScene::resetSlide()
{
    m_slideAnim.reset();
    m_sliding = false;
    m_slideFrom = 1.0;
    m_slide = 1.0;
}

void GLScene::setEntered(bool entered)
{
    m_entered = entered;
    const qreal to = entered ? 1.0 : 0.0;
    if (qFuzzyCompare(m_enterValue, to)) {
        return;
    }
    m_enterFrom = m_enterValue;
    m_enterTo = to;
    const MotionOptions motion = motionOf(m_model);
    m_enterAnim.configure(int(resolveMs(motion.viewEnterDurationMs, motion.animationSpeedMultiplier) * qAbs(to - m_enterValue)), Easing::OutQuint);
    m_enterAnim.start();
}

void GLScene::resetEnter()
{
    m_enterAnim.reset();
    m_enterValue = 0.0;
    m_entered = false;
}

int GLScene::windowAnimDuration() const
{
    const MotionOptions motion = motionOf(m_model);
    if (m_enterAnim.running()) {
        return resolveMs(motion.windowChangeDurationMs, motion.animationSpeedMultiplier);
    }
    if (m_model && m_model->dragPlaceholder() >= 0) {
        return resolveMs(motion.windowChangeDurationMs, motion.animationSpeedMultiplier);
    }
    return resolveMs(kWindowQuickMs, motion.animationSpeedMultiplier);
}

bool GLScene::scrollBarBy(qreal delta, qreal viewportWidth)
{
    if (!m_model) {
        return false;
    }
    const qreal content = m_model->metrics().contentWidth(m_model->barItems().size());
    const qreal maxScroll = barMaxScroll(content, viewportWidth);
    const qreal next = qBound<qreal>(0.0, m_barScrollOffset + delta, maxScroll);
    if (qFuzzyCompare(next, m_barScrollOffset)) {
        return false;
    }
    m_barScrollOffset = next;
    return true;
}

void GLScene::syncBarLayer(const QVector<int> &order, int currentNum, int hoveredSlot)
{
    if (!m_model) {
        return;
    }
    const auto &items = m_model->barItems();
    const BarItem *preview = m_model->barPreviewItem();
    const bool dragActive = m_model->barDragModel().isActive();
    const MotionOptions motion = motionOf(m_model);
    const int dur = dragActive ? resolveMs(kBarReorderDragMs, motion.animationSpeedMultiplier)
                               : resolveMs(motion.workspaceMoveDurationMs, motion.animationSpeedMultiplier);
    const Easing ease = Easing::InOutCubic;
    BarLayer::LayoutInput in;
    in.drag = &m_model->barDragModel();
    in.metrics = &m_model->metrics();
    in.order = &order;
    in.currentNum = currentNum;
    in.hoveredSlot = hoveredSlot;
    in.origin = QPointF(m_model->layout().offX, m_model->layout().offY);
    in.canvasWidth = m_model->layout().fullRect.width();
    in.scrollOffset = m_barScrollOffset;
    in.previewActive = m_model->barPreviewActive();
    in.dropRaiseKey = m_model->barDropRaiseKey();
    in.ghostFadeMs = resolveMs(kBarReorderDragMs, motion.animationSpeedMultiplier);
    m_barLayer.apply(items, preview, dur, ease, dragActive, in);
}

void GLScene::syncWindowLayer(qreal enterOffsetX)
{
    if (!m_model) {
        return;
    }
    const Easing ease = entered() ? Easing::OutCubic : Easing::OutQuint;
    WindowItemLayer::AlignInput in;
    in.cellBase = &m_model->cellBase();
    in.scrollOffset = m_gridScrollTarget;
    in.hoveredIndex = m_model->hoverCell();
    in.entered = entered();
    in.enterOffsetX = enterOffsetX;
    in.placeholderCell = m_model->dragPlaceholder();
    const MotionOptions motion = motionOf(m_model);
    in.retireMs = resolveMs(motion.windowChangeDurationMs, motion.animationSpeedMultiplier);
    in.overlayMs = resolveMs(kOverlayFadeMs, motion.animationSpeedMultiplier);
    m_windowItemLayer.apply(m_model->windowItems(), windowAnimDuration(), ease, in);
}

bool GLScene::animating() const
{
    return m_barLayer.animating()
        || m_ghostLayer.animating()
        || m_outgoingLayer.animating()
        || m_windowItemLayer.animating()
        || m_slideAnim.running()
        || m_enterAnim.running();
}

}
