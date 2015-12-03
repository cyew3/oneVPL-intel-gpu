
#include "mfxvideo.h"
#include "ippvc.h"
#include <string.h>
#include <algorithm>

#include "mfx_dispatcher.h"

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

inline Ipp32u CreateIPPCBPMask420(Ipp32u cbpU, Ipp32u cbpV)
{
    Ipp32u cbp4x4 = (((cbpU & D_CBP_CHROMA_DC) | ((cbpV & D_CBP_CHROMA_DC) << 1)) << D_CBP_1ST_CHROMA_DC_BITPOS) |
                     ((cbpU & D_CBP_CHROMA_AC_420) << (D_CBP_1ST_CHROMA_AC_BITPOS - 1)) |
                     ((cbpV & D_CBP_CHROMA_AC_420) << (D_CBP_1ST_CHROMA_AC_BITPOS + 4 - 1));
    return cbp4x4;

} // Ipp32u CreateIPPCBPMask420(Ipp32u nUCBP, Ipp32u nVCBP)

#define ABS(a)          (((a) < 0) ? (-(a)) : (a))
#define clipd1(x, limit) ((limit < (x)) ? (limit) : (((-limit) > (x)) ? (-limit) : (x)))

template<typename Plane>
inline Plane ClampVal_16U(mfxI32 val, mfxU32 bitDepth)
{
    if (sizeof(Plane) == 1)
        bitDepth = 8;

    Plane maxValue = ((1 << bitDepth) - 1);
    return (Plane) ((maxValue < (val)) ? (maxValue) : ((0 > (val)) ? (0) : (val)));
}

template<typename Plane>
inline Plane ClampTblLookup_16U(mfxI32 val, mfxI32 val1, mfxU32 bitDepth)
{
    if (sizeof(Plane) == 1)
        bitDepth = 8;

    Plane maxValue = ((1 << bitDepth) - 1);
    return ClampVal_16U<Plane>(val + clipd1(val1, maxValue), bitDepth);
}

