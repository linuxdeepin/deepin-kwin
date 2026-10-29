/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "data/workspacebarmetrics.h"
#include "model/screenlayout.h"
#include "scene_state.h"
#include "view/screenview.h"
#include "view/screenviewset.h"

#include <QtTest>

using namespace KWin;

static EffectScreen *screenKey(quintptr v)
{
    return reinterpret_cast<EffectScreen *>(v);
}

static QMap<QString, QString> viewState(const ScreenView &view)
{
    QMap<QString, QString> out;
    const QString p = QStringLiteral("view.");
    out[p + QStringLiteral("screen")] = QString::number(reinterpret_cast<quintptr>(view.model.screen()));
    out[p + QStringLiteral("origin")] = QStringLiteral("%1,%2")
                                            .arg(snapNum(view.model.origin().x()), snapNum(view.model.origin().y()));

    out[p + QStringLiteral("layout.fullRect")] = snapRect(view.model.layout().fullRect);
    out[p + QStringLiteral("layout.clientRect")] = snapRect(view.model.layout().clientRect);
    out[p + QStringLiteral("layout.gridArea")] = snapRect(view.model.layout().gridArea);
    out[p + QStringLiteral("layout.offX")] = snapNum(view.model.layout().offX);
    out[p + QStringLiteral("layout.offY")] = snapNum(view.model.layout().offY);
    out[p + QStringLiteral("layout.barSize")] = QStringLiteral("%1,%2")
                                                    .arg(snapNum(view.model.layout().barSize.width()), snapNum(view.model.layout().barSize.height()));

    out[p + QStringLiteral("metrics.size")] = QStringLiteral("%1,%2")
                                                  .arg(snapNum(view.model.metrics().size().width()), snapNum(view.model.metrics().size().height()));
    out[p + QStringLiteral("metrics.workspaceWidth")] = snapNum(view.model.metrics().workspaceWidth());
    out[p + QStringLiteral("metrics.managerHeight")] = snapNum(view.model.metrics().managerHeight());

    for (int i = 0; i < view.model.baseCells().size(); ++i) {
        out[p + QStringLiteral("baseCells.%1").arg(i)] = snapRect(view.model.baseCells().at(i));
    }
    out[p + QStringLiteral("gridScrollTarget")] = snapNum(view.scene->gridScrollTarget());
    out[p + QStringLiteral("gridMaxScroll")] = snapNum(view.scene->gridMaxScroll());
    out[p + QStringLiteral("hoverCell")] = QString::number(view.model.hoverCell());

    for (int i = 0; i < view.model.dockAreas().size(); ++i) {
        out[p + QStringLiteral("dockAreas.%1").arg(i)] = snapRect(view.model.dockAreas().at(i));
    }

    out[p + QStringLiteral("barDrag.active")] = view.model.barDragModel().isActive() ? QStringLiteral("1") : QStringLiteral("0");
    out[p + QStringLiteral("barDrag.draggedId")] = QString::number(view.model.barDragModel().draggedId());
    out[p + QStringLiteral("barDrag.direction")] = QString::number(view.model.barDragModel().direction());
    out[p + QStringLiteral("barDrag.dragDelete")] = view.model.barDragModel().dragDelete() ? QStringLiteral("1") : QStringLiteral("0");
    QStringList dragOrder;
    for (int id : view.model.barDragModel().order()) {
        dragOrder << QString::number(id);
    }
    out[p + QStringLiteral("barDrag.order")] = dragOrder.join(QStringLiteral(","));

    out[p + QStringLiteral("windows.count")] = QString::number(view.model.windows().size());
    for (int i = 0; i < view.model.windows().size(); ++i) {
        out[p + QStringLiteral("windows.%1").arg(i)] = QString::number(reinterpret_cast<quintptr>(view.model.windows().at(i)));
    }
    QStringList cellMap;
    for (const auto &item : view.model.windowItems()) {
        if (item.retiring) {
            continue;
        }
        cellMap << QStringLiteral("%1->%2")
                       .arg(reinterpret_cast<quintptr>(item.window))
                       .arg(item.cellIndex);
    }
    cellMap.sort();
    out[p + QStringLiteral("windowCell")] = cellMap.join(QStringLiteral(","));
    out[p + QStringLiteral("draggedWindow")] = QString::number(reinterpret_cast<quintptr>(view.model.draggedWindow()));

    out[p + QStringLiteral("acquiredBgUri")] = view.acquiredBgUri;
    out[p + QStringLiteral("acquiredBgSize")] = QStringLiteral("%1x%2")
                                                    .arg(view.acquiredBgSize.width())
                                                    .arg(view.acquiredBgSize.height());

    const QMap<QString, QString> scene = merge({sessionState(*view.scene), barState(*view.scene),
                                                gridState(*view.scene), windowState(*view.scene),
                                                backgroundState(*view.scene)});
    for (auto it = scene.constBegin(); it != scene.constEnd(); ++it) {
        out[p + QStringLiteral("scene.") + it.key()] = it.value();
    }
    return out;
}

