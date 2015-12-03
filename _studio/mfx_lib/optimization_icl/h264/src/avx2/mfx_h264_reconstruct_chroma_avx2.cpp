
#include "mfxvideo.h"
#include "ippvc.h"
#include <string.h>
#include <algorithm>

#include "mfx_dispatcher.h"

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

namespace MFX_PP
{

    enum CBP
{
    D_CBP_LUMA_DC = 0x00001,
    D_CBP_LUMA_AC = 0x1fffe,

    D_CBP_CHROMA_DC = 0x00001,
    D_CBP_CHROMA_AC = 0x1fffe,
    D_CBP_CHROMA_AC_420 = 0x0001e,
    D_CBP_CHROMA_AC_422 = 0x001fe,
    D_CBP_CHROMA_AC_444 = 0x1fffe,

    D_CBP_1ST_LUMA_AC_BITPOS = 1,
    D_CBP_1ST_CHROMA_DC_BITPOS = 17,
    D_CBP_1ST_CHROMA_AC_BITPOS = 19
    };

#define ABS(a)          (((a) < 0) ? (-(a)) : (a))

#define clipd1(x, limit) ((limit < (x)) ? (limit) : (((-limit) > (x)) ? (-limit) : (x)))
#define clip_us(x, limit) ((limit < (x)) ? (limit) : ((0 > (x)) ? (0) : (x)))


ALIGN_DECL(32) static const unsigned char cMaskU08[32] = {0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff};
ALIGN_DECL(32) static const unsigned char cMaskV08[32] = {0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00};

ALIGN_DECL(32) static const unsigned short cMaskU16[16] = {0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff};
ALIGN_DECL(32) static const unsigned short cMaskV16[16] = {0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000};

template<typename Plane>
static inline Plane ClampVal_16U(mfxI32 val, mfxU32 bitDepth)
{
    if (sizeof(Plane) == 1)
        bitDepth = 8;

    Plane maxValue = ((1 << bitDepth) - 1);
    return (Plane) ((maxValue < (val)) ? (maxValue) : ((0 > (val)) ? (0) : (val)));
}

template<typename Plane>
static inline Plane ClampTblLookup_16U(mfxI32 val, mfxI32 val1, mfxU32 bitDepth)
{
    if (sizeof(Plane) == 1)
        bitDepth = 8;

    Plane maxValue = ((1 << bitDepth) - 1);
    return ClampVal_16U<Plane>(val + clipd1(val1, maxValue), bitDepth);
}

// Coeffs = 16s or 32s (input)
// Plane = 8u or 16u (output)
// chroma_format_idc = 1 or 2  --  see GetH264ColorFormat(ColorFormat color_format) in umc_h264_defs_dec.h
//    0 = grayscale (no chroma)
//    1 = YUV420/NV12/etc.
//    2 = YUV422/NV16/etc.
//    3 = YUV444 (not supported)

template<typename Plane, typename Coeffs, int chroma_format_idc>
void ippiReconstructChromaInter4x4MB_H264_16s8u_C2R(Coeffs **ppSrcDstCoeff,
                                                                   Plane *pSrcDstUVPlane,
                                                                   Ipp32u srcdstUVStep,
                                                                   Ipp32u cbpU, Ipp32u cbpV,
                                                                   Ipp32u chromaQPU,
                                                                   Ipp32u chromaQPV,
                                                                   const Ipp16s *pQuantTableU,
                                                                   const Ipp16s *pQuantTableV,
                                                                   Ipp16s levelScaleDCU,
                                                                   Ipp16s levelScaleDCV,
                                                                   Ipp32u bypassFlag,
                                                                   Ipp32u bitDepth)
{
    Coeffs ChromaDC[2][8] = {0}; /* for DC coefficients of chroma */
    Plane *pDst, *pTmpDst;
    ALIGN_DECL(32) Coeffs tmpBuf[16] = {0};
    Ipp32u uBlock, i;
    Ipp32u uCBPMask; /* bit var to isolate cbp4x4 for block being decoded */
    Coeffs a[8];
    Ipp32s q,q2=0;
    const Ipp16s *pQuantTable;
    Ipp32s chromaQP;
    Ipp32s qp6;
    Ipp32s q1=0;
    Ipp32s clipVal;

    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;

    __m128i clipPos16, clipNeg16, clipPos32, clipNeg32;

    _mm256_zeroupper();

    /* chroma blocks */
    if (!cbpU && !cbpV)
        return;

    Ipp32u blockSize = ((chroma_format_idc == 2) ? 8 : 4);

    clipVal = (1 << bitDepth) - 1;

    for (Ipp32u ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++)
    {
        Ipp32u cbp;

        if (ChromaPlane == 0){
            q = (chroma_format_idc == 2) ? levelScaleDCU : pQuantTableU[0];
            chromaQP = chromaQPU;
            cbp = cbpU;
        } else {
            q = (chroma_format_idc == 2) ? levelScaleDCV : pQuantTableV[0];
            chromaQP = chromaQPV;
            cbp = cbpV;
        }

        if (chroma_format_idc == 2)
            chromaQP += 3;

        qp6 = chromaQP/6;

        if (cbp & IPPVC_CBP_LUMA_DC)
        {
            ChromaDC[ChromaPlane][0] = (*ppSrcDstCoeff)[0];
            ChromaDC[ChromaPlane][1] = (*ppSrcDstCoeff)[1];
            ChromaDC[ChromaPlane][2] = (*ppSrcDstCoeff)[2];
            ChromaDC[ChromaPlane][3] = (*ppSrcDstCoeff)[3];

            if (chroma_format_idc == 2)
            {
                ChromaDC[ChromaPlane][4] = (*ppSrcDstCoeff)[4];
                ChromaDC[ChromaPlane][5] = (*ppSrcDstCoeff)[5];
                ChromaDC[ChromaPlane][6] = (*ppSrcDstCoeff)[6];
                ChromaDC[ChromaPlane][7] = (*ppSrcDstCoeff)[7];
            }

            if (!bypassFlag || chromaQP > 0)
            {
                /* bad arguments */
                if (chroma_format_idc == 2)
                {
                    a[0] = (Coeffs) (ChromaDC[ChromaPlane][0] + ChromaDC[ChromaPlane][2] + ChromaDC[ChromaPlane][4] + ChromaDC[ChromaPlane][6]);
                    a[1] = (Coeffs) (ChromaDC[ChromaPlane][0] + ChromaDC[ChromaPlane][2] - ChromaDC[ChromaPlane][4] - ChromaDC[ChromaPlane][6]);
                    a[2] = (Coeffs) (ChromaDC[ChromaPlane][0] - ChromaDC[ChromaPlane][2] - ChromaDC[ChromaPlane][4] + ChromaDC[ChromaPlane][6]);
                    a[3] = (Coeffs) (ChromaDC[ChromaPlane][0] - ChromaDC[ChromaPlane][2] + ChromaDC[ChromaPlane][4] - ChromaDC[ChromaPlane][6]);

                    a[4] = (Coeffs) (ChromaDC[ChromaPlane][1] + ChromaDC[ChromaPlane][3] + ChromaDC[ChromaPlane][5] + ChromaDC[ChromaPlane][7]);
                    a[5] = (Coeffs) (ChromaDC[ChromaPlane][1] + ChromaDC[ChromaPlane][3] - ChromaDC[ChromaPlane][5] - ChromaDC[ChromaPlane][7]);
                    a[6] = (Coeffs) (ChromaDC[ChromaPlane][1] - ChromaDC[ChromaPlane][3] - ChromaDC[ChromaPlane][5] + ChromaDC[ChromaPlane][7]);
                    a[7] = (Coeffs) (ChromaDC[ChromaPlane][1] - ChromaDC[ChromaPlane][3] + ChromaDC[ChromaPlane][5] - ChromaDC[ChromaPlane][7]);

                    ChromaDC[ChromaPlane][0] = (Coeffs) (a[0] + a[4]);
                    ChromaDC[ChromaPlane][2] = (Coeffs) (a[1] + a[5]);
                    ChromaDC[ChromaPlane][4] = (Coeffs) (a[2] + a[6]);
                    ChromaDC[ChromaPlane][6] = (Coeffs) (a[3] + a[7]);

                    ChromaDC[ChromaPlane][1] = (Coeffs) (a[0] - a[4]);
                    ChromaDC[ChromaPlane][3] = (Coeffs) (a[1] - a[5]);
                    ChromaDC[ChromaPlane][5] = (Coeffs) (a[2] - a[6]);
                    ChromaDC[ChromaPlane][7] = (Coeffs) (a[3] - a[7]);
                }
                else
                {
                    a[0] = ChromaDC[ChromaPlane][0];
                    a[1] = ChromaDC[ChromaPlane][1];
                    a[2] = ChromaDC[ChromaPlane][2];
                    a[3] = ChromaDC[ChromaPlane][3];

                    ChromaDC[ChromaPlane][0] = (Coeffs) ((a[0] + a[2]) + (a[1] + a[3]));
                    ChromaDC[ChromaPlane][1] = (Coeffs) ((a[0] + a[2]) - (a[1] + a[3]));
                    ChromaDC[ChromaPlane][2] = (Coeffs) ((a[0] - a[2]) + (a[1] - a[3]));
                    ChromaDC[ChromaPlane][3] = (Coeffs) ((a[0] - a[2]) - (a[1] - a[3]));
                }

                /* Dequantisation */
                if (chroma_format_idc == 2)
                {
                    if (chromaQP < 36)
                    {
                        q1 = 1 << (5 - qp6);
                        q2 = 6 - qp6;
                        ChromaDC[ChromaPlane][0] = (Coeffs) (((ChromaDC[ChromaPlane][0] * q) + q1) >> q2);
                        ChromaDC[ChromaPlane][1] = (Coeffs) (((ChromaDC[ChromaPlane][1] * q) + q1) >> q2);
                        ChromaDC[ChromaPlane][2] = (Coeffs) (((ChromaDC[ChromaPlane][2] * q) + q1) >> q2);
                        ChromaDC[ChromaPlane][3] = (Coeffs) (((ChromaDC[ChromaPlane][3] * q) + q1) >> q2);
                        ChromaDC[ChromaPlane][4] = (Coeffs) (((ChromaDC[ChromaPlane][4] * q) + q1) >> q2);
                        ChromaDC[ChromaPlane][5] = (Coeffs) (((ChromaDC[ChromaPlane][5] * q) + q1) >> q2);
                        ChromaDC[ChromaPlane][6] = (Coeffs) (((ChromaDC[ChromaPlane][6] * q) + q1) >> q2);
                        ChromaDC[ChromaPlane][7] = (Coeffs) (((ChromaDC[ChromaPlane][7] * q) + q1) >> q2);
                    }
                    else
                    {
                        q2 = qp6 - 6;
                        ChromaDC[ChromaPlane][0] = (Coeffs) ((ChromaDC[ChromaPlane][0] * q) << q2);
                        ChromaDC[ChromaPlane][1] = (Coeffs) ((ChromaDC[ChromaPlane][1] * q) << q2);
                        ChromaDC[ChromaPlane][2] = (Coeffs) ((ChromaDC[ChromaPlane][2] * q) << q2);
                        ChromaDC[ChromaPlane][3] = (Coeffs) ((ChromaDC[ChromaPlane][3] * q) << q2);
                        ChromaDC[ChromaPlane][4] = (Coeffs) ((ChromaDC[ChromaPlane][4] * q) << q2);
                        ChromaDC[ChromaPlane][5] = (Coeffs) ((ChromaDC[ChromaPlane][5] * q) << q2);
                        ChromaDC[ChromaPlane][6] = (Coeffs) ((ChromaDC[ChromaPlane][6] * q) << q2);
                        ChromaDC[ChromaPlane][7] = (Coeffs) ((ChromaDC[ChromaPlane][7] * q) << q2);
                    }
                }
                else
                {
                    ChromaDC[ChromaPlane][0] = (Coeffs) (((ChromaDC[ChromaPlane][0] * q) << qp6) >> 5);
                    ChromaDC[ChromaPlane][1] = (Coeffs) (((ChromaDC[ChromaPlane][1] * q) << qp6) >> 5);
                    ChromaDC[ChromaPlane][2] = (Coeffs) (((ChromaDC[ChromaPlane][2] * q) << qp6) >> 5);
                    ChromaDC[ChromaPlane][3] = (Coeffs) (((ChromaDC[ChromaPlane][3] * q) << qp6) >> 5);
                }
            }

            *ppSrcDstCoeff += blockSize;
        }
    }
    pDst = pSrcDstUVPlane;

    for (Ipp32u ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++)
    {
        Ipp32u cbp;
        if(ChromaPlane == 0){
            pQuantTable = pQuantTableU;
            chromaQP = chromaQPU;
            cbp = cbpU;
        } else {
            pQuantTable = pQuantTableV;
            chromaQP = chromaQPV;
            cbp = cbpV;
        }
        qp6 = chromaQP/6;
        uCBPMask = (1 << IPPVC_CBP_1ST_LUMA_AC_BITPOS);

        clipPos16 = _mm_set1_epi16(+clipVal);
        clipNeg16 = _mm_set1_epi16(-clipVal);
        clipPos32 = _mm_set1_epi32(+clipVal);
        clipNeg32 = _mm_set1_epi32(-clipVal);

        for (uBlock = 0; uBlock < blockSize; uBlock++, uCBPMask <<= 1)
        {
            pTmpDst = pDst;
            if ((cbp & uCBPMask) != 0)
            {
                for (i = 0; i < 16; i++)
                    tmpBuf[i] = (*ppSrcDstCoeff)[i];

                *ppSrcDstCoeff += 16;

                /* Chroma blocks always have separatly decoded DC coeffs */
                /* does not need to scale 0 coeff */
                /* if (!ChromaDC[ChromaPlane][uBlock]) */
                /*    tmpbuf[0] * = InvpQuantTable_w7[QP][2]; */
                /* else */

                /* dequantization */

                if (!bypassFlag || chromaQP>0)
                {
                    // setup common vectors
                    q1 = 1 << (3 - qp6);
                    ymm2 = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i *)(&pQuantTable[0])));
                    ymm3 = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i *)(&pQuantTable[8])));
                    ymm4 = _mm256_set1_epi32(q1);

                    if(chromaQP < 24) {

                        if (sizeof(Coeffs) == 4) {
                            ymm0 = _mm256_loadu_si256((__m256i *)(&tmpBuf[0]));  // load 32-bit values 0-7
                            ymm1 = _mm256_loadu_si256((__m256i *)(&tmpBuf[8]));  // load 32-bit values 8-15
                        } else {
                            ymm0 = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i *)(&tmpBuf[0])));  // load 16-bit values 0-7, expand to 32 bits
                            ymm1 = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i *)(&tmpBuf[8])));  // load 16-bit values 8-15, expand to 32 bits
                        }

                        ymm0 = _mm256_mullo_epi32(ymm0, ymm2); 
                        ymm1 = _mm256_mullo_epi32(ymm1, ymm3); 

                        xmm0 = _mm_cvtsi32_si128(4 - qp6);
                        ymm0 = _mm256_add_epi16(ymm0, ymm4);
                        ymm1 = _mm256_add_epi16(ymm1, ymm4);

                        ymm0 = _mm256_sra_epi32(ymm0, xmm0);
                        ymm1 = _mm256_sra_epi32(ymm1, xmm0);

                        if (sizeof(Coeffs) == 4) {
                            _mm256_storeu_si256((__m256i *)(&tmpBuf[0]), ymm0);
                            _mm256_storeu_si256((__m256i *)(&tmpBuf[8]), ymm1);
                        } else {
                            ymm0 = _mm256_packs_epi32(ymm0, ymm1);  // convert back to 16-bit
                            ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
                            _mm256_storeu_si256((__m256i *)(&tmpBuf[0]), ymm0);
                        }
                    } else {
                        if (sizeof(Coeffs) == 4) {
                            ymm0 = _mm256_loadu_si256((__m256i *)(&tmpBuf[0]));  // load 32-bit values 0-7
                            ymm1 = _mm256_loadu_si256((__m256i *)(&tmpBuf[8]));  // load 32-bit values 8-15
                        } else {
                            ymm0 = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i *)(&tmpBuf[0])));  // load 16-bit values 0-7, expand to 32 bits
                            ymm1 = _mm256_cvtepi16_epi32(_mm_loadu_si128((__m128i *)(&tmpBuf[8])));  // load 16-bit values 8-15, expand to 32 bits
                        }

                        ymm0 = _mm256_mullo_epi32(ymm0, ymm2); 
                        ymm1 = _mm256_mullo_epi32(ymm1, ymm3); 

                        xmm0 = _mm_cvtsi32_si128(qp6 - 4);
                        ymm0 = _mm256_sll_epi32(ymm0, xmm0);
                        ymm1 = _mm256_sll_epi32(ymm1, xmm0);

                        if (sizeof(Coeffs) == 4) {
                            _mm256_storeu_si256((__m256i *)(&tmpBuf[0]), ymm0);
                            _mm256_storeu_si256((__m256i *)(&tmpBuf[8]), ymm1);
                        } else {
                            ymm0 = _mm256_packs_epi32(ymm0, ymm1);  // convert back to 16-bit (note - assumes results fit in 16 bits - careful with fake test inputs)
                            ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
                            _mm256_storeu_si256((__m256i *)(&tmpBuf[0]), ymm0);
                        }
                    }

                    tmpBuf[0] = ChromaDC[ChromaPlane][uBlock];

                    if (sizeof(Coeffs) == 4) {
                        xmm0 = _mm_loadu_si128((__m128i *)&tmpBuf[0*4]);
                        xmm1 = _mm_loadu_si128((__m128i *)&tmpBuf[1*4]);
                        xmm2 = _mm_loadu_si128((__m128i *)&tmpBuf[2*4]);
                        xmm3 = _mm_loadu_si128((__m128i *)&tmpBuf[3*4]);
                    } else {
                        xmm0 = _mm_cvtepi16_epi32(_mm_loadl_epi64((__m128i *)&tmpBuf[0*4]));
                        xmm1 = _mm_cvtepi16_epi32(_mm_loadl_epi64((__m128i *)&tmpBuf[1*4]));
                        xmm2 = _mm_cvtepi16_epi32(_mm_loadl_epi64((__m128i *)&tmpBuf[2*4]));
                        xmm3 = _mm_cvtepi16_epi32(_mm_loadl_epi64((__m128i *)&tmpBuf[3*4]));
                    }

                    // transpose
                    xmm4 = _mm_unpackhi_epi32(xmm0, xmm1);
                    xmm5 = _mm_unpacklo_epi32(xmm0, xmm1);
                    xmm0 = _mm_unpackhi_epi32(xmm2, xmm3);
                    xmm1 = _mm_unpacklo_epi32(xmm2, xmm3);

                    xmm2 = _mm_unpacklo_epi64(xmm4, xmm0);
                    xmm3 = _mm_unpackhi_epi64(xmm4, xmm0);
                    xmm0 = _mm_unpacklo_epi64(xmm5, xmm1);
                    xmm1 = _mm_unpackhi_epi64(xmm5, xmm1);

                    // filt horiz (no scale)
                    xmm4 = _mm_sub_epi32(xmm0, xmm2);   // xmm4 = (t0 - t2)
                    xmm0 = _mm_add_epi32(xmm0, xmm2);   // xmm0 = (t0 + t2)
                    xmm5 = _mm_srai_epi32(xmm1, 1);     //  t1/2
                    xmm5 = _mm_sub_epi32(xmm5, xmm3);   // xmm5 = (t1/2 - t3)
                    xmm3 = _mm_srai_epi32(xmm3, 1);     //  t3/2
                    xmm2 = _mm_add_epi32(xmm1, xmm3);   // xmm2 = (t1 + t3/2)
                        
                    xmm1 = _mm_add_epi32(xmm4, xmm5);   // row 1
                    xmm3 = _mm_sub_epi32(xmm0, xmm2);   // row 3
                    xmm0 = _mm_add_epi32(xmm0, xmm2);   // row 0
                    xmm2 = _mm_sub_epi32(xmm4, xmm5);   // row 2

                    // transpose
                    xmm4 = _mm_unpackhi_epi32(xmm0, xmm1);
                    xmm5 = _mm_unpacklo_epi32(xmm0, xmm1);
                    xmm0 = _mm_unpackhi_epi32(xmm2, xmm3);
                    xmm1 = _mm_unpacklo_epi32(xmm2, xmm3);

                    xmm2 = _mm_unpacklo_epi64(xmm4, xmm0);
                    xmm3 = _mm_unpackhi_epi64(xmm4, xmm0);
                    xmm0 = _mm_unpacklo_epi64(xmm5, xmm1);
                    xmm1 = _mm_unpackhi_epi64(xmm5, xmm1);

                    // filt - vert
                    xmm4 = _mm_sub_epi32(xmm0, xmm2);   // xmm4 = (t0 - t2)
                    xmm0 = _mm_add_epi32(xmm0, xmm2);   // xmm0 = (t0 + t2)
                    xmm5 = _mm_srai_epi32(xmm1, 1);     //  t1/2
                    xmm5 = _mm_sub_epi32(xmm5, xmm3);   // xmm5 = (t1/2 - t3)
                    xmm3 = _mm_srai_epi32(xmm3, 1);     //  t3/2
                    xmm2 = _mm_add_epi32(xmm1, xmm3);   // xmm2 = (t1 + t3/2)

                    xmm1 = _mm_add_epi32(xmm4, xmm5);   // row 1
                    xmm3 = _mm_sub_epi32(xmm0, xmm2);   // row 3
                    xmm0 = _mm_add_epi32(xmm0, xmm2);   // row 0
                    xmm2 = _mm_sub_epi32(xmm4, xmm5);   // row 2

                    // round and scale
                    xmm1 = _mm_add_epi32(xmm1, _mm_set1_epi32(32));
                    xmm1 = _mm_srai_epi32(xmm1, 6);
                    xmm3 = _mm_add_epi32(xmm3, _mm_set1_epi32(32));
                    xmm3 = _mm_srai_epi32(xmm3, 6);
                    xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(32));
                    xmm0 = _mm_srai_epi32(xmm0, 6);
                    xmm2 = _mm_add_epi32(xmm2, _mm_set1_epi32(32));
                    xmm2 = _mm_srai_epi32(xmm2, 6);

                    if (sizeof(Coeffs) == 4) {
                        _mm_storeu_si128((__m128i *)&tmpBuf[0*4], xmm0);
                        _mm_storeu_si128((__m128i *)&tmpBuf[1*4], xmm1);
                        _mm_storeu_si128((__m128i *)&tmpBuf[2*4], xmm2);
                        _mm_storeu_si128((__m128i *)&tmpBuf[3*4], xmm3);
                    } else {
                        xmm0 = _mm_packs_epi32(xmm0, xmm1);
                        xmm2 = _mm_packs_epi32(xmm2, xmm3);
                        _mm_storeu_si128((__m128i *)&tmpBuf[0*4], xmm0);
                        _mm_storeu_si128((__m128i *)&tmpBuf[2*4], xmm2);
                    }
                } else {
                    tmpBuf[0] = ChromaDC[ChromaPlane][uBlock];
                }

                /* add to prediction */
                /* Note that JM39a scales and rounds after adding scaled */
                /* prediction, producing the same results. */
                if (sizeof(Plane) == 2) {
                    if (ChromaPlane == 0) {
                        for (i = 0; i < 4; i++) {
                            xmm2 = _mm_loadu_si128((__m128i *)&pTmpDst[0]);     // load 8 16-bit vals
                            xmm0 = xmm2;
                            xmm1 = _mm_loadu_si128((__m128i *)&tmpBuf[i*4]);    // load 4 32-bit vals

                            // clip tmpBuf to [-1023, 1023]
                            xmm1 = _mm_min_epi32(xmm1, clipPos32);
                            xmm1 = _mm_max_epi32(xmm1, clipNeg32);

                            // convert 16-bit [UVUVUVUV] into 32-bit [U U U U]  (little endian)
                            xmm0 = _mm_slli_epi32(xmm0, 16);
                            xmm0 = _mm_srli_epi32(xmm0, 16);    

                            // add to pTmpDst, then clip result to [0, 1023]
                            xmm0 = _mm_add_epi32(xmm0, xmm1);
                            xmm0 = _mm_min_epi32(xmm0, clipPos32);  // clip to [0, 2^bitDepth - 1]
                            xmm0 = _mm_max_epi32(xmm0, _mm_setzero_si128());

                            xmm2 = _mm_and_si128(xmm2, *(__m128i *)cMaskU16);
                            xmm2 = _mm_or_si128(xmm2, xmm0);

                            _mm_storeu_si128((__m128i *)&pTmpDst[0], xmm2);
                            pTmpDst += srcdstUVStep;
                        }
                    } else {
                        for (i = 0; i < 4; i++) {
                            xmm2 = _mm_loadu_si128((__m128i *)&pTmpDst[0]);     // load 8 16-bit vals
                            xmm0 = xmm2;
                            xmm1 = _mm_loadu_si128((__m128i *)&tmpBuf[i*4]);    // load 4 32-bit vals

                            // clip tmpBuf to [-1023, 1023]
                            xmm1 = _mm_min_epi32(xmm1, clipPos32);
                            xmm1 = _mm_max_epi32(xmm1, clipNeg32);

                            // convert 16-bit [UVUVUVUV] into 32-bit [V V V V]  (little endian)
                            xmm0 = _mm_srli_epi32(xmm0, 16);    

                            // add to pTmpDst, then clip result to [0, 1023]
                            xmm0 = _mm_add_epi32(xmm0, xmm1);
                            xmm0 = _mm_min_epi32(xmm0, clipPos32);  // clip to [0, 2^bitDepth - 1]
                            xmm0 = _mm_max_epi32(xmm0, _mm_setzero_si128());

                            xmm0 = _mm_slli_epi32(xmm0, 16);
                            xmm2 = _mm_and_si128(xmm2, *(__m128i *)cMaskV16);
                            xmm2 = _mm_or_si128(xmm2, xmm0);

                            _mm_storeu_si128((__m128i *)&pTmpDst[0], xmm2);
                            pTmpDst += srcdstUVStep;
                        }
                    }
                } else {
                    if (ChromaPlane == 0) {
                        for (i = 0; i < 4; i++) {
                            xmm2 = _mm_loadl_epi64((__m128i *)&pTmpDst[0]);     // load 8 8-bit vals
                            xmm0 = xmm2;
                            xmm1 = _mm_loadl_epi64((__m128i *)&tmpBuf[i*4]);    // load 4 16-bit vals

                            // clip tmpBuf to [-255, 255]
                            xmm1 = _mm_min_epi16(xmm1, clipPos16);
                            xmm1 = _mm_max_epi16(xmm1, clipNeg16);

                            // convert 8-bit [UVUVUVUV] into 16-bit [U U U U]  (little endian)
                            xmm0 = _mm_slli_epi16(xmm0, 8);
                            xmm0 = _mm_srli_epi16(xmm0, 8);

                            // add to pTmpDst, then clip result to [0, 255]
                            xmm0 = _mm_add_epi16(xmm0, xmm1);
                            xmm0 = _mm_min_epi16(xmm0, clipPos16);
                            xmm0 = _mm_max_epi16(xmm0, _mm_setzero_si128());

                            xmm2 = _mm_and_si128(xmm2, *(__m128i *)cMaskU08);
                            xmm2 = _mm_or_si128(xmm2, xmm0);

                            _mm_storel_epi64((__m128i *)&pTmpDst[0], xmm2);
                            pTmpDst += srcdstUVStep;
                        }
                    } else {
                        for (i = 0; i < 4; i++) {
                            xmm2 = _mm_loadl_epi64((__m128i *)&pTmpDst[0]);     // load 8 8-bit vals
                            xmm0 = xmm2;
                            xmm1 = _mm_loadl_epi64((__m128i *)&tmpBuf[i*4]);    // load 4 16-bit vals

                            // clip tmpBuf to [-255, 255]
                            xmm1 = _mm_min_epi16(xmm1, clipPos16);
                            xmm1 = _mm_max_epi16(xmm1, clipNeg16);

                            // convert 8-bit [UVUVUVUV] into 16-bit [V V V V]  (little endian)
                            xmm0 = _mm_srli_epi16(xmm0, 8);

                            // add to pTmpDst, then clip result to [0, 255]
                            xmm0 = _mm_add_epi16(xmm0, xmm1);
                            xmm0 = _mm_min_epi16(xmm0, clipPos16);
                            xmm0 = _mm_max_epi16(xmm0, _mm_setzero_si128());

                            xmm0 = _mm_slli_epi16(xmm0, 8);
                            xmm2 = _mm_and_si128(xmm2, *(__m128i *)cMaskV08);
                            xmm2 = _mm_or_si128(xmm2, xmm0);

                            _mm_storel_epi64((__m128i *)&pTmpDst[0], xmm2);
                            pTmpDst += srcdstUVStep;
                        }
                    }
                }
            }
            else if (ChromaDC[ChromaPlane][uBlock])
            {
                /* only a DC coefficient */
                if (!bypassFlag || chromaQP>0)
                {
                    Coeffs v = (Coeffs) ((ChromaDC[ChromaPlane][uBlock] + 32) >> 6);

                    if (sizeof(Plane) == 2) {
                        // do all 4 rows at once
                        xmm0 = _mm_loadu_si128((__m128i *)&pTmpDst[0*srcdstUVStep]);  // load 8 16-bit vals per row
                        xmm1 = _mm_loadu_si128((__m128i *)&pTmpDst[1*srcdstUVStep]);
                        xmm2 = _mm_loadu_si128((__m128i *)&pTmpDst[2*srcdstUVStep]);
                        xmm3 = _mm_loadu_si128((__m128i *)&pTmpDst[3*srcdstUVStep]);
                    } else {
                        xmm0 = _mm_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pTmpDst[0*srcdstUVStep]));  // load 8 8-bit vals per row, expand to 16
                        xmm1 = _mm_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pTmpDst[1*srcdstUVStep]));
                        xmm2 = _mm_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pTmpDst[2*srcdstUVStep]));
                        xmm3 = _mm_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pTmpDst[3*srcdstUVStep]));
                    }

                    ymm0 = _mm256_permute2x128_si256(mm256(xmm0), mm256(xmm1), 0x20);
                    ymm1 = _mm256_permute2x128_si256(mm256(xmm2), mm256(xmm3), 0x20);
                    ymm2 = ymm0;
                    ymm3 = ymm1;

                    // convert 16-bit [UVUVUVUV] into 32-bit [U U U U] or [V V V V]  (little endian)
                    if (ChromaPlane == 0) {
                        ymm0 = _mm256_slli_epi32(ymm0, 16);
                        ymm1 = _mm256_slli_epi32(ymm1, 16);
                    }
                    ymm0 = _mm256_srai_epi32(ymm0, 16);
                    ymm1 = _mm256_srai_epi32(ymm1, 16);

                    ymm4 = _mm256_set1_epi32(v);
                    ymm5 = _mm256_set1_epi32(clipVal);

                    ymm0 = _mm256_add_epi32(ymm0, ymm4);
                    ymm0 = _mm256_min_epi32(ymm0, ymm5);  // clip to [0, 2^bitDepth - 1]
                    ymm0 = _mm256_max_epi32(ymm0, _mm256_setzero_si256());

                    ymm1 = _mm256_add_epi32(ymm1, ymm4);
                    ymm1 = _mm256_min_epi32(ymm1, ymm5);  // clip to [0, 2^bitDepth - 1]
                    ymm1 = _mm256_max_epi32(ymm1, _mm256_setzero_si256());

                    if (ChromaPlane == 0) {
                        ymm2 = _mm256_and_si256(ymm2, *(__m256i *)cMaskU16);
                        ymm2 = _mm256_or_si256(ymm2, ymm0);
                        ymm3 = _mm256_and_si256(ymm3, *(__m256i *)cMaskU16);
                        ymm3 = _mm256_or_si256(ymm3, ymm1);
                    } else {
                        ymm0 = _mm256_slli_epi32(ymm0, 16);
                        ymm2 = _mm256_and_si256(ymm2, *(__m256i *)cMaskV16);
                        ymm2 = _mm256_or_si256(ymm2, ymm0);
                        ymm1 = _mm256_slli_epi32(ymm1, 16);
                        ymm3 = _mm256_and_si256(ymm3, *(__m256i *)cMaskV16);
                        ymm3 = _mm256_or_si256(ymm3, ymm1);
                    }

                    if (sizeof(Plane) == 2) {
                        _mm_storeu_si128((__m128i *)&pTmpDst[0*srcdstUVStep], mm128(ymm2));
                        ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01);
                        _mm_storeu_si128((__m128i *)&pTmpDst[1*srcdstUVStep], mm128(ymm2));
                        _mm_storeu_si128((__m128i *)&pTmpDst[2*srcdstUVStep], mm128(ymm3));
                        ymm3 = _mm256_permute2x128_si256(ymm3, ymm3, 0x01);
                        _mm_storeu_si128((__m128i *)&pTmpDst[3*srcdstUVStep], mm128(ymm3));
                    } else {
                        ymm2 = _mm256_packus_epi16(ymm2, ymm3);             // row [0 | 2 | 1 | 3]
                        ymm3 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01); // row [1 | 3 | 1 | 3]
                        _mm_storel_epi64((__m128i *)&pTmpDst[0*srcdstUVStep], mm128(ymm2));
                        _mm_storel_epi64((__m128i *)&pTmpDst[1*srcdstUVStep], mm128(ymm3));
                        ymm2 = _mm256_shuffle_epi32(ymm2, 0x0e);
                        ymm3 = _mm256_shuffle_epi32(ymm3, 0x0e);
                        _mm_storel_epi64((__m128i *)&pTmpDst[2*srcdstUVStep], mm128(ymm2));
                        _mm_storel_epi64((__m128i *)&pTmpDst[3*srcdstUVStep], mm128(ymm3));
                    }

                    pTmpDst += 4*srcdstUVStep;
                }
                else
                {
                    pTmpDst[0+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[0+ChromaPlane], ChromaDC[ChromaPlane][uBlock], bitDepth);
                }
            }

            pDst += 8;
            if ((uBlock & 1) == 1)
                pDst += (srcdstUVStep << 2) - 16;

        } /* for uBlock */
        pDst = pSrcDstUVPlane;
    } /* chroma planes */
}

