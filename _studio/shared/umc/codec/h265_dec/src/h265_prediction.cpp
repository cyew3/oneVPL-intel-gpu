/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/


#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include <immintrin.h>

/* NOTE: In debug mode compiler attempts to load data with MOVNTDQA while data is
+only 8-byte aligned, but PMOVZX does not require 16-byte alignment. */
#ifdef NDEBUG
  #define MM_LOAD_EPI64(x) (*(__m128i*)x)
#else
  #define MM_LOAD_EPI64(x) _mm_loadl_epi64( (__m128i*)x )
#endif

#include "h265_prediction.h"
#include "umc_h265_dec_ipplevel.h"
#include "umc_h265_frame_info.h"

namespace UMC_HEVC_DECODER
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / destructor / initialize
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265Prediction::H265Prediction()
{
    m_YUVExt = 0;
    m_temp_interpolarion_buffer = 0;
}

H265Prediction::~H265Prediction()
{
    delete[] m_YUVExt;
    m_YUVExt = 0;

    m_YUVPred[0].destroy();
    m_YUVPred[1].destroy();

    ippsFree(m_temp_interpolarion_buffer);
    m_temp_interpolarion_buffer = 0;
}

void H265Prediction::InitTempBuff()
{
    if (m_YUVExt && m_YUVExtHeight == ((g_MaxCUHeight + 2) << 4) && 
        m_YUVExtStride == ((g_MaxCUWidth  + 8) << 4))
        return;

    if (m_YUVExt)
    {
        delete[] m_YUVExt;

        m_YUVPred[0].destroy();
        m_YUVPred[1].destroy();
    }

    // ML: OPT: TODO: Allocate only when we need it
    m_YUVExtHeight = ((g_MaxCUHeight + 2) << 4);
    m_YUVExtStride = ((g_MaxCUWidth  + 8) << 4);
    m_YUVExt = new H265PlaneYCommon[m_YUVExtStride * m_YUVExtHeight];

    // new structure
    m_YUVPred[0].create(g_MaxCUWidth, g_MaxCUHeight, sizeof(Ipp16s), sizeof(Ipp16s), g_MaxCUWidth, g_MaxCUHeight, g_MaxCUDepth);
    m_YUVPred[1].create(g_MaxCUWidth, g_MaxCUHeight, sizeof(Ipp16s), sizeof(Ipp16s), g_MaxCUWidth, g_MaxCUHeight, g_MaxCUDepth);

    if (!m_temp_interpolarion_buffer)
        m_temp_interpolarion_buffer = ippsMalloc_8u(2*128*128);    
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public member functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265PlaneYCommon H265Prediction::PredIntraGetPredValDC(H265PlanePtrYCommon pSrc, Ipp32s SrcStride, Ipp32s Size)
{
    Ipp32s Ind;
    Ipp32s Sum = 0;
    H265PlaneYCommon DCVal;

    for (Ind = 0; Ind < Size; Ind++)
    {
        Sum += pSrc[Ind - SrcStride];
    }
    for (Ind = 0; Ind < Size; Ind++)
    {
        Sum += pSrc[Ind * SrcStride - 1];
    }

    DCVal = (H265PlaneYCommon) ((Sum + Size) / (Size << 1));

    return DCVal;
}

void H265Prediction::PredIntraLumaAng(H265Pattern* pPattern, Ipp32u DirMode, H265PlanePtrYCommon pPred, Ipp32u Stride, Ipp32s Size)
{
    H265PlanePtrYCommon pDst = pPred;
    H265PlanePtrYCommon pSrc;

    VM_ASSERT(g_ConvertToBit[Size] >= 0); //   4x  4
    VM_ASSERT(g_ConvertToBit[Size] <= 5); // 128x128

    pSrc = pPattern->GetPredictorPtr(DirMode, g_ConvertToBit[Size] + 2, m_YUVExt);

    // get starting pixel in block
    Ipp32s sw = 2 * Size + 1;

    // Create the prediction
    if (DirMode == PLANAR_IDX)
    {
        PredIntraPlanarLuma(pSrc + sw + 1, sw, pDst, Stride, Size);
    }
    else
    {
        if (Size > 16)
        {
            PredIntraAngLuma(g_bitDepthY, pSrc + sw + 1, sw, pDst, Stride, Size, DirMode, false);
        }
        else
        {
            PredIntraAngLuma(g_bitDepthY, pSrc + sw + 1, sw, pDst, Stride, Size, DirMode, true);

            if(DirMode == DC_IDX)
            {
                DCPredFiltering(pSrc + sw + 1, sw, pDst, Stride, Size);
            }
        }
    }
}

void H265Prediction::PredIntraPlanarLuma(H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrYCommon pDst, Ipp32s dstStride, Ipp32s blkSize)
{
    Ipp32s bottomLeft;
    Ipp32s topRight;
    Ipp32s horPred;
    Ipp32s leftColumn[MAX_CU_SIZE];
    Ipp32s topRow[MAX_CU_SIZE];
    Ipp32s bottomRow[MAX_CU_SIZE];
    Ipp32s rightColumn[MAX_CU_SIZE];
    Ipp32u offset2D = blkSize;
    Ipp32u shift1D = g_ConvertToBit[blkSize] + 2;
    Ipp32u shift2D = shift1D + 1;

    // ML: OPT: TODO: For faster vectorized code convert the below code into template function with shift1D as a const parameter, runtime check/dispatch based on shift1D value 

    // Get left and above reference column and row
    for(Ipp32s k = 0; k < blkSize + 1; k++)
    {
        topRow[k] = pSrc[k - srcStride];
        leftColumn[k] = pSrc[k * srcStride - 1];
    }

    // Prepare intermediate variables used in interpolation
    bottomLeft = leftColumn[blkSize];
    topRight = topRow[blkSize];
    for (Ipp32s k = 0; k < blkSize; k++)
    {
        bottomRow[k] = bottomLeft - topRow[k];
        rightColumn[k] = topRight - leftColumn[k];
        topRow[k] <<= shift1D;
        leftColumn[k] <<= shift1D;
    }

    // Generate prediction signal
    for (Ipp32s k = 0; k < blkSize; k++)
    {
        horPred = leftColumn[k] + offset2D;
        for (Ipp32s l = 0; l < blkSize; l++)
        {
            horPred += rightColumn[k];
            topRow[l] += bottomRow[l];
            pDst[k * dstStride + l] = (H265PlaneYCommon) ((horPred + topRow[l]) >> shift2D);
        }
  }
}

void H265Prediction::PredIntraPlanarChroma(H265PlanePtrUVCommon pSrc, Ipp32s srcStride, H265PlanePtrUVCommon pDst, Ipp32s dstStride, Ipp32s blkSize)
{
    Ipp32s bottomLeft1, bottomLeft2;
    Ipp32s topRight1, topRight2;
    Ipp32s horPred1, horPred2;
    Ipp32s leftColumn[MAX_CU_SIZE];
    Ipp32s topRow[MAX_CU_SIZE];
    Ipp32s bottomRow[MAX_CU_SIZE];
    Ipp32s rightColumn[MAX_CU_SIZE];
    Ipp32u offset2D = blkSize;
    Ipp32u shift1D = g_ConvertToBit[blkSize] + 2;
    Ipp32u shift2D = shift1D + 1;
    Ipp32s srcStrideHalf = srcStride >> 1;

    // Get left and above reference column and row
    for(Ipp32s k = 0; k < (blkSize + 1) * 2; k += 2)
    {
        topRow[k] = pSrc[k - srcStride];
        leftColumn[k] = pSrc[k * srcStrideHalf - 2];
        topRow[k + 1] = pSrc[k - srcStride + 1];
        leftColumn[k + 1] = pSrc[k * srcStrideHalf - 2 + 1];
    }

    // Prepare intermediate variables used in interpolation
    bottomLeft1 = leftColumn[blkSize * 2];
    topRight1 = topRow[blkSize * 2];
    bottomLeft2 = leftColumn[blkSize * 2 + 1];
    topRight2 = topRow[blkSize * 2 + 1];
    for (Ipp32s k = 0; k < blkSize * 2; k += 2)
    {
        bottomRow[k] = bottomLeft1 - topRow[k];
        rightColumn[k] = topRight1 - leftColumn[k];
        topRow[k] <<= shift1D;
        leftColumn[k] <<= shift1D;

        bottomRow[k + 1] = bottomLeft2 - topRow[k + 1];
        rightColumn[k + 1] = topRight2 - leftColumn[k + 1];
        topRow[k + 1] <<= shift1D;
        leftColumn[k + 1] <<= shift1D;
    }

    // Generate prediction signal
    for (Ipp32s k = 0; k < blkSize; k++)
    {
        horPred1 = leftColumn[k * 2] + offset2D;
        horPred2 = leftColumn[k * 2 + 1] + offset2D;
        for (Ipp32s l = 0; l < blkSize * 2; l += 2)
        {
            horPred1 += rightColumn[k * 2];
            topRow[l] += bottomRow[l];
            pDst[k * dstStride + l] = (H265PlaneUVCommon) ((horPred1 + topRow[l]) >> shift2D);

            horPred2 += rightColumn[k * 2 + 1];
            topRow[l + 1] += bottomRow[l + 1];
            pDst[k * dstStride + l + 1] = (H265PlaneUVCommon) ((horPred2 + topRow[l + 1]) >> shift2D);
        }
    }
}

static Ipp32s angTableLuma[9] = {0,    2,    5,   9,  13,  17,  21,  26,  32};
static Ipp32s invAngTableLuma[9] = {0, 4096, 1638, 910, 630, 482, 390, 315, 256}; // (256 * 32) / Angle

void H265Prediction::PredIntraAngLuma(Ipp32s bitDepth, H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrYCommon Dst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode, bool Filter)
{
    Ipp32s k;
    Ipp32s l;
    H265PlanePtrYCommon pDst = Dst;

    // Map the mode index to main prediction direction and angle
    VM_ASSERT(dirMode > 0); //no planar
    bool modeDC        = dirMode < 2;
    bool modeHor       = !modeDC && (dirMode < 18);
    bool modeVer       = !modeDC && !modeHor;
    Ipp32s intraPredAngle = modeVer ? (Ipp32s) dirMode - VER_IDX : modeHor ? -((Ipp32s)dirMode - HOR_IDX) : 0;
    Ipp32s absAng = abs(intraPredAngle);
    Ipp32s signAng = intraPredAngle < 0 ? -1 : 1;

    // Set bitshifts and scale the angle parameter to block size
    Ipp32s invAngle = invAngTableLuma[absAng];
    absAng = angTableLuma[absAng];
    intraPredAngle = signAng * absAng;
    Ipp32s maxVal = (1 << bitDepth) - 1;

    // Do the DC prediction
    if (modeDC)
    {
        H265PlaneYCommon dcval = PredIntraGetPredValDC(pSrc, srcStride, blkSize);

        for (k = 0; k < blkSize; k++)
        {
            for (l = 0; l < blkSize; l++)
            {
                pDst[k * dstStride + l] = dcval;
            }
        }
    }

    // Do angular predictions
    else
    {
        H265PlanePtrYCommon refMain;
        H265PlanePtrYCommon refSide;
        H265PlaneYCommon refAbove[2 * MAX_CU_SIZE + 1];
        H265PlaneYCommon refLeft[2 * MAX_CU_SIZE + 1];

        // Initialise the Main and Left reference array.
        if (intraPredAngle < 0)
        {
            for (k = 0; k < blkSize + 1; k++)
            {
                refAbove[k + blkSize - 1] = pSrc[k - srcStride - 1];
            }
            for (k = 0; k < blkSize + 1; k++)
            {
                refLeft[k + blkSize - 1] = pSrc[(k - 1) * srcStride - 1];
            }
            refMain = (modeVer ? refAbove : refLeft) + (blkSize - 1);
            refSide = (modeVer ? refLeft : refAbove) + (blkSize - 1);

            // Extend the Main reference to the left.
            Ipp32s invAngleSum = 128;       // rounding for (shift by 8)
            for (k = -1; k > blkSize * intraPredAngle >> 5; k--)
            {
                invAngleSum += invAngle;
                refMain[k] = refSide[invAngleSum >> 8];
            }
        }
        else
        {
            for (k = 0; k < 2 * blkSize + 1; k++)
            {
                refAbove[k] = pSrc[k - srcStride - 1];
            }
            for (k = 0; k < 2 * blkSize + 1; k++)
            {
                refLeft[k] = pSrc[(k - 1) * srcStride - 1];
            }
            refMain = modeVer ? refAbove : refLeft;
            refSide = modeVer ? refLeft  : refAbove;
        }

        if (intraPredAngle == 0)
        {
            for (k = 0; k < blkSize; k++)
            {
                for (l = 0; l < blkSize; l++)
                {
                    pDst[k * dstStride + l] = refMain[l + 1];
                }
            }
            if (Filter)
            {
#if (HEVC_OPT_CHANGES & 32)
                // ML : OPT: TODO: vectorize, replace 'maxVals' with constant to allow for PACKS usage
                #pragma ivdep
                // #pragma vector always
#endif
                for (k = 0;k < blkSize; k++)
                {
                    pDst[k * dstStride] = (H265PlaneYCommon) Clip3(0, maxVal, pDst[k * dstStride] + ((refSide[k + 1] - refSide[0]) >> 1));
                }
            }
        }
        else
        {
            Ipp32s deltaPos = 0;
            Ipp32s deltaInt;
            Ipp32s deltaFract;
            Ipp32s refMainIndex;

            for (k = 0; k < blkSize; k++)
            {
                deltaPos += intraPredAngle;
                deltaInt = deltaPos >> 5;
                deltaFract = deltaPos & (32 - 1);

                if (deltaFract)
                {
                    // Do linear filtering
                    for (l = 0; l < blkSize; l++)
                    {
                        refMainIndex = l + deltaInt + 1;
                        pDst[k * dstStride + l] = (H265PlaneYCommon) (((32 - deltaFract) * refMain[refMainIndex] + deltaFract * refMain[refMainIndex + 1] + 16) >> 5);
                    }
                }
                else
                {
                    // Just copy the integer samples
                    for (l = 0; l < blkSize; l++)
                    {
                        pDst[k * dstStride + l] = refMain[l + deltaInt + 1];
                    }
                }
            }
        }

        // Flip the block if this is the horizontal mode
        if (modeHor)
        {
            H265PlaneYCommon tmp;
            for (k = 0; k < blkSize - 1; k++)
            {
                for (l = k + 1; l < blkSize; l++)
                {
                    tmp = pDst[k * dstStride + l];
                    pDst[k * dstStride + l] = pDst[l * dstStride + k];
                    pDst[l * dstStride + k] = tmp;
                }
            }
        }
    }
}

static Ipp32s angTableChroma[9] = {0,    2,    5,   9,  13,  17,  21,  26,  32};
static Ipp32s invAngTableChroma[9] = {0, 4096, 1638, 910, 630, 482, 390, 315, 256}; // (256 * 32) / Angle

void H265Prediction::PredIntraAngChroma(Ipp32s bitDepth, H265PlanePtrUVCommon pSrc, Ipp32s srcStride, H265PlanePtrUVCommon Dst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode)
{
    bitDepth;
    Ipp32s k;
    Ipp32s l;
    H265PlanePtrUVCommon pDst = Dst;

    // Map the mode index to main prediction direction and angle
    VM_ASSERT(dirMode > 0); //no planar
    bool modeDC        = dirMode < 2;
    bool modeHor       = !modeDC && (dirMode < 18);
    bool modeVer       = !modeDC && !modeHor;
    Ipp32s intraPredAngle = modeVer ? (Ipp32s) dirMode - VER_IDX : modeHor ? -((Ipp32s)dirMode - HOR_IDX) : 0;
    Ipp32s absAng = abs(intraPredAngle);
    Ipp32s signAng = intraPredAngle < 0 ? -1 : 1;

    // Set bitshifts and scale the angle parameter to block size
    Ipp32s invAngle = invAngTableChroma[absAng];
    absAng = angTableChroma[absAng];
    intraPredAngle = signAng * absAng;
    Ipp32s dstStrideHalf = dstStride >> 1;

    // Do the DC prediction
    if (modeDC)
    {
        H265PlaneUVCommon dc1, dc2;
        Ipp32s Sum1 = 0, Sum2 = 0;
        for (Ipp32s Ind = 0; Ind < blkSize * 2; Ind += 2)
        {
            Sum1 += pSrc[Ind - srcStride];
            Sum2 += pSrc[Ind - srcStride + 1];
        }
        for (Ipp32s Ind = 0; Ind < blkSize; Ind++)
        {
            Sum1 += pSrc[Ind * srcStride - 2];
            Sum2 += pSrc[Ind * srcStride - 2 + 1];
        }

        dc1 = (H265PlaneUVCommon)((Sum1 + blkSize) / (blkSize << 1));
        dc2 = (H265PlaneUVCommon)((Sum2 + blkSize) / (blkSize << 1));

#if (HEVC_OPT_CHANGES & 128)
// ML: TODO: ICC generates bad seq with PEXTR instead of vectorizing properly
#endif
        for (k = 0; k < blkSize; k++)
        {
            for (l = 0; l < blkSize * 2; l += 2)
            {
                pDst[k * dstStride + l] = dc1;
                pDst[k * dstStride + l + 1] = dc2;
            }
        }
    }

    // Do angular predictions
    else
    {
        H265PlanePtrUVCommon refMain;
        H265PlanePtrUVCommon refSide;
        H265PlaneUVCommon refAbove[2 * MAX_CU_SIZE + 1];
        H265PlaneUVCommon refLeft[2 * MAX_CU_SIZE + 1];

        // Initialise the Main and Left reference array.
        if (intraPredAngle < 0)
        {
            for (k = 0; k < (blkSize + 1) * 2; k += 2)
            {
                refAbove[k + (blkSize - 1) * 2] = pSrc[k - srcStride - 2];
                refAbove[k + (blkSize - 1) * 2 + 1] = pSrc[k - srcStride - 2 + 1];
            }
            for (k = 0; k < blkSize + 1; k++)
            {
                refLeft[(k + blkSize - 1) * 2] = pSrc[(k - 1) * srcStride - 2];
                refLeft[(k + blkSize - 1) * 2 + 1] = pSrc[(k - 1) * srcStride - 2 + 1];
            }
            refMain = (modeVer ? refAbove : refLeft) + (blkSize - 1) * 2;
            refSide = (modeVer ? refLeft : refAbove) + (blkSize - 1) * 2;

            // Extend the Main reference to the left.
            Ipp32s invAngleSum = 128;       // rounding for (shift by 8)
            for (k = -2; k > (blkSize * intraPredAngle >> 5) * 2; k -= 2)
            {
                invAngleSum += invAngle;
                refMain[k] = refSide[(invAngleSum >> 8) * 2];
                refMain[k + 1] = refSide[(invAngleSum >> 8) * 2 + 1];
            }
        }
        else
        {
            for (k = 0; k < (2 * blkSize + 1) * 2; k += 2)
            {
                refAbove[k] = pSrc[k - srcStride - 2];
                refAbove[k + 1] = pSrc[k - srcStride - 2 + 1];
            }
            for (k = 0; k < 2 * blkSize + 1; k++)
            {
                refLeft[k * 2] = pSrc[(k - 1) * srcStride - 2];
                refLeft[k * 2 + 1] = pSrc[(k - 1) * srcStride - 2 + 1];
            }
            refMain = modeVer ? refAbove : refLeft;
            refSide = modeVer ? refLeft  : refAbove;
        }

        if (intraPredAngle == 0)
        {
            for (k = 0; k < blkSize; k++)
            {
                for (l = 0; l < blkSize * 2; l += 2)
                {
                    pDst[k * dstStride + l] = refMain[l + 2];
                    pDst[k * dstStride + l + 1] = refMain[l + 2 + 1];
                }
            }
        }
        else
        {
            Ipp32s deltaPos = 0;
            Ipp32s deltaInt;
            Ipp32s deltaFract;
            Ipp32s refMainIndex;

            for (k = 0; k < blkSize; k++)
            {
                deltaPos += intraPredAngle;
                deltaInt = deltaPos >> 5;
                deltaFract = deltaPos & (32 - 1);

                if (deltaFract)
                {
                    // Do linear filtering
                    for (l = 0; l < blkSize * 2; l += 2)
                    {
                        refMainIndex = l + (deltaInt + 1) * 2;
                        pDst[k * dstStride + l] = (H265PlaneUVCommon)(((32 - deltaFract) * refMain[refMainIndex] + deltaFract * refMain[refMainIndex + 2] + 16) >> 5);
                        pDst[k * dstStride + l + 1] = (H265PlaneUVCommon)(((32 - deltaFract) * refMain[refMainIndex + 1] + deltaFract * refMain[refMainIndex + 2 + 1] + 16) >> 5);
                    }
                }
                else
                {
                    // Just copy the integer samples
                    for (l = 0; l < blkSize * 2; l += 2)
                    {
                        pDst[k * dstStride + l] = refMain[l + deltaInt * 2 + 2];
                        pDst[k * dstStride + l + 1] = refMain[l + deltaInt * 2 + 2 + 1];
                    }
                }
            }
        }

        // Flip the block if this is the horizontal mode
        if (modeHor)
        {
            H265PlaneUVCommon tmp;
            for (k = 0; k < blkSize - 1; k++)
            {
                for (l = (k + 1) * 2; l < blkSize * 2; l += 2)
                {
                    tmp = pDst[k * dstStride + l];
                    pDst[k * dstStride + l] = pDst[l * dstStrideHalf + k * 2];
                    pDst[l * dstStrideHalf + k * 2] = tmp;

                    tmp = pDst[k * dstStride + l + 1];
                    pDst[k * dstStride + l + 1] = pDst[l * dstStrideHalf + k * 2 + 1];
                    pDst[l * dstStrideHalf + k * 2 + 1] = tmp;
                }
            }
        }
    }
}

void H265Prediction::DCPredFiltering(H265PlanePtrYCommon pSrc, Ipp32s SrcStride, H265PlanePtrYCommon Dst, Ipp32s DstStride, Ipp32s Size)
{
    H265PlanePtrYCommon pDst = Dst;

    // boundary pixels processing
    pDst[0] = (H265PlaneYCommon)((pSrc[-SrcStride] + pSrc[-1] + 2 * pDst[0] + 2) >> 2);

    #pragma ivdep
    for (Ipp32s x = 1; x < Size; x++)
    {
        pDst[x] = (H265PlaneYCommon)((pSrc[x - SrcStride] + 3 * pDst[x] + 2) >> 2);
    }

    #pragma ivdep
    for (Ipp32s y = 1, DstStride2 = DstStride, SrcStride2 = SrcStride - 1; y < Size; y++, DstStride2 += DstStride, SrcStride2 += SrcStride)
    {
        pDst[DstStride2] = (H265PlaneYCommon)((pSrc[SrcStride2] + 3 * pDst[DstStride2] + 2) >> 2);
    }

    return;
}

// Angular chroma
void H265Prediction::PredIntraChromaAng(H265PlanePtrUVCommon pSrc, Ipp32u DirMode, H265PlanePtrUVCommon pPred, Ipp32u Stride, Ipp32s Size)
{
    H265PlanePtrUVCommon pDst = pPred;
    H265PlanePtrUVCommon ptrSrc = pSrc;

    // get starting pixel in block
    Ipp32s sw = 2 * Size + 1;
    sw *= 2;

    if (DirMode == PLANAR_IDX)
    {
        PredIntraPlanarChroma(ptrSrc + sw + 2, sw, pDst, Stride, Size);
    }
    else
    {
        // Create the prediction
        PredIntraAngChroma(g_bitDepthC, ptrSrc + sw + 2, sw, pDst, Stride, Size, DirMode);
    }
}

void H265Prediction::MotionCompensation(H265CodingUnit* pCU, H265DecYUVBufferPadded* pPredYUV, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);
    bool weighted_prediction = pCU->m_SliceHeader->slice_type == P_SLICE ? pCU->m_SliceHeader->m_PicParamSet->weighted_pred_flag :
        pCU->m_SliceHeader->m_PicParamSet->weighted_bipred_idc;

    for (Ipp32s PartIdx = 0; PartIdx < pCU->getNumPartInter(AbsPartIdx); PartIdx++)
    {
        Ipp32u PartAddr;
        Ipp32u Width, Height;
        pCU->getPartIndexAndSize(AbsPartIdx, Depth, PartIdx, PartAddr, Width, Height);
        PartAddr += AbsPartIdx;

        Ipp32s RefIdx[2] = {-1, -1};
        RefIdx[REF_PIC_LIST_0] = pCU->m_CUMVbuffer[REF_PIC_LIST_0].RefIdx[PartAddr];
        RefIdx[REF_PIC_LIST_1] = pCU->m_CUMVbuffer[REF_PIC_LIST_1].RefIdx[PartAddr];

        VM_ASSERT(RefIdx[REF_PIC_LIST_0] >= 0 || RefIdx[REF_PIC_LIST_1] >= 0);

        if ((CheckIdenticalMotion(pCU, PartAddr) || !(RefIdx[REF_PIC_LIST_0] >= 0 && RefIdx[REF_PIC_LIST_1] >= 0)))
        {
            EnumRefPicList direction = RefIdx[REF_PIC_LIST_0] >= 0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
            if ( ! weighted_prediction )
            {
                PredInterUni<TEXT_LUMA, false>(pCU, PartAddr, Width, Height, direction, &m_YUVPred[0]);
                PredInterUni<TEXT_CHROMA, false>(pCU, PartAddr, Width, Height, direction, &m_YUVPred[0]);
            }
            else
            {
                PredInterUni<TEXT_LUMA, true>(pCU, PartAddr, Width, Height, direction, &m_YUVPred[direction]);
                PredInterUni<TEXT_CHROMA, true>(pCU, PartAddr, Width, Height, direction, &m_YUVPred[direction]);
            }
        }
        else
        {
            bool bOnlyOneIterpY = false, bOnlyOneIterpC = false;
            EnumRefPicList picListY = REF_PIC_LIST_0, picListC = REF_PIC_LIST_0;

            if ( ! weighted_prediction )
            {
                // Check if at least one ref doesn't require subsample interpolation to add average directly from pic
                H265MotionVector MV = pCU->m_CUMVbuffer[REF_PIC_LIST_0].MV[PartAddr];
                int mv_interp0 = (MV.Horizontal | MV.Vertical) & 7;

                MV = pCU->m_CUMVbuffer[REF_PIC_LIST_1].MV[PartAddr];
                int mv_interp1 = (MV.Horizontal | MV.Vertical) & 7;

                bOnlyOneIterpC = !( mv_interp0 && mv_interp1 );
                if (mv_interp0 == 0)
                    picListC = REF_PIC_LIST_1;

                mv_interp0 &= 3;
                mv_interp1 &= 3;
                bOnlyOneIterpY = !( mv_interp0 && mv_interp1 );
                if (mv_interp0 == 0)
                    picListY = REF_PIC_LIST_1;
            }

            EnumAddAverageType addAverage = weighted_prediction ? AVERAGE_NO : AVERAGE_FROM_BUF;
            H265DecYUVBufferPadded* pYuvPred = &m_YUVPred[ weighted_prediction ? REF_PIC_LIST_1 : REF_PIC_LIST_0];

            if ( bOnlyOneIterpY )
                PredInterUni<TEXT_LUMA, true>( pCU, PartAddr, Width, Height, picListY, m_YUVPred, AVERAGE_FROM_PIC );
            else
            {
                PredInterUni<TEXT_LUMA, true>( pCU, PartAddr, Width, Height, REF_PIC_LIST_0, &m_YUVPred[REF_PIC_LIST_0] );
                PredInterUni<TEXT_LUMA, true>( pCU, PartAddr, Width, Height, REF_PIC_LIST_1, pYuvPred, addAverage );
            }

            if ( bOnlyOneIterpC )
                PredInterUni<TEXT_CHROMA, true>( pCU, PartAddr, Width, Height, picListC, m_YUVPred, AVERAGE_FROM_PIC );
            else
            {
                PredInterUni<TEXT_CHROMA, true>( pCU, PartAddr, Width, Height, REF_PIC_LIST_0, &m_YUVPred[REF_PIC_LIST_0] );
                PredInterUni<TEXT_CHROMA, true>( pCU, PartAddr, Width, Height, REF_PIC_LIST_1, pYuvPred, addAverage );
            }
        }

        if (weighted_prediction)
        {
            Ipp32s w0[3], w1[3], o0[3], o1[3], logWD[3], round[3];

            for (Ipp32s plane = 0; plane < 3; plane++)
            {
                Ipp32s bitDepth = plane ? g_bitDepthC : g_bitDepthY;

                w0[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iWeight;
                w1[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_1][RefIdx[1] >=0 ? RefIdx[1] : RefIdx[0]][plane].iWeight;
                o0[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iOffset * (1 << (bitDepth - 8));
                o1[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_1][RefIdx[1] >=0 ? RefIdx[1] : RefIdx[0]][plane].iOffset * (1 << (bitDepth - 8));

                if (RefIdx[0] >= 0)
                    logWD[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0]][plane].uiLog2WeightDenom;
                else
                    logWD[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_1][RefIdx[1]][plane].uiLog2WeightDenom;

                logWD[plane] += 14 - bitDepth;
                round[plane] = 0;

                if (logWD[plane] >= 1)
                    round[plane] = 1 << (logWD[plane] - 1);
                else
                    logWD[plane] = 0;
            }

            if (RefIdx[0] >= 0 && RefIdx[1] < 0)
                pPredYUV->CopyWeighted_S16U8(&m_YUVPred[0], PartAddr, Width, Height, w0, o0, logWD, round);
            else if (RefIdx[0] < 0 && RefIdx[1] >= 0)
                pPredYUV->CopyWeighted_S16U8(&m_YUVPred[1], PartAddr, Width, Height, w1, o1, logWD, round);
            else
            {
                for (Ipp32s plane = 0; plane < 3; plane++)
                {
                    logWD[plane] += 1;
                    round[plane] = (o0[plane] + o1[plane] + 1) << (logWD[plane] - 1);
                }
                pPredYUV->CopyWeightedBidi_S16U8(&m_YUVPred[0], &m_YUVPred[1], PartAddr, Width, Height, w0, w1, logWD, round);
            }

            pPredYUV->CopyPartToPic(pCU->m_Frame, pCU->CUAddr, PartAddr, Width, Height);
        }
    }
}

bool H265Prediction::CheckIdenticalMotion(H265CodingUnit* pCU, Ipp32u PartAddr)
{
    if(pCU->m_SliceHeader->slice_type == B_SLICE && !pCU->m_SliceHeader->m_PicParamSet->getWPBiPred())
    {
        if(pCU->m_CUMVbuffer[REF_PIC_LIST_0].RefIdx[PartAddr] >= 0 && pCU->m_CUMVbuffer[REF_PIC_LIST_1].RefIdx[PartAddr] >= 0)
        {
            Ipp32s RefPOCL0 = pCU->m_Frame->GetRefPicList(pCU->m_SliceIdx, REF_PIC_LIST_0)->m_refPicList[pCU->m_CUMVbuffer[REF_PIC_LIST_0].RefIdx[PartAddr]].refFrame->m_PicOrderCnt;
            Ipp32s RefPOCL1 = pCU->m_Frame->GetRefPicList(pCU->m_SliceIdx, REF_PIC_LIST_1)->m_refPicList[pCU->m_CUMVbuffer[REF_PIC_LIST_1].RefIdx[PartAddr]].refFrame->m_PicOrderCnt;
            if(RefPOCL0 == RefPOCL1 && pCU->m_CUMVbuffer[REF_PIC_LIST_0].MV[PartAddr] == pCU->m_CUMVbuffer[REF_PIC_LIST_1].MV[PartAddr])
            {
                return true;
            }
        }
    }
    return false;
}

template <EnumTextType c_plane_type >
static void PrepareInterpSrc( H265CodingUnit* pCU, Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred,
                              IppVCInterpolateBlock_8u& interpolateInfo, Ipp8u* temp_interpolarion_buffer )
{
    Ipp32s RefIdx = pCU->m_CUMVbuffer[RefPicList].RefIdx[PartAddr];
    VM_ASSERT(RefIdx >= 0);
    YUVPred;

    H265MotionVector MV = pCU->m_CUMVbuffer[RefPicList].MV[PartAddr];
    H265DecoderFrame *PicYUVRef = pCU->m_Frame->GetRefPicList(pCU->m_SliceIdx, RefPicList)->m_refPicList[RefIdx].refFrame;

    Ipp32s in_SrcPitch = (c_plane_type == TEXT_CHROMA) ? PicYUVRef->pitch_chroma() : PicYUVRef->pitch_luma();

    Ipp32s refOffset = (c_plane_type == TEXT_CHROMA) ? 
                            (MV.Horizontal >> 3) * 2 + (MV.Vertical >> 3) * in_SrcPitch :
                            (MV.Horizontal >> 2) + (MV.Vertical >> 2) * in_SrcPitch;

    H265PlanePtrYCommon in_pSrc = (c_plane_type == TEXT_CHROMA) ?
                            PicYUVRef->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) + refOffset :
                            PicYUVRef->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) + refOffset;

    interpolateInfo.pSrc[0] = (c_plane_type == TEXT_CHROMA) ? (const Ipp8u*)PicYUVRef->m_pUVPlane : (const Ipp8u*)PicYUVRef->m_pYPlane;
    interpolateInfo.srcStep = in_SrcPitch;
    interpolateInfo.pointVector.x= MV.Horizontal;
    interpolateInfo.pointVector.y= MV.Vertical;

    Ipp32u block_offset = (Ipp32u)( ( (c_plane_type == TEXT_CHROMA) ?
                                      PicYUVRef->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) :
                                      PicYUVRef->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) )
                          - interpolateInfo.pSrc[0]);

    // ML: TODO: make sure compiler generates only one division
    interpolateInfo.pointBlockPos.x = block_offset % in_SrcPitch;
    interpolateInfo.pointBlockPos.y = block_offset / in_SrcPitch;

    interpolateInfo.sizeBlock.width = Width;
    interpolateInfo.sizeBlock.height = Height;

    interpolateInfo.sizeFrame.width = pCU->m_SliceHeader->m_SeqParamSet->pic_width_in_luma_samples;
    interpolateInfo.sizeFrame.height = pCU->m_SliceHeader->m_SeqParamSet->pic_height_in_luma_samples;

    if ( c_plane_type == TEXT_CHROMA )
    {
        interpolateInfo.pointBlockPos.x >>= 1;
        interpolateInfo.sizeFrame.width >>= 1;
        interpolateInfo.sizeFrame.height >>= 1;
    }

    IppStatus sts = ( c_plane_type == TEXT_CHROMA ) ? 
            ippiInterpolateChromaBlock_H264_8u(&interpolateInfo, temp_interpolarion_buffer) :
            ippiInterpolateLumaBlock_H265_8u(&interpolateInfo, temp_interpolarion_buffer);

    if (sts != ippStsNoOperation)
        interpolateInfo.srcStep = 128;
    else
    {
        interpolateInfo.pSrc[0] = in_pSrc;
        interpolateInfo.srcStep = in_SrcPitch;
    }
}

