/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "model/keynav.h"
#include "model/scenemodel.h"
#include "model/screenlayout.h"
#include "model/wallpaperdata.h"
#include "model/windowgrid.h"
#include "model/windowgrouping.h"
#include "scene_model_state.h"
#include "view/windowitemlayer.h"

#include <QtTest>

using namespace KWin;

inline QVector<int> orderNums(const QVector<QPair<QString, int>> &kn)
{
    QVector<int> nums;
    nums.reserve(kn.size());
    for (const auto &p : kn) {
        nums.append(p.second);
    }
    return nums;
}

class TestSceneModel : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void hoverButtonsAnchorAtWindowCorners();
    void windowLayerPaintOrder();
    void wallpaperCacheMergeKeepsMissingEntries();
    void wallpaperSnapshotDebounceSkipsUnchanged();
    void dockPointerIsNotConsumedByView();
    void keyNavMapsKeysToActions();
    void screenLayoutPerScreenIsConsistent();
    void screenLocalLayoutNormalizesToScreenOrigin();
    void paintTargetsFollowRenderTargetNotBackend();
    void groupIndicesByScreenKeepsOrderAndDropsOutOfRange();
    void placeholderHidesCoverThenRestores();
    void restoreSnapsVisibleInSameFrame();
    void sessionResetClearsPlaceholderAndEnterState();
    void dragSessionOwnsCellAndPos();
    void quickSwitchBackRevivesWindows();
    void windowOpsLeaveBarUntouched();
    void barOpsLeaveGridUntouched();
    void barOpsLeaveWindowsUntouched();
};

void TestSceneModel::hoverButtonsAnchorAtWindowCorners()
{
    WindowNode node;
    node.interp.snap(VisualState{100, 200, 800, 600, 1.0});

    const QRectF top = node.topButtonRect();
    const QRectF close = node.closeButtonRect();
    QCOMPARE(top.center(), QPointF(100.0, 200.0));
    QCOMPARE(close.center(), QPointF(900.0, 200.0));
    QCOMPARE(top.size(), QSizeF(48.0, 48.0));
    QCOMPARE(buttonIconRect(top).size(), QSizeF(24.0, 24.0));
    QCOMPARE(buttonIconRect(top).center(), top.center());
    QCOMPARE(buttonIconRect(close).center(), close.center());

    const QRectF top2 = node.topButtonRect(2.0);
    const QRectF close2 = node.closeButtonRect(2.0);
    QCOMPARE(top2.center(), QPointF(100.0, 200.0));
    QCOMPARE(close2.center(), QPointF(900.0, 200.0));
    QCOMPARE(top2.size(), QSizeF(96.0, 96.0));
    QCOMPARE(node.overlayRect(2.0).size(), QSizeF(812.0, 612.0));
}

void TestSceneModel::windowLayerPaintOrder()
{
    QCOMPARE(windowLayerOf(true, false, false, false), WindowLayer::Desktop);
    QCOMPARE(windowLayerOf(false, true, false, false), WindowLayer::Dock);
    QCOMPARE(windowLayerOf(false, false, true, false), WindowLayer::Skip);
    QCOMPARE(windowLayerOf(false, false, false, true), WindowLayer::Skip);
    QCOMPARE(windowLayerOf(false, false, false, false), WindowLayer::Normal);
}