void MFX_Dispatcher_avx2::ReconstructChromaInter4x4(Ipp32s **ppSrcDstCoeff,
                                                Ipp16u *pSrcDstUVPlane,
                                                Ipp32u srcdstUVStep,
                                                Ipp32u cbpU, Ipp32u cbpV,
                                                Ipp32u chromaQPU,
                                                Ipp32u chromaQPV,
                                                const Ipp16s *pQuantTableU,
                                                const Ipp16s *pQuantTableV,
                                                Ipp16s levelScaleDCU,
                                                Ipp16s levelScaleDCV,
                                                Ipp8u  bypass_flag,
                                                Ipp32u bit_depth,
                                                Ipp32u chroma_format)
{
    if (chroma_format == 2)
    {
        ippiReconstructChromaInter4x4MB_H264_16s8u_C2R<mfxU16, mfxI32, 2>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
            cbpU, cbpV, chromaQPU,chromaQPV,pQuantTableU,pQuantTableV,levelScaleDCU, levelScaleDCV, bypass_flag, bit_depth);
    } else {
        ippiReconstructChromaInter4x4MB_H264_16s8u_C2R<mfxU16, mfxI32, 1>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
            cbpU, cbpV, chromaQPU,chromaQPV,pQuantTableU,pQuantTableV,levelScaleDCU, levelScaleDCV, bypass_flag, bit_depth);
    }
}

