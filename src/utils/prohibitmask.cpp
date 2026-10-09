/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "prohibitmask.h"

#include "main.h"

#include <QDBusConnection>
#include <QDBusMessage>

namespace KWin
{

// 引用计数：记录 KWin 当前打开着的、由 KWin 负责关闭的全局保护数量
static int s_protectionCount = 0;

// 同步调用 displayjack 的设置全局保护方法，严格防止窗口内容泄露
// TODO: 同步调用会阻塞 kwin，后续可能需要重新设计防截屏功能实现
void dbusRootWindowProtect(bool protect)
{
    if (kwinApp()->operationMode() != Application::OperationModeX11) {
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("com.deepin.prohibitscreenshot"),
        QStringLiteral("/com/deepin/prohibitscreenshot"),
        QStringLiteral("deepin.prohibitscreenshot.interface"),
        QStringLiteral("setProhibited"));
    msg << protect;

    if (protect) {
        QDBusConnection::sessionBus().call(msg, QDBus::Block, 1000);
        s_protectionCount++;
    } else if (s_protectionCount > 0) {
        s_protectionCount--;
        if (s_protectionCount == 0) {
            QDBusConnection::sessionBus().call(msg, QDBus::Block, 1000);
        }
    }
}

bool protectionOpenedByKWin()
{
    return s_protectionCount > 0;
}

} // namespace KWin
