//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#include "mfxvideo.h"
#include "ippvc.h"
#include <string.h>
#include <algorithm>

#include "mfx_dispatcher.h"

#include "intrin.h"

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

#define _mm_storeh_epi64(p, A) _mm_storeh_pd((double *)(p), _mm_castsi128_pd(A))

namespace MFX_PP
{

typedef enum _FilterType
{
    VERT_FILT = 0,
    HOR_FILT = 1,
} FilterType;

ALIGN_DECL(32) static const unsigned short RndTab1[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
ALIGN_DECL(32) static const unsigned short RndTab2[16] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
ALIGN_DECL(32) static const unsigned short RndTab4[16] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

ALIGN_DECL(32) static const unsigned short HalfSignTab[16] = {0, 0, 0, 0, -1, -1, -1, -1, 0, 0, 0, 0, -1, -1, -1, -1};

template<typename Plane, FilterType dir, mfxU32 isStrong, mfxU32 isPair>
static __inline void DeblockLumaKernel(Plane *srcDst, mfxU32 pitch, mfxU32 alpha, mfxU32 beta, mfxU8 *pClip, Ipp32s bitDepth)
{
    _mm256_zeroupper();

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
    __m256i yFiltMask, yAlpha, yBeta, yBFSMask, yClip, yClip1;

    ymm0 = _mm256_setzero_si256();
    ymm1 = _mm256_setzero_si256();
    ymm2 = _mm256_setzero_si256();
    ymm3 = _mm256_setzero_si256();
    ymm4 = _mm256_setzero_si256();
    ymm5 = _mm256_setzero_si256();
    ymm6 = _mm256_setzero_si256();
    ymm7 = _mm256_setzero_si256();

    if (dir == VERT_FILT) {
        // transpose 4x8 -> 8x4 (each block)
        ymm0 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 0*pitch)));   // row 0: [0-7]
        ymm1 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 1*pitch)));   // row 1: [0-7]
        ymm2 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 2*pitch)));   // row 2: [0-7]
        ymm3 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 3*pitch)));   // row 3: [0-7]

        if (isPair) {
            ymm0 = _mm256_permute2x128_si256(ymm0, mm256(_mm_loadu_si128((__m128i *)(srcDst + 4*pitch))), 0x20);    // row 4: [0-7]
            ymm1 = _mm256_permute2x128_si256(ymm1, mm256(_mm_loadu_si128((__m128i *)(srcDst + 5*pitch))), 0x20);    // row 5: [0-7]
            ymm2 = _mm256_permute2x128_si256(ymm2, mm256(_mm_loadu_si128((__m128i *)(srcDst + 6*pitch))), 0x20);    // row 6: [0-7]
            ymm3 = _mm256_permute2x128_si256(ymm3, mm256(_mm_loadu_si128((__m128i *)(srcDst + 7*pitch))), 0x20);    // row 7: [0-7]
        }

        ymm4 = _mm256_unpacklo_epi16(ymm0, ymm1);       // 00 10 01 11 02 12 03 13
        ymm5 = _mm256_unpacklo_epi16(ymm2, ymm3);       // 20 30 21 31 22 32 23 33
        ymm6 = _mm256_unpackhi_epi16(ymm0, ymm1);       // 04 14 05 15 06 16 07 17
        ymm7 = _mm256_unpackhi_epi16(ymm2, ymm3);       // 24 34 25 35 26 36 27 37

        ymm0 = _mm256_unpacklo_epi32(ymm4, ymm5);       // 00 10 20 30 01 11 21 31 = [p3 | p2]
        ymm2 = _mm256_unpackhi_epi32(ymm4, ymm5);       // 02 12 22 32 03 13 23 33 = [p3 | p2]
        ymm4 = _mm256_unpacklo_epi32(ymm6, ymm7);       // 04 14 24 34 05 15 25 35 = [q0 | q1]
        ymm6 = _mm256_unpackhi_epi32(ymm6, ymm7);       // 06 16 26 36 07 17 27 37 = [q2 | q3]

        ymm1 = _mm256_srli_si256(ymm0, 8);
        ymm3 = _mm256_srli_si256(ymm2, 8);
        ymm5 = _mm256_srli_si256(ymm4, 8);
        ymm7 = _mm256_srli_si256(ymm6, 8);
    } else {
        if (isPair) {
            ymm1 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 1*pitch)));       // p2
            ymm2 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 2*pitch)));       // p1
            ymm3 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 3*pitch)));       // p0
            ymm4 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 4*pitch)));       // q0
            ymm5 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 5*pitch)));       // q1
            ymm6 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 6*pitch)));       // q2

            ymm1 = _mm256_permute4x64_epi64(ymm1, 0xd8);
            ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);
            ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
            ymm4 = _mm256_permute4x64_epi64(ymm4, 0xd8);
            ymm5 = _mm256_permute4x64_epi64(ymm5, 0xd8);
            ymm6 = _mm256_permute4x64_epi64(ymm6, 0xd8);

            if (isStrong) {
                ymm0 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 0*pitch)));   // p3
                ymm7 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 7*pitch)));   // q3
                ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
                ymm7 = _mm256_permute4x64_epi64(ymm7, 0xd8);
            }
        } else {
            ymm1 = mm256(_mm_loadl_epi64((__m128i *)(srcDst + 1*pitch)));       // p2
            ymm2 = mm256(_mm_loadl_epi64((__m128i *)(srcDst + 2*pitch)));       // p1
            ymm3 = mm256(_mm_loadl_epi64((__m128i *)(srcDst + 3*pitch)));       // p0
            ymm4 = mm256(_mm_loadl_epi64((__m128i *)(srcDst + 4*pitch)));       // q0
            ymm5 = mm256(_mm_loadl_epi64((__m128i *)(srcDst + 5*pitch)));       // q1
            ymm6 = mm256(_mm_loadl_epi64((__m128i *)(srcDst + 6*pitch)));       // q2

            if (isStrong) {
                ymm0 = mm256(_mm_loadl_epi64((__m128i *)(srcDst + 0*pitch)));   // p3
                ymm7 = mm256(_mm_loadl_epi64((__m128i *)(srcDst + 7*pitch)));   // q3
            }
        }
    }

    ymm0 = _mm256_unpacklo_epi64(ymm0, ymm7);
    ymm1 = _mm256_unpacklo_epi64(ymm1, ymm6);
    ymm2 = _mm256_unpacklo_epi64(ymm2, ymm5);
    ymm3 = _mm256_unpacklo_epi64(ymm3, ymm4);
    ymm4 = _mm256_unpacklo_epi64(ymm4, ymm3);
    ymm5 = _mm256_unpacklo_epi64(ymm5, ymm2);
    
    yAlpha = _mm256_set1_epi16((unsigned short)alpha);
    ymm6 = _mm256_sub_epi16(ymm3, ymm4);                            // [p0 - q0 | q0 - p0]
    ymm6 = _mm256_abs_epi16(ymm6);                                  // [abs(p0 - q0) | abs(q0 - p0)]
    ymm6 = _mm256_add_epi16(ymm6, *(__m256i *)(RndTab1));           // (Aedge + 1) > alpha same as (Aedge >= alpha)
    yFiltMask = _mm256_cmpgt_epi16(ymm6, yAlpha);

    yBeta = _mm256_set1_epi16((unsigned short)beta);
    ymm7 = _mm256_sub_epi16(ymm2, ymm3);                            // [p1 - p0 | q1 - q0]
    ymm7 = _mm256_abs_epi16(ymm7);                                  // [abs(p1 - p0) | abs(q1 - q0)]
    ymm7 = _mm256_add_epi16(ymm7, *(__m256i *)(RndTab1));
    ymm7 = _mm256_cmpgt_epi16(ymm7, yBeta);
    ymm7 = _mm256_or_si256(ymm7, _mm256_alignr_epi8(ymm7, ymm7, 8));
    yFiltMask = _mm256_or_si256(yFiltMask, ymm7);              

    // early exit if all zero (i.e. any condition is TRUE)
    yFiltMask = _mm256_xor_si256(yFiltMask, _mm256_set1_epi16(-1));
    if (_mm256_testz_si256(yFiltMask, _mm256_set1_epi16(-1)))
        return;     

    if (isStrong) {
        // strong filter
        ymm6 = _mm256_sub_epi16(ymm3, ymm4);                        // [p0 - q0 | q0 - p0]
        ymm6 = _mm256_abs_epi16(ymm6);                              // [abs(p0 - q0) | abs(q0 - p0)]
        yAlpha = _mm256_srai_epi16(yAlpha, 2);                      //  Alpha >> 2
        yAlpha = _mm256_add_epi16(yAlpha, *(__m256i *)(RndTab2));   // (Alpha >> 2) + 2
        yBFSMask = _mm256_cmpgt_epi16(yAlpha, ymm6);                // set if (Alpha >> 2) + 2 > Aedge

        ymm6 = _mm256_sub_epi16(ymm1, ymm3);                        // [p2 - p0 | q2 - q0]
        ymm6 = _mm256_abs_epi16(ymm6);                              // [abs(p2 -p0) | abs(q2 - q0)]
        ymm6 = _mm256_cmpgt_epi16(yBeta, ymm6);                     // set if < beta
        yBFSMask = _mm256_and_si256(ymm6, yBFSMask);                // bFS && (Ap2 < beta)
        yBFSMask = _mm256_and_si256(yBFSMask, yFiltMask);

        // lower half (4x16) calculates P outputs, upper half calculates Q outputs
        ymm7 = _mm256_add_epi16(ymm2, ymm3);
        ymm7 = _mm256_add_epi16(ymm7, ymm4);                        // tmp = (p1 + p0 + q0) | (q1 + q0 + p0)

        // ymm3 = [p0 | q0] (!bFullStrong)
        // NOTE - this is same as chroma filter
        ymm6 = _mm256_add_epi16(ymm2, ymm2);                        // 2*p1
        ymm6 = _mm256_add_epi16(ymm6, ymm3);                        // 2*p1 + p0
        ymm6 = _mm256_add_epi16(ymm6, ymm5);                        // 2*p1 + p0 + q1
        ymm6 = _mm256_add_epi16(ymm6, *(__m256i *)(RndTab2));       // 2*p1 + p0 + q1 + 2
        ymm6 = _mm256_srai_epi16(ymm6, 2);                          // (2*p1 + p0 + q1 + 2) >> 2
        ymm3 = _mm256_blendv_epi8(ymm3, ymm6, yFiltMask);

        // ymm3 = [p0 | q0]
        ymm6 = _mm256_add_epi16(ymm7, ymm7);                        // 2*tmp
        ymm6 = _mm256_add_epi16(ymm6, ymm1);                        // p2 + 2*tmp
        ymm6 = _mm256_add_epi16(ymm6, ymm5);                        // p2 + 2*tmp + q1
        ymm6 = _mm256_add_epi16(ymm6, *(__m256i *)(RndTab4));       // p2 + 2*tmp + q1 + 4
        ymm6 = _mm256_srai_epi16(ymm6, 3);                          // (p2 + 2*tmp + q1 + 4) >> 3
        ymm3 = _mm256_blendv_epi8(ymm3, ymm6, yBFSMask);            // 30 31 32 33 40 41 42 43

        // ymm2 = [p1 | q1]
        ymm6 = _mm256_add_epi16(ymm7, ymm1);                        // p2 + tmp
        ymm6 = _mm256_add_epi16(ymm6, *(__m256i *)(RndTab2));       // p2 + tmp + 2
        ymm6 = _mm256_srai_epi16(ymm6, 2);                          // (p2 + tmp + 2) >> 2
        ymm2 = _mm256_blendv_epi8(ymm2, ymm6, yBFSMask);            // 20 21 22 23 50 51 52 53

        // ymm1 = [p2 | q2]
        ymm6 = _mm256_add_epi16(ymm7, ymm0);                        // p3 + tmp
        ymm6 = _mm256_add_epi16(ymm6, ymm0);                        // 2*p3 + tmp
        ymm6 = _mm256_add_epi16(ymm6, ymm1);                        // 2*p3 + p2 + tmp
        ymm6 = _mm256_add_epi16(ymm6, ymm1);                        // 2*p3 + 2*p2 + tmp
        ymm6 = _mm256_add_epi16(ymm6, ymm1);                        // 2*p3 + 3*p2 + tmp
        ymm6 = _mm256_add_epi16(ymm6, *(__m256i *)(RndTab4));       // 2*p3 + 3*p2 + tmp + 4
        ymm6 = _mm256_srai_epi16(ymm6, 3);                          // (2*p3 + 3*p2 + tmp + 4) >> 3
        ymm1 = _mm256_blendv_epi8(ymm1, ymm6, yBFSMask);            // 10 11 12 13 60 61 62 63
    } else {
        // weak filter
        ymm6 = _mm256_sub_epi16(ymm1, ymm3);                        // [p2-p0 | q2-q0]
        ymm6 = _mm256_abs_epi16(ymm6);                              // [Ap2 | Aq2]
        yBeta = _mm256_cmpgt_epi16(yBeta, ymm6);                    // set if [Ap2 < beta | Aq2 < beta] (used several times as mask)

        ymm6 = _mm256_add_epi16(yBeta, _mm256_srli_si256(yBeta, 8));    // add top half onto low half
        ymm6 = _mm256_unpacklo_epi64(ymm6, ymm6);                       // replicate low into high - now each lane will contain [0,-1,-2]

        // scale by bitDepth
        if (isPair) {
            mfxU32 uClip0 = pClip[0] * (1 << (bitDepth-8));
            mfxU32 uClip1 = pClip[1] * (1 << (bitDepth-8));
            yClip  = _mm256_permute2x128_si256(mm256(_mm_set1_epi16(uClip0)), mm256(_mm_set1_epi16(uClip1)), 0x20);
        } else {
            mfxU32 uClip0 = pClip[0] * (1 << (bitDepth-8));
            yClip  = _mm256_set1_epi16(uClip0);
        }
        yClip1 = _mm256_sub_epi16(yClip, ymm6);                     // uClip1 (+1 for each of Ap2 < beta, Aq1 < beta)

        // ymm3 = [p0 | q0]
        ymm6 = _mm256_sub_epi16(ymm4, ymm3);                        //  q0 - p0
        ymm6 = _mm256_slli_epi16(ymm6, 2);                          // (q0 - p0) << 2
        ymm6 = _mm256_add_epi16(ymm6, ymm2);                        // (q0 - p0) << 2 + p1
        ymm6 = _mm256_sub_epi16(ymm6, ymm5);                        // (q0 - p0) << 2 + p1 - q1
        ymm6 = _mm256_add_epi16(ymm6, *(__m256i *)RndTab4);         // (q0 - p0) << 2 + p1 - q1 + 4
        ymm6 = _mm256_srai_epi16(ymm6, 3);                          // [...] >> 3
        ymm6 = _mm256_unpacklo_epi64(ymm6, ymm6);                   // replicate to high half

        ymm6 = _mm256_min_epi16(ymm6, yClip1);                      // clip iDelta to [-uClip1, +uClip1]
        yClip1 = _mm256_sub_epi16(_mm256_setzero_si256(), yClip1);
        ymm6 = _mm256_max_epi16(ymm6, yClip1);

        ymm6 = _mm256_xor_si256(ymm6, *(__m256i *)HalfSignTab);     // negate top 4 values
        ymm6 = _mm256_sub_epi16(ymm6, *(__m256i *)HalfSignTab);
        ymm6 = _mm256_add_epi16(ymm6, ymm3);                        // [p0 + iDelta | q0 - iDelta]        

        ymm6 = _mm256_max_epi16(ymm6, _mm256_setzero_si256());      // clip to [0, 2^bitDepth - 1]
        ymm6 = _mm256_min_epi16(ymm6, _mm256_set1_epi16((1 << bitDepth) -1));
        ymm6 = _mm256_blendv_epi8(ymm3, ymm6, yFiltMask);

        // ymm2 = [p1 | q1]
        ymm7 = _mm256_add_epi16(ymm3, ymm4);                        //  p0 + q0
        ymm7 = _mm256_add_epi16(ymm7, *(__m256i *)RndTab1);         //  p0 + q0 + 1
        ymm7 = _mm256_srai_epi16(ymm7, 1);                          // (p0 + q0 + 1) >> 1
        ymm7 = _mm256_add_epi16(ymm7, ymm1);                        // p2 + (p0 + q0 + 1) >> 1
        ymm7 = _mm256_sub_epi16(ymm7, ymm2);                        // p2 - p1 + (p0 + q0 + 1) >> 1
        ymm7 = _mm256_sub_epi16(ymm7, ymm2);                        // p2 - 2*p1 + (p0 + q0 + 1) >> 1
        ymm7 = _mm256_srai_epi16(ymm7, 1);                          // [...] >> 1
        
        ymm7 = _mm256_min_epi16(ymm7, yClip);                       // clip iDelta to [-uClip, +uClip]
        yClip = _mm256_sub_epi16(_mm256_setzero_si256(), yClip);
        ymm7 = _mm256_max_epi16(ymm7, yClip);

        ymm7 = _mm256_add_epi16(ymm2, ymm7);                        // p1 + iDelta | q1 + iDelta
        yBeta = _mm256_and_si256(yBeta, yFiltMask);
        ymm2 = _mm256_blendv_epi8(ymm2, ymm7, yBeta);
        
        ymm3 = ymm6;                                                // [p0 | q0]
    }

    if (dir == VERT_FILT) {
        // transpose back 8x4 -> 4x8
        ymm4 = _mm256_unpacklo_epi16(ymm0, ymm1);               // 00 10 01 11 02 12 03 13
        ymm5 = _mm256_unpacklo_epi16(ymm2, ymm3);               // 20 30 21 31 22 32 23 33
        ymm6 = _mm256_unpackhi_epi16(ymm3, ymm2);               // 40 50 41 51 42 52 43 53
        ymm7 = _mm256_unpackhi_epi16(ymm1, ymm0);               // 60 70 61 71 62 72 63 73

        ymm0 = _mm256_unpacklo_epi32(ymm4, ymm5);               // 00 10 20 30 01 11 21 31
        ymm1 = _mm256_unpacklo_epi32(ymm6, ymm7);               // 40 50 60 70 41 51 61 71
        ymm2 = _mm256_unpackhi_epi32(ymm4, ymm5);               // 02 12 22 32 03 13 23 33
        ymm3 = _mm256_unpackhi_epi32(ymm6, ymm7);               // 42 52 62 72 43 53 63 73

        ymm4 = _mm256_unpacklo_epi64(ymm0, ymm1);               // 00 10 20 30 40 50 60 70
        ymm5 = _mm256_unpackhi_epi64(ymm0, ymm1);               // 01 11 21 31 41 51 61 71
        ymm6 = _mm256_unpacklo_epi64(ymm2, ymm3);               // 02 12 22 32 42 52 62 72
        ymm7 = _mm256_unpackhi_epi64(ymm2, ymm3);               // 03 13 23 33 43 53 63 73

        _mm_storeu_si128((__m128i *)(srcDst + 0*pitch), mm128(ymm4));
        _mm_storeu_si128((__m128i *)(srcDst + 1*pitch), mm128(ymm5));
        _mm_storeu_si128((__m128i *)(srcDst + 2*pitch), mm128(ymm6));
        _mm_storeu_si128((__m128i *)(srcDst + 3*pitch), mm128(ymm7));

        if (isPair) {
            _mm_storeu_si128((__m128i *)(srcDst + 4*pitch), mm128(_mm256_permute2x128_si256(ymm4, ymm4, 0x01)));
            _mm_storeu_si128((__m128i *)(srcDst + 5*pitch), mm128(_mm256_permute2x128_si256(ymm5, ymm5, 0x01)));
            _mm_storeu_si128((__m128i *)(srcDst + 6*pitch), mm128(_mm256_permute2x128_si256(ymm6, ymm6, 0x01)));
            _mm_storeu_si128((__m128i *)(srcDst + 7*pitch), mm128(_mm256_permute2x128_si256(ymm7, ymm7, 0x01)));
        }
    } else {
        if (isPair) {
            ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);
            ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
            _mm_storeu_si128((__m128i *)(srcDst + 2*pitch), mm128(ymm2));
            _mm_storeu_si128((__m128i *)(srcDst + 3*pitch), mm128(ymm3));

            ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01);
            ymm3 = _mm256_permute2x128_si256(ymm3, ymm3, 0x01);
            _mm_storeu_si128((__m128i *)(srcDst + 4*pitch), mm128(ymm3));
            _mm_storeu_si128((__m128i *)(srcDst + 5*pitch), mm128(ymm2));

            if (isStrong) {
                ymm1 = _mm256_permute4x64_epi64(ymm1, 0xd8);
                _mm_storeu_si128((__m128i *)(srcDst + 1*pitch), mm128(ymm1));
                ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 0x01);
                _mm_storeu_si128((__m128i *)(srcDst + 6*pitch), mm128(ymm1));
            }
        } else {
            _mm_storel_epi64((__m128i *)(srcDst + 2*pitch), mm128(ymm2));
            _mm_storel_epi64((__m128i *)(srcDst + 3*pitch), mm128(ymm3));
            _mm_storeh_epi64((__m128i *)(srcDst + 4*pitch), mm128(ymm3));
            _mm_storeh_epi64((__m128i *)(srcDst + 5*pitch), mm128(ymm2));
            if (isStrong) {
                _mm_storel_epi64((__m128i *)(srcDst + 1*pitch), mm128(ymm1));
                _mm_storeh_epi64((__m128i *)(srcDst + 6*pitch), mm128(ymm1));
            }
        }
    }
}

