// Copyright (c) 2012-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#include "umc_structures.h"
#include "umc_vp9_utils.h"
#include "umc_av1_utils.h"

namespace UMC_AV1_DECODER
{
    void SetSegData(SegmentationParams & seg, uint8_t segmentId, SEG_LVL_FEATURES featureId, int32_t seg_data)
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

    inline uint32_t av1_get_qindex(FrameHeader const& fh, uint8_t segmentId)
    {
        if (IsSegFeatureActive(fh.segmentation_params, segmentId, SEG_LVL_ALT_Q))
        {
            const int data = GetSegData(fh.segmentation_params, segmentId, SEG_LVL_ALT_Q);
            return UMC_VP9_DECODER::clamp(fh.quantization_params.base_q_idx + data, 0, UMC_VP9_DECODER::MAXQ);
        }
        else
            return fh.quantization_params.base_q_idx;
    }

    int IsCodedLossless(FrameHeader const& fh)
    {
        int CodedLossless = 1;

        for (uint8_t i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
        {
            const uint32_t qindex = av1_get_qindex(fh, i);

            if (qindex || fh.quantization_params.DeltaQYDc ||
                fh.quantization_params.DeltaQUAc || fh.quantization_params.DeltaQUDc ||
                fh.quantization_params.DeltaQVAc || fh.quantization_params.DeltaQVDc)
            {
                CodedLossless = 0;
                break;
            }
        }

        return CodedLossless;
    }

#if UMC_AV1_DECODER_REV == 5000
    void InheritFromPrevFrame(FrameHeader& fh, FrameHeader const& prev_fh)
    {
        for (uint32_t i = 0; i < TOTAL_REFS; i++)
            fh.loop_filter_params.loop_filter_ref_deltas[i] = prev_fh.loop_filter_params.loop_filter_ref_deltas[i];

        for (uint32_t i = 0; i < MAX_MODE_LF_DELTAS; i++)
            fh.loop_filter_params.loop_filter_mode_deltas[i] = prev_fh.loop_filter_params.loop_filter_mode_deltas[i];

        fh.cdef_params.cdef_damping = prev_fh.cdef_params.cdef_damping;
        for (uint32_t i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            fh.cdef_params.cdef_y_strength[i] = prev_fh.cdef_params.cdef_y_strength[i];
            fh.cdef_params.cdef_uv_strength[i] = prev_fh.cdef_params.cdef_uv_strength[i];
        }

        fh.segmentation_params = prev_fh.segmentation_params;
    }
#endif
}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
