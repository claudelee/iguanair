/****************************************************************************
 ** support.h ***************************************************************
 ****************************************************************************
 *
 * Basic supporting functions needed by the Iguanaworks tools.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#ifndef _SUPPORT_H_
#define _SUPPORT_H_

enum
{
    /* message levels */
    LOG_FATAL,
    LOG_ERROR,
    LOG_WARN,
    LOG_NORMAL,
    LOG_INFO,
    LOG_DEBUG,
    LOG_DEBUG2,
    LOG_DEBUG3,

    LOG_ALWAYS,

    /* other constants */
    MAX_LINE = 1024,
    CTL_INDEX = 0xFF,

    /* for use with readPipe */
    READ  = 0,
    WRITE = 1
};

/* functions for messages (logging) */
void dieCleanly(int level);
IGUANAIR_API void changeLogLevel(int difference);
IGUANAIR_API void openLog(const char *filename);
IGUANAIR_API bool wouldOutput(int level);
IGUANAIR_API int message(int level, char *format, ...);
IGUANAIR_API void appendHex(int level, void *location, unsigned int length);

/* used during shutdown to clean up threads */
void setParentPipe(PIPE_PTR pp);
IGUANAIR_API void makeParentJoin();

#endif