void TestSceneModel::wallpaperCacheMergeKeepsMissingEntries()
{
    WallpaperMap cache{{1, {{QStringLiteral("eDP"), QStringLiteral("file:///w/old1.png")}}},
                       {2, {{QStringLiteral("eDP"), QStringLiteral("file:///w/old2.png")}}}};
    mergeWallpaperCache(cache, {{1, {{QStringLiteral("eDP"), QStringLiteral("file:///w/new1.png")}}}});
    QCOMPARE(cache.value(1).value(QStringLiteral("eDP")), QStringLiteral("file:///w/new1.png"));
    QCOMPARE(cache.value(2).value(QStringLiteral("eDP")), QStringLiteral("file:///w/old2.png"));
    mergeWallpaperCache(cache, {{1, {{QStringLiteral("HDMI-A-0"), QStringLiteral("file:///w/hdmi1.png")}}}});
    QCOMPARE(cache.value(1).value(QStringLiteral("eDP")), QStringLiteral("file:///w/new1.png"));
    QCOMPARE(cache.value(1).value(QStringLiteral("HDMI-A-0")), QStringLiteral("file:///w/hdmi1.png"));
    mergeWallpaperCache(cache, {{1, {{QStringLiteral("eDP"), QStringLiteral("file:///w/a.png")}}}, {2, {{QStringLiteral("eDP"), QStringLiteral("file:///w/b.png")}}}});
    QCOMPARE(cache.value(1).value(QStringLiteral("eDP")), QStringLiteral("file:///w/a.png"));
    QCOMPARE(cache.value(2).value(QStringLiteral("eDP")), QStringLiteral("file:///w/b.png"));
}

void TestSceneModel::wallpaperSnapshotDebounceSkipsUnchanged()
{
    const WallpaperMap a{{1, {{QStringLiteral("eDP"), QStringLiteral("file:///w/1.png")}}},
                         {2, {{QStringLiteral("eDP"), QStringLiteral("file:///w/2.png")}}}};
    const WallpaperMap same = a;
    QVERIFY(wallpaperSnapshotUnchanged(a, same));

    WallpaperMap changed = a;
    changed[2][QStringLiteral("eDP")] = QStringLiteral("file:///w/new2.png");
    QVERIFY(!wallpaperSnapshotUnchanged(a, changed));

    WallpaperMap added = a;
    added[2][QStringLiteral("HDMI-A-0")] = QStringLiteral("file:///w/hdmi.png");
    QVERIFY(!wallpaperSnapshotUnchanged(a, added));

    WallpaperMap removed = a;
    removed.remove(1);
    QVERIFY(!wallpaperSnapshotUnchanged(a, removed));
}

void TestSceneModel::dockPointerIsNotConsumedByView()
{
    const QVector<QRectF> docks{QRectF(0, 1990, 3840, 170)};
    QVERIFY(pointerBelongsToDock(docks, QPointF(1000, 2050)));
    QVERIFY(!pointerBelongsToDock(docks, QPointF(1000, 500)));
    QVERIFY(!pointerBelongsToDock({}, QPointF(1000, 2050)));
}

void TestSceneModel::screenLayoutPerScreenIsConsistent()
{
    const qreal barH = 100;

    const QRectF full1(0, 0, 2880, 1920);
    const ScreenLayout l1 = screenLayoutFor(full1, full1, barH);
    QCOMPARE(l1.offX, 0.0);
    QCOMPARE(l1.offY, 0.0);
    QCOMPARE(l1.barSize, QSizeF(2880, 1920));
    QCOMPARE(l1.gridArea, QRectF(0, barH, 2880, 1920 - barH));

    const QRectF full2(2880, 0, 1920, 1080);
    const ScreenLayout l2 = screenLayoutFor(full2, full2, barH);
    QCOMPARE(l2.offX, 2880.0);
    QCOMPARE(l2.barSize, QSizeF(1920, 1080));
    QCOMPARE(l2.gridArea, QRectF(2880, barH, 1920, 1080 - barH));

    const QRectF full3(0, 0, 2880, 1920);
    const QRectF client3(0, 48, 2880, 1920 - 48);
    const ScreenLayout l3 = screenLayoutFor(full3, client3, barH);
    QCOMPARE(l3.offY, 48.0);
    QCOMPARE(l3.gridArea, QRectF(0, 48 + barH, 2880, 1920 - 48 - barH));
    QVERIFY2(l3.gridArea.bottom() <= l3.clientRect.bottom(), "the grid area must not extend beyond clientRect");

    const ScreenLayout l4 = screenLayoutFor(QRectF(0, 0, 800, 100), QRectF(0, 0, 800, 100), 200);
    QCOMPARE(l4.gridArea.height(), 0.0);
}

