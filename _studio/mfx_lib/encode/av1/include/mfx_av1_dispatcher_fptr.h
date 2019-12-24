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

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_defs.h"

namespace AV1PP {
    typedef void (* predict_intra_av1_fptr_t)(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int delta, int upTop, int upLeft);
    typedef void (* predict_intra_av1_hbd_fptr_t)(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int delta, int upTop, int upLeft);

    typedef void (* predict_intra_all_fptr_t)(const uint8_t *rec, int pitchRec, uint8_t *dst);
    typedef void (* predict_intra_palette_fptr_t)(uint8_t *dst, int pitch, const uint8_t *palette, const uint8_t *color_map);
    typedef void (* ftransform_fptr_t)(const int16_t *src, int16_t *dst, int pitchSrc);
    typedef void (* ftransform_hbd_fptr_t)(const int16_t *src, int32_t *dst, int pitchSrc);
    typedef void (* itransform_fptr_t)(const int16_t *src, int16_t *dst, int pitchDst, int bd);
    typedef void (* itransform_add_fptr_t)(const int16_t *src, uint8_t *dst, int pitchDst);
    typedef void (* itransform_add_hbd_fptr_t)(const int16_t *src, uint16_t *dst, int pitchDst);
    typedef void(*itransform_add_hbd_hbd_fptr_t)(const int32_t *src, uint16_t *dst, int pitchDst);

    typedef int (* quant_fptr_t)(const int16_t *src, int16_t *dst, const int16_t fqpar[8]);
    typedef int (* quant_hbd_fptr_t)(const int32_t *src, int16_t *dst, const int16_t fqpar[8]);
    typedef void (* dequant_fptr_t)(const int16_t *src, int16_t *dst, const int16_t scales[2], int bd);
    typedef void (* dequant_hbd_fptr_t)(const int16_t *src, int32_t *dst, const int16_t scales[2], int bd);
    typedef int (* quant_dequant_fptr_t)(int16_t *srcdst, int16_t *dst, const int16_t qpar[10]);
    typedef void (* average_fptr_t)(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h);
    typedef void (* average_pitch64_fptr_t)(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h);
    typedef void (* interp_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_av1_single_ref_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_av1_first_ref_fptr_t)(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_av1_second_ref_fptr_t)(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_single_ref_fptr_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_single_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, uint16_t *dst, const int16_t *fx, const int16_t *fy, int h);

    typedef void (* interp_pitch64_av1_first_ref_fptr_t)(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_first_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);