static QMap<QString, QString> allViewsState(const ScreenViewSet &set)
{
    QMap<QString, QString> out;
    out[QStringLiteral("set.count")] = QString::number(set.count());
    QStringList screens;
    for (const std::unique_ptr<ScreenView> &v : set.all()) {
        screens << QString::number(reinterpret_cast<quintptr>(v->model.screen()));
    }
    out[QStringLiteral("set.screens")] = screens.join(QStringLiteral(","));
    for (int i = 0; i < set.count(); ++i) {
        const QMap<QString, QString> one = viewState(*set.at(i));
        for (auto it = one.constBegin(); it != one.constEnd(); ++it) {
            out[QStringLiteral("v%1.").arg(i) + it.key()] = it.value();
        }
    }
    return out;
}

static QMap<QString, QString> only(const QMap<QString, QString> &m, const QStringList &prefixes)
{
    QMap<QString, QString> out;
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        for (const QString &p : prefixes) {
            if (it.key().startsWith(p)) {
                out.insert(it.key(), it.value());
                break;
            }
        }
    }
    return out;
}

namespace
{

EffectWindow *hoverOf(const ScreenView &view)
{
    return (view.model.hoverCell() >= 0 && view.model.hoverCell() < view.model.windows().size()) ? view.model.windows().value(view.model.hoverCell()) : nullptr;
}

EffectWindow *stepOf(const ScreenView &view, EffectWindow *w, int step)
{
    const int idx = view.model.windows().indexOf(w);
    if (idx < 0 || view.model.windows().isEmpty()) {
        return nullptr;
    }
    const int count = view.model.windows().size();
    return view.model.windows().value(((idx + step) % count + count) % count);
}

EffectWindow *firstOrLastOf(const ScreenView &view, bool last)
{
    return view.model.windows().isEmpty() ? nullptr : (last ? view.model.windows().last() : view.model.windows().first());
}

}

class TestMultiScreen : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void addingScreenLeavesExistingViewsUntouched();
    void removingScreenLeavesRemainingViewsUntouched();
    void ensureIsIdempotentAndPreservesState();
    void keyboardNavigationIsPerScreen();
    void paintTargetsCoverRenderTargetIntersection();
    void sessionEnterExitIteratesAllViews();
};

void TestMultiScreen::addingScreenLeavesExistingViewsUntouched()
{
    ScreenViewSet set;
    ScreenView *a = set.ensure(screenKey(0xA000));
    QVERIFY(a);
    a->model.syncScreen(QPointF(0, 0), QSizeF(1920, 1080),
                        QRectF(0, 0, 1920, 1080), QRectF(0, 0, 1920, 1040),
                        {QRectF(0, 1000, 1920, 40)});
    a->scene->setGridScrollTarget(12.5);
    a->scene->setGridMaxScroll(300.0);
    a->model.setHoverCell(0);
    a->model.windows() = {reinterpret_cast<EffectWindow *>(0x1111)};
    a->model.setDraggedWindow(reinterpret_cast<EffectWindow *>(0x1111));
    a->acquiredBgUri = QStringLiteral("file:///wall-a.png");
    a->acquiredBgSize = QSize(1920, 1080);
    a->model.syncCellItems({QRectF(100, 400, 800, 300)}, 300.0);
    a->model.syncWindows(a->model.windows(), {QRectF(100, 400, 800, 300)});
    a->scene->setEntered(true);
    a->scene->advance(2000);

    const auto beforeA = viewState(*a);
    const auto beforeSet = allViewsState(set);

    ScreenView *b = set.ensure(screenKey(0xB000));
    QVERIFY(b);
    QCOMPARE(set.count(), 2);

    expectDiff(beforeA, viewState(*a), {}, "screen added: the state of existing views is unchanged");
    expectDiff(only(beforeSet, {QStringLiteral("v0."), QStringLiteral("set.")}),
               only(allViewsState(set), {QStringLiteral("v0."), QStringLiteral("set.")}),
               changes({{QStringLiteral("set.count"), QStringLiteral("2")},
                        {QStringLiteral("set.screens"), QStringLiteral("%1,%2").arg(quintptr(0xA000)).arg(quintptr(0xB000))}}),
               "screen added: existing view slots are unchanged, only appended");

    QCOMPARE(b->model.screen(), screenKey(0xB000));
    QVERIFY(b->model.windows().isEmpty());
    QVERIFY(b->model.baseCells().isEmpty());
    QCOMPARE(b->model.hoverCell(), -1);
    QVERIFY(b->scene.get() != a->scene.get());
}

