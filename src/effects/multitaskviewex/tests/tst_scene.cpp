/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "data/workspacebarmetrics.h"
#include "model/bardraglayout.h"
#include "model/bardragmodel.h"
#include "model/motionoptions.h"
#include "model/screenlayout.h"
#include "model/wallpaperdata.h"
#include "model/windowgrid.h"
#include "model/windowgrouping.h"
#include "scene_state.h"
#include "view/backgroundcontrol.h"
#include "view/glpaintcontext.h"
#include "view/glpaintrenderer.h"
#include "view/glscene.h"
#include "view/hintcontrol.h"
#include "view/imageprovider.h"
#include "view/layers.h"
#include "view/screenview.h"
#include "view/screenviewset.h"
#include "view/windowcellcontrol.h"
#include "view/windowoverlaycontrol.h"
#include "view/workspacebarcontrol.h"

#include <QtTest>

using namespace KWin;

namespace KWin
{
class HintControlTestAccess
{
public:
    static QRectF capsuleRect(const QPointF &anchor, const QSizeF &textSize, const QRectF &bounds)
    {
        return HintControl::capsuleRect(anchor, textSize, bounds);
    }
};
}

inline QVector<int> orderNums(const QVector<QPair<QString, int>> &kn)
{
    QVector<int> nums;
    nums.reserve(kn.size());
    for (const auto &p : kn) {
        nums.append(p.second);
    }
    return nums;
}

class TestScene : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void hitRangeUsesCellNotCover();
    void barSlotHitIncludesDeleteButton();
    void needsFrameReflectsBarLayerAnimation();
    void barAnimConvergesUnderPerFrameSync();
    void sceneLayerOrderIsUniqueAndControlLayersMatch();
    void emptyHintNeverSwallowsClicks();
    void cancelPressDropsPendingClick();
    void windowCellCancelStopsActiveDrag();
    void workspaceBarCancelDropsPendingClick();
    void workspaceBarCancelStopsActiveDrag();
    void windowOverlayCancelDropsButtonClick();
    void windowInfoNeverSwallowsClicks();
    void hintControlStoresValues();
    void hintCapsuleRectClampsOrCenters();
    void wallpaperFadesWithEnterProgress();
    void barFadesInBeforeViewEnter();
    void barFadeHandlesSameFrameReversal();
    void dragDeleteSwitchKeepsConsistency();
    void hoverHitIncludesCornerButtons();
    void overlayButtonsSkipRetiring();
    void localCoordsOriginAndBarShiftY();
    void imageResourceCachesBySizeAndReleases();
    void imageResourcesReleaseOnSessionEnd();
    void resourcesSurviveSessionEndWithoutTrim();
    void pinnedResourcesSurviveTrimUntilUnpinned();
    void textureFilterPlanNeverMipmapsWithoutGeneration();
    void wallpaperLifecycleUnpinsNonCurrent();
    void providerOpsLeaveSceneUntouched();
    void gridOpsLeaveBarUntouched();
    void predecodeFeedsTextureForAndDeduplicates();
    void textureForReadyIsNonBlocking();
    void screenViewSetLifecycleIsStable();
    void projectionDoesNotAccumulateAcrossPasses();
    void windowInGridCoverDerivesFromWindow();
    void placeholderHidesWindowCoverAndOverlay();
    void backgroundSlideProgressAndReset();
    void workspaceSlideKeepsWindowsInOwnCells();
    void barScrollClampedByViewport();
};

void TestScene::hitRangeUsesCellNotCover()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300)};
    scene.model()->syncCellItems(cells, 705.0);
    scene.setGridScrollTarget(0.0);
    scene.advance(600);

    QCOMPARE(scene.windowCells()->cellRectAt(0), cells[0]);
    QCOMPARE(scene.windowCells()->coverRectAt(0), coverFor(cells[0], 705.0));
    QVERIFY2(scene.windowCells()->coverRectAt(0).height() > scene.windowCells()->cellRectAt(0).height(),
             "this case requires cover to be taller than cell to be meaningful");

    const auto before = gridState(scene);
    scene.setGridScrollTarget(120.0);
    expectDiff(before, gridState(scene), changes({
                                             {"cell.0.cellRect", "100.00,520.00,800.00,300.00"},
                                             {"cell.0.rect", "100.00,317.50,800.00,705.00"},
                                             {"cell.0.targetRect", "100.00,317.50,800.00,705.00"},
                                             {"cellRects.0", "100.00,520.00,800.00,300.00"},
                                         }),
               "10-scroll");
    QCOMPARE(scene.windowCells()->cellRectAt(0), cells[0].translated(0, 120.0));
    QCOMPARE(scene.windowCells()->coverRectAt(0), coverFor(cells[0].translated(0, 120.0), 705.0));
}

void TestScene::barSlotHitIncludesDeleteButton()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    WorkspaceBarMetrics metrics;
    metrics.setSize(QSizeF(3840, 2160));
    const QVector<QPair<QString, int>> order{{"d1", 1}, {"d2", 2}};
    const QPointF origin(0, 0);
    WorkspaceBarDragModel drag;
    WorkspaceBarDragModel::Config cfg;
    cfg.workspaceWidth = 480;
    cfg.workspaceHeight = 338;
    cfg.spacing = 80;
    cfg.listX = 1120;
    drag.setConfig(cfg);

    const auto before = merge({barState(scene), sessionState(scene)});
    scene.model()->syncBarItems(order, QHash<QString, QString>());
    scene.model()->barDragModel() = drag;
    scene.syncBarLayer(orderNums(order), 1, -1);
    scene.advance(400);
    expectDiff(before, merge({barState(scene), sessionState(scene)}), changes({
                                                                          {"bar.0.animating", "0"},
                                                                          {"bar.0.cell", "1120.00,40.00,480.00,270.00"},
                                                                          {"bar.0.current", "1"},
                                                                          {"bar.0.hovered", "0"},
                                                                          {"bar.0.key", "d1"},
                                                                          {"bar.0.num", "1"},
                                                                          {"bar.0.opacity", "1.00"},
                                                                          {"bar.0.phRect", "240.00,40.00,0.00,270.00"},
                                                                          {"bar.0.phTargetW", "0.00"},
                                                                          {"bar.0.phTargetX", "0.00"},
                                                                          {"bar.0.phW", "0.00"},
                                                                          {"bar.0.raised", "0"},
                                                                          {"bar.0.rect", "1120.00,40.00,480.00,270.00"},
                                                                          {"bar.0.uri", ""},
                                                                          {"bar.0.visible", "1"},
                                                                          {"bar.1.animating", "0"},
                                                                          {"bar.1.cell", "1680.00,40.00,480.00,270.00"},
                                                                          {"bar.1.current", "0"},
                                                                          {"bar.1.hovered", "0"},
                                                                          {"bar.1.key", "d2"},
                                                                          {"bar.1.num", "2"},
                                                                          {"bar.1.opacity", "1.00"},
                                                                          {"bar.1.phRect", "240.00,40.00,0.00,270.00"},
                                                                          {"bar.1.phTargetW", "0.00"},
                                                                          {"bar.1.phTargetX", "0.00"},
                                                                          {"bar.1.phW", "0.00"},
                                                                          {"bar.1.raised", "0"},
                                                                          {"bar.1.rect", "1680.00,40.00,480.00,270.00"},
                                                                          {"bar.1.uri", ""},
                                                                          {"bar.1.visible", "1"},
                                                                          {"scene.animating", "0"},
                                                                          {"scene.enterAnimRunning", "0"},
                                                                          {"scene.enterProgress", "1.00"},
                                                                          {"scene.needsFrame", "0"},
                                                                      }),
               "12-barHit-setup");

    const QRectF item = scene.barLayer().nodes()[0].rect();
    QVERIFY(!item.isEmpty());
    const QPointF onDeleteBtn(item.right() - 5, item.top() - 5);
    QVERIFY2(!item.contains(onDeleteBtn), "case precondition: the delete button lies outside the bar item");
    QCOMPARE(scene.workspaceBar()->slotAt(onDeleteBtn), 0);
    QVERIFY(scene.workspaceBar()->hitTest(onDeleteBtn));
    QCOMPARE(scene.workspaceBar()->slotAt(item.center()), 0);
    QCOMPARE(scene.workspaceBar()->slotAt(scene.barLayer().nodes()[1].rect().center()), 1);
}

