/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2019 Intel Corporation. All Rights Reserved.

File Name: .cpp

\* ****************************************************************************** */

#include "mfx_protected_transcoding_pipeline.h"
#include "mfx_encoder_protected.h"

#ifdef PAVP_BUILD
mfxStatus MFXProtectedTranscodingPipeline::CreateEncodeWRAPPER(std::unique_ptr<IVideoEncode> &&pEncoder, MFXEncodeWRAPPER ** ppEncoderWrp)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_WITH_ERR(*ppEncoderWrp = new MFXProtectedEncodeWRAPPER(dynamic_cast<CPAVPVideo*>(m_pavpVideo), m_components[eREN], &sts, std::move(pEncoder)), MFX_ERR_MEMORY_ALLOC);
    return MFX_ERR_NONE;
}
#endif//PAVP_BUILD
