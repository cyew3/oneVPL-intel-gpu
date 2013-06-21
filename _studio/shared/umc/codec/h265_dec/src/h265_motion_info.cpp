/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_dec_defs_dec.h"
#include <memory>
#include "vm_debug.h"
#include "umc_h265_bitstream.h"
#include "h265_motion_info.h"

namespace UMC_HEVC_DECODER
{

#if !(HEVC_OPT_CHANGES & 2)
// ML: OPT: moved into the header to allow inlining
Ipp32s H265MotionVector::getAbsHor() const
{
    return abs(Horizontal);
}

Ipp32s H265MotionVector::getAbsVer() const
{
    return abs(Vertical);
}
#endif
} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
