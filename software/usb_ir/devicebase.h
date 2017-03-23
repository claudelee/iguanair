#pragma once

enum
{
    EP_IN,
    EP_OUT,

    INVALID_VENDOR = 0
};

#define END_OF_USB_ID_LIST {INVALID_VENDOR,0,NULL}

typedef struct usbId
{
    unsigned short idVendor;
    unsigned short idProduct;

    /* generic pointer to store info specific to this device type */
    void *data;
} usbId;

typedef struct deviceInfo
{
    /* unique id (counter) */
    unsigned int id;

    /* what device id did it match? */
    usbId type;

    /* set when device is logically stopped prior to removal from list */
    bool stopped;
} deviceInfo;

/* prototype of the function called when a new device is found */
typedef void (*deviceFunc)(deviceInfo *info);

/* prototype of the functions dealing w application shutdown */
typedef bool (*isStoppingFunc)(void *state);
typedef void (*stopNowFunc)(void *state);

/* hide type that we pass to list functions */
typedef void deviceList;
