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


#include "immintrin.h"
#include "mfx_av1_opts_common.h"

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

#define _mm_loadh_epi64(A, p)  _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(A), (double *)(p)))

/* NOTE: In debug mode compiler attempts to load data with MOVNTDQA while data is
+only 8-byte aligned, but PMOVZX does not require 16-byte alignment.
+cannot substitute in SSSE3 emulation mode as PUNPCK used instead of PMOVZX requires alignment
+ICC14 fixed problems with reg-mem forms for PMOVZX/SX with _mm_loadl_epi64() intrinsic and broke old workaround */
#if (defined( NDEBUG ) && !defined(MFX_EMULATE_SSSE3)) && !(defined( __INTEL_COMPILER ) && (__INTEL_COMPILER >= 1400 ))
#define MM_LOAD_EPI64(x) (*(const __m128i*)(x))
#else
#define MM_LOAD_EPI64(x) _mm_loadl_epi64( (const __m128i*)(x) )
#endif

namespace AV1PP
{
    namespace details {
        ALIGN_DECL(32) static const int8_t shufTab4[32] = {
            0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15,
            0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15
        };

        //ALIGN_DECL(32) static const int16_t mulAddOne[16] = {+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1,+1};
        //ALIGN_DECL(32) static const int16_t mulSubOne[16] = {+1,-1,+1,-1,+1,-1,+1,-1,+1,-1,+1,-1,+1,-1,+1,-1};

        /* h265_SATD_4x4() - 4x4 SATD adapted from IPP source code
        *
        * multiply-free implementation of sum(abs(T*D*T))
        *   D = difference matrix (cur - ref)
        *   T = transform matrix (values of +/-1, see IPP docs)
        *
        * implemented as sum(abs(T*transpose(T*D)))
        * final matrix is transposed, but doesn't matter since we just sum all the elements
        *
        * supports up to 10-bit video input (with 16u version)
        */
        inline AV1_FORCEINLINE int32_t satd_4x4_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2)
        {
            __m128i  xmm0, xmm1, xmm2/*, xmm3*/;
            __m256i  ymm0, ymm1, ymm2, ymm3, ymm7;

            _mm256_zeroupper();

            ymm7 = _mm256_load_si256((__m256i*)shufTab4);

            /* difference matrix (4x4) = cur - ref */
            //if (sizeof(PixType) == 2) {
            //    /* 16-bit */
            //    xmm0 = _mm_loadl_epi64((__m128i*)(src1 + 0*pitch1));
            //    xmm0 = _mm_loadh_epi64(xmm0, (__m128i*)(src1 + 1*pitch1));
            //    xmm2 = _mm_loadl_epi64((__m128i*)(src1 + 2*pitch1));
            //    xmm2 = _mm_loadh_epi64(xmm2, (__m128i*)(src1 + 3*pitch1));

            //    xmm1 = _mm_loadl_epi64((__m128i*)(src2 + 0*pitch2));
            //    xmm1 = _mm_loadh_epi64(xmm1, (__m128i*)(src2 + 1*pitch2));
            //    xmm3 = _mm_loadl_epi64((__m128i*)(src2 + 2*pitch2));
            //    xmm3 = _mm_loadh_epi64(xmm3, (__m128i*)(src2 + 3*pitch2));

            //    ymm0 = _mm256_permute2x128_si256(mm256(xmm0), mm256(xmm2), 0x20);
            //    ymm1 = _mm256_permute2x128_si256(mm256(xmm1), mm256(xmm3), 0x20);
            //} else
            {
                /* 8-bit */
                xmm0 = _mm_cvtsi32_si128(*(int*)(src1 + 0*pitch1));
                xmm0 = _mm_insert_epi32(xmm0, *(int*)(src1 + 1*pitch1), 0x01);
                xmm0 = _mm_insert_epi32(xmm0, *(int*)(src1 + 2*pitch1), 0x02);
                xmm0 = _mm_insert_epi32(xmm0, *(int*)(src1 + 3*pitch1), 0x03);

                xmm1 = _mm_cvtsi32_si128(*(int*)(src2 + 0*pitch2));
                xmm1 = _mm_insert_epi32(xmm1, *(int*)(src2 + 1*pitch2), 0x01);
                xmm1 = _mm_insert_epi32(xmm1, *(int*)(src2 + 2*pitch2), 0x02);
                xmm1 = _mm_insert_epi32(xmm1, *(int*)(src2 + 3*pitch2), 0x03);

                ymm0 = _mm256_cvtepu8_epi16(xmm0);
                ymm1 = _mm256_cvtepu8_epi16(xmm1);
            }

            ymm0 = _mm256_sub_epi16(ymm0, ymm1);        /* diff [row0 row1 row2 row3] */

            ymm2 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

            /* first stage */
            ymm1 = _mm256_sub_epi16(ymm0, ymm2);        /* [row0-row2 row1-row3] */
            ymm0 = _mm256_add_epi16(ymm0, ymm2);        /* [row0+row2 row1+row3] */

            ymm2 = _mm256_unpacklo_epi64(ymm0, ymm1);   /* row0+row2 row0-row2 */
            ymm0 = _mm256_unpackhi_epi64(ymm0, ymm1);   /* row1+row3 row1-row3 */

            ymm3 = _mm256_sub_epi16(ymm2, ymm0);        /* row2: 0-1+2-3  row3: 0-1-2+3 */
            ymm2 = _mm256_add_epi16(ymm2, ymm0);        /* row0: 0+1+2+3  row1: 0+1-2-3 */

            /* transpose */
            ymm2 = _mm256_shuffle_epi8(ymm2, ymm7);
            ymm3 = _mm256_shuffle_epi8(ymm3, ymm7);

            ymm0 = _mm256_unpacklo_epi32(ymm2, ymm3);   /* rows 0-3 [0 0 0 0 1 1 1 1] */
            ymm2 = _mm256_unpackhi_epi32(ymm2, ymm3);   /* rows 0-3 [2 2 2 2 3 3 3 3] */

            /* second stage */
            ymm1 = _mm256_sub_epi16(ymm0, ymm2);        /* [row0-row2 row1-row3] */
            ymm0 = _mm256_add_epi16(ymm0, ymm2);        /* [row0+row2 row1+row3] */

            ymm2 = _mm256_unpacklo_epi64(ymm0, ymm1);   /* row0+row2 row0-row2 */
            ymm0 = _mm256_unpackhi_epi64(ymm0, ymm1);   /* row1+row3 row1-row3 */

            ymm3 = _mm256_sub_epi16(ymm2, ymm0);        /* row2: 0-1+2-3  row3: 0-1-2+3 */
            ymm2 = _mm256_add_epi16(ymm2, ymm0);        /* row0: 0+1+2+3  row1: 0+1-2-3 */

            /* sum abs values - avoid phadd (faster to shuffle) */
            ymm2 = _mm256_abs_epi16(ymm2);
            ymm3 = _mm256_abs_epi16(ymm3);
            ymm2 = _mm256_add_epi16(ymm2, ymm3);

            ymm3 = _mm256_shuffle_epi32(ymm2, 0x0E);
            ymm2 = _mm256_add_epi16(ymm2, ymm3);
            ymm3 = _mm256_shuffle_epi32(ymm2, 0x01);
            ymm2 = _mm256_add_epi16(ymm2, ymm3);
            ymm3 = _mm256_shufflelo_epi16(ymm2, 0x01);
            ymm2 = _mm256_add_epi16(ymm2, ymm3);
            xmm2 = _mm_cvtepu16_epi32(mm128(ymm2));

            return _mm_cvtsi128_si32(xmm2);
        }

