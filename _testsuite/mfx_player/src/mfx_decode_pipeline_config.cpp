/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_decode_pipeline_config.h"
#include "mfx_pipeline_dec.h"
#include "mfx_factory_default.h"
#include "mfx_allocators_generic.h"


MFXPipelineConfigDecode::MFXPipelineConfigDecode(int argc, vm_char ** argv)
    : m_argc(argc)
    , m_argv(argv)
{
}
IMFXPipeline * MFXPipelineConfigDecode::CreatePipeline()
{
#ifdef PAVP_BUILD
    const vm_char *strProtected = VM_STRING("-protected");

    for (int i = 0; i < GetArgc(); i++)
        if (0 == vm_string_strncmp(GetArgv()[i], strProtected, vm_string_strlen(strProtected)))
            return new MFXProtectedDecPipeline(new MFXPipelineFactory());
#endif//PAVP_BUILD
    return new MFXDecPipeline(new MFXPipelineFactory());
}