//  srcDst = 16x16 block with pitch scrDstStep (mfxU8 or mfxU16) - input/output
//  pAlpha[4] = values from ALPHA_TABLE[52]
//  pBeta[4]  = values from BETA_TABLE[52]
//  pThresholds[16] = values from CLIP_TABLE[52][5] (for chroma, have pThresholds[32] for separate U/V
//  pBS[16] = values in range [0,4]
//  bitDepth = 8, 10

template<typename Plane, FilterType dir>
static void ippiFilterDeblockingLuma_H264_kernel(Plane* srcDst, Ipp32s  srcDstStep,
                                    Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                    Ipp8u*  pThresholds, Ipp8u*  pBS,
                                    Ipp32s  bitDepth)
{
    mfxI32 i,j;
    Plane *pEdge;
    mfxU32 alpha, beta;
    mfxU8 *pStrong;
    mfxU8 *pClip;
    mfxI32 bitDepthShift;

    int pitchEdge, pitchGrp;

    bitDepthShift = bitDepth - 8;

    if (dir == VERT_FILT) {
        pitchEdge = 1;
        pitchGrp  = srcDstStep;
    } else {
        pitchEdge = srcDstStep;
        pitchGrp  = 1;
    }

    for (j = 0; j < 4; j++) {
        alpha   = pAlpha[j>0] << bitDepthShift;
        beta    = pBeta [j>0] << bitDepthShift;
        pStrong = pBS + 4*j;
        pClip   = pThresholds + 4*j;
        pEdge   = (Plane*)srcDst + 4*j*pitchEdge;

        if (0 == *((int *) pStrong))    // early exit if next 4 are all 0
            continue;

        // for vertical edges:   do 4 rows per pass, then move down and do next 4 rows...
        // for horizontal edges: do 4 cols per pass, then move right and do next 4 cols...
        // if two next blocks are both weak filter (common), do a pair with AVX2 (not worth the overhead to check for both strong)
        if (dir == VERT_FILT) {
            for (i = 0; i < 4; i++, pEdge += 4*pitchGrp) {
                if (i < 3 && pStrong[i+0] >= 1 && pStrong[i+0] <= 3 && pStrong[i+1] >= 1 && pStrong[i+1] <= 3) {
                    DeblockLumaKernel<Plane, VERT_FILT, 0, 1>(&pEdge[-4], srcDstStep, alpha, beta, pClip + i, bitDepth);
                    i++;
                    pEdge += 4*pitchGrp;
                } else if (pStrong[i+0] == 4) {
                    DeblockLumaKernel<Plane, VERT_FILT, 1, 0>(&pEdge[-4], srcDstStep, alpha, beta, pClip + i, bitDepth);
                } else if (pStrong[i+0] >= 1) {
                    DeblockLumaKernel<Plane, VERT_FILT, 0, 0>(&pEdge[-4], srcDstStep, alpha, beta, pClip + i, bitDepth);
                }
            }
        } else {
            for (i = 0; i < 4; i++, pEdge += 4*pitchGrp) {
                if (i < 3 && pStrong[i+0] >= 1 && pStrong[i+0] <= 3 && pStrong[i+1] >= 1 && pStrong[i+1] <= 3) {
                    DeblockLumaKernel<Plane, HOR_FILT, 0, 1>(&pEdge[-4*srcDstStep], srcDstStep, alpha, beta, pClip + i, bitDepth);
                    i++;
                    pEdge += 4*pitchGrp;
                } else if (pStrong[i+0] == 4) {
                    DeblockLumaKernel<Plane, HOR_FILT, 1, 0>(&pEdge[-4*srcDstStep], srcDstStep, alpha, beta, pClip + i, bitDepth);
                } else if (pStrong[i+0] >= 1) {
                    DeblockLumaKernel<Plane, HOR_FILT, 0, 0>(&pEdge[-4*srcDstStep], srcDstStep, alpha, beta, pClip + i, bitDepth);
                }
            }
        }
    }
}

