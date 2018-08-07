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
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_UTILS_H_
#define __UMC_AV1_UTILS_H_

#include "umc_av1_dec_defs.h"

namespace UMC_AV1_DECODER
{
    inline Ipp32u CeilLog2(Ipp32u x) { Ipp32u l = 0; while (x > (1U << l)) l++; return l; }

    // we stop using UMC_VP9_DECODER namespace starting from Rev 0.25.2
    // because after switch to AV1-specific segmentation stuff there are only few definitions we need to re-use from VP9
    void SetSegData(AV1Segmentation & seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId, Ipp32s seg_data);

    inline
    Ipp32s GetSegData(AV1Segmentation const& seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId)
    {
        return
            seg.featureData[segmentId][featureId];
    }

    inline
    bool IsSegFeatureActive(AV1Segmentation const& seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId)
    {
        return
            seg.enabled && (seg.featureMask[segmentId] & (1 << featureId));
    }

    inline
    void EnableSegFeature(AV1Segmentation & seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId)
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
        void ClearAllSegFeatures(AV1Segmentation & seg)
    {
        memset(&seg.featureData, 0, sizeof(seg.featureData));
        memset(&seg.featureMask, 0, sizeof(seg.featureMask));
    }

    void SetupPastIndependence(FrameHeader & info);

    inline bool IsFrameIntraOnly(FrameHeader const * fh)
    {
        return (fh->frameType == KEY_FRAME || fh->intraOnly);
    }

    inline bool IsFrameResilent(FrameHeader const * fh)
    {
        return IsFrameIntraOnly(fh) || fh->errorResilientMode;
    }

    inline void mfx_memcpy(void * dst, size_t dstLen, void * src, size_t len)
    {
        memcpy_s(dst, dstLen, src, len);
    }
}

#endif //__UMC_AV1_UTILS_H_
#endif //UMC_ENABLE_AV1_VIDEO_DECODER
