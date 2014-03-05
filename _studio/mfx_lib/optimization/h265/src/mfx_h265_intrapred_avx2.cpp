/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_AVX2) && defined(MFX_TARGET_OPTIMIZATION_AVX2)

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

namespace MFX_HEVC_PP
{

ALIGN_DECL(32) static const char fppShufTab[3][32] = {
    { 0, -1, 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 0, -1, 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1 },
    { 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 8, -1, 1, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 8, -1 },
    { 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 8, -1, 9, -1, 2, -1, 3, -1, 4, -1, 5, -1, 6, -1, 7, -1, 8, -1, 9, -1 },
};

/* assume PredPel buffer size is at least (4*width + 32) bytes to allow 32-byte loads (see calling code)
 * only the data in range [0, 4*width] is actually used
 * supported widths = 4, 8, 16, 32 (but should not require 4 - see spec)
 */
void MAKE_NAME(h265_FilterPredictPels_8u)(Ipp8u* PredPel, Ipp32s width)
{
    Ipp8u t0, t1, t2, t3;
    Ipp8u *p0;
    Ipp32s i;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    VM_ASSERT(width == 4 || width == 8 || width == 16 || width == 32);

    _mm256_zeroupper();

    /* precalcuate boundary cases (0, 2*w, 2*w+1, 4*w) to allow compact kernel, write back when finished */
    t0 = (PredPel[1] + 2 * PredPel[0] + PredPel[2*width+1] + 2) >> 2;
    t1 = PredPel[2*width];
    t2 = (PredPel[0] + 2 * PredPel[2*width+1] + PredPel[2*width+2] + 2) >> 2;
    t3 = PredPel[4*width];

    i = 4*width;
    p0 = PredPel;
    ymm2 = _mm256_loadu_si256((__m256i *)p0);
    ymm7 = _mm256_set1_epi16(2);
    do {
        /* shuffle and zero-extend to 16 bits */
        ymm2 = _mm256_permute4x64_epi64(ymm2, 0x94);
        ymm0 = _mm256_shuffle_epi8(ymm2, *(__m256i *)fppShufTab[0]);
        ymm1 = _mm256_shuffle_epi8(ymm2, *(__m256i *)fppShufTab[1]);
        ymm2 = _mm256_shuffle_epi8(ymm2, *(__m256i *)fppShufTab[2]);

        /* y[i] = (x[i-1] + 2*x[i] + x[i+1] + 2) >> 2 */
        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm1);
        ymm0 = _mm256_add_epi16(ymm0, ymm2);
        ymm0 = _mm256_add_epi16(ymm0, ymm7);
        ymm0 = _mm256_srli_epi16(ymm0, 2);
        ymm0 = _mm256_packus_epi16(ymm0, ymm0);

        /* read 32 bytes (16 new) before writing */
        ymm2 = _mm256_loadu_si256((__m256i *)(p0 + 16));
        ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
        _mm_storeu_si128((__m128i *)(p0 + 1), mm128(ymm0));

        p0 += 16;
        i -= 16;
    } while (i > 0);

    PredPel[0*width+0] = t0;
    PredPel[2*width+0] = t1;
    PredPel[2*width+1] = t2;
    PredPel[4*width+0] = t3;
}

/* width always = 32 (see spec) */
void MAKE_NAME(h265_FilterPredictPels_Bilinear_8u) (
    Ipp8u* pSrcDst,
    int width,
    int topLeft,
    int bottomLeft,
    int topRight)
{
    int x;
    Ipp8u *p0, *p1;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    /* see calling code - if any of this changes, code would need to be rewritten */
    VM_ASSERT(width == 32 && pSrcDst[0] == topLeft && pSrcDst[2*width] == topRight && pSrcDst[4*width] == bottomLeft);

    p0 = pSrcDst;
    if (topLeft == 128 && topRight == 128 && bottomLeft == 128) {
        /* fast path: set 128 consecutive pixels to 128 */
        ymm0 = _mm256_set1_epi8(128);
        _mm256_storeu_si256((__m256i *)(p0 +  0), ymm0);
        _mm256_storeu_si256((__m256i *)(p0 + 32), ymm0);
        _mm256_storeu_si256((__m256i *)(p0 + 64), ymm0);
        _mm256_storeu_si256((__m256i *)(p0 + 96), ymm0);
    } else {
        /* calculate 16 at a time with successive addition 
         * p[x] = ( ((64-x)*TL + x*TR + 32) >> 6 ) = ( ((64*TL + 32) + x*(TR - TL)) >> 6 )
         * similar for p[x+64]
         */
        ymm0 = _mm256_set1_epi16(64*topLeft + 32);
        ymm1 = ymm0;

        ymm2 = _mm256_set1_epi16(topRight - topLeft);
        ymm3 = _mm256_set1_epi16(bottomLeft - topLeft);
        ymm4 = _mm256_setr_epi16(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
            
        ymm6 = _mm256_slli_epi16(ymm2, 4);          /* 16*(TR-TL) */
        ymm7 = _mm256_slli_epi16(ymm3, 4);          /* 16*(BL-TL) */

        ymm2 = _mm256_mullo_epi16(ymm2, ymm4);      /* [0*(TR-TL) 1*(TR-TL) ... ] */
        ymm3 = _mm256_mullo_epi16(ymm3, ymm4);      /* [0*(BL-TL) 1*(BL-TL) ... ] */

        ymm0 = _mm256_add_epi16(ymm0, ymm2);
        ymm1 = _mm256_add_epi16(ymm1, ymm3);

        for(x = 0; x < 64; x += 32) {
            /* calculate 2 sets of 16 pixels for each direction */
            ymm2 = _mm256_srli_epi16(ymm0, 6);
            ymm3 = _mm256_srli_epi16(ymm1, 6);
            ymm0 = _mm256_add_epi16(ymm0, ymm6);
            ymm1 = _mm256_add_epi16(ymm1, ymm7);

            ymm4 = _mm256_srli_epi16(ymm0, 6);
            ymm5 = _mm256_srli_epi16(ymm1, 6);
            ymm0 = _mm256_add_epi16(ymm0, ymm6);
            ymm1 = _mm256_add_epi16(ymm1, ymm7);

            /* clip to 8 bits, write 32 pixels for each direction */
            ymm2 = _mm256_packus_epi16(ymm2, ymm4);
            ymm3 = _mm256_packus_epi16(ymm3, ymm5);
            _mm256_storeu_si256((__m256i *)(p0 +  0), _mm256_permute4x64_epi64(ymm2, 0xd8));
            _mm256_storeu_si256((__m256i *)(p0 + 64), _mm256_permute4x64_epi64(ymm3, 0xd8));

            p0 += 32;
        }
        pSrcDst[64] = topRight;
    }
}

} // end namespace MFX_HEVC_PP

#endif  // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...

#endif  // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