ALIGN_DECL(32) static const unsigned char ShufTab_Strength_V_422[4][32] = {
    {0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1,  8, -1,  8, -1,  8, -1,  8, -1,  8, -1,  8, -1,  8, -1,  8, -1},
    {1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1,  9, -1,  9, -1,  9, -1,  9, -1,  9, -1,  9, -1,  9, -1,  9, -1},
    {2, -1, 2, -1, 2, -1, 2, -1, 2, -1, 2, -1, 2, -1, 2, -1, 10, -1, 10, -1, 10, -1, 10, -1, 10, -1, 10, -1, 10, -1, 10, -1},
    {3, -1, 3, -1, 3, -1, 3, -1, 3, -1, 3, -1, 3, -1, 3, -1, 11, -1, 11, -1, 11, -1, 11, -1, 11, -1, 11, -1, 11, -1, 11, -1},
};

ALIGN_DECL(32) static const unsigned char ShufTab_Strength_V_420[2][32] = {
    {0, -1, 0, -1, 0, -1, 0, -1, 1, -1, 1, -1, 1, -1, 1, -1,  8, -1,  8, -1,  8, -1,  8, -1,  9, -1,  9, -1,  9, -1,  9, -1},
    {2, -1, 2, -1, 2, -1, 2, -1, 3, -1, 3, -1, 3, -1, 3, -1, 10, -1, 10, -1, 10, -1, 10, -1, 11, -1, 11, -1, 11, -1, 11, -1},
};

