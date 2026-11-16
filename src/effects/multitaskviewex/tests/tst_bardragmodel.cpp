/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "model/bardraglayout.h"
#include "model/bardragmodel.h"
#include "model/barpredicates.h"
#include "model/motionoptions.h"
#include "model/windowdragsession.h"
#include "model/windowmetrics.h"

#include <QMap>
#include <QTest>

using namespace KWin;

namespace
{

QString snapNum(qreal v)
{
    return QString::number(v, 'f', 2);
}

QMap<QString, QString> changes(std::initializer_list<QPair<QString, QString>> items)
{
    QMap<QString, QString> out;
    for (const auto &p : items) {
        out[p.first] = p.second;
    }
    return out;
}

void expectDiff(const QMap<QString, QString> &before, const QMap<QString, QString> &after,
                const QMap<QString, QString> &expected)
{
    QStringList unexpected;
    QStringList missing;
    QStringList wrongValue;
    for (auto it = after.constBegin(); it != after.constEnd(); ++it) {
        const QString b = before.value(it.key(), QStringLiteral("<absent>"));
        if (b == it.value()) {
            if (expected.contains(it.key())) {
                missing << QStringLiteral("%1: %2 (unchanged)").arg(it.key(), it.value());
            }
            continue;
        }
        if (!expected.contains(it.key())) {
            unexpected << QStringLiteral("%1: %2 -> %3").arg(it.key(), b, it.value());
        } else if (expected.value(it.key()) != it.value()) {
            wrongValue << QStringLiteral("%1: expected %2 got %3").arg(it.key(), expected.value(it.key()), it.value());
        }
    }
    for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
        if (!after.contains(it.key())) {
            if (expected.contains(it.key())) {
                missing << QStringLiteral("%1: vanished").arg(it.key());
            } else {
                unexpected << QStringLiteral("%1: %2 -> <vanished>").arg(it.key(), it.value());
            }
        }
    }
    const auto report = [&](const QStringList &list, const QString &what) {
        return list.isEmpty() ? QString() : QStringLiteral("\n  [%1]\n    %2").arg(what, list.join(QStringLiteral("\n    ")));
    };
    const QString detail = report(unexpected, QStringLiteral("unexpected change (changing A touched B)"))
        + report(missing, QStringLiteral("declared but never happened (feature did not take effect)"))
        + report(wrongValue, QStringLiteral("value mismatch (inconsistent with design semantics)"));
    QVERIFY2(unexpected.isEmpty() && missing.isEmpty() && wrongValue.isEmpty(), qPrintable(detail));
}

QMap<QString, QString> modelState(const WorkspaceBarDragModel &m)
{
    QMap<QString, QString> out;
    out[QStringLiteral("active")] = m.isActive() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("count")] = QString::number(m.count());
    QString order;
    for (int id : m.order()) {
        order += QString::number(id) + QLatin1Char(',');
    }
    out[QStringLiteral("order")] = order;
    out[QStringLiteral("draggedId")] = QString::number(m.draggedId());
    out[QStringLiteral("draggedSlot")] = QString::number(m.draggedSlot());
    out[QStringLiteral("pointerX")] = snapNum(m.pointerX());
    out[QStringLiteral("dragDelete")] = m.dragDelete() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("direction")] = QString::number(m.direction());
    out[QStringLiteral("verticalOffset")] = snapNum(m.verticalOffset());
    out[QStringLiteral("clampedVerticalOffset")] = snapNum(m.clampedVerticalOffset());
    out[QStringLiteral("isVertical")] = m.isVertical() ? QStringLiteral("1") : QStringLiteral("0");
    return out;
}

QMap<QString, QString> sessionState(const WindowDragSession &s)
{
    const QRectF c = s.cell();
    QMap<QString, QString> out;
    out[QStringLiteral("active")] = s.active() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("cell")] = QStringLiteral("%1,%2,%3,%4")
                                      .arg(snapNum(c.x()), snapNum(c.y()), snapNum(c.width()), snapNum(c.height()));
    return out;
}

QString dumpTargets(const QVector<BarDragTarget> &ts)
{
    QString s;
    for (const BarDragTarget &t : ts) {
        s += QStringLiteral("{id=%1 x=%2 y=%3 cellX=%4 w=%5 phX=%6 phW=%7 op=%8}\n")
                 .arg(t.id)
                 .arg(snapNum(t.x), snapNum(t.y), snapNum(t.cellX), snapNum(t.w), snapNum(t.placeholderX),
                      snapNum(t.placeholderW), snapNum(t.opacity));
    }
    return s;
}

}

class TestBarDragModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void beginDrag();
    void horizontalLock();
    void swapToRight();
    void swapToLeft();
    void hysteresisBoundary();
    void continuousSwap();
    void dragDeleteGesture();
    void dragAxisLocksOnFirstMove();
    void verticalDragKeepsListLayout();
    void dragTipGeometryMatchesPlugin();
    void barCellSkeletonStaysPutDuringDrag();
    void endKeepsOrder();
    void hoverSwitchRequiresIdleOnOtherDesktop();
    void desktopClickExitsOnlyOnCurrentWhenIdle();
    void dragSessionKeepsCellAcrossResync();
    void dropTargetDecidesByPosition();
    void inputGateClosesDuringExit();
    void ghostDropDoesNotReturnWhenMovedAway();
    void sessionBaseDesktopIsSticky();
    void dragMakesWay();
    void dragKeepsContentsFullWidth();
    void placeholderFollowsLogicalSlot();
    void steadyStateFullLayout();
    void addDesktopRecentersLeft();
    void previewItemSitsAtAddButton();
    void previewVisibleOnHover();
    void previewFadesWhenNotHovered();
    void previewFixedTopRightWithMargin();
    void addedItemTargetsLastSlot();
    void addDesktopInsertsAtEnd();
    void addWorkspaceRespectsLimit();
    void showAddButtonRespectsConfiguredMax();
    void resolveMsAppliesSpeedMultiplier();
    void windowCornerRadiusFollowsRow();
    void barMaxScrollOnlyWhenOverflow();
    void layoutScrollShiftsWholeList();
    void workspaceLimitAllowsMany();

private:
    static void configure(WorkspaceBarDragModel &m)
    {
        WorkspaceBarDragModel::Config c;
        c.workspaceWidth = 480;
        c.workspaceHeight = 338;
        c.spacing = 80;
        c.listX = 1120;
        m.setConfig(c);
    }

    static qreal center(int slot)
    {
        return 1120 + slot * 560 + 240;
    }
};

void TestBarDragModel::beginDrag()
{
    WorkspaceBarDragModel m;
    configure(m);
    const auto before = modelState(m);
    m.begin({1, 2, 3}, 2, 1, 1680, 175);
    expectDiff(before, modelState(m), changes({
                                          {"active", "1"},
                                          {"count", "3"},
                                          {"order", "1,2,3,"},
                                          {"draggedId", "2"},
                                          {"draggedSlot", "1"},
                                          {"pointerX", "1680.00"},
                                      }));
}

void TestBarDragModel::horizontalLock()
{
    WorkspaceBarDragModel m;
    configure(m);
    m.begin({1, 2, 3}, 2, 1, 1680, 175);

    const auto afterBegin = modelState(m);
    m.movePointer(1680, 500);
    expectDiff(afterBegin, modelState(m), changes({
                                              {"direction", "1"},
                                              {"verticalOffset", "325.00"},
                                              {"isVertical", "1"},
                                          }));

    const auto vertical = modelState(m);
    m.movePointer(3000, 175);
    expectDiff(vertical, modelState(m), changes({
                                            {"verticalOffset", "0.00"},
                                        }));
}

void TestBarDragModel::swapToRight()
{
    WorkspaceBarDragModel m;
    configure(m);
    m.begin({1, 2, 3}, 2, 1, 1680, 175);
    const auto before = modelState(m);
    m.movePointer(center(2) - 140 + 1, 175);
    expectDiff(before, modelState(m), changes({
                                          {"direction", "2"},
                                          {"pointerX", "2341.00"},
                                          {"draggedSlot", "2"},
                                          {"order", "1,3,2,"},
                                      }));
}

void TestBarDragModel::swapToLeft()
{
    WorkspaceBarDragModel m;
    configure(m);
    m.begin({1, 2, 3}, 2, 1, 1680, 175);
    const auto before = modelState(m);
    m.movePointer(center(0) + 140 - 1, 175);
    expectDiff(before, modelState(m), changes({
                                          {"direction", "2"},
                                          {"pointerX", "1499.00"},
                                          {"draggedSlot", "0"},
                                          {"order", "2,1,3,"},
                                      }));
}

