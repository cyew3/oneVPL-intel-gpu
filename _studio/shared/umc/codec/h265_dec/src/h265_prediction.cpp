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

//#define HEVC_OPT_CHANGES 8

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "h265_prediction.h"
#include "umc_h265_dec_ipplevel.h"

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

    if (!m_YUVExt)
    {
        delete[] m_YUVExt;

        m_YUVPred[0].destroy();
        m_YUVPred[1].destroy();
    }

    m_YUVExtHeight = ((g_MaxCUHeight + 2) << 4);
    m_YUVExtStride = ((g_MaxCUWidth  + 8) << 4);
    m_YUVExt = new H265PlaneYCommon[m_YUVExtStride * m_YUVExtHeight];

    // new structure
    m_YUVPred[0].create(g_MaxCUWidth, g_MaxCUHeight, sizeof(Ipp16s), sizeof(Ipp16s), g_MaxCUWidth, g_MaxCUHeight, g_MaxCUDepth);
    m_YUVPred[1].create(g_MaxCUWidth, g_MaxCUHeight, sizeof(Ipp16s), sizeof(Ipp16s), g_MaxCUWidth, g_MaxCUHeight, g_MaxCUDepth);

    if (!m_temp_interpolarion_buffer)
        m_temp_interpolarion_buffer = ippsMalloc_8u(128*128);    
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public member functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function for calculating DC value of the reference samples used in Intra prediction
H265PlaneYCommon H265Prediction::PredIntraGetPredValDC(H265PlanePtrYCommon pSrc, Ipp32s SrcStride, Ipp32s Width, Ipp32s Height)
{
    Ipp32s Ind;
    Ipp32s Sum = 0;
    H265PlaneYCommon DCVal;

    for (Ind = 0; Ind < Width; Ind++)
    {
        Sum += pSrc[Ind - SrcStride];
    }
    for (Ind = 0; Ind < Height; Ind++)
    {
        Sum += pSrc[Ind * SrcStride - 1];
    }

    DCVal = (H265PlaneYCommon) ((Sum + Width) / (Width + Height));

    return DCVal;
}

void H265Prediction::PredIntraLumaAng(H265Pattern* pPattern, Ipp32u DirMode, H265PlanePtrYCommon pPred, Ipp32u Stride, Ipp32s Width, Ipp32s Height)
{
    H265PlanePtrYCommon pDst = pPred;
    H265PlanePtrYCommon pSrc;

    VM_ASSERT(g_ConvertToBit[Width] >= 0); //   4x  4
    VM_ASSERT(g_ConvertToBit[Width] <= 5); // 128x128
    VM_ASSERT(Width == Height);

    pSrc = pPattern->GetPredictorPtr(DirMode, g_ConvertToBit[Width] + 2, m_YUVExt);

    // get starting pixel in block
    Ipp32s sw = 2 * Width + 1;

    // Create the prediction
    if (DirMode == PLANAR_IDX)
    {
        PredIntraPlanarLuma(pSrc + sw + 1, sw, pDst, Stride, Width, Height);
    }
    else
    {
        if ((Width > 16) || (Height > 16))
        {
            PredIntraAngLuma(g_bitDepthY, pSrc + sw + 1, sw, pDst, Stride, Width, Height, DirMode, false);
        }
        else
        {
            PredIntraAngLuma(g_bitDepthY, pSrc + sw + 1, sw, pDst, Stride, Width, Height, DirMode, true);

            if(DirMode == DC_IDX)
            {
                DCPredFiltering(pSrc + sw + 1, sw, pDst, Stride, Width, Height);
            }
        }
    }
}

/** Function for deriving planar intra prediction.
 * \param pSrc pointer to reconstructed sample array
 * \param srcStride the stride of the reconstructed sample array
 * \param rpDst reference to pointer for the prediction sample array
 * \param dstStride the stride of the prediction sample array
 * \param width the width of the block
 * \param height the height of the block
 *
 * This function derives the prediction samples for planar mode (intra coding).
 */
void H265Prediction::PredIntraPlanarLuma(H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrYCommon pDst, Ipp32s dstStride, Ipp32u width, Ipp32u height)
{
    VM_ASSERT(width == height);

    Ipp32s k;
    Ipp32s l;
    Ipp32s bottomLeft;
    Ipp32s topRight;
    Ipp32s horPred;
    Ipp32s leftColumn[MAX_CU_SIZE];
    Ipp32s topRow[MAX_CU_SIZE];
    Ipp32s bottomRow[MAX_CU_SIZE];
    Ipp32s rightColumn[MAX_CU_SIZE];
    Ipp32s blkSize = width;
    Ipp32u offset2D = width;
    Ipp32u shift1D = g_ConvertToBit[width] + 2;
    Ipp32u shift2D = shift1D + 1;

    // Get left and above reference column and row
    for(k = 0; k < blkSize + 1; k++)
    {
        topRow[k] = pSrc[k - srcStride];
        leftColumn[k] = pSrc[k * srcStride - 1];
    }

    // Prepare intermediate variables used in interpolation
    bottomLeft = leftColumn[blkSize];
    topRight = topRow[blkSize];
    for (k = 0; k < blkSize; k++)
    {
        bottomRow[k] = bottomLeft - topRow[k];
        rightColumn[k] = topRight - leftColumn[k];
        topRow[k] <<= shift1D;
        leftColumn[k] <<= shift1D;
    }

    // Generate prediction signal
    for (k = 0; k < blkSize; k++)
    {
        horPred = leftColumn[k] + offset2D;
        for (l = 0; l < blkSize; l++)
        {
            horPred += rightColumn[k];
            topRow[l] += bottomRow[l];
            pDst[k * dstStride + l] = (H265PlaneYCommon) ((horPred + topRow[l]) >> shift2D);
        }
  }
}

/** Function for deriving planar intra prediction.
 * \param pSrc pointer to reconstructed sample array
 * \param srcStride the stride of the reconstructed sample array
 * \param rpDst reference to pointer for the prediction sample array
 * \param dstStride the stride of the prediction sample array
 * \param width the width of the block
 * \param height the height of the block
 *
 * This function derives the prediction samples for planar mode (intra coding).
 */