ALIGN_DECL(32) static const unsigned char ShufTab_Strength_H_422[4][32] = {
    { 0, -1,  0, -1,  0, -1,  0, -1,  1, -1,  1, -1,  1, -1,  1, -1,  2, -1,  2, -1,  2, -1,  2, -1,  3, -1,  3, -1,  3, -1,  3, -1},
    { 4, -1,  4, -1,  4, -1,  4, -1,  5, -1,  5, -1,  5, -1,  5, -1,  6, -1,  6, -1,  6, -1,  6, -1,  7, -1,  7, -1,  7, -1,  7, -1},
    { 8, -1,  8, -1,  8, -1,  8, -1,  9, -1,  9, -1,  9, -1,  9, -1, 10, -1, 10, -1, 10, -1, 10, -1, 11, -1, 11, -1, 11, -1, 11, -1},
    {12, -1, 12, -1, 12, -1, 12, -1, 13, -1, 13, -1, 13, -1, 13, -1, 14, -1, 14, -1, 14, -1, 14, -1, 15, -1, 15, -1, 15, -1, 15, -1},
};

ALIGN_DECL(32) static const unsigned char ShufTab_Strength_H_420[2][32] = {
    { 0, -1,  0, -1,  0, -1,  0, -1,  1, -1,  1, -1,  1, -1,  1, -1,  2, -1,  2, -1,  2, -1,  2, -1,  3, -1,  3, -1,  3, -1,  3, -1},
    { 8, -1,  8, -1,  8, -1,  8, -1,  9, -1,  9, -1,  9, -1,  9, -1, 10, -1, 10, -1, 10, -1, 10, -1, 11, -1, 11, -1, 11, -1, 11, -1},
};

