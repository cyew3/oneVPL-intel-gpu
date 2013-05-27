/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_encode_pipeline_config.h"
#include "mfx_pipeline_transcode.h"
#include "mfx_factory_default.h"

MFXPipelineConfigEncode::MFXPipelineConfigEncode(int argc, vm_char ** argv)
    : MFXPipelineConfigDecode(argc, argv)
{
}
IMFXPipeline * MFXPipelineConfigEncode::CreatePipeline()
{
    return new MFXTranscodingPipeline(CreateFactory());
}
IMFXPipelineFactory * MFXPipelineConfigEncode::CreateFactory()
{
    return new MFXPipelineFactory();
}
