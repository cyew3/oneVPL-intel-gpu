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

#include "h265_prediction.h"
#include "umc_h265_dec_ipplevel.h"
#include "umc_h265_frame_info.h"

using namespace MFX_HEVC_COMMON;

namespace UMC_HEVC_DECODER
{
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / destructor / initialize
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

H265Prediction::H265Prediction()
{
    m_YUVExt = 0;
    m_temp_interpolarion_buffer = 0;
    m_context = 0;
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

void H265Prediction::InitTempBuff(DecodingContext* context)
{
    m_context = context;
    const H265SeqParamSet * sps = m_context->m_sps;

    if (m_YUVExt && m_YUVExtHeight == ((sps->MaxCUHeight + 2) << 4) && 
        m_YUVExtStride == ((sps->MaxCUWidth  + 8) << 4))
        return;

    if (m_YUVExt)
    {
        delete[] m_YUVExt;

        m_YUVPred[0].destroy();
        m_YUVPred[1].destroy();
    }

    // ML: OPT: TODO: Allocate only when we need it
    m_YUVExtHeight = ((sps->MaxCUHeight + 2) << 4);
    m_YUVExtStride = ((sps->MaxCUWidth  + 8) << 4);
    m_YUVExt = new H265PlaneYCommon[m_YUVExtStride * m_YUVExtHeight];

    // new structure
    m_YUVPred[0].create(sps->MaxCUWidth, sps->MaxCUHeight, sizeof(Ipp16s), sizeof(Ipp16s));
    m_YUVPred[1].create(sps->MaxCUWidth, sps->MaxCUHeight, sizeof(Ipp16s), sizeof(Ipp16s));

    if (!m_temp_interpolarion_buffer)
        m_temp_interpolarion_buffer = ippsMalloc_8u(2*128*128);    
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public member functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const Ipp8u g_IntraFilter[5] =
{
    10, //4x4
    7, //8x8
    1, //16x16
    0, //32x32
    10, //64x64
};

H265PlanePtrYCommon H265Prediction::GetPredictorPtr(Ipp32u DirMode, Ipp32u log2BlkSize, H265PlanePtrYCommon pAdiBuf)
{
    H265PlanePtrYCommon pSrc;
    VM_ASSERT(log2BlkSize >= 2 && log2BlkSize < 7);
    Ipp32s diff = IPP_MIN((abs((Ipp32s) DirMode - HOR_IDX)), (abs((Ipp32s)DirMode - VER_IDX)));
    Ipp8u FiltIdx = diff > (Ipp32s)g_IntraFilter[log2BlkSize - 2] ? 1 : 0;
    if (DirMode == DC_IDX)
    {
        FiltIdx = 0; //no smoothing for DC or LM chroma
    }
    VM_ASSERT(FiltIdx <= 1);

    pSrc = pAdiBuf;

    if (FiltIdx)
    {
        Ipp32s size = (2 << log2BlkSize) + 1;
        pSrc += size * size;
    }

    return pSrc;
}

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

void H265Prediction::PredIntraLumaAng(Ipp32u DirMode, H265PlanePtrYCommon pPred, Ipp32u Stride, Ipp32s Size)
{
    H265PlanePtrYCommon pDst = pPred;
    H265PlanePtrYCommon pSrc;

    VM_ASSERT(g_ConvertToBit[Size] >= 0); //   4x  4
    VM_ASSERT(g_ConvertToBit[Size] <= 5); // 128x128

    pSrc = GetPredictorPtr(DirMode, g_ConvertToBit[Size] + 2, m_YUVExt);

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
#ifdef __INTEL_COMPILER
                // ML : OPT: TODO: vectorize, replace 'maxVals' with constant to allow for PACKS usage
                #pragma ivdep
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

        // ML: TODO: ICC generates bad seq with PEXTR instead of vectorizing properly
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

#ifdef __INTEL_COMPILER
    #pragma ivdep
#endif
    for (Ipp32s x = 1; x < Size; x++)
    {
        pDst[x] = (H265PlaneYCommon)((pSrc[x - SrcStride] + 3 * pDst[x] + 2) >> 2);
    }

#ifdef __INTEL_COMPILER
    #pragma ivdep
#endif
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

void H265Prediction::MotionCompensation(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, H265PUInfo *PUInfo)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);
    bool weighted_prediction = pCU->m_SliceHeader->slice_type == P_SLICE ? m_context->m_pps->weighted_pred_flag :
        m_context->m_pps->weighted_bipred_idc;

    Ipp32s countPart = pCU->getNumPartInter(AbsPartIdx);
    EnumPartSize PartSize = pCU->GetPartitionSize(AbsPartIdx);
    Ipp32u PUOffset = (g_PUOffset[Ipp32u(PartSize)] << ((m_context->m_sps->MaxCUDepth - Depth) << 1)) >> 4;

    for (Ipp32s PartIdx = 0, subPartIdx = AbsPartIdx; PartIdx < countPart; PartIdx++, subPartIdx += PUOffset)
    {
#if 1
        Ipp32u PartAddr;
        Ipp32u Width, Height;
        pCU->getPartIndexAndSize(AbsPartIdx, Depth, PartIdx, PartAddr, Width, Height);
        PartAddr += AbsPartIdx;

        Ipp32s LPelX = pCU->m_CUPelX + pCU->m_rasterToPelX[subPartIdx];
        Ipp32s TPelY = pCU->m_CUPelY + pCU->m_rasterToPelY[subPartIdx];
        Ipp32s PartX = LPelX >> m_context->m_sps->log2_min_transform_block_size;
        Ipp32s PartY = TPelY >> m_context->m_sps->log2_min_transform_block_size;
#endif
        H265PUInfo &PUi = PUInfo[PartIdx];
        H265MVInfo &MVi = PUi.interinfo;

#if 1
        PUi.PartAddr = PartAddr;
        PUi.Height = Height;
        PUi.Width = Width;

        const H265FrameCodingData::ColocatedTUInfo &tuInfoL0 = m_context->m_frame->getCD()->GetTUInfo(REF_PIC_LIST_0, m_context->m_frame->getNumPartInCUSize() * m_context->m_frame->getFrameWidthInCU() * PartY + PartX);
        const H265FrameCodingData::ColocatedTUInfo &tuInfoL1 = m_context->m_frame->getCD()->GetTUInfo(REF_PIC_LIST_1, m_context->m_frame->getNumPartInCUSize() * m_context->m_frame->getFrameWidthInCU() * PartY + PartX);

        if (tuInfoL0.m_flags != COL_TU_INVALID_INTER)
        {
            PUi.refFrame[REF_PIC_LIST_0] = tuInfoL0.m_refIdx >= 0 ? m_context->m_refPicList[REF_PIC_LIST_0][tuInfoL0.m_refIdx].refFrame : 0;
            MVi.mvinfo[REF_PIC_LIST_0].MV = tuInfoL0.m_mv;
            MVi.mvinfo[REF_PIC_LIST_0].RefIdx = tuInfoL0.m_refIdx;
        }
        else
            MVi.mvinfo[REF_PIC_LIST_0].RefIdx = -1;


        if (tuInfoL1.m_flags != COL_TU_INVALID_INTER)
        {
            PUi.refFrame[REF_PIC_LIST_1] = tuInfoL1.m_refIdx >= 0 ? m_context->m_refPicList[REF_PIC_LIST_1][tuInfoL1.m_refIdx].refFrame : 0;
            MVi.mvinfo[REF_PIC_LIST_1].MV = tuInfoL1.m_mv;
            MVi.mvinfo[REF_PIC_LIST_1].RefIdx = tuInfoL1.m_refIdx;
        }
        else
            MVi.mvinfo[REF_PIC_LIST_1].RefIdx = -1;
#endif

        Ipp32s RefIdx[2] = {-1, -1};
        RefIdx[REF_PIC_LIST_0] = MVi.mvinfo[REF_PIC_LIST_0].RefIdx;
        RefIdx[REF_PIC_LIST_1] = MVi.mvinfo[REF_PIC_LIST_1].RefIdx;

        VM_ASSERT(RefIdx[REF_PIC_LIST_0] >= 0 || RefIdx[REF_PIC_LIST_1] >= 0);

        if ((CheckIdenticalMotion(pCU, PUInfo[PartIdx]) || !(RefIdx[REF_PIC_LIST_0] >= 0 && RefIdx[REF_PIC_LIST_1] >= 0)))
        {
            EnumRefPicList direction = RefIdx[REF_PIC_LIST_0] >= 0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
            if ( ! weighted_prediction )
            {
                PredInterUni<TEXT_LUMA, false>(pCU, PUi, direction, &m_YUVPred[0]);
                PredInterUni<TEXT_CHROMA, false>(pCU, PUi, direction, &m_YUVPred[0]);
            }
            else
            {
                PredInterUni<TEXT_LUMA, true>(pCU, PUi, direction, &m_YUVPred[0]);
                PredInterUni<TEXT_CHROMA, true>(pCU, PUi, direction, &m_YUVPred[0]);
            }
        }
        else
        {
            bool bOnlyOneIterpY = false, bOnlyOneIterpC = false;
            EnumRefPicList picListY = REF_PIC_LIST_0, picListC = REF_PIC_LIST_0;

            if ( ! weighted_prediction )
            {
                // Check if at least one ref doesn't require subsample interpolation to add average directly from pic
                H265MotionVector MV = MVi.mvinfo[REF_PIC_LIST_0].MV;
                int mv_interp0 = (MV.Horizontal | MV.Vertical) & 7;

                MV = MVi.mvinfo[REF_PIC_LIST_1].MV;
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

            MFX_HEVC_COMMON::EnumAddAverageType addAverage = weighted_prediction ? MFX_HEVC_COMMON::AVERAGE_NO : MFX_HEVC_COMMON::AVERAGE_FROM_BUF;
            H265DecYUVBufferPadded* pYuvPred = &m_YUVPred[ weighted_prediction ? REF_PIC_LIST_1 : REF_PIC_LIST_0];

            if ( bOnlyOneIterpY )
                PredInterUni<TEXT_LUMA, true>( pCU, PUi, picListY, m_YUVPred, MFX_HEVC_COMMON::AVERAGE_FROM_PIC );
            else
            {
                PredInterUni<TEXT_LUMA, true>( pCU, PUi, REF_PIC_LIST_0, &m_YUVPred[REF_PIC_LIST_0] );
                PredInterUni<TEXT_LUMA, true>( pCU, PUi, REF_PIC_LIST_1, pYuvPred, addAverage );
            }

            if ( bOnlyOneIterpC )
                PredInterUni<TEXT_CHROMA, true>( pCU, PUi, picListC, m_YUVPred, MFX_HEVC_COMMON::AVERAGE_FROM_PIC );
            else
            {
                PredInterUni<TEXT_CHROMA, true>( pCU, PUi, REF_PIC_LIST_0, &m_YUVPred[REF_PIC_LIST_0] );
                PredInterUni<TEXT_CHROMA, true>( pCU, PUi, REF_PIC_LIST_1, pYuvPred, addAverage );
            }
        }

        if (weighted_prediction)
        {
            Ipp32s w0[3], w1[3], o0[3], o1[3], logWD[3], round[3];
            Ipp32u PartAddr = PUi.PartAddr;
            Ipp32s Width = PUi.Width;
            Ipp32s Height = PUi.Height;

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

            if (RefIdx[0] >= 0 && RefIdx[1] >= 0)
            {
                for (Ipp32s plane = 0; plane < 3; plane++)
                {
                    logWD[plane] += 1;
                    round[plane] = (o0[plane] + o1[plane] + 1) << (logWD[plane] - 1);
                }
                CopyWeightedBidi_S16U8(pCU->m_Frame, &m_YUVPred[0], &m_YUVPred[1], pCU->CUAddr, PartAddr, Width, Height, w0, w1, logWD, round);
            }
            else if (RefIdx[0] >= 0)
                CopyWeighted_S16U8(pCU->m_Frame, &m_YUVPred[0], pCU->CUAddr, PartAddr, Width, Height, w0, o0, logWD, round);
            else
                CopyWeighted_S16U8(pCU->m_Frame, &m_YUVPred[0], pCU->CUAddr, PartAddr, Width, Height, w1, o1, logWD, round);
        }
    }
}

bool H265Prediction::CheckIdenticalMotion(H265CodingUnit* pCU, H265PUInfo &PUi)
{
    if(pCU->m_SliceHeader->slice_type == B_SLICE && !m_context->m_pps->getWPBiPred() &&
        PUi.interinfo.mvinfo[REF_PIC_LIST_0].RefIdx >= 0 && PUi.interinfo.mvinfo[REF_PIC_LIST_1].RefIdx >= 0 &&
        PUi.refFrame[REF_PIC_LIST_0] == PUi.refFrame[REF_PIC_LIST_1] &&
        PUi.interinfo.mvinfo[REF_PIC_LIST_0].MV == PUi.interinfo.mvinfo[REF_PIC_LIST_1].MV)
        return true;
    else
        return false;
}

template <EnumTextType c_plane_type >
static void PrepareInterpSrc( H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList,
                              IppVCInterpolateBlock_8u& interpolateInfo, Ipp8u* temp_interpolarion_buffer )
{
    VM_ASSERT(PUi.interinfo.mvinfo[RefPicList].RefIdx >= 0);

    Ipp32u PartAddr = PUi.PartAddr;
    Ipp32s Width = PUi.Width;
    Ipp32s Height = PUi.Height;
    H265MotionVector MV = PUi.interinfo.mvinfo[RefPicList].MV;
    H265DecoderFrame *PicYUVRef = PUi.refFrame[RefPicList];

    Ipp32s in_SrcPitch = (c_plane_type == TEXT_CHROMA) ? PicYUVRef->pitch_chroma() : PicYUVRef->pitch_luma();

    Ipp32s refOffset = (c_plane_type == TEXT_CHROMA) ? 
                            (MV.Horizontal >> 3) * 2 + (MV.Vertical >> 3) * in_SrcPitch :
                            (MV.Horizontal >> 2) + (MV.Vertical >> 2) * in_SrcPitch;

    H265PlanePtrYCommon in_pSrc = (c_plane_type == TEXT_CHROMA) ?
                            PicYUVRef->GetCbCrAddr(pCU->CUAddr, PartAddr) + refOffset :
                            PicYUVRef->GetLumaAddr(pCU->CUAddr, PartAddr) + refOffset;

    interpolateInfo.pSrc[0] = (c_plane_type == TEXT_CHROMA) ? (const Ipp8u*)PicYUVRef->m_pUVPlane : (const Ipp8u*)PicYUVRef->m_pYPlane;
    interpolateInfo.srcStep = in_SrcPitch;
    interpolateInfo.pointVector.x= MV.Horizontal;
    interpolateInfo.pointVector.y= MV.Vertical;

    Ipp32u block_offset = (Ipp32u)( ( (c_plane_type == TEXT_CHROMA) ?
                                      PicYUVRef->GetCbCrAddr(pCU->CUAddr, PartAddr) :
                                      PicYUVRef->GetLumaAddr(pCU->CUAddr, PartAddr) )
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
void H265Prediction::PredInterUni(H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, MFX_HEVC_COMMON::EnumAddAverageType eAddAverage )
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);
    Ipp32u PartAddr = PUi.PartAddr;
    Ipp32s Width = PUi.Width;
    Ipp32s Height = PUi.Height;

    // Hack to get correct offset in 2-byte elements
    Ipp32s in_DstPitch = (c_plane_type == TEXT_CHROMA) ? YUVPred->pitch_chroma() : YUVPred->pitch_luma();
    H265CoeffsPtrCommon in_pDst = (c_plane_type == TEXT_CHROMA) ? 
                            (H265CoeffsPtrCommon)YUVPred->m_pUVPlane + GetAddrOffset(PartAddr, YUVPred->chromaSize().width) :
                            (H265CoeffsPtrCommon)YUVPred->m_pYPlane + GetAddrOffset(PartAddr, YUVPred->lumaSize().width);

    IppVCInterpolateBlock_8u interpolateSrc;
    PrepareInterpSrc< c_plane_type >( pCU, PUi, RefPicList, interpolateSrc, m_temp_interpolarion_buffer );
    const H265PlaneYCommon * in_pSrc = interpolateSrc.pSrc[0];
    Ipp32s in_SrcPitch = interpolateSrc.srcStep, in_SrcPic2Pitch;

    const H265PlaneYCommon * in_pSrcPic2;
    if ( eAddAverage == MFX_HEVC_COMMON::AVERAGE_FROM_PIC )
    {
        PrepareInterpSrc< c_plane_type >( pCU, PUi, (EnumRefPicList)(RefPicList ^ 1), interpolateSrc, m_temp_interpolarion_buffer + (128*128) );
        in_pSrcPic2 = interpolateSrc.pSrc[0];
        in_SrcPic2Pitch = interpolateSrc.srcStep;
    }

    Ipp32s bitDepth = ( c_plane_type == TEXT_CHROMA ) ? g_bitDepthC : g_bitDepthY;
    Ipp32s tap = ( c_plane_type == TEXT_CHROMA ) ? 4 : 8;
    Ipp32s shift = c_bi ? bitDepth - 8 : 6;
    Ipp16s offset = c_bi ? 0 : 32;

    const Ipp32s low_bits_mask = ( c_plane_type == TEXT_CHROMA ) ? 7 : 3;
    H265MotionVector MV = PUi.interinfo.mvinfo[RefPicList].MV; 
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
                pCU->m_Frame->GetCbCrAddr(pCU->CUAddr) + GetAddrOffset(PartAddr, PicDstStride >> 1) :
                pCU->m_Frame->GetLumaAddr(pCU->CUAddr) + GetAddrOffset(PartAddr, PicDstStride);

    if ((in_dx == 0) && (in_dy == 0))
    {
        if ( ! c_bi )
        {
            VM_ASSERT( eAddAverage == MFX_HEVC_COMMON::AVERAGE_NO );

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
            if ( eAddAverage == MFX_HEVC_COMMON::AVERAGE_FROM_PIC )
                WriteAverageToPic( in_pSrc, in_SrcPitch, in_pSrcPic2, in_SrcPic2Pitch, pPicDst, PicDstStride, iPUWidth, Height );
            else if ( eAddAverage == MFX_HEVC_COMMON::AVERAGE_FROM_BUF )
                VM_ASSERT(0); // it should have been passed with AVERAGE_FROM_PIC in H265Prediction::MotionCompensation with the other block
            else // weighted prediction still requires intermediate copies
            {
                const int c_shift = 14 - g_bitDepthY;
                int copy_width = Width; 
                if (c_plane_type == TEXT_CHROMA) 
                    copy_width <<= 1;

                CopyExtendPU< c_shift >(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, copy_width, Height);
            }
        }
    }
    else if (in_dy == 0)
    {
        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset);
        else if (eAddAverage == MFX_HEVC_COMMON::AVERAGE_NO)
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_HOR, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dx, Width, Height, shift, offset);
        else if (eAddAverage == MFX_HEVC_COMMON::AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset, MFX_HEVC_COMMON::AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_HOR, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dx, Width, Height, shift, offset, MFX_HEVC_COMMON::AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
    else if (in_dx == 0)
    {
        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset);
        else if (eAddAverage == MFX_HEVC_COMMON::AVERAGE_NO)
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_VER, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy, Width, Height, shift, offset);
        else if (eAddAverage == MFX_HEVC_COMMON::AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset, MFX_HEVC_COMMON::AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_VER, in_pSrc, in_SrcPitch, pPicDst, PicDstStride, in_dy, Width, Height, shift, offset, MFX_HEVC_COMMON::AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];
        Ipp32u tmpStride = iPUWidth + tap;

        Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_HOR, 
                                   in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, tmpStride,
                                   in_dx, Width, Height + tap - 1, bitDepth - 8, 0);

        shift = c_bi ? 6 : 20 - bitDepth;
        offset = c_bi ? 0 : 1 << (19 - bitDepth);

        if (!c_bi) // Write directly into buffer
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset);
        else if (eAddAverage == MFX_HEVC_COMMON::AVERAGE_NO)
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, in_pDst, in_DstPitch,
                                       in_dy, Width, Height, shift, offset );
        else if (eAddAverage == MFX_HEVC_COMMON::AVERAGE_FROM_BUF)
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset, MFX_HEVC_COMMON::AVERAGE_FROM_BUF, in_pDst, in_DstPitch );
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<c_plane_type>( MFX_HEVC_COMMON::INTERP_VER,
                                       tmpBuf + ((tap >> 1) - 1) * tmpStride, tmpStride, pPicDst, PicDstStride,
                                       in_dy, Width, Height, shift, offset, MFX_HEVC_COMMON::AVERAGE_FROM_PIC, in_pSrcPic2, in_SrcPic2Pitch );
    }
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
void H265Prediction::CopyExtendPU(const H265PlaneYCommon * in_pSrc,
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

void H265Prediction::CopyWeighted_S16U8(H265DecoderFrame* frame, H265DecYUVBufferPadded* src, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round)
{
    H265CoeffsPtrCommon pSrc = (H265CoeffsPtrCommon)src->m_pYPlane + GetAddrOffset(PartIdx, src->lumaSize().width);
    H265CoeffsPtrCommon pSrcUV = (H265CoeffsPtrCommon)src->m_pUVPlane + GetAddrOffset(PartIdx, src->chromaSize().width);

    Ipp32u SrcStride = src->pitch_luma();
    Ipp32u DstStride = frame->pitch_luma();

    H265PlanePtrYCommon pDst = frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
    H265PlanePtrUVCommon pDstUV = frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> 1);

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (H265PlaneYCommon)ClipY(((w[0] * pSrc[x] + round[0]) >> logWD[0]) + o[0]);
        }
        pSrc += SrcStride;
        pDst += DstStride;
    }

    SrcStride = src->pitch_chroma();
    DstStride = frame->pitch_chroma();
    Width >>= 1;
    Height >>= 1;

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width * 2; x += 2)
        {
            pDstUV[x] = (H265PlaneUVCommon)ClipC(((w[1] * pSrcUV[x] + round[1]) >> logWD[1]) + o[1]);
            pDstUV[x + 1] = (H265PlaneUVCommon)ClipC(((w[2] * pSrcUV[x + 1] + round[2]) >> logWD[2]) + o[2]);
        }
        pSrcUV += SrcStride;
        pDstUV += DstStride;
    }
}

