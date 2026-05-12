// Copyright (C) 2026 Uniontech Technology Co., Ltd.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

class QJsonObject;

namespace KWin
{

enum LaunchType {
    LaunchTypeKeyboard = 1,
    LaunchTypeWindowButton = 2,
    LaunchTypeMouseDrag = 4,
};

class EventLog
{
public:
    EventLog() = delete;

    static void reportMultitaskView();
    static void reportSplitScreen(int launchType, class Window *window);

private:
    static void sendEventLog(const QJsonObject &info);
};

} // namespace KWin
