//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

/* General HEVC SAD functions - optimized with SSE4
 * calculates SAD's of 3 or 4 reference blocks (ref0-3) against a single source block (src)
 * no alignment assumed for input buffers
 * NOTE - may require /Qsfalign 
 */

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4)

#if defined(_WIN32) || defined (_WIN64)
#   include <intrin.h>
#endif

/* from mfx_h265_sad_general_sse.cpp */
#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (const double *)(p)))

namespace MFX_HEVC_PP
{

template <int height, int nRef>
static void SAD_4xN(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s sads[])
{
    int h;
    __m128i xmm0, xmm1, xmm4, xmm5, xmm6, xmm7;

    /* SAD accumulators */
    xmm4 = _mm_setzero_si128();
    xmm5 = _mm_setzero_si128();
    xmm6 = _mm_setzero_si128();
    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h += 4) {
        /* load 4x4 src block into one register */
        xmm0 = _mm_cvtsi32_si128(*(int *)(src + 0*src_stride));
        xmm0 = _mm_insert_epi32(xmm0, *(int *)(src + 1*src_stride), 1);
        xmm0 = _mm_insert_epi32(xmm0, *(int *)(src + 2*src_stride), 2);
        xmm0 = _mm_insert_epi32(xmm0, *(int *)(src + 3*src_stride), 3);
        src += 4*src_stride;

        /* load 4x4 ref block into one register, calculate SAD, add to 32-bit accumulator */
        xmm1 = _mm_cvtsi32_si128(*(int *)(ref0 + 0*ref_stride));
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref0 + 1*ref_stride), 1);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref0 + 2*ref_stride), 2);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref0 + 3*ref_stride), 3);
        xmm1 = _mm_sad_epu8(xmm1, xmm0);
        xmm4 = _mm_add_epi32(xmm4, xmm1);
        ref0 += 4*ref_stride;

        xmm1 = _mm_cvtsi32_si128(*(int *)(ref1 + 0*ref_stride));
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref1 + 1*ref_stride), 1);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref1 + 2*ref_stride), 2);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref1 + 3*ref_stride), 3);
        xmm1 = _mm_sad_epu8(xmm1, xmm0);
        xmm5 = _mm_add_epi32(xmm5, xmm1);
        ref1 += 4*ref_stride;

        xmm1 = _mm_cvtsi32_si128(*(int *)(ref2 + 0*ref_stride));
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref2 + 1*ref_stride), 1);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref2 + 2*ref_stride), 2);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref2 + 3*ref_stride), 3);
        xmm1 = _mm_sad_epu8(xmm1, xmm0);
        xmm6 = _mm_add_epi32(xmm6, xmm1);
        ref2 += 4*ref_stride;

        if (nRef == 3)
            continue;

        xmm1 = _mm_cvtsi32_si128(*(int *)(ref3 + 0*ref_stride));
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref3 + 1*ref_stride), 1);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref3 + 2*ref_stride), 2);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(ref3 + 3*ref_stride), 3);
        xmm1 = _mm_sad_epu8(xmm1, xmm0);
        xmm7 = _mm_add_epi32(xmm7, xmm1);
        ref3 += 4*ref_stride;
    }

    /* sum low and high parts of SAD accumulator */
    xmm4 = _mm_add_epi32(xmm4, _mm_shuffle_epi32(xmm4, 0x02));
    xmm5 = _mm_add_epi32(xmm5, _mm_shuffle_epi32(xmm5, 0x02));
    xmm6 = _mm_add_epi32(xmm6, _mm_shuffle_epi32(xmm6, 0x02));

    sads[0] = _mm_cvtsi128_si32(xmm4);
    sads[1] = _mm_cvtsi128_si32(xmm5);
    sads[2] = _mm_cvtsi128_si32(xmm6);

    if (nRef == 3)
        return;

    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x02));
    sads[3] = _mm_cvtsi128_si32(xmm7);
}

