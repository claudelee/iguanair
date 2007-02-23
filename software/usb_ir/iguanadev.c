/****************************************************************************
 ** iguanadev.c *************************************************************
 ****************************************************************************
 *
 * Functions to interface with an Iguanaworks USB device.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
#include "base.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stddef.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
    #include <sys/socket.h>
#endif

#include "pipes.h"
#include "iguanaIR.h"
#include "support.h"
#include "usbclient.h"
#include "dataPackets.h"
#include "protocol.h"

bool readLabels = true;

typedef struct client
{
    itemHeader header;

    /* which iguana device is this associated with? */
    iguanaDev *idev;

    /* for communication with the client */
    PIPE_PTR fd;

    /* whether recv-related messages should be returned to this
     * client */
    bool receiving;
} client;

static bool handleClientRequest(dataPacket *request, client *target)
{
    bool retval = false;
    dataPacket *response = NULL;

    switch(request->code)
    {
    case IG_DEV_RECVON:
        if (target->idev->receiverCount > 0)
            retval = true;
        break;

    case IG_DEV_RECVOFF:
        target->receiving = false;
        if (target->idev->receiverCount > 0)
        {
            target->idev->receiverCount--;
            if (target->idev->receiverCount > 0)
                retval = true;
        }
        break;

    case IG_DEV_SEND:
    {
        unsigned char *codes;
        request->dataLen /= sizeof(uint32_t);
        codes = pulsesToIguanaSend((uint32_t*)request->data,
                                   &request->dataLen);
        free(request->data);
        request->data = codes;
        break;
    }
    }

    if (retval)
        message(LOG_INFO, "Device transaction skipped: %d\n", request->code);
    else if (! deviceTransaction(target->idev, request, &response))
    {
        if (request->code == IG_DEV_RESET)
        {
            if (usb_reset(getDevHandle(target->idev->usbDev)) != 0)
                message(LOG_ERROR, "Hard reset failed\n");
            else
                retval = true;
        }
        else
            message(LOG_ERROR,
                    "Device transaction (0x%2.2x) failed\n", request->code);
    }
    else
    {
        /* need to know which clients to contact on a receive */
        if (request->code == IG_DEV_RECVON)
        {
            target->idev->receiverCount++;
            target->receiving = true;
        }

        if (response != NULL)
        {
            /* pack data for the client */
            free(request->data);
            request->data = response->data;
            request->dataLen = response->dataLen;
            free(response);
        }

        retval = true;
    }

    return retval;
}

static void releaseClient(client *target, fdSets *fds)
{
    FD_CLR(target->fd, &fds->next);
    closePipe(target->fd);

    if (target->receiving)
    {
        target->idev->receiverCount--;
        if (target->idev->receiverCount == 0)
        {
            dataPacket request = DATA_PACKET_INIT;
            message(LOG_INFO,
                    "No more receivers, turning off the receiver.\n");

            request.code = IG_DEV_RECVOFF;
            if (! deviceTransaction(target->idev, &request, NULL))
                message(LOG_ERROR, "Failed to disable the receiver.\n");
        }
    }

    free(target);
}

static bool handleFD(itemHeader *item, void *userData)
{
    bool retval = true;
    dataPacket request;
    client *me;
    fdSets *fds;

    /* properly cast the arguments */
    me = (client*)item;
    fds = (fdSets*)userData;

    /* check for error, then output */
    switch(checkFD(me->fd, fds))
    {
    case -1:
        message(LOG_ERROR, "Error on client fd: %d\n", me->fd);
        break;

    case 0:
        break;

    case 1:
        if (! readDataPacket(&request, me->fd,
                             me->idev->usbDev->list->recvTimeout))
        {
            releaseClient(me, fds);
            retval = false;
        }
        else
        {
            if (! handleClientRequest(&request, me))
            {
                request.code = IG_DEV_ERROR;
                request.dataLen = -errno;
                message(LOG_ERROR,
                        "handleClientRequest(0x%2.2x) failed with: %s\n",
                        request.code, strerror(errno));
            }

            if (! writeDataPacket(&request, me->fd))
            {
                message(LOG_INFO, "FAILED to write packet back to client.\n");
                releaseClient(me, fds);
                retval = false;
            }
            else
            {
                dataPacket *packet = &request;
                message(LOG_DEBUG3, "Successfully wrote packet: ");
                appendHex(LOG_DEBUG3, (char*)packet + offsetof(dataPacket, code),
                          offsetof(dataPacket, data) - offsetof(dataPacket, code));
                if (packet->dataLen > 0)
                    appendHex(LOG_DEBUG3, packet->data, packet->dataLen);
            }

            free(request.data);
        }
        break;
    }

    return retval;
}