void TestScene::needsFrameReflectsBarLayerAnimation()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    WorkspaceBarMetrics metrics;
    metrics.setSize(QSizeF(3840, 2160));
    WorkspaceBarDragModel cfgModel;
    WorkspaceBarDragModel::Config cfg;
    cfg.workspaceWidth = 480;
    cfg.workspaceHeight = 338;
    cfg.spacing = 80;
    cfg.listX = 1120;
    cfgModel.setConfig(cfg);
    const QVector<QPair<QString, int>> order{{"d1", 1}, {"d2", 2}};

    scene.model()->syncBarItems(order, QHash<QString, QString>());
    scene.model()->barDragModel() = cfgModel;
    scene.syncBarLayer({1, 2}, 1, -1);
    scene.advance(2000);
    QVERIFY2(!scene.needsFrame(), "initial placement is a snap; after enter converges no frames should be requested");

    scene.model()->barDragModel() = cfgModel;
    scene.syncBarLayer({2, 1}, 1, -1);
    QVERIFY2(scene.needsFrame(), "during a reorder transition needsFrame must be true (the controller renders frames from it)");

    scene.advance(kDefaultWorkspaceMoveDurationMs);
    QVERIFY2(!scene.needsFrame(), "after the transition converges needsFrame must be false (stop idle frame requests)");
}

void TestScene::barAnimConvergesUnderPerFrameSync()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    WorkspaceBarMetrics metrics;
    metrics.setSize(QSizeF(3840, 2160));
    WorkspaceBarDragModel cfgModel;
    WorkspaceBarDragModel::Config cfg;
    cfg.workspaceWidth = 480;
    cfg.workspaceHeight = 338;
    cfg.spacing = 80;
    cfg.listX = 1120;
    cfgModel.setConfig(cfg);
    const QVector<QPair<QString, int>> order{{"d1", 1}, {"d2", 2}};
    scene.model()->syncBarItems(order, QHash<QString, QString>());

    for (int i = 0; i < 200; ++i) {
        scene.model()->barDragModel() = cfgModel;
        scene.syncBarLayer({1, 2}, 1, -1);
        scene.advance(16);
    }
    QVERIFY2(!scene.needsFrame(), "after steady state no frames should be requested");

    for (int i = 0; i < 75; ++i) {
        scene.model()->barDragModel() = cfgModel;
        scene.syncBarLayer({2, 1}, 1, -1);
        scene.advance(16);
    }
    QVERIFY2(!scene.needsFrame(), "under per-frame apply the transition must converge (otherwise endless frames and an extremely slow animation)");
    QCOMPARE(scene.barLayer().nodes()[0].rect().x(), qreal(1680));
    QCOMPARE(scene.barLayer().nodes()[1].rect().x(), qreal(1120));
}

void TestScene::sceneLayerOrderIsUniqueAndControlLayersMatch()
{
    const auto order = sceneLayerOrder();
    QCOMPARE(order.size(), 7u);
    for (size_t i = 1; i < order.size(); ++i) {
        QVERIFY2(layerZ(order[i - 1]) < layerZ(order[i]),
                 qPrintable(QStringLiteral("layer order is not strictly ascending: %1 >= %2")
                                .arg(layerZ(order[i - 1]))
                                .arg(layerZ(order[i]))));
    }
    for (size_t i = 0; i < order.size(); ++i) {
        for (size_t j = i + 1; j < order.size(); ++j) {
            QVERIFY(order[i] != order[j]);
        }
    }
    QVERIFY(layerZ(SceneLayer::Cells) < layerZ(SceneLayer::Windows));
    QVERIFY(layerZ(SceneLayer::Windows) < layerZ(SceneLayer::Hint));
    QVERIFY(layerZ(SceneLayer::Hint) < layerZ(SceneLayer::Bar));
    QVERIFY(layerZ(SceneLayer::Bar) < layerZ(SceneLayer::Overlay));
    QVERIFY(layerZ(SceneLayer::Overlay) < layerZ(SceneLayer::Ghost));

    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    QCOMPARE(scene.background()->layer(), SceneLayer::Background);
    QCOMPARE(scene.windowCells()->layer(), SceneLayer::Cells);
    QCOMPARE(scene.emptyHint()->layer(), SceneLayer::Hint);
    QCOMPARE(scene.workspaceBar()->layer(), SceneLayer::Bar);
    QCOMPARE(scene.windowOverlay()->layer(), SceneLayer::Overlay);
    QCOMPARE(scene.windowInfo()->layer(), SceneLayer::Hint);
    QVERIFY(scene.emptyHint()->z() > scene.windowCells()->z());
    QVERIFY(scene.windowInfo()->z() > scene.windowCells()->z());
    QVERIFY(scene.workspaceBar()->z() > scene.emptyHint()->z());
    QVERIFY(scene.workspaceBar()->z() > scene.windowInfo()->z());
    QVERIFY(scene.workspaceBar()->z() > scene.windowCells()->z());
    QVERIFY(scene.windowOverlay()->z() > scene.workspaceBar()->z());
    QVERIFY(scene.windowOverlay()->z() > scene.windowInfo()->z());
    QCOMPARE(scene.background()->z(), layerZ(SceneLayer::Background));
    QCOMPARE(scene.windowCells()->z(), layerZ(SceneLayer::Cells));
    QCOMPARE(scene.emptyHint()->z(), layerZ(SceneLayer::Hint));
    QCOMPARE(scene.workspaceBar()->z(), layerZ(SceneLayer::Bar));
    QCOMPARE(scene.windowOverlay()->z(), layerZ(SceneLayer::Overlay));
    QCOMPARE(scene.windowInfo()->z(), layerZ(SceneLayer::Hint));
    QCOMPARE(scene.background()->hitTest(QPointF(0, 0)), false);
}

void TestScene::emptyHintNeverSwallowsClicks()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(1920, 1080),
                              QRectF(0, 0, 1920, 1080), QRectF(0, 0, 1920, 1080), {});
    scene.emptyHint()->setGeometry(scene.model()->layout().gridArea);
    QVERIFY(!scene.emptyHint()->geometry().isEmpty());
    QCOMPARE(scene.emptyHint()->hitTest(scene.model()->layout().gridArea.center()), false);
    QCOMPARE(scene.emptyHint()->hitTest(QPointF(-10, -10)), false);

    QSignalSpy blankSpy(&scene, &GLScene::blankClicked);
    scene.dispatchPress(scene.model()->layout().gridArea.center());
    scene.dispatchRelease(scene.model()->layout().gridArea.center());
    QCOMPARE(blankSpy.count(), 1);
}

void TestScene::cancelPressDropsPendingClick()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300)};
    scene.model()->syncCellItems(cells, 705.0);
    scene.setGridScrollTarget(0.0);
    scene.advance(600);

    QSignalSpy cellSpy(scene.windowCells(), &WindowCellControl::cellClicked);
    QSignalSpy dragSpy(scene.windowCells(), &WindowCellControl::cellDragStarted);
    QSignalSpy blankSpy(&scene, &GLScene::blankClicked);

    const QPointF center = cells[0].center();
    scene.dispatchPress(center);
    QVERIFY(scene.hasPressedControl());

    scene.cancelPress();
    QVERIFY(!scene.hasPressedControl());

    scene.dispatchMove(center + QPointF(200, 100));
    scene.dispatchRelease(center);

    QCOMPARE(cellSpy.count(), 0);
    QCOMPARE(dragSpy.count(), 0);
    QCOMPARE(blankSpy.count(), 0);

    scene.dispatchPress(center);
    scene.dispatchRelease(center);
    QCOMPARE(cellSpy.count(), 1);
}

void TestScene::windowCellCancelStopsActiveDrag()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300)};
    scene.model()->syncCellItems(cells, 705.0);
    scene.setGridScrollTarget(0.0);
    scene.advance(600);

    QSignalSpy clickSpy(scene.windowCells(), &WindowCellControl::cellClicked);
    QSignalSpy dragStartedSpy(scene.windowCells(), &WindowCellControl::cellDragStarted);
    QSignalSpy dragFinishedSpy(scene.windowCells(), &WindowCellControl::cellDragFinished);

    const QPointF center = cells[0].center();
    scene.dispatchPress(center);
    scene.dispatchMove(center + QPointF(200, 100));
    QCOMPARE(dragStartedSpy.count(), 1);
    QVERIFY(scene.windowCells()->dragging());

    scene.cancelPress();
    QVERIFY(!scene.windowCells()->dragging());
    QVERIFY(!scene.hasPressedControl());

    scene.dispatchRelease(center + QPointF(200, 100));
    QCOMPARE(dragFinishedSpy.count(), 0);
    QCOMPARE(clickSpy.count(), 0);
}

