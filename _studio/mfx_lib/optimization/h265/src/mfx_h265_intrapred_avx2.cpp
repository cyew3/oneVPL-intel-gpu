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

/* optimized kernel with const width and shift */
template <Ipp32s width, Ipp32s shift>
static void h265_PredictIntra_Planar_8u_Kernel(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch)
{
    ALIGN_DECL(32) Ipp16s leftColumn[32], horInc[32];
    Ipp32s row, col;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    if (width == 4) {
        /* broadcast scalar values */
        xmm7 = _mm_set1_epi16(width);
        xmm6 = _mm_set1_epi16(PredPel[1+width]);
        xmm1 = _mm_set1_epi16(PredPel[3*width+1]);

        /* xmm6 = horInc = topRight - leftColumn, xmm5 = leftColumn = (PredPel[2*width+1+i] << shift) + width */
        xmm5 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(int *)(&PredPel[2*width+1])));
        xmm6 = _mm_sub_epi16(xmm6, xmm5);
        xmm5 = _mm_slli_epi16(xmm5, shift);
        xmm5 = _mm_add_epi16(xmm5, xmm7);

        /* xmm2 = verInc = bottomLeft - topRow, xmm0 = topRow = (PredPel[1+i] << shift) */
        xmm0 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(int *)(&PredPel[1])));
        xmm2 = _mm_sub_epi16(xmm1, xmm0);
        xmm0 = _mm_slli_epi16(xmm0, shift);
        xmm7 = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);

        /* generate prediction signal one row at a time */
        for (row = 0; row < width; row++) {
            /* calculate horizontal component */
            xmm3 = _mm_shufflelo_epi16(xmm5, 0);
            xmm4 = _mm_shufflelo_epi16(xmm6, 0);
            xmm4 = _mm_mullo_epi16(xmm4, xmm7);
            xmm3 = _mm_add_epi16(xmm3, xmm4);

            /* add vertical component, scale and pack to 8 bits */
            xmm0 = _mm_add_epi16(xmm0, xmm2);
            xmm3 = _mm_add_epi16(xmm3, xmm0);
            xmm3 = _mm_srli_epi16(xmm3, shift+1);
            xmm3 = _mm_packus_epi16(xmm3, xmm3);

            /* shift in (leftColumn[j] + width) for next row */
            xmm5 = _mm_srli_si128(xmm5, 2);
            xmm6 = _mm_srli_si128(xmm6, 2);

            /* store 4 8-bit pixels */
            *(int *)(&pels[row*pitch]) = _mm_cvtsi128_si32(xmm3);
        }
    } else if (width == 8) {
        /* broadcast scalar values */
        xmm7 = _mm_set1_epi16(width);
        xmm6 = _mm_set1_epi16(PredPel[1+width]);
        xmm1 = _mm_set1_epi16(PredPel[3*width+1]);

        /* 8x8 block (see comments for 4x4 case) */
        xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&PredPel[2*width+1]));
        xmm6 = _mm_sub_epi16(xmm6, xmm5);
        xmm5 = _mm_slli_epi16(xmm5, shift);
        xmm5 = _mm_add_epi16(xmm5, xmm7);

        xmm0 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(&PredPel[1]));
        xmm2 = _mm_sub_epi16(xmm1, xmm0);
        xmm0 = _mm_slli_epi16(xmm0, shift);
        xmm7 = _mm_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8);

        for (row = 0; row < width; row++) {
            /* generate 8 pixels per row */
            xmm3 = _mm_shufflelo_epi16(xmm5, 0);
            xmm4 = _mm_shufflelo_epi16(xmm6, 0);
            xmm3 = _mm_unpacklo_epi64(xmm3, xmm3);
            xmm4 = _mm_unpacklo_epi64(xmm4, xmm4);
            xmm4 = _mm_mullo_epi16(xmm4, xmm7);
            xmm3 = _mm_add_epi16(xmm3, xmm4);

            xmm0 = _mm_add_epi16(xmm0, xmm2);
            xmm3 = _mm_add_epi16(xmm3, xmm0);
            xmm3 = _mm_srli_epi16(xmm3, shift+1);
            xmm3 = _mm_packus_epi16(xmm3, xmm3);

            xmm5 = _mm_srli_si128(xmm5, 2);
            xmm6 = _mm_srli_si128(xmm6, 2);

            /* store 8 8-bit pixels */
            _mm_storel_epi64((__m128i *)(&pels[row*pitch]), xmm3);
        }
    } else if (width == 16) {
        /* broadcast scalar values */
        ymm7 = _mm256_set1_epi16(width);
        ymm6 = _mm256_set1_epi16(PredPel[1+width]);
        ymm1 = _mm256_set1_epi16(PredPel[3*width+1]);

        /* load 16 8-bit pixels, zero extend to 16 bits */
        ymm5 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&PredPel[2*width+1])));
        ymm6 = _mm256_sub_epi16(ymm6, ymm5);
        ymm5 = _mm256_slli_epi16(ymm5, shift);
        ymm5 = _mm256_add_epi16(ymm5, ymm7);

        /* store in aligned temp buffer */
        _mm256_store_si256((__m256i*)(leftColumn), ymm5);
        _mm256_store_si256((__m256i*)(horInc), ymm6);

        ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&PredPel[1])));
        ymm2 = _mm256_sub_epi16(ymm1, ymm0);
        ymm0 = _mm256_slli_epi16(ymm0, shift);
        ymm7 = _mm256_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

        for (row = 0; row < width; row++) {
            /* generate 16 pixels per row */
            ymm3 = _mm256_set1_epi16(leftColumn[row]);
            ymm4 = _mm256_set1_epi16(horInc[row]);
            ymm4 = _mm256_mullo_epi16(ymm4, ymm7);
            ymm3 = _mm256_add_epi16(ymm3, ymm4);

            ymm0 = _mm256_add_epi16(ymm0, ymm2);
            ymm3 = _mm256_add_epi16(ymm3, ymm0);
            ymm3 = _mm256_srli_epi16(ymm3, shift+1);
            ymm3 = _mm256_packus_epi16(ymm3, ymm3);

            /* store 16 8-bit pixels */
            ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
            _mm_storeu_si128((__m128i *)(&pels[row*pitch]), mm128(ymm3));
        }
    } else if (width == 32) {
        /* broadcast scalar values */
        ymm7 = _mm256_set1_epi16(width);
        ymm6 = _mm256_set1_epi16(PredPel[1+width]);
        ymm1 = _mm256_set1_epi16(PredPel[3*width+1]);

        for (col = 0; col < width; col += 16) {
            /* load 16 8-bit pixels, zero extend to 16 bits */
            ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&PredPel[2*width+1+col])));
            ymm2 = _mm256_sub_epi16(ymm6, ymm0);
            ymm0 = _mm256_slli_epi16(ymm0, shift);
            ymm0 = _mm256_add_epi16(ymm0, ymm7);

            /* store in aligned temp buffer */
            _mm256_store_si256((__m256i*)(horInc + col), ymm2);
            _mm256_store_si256((__m256i*)(leftColumn + col), ymm0);
        }
        ymm7 = _mm256_setr_epi16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

        /* generate 8 pixels per row, repeat 2 times (width = 32) */
        for (col = 0; col < width; col += 16) {
            ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&PredPel[1+col])));
            ymm2 = _mm256_sub_epi16(ymm1, ymm0);
            ymm0 = _mm256_slli_epi16(ymm0, shift);

            for (row = 0; row < width; row++) {
                /* generate 16 pixels per row */
                ymm3 = _mm256_set1_epi16(leftColumn[row]);
                ymm4 = _mm256_set1_epi16(horInc[row]);
                ymm4 = _mm256_mullo_epi16(ymm4, ymm7);
                ymm3 = _mm256_add_epi16(ymm3, ymm4);

                ymm0 = _mm256_add_epi16(ymm0, ymm2);
                ymm3 = _mm256_add_epi16(ymm3, ymm0);
                ymm3 = _mm256_srli_epi16(ymm3, shift+1);
                ymm3 = _mm256_packus_epi16(ymm3, ymm3);

                /* store 16 8-bit pixels */
                ymm3 = _mm256_permute4x64_epi64(ymm3, 0xd8);
                _mm_storeu_si128((__m128i *)(&pels[row*pitch+col]), mm128(ymm3));
            }
            /* add 16 to each offset for next 16 columns */
            ymm7 = _mm256_add_epi16(ymm7, _mm256_set1_epi16(16));
        }
    }
}

/* planar intra prediction for 4x4, 8x8, 16x16, and 32x32 blocks (with arbitrary pitch) */
void MAKE_NAME(h265_PredictIntra_Planar_8u)(Ipp8u* PredPel, Ipp8u* pels, Ipp32s pitch, Ipp32s width)
{
    VM_ASSERT(width == 4 || width == 8 || width == 16 || width == 32);

    switch (width) {
    case 4:
        h265_PredictIntra_Planar_8u_Kernel< 4, 2>(PredPel, pels, pitch);
        break;
    case 8:
        h265_PredictIntra_Planar_8u_Kernel< 8, 3>(PredPel, pels, pitch);
        break;
    case 16:
        h265_PredictIntra_Planar_8u_Kernel<16, 4>(PredPel, pels, pitch);
        break;
    case 32:
        h265_PredictIntra_Planar_8u_Kernel<32, 5>(PredPel, pels, pitch);
        break;
    }
}

} // end namespace MFX_HEVC_PP

#endif  // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...

#endif  // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