template <EnumTextType c_plane_type, bool c_bi>
void H265Prediction::PredInterUni(H265CodingUnit* pCU, Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, EnumAddAverageType eAddAverage )
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);
    // Hack to get correct offset in 2-byte elements
    Ipp32s in_DstPitch = (c_plane_type == TEXT_CHROMA) ? YUVPred->pitch_chroma() : YUVPred->pitch_luma();
    H265CoeffsPtrCommon in_pDst = (c_plane_type == TEXT_CHROMA) ? 
                            (H265CoeffsPtrCommon)YUVPred->GetCbCrAddr() + (YUVPred->GetPartCbCrAddr(PartAddr) - YUVPred->GetCbCrAddr()) :
                            (H265CoeffsPtrCommon)YUVPred->GetLumaAddr() + (YUVPred->GetPartLumaAddr(PartAddr) - YUVPred->GetLumaAddr());

    IppVCInterpolateBlock_8u interpolateSrc;
    PrepareInterpSrc< c_plane_type >( pCU, PartAddr, Width, Height, RefPicList, YUVPred, interpolateSrc, m_temp_interpolarion_buffer );
    const H265PlaneYCommon * in_pSrc = interpolateSrc.pSrc[0];
    Ipp32s in_SrcPitch = interpolateSrc.srcStep, in_SrcPic2Pitch;

    const H265PlaneYCommon * in_pSrcPic2;
    if ( eAddAverage == AVERAGE_FROM_PIC )
    {
        PrepareInterpSrc< c_plane_type >( pCU, PartAddr, Width, Height, (EnumRefPicList)(RefPicList ^ 1), YUVPred, interpolateSrc, m_temp_interpolarion_buffer + (128*128) );
        in_pSrcPic2 = interpolateSrc.pSrc[0];
        in_SrcPic2Pitch = interpolateSrc.srcStep;
    }

    Ipp32s bitDepth = ( c_plane_type == TEXT_CHROMA ) ? g_bitDepthC : g_bitDepthY;
    Ipp32s tap = ( c_plane_type == TEXT_CHROMA ) ? 4 : 8;
    Ipp32s shift = c_bi ? bitDepth - 8 : 6;
    Ipp32s  offset = c_bi ? 0 : 32;

    const Ipp32s low_bits_mask = ( c_plane_type == TEXT_CHROMA ) ? 7 : 3;
    H265MotionVector MV = pCU->m_CUMVbuffer[RefPicList].MV[PartAddr]; 
    Ipp32s in_dx = MV.Horizontal & low_bits_mask;
    Ipp32s in_dy = MV.Vertical & low_bits_mask;

    Ipp32s iPUWidth = Width;
    if ( c_plane_type == TEXT_CHROMA )
    {
        Width >>= 1;
        Height >>= 1;
    }

    Ipp32u PicDstStride = ( c_plane_type == TEXT_CHROMA ) ? pCU->m_Frame->pitch_chroma() : pCU->m_Frame->pitch_luma();
    H265PlanePtrYCommon pPicDst = ( c_plane_type == TEXT_CHROMA ) ? 
                pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU) + GetAddrOffset(PartAddr, PicDstStride >> 1) :
                pCU->m_Frame->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU) + GetAddrOffset(PartAddr, PicDstStride);

    if ((in_dx == 0) && (in_dy == 0))
    {
        if ( ! c_bi )
        {
            VM_ASSERT( eAddAverage == AVERAGE_NO );

            const H265PlaneYCommon * pSrc = in_pSrc;
            for (Ipp32s j = 0; j < Height; j++)
            {
                small_memcpy( pPicDst, pSrc, iPUWidth );
                pSrc += in_SrcPitch;
                pPicDst += PicDstStride;
            }
        }
        else
        {
            if ( eAddAverage == AVERAGE_FROM_PIC )
                WriteAverageToPic( in_pSrc, in_SrcPitch, in_pSrcPic2, in_SrcPic2Pitch, pPicDst, PicDstStride, iPUWidth, Height );
            else if ( eAddAverage == AVERAGE_FROM_BUF )
                VM_ASSERT(0); // must be handled by AVERAGE_FROM_PIC
            else // weighted prediction still requires intermediate copies
            {
                const int c_shift = 14 - g_bitDepthY;
                if ( c_plane_type == TEXT_CHROMA ) 
                    CopyPUChroma< c_shift >(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, Width, Height);
                else
                    CopyPULuma< c_shift >(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, Width, Height);
            }
        }
    }
    else if (in_dy == 0)
    {
        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset);
        else if (eAddAverage == AVERAGE_NO)
            Interpolate<c_plane_type>( INTERP_HOR, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dx, Width, Height, shift, offset);
        else if (eAddAverage == AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset, AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset, AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
    else if (in_dx == 0)
    {
        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset);
        else if (eAddAverage == AVERAGE_NO)
            Interpolate<c_plane_type>( INTERP_VER, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy, Width, Height, shift, offset);
        else if (eAddAverage == AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset, AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset, AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];
        Ipp32u tmpStride = iPUWidth + tap;

        shift = c_bi ? 6 : 20 - bitDepth;
        offset = c_bi ? 0 : 1 << (19 - bitDepth);

        Interpolate<c_plane_type>( INTERP_HOR, 
                                   in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, tmpStride,
                                   in_dx, Width, Height + tap - 1, bitDepth - 8, 0);

        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset);
        else if (eAddAverage == AVERAGE_NO)
            Interpolate<c_plane_type>( INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, in_pDst, in_DstPitch,
                                       in_dy, Width, Height, shift, offset );
        else if (eAddAverage == AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset, AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset, AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
}


