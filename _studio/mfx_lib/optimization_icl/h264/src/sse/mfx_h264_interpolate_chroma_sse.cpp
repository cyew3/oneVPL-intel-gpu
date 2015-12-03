
#include "mfxvideo.h"
#include "ippvc.h"
#include <string.h>
#include <algorithm>

#include "mfx_dispatcher.h"

#include <vector>

namespace MFX_PP
{

void ippiInterpolateChromaBlock(H264InterpolationParams_16u *interpolateInfo, Ipp16u *temporary_buffer);

typedef void (*pH264Interpolation_16u) (H264InterpolationParams_16u *pParams);

ALIGN_DECL(16) static const unsigned short filtTabChroma[8][8] = {
    {  8, 0, 8, 0, 8, 0, 8, 0 },
    {  7, 1, 7, 1, 7, 1, 7, 1 },
    {  6, 2, 6, 2, 6, 2, 6, 2 },
    {  5, 3, 5, 3, 5, 3, 5, 3 },
    {  4, 4, 4, 4, 4, 4, 4, 4 },
    {  3, 5, 3, 5, 3, 5, 3, 5 },
    {  2, 6, 2, 6, 2, 6, 2, 6 },
    {  1, 7, 1, 7, 1, 7, 1, 7 },
};

// assume width multiple of 2 (UV pairs), height multiple of 2
void h264_interpolate_chroma_type_nv12_00_16u_sse(H264InterpolationParams_16u *pParams)
{
    Ipp32s x, y;

    const Ipp16u *pSrc  = pParams->pSrc;
    Ipp32u srcStep      = (Ipp32u)pParams->srcStep;
    Ipp16u *pDst        = pParams->pDst;
    Ipp32u dstStep      = (Ipp32u)pParams->dstStep;

    __m128i xmm0, xmm1;

    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    if ((pParams->blockWidth & 0x03) == 0) {
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
static void InterpChroma16u_0x_kernel_sse(H264InterpolationParams_16u *pParams)
{
    Ipp32s x, y;
    const Ipp16u *pSrc  = pParams->pSrc;
    Ipp32u srcStep      = (Ipp32u)pParams->srcStep;
    Ipp16u *pDst        = pParams->pDst;
    Ipp32u dstStep      = (Ipp32u)pParams->dstStep;

    __m128i xmm0, xmm1, xmm2, xmm3, xmm4;

    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    xmm4 = _mm_set1_epi32(4);
    for (y = 0; y < pParams->blockHeight; y++) {
        pSrc = pParams->pSrc + y*srcStep;
        pDst = pParams->pDst + y*dstStep;

        for (x = 0; x < pParams->blockWidth; x += widthMul) {
            if (widthMul == 4) {
                xmm0 = _mm_loadu_si128( (__m128i *)(pSrc + 2*0) );   // 4 UV pairs [0-3]
                xmm1 = _mm_loadu_si128( (__m128i *)(pSrc + 2*1) );   // 4 UV pairs [1-4]
            } else if (widthMul == 2) {
                xmm0 = _mm_loadl_epi64( (__m128i *)(pSrc + 2*0) );   // 2 UV pairs [0-1]
                xmm1 = _mm_loadl_epi64( (__m128i *)(pSrc + 2*1) );   // 2 UV pairs [1-2]
            }

            xmm2 = _mm_unpacklo_epi16(xmm0, xmm1);           // U0U1 V0V1 U1U2 V1V2
            xmm3 = _mm_unpackhi_epi16(xmm0, xmm1);           // U2U3 V2V3 U3U4 V3V4

            xmm2 = _mm_madd_epi16(xmm2, *(__m128i *)(filtTabChroma[pParams->hFraction]));
            xmm3 = _mm_madd_epi16(xmm3, *(__m128i *)(filtTabChroma[pParams->hFraction]));
            
            xmm2 = _mm_add_epi32(xmm2, xmm4);
            xmm3 = _mm_add_epi32(xmm3, xmm4);
            
            xmm2 = _mm_srli_epi32(xmm2, 3);
            xmm3 = _mm_srli_epi32(xmm3, 3);
            xmm2 = _mm_packus_epi32(xmm2, xmm3);

            // store output
            if (widthMul == 4) {
                _mm_storeu_si128((__m128i *)(pDst + 0*dstStep), xmm2);
            } else if (widthMul == 2) {
                _mm_storel_epi64((__m128i *)(pDst + 0*dstStep), xmm2);
            }

            pSrc += 2*widthMul;
            pDst += 2*widthMul;
        }
    }
}

void h264_interpolate_chroma_type_nv12_0x_16u_sse(H264InterpolationParams_16u *pParams)
{
    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    if ((pParams->blockWidth & 0x03) == 0)          InterpChroma16u_0x_kernel_sse<4>(pParams);
    else if ((pParams->blockWidth & 0x01) == 0)     InterpChroma16u_0x_kernel_sse<2>(pParams);
}

template <Ipp32s widthMul>
static void InterpChroma16u_y0_kernel_sse(H264InterpolationParams_16u *pParams)
{
    Ipp32s x, y;
    const Ipp16u *pSrc  = pParams->pSrc;
    Ipp32u srcStep      = (Ipp32u)pParams->srcStep;
    Ipp16u *pDst        = pParams->pDst;
    Ipp32u dstStep      = (Ipp32u)pParams->dstStep;

    __m128i xmm0, xmm1, xmm3, xmm4;

    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    for (x = 0; x < pParams->blockWidth; x += widthMul) {
        pSrc = pParams->pSrc + 2*x;
        pDst = pParams->pDst + 2*x;

        // load row 0
        if (widthMul == 4) {
            xmm0 = _mm_loadu_si128( (__m128i *)(pSrc + 0*srcStep) );   // 4 UV pairs [0-3]
        } else if (widthMul == 2) {
            xmm0 = _mm_loadl_epi64( (__m128i *)(pSrc + 0*srcStep) );   // 2 UV pairs [0-1]
        }

        for (y = 0; y < pParams->blockHeight; y++) {
            if (widthMul == 4) {
                xmm1 = _mm_loadu_si128( (__m128i *)(pSrc + 1*srcStep) );
            } else if (widthMul == 2) {
                xmm1 = _mm_loadl_epi64( (__m128i *)(pSrc + 1*srcStep) );
            }

            xmm3 = _mm_unpacklo_epi16(xmm0, xmm1);   // U0U1 V0V1 U1U2 V1V2
            xmm4 = _mm_unpackhi_epi16(xmm0, xmm1);   // U2U3 V2V3 U3U4 V3V4

            xmm3 = _mm_madd_epi16(xmm3, *(__m128i *)(filtTabChroma[pParams->vFraction]));
            xmm4 = _mm_madd_epi16(xmm4, *(__m128i *)(filtTabChroma[pParams->vFraction]));
            
            xmm3 = _mm_add_epi32(xmm3, _mm_set1_epi32(4));
            xmm4 = _mm_add_epi32(xmm4, _mm_set1_epi32(4));
            
            xmm3 = _mm_srli_epi32(xmm3, 3);
            xmm4 = _mm_srli_epi32(xmm4, 3);
            xmm3 = _mm_packus_epi32(xmm3, xmm4);

            // store output and save next row
            if (widthMul == 4) {
                _mm_storeu_si128((__m128i *)pDst, xmm3);
            } else if (widthMul == 2) {
                _mm_storel_epi64((__m128i *)pDst, xmm3);
            }
            xmm0 = xmm1;

            pSrc += srcStep;
            pDst += dstStep;
        }
    }
}

void h264_interpolate_chroma_type_nv12_y0_16u_sse(H264InterpolationParams_16u *pParams)
{
    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    if ((pParams->blockWidth & 0x03) == 0)          InterpChroma16u_y0_kernel_sse<4>(pParams);
    else if ((pParams->blockWidth & 0x01) == 0)     InterpChroma16u_y0_kernel_sse<2>(pParams);
}

template <Ipp32s widthMul>
static void InterpChroma16u_yx_kernel_sse(H264InterpolationParams_16u *pParams)
{
    Ipp32s x, y;
    const Ipp16u *pSrc  = pParams->pSrc;
    Ipp32u srcStep      = (Ipp32u)pParams->srcStep;
    Ipp16u *pDst        = pParams->pDst;
    Ipp32u dstStep      = (Ipp32u)pParams->dstStep;

    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;

    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    for (x = 0; x < pParams->blockWidth; x += widthMul) {
        pSrc = pParams->pSrc + 2*x;
        pDst = pParams->pDst + 2*x;

        // load row 0
        if (widthMul == 4) {
            xmm0 = _mm_loadu_si128( (__m128i *)(pSrc + 2*0 + 0*srcStep) );   // 4 UV pairs [0-3]
            xmm1 = _mm_loadu_si128( (__m128i *)(pSrc + 2*1 + 0*srcStep) );   // 4 UV pairs [1-4]
        } else if (widthMul == 2) {
            xmm0 = _mm_loadl_epi64( (__m128i *)(pSrc + 2*0 + 0*srcStep) );   // 2 UV pairs [0-1]
            xmm1 = _mm_loadl_epi64( (__m128i *)(pSrc + 2*1 + 0*srcStep) );   // 2 UV pairs [1-2]
        }

        for (y = 0; y < pParams->blockHeight; y++) {
            // load next row
            if (widthMul == 4) {
                xmm2 = _mm_loadu_si128( (__m128i *)(pSrc + 2*0 + 1*srcStep) );
                xmm3 = _mm_loadu_si128( (__m128i *)(pSrc + 2*1 + 1*srcStep) );
            } else if (widthMul == 2) {
                xmm2 = _mm_loadl_epi64( (__m128i *)(pSrc + 2*0 + 1*srcStep) );
                xmm3 = _mm_loadl_epi64( (__m128i *)(pSrc + 2*1 + 1*srcStep) );
            }

            xmm4 = _mm_unpacklo_epi16(xmm0, xmm1);           // U0U1 V0V1 U1U2 V1V2
            xmm5 = _mm_unpackhi_epi16(xmm0, xmm1);           // U2U3 V2V3 U3U4 V3V4

            xmm0 = _mm_madd_epi16(xmm4, *(__m128i *)(filtTabChroma[pParams->hFraction]));
            xmm1 = _mm_madd_epi16(xmm5, *(__m128i *)(filtTabChroma[pParams->hFraction]));
            xmm0 = _mm_packus_epi32(xmm0, xmm1);             // output = max 13 bits 

            xmm4 = _mm_unpacklo_epi16(xmm2, xmm3);
            xmm5 = _mm_unpackhi_epi16(xmm2, xmm3);

            xmm4 = _mm_madd_epi16(xmm4, *(__m128i *)(filtTabChroma[pParams->hFraction]));
            xmm5 = _mm_madd_epi16(xmm5, *(__m128i *)(filtTabChroma[pParams->hFraction]));
            xmm4 = _mm_packus_epi32(xmm4, xmm5);

            xmm1 = _mm_unpackhi_epi16(xmm0, xmm4);
            xmm0 = _mm_unpacklo_epi16(xmm0, xmm4);

            xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)(filtTabChroma[pParams->vFraction]));
            xmm1 = _mm_madd_epi16(xmm1, *(__m128i *)(filtTabChroma[pParams->vFraction]));

            // round
            xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(32));
            xmm1 = _mm_add_epi32(xmm1, _mm_set1_epi32(32));

            // scale (16 bits back to 10 bits)
            xmm0 = _mm_srli_epi32(xmm0, 6);
            xmm1 = _mm_srli_epi32(xmm1, 6);
            xmm0 = _mm_packus_epi32(xmm0, xmm1);

            // store output and save next row
            if (widthMul == 4) {
                _mm_storeu_si128((__m128i *)pDst, xmm0);
            } else if (widthMul == 2) {
                _mm_storel_epi64((__m128i *)pDst, xmm0);
            }
            xmm0 = xmm2;
            xmm1 = xmm3;

            pSrc += srcStep;
            pDst += dstStep;
        }
    }
}

void h264_interpolate_chroma_type_nv12_yx_16u_sse(H264InterpolationParams_16u *pParams)
{
    // assume width = x2, height = x2
    assert((pParams->blockWidth &  0x01) == 0);
    assert((pParams->blockHeight & 0x01) == 0);

    if ((pParams->blockWidth & 0x03) == 0)          InterpChroma16u_yx_kernel_sse<4>(pParams);
    else if ((pParams->blockWidth & 0x01) == 0)     InterpChroma16u_yx_kernel_sse<2>(pParams);
}

static pH264Interpolation_16u h264_interpolate_chroma_type_table_16u_pxmx_sse[4] =
{
    &h264_interpolate_chroma_type_nv12_00_16u_sse,
    &h264_interpolate_chroma_type_nv12_0x_16u_sse,
    &h264_interpolate_chroma_type_nv12_y0_16u_sse,
    &h264_interpolate_chroma_type_nv12_yx_16u_sse
};

void H264_FASTCALL MFX_Dispatcher_sse::InterpolateChromaBlock_16u(const IppVCInterpolateBlock_16u *interpolateInfo)
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

    h264_interpolate_chroma_type_table_16u_pxmx_sse[params.iType](&params);
}

} // namespace MFX_PP

