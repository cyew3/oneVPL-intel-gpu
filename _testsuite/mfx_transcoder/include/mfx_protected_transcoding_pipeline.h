/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include "mfx_pipeline_defs.h"

#ifdef PAVP_BUILD
#include "mfx_pipeline_protected.h"
#include "mfx_pipeline_transcode.h"


class MFXProtectedTranscodingPipeline: public MFXProtectedPipeline<MFXTranscodingPipeline>
{
public:
    MFXProtectedTranscodingPipeline(IMFXPipelineFactory *pFactory)
        :MFXProtectedPipeline<MFXTranscodingPipeline>(pFactory)
    {
    };
protected:
    virtual mfxStatus CreateEncodeWRAPPER(std::unique_ptr<IVideoEncode> &&pEncoder, MFXEncodeWRAPPER ** ppEncoderWrp) override;
    virtual mfxU32 getOutputCodecId() {return m_EncParams.mfx.CodecId;}
};
#endif //PAVP_BUILD