void TestScene::workspaceBarCancelDropsPendingClick()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    WorkspaceBarDragModel::Config cfg;
    cfg.workspaceWidth = 480;
    cfg.workspaceHeight = 338;
    cfg.spacing = 80;
    cfg.listX = 1120;
    WorkspaceBarDragModel drag;
    drag.setConfig(cfg);
    const QVector<QPair<QString, int>> order{{"d1", 1}, {"d2", 2}};
    scene.model()->syncBarItems(order, QHash<QString, QString>());
    scene.model()->barDragModel() = drag;
    scene.syncBarLayer({1, 2}, 1, -1);
    scene.advance(400);

    const QRectF item = scene.barLayer().nodes()[0].rect();
    QVERIFY(!item.isEmpty());
    QSignalSpy clickSpy(scene.workspaceBar(), &WorkspaceBarControl::slotClicked);
    QSignalSpy dragStartedSpy(scene.workspaceBar(), &WorkspaceBarControl::slotDragStarted);
    QSignalSpy dragFinishedSpy(scene.workspaceBar(), &WorkspaceBarControl::slotDragFinished);

    scene.dispatchPress(item.center());
    QVERIFY(scene.hasPressedControl());

    scene.cancelPress();
    QVERIFY(!scene.hasPressedControl());

    scene.dispatchRelease(item.center());
    QCOMPARE(clickSpy.count(), 0);
    QCOMPARE(dragStartedSpy.count(), 0);
    QCOMPARE(dragFinishedSpy.count(), 0);

    scene.dispatchPress(item.center());
    scene.dispatchRelease(item.center());
    QCOMPARE(clickSpy.count(), 1);
}

void TestScene::workspaceBarCancelStopsActiveDrag()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    WorkspaceBarDragModel::Config cfg;
    cfg.workspaceWidth = 480;
    cfg.workspaceHeight = 338;
    cfg.spacing = 80;
    cfg.listX = 1120;
    WorkspaceBarDragModel drag;
    drag.setConfig(cfg);
    const QVector<QPair<QString, int>> order{{"d1", 1}, {"d2", 2}};
    scene.model()->syncBarItems(order, QHash<QString, QString>());
    scene.model()->barDragModel() = drag;
    scene.syncBarLayer({1, 2}, 1, -1);
    scene.advance(400);

    const QRectF item = scene.barLayer().nodes()[0].rect();
    QVERIFY(!item.isEmpty());
    QSignalSpy clickSpy(scene.workspaceBar(), &WorkspaceBarControl::slotClicked);
    QSignalSpy dragStartedSpy(scene.workspaceBar(), &WorkspaceBarControl::slotDragStarted);
    QSignalSpy dragMovedSpy(scene.workspaceBar(), &WorkspaceBarControl::slotDragMoved);
    QSignalSpy dragFinishedSpy(scene.workspaceBar(), &WorkspaceBarControl::slotDragFinished);

    scene.dispatchPress(item.center());
    scene.dispatchMove(item.center() + QPointF(100, 0));
    QCOMPARE(dragStartedSpy.count(), 1);
    QVERIFY(dragMovedSpy.count() >= 1);
    QVERIFY(scene.workspaceBar()->dragging());

    const int movedBeforeCancel = dragMovedSpy.count();
    scene.cancelPress();
    QVERIFY(!scene.workspaceBar()->dragging());
    QVERIFY(!scene.hasPressedControl());

    scene.dispatchMove(item.center() + QPointF(200, 0));
    QCOMPARE(dragMovedSpy.count(), movedBeforeCancel);
    scene.dispatchRelease(item.center() + QPointF(200, 0));
    QCOMPARE(dragFinishedSpy.count(), 0);
    QCOMPARE(clickSpy.count(), 0);
}

void TestScene::windowOverlayCancelDropsButtonClick()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.setEntered(true);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300)};
    EffectWindow *w0 = reinterpret_cast<EffectWindow *>(0x7000);
    const QVector<EffectWindow *> wins{w0};
    const QVector<QRectF> sources{QRectF(0, 0, 1600, 900)};
    scene.model()->syncCellItems(cells, 300.0);
    scene.model()->syncWindows(wins, sources);
    scene.setGridScrollTarget(0.0);
    scene.model()->setHoverCell(0);
    scene.syncWindowLayer();
    scene.advance(800);

    auto *ov = scene.windowOverlay();
    QVERIFY(ov->hasItems());
    const QRectF cb = scene.windowItemLayer().nodes()[0].closeButtonRect();
    QVERIFY2(ov->hitTest(cb.center()), "precondition: the close button must be hit");

    QSignalSpy topSpy(ov, &WindowOverlayControl::topClicked);
    QSignalSpy closeSpy(ov, &WindowOverlayControl::closeClicked);

    scene.dispatchPress(cb.center());
    QVERIFY(scene.hasPressedControl());

    scene.cancelPress();
    QVERIFY(!scene.hasPressedControl());

    scene.dispatchRelease(cb.center());
    QCOMPARE(topSpy.count(), 0);
    QCOMPARE(closeSpy.count(), 0);

    scene.dispatchPress(cb.center());
    scene.dispatchRelease(cb.center());
    QCOMPARE(closeSpy.count(), 1);
}

void TestScene::windowInfoNeverSwallowsClicks()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(1920, 1080),
                              QRectF(0, 0, 1920, 1080), QRectF(0, 0, 1920, 1080), {});
    scene.windowInfo()->setGeometry(scene.model()->layout().fullRect);
    QCOMPARE(scene.windowInfo()->hitTest(scene.model()->layout().gridArea.center()), false);
    QCOMPARE(scene.windowInfo()->hitTest(QPointF(-10, -10)), false);
}

void TestScene::hintControlStoresValues()
{
    HintControl hint;
    QVERIFY(hint.text().isEmpty());

    hint.setText(QStringLiteral("Hint text"));
    hint.setAnchor(QPointF(60, 35));

    QCOMPARE(hint.text(), QStringLiteral("Hint text"));
    QCOMPARE(hint.anchor(), QPointF(60, 35));

    hint.setText(QString());
    QVERIFY(hint.text().isEmpty());
}

void TestScene::hintCapsuleRectClampsOrCenters()
{
    const QSizeF textSize(50, 20);
    const QRectF bounds(0, 0, 200, 200);

    QCOMPARE(HintControlTestAccess::capsuleRect(QPointF(100, 100), textSize, bounds),
             QRectF(59, 82, 82, 36));
    QCOMPARE(HintControlTestAccess::capsuleRect(QPointF(0, 0), textSize, bounds),
             QRectF(0, 0, 82, 36));
    QCOMPARE(HintControlTestAccess::capsuleRect(QPointF(200, 200), textSize, bounds),
             QRectF(118, 164, 82, 36));
    QCOMPARE(HintControlTestAccess::capsuleRect(QPointF(100, 100), textSize, QRectF()),
             QRectF(59, 82, 82, 36));
    QCOMPARE(HintControlTestAccess::capsuleRect(QPointF(5, 5), textSize, QRectF(0, 0, 10, 10)),
             QRectF(-36, -13, 82, 36));
}

void TestScene::wallpaperFadesWithEnterProgress()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    QCOMPARE(scene.enterProgress(), 0.0);

    const auto before = sessionState(scene);
    scene.setEntered(true);
    scene.advance(120);
    expectDiff(before, sessionState(scene), changes({
                                                {"scene.animating", "1"},
                                                {"scene.enterAnimRunning", "1"},
                                                {"scene.enterProgress", "0.83"},
                                                {"scene.exitFinished", "0"},
                                                {"scene.needsFrame", "1"},
                                            }),
               "17-enter-mid");
    const qreal mid = scene.enterProgress();
    QCOMPARE(scene.currentFrame().enterProgress, mid);

    const auto before2 = sessionState(scene);
    scene.advance(1200);
    expectDiff(before2, sessionState(scene), changes({
                                                 {"scene.animating", "0"},
                                                 {"scene.enterAnimRunning", "0"},
                                                 {"scene.enterProgress", "1.00"},
                                                 {"scene.needsFrame", "0"},
                                             }),
               "17-enter-done");
    QCOMPARE(scene.enterProgress(), 1.0);

    const auto before3 = sessionState(scene);
    scene.setEntered(false);
    scene.advance(120);
    expectDiff(before3, sessionState(scene), changes({
                                                 {"scene.animating", "1"},
                                                 {"scene.enterAnimRunning", "1"},
                                                 {"scene.enterProgress", "0.17"},
                                                 {"scene.needsFrame", "1"},
                                             }),
               "17-exit-mid");

    const auto before4 = sessionState(scene);
    scene.advance(1200);
    expectDiff(before4, sessionState(scene), changes({
                                                 {"scene.animating", "0"},
                                                 {"scene.enterAnimRunning", "0"},
                                                 {"scene.enterProgress", "0.00"},
                                                 {"scene.exitFinished", "1"},
                                                 {"scene.needsFrame", "0"},
                                             }),
               "17-exit-done");
    QCOMPARE(scene.enterProgress(), 0.0);
}