template<typename Plane, typename Coeffs, int chroma_format_idc>
static void ippiReconstructChromaInter4x4MB_H264_16s8u_C2R(Coeffs **ppSrcDstCoeff,
                                                                   Plane *pSrcDstUVPlane,
                                                                   Ipp32u srcdstUVStep_,
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
    Coeffs tmpBuf[16] = {0};
    Ipp32u uBlock, i;
    Ipp32u uCBPMask; /* bit var to isolate cbp4x4 for block being decoded */
    Coeffs a[8];
    Ipp32s q,q2=0;
    const Ipp16s *pQuantTable;
    Ipp32s chromaQP;
    Ipp32s qp6;
    Ipp32s q1=0;

    /* chroma blocks */
    if (!cbpU && !cbpV)
        return;

    int srcdstUVStep = (int)srcdstUVStep_;
    Ipp32u blockSize = ((chroma_format_idc == 2) ? 8 : 4);

#ifdef __ICL
#pragma novector
#endif
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
#ifdef __ICL
#pragma novector
#endif
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
                    if(chromaQP < 24) {
                        q1 = 1 << (3 - qp6);
                        q2 = 4 - qp6;
                        for (i = 1; i < 16; i++){
                            tmpBuf[i] = (Coeffs) ((tmpBuf[i] * pQuantTable[i] + q1) >> q2);
                        }
                    } else {
                        q2 = qp6 - 4;
                        for (i = 1; i < 16; i++){
                            tmpBuf[i] = (Coeffs) ((tmpBuf[i] * pQuantTable[i]) << q2);
                        }
                    }
                }

                tmpBuf[0] = ChromaDC[ChromaPlane][uBlock];

                if (!bypassFlag || chromaQP>0)
                {
                    /* horizontal */
                    for (i = 0; i < 4; i ++)
                    {
                        a[0] = (Coeffs) (tmpBuf[i * 4 + 0] + tmpBuf[i * 4 + 2]);         /* A' = A + C */
                        a[1] = (Coeffs) (tmpBuf[i * 4 + 0] - tmpBuf[i * 4 + 2]);         /* B' = A - C */
                        a[2] = (Coeffs) ((tmpBuf[i * 4 + 1] >> 1) - tmpBuf[i * 4 + 3]);  /* C' = (B>>1) - D */
                        a[3] = (Coeffs) (tmpBuf[i * 4 + 1] + (tmpBuf[i * 4 + 3] >> 1));  /* D' = B + (D>>1) */

                        tmpBuf[i * 4 + 0] = (Coeffs) (a[0] + a[3]);    /* A = A' + D' */
                        tmpBuf[i * 4 + 1] = (Coeffs) (a[1] + a[2]);    /* B = B' + C' */
                        tmpBuf[i * 4 + 2] = (Coeffs) (a[1] - a[2]);    /* C = B' - C' */
                        tmpBuf[i * 4 + 3] = (Coeffs) (a[0] - a[3]);    /* A = A' - D' */
                    }

                    /* vertical */
                    for (i = 0; i < 4; i ++)
                    {
                        a[0] = (Coeffs) (tmpBuf[0 * 4 + i] + tmpBuf[2 * 4 + i]);        /* A' = A + C */
                        a[1] = (Coeffs) (tmpBuf[0 * 4 + i] - tmpBuf[2 * 4 + i]);        /* B' = A - C */
                        a[2] = (Coeffs) ((tmpBuf[1 * 4 + i] >> 1) - tmpBuf[3 * 4 + i]); /* C' = (B>>1) - D */
                        a[3] = (Coeffs) (tmpBuf[1 * 4 + i] + (tmpBuf[3 * 4 + i] >> 1)); /* D' = B + (D>>1) */

                        tmpBuf[0 * 4 + i] = (Coeffs) ((a[0] + a[3] + 32) >> 6);  /* A = (A' + D' + 32) >> 6 */
                        tmpBuf[1 * 4 + i] = (Coeffs) ((a[1] + a[2] + 32) >> 6);  /* B = (B' + C' + 32) >> 6 */
                        tmpBuf[2 * 4 + i] = (Coeffs) ((a[1] - a[2] + 32) >> 6);  /* C = (B' - C' + 32) >> 6 */
                        tmpBuf[3 * 4 + i] = (Coeffs) ((a[0] - a[3] + 32) >> 6);  /* D = (A' - D' + 32) >> 6 */
                    }
                }

                /* add to prediction */
                /* Note that JM39a scales and rounds after adding scaled */
                /* prediction, producing the same results. */
                for (i = 0; i < 4; i++)
                {
                    pTmpDst[0+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[0+ChromaPlane], tmpBuf[i * 4 + 0], bitDepth);
                    pTmpDst[2+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[2+ChromaPlane], tmpBuf[i * 4 + 1], bitDepth);
                    pTmpDst[4+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[4+ChromaPlane], tmpBuf[i * 4 + 2], bitDepth);
                    pTmpDst[6+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[6+ChromaPlane], tmpBuf[i * 4 + 3], bitDepth);
                    pTmpDst += srcdstUVStep;
                }
            }
            else if (ChromaDC[ChromaPlane][uBlock])
            {
                /* only a DC coefficient */
                if (!bypassFlag || chromaQP>0)
                {
                    Coeffs v = (Coeffs) ((ChromaDC[ChromaPlane][uBlock] + 32) >> 6);

                    /* add to prediction */
                    for (i = 0; i < 4; i++)
                    {
                        pTmpDst[0+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[0+ChromaPlane], v, bitDepth);
                        pTmpDst[2+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[2+ChromaPlane], v, bitDepth);
                        pTmpDst[4+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[4+ChromaPlane], v, bitDepth);
                        pTmpDst[6+ChromaPlane] = ClampTblLookup_16U<Plane>(pTmpDst[6+ChromaPlane], v, bitDepth);
                        pTmpDst += srcdstUVStep;
                    }
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

template<typename Plane, typename Coeffs, int chroma_format_idc>
void ippiReconstructChromaIntra4x4MB_H264_kernel(Coeffs **ppSrcDstCoeff,
                                                                   Plane *pSrcDstUVPlane,
                                                                   Ipp32u _srcdstUVStep,
                                                                   IppIntraChromaPredMode_H264 intraChromaMode,
                                                                   Ipp32u cbpU, Ipp32u cbpV,
                                                                   Ipp32u chromaQPU,
                                                                   Ipp32u chromaQPV,
                                                                   Ipp32u edgeType,
                                                                   const Ipp16s *pQuantTableU,
                                                                   const Ipp16s *pQuantTableV,
                                                                   Ipp16s levelScaleDCU,
                                                                   Ipp16s levelScaleDCV,
                                                                   Ipp32u bypassFlag,
                                                                   Ipp32u bitDepth)
{
    if (sizeof(Plane) == 1)
        bitDepth = 8;

    Plane defaultValue = (Plane)(1 << (bitDepth - 1));

    int srcdstUVStep = (int)_srcdstUVStep;
    Ipp32u top = (edgeType & IPPVC_TOP_EDGE);
    Ipp32u left = (edgeType & IPPVC_LEFT_EDGE);
    Plane  *pUV = pSrcDstUVPlane;

    Ipp32s i, j;

    switch( intraChromaMode )
    {
    case IPP_CHROMA_VERT:

        if (!top)
        {
            Ipp32s blockSize = (chroma_format_idc == 2) ? 16 : 8;
            Plane* puv = pUV - srcdstUVStep;

            for ( i = 0; i < blockSize; i++)
            {
                for (j = 0;  j < 16; j++)
                {
                    pUV[j] = puv[j];
                }
                pUV += srcdstUVStep;
            }
        }
        break;

   case IPP_CHROMA_HOR:
        if (!left)
        {
            Ipp32s blockSize = (chroma_format_idc == 2) ? 16 : 8;
            for ( i = 0; i < blockSize; i++)
            {
    #ifdef __ICL
    #pragma vector always
    #endif
                for (j = 0; j < 8; j++)
                {
                    pUV[2*j] = pUV[-2];
                    pUV[2*j + 1] = pUV[-1];
                }
                pUV += srcdstUVStep;
            }
        }
        break;

    case IPP_CHROMA_DC:
        {
            Ipp32s Su0, Su1, Su2, Su3, Su6, Su7;
            Ipp32s Sv0, Sv1, Sv2, Sv3, Sv6, Sv7;
            Plane PredictU[8];
            Plane PredictV[8];
            Plane* pu0, *pu1, *pu2, *pu3;
            Plane* pv0, *pv1, *pv2, *pv3;

            Plane* pu4, *pu5, *pu6, *pu7;
            Plane* pv4, *pv5, *pv6, *pv7;

            if (!top &&!left)
            {
                Su0 = Su1 = Su2 = Su3 = 0;
                Sv0 = Sv1 = Sv2 = Sv3 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i   - srcdstUVStep];
                    Su1 += pUV[2*i+8 - srcdstUVStep];
                    Su2 += pUV[ (i  )*srcdstUVStep-2];
                    Su3 += pUV[ (i+4)*srcdstUVStep-2];
                    Sv0 += pUV[1 + 2*i   - srcdstUVStep];
                    Sv1 += pUV[1 + 2*i+8 - srcdstUVStep];
                    Sv2 += pUV[1 +  (i  )*srcdstUVStep-2];
                    Sv3 += pUV[1 +  (i+4)*srcdstUVStep-2];
                }
                PredictU[0] = (Plane)((Su0 + Su2 + 4)>>3);
                PredictU[1] = (Plane)((Su1 + 2)>>2);
                PredictU[2] = (Plane)((Su3 + 2)>>2);
                PredictU[3] = (Plane)((Su1 + Su3 + 4)>>3);
                PredictV[0] = (Plane)((Sv0 + Sv2 + 4)>>3);
                PredictV[1] = (Plane)((Sv1 + 2)>>2);
                PredictV[2] = (Plane)((Sv3 + 2)>>2);
                PredictV[3] = (Plane)((Sv1 + Sv3 + 4)>>3);

                if (chroma_format_idc == 2)
                {
                    Su6 = Su7 = 0;
                    Sv6 = Sv7 = 0;

                    for (i = 0; i < 4; i++)
                    {
                        Su6 += pUV[ i*srcdstUVStep - 2 + srcdstUVStep*8];
                        Su7 += pUV[ (i+4)*srcdstUVStep - 2 + srcdstUVStep*8];

                        Sv6 += pUV[ 1 + i*srcdstUVStep - 2 + srcdstUVStep*8];
                        Sv7 += pUV[ 1 + (i+4)*srcdstUVStep - 2 + srcdstUVStep*8];
                    }

                    PredictU[4] = (Plane)((Su6 + 2)>>2);
                    PredictU[5] = (Plane)((Su6 + Su1 + 4)>>3);
                    PredictU[6] = (Plane)((Su7 + 2)>>2);
                    PredictU[7] = (Plane)((Su7 + Su1 + 4)>>3);

                    PredictV[4] = (Plane)((Sv6 + 2)>>2);
                    PredictV[5] = (Plane)((Sv6 + Sv1 + 4)>>3);
                    PredictV[6] = (Plane)((Sv7 + 2)>>2);
                    PredictV[7] = (Plane)((Sv7 + Sv1 + 4)>>3);
                }
            }
            else if (!top)
            {
                Su0 = Su1 = 0;
                Sv0 = Sv1 = 0;
                for (i = 0; i < 4; i++) {
                    Su0 += pUV[2*i   - srcdstUVStep];
                    Su1 += pUV[2*i+8 - srcdstUVStep];
                    Sv0 += pUV[1 + 2*i   - srcdstUVStep];
                    Sv1 += pUV[1 + 2*i+8 - srcdstUVStep];
                }
                PredictU[0] = PredictU[2] = (Plane)((Su0 + 2)>>2);
                PredictU[1] = PredictU[3] = (Plane)((Su1 + 2)>>2);
                PredictV[0] = PredictV[2] = (Plane)((Sv0 + 2)>>2);
                PredictV[1] = PredictV[3] = (Plane)((Sv1 + 2)>>2);

                if (chroma_format_idc == 2)
                {
                    PredictU[4] = PredictU[6] = (Plane)((Su0 + 2)>>2);
                    PredictU[5] = PredictU[7] = (Plane)((Su1 + 2)>>2);

                    PredictV[4] = PredictV[6] = (Plane)((Sv0 + 2)>>2);
                    PredictV[5] = PredictV[7] = (Plane)((Sv1 + 2)>>2);
                }
            }
            else if (!left)
            {
                Su2 = Su3 = 0;
                Sv2 = Sv3 = 0;

                for (i = 0; i < 4; i++) {
                    Su2 += pUV[ (i  )*srcdstUVStep - 2];
                    Su3 += pUV[ (i+4)*srcdstUVStep - 2];
                    Sv2 += pUV[ (i  )*srcdstUVStep - 1];
                    Sv3 += pUV[ (i+4)*srcdstUVStep - 1];
                }
                PredictU[0] = PredictU[1] = (Plane)((Su2 + 2)>>2);
                PredictU[2] = PredictU[3] = (Plane)((Su3 + 2)>>2);
                PredictV[0] = PredictV[1] = (Plane)((Sv2 + 2)>>2);
                PredictV[2] = PredictV[3] = (Plane)((Sv3 + 2)>>2);

                if (chroma_format_idc == 2)
                {
                    Sv6 = Sv7 = 0;
                    Su6 = Su7 = 0;

                    for (i = 0; i < 4; i++) {
                        Su6 += pUV[ (i  )*srcdstUVStep - 2 + srcdstUVStep*8];
                        Su7 += pUV[ (i+4)*srcdstUVStep - 2 + srcdstUVStep*8];
                        Sv6 += pUV[ (i  )*srcdstUVStep - 1 + srcdstUVStep*8];
                        Sv7 += pUV[ (i+4)*srcdstUVStep - 1 + srcdstUVStep*8];
                    }

                    PredictU[4] = PredictU[5] = (Plane)((Su6 + 2)>>2);
                    PredictU[6] = PredictU[7] = (Plane)((Su7 + 2)>>2);

                    PredictV[4] = PredictV[5] = (Plane)((Sv6 + 2)>>2);
                    PredictV[6] = PredictV[7] = (Plane)((Sv7 + 2)>>2);
                }
            }
            else
            {
                PredictU[0] = PredictU[1] = PredictU[2] = PredictU[3] = defaultValue;
                PredictV[0] = PredictV[1] = PredictV[2] = PredictV[3] = defaultValue;

                if (chroma_format_idc == 2)
                {
                    PredictU[4] = PredictU[5] = PredictU[6] = PredictU[7] = defaultValue;
                    PredictV[4] = PredictV[5] = PredictV[6] = PredictV[7] = defaultValue;
                }
            }

            pu0 = pUV;
            pu1 = pUV + 8;
            pu2 = pUV + 4*srcdstUVStep;
            pu3 = pu2 + 8;
            pv0 = pUV + 1;
            pv1 = pUV + 8 + 1;
            pv2 = pUV + 4*srcdstUVStep + 1;
            pv3 = pv2 + 8;

            if (chroma_format_idc == 2)
            {
                pu4 = pUV + 8*srcdstUVStep;
                pu5 = pu4 + 8;
                pu6 = pu4 + 4*srcdstUVStep;
                pu7 = pu6 + 8;

                pv4 = pUV + 8*srcdstUVStep + 1;
                pv5 = pv4 + 8;
                pv6 = pv4 + 4*srcdstUVStep;
                pv7 = pv6 + 8;
            }

            for (j = 0; j < 4; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    pu0[2*i] = PredictU[0];
                    pu1[2*i] = PredictU[1];
                    pu2[2*i] = PredictU[2];
                    pu3[2*i] = PredictU[3];
                    pv0[2*i] = PredictV[0];
                    pv1[2*i] = PredictV[1];
                    pv2[2*i] = PredictV[2];
                    pv3[2*i] = PredictV[3];

                    if (chroma_format_idc == 2)
                    {
                        pu4[2*i] = PredictU[4];
                        pu5[2*i] = PredictU[5];
                        pu6[2*i] = PredictU[6];
                        pu7[2*i] = PredictU[7];

                        pv4[2*i] = PredictV[4];
                        pv5[2*i] = PredictV[5];
                        pv6[2*i] = PredictV[6];
                        pv7[2*i] = PredictV[7];
                    }
                }

                pu0 += srcdstUVStep;
                pu1 += srcdstUVStep;
                pu2 += srcdstUVStep;
                pu3 += srcdstUVStep;
                pv0 += srcdstUVStep;
                pv1 += srcdstUVStep;
                pv2 += srcdstUVStep;
                pv3 += srcdstUVStep;

                if (chroma_format_idc == 2)
                {
                    pu4+= srcdstUVStep;
                    pu5+= srcdstUVStep;
                    pu6+= srcdstUVStep;
                    pu7+= srcdstUVStep;

                    pv4+= srcdstUVStep;
                    pv5+= srcdstUVStep;
                    pv6+= srcdstUVStep;
                    pv7+= srcdstUVStep;
                }
            }

            break;
        }

    case IPP_CHROMA_PLANE:
        {
            Plane pu1[9], pu2[17];
            Plane pv1[9], pv2[17];
            Ipp32s iHu;
            Ipp32s iVu;
            Ipp32s iHv;
            Ipp32s iVv;
            Ipp32s au,bu,cu;
            Ipp32s av,bv,cv;
            Ipp32s pu0;
            Ipp32s pv0;

            if (chroma_format_idc == 2)
            {
                for (i = 0; i < 9; i++)
                {
                    pu1[i] = pUV[2*i - srcdstUVStep-2];
                    pv1[i] = pUV[1 + 2*i - srcdstUVStep-2];
                }

                for (i = 0; i < 17; i++)
                {
                    pu2[i] = pUV[ srcdstUVStep*(i-1) -2 ];
                    pv2[i] = pUV[1 + srcdstUVStep*(i-1) -2 ];
                }

                iHu = 0;
                iVu = 0;
                iHv = 0;
                iVv = 0;

                for (i = 1; i <= 4; i++)
                {
                    iHu += (Ipp32s) (i * ((Ipp32s)pu1[4+i] - (Ipp32s)pu1[4-i]));
                    iHv += (Ipp32s) (i * ((Ipp32s)pv1[4+i] - (Ipp32s)pv1[4-i]));
                }

                for (i = 1; i <= 8; i++)
                {
                    iVu += (Ipp32s) (i * ((Ipp32s)pu2[8+i] - (Ipp32s)pu2[8-i]));
                    iVv += (Ipp32s) (i * ((Ipp32s)pv2[8+i] - (Ipp32s)pv2[8-i]));
                }

                au = ((Ipp32s)pu1[8] + (Ipp32s)pu2[16])*16;
                bu = (32*iHu + 2*iHu + 32)>>6;
                cu = (4*iVu + iVu + 32)>>6;
                av = ((Ipp32s)pv1[8] + (Ipp32s)pv2[16])*16;
                bv = (32*iHv + 2*iHv + 32)>>6;
                cv = (4*iVv + iVv + 32)>>6;

                for (j = 0; j < 16; j++)
                {
                    for (i = 0; i < 8; i++)
                    {
                        pu0 = (Ipp32s) ((au + (i - 3) * bu + (j - 3 - 4) * cu + 16) >> 5);
                        pUV[2*i + j*srcdstUVStep] = ClampVal_16U<Plane>(pu0, bitDepth);
                        pv0 = (Ipp32s) ((av + (i - 3) * bv + (j - 3 - 4) * cv + 16) >> 5);
                        pUV[2*i + 1 + j*srcdstUVStep] = ClampVal_16U<Plane>(pv0, bitDepth);
                    }
                }
            }
            else
            {
                for (i = 0; i < 9; i++)
                {
                    pu1[i] = pUV[2*i - srcdstUVStep-2];
                    pu2[i] = pUV[ srcdstUVStep*(i-1) -2 ];
                    pv1[i] = pUV[1 + 2*i - srcdstUVStep-2];
                    pv2[i] = pUV[1 + srcdstUVStep*(i-1) -2 ];
                }

                iHu = 0;
                iVu = 0;
                iHv = 0;
                iVv = 0;

                for (i = 1; i <= 4; i++)
                {
                    iVu += (Ipp32s) (i * ((Ipp32s)pu2[4+i] - (Ipp32s)pu2[4-i]));
                    iHu += (Ipp32s) (i * ((Ipp32s)pu1[4+i] - (Ipp32s)pu1[4-i]));
                    iVv += (Ipp32s) (i * ((Ipp32s)pv2[4+i] - (Ipp32s)pv2[4-i]));
                    iHv += (Ipp32s) (i * ((Ipp32s)pv1[4+i] - (Ipp32s)pv1[4-i]));
                }

                au = ((Ipp32s)pu1[8] + (Ipp32s)pu2[8])*16;
                bu = (16*iHu + iHu + 16)>>5;
                cu = (16*iVu + iVu + 16)>>5;
                av = ((Ipp32s)pv1[8] + (Ipp32s)pv2[8])*16;
                bv = (16*iHv + iHv + 16)>>5;
                cv = (16*iVv + iVv + 16)>>5;

                for (j = 0; j < 8; j++)
                {
                    for (i = 0; i < 8; i++)
                    {
                        pu0 = (Ipp32s) ((au + (i - 3) * bu + (j - 3) * cu + 16) >> 5);
                        pUV[2*i + j*srcdstUVStep] = ClampVal_16U<Plane>(pu0, bitDepth);
                        pv0 = (Ipp32s) ((av + (i - 3) * bv + (j - 3) * cv + 16) >> 5);
                        pUV[2*i + 1 + j*srcdstUVStep] = ClampVal_16U<Plane>(pv0, bitDepth);
                    }
                }
            }
        }
        break;
    }

    ippiReconstructChromaInter4x4MB_H264_16s8u_C2R<Plane, Coeffs, chroma_format_idc>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep, cbpU, cbpV, chromaQPU,chromaQPV,pQuantTableU,pQuantTableV, levelScaleDCU, levelScaleDCV, bypassFlag, bitDepth);
}

