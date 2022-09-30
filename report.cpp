// Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later

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

    if(initFunc) {
        (*initFunc)("kwin",false);
    } else{
        dlclose(m_handle);
        m_handle = nullptr;
    }
}

void eventLog::writeEventLog(TriggerType type, const std::string& mode, const std::string& application)
{
    *(void **) (&m_writeFunc) = dlsym(m_handle, "WriteEventLog");
    if (!m_handle || !m_writeFunc) {
        return;
    }

    std::string id = std::to_string(type);
    std::string json;
    if (mode == "") {
        json = "{\"tid\":" + id + ",\"version\":" + version() + "}";
    } else if(application == "") {
        json = "{\"tid\":" + id + "\"triggerMode\":\"" + mode + "\",\"version\":" + version() + "}";
    } else {
        json = "{\"tid\":" + id + "\"triggerMode\":\"" + mode + "\"triggerApplication\":\"" + application + "\",\"version\":" + version() + "}";
    }

    (*m_writeFunc)(json);
}

const std::string eventLog::version()
{
    if(m_version != "") {
        return m_version;
    }

    QString version;
    QProcess p;
    p.start("dpkg -s kwin-data");
    p.waitForFinished();
    while(!p.atEnd()) {
        QString line = p.readLine();
        if (line.contains("Version")) {
            QStringList strlist = line.split(" ");
            if (strlist.size() > 1) {
                version = strlist.at(1);
                version.remove("\n");
                m_version = version.toStdString();
            }
        }
    }
    return m_version;
}

}
}