ALIGN_DECL(32) static const unsigned char ShufTab_Clip_V_422[4][32] = {
    {0, -1,  8, -1, 0, -1,  8, -1, 0, -1,  8, -1, 0, -1,  8, -1, 4, -1, 12, -1, 4, -1, 12, -1, 4, -1, 12, -1, 4, -1, 12, -1},
    {1, -1,  9, -1, 1, -1,  9, -1, 1, -1,  9, -1, 1, -1,  9, -1, 5, -1, 13, -1, 5, -1, 13, -1, 5, -1, 13, -1, 5, -1, 13, -1},
    {2, -1, 10, -1, 2, -1, 10, -1, 2, -1, 10, -1, 2, -1, 10, -1, 6, -1, 14, -1, 6, -1, 14, -1, 6, -1, 14, -1, 6, -1, 14, -1},
    {3, -1, 11, -1, 3, -1, 11, -1, 3, -1, 11, -1, 3, -1, 11, -1, 7, -1, 15, -1, 7, -1, 15, -1, 7, -1, 15, -1, 7, -1, 15, -1},
};

ALIGN_DECL(32) static const unsigned char ShufTab_Clip_V_420[2][32] = {
    {0, -1,  8, -1, 0, -1,  8, -1, 1, -1,  9, -1, 1, -1,  9, -1, 4, -1, 12, -1, 4, -1, 12, -1, 5, -1, 13, -1, 5, -1, 13, -1},
    {2, -1, 10, -1, 2, -1, 10, -1, 3, -1, 11, -1, 3, -1, 11, -1, 6, -1, 14, -1, 6, -1, 14, -1, 7, -1, 15, -1, 7, -1, 15, -1},
};

// 422-H spans 32 bytes so we adjust offset when loading instead of copying values across 128-bit lanes
ALIGN_DECL(32) static const unsigned char ShufTab_Clip_H_422[32] = {0, -1, 4, -1, 0, -1, 4, -1, 1, -1, 5, -1, 1, -1, 5, -1, 2, -1,  6, -1, 2, -1,  6, -1, 3, -1,  7, -1, 3, -1,  7, -1};

ALIGN_DECL(32) static const unsigned char ShufTab_Clip_H_420[2][32] = {
    {0, -1, 8, -1, 0, -1, 8, -1, 1, -1, 9, -1, 1, -1, 9, -1, 2, -1, 10, -1, 2, -1, 10, -1, 3, -1, 11, -1, 3, -1, 11, -1},
    {4, -1,12, -1, 4, -1,12, -1, 5, -1,13, -1, 5, -1,13, -1, 6, -1, 14, -1, 6, -1, 14, -1, 7, -1, 15, -1, 7, -1, 15, -1},
};

