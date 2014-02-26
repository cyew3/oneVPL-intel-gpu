//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"
#include "ippi.h"

namespace H265Enc {

static
void IsAboveLeftAvailable(H265CU *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s maxDepth = pCU->m_par->Log2MaxCUSize - pCU->m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s rasterTopLeftPartIdx = h265_scan_z2r[maxDepth][blockZScanIdx];
    Ipp32s topLeftPartRow = rasterTopLeftPartIdx >> maxDepth;
    Ipp32s topLeftPartColumn = rasterTopLeftPartIdx & (numMinTUInLCU - 1);

    if (topLeftPartColumn != 0)
    {
        if (topLeftPartRow != 0)
        {
            Ipp32s zScanIdx = h265_scan_r2z[maxDepth][rasterTopLeftPartIdx - numMinTUInLCU - 1];

            if (ConstrainedIntraPredFlag)
            {
                outNeighborFlags[0] = ((pCU->m_data[zScanIdx].predMode == MODE_INTRA) ? 1 : 0);
            }
            else
            {
                outNeighborFlags[0] = 1;
            }
        }
        else
        {
            /* Above CU*/
            outNeighborFlags[0] = inNeighborFlags[numMinTUInLCU + 1 + (rasterTopLeftPartIdx - 1)];
        }
    }
    else
    {
        if (topLeftPartRow != 0)
        {
            /* Left CU*/
            outNeighborFlags[0] = inNeighborFlags[numMinTUInLCU - topLeftPartRow];
        }
        else
        {
            /* Above Left CU*/
            outNeighborFlags[0] = inNeighborFlags[numMinTUInLCU];
        }
    }
}

static
void IsAboveAvailable(H265CU *pCU,
                      Ipp32s blockZScanIdx,
                      Ipp32s width,
                      Ipp8u ConstrainedIntraPredFlag,
                      Ipp8u *inNeighborFlags,
                      Ipp8u *outNeighborFlags)
{
    Ipp32s maxDepth = pCU->m_par->Log2MaxCUSize - pCU->m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s numMinTUInPU = width >> pCU->m_par->Log2MinTUSize;
    Ipp32s rasterTopLeftPartIdx = h265_scan_z2r[maxDepth][blockZScanIdx];
    Ipp32s topLeftPartRow = rasterTopLeftPartIdx >> maxDepth;
    Ipp32s topLeftPartColumn = rasterTopLeftPartIdx & (numMinTUInLCU - 1);
    Ipp32s i;

    if (topLeftPartRow == 0)
    {
        for (i = 0; i < numMinTUInPU; i++)
        {
            outNeighborFlags[i] = inNeighborFlags[numMinTUInLCU + 1 + topLeftPartColumn + i];
        }
    }
    else
    {
        for (i = 0; i < numMinTUInPU; i++)
        {
            Ipp32s zScanIdx = h265_scan_r2z[maxDepth][rasterTopLeftPartIdx + i - numMinTUInLCU];

            if (ConstrainedIntraPredFlag)
            {
                outNeighborFlags[i] = ((pCU->m_data[zScanIdx].predMode == MODE_INTRA) ? 1 : 0);
            }
            else
            {
                outNeighborFlags[i] = 1;
            }
        }
    }
}

static
void IsAboveRightAvailable(H265CU *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s maxDepth = pCU->m_par->Log2MaxCUSize - pCU->m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s numMinTUInPU = width >> pCU->m_par->Log2MinTUSize;
    Ipp32s rasterTopRigthPartIdx = h265_scan_z2r[maxDepth][blockZScanIdx] + numMinTUInPU - 1;
    Ipp32s zScanTopRigthPartIdx = h265_scan_r2z[maxDepth][rasterTopRigthPartIdx];
    Ipp32s topRightPartRow = rasterTopRigthPartIdx >> maxDepth;
    Ipp32s topRightPartColumn = rasterTopRigthPartIdx & (numMinTUInLCU - 1);
    Ipp32s i;

    if (topRightPartRow == 0)
    {
        for (i = 0; i < numMinTUInPU; i++)
        {
            Ipp32s aboveRightPartColumn = topRightPartColumn + i + 1;

            if ((pCU->m_ctbPelX + (aboveRightPartColumn << pCU->m_par->Log2MinTUSize)) >=
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

            if ((pCU->m_ctbPelX + (aboveRightPartColumn << pCU->m_par->Log2MinTUSize)) >=
                pCU->m_par->Width)
            {
                outNeighborFlags[i] = 0;
            }
            else
            {
                if (aboveRightPartColumn < numMinTUInLCU)
                {
                    Ipp32s zScanIdx = h265_scan_r2z[maxDepth][rasterTopRigthPartIdx + 1 + i - numMinTUInLCU];

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

static
void IsLeftAvailable(H265CU *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s maxDepth = pCU->m_par->Log2MaxCUSize - pCU->m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s numMinTUInPU = width >> pCU->m_par->Log2MinTUSize;
    Ipp32s rasterTopLeftPartIdx = h265_scan_z2r[maxDepth][blockZScanIdx];
    Ipp32s topLeftPartRow = rasterTopLeftPartIdx >> maxDepth;
    Ipp32s topLeftPartColumn = rasterTopLeftPartIdx & (numMinTUInLCU - 1);
    Ipp32s i;

    if (topLeftPartColumn == 0)
    {
        for (i = numMinTUInPU - 1; i >= 0; i--)
        {
            outNeighborFlags[numMinTUInPU - 1 - i] = inNeighborFlags[numMinTUInLCU - 1 - (topLeftPartRow + i)];
        }
    }
    else
    {
        for (i = numMinTUInPU - 1; i >= 0; i--)
        {
            Ipp32s zScanIdx = h265_scan_r2z[maxDepth][rasterTopLeftPartIdx - 1 + i * numMinTUInLCU];

            if (ConstrainedIntraPredFlag)
            {
                outNeighborFlags[numMinTUInPU - 1 - i] = ((pCU->m_data[zScanIdx].predMode == MODE_INTRA) ? 1 : 0);
            }
            else
            {
                outNeighborFlags[numMinTUInPU - 1 - i] = 1;
            }
        }
    }
}

static
void IsBelowLeftAvailable(H265CU *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s maxDepth = pCU->m_par->Log2MaxCUSize - pCU->m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s numMinTUInPU = width >> pCU->m_par->Log2MinTUSize;
    Ipp32s rasterBottomLeftPartIdx = h265_scan_z2r[maxDepth][blockZScanIdx] + (numMinTUInPU - 1) * numMinTUInLCU;
    Ipp32s zScanBottomLefPartIdx = h265_scan_r2z[maxDepth][rasterBottomLeftPartIdx];
    Ipp32s bottomLeftPartRow = rasterBottomLeftPartIdx >> maxDepth;
    Ipp32s bottomLeftPartColumn = rasterBottomLeftPartIdx & (numMinTUInLCU - 1);
    Ipp32s i;

    for (i = numMinTUInPU - 1; i >= 0; i--)
    {
        Ipp32s belowLefPartRow = bottomLeftPartRow + i + 1;

        if ((pCU->m_ctbPelY + (belowLefPartRow << pCU->m_par->Log2MinTUSize)) >=
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
                Ipp32s zScanIdx = h265_scan_r2z[maxDepth][rasterBottomLeftPartIdx+(i+1)*numMinTUInLCU-1];

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

static
void GetAvailablity(H265CU *pCU,
                    Ipp32s blockZScanIdx,
                    Ipp32s width,
                    Ipp8u ConstrainedIntraPredFlag,
                    Ipp8u *inNeighborFlags,
                    Ipp8u *outNeighborFlags)
{
    Ipp32s numMinTUInPU = width >> pCU->m_par->Log2MinTUSize;

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

static
void GetPredPelsLuma(H265VideoParam *par, PixType* src, PixType* PredPel,
                     Ipp8u* neighborFlags, Ipp32s width, Ipp32s srcPitch)
{
    PixType* tmpSrcPtr;
    PixType dcval;
    Ipp32s minTUSize = 1 << par->Log2MinTUSize;
    Ipp32s numUnitsInCU = width >> par->Log2MinTUSize;
    Ipp32s numIntraNeighbor;
    Ipp32s i, j;

    dcval = (1 << (BIT_DEPTH_LUMA - 1));

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

static
void GetPredPelsChromaNV12(H265VideoParam *par, PixType* src, PixType* PredPel,
                           Ipp8u* neighborFlags, Ipp32s width, Ipp32s srcPitch)
{
    PixType* tmpSrcPtr;
    PixType dcval = (1 << (BIT_DEPTH_CHROMA - 1));
    Ipp32s minTUSize = (1 << par->Log2MinTUSize) >> 1;
    Ipp32s numUnitsInCU = width >> (par->Log2MinTUSize - 1);
    Ipp32s numIntraNeighbor;
    Ipp32s i, j;
    numIntraNeighbor = 0;

    for (i = 0; i < 4*numUnitsInCU + 1; i++) {
        if (neighborFlags[i])
            numIntraNeighbor++;
    }

    if (numIntraNeighbor == 0) {
        // Fill border with DC value
        memset(PredPel, dcval, 4 * 2 * width + 2);

    } else if (numIntraNeighbor == (4*numUnitsInCU + 1)) {
        // Fill top-left border with rec. samples
        // Fill top and top right border with rec. samples
        ippsCopy_8u(src - srcPitch - 2, PredPel, 4 * width + 2);

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

void H265CU::IntraPredTu(Ipp32s blockZScanIdx, Ipp32s width, Ipp32s pred_mode, Ipp8u is_luma)
{
    PixType PredPel[4*2*64+1];
    Ipp32s maxDepth = m_par->Log2MaxCUSize - m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][blockZScanIdx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);
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

            if (diff <= h265_filteredModes[h265_log2table[width - 4] - 2])
            {
                isFilterNeeded = false;
            }
        }

        pRec = m_yRec + ((PUStartRow * m_pitchRec + PUStartColumn) << m_par->Log2MinTUSize);

        GetAvailablity(this, blockZScanIdx, width, m_par->cpps->constrained_intra_pred_flag,
            m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsLuma(m_par, pRec, PredPel, m_outNeighborFlags, width, m_pitchRec);

        if (m_par->csps->strong_intra_smoothing_enabled_flag && isFilterNeeded)
        {
            Ipp32s threshold = 1 << (BIT_DEPTH_LUMA - 5);

            Ipp32s topLeft = PredPel[0];
            Ipp32s topRight = PredPel[2*width];
            Ipp32s midHor = PredPel[width];

            Ipp32s bottomLeft = PredPel[4*width];
            Ipp32s midVer = PredPel[3*width];

            bool bilinearLeft = abs(topLeft + topRight - 2*midHor) < threshold;
            bool bilinearAbove = abs(topLeft + bottomLeft - 2*midVer) < threshold;

            if (width == 32 && (bilinearLeft && bilinearAbove))
            {
                h265_FilterPredictPels_Bilinear_8u(PredPel, width, topLeft, bottomLeft, topRight);
            }
            else
            {
                h265_FilterPredictPels_8u(PredPel, width);
            }
        } else if (isFilterNeeded) {
            h265_FilterPredictPels_8u(PredPel, width);
        }

        switch(pred_mode)
        {
        case INTRA_PLANAR:
            MFX_HEVC_PP::h265_PredictIntra_Planar_8u(PredPel, pRec, pitch, width);
            break;
        case INTRA_DC:
            MFX_HEVC_PP::h265_PredictIntra_DC_8u(PredPel, pRec, pitch, width, is_luma);
            break;
        case INTRA_VER:
            MFX_HEVC_PP::h265_PredictIntra_Ver_8u(PredPel, pRec, pitch, width, 8, is_luma);
            break;
        case INTRA_HOR:
            MFX_HEVC_PP::h265_PredictIntra_Hor_8u(PredPel, pRec, pitch, width, 8, is_luma);
            break;
        default:
            MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_8u)(pred_mode, PredPel, pRec, pitch, width);
        }
    }
    else
    {
        pRec = m_uvRec + ((PUStartRow * m_pitchRec + PUStartColumn * 2) << (m_par->Log2MinTUSize - 1));
        GetAvailablity(this, blockZScanIdx, width << 1, m_par->cpps->constrained_intra_pred_flag,
            m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsChromaNV12(m_par, pRec, PredPel, m_outNeighborFlags, width, pitch);

        switch(pred_mode)
        {
        case INTRA_PLANAR:
            MFX_HEVC_PP::h265_PredictIntra_Planar_ChromaNV12_8u(PredPel, pRec, pitch, width);
            break;
        case INTRA_DC:
            MFX_HEVC_PP::h265_PredictIntra_DC_ChromaNV12_8u(PredPel, pRec, pitch, width);
            break;
        case INTRA_VER:
            MFX_HEVC_PP::h265_PredictIntra_Ver_ChromaNV12_8u(PredPel, pRec, pitch, width);
            break;
        case INTRA_HOR:
            MFX_HEVC_PP::h265_PredictIntra_Hor_ChromaNV12_8u(PredPel, pRec, pitch, width);
            break;
        default:
            MFX_HEVC_PP::h265_PredictIntra_Ang_ChromaNV12_8u(PredPel, pRec, pitch, width, pred_mode);
        }
    }
}

CostType GetIntraLumaBitCost(H265CU * cu, Ipp32u abs_part_idx)
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

    //kolya
#define SQRT_LAMBDA_4_FUTURE 0 //<- fixed and reserved for next usage
#if SQRT_LAMBDA_4_FUTURE
    return (cu->m_bsf->GetNumBits()/256) * cu->m_rdLambdaSqrt;
#else
    return cu->m_bsf->GetNumBits() * cu->m_rdLambda;
#endif
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
            costs[num_costs - idx] = costs[num_costs - idx - 1];
            modes[num_costs - idx] = modes[num_costs - idx - 1];
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

#define NO_TRANSFORM_SPLIT_INTRAPRED_STAGE1 0
void H265CU::IntraLumaModeDecision(Ipp32s abs_part_idx, Ipp32u offset, Ipp8u depth, Ipp8u tr_depth)
{
    __ALIGN32 PixType predPel[4*64+1];
    __ALIGN32 PixType predPelFilt[4*64+1];
    Ipp32s width = m_par->MaxCUSize >> (depth + tr_depth);
    Ipp8u  part_size = (Ipp8u)(tr_depth == 1 ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
    Ipp32s maxDepth = m_par->Log2MaxCUSize - m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][abs_part_idx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);
    Ipp32s num_cand = m_par->num_cand_1[m_par->Log2MaxCUSize - (depth + tr_depth)];
    CostType cost = 0;

    PixType *pSrc = m_ySrc + ((PUStartRow * m_pitchSrc + PUStartColumn) << m_par->Log2MinTUSize);
    PixType *pRec = m_yRec + ((PUStartRow * m_pitchRec + PUStartColumn) << m_par->Log2MinTUSize);

    for (Ipp32s i = 0; i < num_cand; i++) m_intraBestCosts[i] = COST_MAX;

    Ipp8u tr_split_mode = GetTrSplitMode(abs_part_idx, depth, tr_depth, part_size, 1, !NO_TRANSFORM_SPLIT_INTRAPRED_STAGE1);
    m_predIntraAllWidth = 0;

    if (tr_split_mode != SPLIT_MUST)
    {
        m_predIntraAllWidth = width;
        GetAvailablity(this, abs_part_idx, width, m_par->cpps->constrained_intra_pred_flag, m_inNeighborFlags, m_outNeighborFlags);
        GetPredPelsLuma(m_par, pRec, predPel, m_outNeighborFlags, width, m_pitchRec);

        small_memcpy(predPelFilt, predPel, 4*width+1);

        if (m_par->csps->strong_intra_smoothing_enabled_flag && width == 32)
        {
            Ipp32s threshold = 1 << (BIT_DEPTH_LUMA - 5);
            Ipp32s topLeft = predPel[0];
            Ipp32s topRight = predPel[2*width];
            Ipp32s midHor = predPel[width];
            Ipp32s bottomLeft = predPel[4*width];
            Ipp32s midVer = predPel[3*width];
            bool bilinearLeft = abs(topLeft + topRight - 2*midHor) < threshold;
            bool bilinearAbove = abs(topLeft + bottomLeft - 2*midVer) < threshold;

            if (bilinearLeft && bilinearAbove)
                h265_FilterPredictPels_Bilinear_8u(predPelFilt, width, topLeft, bottomLeft, topRight);
            else
                h265_FilterPredictPels_8u(predPelFilt, width);
        } else {
            h265_FilterPredictPels_8u(predPelFilt, width);
        }

        IppiSize roi = {width, width};
        ippiTranspose_8u_C1R(pSrc, m_pitchSrc, m_tuSrcTransposed, width, roi);

        PixType *pred_ptr = m_predIntraAll;

        MFX_HEVC_PP::h265_PredictIntra_Planar_8u(INTRA_HOR <= h265_filteredModes[h265_log2table[width - 4] - 2] ? predPel : predPelFilt, pred_ptr, width, width);
        cost = tuHad(pSrc, m_pitchSrc, pred_ptr, width, width, width);
        pred_ptr += width * width;
        m_data = m_dataSave;
        FillSubPart(abs_part_idx, depth, tr_depth, part_size, INTRA_PLANAR, m_par->QP);
        m_intraModeBitcost[INTRA_PLANAR] = GetIntraLumaBitCost(this, abs_part_idx);

//kolya
//HM-lambda for HAD comparison moved here
//correct fix gives only 0.17% loss in short test
#define SQRT_LAMBDA 0

#if SQRT_LAMBDA
        StoreFewBestModes(cost + (int(m_intraModeBitcost[INTRA_PLANAR]/this->m_rdLambda)/256)*this->m_rdLambdaSqrt, INTRA_PLANAR, m_intraBestCosts, m_intraBestModes, num_cand);
#else
        StoreFewBestModes(cost + m_intraModeBitcost[INTRA_PLANAR], INTRA_PLANAR, m_intraBestCosts, m_intraBestModes, num_cand);
#endif

        MFX_HEVC_PP::h265_PredictIntra_DC_8u(predPel, pred_ptr, width, width, 1);
        cost = tuHad(pSrc, m_pitchSrc, pred_ptr, width, width, width);
        pred_ptr += width * width;
        FillSubPartIntraLumaDir(abs_part_idx, depth, tr_depth, INTRA_DC);
        m_intraModeBitcost[INTRA_DC] = GetIntraLumaBitCost(this, abs_part_idx);

#if SQRT_LAMBDA
        StoreFewBestModes(cost + (int(m_intraModeBitcost[INTRA_DC]/this->m_rdLambda)/256)*this->m_rdLambdaSqrt, INTRA_DC, m_intraBestCosts, m_intraBestModes, num_cand);
#else
        StoreFewBestModes(cost + m_intraModeBitcost[INTRA_DC], INTRA_DC, m_intraBestCosts, m_intraBestModes, num_cand);
#endif

        (m_par->intraAngModes == 2)
            ? MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_All_Even_8u)(predPel, predPelFilt, pred_ptr, width)
            : MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_All_8u)(predPel, predPelFilt, pred_ptr, width);

        Ipp8u step = (Ipp8u)m_par->intraAngModes;
        for (Ipp8u luma_dir = 2; luma_dir < 35; luma_dir += step) {
            cost = (luma_dir < 18)
                ? tuHad(m_tuSrcTransposed, width, pred_ptr, width, width, width)
                : tuHad(pSrc, m_pitchSrc, pred_ptr, width, width, width);
            FillSubPartIntraLumaDir(abs_part_idx, depth, tr_depth, luma_dir);
            m_intraModeBitcost[luma_dir] = GetIntraLumaBitCost(this, abs_part_idx);

#if SQRT_LAMBDA
            StoreFewBestModes(cost + (int(m_intraModeBitcost[luma_dir]/this->m_rdLambda)/256)*this->m_rdLambdaSqrt, luma_dir, m_intraBestCosts, m_intraBestModes, num_cand);
#else
            StoreFewBestModes(cost + m_intraModeBitcost[luma_dir], luma_dir, m_intraBestCosts, m_intraBestModes, num_cand);
#endif
            pred_ptr += width * width * step;
        }
    }
    else
    {
        m_data = m_dataSave;
        FillSubPart(abs_part_idx, depth, tr_depth, part_size, INTRA_PLANAR, m_par->QP);

        CalcCostLuma(abs_part_idx, offset, depth, tr_depth, COST_PRED_TR_0, part_size, INTRA_PLANAR, &cost);
        m_intraModeBitcost[INTRA_PLANAR] = GetIntraLumaBitCost(this, abs_part_idx);

#if SQRT_LAMBDA
        StoreFewBestModes(cost + (int(m_intraModeBitcost[INTRA_PLANAR]/this->m_rdLambda)/256)*this->m_rdLambdaSqrt, INTRA_PLANAR, m_intraBestCosts, m_intraBestModes, num_cand);
#else
        StoreFewBestModes(cost + m_intraModeBitcost[INTRA_PLANAR], INTRA_PLANAR, m_intraBestCosts, m_intraBestModes, num_cand);
#endif

        CalcCostLuma(abs_part_idx, offset, depth, tr_depth, COST_PRED_TR_0, part_size, INTRA_DC, &cost);
        m_intraModeBitcost[INTRA_DC] = GetIntraLumaBitCost(this, abs_part_idx);

#if SQRT_LAMBDA
        StoreFewBestModes(cost + (int(m_intraModeBitcost[INTRA_DC]/this->m_rdLambda)/256)*this->m_rdLambdaSqrt, INTRA_DC, m_intraBestCosts, m_intraBestModes, num_cand);
#else
        StoreFewBestModes(cost + m_intraModeBitcost[INTRA_DC], INTRA_DC, m_intraBestCosts, m_intraBestModes, num_cand);
#endif

        Ipp8u step = (Ipp8u)m_par->intraAngModes;
        for (Ipp8u luma_dir = 2; luma_dir <= 34; luma_dir += step) {
            CalcCostLuma(abs_part_idx, offset, depth, tr_depth, COST_PRED_TR_0, part_size, luma_dir, &cost);
            m_intraModeBitcost[luma_dir] = GetIntraLumaBitCost(this, abs_part_idx);
#if SQRT_LAMBDA
                StoreFewBestModes(cost + (int(m_intraModeBitcost[luma_dir]/this->m_rdLambda)/256)*this->m_rdLambdaSqrt, luma_dir, m_intraBestCosts, m_intraBestModes, num_cand);
#else
                StoreFewBestModes(cost + m_intraModeBitcost[luma_dir], luma_dir, m_intraBestCosts, m_intraBestModes, num_cand);
#endif
        }
    }

    if (m_par->intraAngModes == 2)
    {
        // select odd angular modes for refinement stage
        Ipp8u  odd_modes[16];
        Ipp32s num_odd_modes = 0;
        for (Ipp8u odd_mode = 3; odd_mode < 35; odd_mode += 2)
            for (Ipp32s i = 0; i < num_cand; i++)
                if (m_intraBestModes[i] == odd_mode - 1 || m_intraBestModes[i] == odd_mode + 1) {
                    odd_modes[num_odd_modes++] = odd_mode;
                    break;
                }

        if (tr_split_mode != SPLIT_MUST)
        {
            for (Ipp32s i = 0; i < num_odd_modes; i++) {
                Ipp8u luma_dir = odd_modes[i];
                Ipp32s diff = MIN(abs(luma_dir - 10), abs(luma_dir - 26));
                bool need_filter = (diff > h265_filteredModes[h265_log2table[width - 4] - 2]);
                Ipp8u * pred_pels = need_filter ? predPelFilt : predPel;
                PixType *pred_ptr = m_predIntraAll + luma_dir * width * width;

                MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_NoTranspose_8u)(luma_dir, pred_pels, pred_ptr, width, width);
                cost = (luma_dir < 18)
                    ? tuHad(m_tuSrcTransposed, width, pred_ptr, width, width, width)
                    : tuHad(pSrc, m_pitchSrc, pred_ptr, width, width, width);

                FillSubPartIntraLumaDir(abs_part_idx, depth, tr_depth, luma_dir);
                m_intraModeBitcost[luma_dir] = GetIntraLumaBitCost(this, abs_part_idx);
#if SQRT_LAMBDA
                StoreFewBestModes(cost + (int(m_intraModeBitcost[luma_dir]/this->m_rdLambda)/256)*this->m_rdLambdaSqrt, luma_dir, m_intraBestCosts, m_intraBestModes, num_cand);
#else
                StoreFewBestModes(cost + m_intraModeBitcost[luma_dir], luma_dir, m_intraBestCosts, m_intraBestModes, num_cand);
#endif
            }
        }
        else
        {
            for (Ipp32s i = 0; i < num_odd_modes; i++) {
                Ipp8u luma_dir = odd_modes[i];
                CalcCostLuma(abs_part_idx, offset, depth, tr_depth, COST_PRED_TR_0, part_size, luma_dir, &cost);
                m_intraModeBitcost[luma_dir] = GetIntraLumaBitCost(this, abs_part_idx);
#if SQRT_LAMBDA
                StoreFewBestModes(cost + (int(m_intraModeBitcost[luma_dir]/this->m_rdLambda)/256)*this->m_rdLambdaSqrt, luma_dir, m_intraBestCosts, m_intraBestModes, num_cand);
#else
                StoreFewBestModes(cost + m_intraModeBitcost[luma_dir], luma_dir, m_intraBestCosts, m_intraBestModes, num_cand);
#endif

            }
        }
    }
}


void H265CU::IntraLumaModeDecisionRDO(Ipp32s abs_part_idx, Ipp32u offset, Ipp8u depth, Ipp8u tr_depth, CABAC_CONTEXT_H265 * initCtx, Ipp32s num_cand1)
{
    Ipp32s width = m_par->MaxCUSize >> (depth + tr_depth);
    Ipp32u num_parts = (m_par->NumPartInCU >> ((depth + tr_depth) << 1));
    Ipp32s offset_luma_tu = GetLumaOffset(m_par, abs_part_idx, m_pitchRec);
    Ipp32s tuSplitIntra = (m_par->tuSplitIntra == 1 || m_par->tuSplitIntra == 3 && m_cslice->slice_type == I_SLICE);
    Ipp8u  part_size = (Ipp8u)(tr_depth == 1 ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
    Ipp32s num_cand2 = m_par->num_cand_2[m_par->Log2MaxCUSize - (depth + tr_depth)];
    IppiSize roi = { width, width };
    CostType cost;

    bool restoreContextAndRec = false;

    CABAC_CONTEXT_H265 bestCtx[NUM_CABAC_CONTEXT];
    CostType intraBestCosts_tmp[35];
    Ipp8u intraBestModes_tmp[35];
    CostOpt finalRdoCostOpt = COST_REC_TR_ALL;
    for (Ipp32s i = 0; i < num_cand2; i++) intraBestCosts_tmp[i] = COST_MAX;

    if (tuSplitIntra)
    {
        for (Ipp8u i = 0; i < num_cand1; i++) {
            Ipp8u luma_dir = m_intraBestModes[i];

            if (m_rdOptFlag)
                m_bsf->CtxRestore(initCtx, 0, NUM_CABAC_CONTEXT);
            CalcCostLuma(abs_part_idx, offset, depth, tr_depth, COST_REC_TR_0, part_size, luma_dir, &cost);
            cost += m_intraModeBitcost[luma_dir];

            StoreFewBestModes(cost, luma_dir, intraBestCosts_tmp, intraBestModes_tmp, num_cand2);
        }
    }
    else
    {
        finalRdoCostOpt = COST_REC_TR_0;
        num_cand2 = num_cand1;
        small_memcpy(intraBestCosts_tmp, m_intraBestCosts, num_cand1 * sizeof(intraBestCosts_tmp[0]));
        small_memcpy(intraBestModes_tmp, m_intraBestModes, num_cand1 * sizeof(intraBestModes_tmp[0]));
    }

    m_intraBestCosts[0] = COST_MAX;

    for (Ipp8u i = 0; i < num_cand2; i++) {
        Ipp8u luma_dir = intraBestModes_tmp[i];
        if (m_rdOptFlag)
            m_bsf->CtxRestore(initCtx, 0, NUM_CABAC_CONTEXT);
        CalcCostLuma(abs_part_idx, offset, depth, tr_depth, finalRdoCostOpt, part_size, luma_dir, &cost);
        cost += m_intraModeBitcost[luma_dir];

        if (m_intraBestCosts[0] > cost) {
            m_intraBestCosts[0] = cost;
            m_intraBestModes[0] = luma_dir;
            restoreContextAndRec = (i + 1 != num_cand2);
            if (restoreContextAndRec) {
                if (m_rdOptFlag)
                    m_bsf->CtxSave(bestCtx, 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(m_yRec + offset_luma_tu, m_pitchRec, m_recLumaSaveCu[depth+tr_depth], width, roi);
            }
            small_memcpy(m_dataBest + ((depth + tr_depth) << m_par->Log2NumPartInCU) + abs_part_idx,
                m_dataTemp + ((depth + tr_depth) << m_par->Log2NumPartInCU) + abs_part_idx,
                sizeof(H265CUData) * num_parts);
        }
    }

    if (restoreContextAndRec)
    {
        if (m_rdOptFlag)
            m_bsf->CtxRestore(bestCtx, 0, NUM_CABAC_CONTEXT);
        ippiCopy_8u_C1R(m_recLumaSaveCu[depth+tr_depth], width, m_yRec + offset_luma_tu, m_pitchRec, roi);
    }
}


Ipp8u H265CU::GetTrSplitMode(Ipp32s abs_part_idx, Ipp8u depth, Ipp8u tr_depth, Ipp8u part_size, Ipp8u is_luma, Ipp8u strict)
{
    Ipp32s width = m_par->MaxCUSize >> (depth + tr_depth);
    Ipp8u split_mode = SPLIT_NONE;
    Ipp32u tuLog2MinSizeInCu = H265Enc::GetQuadtreeTuLog2MinSizeInCu(m_par, abs_part_idx, m_par->MaxCUSize >> depth, part_size, MODE_INTRA);

    if (width > 32) {
        split_mode = SPLIT_MUST;
    }
    else if ((m_par->MaxCUSize >> (depth + tr_depth)) > 4 &&
        (m_par->Log2MaxCUSize - depth - tr_depth > tuLog2MinSizeInCu)) {

        Ipp32u lpel_x = m_ctbPelX +
            ((h265_scan_z2r[m_par->MaxCUDepth][abs_part_idx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
        Ipp32u tpel_y = m_ctbPelY +
            ((h265_scan_z2r[m_par->MaxCUDepth][abs_part_idx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);

        if ((strict && m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MaxSize) ||
            lpel_x + width > m_par->Width || tpel_y + width > m_par->Height)
            split_mode = SPLIT_MUST;
        else
            split_mode = SPLIT_TRY;
    }
    return split_mode;
}

void H265CU::IntraPred(Ipp32u abs_part_idx, Ipp8u depth) {
    Ipp32s depth_max = m_data[abs_part_idx].depth + m_data[abs_part_idx].trIdx;
    Ipp32u num_parts = ( m_par->NumPartInCU >> (depth<<1) )>>2;
    Ipp32u i;

    if (depth == depth_max - 1 && (m_data[abs_part_idx].size >> m_data[abs_part_idx].trIdx) == 4) {
        for (i = 0; i < 4; i++) {
            Ipp32s abs_part_idx_tmp = abs_part_idx + num_parts * i;
            IntraPredTu(abs_part_idx_tmp, m_data[abs_part_idx_tmp].size >> m_data[abs_part_idx_tmp].trIdx,
                m_data[abs_part_idx_tmp].intraLumaDir, 1);
        }
        IntraPredTu(abs_part_idx, m_data[abs_part_idx].size >> m_data[abs_part_idx].trIdx << 1, m_data[abs_part_idx].intraChromaDir, 0);
    } else if (depth == depth_max) {
        IntraPredTu(abs_part_idx, m_data[abs_part_idx].size >> m_data[abs_part_idx].trIdx, m_data[abs_part_idx].intraLumaDir, 1);
        IntraPredTu(abs_part_idx, m_data[abs_part_idx].size >> m_data[abs_part_idx].trIdx, m_data[abs_part_idx].intraChromaDir, 0);
    } else {
        for (i = 0; i < 4; i++) {
            IntraPred(abs_part_idx + num_parts * i, depth+1);
        }
    }
}


void H265CU::GetInitAvailablity()
{
    Ipp32s maxDepth = m_par->Log2MaxCUSize - m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp8u *RasterToZscanTab = (Ipp8u*)h265_scan_r2z[maxDepth];

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
            rasterIdx = (numMinTUInLCU - i) * numMinTUInLCU - 1;
            zScanIdx = RasterToZscanTab[rasterIdx];

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
        rasterIdx = numMinTUInLCU * numMinTUInLCU - 1;
        zScanIdx = RasterToZscanTab[rasterIdx];

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
            rasterIdx = (numMinTUInLCU - 1) * numMinTUInLCU + i;
            zScanIdx = RasterToZscanTab[rasterIdx];

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
            rasterIdx = (numMinTUInLCU - 1) * numMinTUInLCU + i;
            zScanIdx = RasterToZscanTab[rasterIdx];

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
