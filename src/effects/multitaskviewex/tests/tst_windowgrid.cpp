/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "model/windowgrid.h"
#include "model/windowitem.h"

#include <QtTest>

using namespace KWin;

static QString rstr(const QRectF &r)
{
    return QStringLiteral("(%1,%2 %3x%4)").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

static bool feq(qreal a, qreal b)

{
    return qAbs(a - b) < 0.01;
}

static QVector<QRectF> referenceOriginalGrid(const QVector<QSize> &naturals,
                                             const QRectF &clientRect,
                                             qreal screenH,
                                             qreal minSpacingH,
                                             qreal spacingW)
{
    const int n = naturals.size();
    QVector<QRectF> targets;
    targets.resize(n);
    if (n == 0)
        return targets;

    qreal scaleHeight = screenH * (720.0 / 1080.0);
    qreal totalw = spacingW;
    int row = 1, index = 1;
    QList<qreal> centerList;
    bool overlap;
    do {
        overlap = false;
        centerList.clear();
        totalw = spacingW;
        index = 1;
        for (int i = n - 1; i >= 0; --i) {
            qreal width = naturals[i].width();
            if (naturals[i].height() > scaleHeight) {
                width = naturals[i].width() * (scaleHeight / naturals[i].height());
            }
            totalw += width + spacingW;
            if (width + spacingW * 2 > clientRect.width()) {
                break;
            }
            if (totalw > clientRect.width()) {
                index++;
                if (index > row) {
                    break;
                }
                centerList.append((clientRect.width() - totalw + width + spacingW) / 2 + spacingW + clientRect.x());
                totalw = spacingW + width + spacingW;
            }
        }
        centerList.append((clientRect.width() - totalw) / 2.0 + spacingW + clientRect.x());
        if (totalw > clientRect.width()) {
            centerList.clear();
            overlap = true;
            scaleHeight -= 15;
            const qreal critical = (clientRect.height() - (row + 2) * minSpacingH) / (row + 1);
            if (scaleHeight <= critical) {
                row++;
            }
            index = 1;
            totalw = spacingW;
        }
    } while (overlap);

    qreal winYPos = (clientRect.height() - (index - 1) * minSpacingH - index * scaleHeight) / 2 + clientRect.y();
    row = 1;
    qreal x = centerList.value(row - 1);
    totalw = spacingW;
    for (int i = n - 1; i >= 0; --i) {
        qreal width = 0.0, height = 0.0;
        if (naturals[i].height() > scaleHeight) {
            width = naturals[i].width() * (scaleHeight / naturals[i].height());
            height = scaleHeight;
        } else {
            width = naturals[i].width();
            height = naturals[i].height();
        }
        totalw += width + spacingW;
        if (totalw > clientRect.width()) {
            row++;
            totalw = spacingW + width + spacingW;
            x = centerList.value(row - 1);
            winYPos += minSpacingH + scaleHeight;
        }
        targets[i] = QRectF(x, winYPos + (scaleHeight - height) / 2, width, height);
        x += width + spacingW;
    }
    return targets;
}

class TestWindowGrid : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void singleWindowCentered();
    void twoWindowsSameRow();
    void tallWindowScaled();
    void wideWindowsMultiRow();
    void multiRowPerRowCentered();
    void regionOffset();
    void invariantProperties();
    void extremeSizes();
    void boundaryInputs();
    void manyWindows();
    void referenceOriginalMatch();
    void coverForGridCellsMatchesRowHeight();
    void extremeWideWindowGetsCover();
    void coverMatchesRowHeightForExtremeWide();
    void rowHeightFormulaMatchesRealMachine();
    void coverDerivesFromWindowGeometry();
    void dragGhostScaleFollowsDesign();
    void ghostBasisIsGridCell();
    void ghostKeepsAspectWhenEnteringBar();
};