template<typename Plane, FilterType dir, int chroma_format_idc>
static void DeblockChromaKernel(Plane *srcDst, mfxU32 pitch, Ipp8u* pAlpha, Ipp8u* pBeta, Ipp8u*  pThresholds, Ipp8u*  pBS, Ipp32s bitDepth, Ipp32s rowIdx, Ipp32s skipFirstCol)
{
    mfxU32 alpha32, beta32, bitDepthShift;
    
    _mm256_zeroupper();

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
    __m256i yFiltMask, yAlpha, yBeta, yClip, yDelta, yStrength, yBlend;

    bitDepthShift = bitDepth - 8;

    if (dir == VERT_FILT) {
        // cycle over pixels (8 or 16 vertical)
        ymm0 = mm256(_mm_loadu_si128((__m128i *)pBS));
        ymm1 = mm256(_mm_loadu_si128((__m128i *)pThresholds));

        ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x00);
        ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 0x00);

        if (chroma_format_idc == 2) {
            yStrength = _mm256_shuffle_epi8(ymm0, *(__m256i *)(ShufTab_Strength_V_422[rowIdx]));
            yClip = _mm256_shuffle_epi8(ymm1, *(__m256i *)(ShufTab_Clip_V_422[rowIdx]));
        } else {
            yStrength = _mm256_shuffle_epi8(ymm0, *(__m256i *)(ShufTab_Strength_V_420[rowIdx]));
            yClip = _mm256_shuffle_epi8(ymm1, *(__m256i *)(ShufTab_Clip_V_420[rowIdx]));
        }
        yClip = _mm256_sll_epi16(yClip, _mm_cvtsi32_si128(bitDepthShift));
        yClip = _mm256_add_epi16(yClip, *(__m256i *)(RndTab1));

        // low half = neighboring column
        alpha32 = ((mfxU32)pAlpha[0]) | ((mfxU32)pAlpha[2] << 16);
        beta32  = ((mfxU32)pBeta[0])  | ((mfxU32)pBeta[2]  << 16);
        yAlpha = mm256( _mm_set1_epi32(alpha32 << bitDepthShift) );
        yBeta  = mm256( _mm_set1_epi32(beta32  << bitDepthShift) );

        // high half = current column
        alpha32 = ((mfxU32)pAlpha[1]) | ((mfxU32)pAlpha[3] << 16);
        beta32  = ((mfxU32)pBeta[1])  | ((mfxU32)pBeta[3]  << 16);
        yAlpha = _mm256_permute2x128_si256(yAlpha, mm256(_mm_set1_epi32(alpha32 << bitDepthShift)), 0x20);
        yBeta  = _mm256_permute2x128_si256(yBeta,  mm256(_mm_set1_epi32(beta32  << bitDepthShift)), 0x20);

        if (sizeof(Plane) == 2) {
            // 16-bit
            if (skipFirstCol) {
                // read/write column 1 only (avoid MT issues with left edge of frame and multiple slices)
                ymm0 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 0*pitch + 8)));
                ymm1 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 1*pitch + 8)));
                ymm2 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 2*pitch + 8)));
                ymm3 = mm256(_mm_loadu_si128((__m128i *)(srcDst + 3*pitch + 8)));

                // replicate low to high (low becomes D/C, won't be written back)
                ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x00);
                ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 0x00);
                ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x00);
                ymm3 = _mm256_permute2x128_si256(ymm3, ymm3, 0x00);
            } else {
                ymm0 = _mm256_loadu_si256((__m256i *)(srcDst + 0*pitch));   // 00 01 02 03 (low part, repeat for high)
                ymm1 = _mm256_loadu_si256((__m256i *)(srcDst + 1*pitch));   // 10 11 12 13
                ymm2 = _mm256_loadu_si256((__m256i *)(srcDst + 2*pitch));   // 20 21 22 23
                ymm3 = _mm256_loadu_si256((__m256i *)(srcDst + 3*pitch));   // 30 31 32 33
            }
        } else {
            // 8-bit
            if (skipFirstCol) {
                ymm0 = _mm256_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(srcDst + 0*pitch + 8)));
                ymm1 = _mm256_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(srcDst + 1*pitch + 8)));
                ymm2 = _mm256_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(srcDst + 2*pitch + 8)));
                ymm3 = _mm256_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(srcDst + 3*pitch + 8)));

                ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x00);
                ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 0x00);
                ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x00);
                ymm3 = _mm256_permute2x128_si256(ymm3, ymm3, 0x00);
            } else {
                ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(srcDst + 0*pitch)));
                ymm1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(srcDst + 1*pitch)));
                ymm2 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(srcDst + 2*pitch)));
                ymm3 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(srcDst + 3*pitch)));
            }
        }

        ymm4 = _mm256_unpacklo_epi32(ymm0, ymm1);                   // 00 10 01 11
        ymm5 = _mm256_unpacklo_epi32(ymm2, ymm3);                   // 20 30 21 31
        ymm6 = _mm256_unpackhi_epi32(ymm0, ymm1);                   // 02 12 03 13
        ymm7 = _mm256_unpackhi_epi32(ymm2, ymm3);                   // 22 32 23 33

        ymm0 = _mm256_unpacklo_epi64(ymm4, ymm5);                   // 00 10 20 30
        ymm1 = _mm256_unpackhi_epi64(ymm4, ymm5);                   // 01 11 21 31
        ymm2 = _mm256_unpacklo_epi64(ymm6, ymm7);                   // 02 12 22 32
        ymm3 = _mm256_unpackhi_epi64(ymm6, ymm7);                   // 03 13 23 33
    } else {
        if (chroma_format_idc == 2) {
            ymm0 = mm256( _mm_cvtsi32_si128(*(Ipp32s *)(pThresholds + 4*rowIdx +  0)) );  // 0-3
            ymm1 = mm256( _mm_cvtsi32_si128(*(Ipp32s *)(pThresholds + 4*rowIdx + 16)) );  // 16-19
            ymm1 = _mm256_unpacklo_epi32(ymm0, ymm1);                                     // [0-7] = 0 1 2 3 16 17 18 19

            ymm0 = mm256(_mm_loadu_si128((__m128i *)pBS));
            ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x00);
            ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 0x00);
            
            yStrength = _mm256_shuffle_epi8(ymm0, *(__m256i *)(ShufTab_Strength_H_422[rowIdx]));
            yClip = _mm256_shuffle_epi8(ymm1, *(__m256i *)(ShufTab_Clip_H_422));
        } else {
            ymm0 = mm256(_mm_loadu_si128((__m128i *)pBS));
            ymm1 = mm256(_mm_loadu_si128((__m128i *)pThresholds));

            ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x00);
            ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 0x00);

            yStrength = _mm256_shuffle_epi8(ymm0, *(__m256i *)(ShufTab_Strength_H_420[rowIdx]));
            yClip = _mm256_shuffle_epi8(ymm1, *(__m256i *)(ShufTab_Clip_H_420[rowIdx]));
        }
        yClip = _mm256_sll_epi16(yClip, _mm_cvtsi32_si128(bitDepthShift));
        yClip = _mm256_add_epi16(yClip, *(__m256i *)(RndTab1));

        alpha32 = ((mfxU32)pAlpha[0]) | ((mfxU32)pAlpha[2] << 16);
        beta32  = ((mfxU32)pBeta[0])  | ((mfxU32)pBeta[2]  << 16);
        yAlpha = _mm256_set1_epi32(alpha32 << bitDepthShift);
        yBeta  = _mm256_set1_epi32(beta32  << bitDepthShift);

        if (sizeof(Plane) == 2) {
            // 16-bit
            ymm0 = _mm256_loadu_si256((__m256i *)(srcDst + 0*pitch));   // p1
            ymm1 = _mm256_loadu_si256((__m256i *)(srcDst + 1*pitch));   // p0
            ymm2 = _mm256_loadu_si256((__m256i *)(srcDst + 2*pitch));   // q0
            ymm3 = _mm256_loadu_si256((__m256i *)(srcDst + 3*pitch));   // q1
        } else {
            // 8-bit
            ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(srcDst + 0*pitch)));   // p1
            ymm1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(srcDst + 1*pitch)));   // p0
            ymm2 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(srcDst + 2*pitch)));   // q0
            ymm3 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(srcDst + 3*pitch)));   // q1
        }
    }

    ymm4 = _mm256_sub_epi16(ymm1, ymm2);                            // [p0 - q0]
    ymm4 = _mm256_abs_epi16(ymm4);                                  // [abs(p0 - q0)]
    ymm4 = _mm256_add_epi16(ymm4, *(__m256i *)(RndTab1));           // (absDiff + 1) > alpha same as (absDiff >= alpha)
    yFiltMask = _mm256_cmpgt_epi16(ymm4, yAlpha);

    ymm4 = _mm256_sub_epi16(ymm0, ymm1);                            // [p1 - p0]
    ymm4 = _mm256_abs_epi16(ymm4);                                  // [abs(p1 - p0)]
    ymm4 = _mm256_add_epi16(ymm4, *(__m256i *)(RndTab1));
    ymm4 = _mm256_cmpgt_epi16(ymm4, yBeta);
    yFiltMask = _mm256_or_si256(yFiltMask, ymm4);              

    ymm4 = _mm256_sub_epi16(ymm3, ymm2);                            // [q1 - q0]
    ymm4 = _mm256_abs_epi16(ymm4);                                  // [abs(q1 - q0)]
    ymm4 = _mm256_add_epi16(ymm4, *(__m256i *)(RndTab1));
    ymm4 = _mm256_cmpgt_epi16(ymm4, yBeta);
    yFiltMask = _mm256_or_si256(yFiltMask, ymm4);              

    // early exit if all zero (i.e. any condition is TRUE)
    yFiltMask = _mm256_xor_si256(yFiltMask, _mm256_set1_epi16(-1));
    if (_mm256_testz_si256(yFiltMask, _mm256_set1_epi16(-1)))
        return;

    // strong deblocking
    ymm4 = _mm256_add_epi16(ymm0, ymm0);                            // 2*p1
    ymm4 = _mm256_add_epi16(ymm4, ymm1);                            // 2*p1 + p0
    ymm4 = _mm256_add_epi16(ymm4, ymm3);                            // 2*p1 + p0 + q1
    ymm4 = _mm256_add_epi16(ymm4, *(__m256i *)(RndTab2));           // 2*p1 + p0 + q1 + 2
    ymm4 = _mm256_srai_epi16(ymm4, 2);

    ymm5 = _mm256_add_epi16(ymm3, ymm3);                            // 2*q1
    ymm5 = _mm256_add_epi16(ymm5, ymm2);                            // 2*q1 + q0
    ymm5 = _mm256_add_epi16(ymm5, ymm0);                            // 2*q1 + q0 + p1
    ymm5 = _mm256_add_epi16(ymm5, *(__m256i *)(RndTab2));
    ymm5 = _mm256_srai_epi16(ymm5, 2);

    // weak deblocking
    yDelta = _mm256_sub_epi16(ymm2, ymm1);                          //  q0 - p0
    yDelta = _mm256_slli_epi16(yDelta, 2);                          // (q0 - p0) << 2
    yDelta = _mm256_add_epi16(yDelta, ymm0);                        // (q0 - p0) << 2 + p1
    yDelta = _mm256_sub_epi16(yDelta, ymm3);                        // (q0 - p0) << 2 + p1 - q1
    yDelta = _mm256_add_epi16(yDelta, *(__m256i *)(RndTab4));
    yDelta = _mm256_srai_epi16(yDelta, 3);

    // clip delta
    yDelta = _mm256_min_epi16(yDelta, yClip);
    yClip  = _mm256_sub_epi16(_mm256_setzero_si256(), yClip);
    yDelta = _mm256_max_epi16(yDelta, yClip);

    ymm6 = _mm256_add_epi16(ymm1, yDelta);                          // p0 += delta
    ymm7 = _mm256_sub_epi16(ymm2, yDelta);                          // q0 -= delta

    // clip to [0, 2^bitDepth - 1]
    ymm6 = _mm256_max_epi16(ymm6, _mm256_setzero_si256());
    ymm7 = _mm256_max_epi16(ymm7, _mm256_setzero_si256());
    ymm6 = _mm256_min_epi16(ymm6, _mm256_set1_epi16((1 << bitDepth) -1));
    ymm7 = _mm256_min_epi16(ymm7, _mm256_set1_epi16((1 << bitDepth) -1));

    // blend if weak OR strong (strength > 0)
    yBlend = _mm256_cmpgt_epi16(yStrength, _mm256_setzero_si256());
    yBlend = _mm256_and_si256(yBlend, yFiltMask);
    ymm1 = _mm256_blendv_epi8(ymm1, ymm6, yBlend);
    ymm2 = _mm256_blendv_epi8(ymm2, ymm7, yBlend);

    // blend if strong (strength == 4)
    yBlend = _mm256_cmpeq_epi16(yStrength, *(__m256i *)(RndTab4));
    yBlend = _mm256_and_si256(yBlend, yFiltMask);
    ymm1 = _mm256_blendv_epi8(ymm1, ymm4, yBlend);
    ymm2 = _mm256_blendv_epi8(ymm2, ymm5, yBlend);

    if (dir == VERT_FILT) {
        ymm4 = _mm256_unpacklo_epi32(ymm0, ymm1);                   // 00 01 10 11
        ymm5 = _mm256_unpacklo_epi32(ymm2, ymm3);                   // 02 03 12 13
        ymm6 = _mm256_unpackhi_epi32(ymm0, ymm1);                   // 20 21 30 31
        ymm7 = _mm256_unpackhi_epi32(ymm2, ymm3);                   // 22 23 32 33

        ymm0 = _mm256_unpacklo_epi64(ymm4, ymm5);                   // 00 01 02 03
        ymm1 = _mm256_unpackhi_epi64(ymm4, ymm5);                   // 10 11 12 13
        ymm2 = _mm256_unpacklo_epi64(ymm6, ymm7);                   // 20 21 22 23
        ymm3 = _mm256_unpackhi_epi64(ymm6, ymm7);                   // 30 31 32 33

        if (sizeof(Plane) == 2) {
            // 16-bit
            if (skipFirstCol) {
                ymm0 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);
                ymm1 = _mm256_permute2x128_si256(ymm1, ymm1, 0x01);
                ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01);
                ymm3 = _mm256_permute2x128_si256(ymm3, ymm3, 0x01);

                _mm_storeu_si128((__m128i *)(srcDst + 0*pitch + 8), mm128(ymm0));
                _mm_storeu_si128((__m128i *)(srcDst + 1*pitch + 8), mm128(ymm1));
                _mm_storeu_si128((__m128i *)(srcDst + 2*pitch + 8), mm128(ymm2));
                _mm_storeu_si128((__m128i *)(srcDst + 3*pitch + 8), mm128(ymm3));
            } else {
                _mm256_storeu_si256((__m256i *)(srcDst + 0*pitch), ymm0);
                _mm256_storeu_si256((__m256i *)(srcDst + 1*pitch), ymm1);
                _mm256_storeu_si256((__m256i *)(srcDst + 2*pitch), ymm2);
                _mm256_storeu_si256((__m256i *)(srcDst + 3*pitch), ymm3);
            }
        } else {
            // 8-bit
            ymm0 = _mm256_packus_epi16(ymm0, ymm0);
            ymm1 = _mm256_packus_epi16(ymm1, ymm1);
            ymm2 = _mm256_packus_epi16(ymm2, ymm2);
            ymm3 = _mm256_packus_epi16(ymm3, ymm3);

            if (skipFirstCol) {
                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x02);
                ymm1 = _mm256_permute4x64_epi64(ymm1, 0x02);
                ymm2 = _mm256_permute4x64_epi64(ymm2, 0x02);
                ymm3 = _mm256_permute4x64_epi64(ymm3, 0x02);

                _mm_storel_epi64((__m128i *)(srcDst + 0*pitch + 8), mm128(ymm0));
                _mm_storel_epi64((__m128i *)(srcDst + 1*pitch + 8), mm128(ymm1));
                _mm_storel_epi64((__m128i *)(srcDst + 2*pitch + 8), mm128(ymm2));
                _mm_storel_epi64((__m128i *)(srcDst + 3*pitch + 8), mm128(ymm3));
            } else {
                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
                ymm1 = _mm256_permute4x64_epi64(ymm1, 0x08);
                ymm2 = _mm256_permute4x64_epi64(ymm2, 0x08);
                ymm3 = _mm256_permute4x64_epi64(ymm3, 0x08);

                _mm_storeu_si128((__m128i *)(srcDst + 0*pitch), mm128(ymm0));
                _mm_storeu_si128((__m128i *)(srcDst + 1*pitch), mm128(ymm1));
                _mm_storeu_si128((__m128i *)(srcDst + 2*pitch), mm128(ymm2));
                _mm_storeu_si128((__m128i *)(srcDst + 3*pitch), mm128(ymm3));
            }
        }
    } else {
        if (sizeof(Plane) == 2) {
            // 16-bit
            _mm256_storeu_si256((__m256i *)(srcDst + 1*pitch), ymm1);
            _mm256_storeu_si256((__m256i *)(srcDst + 2*pitch), ymm2);
        } else {
            // 8-bit
            ymm1 = _mm256_packus_epi16(ymm1, ymm1);
            ymm2 = _mm256_packus_epi16(ymm2, ymm2);

            _mm_storeu_si128((__m128i *)(srcDst + 1*pitch), mm128(_mm256_permute4x64_epi64(ymm1, 0x08)));
            _mm_storeu_si128((__m128i *)(srcDst + 2*pitch), mm128(_mm256_permute4x64_epi64(ymm2, 0x08)));
        }
    }
}

