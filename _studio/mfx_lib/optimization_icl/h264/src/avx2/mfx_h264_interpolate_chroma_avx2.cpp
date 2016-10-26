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

#include <vector>

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

namespace MFX_PP
{

void ippiInterpolateChromaBlock(H264InterpolationParams_16u *interpolateInfo, Ipp16u *temporary_buffer);

typedef void (*pH264Interpolation_16u) (H264InterpolationParams_16u *pParams);

ALIGN_DECL(32) static const unsigned short filtTabChroma[8][16] = {
    {  8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0 },
    {  7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1 },
    {  6, 2, 6, 2, 6, 2, 6, 2, 6, 2, 6, 2, 6, 2, 6, 2 },
    {  5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3 },
    {  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
    {  3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5 },
    {  2, 6, 2, 6, 2, 6, 2, 6, 2, 6, 2, 6, 2, 6, 2, 6 },
    {  1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7, 1, 7 },
};

// assume width multiple of 2 (UV pairs), height multiple of 2
void h264_interpolate_chroma_type_nv12_00_16u_avx2(H264InterpolationParams_16u *pParams)
{
    Ipp32s x, y;

    const Ipp16u *pSrc  = pParams->pSrc;
    Ipp32u srcStep      = (Ipp32u)pParams->srcStep;
    Ipp16u *pDst        = pParams->pDst;
    Ipp32u dstStep      = (Ipp32u)pParams->dstStep;

    __m128i xmm0, xmm1;
    __m256i ymm0, ymm1;

    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    _mm256_zeroupper();

    if ((pParams->blockWidth & 0x07) == 0) {
        for (y = 0; y < pParams->blockHeight; y += 2) {
            for (x = 0; x < pParams->blockWidth; x += 8) {
                ymm0 = _mm256_loadu_si256( (__m256i *)(pSrc + 0*srcStep + 2*x) );
                ymm1 = _mm256_loadu_si256( (__m256i *)(pSrc + 1*srcStep + 2*x) );
                _mm256_storeu_si256( (__m256i *)(pDst + 0*dstStep + 2*x), ymm0 );
                _mm256_storeu_si256( (__m256i *)(pDst + 1*dstStep + 2*x), ymm1 );
            }
            pSrc += 2*srcStep;
            pDst += 2*dstStep;
        }
    } else if ((pParams->blockWidth & 0x03) == 0) {
        for (y = 0; y < pParams->blockHeight; y += 2) {
            for (x = 0; x < pParams->blockWidth; x += 4) {
                xmm0 = _mm_loadu_si128( (__m128i *)(pSrc + 0*srcStep + 2*x) );
                xmm1 = _mm_loadu_si128( (__m128i *)(pSrc + 1*srcStep + 2*x) );
                _mm_storeu_si128( (__m128i *)(pDst + 0*dstStep + 2*x), xmm0 );
                _mm_storeu_si128( (__m128i *)(pDst + 1*dstStep + 2*x), xmm1 );
            }
            pSrc += 2*srcStep;
            pDst += 2*dstStep;
        }
    } else if ((pParams->blockWidth & 0x01) == 0) {
        for (y = 0; y < pParams->blockHeight; y += 2) {
            for (x = 0; x < pParams->blockWidth; x += 2) {
                xmm0 = _mm_loadl_epi64( (__m128i *)(pSrc + 0*srcStep + 2*x) );
                xmm1 = _mm_loadl_epi64( (__m128i *)(pSrc + 1*srcStep + 2*x) );
                _mm_storel_epi64( (__m128i *)(pDst + 0*dstStep + 2*x), xmm0 );
                _mm_storel_epi64( (__m128i *)(pDst + 1*dstStep + 2*x), xmm1 );
            }
            pSrc += 2*srcStep;
            pDst += 2*dstStep;
        }
    }
}

template <Ipp32s widthMul>
static void InterpChroma16u_0x_kernel_avx2(H264InterpolationParams_16u *pParams)
{
    Ipp32s x, y;
    const Ipp32s widthScale = (widthMul == 8 ? 1 : 2);  // 1 for x8, 2 for x4 or x2;

    const Ipp16u *pSrc  = pParams->pSrc;
    Ipp32u srcStep      = pParams->srcStep;
    Ipp16u *pDst        = pParams->pDst;
    Ipp32u dstStep      = pParams->dstStep;

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4;

    _mm256_zeroupper();

    ymm4 = _mm256_set1_epi32(4);
    for (y = 0; y < pParams->blockHeight; y += widthScale) {
        pSrc = pParams->pSrc + y*srcStep;
        pDst = pParams->pDst + y*dstStep;

        for (x = 0; x < pParams->blockWidth; x += widthMul) {
            if (widthMul == 8) {
                ymm0 = _mm256_loadu_si256( (__m256i *)(pSrc + 2*0) );   // 8 UV pairs [0-7]
                ymm1 = _mm256_loadu_si256( (__m256i *)(pSrc + 2*1) );   // 8 UV pairs [1-8]
            } else if (widthMul == 4) {
                ymm0 = _mm256_permute2x128_si256(mm256(_mm_loadu_si128( (__m128i *)(pSrc + 0*srcStep + 2*0) )), mm256(_mm_loadu_si128( (__m128i *)(pSrc + 1*srcStep + 2*0) )), 0x20);   // 4 UV pairs [0-3] for 2 rows
                ymm1 = _mm256_permute2x128_si256(mm256(_mm_loadu_si128( (__m128i *)(pSrc + 0*srcStep + 2*1) )), mm256(_mm_loadu_si128( (__m128i *)(pSrc + 1*srcStep + 2*1) )), 0x20);   // 4 UV pairs [1-4] for 2 rows
            } else if (widthMul == 2) {
                ymm0 = _mm256_permute2x128_si256(mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 0*srcStep + 2*0) )), mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 1*srcStep + 2*0) )), 0x20);   // 2 UV pairs [0-1] for 2 rows
                ymm1 = _mm256_permute2x128_si256(mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 0*srcStep + 2*1) )), mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 1*srcStep + 2*1) )), 0x20);   // 2 UV pairs [1-2] for 2 rows
            }

            ymm2 = _mm256_unpacklo_epi16(ymm0, ymm1);           // U0U1 V0V1 U1U2 V1V2 U4U5 V4V5 U5U6 V5V6
            ymm3 = _mm256_unpackhi_epi16(ymm0, ymm1);           // U2U3 V2V3 U3U4 V3V4 U6U7 V6V7 U7U8 V7V8

            ymm2 = _mm256_madd_epi16(ymm2, *(__m256i *)(filtTabChroma[pParams->hFraction]));
            ymm3 = _mm256_madd_epi16(ymm3, *(__m256i *)(filtTabChroma[pParams->hFraction]));
            
            ymm2 = _mm256_add_epi32(ymm2, ymm4);
            ymm3 = _mm256_add_epi32(ymm3, ymm4);
            
            ymm2 = _mm256_srli_epi32(ymm2, 3);
            ymm3 = _mm256_srli_epi32(ymm3, 3);
            ymm2 = _mm256_packus_epi32(ymm2, ymm3);

            // store output
            if (widthMul == 8) {
                _mm256_storeu_si256((__m256i *)(pDst), ymm2);
            } else if (widthMul == 4) {
                _mm_storeu_si128((__m128i *)(pDst + 0*dstStep), mm128(ymm2));
                _mm_storeu_si128((__m128i *)(pDst + 1*dstStep), _mm256_extracti128_si256(ymm2, 0x01));
            } else if (widthMul == 2) {
                _mm_storel_epi64((__m128i *)(pDst + 0*dstStep), mm128(ymm2));
                _mm_storel_epi64((__m128i *)(pDst + 1*dstStep), _mm256_extracti128_si256(ymm2, 0x01));
            }

            pSrc += 2*widthMul;
            pDst += 2*widthMul;
        }
    }
}

