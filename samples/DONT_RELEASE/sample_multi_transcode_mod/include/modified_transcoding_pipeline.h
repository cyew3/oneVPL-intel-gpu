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

#pragma once
#include "mfxfei.h"
#include "pipeline_transcode.h"

using namespace TranscodingSample;

class CModifiedTranscodingPipeline :
    public CTranscodingPipeline
{
public:
    CModifiedTranscodingPipeline(void);
    ~CModifiedTranscodingPipeline(void);

    virtual mfxStatus InitEncMfxParams(sInputParams *pInParams);

protected:
    // for disabling HME
    mfxExtFeiCodingOption m_ExtFeiCodingOption;

};