template<typename Plane, FilterType dir, int chroma_format_idc>
static void ippiFilterDeblockingChroma_H264_kernel(Plane* srcDst, Ipp32s  srcDstStep, Ipp8u*  pAlpha, Ipp8u*  pBeta, Ipp8u*  pThresholds, Ipp8u*  pBS, Ipp32s  bitDepth)
{
    Ipp32s i, j, skipFirstCol;

    // for 422 we have 4x2 input blocks (4x4 each), so 4 horiz. edges per column (2 columns) and 2 vert. edges per row (4 rows)

    if (dir == HOR_FILT) {
        // cycle over edges
        for (j = 0; j < (chroma_format_idc == 2 ? 4: 2); j += 1) {
            // all 8 pixels in this row are skipped
            if ( 0 == (*(Ipp32u *)(&pBS[(chroma_format_idc == 2 ? 4*j : 8*j)])) )
                continue;

            DeblockChromaKernel<Plane, dir, chroma_format_idc>(srcDst + (4*j - 2)*srcDstStep, (mfxU32)srcDstStep, pAlpha + (j > 0), pBeta + (j > 0), pThresholds, pBS, bitDepth, j, 0);
        }
    } else {
        // avoid multithreading issue: don't touch left edge of frame (strength should always be set to 0) 
        //   otherwise can run into race conditions with H deblocking on last MB in previous slice 
        //   ("left pixels" actually belong to previous slice, and not guaranteed that V and H happen in proper order, since on different threads)
        skipFirstCol = 0;
        if ( 0 == (*(Ipp32u *)(&pBS[0])) )
            skipFirstCol = 1;

        // cycle over pixels (8 or 16 vertical)
        for (i = 0; i < (chroma_format_idc == 2 ? 4: 2); i++) {
            DeblockChromaKernel<Plane, dir, chroma_format_idc>(srcDst + 4*i * srcDstStep - 2*2, srcDstStep, pAlpha, pBeta, pThresholds, pBS, bitDepth, i, skipFirstCol);
        }
    }
}

