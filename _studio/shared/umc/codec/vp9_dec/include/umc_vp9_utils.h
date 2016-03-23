/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#pragma once

#include "umc_defs.h"
#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#include "umc_vp9_dec_defs.h"

#ifndef __UMC_VP9_UTILS_H_
#define __UMC_VP9_UTILS_H_

namespace UMC_VP9_DECODER
{
    class VP9DecoderFrame;

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

    Ipp32s GetQIndex(VP9Segmentation const & seg, Ipp8u segmentId, Ipp32s baseQIndex);

    void InitDequantizer(VP9DecoderFrame* info);

    UMC::ColorFormat GetUMCColorFormat_VP9(VP9DecoderFrame const*);
} //UMC_VP9_DECODER

#endif //__UMC_VP9_UTILS_H_
#endif //UMC_ENABLE_VP9_VIDEO_DECODER
