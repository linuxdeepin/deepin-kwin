/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scene_model_state.h"
#include "view/glscene.h"
#include "view/windowcellcontrol.h"
#include "view/windowoverlaycontrol.h"
#include "view/workspacebarcontrol.h"

namespace KWin
{

inline QMap<QString, QString> sessionState(GLScene &s)
{
    QMap<QString, QString> out = sessionModelState(*s.model());
    out[QStringLiteral("scene.enterProgress")] = snapNum(s.enterProgress());
    out[QStringLiteral("scene.exitFinished")] = s.exitFinished() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.enterAnimRunning")] = s.enterAnimRunning() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.animating")] = s.animating() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.needsFrame")] = s.needsFrame() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.hasItems.bar")] = s.workspaceBar()->hasItems() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.hasItems.cell")] = s.windowCells()->hasItems() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.hasItems.overlay")] = s.windowOverlay()->hasItems() ? QStringLiteral("1") : QStringLiteral("0");
    return out;
}

inline QMap<QString, QString> barState(GLScene &s)
{
    QMap<QString, QString> out;
    const auto &nodes = s.barLayer().nodes();
    for (int i = 0; i < nodes.size(); ++i) {
        const QString key = QStringLiteral("bar.%1").arg(i);
        out[key + QStringLiteral(".rect")] = snapRect(nodes[i].rect());
        out[key + QStringLiteral(".opacity")] = snapNum(nodes[i].cur.opacity);
        out[key + QStringLiteral(".animating")] = nodes[i].animating() ? QStringLiteral("1") : QStringLiteral("0");
        out[key + QStringLiteral(".visible")] = nodes[i].cur.opacity > 0.01 ? QStringLiteral("1") : QStringLiteral("0");
        out[key + QStringLiteral(".key")] = nodes[i].key;
        out[key + QStringLiteral(".num")] = QString::number(s.model()->barItems().value(i).num);
        out[key + QStringLiteral(".uri")] = s.model()->barItems().value(i).uri;
        out[key + QStringLiteral(".cell")] = snapRect(nodes[i].cell);
        out[key + QStringLiteral(".current")] = nodes[i].current ? QStringLiteral("1") : QStringLiteral("0");
        out[key + QStringLiteral(".hovered")] = nodes[i].hovered ? QStringLiteral("1") : QStringLiteral("0");
        out[key + QStringLiteral(".raised")] = nodes[i].raised ? QStringLiteral("1") : QStringLiteral("0");
        out[key + QStringLiteral(".phRect")] = snapRect(nodes[i].placeholderRect());
        out[key + QStringLiteral(".phW")] = snapNum(nodes[i].placeholderW);
        out[key + QStringLiteral(".phTargetX")] = snapNum(nodes[i].placeholderTargetX);
        out[key + QStringLiteral(".phTargetW")] = snapNum(nodes[i].placeholderTargetW);
    }
    out[QStringLiteral("barctrl.hoveredSlot")] = QString::number(s.workspaceBar()->hoveredSlot());
    out[QStringLiteral("barctrl.addHovered")] = s.workspaceBar()->addHovered() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("barctrl.dragTipText")] = s.workspaceBar()->dragTipText();
    out[QStringLiteral("barctrl.pressedSlot")] = QString::number(s.workspaceBar()->pressedSlot());
    out[QStringLiteral("barctrl.pressed")] = s.workspaceBar()->pressed() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("barctrl.dragging")] = s.workspaceBar()->dragging() ? QStringLiteral("1") : QStringLiteral("0");
    return out;
}

