/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_decode_pipeline_config.h"

class MFXPipelineConfigEncode
    : public MFXPipelineConfigDecode
{

public:
    MFXPipelineConfigEncode(int argc, vm_char ** argv);
    virtual IMFXPipeline        * CreatePipeline();
    virtual IMFXPipelineFactory * CreateFactory();
};

