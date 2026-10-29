/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "model/bardraglayout.h"
#include "model/barpredicates.h"
#include "view/barlayer.h"

#include <QTest>

using namespace KWin;

namespace
{

QString snapNum(qreal v)
{
    return QString::number(v, 'f', 2);
}

QString snapRect(const QRectF &r)
{
    return QStringLiteral("%1,%2,%3,%4")
        .arg(snapNum(r.x()), snapNum(r.y()), snapNum(r.width()), snapNum(r.height()));
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

QMap<QString, QString> nodeState(const BarNode &n)
{
    QMap<QString, QString> out;
    out[QStringLiteral("node.key")] = n.key;
    out[QStringLiteral("node.rect")] = snapRect(n.rect());
    out[QStringLiteral("node.opacity")] = snapNum(n.cur.opacity);
    out[QStringLiteral("node.animating")] = n.animating() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("node.toX")] = snapNum(n.to.x);
    out[QStringLiteral("node.toOpacity")] = snapNum(n.to.opacity);
    out[QStringLiteral("node.phX")] = snapNum(n.placeholderX);
    out[QStringLiteral("node.phW")] = snapNum(n.placeholderW);
    out[QStringLiteral("node.phRect")] = snapRect(n.placeholderRect());
    return out;
}

struct TestBar
{
    BarLayer layer;
    BarLayer::AlignInput ai;
    QVector<BarItem> items;
    QVector<BarDragTarget> targets;

    TestBar()
    {
        ai.y = 40;
        ai.h = 338;
    }

    void add(const QString &key, int num, qreal x, qreal w, qreal opacity = 1.0,
             qreal phX = 0.0, qreal phW = 0.0, qreal y = 0.0)
    {
        items.append(BarItem{key, num, QString()});
        BarDragTarget t;
        t.id = num;
        t.x = x;
        t.y = y;
        t.w = w;
        t.opacity = opacity;
        t.cellX = x;
        t.placeholderX = phX;
        t.placeholderW = phW;
        targets.append(t);
    }

    void addPreview(const QString &key, qreal x, qreal w, qreal opacity = 1.0)
    {
        items.append(BarItem{key, 0, QString()});
        BarDragTarget t;
        t.id = kAddPreviewId;
        t.x = x;
        t.y = 0;
        t.w = w;
        t.opacity = opacity;
        t.cellX = x;
        t.placeholderX = 0;
        t.placeholderW = 0;
        targets.append(t);
    }

    void apply(const BarItem *preview = nullptr, int ms = 100, Easing e = Easing::Linear)
    {
        layer.applyTargets(items, preview, targets, ms, e, ai);
    }

    void clear()
    {
        items.clear();
        targets.clear();
    }
};

}

class TestBarItem : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void snapAndQuery();
    void transitionHalfwayAndDone();
    void retargetMidwayNoJump();
    void placeholderInterpolation();
    void alignRetainsNodesByKey();
    void ghostOnRemoveThenDestroy();
    void reviveBeforeGhostEnds();
    void previewCommitsIntoSlot();
    void previewCommitOrderMatters();
    void dragRaisedFollowsPointerImmediately();
};

void TestBarItem::snapAndQuery()
{
    TestBar tb;
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.apply();
    tb.layer.advance(0);
    const BarNode &n = tb.layer.nodes()[0];

    expectDiff({}, nodeState(n), changes({
                                     {"node.key", "k1"},
                                     {"node.rect", "100.00,40.00,480.00,338.00"},
                                     {"node.opacity", "1.00"},
                                     {"node.animating", "0"},
                                     {"node.toX", "100.00"},
                                     {"node.toOpacity", "1.00"},
                                     {"node.phX", "0.00"},
                                     {"node.phW", "0.00"},
                                     {"node.phRect", "240.00,40.00,0.00,338.00"},
                                 }));
}