template<typename Plane, typename Coeffs, int chroma_format_idc>
void ippiReconstructChromaIntraHalfs4x4MB_NV12(Coeffs **ppSrcDstCoeff,
                                                Plane *pSrcDstUVPlane,
                                                Ipp32u srcdstUVStep_,
                                                IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                Ipp32u cbpU, Ipp32u cbpV,
                                                Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                Ipp8u edge_type_top, Ipp8u edge_type_bottom,
                                                const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                Ipp8u bypass_flag,
                                                Ipp32s bitDepth)
{
    Plane* pUV = pSrcDstUVPlane;

    if (sizeof(Plane) == 1)
        bitDepth = 8;

    int srcdstUVStep = (int)srcdstUVStep_;
    Plane defaultValue = (Plane)(1 << (bitDepth - 1));

    Ipp32s j,i;

    if (intra_chroma_mode == IPP_CHROMA_DC)
    {
        Ipp32s Su0, Su1, Su2, Su3, Su6, Su7;
        Ipp32s Sv0, Sv1, Sv2, Sv3, Sv6, Sv7;
        Plane PredictU[4];
        Plane PredictV[4];
        Plane* pu0 = pUV,     *pu1 = pUV + 8;
        Plane* pv0 = pUV + 1, *pv1 = pUV + 8 + 1;
        Ipp32u edge_type = edge_type_top;

        Plane* pu4 = pUV + 4*srcdstUVStep;
        Plane* pv4 = pUV + 4*srcdstUVStep + 1;
        Plane* pu5 = pu4 + 8;
        Plane* pv5 = pv4 + 8;

        /* first cicle iteration for upper halfblock, */
        {
            edge_type = edge_type_top;
            Ipp32u top = (edge_type & IPPVC_TOP_EDGE);
            Ipp32u left = (edge_type & IPPVC_LEFT_EDGE);

            if (!top && !left)
            {
                Su0 = Su1 = Su2 = 0;
                Sv0 = Sv1 = Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i     - srcdstUVStep];
                    Su1 += pUV[2*i + 8 - srcdstUVStep];
                    Su2 += pUV[i * srcdstUVStep - 2];
                    Sv0 += pUV[1 + 2*i     - srcdstUVStep];
                    Sv1 += pUV[1 + 2*i + 8 - srcdstUVStep];
                    Sv2 += pUV[1 + i * srcdstUVStep - 2];
                }
                PredictU[0] = (Plane)((Su0 + Su2 + 4)>>3);
                PredictU[1] = (Plane)((Su1 + 2)>>2);
                PredictV[0] = (Plane)((Sv0 + Sv2 + 4)>>3);
                PredictV[1] = (Plane)((Sv1 + 2)>>2);

                if (chroma_format_idc == 2)
                {
                    Su6 = 0;
                    Sv6 = 0;

                    for (i = 0; i < 4; i++)
                    {
                        Su6 += pUV[i*srcdstUVStep - 2 + srcdstUVStep*4];
                        Sv6 += pUV[1 + i*srcdstUVStep - 2 + srcdstUVStep*4];
                    }

                    PredictU[2] = (Plane)((Su6 + 2)>>2);
                    PredictU[3] = (Plane)((Su6 + Su1 + 4)>>3);
                    PredictV[2] = (Plane)((Sv6 + 2)>>2);
                    PredictV[3] = (Plane)((Sv6 + Sv1 + 4)>>3);
                }
            }
            else if (!top)
            {
                Su0 = Su1 = 0;
                Sv0 = Sv1 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pUV[2*i     - srcdstUVStep];
                    Su1 += pUV[2*i + 8 - srcdstUVStep];
                    Sv0 += pUV[1 + 2*i     - srcdstUVStep];
                    Sv1 += pUV[1 + 2*i + 8 - srcdstUVStep];
                }
                PredictU[0] = (Plane)((Su0 + 2)>>2);
                PredictU[1] = (Plane)((Su1 + 2)>>2);
                PredictV[0] = (Plane)((Sv0 + 2)>>2);
                PredictV[1] = (Plane)((Sv1 + 2)>>2);

                if (chroma_format_idc == 2)
                {
                    PredictU[2] = (Plane)((Su0 + 2)>>2);
                    PredictU[3] = (Plane)((Su1 + 2)>>2);

                    PredictV[2] = (Plane)((Sv0 + 2)>>2);
                    PredictV[3] = (Plane)((Sv1 + 2)>>2);
                }
            }
            else if (!left)
            {
                Su2 = 0;
                Sv2 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su2 += pUV[i * srcdstUVStep - 2];
                    Sv2 += pUV[i * srcdstUVStep - 1];
                }
                PredictU[0] = PredictU[1] = (Plane)((Su2 + 2)>>2);
                PredictV[0] = PredictV[1] = (Plane)((Sv2 + 2)>>2);

                if (chroma_format_idc == 2)
                {
                    Su6 = 0;
                    Sv6 = 0;

                    for (i = 0; i < 4; i++)
                    {
                        Su6 += pUV[i * srcdstUVStep - 2 + srcdstUVStep*4];
                        Sv6 += pUV[i * srcdstUVStep - 1 + srcdstUVStep*4];
                    }
                    PredictU[2] = PredictU[3] = (Plane)((Su6 + 2)>>2);
                    PredictV[2] = PredictV[3] = (Plane)((Sv6 + 2)>>2);
                }
            }
            else
            {
                PredictU[0] = PredictU[1] = defaultValue;
                PredictV[0] = PredictV[1] = defaultValue;

                if (chroma_format_idc == 2)
                {
                    PredictU[2] = PredictU[3] = defaultValue;
                    PredictV[2] = PredictV[3] = defaultValue;
                }
            }

            for (j = 0; j < 4; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    pu0[2*i] = PredictU[0];
                    pu1[2*i] = PredictU[1];
                    pv0[2*i] = PredictV[0];
                    pv1[2*i] = PredictV[1];

                    if (chroma_format_idc == 2)
                    {
                        pu4[2*i] = PredictU[2];
                        pu5[2*i] = PredictU[3];
                        pv4[2*i] = PredictV[2];
                        pv5[2*i] = PredictV[3];
                    }
                }

                pu0 += srcdstUVStep;
                pu1 += srcdstUVStep;
                pv0 += srcdstUVStep;
                pv1 += srcdstUVStep;

                if (chroma_format_idc == 2)
                {
                    pu4 += srcdstUVStep;
                    pu5 += srcdstUVStep;
                    pv4 += srcdstUVStep;
                    pv5 += srcdstUVStep;
                }
            }
        }
        /* second cicle iteration for lower halfblock */
        {
            edge_type = edge_type_bottom;
            Ipp32u top = (edge_type_top & IPPVC_TOP_EDGE);
            Ipp32u left = (edge_type & IPPVC_LEFT_EDGE);

            if (chroma_format_idc == 2)
            {
                pu0 += 4*srcdstUVStep;
                pu1 += 4*srcdstUVStep;
                pv0 += 4*srcdstUVStep;
                pv1 += 4*srcdstUVStep;
                pu4 += 4*srcdstUVStep;
                pu5 += 4*srcdstUVStep;
                pv4 += 4*srcdstUVStep;
                pv5 += 4*srcdstUVStep;

                pUV += 4*srcdstUVStep;
            }

            if (!top && !left)
            {
                Su1 = Su3 = 0;
                Sv1 = Sv3 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su1 += pSrcDstUVPlane[2*i + 8 - srcdstUVStep];
                    Su3 += pUV[(i + 4) * srcdstUVStep - 2];
                    Sv1 += pSrcDstUVPlane[1 + 2*i + 8 - srcdstUVStep];
                    Sv3 += pUV[1+ (i + 4) * srcdstUVStep - 2];
                }

                PredictU[0] = (Plane)((Su3 + 2)>>2);
                PredictU[1] = (Plane)((Su1 + Su3 + 4)>>3);
                PredictV[0] = (Plane)((Sv3 + 2)>>2);
                PredictV[1] = (Plane)((Sv1 + Sv3 + 4)>>3);

                if (chroma_format_idc == 2)
                {
                    Su6 = Su7 = 0;
                    Sv6 = Sv7 = 0;

                    for (i = 0; i < 4; i++)
                    {
                        Su6 += pUV[ i*srcdstUVStep - 2 + srcdstUVStep*4];
                        Su7 += pUV[ (i+4)*srcdstUVStep - 2 + srcdstUVStep*4];

                        Sv6 += pUV[ 1 + i*srcdstUVStep - 2 + srcdstUVStep*4];
                        Sv7 += pUV[ 1 + (i+4)*srcdstUVStep - 2 + srcdstUVStep*4];
                    }

                    PredictU[2] = (Plane)((Su7 + 2)>>2);
                    PredictU[3] = (Plane)((Su7 + Su1 + 4)>>3);
                    PredictV[2] = (Plane)((Sv7 + 2)>>2);
                    PredictV[3] = (Plane)((Sv7 + Sv1 + 4)>>3);
                }
            }
            else if (!top)
            {
                Su0 = Su1 = 0;
                Sv0 = Sv1 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su0 += pSrcDstUVPlane[2*i     - srcdstUVStep];
                    Su1 += pSrcDstUVPlane[2*i + 8 - srcdstUVStep];
                    Sv0 += pSrcDstUVPlane[1 + 2*i     - srcdstUVStep];
                    Sv1 += pSrcDstUVPlane[1 + 2*i + 8 - srcdstUVStep];
                }
                PredictU[0] = (Plane)((Su0 + 2)>>2);
                PredictU[1] = (Plane)((Su1 + 2)>>2);
                PredictV[0] = (Plane)((Sv0 + 2)>>2);
                PredictV[1] = (Plane)((Sv1 + 2)>>2);

                if (chroma_format_idc == 2)
                {
                    PredictU[2] = (Plane)((Su0 + 2)>>2);
                    PredictU[3] = (Plane)((Su1 + 2)>>2);
                    PredictV[2] = (Plane)((Sv0 + 2)>>2);
                    PredictV[3] = (Plane)((Sv1 + 2)>>2);
                }
            }
            else if (!left)
            {
                Su3 = 0;
                Sv3 = 0;
                for (i = 0; i < 4; i++)
                {
                    Su3 += pUV[(i + 4)* srcdstUVStep - 2];
                    Sv3 += pUV[1 + (i + 4) * srcdstUVStep - 2];
                }
                PredictU[0] = PredictU[1] = (Plane)((Su3 + 2)>>2);
                PredictV[0] = PredictV[1] = (Plane)((Sv3 + 2)>>2);

                if (chroma_format_idc == 2)
                {
                    Sv7 = 0;
                    Su7 = 0;

                    for (i = 0; i < 4; i++) {
                        Su7 += pUV[ (i+4)*srcdstUVStep - 2 + srcdstUVStep*4];
                        Sv7 += pUV[ (i+4)*srcdstUVStep - 1 + srcdstUVStep*4];
                    }

                    PredictU[2] = PredictU[3] = (Plane)((Su7 + 2)>>2);
                    PredictV[2] = PredictV[3] = (Plane)((Sv7 + 2)>>2);
                }
            }
            else
            {
                PredictU[0] = PredictU[1] = defaultValue;
                PredictV[0] = PredictV[1] = defaultValue;
                if (chroma_format_idc == 2)
                {
                    PredictU[2] = PredictU[3] = defaultValue;
                    PredictV[2] = PredictV[3] = defaultValue;
                }
            }

            for (j = 0; j < 4; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    pu0[2*i] = PredictU[0];
                    pu1[2*i] = PredictU[1];
                    pv0[2*i] = PredictV[0];
                    pv1[2*i] = PredictV[1];

                    if (chroma_format_idc == 2)
                    {
                        pu4[2*i] = PredictU[2];
                        pu5[2*i] = PredictU[3];
                        pv4[2*i] = PredictV[2];
                        pv5[2*i] = PredictV[3];
                    }
                }

                pu0 += srcdstUVStep;
                pu1 += srcdstUVStep;
                pv0 += srcdstUVStep;
                pv1 += srcdstUVStep;

                if (chroma_format_idc == 2)
                {
                    pu4 += srcdstUVStep;
                    pu5 += srcdstUVStep;
                    pv4 += srcdstUVStep;
                    pv5 += srcdstUVStep;
                }
            }
        }
    }
    else if (IPP_CHROMA_VERT == intra_chroma_mode)
    {
        if (0 == (edge_type_top & IPPVC_TOP_EDGE))
        {
            Plane* puv = pUV - srcdstUVStep;

            Ipp32s blockSize = (chroma_format_idc == 2) ? 16 : 8;

            for ( i = 0; i < blockSize; i++)
            {
                for (j = 0;  j < 16; j++)
                {
                    pUV[j] = puv[j];
                }
                pUV += srcdstUVStep;
            }
        }
        else
            return;
    }
    else
        return;

    ippiReconstructChromaInter4x4MB_H264_16s8u_C2R<Plane, Coeffs, chroma_format_idc>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep, cbpU, cbpV, chromaQPU,chromaQPV,pQuantTableU,pQuantTableV, levelScaleDCU, levelScaleDCV, bypass_flag, bitDepth);
}