void TestSceneModel::screenLocalLayoutNormalizesToScreenOrigin()
{
    const qreal barH = 100;
    const QRectF full(2880, 0, 1920, 1080);
    const ScreenLayout abs = screenLayoutFor(full, full, barH);
    const ScreenLayout local = screenLocalLayout(abs, full);

    QCOMPARE(local.fullRect, QRectF(0, 0, 1920, 1080));
    QCOMPARE(abs.offX, 2880.0);
    QCOMPARE(local.offX, 0.0);
    QCOMPARE(local.offY, 0.0);
    QCOMPARE(local.gridArea, QRectF(0, barH, 1920, 1080 - barH));
    QVERIFY2(local.clientRect.contains(local.gridArea), "the grid area must fall inside clientRect");

    const QRectF client(2880, 48, 1920, 1080 - 48);
    const ScreenLayout docked = screenLocalLayout(screenLayoutFor(full, client, barH), full);
    QCOMPARE(docked.clientRect, QRectF(0, 48, 1920, 1080 - 48));
    QCOMPARE(docked.offY, 48.0);
    QCOMPARE(docked.gridArea, QRectF(0, 48 + barH, 1920, 1080 - 48 - barH));
    QVERIFY2(docked.clientRect.contains(docked.gridArea), "with a dock offset the grid area must not exceed clientRect");

    const QRectF mainFull(0, 0, 2880, 1920);
    QCOMPARE(screenLocalLayout(screenLayoutFor(mainFull, mainFull, barH), mainFull).gridArea,
             screenLayoutFor(mainFull, mainFull, barH).gridArea);
}

void TestSceneModel::paintTargetsFollowRenderTargetNotBackend()
{
    const QVector<QRectF> one{QRectF(0, 0, 2880, 1920)};
    QCOMPARE(paintTargets(one, QRectF(0, 0, 2880, 1920)), QVector<int>{0});

    const QVector<QRectF> two{QRectF(0, 0, 2880, 1920), QRectF(2880, 0, 1920, 1080)};
    QCOMPARE(paintTargets(two, QRectF(0, 0, 4800, 1920)), (QVector<int>{0, 1}));
    QCOMPARE(paintTargets(two, QRectF(2880, 0, 1920, 1080)), QVector<int>{1});
    QCOMPARE(paintTargets(two, QRectF(0, 0, 2880, 1920)), QVector<int>{0});

    const QVector<QRectF> three{QRectF(0, 0, 1920, 1080),
                                QRectF(1920, 0, 1920, 1080),
                                QRectF(0, 1080, 1920, 1080)};
    QCOMPARE(paintTargets(three, QRectF(0, 0, 3840, 2160)), (QVector<int>{0, 1, 2}));
    QCOMPARE(paintTargets(three, QRectF(0, 1080, 1920, 1080)), QVector<int>{2});

    QVERIFY(paintTargets(two, QRectF(9999, 9999, 100, 100)).isEmpty());
    QVERIFY(paintTargets({}, QRectF(0, 0, 100, 100)).isEmpty());

    QCOMPARE(paintTargets(two, QRectF(2000, 0, 1000, 100)), (QVector<int>{0, 1}));
}

