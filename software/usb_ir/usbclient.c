/****************************************************************************
 ** usbclient.c *************************************************************
 ****************************************************************************
 *
 * Lowest level interface to the USB devices.  
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
#include <usb.h>
#include <errno.h>

#include "pipes.h"
#include "support.h"
#include "usbclient.h"

static void setError(usbDevice *handle, char *error)
{
    if (handle != NULL)
    {
        /* clear error codes */
        handle->error = error;
        if (error != NULL)
            handle->usbError = usb_strerror();
    }
}

void printError(int level, char *msg, usbDevice *handle)
{
    if (msg != NULL)
        if (handle == NULL || handle->error == NULL)
            message(level, "%s\n", msg);
        else if (handle->usbError == NULL)
            message(level, "%s: %s\n", msg, handle->error);
        else
            message(level,
                    "%s: %s: %s\n", msg, handle->error, handle->usbError);
    else if (handle != NULL && handle->error != NULL)
        if (handle->usbError == NULL)
            message(level, "%s\n", handle->error);
        else
            message(level, "%s: %s\n", handle->error, handle->usbError);
    else
        message(level, "No error recorded\n");
}

int interruptRecv(usbDevice *handle, void *buffer, int bufSize)
{
    int retval;

    retval = usb_interrupt_read(handle->device,
                                handle->epIn->bEndpointAddress,
                                buffer, bufSize,
                                handle->list->recvTimeout);
    if (retval < 0)
        setError(handle, "Failed to read (interrupt end point)");
    else
    {
        message(LOG_DEBUG2, "i");
        appendHex(LOG_DEBUG2, buffer, retval);
    }

    return retval;
}

int interruptSend(usbDevice *handle, void *buffer, int bufSize)
{
    int retval;

    message(LOG_DEBUG2, "o");
    appendHex(LOG_DEBUG2, buffer, bufSize);
    retval = usb_interrupt_write(handle->device,
                                 handle->epOut->bEndpointAddress,
                                 buffer, bufSize,
                                 handle->list->sendTimeout);
    if (retval < 0)
        setError(handle, "Failed to write (interrupt end point)");

    return retval;
}

void releaseDevice(usbDevice *handle)
{
    if (handle != NULL &&
        ! handle->removed)
    {
        setError(handle, NULL);

        /* close the usb interface and handle */
        if (usb_release_interface(handle->device, 0) < 0 && errno != ENODEV)
            setError(handle, "Failed to release interface");
        else if (usb_close(handle->device) < 0)
            setError(handle, "Failed to close device");

        /* print errors from the usb closes */
        if (handle->error != NULL)
            printError(LOG_ERROR, NULL, handle);

        /* remove the device from the list */
        removeItem((itemHeader*)handle);

        /* record the removal */
        handle->removed = true;
        handle->list->count--;
    }
}

bool initDeviceList(usbDeviceList *list, usbId *ids,
                    unsigned int recvTimeout, unsigned int sendTimeout,
                    deviceFunc ndf)
{
    bool retval = false;

    memset(list, 0, sizeof(usbDeviceList));
    if (createPipePair(list->childPipe) == 0)
    {
        list->ids = ids;
        list->recvTimeout = recvTimeout;
        list->sendTimeout = sendTimeout;
        list->newDev = ndf;
        retval = true;
    }

    return retval;
}

/* increment the id for each item in the list */
static bool findId(itemHeader *item, void *userData)
{
    int *id = (int*)userData;
    if (((usbDevice*)item)->id == *id)
        (*id)++;
    return true;
}

bool updateDeviceList(usbDeviceList *list)
{
    struct usb_bus *bus;
    struct usb_device *dev;
    usbDevice *devPos;
    unsigned int pos, count = 0;

    devPos = (usbDevice*)firstItem(&list->deviceList);
    setError(devPos, NULL);

    /* initialize usb */
    usb_init();

    /* the next two return counts of busses and devices respectively */
    usb_find_busses();
    usb_find_devices();

    /* search for the first device we find */
    for (bus = usb_busses; bus; bus = bus->next)
        for (dev = bus->devices; dev; dev = dev->next)
            for(pos = 0; list->ids[pos].idVendor != INVALID_VENDOR; pos++)
                /* continue if we are not examining the correct device */
                if (dev->descriptor.idVendor  == list->ids[pos].idVendor &&
                    dev->descriptor.idProduct == list->ids[pos].idProduct)
                {
                    int busIndex;
                    count++;
                    
                    /* couldn't find the bus index as a number anywhere */
                    busIndex = atoi(bus->dirname);

                    /* found a device instance, now find position in
                     * current list */
                    while(devPos != NULL &&
                          (devPos->busIndex < busIndex ||
                           (devPos->busIndex == busIndex &&
                            devPos->devIndex < dev->devnum)))
                        /* used to release devices here, since they
                         * are no longer used, however, this races
                         * with reinsertion of the device, and
                         * therefore reuse of the ID.  Additionally,
                         * unplugs are detected now, so it is no
                         * longer necessary. */
                        devPos = (usbDevice*)devPos->header.next;

                    /* append or insert a new device */
                    if (devPos == NULL ||
                        devPos->busIndex != busIndex ||
                        devPos->devIndex != dev->devnum)
                    {
                        bool success = false;
                        usbDevice *newDev = NULL;
                        newDev = (usbDevice*)malloc(sizeof(usbDevice));
                        memset(newDev, 0, sizeof(usbDevice));

                        /* basic stuff */
                        newDev->list = list;
                        newDev->busIndex = busIndex;
                        newDev->devIndex = dev->devnum;

                        /* determine the id (reusing if possible) */
                        newDev->id = 0;
                        while(true)
                        {
                            int prev = newDev->id;
                            forEach(&list->deviceList, findId, &newDev->id);
                            if (prev == newDev->id)
                                break;
                        }

                        /* open a handle to the usb device */
                        if ((newDev->device = usb_open(dev)) == NULL)
                            setError(newDev, "Failed to open usb device");
                        else if (! dev->config)
                            setError(newDev, "Failed to receive device descriptors");
                        /* claim the interface */
                        else if (usb_claim_interface(newDev->device, 0) < 0)
                            setError(newDev, "usb_claim_interface failed 0");
                        else
                        {
                            insertItem(&list->deviceList,
                                       (itemHeader*)devPos,
                                       (itemHeader*)newDev);
                            success = true;
                        }

                        /* grab error if there was one */
                        if (!success)
                        {
                            printError(LOG_ERROR,
                                       "updateDeviceList failed", newDev);
                            return false;
                        }
                        else
                            list->newDev(newDev);
                    }
                }

    /* if none were found remove the rest */
    if (count == 0)
        releaseDevices(list);

    if (wouldOutput(LOG_DEBUG))
    {
        message(LOG_DEBUG, "Device list %p:\n", (void*)list);
        devPos = (usbDevice*)list->deviceList.head;
        for(; devPos; devPos = (usbDevice*)devPos->header.next)
            message(LOG_DEBUG,
                    "  %p: usb:%d.%d id=%d\n", (void*)devPos,
                    devPos->busIndex, devPos->devIndex, devPos->id);
    }

    return true;
}

unsigned int releaseDevices(usbDeviceList *list)
{
    unsigned int count = list->deviceList.count;
    usbDevice *head;

    while((head = (usbDevice*)firstItem(&list->deviceList)) != NULL)
        releaseDevice(head);

    return count;
}