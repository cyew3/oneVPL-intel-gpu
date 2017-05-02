//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_MFX_UTILS_H
#define __UMC_H265_MFX_UTILS_H

namespace UMC_HEVC_DECODER
{

// MFX utility API functions
namespace MFX_Utility
{
    // Initialize mfxVideoParam structure based on decoded bitstream header values
    UMC::Status FillVideoParam(const H265SeqParamSet * seq, mfxVideoParam *par, bool full);

    // Returns implementation platform
    eMFXPlatform GetPlatform_H265(VideoCORE * core, mfxVideoParam * par);

    // Find bitstream header NAL units, parse them and fill application parameters structure
    UMC::Status DecodeHeader(TaskSupplier_H265 * supplier, UMC::VideoDecoderParams* params, mfxBitstream *bs, mfxVideoParam *out);

    // MediaSDK DECODE_Query API function
    mfxStatus Query_H265(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type);
    // Validate input parameters
    bool CheckVideoParam_H265(mfxVideoParam *in, eMFXHWType type);

    bool IsBugSurfacePoolApplicable(eMFXHWType hwtype, mfxVideoParam * par);

    // Check HW capabilities
    bool IsNeedPartialAcceleration_H265(mfxVideoParam * par, eMFXHWType type);
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_MFX_UTILS_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