//=================================================================================================
const Ipp16s g_lumaInterpolateFilter8X[3][8 * 8] =
{
    {   
      -1, -1, -1, -1, -1, -1, -1, -1, 
       4,  4,  4,  4,  4,  4,  4,  4,
     -10,-10,-10,-10,-10,-10,-10,-10,
      58, 58, 58, 58, 58, 58, 58, 58, 
      17, 17, 17, 17, 17, 17, 17, 17, 
      -5, -5, -5, -5, -5, -5, -5, -5, 
       1,  1,  1,  1,  1,  1,  1,  1,
       0,  0,  0,  0,  0,  0,  0,  0 
    },
    {
      -1, -1, -1, -1, -1, -1, -1, -1, 
       4,  4,  4,  4,  4,  4,  4,  4,
     -11,-11,-11,-11,-11,-11,-11,-11,
      40, 40, 40, 40, 40, 40, 40, 40,
      40, 40, 40, 40, 40, 40, 40, 40,
     -11,-11,-11,-11,-11,-11,-11,-11,
       4,  4,  4,  4,  4,  4,  4,  4,
      -1, -1, -1, -1, -1, -1, -1, -1 
    },
    {
       0,  0,  0,  0,  0,  0,  0,  0,
       1,  1,  1,  1,  1,  1,  1,  1,
      -5, -5, -5, -5, -5, -5, -5, -5, 
      17, 17, 17, 17, 17, 17, 17, 17, 
      58, 58, 58, 58, 58, 58, 58, 58, 
     -10,-10,-10,-10,-10,-10,-10,-10,
       4,  4,  4,  4,  4,  4,  4,  4,
      -1, -1, -1, -1, -1, -1, -1, -1 
    }
};


