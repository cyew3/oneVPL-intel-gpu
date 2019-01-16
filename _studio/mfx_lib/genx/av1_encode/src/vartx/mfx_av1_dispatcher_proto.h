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

namespace VP9PP {
    // forward transform
    template <int size, int type> void ftransform_vp9_px  (const int16_t *src, int16_t *dst, int pitchSrc);
    template <int size, int type> void ftransform_vp9_sse2(const int16_t *src, int16_t *dst, int pitchSrc);
    template <int size, int type> void ftransform_vp9_avx2(const int16_t *src, int16_t *dst, int pitchSrc);
    void fdct32x32_fast_sse2(const int16_t *src, int16_t *dst, int pitchSrc);
    void fdct32x32_fast_avx2(const int16_t *src, int16_t *dst, int pitchSrc);
    template <int size, int type> void ftransform_av1_px  (const int16_t *src, int16_t *dst, int pitchSrc);
    template <int size, int type> void ftransform_av1_avx2(const int16_t *src, int16_t *dst, int pitchSrc);
    // inverse transform
    template <int size, int type> void itransform_vp9_px  (const int16_t *src, int16_t *dst, int pitchDst);
    template <int size, int type> void itransform_vp9_sse2(const int16_t *src, int16_t *dst, int pitchDst);
    template <int size, int type> void itransform_vp9_avx2(const int16_t *src, int16_t *dst, int pitchDst);
    template <int size, int type> void itransform_add_vp9_px  (const int16_t *src, uint8_t *dst, int pitchDst);
    template <int size, int type> void itransform_add_vp9_sse2(const int16_t *src, uint8_t *dst, int pitchDst);
    template <int size, int type> void itransform_add_vp9_avx2(const int16_t *src, uint8_t *dst, int pitchDst);
    template <int size, int type> void itransform_av1_px  (const int16_t *src, int16_t *dst, int pitchDst);
    template <int size, int type> void itransform_av1_avx2(const int16_t *src, int16_t *dst, int pitchDst);
    template <int size, int type> void itransform_add_av1_px  (const int16_t *src, uint8_t *dst, int pitchDst);
    template <int size, int type> void itransform_add_av1_avx2(const int16_t *src, uint8_t *dst, int pitchDst);
    // quantization
    template <int size> int quant_px  (const int16_t *src, int16_t *dst, const int16_t fqpar[8]);
    template <int size> int quant_avx2(const int16_t *src, int16_t *dst, const int16_t fqpar[8]);
    template <int size> void dequant_px  (const int16_t *src, int16_t *dst, const int16_t *scales);
    template <int size> void dequant_avx2(const int16_t *src, int16_t *dst, const int16_t *scales);
    template <int size> int quant_dequant_px  (int16_t *coef, int16_t *qcoef, const int16_t qpar[10]);
    template <int size> int quant_dequant_avx2(int16_t *coef, int16_t *qcoef, const int16_t qpar[10]);
    // average (x + y + 1) >> 1
    template <int w> void average_px  (const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h);
    template <int w> void average_avx2(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h);
    template <int w> void average_pitch64_px  (const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h);
    template <int w> void average_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h);
    // luma inter prediction
    template <int w, int horz, int vert, int avg> void interp_px   (const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert, int avg> void interp_ssse3(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert, int avg> void interp_avx2 (const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert, int avg> void interp_pitch64_px   (const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert, int avg> void interp_pitch64_avx2 (const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int horz, int vert, int avg> void interp_flex_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h);
    // luma inter prediction av1
    template <int w, int horz, int vert> void interp_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_av1_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_av1_avx2(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_av1_avx2(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_pitch64_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    // nv12 inter prediction
    template <int w, int horz, int vert, int avg> void interp_nv12_px  (const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert, int avg> void interp_nv12_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert, int avg> void interp_nv12_pitch64_px  (const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert, int avg> void interp_nv12_pitch64_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    // nv12 inter prediction av1
    template <int w, int horz, int vert> void interp_nv12_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_av1_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_av1_avx2(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_av1_avx2(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_pitch64_av1_px(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_pitch64_av1_px(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_pitch64_av1_px(const uint8_t *src, int pitchSrc, const int16_t *ref, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, int16_t *dst, const int16_t *fx, const int16_t *fy, int h);
    template <int w, int horz, int vert> void interp_nv12_pitch64_av1_avx2(const uint8_t *src, int pitchSrc, const int16_t *ref, uint8_t *dst, const int16_t *fx, const int16_t *fy, int h);
    // diff
    template <int w> void diff_nxn_px  (const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff);
    template <int w> void diff_nxn_avx2(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff);
    template <int w> void diff_nxn_p64_p64_pw_px  (const uint8_t *src, const uint8_t *pred, int16_t *diff);
    template <int w> void diff_nxn_p64_p64_pw_avx2(const uint8_t *src, const uint8_t *pred, int16_t *diff);
    template <int w> void diff_nxm_px  (const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff, int height);
    template <int w> void diff_nxm_avx2(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *diff, int pitchDiff, int height);
    template <int w> void diff_nxm_p64_p64_pw_px  (const uint8_t *src, const uint8_t *pred, int16_t *diff, int height);
    template <int w> void diff_nxm_p64_p64_pw_avx2(const uint8_t *src, const uint8_t *pred, int16_t *diff, int height);
    template <int w> void diff_nv12_px  (const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *dstU, int16_t *dstV, int pitchDst, int height);
    template <int w> void diff_nv12_avx2(const uint8_t *src, int pitchSrc, const uint8_t *pred, int pitchPred, int16_t *dstU, int16_t *dstV, int pitchDst, int height);
    template <int w> void diff_nv12_p64_p64_pw_px  (const uint8_t *src, const uint8_t *pred, int16_t *dstU, int16_t *dstV, int height);
    template <int w> void diff_nv12_p64_p64_pw_avx2(const uint8_t *src, const uint8_t *pred, int16_t *dstU, int16_t *dstV, int height);
    // satd
    int satd_4x4_px  (const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    int satd_4x4_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    int satd_4x4_pitch64_px  (const uint8_t *src1, const uint8_t *src2, int pitch2);
    int satd_4x4_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, int pitch2);
    int satd_4x4_pitch64_both_px  (const uint8_t *src1, const uint8_t *src2);
    int satd_4x4_pitch64_both_avx2(const uint8_t *src1, const uint8_t *src2);
    int satd_8x8_px  (const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    int satd_8x8_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    int satd_8x8_pitch64_px  (const uint8_t *src1, const uint8_t *src2, int pitch2);
    int satd_8x8_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, int pitch2);
    int satd_8x8_pitch64_both_px  (const uint8_t *src1, const uint8_t *src2);
    int satd_8x8_pitch64_both_avx2(const uint8_t *src1, const uint8_t *src2);
    void satd_4x4_pair_px  (const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int *satdPair);
    void satd_4x4_pair_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_4x4_pair_pitch64_px  (const uint8_t *src1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_4x4_pair_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_4x4_pair_pitch64_both_px  (const uint8_t *src1, const uint8_t *src2, int *satdPair);
    void satd_4x4_pair_pitch64_both_avx2(const uint8_t *src1, const uint8_t *src2, int *satdPair);
    void satd_8x8_pair_px  (const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int *satdPair);
    void satd_8x8_pair_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_8x8_pair_pitch64_px  (const uint8_t *src1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_8x8_pair_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_8x8_pair_pitch64_both_px  (const uint8_t *src1, const uint8_t *src2, int *satdPair);
    void satd_8x8_pair_pitch64_both_avx2(const uint8_t *src1, const uint8_t *src2, int *satdPair);
    void satd_8x8x2_px  (const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int *satdPair);
    void satd_8x8x2_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_8x8x2_pitch64_px  (const uint8_t *src1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_8x8x2_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, int pitch2, int *satdPair);
    void satd_8x8x2_pitch64_both_px  (const uint8_t *src1, const uint8_t *src2, int *satdPair);
    void satd_8x8x2_pitch64_both_avx2(const uint8_t *src1, const uint8_t *src2, int *satdPair);
    template <int w, int h> int satd_px  (const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2);
    template <int w, int h> int satd_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2);
    template <int w, int h> int satd_pitch64_px  (const uint8_t* src1, const uint8_t* src2, int pitch2);
    template <int w, int h> int satd_pitch64_avx2(const uint8_t* src1, const uint8_t* src2, int pitch2);
    template <int w, int h> int satd_pitch64_both_px  (const uint8_t* src1, const uint8_t* src2);
    template <int w, int h> int satd_pitch64_both_avx2(const uint8_t* src1, const uint8_t* src2);
    // sse
    template <int w, int h> int sse_px  (const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    template <int w, int h> int sse_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    template <int w, int h> int sse_p64_pw_px  (const uint8_t *src1, const uint8_t *src2);
    template <int w, int h> int sse_p64_pw_avx2(const uint8_t *src1, const uint8_t *src2);
    template <int w, int h> int sse_p64_p64_px  (const uint8_t *src1, const uint8_t *src2);
    template <int w, int h> int sse_p64_p64_avx2(const uint8_t *src1, const uint8_t *src2);
    template <int w> int sse_px  (const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h);
    template <int w> int sse_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h);
    /*template <int w> */int64_t sse_cont_px  (const int16_t *src1, const int16_t *src2, int len);
    /*template <int w> */int64_t sse_cont_avx2(const int16_t *src1, const int16_t *src2, int len);
    /*template <int w> */int64_t ssz_cont_px  (const int16_t *src1, int len);
    /*template <int w> */int64_t ssz_cont_avx2(const int16_t *src1, int len);
    // sad
    template <int w, int h> int sad_special_px  (const uint8_t *src1, int pitch1, const uint8_t *src2);
    template <int w, int h> int sad_special_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2);
    template <int w, int h> int sad_general_px  (const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    template <int w, int h> int sad_general_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
    template <int size> int sad_store8x8_px  (const uint8_t *p1, const uint8_t *p2, int *sads8x8);
    template <int size> int sad_store8x8_avx2(const uint8_t *p1, const uint8_t *p2, int *sads8x8);
    // deblocking
    void lpf_vertical_4_8u_av1_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_4_8u_av1_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_6_8u_av1_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_6_8u_av1_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_8_8u_av1_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_8_8u_av1_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_14_8u_av1_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_14_8u_av1_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_4_8u_av1_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_4_8u_av1_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_6_8u_av1_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_6_8u_av1_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_8_8u_av1_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_8_8u_av1_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_14_8u_av1_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_14_8u_av1_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    void lpf_horizontal_4_8u_px  (uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_4_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_4_dual_8u_px(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
    void lpf_horizontal_4_dual_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
    void lpf_horizontal_8_8u_px(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_8_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_8_dual_8u_px(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
    void lpf_horizontal_8_dual_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
    void lpf_horizontal_edge_16_8u_px(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_edge_16_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_edge_16_8u_avx2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_edge_8_8u_px(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_edge_8_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_horizontal_edge_8_8u_avx2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_16_8u_px(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_16_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_16_dual_8u_px(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_16_dual_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_4_8u_px(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_4_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_4_dual_8u_px(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
    void lpf_vertical_4_dual_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
    void lpf_vertical_8_8u_px(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_8_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void lpf_vertical_8_dual_8u_px(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
    void lpf_vertical_8_dual_8u_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
    void lpf_horizontal_4_16u_px(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
    void lpf_horizontal_4_dual_16u_px(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
    void lpf_horizontal_8_16u_px(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
    void lpf_horizontal_8_dual_16u_px(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
    void lpf_horizontal_edge_16_16u_px(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
    void lpf_horizontal_edge_8_16u_px(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
    void lpf_vertical_16_16u_px(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
    void lpf_vertical_16_dual_16u_px(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
    void lpf_vertical_4_16u_px(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
    void lpf_vertical_4_dual_16u_px(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
    void lpf_vertical_8_16u_px(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
    void lpf_vertical_8_dual_16u_px(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
    // etc
    int diff_dc_px(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int size);
    void adds_nv12_px(uint8_t *dst, int pitchDst, const uint8_t *src1, int pitch1, const int16_t *src2u, const int16_t *src2v, int pitch2, int size);
    void adds_nv12_avx2(uint8_t *dst, int pitchDst, const uint8_t *src1, int pitch1, const int16_t *src2u, const int16_t *src2v, int pitch2, int size);
    void search_best_block8x8_px  (uint8_t *src, uint8_t *ref, int pitch, int xrange, int yrange, uint32_t *bestSAD, int *bestX, int *bestY);
    void search_best_block8x8_avx2(uint8_t *src, uint8_t *ref, int pitch, int xrange, int yrange, uint32_t *bestSAD, int *bestX, int *bestY);
    void compute_rscs_px(const uint8_t *ySrc, int pitchSrc, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height);
    void compute_rscs_avx2(const uint8_t *ySrc, int pitchSrc, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height);
    void compute_rscs_8u_avx2 (const uint8_t *src, int pitch, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height);
    void compute_rscs_16u_sse4(const uint16_t *src, int pitch, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height);
    void compute_rscs_4x4_px  (const uint8_t* pSrc, int srcPitch, int wblocks, int hblocks, float* pRs, float* pCs);
    void compute_rscs_4x4_avx2(const uint8_t* pSrc, int srcPitch, int wblocks, int hblocks, float* pRs, float* pCs);
    void compute_rscs_diff_px  (float* pRs0, float* pCs0, float* pRs1, float* pCs1, int len, float* pRsDiff, float* pCsDiff);
    void compute_rscs_diff_avx2(float* pRs0, float* pCs0, float* pRs1, float* pCs1, int len, float* pRsDiff, float* pCsDiff);
    void diff_histogram_px(uint8_t *pSrc, uint8_t *pRef, int pitch, int width, int height, int histogram[5], int64_t *pSrcDC, int64_t *pRefDC);
    void diff_histogram_avx2(uint8_t *pSrc, uint8_t *pRef, int pitch, int width, int height, int histogram[5], int64_t *pSrcDC, int64_t *pRefDC);

    int cdef_find_dir_px  (const uint16_t *img, int stride, int *var, int coeff_shift);
    int cdef_find_dir_avx2(const uint16_t *img, int stride, int *var, int coeff_shift);
    template <typename TDst> void cdef_filter_block_4x4_px  (TDst *dst, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping);
    template <typename TDst> void cdef_filter_block_8x8_px  (TDst *dst, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping);
    template <typename TDst> void cdef_filter_block_4x4_avx2(TDst *dst, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping);
    template <typename TDst> void cdef_filter_block_8x8_avx2(TDst *dst, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping);
    template <typename TDst> void cdef_filter_block_4x4_nv12_px  (TDst *dst, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping);
    template <typename TDst> void cdef_filter_block_4x4_nv12_avx2(TDst *dst, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping);
    void cdef_estimate_block_4x4_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_8x8_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_8x8_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_sec0_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, uint8_t* dst, int32_t dstride);
    void cdef_estimate_block_4x4_nv12_sec0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, uint8_t* dst, int32_t dstride);
    void cdef_estimate_block_8x8_sec0_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, uint8_t *dst, int dstride);
    void cdef_estimate_block_8x8_sec0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, uint8_t *dst, int dstride);
    void cdef_estimate_block_4x4_pri0_px  (const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_pri0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_pri0_px  (const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_pri0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse);
    void cdef_estimate_block_8x8_pri0_px  (const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse);
    void cdef_estimate_block_8x8_pri0_avx2(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_all_sec_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_all_sec_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_all_sec_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_all_sec_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_8x8_all_sec_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_8x8_all_sec_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_2pri_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_2pri_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_2pri_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_4x4_nv12_2pri_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_8x8_2pri_px  (const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1, int dir, int pri_damping, int sec_damping, int *sse);
    void cdef_estimate_block_8x8_2pri_avx2(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1, int dir, int pri_damping, int sec_damping, int *sse);
};
