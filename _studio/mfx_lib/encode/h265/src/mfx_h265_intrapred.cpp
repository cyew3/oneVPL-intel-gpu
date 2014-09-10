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

#if defined (MFX_VA)
#include "mfx_h265_enc_cm_defs.h"
#endif // MFX_VA

namespace H265Enc {

#if defined (MFX_VA)
    extern mfxU32 width;
    extern mfxU32 height;
    extern CmMbIntraGrad * cmMbIntraGrad4x4[2];
    extern CmMbIntraGrad * cmMbIntraGrad8x8[2];
    extern Ipp32s cmCurIdx;
#endif // MFX_VA


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
                      Ipp8u shiftW,
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
            outNeighborFlags[i] = inNeighborFlags[numMinTUInLCU + 1 + topLeftPartColumn + (i << shiftW)];
    }
    else {

        if (ConstrainedIntraPredFlag) {
            for (i = 0; i < numMinTUInPU; i++) {
                Ipp32s zScanIdx = h265_scan_r2z4[rasterTopLeftPartIdx + (i << shiftW) - PITCH_TU];
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
                          Ipp8u shiftW,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s numMinTUInLCU = pCU->m_par->NumPartInCUSize;
    Ipp32s numMinTUInPU = width >> pCU->m_par->QuadtreeTULog2MinSize;
    Ipp32s rasterTopRigthPartIdx = h265_scan_z2r4[blockZScanIdx] + (numMinTUInPU << shiftW) - 1;
    Ipp32s zScanTopRigthPartIdx = h265_scan_r2z4[rasterTopRigthPartIdx];
    Ipp32s topRightPartRow = rasterTopRigthPartIdx >> 4;
    Ipp32s topRightPartColumn = rasterTopRigthPartIdx & 15;
    Ipp32s i;

    if (topRightPartRow == 0)
    {
        for (i = 0; i < numMinTUInPU; i++)
        {
            Ipp32s aboveRightPartColumn = topRightPartColumn + (i << shiftW) + 1;

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
            Ipp32s aboveRightPartColumn = topRightPartColumn + (i << shiftW) + 1;

            if ((pCU->m_ctbPelX + (aboveRightPartColumn << pCU->m_par->QuadtreeTULog2MinSize)) >=
                pCU->m_par->Width)
            {
                outNeighborFlags[i] = 0;
            }
            else
            {
                if (aboveRightPartColumn < numMinTUInLCU)
                {
                    Ipp32s zScanIdx = h265_scan_r2z4[rasterTopRigthPartIdx + 1 + (i << shiftW) - PITCH_TU];

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
                    Ipp8u shiftW,
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

    IsAboveAvailable(pCU, blockZScanIdx, width, shiftW, ConstrainedIntraPredFlag, inNeighborFlags, outNeighborFlags);
    outNeighborFlags += numMinTUInPU;

    IsAboveRightAvailable(pCU, blockZScanIdx, width, shiftW, ConstrainedIntraPredFlag, inNeighborFlags, outNeighborFlags);
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
                           Ipp8u* neighborFlags, Ipp32s width, Ipp8u chroma422, Ipp32s srcPitch, PixType* src_ref)
{
    PixType* tmpSrcPtr;
    PixType dcval = (1 << (par->bitDepthChroma - 1));
    Ipp32s minTUSize = 1 << par->QuadtreeTULog2MinSize;
    Ipp32s numUnitsInCU = width >> par->QuadtreeTULog2MinSize;
    if (!chroma422) {
        minTUSize >>= 1;
        numUnitsInCU <<= 1;
    }
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
void H265CU<PixType>::IntraPredTu(Ipp32s blockZScanIdx, Ipp32u idx422, Ipp32s width, Ipp32s pred_mode, Ipp8u is_luma)
{
    PixType PredPel[4*2*64+1];
    Ipp32s PURasterIdx = h265_scan_z2r4[blockZScanIdx + idx422];
    Ipp32s PUStartRow = PURasterIdx >> 4;
    Ipp32s PUStartColumn = PURasterIdx & 15;
    PixType *pRec;

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

        pRec = m_yRec + ((PUStartRow * m_pitchRecLuma + PUStartColumn) << m_par->QuadtreeTULog2MinSize);

        GetAvailablity(this, blockZScanIdx, width, 0, m_par->cpps->constrained_intra_pred_flag,
            m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsLuma(m_par, pRec, PredPel, m_outNeighborFlags, width, m_pitchRecLuma);

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
            h265_PredictIntra_Planar(PredPel, pRec, m_pitchRecLuma, width);
            break;
        case INTRA_DC:
            h265_PredictIntra_DC(PredPel, pRec, m_pitchRecLuma, width, is_luma);
            break;
        case INTRA_VER:
            h265_PredictIntra_Ver(PredPel, pRec, m_pitchRecLuma, width, m_par->bitDepthLuma, is_luma);
            break;
        case INTRA_HOR:
            h265_PredictIntra_Hor(PredPel, pRec, m_pitchRecLuma, width, m_par->bitDepthLuma, is_luma);
            break;
        default:
            h265_PredictIntra_Ang(pred_mode, PredPel, pRec, m_pitchRecLuma, width);
        }
    }
    else
    {
        pRec = m_uvRec + (((PUStartRow * m_pitchRecChroma >> m_par->chromaShiftH) + (PUStartColumn << m_par->chromaShiftWInv)) << m_par->QuadtreeTULog2MinSize);
        GetAvailablity(this, blockZScanIdx + idx422,
            m_par->chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? width << 1 : width,
            m_par->chroma422, m_par->cpps->constrained_intra_pred_flag, m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsChromaNV12(m_par, pRec, PredPel, m_outNeighborFlags, width, m_par->chroma422, m_pitchRecChroma, m_uvRec- ((m_ctbPelX << m_par->chromaShiftWInv) + (m_ctbPelY * m_pitchRecChroma >> m_par->chromaShiftH)));

        switch(pred_mode)
        {
        case INTRA_PLANAR:
            h265_PredictIntra_Planar_ChromaNV12(PredPel, pRec, m_pitchRecChroma, width);
            break;
        case INTRA_DC:
            h265_PredictIntra_DC_ChromaNV12(PredPel, pRec, m_pitchRecChroma, width);
            break;
        case INTRA_VER:
            h265_PredictIntra_Ver_ChromaNV12(PredPel, pRec, m_pitchRecChroma, width);
            break;
        case INTRA_HOR:
            h265_PredictIntra_Hor_ChromaNV12(PredPel, pRec, m_pitchRecChroma, width);
            break;
        default:
            if (m_par->chroma422)
                pred_mode = h265_chroma422IntraAngleMappingTable[pred_mode];
            h265_PredictIntra_Ang_ChromaNV12(PredPel, pRec, m_pitchRecChroma, width, pred_mode);
        }
    }
}

template <typename PixType>
Ipp32s H265CU<PixType>::GetIntraLumaModeCost(Ipp8u mode, const CABAC_CONTEXT_H265 ctx)
{
    m_bsf->Reset();
    if (mode == m_mpm[0]) {
        m_bsf->EncodeSingleBin_CABAC(&ctx, 1);
        m_bsf->EncodeBinEP_CABAC(0);
    }
    else if (mode == m_mpm[1]) {
        m_bsf->EncodeSingleBin_CABAC(&ctx, 1);
        m_bsf->EncodeBinEP_CABAC(1);
        m_bsf->EncodeBinEP_CABAC(0);
    }
    else if (mode == m_mpm[2]) {
        m_bsf->EncodeSingleBin_CABAC(&ctx, 1);
        m_bsf->EncodeBinEP_CABAC(1);
        m_bsf->EncodeBinEP_CABAC(1);
    }
    else {
        m_bsf->EncodeSingleBin_CABAC(&ctx, 0);
        for (Ipp32s i = 2; i >= 0; i--)
            if (mode > m_mpmSorted[i])
                mode--;
        m_bsf->EncodeBinsEP_CABAC(mode, 5);
    }

    return (Ipp32s)m_bsf->GetNumBits();
}


template <typename PixType>
Ipp32s H265CU<PixType>::GetIntraLumaModeCost(Ipp8u mode)
{
    CABAC_CONTEXT_H265 *ctx = m_bsf->m_base.context_array + tab_ctxIdxOffset[INTRA_LUMA_PRED_MODE_HEVC];

    Ipp32s isOneOfMostProb = (mode == m_mpm[0] || mode == m_mpm[1] || mode == m_mpm[2]);

    m_bsf->Reset();
    m_bsf->EncodeSingleBin_CABAC(ctx, isOneOfMostProb); // context is updated
    return (Ipp32s)m_bsf->GetNumBits();
}


template <typename PixType>
CostType H265CU<PixType>::GetTransformSubdivFlagCost(Ipp32s depth, Ipp32s trDepth)
{
    Ipp32s trSize = m_par->Log2MaxCUSize - depth - trDepth;
    m_bsf->Reset();
    m_bsf->EncodeSingleBin_CABAC(CTX(m_bsf, TRANS_SUBDIV_FLAG_HEVC) + 5 - trSize, 1);
    return BIT_COST(m_bsf->GetNumBits());
}

template <typename PixType>
CostType H265CU<PixType>::GetCuSplitFlagCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth)
{
    m_bsf->Reset();
    m_data = data;
    EncodeCU(m_bsf, absPartIdx, depth, RD_CU_SPLITFLAG);
    return BIT_COST(m_bsf->GetNumBits());
}

template <typename PixType>
CostType H265CU<PixType>::GetCuModesCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth)
{
    m_bsf->Reset();
    m_data = data;
    EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
    return BIT_COST(m_bsf->GetNumBits());
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

bool operator <(const IntraLumaMode &l, const IntraLumaMode &r) { return l.cost < r.cost; }

void SortLumaModesByCost(IntraLumaMode *modes, Ipp32s numCandInput, Ipp32s numCandSorted)
{
    VM_ASSERT(numCandInput >= numCandSorted);
    // stable partial sort
    for (Ipp32s i = 0; i < numCandSorted; i++)
        std::swap(modes[i],
                  modes[Ipp32s(std::min_element(modes + i, modes + numCandInput) - modes)]);
}

template <typename PixType>
void H265CU<PixType>::GetAngModesFromHistogram(Ipp32s xPu, Ipp32s yPu, Ipp32s puSize, Ipp8s *modes, Ipp32s numModes)
{
    VM_ASSERT(numModes <= 33);
    VM_ASSERT(puSize >= 4);
    VM_ASSERT(puSize <= 64);
    VM_ASSERT(m_ctbPelX + xPu + puSize <= m_par->Width);
    VM_ASSERT(m_ctbPelY + yPu + puSize <= m_par->Height);

    Ipp32s histogram[35] = {};

    if (m_par->enableCmFlag) {
#ifdef MFX_VA
        if (puSize == 4) {
            Ipp32s pitch = width >> 2;
            xPu += m_ctbPelX;
            yPu += m_ctbPelY;
            const CmMbIntraGrad *histBlock = cmMbIntraGrad4x4[cmCurIdx] + (xPu >> 2) + (yPu >> 2) * pitch;
            for (Ipp32s i = 0; i < 35; i++)
                histogram[i] = histBlock->histogram[i];
        }
        else {
            puSize >>= 3;
            Ipp32s pitch = width >> 3;
            xPu += m_ctbPelX;
            yPu += m_ctbPelY;
            const CmMbIntraGrad *histBlock = cmMbIntraGrad8x8[cmCurIdx] + (xPu >> 3) + (yPu >> 3) * pitch;
            for (Ipp32s y = 0; y < puSize; y++, histBlock += pitch)
                for (Ipp32s x = 0; x < puSize; x++)
                    for (Ipp32s i = 0; i < 35; i++)
                        histogram[i] += histBlock[x].histogram[i];
        }
#endif // MFX_VA
    }
    else {
        // all in units of 4x4 blocks
        if (puSize == 4) {
            Ipp32s pitch = 40 * m_par->MaxCUSize / 4;
            const HistType *histBlock = m_hist4 + (xPu >> 2) * 40 + (yPu >> 2) * pitch;
            for (Ipp32s i = 0; i < 35; i++)
                histogram[i] = histBlock[i];
        }
        else {
            puSize >>= 3;
            Ipp32s pitch = 40 * m_par->MaxCUSize / 8;
            const HistType *histBlock = m_hist8 + (xPu >> 3) * 40 + (yPu >> 3) * pitch;
            for (Ipp32s y = 0; y < puSize; y++, histBlock += pitch)
                for (Ipp32s x = 0; x < puSize; x++)
                    for (Ipp32s i = 0; i < 35; i++)
                        histogram[i] += histBlock[40 * x + i];
        }
    }

    for (Ipp32s i = 0; i < numModes; i++) {
        Ipp32s mode = (Ipp32s)(std::max_element(histogram + 2, histogram + 35) - histogram);
        modes[i] = mode;
        histogram[mode] = -1;
    }
}


template <typename PixType>
void H265CU<PixType>::CheckIntra(Ipp32s absPartIdx, Ipp32s depth)
{
    CheckIntraLuma(absPartIdx, depth);

    if (m_costCurr <= m_costStored[depth])
        CheckIntraChroma(absPartIdx, depth);


}
#ifdef AMT_ICRA_OPT
template <typename PixType>
bool H265CU<PixType>::tryIntraRD(Ipp32s absPartIdx, Ipp32s depth, IntraLumaMode *modes)
{
    if ( modes[0].satd < 0 ) return true;
    if ( m_cuIntraAngMode > 2 ) return true;
    Ipp32s widthCu = m_par->MaxCUSize >> depth;
    Ipp32s InterHad = 0;
    Ipp32s IntraHad = 0;
    Ipp32s numCand1 = m_par->num_cand_1[m_par->Log2MaxCUSize - depth];
    
    Ipp32s offsetLuma = GetLumaOffset(m_par, absPartIdx, m_pitchSrcLuma);
    PixType *pSrc = m_ySrc + offsetLuma;
    Ipp32s offsetPred = GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
    const PixType *predY = m_interPredY + offsetPred;

    InterHad = tuHad(pSrc, m_pitchSrcLuma, predY, MAX_CU_SIZE, widthCu, widthCu)>> m_par->bitDepthLumaShift;
    InterHad+= ((Ipp64f)m_mvdCost*m_rdLambda*256.0 + 0.5);

    bool skipIntraRD = true;
    for(Ipp32s ic=0; ic<numCand1; ic++) {
        Ipp32s mode = modes[ic].mode;
        PixType *pred = m_predIntraAll + mode * widthCu * widthCu;
        float dc=0;        
        if(mode < 2 || mode >= 18 || m_cuIntraAngMode == INTRA_ANG_MODE_GRADIENT && mode == 10) {
            for(Ipp32s r=0;r<widthCu; r++)
                for(Ipp32s c=0;c<widthCu; c++)
                    dc += (pSrc[r*m_pitchSrcLuma+c] - pred[r*widthCu+c]);
            dc/=(widthCu*widthCu);
            dc=abs(dc);
        } else {
            for(Ipp32s r=0;r<widthCu; r++)
                for(Ipp32s c=0;c<widthCu; c++)
                    dc += (m_srcTr[r*widthCu+c] - pred[r*widthCu+c]);
            dc/=(widthCu*widthCu);
            dc=abs(dc);
        }
        IntraHad = modes[ic].satd;
        IntraHad -= dc*16*(widthCu/8)*(widthCu/8);
        IntraHad += (ic*m_rdLambda*256.0 + 0.5);

        if(IntraHad<InterHad || dc<1.0) {
            skipIntraRD = false;
            break;
        }
    }
    return !skipIntraRD;
}
#endif

template <typename PixType>
void H265CU<PixType>::CheckIntraLuma(Ipp32s absPartIdx, Ipp32s depth)
{
    CostType costInitial = m_costCurr;
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
    if (depth == m_par->MaxCUDepth - m_par->AddCUDepth && depth + 1 <= m_par->MaxCUDepth) {
        // we are going to test 2 modes: Intra_2Nx2N and Intra_NxN
        // need to save initial state
        m_bsf->CtxSave(ctxInitial);
        m_costStored[depth + 1] = COST_MAX;
    }

    // estimate bitcost of cu split flag and part mode
    Ipp32s cuWidth = m_par->MaxCUSize >> depth;
    Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
    FillSubPartIntraCuModes_(m_data + absPartIdx, numParts, cuWidth, depth, PART_SIZE_2Nx2N);
    m_costCurr += GetCuModesCost(m_data, absPartIdx, depth);

    IntraLumaMode modes[35];
    Ipp32s numModes = InitIntraLumaModes(absPartIdx, depth, 0, modes);
    numModes = FilterIntraLumaModesBySatd(absPartIdx, depth, 0, modes, numModes);
    bool tryIntra = true;
#ifdef AMT_ICRA_OPT
    if(m_par->TryIntra>=2 && m_cslice->slice_type != I_SLICE) tryIntra = tryIntraRD(absPartIdx, depth, modes);
#endif
    if(tryIntra) {
        numModes = FilterIntraLumaModesByRdoTr0(absPartIdx, depth, 0, modes, numModes);
        numModes = FilterIntraLumaModesByRdoTrAll(absPartIdx, depth, 0, modes, numModes);
    } else {
        m_costCurr = COST_MAX;
    }

    if (depth == m_par->MaxCUDepth - m_par->AddCUDepth && depth + 1 <= m_par->MaxCUDepth) {
        // save Intra_2Nx2N decision
        SaveIntraLumaDecision(absPartIdx, depth);
        // restore initial states
        m_bsf->CtxRestore(ctxInitial);
        m_costCurr = costInitial;

        // estimate bitcost of cu split flag and part mode
        FillSubPartIntraPartMode_(m_data + absPartIdx, numParts, PART_SIZE_NxN);
        m_costCurr += GetCuModesCost(m_data, absPartIdx, depth); // cu split flag and part mode

        // process 4 NxN PUs
        Ipp32s nonZero = 0;
        numParts >>= 2;
        for (Ipp32s i = 0, absPartIdxPu = absPartIdx; i < 4; i++, absPartIdxPu += numParts) {
            Ipp32s numModes = InitIntraLumaModes(absPartIdxPu, depth, 1, modes);
            numModes = FilterIntraLumaModesBySatd(absPartIdxPu, depth, 1, modes, numModes);
            numModes = FilterIntraLumaModesByRdoTr0(absPartIdxPu, depth, 1, modes, numModes);
            numModes = FilterIntraLumaModesByRdoTrAll(absPartIdxPu, depth, 1, modes, numModes);
            nonZero |= m_data[absPartIdxPu].cbf[0];
        }
        if (nonZero)
            for (Ipp32s i = 0; i < 4; i++)
                SetCbfBit<0>(m_data + absPartIdx + i * numParts, 0);

        // keep best decision
        LoadIntraLumaDecision(absPartIdx, depth);
    }
}


template <typename PixType>
void H265CU<PixType>::CheckIntraChroma(Ipp32s absPartIdx, Ipp32s depth)
{
    Ipp32u numParts = m_par->NumPartInCU >> (2 * depth);
    CostType chromaCost = 0;
    CostType lumaCost = m_costCurr;

    if (HaveChromaRec()) {
        Ipp8u allowedChromaDir[NUM_CHROMA_MODE];
        GetAllowedChromaDir(absPartIdx, allowedChromaDir);

        // we are going to test to modes: Intra_2Nx2N and Intra_NxN
        // need to save initial state
        CostType costInitial = m_costCurr;
        CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
        m_bsf->CtxSave(ctxInitial);
        m_costStored[depth + 1] = COST_MAX;

        for (Ipp8u i = 0; i < NUM_CHROMA_MODE; i++) {
            Ipp8u chromaDir = allowedChromaDir[i];

            if (i > 0) {
                // before testing second chroma mode
                // save first chroma decision
                SaveIntraChromaDecision(absPartIdx, depth);
                // and restore initial states
                m_bsf->CtxRestore(ctxInitial);
                m_costCurr = costInitial;
            }

            FillSubPartIntraPredModeC_(m_data + absPartIdx, numParts, chromaDir);
            EncAndRecChroma(absPartIdx, absPartIdx << (4 - m_par->chromaShift), depth, &chromaCost);
            chromaCost += GetIntraChromaModeCost(absPartIdx);

            m_costCurr += chromaCost;
        }

        LoadIntraChromaDecision(absPartIdx, depth);
    }
    else {
        FillSubPartIntraPredModeC_(m_data + absPartIdx, numParts, INTRA_DM_CHROMA);
    }

    if (!(m_par->AnalyseFlags & HEVC_COST_CHROMA))
        m_costCurr = lumaCost; // do not count chroma cost
}


template <typename PixType>
Ipp32s H265CU<PixType>::InitIntraLumaModes(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth, IntraLumaMode *modes)
{
    Ipp32s numModes = 2;
    modes[0].mode = INTRA_PLANAR;
    modes[1].mode = INTRA_DC;
    if (m_cuIntraAngMode == INTRA_ANG_MODE_DC_PLANAR_ONLY) {
    }
    else if (m_cuIntraAngMode == INTRA_ANG_MODE_GRADIENT) {
        Ipp32s x = (h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize;
        Ipp32s y = (h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize;

        Ipp32s log2BlockSize = m_par->Log2MaxCUSize - depth - trDepth;

        // B nonRef [3]; B Ref [2], P [1], I [0]
        // I_SLICE is 2, P_SLICE is 1, B_SLICE is 0
        // npshosta: check frame->m_PicCodType here
        Ipp32s numAngModes = (B_SLICE == m_cslice->slice_type && !m_currFrame->m_isRef)
            ? m_par->num_cand_0[B_NONREF][log2BlockSize]
            : m_par->num_cand_0[SliceTypeIndex(m_cslice->slice_type)][log2BlockSize];

        Ipp8s angModes[33];
        GetAngModesFromHistogram(x, y, 1 << log2BlockSize, angModes, numAngModes);

        for (Ipp32s i = 0; i < numAngModes; i++)
            modes[2 + i].mode = angModes[i];
        numModes += numAngModes;
    }
    else if (m_cuIntraAngMode == INTRA_ANG_MODE_EVEN) {
        for (Ipp8u mode = numModes; mode < 35; numModes++, mode += 2)
            modes[numModes].mode = mode;
    }
    else { // INTRA_ANG_MODE_ALL
        for (Ipp32s i = 2; i < 35; i++)
            modes[i].mode = i;
        numModes = 35;
    }

    // get list of most probable modes once
    GetIntradirLumaPred(absPartIdx, m_mpm);
    m_mpmSorted[0] = m_mpm[0];
    m_mpmSorted[1] = m_mpm[1];
    m_mpmSorted[2] = m_mpm[2];
    if (m_mpmSorted[0] > m_mpmSorted[1])
        std::swap(m_mpmSorted[0], m_mpmSorted[1]);
    if (m_mpmSorted[0] > m_mpmSorted[2])
        std::swap(m_mpmSorted[0], m_mpmSorted[2]);
    if (m_mpmSorted[1] > m_mpmSorted[2])
        std::swap(m_mpmSorted[1], m_mpmSorted[2]);

    // calculate bit cost for each intra luma mode
    const CABAC_CONTEXT_H265 ctx = m_bsf->m_base.context_array[tab_ctxIdxOffset[INTRA_LUMA_PRED_MODE_HEVC]];
    Ipp64f lambdaSatd = m_rdLambda;
    for (Ipp32s i = 0; i < numModes; i++) {
        modes[i].numBits = GetIntraLumaModeCost(modes[i].mode, ctx);
        modes[i].bitCost = modes[i].numBits * lambdaSatd;
    }

    return numModes;
    
}


template <typename PixType>
void H265CU<PixType>::FilterIntraPredPels(const PixType *predPel, PixType *predPelFilt, Ipp32s width)
{
    small_memcpy(predPelFilt, predPel, (4 * width + 1) * sizeof(PixType));
    if (m_par->csps->strong_intra_smoothing_enabled_flag && width == 32) {
        Ipp32s threshold = 1 << (m_par->bitDepthLuma - 5);
        Ipp32s topLeft = predPel[0];
        Ipp32s topRight = predPel[2 * width];
        Ipp32s midHor = predPel[width];
        Ipp32s bottomLeft = predPel[4 * width];
        Ipp32s midVer = predPel[3 * width];
        bool bilinearLeft = abs(topLeft + topRight - 2 * midHor) < threshold;
        bool bilinearAbove = abs(topLeft + bottomLeft - 2 * midVer) < threshold;

        if (bilinearLeft && bilinearAbove)
            h265_FilterPredictPels_Bilinear(predPelFilt, width, topLeft, bottomLeft, topRight);
        else
            h265_FilterPredictPels(predPelFilt, width);
    }
    else {
        h265_FilterPredictPels(predPelFilt, width);
    }
}

template <typename PixType>
Ipp32s H265CU<PixType>::FilterIntraLumaModesBySatd(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                                   IntraLumaMode *modes, Ipp32s numModes)
{
    __ALIGN32 PixType predPel[4 * 64 + 1];
    __ALIGN32 PixType predPelFilt[4 * 64 + 1];

    Ipp32u numParts = m_par->NumPartInCU >> (2 * (depth + trDepth));
    Ipp32s width = m_par->MaxCUSize >> (depth + trDepth);
    Ipp32s size = width * width;
    IppiSize roi = { width, width };
    PixType *rec = m_yRec + GetLumaOffset(m_par, absPartIdx, m_pitchRecLuma);
    PixType *src = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchSrcLuma);
    Ipp32s partMode = (trDepth == 0) ? PART_SIZE_2Nx2N : PART_SIZE_NxN;
    Ipp64f lambdaSatd = m_rdLambda;

    bool optimizedBranch = (SPLIT_MUST != GetTrSplitMode(absPartIdx, depth, trDepth, partMode, 1));
    if (optimizedBranch) {
        GetAvailablity(this, absPartIdx, width, 0, m_par->cpps->constrained_intra_pred_flag, m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsLuma(m_par, rec, predPel, m_outNeighborFlags, width, m_pitchRecLuma);
        FilterIntraPredPels(predPel, predPelFilt, width);

        _ippiTranspose_C1R(src, m_pitchSrcLuma, m_srcTr, width, roi);

        // calc prediction intra modes selected by costs/numCosts
        PixType *predPtr = m_predIntraAll; // buffer for all 35 intra modes
        bool needFilter = (INTRA_HOR - INTRA_PLANAR) > h265_filteredModes[h265_log2m2[width]];
        h265_PredictIntra_Planar(needFilter ? predPelFilt : predPel, predPtr, width, width);
        predPtr += size;
        h265_PredictIntra_DC(predPel, predPtr, width, width, 1);
        predPtr += size;

        if (m_cuIntraAngMode == INTRA_ANG_MODE_DC_PLANAR_ONLY) {
        }
        else if (m_cuIntraAngMode == INTRA_ANG_MODE_GRADIENT) {
            for (Ipp32s i = 2; i < numModes; i++) { // first 2 are always PLANAR & DC
                Ipp8u mode = modes[i].mode;
                Ipp32s diff = MIN(abs(mode - INTRA_HOR), abs(mode - INTRA_VER));
                PixType *predPels = (diff > h265_filteredModes[h265_log2m2[width]]) ? predPelFilt : predPel;
                PixType *predPtr = m_predIntraAll + mode * size;

                if (mode == INTRA_HOR)
                    h265_PredictIntra_Hor(predPels, predPtr, width, width, m_par->bitDepthLuma, 1);
                else if (mode == INTRA_VER)
                    h265_PredictIntra_Ver(predPels, predPtr, width, width, m_par->bitDepthLuma, 1);
                else
                    h265_PredictIntra_Ang_NoTranspose(mode, predPels, predPtr, width, width);
            }
        }
        else if (m_cuIntraAngMode == INTRA_ANG_MODE_EVEN) {
            h265_PredictIntra_Ang_All_Even(predPel, predPelFilt, predPtr, width);
        }
        else { // INTRA_ANG_MODE_ALL
            h265_PredictIntra_Ang_All(predPel, predPelFilt, predPtr, width, m_par->bitDepthLuma);
        }

        // calc prediction satd and bit cost for intra modes selected by costs/numCosts
        for (Ipp32s i = 0; i < numModes; i++) {
            Ipp8u mode = modes[i].mode;
            predPtr = m_predIntraAll + mode * size;
            Ipp32s satd = (mode < 2 || mode >= 18 || m_cuIntraAngMode == INTRA_ANG_MODE_GRADIENT && mode == 10)
                ? tuHad(src, m_pitchSrcLuma, predPtr, width, width, width) // prediction block is not transposed
                : tuHad(m_srcTr, width, predPtr, width, width, width); // prediction block is transposed
            modes[i].satd = modes[i].cost = satd >> m_par->bitDepthLumaShift;
            modes[i].cost += modes[i].bitCost;
        }
    }
    else {
        CostType costCurrSaved = m_costCurr;
        for (Ipp32s i = 0; i < numModes; i++) {
            Ipp8u mode = modes[i].mode;
            m_costCurr = 0.0;
            FillSubPartIntraPredModeY_(m_data + absPartIdx, numParts, mode);
            CalcCostLuma(absPartIdx, depth, trDepth, COST_PRED_TR_0);
            modes[i].satd = -1;
            modes[i].cost = m_costCurr;
            modes[i].cost += modes[i].bitCost;
        }
        m_costCurr = costCurrSaved;
    }

    Ipp32s numModesAfterSatdStage = m_par->num_cand_1[m_par->Log2MaxCUSize - depth - trDepth];
    SortLumaModesByCost(modes, numModes, numModesAfterSatdStage);

    if (INTRA_ANG_MODE_EVEN == m_cuIntraAngMode) {
        // select odd angular modes for refinement stage
        const CABAC_CONTEXT_H265 ctx = m_bsf->m_base.context_array[tab_ctxIdxOffset[INTRA_LUMA_PRED_MODE_HEVC]];
        Ipp32s numOddModes = 0;
        for (Ipp8u oddMode = 3; oddMode < 35; oddMode += 2) {
            for (Ipp32s i = 0; i < numModesAfterSatdStage; i++) {
                if (oddMode + 1 == modes[i].mode || oddMode == modes[i].mode + 1) {
                    Ipp32s numBits = GetIntraLumaModeCost(oddMode, ctx);
                    modes[numModesAfterSatdStage + numOddModes].mode = oddMode;
                    modes[numModesAfterSatdStage + numOddModes].numBits = numBits;
                    modes[numModesAfterSatdStage + numOddModes].bitCost = numBits * lambdaSatd;
                    numOddModes++;
                    break;
                }
            }
        }

        // calculate cost for few more angular modes
        if (optimizedBranch) {
            for (Ipp32s i = numModesAfterSatdStage; i < numModesAfterSatdStage + numOddModes; i++) {
                Ipp8u mode = modes[i].mode;

                Ipp32s diff = MIN(abs(mode - INTRA_HOR), abs(mode - INTRA_VER));
                bool needFilter = (diff > h265_filteredModes[h265_log2m2[width]]);
                PixType *predPels = needFilter ? predPelFilt : predPel;
                PixType *predPtr = m_predIntraAll + mode * size;
                h265_PredictIntra_Ang_NoTranspose(mode, predPels, predPtr, width, width);
                Ipp32s satd = (mode >= 2 && mode < 18)
                    ? tuHad(m_srcTr, width, predPtr, width, width, width)
                    : tuHad(src, m_pitchSrcLuma, predPtr, width, width, width);
                modes[i].satd = modes[i].cost = satd >> m_par->bitDepthLumaShift;
                modes[i].cost += modes[i].bitCost;
            }
        }
        else {
            CostType costCurrSaved = m_costCurr;
            for (Ipp32s i = numModesAfterSatdStage; i < numModesAfterSatdStage + numOddModes; i++) {
                Ipp8u mode = modes[i].mode;
                m_costCurr = 0.0;
                FillSubPartIntraPredModeY_(m_data + absPartIdx, numParts, mode);
                CalcCostLuma(absPartIdx, depth, trDepth, COST_PRED_TR_0);
                modes[i].satd = -1;
                modes[i].cost = m_costCurr;
                modes[i].cost += modes[i].bitCost;
            }
            m_costCurr = costCurrSaved;
        }

        SortLumaModesByCost(modes, numModesAfterSatdStage + numOddModes, numModesAfterSatdStage);
    }

    return numModesAfterSatdStage;
}


template <typename PixType>
Ipp32s H265CU<PixType>::FilterIntraLumaModesByRdoTr0(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                                     IntraLumaMode *modes, Ipp32s numModes)
{
    Ipp32s tuSplitIntra = (m_par->tuSplitIntra == 1 ||
                           m_par->tuSplitIntra == 3 && m_cslice->slice_type == I_SLICE);
    if (!tuSplitIntra)
        return numModes; // final decision will be made at RdoTrAll stage but without transform split, so we may skip RdoTr0 stage

    Ipp32s numModesAfterRdoTr0Stage = m_par->num_cand_2[m_par->Log2MaxCUSize - depth - trDepth];
    if (numModesAfterRdoTr0Stage >= numModes)
        return numModes; // do nothing if RdoTr0 stage doesn't reduce number of modes

    Ipp32s partMode = (trDepth == 0) ? PART_SIZE_2Nx2N : PART_SIZE_NxN;
    Ipp32s numParts = m_par->NumPartInCU >> (2 * (depth + trDepth));

    // store initial states before making decision between several modes
    CostType costInitial = m_costCurr;
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
    m_bsf->CtxSave(ctxInitial);

    CostType costCurrSaved = m_costCurr;
    for (Ipp8u i = 0; i < numModes; i++) {
        m_costCurr = modes[i].numBits * m_rdLambda;
        FillSubPartIntraPredModeY_(m_data + absPartIdx, numParts, modes[i].mode);
        CalcCostLuma(absPartIdx, depth, trDepth, COST_REC_TR_0);
        modes[i].cost = m_costCurr;

        // states should be restored to initial after loop ends because final decision is not made here
        m_bsf->CtxRestore(ctxInitial);
        m_costCurr = costCurrSaved;
    }

    SortLumaModesByCost(modes, numModes, numModesAfterRdoTr0Stage);
    return numModesAfterRdoTr0Stage;
}


template <typename PixType>
Ipp32s H265CU<PixType>::FilterIntraLumaModesByRdoTrAll(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                                       IntraLumaMode *modes, Ipp32s numModes)
{
    Ipp32s tuSplitIntra = (m_par->tuSplitIntra == 1 ||
                           m_par->tuSplitIntra == 3 && m_cslice->slice_type == I_SLICE);
    CostOpt rdoCostOpt = tuSplitIntra ? COST_REC_TR_ALL : COST_REC_TR_0;
    Ipp32s partMode = (trDepth == 0) ? PART_SIZE_2Nx2N : PART_SIZE_NxN;
    Ipp32s numParts = m_par->NumPartInCU >> (2 * (depth + trDepth));

    // store initial states before making decision between several modes
    CostType costInitial = m_costCurr;
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
    m_bsf->CtxSave(ctxInitial);
    // and initialize storage
    m_costStored[depth + trDepth + 1] = COST_MAX;

    for (Ipp32s i = 0; i < numModes; i++) {
        if (i > 0) {
            SaveIntraLumaDecision(absPartIdx, depth + trDepth);
            m_bsf->CtxRestore(ctxInitial);
            m_costCurr = costInitial;
        }

        Ipp8u mode = modes[i].mode;
        m_costCurr += modes[i].numBits * m_rdLambda;
        FillSubPartIntraPredModeY_(m_data + absPartIdx, numParts, mode);
        CalcCostLuma(absPartIdx, depth, trDepth, rdoCostOpt);
    }

    // keep the best decision
    LoadIntraLumaDecision(absPartIdx, depth + trDepth);

    GetIntraLumaModeCost(m_data[absPartIdx].intraLumaDir); // just update context here, bit cost has been added already

    return 1;
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
            Ipp32s width = m_par->MaxCUSize >> (depth + tr_depth);
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

template class H265CU<Ipp8u>;
template class H265CU<Ipp16u>;

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