void h264_interpolate_chroma_type_nv12_0x_16u_avx2(H264InterpolationParams_16u *pParams)
{
    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    if      ((pParams->blockWidth & 0x07) == 0)     InterpChroma16u_0x_kernel_avx2<8>(pParams);
    else if ((pParams->blockWidth & 0x03) == 0)     InterpChroma16u_0x_kernel_avx2<4>(pParams);
    else if ((pParams->blockWidth & 0x01) == 0)     InterpChroma16u_0x_kernel_avx2<2>(pParams);
}

template <Ipp32s widthMul>
static void InterpChroma16u_y0_kernel_avx2(H264InterpolationParams_16u *pParams)
{
    Ipp32s x, y;
    const Ipp32s widthScale = (widthMul == 8 ? 1 : 2);  // 1 for x8, 2 for x4 or x2;

    const Ipp16u *pSrc  = pParams->pSrc;
    Ipp32u srcStep      = pParams->srcStep;
    Ipp16u *pDst        = pParams->pDst;
    Ipp32u dstStep      = pParams->dstStep;

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4;

    _mm256_zeroupper();

    for (x = 0; x < pParams->blockWidth; x += widthMul) {
        pSrc = pParams->pSrc + 2*x;
        pDst = pParams->pDst + 2*x;

        // load row 0
        if (widthMul == 8) {
            ymm0 = _mm256_loadu_si256( (__m256i *)(pSrc + 2*0 + 0*srcStep) );   // 8 UV pairs [0-7]
        } else if (widthMul == 4) {
            ymm0 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*0 + 0*srcStep)) );
        } else if (widthMul == 2) {
            ymm0 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*0 + 0*srcStep)) );
        }

        for (y = 0; y < pParams->blockHeight; y += widthScale) {
            if (widthMul == 8) {
                ymm1 = _mm256_loadu_si256( (__m256i *)(pSrc + 2*0 + 1*srcStep) );
            } else if (widthMul == 4) {
                ymm1 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*0 + 1*srcStep)) );
                ymm2 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*0 + 2*srcStep)) );

                ymm0 = _mm256_inserti128_si256(ymm0, mm128(ymm1), 0x01); // [row 0 | row 1]
                ymm1 = _mm256_inserti128_si256(ymm1, mm128(ymm2), 0x01); // [row 1 | row 2]
            } else if (widthMul == 2) {
                ymm1 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*0 + 1*srcStep)) );
                ymm2 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*0 + 2*srcStep)) );

                ymm0 = _mm256_inserti128_si256(ymm0, mm128(ymm1), 0x01); // [row 0 | row 1]
                ymm1 = _mm256_inserti128_si256(ymm1, mm128(ymm2), 0x01); // [row 1 | row 2]
            }

            ymm3 = _mm256_unpacklo_epi16(ymm0, ymm1);           // U0U1 V0V1 U1U2 V1V2 U4U5 V4V5 U5U6 V5V6
            ymm4 = _mm256_unpackhi_epi16(ymm0, ymm1);           // U2U3 V2V3 U3U4 V3V4 U6U7 V6V7 U7U8 V7V8

            ymm3 = _mm256_madd_epi16(ymm3, *(__m256i *)(filtTabChroma[pParams->vFraction]));
            ymm4 = _mm256_madd_epi16(ymm4, *(__m256i *)(filtTabChroma[pParams->vFraction]));
            
            ymm3 = _mm256_add_epi32(ymm3, _mm256_set1_epi32(4));
            ymm4 = _mm256_add_epi32(ymm4, _mm256_set1_epi32(4));
            
            ymm3 = _mm256_srli_epi32(ymm3, 3);
            ymm4 = _mm256_srli_epi32(ymm4, 3);
            ymm3 = _mm256_packus_epi32(ymm3, ymm4);

            // store output and save next row
            if (widthMul == 8) {
                _mm256_storeu_si256((__m256i *)pDst, ymm3);
                ymm0 = ymm1;
            } else if (widthMul == 4) {
                _mm_storeu_si128((__m128i *)(pDst + 0*dstStep), mm128(ymm3));
                _mm_storeu_si128((__m128i *)(pDst + 1*dstStep), _mm256_extracti128_si256(ymm3, 0x01));
                ymm0 = ymm2;
            } else if (widthMul == 2) {
                _mm_storel_epi64((__m128i *)(pDst + 0*dstStep), mm128(ymm3));
                _mm_storel_epi64((__m128i *)(pDst + 1*dstStep), _mm256_extracti128_si256(ymm3, 0x01));
                ymm0 = ymm2;
            }

            pSrc += srcStep*widthScale;
            pDst += dstStep*widthScale;
        }
    }
}

