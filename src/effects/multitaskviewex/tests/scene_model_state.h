/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "data/workspacebarmetrics.h"
#include "model/backgrounditem.h"
#include "model/bardragmodel.h"
#include "model/baritem.h"
#include "model/scenemodel.h"
#include "model/windowitem.h"
#include "view/layers.h"

#include <QMap>
#include <QRectF>
#include <QString>
#include <QtTest>

#include <initializer_list>

namespace KWin
{

inline QString snapNum(qreal v)
{
    return QString::number(v, 'f', 2);
}

inline QString snapRect(const QRectF &r)
{
    return QStringLiteral("%1,%2,%3,%4")
        .arg(snapNum(r.x()), snapNum(r.y()), snapNum(r.width()), snapNum(r.height()));
}

inline void snapBar(QMap<QString, QString> &out, int i, const BarItem &b)
{
    const QString key = QStringLiteral("bar.%1").arg(i);
    out[key + QStringLiteral(".key")] = b.key;
    out[key + QStringLiteral(".num")] = QString::number(b.num);
    out[key + QStringLiteral(".uri")] = b.uri;
}

inline void snapWindow(QMap<QString, QString> &out, int i, const WindowItem &w)
{
    const QString key = QStringLiteral("win.%1").arg(i);
    out[key + QStringLiteral(".window")] = QString::number(reinterpret_cast<quintptr>(w.window));
    out[key + QStringLiteral(".src")] = snapRect(w.source);
    out[key + QStringLiteral(".retiring")] = w.retiring ? QStringLiteral("1") : QStringLiteral("0");
    out[key + QStringLiteral(".cellIndex")] = QString::number(w.cellIndex);
}

inline qreal modelSlideShiftX(const SceneModel &m)
{
    Q_UNUSED(m)
    return 0.0;
}

inline QMap<QString, QString> sessionModelState(const SceneModel &m)
{
    QMap<QString, QString> out;
    const bool anim = false;
    out[QStringLiteral("scene.animating")] = anim ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.needsFrame")] = anim ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.slideShiftX")] = snapNum(modelSlideShiftX(m));
    out[QStringLiteral("scene.dragActive")] = m.dragActive() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.dragCell")] = snapRect(m.dragCell());
    out[QStringLiteral("scene.dragPos")] = QStringLiteral("%1,%2").arg(snapNum(m.dragPos().x()), snapNum(m.dragPos().y()));
    out[QStringLiteral("scene.dragPlaceholder")] = QString::number(m.dragPlaceholder());
    out[QStringLiteral("scene.ghostReturning")] = m.ghostReturning() ? QStringLiteral("1") : QStringLiteral("0");
    out[QStringLiteral("scene.barPreviewActive")] = m.barPreviewActive() ? QStringLiteral("1") : QStringLiteral("0");
    return out;
}

inline QMap<QString, QString> barModelState(const SceneModel &m)
{
    QMap<QString, QString> out;
    for (int i = 0; i < m.barItems().size(); ++i) {
        snapBar(out, i, m.barItems()[i]);
    }
    return out;
}

inline QMap<QString, QString> gridModelState(const SceneModel &m)
{
    QMap<QString, QString> out;
    out[QStringLiteral("scene.rowHeight")] = snapNum(m.rowHeight());
    for (int i = 0; i < m.cellBase().size(); ++i) {
        out[QStringLiteral("cellBase.%1").arg(i)] = snapRect(m.cellBase()[i]);
    }
    return out;
}

inline QMap<QString, QString> windowModelState(const SceneModel &m)
{
    QMap<QString, QString> out;
    for (int i = 0; i < m.windowItems().size(); ++i) {
        snapWindow(out, i, m.windowItems()[i]);
        out[QStringLiteral("win.%1.cellIndexOf").arg(i)] = QString::number(m.windowItems()[i].cellIndex);
    }
    return out;
}

inline QMap<QString, QString> backgroundModelState(const SceneModel &m)
{
    QMap<QString, QString> out;
    out[QStringLiteral("bg.wallpaper")] = m.backgroundItem().wallpaper;
    return out;
}

inline QMap<QString, QString> merge(std::initializer_list<QMap<QString, QString>> maps)
{
    QMap<QString, QString> out;
    for (const QMap<QString, QString> &m : maps) {
        for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
            out.insert(it.key(), it.value());
        }
    }
    return out;
}