void TestBarItem::transitionHalfwayAndDone()
{
    TestBar tb;
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.apply(nullptr, 100, Easing::Linear);
    tb.layer.advance(100);
    const auto before = nodeState(tb.layer.nodes()[0]);

    tb.targets[0].x = 1000;
    tb.apply(nullptr, 100, Easing::Linear);
    tb.layer.advance(0);
    QCOMPARE(tb.layer.nodes()[0].cur.x, qreal(100));

    tb.layer.advance(50);
    expectDiff(before, nodeState(tb.layer.nodes()[0]), changes({
                                                           {"node.rect", "550.00,40.00,480.00,338.00"},
                                                           {"node.animating", "1"},
                                                           {"node.toX", "1000.00"},
                                                       }));

    tb.layer.advance(50);
    const BarNode &done = tb.layer.nodes()[0];
    QVERIFY2(!done.animating(), "must converge after advancing the full duration");
    QCOMPARE(done.rect().x(), qreal(1000));
}

void TestBarItem::retargetMidwayNoJump()
{
    TestBar tb;
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.apply(nullptr, 100, Easing::Linear);
    tb.targets[0].x = 500;
    tb.apply(nullptr, 100, Easing::Linear);
    tb.layer.advance(50);
    const qreal beforeX = tb.layer.nodes()[0].rect().x();
    QCOMPARE(beforeX, qreal(300));

    tb.targets[0].x = 900;
    tb.apply(nullptr, 100, Easing::Linear);
    const BarNode &n = tb.layer.nodes()[0];
    QCOMPARE(n.from.x, beforeX);
    QCOMPARE(n.to.x, qreal(900));
    tb.layer.advance(50);
    QCOMPARE(n.rect().x(), qreal(600));
}

void TestBarItem::placeholderInterpolation()
{
    TestBar tb;
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.apply(nullptr, 100, Easing::Linear);
    tb.targets[0].placeholderX = 240;
    tb.targets[0].placeholderW = 240;
    tb.apply(nullptr, 100, Easing::Linear);
    tb.layer.advance(50);
    const BarNode &n = tb.layer.nodes()[0];
    QCOMPARE(n.placeholderW, qreal(120));
    QCOMPARE(n.placeholderX, qreal(120));
    tb.layer.advance(50);
    QCOMPARE(n.placeholderW, qreal(240));
}

void TestBarItem::alignRetainsNodesByKey()
{
    TestBar tb;
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.add(QStringLiteral("k2"), 2, 700, 480);
    tb.apply(nullptr, 100, Easing::Linear);
    tb.layer.advance(50);

    tb.targets[1].x = 900;
    tb.apply(nullptr, 100, Easing::Linear);
    QCOMPARE(tb.layer.nodes().size(), 2);
    QCOMPARE(tb.layer.nodes()[0].key, QStringLiteral("k1"));
    QCOMPARE(tb.layer.nodes()[1].key, QStringLiteral("k2"));
    QCOMPARE(tb.layer.nodes()[1].from.x, qreal(700));
}

void TestBarItem::ghostOnRemoveThenDestroy()
{
    TestBar tb;
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.add(QStringLiteral("k2"), 2, 700, 480);
    tb.apply(nullptr, 400, Easing::InOutCubic);
    tb.layer.advance(200);

    tb.clear();
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.apply(nullptr, 400, Easing::InOutCubic);
    QCOMPARE(tb.layer.nodes().size(), 1);
    tb.layer.advance(0);
    tb.layer.advance(300);
    QVERIFY2(tb.layer.nodes().size() == 1, "there is only one real node, k1");
    tb.layer.advance(500);
    QVERIFY2(!tb.layer.animating(), "after the ghost converges and is destroyed, no more frames are requested");
}

void TestBarItem::reviveBeforeGhostEnds()
{
    TestBar tb;
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.add(QStringLiteral("k2"), 2, 700, 480);
    tb.apply(nullptr, 400, Easing::InOutCubic);
    tb.layer.advance(200);

    tb.clear();
    tb.add(QStringLiteral("k1"), 1, 100, 480);
    tb.apply(nullptr, 400, Easing::InOutCubic);
    tb.layer.advance(50);

    tb.add(QStringLiteral("k2"), 2, 900, 480);
    tb.apply(nullptr, 400, Easing::InOutCubic);
    QCOMPARE(tb.layer.nodes().size(), 2);
    QCOMPARE(tb.layer.nodes()[1].key, QStringLiteral("k2"));
    QVERIFY2(!tb.layer.nodes()[1].ghost, "not a ghost after revival");
    QVERIFY2(tb.layer.nodes()[1].cur.opacity > 0.01, "revival continues from the current visual state (no flicker)");
}