void MFX_Dispatcher_avx2::FilterDeblockingLumaEdge(Ipp16u* pSrcDst, Ipp32s  srcdstStep,
                                    Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                    Ipp8u*  pThresholds, Ipp8u*  pBS,
                                    Ipp32s  bit_depth,
                                    Ipp32u  isDeblHor)
{
    if (isDeblHor)
    {
        ippiFilterDeblockingLuma_H264_kernel<mfxU16, HOR_FILT>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
    else
    {
        ippiFilterDeblockingLuma_H264_kernel<mfxU16, VERT_FILT>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
}

void MFX_Dispatcher_avx2::FilterDeblockingChromaEdge(Ipp8u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc,
                                                Ipp32u  isDeblHor)
{
    assert(bit_depth == 8);
    
    if (isDeblHor)
    {
        if (chroma_format_idc == 2)
            ippiFilterDeblockingChroma_H264_kernel<mfxU8, HOR_FILT, 2>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        else
            ippiFilterDeblockingChroma_H264_kernel<mfxU8, HOR_FILT, 1>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
    else
    {
        if (chroma_format_idc == 2)
            ippiFilterDeblockingChroma_H264_kernel<mfxU8, VERT_FILT, 2>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        else
            ippiFilterDeblockingChroma_H264_kernel<mfxU8, VERT_FILT, 1>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
}

void MFX_Dispatcher_avx2::FilterDeblockingChromaEdge(Ipp16u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc,
                                                Ipp32u  isDeblHor)
{
    if (isDeblHor)
    {
        if (chroma_format_idc == 2)
            ippiFilterDeblockingChroma_H264_kernel<mfxU16, HOR_FILT, 2>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        else
            ippiFilterDeblockingChroma_H264_kernel<mfxU16, HOR_FILT, 1>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
    else
    {
        if (chroma_format_idc == 2)
            ippiFilterDeblockingChroma_H264_kernel<mfxU16, VERT_FILT, 2>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
        else
            ippiFilterDeblockingChroma_H264_kernel<mfxU16, VERT_FILT, 1>(pSrcDst, srcdstStep, pAlpha, pBeta, pThresholds, pBS, bit_depth);
    }
}

} // namespace MFX_PP