void MFX_Dispatcher::ReconstructChromaIntra4x4(Ipp32s **ppSrcDstCoeff,
                                                Ipp16u *pSrcDstUVPlane,
                                                Ipp32u srcdstUVStep,
                                                IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                Ipp32u cbpU, Ipp32u cbpV,
                                                Ipp32u chromaQPU,
                                                Ipp32u chromaQPV,
                                                Ipp8u  edge_type,
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
        ippiReconstructChromaIntra4x4MB_H264_kernel<mfxU16, mfxI32, 2>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
                                                    intra_chroma_mode,
                                                    cbpU, cbpV,
                                                    chromaQPU, chromaQPV,
                                                    edge_type,
                                                    pQuantTableU, pQuantTableV,
                                                    levelScaleDCU, levelScaleDCV, 
                                                    bypass_flag,
                                                    bit_depth);
    } else {
        ippiReconstructChromaIntra4x4MB_H264_kernel<mfxU16, mfxI32, 1>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
                                                    intra_chroma_mode,
                                                    cbpU, cbpV,
                                                    chromaQPU, chromaQPV,
                                                    edge_type,
                                                    pQuantTableU, pQuantTableV,
                                                    levelScaleDCU, levelScaleDCV, 
                                                    bypass_flag,
                                                    bit_depth);
    }
}