void TestWindowGrid::singleWindowCentered()
{
    QVector<QRectF> r = layoutWindowGrid({QSize(1000, 800)}, QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    QCOMPARE(r.size(), 1);
    const qreal expY = (1672.0 - 2022.0 * 720.0 / 1080.0) / 2 + 350 + (2022.0 * 720.0 / 1080.0 - 800) / 2;
    QVERIFY2(feq(r[0].x(), 1420), qPrintable(QStringLiteral("x=%1").arg(r[0].x())));
    QVERIFY2(feq(r[0].width(), 1000), "width = natural size");
    QVERIFY2(feq(r[0].height(), 800), "height = natural size");
    QVERIFY2(feq(r[0].y(), expY), qPrintable(QStringLiteral("y=%1 exp=%2").arg(r[0].y()).arg(expY)));
}

void TestWindowGrid::twoWindowsSameRow()
{
    QVector<QRectF> r = layoutWindowGrid({QSize(1000, 800), QSize(900, 700)},
                                         QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    QCOMPARE(r.size(), 2);
    QVERIFY2(feq(r[1].x(), 950), qPrintable(QStringLiteral("x1=%1").arg(r[1].x())));
    QVERIFY2(feq(r[0].x(), 1890), qPrintable(QStringLiteral("x0=%1").arg(r[0].x())));
    QVERIFY2(r[1].y() > r[0].y(), "shorter window has larger y (vertically centered in the row)");
}

void TestWindowGrid::tallWindowScaled()
{
    QVector<QRectF> r = layoutWindowGrid({QSize(500, 1600)}, QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    QCOMPARE(r.size(), 1);
    const qreal expSH = 2022.0 * 720.0 / 1080.0;
    QVERIFY2(feq(r[0].height(), expSH), qPrintable(QStringLiteral("h=%1").arg(r[0].height())));
    QVERIFY2(feq(r[0].width(), 500.0 * expSH / 1600.0), "uniform scaling");
    const qreal w = 500.0 * expSH / 1600.0;
    const qreal expX = (3840 - (40 + w + 40)) / 2 + 40;
    QVERIFY2(feq(r[0].x(), expX), qPrintable(QStringLiteral("x=%1 exp=%2").arg(r[0].x()).arg(expX)));
}

void TestWindowGrid::wideWindowsMultiRow()
{
    QVector<QRectF> r = layoutWindowGrid({QSize(1600, 800), QSize(1600, 800),
                                          QSize(1600, 800), QSize(1600, 800)},
                                         QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    QCOMPARE(r.size(), 4);
    QVERIFY2(r[0].height() <= 930, qPrintable(QStringLiteral("row height=%1").arg(r[0].height())));
    QVERIFY2(feq(r[0].height(), r[2].height()), "both rows have the same row height");
    QVERIFY2(r[0].y() > r[2].y(), qPrintable(QStringLiteral("y0=%1 y2=%2").arg(r[0].y()).arg(r[2].y())));
    QVERIFY2(r[2].x() > r[3].x(), "order within row 0");
}

void TestWindowGrid::multiRowPerRowCentered()
{
    QVector<QRectF> r = layoutWindowGrid({QSize(1600, 800), QSize(1600, 800), QSize(800, 800)},
                                         QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    QCOMPARE(r.size(), 3);
    const qreal sh = r[0].height();
    QVERIFY2(sh > 700 && sh < 915, qPrintable(QStringLiteral("row height=%1").arg(sh)));

    const qreal row1W = sh + 2 * sh;
    const qreal xRow1 = (3840 - (row1W + 3 * 40)) / 2 + 40;
    QVERIFY2(feq(r[2].x(), xRow1), qPrintable(QStringLiteral("row1 left x=%1 exp=%2").arg(r[2].x()).arg(xRow1)));
    QVERIFY2(feq(r[1].x(), xRow1 + sh + 40), "row1 second = left edge + sh + 40");
    QVERIFY2(feq(r[1].y(), r[2].y()), "same row, same y");

    const qreal xRow2 = (3840 - (2 * sh + 2 * 40)) / 2 + 40;
    QVERIFY2(feq(r[0].x(), xRow2), qPrintable(QStringLiteral("row2 x=%1 exp=%2").arg(r[0].x()).arg(xRow2)));
    QVERIFY2(qAbs(r[0].x() - r[2].x()) > 1, "row2 single window and row1 two windows differ in x (each row is centered independently)");
    const qreal row1Top = (1672 - 37 - 2 * sh) / 2 + 350;
    QVERIFY2(feq(r[2].y(), row1Top), "800x800 window scaled to sh square -> no in-row offset (y = row1 top)");
    QVERIFY2(feq(r[1].y(), r[2].y()), "within row1, y is consistent");
    QVERIFY2(feq(r[0].y(), row1Top + 37 + sh), "row2 top = row1 top + spacingH + scaleHeight");
}

void TestWindowGrid::regionOffset()
{
    const QVector<QSize> in = {QSize(1000, 800), QSize(900, 700), QSize(1600, 800)};
    const QRectF base(0, 350, 3840, 1672);
    const QRectF off(200, 500, 3840, 1672);
    const auto r0 = layoutWindowGrid(in, base, 2022, 37, 40);
    const auto r1 = layoutWindowGrid(in, off, 2022, 37, 40);
    QCOMPARE(r0.size(), r1.size());
    for (int i = 0; i < r0.size(); ++i) {
        const bool shifted = feq(r1[i].x(), r0[i].x() + 200) && feq(r1[i].y(), r0[i].y() + 150);
        QVERIFY2(shifted, qPrintable(QStringLiteral("cell%1 shifted: (%2,%3) vs (%4,%5)").arg(i).arg(r1[i].x()).arg(r1[i].y()).arg(r0[i].x() + 200).arg(r0[i].y() + 150)));
    }
}

static void checkInvariants(const QVector<QSize> &in, const QRectF &area, qreal screenH, qreal spacingH, qreal spacingW)
{
    const QVector<QRectF> r = layoutWindowGrid(in, area, screenH, spacingH, spacingW);
    QCOMPARE(r.size(), in.size());
    for (const QRectF &c : r) {
        const bool inBounds = c.left() >= area.x() - 0.5 && c.top() >= area.y() - 0.5
            && c.right() <= area.right() + 0.5 && c.bottom() <= area.bottom() + 0.5;
        QVERIFY2(inBounds, qPrintable(QStringLiteral("out of bounds: %1").arg(rstr(c))));
    }
    for (int i = 0; i < in.size(); ++i) {
        if (in[i].height() > 0 && r[i].height() > 0) {
            const qreal srcA = qreal(in[i].width()) / in[i].height();
            const qreal dstA = r[i].width() / r[i].height();
            const bool keepAspect = qAbs(srcA - dstA) < 0.01;
            QVERIFY2(keepAspect, qPrintable(QStringLiteral("window%1 aspect ratio %2 vs %3").arg(i).arg(srcA).arg(dstA)));
        }
    }
    QList<qreal> ys;
    for (const QRectF &c : r) {
        bool found = false;
        for (qreal y : ys) {
            if (qAbs(y - c.y()) < 1.0) {
                found = true;
                break;
            }
        }
        if (!found) {
            ys.append(c.y());
        }
    }
    std::sort(ys.begin(), ys.end());
    for (int i = 1; i < ys.size(); ++i) {
        const qreal gap = ys[i] - ys[i - 1];
        const bool gapOk = qAbs(gap - spacingH) < 1.0 || gap > spacingH;
        QVERIFY2(gapOk, qPrintable(QStringLiteral("row gap=%1").arg(gap)));
    }
    for (int i = 0; i < r.size(); ++i) {
        for (int j = i + 1; j < r.size(); ++j) {
            if (qAbs(r[i].y() - r[j].y()) < 1.0) {
                const bool noOverlap = r[i].right() <= r[j].left() + 0.01 || r[j].right() <= r[i].left() + 0.01;
                QVERIFY2(noOverlap, qPrintable(QStringLiteral("overlap: %1 %2").arg(rstr(r[i])).arg(rstr(r[j]))));
            }
        }
    }
}

void TestWindowGrid::invariantProperties()
{
    const QVector<QSize> cases[] = {
        {QSize(800, 600), QSize(1200, 900), QSize(500, 1600), QSize(1920, 1080), QSize(300, 400)},
        {QSize(2000, 800), QSize(2000, 800), QSize(2000, 800)},
        {QSize(400, 200), QSize(400, 200), QSize(400, 200), QSize(400, 200), QSize(400, 200), QSize(400, 200), QSize(400, 200)},
        {QSize(100, 50)},
        {QSize(1920, 1080), QSize(800, 200)},
        {QSize(1600, 900), QSize(1600, 900), QSize(800, 450), QSize(800, 450)},
    };
    for (const auto &in : cases) {
        checkInvariants(in, QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    }
    checkInvariants({QSize(800, 600), QSize(500, 1600)}, QRectF(100, 200, 2560, 1300), 1400, 24, 30);
    checkInvariants({QSize(800, 600), QSize(1200, 900), QSize(500, 1600)}, QRectF(0, 0, 1280, 800), 800, 10, 12);
}

void TestWindowGrid::extremeSizes()
{
    checkInvariants({QSize(3840, 2160)}, QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    checkInvariants({QSize(10, 10)}, QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    checkInvariants({QSize(3900, 1000)}, QRectF(0, 350, 3840, 1672), 2022, 37, 40);
    checkInvariants({QSize(1, 1), QSize(1, 1), QSize(1, 1)}, QRectF(0, 350, 3840, 1672), 2022, 37, 40);
}

void TestWindowGrid::boundaryInputs()
{
    const QRectF area(0, 350, 3840, 1672);
    QCOMPARE(layoutWindowGrid({}, area, 2022, 37, 40).size(), 0);
    checkInvariants({QSize(0, 0)}, area, 2022, 37, 40);
    checkInvariants({QSize(1, 1)}, area, 2022, 37, 40);
}

void TestWindowGrid::manyWindows()
{
    QVector<QSize> in;
    for (int i = 0; i < 24; ++i) {
        in.append(QSize(800, 600));
    }
    checkInvariants(in, QRectF(0, 350, 3840, 1672), 2022, 37, 40);
}

void TestWindowGrid::referenceOriginalMatch()
{
    const QRectF clientRect(0, 350, 3840, 1672);
    const qreal screenH = 2022;
    const QVector<QSize> cases[] = {
        {QSize(900, 600), QSize(600, 800), QSize(1400, 900)},
        {QSize(1000, 800), QSize(900, 700)},
        {QSize(500, 1600)},
        {QSize(800, 600), QSize(1200, 900), QSize(500, 1600), QSize(1920, 1080), QSize(300, 400)},
        {QSize(1600, 800), QSize(1600, 800), QSize(1600, 800), QSize(1600, 800)},
        {QSize(100, 50), QSize(2000, 800), QSize(640, 480)},
    };
    for (const auto &in : cases) {
        const auto mine = layoutWindowGrid(in, clientRect, screenH, 37, 40);
        const auto ref = referenceOriginalGrid(in, clientRect, screenH, 37, 40);
        QCOMPARE(mine.size(), ref.size());
        for (int i = 0; i < mine.size(); ++i) {
            const bool same = feq(mine[i].x(), ref[i].x()) && feq(mine[i].y(), ref[i].y())
                && feq(mine[i].width(), ref[i].width()) && feq(mine[i].height(), ref[i].height());
            QVERIFY2(same, qPrintable(QStringLiteral("window%1 cell mismatch: %2 vs original %3").arg(i).arg(rstr(mine[i])).arg(rstr(ref[i]))));
        }
    }
}

void TestWindowGrid::extremeWideWindowGetsCover()
{
    const QSizeF fit = fitWindowIntoRow(QSize(3840, 200), 1440.0, 1408.0);
    QCOMPARE(fit.width(), 1408.0);
    QCOMPARE(fit.height(), 200.0 * (1408.0 / 3840.0));
    QVERIFY(fit.height() < 1440.0);
    const QSizeF normal = fitWindowIntoRow(QSize(800, 600), 1440.0, 1408.0);
    QCOMPARE(normal, QSizeF(800, 600));
    const QSizeF tall = fitWindowIntoRow(QSize(600, 1200), 600.0, 1408.0);
    QCOMPARE(tall, QSizeF(300, 600));
    const QSizeF both = fitWindowIntoRow(QSize(3840, 3000), 1440.0, 1408.0);
    QVERIFY(both.width() <= 1408.0);
    QVERIFY(both.height() <= 1440.0);
}

void TestWindowGrid::coverMatchesRowHeightForExtremeWide()
{
    const QVector<QSize> naturals{QSize(3840, 200), QSize(800, 600)};
    const QRectF area(0, 350, 3840, 1672);
    const qreal spacingH = 37, spacingW = 40;
    qreal scaleH = 0;
    const QVector<QRectF> cells =
        layoutWindowGrid(naturals, area, 2160, spacingH, spacingW, &scaleH);

    QCOMPARE(cells.size(), 2);
    QVERIFY(scaleH > 0);

    QVERIFY2(cells[0].width() <= area.width() - spacingW * 2 + 0.01,
             qPrintable(QStringLiteral("wide window width %1 exceeds the row's available width").arg(cells[0].width())));
    QVERIFY2(cells[0].height() < scaleH,
             qPrintable(QStringLiteral("wide window height %1 is not below row height %2").arg(cells[0].height()).arg(scaleH)));
    const QRectF cover0 = coverFor(cells[0], scaleH);
    QCOMPARE(cover0.height(), scaleH);
    QVERIFY(qAbs(cover0.width() - cells[0].width()) < 0.01);
    QVERIFY(cover0.contains(cells[0]));
}

void TestWindowGrid::rowHeightFormulaMatchesRealMachine()
{
    const QVector<QSize> naturals(5, QSize(800, 600));

    qreal scaleH = 0;
    layoutWindowGrid(naturals, QRectF(0, 350, 3840, 1672), 1080, 37, 40, &scaleH);
    QVERIFY2(scaleH > 0 && scaleH <= 720.0,
             qPrintable(QStringLiteral("real-machine row height %1 is outside (0,720]").arg(scaleH)));
    QVERIFY2(qAbs(std::fmod(720.0 - scaleH, 15.0)) < 0.001,
             qPrintable(QStringLiteral("row height %1 is not on the 720-15k step (convergence step changed?)").arg(scaleH)));

    qreal scaleH2 = 0;
    layoutWindowGrid(naturals, QRectF(0, 350, 3840, 1672), 2160, 37, 40, &scaleH2);
    QVERIFY2(scaleH2 > scaleH,
             qPrintable(QStringLiteral("row height did not grow after doubling screenH: %1 -> %2").arg(scaleH).arg(scaleH2)));
}

void TestWindowGrid::coverDerivesFromWindowGeometry()
{
    const QRectF cell(100, 500, 800, 300);
    const QRectF cover = coverFor(cell, 705.0);
    QCOMPARE(cover.height(), 705.0);
    QCOMPARE(cover.width(), cell.width());
    QCOMPARE(cover.center().y(), cell.center().y());
    QCOMPARE(cover.x(), cell.x());
    QCOMPARE(coverFor(QRectF(0, 0, 500, 900), 705.0), QRectF(0, 0, 500, 900));

    qreal rowH = 0;
    const QVector<QRectF> cells = layoutWindowGrid({QSize(800, 300), QSize(600, 1200)},
                                                   QRectF(0, 350, 3840, 1672), 1080, 37, 40, &rowH);
    for (int i = 0; i < cells.size(); ++i) {
        const QRectF derived = coverFor(cells[i], rowH);
        QVERIFY2(qAbs(derived.height() - rowH) < 0.01
                     && qAbs(derived.center().y() - cells[i].center().y()) < 0.01
                     && qAbs(derived.x() - cells[i].x()) < 0.01
                     && qAbs(derived.width() - cells[i].width()) < 0.01,
                 qPrintable(QStringLiteral("cover %1 is not derived from cell %2 / rowH %3")
                                .arg(QStringLiteral("%1,%2 %3x%4").arg(derived.x()).arg(derived.y()).arg(derived.width()).arg(derived.height()))
                                .arg(QStringLiteral("%1,%2 %3x%4").arg(cells[i].x()).arg(cells[i].y()).arg(cells[i].width()).arg(cells[i].height()))
                                .arg(rowH)));
    }
}

void TestWindowGrid::dragGhostScaleFollowsDesign()
{
    const qreal barBottom = 400.0;

    QCOMPARE(dragGhostScale(1000.0, barBottom), 1.0);
    QCOMPARE(dragGhostScale(barBottom + 400.0, barBottom), 1.0);
    QCOMPARE(dragGhostScale(barBottom, barBottom), 0.5);
    QCOMPARE(dragGhostScale(barBottom - 80.0, barBottom), 0.5);
    QCOMPARE(dragGhostScale(50.0, barBottom), 0.5);

    qreal prev = 1.0;
    for (qreal y = barBottom + 140.0; y >= barBottom; y -= 20.0) {
        const qreal s = dragGhostScale(y, barBottom);
        QVERIFY2(s <= prev + 1e-9,
                 qPrintable(QStringLiteral("y=%1 scale=%2 is larger than the farther y=%3 (direction reversed)").arg(y).arg(s).arg(prev)));
        prev = s;
    }
    QVERIFY(qAbs(dragGhostScale(barBottom + 70.0, barBottom) - 0.75) < 0.02);
}

void TestWindowGrid::ghostBasisIsGridCell()
{
    const QRectF cell(1000, 800, 480, 270);

    const QRectF full = dragGhostRect(cell, QPointF(2000, 1200), 1.0);
    QCOMPARE(full.size(), cell.size());
    QCOMPARE(full.center(), QPointF(2000, 1200));

    const QRectF half = dragGhostRect(cell, QPointF(2000, 1200), 0.5);
    QCOMPARE(half.size(), QSizeF(240, 135));
    QCOMPARE(half.center(), QPointF(2000, 1200));

    QCOMPARE(half.size(), QSizeF(cell.width() * 0.5, cell.height() * 0.5));
}

void TestWindowGrid::ghostKeepsAspectWhenEnteringBar()
{
    const QRectF cell(0, 0, 1600, 900);
    const qreal k = dragGhostFitScale(cell, 480 * 0.5, 640 * 0.5);
    const QRectF r = dragGhostRect(cell, QPointF(500, 500), k);
    QVERIFY2(qAbs(r.width() / r.height() - cell.width() / cell.height()) < 1e-6,
             "the aspect ratio must be preserved after entering the bar");
    QVERIFY2(r.width() <= 240.0 + 1e-6 && r.height() <= 320.0 + 1e-6, "must fit inside the item's 1/2 area");
    const qreal k2 = dragGhostFitScale(QRectF(0, 0, 400, 1200), 240, 320);
    const QRectF r2 = dragGhostRect(QRectF(0, 0, 400, 1200), QPointF(0, 0), k2);
    QVERIFY(qAbs(r2.width() / r2.height() - 400.0 / 1200.0) < 1e-6);
}

void TestWindowGrid::coverForGridCellsMatchesRowHeight()
{
    qreal scaleH = 0;
    const QVector<QRectF> cells = layoutWindowGrid({QSize(1000, 800), QSize(600, 1200)},
                                                   QRectF(0, 350, 3840, 1672), 2022, 37, 40, &scaleH);
    QCOMPARE(cells.size(), 2);
    const qreal expSH = 2022.0 * 720.0 / 1080.0;
    const QRectF cover0 = coverFor(cells[0], scaleH);
    QVERIFY2(feq(cover0.height(), expSH), qPrintable(QStringLiteral("short window cover height=%1").arg(cover0.height())));
    QVERIFY2(cover0.center().y() == cells[0].center().y(), "cover shares the vertical center with the window (centered)");
    QVERIFY2(feq(cover0.x(), cells[0].x()) && feq(cover0.width(), cells[0].width()), "cover has the same width as the window");
}

QTEST_MAIN(TestWindowGrid)
#include "tst_windowgrid.moc"
