//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "algorithm"
#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"
#include "ippi.h"

#if defined (MFX_ENABLE_CM)
#include "mfx_h265_enc_cm_defs.h"
#endif // MFX_ENABLE_CM

namespace H265Enc {

#if defined (MFX_ENABLE_CM)
    extern mfxU32 width;
    extern mfxU32 height;
    extern CmMbIntraGrad * cmMbIntraGrad4x4[2];
    extern CmMbIntraGrad * cmMbIntraGrad8x8[2];
    extern Ipp32s cmCurIdx;
#endif // MFX_ENABLE_CM

void GetAngModesFromHistogram(Ipp32s xPu, Ipp32s yPu, Ipp32s puSize, Ipp8s *modes, Ipp32s numModes)
{
#if !defined (MFX_ENABLE_CM)
    mfxU32 width = 0;
    mfxU32 height = 0;
#endif // MFX_ENABLE_CM

    VM_ASSERT(numModes <= 33);
    VM_ASSERT(puSize >= 4);
    VM_ASSERT(puSize <= 64);
    VM_ASSERT(xPu + puSize <= (Ipp32s)width);
    VM_ASSERT(yPu + puSize <= (Ipp32s)height);

    Ipp32s pitch = width;

    Ipp32s histogram[35] = {};

    // all in units of 4x4 blocks
    if (puSize == 4) {
        pitch >>= 2;
        puSize >>= 2;
#if defined (MFX_ENABLE_CM)
        const CmMbIntraGrad *histBlock = cmMbIntraGrad4x4[cmCurIdx] + (xPu >> 2) + (yPu >> 2) * pitch;
        for (Ipp32s i = 0; i < 35; i++)
            histogram[i] = histBlock->histogram[i];
#else
        for (Ipp32s i = 0; i < 35; i++)
            histogram[i] = 0;
#endif // MFX_ENABLE_CM
    }
    else {
        pitch >>= 3;
        puSize >>= 3;
#if defined (MFX_ENABLE_CM)
        const CmMbIntraGrad *histBlock = cmMbIntraGrad8x8[cmCurIdx] + (xPu >> 3) + (yPu >> 3) * pitch;

        for (Ipp32s y = 0; y < puSize; y++, histBlock += pitch)
            for (Ipp32s x = 0; x < puSize; x++)
                for (Ipp32s i = 0; i < 35; i++)
                    histogram[i] += histBlock[x].histogram[i];
#else
        for (Ipp32s x = 0; x < puSize; x++)
            for (Ipp32s i = 0; i < 35; i++)
                histogram[i] += 0;
#endif // MFX_ENABLE_CM
    }

    for (Ipp32s i = 0; i < numModes; i++) {
        Ipp32s mode = (Ipp32s)(std::max_element(histogram + 2, histogram + 35) - histogram);
        modes[i] = mode;
        histogram[mode] = -1;
    }
}

template <typename PixType>
static
void IsAboveLeftAvailable(H265CU<PixType> *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s numMinTUInLCU = pCU->m_par->NumPartInCUSize;
    Ipp32s rasterTopLeftPartIdx = h265_scan_z2r4[blockZScanIdx];
    Ipp32s topLeftPartRow = rasterTopLeftPartIdx >> 4;
    Ipp32s topLeftPartColumn = rasterTopLeftPartIdx & 15;

    if (topLeftPartColumn != 0) {
        if (topLeftPartRow != 0) {
            if (ConstrainedIntraPredFlag) {
                Ipp32s zScanIdx = h265_scan_r2z4[rasterTopLeftPartIdx - PITCH_TU - 1];
                outNeighborFlags[0] = ((pCU->m_data[zScanIdx].predMode == MODE_INTRA) ? 1 : 0);
            }
            else
                outNeighborFlags[0] = 1;
        }
        else {
            /* Above CU*/
            outNeighborFlags[0] = inNeighborFlags[numMinTUInLCU + 1 + (topLeftPartColumn - 1)];
        }
    }
    else {
        /* Left or Above Left CU*/
        outNeighborFlags[0] = inNeighborFlags[numMinTUInLCU - topLeftPartRow];
    }
}

template <typename PixType>
static
void IsAboveAvailable(H265CU<PixType> *pCU,
                      Ipp32s blockZScanIdx,
                      Ipp32s width,
                      Ipp8u ConstrainedIntraPredFlag,
                      Ipp8u *inNeighborFlags,
                      Ipp8u *outNeighborFlags)
{
    Ipp32s numMinTUInLCU = pCU->m_par->NumPartInCUSize;
    Ipp32s numMinTUInPU = width >> pCU->m_par->QuadtreeTULog2MinSize;
    Ipp32s rasterTopLeftPartIdx = h265_scan_z2r4[blockZScanIdx];
    Ipp32s topLeftPartRow = rasterTopLeftPartIdx >> 4;
    Ipp32s topLeftPartColumn = rasterTopLeftPartIdx & 15;
    Ipp32s i;

    if (topLeftPartRow == 0) {
        for (i = 0; i < numMinTUInPU; i++)
            outNeighborFlags[i] = inNeighborFlags[numMinTUInLCU + 1 + topLeftPartColumn + i];
    }
    else {

        if (ConstrainedIntraPredFlag) {
            for (i = 0; i < numMinTUInPU; i++) {
                Ipp32s zScanIdx = h265_scan_r2z4[rasterTopLeftPartIdx + i - PITCH_TU];
                outNeighborFlags[i] = ((pCU->m_data[zScanIdx].predMode == MODE_INTRA) ? 1 : 0);
            }
        }
        else {
            for (i = 0; i < numMinTUInPU; i++)
                outNeighborFlags[i] = 1;
        }
    }
}

template <typename PixType>
static
void IsAboveRightAvailable(H265CU<PixType> *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s numMinTUInLCU = pCU->m_par->NumPartInCUSize;
    Ipp32s numMinTUInPU = width >> pCU->m_par->QuadtreeTULog2MinSize;
    Ipp32s rasterTopRigthPartIdx = h265_scan_z2r4[blockZScanIdx] + numMinTUInPU - 1;
    Ipp32s zScanTopRigthPartIdx = h265_scan_r2z4[rasterTopRigthPartIdx];
    Ipp32s topRightPartRow = rasterTopRigthPartIdx >> 4;
    Ipp32s topRightPartColumn = rasterTopRigthPartIdx & 15;
    Ipp32s i;

    if (topRightPartRow == 0)
    {
        for (i = 0; i < numMinTUInPU; i++)
        {
            Ipp32s aboveRightPartColumn = topRightPartColumn + i + 1;

            if ((pCU->m_ctbPelX + (aboveRightPartColumn << pCU->m_par->QuadtreeTULog2MinSize)) >=
                pCU->m_par->Width)
            {
                outNeighborFlags[i] = 0;
            }
            else
            {
                outNeighborFlags[i] = inNeighborFlags[numMinTUInLCU+1+aboveRightPartColumn];
            }
        }
    }
    else
    {
        for (i = 0; i < numMinTUInPU; i++)
        {
            Ipp32s aboveRightPartColumn = topRightPartColumn + i + 1;

            if ((pCU->m_ctbPelX + (aboveRightPartColumn << pCU->m_par->QuadtreeTULog2MinSize)) >=
                pCU->m_par->Width)
            {
                outNeighborFlags[i] = 0;
            }
            else
            {
                if (aboveRightPartColumn < numMinTUInLCU)
                {
                    Ipp32s zScanIdx = h265_scan_r2z4[rasterTopRigthPartIdx + 1 + i - PITCH_TU];

                    if (zScanIdx < zScanTopRigthPartIdx)
                    {
                        if (ConstrainedIntraPredFlag)
                        {
                            outNeighborFlags[i] = ((pCU->m_data[zScanIdx].predMode == MODE_INTRA) ? 1 : 0);
                        }
                        else
                        {
                            outNeighborFlags[i] = 1;
                        }
                    }
                    else
                    {
                        outNeighborFlags[i] = 0;
                    }
                }
                else
                {
                    outNeighborFlags[i] = 0;
                }
            }
        }
    }
}

template <typename PixType>
static
void IsLeftAvailable(H265CU<PixType> *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s numMinTUInLCU = pCU->m_par->NumPartInCUSize;
    Ipp32s numMinTUInPU = width >> pCU->m_par->QuadtreeTULog2MinSize;
    Ipp32s rasterTopLeftPartIdx = h265_scan_z2r4[blockZScanIdx];
    Ipp32s topLeftPartRow = rasterTopLeftPartIdx >> 4;
    Ipp32s topLeftPartColumn = rasterTopLeftPartIdx & 15;
    Ipp32s i;

    if (topLeftPartColumn == 0) {
        for (i = numMinTUInPU - 1; i >= 0; i--) {
            outNeighborFlags[numMinTUInPU - 1 - i] = inNeighborFlags[numMinTUInLCU - 1 - (topLeftPartRow + i)];
        }
    }
    else {
        if (ConstrainedIntraPredFlag) {
            for (i = numMinTUInPU - 1; i >= 0; i--) {
                Ipp32s zScanIdx = h265_scan_r2z4[rasterTopLeftPartIdx - 1 + i * PITCH_TU];
                outNeighborFlags[numMinTUInPU - 1 - i] = ((pCU->m_data[zScanIdx].predMode == MODE_INTRA) ? 1 : 0);
            }
        }
        else {
            for (i = numMinTUInPU - 1; i >= 0; i--)
                outNeighborFlags[numMinTUInPU - 1 - i] = 1;
        }
    }
}

template <typename PixType>
static
void IsBelowLeftAvailable(H265CU<PixType> *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s numMinTUInLCU = pCU->m_par->NumPartInCUSize;
    Ipp32s numMinTUInPU = width >> pCU->m_par->QuadtreeTULog2MinSize;
    Ipp32s rasterBottomLeftPartIdx = h265_scan_z2r4[blockZScanIdx] + (numMinTUInPU - 1) * PITCH_TU;
    Ipp32s zScanBottomLefPartIdx = h265_scan_r2z4[rasterBottomLeftPartIdx];
    Ipp32s bottomLeftPartRow = rasterBottomLeftPartIdx >> 4;
    Ipp32s bottomLeftPartColumn = rasterBottomLeftPartIdx & 15;
    Ipp32s i;

    for (i = numMinTUInPU - 1; i >= 0; i--)
    {
        Ipp32s belowLefPartRow = bottomLeftPartRow + i + 1;

        if ((pCU->m_ctbPelY + (belowLefPartRow << pCU->m_par->QuadtreeTULog2MinSize)) >=
             pCU->m_par->Height)
        {
            outNeighborFlags[numMinTUInPU - 1 - i] = 0;
        }
        else if (belowLefPartRow < numMinTUInLCU)
        {
            if (bottomLeftPartColumn == 0)
            {
                outNeighborFlags[numMinTUInPU - 1 - i] = inNeighborFlags[numMinTUInLCU - 1 - belowLefPartRow];
            }
            else
            {
                Ipp32s zScanIdx = h265_scan_r2z4[rasterBottomLeftPartIdx+(i+1)*PITCH_TU-1];

                if (zScanIdx < zScanBottomLefPartIdx)
                {
                    if (ConstrainedIntraPredFlag)
                    {
                        outNeighborFlags[numMinTUInPU - 1 - i] = ((pCU->m_data[zScanIdx].predMode == MODE_INTRA) ? 1 : 0);
                    }
                    else
                    {
                        outNeighborFlags[numMinTUInPU - 1 - i] = 1;
                    }
                }
                else
                {
                    outNeighborFlags[numMinTUInPU - 1 - i] = 0;
                }

            }
        }
        else
        {
            outNeighborFlags[numMinTUInPU - 1 - i] = 0;
        }
    }
}

template <typename PixType>
static
void GetAvailablity(H265CU<PixType> *pCU,
                    Ipp32s blockZScanIdx,
                    Ipp32s width,
                    Ipp8u ConstrainedIntraPredFlag,
                    Ipp8u *inNeighborFlags,
                    Ipp8u *outNeighborFlags)
{
    Ipp32s numMinTUInPU = width >> pCU->m_par->QuadtreeTULog2MinSize;

    IsBelowLeftAvailable(pCU, blockZScanIdx, width, ConstrainedIntraPredFlag, inNeighborFlags, outNeighborFlags);
    outNeighborFlags += numMinTUInPU;

    IsLeftAvailable(pCU, blockZScanIdx, width, ConstrainedIntraPredFlag, inNeighborFlags, outNeighborFlags);
    outNeighborFlags += numMinTUInPU;

    IsAboveLeftAvailable(pCU, blockZScanIdx, width, ConstrainedIntraPredFlag, inNeighborFlags, outNeighborFlags);
    outNeighborFlags += 1;

    IsAboveAvailable(pCU, blockZScanIdx, width, ConstrainedIntraPredFlag, inNeighborFlags, outNeighborFlags);
    outNeighborFlags += numMinTUInPU;

    IsAboveRightAvailable(pCU, blockZScanIdx, width, ConstrainedIntraPredFlag, inNeighborFlags, outNeighborFlags);
}

template <typename PixType>
static
void GetPredPelsLuma(H265VideoParam *par, PixType* src, PixType* PredPel,
                     Ipp8u* neighborFlags, Ipp32s width, Ipp32s srcPitch)
{
    PixType* tmpSrcPtr;
    PixType dcval;
    Ipp32s minTUSize = 1 << par->QuadtreeTULog2MinSize;
    Ipp32s numUnitsInCU = width >> par->QuadtreeTULog2MinSize;
    Ipp32s numIntraNeighbor;
    Ipp32s i, j;

    dcval = (1 << (par->bitDepthLuma - 1));

    numIntraNeighbor = 0;

    for (i = 0; i < 4*numUnitsInCU + 1; i++)
    {
        if (neighborFlags[i])
            numIntraNeighbor++;
    }

    if (numIntraNeighbor == 0)
    {
        // Fill border with DC value

        for (i = 0; i <= 4*width; i++)
        {
            PredPel[i] = dcval;
        }
    }
    else if (numIntraNeighbor == (4*numUnitsInCU + 1))
    {
        // Fill top-left border with rec. samples
        tmpSrcPtr = src - srcPitch - 1;
        PredPel[0] = tmpSrcPtr[0];

        // Fill top and top right border with rec. samples
        tmpSrcPtr = src - srcPitch;
        for (i = 0; i < 2*width; i++)
        {
            PredPel[1+i] = tmpSrcPtr[i];
        }

        // Fill left and below left border with rec. samples
        tmpSrcPtr = src - 1;

        for (i = 0; i < 2*width; i++)
        {
            PredPel[2*width+1+i] = tmpSrcPtr[0];
            tmpSrcPtr += srcPitch;
        }
    }
    else // reference samples are partially available
    {
        Ipp8u      *pbNeighborFlags;
        Ipp32s     firstAvailableBlock;
        PixType availableRef = 0;

        // Fill top-left sample
        pbNeighborFlags = neighborFlags + 2*numUnitsInCU;

        if (*pbNeighborFlags)
        {
            tmpSrcPtr = src - srcPitch - 1;
            PredPel[0] = tmpSrcPtr[0];
        }

        // Fill left & below-left samples
        tmpSrcPtr = src - 1;

        pbNeighborFlags = neighborFlags + 2*numUnitsInCU - 1;

        for (j = 0; j < 2*numUnitsInCU; j++)
        {
            if (*pbNeighborFlags)
            {
                for (i = 0; i < minTUSize; i++)
                {
                    PredPel[2*width+1+j*minTUSize+i] = tmpSrcPtr[0];
                    tmpSrcPtr += srcPitch;
                }
            } else {
                tmpSrcPtr += srcPitch * minTUSize;
            }
            pbNeighborFlags --;
        }

        // Fill above & above-right samples
        tmpSrcPtr = src - srcPitch;
        pbNeighborFlags = neighborFlags + 2*numUnitsInCU + 1;

        for (j = 0; j < 2*numUnitsInCU; j++)
        {
            if (*pbNeighborFlags)
            {
                for (i = 0; i < minTUSize; i++)
                {
                    PredPel[1+j*minTUSize+i] = tmpSrcPtr[i];
                }
            }
            tmpSrcPtr += minTUSize;
            pbNeighborFlags ++;
        }

        /* Search first available block */
        firstAvailableBlock = 0;

        for (i = 0; i < 4*numUnitsInCU + 1; i++)
        {
            if (neighborFlags[i])
                break;

            firstAvailableBlock++;
        }

        /* Search first available sample */
        if (firstAvailableBlock != 0)
        {
            if (firstAvailableBlock < 2*numUnitsInCU) /* left & below-left */
            {
                availableRef = PredPel[4*width-firstAvailableBlock*minTUSize];

            }
            else if (firstAvailableBlock == 2*numUnitsInCU) /* top-left */
            {
                availableRef = PredPel[0];

            }
            else
            {
                availableRef = PredPel[(firstAvailableBlock-2*numUnitsInCU-1)*minTUSize+1];
            }
        }

        /* left & below-left samples */
        for (j = 0; j < 2*numUnitsInCU; j++)
        {
            if (!neighborFlags[j])
            {
                if (j != 0)
                {
                    availableRef = PredPel[4*width+1-j*minTUSize];
                }

                for (i = 0; i < minTUSize; i++)
                {
                    PredPel[4*width+1-(j+1)*minTUSize+i] = availableRef;
                }
            }
        }

        /* top-left sample */
        if (!neighborFlags[2*numUnitsInCU])
        {
            PredPel[0] = PredPel[2*width+1];
        }

        /* above & above-right samples */
        for (j = 0; j < 2*numUnitsInCU; j++)
        {
            if (!neighborFlags[j+2*numUnitsInCU+1])
            {
                availableRef = PredPel[j*minTUSize];

                for (i = 0; i < minTUSize; i++)
                {
                    PredPel[1+j*minTUSize+i] = availableRef;
                }
            }
        }
    }
}

template <typename PixType>
static
void GetPredPelsChromaNV12(H265VideoParam *par, PixType* src, PixType* PredPel,
                           Ipp8u* neighborFlags, Ipp32s width, Ipp32s srcPitch)
{
    PixType* tmpSrcPtr;
    PixType dcval = (1 << (par->bitDepthChroma - 1));
    Ipp32s minTUSize = (1 << par->QuadtreeTULog2MinSize) >> 1;
    Ipp32s numUnitsInCU = width >> (par->QuadtreeTULog2MinSize - 1);
    Ipp32s numIntraNeighbor;
    Ipp32s i, j;
    numIntraNeighbor = 0;

    for (i = 0; i < 4*numUnitsInCU + 1; i++) {
        if (neighborFlags[i])
            numIntraNeighbor++;
    }

    if (numIntraNeighbor == 0) {
        // Fill border with DC value
        _ippsSet(dcval, PredPel, 4 * 2 * width + 2);

    } else if (numIntraNeighbor == (4*numUnitsInCU + 1)) {
        // Fill top-left border with rec. samples
        // Fill top and top right border with rec. samples
        _ippsCopy(src - srcPitch - 2, PredPel, 4 * width + 2);

        // Fill left and below left border with rec. samples
        tmpSrcPtr = src - 2;
        for (i = 0; i < 2 * width; i++) {
            PredPel[2 * 2 * width + 2 + 2 * i + 0] = tmpSrcPtr[0];
            PredPel[2 * 2 * width + 2 + 2 * i + 1] = tmpSrcPtr[1];
            tmpSrcPtr += srcPitch;
        }

    } else { // reference samples are partially available
        Ipp8u  *pbNeighborFlags;
        Ipp32s firstAvailableBlock;

        // Fill top-left sample
        pbNeighborFlags = neighborFlags + 2*numUnitsInCU;

        if (*pbNeighborFlags) {
            tmpSrcPtr = src - srcPitch - 2;
            PredPel[0] = tmpSrcPtr[0];
            PredPel[1] = tmpSrcPtr[1];
        }

        // Fill left & below-left samples
        tmpSrcPtr = src - 2;

        pbNeighborFlags = neighborFlags + 2*numUnitsInCU - 1;

        for (j = 0; j < 2*numUnitsInCU; j++) {
            if (*pbNeighborFlags) {
                for (i = 0; i < minTUSize; i++) {
                    PredPel[2 * 2 * width + 2 + j * 2 * minTUSize + 2 * i + 0] = tmpSrcPtr[0];
                    PredPel[2 * 2 * width + 2 + j * 2 * minTUSize + 2 * i + 1] = tmpSrcPtr[1];
                    tmpSrcPtr += srcPitch;
                }
            } else {
                tmpSrcPtr += srcPitch * minTUSize;
            }
            pbNeighborFlags --;
        }

        // Fill above & above-right samples
        tmpSrcPtr = src - srcPitch;
        pbNeighborFlags = neighborFlags + 2*numUnitsInCU + 1;

        for (j = 0; j < 2*numUnitsInCU; j++) {
            if (*pbNeighborFlags) {
                for (i = 0; i < minTUSize; i++) {
                    PredPel[2 + j * 2 * minTUSize + 2 * i + 0] = tmpSrcPtr[2 * i + 0];
                    PredPel[2 + j * 2 * minTUSize + 2 * i + 1] = tmpSrcPtr[2 * i + 1];
                }
            }
            tmpSrcPtr += 2 * minTUSize;
            pbNeighborFlags ++;
        }

        /* Search first available block */
        firstAvailableBlock = 0;

        for (i = 0; i < 4*numUnitsInCU + 1; i++) {
            if (neighborFlags[i])
                break;
            firstAvailableBlock++;
        }

        /* Search first available sample */
        PixType availableRef[2] = { 0, 0 };
        if (firstAvailableBlock != 0) {
            if (firstAvailableBlock < 2 * numUnitsInCU) { /* left & below-left */
                availableRef[0] = PredPel[4 * 2 * width - firstAvailableBlock * 2 * minTUSize + 0];
                availableRef[1] = PredPel[4 * 2 * width - firstAvailableBlock * 2 * minTUSize + 1];
            } else if (firstAvailableBlock == 2 * numUnitsInCU) { /* top-left */
                availableRef[0] = PredPel[0];
                availableRef[1] = PredPel[1];
            } else {
                availableRef[0] = PredPel[(firstAvailableBlock - 2 * numUnitsInCU - 1) * 2 * minTUSize + 2 + 0];
                availableRef[1] = PredPel[(firstAvailableBlock - 2 * numUnitsInCU - 1) * 2 * minTUSize + 2 + 1];
            }
        }

        /* left & below-left samples */
        for (j = 0; j < 2*numUnitsInCU; j++) {
            if (!neighborFlags[j]) {
                if (j != 0) {
                    availableRef[0] = PredPel[4 * 2 * width + 2 - j * 2 * minTUSize + 0];
                    availableRef[1] = PredPel[4 * 2 * width + 2 - j * 2 * minTUSize + 1];
                }
                for (i = 0; i < minTUSize; i++) {
                    PredPel[4 * 2 * width + 2 - (j + 1) * 2 * minTUSize + 2 * i + 0] = availableRef[0];
                    PredPel[4 * 2 * width + 2 - (j + 1) * 2 * minTUSize + 2 * i + 1] = availableRef[1];
                }
            }
        }

        /* top-left sample */
        if (!neighborFlags[2 * numUnitsInCU]) {
            PredPel[0] = PredPel[2 * 2 * width + 2 + 0];
            PredPel[1] = PredPel[2 * 2 * width + 2 + 1];
        }

        /* above & above-right samples */
        for (j = 0; j < 2 * numUnitsInCU; j++) {
            if (!neighborFlags[j + 2 * numUnitsInCU + 1]) {
                availableRef[0] = PredPel[j * 2 * minTUSize + 0];
                availableRef[1] = PredPel[j * 2 * minTUSize + 1];
                for (i = 0; i < minTUSize; i++) {
                    PredPel[2 + j * 2 * minTUSize + 2 * i + 0] = availableRef[0];
                    PredPel[2 + j * 2 * minTUSize + 2 * i + 1] = availableRef[1];
                }
            }
        }

    }
}

template <typename PixType>
void H265CU<PixType>::IntraPredTu(Ipp32s blockZScanIdx, Ipp32s width, Ipp32s pred_mode, Ipp8u is_luma)
{
    PixType PredPel[4*2*64+1];
    Ipp32s PURasterIdx = h265_scan_z2r4[blockZScanIdx];
    Ipp32s PUStartRow = PURasterIdx >> 4;
    Ipp32s PUStartColumn = PURasterIdx & 15;
    PixType *pRec;

    Ipp32s pitch = m_pitchRec;

    if (is_luma)
    {
        bool isFilterNeeded = true;

        if (pred_mode == INTRA_DC)
        {
            isFilterNeeded = false;
        }
        else
        {
            Ipp32s diff = MIN(abs(pred_mode - INTRA_HOR), abs(pred_mode - INTRA_VER));

            if (diff <= h265_filteredModes[h265_log2m2[width]])
            {
                isFilterNeeded = false;
            }
        }

        pRec = m_yRec + ((PUStartRow * m_pitchRec + PUStartColumn) << m_par->QuadtreeTULog2MinSize);

        GetAvailablity(this, blockZScanIdx, width, m_par->cpps->constrained_intra_pred_flag,
            m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsLuma(m_par, pRec, PredPel, m_outNeighborFlags, width, m_pitchRec);

        if (m_par->csps->strong_intra_smoothing_enabled_flag && isFilterNeeded)
        {
            Ipp32s threshold = 1 << (m_par->bitDepthLuma - 5);

            Ipp32s topLeft = PredPel[0];
            Ipp32s topRight = PredPel[2*width];
            Ipp32s midHor = PredPel[width];

            Ipp32s bottomLeft = PredPel[4*width];
            Ipp32s midVer = PredPel[3*width];

            bool bilinearLeft = abs(topLeft + topRight - 2*midHor) < threshold;
            bool bilinearAbove = abs(topLeft + bottomLeft - 2*midVer) < threshold;

            if (width == 32 && (bilinearLeft && bilinearAbove))
            {
                h265_FilterPredictPels_Bilinear(PredPel, width, topLeft, bottomLeft, topRight);
            }
            else
            {
                h265_FilterPredictPels(PredPel, width);
            }
        } else if (isFilterNeeded) {
            h265_FilterPredictPels(PredPel, width);
        }

        switch(pred_mode)
        {
        case INTRA_PLANAR:
            h265_PredictIntra_Planar(PredPel, pRec, pitch, width);
            break;
        case INTRA_DC:
            h265_PredictIntra_DC(PredPel, pRec, pitch, width, is_luma);
            break;
        case INTRA_VER:
            h265_PredictIntra_Ver(PredPel, pRec, pitch, width, m_par->bitDepthLuma, is_luma);
            break;
        case INTRA_HOR:
            h265_PredictIntra_Hor(PredPel, pRec, pitch, width, m_par->bitDepthLuma, is_luma);
            break;
        default:
            h265_PredictIntra_Ang(pred_mode, PredPel, pRec, pitch, width);
        }
    }
    else
    {
        pRec = m_uvRec + ((PUStartRow * m_pitchRec + PUStartColumn * 2) << (m_par->QuadtreeTULog2MinSize - 1));
        GetAvailablity(this, blockZScanIdx, width << 1, m_par->cpps->constrained_intra_pred_flag,
            m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsChromaNV12(m_par, pRec, PredPel, m_outNeighborFlags, width, pitch);

        switch(pred_mode)
        {
        case INTRA_PLANAR:
            h265_PredictIntra_Planar_ChromaNV12(PredPel, pRec, pitch, width);
            break;
        case INTRA_DC:
            h265_PredictIntra_DC_ChromaNV12(PredPel, pRec, pitch, width);
            break;
        case INTRA_VER:
            h265_PredictIntra_Ver_ChromaNV12(PredPel, pRec, pitch, width);
            break;
        case INTRA_HOR:
            h265_PredictIntra_Hor_ChromaNV12(PredPel, pRec, pitch, width);
            break;
        default:
            h265_PredictIntra_Ang_ChromaNV12(PredPel, pRec, pitch, width, pred_mode);
        }
    }
}

template <typename PixType>
Ipp32s GetIntraLumaBitCost(H265CU<PixType> * cu, Ipp32u abs_part_idx)
{
    if (!cu->m_rdOptFlag)
        return 0;

    CABAC_CONTEXT_H265 intra_luma_pred_mode_ctx = cu->m_bsf->m_base.context_array[tab_ctxIdxOffset[INTRA_LUMA_PRED_MODE_HEVC]];
    H265CUData *old_data = cu->m_data; // [NS]: is this trick actually needed?

    cu->m_bsf->Reset();
    cu->m_data = cu->m_dataSave;
    cu->CodeIntradirLumaAng(cu->m_bsf, abs_part_idx, false);

    // restore everything
    cu->m_bsf->m_base.context_array[tab_ctxIdxOffset[INTRA_LUMA_PRED_MODE_HEVC]] = intra_luma_pred_mode_ctx;
    cu->m_data = old_data;

    return (Ipp32s)cu->m_bsf->GetNumBits();
}


#define HM_MATCH_1 0
//kolya
//just copied from HM's code to check compression gain
//todo: optimize
#if HM_MATCH_1
Ipp32s StoreFewBestModes(CostType cost, Ipp8u mode, CostType * costs, Ipp8u * modes, Ipp32s num_costs)
{
    Ipp32s idx = 0;

    while (idx < num_costs && cost < costs[num_costs - 1 - idx]) idx++;

    if (idx > 0)
    {
        for(Ipp32s i=1; i < idx; i++)
        {
            costs[num_costs - i] = costs[num_costs - i - 1];
            modes[num_costs - i] = modes[num_costs - i - 1];
        }
        costs[num_costs - idx] = cost;
        modes[num_costs - idx] = mode;
        return 1;
    }

    return 0; // nothing inserted
}
#else
Ipp32s StoreFewBestModes(CostType cost, Ipp8u mode, CostType * costs, Ipp8u * modes, Ipp32s num_costs)
{
    CostType max_cost = -1;
    Ipp32s max_cost_idx = 0;
    for (Ipp32s i = 0; i < num_costs; i++) {
        if (max_cost < costs[i]) {
            max_cost = costs[i];
            max_cost_idx = i;
        }
    }

    if (max_cost >= cost) {
        costs[max_cost_idx] = cost;
        modes[max_cost_idx] = mode;
        return max_cost_idx;
    }
    return num_costs; // nothing inserted
}
#endif

void SortLumaModesByCost(CostType *costs, Ipp8u *modes, Ipp32s *bits, Ipp32s numCandInput, Ipp32s numCandSorted)
{
    VM_ASSERT(numCandInput >= numCandSorted);
    for (Ipp32s i = 0; i < numCandSorted; i++) {
        Ipp32s idx = (Ipp32s)(std::min_element(costs + i, costs + numCandInput) - costs);
        std::swap(costs[i], costs[idx]);
        std::swap(modes[i], modes[idx]);
        std::swap(bits[i], bits[idx]);
    }
}

#define NO_TRANSFORM_SPLIT_INTRAPRED_STAGE1 0
template <typename PixType>
void H265CU<PixType>::IntraLumaModeDecision(Ipp32s absPartIdx, Ipp32u offset, Ipp8u depth, Ipp8u trDepth)
{
    __ALIGN32 PixType predPel[4*64+1];
    __ALIGN32 PixType predPelFilt[4*64+1];
    Ipp32s width = m_par->MaxCUSize >> (depth + trDepth);
    Ipp8u  partSize = (Ipp8u)(trDepth == 1 ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
    Ipp32s puRasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s puStartRow = puRasterIdx >> 4;
    Ipp32s puStartCol = puRasterIdx & 15;

    PixType *src = m_ySrc + ((puStartRow * m_pitchSrc + puStartCol) << m_par->QuadtreeTULog2MinSize);
    PixType *rec = m_yRec + ((puStartRow * m_pitchRec + puStartCol) << m_par->QuadtreeTULog2MinSize);

    std::fill_n(m_intraCosts, 35, COST_MAX);

    Ipp8u trSplitMode = GetTrSplitMode(absPartIdx, depth, trDepth, partSize, !NO_TRANSFORM_SPLIT_INTRAPRED_STAGE1);
    m_predIntraAllWidth = 0;

#define SQRT_LAMBDA 0
#if SQRT_LAMBDA
    Ipp64f lambdaSatd = m_rdLambdaSqrt / 256.0;
#else
    Ipp64f lambdaSatd = m_rdLambda;
#endif

    Ipp8s angModeCand[33];
    Ipp32s numAngModeCand = 33;

    if (m_par->intraAngModes == 3) {
        Ipp32s xPu = m_ctbPelX + (puStartCol << m_par->QuadtreeTULog2MinSize);
        Ipp32s yPu = m_ctbPelY + (puStartRow << m_par->QuadtreeTULog2MinSize);
        Ipp32s log2BlockSize = m_par->Log2MaxCUSize - depth - trDepth;
        numAngModeCand = m_par->num_cand_0[log2BlockSize];
        GetAngModesFromHistogram(xPu, yPu, 1 << log2BlockSize, angModeCand, numAngModeCand);
    }

    if (trSplitMode != SPLIT_MUST) {
        m_predIntraAllWidth = width;
        GetAvailablity(this, absPartIdx, width, m_par->cpps->constrained_intra_pred_flag, m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsLuma(m_par, rec, predPel, m_outNeighborFlags, width, m_pitchRec);

        small_memcpy(predPelFilt, predPel, 4 * width + 1);

        if (m_par->csps->strong_intra_smoothing_enabled_flag && width == 32)
        {
            Ipp32s threshold = 1 << (m_par->bitDepthLuma - 5);
            Ipp32s topLeft = predPel[0];
            Ipp32s topRight = predPel[2*width];
            Ipp32s midHor = predPel[width];
            Ipp32s bottomLeft = predPel[4*width];
            Ipp32s midVer = predPel[3*width];
            bool bilinearLeft = abs(topLeft + topRight - 2 * midHor) < threshold;
            bool bilinearAbove = abs(topLeft + bottomLeft - 2 * midVer) < threshold;

            if (bilinearLeft && bilinearAbove)
                h265_FilterPredictPels_Bilinear(predPelFilt, width, topLeft, bottomLeft, topRight);
            else
                h265_FilterPredictPels(predPelFilt, width);
        } else {
            h265_FilterPredictPels(predPelFilt, width);
        }

        IppiSize roi = {width, width};
        _ippiTranspose_C1R(src, m_pitchSrc, m_tuSrcTransposed, width, roi);

        PixType *predPtr = m_predIntraAll;

        bool needFilter = (INTRA_HOR > h265_filteredModes[h265_log2m2[width]]);
        PixType *predPels = needFilter ? predPelFilt : predPel;
        h265_PredictIntra_Planar(predPels, predPtr, width, width);
        m_intraCosts[INTRA_PLANAR] = tuHad(src, m_pitchSrc, predPtr, width, width, width);
        predPtr += width * width;
        m_data = m_dataSave;
        FillSubPart(absPartIdx, depth, trDepth, partSize, INTRA_PLANAR, m_par->m_lcuQps[m_ctbAddr]);
        m_intraBits[INTRA_PLANAR] = GetIntraLumaBitCost(this, absPartIdx);
        m_intraCosts[INTRA_PLANAR] += m_intraBits[INTRA_PLANAR] * lambdaSatd;
        m_intraModes[INTRA_PLANAR] = INTRA_PLANAR;

        h265_PredictIntra_DC(predPel, predPtr, width, width, 1);
        m_intraCosts[INTRA_DC] = tuHad(src, m_pitchSrc, predPtr, width, width, width);
        predPtr += width * width;
        FillSubPartIntraLumaDir(absPartIdx, depth, trDepth, INTRA_DC);
        m_intraBits[INTRA_DC] = GetIntraLumaBitCost(this, absPartIdx);
        m_intraCosts[INTRA_DC] += m_intraBits[INTRA_DC] * lambdaSatd;
        m_intraModes[INTRA_DC] = INTRA_DC;

        if (m_par->intraAngModes == 3) {
            for (Ipp32s i = 0; i < numAngModeCand; i++) {
                Ipp8s lumaDir = angModeCand[i];

                Ipp32s diff = MIN(abs(lumaDir - 10), abs(lumaDir - 26));
                bool needFilter = (diff > h265_filteredModes[h265_log2m2[width]]);
                PixType *predPels = needFilter ? predPelFilt : predPel;
                PixType *predPtr = m_predIntraAll + lumaDir * width * width;

                h265_PredictIntra_Ang_NoTranspose(lumaDir, predPels, predPtr,
                                                                        width, width);
                m_intraCosts[lumaDir] = (lumaDir < 18)
                    ? tuHad(m_tuSrcTransposed, width, predPtr, width, width, width)
                    : tuHad(src, m_pitchSrc, predPtr, width, width, width);

                FillSubPartIntraLumaDir(absPartIdx, depth, trDepth, lumaDir);
                m_intraBits[lumaDir] = GetIntraLumaBitCost(this, absPartIdx);
                m_intraCosts[lumaDir] += m_intraBits[lumaDir] * lambdaSatd;
                m_intraModes[lumaDir] = lumaDir;
            }
        }
        else {
            (m_par->intraAngModes == 2)
                ? h265_PredictIntra_Ang_All_Even(predPel, predPelFilt, predPtr, width)
                : h265_PredictIntra_Ang_All(predPel, predPelFilt, predPtr, width, m_par->bitDepthLuma);

            Ipp8u step = (Ipp8u)m_par->intraAngModes;
            for (Ipp8u lumaDir = 2; lumaDir < 35; lumaDir += step, predPtr += width * width * step) {
                m_intraCosts[lumaDir] = (lumaDir < 18)
                    ? tuHad(m_tuSrcTransposed, width, predPtr, width, width, width)
                    : tuHad(src, m_pitchSrc, predPtr, width, width, width);
                FillSubPartIntraLumaDir(absPartIdx, depth, trDepth, lumaDir);
                m_intraBits[lumaDir] = GetIntraLumaBitCost(this, absPartIdx);
                m_intraCosts[lumaDir] += m_intraBits[lumaDir] * lambdaSatd;
                m_intraModes[lumaDir] = lumaDir;
            }
        }
    }
    else
    {
        m_data = m_dataSave;
        FillSubPart(absPartIdx, depth, trDepth, partSize, INTRA_PLANAR, m_par->m_lcuQps[m_ctbAddr]);
        CalcCostLuma(absPartIdx, offset, depth, trDepth, COST_PRED_TR_0, partSize, INTRA_PLANAR,
                     m_intraCosts + INTRA_PLANAR);
        m_intraBits[INTRA_PLANAR] = GetIntraLumaBitCost(this, absPartIdx);
        m_intraCosts[INTRA_PLANAR] += m_intraBits[INTRA_PLANAR] * lambdaSatd;
        m_intraModes[INTRA_PLANAR] = INTRA_PLANAR;

        CalcCostLuma(absPartIdx, offset, depth, trDepth, COST_PRED_TR_0, partSize, INTRA_DC,
                     m_intraCosts + INTRA_DC);
        m_intraBits[INTRA_DC] = GetIntraLumaBitCost(this, absPartIdx);
        m_intraCosts[INTRA_DC] += m_intraBits[INTRA_DC] * lambdaSatd;
        m_intraModes[INTRA_DC] = INTRA_DC;

        if (m_par->intraAngModes == 3) {
            for (Ipp32s i = 0; i < numAngModeCand; i++) {
                Ipp8s lumaDir = angModeCand[i];
                CalcCostLuma(absPartIdx, offset, depth, trDepth, COST_PRED_TR_0, partSize, lumaDir,
                             m_intraCosts + lumaDir);
                m_intraBits[lumaDir] = GetIntraLumaBitCost(this, absPartIdx);
                m_intraCosts[lumaDir] += m_intraBits[lumaDir] * lambdaSatd;
                m_intraModes[lumaDir] = lumaDir;
            }
        }
        else {
            Ipp8u step = (Ipp8u)m_par->intraAngModes;
            for (Ipp8u lumaDir = 2; lumaDir <= 34; lumaDir += step) {
                CalcCostLuma(absPartIdx, offset, depth, trDepth, COST_PRED_TR_0, partSize, lumaDir,
                             m_intraCosts + lumaDir);
                m_intraBits[lumaDir] = GetIntraLumaBitCost(this, absPartIdx);
                m_intraCosts[lumaDir] += m_intraBits[lumaDir] * lambdaSatd;
                m_intraModes[lumaDir] = lumaDir;
            }
        }
    }

    Ipp32s numCand1 = m_par->num_cand_1[m_par->Log2MaxCUSize - depth - trDepth];
    SortLumaModesByCost(m_intraCosts, m_intraModes, m_intraBits, 35, numCand1);

    if (m_par->intraAngModes == 2)
    {
        // select odd angular modes for refinement stage
        Ipp32s numOddModes = 0;
        for (Ipp8u oddMode = 3; oddMode < 35; oddMode += 2) {
            for (Ipp32s i = 0; i < numCand1; i++) {
                if (oddMode + 1 == m_intraModes[i] || oddMode == m_intraModes[i] + 1) {
                    m_intraModes[numCand1 + numOddModes++] = oddMode;
                    break;
                }
            }
        }

        if (trSplitMode != SPLIT_MUST) {
            for (Ipp32s i = numCand1; i < numCand1 + numOddModes; i++) {
                Ipp8u lumaDir = m_intraModes[i];
                Ipp32s diff = MIN(abs(lumaDir - 10), abs(lumaDir - 26));
                bool needFilter = (diff > h265_filteredModes[h265_log2m2[width]]);
                PixType *predPels = needFilter ? predPelFilt : predPel;
                PixType *predPtr = m_predIntraAll + lumaDir * width * width;

                h265_PredictIntra_Ang_NoTranspose(lumaDir, predPels, predPtr, width, width);
                m_intraCosts[i] = (lumaDir < 18)
                    ? tuHad(m_tuSrcTransposed, width, predPtr, width, width, width)
                    : tuHad(src, m_pitchSrc, predPtr, width, width, width);

                FillSubPartIntraLumaDir(absPartIdx, depth, trDepth, lumaDir);
                m_intraBits[i] = GetIntraLumaBitCost(this, absPartIdx);
                m_intraCosts[i] += m_intraBits[i] * lambdaSatd;
            }
        }
        else {
            for (Ipp32s i = numCand1; i < numCand1 + numOddModes; i++) {
                Ipp8u lumaDir = m_intraModes[i];
                CalcCostLuma(absPartIdx, offset, depth, trDepth, COST_PRED_TR_0, partSize, lumaDir,
                             m_intraCosts + i);
                m_intraBits[i] = GetIntraLumaBitCost(this, absPartIdx);
                m_intraCosts[i] += m_intraBits[i] * lambdaSatd;
            }
        }

        SortLumaModesByCost(m_intraCosts, m_intraModes, m_intraBits, numCand1 + numOddModes, numCand1);
    }
}


template <typename PixType>
void H265CU<PixType>::IntraLumaModeDecisionRDO(Ipp32s absPartIdx, Ipp32u offset, Ipp8u depth, Ipp8u trDepth)
{
    Ipp32u numParts = (m_par->NumPartInCU >> ((depth + trDepth) << 1));
    Ipp8u  partSize = (Ipp8u)(trDepth == 1 ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
    Ipp32s numCand1 = m_par->num_cand_1[m_par->Log2MaxCUSize - (depth + trDepth)];
    Ipp32s numCand2 = m_par->num_cand_2[m_par->Log2MaxCUSize - (depth + trDepth)];
#define HM_MATCH_3 0
#if HM_MATCH_3
    Ipp32s numCand2 = numCand1;
#endif

    // FAST_UDI_USE_MPM //kolya
    if(0) {
        Ipp8u  uiPreds[3] = {0xFF, 0xFF, 0xFF}; //all 3 values will be changed to different 
        Ipp32s iMode = GetIntradirLumaPred(absPartIdx, uiPreds);
        for(Ipp32s j = 0; j < iMode; j++) {
            Ipp32s isInList = 0;
            for(Ipp32s i = 0; i < numCand1; i++) 
                isInList |= (uiPreds[j] == this->m_intraModes[i]); 
                if (!isInList) {
                    this->m_intraModes[numCand1++] = uiPreds[j];
                    this->m_intraCosts[numCand1] = this->m_intraCosts[uiPreds[j]];
                    this->m_intraBits[numCand1] = this->m_intraBits[uiPreds[j]];
                }
            } 
    } // <-- end of FAST_UDI_USE_MPM

    CABAC_CONTEXT_H265 initCtx[NUM_CABAC_CONTEXT];
    CABAC_CONTEXT_H265 bestCtx[NUM_CABAC_CONTEXT];
    CostOpt finalRdoCostOpt = COST_REC_TR_ALL;

    Ipp32s tuSplitIntra = (m_par->tuSplitIntra == 1 ||
                           m_par->tuSplitIntra == 3 && m_cslice->slice_type == I_SLICE);
    if (!tuSplitIntra) {
        finalRdoCostOpt = COST_REC_TR_0;
        numCand2 = numCand1;
    }

    if (m_rdOptFlag)
        m_bsf->CtxSave(initCtx, 0, NUM_CABAC_CONTEXT);

    if (numCand1 > numCand2) { // REC_TR_0 stage makes sense only of it reduce number of candidates
        for (Ipp8u i = 0; i < numCand1; i++) {
            Ipp8u lumaDir = m_intraModes[i];
            if (m_rdOptFlag)
                m_bsf->CtxRestore(initCtx, 0, NUM_CABAC_CONTEXT);
            CalcCostLuma(absPartIdx, offset, depth, trDepth, COST_REC_TR_0, partSize, lumaDir,
                         m_intraCosts + i);
            m_intraCosts[i] += m_intraBits[i] * m_rdLambda;
        }

        SortLumaModesByCost(m_intraCosts, m_intraModes, m_intraBits, numCand1, numCand2);
    }

    Ipp32s width = m_par->MaxCUSize >> (depth + trDepth);
    IppiSize roi = { width, width };
    H265CUData *dataTemp = m_dataTemp + ((depth + trDepth) << m_par->Log2NumPartInCU) + absPartIdx;
    H265CUData *dataBest = m_dataBest + ((depth + trDepth) << m_par->Log2NumPartInCU) + absPartIdx;
    Ipp32s puRasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s puStartRow = puRasterIdx >> 4;
    Ipp32s puStartColumn = puRasterIdx & 15;
    PixType *rec = m_yRec + ((puStartRow * m_pitchRec + puStartColumn) << m_par->QuadtreeTULog2MinSize);

    m_intraCosts[0] = COST_MAX;
    bool lastCandIsBest = 0;
    for (Ipp32s i = 0; i < numCand2; i++) {
        Ipp8u lumaDir = m_intraModes[i];
        if (m_rdOptFlag)
            m_bsf->CtxRestore(initCtx, 0, NUM_CABAC_CONTEXT);
        CostType cost;
        CalcCostLuma(absPartIdx, offset, depth, trDepth, finalRdoCostOpt, partSize, lumaDir, &cost);
        cost += m_intraBits[i] * m_rdLambda;

        if (m_intraCosts[0] > cost) {
            m_intraCosts[0] = cost;
            m_intraModes[0] = lumaDir;
            m_intraBits[0] = m_intraBits[i];
            small_memcpy(dataBest, dataTemp, numParts * sizeof(H265CUData));
            lastCandIsBest = (i == numCand2 - 1);
            if (!lastCandIsBest) { // save current best
                if (m_rdOptFlag)
                    m_bsf->CtxSave(bestCtx, 0, NUM_CABAC_CONTEXT);
                _ippiCopy_C1R(rec, m_pitchRec, m_recLumaSaveCu[depth + trDepth], width, roi);
            }
        }
    }

    if (!lastCandIsBest) {
        if (m_rdOptFlag)
            m_bsf->CtxRestore(bestCtx, 0, NUM_CABAC_CONTEXT);
        _ippiCopy_C1R(m_recLumaSaveCu[depth + trDepth], width, rec, m_pitchRec, roi);
    }
}

// Luma only
template <typename PixType>
Ipp8u H265CU<PixType>::GetTrSplitMode(Ipp32s abs_part_idx, Ipp8u depth, Ipp8u tr_depth, Ipp8u part_size, Ipp8u strict)
{
    if (m_par->Log2MaxCUSize - depth - tr_depth > 5)
        return SPLIT_MUST;
    if (m_par->Log2MaxCUSize - depth - tr_depth > 2) {
        Ipp32u tuLog2MinSizeInCu = H265Enc::GetQuadtreeTuLog2MinSizeInCu(m_par, m_par->Log2MaxCUSize - depth, part_size, MODE_INTRA);
        if (m_par->Log2MaxCUSize - depth - tr_depth > tuLog2MinSizeInCu) {
            if (strict && m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MaxSize)
                return SPLIT_MUST;
            Ipp32u lpel_x = m_ctbPelX + ((h265_scan_z2r4[abs_part_idx] & 15) << m_par->QuadtreeTULog2MinSize);
            Ipp32u tpel_y = m_ctbPelY + ((h265_scan_z2r4[abs_part_idx] >> 4) << m_par->QuadtreeTULog2MinSize);
            if (lpel_x + width > m_par->Width || tpel_y + width > m_par->Height)
                return SPLIT_MUST;
            return SPLIT_TRY;
        }
    }
    return SPLIT_NONE;
}


template <typename PixType>
void H265CU<PixType>::GetInitAvailablity()
{
    Ipp32s numMinTUInLCU = m_par->NumPartInCUSize;

    Ipp32s rasterIdx, zScanIdx;
    Ipp32s i;
    Ipp8u constrained_intra_flag = m_par->cpps->constrained_intra_pred_flag;

    for (i = 0; i < 3 * numMinTUInLCU + 1; i++)
    {
        m_inNeighborFlags[i] = 0;
    }

    /* Left */
    if (m_leftAddr >= m_cslice->slice_segment_address /* and in one tile */)
    {
        for (i = 0; i < numMinTUInLCU; i++)
        {
            //rasterIdx = (numMinTUInLCU - i) * numMinTUInLCU - 1;
            rasterIdx = i * PITCH_TU + numMinTUInLCU - 1;
            zScanIdx = h265_scan_r2z4[rasterIdx];

            if (constrained_intra_flag)
            {
                if (m_left[zScanIdx].predMode == MODE_INTRA)
                {
                    m_inNeighborFlags[i] = 1;
                }
            }
            else
            {
                m_inNeighborFlags[i] = 1;
            }
        }
    }

    /* Above Left */
    if (m_aboveLeftAddr >= m_cslice->slice_segment_address /* and in one tile */)
    {
        //rasterIdx = numMinTUInLCU * numMinTUInLCU - 1;
        rasterIdx = (numMinTUInLCU - 1) * PITCH_TU + numMinTUInLCU - 1;
        zScanIdx = h265_scan_r2z4[rasterIdx];

        if (constrained_intra_flag)
        {
            if (m_aboveLeft[zScanIdx].predMode == MODE_INTRA)
            {
                m_inNeighborFlags[numMinTUInLCU] = 1;
            }
        }
        else
        {
            m_inNeighborFlags[numMinTUInLCU] = 1;
        }
    }

    /* Above */
    if (m_aboveAddr >= m_cslice->slice_segment_address /* and in one tile */)
    {
        for (i = 0; i < numMinTUInLCU; i++)
        {
            //rasterIdx = (numMinTUInLCU - 1) * numMinTUInLCU + i;
            rasterIdx = (numMinTUInLCU - 1) * PITCH_TU + i;
            zScanIdx = h265_scan_r2z4[rasterIdx];

            if (constrained_intra_flag)
            {
                if (m_above[zScanIdx].predMode == MODE_INTRA)
                {
                    m_inNeighborFlags[numMinTUInLCU+1+i] = 1;
                }
            }
            else
            {
                m_inNeighborFlags[numMinTUInLCU+1+i] = 1;
            }
        }
    }

    /* Above Right*/
    if (m_aboveRightAddr >= m_cslice->slice_segment_address /* and in one tile */)
    {
        for (i = 0; i < numMinTUInLCU; i++)
        {
            //rasterIdx = (numMinTUInLCU - 1) * numMinTUInLCU + i;
            rasterIdx = (numMinTUInLCU - 1) * PITCH_TU + i;
            zScanIdx = h265_scan_r2z4[rasterIdx];

            if (constrained_intra_flag)
            {
                if (m_aboveRight[zScanIdx].predMode == MODE_INTRA)
                {
                    m_inNeighborFlags[2*numMinTUInLCU+1+i] = 1;
                }
            }
            else
            {
                m_inNeighborFlags[2*numMinTUInLCU+1+i] = 1;
            }
        }
    }
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