void TestScene::barFadesInBeforeViewEnter()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.setEntered(true);

    scene.advance(30);
    const qreal barMid = scene.currentFrame().barOpacity;
    QVERIFY2(barMid > 0.8 && barMid < 0.9,
             qPrintable(QStringLiteral("bar opacity at 30ms should be ~0.83, got %1").arg(barMid)));
    QVERIFY2(scene.enterProgress() < 0.5,
             qPrintable(QStringLiteral("view enter at 30ms should still be early, got %1").arg(scene.enterProgress())));

    scene.advance(70);
    QCOMPARE(scene.currentFrame().barOpacity, 1.0);
    QVERIFY2(scene.enterProgress() < 1.0, "view enter must still be running when the bar fade has finished");

    scene.advance(300);
    QCOMPARE(scene.enterProgress(), 1.0);

    scene.setEntered(false);
    scene.advance(120);
    const qreal barExitMid = scene.currentFrame().barOpacity;
    QVERIFY2(barExitMid > 0.1 && barExitMid < 0.25,
             qPrintable(QStringLiteral("bar opacity at 120ms of exit should be ~0.17, got %1").arg(barExitMid)));

    scene.advance(400);
    QCOMPARE(scene.currentFrame().barOpacity, 0.0);
}

void TestScene::barFadeHandlesSameFrameReversal()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.setEntered(true);
    scene.advance(200);

    QCOMPARE(scene.currentFrame().barOpacity, 1.0);
    QVERIFY2(scene.enterProgress() < 1.0, "the view enter must still be running before the reversal");

    scene.setEntered(false);
    scene.setEntered(true);
    scene.advance(1);
    QCOMPARE(scene.currentFrame().barOpacity, 1.0);

    scene.advance(400);
    QCOMPARE(scene.enterProgress(), 1.0);
    QCOMPARE(scene.currentFrame().barOpacity, 1.0);
}

void TestScene::overlayButtonsSkipRetiring()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.setEntered(true);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300)};
    EffectWindow *w0 = reinterpret_cast<EffectWindow *>(0x7000);
    const QVector<EffectWindow *> wins{w0};
    const QVector<QRectF> sources{QRectF(0, 0, 1600, 900)};
    scene.model()->syncCellItems(cells, 300.0);
    scene.model()->syncWindows(wins, sources);
    scene.setGridScrollTarget(0.0);
    scene.model()->setHoverCell(0);
    scene.syncWindowLayer();
    scene.advance(800);
    auto *ov = scene.windowOverlay();
    QVERIFY(ov->hasItems());
    const QRectF cb = scene.windowItemLayer().nodes()[0].closeButtonRect();
    QVERIFY2(ov->hitTest(cb.center()), "normal window: the button must be hit");

    const auto before = merge({windowState(scene), sessionState(scene)});
    scene.model()->syncWindows({}, {});
    scene.syncWindowLayer();
    expectDiff(before, merge({windowState(scene), sessionState(scene)}), changes({
                                                                             {"scene.animating", "1"},
                                                                             {"scene.needsFrame", "1"},
                                                                             {"win.0.animating", "1"},
                                                                             {"win.0.cellIndex", "-1"},
                                                                             {"win.0.cellIndexOf", "-1"},
                                                                             {"win.0.ovAnimating", "1"},
                                                                             {"win.0.ovTarget", "0.00"},
                                                                             {"win.0.retiring", "1"},
                                                                             {"win.0.targetOpacity", "0.00"},
                                                                         }),
               "24-sync-close");

    const auto before2 = windowState(scene);
    scene.advance(150);
    expectDiff(before2, windowState(scene), changes({
                                                {"win.0.opacity", "0.13"},
                                                {"win.0.ovAnimating", "0"},
                                                {"win.0.ovOpacity", "0.00"},
                                            }),
               "24-advance-150");
    QVERIFY2(scene.model()->windowItemFor(w0) && scene.model()->windowItemFor(w0)->retiring,
             "precondition: the retiring window is still in Items (needed for the assertion to be meaningful)");
    QVERIFY2(!ov->hitTest(cb.center()), "a retiring window must not hit its buttons (avoid operating on a possibly-invalid window)");
}

void TestScene::imageResourceCachesBySizeAndReleases()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString big = dir.filePath(QStringLiteral("big.png"));
    {
        QImage img(7680, 4320, QImage::Format_RGB32);
        img.fill(QColor(120, 80, 200));
        QVERIFY(img.save(big));
    }

    ImageProvider provider;
    const QSize thumb(480, 338);
    const QSize bg(1920, 1080);
    const auto t1 = provider.textureFor(big, thumb);
    const auto t2 = provider.textureFor(big, bg);
    QCOMPARE(provider.cachedCount(), 2);

    QVERIFY2(t1.imageSize.width() <= thumb.width() && t1.imageSize.height() <= thumb.height(),
             "the thumbnail must be downsampled to the target scale, not the 8K original");
    QVERIFY2(t2.imageSize.width() <= bg.width() && t2.imageSize.height() <= bg.height(),
             "the background must be optimized for its resolution, not the 8K original");
    QVERIFY2(t1.imageSize != t2.imageSize, "different target sizes must produce different sizes");
    const auto t1again = provider.textureFor(big, thumb);
    QCOMPARE(provider.cachedCount(), 2);

    provider.acquireTexture(big, thumb);
    provider.acquireTexture(big, bg);
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 2);
    provider.releaseTexture(big, thumb);
    provider.releaseTexture(big, bg);
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 0);
}

void TestScene::imageResourcesReleaseOnSessionEnd()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString bg = dir.filePath(QStringLiteral("bg.png"));
    {
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(QColor(200, 120, 80));
        QVERIFY(img.save(bg));
    }

    ImageProvider provider;
    const QSize sz(1920, 1080);

    provider.acquireTexture(bg, sz);
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 1);

    const auto info = provider.textureFor(bg, sz);
    QVERIFY2(!info.imageSize.isEmpty(), "texture loaded (offscreen texture may be 0, but the size decision is assertable)");

    provider.releaseTexture(bg, sz);
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 0);

    provider.releaseTexture(bg, sz);
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 0);
}

void TestScene::resourcesSurviveSessionEndWithoutTrim()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString bg = dir.filePath(QStringLiteral("bg2.png"));
    {
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(QColor(200, 120, 80));
        QVERIFY(img.save(bg));
    }

    ImageProvider provider;
    const QSize sz(1920, 1080);
    provider.acquireTexture(bg, sz);
    provider.textureFor(bg, sz);
    provider.releaseTexture(bg, sz);
    QCOMPARE(provider.cachedCount(), 1);

    const auto info2 = provider.textureFor(bg, sz);
    QCOMPARE(provider.cachedCount(), 1);
    QVERIFY2(!info2.imageSize.isEmpty(), "a second fetch must hit the cache (size decision usable)");

    provider.invalidate();
    QCOMPARE(provider.cachedCount(), 1);
}

void TestScene::pinnedResourcesSurviveTrimUntilUnpinned()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString bg = dir.filePath(QStringLiteral("bg3.png"));
    {
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(QColor(200, 120, 80));
        QVERIFY(img.save(bg));
    }

    ImageProvider provider;
    const QSize sz(1920, 1080);

    provider.pin(bg, sz);
    QCOMPARE(provider.cachedCount(), 1);
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 1);

    const auto info = provider.textureFor(bg, sz);
    QVERIFY2(!info.imageSize.isEmpty(), "an empty slot after pin must be lazily filled");

    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 1);

    provider.unpin(bg, sz);
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 0);
}