void h264_interpolate_chroma_type_nv12_y0_16u_avx2(H264InterpolationParams_16u *pParams)
{
    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    if      ((pParams->blockWidth & 0x07) == 0)     InterpChroma16u_y0_kernel_avx2<8>(pParams);
    else if ((pParams->blockWidth & 0x03) == 0)     InterpChroma16u_y0_kernel_avx2<4>(pParams);
    else if ((pParams->blockWidth & 0x01) == 0)     InterpChroma16u_y0_kernel_avx2<2>(pParams);
}

template <Ipp32s widthMul>
static void InterpChroma16u_yx_kernel_avx2(H264InterpolationParams_16u *pParams)
{
    Ipp32s x, y;
    const Ipp32s widthScale = (widthMul == 8 ? 1 : 2);  // 1 for x8, 2 for x4 or x2;

    const Ipp16u *pSrc  = pParams->pSrc;
    Ipp32u srcStep      = pParams->srcStep;
    Ipp16u *pDst        = pParams->pDst;
    Ipp32u dstStep      = pParams->dstStep;

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    _mm256_zeroupper();

    for (x = 0; x < pParams->blockWidth; x += widthMul) {
        pSrc = pParams->pSrc + 2*x;
        pDst = pParams->pDst + 2*x;

        // load row 0
        if (widthMul == 8) {
            ymm0 = _mm256_loadu_si256( (__m256i *)(pSrc + 2*0 + 0*srcStep) );   // 8 UV pairs [0-7]
            ymm1 = _mm256_loadu_si256( (__m256i *)(pSrc + 2*1 + 0*srcStep) );   // 8 UV pairs [1-8]
        } else if (widthMul == 4) {
            ymm0 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*0 + 0*srcStep)) );
            ymm1 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*1 + 0*srcStep)) );
        } else if (widthMul == 2) {
            ymm0 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*0 + 0*srcStep)) );
            ymm1 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*1 + 0*srcStep)) );
        }

        for (y = 0; y < pParams->blockHeight; y += widthScale) {
            // load next row
            if (widthMul == 8) {
                ymm2 = _mm256_loadu_si256( (__m256i *)(pSrc + 2*0 + 1*srcStep) );
                ymm3 = _mm256_loadu_si256( (__m256i *)(pSrc + 2*1 + 1*srcStep) );
            } else if (widthMul == 4) {
                ymm2 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*0 + 1*srcStep)) );
                ymm3 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*1 + 1*srcStep)) );
                ymm6 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*0 + 2*srcStep)) );
                ymm7 = mm256(_mm_loadu_si128( (__m128i *)(pSrc + 2*1 + 2*srcStep)) );

                ymm0 = _mm256_inserti128_si256(ymm0, mm128(ymm2), 0x01); // [row 0 | row 1]
                ymm1 = _mm256_inserti128_si256(ymm1, mm128(ymm3), 0x01); // [row 0 | row 1]
                ymm2 = _mm256_inserti128_si256(ymm2, mm128(ymm6), 0x01); // [row 1 | row 2]
                ymm3 = _mm256_inserti128_si256(ymm3, mm128(ymm7), 0x01); // [row 1 | row 2]
            } else if (widthMul == 2) {
                ymm2 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*0 + 1*srcStep)) );
                ymm3 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*1 + 1*srcStep)) );
                ymm6 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*0 + 2*srcStep)) );
                ymm7 = mm256(_mm_loadl_epi64( (__m128i *)(pSrc + 2*1 + 2*srcStep)) );

                ymm0 = _mm256_inserti128_si256(ymm0, mm128(ymm2), 0x01); // [row 0 | row 1]
                ymm1 = _mm256_inserti128_si256(ymm1, mm128(ymm3), 0x01); // [row 0 | row 1]
                ymm2 = _mm256_inserti128_si256(ymm2, mm128(ymm6), 0x01); // [row 1 | row 2]
                ymm3 = _mm256_inserti128_si256(ymm3, mm128(ymm7), 0x01); // [row 1 | row 2]
            }

            ymm4 = _mm256_unpacklo_epi16(ymm0, ymm1);           // U0U1 V0V1 U1U2 V1V2 U4U5 V4V5 U5U6 V5V6
            ymm5 = _mm256_unpackhi_epi16(ymm0, ymm1);           // U2U3 V2V3 U3U4 V3V4 U6U7 V6V7 U7U8 V7V8

            ymm0 = _mm256_madd_epi16(ymm4, *(__m256i *)(filtTabChroma[pParams->hFraction]));
            ymm1 = _mm256_madd_epi16(ymm5, *(__m256i *)(filtTabChroma[pParams->hFraction]));
            ymm0 = _mm256_packus_epi32(ymm0, ymm1);             // output = max 13 bits 

            ymm4 = _mm256_unpacklo_epi16(ymm2, ymm3);
            ymm5 = _mm256_unpackhi_epi16(ymm2, ymm3);

            ymm4 = _mm256_madd_epi16(ymm4, *(__m256i *)(filtTabChroma[pParams->hFraction]));
            ymm5 = _mm256_madd_epi16(ymm5, *(__m256i *)(filtTabChroma[pParams->hFraction]));
            ymm4 = _mm256_packus_epi32(ymm4, ymm5);

            ymm1 = _mm256_unpackhi_epi16(ymm0, ymm4);
            ymm0 = _mm256_unpacklo_epi16(ymm0, ymm4);

            ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)(filtTabChroma[pParams->vFraction]));
            ymm1 = _mm256_madd_epi16(ymm1, *(__m256i *)(filtTabChroma[pParams->vFraction]));

            // round
            ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(32));
            ymm1 = _mm256_add_epi32(ymm1, _mm256_set1_epi32(32));

            // scale (16 bits back to 10 bits)
            ymm0 = _mm256_srli_epi32(ymm0, 6);
            ymm1 = _mm256_srli_epi32(ymm1, 6);
            ymm0 = _mm256_packus_epi32(ymm0, ymm1);

            // store output and save next row
            if (widthMul == 8) {
                _mm256_storeu_si256((__m256i *)pDst, ymm0);
                ymm0 = ymm2;
                ymm1 = ymm3;
            } else if (widthMul == 4) {
                _mm_storeu_si128((__m128i *)(pDst + 0*dstStep), mm128(ymm0));
                _mm_storeu_si128((__m128i *)(pDst + 1*dstStep), _mm256_extracti128_si256(ymm0, 0x01));
                ymm0 = ymm6;
                ymm1 = ymm7;
            } else if (widthMul == 2) {
                _mm_storel_epi64((__m128i *)(pDst + 0*dstStep), mm128(ymm0));
                _mm_storel_epi64((__m128i *)(pDst + 1*dstStep), _mm256_extracti128_si256(ymm0, 0x01));
                ymm0 = ymm6;
                ymm1 = ymm7;
            }

            pSrc += srcStep*widthScale;
            pDst += dstStep*widthScale;
        }
    }
}

