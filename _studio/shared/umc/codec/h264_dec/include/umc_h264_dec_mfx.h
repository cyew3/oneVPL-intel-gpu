//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_H264_DEC_MFX_H
#define __UMC_H264_DEC_MFX_H

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_dec_defs_dec.h"
#include "mfxdefs.h"
#include "mfxstructures.h"

namespace UMC
{

inline mfxU8 ConvertH264FrameTypeToMFX(Ipp32s slice_type)
{
    mfxU8 mfx_type;
    switch(slice_type)
    {
    case UMC::INTRASLICE:
        mfx_type = MFX_FRAMETYPE_I;
        break;
    case UMC::PREDSLICE:
        mfx_type = MFX_FRAMETYPE_P;
        break;
    case UMC::BPREDSLICE:
        mfx_type = MFX_FRAMETYPE_B;
        break;
    case UMC::S_PREDSLICE:
        mfx_type = MFX_FRAMETYPE_P;//MFX_SLICETYPE_SP;
        break;
    case UMC::S_INTRASLICE:
        mfx_type = MFX_FRAMETYPE_I;//MFX_SLICETYPE_SI;
        break;
    default:
        VM_ASSERT(false);
        mfx_type = MFX_FRAMETYPE_I;
    }

    return mfx_type;
}

Status FillVideoParam(const UMC_H264_DECODER::H264SeqParamSet * seq, mfxVideoParam *par, bool full);
Status FillVideoParamExtension(const UMC_H264_DECODER::H264SeqParamSetMVCExtension * seqEx, mfxVideoParam *par);

} // namespace UMC

#endif // UMC_ENABLE_H264_VIDEO_DECODER

#endif // __UMC_H264_DEC_MFX_H