void TestScene::textureFilterPlanNeverMipmapsWithoutGeneration()
{
    const auto real = textureFilterPlan(true, true);
    QCOMPARE(real.minFilter, TextureFilterMode::Mipmap);
    QCOMPARE(real.magFilter, TextureFilterMode::Linear);
    QVERIFY2(real.generateMipmap, "the real-machine path must generate mipmaps");

    const auto noCtx = textureFilterPlan(true, false);
    QCOMPARE(noCtx.minFilter, TextureFilterMode::Linear);
    QVERIFY2(!noCtx.generateMipmap, "without a context, mipmaps must not be generated");

    QCOMPARE(textureFilterPlan(false, true).minFilter, TextureFilterMode::Nearest);
    QCOMPARE(textureFilterPlan(false, false).magFilter, TextureFilterMode::Nearest);

    for (bool smooth : {true, false}) {
        for (bool ctx : {true, false}) {
            const auto p = textureFilterPlan(smooth, ctx);
            const bool mipmapFilter = (p.minFilter == TextureFilterMode::Mipmap);
            QVERIFY2(mipmapFilter == p.generateMipmap,
                     "mipmap filtering and mipmap generation must hold together (otherwise the texture is incomplete -> samples render black)");
        }
    }
}

void TestScene::predecodeFeedsTextureForAndDeduplicates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString p = dir.filePath(QStringLiteral("wp.png"));
    QImage img(64, 64, QImage::Format_RGB32);
    img.fill(QColor(10, 200, 90));
    QVERIFY(img.save(p));

    ImageProvider provider;
    provider.predecode({p});
    (void)provider.textureFor(p, QSize(32, 32));

    const int maxWait = 200;
    int waited = 0;
    while (provider.decodedCount() == 0 && waited < maxWait) {
        QCoreApplication::processEvents();
        QTest::qWait(5);
        ++waited;
    }
    QCOMPARE(provider.decodedCount(), 1);

    provider.predecode({p, p, dir.filePath(QStringLiteral("missing.png"))});
    QCOMPARE(provider.decodedCount(), 1);

    QFile::remove(p);
    const auto info = provider.textureFor(p, QSize(32, 32));
    QVERIFY2(!info.imageSize.isEmpty(), "hits the predecoded original; an image is produced even after the source file is deleted");
}

void TestScene::textureForReadyIsNonBlocking()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString p = dir.filePath(QStringLiteral("ready.png"));
    QImage img(80, 40, QImage::Format_RGB32);
    img.fill(QColor(200, 30, 60));
    QVERIFY(img.save(p));

    ImageProvider provider;
    QSignalSpy spy(&provider, &ImageProvider::predecoded);

    const auto notReady = provider.textureForReady(p, QSize(40, 20));
    QVERIFY2(notReady.texture == nullptr, "must not load synchronously before predecode");
    QVERIFY2(notReady.imageSize.isEmpty(), "imageSize must be empty before predecode");
    QCOMPARE(provider.cachedCount(), 0);

    provider.predecode({p});
    const int maxWait = 200;
    int waited = 0;
    while (provider.decodedCount() == 0 && waited < maxWait) {
        QCoreApplication::processEvents();
        QTest::qWait(5);
        ++waited;
    }
    QCOMPARE(provider.decodedCount(), 1);
    QVERIFY2(spy.count() >= 1, "predecode completion must emit predecoded (so the caller can request a frame)");

    QFile::remove(p);
    const auto ready = provider.textureForReady(p, QSize(40, 20), false);
    QVERIFY2(!ready.imageSize.isEmpty(), "after predecode a non-blocking fetch is usable (source file deleted)");
    QVERIFY2(ready.imageSize.width() <= 40 && ready.imageSize.height() <= 20,
             "downsample to the target size");
}

void TestScene::wallpaperLifecycleUnpinsNonCurrent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QSize sz(64, 64);
    QHash<int, QString> uris;
    for (int i = 1; i <= 6; ++i) {
        const QString p = dir.filePath(QStringLiteral("bg%1.png").arg(i));
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(QColor(10 * i, 20, 30));
        QVERIFY(img.save(p));
        uris.insert(i, p);
    }

    ImageProvider provider;
    for (auto it = uris.constBegin(); it != uris.constEnd(); ++it) {
        provider.pin(it.value(), sz);
    }
    QCOMPARE(provider.cachedCount(), 6);
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 6);

    const int current = 3;
    for (auto it = uris.constBegin(); it != uris.constEnd(); ++it) {
        if (it.key() == current) {
            continue;
        }
        provider.unpin(it.value(), sz);
    }
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 1);
    QVERIFY2(provider.cachedCount() == 1, "the 5 non-current images are reclaimable, the current one is kept");

    for (auto it = uris.constBegin(); it != uris.constEnd(); ++it) {
        provider.pin(it.value(), sz);
    }
    provider.trimUnused();
    QCOMPARE(provider.cachedCount(), 6);
}

void TestScene::providerOpsLeaveSceneUntouched()
{
    SettledScene s;
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    s.build(*scene.model());
    const auto before = merge({barState(scene), gridState(scene), windowState(scene), sessionState(scene)});

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVector<QString> uris;
    for (int i = 0; i < 3; ++i) {
        const QString p = dir.filePath(QStringLiteral("a%1.png").arg(i));
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(QColor(10, 20, 30));
        QVERIFY(img.save(p));
        uris.append(p);
    }
    ImageProvider provider;
    provider.textureFor(uris[0], QSize(64, 64));
    provider.pin(uris[1], QSize(64, 64));
    provider.trimUnused();
    provider.unpin(uris[1], QSize(64, 64));
    provider.trimUnused();

    QCOMPARE(merge({barState(scene), gridState(scene), windowState(scene), sessionState(scene)}), before);
}

void TestScene::hoverHitIncludesCornerButtons()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300)};
    const QVector<EffectWindow *> wins{reinterpret_cast<EffectWindow *>(0x6000)};
    const QVector<QRectF> sources{QRectF(0, 0, 1600, 900)};
    const auto before = merge({gridState(scene), windowState(scene), sessionState(scene)});
    scene.model()->syncCellItems(cells, 300.0);
    scene.model()->syncWindows(wins, sources);
    scene.syncWindowLayer();

    scene.advance(800);
    scene.setGridScrollTarget(0.0);
    expectDiff(before, merge({gridState(scene), windowState(scene), sessionState(scene)}), changes({
                                                                                               {"cell.0.animating", "0"},
                                                                                               {"cell.0.cellRect", "100.00,400.00,800.00,300.00"},
                                                                                               {"cell.0.opacity", "1.00"},
                                                                                               {"cell.0.radius", "18.00"},
                                                                                               {"cell.0.rect", "100.00,400.00,800.00,300.00"},
                                                                                               {"cell.0.targetOpacity", "1.00"},
                                                                                               {"cell.0.targetRect", "100.00,400.00,800.00,300.00"},
                                                                                               {"cell.0.visible", "1"},
                                                                                               {"cellBase.0", "100.00,400.00,800.00,300.00"},
                                                                                               {"cellRects.0", "100.00,400.00,800.00,300.00"},
                                                                                               {"scene.animating", "0"},
                                                                                               {"scene.enterAnimRunning", "0"},
                                                                                               {"scene.enterProgress", "1.00"},
                                                                                               {"scene.hasItems.overlay", "1"},
                                                                                               {"scene.needsFrame", "0"},
                                                                                               {"scene.rowHeight", "300.00"},
                                                                                               {"win.0.animating", "0"},
                                                                                               {"win.0.cellIndex", "0"},
                                                                                               {"win.0.cellIndexOf", "0"},
                                                                                               {"win.0.closeBtn", "876.00,376.00,48.00,48.00"},
                                                                                               {"win.0.opacity", "1.00"},
                                                                                               {"win.0.ovAnimating", "0"},
                                                                                               {"win.0.ovOpacity", "0.00"},
                                                                                               {"win.0.ovRect", "97.00,397.00,806.00,306.00"},
                                                                                               {"win.0.ovTarget", "0.00"},
                                                                                               {"win.0.rect", "100.00,400.00,800.00,300.00"},
                                                                                               {"win.0.retiring", "0"},
                                                                                               {"win.0.src", "0.00,0.00,1600.00,900.00"},
                                                                                               {"win.0.targetOpacity", "1.00"},
                                                                                               {"win.0.targetRect", "100.00,400.00,800.00,300.00"},
                                                                                               {"win.0.topBtn", "76.00,376.00,48.00,48.00"},
                                                                                               {"win.0.visible", "1"},
                                                                                               {"win.0.window", "24576"},
                                                                                           }),
               "20-hoverHit-setup");

    const QRectF cv = scene.windowCells()->coverRectAt(0);
    QVERIFY(!cv.isEmpty());
    QCOMPARE(scene.windowCells()->cellAt(cv.center()), 0);
    QCOMPARE(scene.windowCells()->cellAt(QPointF(cv.left(), cv.top())), 0);
    QCOMPARE(scene.windowCells()->cellAt(QPointF(cv.right(), cv.top())), 0);
    const QPointF outside(cv.right() + 10, cv.top() - 10);
    QVERIFY(!scene.windowCells()->cellRectAt(0).contains(outside));
    QCOMPARE(scene.windowCells()->cellAt(outside), 0);
}