static bool checkVersion(iguanaDev *idev)
{
    dataPacket request = DATA_PACKET_INIT, *response = NULL;
    bool retval = false;

    request.code = IG_DEV_GETVERSION;
    if (! deviceTransaction(idev, &request, &response))
        message(LOG_ERROR, "Failed to get version.\n");
    else
    {
        if (response->dataLen != 2)
            message(LOG_ERROR, "Incorrectly sized version response.\n");
        else
        {
            idev->version = (response->data[1] << 8) + response->data[0];

            message(LOG_INFO, "Found device version %d\n", idev->version);
            /* ensure we have an acceptable version */
            if (idev->version > MAX_VERSION)
                message(LOG_ERROR,
                        "Unsupported hardware version %d\n", idev->version);
            else
                retval = true;
        }

        freeDataPacket(response);
    }

    return retval;
}

static char* getID(iguanaDev *idev, char *name)
{
    dataPacket request = DATA_PACKET_INIT, *response = NULL;
    char *retval = NULL;

    request.code = IG_DEV_GETID;
    if (! deviceTransaction(idev, &request, &response))
        message(LOG_ERROR, "Failed to get id.\n");
    else
    {
        retval = (char*)iguanaRemoveData(response, NULL);
        freeDataPacket(response);
    }

    return retval;
}

static void joinWithReader(iguanaDev *idev)
{
    void *exitVal;

    /* signal then join with the reader */
    idev->quitRequested = true;
    joinThread(idev->reader, &exitVal);
}

static bool acceptNewClients(PIPE_PTR listener, fdSets *fds,
                             listHeader *clientList, iguanaDev *idev)
{
    bool retval = false;
    client *newClient;

    switch(checkFD(listener, fds))
    {
    /* quit on listener errors */
    case -1:
        message(LOG_ERROR, "error on client listener socket.\n");
        break;

    case 0:
        retval = true;
        break;

    /* add new clients to the list */
    case 1:
        newClient = (client*)malloc(sizeof(client));
        if (newClient == NULL)
            message(LOG_ERROR,
                    "Out of memory allocating client struct.");
        else
        {
            newClient->idev = idev;
            newClient->receiving = false;
            newClient->fd = acceptClient(listener);
            if (newClient->fd == INVALID_PIPE)
                message(LOG_ERROR, "Error accepting client: %s\n",
                        strerror(errno));
            else
            {
                insertItem(clientList, NULL, (itemHeader*)newClient);
                retval = true;
            }
        }
        break;
    }

    return retval;
}

static bool tellReceivers(itemHeader *item, void *userData)
{
    client *me = (client*)item;

    if (me->receiving)
    {
        if (! writeDataPacket((dataPacket*)userData, me->fd))
            message(LOG_ERROR, "Failed to send packet to receiver.\n");
        else
        {
            dataPacket *packet = (dataPacket*)userData;

            message(LOG_DEBUG3, "Sent receivers: ");
            appendHex(LOG_DEBUG3, (char*)packet + offsetof(dataPacket, code),
                      offsetof(dataPacket, data) - offsetof(dataPacket, code));
            if (packet->dataLen > 0)
                appendHex(LOG_DEBUG3, packet->data, packet->dataLen);
        }
    }

    return true;
}

static bool handleReader(iguanaDev *idev, fdSets *fds, listHeader *clientList)
{
    bool retval = false;
    char byte;

    switch(checkFD(idev->readPipe[READ], fds))
    {
    case -1:
        break;

    case 0:
        retval = true;
        break;

    case 1:
        switch(readPipe(idev->readPipe[READ], &byte, 1))
        {
        case -1:
            message(LOG_ERROR, "Error reading from readPipe.\n");
            break;

        /* check for removal of usb device (or shutdown) */
        case 0:
            break;

        case 1:
        {
            dataPacket *packet;

            packet = removeNextPacket(idev);
            switch(packet->code)
            {
            case IG_DEV_RECV:
            {
                uint32_t *pulses;
                pulses = iguanaDevToPulses(packet->data, &packet->dataLen);
                free(packet->data);
                packet->data = (unsigned char*)pulses;

                forEach(clientList, tellReceivers, packet);
                break;
            }

            case IG_DEV_BIGRECV:
                message(LOG_ERROR, "Receive too large from USB device.\n");
                forEach(clientList, tellReceivers, packet);
                break;

            case IG_DEV_BIGSEND:
                message(LOG_ERROR, "Send too large for USB device.\n");
                break;

            default:
                message(LOG_ERROR,
                        "Unexpected code (0x%x) with %d data bytes from usb: ",
                        packet->code, packet->dataLen);
                if (packet->dataLen != 0)
                    appendHex(LOG_ERROR, packet->data, packet->dataLen);
            }

            freeDataPacket(packet);
            retval = true;
            break;
        }
        }
    }

    return retval;
}