void TestSceneModel::groupIndicesByScreenKeepsOrderAndDropsOutOfRange()
{
    const auto g = groupIndicesByScreen(2, {0, 1, 0, 1, 0});
    QCOMPARE(g.size(), 2);
    QCOMPARE(g.at(0), (QVector<int>{0, 2, 4}));
    QCOMPARE(g.at(1), (QVector<int>{1, 3}));

    const auto dropped = groupIndicesByScreen(2, {0, -1, 99, 1});
    QCOMPARE(dropped.at(0), QVector<int>{0});
    QCOMPARE(dropped.at(1), QVector<int>{3});

    const auto empty = groupIndicesByScreen(3, {});
    QCOMPARE(empty.size(), 3);
    QVERIFY(empty.at(0).isEmpty() && empty.at(1).isEmpty() && empty.at(2).isEmpty());

    QVERIFY(groupIndicesByScreen(0, {0, 1}).isEmpty());
    QVERIFY(groupIndicesByScreen(-1, {0}).isEmpty());
}

void TestSceneModel::placeholderHidesCoverThenRestores()
{
    SceneModel model;
    model.syncCellItems(twoCells(), 705.0);
    const auto baseline = merge({gridModelState(model), sessionModelState(model)});

    const auto before = merge({gridModelState(model), sessionModelState(model)});
    model.setDragPlaceholder(1, true);
    expectDiff(before, merge({gridModelState(model), sessionModelState(model)}), changes({
                                                                                     {"scene.dragPlaceholder", "1"},
                                                                                 }),
               "3-placeholder-on");

    QCOMPARE(model.dragPlaceholder(), 1);

    const auto before2 = merge({gridModelState(model), sessionModelState(model)});
    model.setDragPlaceholder(-1, false);
    expectDiff(before2, merge({gridModelState(model), sessionModelState(model)}), changes({
                                                                                      {"scene.dragPlaceholder", "-1"},
                                                                                  }),
               "3-placeholder-off");
    QCOMPARE(merge({gridModelState(model), sessionModelState(model)}), baseline);
}

void TestSceneModel::restoreSnapsVisibleInSameFrame()
{
    SceneModel model;
    model.syncCellItems(twoCells(), 705.0);
    model.setDragPlaceholder(0, true);

    const int idx = model.dragPlaceholder();

    const auto before = merge({gridModelState(model), sessionModelState(model)});
    model.setDragPlaceholder(-1, false);

    expectDiff(before, merge({gridModelState(model), sessionModelState(model)}), changes({
                                                                                     {"scene.dragPlaceholder", "-1"},
                                                                                 }),
               "4-snapRestore");
}

void TestSceneModel::sessionResetClearsPlaceholderAndEnterState()
{
    SceneModel model;
    model.syncCellItems(twoCells(), 705.0);
    model.setDragPlaceholder(0, true);

    const auto before = merge({gridModelState(model), sessionModelState(model)});
    model.resetItems();
    expectDiff(before, merge({gridModelState(model), sessionModelState(model)}), changes({
                                                                                     {"scene.dragPlaceholder", "-1"},
                                                                                 }),
               "5-resetSession");

    QCOMPARE(model.dragPlaceholder(), -1);
}

void TestSceneModel::dragSessionOwnsCellAndPos()
{
    SceneModel model;

    const QVector<QRectF> cells{QRectF(100, 400, 800, 300)};
    model.syncCellItems(cells, 705.0);

    QVERIFY(!model.dragActive());

    {
        const auto before = sessionModelState(model);
        model.beginDrag(cells[0]);
        expectDiff(before, sessionModelState(model), changes({
                                                         {"scene.dragActive", "1"},
                                                         {"scene.dragCell", "100.00,400.00,800.00,300.00"},
                                                     }),
                   "13-beginDrag");
        QVERIFY(model.dragActive());
        QCOMPARE(model.dragCell(), cells[0]);
        QVERIFY(model.dragPos().isNull());
    }

    {
        const auto before = sessionModelState(model);
        model.dragTo(QPointF(1000, 120));
        expectDiff(before, sessionModelState(model), changes({
                                                         {"scene.dragPos", "1000.00,120.00"},
                                                     }),
                   "13-dragTo");
        QCOMPARE(model.dragPos(), QPointF(1000, 120));
        QCOMPARE(model.dragCell(), cells[0]);
    }

    {
        const auto before = sessionModelState(model);
        model.endDrag();
        expectDiff(before, sessionModelState(model), changes({
                                                         {"scene.dragActive", "0"},
                                                         {"scene.dragCell", "0.00,0.00,0.00,0.00"},
                                                     }),
                   "13-endDrag");
        QVERIFY(!model.dragActive());
    }
}

