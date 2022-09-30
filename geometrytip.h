// Copyright (c) 2003, Karol Szwed <kszwed@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_GEOMETRY_TIP_H
#define KWIN_GEOMETRY_TIP_H
#include "xcbutils.h"

#include <QLabel>

namespace KWin
{

class GeometryTip: public QLabel
{
    Q_OBJECT
public:
    GeometryTip(const Xcb::GeometryHints* xSizeHints);
    ~GeometryTip();
    void setGeometry(const QRect& geom);

private:
    const Xcb::GeometryHints* sizeHints;
};

} // namespace

#endif
