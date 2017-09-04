//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_defs.h"
#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#include "umc_vp9_dec_defs.h"

#ifndef __UMC_VP9_UTILS_H_
#define __UMC_VP9_UTILS_H_

#define ALIGN_POWER_OF_TWO(value, n) \
    (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

namespace UMC_VP9_DECODER
{
    class VP9DecoderFrame;

    inline
    Ipp32s clamp(Ipp32s value, Ipp32s low, Ipp32s high)
    {
        return value < low ? low : (value > high ? high : value);
    }

    void SetSegData(VP9Segmentation & seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId, Ipp32s seg_data);

    inline
    Ipp32s GetSegData(VP9Segmentation const& seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId)
    {
        return
            seg.featureData[segmentId][featureId];
    }

    inline
    bool IsSegFeatureActive(VP9Segmentation const& seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId)
    {
        return
            seg.enabled && (seg.featureMask[segmentId] & (1 << featureId));
    }

    inline
    void EnableSegFeature(VP9Segmentation & seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId)
    {
        seg.featureMask[segmentId] |= 1 << featureId;
    }

    inline
    Ipp8u IsSegFeatureSigned(SEG_LVL_FEATURES featureId)
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
    Ipp32u GetMostSignificantBit(Ipp32u n)
    {
        Ipp32u log = 0;
        Ipp32u value = n;

        for (Ipp8s i = 4; i >= 0; --i)
        {
            const Ipp32u shift = (1u << i);
            const Ipp32u x = value >> shift;
            if (x != 0)
            {
                value = x;
                log += shift;
            }
        }

        return log;
    }

    inline
    Ipp32u GetUnsignedBits(Ipp32u numValues)
    {
        return
            numValues > 0 ? GetMostSignificantBit(numValues) + 1 : 0;
    }

    Ipp32s GetQIndex(VP9Segmentation const & seg, Ipp8u segmentId, Ipp32s baseQIndex);

    void InitDequantizer(VP9DecoderFrame* info);

    UMC::ColorFormat GetUMCColorFormat_VP9(VP9DecoderFrame const*);

    void SetupPastIndependence(VP9DecoderFrame & info);

    void GetTileNBits(const Ipp32s miCols, Ipp32s & minLog2TileCols, Ipp32s & maxLog2TileCols);

} //UMC_VP9_DECODER

#endif //__UMC_VP9_UTILS_H_
#endif //UMC_ENABLE_VP9_VIDEO_DECODER
