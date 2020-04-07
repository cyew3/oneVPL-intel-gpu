// Copyright (c) 2019-2020 Intel Corporation
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

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include "av1ehw_g12_segmentation_win.h"
#include "av1ehw_g12_ddi_packer_win.h"

#include <algorithm>

using namespace AV1EHW;
using namespace AV1EHW::Gen12;
using namespace AV1EHW::Windows;
using namespace AV1EHW::Windows::Gen12;

namespace AV1EHW
{
mfxU32 MapSegIdBlockSizeToDDI(mfxU16 size)
{
    switch (size)
    {
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_8x8:
        return BLOCK_8x8;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_32x32:
        return BLOCK_32x32;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_64x64:
        return BLOCK_64x64;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_16x16:
    default:
        return BLOCK_16x16;
    }
}

void Windows::Gen12::Segmentation::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        // TODO: Get rid of this feature block and move the code to regular DDI packer

        const auto& fh = Task::FH::Get(s_task);

        auto& ddiPar = Glob::DDI_SubmitParam::Get(global);
        auto vaType = Glob::VideoCore::Get(global).GetVAType();

        auto& pps = Deref(GetDDICB<ENCODE_SET_PICTURE_PARAMETERS_AV1>(
            ENCODE_ENC_PAK_ID, DDIPar_In, vaType, ddiPar));

        pps.stAV1Segments = DXVA_Intel_Seg_AV1{};

        MFX_CHECK(fh.segmentation_params.segmentation_enabled, MFX_ERR_NONE);

        auto& segFlags = pps.stAV1Segments.SegmentFlags.fields;
        segFlags.segmentation_enabled = fh.segmentation_params.segmentation_enabled;

        const auto& segPar = Task::Segment::Get(s_task);
        segFlags.SegmentNumber = segPar.NumSegments;
        pps.PicFlags.fields.SegIdBlockSize= MapSegIdBlockSizeToDDI(segPar.SegmentIdBlockSize);

        segFlags.update_map = fh.segmentation_params.segmentation_update_map;
        segFlags.temporal_update = fh.segmentation_params.segmentation_temporal_update;

        std::transform(fh.segmentation_params.FeatureMask, fh.segmentation_params.FeatureMask + AV1_MAX_NUM_OF_SEGMENTS,
            pps.stAV1Segments.feature_mask,
            [](uint32_t x) -> UCHAR {
                return static_cast<UCHAR>(x);
            });

        for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; ++i) {
            std::transform(fh.segmentation_params.FeatureData[i], fh.segmentation_params.FeatureData[i] + SEG_LVL_MAX,
                pps.stAV1Segments.feature_data[i],
                [](uint32_t x) -> SHORT {
                    return static_cast<SHORT>(x);
                });
        }

        return MFX_ERR_NONE;
    });
}

} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