        /* h265_SATD_4x4_Pair() - pairs of 4x4 SATD's
        *
        * load two consecutive 4x4 blocks for cur and ref
        * use low and high halves of ymm registers to process in parallel
        * avoids lane-crossing penalties (except for getting final sum from top half)
        *
        * supports up to 10-bit video input (with 16u version)
        */
        inline AV1_FORCEINLINE void satd_4x4_pair_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int32_t* satdPair)
        {
            __m128i  xmm0, xmm1, xmm2, xmm3, xmm4;
            __m256i  ymm0, ymm1, ymm2, ymm3, ymm7;

            _mm256_zeroupper();

            ymm7 = _mm256_load_si256((__m256i*)shufTab4);

            //if (sizeof(PixType) == 2) {
            //    /* 16-bit */
            //    xmm0 = _mm_loadu_si128((__m128i* )(src1 + 0*pitch1));
            //    xmm4 = _mm_loadu_si128((__m128i* )(src1 + 1*pitch1));
            //    ymm0 = _mm256_permute2x128_si256(mm256(xmm0), mm256(xmm4), 0x20);
            //    ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);

            //    xmm2 = _mm_loadu_si128((__m128i* )(src1 + 2*pitch1));
            //    xmm4 = _mm_loadu_si128((__m128i* )(src1 + 3*pitch1));
            //    ymm2 = _mm256_permute2x128_si256(mm256(xmm2), mm256(xmm4), 0x20);
            //    ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);

            //    xmm1 = _mm_loadu_si128((__m128i* )(src2 + 0*pitch2));
            //    xmm4 = _mm_loadu_si128((__m128i* )(src2 + 1*pitch2));
            //    ymm1 = _mm256_permute2x128_si256(mm256(xmm1), mm256(xmm4), 0x20);
            //    ymm1 = _mm256_permute4x64_epi64(ymm1, 0xd8);

            //    xmm3 = _mm_loadu_si128((__m128i* )(src2 + 2*pitch2));
            //    xmm4 = _mm_loadu_si128((__m128i* )(src2 + 3*pitch2));
            //    ymm3 = _mm256_permute2x128_si256(mm256(xmm3), mm256(xmm4), 0x20);
            //    ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
            //} else
            {
                /* 8-bit */
                xmm0 = _mm_loadl_epi64((__m128i* )(src1 + 0*pitch1));
                xmm4 = _mm_loadl_epi64((__m128i* )(src1 + 1*pitch1));
                xmm0 = _mm_unpacklo_epi32(xmm0, xmm4);

                xmm2 = _mm_loadl_epi64((__m128i* )(src1 + 2*pitch1));
                xmm4 = _mm_loadl_epi64((__m128i* )(src1 + 3*pitch1));
                xmm2 = _mm_unpacklo_epi32(xmm2, xmm4);

                xmm1 = _mm_loadl_epi64((__m128i* )(src2 + 0*pitch2));
                xmm4 = _mm_loadl_epi64((__m128i* )(src2 + 1*pitch2));
                xmm1 = _mm_unpacklo_epi32(xmm1, xmm4);

                xmm3 = _mm_loadl_epi64((__m128i* )(src2 + 2*pitch2));
                xmm4 = _mm_loadl_epi64((__m128i* )(src2 + 3*pitch2));
                xmm3 = _mm_unpacklo_epi32(xmm3, xmm4);

                ymm0 = _mm256_cvtepu8_epi16(xmm0);
                ymm1 = _mm256_cvtepu8_epi16(xmm1);
                ymm2 = _mm256_cvtepu8_epi16(xmm2);
                ymm3 = _mm256_cvtepu8_epi16(xmm3);
            }
            ymm0 = _mm256_sub_epi16(ymm0, ymm1);
            ymm2 = _mm256_sub_epi16(ymm2, ymm3);

            /* first stage */
            ymm1 = _mm256_sub_epi16(ymm0, ymm2);
            ymm0 = _mm256_add_epi16(ymm0, ymm2);

            ymm2 = _mm256_unpacklo_epi64(ymm0, ymm1);
            ymm0 = _mm256_unpackhi_epi64(ymm0, ymm1);

            ymm3 = _mm256_sub_epi16(ymm2, ymm0);
            ymm2 = _mm256_add_epi16(ymm2, ymm0);

            /* transpose */
            ymm2 = _mm256_shuffle_epi8(ymm2, ymm7);
            ymm3 = _mm256_shuffle_epi8(ymm3, ymm7);

            ymm0 = _mm256_unpacklo_epi32(ymm2, ymm3);
            ymm2 = _mm256_unpackhi_epi32(ymm2, ymm3);

            /* second stage */
            ymm1 = _mm256_sub_epi16(ymm0, ymm2);
            ymm0 = _mm256_add_epi16(ymm0, ymm2);

            ymm2 = _mm256_unpacklo_epi64(ymm0, ymm1);
            ymm0 = _mm256_unpackhi_epi64(ymm0, ymm1);

            ymm3 = _mm256_sub_epi16(ymm2, ymm0);
            ymm2 = _mm256_add_epi16(ymm2, ymm0);

            /* sum abs values - avoid phadd (faster to shuffle) */
            ymm2 = _mm256_abs_epi16(ymm2);
            ymm3 = _mm256_abs_epi16(ymm3);
            ymm2 = _mm256_add_epi16(ymm2, ymm3);

            ymm3 = _mm256_shuffle_epi32(ymm2, 0x0E);
            ymm2 = _mm256_add_epi16(ymm2, ymm3);
            ymm3 = _mm256_shuffle_epi32(ymm2, 0x01);
            ymm2 = _mm256_add_epi16(ymm2, ymm3);
            ymm3 = _mm256_shufflelo_epi16(ymm2, 0x01);
            ymm2 = _mm256_add_epi16(ymm2, ymm3);
            ymm3 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01);

            xmm2 = _mm_cvtepu16_epi32(mm128(ymm2));
            xmm3 = _mm_cvtepu16_epi32(mm128(ymm3));

            satdPair[0] = _mm_cvtsi128_si32(xmm2);
            satdPair[1] = _mm_cvtsi128_si32(xmm3);
        }

        /* h265_SATD_8x8() - 8x8 SATD adapted from IPP source code
        *
        * multiply-free implementation of sum(abs(T*D*T)) (see comments on SSE version)
        * using full 256 bit registers for single 8x8 transform requires permutes to load data
        *   but avoids spilling to stack as in SSE
        * only partial transpose is required, avoiding cross-lane penalties
        *
        * supports up to 10-bit video input (with 16u version)
        */
        inline AV1_FORCEINLINE int32_t satd_8x8_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2)
        {
            const uint8_t *pCT, *pRT;
            int stepC4, stepR4;
            __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;
            __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;

            pCT = src1;
            pRT = src2;
            stepC4 = 4*pitch1;
            stepR4 = 4*pitch2;

            _mm256_zeroupper();

            /* ymm registers have [row0 row4], [row1 row5], etc */
            //if (sizeof(PixType) == 2) {
            //    /* 16-bit */
            //    xmm0 = _mm_loadu_si128((__m128i*)(pCT));
            //    xmm5 = _mm_loadu_si128((__m128i*)(pRT));
            //    xmm0 = _mm_sub_epi16(xmm0, xmm5);
            //    xmm5 = _mm_loadu_si128((__m128i*)(pCT + stepC4));
            //    xmm4 = _mm_loadu_si128((__m128i*)(pRT + stepR4));
            //    xmm5 = _mm_sub_epi16(xmm5, xmm4);
            //    xmm4 = _mm_sub_epi16(xmm0, xmm5);
            //    xmm0 = _mm_add_epi16(xmm0, xmm5);
            //    ymm0 = _mm256_inserti128_si256(mm256(xmm0), xmm4, 0x01);
            //    pCT += pitch1;
            //    pRT += pitch2;

            //    xmm1 = _mm_loadu_si128((__m128i*)(pCT));
            //    xmm5 = _mm_loadu_si128((__m128i*)(pRT));
            //    xmm1 = _mm_sub_epi16(xmm1, xmm5);
            //    xmm5 = _mm_loadu_si128((__m128i*)(pCT + stepC4));
            //    xmm4 = _mm_loadu_si128((__m128i*)(pRT + stepR4));
            //    xmm5 = _mm_sub_epi16(xmm5, xmm4);
            //    xmm4 = _mm_sub_epi16(xmm1, xmm5);
            //    xmm1 = _mm_add_epi16(xmm1, xmm5);
            //    ymm1 = _mm256_inserti128_si256(mm256(xmm1), xmm4, 0x01);
            //    pCT += pitch1;
            //    pRT += pitch2;

            //    xmm2 = _mm_loadu_si128((__m128i*)(pCT));
            //    xmm5 = _mm_loadu_si128((__m128i*)(pRT));
            //    xmm2 = _mm_sub_epi16(xmm2, xmm5);
            //    xmm5 = _mm_loadu_si128((__m128i*)(pCT + stepC4));
            //    xmm4 = _mm_loadu_si128((__m128i*)(pRT + stepR4));
            //    xmm5 = _mm_sub_epi16(xmm5, xmm4);
            //    xmm4 = _mm_sub_epi16(xmm2, xmm5);
            //    xmm2 = _mm_add_epi16(xmm2, xmm5);
            //    ymm2 = _mm256_inserti128_si256(mm256(xmm2), xmm4, 0x01);
            //    pCT += pitch1;
            //    pRT += pitch2;

            //    xmm3 = _mm_loadu_si128((__m128i*)(pCT));
            //    xmm5 = _mm_loadu_si128((__m128i*)(pRT));
            //    xmm3 = _mm_sub_epi16(xmm3, xmm5);
            //    xmm5 = _mm_loadu_si128((__m128i*)(pCT + stepC4));
            //    xmm4 = _mm_loadu_si128((__m128i*)(pRT + stepR4));
            //    xmm5 = _mm_sub_epi16(xmm5, xmm4);
            //    xmm4 = _mm_sub_epi16(xmm3, xmm5);
            //    xmm3 = _mm_add_epi16(xmm3, xmm5);
            //    ymm3 = _mm256_inserti128_si256(mm256(xmm3), xmm4, 0x01);
            //} else
            {
                /* 8-bit*/
                xmm0 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
                xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
                xmm0 = _mm_sub_epi16(xmm0, xmm5);
                xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
                xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
                xmm5 = _mm_sub_epi16(xmm5, xmm4);
                xmm4 = _mm_sub_epi16(xmm0, xmm5);
                xmm0 = _mm_add_epi16(xmm0, xmm5);
                ymm0 = _mm256_inserti128_si256(mm256(xmm0), xmm4, 0x01);
                pCT += pitch1;
                pRT += pitch2;

                xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
                xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
                xmm1 = _mm_sub_epi16(xmm1, xmm5);
                xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
                xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
                xmm5 = _mm_sub_epi16(xmm5, xmm4);
                xmm4 = _mm_sub_epi16(xmm1, xmm5);
                xmm1 = _mm_add_epi16(xmm1, xmm5);
                ymm1 = _mm256_inserti128_si256(mm256(xmm1), xmm4, 0x01);
                pCT += pitch1;
                pRT += pitch2;

                xmm2 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
                xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
                xmm2 = _mm_sub_epi16(xmm2, xmm5);
                xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
                xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
                xmm5 = _mm_sub_epi16(xmm5, xmm4);
                xmm4 = _mm_sub_epi16(xmm2, xmm5);
                xmm2 = _mm_add_epi16(xmm2, xmm5);
                ymm2 = _mm256_inserti128_si256(mm256(xmm2), xmm4, 0x01);
                pCT += pitch1;
                pRT += pitch2;

                xmm3 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
                xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
                xmm3 = _mm_sub_epi16(xmm3, xmm5);
                xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
                xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
                xmm5 = _mm_sub_epi16(xmm5, xmm4);
                xmm4 = _mm_sub_epi16(xmm3, xmm5);
                xmm3 = _mm_add_epi16(xmm3, xmm5);
                ymm3 = _mm256_inserti128_si256(mm256(xmm3), xmm4, 0x01);
            }

            /* first stage */
            ymm4 = _mm256_add_epi16(ymm2, ymm3);
            ymm2 = _mm256_sub_epi16(ymm2, ymm3);

            ymm3 = _mm256_sub_epi16(ymm0, ymm1);
            ymm0 = _mm256_add_epi16(ymm0, ymm1);

            ymm1 = _mm256_add_epi16(ymm3, ymm2);
            ymm3 = _mm256_sub_epi16(ymm3, ymm2);

            ymm2 = _mm256_sub_epi16(ymm0, ymm4);
            ymm0 = _mm256_add_epi16(ymm0, ymm4);

            /* transpose and split */
            ymm4 = _mm256_unpackhi_epi64(ymm0, ymm2);
            ymm0 = _mm256_unpacklo_epi64(ymm0, ymm2);

            ymm2 = _mm256_sub_epi16(ymm0, ymm4);
            ymm0 = _mm256_add_epi16(ymm0, ymm4);

            ymm4 = _mm256_unpackhi_epi64(ymm1, ymm3);
            ymm1 = _mm256_unpacklo_epi64(ymm1, ymm3);

            ymm3 = _mm256_sub_epi16(ymm1, ymm4);
            ymm1 = _mm256_add_epi16(ymm1, ymm4);

            ymm4 = _mm256_unpackhi_epi16(ymm0, ymm1);
            ymm0 = _mm256_unpacklo_epi16(ymm0, ymm1);

            ymm5 = _mm256_unpackhi_epi16(ymm2, ymm3);
            ymm2 = _mm256_unpacklo_epi16(ymm2, ymm3);

            ymm1 = _mm256_unpackhi_epi32(ymm0, ymm2);
            ymm0 = _mm256_unpacklo_epi32(ymm0, ymm2);

            ymm2 = _mm256_unpacklo_epi32(ymm4, ymm5);
            ymm3 = _mm256_unpackhi_epi32(ymm4, ymm5);

            /* second stage - don't worry about final matrix permutation since we just sum the elements in the end */
            ymm4 = ymm0;
            ymm5 = ymm1;
            ymm0 = _mm256_unpacklo_epi64(ymm4, ymm2);
            ymm1 = _mm256_unpackhi_epi64(ymm4, ymm2);
            ymm2 = _mm256_unpacklo_epi64(ymm5, ymm3);
            ymm3 = _mm256_unpackhi_epi64(ymm5, ymm3);

            ymm4 = _mm256_add_epi16(ymm2, ymm3);
            ymm2 = _mm256_sub_epi16(ymm2, ymm3);

            ymm3 = _mm256_sub_epi16(ymm0, ymm1);
            ymm0 = _mm256_add_epi16(ymm0, ymm1);

            //if (sizeof(PixType) == 2) {
            //    __m256i ymm6;

            //    /* to avoid overflow switch to 32-bit accumulators */
            //    ymm5 = _mm256_unpacklo_epi16(ymm3, ymm2);
            //    ymm6 = _mm256_unpackhi_epi16(ymm3, ymm2);
            //    ymm2 = ymm5;
            //    ymm3 = ymm6;

            //    ymm5 = _mm256_abs_epi32( _mm256_madd_epi16(ymm5, *(__m256i *)mulAddOne) );
            //    ymm6 = _mm256_abs_epi32( _mm256_madd_epi16(ymm6, *(__m256i *)mulAddOne) );
            //    ymm2 = _mm256_abs_epi32( _mm256_madd_epi16(ymm2, *(__m256i *)mulSubOne) );
            //    ymm3 = _mm256_abs_epi32( _mm256_madd_epi16(ymm3, *(__m256i *)mulSubOne) );

            //    ymm2 = _mm256_add_epi32(ymm2, ymm3);
            //    ymm2 = _mm256_add_epi32(ymm2, ymm5);
            //    ymm2 = _mm256_add_epi32(ymm2, ymm6);

            //    ymm5 = _mm256_unpacklo_epi16(ymm0, ymm4);
            //    ymm6 = _mm256_unpackhi_epi16(ymm0, ymm4);
            //    ymm4 = ymm5;
            //    ymm3 = ymm6;

            //    ymm5 = _mm256_abs_epi32( _mm256_madd_epi16(ymm5, *(__m256i *)mulAddOne) );
            //    ymm6 = _mm256_abs_epi32( _mm256_madd_epi16(ymm6, *(__m256i *)mulAddOne) );
            //    ymm4 = _mm256_abs_epi32( _mm256_madd_epi16(ymm4, *(__m256i *)mulSubOne) );
            //    ymm3 = _mm256_abs_epi32( _mm256_madd_epi16(ymm3, *(__m256i *)mulSubOne) );

            //    ymm0 = _mm256_add_epi32(ymm2, ymm5);
            //    ymm0 = _mm256_add_epi32(ymm0, ymm6);
            //    ymm0 = _mm256_add_epi32(ymm0, ymm4);
            //    ymm0 = _mm256_add_epi32(ymm0, ymm3);
            //} else
            {
                ymm1 = _mm256_add_epi16(ymm3, ymm2);
                ymm3 = _mm256_sub_epi16(ymm3, ymm2);

                ymm2 = _mm256_sub_epi16(ymm0, ymm4);
                ymm0 = _mm256_add_epi16(ymm0, ymm4);

                /* final abs and sum */
                ymm0 = _mm256_abs_epi16(ymm0);
                ymm1 = _mm256_abs_epi16(ymm1);
                ymm2 = _mm256_abs_epi16(ymm2);
                ymm3 = _mm256_abs_epi16(ymm3);

                ymm0 = _mm256_add_epi16(ymm0, ymm1);
                ymm0 = _mm256_add_epi16(ymm0, ymm2);
                ymm0 = _mm256_add_epi16(ymm0, ymm3);

                /* use single packed 16*16->32 multiply for first step (slightly faster) */
                ymm0 = _mm256_madd_epi16(ymm0, _mm256_set1_epi16(1));
            }
            ymm1 = _mm256_shuffle_epi32(ymm0, 0x0E);
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            ymm1 = _mm256_shuffle_epi32(ymm0, 0x01);
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            xmm1 = _mm256_extracti128_si256(ymm0, 1);
            xmm0 = _mm_add_epi32(mm128(ymm0), xmm1);

            return _mm_cvtsi128_si32(xmm0);
        }

        /* h265_SATD_8x8_Pair_8u() - 8x8 SATD adapted from IPP source code
        *
        * load two consecutive 8x8 blocks for cur and ref
        * use low and high halves of ymm registers to process in parallel (double-width version of IPP SSE algorithm)
        * avoids lane-crossing penalties (except for loading pixels and storing final sum)
        */
        inline AV1_FORCEINLINE void satd_8x8_pair_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int32_t* satdPair) {
            const uint8_t *pS1, *pS2;
            int32_t  s;

            __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
            __m256i ymm_s0, ymm_s1, ymm_s2, ymm_one;

            static ALIGN_DECL(32) int8_t ymm_const11nb[32] = {1,1,1,1,1,1,1,1,1,-1,1,-1,1,-1,1,-1,   1,1,1,1,1,1,1,1,1,-1,1,-1,1,-1,1,-1};

            _mm256_zeroupper();

            pS1 = src1;
            pS2 = src2;

            ymm7 = _mm256_load_si256((__m256i*)&(ymm_const11nb[0]));
            ymm_one = _mm256_set1_epi16(1);

            ymm0 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
            ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
            ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+pitch2))), 0x50);
            ymm1 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+pitch1))), 0x50);
            pS2 += 2 * pitch2;
            pS1 += 2 * pitch1;
            ymm0 = _mm256_maddubs_epi16(ymm0, ymm7);
            ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
            ymm0 = _mm256_subs_epi16(ymm0, ymm5);
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm7);
            ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
            ymm1 = _mm256_subs_epi16(ymm1, ymm6);

            ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
            ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
            ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+pitch1))), 0x50);
            ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+pitch2))), 0x50);
            pS2 += 2 * pitch2;
            pS1 += 2 * pitch1;
            ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
            ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
            ymm2 = _mm256_subs_epi16(ymm2, ymm5);
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
            ymm3 = _mm256_subs_epi16(ymm3, ymm6);

            ymm_s0 = ymm2;
            ymm_s1 = ymm3;
            ymm_s2 = ymm1;

            ymm4 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
            ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
            ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+pitch1))), 0x50);
            ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+pitch2))), 0x50);
            pS2 += 2 * pitch2;
            pS1 += 2 * pitch1;
            ymm4 = _mm256_maddubs_epi16(ymm4, ymm7);
            ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
            ymm4 = _mm256_subs_epi16(ymm4, ymm2);
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
            ymm5 = _mm256_subs_epi16(ymm5, ymm3);

            ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
            ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
            ymm1 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+pitch1))), 0x50);
            ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+pitch2))), 0x50);
            pS2 += 2 * pitch2;
            pS1 += 2 * pitch1;
            ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
            ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
            ymm6 = _mm256_subs_epi16(ymm6, ymm2);
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm7);
            ymm2 = ymm4;
            ymm1 = _mm256_subs_epi16(ymm1, ymm3);

            ymm7 = ymm_s2;

            ymm4 = _mm256_adds_epi16(ymm4, ymm5);
            ymm3 = ymm6;
            ymm5 = _mm256_subs_epi16(ymm5, ymm2);
            ymm6 = _mm256_adds_epi16(ymm6, ymm1);
            ymm2 = ymm4;
            ymm1 = _mm256_subs_epi16(ymm1, ymm3);
            ymm4 = _mm256_adds_epi16(ymm4, ymm6);
            ymm3 = ymm5;
            ymm6 = _mm256_subs_epi16(ymm6, ymm2);
            ymm2 = ymm_s0;
            ymm5 = _mm256_adds_epi16(ymm5, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm3);
            ymm3 = ymm_s1;
            ymm_s1 = ymm1;
            ymm_s0 = ymm6;
            ymm_s2 = ymm5;

            ymm1 = ymm0;
            ymm5 = ymm2;
            ymm0 = _mm256_adds_epi16(ymm0, ymm7);
            ymm7 = _mm256_subs_epi16(ymm7, ymm1);
            ymm2 = _mm256_adds_epi16(ymm2, ymm3);
            ymm3 = _mm256_subs_epi16(ymm3, ymm5);
            ymm1 = ymm0;
            ymm5 = ymm7;
            ymm0 = _mm256_adds_epi16(ymm0, ymm2);
            ymm2 = _mm256_subs_epi16(ymm2, ymm1);
            ymm1 = ymm0;
            ymm7 = _mm256_adds_epi16(ymm7, ymm3);
            ymm3 = _mm256_subs_epi16(ymm3, ymm5);
            ymm0 = _mm256_adds_epi16(ymm0, ymm4);
            ymm4 = _mm256_subs_epi16(ymm4, ymm1);
            ymm5 = ymm_s2;
            ymm1 = ymm7;

            ymm7 = _mm256_adds_epi16(ymm7, ymm5);
            ymm5 = _mm256_subs_epi16(ymm5, ymm1);

            ymm6 = ymm0;
            ymm0 = _mm256_blend_epi16(ymm0, ymm4, 204);
            ymm4 = _mm256_slli_epi64(ymm4, 32);
            ymm6 = _mm256_srli_epi64(ymm6, 32);
            ymm4 = _mm256_or_si256(ymm4, ymm6);
            ymm1 = ymm0;
            ymm0 = _mm256_adds_epi16(ymm0, ymm4);
            ymm4 = _mm256_subs_epi16(ymm4, ymm1);

            ymm1 = ymm7;
            ymm7 = _mm256_blend_epi16(ymm7, ymm5, 204);
            ymm5 = _mm256_slli_epi64(ymm5, 32);
            ymm1 = _mm256_srli_epi64(ymm1, 32);
            ymm5 = _mm256_or_si256(ymm5, ymm1);

            ymm1 = ymm7;
            ymm7 = _mm256_adds_epi16(ymm7, ymm5);
            ymm5 = _mm256_subs_epi16(ymm5, ymm1);

            ymm1 = ymm0;
            ymm0 = _mm256_blend_epi16(ymm0, ymm4, 170);
            ymm4 = _mm256_slli_epi32(ymm4, 16);
            ymm1 = _mm256_srli_epi32(ymm1, 16);
            ymm4 = _mm256_or_si256(ymm4, ymm1);

            ymm4 = _mm256_abs_epi16(ymm4);
            ymm0 = _mm256_abs_epi16(ymm0);
            ymm0 = _mm256_max_epi16(ymm0, ymm4);

            ymm1 = ymm7;
            ymm7 = _mm256_blend_epi16(ymm7, ymm5, 170);
            ymm5 = _mm256_slli_epi32(ymm5, 16);
            ymm1 = _mm256_srli_epi32(ymm1, 16);
            ymm5 = _mm256_or_si256(ymm5, ymm1);

            ymm6 = ymm_s0;
            ymm5 = _mm256_abs_epi16(ymm5);
            ymm7 = _mm256_abs_epi16(ymm7);
            ymm4 = ymm2;
            ymm7 = _mm256_max_epi16(ymm7, ymm5);
            ymm1 = ymm_s1;
            ymm0 = _mm256_adds_epi16(ymm0, ymm7);
            ymm2 = _mm256_adds_epi16(ymm2, ymm6);
            ymm6 = _mm256_subs_epi16(ymm6, ymm4);
            ymm4 = ymm3;
            ymm3 = _mm256_adds_epi16(ymm3, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm4);

            ymm4 = ymm2;
            ymm2 = _mm256_blend_epi16(ymm2, ymm6, 204);
            ymm6 = _mm256_slli_epi64(ymm6, 32);
            ymm4 = _mm256_srli_epi64(ymm4, 32);
            ymm6 = _mm256_or_si256(ymm6, ymm4);
            ymm4 = ymm2;
            ymm2 = _mm256_adds_epi16(ymm2, ymm6);
            ymm6 = _mm256_subs_epi16(ymm6, ymm4);

            ymm4 = ymm3;
            ymm3 = _mm256_blend_epi16(ymm3, ymm1, 204);
            ymm1 = _mm256_slli_epi64(ymm1, 32);
            ymm4 = _mm256_srli_epi64(ymm4, 32);
            ymm1 = _mm256_or_si256(ymm1, ymm4);
            ymm4 = ymm3;
            ymm3 = _mm256_adds_epi16(ymm3, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm4);

            ymm4 = ymm2;
            ymm2 = _mm256_blend_epi16(ymm2, ymm6, 170);
            ymm6 = _mm256_slli_epi32(ymm6, 16);
            ymm4 = _mm256_srli_epi32(ymm4, 16);
            ymm6 = _mm256_or_si256(ymm6, ymm4);

            ymm4 = ymm3;
            ymm3 = _mm256_blend_epi16(ymm3, ymm1, 170);
            ymm1 = _mm256_slli_epi32(ymm1, 16);
            ymm4 = _mm256_srli_epi32(ymm4, 16);
            ymm1 = _mm256_or_si256(ymm1, ymm4);

            ymm6 = _mm256_abs_epi16(ymm6);
            ymm2 = _mm256_abs_epi16(ymm2);
            ymm2 = _mm256_max_epi16(ymm2, ymm6);
            ymm0 = _mm256_add_epi32(ymm0, ymm2);
            ymm3 = _mm256_abs_epi16(ymm3);
            ymm1 = _mm256_abs_epi16(ymm1);
            ymm3 = _mm256_max_epi16(ymm3, ymm1);
            ymm0 = _mm256_add_epi32(ymm0, ymm3);

            ymm0 = _mm256_madd_epi16(ymm0, ymm_one);
            ymm1 = _mm256_shuffle_epi32(ymm0, 14);
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            ymm1 = _mm256_shufflelo_epi16(ymm0, 14);
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            ymm1 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

            s = _mm_cvtsi128_si32(mm128(ymm0));
            satdPair[0] = (s << 1);

            s = _mm_cvtsi128_si32(mm128(ymm1));
            satdPair[1] = (s << 1);
        }

        AV1_FORCEINLINE void satd_8x8x2_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int32_t* satdPair) {
            const uint8_t *pS1, *pS2;
            int32_t  s;

            __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
            __m256i ymm_s0, ymm_s1, ymm_s2, ymm_one;

            static ALIGN_DECL(32) int8_t ymm_const11nb[32] = {1,1,1,1,1,1,1,1,1,-1,1,-1,1,-1,1,-1,   1,1,1,1,1,1,1,1,1,-1,1,-1,1,-1,1,-1};

            _mm256_zeroupper();

            pS1 = src1;
            pS2 = src2;

            ymm7 = _mm256_load_si256((__m256i*)&(ymm_const11nb[0]));
            ymm_one = _mm256_set1_epi16(1);

            //ymm0 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
            ymm0 = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i*)pS1));
            ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
            ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+pitch2))), 0x50);
            //ymm1 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+pitch1))), 0x50);
            ymm1 = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i*)(pS1+pitch1)));
            pS2 += 2 * pitch2;
            pS1 += 2 * pitch1;
            ymm0 = _mm256_maddubs_epi16(ymm0, ymm7);
            ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
            ymm0 = _mm256_subs_epi16(ymm0, ymm5);
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm7);
            ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
            ymm1 = _mm256_subs_epi16(ymm1, ymm6);

            //ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
            ymm2 = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i*)pS1));
            ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
            //ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+pitch1))), 0x50);
            ymm3 = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i*)(pS1+pitch1)));
            ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+pitch2))), 0x50);
            pS2 += 2 * pitch2;
            pS1 += 2 * pitch1;
            ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
            ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
            ymm2 = _mm256_subs_epi16(ymm2, ymm5);
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
            ymm3 = _mm256_subs_epi16(ymm3, ymm6);

            ymm_s0 = ymm2;
            ymm_s1 = ymm3;
            ymm_s2 = ymm1;

            //ymm4 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
            ymm4 = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i*)pS1));
            ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
            //ymm5 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+pitch1))), 0x50);
            ymm5 = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i*)(pS1+pitch1)));
            ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+pitch2))), 0x50);
            pS2 += 2 * pitch2;
            pS1 += 2 * pitch1;
            ymm4 = _mm256_maddubs_epi16(ymm4, ymm7);
            ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
            ymm4 = _mm256_subs_epi16(ymm4, ymm2);
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
            ymm5 = _mm256_subs_epi16(ymm5, ymm3);

            //ymm6 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1))), 0x50);
            ymm6 = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i*)pS1));
            ymm2 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2))), 0x50);
            //ymm1 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS1+pitch1))), 0x50);
            ymm1 = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i*)(pS1+pitch1)));
            ymm3 = _mm256_permute4x64_epi64(mm256(_mm_loadu_si128((__m128i*)(pS2+pitch2))), 0x50);
            pS2 += 2 * pitch2;
            pS1 += 2 * pitch1;
            ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
            ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
            ymm6 = _mm256_subs_epi16(ymm6, ymm2);
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm7);
            ymm2 = ymm4;
            ymm1 = _mm256_subs_epi16(ymm1, ymm3);

            ymm7 = ymm_s2;

            ymm4 = _mm256_adds_epi16(ymm4, ymm5);
            ymm3 = ymm6;
            ymm5 = _mm256_subs_epi16(ymm5, ymm2);
            ymm6 = _mm256_adds_epi16(ymm6, ymm1);
            ymm2 = ymm4;
            ymm1 = _mm256_subs_epi16(ymm1, ymm3);
            ymm4 = _mm256_adds_epi16(ymm4, ymm6);
            ymm3 = ymm5;
            ymm6 = _mm256_subs_epi16(ymm6, ymm2);
            ymm2 = ymm_s0;
            ymm5 = _mm256_adds_epi16(ymm5, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm3);
            ymm3 = ymm_s1;
            ymm_s1 = ymm1;
            ymm_s0 = ymm6;
            ymm_s2 = ymm5;

            ymm1 = ymm0;
            ymm5 = ymm2;
            ymm0 = _mm256_adds_epi16(ymm0, ymm7);
            ymm7 = _mm256_subs_epi16(ymm7, ymm1);
            ymm2 = _mm256_adds_epi16(ymm2, ymm3);
            ymm3 = _mm256_subs_epi16(ymm3, ymm5);
            ymm1 = ymm0;
            ymm5 = ymm7;
            ymm0 = _mm256_adds_epi16(ymm0, ymm2);
            ymm2 = _mm256_subs_epi16(ymm2, ymm1);
            ymm1 = ymm0;
            ymm7 = _mm256_adds_epi16(ymm7, ymm3);
            ymm3 = _mm256_subs_epi16(ymm3, ymm5);
            ymm0 = _mm256_adds_epi16(ymm0, ymm4);
            ymm4 = _mm256_subs_epi16(ymm4, ymm1);
            ymm5 = ymm_s2;
            ymm1 = ymm7;

            ymm7 = _mm256_adds_epi16(ymm7, ymm5);
            ymm5 = _mm256_subs_epi16(ymm5, ymm1);

            ymm6 = ymm0;
            ymm0 = _mm256_blend_epi16(ymm0, ymm4, 204);
            ymm4 = _mm256_slli_epi64(ymm4, 32);
            ymm6 = _mm256_srli_epi64(ymm6, 32);
            ymm4 = _mm256_or_si256(ymm4, ymm6);
            ymm1 = ymm0;
            ymm0 = _mm256_adds_epi16(ymm0, ymm4);
            ymm4 = _mm256_subs_epi16(ymm4, ymm1);

            ymm1 = ymm7;
            ymm7 = _mm256_blend_epi16(ymm7, ymm5, 204);
            ymm5 = _mm256_slli_epi64(ymm5, 32);
            ymm1 = _mm256_srli_epi64(ymm1, 32);
            ymm5 = _mm256_or_si256(ymm5, ymm1);

            ymm1 = ymm7;
            ymm7 = _mm256_adds_epi16(ymm7, ymm5);
            ymm5 = _mm256_subs_epi16(ymm5, ymm1);

            ymm1 = ymm0;
            ymm0 = _mm256_blend_epi16(ymm0, ymm4, 170);
            ymm4 = _mm256_slli_epi32(ymm4, 16);
            ymm1 = _mm256_srli_epi32(ymm1, 16);
            ymm4 = _mm256_or_si256(ymm4, ymm1);

            ymm4 = _mm256_abs_epi16(ymm4);
            ymm0 = _mm256_abs_epi16(ymm0);
            ymm0 = _mm256_max_epi16(ymm0, ymm4);

            ymm1 = ymm7;
            ymm7 = _mm256_blend_epi16(ymm7, ymm5, 170);
            ymm5 = _mm256_slli_epi32(ymm5, 16);
            ymm1 = _mm256_srli_epi32(ymm1, 16);
            ymm5 = _mm256_or_si256(ymm5, ymm1);

            ymm6 = ymm_s0;
            ymm5 = _mm256_abs_epi16(ymm5);
            ymm7 = _mm256_abs_epi16(ymm7);
            ymm4 = ymm2;
            ymm7 = _mm256_max_epi16(ymm7, ymm5);
            ymm1 = ymm_s1;
            ymm0 = _mm256_adds_epi16(ymm0, ymm7);
            ymm2 = _mm256_adds_epi16(ymm2, ymm6);
            ymm6 = _mm256_subs_epi16(ymm6, ymm4);
            ymm4 = ymm3;
            ymm3 = _mm256_adds_epi16(ymm3, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm4);

            ymm4 = ymm2;
            ymm2 = _mm256_blend_epi16(ymm2, ymm6, 204);
            ymm6 = _mm256_slli_epi64(ymm6, 32);
            ymm4 = _mm256_srli_epi64(ymm4, 32);
            ymm6 = _mm256_or_si256(ymm6, ymm4);
            ymm4 = ymm2;
            ymm2 = _mm256_adds_epi16(ymm2, ymm6);
            ymm6 = _mm256_subs_epi16(ymm6, ymm4);

            ymm4 = ymm3;
            ymm3 = _mm256_blend_epi16(ymm3, ymm1, 204);
            ymm1 = _mm256_slli_epi64(ymm1, 32);
            ymm4 = _mm256_srli_epi64(ymm4, 32);
            ymm1 = _mm256_or_si256(ymm1, ymm4);
            ymm4 = ymm3;
            ymm3 = _mm256_adds_epi16(ymm3, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm4);

            ymm4 = ymm2;
            ymm2 = _mm256_blend_epi16(ymm2, ymm6, 170);
            ymm6 = _mm256_slli_epi32(ymm6, 16);
            ymm4 = _mm256_srli_epi32(ymm4, 16);
            ymm6 = _mm256_or_si256(ymm6, ymm4);

            ymm4 = ymm3;
            ymm3 = _mm256_blend_epi16(ymm3, ymm1, 170);
            ymm1 = _mm256_slli_epi32(ymm1, 16);
            ymm4 = _mm256_srli_epi32(ymm4, 16);
            ymm1 = _mm256_or_si256(ymm1, ymm4);

            ymm6 = _mm256_abs_epi16(ymm6);
            ymm2 = _mm256_abs_epi16(ymm2);
            ymm2 = _mm256_max_epi16(ymm2, ymm6);
            ymm0 = _mm256_add_epi32(ymm0, ymm2);
            ymm3 = _mm256_abs_epi16(ymm3);
            ymm1 = _mm256_abs_epi16(ymm1);
            ymm3 = _mm256_max_epi16(ymm3, ymm1);
            ymm0 = _mm256_add_epi32(ymm0, ymm3);

            ymm0 = _mm256_madd_epi16(ymm0, ymm_one);
            ymm1 = _mm256_shuffle_epi32(ymm0, 14);
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            ymm1 = _mm256_shufflelo_epi16(ymm0, 14);
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            ymm1 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

            s = _mm_cvtsi128_si32(mm128(ymm0));
            satdPair[0] = (s << 1);

            s = _mm_cvtsi128_si32(mm128(ymm1));
            satdPair[1] = (s << 1);
        }

        inline AV1_FORCEINLINE void satd_with_const_8x8_pair_avx2(const uint8_t *src1, int pitch1, const uint8_t src2, int32_t* satdPair) {
            const uint8_t *pS1;
            int32_t  s;

            __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;
            __m256i ymm_s0, ymm_s1, ymm_s2, ymm_one;

            uint64_t s2x4_ = src2 + (src2 << 16); s2x4_ += (s2x4_ << 32);
            __m256i s2x4 = _mm256_setr_epi64x((s2x4_ << 1), 0, (s2x4_ << 1), 0);

            static ALIGN_DECL(32) int8_t ymm_const11nb[32] = { 1,1,1,1,1,1,1,1,1,-1,1,-1,1,-1,1,-1,   1,1,1,1,1,1,1,1,1,-1,1,-1,1,-1,1,-1 };

            _mm256_zeroupper();

            pS1 = src1;

            ymm7 = _mm256_load_si256((__m256i*)&(ymm_const11nb[0]));
            ymm_one = _mm256_set1_epi16(1);

            ymm0 = _mm256_permute4x64_epi64(mm256(_mm_load_si128((__m128i*)(pS1))), 0x50);
            ymm1 = _mm256_permute4x64_epi64(mm256(_mm_load_si128((__m128i*)(pS1 + pitch1))), 0x50);
            pS1 += 2 * pitch1;
            ymm0 = _mm256_maddubs_epi16(ymm0, ymm7);
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm7);
            ymm0 = _mm256_subs_epi16(ymm0, s2x4);
            ymm1 = _mm256_subs_epi16(ymm1, s2x4);

            ymm2 = _mm256_permute4x64_epi64(mm256(_mm_load_si128((__m128i*)(pS1))), 0x50);
            ymm3 = _mm256_permute4x64_epi64(mm256(_mm_load_si128((__m128i*)(pS1 + pitch1))), 0x50);
            pS1 += 2 * pitch1;
            ymm2 = _mm256_maddubs_epi16(ymm2, ymm7);
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm2 = _mm256_subs_epi16(ymm2, s2x4);
            ymm3 = _mm256_subs_epi16(ymm3, s2x4);

            ymm_s0 = ymm2;
            ymm_s1 = ymm3;
            ymm_s2 = ymm1;

            ymm4 = _mm256_permute4x64_epi64(mm256(_mm_load_si128((__m128i*)(pS1))), 0x50);
            ymm5 = _mm256_permute4x64_epi64(mm256(_mm_load_si128((__m128i*)(pS1 + pitch1))), 0x50);
            pS1 += 2 * pitch1;
            ymm4 = _mm256_maddubs_epi16(ymm4, ymm7);
            ymm5 = _mm256_maddubs_epi16(ymm5, ymm7);
            ymm4 = _mm256_subs_epi16(ymm4, s2x4);
            ymm5 = _mm256_subs_epi16(ymm5, s2x4);

            ymm6 = _mm256_permute4x64_epi64(mm256(_mm_load_si128((__m128i*)(pS1))), 0x50);
            ymm1 = _mm256_permute4x64_epi64(mm256(_mm_load_si128((__m128i*)(pS1 + pitch1))), 0x50);
            pS1 += 2 * pitch1;
            ymm6 = _mm256_maddubs_epi16(ymm6, ymm7);
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm7);
            ymm6 = _mm256_subs_epi16(ymm6, s2x4);
            ymm1 = _mm256_subs_epi16(ymm1, s2x4);
            ymm2 = ymm4;

            ymm7 = ymm_s2;

            ymm4 = _mm256_adds_epi16(ymm4, ymm5);
            ymm3 = ymm6;
            ymm5 = _mm256_subs_epi16(ymm5, ymm2);
            ymm6 = _mm256_adds_epi16(ymm6, ymm1);
            ymm2 = ymm4;
            ymm1 = _mm256_subs_epi16(ymm1, ymm3);
            ymm4 = _mm256_adds_epi16(ymm4, ymm6);
            ymm3 = ymm5;
            ymm6 = _mm256_subs_epi16(ymm6, ymm2);
            ymm2 = ymm_s0;
            ymm5 = _mm256_adds_epi16(ymm5, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm3);
            ymm3 = ymm_s1;
            ymm_s1 = ymm1;
            ymm_s0 = ymm6;
            ymm_s2 = ymm5;

            ymm1 = ymm0;
            ymm5 = ymm2;
            ymm0 = _mm256_adds_epi16(ymm0, ymm7);
            ymm7 = _mm256_subs_epi16(ymm7, ymm1);
            ymm2 = _mm256_adds_epi16(ymm2, ymm3);
            ymm3 = _mm256_subs_epi16(ymm3, ymm5);
            ymm1 = ymm0;
            ymm5 = ymm7;
            ymm0 = _mm256_adds_epi16(ymm0, ymm2);
            ymm2 = _mm256_subs_epi16(ymm2, ymm1);
            ymm1 = ymm0;
            ymm7 = _mm256_adds_epi16(ymm7, ymm3);
            ymm3 = _mm256_subs_epi16(ymm3, ymm5);
            ymm0 = _mm256_adds_epi16(ymm0, ymm4);
            ymm4 = _mm256_subs_epi16(ymm4, ymm1);
            ymm5 = ymm_s2;
            ymm1 = ymm7;

            ymm7 = _mm256_adds_epi16(ymm7, ymm5);
            ymm5 = _mm256_subs_epi16(ymm5, ymm1);

            ymm6 = ymm0;
            ymm0 = _mm256_blend_epi16(ymm0, ymm4, 204);
            ymm4 = _mm256_slli_epi64(ymm4, 32);
            ymm6 = _mm256_srli_epi64(ymm6, 32);
            ymm4 = _mm256_or_si256(ymm4, ymm6);
            ymm1 = ymm0;
            ymm0 = _mm256_adds_epi16(ymm0, ymm4);
            ymm4 = _mm256_subs_epi16(ymm4, ymm1);

            ymm1 = ymm7;
            ymm7 = _mm256_blend_epi16(ymm7, ymm5, 204);
            ymm5 = _mm256_slli_epi64(ymm5, 32);
            ymm1 = _mm256_srli_epi64(ymm1, 32);
            ymm5 = _mm256_or_si256(ymm5, ymm1);

            ymm1 = ymm7;
            ymm7 = _mm256_adds_epi16(ymm7, ymm5);
            ymm5 = _mm256_subs_epi16(ymm5, ymm1);

            ymm1 = ymm0;
            ymm0 = _mm256_blend_epi16(ymm0, ymm4, 170);
            ymm4 = _mm256_slli_epi32(ymm4, 16);
            ymm1 = _mm256_srli_epi32(ymm1, 16);
            ymm4 = _mm256_or_si256(ymm4, ymm1);

            ymm4 = _mm256_abs_epi16(ymm4);
            ymm0 = _mm256_abs_epi16(ymm0);
            ymm0 = _mm256_max_epi16(ymm0, ymm4);

            ymm1 = ymm7;
            ymm7 = _mm256_blend_epi16(ymm7, ymm5, 170);
            ymm5 = _mm256_slli_epi32(ymm5, 16);
            ymm1 = _mm256_srli_epi32(ymm1, 16);
            ymm5 = _mm256_or_si256(ymm5, ymm1);

            ymm6 = ymm_s0;
            ymm5 = _mm256_abs_epi16(ymm5);
            ymm7 = _mm256_abs_epi16(ymm7);
            ymm4 = ymm2;
            ymm7 = _mm256_max_epi16(ymm7, ymm5);
            ymm1 = ymm_s1;
            ymm0 = _mm256_adds_epi16(ymm0, ymm7);
            ymm2 = _mm256_adds_epi16(ymm2, ymm6);
            ymm6 = _mm256_subs_epi16(ymm6, ymm4);
            ymm4 = ymm3;
            ymm3 = _mm256_adds_epi16(ymm3, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm4);

            ymm4 = ymm2;
            ymm2 = _mm256_blend_epi16(ymm2, ymm6, 204);
            ymm6 = _mm256_slli_epi64(ymm6, 32);
            ymm4 = _mm256_srli_epi64(ymm4, 32);
            ymm6 = _mm256_or_si256(ymm6, ymm4);
            ymm4 = ymm2;
            ymm2 = _mm256_adds_epi16(ymm2, ymm6);
            ymm6 = _mm256_subs_epi16(ymm6, ymm4);

            ymm4 = ymm3;
            ymm3 = _mm256_blend_epi16(ymm3, ymm1, 204);
            ymm1 = _mm256_slli_epi64(ymm1, 32);
            ymm4 = _mm256_srli_epi64(ymm4, 32);
            ymm1 = _mm256_or_si256(ymm1, ymm4);
            ymm4 = ymm3;
            ymm3 = _mm256_adds_epi16(ymm3, ymm1);
            ymm1 = _mm256_subs_epi16(ymm1, ymm4);

            ymm4 = ymm2;
            ymm2 = _mm256_blend_epi16(ymm2, ymm6, 170);
            ymm6 = _mm256_slli_epi32(ymm6, 16);
            ymm4 = _mm256_srli_epi32(ymm4, 16);
            ymm6 = _mm256_or_si256(ymm6, ymm4);

            ymm4 = ymm3;
            ymm3 = _mm256_blend_epi16(ymm3, ymm1, 170);
            ymm1 = _mm256_slli_epi32(ymm1, 16);
            ymm4 = _mm256_srli_epi32(ymm4, 16);
            ymm1 = _mm256_or_si256(ymm1, ymm4);

            ymm6 = _mm256_abs_epi16(ymm6);
            ymm2 = _mm256_abs_epi16(ymm2);
            ymm2 = _mm256_max_epi16(ymm2, ymm6);
            ymm0 = _mm256_add_epi32(ymm0, ymm2);
            ymm3 = _mm256_abs_epi16(ymm3);
            ymm1 = _mm256_abs_epi16(ymm1);
            ymm3 = _mm256_max_epi16(ymm3, ymm1);
            ymm0 = _mm256_add_epi32(ymm0, ymm3);

            ymm0 = _mm256_madd_epi16(ymm0, ymm_one);
            ymm1 = _mm256_shuffle_epi32(ymm0, 14);
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            ymm1 = _mm256_shufflelo_epi16(ymm0, 14);
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            ymm1 = _mm256_permute2x128_si256(ymm0, ymm0, 0x01);

            s = _mm_cvtsi128_si32(mm128(ymm0));
            satdPair[0] = (s << 1);

            s = _mm_cvtsi128_si32(mm128(ymm1));
            satdPair[1] = (s << 1);
        }
    };

    int32_t satd_4x4_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2) {
        return details::satd_4x4_avx2(src1, pitch1, src2, pitch2);
    }
    int32_t satd_4x4_pitch64_avx2(const uint8_t* src1, const uint8_t* src2, int pitch2) {
        return details::satd_4x4_avx2(src1, 64, src2, pitch2);
    }
    int32_t satd_4x4_pitch64_both_avx2(const uint8_t* src1, const uint8_t* src2) {
        return details::satd_4x4_avx2(src1, 64, src2, 64);
    }

    void satd_4x4_pair_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int32_t* satdPair) {
        return details::satd_4x4_pair_avx2(src1, pitch1, src2, pitch2, satdPair);
    }
    void satd_4x4_pair_pitch64_avx2(const uint8_t* src1, const uint8_t* src2, int pitch2, int32_t* satdPair) {
        return details::satd_4x4_pair_avx2(src1, 64, src2, pitch2, satdPair);
    }
    void satd_4x4_pair_pitch64_both_avx2(const uint8_t* src1, const uint8_t* src2, int32_t* satdPair) {
        return details::satd_4x4_pair_avx2(src1, 64, src2, 64, satdPair);
    }

    int32_t satd_8x8_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2) {
        return details::satd_8x8_avx2(src1, pitch1, src2, pitch2);
    }
    int32_t satd_8x8_pitch64_avx2(const uint8_t* src1, const uint8_t* src2, int pitch2) {
        return details::satd_8x8_avx2(src1, 64, src2, pitch2);
    }
    int32_t satd_8x8_pitch64_both_avx2(const uint8_t* src1, const uint8_t* src2) {
        return details::satd_8x8_avx2(src1, 64, src2, 64);
    }

    void satd_8x8_pair_avx2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int32_t* satdPair) {
        return details::satd_8x8_pair_avx2(src1, pitch1, src2, pitch2, satdPair);
    }
    void satd_8x8_pair_pitch64_avx2(const uint8_t* src1, const uint8_t* src2, int pitch2, int32_t* satdPair) {
        return details::satd_8x8_pair_avx2(src1, 64, src2, pitch2, satdPair);
    }
    void satd_8x8_pair_pitch64_both_avx2(const uint8_t* src1, const uint8_t* src2, int32_t* satdPair) {
        return details::satd_8x8_pair_avx2(src1, 64, src2, 64, satdPair);
    }

    void satd_8x8x2_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int32_t* satdPair) {
        return details::satd_8x8x2_avx2(src1, pitch1, src2, pitch2, satdPair);
    }
    void satd_8x8x2_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, int pitch2, int32_t* satdPair) {
        return details::satd_8x8x2_avx2(src1, 64, src2, pitch2, satdPair);
    }

    template <int w, int h> int satd_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2) {
        // assume height and width are multiple of 4
        assert(!(w & 0x03));
        assert(!(h & 0x03));

        int satdTotal = 0;
        int satd[2] = {0, 0};

        if (w == 4 && h == 4) {
            return (satd_4x4_avx2(src1, pitch1, src2, pitch2) + 1) >> 1;
        } else if ( (h | w) & 0x07 ) {
            // multiple 4x4 blocks - do as many pairs as possible
            int widthPair = w & ~0x07;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 4, src1 += pitch1 * 4, src2 += pitch2 * 4) {
                int i = 0;
                for (; i < widthPair; i += 4*2) {
                    satd_4x4_pair_avx2(src1 + i, pitch1, src2 + i, pitch2, satd);
                    satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
                }

                if (widthRem) {
                    satd[0] = satd_4x4_avx2(src1 + i, pitch1, src2 + i, pitch2);
                    satdTotal += (satd[0] + 1) >> 1;
                }
            }
        }
        else if (w == 8 && h == 8) {
            /* single 8x8 block */
            satd[0] = satd_8x8_avx2(src1, pitch1, src2, pitch2);
            satdTotal += (satd[0] + 2) >> 2;
        } else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int widthPair = w & ~0x0f;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 8, src1 += pitch1 * 8, src2 += pitch2 * 8) {
                int i = 0;
                for (; i < widthPair; i += 8*2) {
                    satd_8x8_pair_avx2(src1 + i, pitch1, src2 + i, pitch2, satd);
                    satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
                }
                if (widthRem) {
                    satd[0] = satd_8x8_avx2(src1 + i, pitch1, src2 + i, pitch2);
                    satdTotal += (satd[0] + 2) >> 2;
                }
            }
        }

        return satdTotal;
    }
    template int satd_avx2< 4, 4>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2< 4, 8>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2< 8, 4>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2< 8, 8>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2< 8,16>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2<16, 8>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2<16,16>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2<16,32>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2<32,16>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2<32,32>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2<32,64>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2<64,32>(const uint8_t*,int,const uint8_t*,int);
    template int satd_avx2<64,64>(const uint8_t*,int,const uint8_t*,int);
    template <int w, int h> int satd_pitch64_avx2(const uint8_t *src1, const uint8_t *src2, int pitch2) {
        // assume height and width are multiple of 4
        assert(!(w & 0x03));
        assert(!(h & 0x03));

        int satdTotal = 0;
        int satd[2] = {0, 0};

        if (w == 4 && h == 4) {
            return (satd_4x4_pitch64_avx2(src1, src2, pitch2) + 1) >> 1;
        } else if ( (h | w) & 0x07 ) {
            // multiple 4x4 blocks - do as many pairs as possible
            int widthPair = w & ~0x07;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 4, src1 += 64 * 4, src2 += pitch2 * 4) {
                int i = 0;
                for (; i < widthPair; i += 4*2) {
                    satd_4x4_pair_avx2(src1 + i, 64, src2 + i, pitch2, satd);
                    satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
                }

                if (widthRem) {
                    satd[0] = satd_4x4_pitch64_avx2(src1 + i, src2 + i, pitch2);
                    satdTotal += (satd[0] + 1) >> 1;
                }
            }
        }
        else if (w == 8 && h == 8) {
            /* single 8x8 block */
            satd[0] = satd_8x8_pitch64_avx2(src1, src2, pitch2);
            satdTotal += (satd[0] + 2) >> 2;
        } else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int widthPair = w & ~0x0f;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 8, src1 += 64 * 8, src2 += pitch2 * 8) {
                int i = 0;
                for (; i < widthPair; i += 8*2) {
                    satd_8x8_pair_pitch64_avx2(src1 + i, src2 + i, pitch2, satd);
                    satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
                }
                if (widthRem) {
                    satd[0] = satd_8x8_pitch64_avx2(src1 + i, src2 + i, pitch2);
                    satdTotal += (satd[0] + 2) >> 2;
                }
            }
        }

        return satdTotal;
    }
    template int satd_pitch64_avx2< 4, 4>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2< 4, 8>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2< 8, 4>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2< 8, 8>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2< 8,16>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2<16, 8>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2<16,16>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2<16,32>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2<32,16>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2<32,32>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2<32,64>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2<64,32>(const uint8_t*,const uint8_t*,int);
    template int satd_pitch64_avx2<64,64>(const uint8_t*,const uint8_t*,int);

    template <int w, int h> int satd_pitch64_both_avx2(const uint8_t *src1, const uint8_t *src2) {
        // assume height and width are multiple of 4
        assert(!(w & 0x03));
        assert(!(h & 0x03));

        int satdTotal = 0;
        int satd[2] = {0, 0};

        if (w == 4 && h == 4) {
            return (satd_4x4_pitch64_both_avx2(src1, src2) + 1) >> 1;
        } else if ( (h | w) & 0x07 ) {
            // multiple 4x4 blocks - do as many pairs as possible
            int widthPair = w & ~0x07;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 4, src1 += 64 * 4, src2 += 64 * 4) {
                int i = 0;
                for (; i < widthPair; i += 4*2) {
                    satd_4x4_pair_pitch64_both_avx2(src1 + i, src2 + i, satd);
                    satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
                }

                if (widthRem) {
                    satd[0] = satd_4x4_pitch64_both_avx2(src1 + i, src2 + i);
                    satdTotal += (satd[0] + 1) >> 1;
                }
            }
        }
        else if (w == 8 && h == 8) {
            /* single 8x8 block */
            satd[0] = satd_8x8_pitch64_both_avx2(src1, src2);
            satdTotal += (satd[0] + 2) >> 2;
        } else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int widthPair = w & ~0x0f;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 8, src1 += 64 * 8, src2 += 64 * 8) {
                int i = 0;
                for (; i < widthPair; i += 8*2) {
                    satd_8x8_pair_pitch64_both_avx2(src1 + i, src2 + i, satd);
                    satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
                }
                if (widthRem) {
                    satd[0] = satd_8x8_pitch64_both_avx2(src1 + i, src2 + i);
                    satdTotal += (satd[0] + 2) >> 2;
                }
            }
        }

        return satdTotal;
    }
    template int satd_pitch64_both_avx2< 4, 4>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2< 4, 8>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2< 8, 4>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2< 8, 8>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2< 8,16>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2<16, 8>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2<16,16>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2<16,32>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2<32,16>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2<32,32>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2<32,64>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2<64,32>(const uint8_t*,const uint8_t*);
    template int satd_pitch64_both_avx2<64,64>(const uint8_t*,const uint8_t*);


    void satd_with_const_8x8_pitch64_avx2(const uint8_t* src1, const uint8_t src2, int32_t* satdPair) {
        return details::satd_with_const_8x8_pair_avx2(src1, 64, src2, satdPair);
    }

    template <int w, int h> int satd_with_const_pitch64_avx2(const uint8_t *src1, const uint8_t src2) {
        // assume height and width are multiple of 4
        assert(!(w & 0x03));
        assert(!(h & 0x03));

        int satdTotal = 0;
        int satd[2] = { 0, 0 };

        if (w == 4 && h == 4) {
            assert(0);
            return 0;
            //return (satd_4x4_pitch64_both_avx2(src1, src2) + 1) >> 1;
        }
        else if ((h | w) & 0x07) {
            // multiple 4x4 blocks - do as many pairs as possible
            assert(0);
            return 0;
            //int widthPair = w & ~0x07;
            //int widthRem = w - widthPair;
            //for (int j = 0; j < h; j += 4, src1 += 64 * 4, src2 += 64 * 4) {
            //    int i = 0;
            //    for (; i < widthPair; i += 4 * 2) {
            //        satd_4x4_pair_pitch64_both_avx2(src1 + i, src2 + i, satd);
            //        satdTotal += ((satd[0] + 1) >> 1) + ((satd[1] + 1) >> 1);
            //    }

            //    if (widthRem) {
            //        satd[0] = satd_4x4_pitch64_both_avx2(src1 + i, src2 + i);
            //        satdTotal += (satd[0] + 1) >> 1;
            //    }
            //}
        }
        else if (w == 8 && h == 8) {
            /* single 8x8 block */
            assert(0);
            return 0;
            //satd[0] = satd_8x8_pitch64_both_avx2(src1, src2);
            //satdTotal += (satd[0] + 2) >> 2;
        }
        else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int widthPair = w & ~0x0f;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 8, src1 += 64 * 8) {
                int i = 0;
                for (; i < widthPair; i += 8 * 2) {
                    satd_with_const_8x8_pitch64_avx2(src1 + i, src2, satd);
                    satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
                }
                if (widthRem) {
                    assert(0);
                    //satd[0] = satd_8x8_pitch64_both_avx2(src1 + i, src2 + i);
                    //satdTotal += (satd[0] + 2) >> 2;
                }
            }
        }

        return satdTotal;
    }
    template int satd_with_const_pitch64_avx2< 4, 4>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2< 4, 8>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2< 8, 4>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2< 8, 8>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2< 8, 16>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2<16, 8>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2<16, 16>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2<16, 32>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2<32, 16>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2<32, 32>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2<32, 64>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2<64, 32>(const uint8_t*, const uint8_t);
    template int satd_with_const_pitch64_avx2<64, 64>(const uint8_t*, const uint8_t);
}
