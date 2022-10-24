// Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 2015 Martin Gräßlin <mgraesslin@kde.org>
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "screens_virtual.h"
#include "virtual_backend.h"
#include "virtual_output.h"

namespace KWin
{

VirtualScreens::VirtualScreens(VirtualBackend *backend, QObject *parent)
    : OutputScreens(backend, parent)
    , m_backend(backend)
{
}

VirtualScreens::~VirtualScreens() = default;

void VirtualScreens::init()
{
    updateCount();
    KWin::Screens::init();

    connect(m_backend, &VirtualBackend::virtualOutputsSet, this,
        [this] (bool countChanged) {
            if (countChanged) {
                setCount(m_backend->outputs().size());
            } else {
                emit changed();
            }
        }
    );

    emit changed();
}

}