template <int height, int nRef>
static void SAD_8xN(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s sads[])
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* SAD accumulators */
    xmm4 = _mm_setzero_si128();
    xmm5 = _mm_setzero_si128();
    xmm6 = _mm_setzero_si128();
    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h += 4) {
        /* load 8x4 src block into two registers */
        xmm0 = _mm_loadl_epi64((__m128i *)(src + 0*src_stride));
        xmm0 = _mm_loadh_epi64(xmm0, (src + 1*src_stride));
        xmm1 = _mm_loadl_epi64((__m128i *)(src + 2*src_stride));
        xmm1 = _mm_loadh_epi64(xmm1, (src + 3*src_stride));
        src += 4*src_stride;

        /* load 8x4 ref block into two registers, calculate SAD, add to 32-bit accumulator */
        xmm2 = _mm_loadl_epi64((__m128i *)(ref0 + 0*ref_stride));
        xmm2 = _mm_loadh_epi64(xmm2, (ref0 + 1*ref_stride));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref0 + 2*ref_stride));
        xmm3 = _mm_loadh_epi64(xmm3, (ref0 + 3*ref_stride));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm4 = _mm_add_epi32(xmm4, xmm2);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        ref0 += 4*ref_stride;

        xmm2 = _mm_loadl_epi64((__m128i *)(ref1 + 0*ref_stride));
        xmm2 = _mm_loadh_epi64(xmm2, (ref1 + 1*ref_stride));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref1 + 2*ref_stride));
        xmm3 = _mm_loadh_epi64(xmm3, (ref1 + 3*ref_stride));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm5 = _mm_add_epi32(xmm5, xmm2);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        ref1 += 4*ref_stride;

        xmm2 = _mm_loadl_epi64((__m128i *)(ref2 + 0*ref_stride));
        xmm2 = _mm_loadh_epi64(xmm2, (ref2 + 1*ref_stride));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref2 + 2*ref_stride));
        xmm3 = _mm_loadh_epi64(xmm3, (ref2 + 3*ref_stride));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm6 = _mm_add_epi32(xmm6, xmm2);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        ref2 += 4*ref_stride;

        if (nRef == 3)
            continue;

        xmm2 = _mm_loadl_epi64((__m128i *)(ref3 + 0*ref_stride));
        xmm2 = _mm_loadh_epi64(xmm2, (ref3 + 1*ref_stride));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref3 + 2*ref_stride));
        xmm3 = _mm_loadh_epi64(xmm3, (ref3 + 3*ref_stride));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm7 = _mm_add_epi32(xmm7, xmm2);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        ref3 += 4*ref_stride;
    }

    /* sum low and high parts of SAD accumulator */
    xmm4 = _mm_add_epi32(xmm4, _mm_shuffle_epi32(xmm4, 0x02));
    xmm5 = _mm_add_epi32(xmm5, _mm_shuffle_epi32(xmm5, 0x02));
    xmm6 = _mm_add_epi32(xmm6, _mm_shuffle_epi32(xmm6, 0x02));

    sads[0] = _mm_cvtsi128_si32(xmm4);
    sads[1] = _mm_cvtsi128_si32(xmm5);
    sads[2] = _mm_cvtsi128_si32(xmm6);

    if (nRef == 3)
        return;

    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x02));
    sads[3] = _mm_cvtsi128_si32(xmm7);
}

