/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
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

inline mfxU8 ConvertH264FrameTypeToMFX(Ipp32s slice_type)
{
    mfxU8 mfx_type;
    switch(slice_type)
    {
    case I_SLICE:
        mfx_type = MFX_FRAMETYPE_I;
        break;
    case P_SLICE:
        mfx_type = MFX_FRAMETYPE_P;
        break;
    case B_SLICE:
        mfx_type = MFX_FRAMETYPE_B;
        break;
    default:
        VM_ASSERT(false);
        mfx_type = MFX_FRAMETYPE_I;
    }

    return mfx_type;
}

inline mfxU8 ConvertH264SliceTypeToMFX(Ipp32s slice_type)
{
    mfxU8 mfx_type;
    switch(slice_type)
    {
    case I_SLICE:
        mfx_type = MFX_SLICETYPE_I;
        break;
    case P_SLICE:
        mfx_type = MFX_SLICETYPE_P;
        break;
    case B_SLICE:
        mfx_type = MFX_SLICETYPE_B;
        break;
    default:
        VM_ASSERT(false);
        mfx_type = MFX_SLICETYPE_I;
    }

    return mfx_type;
}

UMC::Status FillVideoParam(const H265SeqParamSet * seq, mfxVideoParam *par, bool full);

} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER

#endif // __UMC_H264_DEC_MFX_H