void H265Prediction::PredIntraPlanarChroma(H265PlanePtrUVCommon pSrc, Ipp32s srcStride, H265PlanePtrUVCommon pDst, Ipp32s dstStride, Ipp32u width, Ipp32u height)
{
    VM_ASSERT(width == height);

    Ipp32s k;
    Ipp32s l;
    Ipp32s bottomLeft1, bottomLeft2;
    Ipp32s topRight1, topRight2;
    Ipp32s horPred1, horPred2;
    Ipp32s leftColumn[MAX_CU_SIZE];
    Ipp32s topRow[MAX_CU_SIZE];
    Ipp32s bottomRow[MAX_CU_SIZE];
    Ipp32s rightColumn[MAX_CU_SIZE];
    Ipp32s blkSize = width;
    Ipp32u offset2D = width;
    Ipp32u shift1D = g_ConvertToBit[width] + 2;
    Ipp32u shift2D = shift1D + 1;
    Ipp32s srcStrideHalf = srcStride >> 1;

    // Get left and above reference column and row
    for(k = 0; k < (blkSize + 1) * 2; k += 2)
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
    for (k = 0; k < blkSize * 2; k += 2)
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
    for (k = 0; k < blkSize; k++)
    {
        horPred1 = leftColumn[k * 2] + offset2D;
        horPred2 = leftColumn[k * 2 + 1] + offset2D;
        for (l = 0; l < blkSize * 2; l += 2)
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

// Function for deriving the angular Intra predictions

/** Function for deriving the simplified angular intra predictions.
 * \param pSrc pointer to reconstructed sample array
 * \param srcStride the stride of the reconstructed sample array
 * \param rpDst reference to pointer for the prediction sample array
 * \param dstStride the stride of the prediction sample array
 * \param width the width of the block
 * \param height the height of the block
 * \param dirMode the intra prediction mode index
 * \param blkAboveAvailable boolean indication if the block above is available
 * \param blkLeftAvailable boolean indication if the block to the left is available
 *
 * This function derives the prediction samples for the angular mode based on the prediction direction indicated by
 * the prediction mode index. The prediction direction is given by the displacement of the bottom row of the block and
 * the reference row above the block in the case of vertical prediction or displacement of the rightmost column
 * of the block and reference column left from the block in the case of the horizontal prediction. The displacement
 * is signalled at 1/32 pixel accuracy. When projection of the predicted pixel falls inbetween reference samples,
 * the predicted value for the pixel is linearly interpolated from the reference samples. All reference samples are taken
 * from the extended main reference.
 */
void H265Prediction::PredIntraAngLuma(Ipp32s bitDepth, H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrYCommon Dst, Ipp32s dstStride, Ipp32u width, Ipp32u height, Ipp32u dirMode, bool Filter)
{
    Ipp32s k;
    Ipp32s l;
    Ipp32s blkSize = (Ipp32s) width;
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
    Ipp32s angTable[9] = {0,    2,    5,   9,  13,  17,  21,  26,  32};
    Ipp32s invAngTable[9] = {0, 4096, 1638, 910, 630, 482, 390, 315, 256}; // (256 * 32) / Angle
    Ipp32s invAngle = invAngTable[absAng];
    absAng = angTable[absAng];
    intraPredAngle = signAng * absAng;
    Ipp32s maxVal = (1 << bitDepth) - 1;

    // Do the DC prediction
    if (modeDC)
    {
        H265PlaneYCommon dcval = PredIntraGetPredValDC(pSrc, srcStride, width, height);

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

// Function for deriving the angular Intra predictions

/** Function for deriving the simplified angular intra predictions.
 * \param pSrc pointer to reconstructed sample array
 * \param srcStride the stride of the reconstructed sample array
 * \param rpDst reference to pointer for the prediction sample array
 * \param dstStride the stride of the prediction sample array
 * \param width the width of the block
 * \param height the height of the block
 * \param dirMode the intra prediction mode index
 * \param blkAboveAvailable boolean indication if the block above is available
 * \param blkLeftAvailable boolean indication if the block to the left is available
 *
 * This function derives the prediction samples for the angular mode based on the prediction direction indicated by
 * the prediction mode index. The prediction direction is given by the displacement of the bottom row of the block and
 * the reference row above the block in the case of vertical prediction or displacement of the rightmost column
 * of the block and reference column left from the block in the case of the horizontal prediction. The displacement
 * is signalled at 1/32 pixel accuracy. When projection of the predicted pixel falls inbetween reference samples,
 * the predicted value for the pixel is linearly interpolated from the reference samples. All reference samples are taken
 * from the extended main reference.
 */
void H265Prediction::PredIntraAngChroma(Ipp32s bitDepth, H265PlanePtrUVCommon pSrc, Ipp32s srcStride, H265PlanePtrUVCommon Dst, Ipp32s dstStride, Ipp32u width, Ipp32u height, Ipp32u dirMode)
{
    Ipp32s k;
    Ipp32s l;
    Ipp32s blkSize = (Ipp32s) width;
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
    Ipp32s angTable[9] = {0,    2,    5,   9,  13,  17,  21,  26,  32};
    Ipp32s invAngTable[9] = {0, 4096, 1638, 910, 630, 482, 390, 315, 256}; // (256 * 32) / Angle
    Ipp32s invAngle = invAngTable[absAng];
    absAng = angTable[absAng];
    intraPredAngle = signAng * absAng;
    Ipp32s dstStrideHalf = dstStride >> 1;

    // Do the DC prediction
    if (modeDC)
    {
        H265PlaneUVCommon dc1, dc2;
        Ipp32s Sum1 = 0, Sum2 = 0;
        for (Ipp32s Ind = 0; Ind < width * 2; Ind += 2)
        {
            Sum1 += pSrc[Ind - srcStride];
            Sum2 += pSrc[Ind - srcStride + 1];
        }
        for (Ipp32s Ind = 0; Ind < height; Ind++)
        {
            Sum1 += pSrc[Ind * srcStride - 2];
            Sum2 += pSrc[Ind * srcStride - 2 + 1];
        }

        dc1 = (H265PlaneUVCommon)((Sum1 + width) / (width + height));
        dc2 = (H265PlaneUVCommon)((Sum2 + width) / (width + height));

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

/** Function for filtering intra DC predictor.
 * \param pSrc pointer to reconstructed sample array
 * \param iSrcStride the stride of the reconstructed sample array
 * \param rpDst reference to pointer for the prediction sample array
 * \param iDstStride the stride of the prediction sample array
 * \param iWidth the width of the block
 * \param iHeight the height of the block
 *
 * This function performs filtering left and top edges of the prediction samples for DC mode (intra coding).
 */
void H265Prediction::DCPredFiltering(H265PlanePtrYCommon pSrc, Ipp32s SrcStride, H265PlanePtrYCommon Dst, Ipp32s DstStride, Ipp32u Width, Ipp32u Height)
{
    H265PlanePtrYCommon pDst = Dst;
    Ipp32s x;
    Ipp32s y;
    Ipp32s DstStride2;
    Ipp32s SrcStride2;

    // boundary pixels processing
    pDst[0] = (H265PlaneYCommon)((pSrc[-SrcStride] + pSrc[-1] + 2 * pDst[0] + 2) >> 2);

    #pragma ivdep
    for (x = 1; x < Width; x++)
    {
        pDst[x] = (H265PlaneYCommon)((pSrc[x - SrcStride] + 3 * pDst[x] + 2) >> 2);
    }

    #pragma ivdep
    for (y = 1, DstStride2 = DstStride, SrcStride2 = SrcStride - 1; y < Height; y++, DstStride2 += DstStride, SrcStride2 += SrcStride)
    {
        pDst[DstStride2] = (H265PlaneYCommon)((pSrc[SrcStride2] + 3 * pDst[DstStride2] + 2) >> 2);
    }

    return;
}

// Angular chroma
void H265Prediction::PredIntraChromaAng(H265PlanePtrUVCommon pSrc, Ipp32u DirMode, H265PlanePtrUVCommon pPred, Ipp32u Stride, Ipp32s Width, Ipp32s Height)
{
    H265PlanePtrUVCommon pDst = pPred;
    H265PlanePtrUVCommon ptrSrc = pSrc;

    // get starting pixel in block
    Ipp32s sw = 2 * Width + 1;
    sw *= 2;

    if (DirMode == PLANAR_IDX)
    {
        PredIntraPlanarChroma(ptrSrc + sw + 2, sw, pDst, Stride, Width, Height);
    }
    else
    {
        // Create the prediction
        PredIntraAngChroma(g_bitDepthC, ptrSrc + sw + 2, sw, pDst, Stride, Width, Height, DirMode);
    }
}

void H265Prediction::MotionCompensation(H265CodingUnit* pCU, H265DecYUVBufferPadded* pPredYUV, Ipp32u AbsPartIdx)
{
    bool weighted_prediction = pCU->m_SliceHeader->slice_type == P_SLICE ? pCU->m_SliceHeader->m_PicParamSet->weighted_pred_flag :
        pCU->m_SliceHeader->m_PicParamSet->weighted_bipred_idc;

    for (Ipp32s PartIdx = 0; PartIdx < pCU->getNumPartInter(0); PartIdx++)
    {
        Ipp32u PartAddr;
        Ipp32u Width, Height;
        pCU->getPartIndexAndSize(0, PartIdx, PartAddr, Width, Height);

        Ipp32s RefIdx[2] = {-1, -1};
        RefIdx[REF_PIC_LIST_0] = pCU->m_CUMVbuffer[REF_PIC_LIST_0].RefIdx[PartAddr];
        RefIdx[REF_PIC_LIST_1] = pCU->m_CUMVbuffer[REF_PIC_LIST_1].RefIdx[PartAddr];

        VM_ASSERT(RefIdx[REF_PIC_LIST_0] >= 0 || RefIdx[REF_PIC_LIST_1] >= 0);

        if (!weighted_prediction && (CheckIdenticalMotion(pCU, PartAddr) || !(RefIdx[REF_PIC_LIST_0] >= 0 && RefIdx[REF_PIC_LIST_1] >= 0)))
        {
            EnumRefPicList direction = RefIdx[REF_PIC_LIST_0] >= 0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
            PredInterUni(pCU, PartAddr, Width, Height, direction, &m_YUVPred[0], false);
        }
        else
        {
            for (Ipp32s RefList = 0; RefList < 2; RefList++)
            {
                EnumRefPicList RefPicList = (RefList ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
                if (RefIdx[RefList] < 0)
                    continue;

                VM_ASSERT(RefIdx[RefList] < pCU->m_SliceHeader->m_numRefIdx[RefPicList]);

                H265DecYUVBufferPadded *pcMbYuv = &m_YUVPred[RefList];
                PredInterUni(pCU, PartAddr, Width, Height, RefPicList, pcMbYuv, true);
            }

            if (!weighted_prediction)
            {
                m_YUVPred[0].AddAverageToPic(pCU->m_Frame, &m_YUVPred[1], pCU->CUAddr, AbsPartIdx, PartAddr, Width, Height);
            }
            else
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

                pPredYUV->CopyPartToPic(pCU->m_Frame, pCU->CUAddr, AbsPartIdx, PartAddr, Width, Height);
            }
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

void H265Prediction::PredInterUni(H265CodingUnit* pCU, Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList,
                                  H265DecYUVBufferPadded *YUVPred, bool bi)
{
    Ipp32s RefIdx = pCU->m_CUMVbuffer[RefPicList].RefIdx[PartAddr];
    VM_ASSERT(RefIdx >= 0);

    H265MotionVector MV = pCU->m_CUMVbuffer[RefPicList].MV[PartAddr];
    H265DecoderFrame *PicYUVRef = pCU->m_Frame->GetRefPicList(pCU->m_SliceIdx, RefPicList)->m_refPicList[RefIdx].refFrame;

    // LUMA
    Ipp32s in_SrcPitch = PicYUVRef->pitch_luma();
    Ipp32s refOffset = (MV.Horizontal >> 2) + (MV.Vertical >> 2) * in_SrcPitch;
    H265PlanePtrUVCommon in_pSrc = PicYUVRef->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) + refOffset;

    Ipp32s in_DstPitch = YUVPred->pitch_luma();
    // Hack to get correct offset in 2-byte elements
    H265CoeffsPtrCommon in_pDst = (H265CoeffsPtrCommon)YUVPred->GetLumaAddr() + (YUVPred->GetPartLumaAddr(PartAddr) - YUVPred->GetLumaAddr());

    Ipp32s in_dx, in_dy;
    Ipp32s bitDepth, shift, offset, tap;
    Ipp32s bitDepthLuma = g_bitDepthY;
    Ipp32s bitDepthChroma = g_bitDepthC;

    IppVCInterpolateBlock_8u interpolateInfo;
    IppVCInterpolateBlock_8u interpolateInfo_temp;
    interpolateInfo.pSrc[0] = (const Ipp8u*)PicYUVRef->m_pYPlane;
    interpolateInfo.srcStep = in_SrcPitch;
    interpolateInfo.pDst[0] = (Ipp8u*) in_pDst;
    interpolateInfo.dstStep = in_DstPitch;
    interpolateInfo.pointVector.x= MV.Horizontal;
    interpolateInfo.pointVector.y= MV.Vertical;

    interpolateInfo.pointBlockPos.x = Ipp32u(PicYUVRef->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) - interpolateInfo.pSrc[0]) % in_SrcPitch;
    interpolateInfo.pointBlockPos.y = Ipp32u(PicYUVRef->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) - interpolateInfo.pSrc[0]) / in_SrcPitch;
    interpolateInfo.sizeBlock.width = Width;
    interpolateInfo.sizeBlock.height = Height;
    /**/
    interpolateInfo.sizeFrame.width = pCU->m_SliceHeader->m_SeqParamSet->pic_width_in_luma_samples;
    interpolateInfo.sizeFrame.height = pCU->m_SliceHeader->m_SeqParamSet->pic_height_in_luma_samples;

    bitDepth = bitDepthLuma;
    tap = 8;
    in_dx = MV.Horizontal & 3;
    in_dy = MV.Vertical & 3;

    shift = 6;
    offset = 32;

    if (bi)
    {
        shift = bitDepth - 8;
        offset = 0;
    }

    IppStatus sts = ippiInterpolateLumaBlock_H265_8u(&interpolateInfo, m_temp_interpolarion_buffer);
    if (sts != ippStsNoOperation)
    {
        in_pSrc = (H265PlanePtrUVCommon)interpolateInfo.pSrc[0];
        in_SrcPitch = 128;
    }

    if ((in_dx == 0) && (in_dy == 0))
    {
        shift = 0;

        if (bi)
            shift = 14 - bitDepth;

        if (!bi)
        {
            Ipp32u DstStride = pCU->m_Frame->pitch_luma();
            H265PlanePtrYCommon pDst = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU) + GetAddrOffset(PartAddr, DstStride);
            H265PlanePtrYCommon pSrc = in_pSrc;

            for (Ipp32s j = 0; j < Height; j++)
            {
                memcpy(pDst, pSrc, sizeof(pSrc[0])*Width);
                pSrc += in_SrcPitch;
                pDst += DstStride;
            }
        }
        else
        {
            CopyPULuma(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, Width, Height, shift);
        }
    }
    else if (in_dy == 0)
    {
#if (HEVC_OPT_CHANGES & 8)
        Interpolate<TEXT_LUMA>( INTERP_HOR,
                                in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                                in_dx, Width, Height, shift, offset);
#else
        InterpolateHorLuma(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                        in_dx, Width, Height, shift, offset);
#endif
#if (HEVC_OPT_CHANGES & 0x10000)
// ML: FIXME: remove the checks afte interpolation is finished
        Ipp16s tmpBuf[80 * 80];
        InterpolateHorLuma(in_pSrc, in_SrcPitch, tmpBuf, 80,
                           in_dx, Width, Height, shift, offset);

        for (int i=0; i < Height; ++i )
            for (int j=0; j < Width; ++j )
                if ( tmpBuf[ i*80 + j] != in_pDst[ i*in_DstPitch + j ] )
                    _asm int 3;
#endif
        if (!bi)
        {
            YUVPred->CopyPartToPicAndSaturate(pCU->m_Frame, pCU->CUAddr, pCU->m_AbsIdxInLCU, PartAddr, Width, Height, 1);
        }
    }
    else if (in_dx == 0)
    {
#if (HEVC_OPT_CHANGES & 8)
        Interpolate<TEXT_LUMA>( INTERP_VER,
                                in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                                in_dy, Width, Height, shift, offset);
#else
        InterpolateVert0Luma(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                                in_dy, Width, Height, shift, offset);
#endif
#if (HEVC_OPT_CHANGES & 0x10000)
// ML: FIXME: remove the checks afte interpolation is finished
        Ipp16s tmpBuf[80 * 80];
        InterpolateVert0Luma(in_pSrc, in_SrcPitch, tmpBuf, 80,
                             in_dy, Width, Height, shift, offset);

        for (int i=0; i < Height; ++i )
            for (int j=0; j < Width; ++j )
                if ( tmpBuf[ i*80 + j] != in_pDst[ i*in_DstPitch + j ] )
                    _asm int 3;
#endif
        if (!bi)
        {
            YUVPred->CopyPartToPicAndSaturate(pCU->m_Frame, pCU->CUAddr, pCU->m_AbsIdxInLCU, PartAddr, Width, Height, 1);
        }

   }
    else
    {
        // ML: OPT: TODO: Check if aligning temp buffer's width by cache line boundary (64B) helps
        Ipp16s tmpBuf[80 * 80];

        shift = 20 - bitDepth;
        offset = 1 << (19 - bitDepth);

        if (bi)
        {
            shift = 6;
            offset = 0;
        }

        #if (HEVC_OPT_CHANGES & 8)
                Interpolate<TEXT_LUMA>( INTERP_HOR, 
                                         in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80,
                                         in_dx, Width, Height + tap - 1, bitDepth - 8, 0);
        #else
                InterpolateHorLuma(in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80,
                                   in_dx, Width, Height + tap - 1, bitDepth - 8, 0);
        #endif
        #if (HEVC_OPT_CHANGES & 0x10000)
        // ML: FIXME: remove the checks afte interpolation is finished
                Ipp16s tmpBuf2[80 * 80];
                InterpolateHorLuma(in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf2, 80,
                                   in_dx, Width, Height + tap - 1, bitDepth - 8, 0);

                for (int i=0; i < Height + tap - 1; ++i )
                    for (int j=0; j < Width; ++j )
                        if ( tmpBuf[ i*80 + j] != tmpBuf2[ i*80 + j ] )
                            _asm int 3;
        #endif

        #if (HEVC_OPT_CHANGES & 8)
                Interpolate<TEXT_LUMA>( INTERP_VER, 
                                        tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch,
                                        in_dy, Width, Height, shift, offset);
        #else
                InterpolateVertLuma(tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch,
                                    in_dy, Width, Height, shift, offset);
        #endif

        #if (HEVC_OPT_CHANGES & 0x10000)
        // ML: FIXME: remove the checks afte interpolation is finished
                InterpolateVertLuma( tmpBuf + ((tap >> 1) - 1) * 80, 80, tmpBuf2, 80,
                                    in_dy, Width, Height, shift, offset);

                for (int i=0; i < Height; ++i )
                    for (int j=0; j < Width; ++j )
                        if ( tmpBuf2[ i*80 + j] != in_pDst[ i*in_DstPitch + j ] )
                            _asm int 3;
        #endif

        if (!bi)
        {
            YUVPred->CopyPartToPicAndSaturate(pCU->m_Frame, pCU->CUAddr, pCU->m_AbsIdxInLCU, PartAddr, Width, Height, 1);
        }
    }


    // CHROMA
    Width >>= 1;
    Height >>= 1;
    in_SrcPitch = PicYUVRef->pitch_chroma();
    refOffset = (MV.Horizontal >> 3) * 2 + (MV.Vertical >> 3) * in_SrcPitch;
    in_DstPitch = YUVPred->pitch_chroma();
    bitDepth = bitDepthChroma;
    tap = 4;
    in_dx = MV.Horizontal & 7;
    in_dy = MV.Vertical & 7;

    // CHROMA UV
    in_pSrc = PicYUVRef->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) + refOffset;
    // Hack to get correct offset in 2-byte elements
    in_pDst = (H265CoeffsPtrCommon)YUVPred->GetCbCrAddr() + (YUVPred->GetPartCbCrAddr(PartAddr) - YUVPred->GetCbCrAddr());

    shift = 6;
    offset = 32;

    interpolateInfo.pSrc[0] = (const Ipp8u*)PicYUVRef->m_pUVPlane;
    interpolateInfo.srcStep = in_SrcPitch;
    interpolateInfo.pDst[0] = (Ipp8u*) in_pDst;
    interpolateInfo.dstStep = in_DstPitch;
    interpolateInfo.pointVector.x= MV.Horizontal;
    interpolateInfo.pointVector.y= MV.Vertical;

    interpolateInfo.pointBlockPos.x = (Ipp32u(PicYUVRef->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) - interpolateInfo.pSrc[0]) % in_SrcPitch) / 2;
    interpolateInfo.pointBlockPos.y = Ipp32u(PicYUVRef->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartAddr) - interpolateInfo.pSrc[0]) / in_SrcPitch;
    interpolateInfo.sizeBlock.width = Width;
    interpolateInfo.sizeBlock.height = Height;
    /**/
    interpolateInfo.sizeFrame.width = pCU->m_SliceHeader->m_SeqParamSet->pic_width_in_luma_samples >> 1;
    interpolateInfo.sizeFrame.height = pCU->m_SliceHeader->m_SeqParamSet->pic_height_in_luma_samples >> 1;

    if (bi)
    {
        shift = bitDepth - 8;
        offset = 0;
    }

    sts = ippiInterpolateChromaBlock_H264_8u(&interpolateInfo, m_temp_interpolarion_buffer);
    if (sts != ippStsNoOperation)
    {
        in_pSrc = (H265PlanePtrUVCommon)interpolateInfo.pSrc[0];
        in_SrcPitch = 128;
    }

    if ((in_dx == 0) && (in_dy == 0))
    {
        if (!bi)
        {
            Ipp32u DstStride = pCU->m_Frame->pitch_chroma();
            H265PlanePtrYCommon pDst = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU) + GetAddrOffset(PartAddr, DstStride >> 1);
            H265PlanePtrYCommon pSrc = in_pSrc;

            for (Ipp32s j = 0; j < Height; j++)
            {
                memcpy(pDst, pSrc, sizeof(pSrc[0])*Width*2);
                pSrc += in_SrcPitch;
                pDst += DstStride;
            }
        }
        else
        {
            shift = 14 - bitDepth;
            CopyPUChroma(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, Width, Height, shift);
        }
    }
    else if (in_dy == 0)
    {
#if (HEVC_OPT_CHANGES & 8)
        Interpolate<TEXT_CHROMA>( INTERP_HOR,
                                   in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                                   in_dx, Width, Height, shift, offset);
#else
        InterpolateHorChroma(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                                in_dx, Width, Height, shift, offset);

#endif
#if (HEVC_OPT_CHANGES & 0x10000)
// ML: FIXME: remove the checks afte interpolation is finished
        Ipp16s tmpBuf[80 * 80];
        InterpolateHorChroma(in_pSrc, in_SrcPitch, tmpBuf, 80,
                             in_dx, Width, Height, shift, offset);

        for (int i=0; i < Height; ++i )
            for (int j=0; j < Width * 2; ++j )
                if ( tmpBuf[ i*80 + j] != in_pDst[ i*in_DstPitch + j ] )
                    _asm int 3;
#endif
        if (!bi)
        {
            Width <<= 1;
            Height <<= 1;

            YUVPred->CopyPartToPicAndSaturate(pCU->m_Frame, pCU->CUAddr, pCU->m_AbsIdxInLCU, PartAddr, Width, Height, 2);
        }
    }
    else if (in_dx == 0)
    {
#if (HEVC_OPT_CHANGES & 8)
        Interpolate<TEXT_CHROMA>( INTERP_VER,
                                   in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                                   in_dy, Width, Height, shift, offset);
#else
        InterpolateVert0Chroma(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                                in_dy, Width, Height, shift, offset);
#endif
#if (HEVC_OPT_CHANGES & 0x10000)
// ML: FIXME: remove the checks afte interpolation is finished
        Ipp16s tmpBuf[80 * 80];
        InterpolateVert0Chroma(in_pSrc, in_SrcPitch, tmpBuf, 80,
                             in_dy, Width, Height, shift, offset);

        for (int i=0; i < Height; ++i )
            for (int j=0; j < Width * 2; ++j )
                if ( tmpBuf[ i*80 + j] != in_pDst[ i*in_DstPitch + j ] )
                    _asm int 3;
#endif
        if (!bi)
        {
            Width <<= 1;
            Height <<= 1;

            YUVPred->CopyPartToPicAndSaturate(pCU->m_Frame, pCU->CUAddr, pCU->m_AbsIdxInLCU, PartAddr, Width, Height, 2);
        }
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];

        shift = 20 - bitDepth;
        offset = 1 << (19 - bitDepth);

        if (bi)
        {
            shift = 6;
            offset = 0;
        }

        #if (HEVC_OPT_CHANGES & 8)
                Interpolate<TEXT_CHROMA>( INTERP_HOR, 
                                    in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80,
                                    in_dx, Width, Height + tap - 1, bitDepth - 8, 0);
        #else
                InterpolateHorChroma(in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80,
                                        in_dx, Width, Height + tap - 1, bitDepth - 8, 0);
        #endif
        #if (HEVC_OPT_CHANGES & 0x10000)
        // ML: FIXME: remove the checks afte interpolation is finished
                Ipp16s tmpBuf2[80 * 80];
                InterpolateHorChroma(in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf2, 80,
                                        in_dx, Width, Height + tap - 1, bitDepth - 8, 0);

                for (int i=0; i < Height + tap - 1; ++i )
                    for (int j=0; j < Width * 2; ++j )
                        if ( tmpBuf[ i*80 + j] != tmpBuf2[ i*80 + j] )
                            _asm int 3;
        #endif


        #if (HEVC_OPT_CHANGES & 8)
                Interpolate<TEXT_CHROMA>( INTERP_VER,
                                            tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch,
                                            in_dy, Width, Height, shift, offset);
        #else
                InterpolateVertChroma(tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch,
                                        in_dy, Width, Height, shift, offset);
        #endif
        #if (HEVC_OPT_CHANGES & 0x10000)
        // ML: FIXME: remove the checks afte interpolation is finished
                InterpolateVertChroma( tmpBuf + ((tap >> 1) - 1) * 80, 80, tmpBuf2, 80,
                                        in_dy, Width, Height, shift, offset);
                for (int i=0; i < Height; ++i )
                    for (int j=0; j < Width * 2; ++j )
                        if ( tmpBuf2[ i*80 + j] != in_pDst[ i*in_DstPitch + j ] )
                            _asm int 3;
        #endif
        if (!bi)
        {
            Width <<= 1;
            Height <<= 1;

            YUVPred->CopyPartToPicAndSaturate(pCU->m_Frame, pCU->CUAddr, pCU->m_AbsIdxInLCU, PartAddr, Width, Height, 2);
        }
    }

} /*void H265Prediction::PredInterUni*/