void TestScene::dragDeleteSwitchKeepsConsistency()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.model()->syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                              QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
    scene.setEntered(true);
    WorkspaceBarMetrics metrics;
    metrics.setSize(QSizeF(3840, 2160));
    WorkspaceBarDragModel cfgModel;
    WorkspaceBarDragModel::Config cfg;
    cfg.workspaceWidth = 480;
    cfg.workspaceHeight = 338;
    cfg.spacing = 80;
    cfg.listX = 1120;
    cfgModel.setConfig(cfg);
    const qreal slotW = 560.0, w = 480.0, cy = 200.0;
    const auto centerX = [&](int slot) {
        return 1120.0 + slot * slotW + w / 2.0;
    };
    const auto keyed = [](const QVector<int> &nums) {
        QVector<QPair<QString, int>> kn;
        for (int n : nums) {
            kn.append({QStringLiteral("d%1").arg(n), n});
        }
        return kn;
    };

    const QVector<int> three{1, 2, 3};
    scene.model()->syncBarItems(keyed(three), QHash<QString, QString>());
    scene.model()->barDragModel() = cfgModel;
    scene.syncBarLayer(three, 1, -1);
    scene.advance(600);
    QVERIFY(scene.model()->barItems().size() == 3);

    WorkspaceBarDragModel drag;
    drag.setConfig(cfg);
    drag.begin({1, 2, 3}, 1, 0, centerX(0), cy);
    drag.movePointer(centerX(2), cy);
    QCOMPARE(drag.order(), QVector<int>({2, 1, 3}));

    {
        const auto before = merge({barState(scene), sessionState(scene)});
        scene.model()->barDragModel() = drag;
        scene.syncBarLayer(drag.order(), 1, -1);
        scene.advance(600);
        expectDiff(before, merge({barState(scene), sessionState(scene)}), changes({
                                                                              {"bar.0.cell", "1680.00,40.00,480.00,270.00"},
                                                                              {"bar.0.phRect", "1920.00,40.00,240.00,270.00"},
                                                                              {"bar.0.phTargetW", "240.00"},
                                                                              {"bar.0.phTargetX", "1800.00"},
                                                                              {"bar.0.phW", "240.00"},
                                                                              {"bar.0.rect", "2240.00,40.00,480.00,270.00"},
                                                                              {"bar.1.cell", "1120.00,40.00,480.00,270.00"},
                                                                              {"bar.0.raised", "1"},
                                                                              {"bar.1.rect", "1240.00,40.00,480.00,270.00"},
                                                                              {"bar.2.rect", "2120.00,40.00,480.00,270.00"},
                                                                          }),
                   "19-drag-reorder");
    }

    {
        const auto before = barState(scene);
        scene.model()->syncBarItems(keyed({2, 3}), QHash<QString, QString>());
        scene.model()->barDragModel() = cfgModel;
        scene.syncBarLayer({2, 3}, 1, -1);
        scene.advance(600);
        expectDiff(before, barState(scene), changes({
                                                {"bar.0.key", "d2"},
                                                {"bar.0.num", "2"},
                                                {"bar.0.cell", "1120.00,40.00,480.00,270.00"},
                                                {"bar.0.current", "0"},
                                                {"bar.0.phRect", "240.00,40.00,0.00,270.00"},
                                                {"bar.0.phTargetW", "0.00"},
                                                {"bar.0.phTargetX", "0.00"},
                                                {"bar.0.phW", "0.00"},
                                                {"bar.0.raised", "0"},
                                                {"bar.0.rect", "1120.00,40.00,480.00,270.00"},
                                                {"bar.1.key", "d3"},
                                                {"bar.1.num", "3"},
                                                {"bar.1.cell", "1680.00,40.00,480.00,270.00"},
                                                {"bar.1.rect", "1680.00,40.00,480.00,270.00"},
                                                {"bar.2.key", "<removed>"},
                                                {"bar.2.num", "<removed>"},
                                                {"bar.2.uri", "<removed>"},
                                                {"bar.2.cell", "<removed>"},
                                                {"bar.2.phRect", "<removed>"},
                                                {"bar.2.phTargetX", "<removed>"},
                                                {"bar.2.phTargetW", "<removed>"},
                                                {"bar.2.phW", "<removed>"},
                                                {"bar.2.rect", "<removed>"},
                                                {"bar.2.opacity", "<removed>"},
                                                {"bar.2.animating", "<removed>"},
                                                {"bar.2.visible", "<removed>"},
                                                {"bar.2.current", "<removed>"},
                                                {"bar.2.hovered", "<removed>"},
                                                {"bar.2.raised", "<removed>"},
                                            }),
                   "19-sync-delete");
        QCOMPARE(scene.model()->barItems().size(), 2);
        QVERIFY2(scene.barLayer().nodes().size() >= 1 && !scene.barLayer().nodes()[0].rect().isEmpty(),
                 "inherited items must carry a geometry starting point (otherwise they jump)");
    }

    cfg.listX = metrics.itemX(0, 2);
    cfgModel.setConfig(cfg);
    scene.model()->barDragModel() = cfgModel;
    scene.syncBarLayer({2, 3}, 2, -1);
    scene.advance(600);
    const auto beforeSwitch = barState(scene);
    {
        const auto before = barState(scene);
        scene.model()->barDragModel() = cfgModel;
        scene.syncBarLayer({2, 3}, 1, -1);
        scene.advance(600);
        expectDiff(before, barState(scene), changes({
                                                {"bar.0.current", "0"},
                                            }),
                   "19-desktopSwitch");
    }
    const auto afterSwitch = barState(scene);
    for (int i = 0; i < scene.model()->barItems().size(); ++i) {
        QVERIFY2(beforeSwitch.value(QStringLiteral("bar.%1.rect").arg(i))
                     == afterSwitch.value(QStringLiteral("bar.%1.rect").arg(i)),
                 "switching workspace (currentId change) must not change bar geometry");
    }
    const auto nodes = scene.barLayer().nodes();
    QCOMPARE(scene.workspaceBar()->slotAt(nodes[1].rect().center()), 1);
    QCOMPARE(scene.workspaceBar()->slotAt(nodes[0].rect().center()), 0);
}

void TestScene::gridOpsLeaveBarUntouched()
{
    SettledScene s;
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    s.build(*scene.model());
    const auto bar0 = barState(scene);

    scene.setGridScrollTarget(120.0);
    scene.advance(600);
    QCOMPARE(scene.windowCells()->cellAt(scene.windowCells()->cellRectAt(0).center()), 0);

    scene.model()->setDragPlaceholder(0, true);
    scene.setGridScrollTarget(120.0);
    scene.advance(600);
    scene.model()->setDragPlaceholder(-1, false);

    scene.setGridScrollTarget(120.0);
    scene.advance(600);

    scene.model()->syncCellItems({QRectF(550, 400, 800, 300)}, 705.0);
    scene.model()->syncWindows({s.wins[1]}, {s.sources[1]});
    scene.setGridScrollTarget(0.0);

    scene.advance(800);

    QCOMPARE(barState(scene), bar0);
}

