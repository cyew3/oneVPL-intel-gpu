/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_factory_transcode.h"
#include "mfx_encoder_wrap.h"

IMFXVideoRender * MFXPipelineTrancodeFactory::CreateRender( IPipelineObjectDesc * pParams)
{
    PipelineObjectDescPtr<IMFXVideoRender>create(pParams);

    switch (create->Type)
    {
        case RENDER_MFX_ENCODER:
        {
            //TODO: incorrect
            return NULL;//new MFXEncodeWRAPPER(pCreate->m_refParams, pCreate->m_status, NULL);
        }
    default:
        return NULL;
    }
}