template <int height, int nRef>
static void SAD_12xN(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s sads[])
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* SAD accumulators */
    xmm4 = _mm_setzero_si128();
    xmm5 = _mm_setzero_si128();
    xmm6 = _mm_setzero_si128();
    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h += 2) {
        /* load 12x2 src block into two registers */
        xmm0 = _mm_loadl_epi64((__m128i *)(src + 0*src_stride));
        xmm0 = _mm_insert_epi32(xmm0, *(int *)(src + 0*src_stride + 8), 2);
        xmm1 = _mm_loadl_epi64((__m128i *)(src + 1*src_stride));
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(src + 1*src_stride + 8), 2);
        src += 2*src_stride;

        /* load 12x2 ref block into two registers, calculate SAD, add to 32-bit accumulator */
        xmm2 = _mm_loadl_epi64((__m128i *)(ref0 + 0*ref_stride));
        xmm2 = _mm_insert_epi32(xmm2, *(int *)(ref0 + 0*ref_stride + 8), 2);
        xmm3 = _mm_loadl_epi64((__m128i *)(ref0 + 1*ref_stride));
        xmm3 = _mm_insert_epi32(xmm3, *(int *)(ref0 + 1*ref_stride + 8), 2);
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm4 = _mm_add_epi32(xmm4, xmm2);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        ref0 += 2*ref_stride;

        xmm2 = _mm_loadl_epi64((__m128i *)(ref1 + 0*ref_stride));
        xmm2 = _mm_insert_epi32(xmm2, *(int *)(ref1 + 0*ref_stride + 8), 2);
        xmm3 = _mm_loadl_epi64((__m128i *)(ref1 + 1*ref_stride));
        xmm3 = _mm_insert_epi32(xmm3, *(int *)(ref1 + 1*ref_stride + 8), 2);
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm5 = _mm_add_epi32(xmm5, xmm2);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        ref1 += 2*ref_stride;

        xmm2 = _mm_loadl_epi64((__m128i *)(ref2 + 0*ref_stride));
        xmm2 = _mm_insert_epi32(xmm2, *(int *)(ref2 + 0*ref_stride + 8), 2);
        xmm3 = _mm_loadl_epi64((__m128i *)(ref2 + 1*ref_stride));
        xmm3 = _mm_insert_epi32(xmm3, *(int *)(ref2 + 1*ref_stride + 8), 2);
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm6 = _mm_add_epi32(xmm6, xmm2);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        ref2 += 2*ref_stride;

        if (nRef == 3)
            continue;

        xmm2 = _mm_loadl_epi64((__m128i *)(ref3 + 0*ref_stride));
        xmm2 = _mm_insert_epi32(xmm2, *(int *)(ref3 + 0*ref_stride + 8), 2);
        xmm3 = _mm_loadl_epi64((__m128i *)(ref3 + 1*ref_stride));
        xmm3 = _mm_insert_epi32(xmm3, *(int *)(ref3 + 1*ref_stride + 8), 2);
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm7 = _mm_add_epi32(xmm7, xmm2);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        ref3 += 2*ref_stride;
    }

    /* sum low and high parts of SAD accumulator */
    xmm4 = _mm_add_epi32(xmm4, _mm_shuffle_epi32(xmm4, 0x02));
    xmm5 = _mm_add_epi32(xmm5, _mm_shuffle_epi32(xmm5, 0x02));
    xmm6 = _mm_add_epi32(xmm6, _mm_shuffle_epi32(xmm6, 0x02));

    sads[0] = _mm_cvtsi128_si32(xmm4);
    sads[1] = _mm_cvtsi128_si32(xmm5);
    sads[2] = _mm_cvtsi128_si32(xmm6);

    if (nRef == 3)
        return;

    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x02));
    sads[3] = _mm_cvtsi128_si32(xmm7);
}

template <int height, int nRef>
static void SAD_16xN(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s sads[])
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* SAD accumulators */
    xmm4 = _mm_setzero_si128();
    xmm5 = _mm_setzero_si128();
    xmm6 = _mm_setzero_si128();
    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h += 2) {
        /* load 16x2 src block into two registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src + 0*src_stride));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 1*src_stride));
        src += 2*src_stride;

        /* load 16x2 ref block into two registers, calculate SAD, add to 32-bit accumulator */
        xmm2 = _mm_loadu_si128((__m128i *)(ref0 + 0*ref_stride));
        xmm3 = _mm_loadu_si128((__m128i *)(ref0 + 1*ref_stride));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm4 = _mm_add_epi32(xmm4, xmm2);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        ref0 += 2*ref_stride;

        xmm2 = _mm_loadu_si128((__m128i *)(ref1 + 0*ref_stride));
        xmm3 = _mm_loadu_si128((__m128i *)(ref1 + 1*ref_stride));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm5 = _mm_add_epi32(xmm5, xmm2);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        ref1 += 2*ref_stride;

        xmm2 = _mm_loadu_si128((__m128i *)(ref2 + 0*ref_stride));
        xmm3 = _mm_loadu_si128((__m128i *)(ref2 + 1*ref_stride));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm6 = _mm_add_epi32(xmm6, xmm2);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        ref2 += 2*ref_stride;

        if (nRef == 3)
            continue;

        xmm2 = _mm_loadu_si128((__m128i *)(ref3 + 0*ref_stride));
        xmm3 = _mm_loadu_si128((__m128i *)(ref3 + 1*ref_stride));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm7 = _mm_add_epi32(xmm7, xmm2);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        ref3 += 2*ref_stride;
    }

    /* sum low and high parts of SAD accumulator */
    xmm4 = _mm_add_epi32(xmm4, _mm_shuffle_epi32(xmm4, 0x02));
    xmm5 = _mm_add_epi32(xmm5, _mm_shuffle_epi32(xmm5, 0x02));
    xmm6 = _mm_add_epi32(xmm6, _mm_shuffle_epi32(xmm6, 0x02));

    sads[0] = _mm_cvtsi128_si32(xmm4);
    sads[1] = _mm_cvtsi128_si32(xmm5);
    sads[2] = _mm_cvtsi128_si32(xmm6);

    if (nRef == 3)
        return;

    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x02));
    sads[3] = _mm_cvtsi128_si32(xmm7);
}