void TestSceneModel::quickSwitchBackRevivesWindows()
{
    SceneModel model;

    EffectWindow *w0 = reinterpret_cast<EffectWindow *>(0x8100);
    EffectWindow *w1 = reinterpret_cast<EffectWindow *>(0x8200);
    const QVector<QRectF> cells{QRectF(100, 400, 800, 300), QRectF(1000, 400, 800, 300)};
    const QVector<QRectF> srcs{QRectF(0, 0, 1600, 900), QRectF(100, 100, 1200, 800)};

    model.syncCellItems(cells, 705.0);
    model.syncWindows({w0, w1}, srcs);

    QCOMPARE(model.windowItems().size(), 2);

    model.syncWindows({}, {});
    QVERIFY(model.windowItemFor(w0));
    QVERIFY2(model.windowItemFor(w0)->retiring, "switched away: the window enters the retiring state");

    const auto before = merge({windowModelState(model), sessionModelState(model)});
    model.syncWindows({w0, w1}, srcs);
    QVERIFY2(model.windowItemFor(w0) && !model.windowItemFor(w0)->retiring,
             "quick switch back: the window must be revived (retiring cleared), otherwise only the cover remains");
    QCOMPARE(model.windowItemFor(w1)->retiring, false);
    QVERIFY2(model.windowItems().size() == 2, "after revival both windows must be back in the list");
}

void TestSceneModel::windowOpsLeaveBarUntouched()
{
    SettledScene s;
    SceneModel model;
    s.build(model);
    const auto bar0 = barModelState(model);

    model.syncWindows({s.wins[0]}, {s.sources[0]});

    model.dropWindow(s.wins[0]);

    QCOMPARE(barModelState(model), bar0);
}

void TestSceneModel::barOpsLeaveGridUntouched()
{
    SettledScene s;
    SceneModel model;
    s.build(model);
    const auto grid0 = gridModelState(model);

    runBarOps(model, s);

    QCOMPARE(gridModelState(model), grid0);
}

void TestSceneModel::barOpsLeaveWindowsUntouched()
{
    SettledScene s;
    SceneModel model;
    s.build(model);
    const auto win0 = windowModelState(model);

    runBarOps(model, s);

    QCOMPARE(windowModelState(model), win0);
}