void TestBarDragModel::hysteresisBoundary()
{
    WorkspaceBarDragModel m;
    configure(m);
    m.begin({1, 2, 3}, 2, 1, 1680, 175);
    const auto before = modelState(m);
    m.movePointer(center(2) - 141, 175);
    expectDiff(before, modelState(m), changes({
                                          {"direction", "2"},
                                          {"pointerX", "2339.00"},
                                      }));

    const auto inDeadZone = modelState(m);
    m.movePointer(center(2) - 140, 175);
    expectDiff(inDeadZone, modelState(m), changes({
                                              {"pointerX", "2340.00"},
                                              {"draggedSlot", "2"},
                                              {"order", "1,3,2,"},
                                          }));
}

void TestBarDragModel::continuousSwap()
{
    WorkspaceBarDragModel m;
    configure(m);
    m.begin({1, 2, 3, 4}, 2, 1, 1680, 175);
    auto before = modelState(m);
    m.movePointer(center(2) - 140, 175);
    expectDiff(before, modelState(m), changes({
                                          {"direction", "2"},
                                          {"pointerX", "2340.00"},
                                          {"draggedSlot", "2"},
                                          {"order", "1,3,2,4,"},
                                      }));

    before = modelState(m);
    m.movePointer(center(3) - 140, 175);
    expectDiff(before, modelState(m), changes({
                                          {"pointerX", "2900.00"},
                                          {"draggedSlot", "3"},
                                          {"order", "1,3,4,2,"},
                                      }));

    before = modelState(m);
    m.movePointer(center(2) - 140, 175);
    expectDiff(before, modelState(m), changes({
                                          {"pointerX", "2340.00"},
                                          {"draggedSlot", "2"},
                                          {"order", "1,3,2,4,"},
                                      }));
}

void TestBarDragModel::dragDeleteGesture()
{
    WorkspaceBarDragModel m;
    configure(m);
    m.begin({1, 2, 3}, 2, 1, 1680, 175);
    auto before = modelState(m);
    m.movePointer(1680, 175 - 100);
    expectDiff(before, modelState(m), changes({
                                          {"direction", "1"},
                                          {"verticalOffset", "-100.00"},
                                          {"clampedVerticalOffset", "-100.00"},
                                          {"isVertical", "1"},
                                      }));

    before = modelState(m);
    m.movePointer(1680, 175 - 195);
    expectDiff(before, modelState(m), changes({
                                          {"verticalOffset", "-195.00"},
                                          {"clampedVerticalOffset", "-195.00"},
                                          {"dragDelete", "1"},
                                      }));

    before = modelState(m);
    m.movePointer(1680, 175);
    expectDiff(before, modelState(m), changes({
                                          {"verticalOffset", "0.00"},
                                          {"clampedVerticalOffset", "0.00"},
                                          {"dragDelete", "0"},
                                      }));
}

void TestBarDragModel::dragAxisLocksOnFirstMove()
{
    const qreal cy = 175;
    const qreal c1 = center(1);

    WorkspaceBarDragModel h;
    configure(h);
    h.begin({1, 2, 3}, 2, 1, c1, cy);
    auto before = modelState(h);
    h.movePointer(c1 + 30, cy);
    expectDiff(before, modelState(h), changes({
                                          {"direction", "2"},
                                          {"pointerX", "1950.00"},
                                      }));
    before = modelState(h);
    h.movePointer(c1 + 30, cy - 400);
    expectDiff(before, modelState(h), changes({}));
    QVERIFY2(!h.dragDelete(), "after horizontal lock, vertical movement must not trigger delete");
    QCOMPARE(h.order(), QVector<int>({1, 2, 3}));

    WorkspaceBarDragModel v;
    configure(v);
    v.begin({1, 2, 3}, 2, 1, c1, cy);
    before = modelState(v);
    v.movePointer(c1, cy - 30);
    expectDiff(before, modelState(v), changes({
                                          {"direction", "1"},
                                          {"verticalOffset", "-30.00"},
                                          {"clampedVerticalOffset", "-30.00"},
                                          {"isVertical", "1"},
                                      }));
    before = modelState(v);
    v.movePointer(center(2) + 200, cy - 200);
    expectDiff(before, modelState(v), changes({
                                          {"verticalOffset", "-200.00"},
                                          {"clampedVerticalOffset", "-200.00"},
                                          {"dragDelete", "1"},
                                      }));
    QCOMPARE(v.order(), QVector<int>({1, 2, 3}));
}

