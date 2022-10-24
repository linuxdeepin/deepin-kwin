// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_QPA_SCREEN_H
#define KWIN_QPA_SCREEN_H

#include <qpa/qplatformscreen.h>
#include <QScopedPointer>

namespace KWin
{
namespace QPA
{
class Integration;
class PlatformCursor;

class Screen : public QPlatformScreen
{
public:
    explicit Screen(int screen, Integration *integration);
    virtual ~Screen();

    QList<QPlatformScreen *> virtualSiblings() const override;
    QRect geometry() const override;
    int depth() const override;
    QImage::Format format() const override;
    QSizeF physicalSize() const override;
    QPlatformCursor *cursor() const override;
    QDpi logicalDpi() const override;
    qreal devicePixelRatio() const override;

private:
    int m_screen;
    Integration *m_integration;
    QScopedPointer<PlatformCursor> m_cursor;
};

}
}

#endif