#if (HEVC_OPT_CHANGES & 8)

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

// This better be placed in some general/common header
#ifdef __INTEL_COMPILER
# define RESTRICT __restrict
#elif defined _MSC_VER
# if _MSC_VER >= 1400
#  define RESTRICT __restrict
# else
#  define RESTRICT
# endif
#else
# define RESTRICT
#endif

template <typename> class upconvert_int;
template <> class upconvert_int<Ipp8u>  { public: typedef Ipp16s  result; };
template <> class upconvert_int<Ipp16s> { public: typedef Ipp32s  result; };

//=================================================================================================
template
< 
    typename     t_vec, 
    EnumTextType c_plane_type,
    int          c_shift,
    int          c_width,
    typename     t_src, 
    typename     t_dst
>
class t_InterpKernel_intrin
{
public:
    static void func(
        t_dst* RESTRICT pDst, 
        const t_src* pSrc, 
        int    in_SrcPitch, // in samples
        int    in_DstPitch, // in samples
        int    height,
        int    accum_pitch,
        int    tab_index,
        int    offset
    );
};

template
< 
    EnumTextType c_plane_type,
    int          c_shift,
    int          c_width,
    typename     t_src, 
    typename     t_dst
>
class t_InterpKernel_intrin< __m128i, c_plane_type, c_shift, c_width, t_src, t_dst >
{
    typedef __m128i t_vec;

public:
    static void func(
        t_dst* RESTRICT pDst, 
        const t_src* pSrc, 
        int    in_SrcPitch, // in samples
        int    in_DstPitch, // in samples
        int    height,
        int    accum_pitch,
        int    tab_index,
        int    offset
    )
    {
        typedef upconvert_int< t_src >::result t_acc;
        const int c_tap = (c_plane_type == TEXT_LUMA) ? 8 : 4;

        const Ipp16s* coeffs8x = (c_plane_type == TEXT_LUMA) 
            ? g_lumaInterpolateFilter8X[tab_index - 1] 
            : g_chromaInterpolateFilter8X[tab_index - 1];

        int c_nvec_width = c_width / (sizeof( t_vec ) / sizeof( t_dst ));

        if (sizeof( t_dst ) == sizeof( t_src ) && sizeof( t_src )  == 1)
            c_nvec_width *= 2;

        if (!c_nvec_width)
            c_nvec_width = 1;

        t_vec v_offset = _mm_cvtsi32_si128( sizeof(t_acc)==4 ? offset : (offset << 16) | offset );
        v_offset = _mm_shuffle_epi32( v_offset, 0 );

        for (int j = 0; j < height; ++j) 
        {
            _mm_prefetch( (const char*)(pSrc + in_SrcPitch), _MM_HINT_NTA ); 

            #pragma ivdep
            #pragma nounroll
            for (int i = 0; i < c_nvec_width; ++i )
            {
                t_vec v_acc = _mm_setzero_si128(), v_acc2 = _mm_setzero_si128();
                const Ipp16s* coeffs = coeffs8x;
                const t_src*  pSrc_ = pSrc + i*8;

                #pragma unroll(c_tap)
                for (int k = 0; k < c_tap; ++k )
                {
                    t_vec v_coeff = _mm_loadu_si128( k + ( const __m128i* )coeffs );

                    if (sizeof(t_src) == 1) // 8-bit source, 16-bit accum [check is resolved/eliminated at compile time]
                    {
                        t_vec v_chunk = _mm_cvtepu8_epi16( *(const t_vec*)pSrc_ );
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

                if ( sizeof(t_src)==1 ) // resolved at compile time
                {
                    t_vec v_chunk = _mm_add_epi16( v_acc, v_offset );
                    if ( c_shift )
                        v_chunk = _mm_srai_epi16( v_chunk, c_shift );
                    if (sizeof(t_dst)==1 ) // resolved at compile time
                    {
                        v_chunk = _mm_packus_epi16(v_chunk, v_chunk);
                        _mm_storel_epi64( (t_vec *)(8*i + (t_dst*)pDst), v_chunk );
                    }
                    else
                    {
                        _mm_storeu_si128( i + (t_vec*)pDst, v_chunk );
                    }
                }
                else // 32-bit accum 
                {
                    t_vec v_lo = _mm_add_epi32( v_acc,  v_offset );
                    t_vec v_hi = _mm_add_epi32( v_acc2, v_offset );

                    if ( c_shift ) {
                        v_lo = _mm_srai_epi32( v_lo, c_shift );
                        v_hi = _mm_srai_epi32( v_hi, c_shift );
                    }
                    v_lo = _mm_packs_epi32( v_lo, v_hi );
                    _mm_storeu_si128( i + (t_vec*)pDst, v_lo );
                }
            }

            pSrc += in_SrcPitch;
            pDst += in_DstPitch;
        }
    }
};

//=================================================================================================
// for non-power-2 width, function will write power-2 widths to the dest buffer
template
<
    typename        t_vec,
    EnumTextType    c_plane_type,
    int             c_shift,
    typename        t_src,
    typename        t_dst 
>
static void t_InterpKernel_dispatched( 
    t_dst* RESTRICT pDst, 
    const t_src* pSrc, 
    Ipp32u in_SrcPitch, // in samples
    Ipp32u in_DstPitch, // in samples
    Ipp32s width,
    Ipp32s height,
    Ipp32s accum_pitch,
    int    tab_index,
    int    offset
)
{
    VM_ASSERT( width <= 64 );

    // compiler-proof loop unroll
    if (width <= 8 )
        t_InterpKernel_intrin< t_vec, c_plane_type, c_shift, 8,  t_src, t_dst >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, height, accum_pitch, tab_index, offset );
    else if (width <= 16 )
        t_InterpKernel_intrin< t_vec, c_plane_type, c_shift, 16, t_src, t_dst >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, height, accum_pitch, tab_index, offset );
    else if (width <= 32 )
        t_InterpKernel_intrin< t_vec, c_plane_type, c_shift, 32, t_src, t_dst >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, height, accum_pitch, tab_index, offset );
    else if (width <= 48 )
        t_InterpKernel_intrin< t_vec, c_plane_type, c_shift, 48, t_src, t_dst >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, height, accum_pitch, tab_index, offset );
    else if (width <= 64 )
        t_InterpKernel_intrin< t_vec, c_plane_type, c_shift, 64, t_src, t_dst >::func( pDst, pSrc, in_SrcPitch, in_DstPitch, height, accum_pitch, tab_index, offset );
}