void H265Prediction::CopyWeightedBidi_S16U8(H265DecoderFrame* frame, H265DecYUVBufferPadded* src0, H265DecYUVBufferPadded* src1, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round)
{
    H265CoeffsPtrCommon pSrc0 = (H265CoeffsPtrCommon)src0->m_pYPlane + GetAddrOffset(PartIdx, src0->lumaSize().width);
    H265CoeffsPtrCommon pSrcUV0 = (H265CoeffsPtrCommon)src0->m_pUVPlane + GetAddrOffset(PartIdx, src0->chromaSize().width);

    H265CoeffsPtrCommon pSrc1 = (H265CoeffsPtrCommon)src1->m_pYPlane + GetAddrOffset(PartIdx, src1->lumaSize().width);
    H265CoeffsPtrCommon pSrcUV1 = (H265CoeffsPtrCommon)src1->m_pUVPlane + GetAddrOffset(PartIdx, src1->chromaSize().width);

    Ipp32u SrcStride0 = src0->pitch_luma();
    Ipp32u SrcStride1 = src1->pitch_luma();
    Ipp32u DstStride = frame->pitch_luma();

    H265PlanePtrYCommon pDst = frame->GetLumaAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride);
    H265PlanePtrUVCommon pDstUV = frame->GetCbCrAddr(CUAddr) + GetAddrOffset(PartIdx, DstStride >> 1);

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (H265PlaneYCommon)ClipY((w0[0] * pSrc0[x] + w1[0] * pSrc1[x] + round[0]) >> logWD[0]);
        }
        pSrc0 += SrcStride0;
        pSrc1 += SrcStride1;
        pDst += DstStride;
    }

    SrcStride0 = src0->pitch_chroma();
    SrcStride1 = src1->pitch_chroma();
    DstStride = frame->pitch_chroma();
    Width >>= 1;
    Height >>= 1;

    for (Ipp32u y = 0; y < Height; y++)
    {
        // ML: OPT: TODO: make sure it is vectorized with PACK
        for (Ipp32u x = 0; x < Width * 2; x += 2)
        {
            pDstUV[x] = (H265PlaneUVCommon)ClipC((w0[1] * pSrcUV0[x] + w1[1] * pSrcUV1[x] + round[1]) >> logWD[1]);
            pDstUV[x + 1] = (H265PlaneUVCommon)ClipC((w0[2] * pSrcUV0[x + 1] + w1[2] * pSrcUV1[x + 1] + round[2]) >> logWD[2]);
        }
        pSrcUV0 += SrcStride0;
        pSrcUV1 += SrcStride1;
        pDstUV += DstStride;
    }
}

Ipp32s H265Prediction::GetAddrOffset(Ipp32u PartUnitIdx, Ipp32u width)
{
    Ipp32s blkX = m_context->m_frame->getCD()->m_partitionInfo.m_rasterToPelX[PartUnitIdx];
    Ipp32s blkY = m_context->m_frame->getCD()->m_partitionInfo.m_rasterToPelY[PartUnitIdx];

    return blkX + blkY * width;
}
} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
