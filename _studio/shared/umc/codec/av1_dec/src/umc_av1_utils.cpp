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
#include "umc_av1_utils.h"

namespace UMC_AV1_DECODER
{
#if UMC_AV1_DECODER_REV >= 2520
    void SetSegData(AV1Segmentation & seg, Ipp8u segmentId, SEG_LVL_FEATURES featureId, Ipp32s seg_data)
    {
        VM_ASSERT(seg_data <= SEG_FEATURE_DATA_MAX[featureId]);
        if (seg_data < 0)
        {
            VM_ASSERT(SEG_FEATURE_DATA_SIGNED[featureId]);
            VM_ASSERT(-seg_data <= SEG_FEATURE_DATA_MAX[featureId]);
        }

        seg.featureData[segmentId][featureId] = seg_data;
    }

    void SetupPastIndependence(FrameHeader & info)
    {
        // Reset the segment feature data to the default stats:
        // Features disabled, 0, with delta coding (Default state).

        ClearAllSegFeatures(info.segmentation);
        info.segmentation.absDelta = UMC_VP9_DECODER::SEGMENT_DELTADATA;

        // set_default_lf_deltas()
        info.lf.modeRefDeltaEnabled = 1;
        info.lf.modeRefDeltaUpdate = 1;

        info.lf.refDeltas[INTRA_FRAME] = 1;
        info.lf.refDeltas[LAST_FRAME] = 0;
        info.lf.refDeltas[LAST2_FRAME] = info.lf.refDeltas[LAST_FRAME];
        info.lf.refDeltas[LAST3_FRAME] = info.lf.refDeltas[LAST_FRAME];
        info.lf.refDeltas[BWDREF_FRAME] = info.lf.refDeltas[LAST_FRAME];
        info.lf.refDeltas[GOLDEN_FRAME] = -1;
        info.lf.refDeltas[ALTREF2_FRAME] = -1;
        info.lf.refDeltas[ALTREF_FRAME] = -1;

        info.lf.modeDeltas[0] = 0;
        info.lf.modeDeltas[1] = 0;
    }
#endif
}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