void TestMultiScreen::removingScreenLeavesRemainingViewsUntouched()
{
    ScreenViewSet set;
    ScreenView *a = set.ensure(screenKey(0xA000));
    ScreenView *b = set.ensure(screenKey(0xB000));
    ScreenView *c = set.ensure(screenKey(0xC000));
    QVERIFY(a && b && c);

    auto fill = [](ScreenView *v, qreal scroll, int hover) {
        v->model.syncScreen(v->model.origin(), QSizeF(1920, 1080), QRectF(0, 0, 1920, 1080), QRectF(0, 0, 1920, 1040), v->model.dockAreas());
        v->scene->setGridScrollTarget(scroll);
        v->model.setHoverCell(hover);
        v->model.windows() = {reinterpret_cast<EffectWindow *>(quintptr(0x1111 + int(scroll)))};
        v->acquiredBgUri = QStringLiteral("file:///w%1.png").arg(int(scroll));
        v->model.syncWindows(v->model.windows(), {QRectF(0, 0, 100, 100)});
    };
    fill(a, 10.0, 0);
    fill(b, 20.0, -1);
    fill(c, 30.0, 0);

    const auto beforeA = viewState(*a);
    const auto beforeC = viewState(*c);
    const auto beforeSet = allViewsState(set);

    set.remove(screenKey(0xB000));
    QCOMPARE(set.count(), 2);
    QCOMPARE(set.at(0), a);
    QCOMPARE(set.at(1), c);
    QVERIFY(set.find(screenKey(0xB000)) == nullptr);

    expectDiff(beforeA, viewState(*a), {}, "screen removed: surviving view a state is unchanged");
    expectDiff(beforeC, viewState(*c), {}, "screen removed: surviving view c state is unchanged (and not rebuilt)");
    expectDiff(only(beforeSet, {QStringLiteral("v0."), QStringLiteral("set.")}),
               only(allViewsState(set), {QStringLiteral("v0."), QStringLiteral("set.")}),
               changes({{QStringLiteral("set.count"), QStringLiteral("2")},
                        {QStringLiteral("set.screens"), QStringLiteral("%1,%2").arg(quintptr(0xA000)).arg(quintptr(0xC000))}}),
               "screen removed: first view slot is unchanged, insertion order preserved");

    QCOMPARE(a->scene->gridScrollTarget(), 10.0);
    QCOMPARE(c->scene->gridScrollTarget(), 30.0);
    QCOMPARE(a->acquiredBgUri, QStringLiteral("file:///w10.png"));
    QCOMPARE(c->acquiredBgUri, QStringLiteral("file:///w30.png"));
}

void TestMultiScreen::ensureIsIdempotentAndPreservesState()
{
    ScreenViewSet set;
    ScreenView *a = set.ensure(screenKey(0xA000));
    a->model.setHoverCell(2);
    a->scene->setGridScrollTarget(77.0);
    a->scene->setEntered(true);

    const auto before = allViewsState(set);
    ScreenView *again = set.ensure(screenKey(0xA000));
    QCOMPARE(again, a);
    QCOMPARE(set.count(), 1);
    expectDiff(before, allViewsState(set), {}, "idempotent ensure: no rebuild, state unchanged");
    QCOMPARE(a->model.hoverCell(), 2);
    QCOMPARE(a->scene->gridScrollTarget(), 77.0);

    QVERIFY(set.ensure(nullptr) == nullptr);
    QCOMPARE(set.count(), 1);
}