inline QMap<QString, QString> gridState(GLScene &s)
{
    QMap<QString, QString> out = gridModelState(*s.model());
    auto *cells = s.windowCells();
    for (int i = 0; i < cells->cellCountPublic(); ++i) {
        const QString key = QStringLiteral("cell.%1").arg(i);
        out[key + QStringLiteral(".rect")] = snapRect(cells->coverRectAt(i));
        out[key + QStringLiteral(".opacity")] = QStringLiteral("1.00");
        out[key + QStringLiteral(".animating")] = QStringLiteral("0");
        out[key + QStringLiteral(".visible")] = QStringLiteral("1");
        out[key + QStringLiteral(".targetRect")] = snapRect(cells->coverRectAt(i));
        out[key + QStringLiteral(".targetOpacity")] = QStringLiteral("1.00");
        out[key + QStringLiteral(".cellRect")] = snapRect(cells->cellRectAt(i));
        out[key + QStringLiteral(".radius")] = snapNum(cells->cellRadiusAt(i));
    }
    for (int i = 0; i < cells->cellCountPublic(); ++i) {
        out[QStringLiteral("cellRects.%1").arg(i)] = snapRect(cells->cellRectAt(i));
    }
    out[QStringLiteral("cellctrl.pressedCell")] = QString::number(s.windowCells()->pressedCell());
    out[QStringLiteral("cellctrl.pressed")] = s.windowCells()->pressed() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("cellctrl.dragging")] = s.windowCells()->dragging() ? QStringLiteral("1") : QStringLiteral("0");
    return out;
}

inline QMap<QString, QString> windowState(GLScene &s)
{
    QMap<QString, QString> out = windowModelState(*s.model());
    const auto &nodes = s.windowItemLayer().nodes();
    for (int i = 0; i < nodes.size(); ++i) {
        const QString key = QStringLiteral("win.%1").arg(i);
        out[key + QStringLiteral(".rect")] = snapRect(nodes[i].rect());
        out[key + QStringLiteral(".opacity")] = snapNum(nodes[i].opacity());
        out[key + QStringLiteral(".animating")] = nodes[i].animating() ? QStringLiteral("1") : QStringLiteral("0");
        out[key + QStringLiteral(".visible")] = nodes[i].opacity() > 0.01 ? QStringLiteral("1") : QStringLiteral("0");
        out[key + QStringLiteral(".targetRect")] = snapRect(QRectF(nodes[i].interp.to.x, nodes[i].interp.to.y, nodes[i].interp.to.w, nodes[i].interp.to.h));
        out[key + QStringLiteral(".targetOpacity")] = snapNum(nodes[i].interp.to.opacity);
        out[key + QStringLiteral(".ovRect")] = snapRect(nodes[i].overlayRect());
        out[key + QStringLiteral(".ovOpacity")] = snapNum(nodes[i].overlayOpacity());
        out[key + QStringLiteral(".ovTarget")] = snapNum(nodes[i].overlayTarget);
        out[key + QStringLiteral(".ovAnimating")] = nodes[i].overlayAnimating() ? QStringLiteral("1") : QStringLiteral("0");
        out[key + QStringLiteral(".closeBtn")] = snapRect(nodes[i].closeButtonRect());
        out[key + QStringLiteral(".topBtn")] = snapRect(nodes[i].topButtonRect());
    }
    out[QStringLiteral("ovctrl.pressTop")] = QString::number(s.windowOverlay()->pressTop());
    out[QStringLiteral("ovctrl.pressClose")] = QString::number(s.windowOverlay()->pressClose());
    return out;
}

inline QMap<QString, QString> backgroundState(GLScene &s)
{
    QMap<QString, QString> out = backgroundModelState(*s.model());
    const SceneFrame fr = s.currentFrame();
    out[QStringLiteral("bg.sliding")] = fr.bgSliding ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("bg.slideProgress")] = snapNum(fr.bgSlideProgress);
    out[QStringLiteral("bg.slideDir")] = QString::number(int(fr.bgSlideDir));
    out[QStringLiteral("bg.slideFromUri")] = fr.bgSlideFromUri;
    out[QStringLiteral("bg.slideToUri")] = fr.bgSlideToUri;
    const GhostLayer &gl = s.ghostLayer();
    out[QStringLiteral("ghost.rect")] = snapRect(gl.rect());
    out[QStringLiteral("ghost.opacity")] = snapNum(gl.opacity());
    out[QStringLiteral("ghost.animating")] = gl.animating() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("ghost.visible")] = gl.opacity() > 0.01 ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("ghost.targetRect")] = snapRect(QRectF(gl.node().to.x, gl.node().to.y, gl.node().to.w, gl.node().to.h));
    out[QStringLiteral("ghost.targetOpacity")] = snapNum(gl.node().to.opacity);
    out[QStringLiteral("ghost.window")] = QString::number(reinterpret_cast<quintptr>(gl.window()));
    out[QStringLiteral("ghost.src")] = snapRect(gl.sourceGeometry());
    return out;
}

}
