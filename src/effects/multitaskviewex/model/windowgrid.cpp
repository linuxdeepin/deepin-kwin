/*
    SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "windowgrid.h"

#include <QDebug>
#include <QtMath>

namespace KWin
{

QVector<QRectF> layoutWindowGrid(const QVector<QSize> &naturals,
                                 const QRectF &area,
                                 qreal screenH,
                                 qreal spacingH,
                                 qreal spacingW,
                                 qreal *outScaleHeight)
{
    QVector<QRectF> out;
    const int n = naturals.size();
    out.resize(n);
    if (n == 0) {
        return out;
    }

    const qreal areaH = area.height();
    const qreal areaW = area.width();

    qreal scaleHeight = screenH * (720.0 / 1080.0);
    int rows = 1;
    int guard = 0;
    bool overlap = true;
    QVector<qreal> centerList;
    while (overlap && guard++ < 2048) {
        overlap = false;
        centerList.clear();
        qreal totalw = spacingW;
        int idx = 1;
        bool rowOverflow = false;
        for (int i = n - 1; i >= 0; --i) {
            const qreal width = fitWindowIntoRow(naturals[i], scaleHeight, areaW - spacingW * 2).width();
            totalw += width + spacingW;
            if (width + spacingW * 2 > areaW) {
                rowOverflow = true;
                break;
            }
            if (totalw > areaW) {
                idx++;
                if (idx > rows) {
                    rowOverflow = true;
                    break;
                }
                centerList.append((areaW - totalw + width + spacingW) / 2 + spacingW);
                totalw = spacingW + width + spacingW;
            }
        }
        centerList.append((areaW - totalw) / 2 + spacingW);
        if (totalw > areaW || rowOverflow) {
            overlap = true;
            scaleHeight -= 15;
            const qreal critical = (areaH - (rows + 2) * spacingH) / (rows + 1);
            if (scaleHeight <= critical) {
                rows++;
            }
        }
    }
    scaleHeight = qMax<qreal>(scaleHeight, 1.0);
    const int rowsUsed = centerList.size();
    if (outScaleHeight) {
        *outScaleHeight = scaleHeight;
    }

    qreal winYPos = (areaH - (rowsUsed - 1) * spacingH - rowsUsed * scaleHeight) / 2 + area.y();
    int row = 0;
    qreal x = centerList.value(0, 0);
    qreal totalw = spacingW;
    for (int i = n - 1; i >= 0; --i) {
        const QSizeF fitted = fitWindowIntoRow(naturals[i], scaleHeight, areaW - spacingW * 2);
        const qreal width = fitted.width();
        const qreal height = fitted.height();
        totalw += width + spacingW;
        if (totalw > areaW) {
            row++;
            totalw = spacingW + width + spacingW;
            x = centerList.value(row, x);
            winYPos += spacingH + scaleHeight;
        }
        const qreal y = winYPos + (scaleHeight - height) / 2;
        out[i] = QRectF(area.x() + x, y, width, height);
        x += width + spacingW;
    }
    return out;
}

}