// ML: OPT: Doubled length of the filter to process both U and V at once
const Ipp16s g_chromaInterpolateFilter8X[7][4 * 8] =
{
    {
      -2, -2, -2, -2, -2, -2, -2, -2, 
      58, 58, 58, 58, 58, 58, 58, 58, 
      10, 10, 10, 10, 10, 10, 10, 10, 
      -2, -2, -2, -2, -2, -2, -2, -2 
    },
    { -4, -4, -4, -4, -4, -4, -4, -4, 
      54, 54, 54, 54, 54, 54, 54, 54, 
      16, 16, 16, 16, 16, 16, 16, 16, 
      -2, -2, -2, -2, -2, -2, -2, -2 
    },
    { -6, -6, -6, -6, -6, -6, -6, -6, 
      46, 46, 46, 46, 46, 46, 46, 46,
      28, 28, 28, 28, 28, 28, 28, 28,
      -4, -4, -4, -4, -4, -4, -4, -4
    },
    { -4, -4, -4, -4, -4, -4, -4, -4, 
      36, 36, 36, 36, 36, 36, 36, 36, 
      36, 36, 36, 36, 36, 36, 36, 36, 
      -4, -4, -4, -4, -4, -4, -4, -4
    },
    { -4, -4, -4, -4, -4, -4, -4, -4, 
      28, 28, 28, 28, 28, 28, 28, 28,
      46, 46, 46, 46, 46, 46, 46, 46,
      -6, -6, -6, -6, -6, -6, -6, -6 
    },
    { -2, -2, -2, -2, -2, -2, -2, -2, 
      16, 16, 16, 16, 16, 16, 16, 16, 
      54, 54, 54, 54, 54, 54, 54, 54, 
      -4, -4, -4, -4, -4, -4, -4, -4
    },
    { -2, -2, -2, -2, -2, -2, -2, -2, 
      10, 10, 10, 10, 10, 10, 10, 10, 
      58, 58, 58, 58, 58, 58, 58, 58, 
      -2, -2, -2, -2, -2, -2, -2, -2 
    }
};