void MFX_Dispatcher::ReconstructChromaInter4x4(Ipp32s **ppSrcDstCoeff,
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

void MFX_Dispatcher::ReconstructChromaIntra4x4(Ipp16s **ppSrcDstCoeff,
                                                Ipp8u *pSrcDstUVPlane,
                                                Ipp32u srcdstUVStep,
                                                IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                Ipp32u cbpU, Ipp32u cbpV,
                                                Ipp32u chromaQPU,
                                                Ipp32u chromaQPV,
                                                Ipp8u  edge_type,
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
        ippiReconstructChromaIntra4x4MB_H264_kernel<mfxU8, mfxI16, 2>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
                                                    intra_chroma_mode,
                                                    cbpU, cbpV,
                                                    chromaQPU, chromaQPV,
                                                    edge_type,
                                                    pQuantTableU, pQuantTableV,
                                                    levelScaleDCU, levelScaleDCV, 
                                                    bypass_flag,
                                                    bit_depth);
    } else {
        ippiReconstructChromaIntra4x4MB_H264_kernel<mfxU8, mfxI16, 1>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
                                                    intra_chroma_mode,
                                                    cbpU, cbpV,
                                                    chromaQPU, chromaQPV,
                                                    edge_type,
                                                    pQuantTableU, pQuantTableV,
                                                    levelScaleDCU, levelScaleDCV, 
                                                    bypass_flag,
                                                    bit_depth);
    }
}

