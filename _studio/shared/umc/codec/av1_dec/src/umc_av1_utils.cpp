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
        // Reset the segment feature data to the default stats:
        // Features disabled, 0, with delta coding (Default state).

        ClearAllSegFeatures(info.segmentation_params);

        // set_default_lf_deltas()
        info.loop_filter_params.loop_filter_delta_enabled = 1;
        info.loop_filter_params.loop_filter_delta_update = 1;

        info.loop_filter_params.loop_filter_ref_deltas[INTRA_FRAME] = 1;
        info.loop_filter_params.loop_filter_ref_deltas[LAST_FRAME] = 0;
        info.loop_filter_params.loop_filter_ref_deltas[LAST2_FRAME] = info.loop_filter_params.loop_filter_ref_deltas[LAST_FRAME];
        info.loop_filter_params.loop_filter_ref_deltas[LAST3_FRAME] = info.loop_filter_params.loop_filter_ref_deltas[LAST_FRAME];
        info.loop_filter_params.loop_filter_ref_deltas[BWDREF_FRAME] = info.loop_filter_params.loop_filter_ref_deltas[LAST_FRAME];
        info.loop_filter_params.loop_filter_ref_deltas[GOLDEN_FRAME] = -1;
        info.loop_filter_params.loop_filter_ref_deltas[ALTREF2_FRAME] = -1;
        info.loop_filter_params.loop_filter_ref_deltas[ALTREF_FRAME] = -1;

        info.loop_filter_params.loop_filter_mode_deltas[0] = 0;
        info.loop_filter_params.loop_filter_mode_deltas[1] = 0;
    }
}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