template <int height, int nRef>
static void SAD_24xN(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s sads[])
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* SAD accumulators */
    xmm4 = _mm_setzero_si128();
    xmm5 = _mm_setzero_si128();
    xmm6 = _mm_setzero_si128();
    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h++) {
        /* load 24-pixel src row into two registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src +  0));
        xmm1 = _mm_loadl_epi64((__m128i *)(src + 16));
        src += src_stride;

        /* load 24-pixel ref row into two registers, calculate SAD, add to 32-bit accumulator */
        xmm2 = _mm_loadu_si128((__m128i *)(ref0 +  0));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref0 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm4 = _mm_add_epi32(xmm4, xmm2);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        ref0 += ref_stride;

        xmm2 = _mm_loadu_si128((__m128i *)(ref1 +  0));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref1 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm5 = _mm_add_epi32(xmm5, xmm2);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        ref1 += ref_stride;

        xmm2 = _mm_loadu_si128((__m128i *)(ref2 +  0));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref2 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm6 = _mm_add_epi32(xmm6, xmm2);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        ref2 += ref_stride;

        if (nRef == 3)
            continue;

        xmm2 = _mm_loadu_si128((__m128i *)(ref3 +  0));
        xmm3 = _mm_loadl_epi64((__m128i *)(ref3 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm7 = _mm_add_epi32(xmm7, xmm2);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        ref3 += ref_stride;
    }

    /* sum low and high parts of SAD accumulator */
    xmm4 = _mm_add_epi32(xmm4, _mm_shuffle_epi32(xmm4, 0x02));
    xmm5 = _mm_add_epi32(xmm5, _mm_shuffle_epi32(xmm5, 0x02));
    xmm6 = _mm_add_epi32(xmm6, _mm_shuffle_epi32(xmm6, 0x02));

    sads[0] = _mm_cvtsi128_si32(xmm4);
    sads[1] = _mm_cvtsi128_si32(xmm5);
    sads[2] = _mm_cvtsi128_si32(xmm6);

    if (nRef == 3)
        return;

    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x02));
    sads[3] = _mm_cvtsi128_si32(xmm7);
}

