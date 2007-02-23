/****************************************************************************
 ** iguanaIR.i **************************************************************
 ****************************************************************************
 *
 * Swig interface file to provide access to Iguanaworks USB devices to
 * python.
 *
 * Copyright (C) 2006, Joseph Dunn <jdunn@iguanaworks.net>
 *
 * Distribute under the GPL version 2.
 * See COPYING for license details.
 */
%module iguanaIR
%include "typemaps.i"

%{
#include "iguanaIR.h"
%}

%rename(connect) iguanaConnect;
%rename(createRequest) iguanaCreateRequest;
%rename(writeRequest) iguanaWriteRequest;
%rename(readResponse) iguanaReadResponse;
%rename(responseIsError) iguanaResponseIsError;
%rename(removeData) iguanaRemoveData;

%typemap(default) (unsigned int dataLength, void *data)
{
    $1 = 0;
    $2 = NULL;
}

%typemap(in) (unsigned int dataLength, void *data)
{
    if (!PyString_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting a string");
        return NULL;
    }
    $1 = PyString_Size($input);
    $2 = (void *) PyString_AsString($input);
};


%typemap(in, numinputs=0) unsigned int *dataLength (unsigned int temp)
{
    $1 = &temp;
}

%typemap(out) unsigned char*
{
    $result = $1;
}

%typemap(argout) unsigned int *dataLength
{
    $result = PyString_FromStringAndSize($result, *$1);
}


/*
void* iguanaRemoveData(iguanaPacket pkt, unsigned int *dataLength);
*/

%include iguanaIR.h