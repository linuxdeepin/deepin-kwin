/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2024 zhang yu <zhangyud@uniontech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "logutils.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(KWIN_FUNC, "kwin_func", QtWarningMsg)
namespace KWin
{

FunctionLog::FunctionLog(QString func)
{
    m_func = func.left(func.indexOf("("));
    qCDebug(KWIN_FUNC) << "<<==" << m_func;
}

FunctionLog::FunctionLog(QString func, uint32_t winId)
    : m_winId(winId)
{
    m_func = func.left(func.indexOf("("));
    qCDebug(KWIN_FUNC) << "<<==" << m_func << " winId=" << winId;
}

FunctionLog::~FunctionLog()
{
    if (m_winId != 0) {
        qCDebug(KWIN_FUNC) << "==>>" << m_func << "winId=" << m_winId;
    } else {
        qCDebug(KWIN_FUNC) << "==>>" << m_func;
    }
}

}