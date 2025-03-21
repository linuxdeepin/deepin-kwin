/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QDebug>
#include <QObject>

namespace KWin
{
class FunctionLog : public QObject
{
public:
    FunctionLog(QString func);
    FunctionLog(QString func, uint32_t winId);
    ~FunctionLog();

private:
    QString     m_func;
    uint32_t    m_winId = 0;
};

#define FUNC_DEBUG_LOG(name, ...) \
        FunctionLog d(name, ##__VA_ARGS__)
}