template <int height, int nRef>
static void SAD_32xN(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s sads[])
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* SAD accumulators */
    xmm4 = _mm_setzero_si128();
    xmm5 = _mm_setzero_si128();
    xmm6 = _mm_setzero_si128();
    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h++) {
        /* load 32-pixel src row into two registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src +  0));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 16));
        src += src_stride;

        /* load 32-pixel ref row into two registers, calculate SAD, add to 32-bit accumulator */
        xmm2 = _mm_loadu_si128((__m128i *)(ref0 +  0));
        xmm3 = _mm_loadu_si128((__m128i *)(ref0 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm4 = _mm_add_epi32(xmm4, xmm2);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        ref0 += ref_stride;

        xmm2 = _mm_loadu_si128((__m128i *)(ref1 +  0));
        xmm3 = _mm_loadu_si128((__m128i *)(ref1 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm5 = _mm_add_epi32(xmm5, xmm2);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        ref1 += ref_stride;

        xmm2 = _mm_loadu_si128((__m128i *)(ref2 +  0));
        xmm3 = _mm_loadu_si128((__m128i *)(ref2 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm6 = _mm_add_epi32(xmm6, xmm2);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        ref2 += ref_stride;

        if (nRef == 3)
            continue;

        xmm2 = _mm_loadu_si128((__m128i *)(ref3 +  0));
        xmm3 = _mm_loadu_si128((__m128i *)(ref3 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm7 = _mm_add_epi32(xmm7, xmm2);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        ref3 += ref_stride;
    }

    /* sum low and high parts of SAD accumulator */
    xmm4 = _mm_add_epi32(xmm4, _mm_shuffle_epi32(xmm4, 0x02));
    xmm5 = _mm_add_epi32(xmm5, _mm_shuffle_epi32(xmm5, 0x02));
    xmm6 = _mm_add_epi32(xmm6, _mm_shuffle_epi32(xmm6, 0x02));

    sads[0] = _mm_cvtsi128_si32(xmm4);
    sads[1] = _mm_cvtsi128_si32(xmm5);
    sads[2] = _mm_cvtsi128_si32(xmm6);

    if (nRef == 3)
        return;

    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x02));
    sads[3] = _mm_cvtsi128_si32(xmm7);
}

template <int height, int nRef>
static void SAD_48xN(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s sads[])
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* SAD accumulators */
    xmm4 = _mm_setzero_si128();
    xmm5 = _mm_setzero_si128();
    xmm6 = _mm_setzero_si128();
    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h++) {
        /* load 48-pixel src row into three registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src +  0));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 16));
        xmm2 = _mm_loadu_si128((__m128i *)(src + 32));
        src += src_stride;

        /* load 48-pixel ref row into three registers, calculate SAD, add to 32-bit accumulator */
        xmm3 = _mm_loadu_si128((__m128i *)(ref0 +  0));
        xmm3 = _mm_sad_epu8(xmm3, xmm0);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        xmm3 = _mm_loadu_si128((__m128i *)(ref0 + 16));
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        xmm3 = _mm_loadu_si128((__m128i *)(ref0 + 32));
        xmm3 = _mm_sad_epu8(xmm3, xmm2);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        ref0 += ref_stride;

        xmm3 = _mm_loadu_si128((__m128i *)(ref1 +  0));
        xmm3 = _mm_sad_epu8(xmm3, xmm0);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        xmm3 = _mm_loadu_si128((__m128i *)(ref1 + 16));
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        xmm3 = _mm_loadu_si128((__m128i *)(ref1 + 32));
        xmm3 = _mm_sad_epu8(xmm3, xmm2);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        ref1 += ref_stride;

        xmm3 = _mm_loadu_si128((__m128i *)(ref2 +  0));
        xmm3 = _mm_sad_epu8(xmm3, xmm0);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        xmm3 = _mm_loadu_si128((__m128i *)(ref2 + 16));
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        xmm3 = _mm_loadu_si128((__m128i *)(ref2 + 32));
        xmm3 = _mm_sad_epu8(xmm3, xmm2);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        ref2 += ref_stride;

        if (nRef == 3)
            continue;

        xmm3 = _mm_loadu_si128((__m128i *)(ref3 +  0));
        xmm3 = _mm_sad_epu8(xmm3, xmm0);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        xmm3 = _mm_loadu_si128((__m128i *)(ref3 + 16));
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        xmm3 = _mm_loadu_si128((__m128i *)(ref3 + 32));
        xmm3 = _mm_sad_epu8(xmm3, xmm2);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        ref3 += ref_stride;
    }

    /* sum low and high parts of SAD accumulator */
    xmm4 = _mm_add_epi32(xmm4, _mm_shuffle_epi32(xmm4, 0x02));
    xmm5 = _mm_add_epi32(xmm5, _mm_shuffle_epi32(xmm5, 0x02));
    xmm6 = _mm_add_epi32(xmm6, _mm_shuffle_epi32(xmm6, 0x02));

    sads[0] = _mm_cvtsi128_si32(xmm4);
    sads[1] = _mm_cvtsi128_si32(xmm5);
    sads[2] = _mm_cvtsi128_si32(xmm6);

    if (nRef == 3)
        return;

    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x02));
    sads[3] = _mm_cvtsi128_si32(xmm7);
}

template <int height, int nRef>
static void SAD_64xN(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s sads[])
{
    int h;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* SAD accumulators */
    xmm4 = _mm_setzero_si128();
    xmm5 = _mm_setzero_si128();
    xmm6 = _mm_setzero_si128();
    xmm7 = _mm_setzero_si128();

    for (h = 0; h < height; h++) {
        /* load first 32 pixels of src row into two registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src +  0));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 16));

        /* load first 32 pixels of ref row into two registers, calculate SAD, add to 32-bit accumulator */
        xmm2 = _mm_loadu_si128((__m128i *)(ref0 +  0));
        xmm3 = _mm_loadu_si128((__m128i *)(ref0 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm4 = _mm_add_epi32(xmm4, xmm2);
        xmm4 = _mm_add_epi32(xmm4, xmm3);

        xmm2 = _mm_loadu_si128((__m128i *)(ref1 +  0));
        xmm3 = _mm_loadu_si128((__m128i *)(ref1 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm5 = _mm_add_epi32(xmm5, xmm2);
        xmm5 = _mm_add_epi32(xmm5, xmm3);

        xmm2 = _mm_loadu_si128((__m128i *)(ref2 +  0));
        xmm3 = _mm_loadu_si128((__m128i *)(ref2 + 16));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm6 = _mm_add_epi32(xmm6, xmm2);
        xmm6 = _mm_add_epi32(xmm6, xmm3);

        if (nRef == 4) {
            xmm2 = _mm_loadu_si128((__m128i *)(ref3 +  0));
            xmm3 = _mm_loadu_si128((__m128i *)(ref3 + 16));
            xmm2 = _mm_sad_epu8(xmm2, xmm0);
            xmm3 = _mm_sad_epu8(xmm3, xmm1);
            xmm7 = _mm_add_epi32(xmm7, xmm2);
            xmm7 = _mm_add_epi32(xmm7, xmm3);
        }

        /* load second 32 pixels of src row into two registers */
        xmm0 = _mm_loadu_si128((__m128i *)(src + 32));
        xmm1 = _mm_loadu_si128((__m128i *)(src + 48));
        src += src_stride;

        /* load second 32 pixels of ref row into two registers, calculate SAD, add to 32-bit accumulator */
        xmm2 = _mm_loadu_si128((__m128i *)(ref0 + 32));
        xmm3 = _mm_loadu_si128((__m128i *)(ref0 + 48));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm4 = _mm_add_epi32(xmm4, xmm2);
        xmm4 = _mm_add_epi32(xmm4, xmm3);
        ref0 += ref_stride;

        xmm2 = _mm_loadu_si128((__m128i *)(ref1 + 32));
        xmm3 = _mm_loadu_si128((__m128i *)(ref1 + 48));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm5 = _mm_add_epi32(xmm5, xmm2);
        xmm5 = _mm_add_epi32(xmm5, xmm3);
        ref1 += ref_stride;

        xmm2 = _mm_loadu_si128((__m128i *)(ref2 + 32));
        xmm3 = _mm_loadu_si128((__m128i *)(ref2 + 48));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm6 = _mm_add_epi32(xmm6, xmm2);
        xmm6 = _mm_add_epi32(xmm6, xmm3);
        ref2 += ref_stride;

        if (nRef == 3)
            continue;

        xmm2 = _mm_loadu_si128((__m128i *)(ref3 + 32));
        xmm3 = _mm_loadu_si128((__m128i *)(ref3 + 48));
        xmm2 = _mm_sad_epu8(xmm2, xmm0);
        xmm3 = _mm_sad_epu8(xmm3, xmm1);
        xmm7 = _mm_add_epi32(xmm7, xmm2);
        xmm7 = _mm_add_epi32(xmm7, xmm3);
        ref3 += ref_stride;
    }

    /* sum low and high parts of SAD accumulator */
    xmm4 = _mm_add_epi32(xmm4, _mm_shuffle_epi32(xmm4, 0x02));
    xmm5 = _mm_add_epi32(xmm5, _mm_shuffle_epi32(xmm5, 0x02));
    xmm6 = _mm_add_epi32(xmm6, _mm_shuffle_epi32(xmm6, 0x02));

    sads[0] = _mm_cvtsi128_si32(xmm4);
    sads[1] = _mm_cvtsi128_si32(xmm5);
    sads[2] = _mm_cvtsi128_si32(xmm6);

    if (nRef == 3)
        return;

    xmm7 = _mm_add_epi32(xmm7, _mm_shuffle_epi32(xmm7, 0x02));
    sads[3] = _mm_cvtsi128_si32(xmm7);
}

void H265_FASTCALL MAKE_NAME(h265_SAD_MxN_x3_8u)(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[3])
{
    switch (width) {
    case 4:
        if (height == 4)        SAD_4xN< 4, 3> (src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 8)   SAD_4xN< 8, 3> (src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 16)  SAD_4xN<16, 3> (src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        break;
    case 8:
        if (height == 4)        SAD_8xN< 4, 3> (src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 8)   SAD_8xN< 8, 3> (src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 16)  SAD_8xN<16, 3> (src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 32)  SAD_8xN<32, 3> (src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        break;
    case 12:
        if (height == 16)       SAD_12xN<16, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        break;
    case 16:
        if (height == 4)        SAD_16xN< 4, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 8)   SAD_16xN< 8, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 12)  SAD_16xN<12, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 16)  SAD_16xN<16, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 32)  SAD_16xN<32, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 64)  SAD_16xN<64, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        break;
    case 24:
        if (height == 32)       SAD_24xN<32, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        break;
    case 32:
        if (height == 8)        SAD_32xN< 8, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 16)  SAD_32xN<16, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 24)  SAD_32xN<24, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 32)  SAD_32xN<32, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 64)  SAD_32xN<64, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        break;
    case 48:
        if (height == 64)       SAD_48xN<64, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        break;
    case 64:
        if (height == 16)       SAD_64xN<16, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 32)  SAD_64xN<32, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 48)  SAD_64xN<48, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        else if (height == 64)  SAD_64xN<64, 3>(src, src_stride, ref0, ref1, ref2, 0, ref_stride, sads);
        break;
    default:
        /* error - unsupported size */
        break;
    }
}

