/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */
#include "decorationsettings.h"
#include "private/decorationbridge.h"
#include "private/decorationsettingsprivate.h"

#include <QFontMetrics>

namespace KDecoration2
{
DecorationSettings::DecorationSettings(DecorationBridge *bridge, QObject *parent)
    : QObject(parent)
    , d(bridge->settings(this))
{
    auto updateUnits = [this] {
        int gridUnit = QFontMetrics(font()).boundingRect(QLatin1Char('M')).height();
        ;
        if (gridUnit % 2 != 0) {
            gridUnit++;
        }
        if (gridUnit != d->gridUnit()) {
            d->setGridUnit(gridUnit);
            Q_EMIT gridUnitChanged(gridUnit);
        }
        if (gridUnit != d->largeSpacing()) {
            d->setSmallSpacing(qMax(2, (int)(gridUnit / 4))); // 1/4 of gridUnit, at least 2
            d->setLargeSpacing(gridUnit); // msize.height
            Q_EMIT spacingChanged();
        }
    };
    updateUnits();
    connect(this, &DecorationSettings::fontChanged, this, updateUnits);
}

DecorationSettings::~DecorationSettings() = default;

#define DELEGATE(type, method)              \
    type DecorationSettings::method() const \
    {                                       \
        return d->method();                 \
    }

DELEGATE(bool, isOnAllDesktopsAvailable)
DELEGATE(bool, isAlphaChannelSupported)
DELEGATE(bool, isCloseOnDoubleClickOnMenu)
DELEGATE(QVector<DecorationButtonType>, decorationButtonsLeft)
DELEGATE(QVector<DecorationButtonType>, decorationButtonsRight)
DELEGATE(BorderSize, borderSize)
DELEGATE(QFont, font)
DELEGATE(QFontMetricsF, fontMetrics)
DELEGATE(int, gridUnit)
DELEGATE(int, smallSpacing)
DELEGATE(int, largeSpacing)

#undef DELEGATE

}