void MFX_Dispatcher_avx2::ReconstructChromaInter4x4(Ipp16s **ppSrcDstCoeff,
                                                Ipp8u *pSrcDstUVPlane,
                                                Ipp32u srcdstUVStep,
                                                Ipp32u cbpU, Ipp32u cbpV,
                                                Ipp32u chromaQPU,
                                                Ipp32u chromaQPV,
                                                const Ipp16s *pQuantTableU,
                                                const Ipp16s *pQuantTableV,
                                                Ipp16s levelScaleDCU,
                                                Ipp16s levelScaleDCV,
                                                Ipp8u  bypass_flag,
                                                Ipp32u bit_depth,
                                                Ipp32u chroma_format)
{
    if (chroma_format == 2)
    {
        ippiReconstructChromaInter4x4MB_H264_16s8u_C2R<mfxU8, mfxI16, 2>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
            cbpU, cbpV, chromaQPU,chromaQPV,pQuantTableU,pQuantTableV,levelScaleDCU, levelScaleDCV, bypass_flag, bit_depth);
    } else {
        ippiReconstructChromaInter4x4MB_H264_16s8u_C2R<mfxU8, mfxI16, 1>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
            cbpU, cbpV, chromaQPU,chromaQPV,pQuantTableU,pQuantTableV,levelScaleDCU, levelScaleDCV, bypass_flag, bit_depth);
    }
}

} // namespace MFX_PP