void TestScene::screenViewSetLifecycleIsStable()
{
    ScreenViewSet set;
    QVERIFY(set.isEmpty());
    QVERIFY(set.at(0) == nullptr);
    QVERIFY(set.find(nullptr) == nullptr);

    auto *a = reinterpret_cast<EffectScreen *>(0xA000);
    auto *b = reinterpret_cast<EffectScreen *>(0xB000);
    auto *c = reinterpret_cast<EffectScreen *>(0xC000);

    ScreenView *va = set.ensure(a);
    ScreenView *vb = set.ensure(b);
    QCOMPARE(set.count(), 2);
    QCOMPARE(va->model.screen(), a);
    QCOMPARE(set.at(0), va);

    QCOMPARE(set.ensure(a), va);
    QCOMPARE(set.count(), 2);
    QCOMPARE(set.find(a), va);
    QCOMPARE(set.find(b), vb);

    set.ensure(c);
    QCOMPARE(set.count(), 3);
    QCOMPARE(set.all().at(0)->model.screen(), a);
    QCOMPARE(set.all().at(1)->model.screen(), b);
    QCOMPARE(set.all().at(2)->model.screen(), c);

    set.remove(b);
    QCOMPARE(set.count(), 2);
    QVERIFY(set.find(b) == nullptr);
    QCOMPARE(set.all().at(0)->model.screen(), a);
    QCOMPARE(set.all().at(1)->model.screen(), c);
    QCOMPARE(set.at(0), va);

    set.remove(b);
    QCOMPARE(set.count(), 2);

    set.remove(a);
    QCOMPARE(set.at(0)->model.screen(), c);
    set.remove(c);
    QVERIFY(set.isEmpty());
    QVERIFY(set.at(0) == nullptr);
    QVERIFY(set.ensure(nullptr) == nullptr);
}

void TestScene::projectionDoesNotAccumulateAcrossPasses()
{
    GLPaintContext ctx(nullptr);
    const QRectF canvas(0, 0, 3840, 2160);
    const QVector4D p1(1000, 500, 0, 1);
    const QVector4D p2(2000, 1000, 0, 1);

    ctx.setProjection(canvas);
    const QVector4D a1 = ctx.projection() * p1;
    const QVector4D a2 = ctx.projection() * p2;
    QVERIFY2(!qFuzzyCompare(a1.x(), a2.x()), "different points must map to different positions");

    ctx.setProjection(canvas);
    const QVector4D b1 = ctx.projection() * p1;
    const QVector4D b2 = ctx.projection() * p2;
    QCOMPARE(b1.x(), a1.x());
    QCOMPARE(b1.y(), a1.y());
    QCOMPARE(b2.x(), a2.x());
    QCOMPARE(b2.y(), a2.y());
    QVERIFY2(!qFuzzyCompare(b1.x(), b2.x()), "after setting the projection twice, different points must not collapse to the same position");

    const QRectF small(0, 0, 1000, 1000);
    ctx.setProjection(small);
    const QVector4D c = ctx.projection() * QVector4D(500, 500, 0, 1);
    QVERIFY2(qAbs(c.x()) < 0.001 && qAbs(c.y()) < 0.001, "after changing size the projection is recomputed from zero (center point -> 0,0)");
}

void TestScene::localCoordsOriginAndBarShiftY()
{
    {
        GLPaintContext ctx(nullptr);
        ctx.origin = QPointF(0, 0);
        QCOMPARE(ctx.globalRect(QRectF(100, 50, 200, 100)), QRectF(100, 50, 200, 100));
        ctx.origin = QPointF(2880, 0);
        QCOMPARE(ctx.globalRect(QRectF(0, 0, 1920, 1080)), QRectF(2880, 0, 1920, 1080));
        QCOMPARE(ctx.globalRect(QRectF(100, 50, 200, 100)), QRectF(2980, 50, 200, 100));
    }
    {
        SceneModel model;
        GLScene scene;
        scene.setModel(&model);
        model.syncScreen(QPointF(0, 0), QSizeF(3840, 2160), QRectF(0, 0, 3840, 2160),
                         QRectF(0, 0, 3840, 2120), {});
        scene.setEntered(true);
        scene.advance(100);
        const SceneFrame mid = scene.currentFrame();
        QVERIFY2(mid.barShiftY < 0.0 && mid.barShiftY > -265.0,
                 qPrintable(QStringLiteral("while entering, barShiftY should be in (-265,0), got %1").arg(mid.barShiftY)));
        scene.advance(2000);
        QCOMPARE(scene.currentFrame().barShiftY, 0.0);
        QCOMPARE(scene.currentFrame().enterProgress, 1.0);
    }
}

QTEST_MAIN(TestScene)
#include "tst_scene.moc"

void TestScene::windowInGridCoverDerivesFromWindow()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.setEntered(true);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300)};
    const QVector<EffectWindow *> wins{reinterpret_cast<EffectWindow *>(0x9000)};
    const QVector<QRectF> sources{QRectF(0, 0, 1600, 900)};

    scene.model()->syncCellItems(cells, 300.0);
    scene.model()->syncWindows(wins, sources);
    scene.syncWindowLayer();
    scene.advance(800);
    scene.setGridScrollTarget(0.0);

    QCOMPARE(scene.windowItemLayer().nodes()[0].rect(), cells[0]);
    QCOMPARE(scene.windowCells()->coverRectAt(0), coverFor(cells[0], 300.0));
}

void TestScene::placeholderHidesWindowCoverAndOverlay()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    scene.setEntered(true);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300), QRectF(1000, 400, 800, 300)};
    const QVector<EffectWindow *> wins{reinterpret_cast<EffectWindow *>(0xA100),
                                       reinterpret_cast<EffectWindow *>(0xA200)};
    const QVector<QRectF> sources{QRectF(0, 0, 1600, 900), QRectF(100, 100, 1200, 800)};

    scene.model()->syncCellItems(cells, 705.0);
    scene.model()->syncWindows(wins, sources);
    scene.syncWindowLayer();
    scene.advance(800);

    scene.model()->setDragPlaceholder(0, true);
    scene.setGridScrollTarget(0.0);
    scene.syncWindowLayer();
    scene.advance(800);

    const auto &nodes = scene.windowItemLayer().nodes();
    QCOMPARE(nodes.size(), 2);
    QVERIFY2(nodes[0].opacity() < 0.01, "the placeholder window body must be hidden (left blank)");
    QVERIFY2(nodes[0].overlayOpacity() < 0.01, "the placeholder window's overlay must be hidden");
    QVERIFY2(nodes[1].opacity() > 0.99, "other windows must be visible");
    QVERIFY2(nodes[1].overlayOpacity() < 0.01, "a window that is not hovered must not show buttons");
    QCOMPARE(nodes[1].rect(), cells[1]);
}

void TestScene::backgroundSlideProgressAndReset()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);

    scene.startSlide(QStringLiteral("from.png"), QStringLiteral("to.png"), 1);
    QVERIFY2(scene.slideAnimating(), "after startSlide the slide must be running");
    QCOMPARE(scene.slideProgress(), 0.0);
    // The grid offset is published in the very frame the slide starts, so a paint between
    // startSlide() and the next advance() cannot show windows and cells at different offsets.
    QCOMPARE(scene.slideOffsetX(), scene.gridSlideDx());

    scene.advance(200);
    QVERIFY2(scene.slideProgress() > 0.0 && scene.slideProgress() < 1.0, "mid-slide progress must be in (0,1)");

    scene.advance(200);
    QCOMPARE(scene.slideProgress(), 1.0);
    QVERIFY2(!scene.slideAnimating(), "after the slide ends it must reset (no more frame requests)");

    scene.startSlide(QStringLiteral("a.png"), QStringLiteral("b.png"), -1);
    scene.resetSlide();
    QVERIFY2(!scene.slideAnimating(), "after resetSlide the slide must stop");
    QCOMPARE(scene.slideProgress(), 1.0);
}