template <typename> class upconvert_int;
template <> class upconvert_int<Ipp8u>  { public: typedef Ipp16s  result; };
template <> class upconvert_int<Ipp16s> { public: typedef Ipp32s  result; };

//=================================================================================================
// general template for Interpolate kernel
template
< 
    typename     t_vec, 
    EnumTextType c_plane_type,
    typename     t_src, 
    typename     t_dst
>
class t_InterpKernel_intrin
{
public:
    static void func(
        t_dst* H265_RESTRICT pDst, 
        const t_src* pSrc, 
        int    in_SrcPitch, // in samples
        int    in_DstPitch, // in samples
        int    width,
        int    height,
        int    accum_pitch,
        int    tab_index,
        int    shift,
        int    offset,
        H265Prediction::EnumAddAverageType eAddAverage = H265Prediction::AVERAGE_NO,
        const void* in_pSrc2 = NULL,
        int   in_Src2Pitch = 0 // in samples
    );
};

//=================================================================================================
// partioal specialization for __m128i; TODO: add __m256i version for AVX2 + dispatch
// NOTE: always reads a block with a width extended to a multiple of 8
template
< 
    EnumTextType c_plane_type,
    typename     t_src, 
    typename     t_dst
>
class t_InterpKernel_intrin< __m128i, c_plane_type, t_src, t_dst >
{
    typedef __m128i t_vec;

public:
    static void func(
        t_dst* H265_RESTRICT pDst, 
        const t_src* pSrc, 
        int    in_SrcPitch, // in samples
        int    in_DstPitch, // in samples
        int    width,
        int    height,
        int    accum_pitch,
        int    tab_index,
        int    shift,
        int    offset,
        H265Prediction::EnumAddAverageType eAddAverage,
        const void* in_pSrc2,
        int   in_Src2Pitch // in samples
    )
    {
        typedef typename upconvert_int< t_src >::result t_acc;
        const int c_tap = (c_plane_type == TEXT_LUMA) ? 8 : 4;

        const Ipp16s* coeffs8x = (c_plane_type == TEXT_LUMA) 
            ? g_lumaInterpolateFilter8X[tab_index - 1] 
            : g_chromaInterpolateFilter8X[tab_index - 1];

        t_vec v_offset = _mm_cvtsi32_si128( sizeof(t_acc)==4 ? offset : (offset << 16) | offset );
        v_offset = _mm_shuffle_epi32( v_offset, 0 ); // broadcast
        in_Src2Pitch *= (eAddAverage == H265Prediction::AVERAGE_FROM_BUF ? 2 : 1);

        for (int i, j = 0; j < height; ++j) 
        {
            t_dst* pDst_ = pDst;
            const Ipp8u* pSrc2 = (const Ipp8u*)in_pSrc2;
            t_vec  v_acc;

            _mm_prefetch( (const char*)(pSrc + in_SrcPitch), _MM_HINT_NTA ); 
            #pragma ivdep
            #pragma nounroll
            for (i = 0; i < width; i += 8, pDst_ += 8 )
            {
                t_vec v_acc2 = _mm_setzero_si128(); v_acc = _mm_setzero_si128();
                const Ipp16s* coeffs = coeffs8x;
                const t_src*  pSrc_ = pSrc + i;

                #pragma unroll(c_tap)
                for (int k = 0; k < c_tap; ++k )
                {
                    t_vec v_coeff = _mm_loadu_si128( k + ( const __m128i* )coeffs );

                    if (sizeof(t_src) == 1) // 8-bit source, 16-bit accum [check is resolved/eliminated at compile time]
                    {
                        t_vec v_chunk = _mm_cvtepu8_epi16( MM_LOAD_EPI64(pSrc_) );
                        v_chunk = _mm_mullo_epi16( v_chunk, v_coeff );
                        v_acc = _mm_add_epi16( v_acc, v_chunk );
                    }
                    else // (sizeof(t_src)==2  // 16-bit source, 32-bit accum 
                    {
                        t_vec v_chunk = _mm_loadu_si128( (const t_vec*)pSrc_ );

                        t_vec v_chunk_h = _mm_mulhi_epi16( v_chunk, v_coeff );
                        v_chunk = _mm_mullo_epi16( v_chunk, v_coeff );

                        t_vec v_lo = _mm_unpacklo_epi16( v_chunk, v_chunk_h );
                        t_vec v_hi = _mm_unpackhi_epi16( v_chunk, v_chunk_h );

                        v_acc  = _mm_add_epi32( v_acc,  v_lo );
                        v_acc2 = _mm_add_epi32( v_acc2, v_hi );
                    }

                    pSrc_ += accum_pitch;
                }

                if ( sizeof(t_acc) == 2 ) // resolved at compile time
                {
                    if ( offset ) // cmp/jmp is nearly free, branch prediction removes 1-instruction from critical dep chain
                        v_acc = _mm_add_epi16( v_acc, v_offset ); 

                    if ( shift == 6 )
                        v_acc = _mm_srai_epi16( v_acc, 6 );
                    else
                        VM_ASSERT(shift == 0);
                }
                else // if ( sizeof(t_acc) == 4 ) // 16-bit src, 32-bit accum
                {
                    if ( offset ) {
                        v_acc  = _mm_add_epi32( v_acc,  v_offset );
                        v_acc2 = _mm_add_epi32( v_acc2, v_offset );
                    }

                    if ( shift == 6 ) {
                        v_acc  = _mm_srai_epi32( v_acc, 6 );
                        v_acc2 = _mm_srai_epi32( v_acc2, 6 );
                    } 
                    else if ( shift == 12 ) {
                        v_acc  = _mm_srai_epi32( v_acc, 12 );
                        v_acc2 = _mm_srai_epi32( v_acc2, 12 );
                    }
                    else
                        VM_ASSERT(shift == 0);

                    v_acc = _mm_packs_epi32( v_acc, v_acc2 );
                }

                if ( eAddAverage != H265Prediction::AVERAGE_NO )
                {
                    if ( eAddAverage == H265Prediction::AVERAGE_FROM_PIC ) {
                        v_acc2 = _mm_cvtepu8_epi16( MM_LOAD_EPI64(pSrc2) );
                        pSrc2 += 8;
                        v_acc2 = _mm_slli_epi16( v_acc2, 6 );
                    }
                    else {
                        v_acc2 = _mm_loadu_si128( (const t_vec*)pSrc2 ); pSrc2 += 16;
                    }

                    v_acc2 = _mm_adds_epi16( v_acc2, _mm_set1_epi16( 1<<6 ) );
                    v_acc = _mm_adds_epi16( v_acc, v_acc2 );
                    v_acc = _mm_srai_epi16( v_acc, 7 );
                }

                if ( sizeof( t_dst ) == 1 )
                    v_acc = _mm_packus_epi16(v_acc, v_acc);

                if ( i + 8 > width )
                    break;

                if ( sizeof(t_dst) == 1 ) // 8-bit dest, check resolved at compile time
                    _mm_storel_epi64( (t_vec*)pDst_, v_acc );
                else
                    _mm_storeu_si128( (t_vec*)pDst_, v_acc );
            }

            int rem = (width & 7) * sizeof(t_dst);
            if ( rem )
            {
                if (rem > 7) {
                    rem -= 8;
                    _mm_storel_epi64( (t_vec*)pDst_, v_acc );
                    v_acc = _mm_srli_si128( v_acc, 8 );
                    pDst_ = (t_dst*)(8 + (Ipp8u*)pDst_);
                }
                if (rem > 3) {
                    rem -= 4;
                    *(Ipp32u*)(pDst_) = _mm_cvtsi128_si32( v_acc );
                    v_acc = _mm_srli_si128( v_acc, 4 );
                    pDst_ = (t_dst*)(4 + (Ipp8u*)pDst_);
                }
                if (rem > 1)
                    *(Ipp16u*)(pDst_) = (Ipp16u)_mm_cvtsi128_si32( v_acc );
            }

            pSrc += in_SrcPitch;
            pDst += in_DstPitch;
            in_pSrc2 = (const Ipp8u*)in_pSrc2 + in_Src2Pitch;
        }
    }
};

