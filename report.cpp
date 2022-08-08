#include <report.h>
#include <dlfcn.h>
#include <stdio.h>
#include <QProcess>

namespace KWin {
namespace Report {
void init()
{
    INIT_FUNC initFunc = nullptr;

    void *handle = dlopen(LIB_CACULATE_PATH, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return;
        //exit(EXIT_FAILURE);
    }

    *(void **) (&initFunc) = dlsym(handle, "Initialize");
    (*initFunc)("kwin-x11",false);
    dlclose(handle);
}

void writeEventLog(TriggerType type, const std::string& mode)
{
    std::string id = std::to_string(type);
    std::string json;
    if(!mode.empty() && mode.size() ==0) {
        json = "{\"tid\":" + id + ",\"version\":" + version() + "}";
    } else {
        json = "{\"tid\":" + id + "\"triggerMode\":\"" + mode + "\",\"version\":" + version() + "}";
    }
    WRITE_FUNC writeFunc = nullptr;

    void *handle = dlopen(LIB_CACULATE_PATH, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return;
        //exit(EXIT_FAILURE);
    }

    *(void **) (&writeFunc) = dlsym(handle, "WriteEventLog");
    (*writeFunc)(json);
    dlclose(handle);
}

const std::string version()
{
    QProcess p;
    p.start("apt policy kwin-data");
    p.waitForFinished();
    QString str = p.readAllStandardOutput();
    QString ret = str.split("\n").at(1).split("：").at(1);
    return ret.toStdString();
}

}
}
