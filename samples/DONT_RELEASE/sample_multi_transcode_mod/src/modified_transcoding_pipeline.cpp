/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2015 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "modified_transcoding_pipeline.h"

CModifiedTranscodingPipeline::CModifiedTranscodingPipeline(void)
{
    MSDK_ZERO_MEMORY(m_ExtFeiCodingOption);
    m_ExtFeiCodingOption.Header.BufferId = MFX_EXTBUFF_FEI_CODING_OPTION;
    m_ExtFeiCodingOption.Header.BufferSz = sizeof(m_ExtFeiCodingOption);
}

CModifiedTranscodingPipeline::~CModifiedTranscodingPipeline(void)
{
}

mfxStatus CModifiedTranscodingPipeline::InitEncMfxParams(sInputParams *pInParams)
{
    // disabling of HME is requested
    if (!!pInParams->reserved[0])
    {
        m_ExtFeiCodingOption.DisableHME = 1;
        m_ExtFeiCodingOption.DisableSuperHME = 1;
        m_ExtFeiCodingOption.DisableUltraHME = 1;
        m_EncExtParams.push_back((mfxExtBuffer*)&m_ExtFeiCodingOption);
    }

    if (!!pInParams->reserved[1])
    {
        m_mfxEncParams.mfx.NumRefFrame = pInParams->reserved[1];
    }

    mfxStatus sts = CTranscodingPipeline::InitEncMfxParams(pInParams);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}