//=================================================================================================
// for non-power-2 width, function will write power-2 widths to the dest buffer
template
<
    EnumTextType    c_plane_type,
    int             c_shift,
    typename        t_src,
    typename        t_dst 
>
static void t_InterpKernel( 
    t_dst* RESTRICT pDst, 
    const t_src* pSrc, 
    Ipp32u in_SrcPitch, // in samples
    Ipp32u in_DstPitch, // in samples
    Ipp32s width,
    Ipp32s height,
    Ipp32s accum_pitch,
    int    tab_index,
    int    offset
)
{
    t_InterpKernel_dispatched< __m128i, c_plane_type, c_shift >( pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, tab_index, offset );
}

//=================================================================================================
template < EnumTextType plane_type, typename t_src, typename t_dst >
void H265Prediction::Interpolate( 
                        EnumInterpType interp_type,
                        const t_src* in_pSrc,
                        Ipp32u in_SrcPitch, // in samples
                        t_dst* in_pDst,
                        Ipp32u in_DstPitch, // in samples
                        Ipp32s tab_index,
                        Ipp32s width,
                        Ipp32s height,
                        Ipp32s shift,
                        Ipp16s offset)
{
    Ipp32s accum_pitch = ((interp_type == INTERP_HOR) ? (plane_type == TEXT_LUMA ? 1 : 2) : in_SrcPitch);

    const t_src* pSrc = in_pSrc - (((( plane_type == TEXT_LUMA) ? 8 : 4) >> 1) - 1) * accum_pitch;

    width <<= int(plane_type == TEXT_CHROMA);

    if ( shift == 0 )
        t_InterpKernel< plane_type, 0 >( in_pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, tab_index, offset );
    else if ( shift == 6 )
        t_InterpKernel< plane_type, 6 >( in_pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, tab_index, offset );
    else if ( shift == 12 )
        t_InterpKernel< plane_type, 12>( in_pDst, pSrc, in_SrcPitch, in_DstPitch, width, height, accum_pitch, tab_index, offset );
    else
        // may need to add cases for 10-bit
        VM_ASSERT( 0 ); // this should never happen
}
#endif

