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
#ifdef WIN32
    #include <windows.h>
#endif
#include "iguanaIR.h"
%}

/* iguanaConnect_real handled below */
%rename(close)           iguanaClose;
%rename(createRequest)   iguanaCreateRequest;
%rename(removeData)      iguanaRemoveData;
%rename(code)            iguanaCode;
%rename(freePacket)      iguanaFreePacket;
%rename(writeRequest)    iguanaWriteRequest;
/* iguanaReadResponse handled below */
%rename(responseIsError) iguanaResponseIsError;
%rename(readPulseFile)   iguanaReadPulseFile;
%rename(readBlockFile)   iguanaReadBlockFile;
%rename(pinSpecToData)   iguanaPinSpecToData;
%rename(dataToPinSpec)   iguanaDataToPinSpec;

%typemap(default) (unsigned int dataLength, void *data)
{
    $1 = 0;
    $2 = NULL;
}

%typemap(in) (unsigned int dataLength, void *data)
{
    if (!PyBytes_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting byte string");
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
    $result = (PyObject*)$1;
}

%typemap(argout) unsigned int *dataLength
{
    $result = PyBytes_FromStringAndSize((char*)$result, *$1);
}

%typemap(in, numinputs=0) void **pulses (void *pulses)
{
    $1 = &pulses;
}
%typemap(argout) void **pulses
{
    PyObject *list;

    list = PyList_New(0);
    PyList_Append(list, $result);
    PyList_Append(list, PyBytes_FromStringAndSize(*$1, PyInt_AsLong($result) * 4));

    $result = list;
}

/* remove the old definition of iguanaReadResponse and insert one that
 * properly releases the GIL before blocking. */
%ignore iguanaReadResponse;
%inline %{
iguanaPacket readResponse(PIPE_PTR connection, unsigned int timeout)
{
    iguanaPacket retval;

    /* Release the python lock while we wait for a response event,
     * otherwise threads in Python can block while one thread loops on
     * a call to the readResponse function. */
    Py_BEGIN_ALLOW_THREADS;
    retval = iguanaReadResponse(connection, timeout);
    Py_END_ALLOW_THREADS;

    return retval;
}
%}

/* Remove the old connect call and replace it with a call with a
 * default value. */
%rename(connect) iguanaConnect_python;
%ignore iguanaConnect_real;
%typemap(default) const char *name {
    $1 = "0";
}
%inline %{
PIPE_PTR iguanaConnect_python(const char *name)
{
    return iguanaConnect(name);
}
%}

%include iguanaIR.h