void MFX_Dispatcher::ReconstructChromaInter4x4(Ipp16s **ppSrcDstCoeff,
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

void MFX_Dispatcher::ReconstructChromaIntraHalfs4x4MB_NV12(Ipp16s **ppSrcDstCoeff,
                                                      Ipp8u *pSrcDstUVPlane,
                                                      Ipp32u srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      Ipp32u cbpU, Ipp32u cbpV,
                                                      Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                      Ipp8u  edge_type_top, Ipp8u  edge_type_bottom,
                                                      const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                      Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                      Ipp8u  bypass_flag,
                                                      Ipp32s bit_depth,
                                                      Ipp32u chroma_format)
{
    if (chroma_format == 2)
    {
        ippiReconstructChromaIntraHalfs4x4MB_NV12<mfxU8, mfxI16, 2>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
                                                    intra_chroma_mode,
                                                    cbpU, cbpV,
                                                    chromaQPU, chromaQPV,
                                                    edge_type_top, edge_type_bottom,
                                                    pQuantTableU, pQuantTableV,
                                                    levelScaleDCU, levelScaleDCV,
                                                    bypass_flag,
                                                    bit_depth);
    } else {
        ippiReconstructChromaIntraHalfs4x4MB_NV12<mfxU8, mfxI16, 1>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
                                                    intra_chroma_mode,
                                                    cbpU, cbpV,
                                                    chromaQPU, chromaQPV,
                                                    edge_type_top, edge_type_bottom,
                                                    pQuantTableU, pQuantTableV,
                                                    levelScaleDCU, levelScaleDCV,
                                                    bypass_flag,
                                                    bit_depth);
    }
}