//=================================================================================================
template < EnumTextType plane_type, typename t_src, typename t_dst >
void H265_FORCEINLINE H265Prediction::Interpolate( 
                        EnumInterpType interp_type,
                        const t_src* in_pSrc,
                        Ipp32u in_SrcPitch, // in samples
                        t_dst* H265_RESTRICT in_pDst,
                        Ipp32u in_DstPitch, // in samples
                        Ipp32s tab_index,
                        Ipp32s width,
                        Ipp32s height,
                        Ipp32s shift,
                        Ipp16s offset,
                        H265Prediction::EnumAddAverageType eAddAverage,
                        const void* in_pSrc2,
                        int    in_Src2Pitch ) // in samples
{
    Ipp32s accum_pitch = ((interp_type == INTERP_HOR) ? (plane_type == TEXT_LUMA ? 1 : 2) : in_SrcPitch);

    const t_src* pSrc = in_pSrc - (((( plane_type == TEXT_LUMA) ? 8 : 4) >> 1) - 1) * accum_pitch;

    width <<= int(plane_type == TEXT_CHROMA);

    t_InterpKernel_intrin< __m128i, plane_type, t_src, t_dst >::func( in_pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, tab_index, shift, offset, eAddAverage, in_pSrc2, in_Src2Pitch );
}