void TestScene::workspaceSlideKeepsWindowsInOwnCells()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    sceneModel.syncScreen(QPointF(0, 0), QSizeF(1920, 1080),
                          QRectF(0, 0, 1920, 1080), QRectF(0, 0, 1920, 1080), {});
    scene.setEntered(true);

    const QVector<QRectF> cells{QRectF(100, 300, 400, 300),
                                QRectF(700, 300, 400, 300),
                                QRectF(1300, 300, 400, 300)};
    const qreal rowHeight = 300.0;
    QVector<EffectWindow *> windows{reinterpret_cast<EffectWindow *>(0x9100),
                                    reinterpret_cast<EffectWindow *>(0x9101)};
    const QVector<QRectF> sources{QRectF(0, 0, 1600, 900), QRectF(0, 0, 1600, 900),
                                  QRectF(0, 0, 1600, 900)};

    scene.model()->syncCellItems(cells, rowHeight);
    scene.model()->syncWindows(windows, sources.mid(0, windows.size()));
    scene.syncWindowLayer();
    scene.advance(800);

    const qreal canvasWidth = sceneModel.layout().fullRect.width();
    QVERIFY2(canvasWidth > 0.0, "the canvas must have a width");

    // Production frame order is syncWindowLayer() then advance(delta) then paint, so every
    // assertion below reads the scene after exactly that pair, never after advance alone.
    const auto frame = [&](int dtMs) {
        scene.syncWindowLayer();
        scene.advance(dtMs);
    };

    const auto checkRigid = [&]() {
        const qreal dx = scene.gridSlideDx();
        QCOMPARE(dx, std::round(dx));
        // Windows read the layer offset, cells read the scene offset: both must be the very
        // same value in the same frame, otherwise they move at different speeds.
        QCOMPARE(scene.slideOffsetX(), dx);
        for (int i = 0; i < windows.size(); ++i) {
            const WindowNode *n = scene.windowItemLayer().nodeFor(windows.at(i));
            QVERIFY2(n != nullptr, "every window must own a node");
            QCOMPARE(n->rect(), cells.at(i));
            const QRectF painted = scene.windowItemLayer().paintedRect(*n);
            QCOMPARE(painted, cells.at(i).translated(dx, 0.0));
            QCOMPARE(painted.topLeft() - n->rect().topLeft(), QPointF(dx, 0.0));
            QCOMPARE(painted, scene.windowCells()->cellRectAt(i));
            QCOMPARE(scene.windowCells()->coverRectAt(i), coverFor(painted, rowHeight));
        }
    };

    const auto checkEmptyCellFollowsSlide = [&]() {
        const qreal dx = scene.gridSlideDx();
        QVERIFY2(dx != 0.0, "the slide must still be running");
        QCOMPARE(scene.windowCells()->cellRectAt(2), cells.at(2).translated(dx, 0.0));
        QCOMPARE(scene.windowCells()->cellAt(scene.windowCells()->cellRectAt(2).center()), 2);
        QVERIFY2(scene.windowCells()->cellAt(cells.at(2).center()) != 2,
                 "the empty cell must no longer be hit at its resting position");
    };

    checkRigid();

    // The defect: windows read the offset the scene published, while cells read the animation
    // state directly, so any slide state change that did not publish left the two disagreeing
    // for one frame. Every entry point that changes slide state must therefore leave windows
    // and cells at the same offset, with no advance() in between.
    const auto checkOffsetPublished = [&]() {
        QCOMPARE(scene.slideOffsetX(), scene.gridSlideDx());
        for (int i = 0; i < windows.size(); ++i) {
            const WindowNode *n = scene.windowItemLayer().nodeFor(windows.at(i));
            QVERIFY2(n != nullptr, "every window must own a node");
            QCOMPARE(scene.windowItemLayer().paintedRect(*n), scene.windowCells()->cellRectAt(i));
        }
    };

    scene.startSlide(QStringLiteral("from.png"), QStringLiteral("to.png"), 1);
    QCOMPARE(scene.gridSlideDx(), canvasWidth);

    // The bug this test pins down: windows read the slide offset the scene published, cells
    // used to read the animation state directly, so a paint between startSlide() and the next
    // advance() moved the cells a full canvas width while the windows stayed in their cells.
    // In that first frame of the slide both must already agree.
    QCOMPARE(scene.slideOffsetX(), canvasWidth);
    checkOffsetPublished();

    frame(16);
    QVERIFY2(scene.gridSlideDx() > 0.0, "dir=1 must offset the grid to the right");
    QVERIFY2(scene.gridSlideDx() < canvasWidth, "the first frame must already have moved");
    checkRigid();
    checkEmptyCellFollowsSlide();

    // A window of the incoming workspace appears mid-slide: its node must start at its own
    // cell, not at a per-window entry offset, so the whole grid keeps moving as one block.
    QVERIFY2(scene.gridSlideDx() != 0.0, "the add below must happen while the slide runs");
    const qreal dxBeforeAdd = scene.gridSlideDx();
    windows.append(reinterpret_cast<EffectWindow *>(0x9102));
    scene.model()->syncWindows(windows, sources.mid(0, windows.size()));
    scene.syncWindowLayer();

    const WindowNode *fresh = scene.windowItemLayer().nodeFor(windows.last());
    QVERIFY2(fresh != nullptr, "the new window must own a node");
    QCOMPARE(fresh->rect(), cells.at(2));
    QCOMPARE(scene.windowItemLayer().paintedRect(*fresh), cells.at(2).translated(dxBeforeAdd, 0.0));

    frame(16);
    checkRigid();

    for (int i = 0; i < 40; ++i) {
        frame(16);
    }
    QVERIFY2(!scene.slideAnimating(), "the slide must have finished");
    QCOMPARE(scene.gridSlideDx(), 0.0);
    QCOMPARE(scene.slideOffsetX(), 0.0);
    checkRigid();

    scene.startSlide(QStringLiteral("b.png"), QStringLiteral("c.png"), -1);
    QCOMPARE(scene.gridSlideDx(), -canvasWidth);
    QCOMPARE(scene.slideOffsetX(), -canvasWidth);
    checkOffsetPublished();
    frame(16);
    QVERIFY2(scene.gridSlideDx() < 0.0, "dir=-1 must offset the grid to the left");
    checkRigid();

    // Leaving a populated workspace turns its windows into retiring nodes, which are painted by
    // WindowOverlayControl instead of paintWindow(). They must travel with the grid as well.
    scene.model()->syncWindows({}, {});
    scene.syncWindowLayer();
    frame(16);
    const qreal retiringDx = scene.gridSlideDx();
    QVERIFY2(qAbs(retiringDx) > 1.0, "precondition: the grid must still be sliding");
    WindowOverlayControl overlay;
    overlay.setWindowItemLayer(&scene.windowItemLayer());
    int retiringCount = 0;
    for (const WindowNode &n : scene.windowItemLayer().nodes()) {
        if (!n.retiring) {
            continue;
        }
        ++retiringCount;
        QCOMPARE(overlay.retiringRect(n), n.rect().translated(retiringDx, 0.0));
    }
    QVERIFY2(retiringCount > 0, "leaving the workspace must leave retiring windows behind");

    // resetSlide() is the other entry point that changes slide state; it must publish too,
    // otherwise the windows stay offset while the cells snap back (or the reverse).
    scene.startSlide(QStringLiteral("d.png"), QStringLiteral("e.png"), 1);
    QCOMPARE(scene.slideOffsetX(), canvasWidth);
    scene.resetSlide();
    QCOMPARE(scene.gridSlideDx(), 0.0);
    QCOMPARE(scene.slideOffsetX(), 0.0);
}

void TestScene::barScrollClampedByViewport()
{
    SceneModel sceneModel;
    GLScene scene;
    scene.setModel(&sceneModel);
    sceneModel.syncScreen(QPointF(0, 0), QSizeF(1920, 1080), QRectF(0, 0, 1920, 1080),
                          QRectF(0, 0, 1920, 1080), {});

    QVector<QPair<QString, int>> order;
    for (int i = 1; i <= 20; ++i) {
        order.append({QStringLiteral("d%1").arg(i), i});
    }
    sceneModel.syncBarItems(order, QHash<QString, QString>());
    QVERIFY2(scene.scrollBarBy(120.0, 1920.0), "20 items exceed the visible width: scrolling must be possible");
    QVERIFY(scene.barScrollOffset() > 0.0);
    scene.scrollBarBy(-99999.0, 1920.0);
    QCOMPARE(scene.barScrollOffset(), 0.0);
    QVERIFY2(!scene.scrollBarBy(-10.0, 1920.0), "already at 0: cannot scroll further in the negative direction");
    scene.scrollBarBy(99999.0, 1920.0);
    const qreal maxScroll = barMaxScroll(sceneModel.metrics().contentWidth(20), 1920.0);
    QVERIFY2(qFuzzyCompare(scene.barScrollOffset(), maxScroll), "already at the limit: must not exceed");

    sceneModel.syncBarItems({{QStringLiteral("d1"), 1}, {QStringLiteral("d2"), 2}},
                            QHash<QString, QString>());
    scene.resetBarScroll();
    QVERIFY2(!scene.scrollBarBy(120.0, 1920.0), "few items, no overflow: cannot scroll");
    QCOMPARE(scene.barScrollOffset(), 0.0);
}