void H265_FASTCALL MAKE_NAME(h265_SAD_MxN_x4_8u)(const Ipp8u* src, Ipp32s src_stride, const Ipp8u* ref0, const Ipp8u* ref1, const Ipp8u* ref2, const Ipp8u* ref3, Ipp32s ref_stride, Ipp32s width, Ipp32s height, Ipp32s sads[4])
{
    switch (width) {
    case 4:
        if (height == 4)        SAD_4xN< 4, 4> (src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 8)   SAD_4xN< 8, 4> (src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 16)  SAD_4xN<16, 4> (src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        break;
    case 8:
        if (height == 4)        SAD_8xN< 4, 4> (src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 8)   SAD_8xN< 8, 4> (src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 16)  SAD_8xN<16, 4> (src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 32)  SAD_8xN<32, 4> (src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        break;
    case 12:
        if (height == 16)       SAD_12xN<16, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        break;
    case 16:
        if (height == 4)        SAD_16xN< 4, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 8)   SAD_16xN< 8, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 12)  SAD_16xN<12, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 16)  SAD_16xN<16, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 32)  SAD_16xN<32, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 64)  SAD_16xN<64, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        break;
    case 24:
        if (height == 32)       SAD_24xN<32, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        break;
    case 32:
        if (height == 8)        SAD_32xN< 8, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 16)  SAD_32xN<16, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 24)  SAD_32xN<24, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 32)  SAD_32xN<32, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 64)  SAD_32xN<64, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        break;
    case 48:
        if (height == 64)       SAD_48xN<64, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        break;
    case 64:
        if (height == 16)       SAD_64xN<16, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 32)  SAD_64xN<32, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 48)  SAD_64xN<48, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        else if (height == 64)  SAD_64xN<64, 4>(src, src_stride, ref0, ref1, ref2, ref3, ref_stride, sads);
        break;
    default:
        /* error - unsupported size */
        break;
    }
}

} // end namespace MFX_HEVC_PP

#endif
#endif

