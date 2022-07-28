#ifndef KWIN_REPORT_H
#define KWIN_REPORT_H

#include <iostream>

//函数指针
typedef int (*INIT_FUNC)(std::string, bool);
typedef int (*WRITE_FUNC)(std::string);
#define LIB_CACULATE_PATH "/usr/lib/libdeepin-event-log.so"


namespace KWin {
namespace Report {
void init();
void writeEventLog(const std::string& str);
const std::string version();
}
}

#endif //KWIN_REPORT_H