//=================================================================================================
void H265Prediction::WriteAverageToPic(
                const H265PlaneYCommon * pSrc0,
                Ipp32u in_Src0Pitch,      // in samples
                const H265PlaneYCommon * pSrc1,
                Ipp32u in_Src1Pitch,      // in samples
                H265PlaneYCommon* H265_RESTRICT pDst,
                Ipp32u in_DstPitch,       // in samples
                Ipp32s width,
                Ipp32s height )
{
    #pragma unroll(2)
    #pragma ivdep
    for (int j = 0; j < height; j++)
    {
        #pragma ivdep
        #pragma vector always
        for (int i = 0; i < width; i++)
             pDst[i] = (((Ipp16u)pSrc0[i] + (Ipp16u)pSrc1[i] + 1) >> 1);

        pSrc0 += in_Src0Pitch;
        pSrc1 += in_Src1Pitch;
        pDst += in_DstPitch;
    }
}

/* ****************************************************************************** *\
FUNCTION: CopyPU
DESCRIPTION:
\* ****************************************************************************** */

// ML: OPT: TODO: Parameterize for const shift
template <int c_shift>
void H265Prediction::CopyPULuma(const H265PlaneYCommon * in_pSrc,
                                Ipp32u in_SrcPitch, // in samples
                                Ipp16s* H265_RESTRICT in_pDst,
                                Ipp32u in_DstPitch, // in samples
                                Ipp32s width,
                                Ipp32s height )
{
    const H265PlaneYCommon * pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    Ipp32s i, j;

#pragma ivdep
    for (j = 0; j < height; j++)
    {
#pragma vector always
        for (i = 0; i < width; i++)
        {
            pDst[i] = (Ipp16s)(((Ipp32s)pSrc[i]) << c_shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

template <int c_shift>
void H265Prediction::CopyPUChroma(const H265PlaneUVCommon * in_pSrc,
                                  Ipp32u in_SrcPitch, // in samples
                                  Ipp16s* H265_RESTRICT in_pDst,
                                  Ipp32u in_DstPitch, // in samples
                                  Ipp32s width,
                                  Ipp32s height)
{
    const H265PlaneUVCommon * pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    Ipp32s i, j;

#pragma ivdep
    for (j = 0; j < height; j++)
    {
#pragma vector always
        for (i = 0; i < width * 2; i++)
        {
            pDst[i] = (Ipp16s)(((Ipp32s)pSrc[i]) << c_shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