void h264_interpolate_chroma_type_nv12_yx_16u_avx2(H264InterpolationParams_16u *pParams)
{
    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    if      ((pParams->blockWidth & 0x07) == 0)     InterpChroma16u_yx_kernel_avx2<8>(pParams);
    else if ((pParams->blockWidth & 0x03) == 0)     InterpChroma16u_yx_kernel_avx2<4>(pParams);
    else if ((pParams->blockWidth & 0x01) == 0)     InterpChroma16u_yx_kernel_avx2<2>(pParams);
}

static pH264Interpolation_16u h264_interpolate_chroma_type_table_16u_pxmx_avx2[4] =
{
    &h264_interpolate_chroma_type_nv12_00_16u_avx2,
    &h264_interpolate_chroma_type_nv12_0x_16u_avx2,
    &h264_interpolate_chroma_type_nv12_y0_16u_avx2,
    &h264_interpolate_chroma_type_nv12_yx_16u_avx2
};

void H264_FASTCALL MFX_Dispatcher_avx2::InterpolateChromaBlock_16u(const IppVCInterpolateBlock_16u *interpolateInfo)
{
    H264InterpolationParams_16u params;
    params.pSrc = interpolateInfo->pSrc[0];
    params.srcStep = interpolateInfo->srcStep;
    params.pDst = interpolateInfo->pDst[0];
    params.dstStep = interpolateInfo->dstStep;

    params.blockWidth = interpolateInfo->sizeBlock.width;
    params.blockHeight = interpolateInfo->sizeBlock.height;

    params.pointVector = interpolateInfo->pointVector;
    params.pointBlockPos = interpolateInfo->pointBlockPos;
    params.frameSize = interpolateInfo->sizeFrame;

    Ipp16u temporary_buffer[2*16*17 + 16];

    ippiInterpolateChromaBlock(&params, temporary_buffer); // boundaries

    h264_interpolate_chroma_type_table_16u_pxmx_avx2[params.iType](&params);
}

} // namespace MFX_PP

