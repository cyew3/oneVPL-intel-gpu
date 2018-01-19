/******************************************************************************\
Copyright (c) 2005-2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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

    mfxStatus sts = CTranscodingPipeline::InitEncMfxParams(pInParams);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}
