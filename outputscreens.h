// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
// Copyright 2018 Roman Gilg <subdiff@gmail.com>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_OUTPUTSCREENS_H
#define KWIN_OUTPUTSCREENS_H

#include "screens.h"

namespace KWin
{

/**
 * @brief Implementation for backends with Outputs
 **/
class KWIN_EXPORT OutputScreens : public Screens
{
    Q_OBJECT
public:
    OutputScreens(Platform *platform, QObject *parent = nullptr);
    virtual ~OutputScreens();

    void init() override;
    QString name(int screen) const override;
    bool isInternal(int screen) const;
    QSizeF physicalSize(int screen) const;
    QByteArray uuid(int screen) const;
    QRect geometry(int screen) const override;
    QSize size(int screen) const override;
    qreal scale(int screen) const override;
    float refreshRate(int screen) const override;
    Qt::ScreenOrientation orientation(int screen) const override;
    void updateCount() override;
    int number(const QPoint &pos) const override;

protected:
    Platform *m_platform;
};

}

#endif // KWIN_OUTPUTSCREENS_H
