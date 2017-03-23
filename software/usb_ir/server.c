/****************************************************************************
 ** server.c ****************************************************************
 ****************************************************************************
 *
 * Common code used by all daemon/server/service implementations.
 *
 * Copyright (C) 2009, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */

#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include <argp.h>
#include "version.h"

#include "iguanaIR.h"
#include "compat.h"
#include "support.h"
#include "driver.h"
#include "devicebase.h"
#include "device-interface.h"
#include "server.h"
#include "pipes.h"

/* global variables, internal and shared */
serverSettings srvSettings;
usbId usbIds[] = {
    {0x1781, 0x0938, NULL}, /* iguanaworks USB transceiver */
    END_OF_USB_ID_LIST
};

void triggerCommand(THREAD_PTR cmd)
{
    THREAD_PTR flg = INVALID_THREAD_PTR;
    if (writePipe(srvSettings.commPipe[WRITE], &flg, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR) ||
        writePipe(srvSettings.commPipe[WRITE], &cmd, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
        message(LOG_ERROR, "failed to write flag and command over commPipe: %s\n",
                translateError(errno));
}

void initServerSettings(deviceFunc devFunc)
{
    /* Driver location and preference information.  The preferred list
       is a NULL terminated list of strings. */
    srvSettings.onlyPreferred = false;
    srvSettings.driverDir = NULL,
    srvSettings.preferred = (const char**)malloc(sizeof(char*));
    srvSettings.preferredCount = 0;
    srvSettings.preferred[srvSettings.preferredCount++] = NULL;

    /* timeouts that can be adjusted for different conditions */
#ifdef LIBUSB_NO_THREADS
  #ifdef LIBUSB_NO_THREADS_OPTION
    srvSettings.devSettings.recvTimeout = 1000;
  #else
    srvSettings.devSettings.recvTimeout = 100;
  #endif
#else
    srvSettings.devSettings.recvTimeout = 1000;
#endif
    srvSettings.devSettings.sendTimeout = 1000;

    /* EPIPE usually means device disconnect, but not reliably */
    srvSettings.devSettings.disconnectOnEPipe = false;

    /* an OS-specific function */
    srvSettings.devFunc = devFunc;

    /* default to reading device ids */
    srvSettings.readLabels = true;

    /* default to rescaning when a device is lost */
    srvSettings.autoRescan = true;

    /* default to claiming the hardware devices */
    srvSettings.justDescribe = false;

    /* default to playing nice with other drivers */
    srvSettings.unbind = false;

    /* default to just waiting for hotplug events from an external source */
    srvSettings.scanSeconds = 0;
    srvSettings.scanTimerThread = INVALID_THREAD_PTR;

    /* old flag to handle libusb pre 1.0 threading issues */
#ifdef LIBUSB_NO_THREADS_OPTION
    srvSettings.noThreads = false;
#endif
}

static struct argp_option options[] =
{
    { NULL, 0, NULL, 0, "Logging options:", LOG_GROUP },
    { "log-file",    'l',           "FILE",   0, "Specify a log file (defaults to \"-\").", LOG_GROUP },
    { "quiet",       'q',           NULL,     0, "Reduce the verbosity.",                   LOG_GROUP },
    { "verbose",     'v',           NULL,     0, "Increase the verbosity.",                 LOG_GROUP },
    { "log-level",   ARG_LOG_LEVEL, "NUM",    0, "Set the verbosity directly.",             LOG_GROUP },

    { NULL, 0, NULL, 0, "Driver selection options:", DRV_GROUP },
    { "driver",         'd',             "DRIVER", 0, "Use this driver in preference to others.  This command can be111 used multiple times.", DRV_GROUP },
    { "only-preferred", ARG_ONLY_PREFER, NULL,     0, "Use only drivers specified by the --driver option.",                                 DRV_GROUP },
    { "driver-dir",     ARG_DRIVER_DIR,  "DIR",    0, "Specify the location of driver objects.",                                            DRV_GROUP },

    { NULL, 0, NULL, 0, "Miscellaneous options:", MSC_GROUP },
    { "no-auto-rescan",  ARG_NO_RESCAN,    NULL,     0, "Do not automatically rescan the USB bus after device disconnect.",              MSC_GROUP },
    { "no-ids",          ARG_NO_IDS,       NULL,     0, "Do not query the device for its label.",                                        MSC_GROUP },
    { "no-labels",       ARG_NO_IDS,       NULL,     0, "DEPRECATED: same as --no-ids",                                                  MSC_GROUP },
    { "scan-timer",      ARG_NO_SCANWHEN,  "SECS",   0, "Periodically rescan the USB bus for new devices regardless of hotplug events.", MSC_GROUP },

    /* argp seems to recognize this and move it to the end */
    { "version",         'V',              NULL,     0, "Print the build and version numbers.",                                          MSC_GROUP },

    /* end of table */
    {0}
};

static error_t parseOption(int key, char *arg, struct argp_state *state)
{
//    struct parameters *params = (struct parameters*)state->input;
    switch(key)
    {
    /* Logging options */
    case 'l':
        openLog(arg);
        break;

    case 'q':
        changeLogLevel(-1);
        break;

    case 'v':
        changeLogLevel(+1);
        break;

    case 'V':
        printf("Software version: %s\n", IGUANAIR_VER_STR("igdaemon"));
        exit(0);
        break;

    case ARG_LOG_LEVEL:
    {
        char *end;
        long int res = strtol(arg, &end, 0);
        if (arg[0] == '\0' || end[0] != '\0' || res < LOG_FATAL || res > LOG_DEBUG3 )
        {
            argp_error(state, "Log level requires a numeric argument between %d and %d\n",
                       LOG_FATAL, LOG_DEBUG3);
            return ARGP_HELP_STD_ERR;
        }
        else
            setLogLevel(res);
        break;
    }

    /* driver options */
    case 'd':
        srvSettings.preferred = (const char**)realloc(srvSettings.preferred, sizeof(char*) * (srvSettings.preferredCount + 1));
        srvSettings.preferred[srvSettings.preferredCount - 1] = arg;
        srvSettings.preferred[srvSettings.preferredCount++] = NULL;
        break;

    case ARG_ONLY_PREFER:
        srvSettings.onlyPreferred = true;
        break;

    case ARG_DRIVER_DIR:
        srvSettings.driverDir = arg;
        break;

    /* Miscellaneous options */
    case ARG_NO_IDS:
        srvSettings.readLabels = false;
        break;

    case ARG_NO_RESCAN:
        srvSettings.autoRescan = false;
        break;

    case ARG_NO_SCANWHEN:
    {
        char *end;
        long int res = strtol(arg, &end, 0);
        if (arg[0] == '\0' || end[0] != '\0' || res < 0 || res > 3600 )
        {
            argp_error(state, "Scan timeer requires a numeric argument between 0 and 3600\n");
            return ARGP_HELP_STD_ERR;
        }
        else
            srvSettings.scanSeconds = res;
        break;
    }

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp parser = {
    options,
    parseOption
};

struct argp* baseArgParser()
{
    return &parser;
}

static void* scanTrigger(void *junk)
{
    while(true)
    {
        Sleep(srvSettings.scanSeconds * 1000);
        triggerCommand((THREAD_PTR)SCAN_TRIGGER);
    }
    return NULL;
}

deviceList* initServer()
{
    deviceList *list = NULL;
    int x;
    for(x = 0; usbIds[x].idVendor != INVALID_VENDOR; x++)
        usbIds[x].data = &srvSettings.devSettings;

    /* print a few parameters for the user */
    message(LOG_DEBUG, "Parameters:\n");
    message(LOG_DEBUG,
            "  recvTimeout: %d\n", srvSettings.devSettings.recvTimeout);
    message(LOG_DEBUG,
            "  sendTimeout: %d\n", srvSettings.devSettings.sendTimeout);

    /* start up a thread to trigger periodic rescans if requested */
    if (srvSettings.scanSeconds > 0 && ! startThread(&srvSettings.scanTimerThread, scanTrigger, NULL))
        message(LOG_ERROR, "failed to start a scanning timer.\n");
    /* initialize the commPipe, driver, and device list */
    else if (! createPipePair(srvSettings.commPipe))
    {
#ifdef _WIN32
        message(LOG_ERROR, "failed to open communication pipe, is another igdaemon running?\n");
#else
        message(LOG_ERROR, "failed to open communication pipe.\n");
#endif
    }
    else if (! findDriver(srvSettings.driverDir,
                          srvSettings.preferred, srvSettings.onlyPreferred))
        message(LOG_ERROR, "failed to find a loadable driver layer.\n");
    else if (! initializeDriver())
        message(LOG_ERROR, "failed to initialize the loadable driver layer.\n");
    else if ((list = prepareDeviceList(usbIds, srvSettings.devFunc)) == NULL)
        message(LOG_ERROR, "failed to initialize the device list.\n");
    else
        claimDevices(list, ! srvSettings.justDescribe, srvSettings.unbind);

#if DEBUG
message(LOG_WARN, "OPEN %d %s(%d)\n", srvSettings.commPipe[0],   __FILE__, __LINE__);
message(LOG_WARN, "OPEN %d %s(%d)\n", srvSettings.commPipe[1],   __FILE__, __LINE__);
#endif
    return list;
}

void cleanupServer()
{
    cleanupDriver();
}

void makeParentJoin(THREAD_PTR thread)
{
    if (writePipe(srvSettings.commPipe[WRITE],
                  &thread, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
        message(LOG_ERROR, "Failed to write thread id to parentPipe.\n");
}