void TestBarDragModel::verticalDragKeepsListLayout()
{
    WorkspaceBarDragModel m;
    configure(m);
    const QVector<int> order{1, 2, 3};
    const qreal cy = 175;
    const qreal c1 = center(1);
    const auto still = layoutBarDragTargets(m, order);
    QCOMPARE(still.size(), 3);

    const auto beforeBegin = modelState(m);
    m.begin(order, 2, 1, c1, cy);
    m.movePointer(c1, cy - 60);
    expectDiff(beforeBegin, modelState(m), changes({
                                               {"active", "1"},
                                               {"count", "3"},
                                               {"order", "1,2,3,"},
                                               {"draggedId", "2"},
                                               {"draggedSlot", "1"},
                                               {"pointerX", "1920.00"},
                                               {"direction", "1"},
                                               {"verticalOffset", "-60.00"},
                                               {"clampedVerticalOffset", "-60.00"},
                                               {"isVertical", "1"},
                                           }));
    const auto targets = layoutBarDragTargets(m, order);
    QCOMPARE(dumpTargets(targets), QStringLiteral("{id=1 x=1120.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                  "{id=2 x=1680.00 y=-60.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=0.50}\n"
                                                  "{id=3 x=2240.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));

    {
        const qreal h = 338.0;
        auto draggedY = [&](WorkspaceBarDragModel &m) {
            const auto ts = layoutBarDragTargets(m, order);
            for (const auto &t : ts) {
                if (t.id == 2)
                    return t.y;
            }
            return 0.0;
        };
        auto draggedOpacity = [&](WorkspaceBarDragModel &m) {
            const auto ts = layoutBarDragTargets(m, order);
            for (const auto &t : ts) {
                if (t.id == 2)
                    return t.opacity;
            }
            return 1.0;
        };
        WorkspaceBarDragModel r;
        configure(r);
        r.begin(order, 2, 1, c1, cy);
        r.movePointer(c1, cy - 30);
        QCOMPARE(draggedY(r), -30.0);
        QCOMPARE(draggedOpacity(r), 0.5);
        r.movePointer(c1, cy - h * 2.0);
        QCOMPARE(draggedY(r), -0.9 * h);
        QCOMPARE(draggedOpacity(r), 0.5);
        r.movePointer(c1, cy + 40);
        QCOMPARE(draggedY(r), 0.0);
        QCOMPARE(draggedOpacity(r), 1.0);
    }

    QCOMPARE(kDefaultWorkspaceMoveDurationMs, 400);
    QCOMPARE(kBarReorderDragMs, 150);

    WorkspaceBarDragModel h;
    configure(h);
    h.begin(order, 2, 1, c1, cy);
    h.movePointer(center(2) + 40, cy);
    QCOMPARE(h.direction(), 2);
    const auto ht = layoutBarDragTargets(h, order);
    bool makeWay = false;
    for (const auto &t : ht) {
        if (t.placeholderW > 0)
            makeWay = true;
    }
    QVERIFY2(makeWay, "horizontal direction must make way (half-width placeholder)");
}

void TestBarDragModel::barCellSkeletonStaysPutDuringDrag()
{
    WorkspaceBarDragModel m;
    configure(m);
    const QVector<int> order{1, 2, 3};
    const qreal c1 = center(1);
    auto cellsOf = [](const QVector<BarDragTarget> &ts) {
        QVector<qreal> v;
        for (const auto &t : ts)
            v.append(t.cellX);
        return v;
    };
    const QVector<qreal> stillCells = cellsOf(layoutBarDragTargets(m, order));

    WorkspaceBarDragModel h;
    configure(h);
    h.begin(order, 2, 1, c1, 175);
    h.movePointer(center(2) + 40, 175);
    QCOMPARE(h.direction(), 2);
    QCOMPARE(cellsOf(layoutBarDragTargets(h, order)), stillCells);

    WorkspaceBarDragModel v;
    configure(v);
    v.begin(order, 2, 1, c1, 175);
    v.movePointer(c1, 175 - 120);
    QVERIFY(v.isVertical());
    QVERIFY(layoutBarDragTargets(v, order)[1].y != 0.0);
    QCOMPARE(cellsOf(layoutBarDragTargets(v, order)), stillCells);

    const auto withPreview = layoutBarDragTargets(m, order, 3000.0, true);
    QCOMPARE(withPreview.last().id, kAddPreviewId);
    QCOMPARE(withPreview.last().cellX, 3000.0);

    const QRectF cell(1000, 200, 480, 338);
    const auto tip = dragTipLayout(cell, kDragTipIconSize, 140.0, 20.0);
    QCOMPARE(tip.rect.center(), cell.center());
}

void TestBarDragModel::dragTipGeometryMatchesPlugin()
{
    const QRectF item(1000, 200, 480, 338);
    const auto wide = dragTipLayout(item, 21.0, 140.0, 20.0);
    QVERIFY2(wide.showText, "text must be shown when wide enough");
    QCOMPARE(wide.rect.width(), 21.0 + 140.0 + 13.0);
    QCOMPARE(wide.rect.height(), 28.0);
    QCOMPARE(wide.rect.center().x(), item.center().x());
    QCOMPARE(wide.rect.center().y(), item.center().y());

    const QRectF narrow(0, 200, 60, 338);
    const auto thin = dragTipLayout(narrow, 21.0, 140.0, 20.0);
    QVERIFY2(!thin.showText, "only the icon is shown when too narrow");
    QCOMPARE(thin.rect.width(), 21.0);

    QVERIFY(shouldShowDragTip(true, 3));
    QVERIFY(!shouldShowDragTip(false, 3));
    QVERIFY2(!shouldShowDragTip(true, 1), "with a single workspace left, delete is not possible and the tip must not be shown");
}

static WorkspaceBarDragModel makeModel()
{
    WorkspaceBarDragModel m;
    WorkspaceBarDragModel::Config c;
    c.workspaceWidth = 480;
    c.workspaceHeight = 338;
    c.spacing = 80;
    c.listX = 1120;
    m.setConfig(c);
    return m;
}

void TestBarDragModel::dragMakesWay()
{
    WorkspaceBarDragModel m = makeModel();
    m.begin({1, 2, 3}, 1, 0, 1360, 200);
    m.movePointer(1650, 200);
    QCOMPARE(m.draggedSlot(), 0);
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, m.order())), QStringLiteral("{id=1 x=1410.00 y=0.00 cellX=1120.00 w=480.00 phX=1240.00 phW=240.00 op=1.00}\n"
                                                                             "{id=2 x=1560.00 y=0.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                             "{id=3 x=2120.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
    m.end();
}

void TestBarDragModel::dragKeepsContentsFullWidth()
{
    WorkspaceBarDragModel m = makeModel();
    m.begin({1, 2, 3}, 2, 1, 1680, 200);
    m.movePointer(1900, 200);
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, m.order())), QStringLiteral("{id=1 x=1240.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                             "{id=2 x=1660.00 y=0.00 cellX=1680.00 w=480.00 phX=1800.00 phW=240.00 op=1.00}\n"
                                                                             "{id=3 x=2120.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
}

void TestBarDragModel::placeholderFollowsLogicalSlot()
{
    WorkspaceBarDragModel m = makeModel();
    m.begin({1, 2, 3}, 1, 0, 1360, 200);
    m.movePointer(1800, 200);
    QCOMPARE(m.draggedSlot(), 1);
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, m.order())), QStringLiteral("{id=2 x=1240.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                             "{id=1 x=1560.00 y=0.00 cellX=1680.00 w=480.00 phX=1800.00 phW=240.00 op=1.00}\n"
                                                                             "{id=3 x=2120.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
}

