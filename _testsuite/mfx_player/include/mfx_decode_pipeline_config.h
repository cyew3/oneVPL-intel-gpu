/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ipipeline_config.h"
#include "mfx_pipeline_dec.h"
#include "mfx_factory_default.h"

class MFXPipelineConfigDecode
    : public IMFXPipelineConfig
{
public:
    MFXPipelineConfigDecode(int argc, vm_char ** argv);

    virtual IPipelineSynhro     * GetExternalSync() {return NULL;}
    virtual IMFXPipeline * CreatePipeline();

    virtual vm_char** GetArgv()
    {
        return m_argv;
    }
    virtual int   GetArgc()
    {
        return m_argc;
    }


public:
    int      m_argc;
    vm_char ** m_argv;
};

