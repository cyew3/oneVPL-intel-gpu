//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
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
    static const Ipp8u MAX_LOOP_FILTER = 63;

    static const Ipp8u SEG_FEATURE_DATA_SIGNED[UMC_VP9_DECODER::SEG_LVL_MAX] = { 1, 1, 0, 0 };
    static const Ipp8u SEG_FEATURE_DATA_MAX[UMC_VP9_DECODER::SEG_LVL_MAX] = { UMC_VP9_DECODER::MAXQ, MAX_LOOP_FILTER, 3, 0 };

    inline mfxI32 GetMostSignificantBit(mfxU32 n)
    {
        mfxI32 log = 0;
        mfxU32 value = n;

        for (mfxI32 i = 4; i >= 0; --i)
        {
            const mfxI32 shift = (1 << i);
            const mfxU32 x = value >> shift;
            if (x != 0)
            {
                value = x;
                log += shift;
            }
        }
        return log;
    }

    static inline mfxI32 GetUnsignedBits(mfxU32 numValues)
    {
        return numValues > 0 ? GetMostSignificantBit(numValues) + 1 : 0;
    }

    static const mfxU8 MIN_TILE_WIDTH_B64 = 4;
    static const mfxU8 MAX_TILE_WIDTH_B64 = 64;
    static const mfxU8 MI_SIZE_LOG2 = 3;
    static const mfxU8 MI_BLOCK_SIZE_LOG2 = 6 - MI_SIZE_LOG2; // 64 = 2^6

    #define ALIGN_POWER_OF_TWO(value, n) \
        (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

    inline mfxI32 clamp(mfxI32 value, mfxI32 low, mfxI32 high)
    {
        return value < low ? low : (value > high ? high : value);
    }

    void GetTileNBits(const mfxI32 miCols, mfxI32 & minLog2TileCols, mfxI32 & maxLog2TileCols);

    ///////////////////////////////////////////////////////////

    void ClearAllSegFeatures(UMC_VP9_DECODER::VP9Segmentation & seg);

    void EnableSegFeature(UMC_VP9_DECODER::VP9Segmentation & seg, mfxU8 segmentId, UMC_VP9_DECODER::SEG_LVL_FEATURES featureId);

    mfxU8 IsSegFeatureSigned(UMC_VP9_DECODER::SEG_LVL_FEATURES featureId);

    void SetSegData(UMC_VP9_DECODER::VP9Segmentation & seg, mfxU8 segmentId,
                           UMC_VP9_DECODER::SEG_LVL_FEATURES featureId, mfxI32 seg_data);

    ////////////////////////////////////////////////

    void SetupPastIndependence(UMC_VP9_DECODER::VP9DecoderFrame & info);

    void ParseSuperFrameIndex(const mfxU8 *data, size_t data_sz,
                              mfxU32 sizes[8], mfxU32 *count);

    inline mfxU8 ReadMarker(const mfxU8 *data)
    {
        return *data;
    }

}; // namespace MfxVP9Decode

#endif // _MFX_VP9_DECODE_UTILS_H_
#endif // MFX_ENABLE_VP9_VIDEO_DECODE || MFX_ENABLE_VP9_VIDEO_DECODE_HW
