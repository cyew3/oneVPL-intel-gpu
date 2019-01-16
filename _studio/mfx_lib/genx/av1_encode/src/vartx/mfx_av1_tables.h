// Copyright (c) 2014-2019 Intel Corporation
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

#include "mfx_av1_defs.h"

namespace H265Enc {
    extern const uint8_t qindexMinus1[256];
    extern const uint8_t qindexPlus1[256];
    extern const int8_t h265_log2m2[257];
    extern const uint8_t h265_scan_z2r4[256];
    extern const uint8_t h265_scan_r2z4[256];
    extern const uint8_t h265_scan_left[256];
    extern const uint8_t h265_scan_above[256];
    extern const uint8_t h265_pgop_layers[PGOP_PIC_SIZE];
    extern const float h265_reci_1to116[16];

    __ALIGN2 struct MvRefLoc { uint8_t sbTag, absPartIdx; };
    extern const MvRefLoc mv_ref_locations_4x4[256][8];
    extern const MvRefLoc mv_ref_locations_4x8[128][8];
    extern const MvRefLoc mv_ref_locations_8x4[128][8];
    extern const MvRefLoc mv_ref_locations_8x8[64][8];
    extern const MvRefLoc mv_ref_locations_8x16[32][8];
    extern const MvRefLoc mv_ref_locations_16x8[32][8];
    extern const MvRefLoc mv_ref_locations_16x16[16][8];
    extern const MvRefLoc mv_ref_locations_16x32[8][8];
    extern const MvRefLoc mv_ref_locations_32x16[8][8];
    extern const MvRefLoc mv_ref_locations_32x32[4][8];
    extern const MvRefLoc mv_ref_locations_32x64[2][8];
    extern const MvRefLoc mv_ref_locations_64x32[2][8];
    extern const MvRefLoc mv_ref_locations_64x64[1][8];
    extern const MvRefLoc (*mv_ref_locations[BLOCK_SIZES])[8];


    static const int32_t SUBPEL_TAPS   = 8;
    typedef int16_t InterpKernel[SUBPEL_TAPS];
    extern const InterpKernel bilinear_filters[SUBPEL_SHIFTS];
    extern const InterpKernel sub_pel_filters_8[SUBPEL_SHIFTS];
    extern const InterpKernel sub_pel_filters_8s[SUBPEL_SHIFTS];
    extern const InterpKernel sub_pel_filters_8lp[SUBPEL_SHIFTS];
    extern const InterpKernel *vp9_filter_kernels[4];
    extern const InterpKernel *av1_filter_kernels[4];
    extern const InterpKernel *av1_short_filter_kernels[4];
    extern const uint8_t mode_to_angle_map[];
    extern const TxSize txsize_sqr_map[TX_SIZES_ALL];
    extern const TxSize txsize_sqr_up_map[TX_SIZES_ALL];
    extern const TxClass tx_type_to_class[TX_TYPES];
    extern const uint8_t txsize_log2_minus4[TX_SIZES_ALL];
    extern const int32_t ext_tx_cnt_intra[EXT_TX_SETS_INTRA];
    extern const int32_t av1_num_ext_tx_set[EXT_TX_SET_TYPES];
    extern const int32_t num_ext_tx_set[EXT_TX_SET_TYPES];
    extern const int32_t av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES];
    extern const int32_t av1_ext_tx_ind[EXT_TX_SET_TYPES][TX_TYPES];
    extern const int32_t av1_ext_tx_inter_ind[EXT_TX_SETS_INTER][TX_TYPES];
    extern const int32_t av1_ext_tx_intra_ind[EXT_TX_SETS_INTRA][TX_TYPES];
    extern const uint8_t hasTopRight[8][8];
    extern const uint8_t hasBelowLeft[8][8];
    extern const BlockSize subsize_lookup[EXT_PARTITION_TYPES][BLOCK_SIZES_ALL];
    extern const int32_t intra_mode_context[AV1_INTRA_MODES];
    extern const int8_t av1_nz_map_ctx_offset[TX_SIZES_ALL][5][5];
};