void TestSceneModel::keyNavMapsKeysToActions()
{
    using K = NavActionType;
    auto A = [](int key, Qt::KeyboardModifiers m) {
        return navActionFor(key, m);
    };

    QCOMPARE(A(Qt::Key_Escape, Qt::NoModifier).type, K::Exit);

    for (int i = 1; i <= 6; ++i) {
        const NavAction a = A(Qt::Key_0 + i, Qt::MetaModifier);
        QCOMPARE(a.type, K::SwitchDesktop);
        QCOMPARE(a.arg, i);
        QCOMPARE(A(Qt::Key_0 + i, Qt::MetaModifier | Qt::KeypadModifier).type, K::SwitchDesktop);
        QCOMPARE(A(Qt::Key_0 + i, Qt::NoModifier).type, K::None);
    }

    const QPair<int, int> digits[] = {{Qt::Key_Exclam, 1}, {Qt::Key_At, 2}, {Qt::Key_NumberSign, 3}, {Qt::Key_Dollar, 4}, {Qt::Key_Percent, 5}, {Qt::Key_AsciiCircum, 6}};
    for (const auto &d : digits) {
        const NavAction a = A(d.first, Qt::ShiftModifier | Qt::MetaModifier);
        QCOMPARE(a.type, K::MoveHoverToDesktop);
        QCOMPARE(a.arg, d.second);
        QCOMPARE(A(d.first, Qt::NoModifier).type, K::None);
    }

    QCOMPARE(A(Qt::Key_Home, Qt::NoModifier).type, K::HoverFirst);
    QCOMPARE(A(Qt::Key_End, Qt::NoModifier).type, K::HoverLast);
    QCOMPARE(A(Qt::Key_End, Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier).arg, 1);

    QCOMPARE(A(Qt::Key_PageDown, Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier).arg, 3);
    QCOMPARE(A(Qt::Key_Clear, Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier).arg, 5);
    QCOMPARE(A(Qt::Key_Down, Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier).arg, 2);

    QCOMPARE(A(Qt::Key_Right, Qt::MetaModifier).type, K::SwitchDesktopStep);
    QCOMPARE(A(Qt::Key_Right, Qt::MetaModifier).arg, +1);
    QCOMPARE(A(Qt::Key_Left, Qt::MetaModifier).arg, -1);
    QCOMPARE(A(Qt::Key_Right, Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier).arg, 6);
    QCOMPARE(A(Qt::Key_Left, Qt::ShiftModifier | Qt::MetaModifier | Qt::KeypadModifier).arg, 4);
    QCOMPARE(A(Qt::Key_Down, Qt::NoModifier).type, K::CycleHover);
    QCOMPARE(A(Qt::Key_Down, Qt::NoModifier).arg, +1);
    QCOMPARE(A(Qt::Key_Up, Qt::NoModifier).arg, -1);

    QCOMPARE(A(Qt::Key_Tab, Qt::NoModifier).type, K::CycleHoverOrFirst);
    QCOMPARE(A(Qt::Key_Tab, Qt::ShiftModifier).type, K::None);
    QCOMPARE(A(Qt::Key_Backtab, Qt::ShiftModifier).type, K::CycleHover);
    QCOMPARE(A(Qt::Key_Backtab, Qt::ShiftModifier).arg, -1);

    QCOMPARE(A(Qt::Key_Equal, Qt::AltModifier).type, K::AddDesktop);
    QCOMPARE(A(Qt::Key_Plus, Qt::AltModifier | Qt::KeypadModifier).type, K::AddDesktop);
    QCOMPARE(A(Qt::Key_Minus, Qt::AltModifier).type, K::RemoveDesktop);
    QCOMPARE(A(Qt::Key_Minus, Qt::AltModifier | Qt::KeypadModifier).type, K::RemoveDesktop);

    QCOMPARE(A(Qt::Key_Return, Qt::NoModifier).type, K::ActivateHover);
    QCOMPARE(A(Qt::Key_Enter, Qt::NoModifier).type, K::ActivateHover);
    QCOMPARE(A(Qt::Key_Delete, Qt::NoModifier).type, K::CloseHover);
    QCOMPARE(A(Qt::Key_Delete, Qt::ShiftModifier).type, K::None);
    QCOMPARE(A(Qt::Key_L, Qt::MetaModifier).type, K::Lock);
    QCOMPARE(A(Qt::Key_QuoteLeft, Qt::AltModifier).type, K::CycleSameType);
    QCOMPARE(A(Qt::Key_QuoteLeft, Qt::AltModifier).arg, +1);
    QCOMPARE(A(Qt::Key_AsciiTilde, Qt::AltModifier | Qt::ShiftModifier).arg, -1);

    QCOMPARE(A(Qt::Key_Space, Qt::AltModifier).type, K::Consume);
    QCOMPARE(A(Qt::Key_Space, Qt::NoModifier).type, K::None);
    QCOMPARE(A(Qt::Key_A, Qt::NoModifier).type, K::None);
}

QTEST_MAIN(TestSceneModel)
#include "tst_scenemodel.moc"
