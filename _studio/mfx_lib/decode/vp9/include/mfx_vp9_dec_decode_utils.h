//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

#ifndef _MFX_VP9_DECODE_UTILS_H_
#define _MFX_VP9_DECODE_UTILS_H_

#include <assert.h>
#include "umc_vp9_dec_defs.h"

namespace UMC_VP9_DECODER
{
    class VP9Bitstream;
    class VP9DecoderFrame;
}

namespace MfxVP9Decode
{
    void ParseSuperFrameIndex(const mfxU8 *data, size_t data_sz,
                              mfxU32 sizes[8], mfxU32 *count);

    inline mfxU8 ReadMarker(const mfxU8 *data)
    {
        return *data;
    }

}; // namespace MfxVP9Decode

#endif // _MFX_VP9_DECODE_UTILS_H_
#endif // MFX_ENABLE_VP9_VIDEO_DECODE || MFX_ENABLE_VP9_VIDEO_DECODE_HW
