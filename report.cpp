#include <report.h>
#include <dlfcn.h>
#include <stdio.h>
#include <QProcess>

namespace KWin {
namespace Report {
void init()
{
    INIT_FUNC initFunc = NULL;

    void *handle = dlopen(LIB_CACULATE_PATH, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    *(void **) (&initFunc) = dlsym(handle, "Initialize");
    (*initFunc)("kwin-x11",false);
}

void writeEventLog(const std::string& str)
{
    WRITE_FUNC writeFunc = NULL;

    void *handle = dlopen(LIB_CACULATE_PATH, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    *(void **) (&writeFunc) = dlsym(handle, "WriteEventLog");
    (*writeFunc)(str);
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
