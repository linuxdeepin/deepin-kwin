/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#define _GNU_SOURCE 1

#include "realtime.h"
#include "common.h" // for KWIN_CORE logging category

#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace KWin
{

void gainRealTime()
{
    char threadName[16] = {0};
    pthread_getname_np(pthread_self(), threadName, sizeof(threadName));
    const long tid = syscall(SYS_gettid);

#if HAVE_SCHED_RESET_ON_FORK
    const int minPriority = sched_get_priority_min(SCHED_RR);
    sched_param sp;
    sp.sched_priority = minPriority;
    const int result = pthread_setschedparam(pthread_self(), SCHED_RR | SCHED_RESET_ON_FORK, &sp);
    if (result == 0) {
        qCInfo(KWIN_CORE) << "Gained realtime scheduling (SCHED_RR, priority" << minPriority << ") thread name:" << threadName << "tid:" << tid;
    } else {
        qCWarning(KWIN_CORE) << "Failed to set realtime scheduling: error" << result << strerror(result) << "thread name:" << threadName << "tid:" << tid;
    }
#else
    qCDebug(KWIN_CORE) << "Realtime scheduling disabled at compile time (HAVE_SCHED_RESET_ON_FORK = 0) thread name:" << threadName << "tid:" << tid;
#endif
}

} // namespace KWin