inline void expectDiff(const QMap<QString, QString> &before,
                       const QMap<QString, QString> &after,
                       const QMap<QString, QString> &expected,
                       const char *context = "")
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
            if (!expected.contains(it.key())) {
                unexpected << QStringLiteral("%1: %2 -> <vanished>").arg(it.key(), it.value());
            }
        }
    }
    for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
        if (!after.contains(it.key()) && !before.contains(it.key())) {
            missing << QStringLiteral("%1: declared but never happened (never appeared, expected %2)").arg(it.key(), it.value());
        }
    }

    const auto report = [&](const QStringList &list, const QString &what) {
        return list.isEmpty() ? QString() : QStringLiteral("\n  [%1]\n    %2").arg(what, list.join(QStringLiteral("\n    ")));
    };
    const QString detail = QStringLiteral("scene: %1").arg(QString::fromUtf8(context)) + report(unexpected, QStringLiteral("unexpected change (changing A touched B)"))
        + report(missing, QStringLiteral("declared but never happened (feature did not take effect)"))
        + report(wrongValue, QStringLiteral("value mismatch (inconsistent with design semantics)"));
    QVERIFY2(unexpected.isEmpty() && missing.isEmpty() && wrongValue.isEmpty(), qPrintable(detail));
}

inline QMap<QString, QString> changes(std::initializer_list<QPair<QString, QString>> items)
{
    QMap<QString, QString> out;
    for (const auto &p : items) {
        out[p.first] = p.second;
    }
    return out;
}

struct SettledScene
{
    WorkspaceBarMetrics metrics;
    WorkspaceBarDragModel drag;
    QVector<int> order{1, 2, 3};
    QVector<EffectWindow *> wins{reinterpret_cast<EffectWindow *>(0xA100),
                                 reinterpret_cast<EffectWindow *>(0xA200)};
    QVector<QRectF> sources{QRectF(0, 0, 1600, 900), QRectF(100, 100, 1200, 800)};
    QVector<QRectF> cells{QRectF(100, 400, 800, 300), QRectF(1000, 400, 800, 300)};

    SettledScene()
    {
        metrics.setSize(QSizeF(3840, 2160));
        WorkspaceBarDragModel::Config cfg;
        cfg.workspaceWidth = 480;
        cfg.workspaceHeight = 338;
        cfg.spacing = 80;
        cfg.listX = 1120;
        drag.setConfig(cfg);
    }

    void build(SceneModel &m) const
    {
        m.syncScreen(QPointF(0, 0), QSizeF(3840, 2160),
                     QRectF(0, 0, 3840, 2160), QRectF(0, 0, 3840, 2160), {});
        m.syncBarItems(keyedOrder(), QHash<QString, QString>());
        m.syncCellItems(cells, 705.0);
        m.syncWindows(wins, sources);
    }

    QVector<QPair<QString, int>> keyedOrder() const
    {
        QVector<QPair<QString, int>> kn;
        kn.reserve(order.size());
        for (int n : order) {
            kn.append({QStringLiteral("d%1").arg(n), n});
        }
        return kn;
    }
};

inline void runBarOps(SceneModel &m, const SettledScene &s)
{
    const auto keyed = [](const QVector<int> &nums) {
        QVector<QPair<QString, int>> kn;
        for (int n : nums) {
            kn.append({QStringLiteral("d%1").arg(n), n});
        }
        return kn;
    };
    m.syncBarItems(keyed(s.order), QHash<QString, QString>());

    WorkspaceBarDragModel reorder;
    reorder.setConfig(s.drag.config());
    const qreal slotW = s.drag.config().workspaceWidth + s.drag.config().spacing;
    const qreal cy = 200.0;
    reorder.begin({1, 2, 3}, 1, 0, 1120.0 + 240.0, cy);
    reorder.movePointer(1120.0 + 2 * slotW + 240.0, cy);
    m.barDragModel() = reorder;

    reorder.end();
    m.syncBarItems(keyed({1, 3}), QHash<QString, QString>());
    m.barDragModel() = reorder;

    m.syncBarItems(keyed({1, 3, 4}), QHash<QString, QString>());
}

inline QVector<QRectF> twoCells()
{
    return {QRectF(100, 400, 800, 300), QRectF(1000, 400, 600, 900)};
}

}
