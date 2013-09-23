/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

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
#ifdef PAVP_BUILD
    const vm_char *strProtected = VM_STRING("-protected");

    for (int i = 0; i < GetArgc(); i++)
        if (0 == vm_string_strncmp(GetArgv()[i], strProtected, vm_string_strlen(strProtected)))
            return new MFXProtectedTranscodingPipeline(CreateFactory());
#endif//PAVP_BUILD
    return new MFXTranscodingPipeline(CreateFactory());
}
IMFXPipelineFactory * MFXPipelineConfigEncode::CreateFactory()
{
    return new MFXPipelineFactory();
}
