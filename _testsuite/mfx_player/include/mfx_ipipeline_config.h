/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include <mutex>
#include "mfx_ipipeline.h"
#include "mfx_ifactory.h"
#include "mfx_iallocator_factory.h"

class IMFXPipelineConfig
{
public:
    virtual ~IMFXPipelineConfig() {}
    virtual IMFXPipeline *    CreatePipeline()      = 0;
    virtual std::mutex      * GetExternalSync()     = 0;
    virtual vm_char**         GetArgv()             = 0;
    virtual int               GetArgc()             = 0;
};

