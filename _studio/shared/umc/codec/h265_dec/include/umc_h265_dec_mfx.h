/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#ifndef __UMC_H265_DEC_MFX_H
#define __UMC_H265_DEC_MFX_H

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_dec_defs_dec.h"
#include "mfx_enc_common.h"

namespace UMC_HEVC_DECODER
{

// Initialize mfxVideoParam structure based on decoded bitstream header values
UMC::Status FillVideoParam(const H265SeqParamSet * seq, mfxVideoParam *par, bool full);

} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER

#endif // __UMC_H264_DEC_MFX_H
