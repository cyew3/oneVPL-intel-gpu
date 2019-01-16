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

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_dispatcher_fptr.h"

namespace AV1PP {
    predict_intra_vp9_fptr_t predict_intra_vp9_fptr_arr[4][2][2][10]; // [txSize] [haveLeft] [haveAbove] [mode]
    predict_intra_av1_fptr_t predict_intra_av1_fptr_arr[4][2][2][13]; // [txSize] [haveLeft] [haveAbove] [mode]

    predict_intra_vp9_fptr_t predict_intra_nv12_vp9_fptr_arr[4][2][2][10]; // [txSize] [haveLeft] [haveAbove] [mode]
    predict_intra_av1_fptr_t predict_intra_nv12_av1_fptr_arr[4][2][2][13]; // [txSize] [haveLeft] [haveAbove] [mode]

    predict_intra_all_fptr_t predict_intra_all_fptr_arr[4][2][2]; // [txSize] [haveLeft] [haveAbove]
    predict_intra_all_fptr_t predict_intra_nv12_all_fptr_arr[4][2][2]; // [txSize] [haveLeft] [haveAbove]
    pick_intra_nv12_fptr_t pick_intra_nv12_fptr_arr[4][2][2]; // [txSize] [haveLeft] [haveAbove]

    ftransform_fptr_t ftransform_vp9_fptr_arr[4][4]; // [txSize] [txType]
    ftransform_fptr_t ftransform_av1_fptr_arr[4][4]; // [txSize] [txType]
    ftransform_fptr_t ftransform_fast32x32_fptr;
    itransform_fptr_t itransform_vp9_fptr_arr[4][4]; // [txSize] [txType]
    itransform_fptr_t itransform_av1_fptr_arr[4][4]; // [txSize] [txType]
    itransform_add_fptr_t itransform_add_vp9_fptr_arr[4][4]; // [txSize] [txType]
    itransform_add_fptr_t itransform_add_av1_fptr_arr[4][4]; // [txSize] [txType]
    quant_fptr_t quant_fptr_arr[4]; // [txSize]
    dequant_fptr_t dequant_fptr_arr[4]; // [txSize]
    quant_dequant_fptr_t quant_dequant_fptr_arr[4]; // [txSize]
    average_fptr_t average_fptr_arr[5]; // [log2(width/4)]
    average_pitch64_fptr_t average_pitch64_fptr_arr[5]; // [log2(width/4)]
    interp_fptr_t interp_fptr_arr[5][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    interp_fptr_t interp_fptr_arr_ext[6][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg] - for non standard width
    interp_fptr_t interp_nv12_fptr_arr[4][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    interp_pitch64_fptr_t interp_pitch64_fptr_arr[5][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    interp_pitch64_fptr_t interp_nv12_pitch64_fptr_arr[4][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    interp_av1_single_ref_fptr_t (*interp_av1_single_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_av1_first_ref_fptr_t  (*interp_av1_first_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_av1_second_ref_fptr_t (*interp_av1_second_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_av1_single_ref_fptr_t (*interp_nv12_av1_single_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_av1_first_ref_fptr_t  (*interp_nv12_av1_first_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_av1_second_ref_fptr_t (*interp_nv12_av1_second_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_pitch64_av1_single_ref_fptr_t (*interp_pitch64_av1_single_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_pitch64_av1_first_ref_fptr_t  (*interp_pitch64_av1_first_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_pitch64_av1_second_ref_fptr_t (*interp_pitch64_av1_second_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_pitch64_av1_single_ref_fptr_t (*interp_nv12_pitch64_av1_single_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_pitch64_av1_first_ref_fptr_t  (*interp_nv12_pitch64_av1_first_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    interp_pitch64_av1_second_ref_fptr_t (*interp_nv12_pitch64_av1_second_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    satd_fptr_t satd_4x4_fptr;
    satd_pitch64_fptr_t satd_4x4_pitch64_fptr;
    satd_pitch64_both_fptr_t satd_4x4_pitch64_both_fptr;
    satd_fptr_t satd_8x8_fptr;
    satd_pitch64_fptr_t satd_8x8_pitch64_fptr;
    satd_pitch64_both_fptr_t satd_8x8_pitch64_both_fptr;
    satd_pair_fptr_t satd_4x4_pair_fptr;
    satd_pair_pitch64_fptr_t satd_4x4_pair_pitch64_fptr;
    satd_pair_pitch64_both_fptr_t satd_4x4_pair_pitch64_both_fptr;
    satd_pair_fptr_t satd_8x8_pair_fptr;
    satd_pair_pitch64_fptr_t satd_8x8_pair_pitch64_fptr;
    satd_pair_pitch64_both_fptr_t satd_8x8_pair_pitch64_both_fptr;
    satd_pair_fptr_t satd_8x8x2_fptr;
    satd_pair_pitch64_fptr_t satd_8x8x2_pitch64_fptr;
    satd_fptr_t satd_fptr_arr[5][5];
    satd_pitch64_fptr_t satd_pitch64_fptr_arr[5][5];
    satd_pitch64_both_fptr_t satd_pitch64_both_fptr_arr[5][5];
    diff_nxn_fptr_t diff_nxn_fptr_arr[5]; // [log2(width/4)]
    diff_nxn_p64_p64_pw_fptr_t diff_nxn_p64_p64_pw_fptr_arr[5]; // [log2(width/4)]
    diff_nxm_fptr_t diff_nxm_fptr_arr[5]; // [log2(width/4)]
    diff_nxm_p64_p64_pw_fptr_t diff_nxm_p64_p64_pw_fptr_arr[5]; // [log2(width/4)]
    diff_nv12_fptr_t diff_nv12_fptr_arr[4]; // [log2(width/4)]
    diff_nv12_p64_p64_pw_fptr_t diff_nv12_p64_p64_pw_fptr_arr[4]; // [log2(width/4)]
    sse_fptr_t sse_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    sse_p64_pw_fptr_t sse_p64_pw_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    sse_p64_p64_fptr_t sse_p64_p64_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    sse_flexh_fptr_t sse_flexh_fptr_arr[5]; // [log2(width/4)]
    sse_cont_fptr_t sse_cont_fptr/*_arr[5]*/; // [log2(width/4)]
    ssz_cont_fptr_t ssz_cont_fptr/*_arr[5]*/; // [log2(width/4)]
    sad_special_fptr_t sad_special_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    sad_general_fptr_t sad_general_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    sad_store8x8_fptr_t sad_store8x8_fptr_arr[5]; // [log2(width/4)], first pointer is null
    lpf_fptr_t lpf_horizontal_4_fptr;
    lpf_dual_fptr_t lpf_horizontal_4_dual_fptr;
    lpf_fptr_t lpf_horizontal_8_fptr;
    lpf_dual_fptr_t lpf_horizontal_8_dual_fptr;
    lpf_fptr_t lpf_horizontal_edge_16_fptr;
    lpf_fptr_t lpf_horizontal_edge_8_fptr;
    lpf_fptr_t lpf_vertical_16_fptr;
    lpf_fptr_t lpf_vertical_16_dual_fptr;
    lpf_fptr_t lpf_vertical_4_fptr;
    lpf_dual_fptr_t lpf_vertical_4_dual_fptr;
    lpf_fptr_t lpf_vertical_8_fptr;
    lpf_dual_fptr_t lpf_vertical_8_dual_fptr;
    lpf16_fptr_t lpf16_horizontal_4_fptr;
    lpf16_dual_fptr_t lpf16_horizontal_4_dual_fptr;
    lpf16_fptr_t lpf16_horizontal_8_fptr;
    lpf16_dual_fptr_t lpf16_horizontal_8_dual_fptr;
    lpf16_fptr_t lpf16_horizontal_edge_16_fptr;
    lpf16_fptr_t lpf16_horizontal_edge_8_fptr;
    lpf16_fptr_t lpf16_vertical_16_fptr;
    lpf16_fptr_t lpf16_vertical_16_dual_fptr;
    lpf16_fptr_t lpf16_vertical_4_fptr;
    lpf16_dual_fptr_t lpf16_vertical_4_dual_fptr;
    lpf16_fptr_t lpf16_vertical_8_fptr;
    lpf16_dual_fptr_t lpf16_vertical_8_dual_fptr;
    lpf_fptr_t lpf_fptr_arr[2][4];  // [edge_dir][filter_length_idx]
    diff_dc_fptr_t diff_dc_fptr;
    adds_nv12_fptr_t adds_nv12_fptr;
    search_best_block8x8_fptr_t search_best_block8x8_fptr;
    compute_rscs_fptr_t compute_rscs_fptr;
    compute_rscs_4x4_fptr_t compute_rscs_4x4_fptr;
    compute_rscs_diff_fptr_t compute_rscs_diff_fptr;
    diff_histogram_fptr_t diff_histogram_fptr;
    cdef_find_dir_fptr_t cdef_find_dir_fptr;
    cdef_filter_block_u8_fptr_t cdef_filter_block_u8_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    cdef_filter_block_u16_fptr_t cdef_filter_block_u16_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    cdef_estimate_block_fptr_t cdef_estimate_block_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    cdef_estimate_block_sec0_fptr_t cdef_estimate_block_sec0_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    cdef_estimate_block_pri0_fptr_t cdef_estimate_block_pri0_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    cdef_estimate_block_all_sec_fptr_t cdef_estimate_block_all_sec_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    cdef_estimate_block_2pri_fptr_t cdef_estimate_block_2pri_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
