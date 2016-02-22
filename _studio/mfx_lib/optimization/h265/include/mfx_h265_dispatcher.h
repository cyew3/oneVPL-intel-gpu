/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AUTO)

namespace MFX_HEVC_PP
{

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* ******************************************************** */
/*                    List of All functions                 */
/* ******************************************************** */
    //[SAD]
    #define SAD_PARAMETERS_LIST_SPECIAL const unsigned char *image,  const unsigned char *block, int img_stride
    // [PX]
    int H265_FASTCALL SAD_4x4_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x8_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x16_px(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_8x4_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x8_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x16_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x32_px(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_12x16_px(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_16x4_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x8_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x12_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x16_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x32_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x64_px(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_24x32_px(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_32x8_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x16_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x24_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x32_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x64_px(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_48x64_px(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_64x16_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x32_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x48_px(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x64_px(SAD_PARAMETERS_LIST_SPECIAL);
    // [SSE4]
    int H265_FASTCALL SAD_4x4_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x8_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x16_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_8x4_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x8_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x16_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x32_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_12x16_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_16x4_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x8_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x12_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x16_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x32_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x64_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_24x32_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_32x8_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x16_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x24_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x32_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x64_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_48x64_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_64x16_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x32_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x48_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x64_sse(SAD_PARAMETERS_LIST_SPECIAL);

    // [SSSE3]
    int H265_FASTCALL SAD_4x4_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x8_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x16_ssse3(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_8x4_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x8_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x16_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x32_ssse3(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_12x16_ssse3(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_16x4_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x8_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x12_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x16_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x32_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x64_ssse3(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_24x32_ssse3(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_32x8_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x16_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x24_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x32_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x64_ssse3(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_48x64_ssse3(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_64x16_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x32_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x48_ssse3(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x64_ssse3(SAD_PARAMETERS_LIST_SPECIAL);

    // [AVX2]
    int H265_FASTCALL SAD_4x4_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x8_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_8x4_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x8_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_12x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_16x4_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x8_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x12_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x64_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_24x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_32x8_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x24_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x64_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_48x64_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_64x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x48_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x64_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    
    #define SAD_PARAMETERS_LIST_GENERAL const unsigned char *image,  const unsigned char *block, int img_stride, int block_stride
    //[PX.general]
    int H265_FASTCALL SAD_4x4_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x8_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x16_general_px(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_8x4_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x8_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x16_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x32_general_px(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_12x16_general_px(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_16x4_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x8_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x12_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x16_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x32_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x64_general_px(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_24x32_general_px(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_32x8_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x16_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x24_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x32_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x64_general_px(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_48x64_general_px(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_64x16_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x32_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x48_general_px(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x64_general_px(SAD_PARAMETERS_LIST_GENERAL);
    //[SSE4. General]
    int H265_FASTCALL SAD_4x4_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x8_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_8x4_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x8_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_12x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_16x4_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x8_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x12_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x64_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_24x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_32x8_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x24_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x64_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_48x64_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_64x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x48_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x64_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    //[SSSE3. General]
    int H265_FASTCALL SAD_4x4_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x8_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x16_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_8x4_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x8_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x16_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x32_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_12x16_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_16x4_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x8_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x12_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x16_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x32_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x64_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_24x32_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_32x8_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x16_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x24_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x32_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x64_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_48x64_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_64x16_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x32_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x48_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x64_general_ssse3(SAD_PARAMETERS_LIST_GENERAL);

    // [AVX2.General]
    int H265_FASTCALL SAD_4x4_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x8_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_8x4_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x8_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_12x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_16x4_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x8_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x12_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x64_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_24x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_32x8_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x24_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x64_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_48x64_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_64x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x48_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x64_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    void H265_FASTCALL h265_SAD_MxN_x3_8u_px  (const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[3]);
    void H265_FASTCALL h265_SAD_MxN_x3_8u_sse (const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[3]);
    void H265_FASTCALL h265_SAD_MxN_x3_8u_avx2(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[3]);

    void H265_FASTCALL h265_SAD_MxN_x4_8u_px  (const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[4]);
    void H265_FASTCALL h265_SAD_MxN_x4_8u_sse (const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[4]);
    void H265_FASTCALL h265_SAD_MxN_x4_8u_avx2(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[4]);

    int H265_FASTCALL h265_SAD_MxN_general_16s_px  (const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride, Ipp32s width, Ipp32s height);
    int H265_FASTCALL h265_SAD_MxN_general_16s_sse (const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride, Ipp32s width, Ipp32s height);
    int H265_FASTCALL h265_SAD_MxN_general_16s_avx2(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s ref_stride, Ipp32s width, Ipp32s height);

    int H265_FASTCALL h265_SAD_MxN_16s_px  (const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s width, Ipp32s height);
    int H265_FASTCALL h265_SAD_MxN_16s_sse (const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s width, Ipp32s height);
    int H265_FASTCALL h265_SAD_MxN_16s_avx2(const Ipp16s* src, Ipp32s src_stride, const Ipp16s* ref, Ipp32s width, Ipp32s height);

    template <class T> Ipp32s H265_FASTCALL h265_SSE_px  (const T *src1, Ipp32s pitchSrc1, const T *src2, Ipp32s pitchSrc2, Ipp32s width, Ipp32s height, Ipp32s shift);
    template <class T> Ipp32s H265_FASTCALL h265_SSE_avx2(const T *src1, Ipp32s pitchSrc1, const T *src2, Ipp32s pitchSrc2, Ipp32s width, Ipp32s height, Ipp32s shift);

    template <class T> void H265_FASTCALL h265_DiffNv12_px  (const T *src, Ipp32s pitchSrc, const T *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s width, Ipp32s height);
    template <class T> void H265_FASTCALL h265_DiffNv12_avx2(const T *src, Ipp32s pitchSrc, const T *pred, Ipp32s pitchPred, Ipp16s *diff1, Ipp32s pitchDiff1, Ipp16s *diff2, Ipp32s pitchDiff2, Ipp32s width, Ipp32s height);

    template <class T> void H265_FASTCALL h265_SplitChromaCtb_px  (const T *nv12, Ipp32s pitchNv12, T *u, Ipp32s pitchU, T *v, Ipp32s pitchV, Ipp32s width, Ipp32s height);
    template <class T> void H265_FASTCALL h265_SplitChromaCtb_avx2(const T *nv12, Ipp32s pitchNv12, T *u, Ipp32s pitchU, T *v, Ipp32s pitchV, Ipp32s width, Ipp32s height);

    // SATD
    Ipp32s H265_FASTCALL h265_SATD_4x4_8u_px  (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep);
    Ipp32s H265_FASTCALL h265_SATD_4x4_8u_sse (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep);
    Ipp32s H265_FASTCALL h265_SATD_4x4_8u_avx2(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep);

    Ipp32s H265_FASTCALL h265_SATD_8x8_8u_px  (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep);
    Ipp32s H265_FASTCALL h265_SATD_8x8_8u_sse (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep);
    Ipp32s H265_FASTCALL h265_SATD_8x8_8u_avx2(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep);

    void H265_FASTCALL h265_SATD_4x4_Pair_8u_px  (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair);
    void H265_FASTCALL h265_SATD_4x4_Pair_8u_sse (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair);
    void H265_FASTCALL h265_SATD_4x4_Pair_8u_avx2(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair);

    void H265_FASTCALL h265_SATD_8x8_Pair_8u_px  (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair);
    void H265_FASTCALL h265_SATD_8x8_Pair_8u_sse (const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair);
    void H265_FASTCALL h265_SATD_8x8_Pair_8u_avx2(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair);

    Ipp32s H265_FASTCALL h265_SATD_4x4_16u_px  (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep);
    Ipp32s H265_FASTCALL h265_SATD_4x4_16u_sse (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep);
    Ipp32s H265_FASTCALL h265_SATD_4x4_16u_avx2(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep);

    Ipp32s H265_FASTCALL h265_SATD_8x8_16u_px  (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep);
    Ipp32s H265_FASTCALL h265_SATD_8x8_16u_sse (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep);
    Ipp32s H265_FASTCALL h265_SATD_8x8_16u_avx2(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep);

    void H265_FASTCALL h265_SATD_4x4_Pair_16u_px  (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair);
    void H265_FASTCALL h265_SATD_4x4_Pair_16u_sse (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair);
    void H265_FASTCALL h265_SATD_4x4_Pair_16u_avx2(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair);

    void H265_FASTCALL h265_SATD_8x8_Pair_16u_px  (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair);
    void H265_FASTCALL h265_SATD_8x8_Pair_16u_sse (const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair);
    void H265_FASTCALL h265_SATD_8x8_Pair_16u_avx2(const Ipp16u* pSrcCur, int srcCurStep, const Ipp16u* pSrcRef, int srcRefStep, Ipp32s* satdPair);

    // [transform.forward]
    void H265_FASTCALL h265_DST4x4Fwd_16s_px(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT4x4Fwd_16s_px(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT8x8Fwd_16s_px(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT16x16Fwd_16s_px(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT32x32Fwd_16s_px(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dest, Ipp32u bitDepth);

    void H265_FASTCALL h265_DST4x4Fwd_16s_sse(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT4x4Fwd_16s_sse(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT8x8Fwd_16s_sse(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT16x16Fwd_16s_sse(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT32x32Fwd_16s_sse(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dest, Ipp32u bitDepth);

    void H265_FASTCALL h265_DST4x4Fwd_16s_ssse3(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT4x4Fwd_16s_ssse3(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT8x8Fwd_16s_ssse3(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT16x16Fwd_16s_ssse3(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT32x32Fwd_16s_ssse3(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dest, Ipp32u bitDepth);

    void H265_FASTCALL h265_DST4x4Fwd_16s_avx2(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT4x4Fwd_16s_avx2(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT8x8Fwd_16s_avx2(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT16x16Fwd_16s_avx2(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dst, Ipp32u bitDepth);
    void H265_FASTCALL h265_DCT32x32Fwd_16s_avx2(const short *H265_RESTRICT src, int srcPitch, short *H265_RESTRICT dest, Ipp32u bitDepth);

    // [quantization.forward]
    void H265_FASTCALL h265_QuantFwd_16s_px(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);
    void H265_FASTCALL h265_QuantFwd_16s_sse(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);
    void H265_FASTCALL h265_QuantFwd_16s_avx2(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);

    Ipp32s H265_FASTCALL h265_QuantFwd_SBH_16s_px(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift);
    Ipp32s H265_FASTCALL h265_QuantFwd_SBH_16s_sse(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift);
    Ipp32s H265_FASTCALL h265_QuantFwd_SBH_16s_avx2(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift);

    void H265_FASTCALL h265_Quant_zCost_16s_px(const Ipp16s* pSrc, Ipp32u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0);
    void H265_FASTCALL h265_Quant_zCost_16s_sse(const Ipp16s* pSrc, Ipp32u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0);
    void H265_FASTCALL h265_Quant_zCost_16s_avx2(const Ipp16s* pSrc, Ipp32u* qLevels, Ipp64s* zlCosts, Ipp32s len, Ipp32s qScale, Ipp32s qoffset, Ipp32s qbits, Ipp32s rdScale0);

    void H265_FASTCALL h265_QuantInv_16s_px(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);
    void H265_FASTCALL h265_QuantInv_16s_avx2(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift);
#endif
    // [transform.inv]
    void h265_DST4x4Inv_16sT_px  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT4x4Inv_16sT_px  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT8x8Inv_16sT_px  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT16x16Inv_16sT_px(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT32x32Inv_16sT_px(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);

    void h265_DST4x4Inv_16sT_sse  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT4x4Inv_16sT_sse  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT8x8Inv_16sT_sse  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT16x16Inv_16sT_sse(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT32x32Inv_16sT_sse(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);

    void h265_DST4x4Inv_16sT_ssse3  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT4x4Inv_16sT_ssse3  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT8x8Inv_16sT_ssse3  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT16x16Inv_16sT_ssse3(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT32x32Inv_16sT_ssse3(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);

    void h265_DST4x4Inv_16sT_avx2  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT4x4Inv_16sT_avx2  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT8x8Inv_16sT_avx2  (void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT16x16Inv_16sT_avx2(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);
    void h265_DCT32x32Inv_16sT_avx2(void *pred, int predStride, void *destPtr, const short *H265_RESTRICT coeff, int destStride, int inplace, Ipp32u bitDepth);

    // [deblocking]
    Ipp32s h265_FilterEdgeLuma_8u_I_px(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir);
    Ipp32s h265_FilterEdgeLuma_8u_I_sse(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir);
    Ipp32s h265_FilterEdgeLuma_8u_I_ssse3(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir);

    void h265_FilterEdgeChroma_Interleaved_8u_I_px(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr);
    void h265_FilterEdgeChroma_Interleaved_8u_I_sse(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr);
    void h265_FilterEdgeChroma_Interleaved_8u_I_ssse3(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr);

    void h265_FilterEdgeChroma_Plane_8u_I_px(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp);
    void h265_FilterEdgeChroma_Plane_8u_I_sse(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp);
    void h265_FilterEdgeChroma_Plane_8u_I_ssse3(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp);

    Ipp32s h265_FilterEdgeLuma_16u_I_px(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32u bit_depth);
    Ipp32s h265_FilterEdgeLuma_16u_I_sse(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32u bit_depth);
    Ipp32s h265_FilterEdgeLuma_16u_I_ssse3(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32u bit_depth);

    void h265_FilterEdgeChroma_Interleaved_16u_I_px(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u bit_depth);
    void h265_FilterEdgeChroma_Interleaved_16u_I_sse(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u bit_depth);
    void h265_FilterEdgeChroma_Interleaved_16u_I_ssse3(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u bit_depth);

    void h265_FilterEdgeChroma_Plane_16u_I_px(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp, Ipp32u bit_depth);
    void h265_FilterEdgeChroma_Plane_16u_I_sse(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp, Ipp32u bit_depth);
    void h265_FilterEdgeChroma_Plane_16u_I_ssse3(H265EdgeData *edge, Ipp16u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQp, Ipp32u bit_depth);

    // [SAO]
    void h265_ProcessSaoCuOrg_Luma_8u_px( SAOCU_ORG_PARAMETERS_LIST );
    void h265_ProcessSaoCuOrg_Luma_8u_sse( SAOCU_ORG_PARAMETERS_LIST );
    void h265_ProcessSaoCuOrg_Luma_8u_ssse3( SAOCU_ORG_PARAMETERS_LIST );

    void h265_ProcessSaoCu_Luma_8u_px(SAOCU_PARAMETERS_LIST);
    void h265_ProcessSaoCu_Luma_8u_sse(SAOCU_PARAMETERS_LIST);
    void h265_ProcessSaoCu_Luma_8u_ssse3(SAOCU_PARAMETERS_LIST);

    void h265_ProcessSaoCuOrg_Luma_16u_px( SAOCU_ORG_PARAMETERS_LIST_U16 );
    void h265_ProcessSaoCuOrg_Luma_16u_sse( SAOCU_ORG_PARAMETERS_LIST_U16 );
    void h265_ProcessSaoCuOrg_Luma_16u_ssse3( SAOCU_ORG_PARAMETERS_LIST_U16 );

    void h265_ProcessSaoCu_Luma_16u_px(SAOCU_PARAMETERS_LIST_U16);
    void h265_ProcessSaoCu_Luma_16u_sse(SAOCU_PARAMETERS_LIST_U16);
    void h265_ProcessSaoCu_Luma_16u_ssse3(SAOCU_PARAMETERS_LIST_U16);

    void h265_GetCtuStatistics_8u_px( SAOCU_ENCODE_PARAMETERS_LIST );
    void h265_GetCtuStatistics_8u_sse( SAOCU_ENCODE_PARAMETERS_LIST );
    void h265_GetCtuStatistics_8u_avx2( SAOCU_ENCODE_PARAMETERS_LIST );

    void h265_GetCtuStatistics_16u_px( SAOCU_ENCODE_PARAMETERS_LIST_16U );
    void h265_GetCtuStatistics_16u_avx2( SAOCU_ENCODE_PARAMETERS_LIST_16U );

    // [INTRA predict]
    void h265_PredictIntra_Ang_8u_px(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_8u_sse(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_8u_ssse3(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_8u_avx2(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_All_8u_px(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width);
    void h265_PredictIntra_Ang_All_8u_sse(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width);
    void h265_PredictIntra_Ang_All_8u_ssse3(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width);
    void h265_PredictIntra_Ang_All_8u_avx2(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width);
    void h265_PredictIntra_Ang_NoTranspose_8u_px(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_NoTranspose_8u_sse(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_NoTranspose_8u_ssse3(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_NoTranspose_8u_avx2(Ipp32s mode, Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_All_Even_8u_px(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width);
    void h265_PredictIntra_Ang_All_Even_8u_sse(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width);
    void h265_PredictIntra_Ang_All_Even_8u_ssse3(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width);
    void h265_PredictIntra_Ang_All_Even_8u_avx2(Ipp8u* PredPel, Ipp8u* FiltPel, Ipp8u* pels, Ipp32s width);

    void h265_PredictIntra_Ang_16u_px(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_16u_sse(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_16u_ssse3(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_16u_avx2(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_NoTranspose_16u_px(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_NoTranspose_16u_sse(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_NoTranspose_16u_ssse3(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_NoTranspose_16u_avx2(Ipp32s mode, Ipp16u* PredPel, Ipp16u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Ang_All_16u_px(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth);
    void h265_PredictIntra_Ang_All_16u_sse(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth);
    void h265_PredictIntra_Ang_All_16u_ssse3(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth);
    void h265_PredictIntra_Ang_All_16u_avx2(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth);
    void h265_PredictIntra_Ang_All_Even_16u_px(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth);
    void h265_PredictIntra_Ang_All_Even_16u_sse(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth);
    void h265_PredictIntra_Ang_All_Even_16u_ssse3(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth);
    void h265_PredictIntra_Ang_All_Even_16u_avx2(Ipp16u* PredPel, Ipp16u* FiltPel, Ipp16u* pels, Ipp32s width, Ipp32s bitDepth);

    void h265_FilterPredictPels_8u_px(Ipp8u* PredPel, Ipp32s width);
    void h265_FilterPredictPels_8u_sse(Ipp8u* PredPel, Ipp32s width);
    void h265_FilterPredictPels_8u_avx2(Ipp8u* PredPel, Ipp32s width);

    void h265_FilterPredictPels_Bilinear_8u_px(Ipp8u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);
    void h265_FilterPredictPels_Bilinear_8u_sse(Ipp8u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);
    void h265_FilterPredictPels_Bilinear_8u_avx2(Ipp8u* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);

    void h265_PredictIntra_Planar_8u_px(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Planar_8u_sse(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Planar_8u_avx2(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);

    void h265_FilterPredictPels_16s_px(Ipp16s* PredPel, Ipp32s width);
    void h265_FilterPredictPels_16s_sse(Ipp16s* PredPel, Ipp32s width);
    void h265_FilterPredictPels_16s_avx2(Ipp16s* PredPel, Ipp32s width);

    void h265_FilterPredictPels_Bilinear_16s_px(Ipp16s* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);
    void h265_FilterPredictPels_Bilinear_16s_sse(Ipp16s* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);
    void h265_FilterPredictPels_Bilinear_16s_avx2(Ipp16s* pSrcDst, int width, int topLeft, int bottomLeft, int topRight);

    void h265_PredictIntra_Planar_16s_px(Ipp16s* PredPel, Ipp16s* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Planar_16s_sse(Ipp16s* PredPel, Ipp16s* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Planar_16s_avx2(Ipp16s* PredPel, Ipp16s* pels, Ipp32s pitch, Ipp32s width);

    void h265_PredictIntra_Planar_ChromaNV12_8u_px(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);
    void h265_PredictIntra_Planar_ChromaNV12_8u_avx2(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width);

    template <class PixType, class HistType>
    void h265_AnalyzeGradient_px(const PixType* src, Ipp32s pitch, HistType* hist4, HistType* hist8, Ipp32s width, Ipp32s height);
    template <class PixType, class HistType>
    void h265_AnalyzeGradient_sse(const PixType* src, Ipp32s pitch, HistType* hist4, HistType* hist8, Ipp32s width, Ipp32s height);
    template <class PixType, class HistType>
    void h265_AnalyzeGradient_avx2(const PixType* src, Ipp32s pitch, HistType* hist4, HistType* hist8, Ipp32s width, Ipp32s height);
    
    template <class PixType>
    void h265_ComputeRsCs_px(const PixType* ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height);
    void h265_ComputeRsCs_8u_sse (const Ipp8u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height);
    void h265_ComputeRsCs_8u_avx2 (const Ipp8u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height);
    void h265_ComputeRsCs_16u_sse (const Ipp16u *ySrc, Ipp32s pitchSrc, Ipp32s *lcuRs, Ipp32s *lcuCs, Ipp32s pitchRsCs, Ipp32s width, Ipp32s height);

    void h265_AddClipNv12UV_8u_px  (Ipp8u *dstNv12, Ipp32s pitchDst, const Ipp8u *src1Nv12, Ipp32s pitchSrc1, 
                                      const Ipp16s *src2Yv12U, const Ipp16s *src2Yv12V, Ipp32s pitchSrc2, Ipp32s size);
    void h265_AddClipNv12UV_8u_sse (Ipp8u *dstNv12, Ipp32s pitchDst, const Ipp8u *src1Nv12, Ipp32s pitchSrc1, 
                                      const Ipp16s *src2Yv12U, const Ipp16s *src2Yv12V, Ipp32s pitchSrc2, Ipp32s size);

    // Interpolation
    void h265_InterpLuma_s8_d16_H_px(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s8_d16_H_sse(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s8_d16_H_ssse3(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s8_d16_H_atom(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s8_d16_H_avx2(INTERP_S8_D16_PARAMETERS_LIST);
    // 4-tap filter -1,5,5,-1
    void h265_InterpLumaFast_s8_d16_H_px(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s8_d16_H_sse(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s8_d16_H_ssse3(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s8_d16_H_atom(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s8_d16_H_avx2(INTERP_S8_D16_PARAMETERS_LIST);

    // pack intermediate interpolation pels s16 to u8/u16
    template <typename PixType> void h265_InterpLumaPack_px(const Ipp16s *src, Ipp32s pitchSrc, PixType *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s bitDepth);
    template <typename PixType> void h265_InterpLumaPack_sse(const Ipp16s *src, Ipp32s pitchSrc, PixType *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s bitDepth);
    template <typename PixType> void h265_InterpLumaPack_avx2(const Ipp16s *src, Ipp32s pitchSrc, PixType *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s bitDepth);

    void h265_InterpChroma_s8_d16_H_px(INTERP_S8_D16_PARAMETERS_LIST, int plane);
    void h265_InterpChroma_s8_d16_H_sse(INTERP_S8_D16_PARAMETERS_LIST, int plane);
    void h265_InterpChroma_s8_d16_H_ssse3(INTERP_S8_D16_PARAMETERS_LIST, int plane);
    void h265_InterpChroma_s8_d16_H_atom(INTERP_S8_D16_PARAMETERS_LIST, int plane);
    void h265_InterpChroma_s8_d16_H_avx2(INTERP_S8_D16_PARAMETERS_LIST, int plane);

    void h265_InterpLuma_s8_d16_V_px(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s8_d16_V_sse(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s8_d16_V_ssse3(INTERP_S8_D16_PARAMETERS_LIST);    
    void h265_InterpLuma_s8_d16_V_atom(INTERP_S8_D16_PARAMETERS_LIST);    
    void h265_InterpLuma_s8_d16_V_avx2(INTERP_S8_D16_PARAMETERS_LIST);
    // 4-tap filter -1 5 5 -1
    void h265_InterpLumaFast_s8_d16_V_px(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s8_d16_V_sse(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s8_d16_V_ssse3(INTERP_S8_D16_PARAMETERS_LIST);    
    void h265_InterpLumaFast_s8_d16_V_atom(INTERP_S8_D16_PARAMETERS_LIST);    
    void h265_InterpLumaFast_s8_d16_V_avx2(INTERP_S8_D16_PARAMETERS_LIST);    

    void h265_InterpChroma_s8_d16_V_px(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpChroma_s8_d16_V_sse(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpChroma_s8_d16_V_ssse3(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpChroma_s8_d16_V_atom(INTERP_S8_D16_PARAMETERS_LIST);
    void h265_InterpChroma_s8_d16_V_avx2(INTERP_S8_D16_PARAMETERS_LIST);
    
    void h265_InterpLuma_s16_d16_V_px(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s16_d16_V_sse(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s16_d16_V_ssse3(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s16_d16_V_atom(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s16_d16_V_avx2(INTERP_S16_D16_PARAMETERS_LIST);
    
    //kolya
    void h265_InterpLumaFast_s16_d16_V_px(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s16_d16_V_sse(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s16_d16_V_ssse3(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s16_d16_V_atom(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s16_d16_V_avx2(INTERP_S16_D16_PARAMETERS_LIST);


    void h265_InterpChroma_s16_d16_V_px(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpChroma_s16_d16_V_sse(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpChroma_s16_d16_V_ssse3(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpChroma_s16_d16_V_atom(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpChroma_s16_d16_V_avx2(INTERP_S16_D16_PARAMETERS_LIST);

    // 10bit to 8bit surface conversion
    void h265_ConvertShiftR_px(const Ipp16s *src, Ipp32s pitchSrc, unsigned char *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s rshift);
    void h265_ConvertShiftR_sse(const Ipp16s *src, Ipp32s pitchSrc, unsigned char *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s rshift);
    void h265_ConvertShiftR_avx2(const Ipp16s *src, Ipp32s pitchSrc, unsigned char *dst, Ipp32s pitchDst, Ipp32s width, Ipp32s height, Ipp32s rshift);
    
    // average
    void h265_AverageModeN_px(INTERP_AVG_NONE_PARAMETERS_LIST);
    void h265_AverageModeN_sse(INTERP_AVG_NONE_PARAMETERS_LIST);
    void h265_AverageModeN_ssse3(INTERP_AVG_NONE_PARAMETERS_LIST);
    void h265_AverageModeN_atom(INTERP_AVG_NONE_PARAMETERS_LIST);
    void h265_AverageModeN_avx2(INTERP_AVG_NONE_PARAMETERS_LIST);

    void h265_AverageModeP_px(INTERP_AVG_PIC_PARAMETERS_LIST);
    void h265_AverageModeP_sse(INTERP_AVG_PIC_PARAMETERS_LIST);
    void h265_AverageModeP_ssse3(INTERP_AVG_PIC_PARAMETERS_LIST);
    void h265_AverageModeP_atom(INTERP_AVG_PIC_PARAMETERS_LIST);
    void h265_AverageModeP_avx2(INTERP_AVG_PIC_PARAMETERS_LIST);

    void h265_AverageModeB_px(INTERP_AVG_BUF_PARAMETERS_LIST);
    void h265_AverageModeB_sse(INTERP_AVG_BUF_PARAMETERS_LIST);
    void h265_AverageModeB_ssse3(INTERP_AVG_BUF_PARAMETERS_LIST);
    void h265_AverageModeB_atom(INTERP_AVG_BUF_PARAMETERS_LIST);
    void h265_AverageModeB_avx2(INTERP_AVG_BUF_PARAMETERS_LIST);

    /* additional filter kernels for bitDepth > 8 */
    void h265_InterpLuma_s16_d16_H_px(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s16_d16_H_sse(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s16_d16_H_ssse3(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s16_d16_H_atom(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLuma_s16_d16_H_avx2(INTERP_S16_D16_PARAMETERS_LIST);

    //FastInterp   //kolya
    void h265_InterpLumaFast_s16_d16_H_px(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s16_d16_H_sse(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s16_d16_H_ssse3(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s16_d16_H_atom(INTERP_S16_D16_PARAMETERS_LIST);
    void h265_InterpLumaFast_s16_d16_H_avx2(INTERP_S16_D16_PARAMETERS_LIST);

    
    void h265_InterpChroma_s16_d16_H_px(INTERP_S16_D16_PARAMETERS_LIST, int plane);
    void h265_InterpChroma_s16_d16_H_sse(INTERP_S16_D16_PARAMETERS_LIST, int plane);
    void h265_InterpChroma_s16_d16_H_ssse3(INTERP_S16_D16_PARAMETERS_LIST, int plane);
    void h265_InterpChroma_s16_d16_H_atom(INTERP_S16_D16_PARAMETERS_LIST, int plane);
    void h265_InterpChroma_s16_d16_H_avx2(INTERP_S16_D16_PARAMETERS_LIST, int plane);

    void h265_AverageModeN_U16_px(INTERP_AVG_NONE_PARAMETERS_LIST_U16);
    void h265_AverageModeN_U16_sse(INTERP_AVG_NONE_PARAMETERS_LIST_U16);
    void h265_AverageModeN_U16_ssse3(INTERP_AVG_NONE_PARAMETERS_LIST_U16);
    void h265_AverageModeN_U16_atom(INTERP_AVG_NONE_PARAMETERS_LIST_U16);
    void h265_AverageModeN_U16_avx2(INTERP_AVG_NONE_PARAMETERS_LIST_U16);

    void h265_AverageModeP_U16_px(INTERP_AVG_PIC_PARAMETERS_LIST_U16);
    void h265_AverageModeP_U16_sse(INTERP_AVG_PIC_PARAMETERS_LIST_U16);
    void h265_AverageModeP_U16_ssse3(INTERP_AVG_PIC_PARAMETERS_LIST_U16);
    void h265_AverageModeP_U16_atom(INTERP_AVG_PIC_PARAMETERS_LIST_U16);
    void h265_AverageModeP_U16_avx2(INTERP_AVG_PIC_PARAMETERS_LIST_U16);

    void h265_AverageModeB_U16_px(INTERP_AVG_BUF_PARAMETERS_LIST_U16);
    void h265_AverageModeB_U16_sse(INTERP_AVG_BUF_PARAMETERS_LIST_U16);
    void h265_AverageModeB_U16_ssse3(INTERP_AVG_BUF_PARAMETERS_LIST_U16);
    void h265_AverageModeB_U16_atom(INTERP_AVG_BUF_PARAMETERS_LIST_U16);
    void h265_AverageModeB_U16_avx2(INTERP_AVG_BUF_PARAMETERS_LIST_U16);

    // WeightedPred
    void h265_CopyWeighted_S16U8_px(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round);
    void h265_CopyWeighted_S16U8_sse(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round);
    void h265_CopyWeighted_S16U8_ssse3(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round);
    void h265_CopyWeighted_S16U8_avx2(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round);

    void h265_CopyWeightedBidi_S16U8_px(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round);
    void h265_CopyWeightedBidi_S16U8_sse(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round);
    void h265_CopyWeightedBidi_S16U8_ssse3(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round);
    void h265_CopyWeightedBidi_S16U8_avx2(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp8u* pDst, Ipp8u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round);

    void h265_CopyWeighted_S16U16_px(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);
    void h265_CopyWeighted_S16U16_sse(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);
    void h265_CopyWeighted_S16U16_ssse3(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);
    void h265_CopyWeighted_S16U16_avx2(Ipp16s* pSrc, Ipp16s* pSrcUV, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStrideY, Ipp32u DstStrideY, Ipp32u SrcStrideC, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);

    void h265_CopyWeightedBidi_S16U16_px(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);
    void h265_CopyWeightedBidi_S16U16_sse(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);
    void h265_CopyWeightedBidi_S16U16_ssse3(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);
    void h265_CopyWeightedBidi_S16U16_avx2(Ipp16s* pSrc0, Ipp16s* pSrcUV0, Ipp16s* pSrc1, Ipp16s* pSrcUV1, Ipp16u* pDst, Ipp16u* pDstUV, Ipp32u SrcStride0Y, Ipp32u SrcStride1Y, Ipp32u DstStrideY, Ipp32u SrcStride0C, Ipp32u SrcStride1C, Ipp32u DstStrideC, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);

}; // namespace MFX_HEVC_PP

#else// #if defined (MFX_TARGET_OPTIMIZATION_AUTO)

namespace MFX_HEVC_PP
{}

#endif // #if defined (MFX_TARGET_OPTIMIZATION_AUTO)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
