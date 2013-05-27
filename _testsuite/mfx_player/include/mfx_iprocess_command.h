/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxdefs.h"
#include "vm_strings.h"

class IProcessCommand
{
public:
    virtual ~IProcessCommand(){}
    virtual mfxStatus  ProcessCommand( vm_char **argv, mfxI32 argc) = 0;
    virtual bool       IsPrintParFile() = 0;
};