QTEST_MAIN(TestBarItem)
#include "tst_baritem.moc"

void TestBarItem::previewCommitsIntoSlot()
{
    TestBar tb;
    BarItem preview;
    preview.key = QStringLiteral("__preview__");

    tb.addPreview(QStringLiteral("__preview__"), 3000, 480);
    tb.apply(&preview, 400, Easing::InOutCubic);
    QVERIFY2(tb.layer.previewNode(), "the preview node must exist while hovering");
    QCOMPARE(tb.layer.previewNode()->rect().x(), 3000.0);

    tb.clear();
    tb.add(QStringLiteral("d1"), 1, 1120, 480);
    tb.apply(nullptr, 400, Easing::InOutCubic);

    QCOMPARE(tb.layer.nodes().size(), 1);
    QCOMPARE(tb.layer.nodes()[0].key, QStringLiteral("d1"));
    QCOMPARE(tb.layer.nodes()[0].from.x, 3000.0);
    QCOMPARE(tb.layer.nodes()[0].to.x, 1120.0);
    QVERIFY2(tb.layer.nodes()[0].animating(), "the handover must animate (not snap to the slot)");
    QVERIFY2(!tb.layer.previewNode(), "the preview node must be cleared after commit");
}

void TestBarItem::dragRaisedFollowsPointerImmediately()
{
    TestBar tb;
    tb.add(QStringLiteral("a"), 1, 100, 480);
    tb.apply(nullptr, 400, Easing::InOutCubic);

    tb.ai.draggedId = 1;
    tb.ai.dragActive = true;
    tb.targets[0].x = 200;
    tb.apply(nullptr, 150, Easing::InOutCubic);
    QCOMPARE(tb.layer.nodes()[0].cur.x, 200.0);
    QVERIFY2(!tb.layer.nodes()[0].animating(), "follows the pointer immediately: no transition");

    tb.targets[0].x = 300;
    tb.apply(nullptr, 150, Easing::InOutCubic);
    QCOMPARE(tb.layer.nodes()[0].cur.x, 300.0);

    tb.ai.dragActive = false;
    tb.ai.draggedId = -1;
    tb.targets[0].x = 1120;
    tb.apply(nullptr, 400, Easing::InOutCubic);
    QVERIFY2(tb.layer.nodes()[0].animating(), "after release it must animate (not snap immediately)");
    QCOMPARE(tb.layer.nodes()[0].to.x, 1120.0);
}

void TestBarItem::previewCommitOrderMatters()
{
    const auto nodeFor = [](const BarLayer &layer, const QString &key) -> const BarNode * {
        for (const BarNode &n : layer.nodes()) {
            if (n.key == key) {
                return &n;
            }
        }
        return nullptr;
    };

    {
        TestBar tb;
        BarItem p;
        p.key = QStringLiteral("__preview__");
        tb.add(QStringLiteral("a"), 1, 1360, 480);
        tb.addPreview(QStringLiteral("__preview__"), 3320, 480);
        tb.apply(&p, 400, Easing::InOutCubic);
        tb.clear();
        tb.add(QStringLiteral("a"), 1, 1360, 480);
        tb.add(QStringLiteral("d3"), 3, 2240, 480);
        tb.apply(nullptr, 400, Easing::InOutCubic);
        const BarNode *n = nodeFor(tb.layer, QStringLiteral("d3"));
        QVERIFY(n);
        QCOMPARE(n->from.x, 3320.0);
        QCOMPARE(n->to.x, 2240.0);
        QVERIFY2(n->animating(), "correct order: new items must have a handover transition");
    }

    {
        TestBar tb;
        BarItem p;
        p.key = QStringLiteral("__preview__");
        tb.add(QStringLiteral("a"), 1, 1360, 480);
        tb.addPreview(QStringLiteral("__preview__"), 3320, 480);
        tb.apply(&p, 400, Easing::InOutCubic);
        tb.add(QStringLiteral("d3"), 3, 2240, 480);
        tb.apply(&p, 400, Easing::InOutCubic);
        const BarNode *n = nodeFor(tb.layer, QStringLiteral("d3"));
        QVERIFY(n);
        QCOMPARE(n->from.x, 2240.0);
        QVERIFY2(!n->animating(), "wrong order: new items are snapped (no handover)");
    }
}
