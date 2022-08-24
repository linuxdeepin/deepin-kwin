#include "report.h"
#include <dlfcn.h>
#include <stdio.h>
#include <QProcess>
#include <QFileInfo>

namespace KWin {
namespace Report {

eventLog* eventLog::instance()
{
    static eventLog m_instance;
    return &m_instance;
}

eventLog::eventLog()
{
    if (QFileInfo::exists(LIB_CACULATE_PATH)) {
        m_handle = dlopen(LIB_CACULATE_PATH, RTLD_LAZY);
    }

    QProcess p;
    p.start("apt policy kwin-data");
    p.waitForFinished();
    QString str = p.readAllStandardOutput();
    QStringList lines = str.split("\n");
    if (lines.size() > 1) {
        if (lines.at(1).split("：").size() > 1) {
            m_version = lines.at(1).split("：").at(1).toStdString();
        } else if (lines.at(1).split(":").size() > 1){
            m_version = lines.at(1).split(":").at(1).toStdString();
        }
    }
}

eventLog::~eventLog()
{
    if (m_handle) {
        dlclose(m_handle);
    }
}

void eventLog::init()
{
    INIT_FUNC initFunc = nullptr;
    if (!m_handle) {
        return;
    }

    *(void **) (&initFunc) = dlsym(m_handle, "Initialize");
    *(void **) (&m_writeFunc) = dlsym(m_handle, "WriteEventLog");
    (*initFunc)("kwin-x11",false);
}

void eventLog::writeEventLog(TriggerType type, const std::string& mode)
{
    if (!m_handle || !m_writeFunc) {
        return;
    }

    std::string id = std::to_string(type);
    std::string json;
    if (!mode.empty() && mode.size() ==0) {
        json = "{\"tid\":" + id + ",\"version\":" + version() + "}";
    } else {
        json = "{\"tid\":" + id + "\"triggerMode\":\"" + mode + "\",\"version\":" + version() + "}";
    }

    (*m_writeFunc)(json);
}

const std::string eventLog::version()
{
    return m_version;
}

}
}