void MFX_Dispatcher::ReconstructChromaIntraHalfs4x4MB_NV12(Ipp32s **ppSrcDstCoeff,
                                                      Ipp16u *pSrcDstUVPlane,
                                                      Ipp32u srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      Ipp32u cbpU, Ipp32u cbpV,
                                                      Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                      Ipp8u  edge_type_top, Ipp8u  edge_type_bottom,
                                                      const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                      Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                      Ipp8u  bypass_flag,
                                                      Ipp32s bit_depth,
                                                      Ipp32u chroma_format)
{
    if (chroma_format == 2)
    {
        ippiReconstructChromaIntraHalfs4x4MB_NV12<mfxU16, mfxI32, 2>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
                                                    intra_chroma_mode,
                                                    cbpU, cbpV,
                                                    chromaQPU, chromaQPV,
                                                    edge_type_top, edge_type_bottom,
                                                    pQuantTableU, pQuantTableV,
                                                    levelScaleDCU, levelScaleDCV,
                                                    bypass_flag,
                                                    bit_depth);
    } else {
        ippiReconstructChromaIntraHalfs4x4MB_NV12<mfxU16, mfxI32, 1>(ppSrcDstCoeff, pSrcDstUVPlane, srcdstUVStep,
                                                    intra_chroma_mode,
                                                    cbpU, cbpV,
                                                    chromaQPU, chromaQPV,
                                                    edge_type_top, edge_type_bottom,
                                                    pQuantTableU, pQuantTableV,
                                                    levelScaleDCU, levelScaleDCV,
                                                    bypass_flag,
                                                    bit_depth);
    }
}

} // namespace MFX_PP
