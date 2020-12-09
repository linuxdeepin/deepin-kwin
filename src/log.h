/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2020 jccKevein <luochaojiang@uniontech.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef D_LOG_H_
#define D_LOG_H_

#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------

/*
 * Normally we strip DLOGD (LOG_DEBUG messages) from release builds.
 * You can modify this (for example with "#define LOG_NDEBUG 0"
 * at the top of your source file) to change that behavior.
 */
#ifndef LOG_NDEBUG
    #ifdef NDEBUG
        #define LOG_NDEBUG 1
    #else
        #define LOG_NDEBUG 0
    #endif
#endif

/*
 * This is the local tag used for the following simplified
 * logging macros.  You can change this preprocessor definition
 * before using the other macros to change the tag.
 */
#ifndef LOG_TAG
#define LOG_TAG "UT-Kwin"
#endif

// ---------------------------------------------------------------------

#define CONDITION(cond)     (__builtin_expect((cond)!=0, 0))

/*
 * Simplified macro to send a debug log message using the current LOG_TAG.
 */
#ifndef DLOGD
#if LOG_NDEBUG
#define DLOGD(format, ...)   ((void)0)
#else
#define DLOGD(format, ...) ((void)DLOG(LOG_DEBUG, LOG_TAG, format, ## __VA_ARGS__))
#endif
#endif

#ifndef DLOGD_IF
#if LOG_NDEBUG
#define DLOGD_IF(cond, format, ...)   ((void)0)
#else
#define DLOGD_IF(cond, format, ...) \
    ( (CONDITION(cond)) \
    ? ((void)DLOG(LOG_DEBUG, LOG_TAG, format, ## __VA_ARGS__)) \
    : (void)0 )
#endif
#endif

/*
 * Simplified macro to send an info log message using the current LOG_TAG.
 */
#ifndef DLOGI
#define DLOGI(format, ...) ((void)DLOG(LOG_INFO, LOG_TAG, format, ## __VA_ARGS__))
#endif

#ifndef DLOGI_IF
#define DLOGI_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)DLOG(LOG_INFO, LOG_TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an notice log message using the current LOG_TAG.
 */
#ifndef DLOGN
#define DLOGN(format, ...) ((void)DLOG(LOG_NOTICE, LOG_TAG, format, ## __VA_ARGS__))
#endif

#ifndef DLOGN_IF
#define DLOGN_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)DLOG(LOG_NOTICE, LOG_TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send a warning log message using the current LOG_TAG.
 */
#ifndef DLOGW
#define DLOGW(format, ...) ((void)DLOG(LOG_WARNING, LOG_TAG, format, ## __VA_ARGS__))
#endif

#ifndef DLOGW_IF
#define DLOGW_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)DLOG(LOG_WARNING, LOG_TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an error log message using the current LOG_TAG.
 */
#ifndef DLOGE
#define DLOGE(format, ...) ((void)DLOG(LOG_ERR, LOG_TAG, format, ## __VA_ARGS__))
#endif

#ifndef DLOGE_IF
#define DLOGE_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)DLOG(LOG_ERR, LOG_TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an critical log message using the current LOG_TAG.
 */
#ifndef DLOGC
#define DLOGC(format, ...) ((void)DLOG(LOG_CRIT, LOG_TAG, format, ## __VA_ARGS__))
#endif

#ifndef DLOGC_IF
#define DLOGC_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)DLOG(LOG_CRIT, LOG_TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an alert log message using the current LOG_TAG.
 */
#ifndef DLOGA
#define DLOGA(format, ...) ((void)DLOG(LOG_ALERT, LOG_TAG, format, ## __VA_ARGS__))
#endif

#ifndef DLOGA_IF
#define DLOGA_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)DLOG(LOG_ALERT, LOG_TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an emerg log message using the current LOG_TAG.
 */
#ifndef DLOGEE
#define DLOGEE(format, ...) ((void)DLOG(LOG_EMERG, LOG_TAG, format, ## __VA_ARGS__))
#endif

#ifndef DLOGEE_IF
#define DLOGEE_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)DLOG(LOG_EMERG, LOG_TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

// ---------------------------------------------------------------------

/*
 * Basic log message macro.
 *
 * Example:
 *  DLOG(LOG_WARNING, NULL, "Failed with error %d", errno);
 *
 * The second argument may be NULL or "" to indicate the "global" tag.
 */
#ifndef DLOG
#define DLOG(priority, tag, format, ...) \
    SLOG_PRI(priority, tag, format, ## __VA_ARGS__)
#endif

/*
 * Log syslog macro that allows you to specify a number for the priority.
 */
#ifndef SLOG_PRI
#define SLOG_PRI(priority, tag, format, ...) \
    syslog(priority, "%s|%s:%d " format "\n", tag,  __FUNCTION__, __LINE__, ## __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* D_LOG_H_ */