void TestMultiScreen::keyboardNavigationIsPerScreen()
{
    ScreenViewSet set;
    ScreenView *a = set.ensure(screenKey(0xA000));
    ScreenView *b = set.ensure(screenKey(0xB000));

    auto *w0 = reinterpret_cast<EffectWindow *>(0x1111);
    auto *w1 = reinterpret_cast<EffectWindow *>(0x2222);
    auto *w2 = reinterpret_cast<EffectWindow *>(0x3333);
    a->model.windows() = {w0, w1};
    b->model.windows() = {w1, w2};

    a->model.setHoverCell(0);
    b->model.setHoverCell(1);
    QCOMPARE(hoverOf(*a), w0);
    QCOMPARE(hoverOf(*b), w2);

    QCOMPARE(stepOf(*a, w0, +1), w1);
    QCOMPARE(stepOf(*a, w1, +1), w0);
    QCOMPARE(stepOf(*b, w2, +1), w1);
    QCOMPARE(stepOf(*b, w1, -1), w2);
    QVERIFY(stepOf(*a, w2, +1) == nullptr);
    QVERIFY(stepOf(*b, w0, +1) == nullptr);

    QCOMPARE(firstOrLastOf(*a, true), w1);
    QCOMPARE(firstOrLastOf(*b, true), w2);
    QCOMPARE(firstOrLastOf(*a, false), w0);
    QCOMPARE(firstOrLastOf(*b, false), w1);

    const auto before = allViewsState(set);
    a->model.setHoverCell(a->model.windows().indexOf(w1));
    QMap<QString, QString> expected;
    expected[QStringLiteral("v0.view.hoverCell")] = QStringLiteral("1");
    expectDiff(before, allViewsState(set), expected, "keyboard navigation is per-screen: only the affected screen changes");

    ScreenView *empty = set.ensure(screenKey(0xC000));
    QVERIFY(hoverOf(*empty) == nullptr);
    QVERIFY(stepOf(*empty, w0, +1) == nullptr);
    QVERIFY(firstOrLastOf(*empty, true) == nullptr);
    QVERIFY(firstOrLastOf(*empty, false) == nullptr);
}

void TestMultiScreen::paintTargetsCoverRenderTargetIntersection()
{
    const QVector<QRectF> one{QRectF(0, 0, 1920, 1080)};
    QCOMPARE(paintTargets(one, QRectF(0, 0, 1920, 1080)), QVector<int>{0});

    const QVector<QRectF> two{QRectF(0, 0, 2880, 1920), QRectF(2880, 0, 1920, 1080)};
    QCOMPARE(paintTargets(two, QRectF(0, 0, 4800, 1920)), (QVector<int>{0, 1}));
    QCOMPARE(paintTargets(two, QRectF(2880, 0, 1920, 1080)), QVector<int>{1});
    QVERIFY(paintTargets(two, QRectF(9999, 9999, 100, 100)).isEmpty());
    QVERIFY(paintTargets({}, QRectF(0, 0, 100, 100)).isEmpty());
}

void TestMultiScreen::sessionEnterExitIteratesAllViews()
{
    ScreenViewSet set;
    set.ensure(screenKey(0xA000));
    set.ensure(screenKey(0xB000));

    for (const std::unique_ptr<ScreenView> &v : set.all()) {
        v->scene->setEntered(true);
        v->scene->advance(1200);
    }
    QCOMPARE(set.at(0)->scene->enterProgress(), 1.0);
    QCOMPARE(set.at(1)->scene->enterProgress(), 1.0);
    QVERIFY(!set.at(0)->scene->exitFinished());
    QVERIFY(!set.at(1)->scene->exitFinished());

    auto exitFinished = [&set] {
        for (const std::unique_ptr<ScreenView> &v : set.all()) {
            if (!v->scene->exitFinished()) {
                return false;
            }
        }
        return true;
    };
    QVERIFY(!exitFinished());

    for (const std::unique_ptr<ScreenView> &v : set.all()) {
        v->scene->setEntered(false);
    }
    for (const std::unique_ptr<ScreenView> &v : set.all()) {
        v->scene->advance(1200);
    }
    QVERIFY(set.at(0)->scene->exitFinished());
    QVERIFY(set.at(1)->scene->exitFinished());
    QVERIFY(exitFinished());
}

QTEST_MAIN(TestMultiScreen)
#include "tst_multiscreen.moc"