void TestBarDragModel::steadyStateFullLayout()
{
    WorkspaceBarDragModel m = makeModel();
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, {1, 2, 3})), QStringLiteral("{id=1 x=1120.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                             "{id=2 x=1680.00 y=0.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                             "{id=3 x=2240.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
}

void TestBarDragModel::addDesktopRecentersLeft()
{
    WorkspaceBarDragModel m = makeModel();
    auto t3 = layoutBarDragTargets(m, {1, 2, 3});
    QCOMPARE(dumpTargets(t3), QStringLiteral("{id=1 x=1120.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                             "{id=2 x=1680.00 y=0.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                             "{id=3 x=2240.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
    WorkspaceBarDragModel m4 = makeModel();
    WorkspaceBarDragModel::Config c4 = m4.config();
    c4.listX = 840.0;
    m4.setConfig(c4);
    auto t4 = layoutBarDragTargets(m4, {1, 2, 3, 4});
    QCOMPARE(dumpTargets(t4), QStringLiteral("{id=1 x=840.00 y=0.00 cellX=840.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                             "{id=2 x=1400.00 y=0.00 cellX=1400.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                             "{id=3 x=1960.00 y=0.00 cellX=1960.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                             "{id=4 x=2520.00 y=0.00 cellX=2520.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
    const qreal center3 = (t3[0].x + t3[2].x + 480.0) / 2.0;
    const qreal center4 = (t4[0].x + t4[3].x + 480.0) / 2.0;
    QCOMPARE(center3, center4);
    QCOMPARE(t3[0].x - t4[0].x, 280.0);
}

void TestBarDragModel::previewItemSitsAtAddButton()
{
    WorkspaceBarDragModel m = makeModel();
    const qreal btnX = 3040.0;
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, {1, 2, 3}, btnX)), QStringLiteral("{id=1 x=1120.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                   "{id=2 x=1680.00 y=0.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                   "{id=3 x=2240.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                   "{id=0 x=3040.00 y=0.00 cellX=3040.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, {1, 2, 3})), QStringLiteral("{id=1 x=1120.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                             "{id=2 x=1680.00 y=0.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                             "{id=3 x=2240.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
    m.begin({1, 2, 3}, 1, 0, 1360, 200);
    m.movePointer(1500, 200);
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, m.order(), btnX)), QStringLiteral("{id=1 x=1260.00 y=0.00 cellX=1120.00 w=480.00 phX=1240.00 phW=240.00 op=1.00}\n"
                                                                                   "{id=2 x=1560.00 y=0.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                   "{id=3 x=2120.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
    m.end();
}

void TestBarDragModel::previewVisibleOnHover()
{
    WorkspaceBarDragModel m = makeModel();
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, {1, 2, 3}, 3040.0, true)), QStringLiteral("{id=1 x=1120.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                           "{id=2 x=1680.00 y=0.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                           "{id=3 x=2240.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                           "{id=0 x=3040.00 y=0.00 cellX=3040.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
}

void TestBarDragModel::previewFadesWhenNotHovered()
{
    WorkspaceBarDragModel m = makeModel();
    QCOMPARE(dumpTargets(layoutBarDragTargets(m, {1, 2, 3}, 3040.0, false)), QStringLiteral("{id=1 x=1120.00 y=0.00 cellX=1120.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                            "{id=2 x=1680.00 y=0.00 cellX=1680.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                            "{id=3 x=2240.00 y=0.00 cellX=2240.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                                                            "{id=0 x=3040.00 y=0.00 cellX=3040.00 w=480.00 phX=0.00 phW=0.00 op=0.00}\n"));
}

void TestBarDragModel::previewFixedTopRightWithMargin()
{
    QCOMPARE(previewXFor(3840.0, 480.0), 3320.0);
    QCOMPARE(previewXFor(1920.0, 240.0), 1660.0);
    QVERIFY(previewXFor(400.0, 480.0) >= 0.0);
    const qreal left = previewXFor(3840.0, 480.0);
    QVERIFY(left + 480.0 <= 3840.0);
}

void TestBarDragModel::addedItemTargetsLastSlot()
{
    WorkspaceBarDragModel m = makeModel();
    const qreal btnX = 3040.0;
    qreal previewX = -1.0;
    for (const BarDragTarget &x : layoutBarDragTargets(m, {1, 2, 3}, btnX)) {
        if (x.id == kAddPreviewId)
            previewX = x.x;
    }
    QCOMPARE(previewX, btnX);
    WorkspaceBarDragModel m4 = makeModel();
    WorkspaceBarDragModel::Config c4 = m4.config();
    c4.listX = 840.0;
    m4.setConfig(c4);
    auto after = layoutBarDragTargets(m4, {1, 2, 3, 4});
    QCOMPARE(dumpTargets(after), QStringLiteral("{id=1 x=840.00 y=0.00 cellX=840.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                "{id=2 x=1400.00 y=0.00 cellX=1400.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                "{id=3 x=1960.00 y=0.00 cellX=1960.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"
                                                "{id=4 x=2520.00 y=0.00 cellX=2520.00 w=480.00 phX=0.00 phW=0.00 op=1.00}\n"));
    QCOMPARE(after.last().x, 2520.0);
    QVERIFY(after.last().x != previewX);
}

void TestBarDragModel::windowCornerRadiusFollowsRow()
{
    QCOMPARE(windowCornerRadius(0), 18.0);
    QCOMPARE(windowCornerRadius(1), 18.0);
    QCOMPARE(windowCornerRadius(2), 10.0);
    QCOMPARE(windowCornerRadius(5), 10.0);
    QCOMPARE(windowCornerRadius(0, 2.0), 36.0);
    QCOMPARE(windowCornerRadius(2, 2.0), 20.0);
    const QVector<qreal> ys{100.0, 400.0, 700.0};
    QCOMPARE(gridRowOf(100.0, ys), 0);
    QCOMPARE(gridRowOf(400.0, ys), 1);
    QCOMPARE(gridRowOf(700.0, ys), 2);
    QCOMPARE(gridRowOf(108.0, ys), 0);
}

void TestBarDragModel::barMaxScrollOnlyWhenOverflow()
{
    QCOMPARE(barMaxScroll(1000.0, 1200.0), 0.0);
    QCOMPARE(barMaxScroll(1200.0, 1000.0), 200.0);
    QCOMPARE(barMaxScroll(1000.0, 1000.0), 0.0);
}

void TestBarDragModel::layoutScrollShiftsWholeList()
{
    WorkspaceBarDragModel m = makeModel();
    const QVector<int> order{1, 2, 3};
    const auto base = layoutBarDragTargets(m, order);
    const auto scrolled = layoutBarDragTargets(m, order, -1.0, true, 120.0);
    QCOMPARE(scrolled.size(), base.size());
    for (int i = 0; i < base.size(); ++i) {
        QCOMPARE(scrolled[i].x, base[i].x - 120.0);
        QCOMPARE(scrolled[i].cellX, base[i].cellX - 120.0);
        QCOMPARE(scrolled[i].placeholderX, base[i].placeholderX - 120.0);
        QCOMPARE(scrolled[i].w, base[i].w);
        QCOMPARE(scrolled[i].opacity, base[i].opacity);
    }
}

void TestBarDragModel::workspaceLimitAllowsMany()
{
    QVERIFY(showAddButton(kDefaultMaxWorkspaces - 1, kDefaultMaxWorkspaces));
    QVERIFY(!showAddButton(kDefaultMaxWorkspaces, kDefaultMaxWorkspaces));
}

void TestBarDragModel::addWorkspaceRespectsLimit()
{
    QVERIFY(showAddButton(0, kDefaultMaxWorkspaces));
    QVERIFY(showAddButton(5, kDefaultMaxWorkspaces));
    QVERIFY(!showAddButton(6, kDefaultMaxWorkspaces));
    QVERIFY(!showAddButton(7, kDefaultMaxWorkspaces));
}

void TestBarDragModel::showAddButtonRespectsConfiguredMax()
{
    QCOMPARE(kDefaultMaxWorkspaces, 6);
    QVERIFY(showAddButton(4, 10));
    QVERIFY(!showAddButton(10, 10));
    QVERIFY(!showAddButton(11, 10));
    QVERIFY(showAddButton(0, 1));
    QVERIFY(!showAddButton(1, 1));
}

void TestBarDragModel::resolveMsAppliesSpeedMultiplier()
{
    QCOMPARE(resolveMs(400, 1.0), 400);
    QCOMPARE(resolveMs(400, 2.0), 200);
    QCOMPARE(resolveMs(400, 0.5), 800);
    QCOMPARE(resolveMs(300, 1.0), 300);
    QCOMPARE(resolveMs(300, 2.0), 150);
    QCOMPARE(resolveMs(kBarReorderDragMs, 1.0), 150);
    QCOMPARE(resolveMs(kDefaultMaxWorkspaces * 100, 2.0), 300);
    QCOMPARE(resolveMs(400, 10.0), resolveMs(400, kMaxAnimationSpeedMultiplier));
    QCOMPARE(resolveMs(400, kMaxAnimationSpeedMultiplier), 80);
    QCOMPARE(resolveMs(400, 0.01), resolveMs(400, kMinAnimationSpeedMultiplier));
    QCOMPARE(resolveMs(400, kMinAnimationSpeedMultiplier), 4000);
    QCOMPARE(resolveMs(16, kMaxAnimationSpeedMultiplier), 16);
    QCOMPARE(resolveMs(10, 1.0), kMinAnimationDurationMs);
    QCOMPARE(resolveMs(0, 1.0), kMinAnimationDurationMs);
}

void TestBarDragModel::addDesktopInsertsAtEnd()
{
    QCOMPARE(addDesktopPosition(3), 3u);
    QCOMPARE(addDesktopPosition(1), 1u);
    QCOMPARE(addDesktopPosition(5), 5u);
    QCOMPARE(addDesktopPosition(0), 0u);
}

void TestBarDragModel::endKeepsOrder()
{
    WorkspaceBarDragModel m;
    configure(m);
    m.begin({1, 2, 3}, 2, 1, 1680, 175);
    m.movePointer(center(2) - 140, 175);
    const auto beforeEnd = modelState(m);
    m.end();
    expectDiff(beforeEnd, modelState(m), changes({
                                             {"active", "0"},
                                             {"draggedId", "-1"},
                                         }));
}

void TestBarDragModel::hoverSwitchRequiresIdleOnOtherDesktop()
{
    QVERIFY(shouldSwitchDesktop(0, 1, 2));
    QVERIFY(!shouldSwitchDesktop(-1, 1, 2));
    QVERIFY(!shouldSwitchDesktop(0, 2, 2));
    QVERIFY(shouldSwitchDesktop(1, 2, 3));
    QVERIFY(!shouldSwitchDesktop(0, 1, 0));
    QCOMPARE(kDragHoverSwitchMs, 1000);
}

void TestBarDragModel::desktopClickExitsOnlyOnCurrentWhenIdle()
{
    QVERIFY(!shouldExitDesktopOnClick(0, 1, false));
    QVERIFY(!shouldExitDesktopOnClick(-1, 1, false));
    QVERIFY(!shouldExitDesktopOnClick(2, 1, false));
    QVERIFY(shouldExitDesktopOnClick(1, 1, false));
    QVERIFY(!shouldExitDesktopOnClick(1, 1, true));
    QVERIFY(!shouldExitDesktopOnClick(2, 1, true));
}

void TestBarDragModel::dragSessionKeepsCellAcrossResync()
{
    WindowDragSession s;
    const QRectF cell(100, 400, 800, 300);
    expectDiff({}, sessionState(s), changes({
                                        {"active", "0"},
                                        {"cell", "0.00,0.00,0.00,0.00"},
                                    }));

    const auto beforeBegin = sessionState(s);
    s.begin(cell);
    expectDiff(beforeBegin, sessionState(s), changes({
                                                 {"active", "1"},
                                                 {"cell", "100.00,400.00,800.00,300.00"},
                                             }));

    const auto beforeEnd = sessionState(s);
    s.end();
    expectDiff(beforeEnd, sessionState(s), changes({
                                               {"active", "0"},
                                               {"cell", "0.00,0.00,0.00,0.00"},
                                           }));
}

void TestBarDragModel::dropTargetDecidesByPosition()
{
    QCOMPARE(dropTargetFor(0, false, true), DropTarget::DesktopItem);
    QCOMPARE(dropTargetFor(3, true, true), DropTarget::DesktopItem);
    QCOMPARE(dropTargetFor(-1, true, true), DropTarget::AddButton);
    QCOMPARE(dropTargetFor(-1, true, false), DropTarget::None);
    QCOMPARE(dropTargetFor(-1, false, true), DropTarget::None);
}

void TestBarDragModel::inputGateClosesDuringExit()
{
    QVERIFY(acceptsInput(true, false));
    QVERIFY(!acceptsInput(true, true));
    QVERIFY(!acceptsInput(false, false));
    QVERIFY(!acceptsInput(false, true));
}

void TestBarDragModel::ghostDropDoesNotReturnWhenMovedAway()
{
    const QRectF cell(100, 400, 800, 300);
    const QRectF ghostNow(2000, 1500, 380, 140);

    const GhostDrop away = ghostDropBehavior(true, ghostNow, cell);
    QCOMPARE(away.rect, ghostNow);
    QCOMPARE(away.opacity, 0.0);
    QVERIFY(away.durationMs <= 200);

    const GhostDrop back = ghostDropBehavior(false, ghostNow, cell);
    QCOMPARE(back.rect, cell);
    QCOMPARE(back.opacity, 1.0);
    QCOMPARE(back.durationMs, 400);
}

void TestBarDragModel::sessionBaseDesktopIsSticky()
{
    int base = 0;
    base = sessionBaseDesktop(base, 1);
    QCOMPARE(base, 1);
    QCOMPARE(sessionBaseDesktop(base, 2), 1);
    QCOMPARE(sessionBaseDesktop(base, 3), 1);
    base = 0;
    QCOMPARE(sessionBaseDesktop(base, 2), 2);
}

QTEST_MAIN(TestBarDragModel)
#include "tst_bardragmodel.moc"
