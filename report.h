// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef KWIN_REPORT_H
#define KWIN_REPORT_H

#include <iostream>

//函数指针
typedef int (*INIT_FUNC)(std::string, bool);
typedef int (*WRITE_FUNC)(std::string);
#define LIB_CACULATE_PATH "/usr/lib/libdeepin-event-log.so"

namespace KWin {
namespace Report {

enum TriggerType {
    TriggerMutitaskview = 1000300000,        //触发多任务视图
    TriggerAddWorkspace = 1000300001,        //工作区删除
    TriggerMoveWorkspace = 1000300002,       //工作区添加
    TriggerDeleteWorkspace = 1000300003,     //调整工作区顺序
    TriggerSplitScreen = 1000300004,         //触发分屏
    TriggerAllWindow = 1000300005,           //触发 所有窗口
    TriggerWindowLabel = 1000300006,         //触发窗口标题栏菜单功能
};

class eventLog{

    eventLog();

public:
    static eventLog* instance();
    ~eventLog();
    void init();
    void writeEventLog(TriggerType type, const std::string& str = "", const std::string &application = "");
    const std::string version();
private:
    //static eventLog m_instance;
    void* m_handle {nullptr};
    std::string m_version {""};
    WRITE_FUNC m_writeFunc = nullptr;
};


}
}

#endif //KWIN_REPORT_H