    typedef void (* interp_pitch64_av1_second_ref_fptr_t)(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    typedef void (* interp_pitch64_av1_second_ref_hbd_fptr_t)(const uint16_t *src, int pitchSrc, const int16_t *ref0, uint16_t *dst, const int16_t *fx, const int16_t *fy, int h);

    typedef void (* diff_nxn_fptr_t)(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, int16_t *dst, int pitchDst);
    typedef void(*diff_nxn_hbd_fptr_t)(const uint16_t *src1, int pitchSrc1, const uint16_t *src2, int pitchSrc2, int16_t *dst, int pitchDst);
    typedef void (* diff_nxn_p64_p64_pw_fptr_t)(const uint8_t *src1, const uint8_t *src2, int16_t *dst);
    typedef void(*diff_nxn_p64_p64_pw_hbd_fptr_t)(const uint16_t *src1, const uint16_t *src2, int16_t *dst);
    typedef void (* diff_nxm_fptr_t)(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, int16_t *dst, int pitchDst, int height);
    typedef void (* diff_nxm_p64_p64_pw_fptr_t)(const uint8_t *src1, const uint8_t *src2, int16_t *dst, int height);
    typedef void (* diff_nv12_fptr_t)(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, int16_t *dstU, int16_t *dstV, int pitchDst, int height);
    typedef void (* diff_nv12_hbd_fptr_t)(const uint16_t *src1, int pitchSrc1, const uint16_t *src2, int pitchSrc2, int16_t *dstU, int16_t *dstV, int pitchDst, int height);
    typedef void (* diff_nv12_p64_p64_pw_fptr_t)(const uint8_t *src1, const uint8_t *src2, int16_t *dstU, int16_t *dstV, int height);
    typedef void (* diff_nv12_p64_p64_pw_hbd_fptr_t)(const uint16_t *src1, const uint16_t *src2, int16_t *dstU, int16_t *dstV, int height);

    typedef int (* satd_fptr_t)(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2);
    typedef int (* satd_pitch64_fptr_t)(const uint8_t* src1, const uint8_t* src2, int pitch2);
    typedef int (* satd_pitch64_both_fptr_t)(const uint8_t* src1, const uint8_t* src2);
    typedef int (* satd_with_const_pitch64_fptr_t)(const uint8_t* src1, const uint8_t src2);
    typedef void (* satd_pair_fptr_t)(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int *satdPair);
    typedef void (* satd_pair_pitch64_fptr_t)(const uint8_t* src1, const uint8_t* src2, int pitch2, int *satdPair);
    typedef void (* satd_pair_pitch64_both_fptr_t)(const uint8_t* src1, const uint8_t* src2, int *satdPair);
    typedef int (* sse_fptr_t)(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    typedef int (* sse_hbd_fptr_t)(const uint16_t *src1, int pitch1, const uint16_t *src2, int pitch2);

    typedef int (* sse_p64_pw_fptr_t)(const uint8_t *src1, const uint8_t *src2);
    typedef void(*sse_p64_pw_uv_fptr_t)(const uint8_t *src1, const uint8_t *src2, int& costU, int& costV);
    typedef int (* sse_p64_p64_fptr_t)(const uint8_t *src1, const uint8_t *src2);
    typedef int(*sse_p64_p64_hbd_fptr_t)(const uint16_t *src1, const uint16_t *src2);
    typedef int (* sse_flexh_fptr_t)(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h);
    typedef int64_t (* sse_cont_fptr_t)(const int16_t *src1, const int16_t *src2, int len);
    typedef int64_t (* ssz_cont_fptr_t)(const int16_t *src1, int len);
    typedef int (* sad_special_fptr_t)(const uint8_t *src1, int pitch1, const uint8_t *src2);
    typedef int (* sad_general_fptr_t)(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    typedef int (* sad_store8x8_fptr_t)(const uint8_t *p1, const uint8_t *p2, int *sads8x8);
    typedef void (* lpf_fptr_t)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    typedef void (* lpf16_fptr_t)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    typedef int (* diff_dc_fptr_t)(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int size);
    typedef void (* adds_nv12_fptr_t)(uint8_t *dst, int pitchDst, const uint8_t *src1, int pitchSrc1, const int16_t *src2u, const int16_t *src2v, int pitchSrc2, int size);
    typedef void(*adds_nv12_hbd_fptr_t)(uint16_t *dst, int pitchDst, const uint16_t *src1, int pitchSrc1, const int16_t *src2u, const int16_t *src2v, int pitchSrc2, int size);
    typedef void (* compute_rscs_fptr_t)(const uint8_t *src, int pitchSrc, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height);
    typedef void (* compute16_rscs_fptr_t)(const uint16_t *src, int pitchSrc, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height);
    typedef void (* compute_rscs_4x4_fptr_t)(const uint8_t* pSrc, int srcPitch, int wblocks, int hblocks, float *pRs, float *pCs);
    typedef void (* compute_rscs_diff_fptr_t)(float *pRs0, float *pCs0, float *pRs1, float *pCs1, int len, float *pRsDiff, float *pCsDiff);
    typedef void (* search_best_block8x8_fptr_t)(uint8_t *pSrc, uint8_t *pRef, int pitch, int xrange, int yrange, uint32_t *bestSAD, int *bestX, int *bestY);
    typedef void (* diff_histogram_fptr_t)(uint8_t* src, uint8_t* ref, int pitch, int width, int height, int histogram[5], long long *srcDC, long long *refDC);
    typedef int (* cdef_find_dir_fptr_t)(const uint16_t *img, int stride, int *var, int coeff_shift);
    typedef void (* cdef_filter_block_u8_fptr_t)(uint8_t *dst, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping);
    typedef void (* cdef_filter_block_u16_fptr_t)(uint16_t *dst, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping);
    typedef void (* cdef_estimate_block_fptr_t)(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, uint8_t *dst, int dstride);
    typedef void (* cdef_estimate_block_hbd_fptr_t)(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, uint16_t *dst, int dstride);

    typedef void (* cdef_estimate_block_sec0_fptr_t)(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, uint8_t *dst, int dstride);
    typedef void (* cdef_estimate_block_sec0_hbd_fptr_t)(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, uint16_t *dst, int dstride);

    typedef void (* cdef_estimate_block_pri0_fptr_t)(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse);
    typedef void (* cdef_estimate_block_all_sec_fptr_t)(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse,
        uint8_t **dst, int dstride);
    typedef void (* cdef_estimate_block_all_sec_hbd_fptr_t)(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse,
        uint16_t **dst, int dstride);

    typedef void (* cdef_estimate_block_2pri_fptr_t)(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1, int dir, int pri_damping, int sec_damping, int *sse);

    //cfl functions
    typedef void(*cfl_subsample_u8_fptr_t)(const uint8_t *input, int input_stride, uint16_t *output_q3);
    typedef void(*cfl_subsample_u16_fptr_t)(const uint16_t *input, int input_stride, uint16_t *output_q3);
    typedef void(*cfl_subtract_average_fptr_t)(const uint16_t *src, int16_t *dst);
    typedef void(*cfl_predict_nv12_u8_fptr_t)(const int16_t *pred_buf_q3, uint8_t *dst, int dst_stride, uint8_t dcU, uint8_t dcV, int alpha_q3, int bd);
    typedef void(*cfl_predict_nv12_u16_fptr_t)(const int16_t *pred_buf_q3, uint16_t *dst, int dst_stride, uint16_t dcU, uint16_t dcV, int alpha_q3, int bd);

    extern predict_intra_av1_fptr_t predict_intra_av1_fptr_arr[4][2][2][13]; // [txSize] [haveLeft] [haveAbove] [mode]
    extern predict_intra_av1_hbd_fptr_t predict_intra_av1_hbd_fptr_arr[4][2][2][13]; // [txSize] [haveLeft] [haveAbove] [mode]
    extern predict_intra_av1_fptr_t predict_intra_nv12_av1_fptr_arr[4][2][2][13]; // [txSize] [haveLeft] [haveAbove] [mode]
    extern predict_intra_av1_hbd_fptr_t predict_intra_nv12_av1_hbd_fptr_arr[4][2][2][13]; // [txSize] [haveLeft] [haveAbove] [mode]

    extern predict_intra_all_fptr_t predict_intra_all_fptr_arr[4][2][2]; // [txSize] [haveLeft] [haveAbove]
    extern predict_intra_palette_fptr_t predict_intra_palette_fptr_arr[4]; // [txSize]

    extern ftransform_fptr_t ftransform_vp9_fptr_arr[4][4]; // [txSize] [txType]
    extern ftransform_fptr_t ftransform_av1_fptr_arr[4][10]; // [txSize] [txType]
    extern ftransform_hbd_fptr_t ftransform_av1_hbd_fptr_arr[4][10]; // [txSize] [txType]
    extern ftransform_fptr_t ftransform_fast32x32_fptr;
    extern itransform_fptr_t itransform_vp9_fptr_arr[4][4]; // [txSize] [txType]
    extern itransform_fptr_t itransform_av1_fptr_arr[4][10]; // [txSize] [txType]
    extern itransform_add_fptr_t itransform_add_vp9_fptr_arr[4][4]; // [txSize] [txType]
    extern itransform_add_fptr_t itransform_add_av1_fptr_arr[4][10]; // [txSize] [txType]
    extern itransform_add_hbd_fptr_t itransform_add_av1_hbd_fptr_arr[4][10]; // [txSize] [txType]
    extern itransform_add_hbd_hbd_fptr_t itransform_add_av1_hbd_hbd_fptr_arr[4][10]; // [txSize] [txType]

    extern quant_fptr_t quant_fptr_arr[4]; // [txSize]
    extern quant_hbd_fptr_t quant_hbd_fptr_arr[4]; // [txSize]
    extern dequant_fptr_t dequant_fptr_arr[4]; // [txSize]
    extern dequant_hbd_fptr_t dequant_hbd_fptr_arr[4]; // [txSize]
    extern quant_dequant_fptr_t quant_dequant_fptr_arr[4]; // [txSize]
    extern average_fptr_t average_fptr_arr[5]; // [log2(width/4)]
    extern average_pitch64_fptr_t average_pitch64_fptr_arr[5]; // [log2(width/4)]
    extern interp_fptr_t interp_fptr_arr[5][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    extern interp_fptr_t interp_fptr_arr_ext[6][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    extern interp_fptr_t interp_nv12_fptr_arr[4][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    extern interp_pitch64_fptr_t interp_pitch64_fptr_arr[5][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    extern interp_pitch64_fptr_t interp_nv12_pitch64_fptr_arr[4][2][2][2]; // [log2(width/4)] [dx!=0] [dy!=0] [avg]
    extern interp_av1_single_ref_fptr_t (*interp_av1_single_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_av1_first_ref_fptr_t  (*interp_av1_first_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_av1_second_ref_fptr_t (*interp_av1_second_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_av1_single_ref_fptr_t (*interp_nv12_av1_single_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_av1_first_ref_fptr_t  (*interp_nv12_av1_first_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_av1_second_ref_fptr_t (*interp_nv12_av1_second_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_single_ref_fptr_t (*interp_pitch64_av1_single_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_single_ref_hbd_fptr_t(*interp_pitch64_av1_single_ref_hbd_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_first_ref_fptr_t  (*interp_pitch64_av1_first_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_first_ref_hbd_fptr_t  (*interp_pitch64_av1_first_ref_hbd_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]

    extern interp_pitch64_av1_second_ref_fptr_t (*interp_pitch64_av1_second_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_second_ref_hbd_fptr_t (*interp_pitch64_av1_second_ref_hbd_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_single_ref_fptr_t (*interp_nv12_pitch64_av1_single_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_single_ref_hbd_fptr_t(*interp_nv12_pitch64_av1_single_ref_hbd_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_first_ref_fptr_t  (*interp_nv12_pitch64_av1_first_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_first_ref_hbd_fptr_t  (*interp_nv12_pitch64_av1_first_ref_hbd_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_second_ref_fptr_t (*interp_nv12_pitch64_av1_second_ref_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_pitch64_av1_second_ref_hbd_fptr_t (*interp_nv12_pitch64_av1_second_ref_hbd_fptr_arr)[2][2]; // [log2(width/4)] [dx!=0] [dy!=0]
    extern interp_av1_single_ref_fptr_t interp_av1_single_ref_fptr_arr_px[5][2][2];
    extern interp_av1_first_ref_fptr_t interp_av1_first_ref_fptr_arr_px[5][2][2];
    extern interp_av1_second_ref_fptr_t interp_av1_second_ref_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_single_ref_fptr_t interp_pitch64_av1_single_ref_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_single_ref_hbd_fptr_t interp_pitch64_av1_single_ref_hbd_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_first_ref_fptr_t interp_pitch64_av1_first_ref_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_first_ref_hbd_fptr_t interp_pitch64_av1_first_ref_hbd_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_second_ref_fptr_t interp_pitch64_av1_second_ref_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_second_ref_hbd_fptr_t interp_pitch64_av1_second_ref_hbd_fptr_arr_px[5][2][2];
    extern interp_av1_single_ref_fptr_t interp_nv12_av1_single_ref_fptr_arr_px[5][2][2];
    extern interp_av1_first_ref_fptr_t interp_nv12_av1_first_ref_fptr_arr_px[5][2][2];
    extern interp_av1_second_ref_fptr_t interp_nv12_av1_second_ref_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_single_ref_fptr_t interp_nv12_pitch64_av1_single_ref_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_single_ref_hbd_fptr_t interp_nv12_pitch64_av1_single_ref_hbd_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_first_ref_fptr_t interp_nv12_pitch64_av1_first_ref_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_first_ref_hbd_fptr_t interp_nv12_pitch64_av1_first_ref_hbd_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_second_ref_fptr_t interp_nv12_pitch64_av1_second_ref_fptr_arr_px[5][2][2];
    extern interp_pitch64_av1_second_ref_hbd_fptr_t interp_nv12_pitch64_av1_second_ref_hbd_fptr_arr_px[5][2][2];

    extern interp_av1_single_ref_fptr_t interp_av1_single_ref_fptr_arr_avx2[6][2][2];
    extern interp_av1_first_ref_fptr_t interp_av1_first_ref_fptr_arr_avx2[5][2][2];
    extern interp_av1_second_ref_fptr_t interp_av1_second_ref_fptr_arr_avx2[5][2][2];
    extern interp_pitch64_av1_single_ref_fptr_t interp_pitch64_av1_single_ref_fptr_arr_avx2[5][2][2];
    extern interp_pitch64_av1_first_ref_fptr_t interp_pitch64_av1_first_ref_fptr_arr_avx2[5][2][2];
    extern interp_pitch64_av1_second_ref_fptr_t interp_pitch64_av1_second_ref_fptr_arr_avx2[5][2][2];
    extern interp_av1_single_ref_fptr_t interp_nv12_av1_single_ref_fptr_arr_avx2[5][2][2];
    extern interp_av1_first_ref_fptr_t interp_nv12_av1_first_ref_fptr_arr_avx2[5][2][2];
    extern interp_av1_second_ref_fptr_t interp_nv12_av1_second_ref_fptr_arr_avx2[5][2][2];
    extern interp_pitch64_av1_single_ref_fptr_t interp_nv12_pitch64_av1_single_ref_fptr_arr_avx2[5][2][2];
    extern interp_pitch64_av1_first_ref_fptr_t interp_nv12_pitch64_av1_first_ref_fptr_arr_avx2[5][2][2];
    extern interp_pitch64_av1_second_ref_fptr_t interp_nv12_pitch64_av1_second_ref_fptr_arr_avx2[5][2][2];

    extern diff_nxn_fptr_t diff_nxn_fptr_arr[5]; // [log2(width/4)]
    extern diff_nxn_hbd_fptr_t diff_nxn_hbd_fptr_arr[5]; // [log2(width/4)]

    extern diff_nxn_p64_p64_pw_fptr_t diff_nxn_p64_p64_pw_fptr_arr[5]; // [log2(width/4)]
    extern diff_nxn_p64_p64_pw_hbd_fptr_t diff_nxn_p64_p64_pw_hbd_fptr_arr[5]; // [log2(width/4)]
    extern diff_nxm_fptr_t diff_nxm_fptr_arr[5]; // [log2(width/4)]
    extern diff_nxm_p64_p64_pw_fptr_t diff_nxm_p64_p64_pw_fptr_arr[5]; // [log2(width/4)]
    extern diff_nv12_fptr_t diff_nv12_fptr_arr[4]; // [log2(width/4)]
    extern diff_nv12_hbd_fptr_t diff_nv12_hbd_fptr_arr[4]; // [log2(width/4)]
    extern diff_nv12_p64_p64_pw_fptr_t diff_nv12_p64_p64_pw_fptr_arr[4]; // [log2(width/4)]
    extern diff_nv12_p64_p64_pw_hbd_fptr_t diff_nv12_p64_p64_pw_hbd_fptr_arr[4]; // [log2(width/4)]

    extern satd_fptr_t satd_4x4_fptr;
    extern satd_pitch64_fptr_t satd_4x4_pitch64_fptr;
    extern satd_pitch64_both_fptr_t satd_4x4_pitch64_both_fptr;
    extern satd_fptr_t satd_8x8_fptr;
    extern satd_pitch64_fptr_t satd_8x8_pitch64_fptr;
    extern satd_pitch64_both_fptr_t satd_8x8_pitch64_both_fptr;
    extern satd_pair_fptr_t satd_4x4_pair_fptr;
    extern satd_pair_pitch64_fptr_t satd_4x4_pair_pitch64_fptr;
    extern satd_pair_pitch64_both_fptr_t satd_4x4_pair_pitch64_both_fptr;
    extern satd_pair_fptr_t satd_8x8_pair_fptr;
    extern satd_pair_pitch64_fptr_t satd_8x8_pair_pitch64_fptr;
    extern satd_pair_pitch64_both_fptr_t satd_8x8_pair_pitch64_both_fptr;
    extern satd_pair_fptr_t satd_8x8x2_fptr;
    extern satd_pair_pitch64_fptr_t satd_8x8x2_pitch64_fptr;
    extern satd_fptr_t satd_fptr_arr[5][5];
    extern satd_pitch64_fptr_t satd_pitch64_fptr_arr[5][5];
    extern satd_pitch64_both_fptr_t satd_pitch64_both_fptr_arr[5][5];
    extern satd_with_const_pitch64_fptr_t satd_with_const_pitch64_fptr_arr[5][5];
    extern sse_fptr_t sse_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    extern sse_hbd_fptr_t sse_hbd_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]

    extern sse_p64_pw_fptr_t sse_p64_pw_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    extern sse_p64_pw_uv_fptr_t sse_p64_pw_uv_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    extern sse_p64_p64_fptr_t sse_p64_p64_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    extern sse_p64_p64_hbd_fptr_t sse_p64_p64_hbd_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    extern sse_flexh_fptr_t sse_flexh_fptr_arr[5]; // [log2(width/4)]
    extern sse_cont_fptr_t sse_cont_fptr/*_arr[5]*/; // [log2(width/4)]
    extern ssz_cont_fptr_t ssz_cont_fptr/*_arr[5]*/; // [log2(width/4)]
    extern sad_special_fptr_t sad_special_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    extern sad_general_fptr_t sad_general_fptr_arr[5][5]; // [log2(width/4)] [log2(height/4)]
    extern sad_store8x8_fptr_t sad_store8x8_fptr_arr[5]; // [log2(width/4)], first pointer is null

    extern lpf_fptr_t lpf_fptr_arr[2][4];  // [edge_dir][filter_length_idx]
    extern lpf16_fptr_t lpf_hbd_fptr_arr[2][4];  // [edge_dir][filter_length_idx]

    extern diff_dc_fptr_t diff_dc_fptr;
    extern adds_nv12_fptr_t adds_nv12_fptr;
    extern adds_nv12_hbd_fptr_t adds_nv12_hbd_fptr;
    extern search_best_block8x8_fptr_t search_best_block8x8_fptr;
    extern compute_rscs_fptr_t compute_rscs_fptr;
    extern compute_rscs_4x4_fptr_t compute_rscs_4x4_fptr;
    extern compute_rscs_diff_fptr_t compute_rscs_diff_fptr;
    extern diff_histogram_fptr_t diff_histogram_fptr;
    extern cdef_find_dir_fptr_t cdef_find_dir_fptr;
    extern cdef_filter_block_u8_fptr_t cdef_filter_block_u8_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_filter_block_u16_fptr_t cdef_filter_block_u16_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_estimate_block_fptr_t cdef_estimate_block_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_estimate_block_hbd_fptr_t cdef_estimate_block_hbd_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_estimate_block_sec0_fptr_t cdef_estimate_block_sec0_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_estimate_block_sec0_hbd_fptr_t cdef_estimate_block_sec0_hbd_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_estimate_block_pri0_fptr_t cdef_estimate_block_pri0_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_estimate_block_all_sec_fptr_t cdef_estimate_block_all_sec_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_estimate_block_all_sec_hbd_fptr_t cdef_estimate_block_all_sec_hbd_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12
    extern cdef_estimate_block_2pri_fptr_t cdef_estimate_block_2pri_fptr_arr[2]; // 8x8 luma and 4x4 chroma nv12

    extern cfl_subsample_u8_fptr_t cfl_subsample_420_u8_fptr_arr[AV1Enc::TX_SIZES_ALL];
    extern cfl_subsample_u16_fptr_t cfl_subsample_420_u16_fptr_arr[AV1Enc::TX_SIZES_ALL];
    extern cfl_subtract_average_fptr_t cfl_subtract_average_fptr_arr[AV1Enc::TX_SIZES_ALL];

    extern cfl_predict_nv12_u8_fptr_t cfl_predict_nv12_u8_fptr_arr[AV1Enc::TX_SIZES_ALL];
    extern cfl_predict_nv12_u16_fptr_t cfl_predict_nv12_u16_fptr_arr[AV1Enc::TX_SIZES_ALL];
};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
