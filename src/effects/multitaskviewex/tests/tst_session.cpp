/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "model/backgrounditem.h"
#include "model/baritem.h"
#include "model/elevationstate.h"
#include "model/windowitem.h"
#include "scene_state.h"
#include "view/windowitemlayer.h"

#include <QtTest>

using namespace KWin;

namespace
{

QMap<QString, QString> elevState(const ExitElevationState &st)
{
    QMap<QString, QString> out;
    out[QStringLiteral("elevated")] = QString::number(reinterpret_cast<quintptr>(st.elevated()));
    out[QStringLiteral("selected")] = QString::number(reinterpret_cast<quintptr>(st.selected()));
    return out;
}

EffectWindow *fakeWindow(quintptr n)
{
    return reinterpret_cast<EffectWindow *>(n);
}

WindowItem makeWindowItem(const QRectF &source)
{
    WindowItem item;
    item.setStatic(nullptr, source);
    return item;
}

QMap<QString, QString> itemStateForWindow(const WindowItem &w)
{
    QMap<QString, QString> out;
    snapWindow(out, 0, w);
    return out;
}

QMap<QString, QString> nodeState(const WindowNode &n)
{
    QMap<QString, QString> out;
    out[QStringLiteral("node.rect")] = snapRect(n.rect());
    out[QStringLiteral("node.animating")] = n.animating() ? QStringLiteral("1") : QStringLiteral("0");
    return out;
}

}

class TestSession : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void windowItemFirstEnterEqualsReenter();
    void windowItemSetTargetSameValueDoesNotRestart();
    void elevationPrefersSelectedWindow();
    void elevationNeverSurvivesInterrupt();
    void elevationClearedAtEveryBoundary();
};

void TestSession::windowItemFirstEnterEqualsReenter()
{
    const QRectF source(100, 100, 800, 600);
    const WindowItem first = makeWindowItem(source);

    QMap<QString, QString> firstState;
    snapWindow(firstState, 0, first);
    expectDiff({}, firstState, changes({
                                   {"win.0.window", "0"},
                                   {"win.0.src", "100.00,100.00,800.00,600.00"},
                                   {"win.0.retiring", "0"},
                                   {"win.0.cellIndex", "-1"},
                               }));
}

void TestSession::windowItemSetTargetSameValueDoesNotRestart()
{
    WindowNode node;
    node.interp.snap(VisualState{100, 100, 800, 600, 1.0});

    const VisualState target{900, 100, 800, 600, 1.0};
    node.setTarget(QRectF(900, 100, 800, 600), 1.0, 100, Easing::Linear);
    QVERIFY(node.animating());
    node.advance(50);

    const auto mid = nodeState(node);
    node.setTarget(QRectF(900, 100, 800, 600), 1.0, 100, Easing::Linear);
    expectDiff(mid, nodeState(node), changes({}));

    node.advance(10);
    QCOMPARE(node.rect().x(), 580.0);

    node.setTarget(QRectF(0, 100, 800, 600), 1.0, 100, Easing::Linear);
    QVERIFY(node.animating());
    node.advance(50);
    QCOMPARE(node.rect().x(), 290.0);
}

void TestSession::elevationPrefersSelectedWindow()
{
    ExitElevationState st;
    expectDiff({}, elevState(st), changes({
                                      {"elevated", "0"},
                                      {"selected", "0"},
                                  }));

    auto before = elevState(st);
    st.request(fakeWindow(11), true, fakeWindow(22), true);
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "11"},
                                          {"selected", "11"},
                                      }));

    before = elevState(st);
    st.request(fakeWindow(11), false, fakeWindow(22), true);
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "22"},
                                      }));

    before = elevState(st);
    st.request(fakeWindow(11), false, fakeWindow(22), false);
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "0"},
                                      }));

    before = elevState(st);
    st.request(fakeWindow(0), false, fakeWindow(22), true);
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "22"},
                                          {"selected", "0"},
                                      }));
}

void TestSession::elevationNeverSurvivesInterrupt()
{
    ExitElevationState st;
    auto before = elevState(st);
    st.request(fakeWindow(11), true, fakeWindow(0), false);
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "11"},
                                          {"selected", "11"},
                                      }));

    before = elevState(st);
    st.interrupt();
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "0"},
                                      }));

    before = elevState(st);
    st.request(fakeWindow(11), true, fakeWindow(0), false);
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "11"},
                                      }));

    before = elevState(st);
    st.interrupt();
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "0"},
                                      }));

    before = elevState(st);
    st.request(fakeWindow(11), true, fakeWindow(0), false);
    expectDiff(before, elevState(st), changes({
                                          {"elevated", "11"},
                                      }));
}

void TestSession::elevationClearedAtEveryBoundary()
{
    ExitElevationState st;
    for (int i = 0; i < 3; ++i) {
        auto before = elevState(st);
        st.request(fakeWindow(7), true, fakeWindow(0), false);
        if (i == 0) {
            expectDiff(before, elevState(st), changes({
                                                  {"elevated", "7"},
                                                  {"selected", "7"},
                                              }));
        } else {
            expectDiff(before, elevState(st), changes({
                                                  {"elevated", "7"},
                                              }));
        }

        before = elevState(st);
        if (i == 0) {
            st.finish();
        } else if (i == 1) {
            st.interrupt();
        } else {
            st.sessionBoundary();
        }
        if (i == 2) {
            expectDiff(before, elevState(st), changes({
                                                  {"elevated", "0"},
                                                  {"selected", "0"},
                                              }));
        } else {
            expectDiff(before, elevState(st), changes({
                                                  {"elevated", "0"},
                                              }));
        }
    }

    st.request(fakeWindow(9), true, fakeWindow(0), false);
    st.sessionBoundary();
    expectDiff({}, elevState(st), changes({
                                      {"elevated", "0"},
                                      {"selected", "0"},
                                  }));
}

QTEST_MAIN(TestSession)
#include "tst_session.moc"
