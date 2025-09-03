/*
    SPDX-FileCopyrightText: 2021 Tobias C. Berner <tcberner@FreeBSD.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "executable_path.h"

#include <QFileInfo>
#include <QTextStream>

QString executablePathFromPid(pid_t pid)
{
    return QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
}

QString executableCommFromPid(pid_t pid)
{
    QString commFile = QStringLiteral("/proc/%1/comm").arg(pid);
    QFile file(commFile);
    QString processName;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        processName = in.readLine().trimmed();
        file.close();
    }
    return processName;
}
