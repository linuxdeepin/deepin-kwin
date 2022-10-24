// Copyright (c) 2003, Karol Szwed <kszwed@kde.org>
// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "geometrytip.h"

namespace KWin
{

GeometryTip::GeometryTip(const Xcb::GeometryHints* xSizeHints):
    QLabel(0)
{
    setObjectName(QLatin1String("kwingeometry"));
    setMargin(1);
    setIndent(0);
    setLineWidth(1);
    setFrameStyle(QFrame::Raised | QFrame::StyledPanel);
    setAlignment(Qt::AlignCenter | Qt::AlignTop);
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    sizeHints = xSizeHints;
}

GeometryTip::~GeometryTip()
{
}

void GeometryTip::setGeometry(const QRect& geom)
{
    int w = geom.width();
    int h = geom.height();

    if (sizeHints) {
        if (sizeHints->hasResizeIncrements()) {
            w = (w - sizeHints->baseSize().width()) / sizeHints->resizeIncrements().width();
            h = (h - sizeHints->baseSize().height()) / sizeHints->resizeIncrements().height();
        }
    }

    h = qMax(h, 0);   // in case of isShade() and PBaseSize
    QString pos;
    pos.sprintf("%+d,%+d<br>(<b>%d&nbsp;x&nbsp;%d</b>)",
                geom.x(), geom.y(), w, h);
    setText(pos);
    adjustSize();
    move(geom.x() + ((geom.width()  - width())  / 2),
         geom.y() + ((geom.height() - height()) / 2));
}

} // namespace

