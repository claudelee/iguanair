/****************************************************************************
 ** driver.h ****************************************************************
 ****************************************************************************
 *
 * Header for the lowest level interface to the USB device.
 *
 * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
 * Author: Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distributed under the GPL version 2.
 * See LICENSE for license details.
 */
#pragma once

#include "devicebase.h"

/* remaining function calls are illegal until this returns true */
bool findDriver(const char *path, const char **preferred, bool onlyPreferred);

/* initialization and cleanup */
bool initializeDriver();
void cleanupDriver();

/* wrapped usb methods */
bool findDeviceEndpoints(deviceInfo *info, int *maxPacketSize);
int interruptRecv(deviceInfo *info, void *buffer, int bufSize, int timeout);
int interruptSend(deviceInfo *info, void *buffer, int bufSize, int timeout);
int clearHalt(deviceInfo *info, unsigned int ep);
int resetDevice(deviceInfo *info);

/* miscellaneous helper functions */
void getDeviceLocation(deviceInfo *info, uint8_t loc[2]);

/* release a single device (during destruction) */
void releaseDevice(deviceInfo *info);
void freeDevice(deviceInfo *info);

/* methods of a device list */
deviceList* prepareDeviceList(usbId *ids, deviceFunc ndf);
void claimDevices(deviceList *devList, bool claim, bool force);
bool updateDeviceList(deviceList *devList);
unsigned int stopDevices(deviceList *devList);
unsigned int releaseDevices(deviceList *devList);

/* dump errors to a stream */
void printError(int level, char *msg, deviceInfo *info);
