
/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "etw_event.h"


void ETWEventHelper::Write(REGHANDLE handle, int opcode, int level, const TCHAR * msg)
{
    //event not registered
    if (0 == handle)
    {
        return;
    }
 
    EVENT_DESCRIPTOR descriptor = {};

    descriptor.Opcode = (UCHAR)opcode; 
    descriptor.Level  = (UCHAR)level;
    //convertion to ascii

    if (strlen(msg))
    {    
        EVENT_DATA_DESCRIPTOR data_descriptor;
        EventDataDescCreate(&data_descriptor, msg, (ULONG)(strlen(msg) + 1));
        EventWrite(handle, &descriptor, 1, &data_descriptor);
    }
    else
    {
        EventWrite(handle, &descriptor, 0, 0);
    }
}
