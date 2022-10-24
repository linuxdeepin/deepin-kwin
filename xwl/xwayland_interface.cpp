// Copyright 2019 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 2019 Roman Gilg <subdiff@gmail.com>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xwayland_interface.h"

namespace KWin
{

XwaylandInterface *s_self = nullptr;

XwaylandInterface *XwaylandInterface::self()
{
    return s_self;
}

XwaylandInterface::XwaylandInterface(QObject *parent)
    : QObject(parent)
{
    s_self = this;
}

XwaylandInterface::~XwaylandInterface()
{
    s_self = nullptr;
}

}
