// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SCREENS_HWCOMPOSER_H
#define KWIN_SCREENS_HWCOMPOSER_H
#include "screens.h"

namespace KWin
{
class HwcomposerBackend;

class HwcomposerScreens : public BasicScreens
{
    Q_OBJECT
public:
    HwcomposerScreens(HwcomposerBackend *backend, QObject *parent = nullptr);
    virtual ~HwcomposerScreens();
    float refreshRate(int screen) const override;
    QSizeF physicalSize(int screen) const override;

private:
    HwcomposerBackend *m_backend;
};

}

#endif
