//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#include "umc_structures.h"
#include "umc_vp9_utils.h"
#include "umc_av1_utils.h"

namespace UMC_AV1_DECODER
{
    void SetSegData(AV1Segmentation & seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId, Ipp32s seg_data)
    {
        VM_ASSERT(seg_data <= SEG_FEATURE_DATA_MAX[featureId]);
        if (seg_data < 0)
        {
            VM_ASSERT(SEG_FEATURE_DATA_SIGNED[featureId]);
            VM_ASSERT(-seg_data <= SEG_FEATURE_DATA_MAX[featureId]);
        }

        seg.FeatureData[segmentId][featureId] = seg_data;
    }

    void SetupPastIndependence(FrameHeader & info)
    {
        ClearAllSegFeatures(info.segmentation_params);

        SetDefaultLFParams(info.loop_filter_params);
    }

    inline Ipp32u av1_get_qindex(FrameHeader const * fh, Ipp8u segmentId)
    {
        if (IsSegFeatureActive(fh->segmentation_params, segmentId, SEG_LVL_ALT_Q))
        {
            const int data = GetSegData(fh->segmentation_params, segmentId, SEG_LVL_ALT_Q);
            return UMC_VP9_DECODER::clamp(fh->base_q_idx + data, 0, UMC_VP9_DECODER::MAXQ);
        }
        else
            return fh->base_q_idx;
    }

    int IsCodedLossless(FrameHeader const * fh)
    {
        int CodedLossless = 1;

        for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
        {
            const Ipp32u qindex = av1_get_qindex(fh, i);

            if (qindex || fh->DeltaQYDc ||
                fh->DeltaQUAc || fh->DeltaQUDc ||
                fh->DeltaQVAc || fh->DeltaQVDc)
            {
                CodedLossless = 0;
                break;
            }
        }

        return CodedLossless;
    }

    void InheritFromPrevFrame(FrameHeader* fh, FrameHeader const* prev_fh)
    {
        for (Ipp32u i = 0; i < TOTAL_REFS; i++)
            fh->loop_filter_params.loop_filter_ref_deltas[i] = prev_fh->loop_filter_params.loop_filter_ref_deltas[i];

        for (Ipp32u i = 0; i < MAX_MODE_LF_DELTAS; i++)
            fh->loop_filter_params.loop_filter_mode_deltas[i] = prev_fh->loop_filter_params.loop_filter_mode_deltas[i];

        fh->cdef_damping = prev_fh->cdef_damping;
        for (Ipp32u i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            fh->cdef_y_strength[i] = prev_fh->cdef_y_strength[i];
            fh->cdef_uv_strength[i] = prev_fh->cdef_uv_strength[i];
        }

        fh->segmentation_params = prev_fh->segmentation_params;
    }
}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
