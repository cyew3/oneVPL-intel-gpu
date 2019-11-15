// Copyright (c) 2019 Intel Corporation
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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_g11_data.h"

namespace HEVCEHW
{
namespace Gen12
{
    using Gen11::Defaults;
    using Gen11::FrameBaseInfo;
    using Gen11::Task;

    enum eFeatureId
    {
        FEATURE_REXT = Gen11::eFeatureId::NUM_FEATURES
        , FEATURE_SCC
        , FEATURE_MFE
        , FEATURE_CAPS
        , FEATURE_SAO
        , FEATURE_QP_MODULATION
        , NUM_FEATURES
    };

    struct SccSpsExt
    {
        mfxU8 scc_extension_flag                         : 1;
        mfxU8 curr_pic_ref_enabled_flag                  : 1;
        mfxU8 palette_mode_enabled_flag                  : 1;
        mfxU8 motion_vector_resolution_control_idc       : 2; // MBZ for Gen12
        mfxU8 intra_boundary_filtering_disabled_flag     : 1; // MBZ for Gen12
        mfxU8 palette_predictor_initializer_present_flag : 1; // MBZ for Gen12
        mfxU8 : 1;

        mfxU32 palette_max_size;                    // A.3.7: palette_max_size<=64
        mfxU32 delta_palette_max_predictor_size;    // A.3.7: palette_max_size+delta_palette_max_predictor_size <= 128
        mfxU32 num_palette_predictor_initializer_minus1;
        mfxU32 palette_predictor_initializers[3][128];
    };

    struct SccPpsExt
    {
        mfxU8 scc_extension_flag                                : 1;
        mfxU8 curr_pic_ref_enabled_flag                         : 1;
        mfxU8 residual_adaptive_colour_transform_enabled_flag   : 1;  // MBZ for Gen12
        mfxU8 palette_predictor_initializer_present_flag        : 1;   // MBZ for Gen12
        mfxU8 slice_act_qp_offsets_present_flag                 : 1;
        mfxU8 monochrome_palette_flag                           : 1;
        mfxU8 : 2;

        mfxU32 act_y_qp_offset_plus5;
        mfxU32 act_cb_qp_offset_plus5;
        mfxU32 act_cr_qp_offset_plus3;
        mfxU32 num_palette_predictor_initializer;
        mfxU32 luma_bit_depth_entry_minus8;
        mfxU32 chroma_bit_depth_entry_minus8;
        mfxU32 palette_predictor_initializers[3][128];
    };

    struct Glob
        : Gen11::Glob
    {
        static const StorageR::TKey _KD = __LINE__ + 1 - Gen11::Glob::NUM_KEYS;
        typedef StorageVar<__LINE__ - _KD, Gen12::SccSpsExt> SccSpsExt;
        typedef StorageVar<__LINE__ - _KD, Gen12::SccPpsExt> SccPpsExt;
        static const StorageR::TKey NUM_KEYS = __LINE__ - _KD;
    };


} //namespace Gen12
} //namespace HEVCEHW

#endif