static void* workLoop(void *instance)
{
    iguanaDev *idev = (iguanaDev*)instance;

#ifndef WIN32
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        message(LOG_ERROR, "Failed when setting SIG_IGN for SIGPIPE.\n");
#endif

    message(LOG_INFO, "Worker %d starting\n", idev->usbDev->id);
    if (checkVersion(idev))
    {
        PIPE_PTR listener;
        char name[4], *alias = NULL;

        snprintf(name, 4, "%d", idev->usbDev->id);
        if (readLabels)
            alias = getID(idev, name);
        listener = startListening(name, alias);
        if (listener != INVALID_PIPE)
        {
            bool done = false;
            listHeader clientList;
            fdSets fds;

            initializeList(&clientList);

            FD_ZERO(&fds.in);
            FD_ZERO(&fds.err);
            while(! done)
            {
                /* prepare the basics for the next select */
                FD_ZERO(&fds.next);
                fds.max = -1;
                checkFD(listener, &fds);
                checkFD(idev->readPipe[READ], &fds);

                /* service each client and add them to the fds */
                forEach(&clientList, handleFD, &fds);

                /* wait until there is data ready */
                done = true;
                fds.err = fds.in = fds.next;
                if (select(fds.max + 1, &fds.in, NULL, &fds.err, NULL) < 0)
                    message(LOG_ERROR, "select failed: %s\n", strerror(errno));
                /* watch for incoming clients and packets from usb */
                else if (acceptNewClients(listener, &fds, &clientList, idev) &&
                         handleReader(idev, &fds, &clientList))
                    done = false;
            }
            stopListening(listener, name, alias);
        }
    }

    /* release resources for reader and usb device */
    joinWithReader(idev);
    releaseDevice(idev->usbDev);

    message(LOG_INFO, "Worker %d exiting\n", idev->usbDev->id);
    if (writePipe(idev->usbDev->list->childPipe[WRITE],
                  &idev->worker, sizeof(THREAD_PTR)) != sizeof(THREAD_PTR))
        message(LOG_ERROR, "Failed to write thread id to childPipe.\n");

    /* now actually free the malloc'd data */
    free(idev->usbDev);
    free(idev);

    return NULL;
}

void startWorker(usbDevice *dev)
{
    iguanaDev *idev = NULL;

    idev = (iguanaDev*)malloc(sizeof(iguanaDev));
    if (idev == NULL)
        message(LOG_ERROR,
                "Out of memory allocating iguanaDev for %d\n, dev->id");
    else
    {
        memset(idev, 0, sizeof(iguanaDev));
        InitializeCriticalSection(&idev->listLock);
        if (createPipePair(idev->readPipe))
            message(LOG_ERROR, "Failed to create readPipe for %d\n", dev->id);
        else if (createPipePair(idev->responsePipe))
            message(LOG_ERROR,
                    "Failed to create responsePipe for %d\n", dev->id);
#ifdef LIBUSB_NO_THREADS
        else if (pthread_mutex_init(&idev->devLock, NULL) != 0)
            message(LOG_ERROR, "Failed to initialize devLock\n");
#endif
        else
        {
            /* this must be set before the call to findDeviceEndpoints */
            idev->usbDev = dev;

            if (! findDeviceEndpoints(idev))
                message(LOG_ERROR,
                        "Failed find device endpoints for %d\n", dev->id);
            else if (startThread(&idev->reader,
                                 (void *(*)(void*))handleIncomingPackets,
                                 idev))
                message(LOG_ERROR,
                        "Failed to create reader thread %d\n", dev->id);
            else
            {
                if (startThread(&idev->worker, workLoop, idev) != 0)
                    message(LOG_ERROR,
                            "Failed to create worker thread %d\n", dev->id);
                else
                    /* return on success to skip cleanup */
                    return;

                joinWithReader(idev);
            }
        }
        free(idev);
    }
    releaseDevice(dev);
}

bool reapAllChildren(usbDeviceList *list)
{
    unsigned int x;
    /* stop all the readers and thereby the workers */
    x = releaseDevices(list);

    for(; x > 0; x--)
    {
        void *exitval;
        int result;
        THREAD_PTR child;

        /* NOTE: using 2*recv timeout to allow readers to exit. */
        result = readBytes(list->childPipe[READ], 2 * list->recvTimeout,
                           (char*)&child, sizeof(THREAD_PTR));
        /* no one ready */
        if (result == 0)
            break;
        /* try to join with the worker thread */
        else if (result != sizeof(THREAD_PTR) ||
                 joinThread(child, &exitval) != 0)
        {
            message(LOG_ERROR, "failed while reaping worker thread.\n");
            return false;
        }
        else
            message(LOG_DEBUG, "Reaped child: %p\n", child);
    }

    return true;
}