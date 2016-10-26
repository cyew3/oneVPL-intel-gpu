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

#include <stdexcept>
#include <string>

#include "mfx_vp9_dec_decode_utils.h"

#include "umc_vp9_bitstream.h"
#include "umc_vp9_frame.h"

namespace MfxVP9Decode
{

    void GetTileNBits(const mfxI32 miCols, mfxI32 & minLog2TileCols, mfxI32 & maxLog2TileCols)
    {
        const mfxI32 sbCols = ALIGN_POWER_OF_TWO(miCols, MI_BLOCK_SIZE_LOG2) >> MI_BLOCK_SIZE_LOG2;
        mfxI32 minLog2 = 0, maxLog2 = 0;

        while ((sbCols >> maxLog2) >= MIN_TILE_WIDTH_B64)
            ++maxLog2;
        --maxLog2;
        if (maxLog2 < 0)
            maxLog2 = 0;

        while ((MAX_TILE_WIDTH_B64 << minLog2) < sbCols)
            ++minLog2;

        assert(minLog2 <= maxLog2);

        minLog2TileCols = minLog2;
        maxLog2TileCols = maxLog2;
    }

    void ClearAllSegFeatures(UMC_VP9_DECODER::VP9Segmentation & seg)
    {
        memset(&seg.featureData, 0, sizeof(seg.featureData));
        memset(&seg.featureMask, 0, sizeof(seg.featureMask));
    }

    void EnableSegFeature(UMC_VP9_DECODER::VP9Segmentation & seg, mfxU8 segmentId, UMC_VP9_DECODER::SEG_LVL_FEATURES featureId)
    {
        seg.featureMask[segmentId] |= 1 << featureId;
    }

    mfxU8 IsSegFeatureSigned(UMC_VP9_DECODER::SEG_LVL_FEATURES featureId)
    {
        return SEG_FEATURE_DATA_SIGNED[featureId];
    }

    void SetSegData(UMC_VP9_DECODER::VP9Segmentation & seg, mfxU8 segmentId, UMC_VP9_DECODER::SEG_LVL_FEATURES featureId, mfxI32 seg_data)
    {
        VM_ASSERT(seg_data <= UMC_VP9_DECODER::SEG_FEATURE_DATA_MAX[featureId]);
        if (seg_data < 0)
        {
            VM_ASSERT(UMC_VP9_DECODER::SEG_FEATURE_DATA_SIGNED[featureId]);
            VM_ASSERT(-seg_data <= UMC_VP9_DECODER::SEG_FEATURE_DATA_MAX[featureId]);
        }

        seg.featureData[segmentId][featureId] = seg_data;
    }

    void SetupPastIndependence(UMC_VP9_DECODER::VP9DecoderFrame & info)
    {
        // Reset the segment feature data to the default stats:
        // Features disabled, 0, with delta coding (Default state).

        ClearAllSegFeatures(info.segmentation);
        info.segmentation.absDelta = UMC_VP9_DECODER::SEGMENT_DELTADATA;

        // set_default_lf_deltas()
        info.lf.modeRefDeltaEnabled = 1;
        info.lf.modeRefDeltaUpdate = 1;

        info.lf.refDeltas[UMC_VP9_DECODER::INTRA_FRAME] = 1;
        info.lf.refDeltas[UMC_VP9_DECODER::LAST_FRAME] = 0;
        info.lf.refDeltas[UMC_VP9_DECODER::GOLDEN_FRAME] = -1;
        info.lf.refDeltas[UMC_VP9_DECODER::ALTREF_FRAME] = -1;

        info.lf.modeDeltas[0] = 0;
        info.lf.modeDeltas[1] = 0;

        memset(info.refFrameSignBias, 0, sizeof(info.refFrameSignBias));
    }

    void ParseSuperFrameIndex(const mfxU8 *data, size_t data_sz,
                              mfxU32 sizes[8], mfxU32 *count)
    {
        assert(data_sz);
        mfxU8 marker = ReadMarker(data + data_sz - 1);
        *count = 0;

        if ((marker & 0xe0) == 0xc0)
        {
            const mfxU32 frames = (marker & 0x7) + 1;
            const mfxU32 mag = ((marker >> 3) & 0x3) + 1;
            const size_t index_sz = 2 + mag * frames;

            mfxU8 marker2 = ReadMarker(data + data_sz - index_sz);

            if (data_sz >= index_sz && marker2 == marker)
            {
                // found a valid superframe index
                const mfxU8 *x = &data[data_sz - index_sz + 1];

                for (mfxU32 i = 0; i < frames; i++)
                {
                    mfxU32 this_sz = 0;

                    for (mfxU32 j = 0; j < mag; j++)
                        this_sz |= (*x++) << (j * 8);
                    sizes[i] = this_sz;
                }

                *count = frames;
            }
        }
    }

}; // namespace MfxVP9Decode

#endif // _MFX_VP9_DECODE_UTILS_H_
