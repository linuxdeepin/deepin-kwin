// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_SCREENS_VIRTUAL_H
#define KWIN_SCREENS_VIRTUAL_H
#include "outputscreens.h"
#include <QVector>

namespace KWin
{
class VirtualBackend;

class VirtualScreens : public OutputScreens
{
    Q_OBJECT
public:
    VirtualScreens(VirtualBackend *backend, QObject *parent = nullptr);
    virtual ~VirtualScreens();
    void init() override;

private:
    void createOutputs();
    VirtualBackend *m_backend;
};

}

#endif