#if !(HEVC_OPT_CHANGES) || (HEVC_OPT_CHANGES  & 0x10000)

/* ****************************************************************************** *\
FUNCTION: InterpolateHor
DESCRIPTION:
\* ****************************************************************************** */

void H265Prediction::InterpolateHorLuma(const H265PlanePtrYCommon in_pSrc,
                                        Ipp32u in_SrcPitch, // in samples
                                        Ipp16s *in_pDst,
                                        Ipp32u in_DstPitch, // in samples
                                        Ipp32s tab_index,
                                        Ipp32s width,
                                        Ipp32s height,
                                        Ipp32s shift,
                                        Ipp32s offset)
{
    H265PlanePtrYCommon pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k;

    const Ipp32s tap = 8;
    coeffs = &g_lumaInterpolateFilter[tab_index][0];
    pSrc -= ((tap >> 1) - 1);

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            Ipp32s tmp = 0;

            for (k = 0; k < tap; k++)
            {
                tmp += ((Ipp32s)pSrc[i + k]) * coeffs[k];
            }
            pDst[i] = (Ipp16s)((tmp + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

void H265Prediction::InterpolateHorChroma(const H265PlanePtrUVCommon in_pSrc,
                                          Ipp32u in_SrcPitch, // in samples
                                          Ipp16s *in_pDst,
                                          Ipp32u in_DstPitch, // in samples
                                          Ipp32s tab_index,
                                          Ipp32s width,
                                          Ipp32s height,
                                          Ipp32s shift,
                                          Ipp32s offset)
{
    H265PlanePtrUVCommon pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k;

    const Ipp32s tap = 4;
    coeffs = &g_chromaInterpolateFilter[tab_index][0];
    pSrc -= ((tap >> 1) - 1) * 2;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width * 2; i += 2)
        {
            Ipp32s tmp1 = 0, tmp2 = 0;

            for (k = 0; k < tap; k++)
            {
                Ipp32s k2 = k * 2;
                tmp1 += ((Ipp32s)pSrc[i + k2]) * coeffs[k];
                tmp2 += ((Ipp32s)pSrc[i + 1 + k2]) * coeffs[k];
            }
            pDst[i] = (Ipp16s)((tmp1 + offset) >> shift);
            pDst[i + 1] = (Ipp16s)((tmp2 + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

/* ****************************************************************************** *\
FUNCTION: InterpolateVert
DESCRIPTION:
\* ****************************************************************************** */

void H265Prediction::InterpolateVertLuma(const Ipp16s *in_pSrc,
                                         Ipp32u in_SrcPitch, // in samples
                                         Ipp16s *in_pDst,
                                         Ipp32u in_DstPitch, // in samples
                                         Ipp32s tab_index,
                                         Ipp32s width,
                                         Ipp32s height,
                                         Ipp32s shift,
                                         Ipp32s offset)
{
    Ipp16s *pSrc = const_cast<Ipp16s *>(in_pSrc);
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k;

    const Ipp32s tap = 8;
    coeffs = &g_lumaInterpolateFilter[tab_index][0];
    pSrc -= ((tap >> 1) - 1) * in_SrcPitch;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            Ipp32s tmp = 0;

            for (k = 0; k < tap; k++)
            {
                tmp += ((Ipp32s)pSrc[i + k * in_SrcPitch]) * coeffs[k];
            }
            pDst[i] = (Ipp16s)((tmp + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

void H265Prediction::InterpolateVertChroma(const Ipp16s *in_pSrc,
                                           Ipp32u in_SrcPitch, // in samples
                                           Ipp16s *in_pDst,
                                           Ipp32u in_DstPitch, // in samples
                                           Ipp32s tab_index,
                                           Ipp32s width,
                                           Ipp32s height,
                                           Ipp32s shift,
                                           Ipp32s offset)
{
    Ipp16s *pSrc = const_cast<Ipp16s *>(in_pSrc);
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k;

    const Ipp32s tap = 4;
    coeffs = &g_chromaInterpolateFilter[tab_index][0];
    pSrc -= ((tap >> 1) - 1) * in_SrcPitch;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width * 2; i += 2)
        {
            Ipp32s tmp1 = 0, tmp2 = 0;

            for (k = 0; k < tap; k++)
            {
                tmp1 += ((Ipp32s)pSrc[i + k * in_SrcPitch]) * coeffs[k];
                tmp2 += ((Ipp32s)pSrc[i + 1 + k * in_SrcPitch]) * coeffs[k];
            }
            pDst[i] = (Ipp16s)((tmp1 + offset) >> shift);
            pDst[i + 1] = (Ipp16s)((tmp2 + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

void H265Prediction::InterpolateVert0Luma(const H265PlanePtrYCommon in_pSrc,
                                          Ipp32u in_SrcPitch, // in samples
                                          Ipp16s *in_pDst,
                                          Ipp32u in_DstPitch, // in samples
                                          Ipp32s tab_index,
                                          Ipp32s width,
                                          Ipp32s height,
                                          Ipp32s shift,
                                          Ipp32s offset)
{
    H265PlanePtrYCommon pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k;

    const Ipp32s tap = 8;
    coeffs = &g_lumaInterpolateFilter[tab_index][0];
    pSrc -= ((tap >> 1) - 1) * in_SrcPitch;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            Ipp32s tmp = 0;

            for (k = 0; k < tap; k++)
            {
                tmp += ((Ipp32s)pSrc[i + k * in_SrcPitch]) * coeffs[k];
            }
            pDst[i] = (Ipp16s)((tmp + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

void H265Prediction::InterpolateVert0Chroma(const H265PlanePtrUVCommon in_pSrc,
                                            Ipp32u in_SrcPitch, // in samples
                                            Ipp16s *in_pDst,
                                            Ipp32u in_DstPitch, // in samples
                                            Ipp32s tab_index,
                                            Ipp32s width,
                                            Ipp32s height,
                                            Ipp32s shift,
                                            Ipp32s offset)
{
    H265PlanePtrUVCommon pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k;

    const Ipp32s tap = 4;
    coeffs = &g_chromaInterpolateFilter[tab_index][0];
    pSrc -= ((tap >> 1) - 1) * in_SrcPitch;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width * 2; i += 2)
        {
            Ipp32s tmp1 = 0, tmp2 = 0;

            for (k = 0; k < tap; k++)
            {
                tmp1 += ((Ipp32s)pSrc[i + k * in_SrcPitch]) * coeffs[k];
                tmp2 += ((Ipp32s)pSrc[i + 1 + k * in_SrcPitch]) * coeffs[k];
            }
            pDst[i] = (Ipp16s)((tmp1 + offset) >> shift);
            pDst[i + 1] = (Ipp16s)((tmp2 + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}
#endif // HEVC_OPT_CHANGES

/* ****************************************************************************** *\
FUNCTION: CopyPU
DESCRIPTION:
\* ****************************************************************************** */

// ML: OPT: TODO: Parameterize for const shift
void H265Prediction::CopyPULuma(const H265PlanePtrYCommon in_pSrc,
                                Ipp32u in_SrcPitch, // in samples
                                Ipp16s *in_pDst,
                                Ipp32u in_DstPitch, // in samples
                                Ipp32s width,
                                Ipp32s height,
                                Ipp32s shift)
{
    H265PlanePtrYCommon pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    Ipp32s i, j;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            pDst[i] = (Ipp16s)(((Ipp32s)pSrc[i]) << shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

void H265Prediction::CopyPUChroma(const H265PlanePtrUVCommon in_pSrc,
                                  Ipp32u in_SrcPitch, // in samples
                                  Ipp16s *in_pDst,
                                  Ipp32u in_DstPitch, // in samples
                                  Ipp32s width,
                                  Ipp32s height,
                                  Ipp32s shift)
{
    H265PlanePtrUVCommon pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    Ipp32s i, j;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width * 2; i++)
        {
            pDst[i] = (Ipp16s)(((Ipp32s)pSrc[i]) << shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
