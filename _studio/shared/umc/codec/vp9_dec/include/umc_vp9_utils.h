//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_defs.h"
#if defined(UMC_ENABLE_VP9_VIDEO_DECODER) || defined(UMC_ENABLE_AV1_VIDEO_DECODER)

#include "umc_vp9_dec_defs.h"

#ifndef __UMC_VP9_UTILS_H_
#define __UMC_VP9_UTILS_H_

#define ALIGN_POWER_OF_TWO(value, n) \
    (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

namespace UMC_VP9_DECODER
{
    class VP9DecoderFrame;

    inline
    int32_t clamp(int32_t value, int32_t low, int32_t high)
    {
        return value < low ? low : (value > high ? high : value);
    }

    void SetSegData(VP9Segmentation & seg, uint8_t segmentId, SEG_LVL_FEATURES featureId, int32_t seg_data);

    inline
    int32_t GetSegData(VP9Segmentation const& seg, uint8_t segmentId, SEG_LVL_FEATURES featureId)
    {
        return
            seg.featureData[segmentId][featureId];
    }

    inline
    bool IsSegFeatureActive(VP9Segmentation const& seg, uint8_t segmentId, SEG_LVL_FEATURES featureId)
    {
        return
            seg.enabled && (seg.featureMask[segmentId] & (1 << featureId));
    }

    inline
    void EnableSegFeature(VP9Segmentation & seg, uint8_t segmentId, SEG_LVL_FEATURES featureId)
    {
        seg.featureMask[segmentId] |= 1 << featureId;
    }

    inline
    uint8_t IsSegFeatureSigned(SEG_LVL_FEATURES featureId)
    {
        return
            SEG_FEATURE_DATA_SIGNED[featureId];
    }

    inline
    void ClearAllSegFeatures(VP9Segmentation & seg)
    {
        memset(&seg.featureData, 0, sizeof(seg.featureData));
        memset(&seg.featureMask, 0, sizeof(seg.featureMask));
    }

    inline
    uint32_t GetMostSignificantBit(uint32_t n)
    {
        uint32_t log = 0;
        uint32_t value = n;

        for (int8_t i = 4; i >= 0; --i)
        {
            const uint32_t shift = (1u << i);
            const uint32_t x = value >> shift;
            if (x != 0)
            {
                value = x;
                log += shift;
            }
        }

        return log;
    }

    inline
    uint32_t GetUnsignedBits(uint32_t numValues)
    {
        return
            numValues > 0 ? GetMostSignificantBit(numValues) + 1 : 0;
    }

    int32_t GetQIndex(VP9Segmentation const & seg, uint8_t segmentId, int32_t baseQIndex);

    void InitDequantizer(VP9DecoderFrame* info);

    UMC::ColorFormat GetUMCColorFormat_VP9(VP9DecoderFrame const*);

    void SetupPastIndependence(VP9DecoderFrame & info);

    void GetTileNBits(const int32_t miCols, int32_t & minLog2TileCols, int32_t & maxLog2TileCols);

} //UMC_VP9_DECODER

#endif //__UMC_VP9_UTILS_H_
#endif //UMC_ENABLE_VP9_VIDEO_DECODER
