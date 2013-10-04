//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_optimization.h"

static Ipp32s FilteredModes[] = {10, 7, 1, 0, 10};

static
void IsAboveLeftAvailable(H265CU *pCU,
                          Ipp32s blockZScanIdx,
                          Ipp32s width,
                          Ipp8u ConstrainedIntraPredFlag,
                          Ipp8u *inNeighborFlags,
                          Ipp8u *outNeighborFlags)
{
    Ipp32s maxDepth = pCU->par->Log2MaxCUSize - pCU->par->Log2MinTUSize;
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
                outNeighborFlags[0] = ((pCU->data[zScanIdx].pred_mode == MODE_INTRA) ? 1 : 0);
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
    Ipp32s maxDepth = pCU->par->Log2MaxCUSize - pCU->par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s numMinTUInPU = width >> pCU->par->Log2MinTUSize;
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
                outNeighborFlags[i] = ((pCU->data[zScanIdx].pred_mode == MODE_INTRA) ? 1 : 0);
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
    Ipp32s maxDepth = pCU->par->Log2MaxCUSize - pCU->par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s numMinTUInPU = width >> pCU->par->Log2MinTUSize;
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

            if ((pCU->ctb_pelx + (aboveRightPartColumn << pCU->par->Log2MinTUSize)) >=
                pCU->par->Width)
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

            if ((pCU->ctb_pelx + (aboveRightPartColumn << pCU->par->Log2MinTUSize)) >=
                pCU->par->Width)
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
                            outNeighborFlags[i] = ((pCU->data[zScanIdx].pred_mode == MODE_INTRA) ? 1 : 0);
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
    Ipp32s maxDepth = pCU->par->Log2MaxCUSize - pCU->par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s numMinTUInPU = width >> pCU->par->Log2MinTUSize;
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
                outNeighborFlags[numMinTUInPU - 1 - i] = ((pCU->data[zScanIdx].pred_mode == MODE_INTRA) ? 1 : 0);
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
    Ipp32s maxDepth = pCU->par->Log2MaxCUSize - pCU->par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s numMinTUInPU = width >> pCU->par->Log2MinTUSize;
    Ipp32s rasterBottomLeftPartIdx = h265_scan_z2r[maxDepth][blockZScanIdx] + (numMinTUInPU - 1) * numMinTUInLCU;
    Ipp32s zScanBottomLefPartIdx = h265_scan_r2z[maxDepth][rasterBottomLeftPartIdx];
    Ipp32s bottomLeftPartRow = rasterBottomLeftPartIdx >> maxDepth;
    Ipp32s bottomLeftPartColumn = rasterBottomLeftPartIdx & (numMinTUInLCU - 1);
    Ipp32s i;

    for (i = numMinTUInPU - 1; i >= 0; i--)
    {
        Ipp32s belowLefPartRow = bottomLeftPartRow + i + 1;

        if ((pCU->ctb_pely + (belowLefPartRow << pCU->par->Log2MinTUSize)) >=
             pCU->par->Height)
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
                        outNeighborFlags[numMinTUInPU - 1 - i] = ((pCU->data[zScanIdx].pred_mode == MODE_INTRA) ? 1 : 0);
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
    Ipp32s numMinTUInPU = width >> pCU->par->Log2MinTUSize;

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
void GetPredPels(H265VideoParam *par,
                 PixType* src,
                 PixType* PredPel,
                 Ipp8u* neighborFlags,
                 Ipp32s width,
                 Ipp32s srcPitch,
                 Ipp32s isLuma)
{
    PixType* tmpSrcPtr;
    PixType dcval;
    Ipp32s minTUSize = 1 << par->Log2MinTUSize;
    Ipp32s numUnitsInCU = width >> par->Log2MinTUSize;
    Ipp32s numIntraNeighbor;
    Ipp32s i, j;

    if (isLuma)
    {
        dcval = (1 << (BIT_DEPTH_LUMA - 1));
    }
    else
    {
        dcval = (1 << (BIT_DEPTH_CHROMA - 1));
        minTUSize >>= 1;
        numUnitsInCU = width >> (par->Log2MinTUSize - 1);
    }

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

void H265CU::IntraPredTU(Ipp32s blockZScanIdx, Ipp32s width, Ipp32s pred_mode, Ipp8u is_luma)
{
    PixType PredPel[4*64+1];
    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][blockZScanIdx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);
    PixType *pRec;

    for (Ipp32s c_idx = 0; c_idx < (is_luma ? 1 : 2); c_idx ++) {
        Ipp32s pitch = is_luma ? pitch_rec_luma : pitch_rec_chroma;

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

                if (diff <= FilteredModes[h265_log2table[width - 4] - 2])
                {
                    isFilterNeeded = false;
                }
            }

            pRec = y_rec + ((PUStartRow * pitch_rec_luma + PUStartColumn) << par->Log2MinTUSize);

            GetAvailablity(this, blockZScanIdx, width, par->cpps->constrained_intra_pred_flag,
                inNeighborFlags, outNeighborFlags);
            GetPredPels(par, pRec, PredPel, outNeighborFlags, width,
                pitch_rec_luma, 1);

            if (par->csps->strong_intra_smoothing_enabled_flag && isFilterNeeded)
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
        }
        else
        {
            if (c_idx == 0) {
                GetAvailablity(this, blockZScanIdx, width << 1, par->cpps->constrained_intra_pred_flag,
                    inNeighborFlags, outNeighborFlags);
                pRec = u_rec + ((PUStartRow * pitch_rec_chroma + PUStartColumn) << (par->Log2MinTUSize - 1));
            } else {
                pRec = v_rec + ((PUStartRow * pitch_rec_chroma + PUStartColumn) << (par->Log2MinTUSize - 1));
            }
            GetPredPels(par, pRec, PredPel, outNeighborFlags, width,
                pitch, 0);
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
}

void H265CU::IntraPredTULumaAllHAD(Ipp32s abs_part_idx, Ipp32s width)
{
    __ALIGN32 PixType PredPel[4*64+1];
    __ALIGN32 PixType PredPelFilt[4*64+1];
    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][abs_part_idx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    PixType *pSrc = y_src + ((PUStartRow * pitch_src + PUStartColumn) << par->Log2MinTUSize);
    PixType *pRec = y_rec + ((PUStartRow * pitch_rec_luma + PUStartColumn) << par->Log2MinTUSize);

    GetAvailablity(this, abs_part_idx, width, par->cpps->constrained_intra_pred_flag,
        inNeighborFlags, outNeighborFlags);
    GetPredPels(par, pRec, PredPel, outNeighborFlags, width,
        pitch_rec_luma, 1);

    small_memcpy(PredPelFilt, PredPel, 4*width+1);

    if (par->csps->strong_intra_smoothing_enabled_flag && width == 32)
    {
        Ipp32s threshold = 1 << (BIT_DEPTH_LUMA - 5);        

        Ipp32s topLeft = PredPel[0];
        Ipp32s topRight = PredPel[2*width];
        Ipp32s midHor = PredPel[width];

        Ipp32s bottomLeft = PredPel[4*width];
        Ipp32s midVer = PredPel[3*width];

        bool bilinearLeft = abs(topLeft + topRight - 2*midHor) < threshold; 
        bool bilinearAbove = abs(topLeft + bottomLeft - 2*midVer) < threshold;

        if (bilinearLeft && bilinearAbove)
        {
            h265_FilterPredictPels_Bilinear_8u(PredPelFilt, width, topLeft, bottomLeft, topRight);
        }
        else
        {
            h265_FilterPredictPels_8u(PredPelFilt, width);
        }
    } else {
        h265_FilterPredictPels_8u(PredPelFilt, width);
    }

    IppiSize size = {width, width};
    ippiTranspose_8u_C1R(pSrc, pitch_src, tu_src_transposed, width, size);

    PixType *pred_ptr = pred_intra_all;

    MFX_HEVC_PP::h265_PredictIntra_Planar_8u(INTRA_HOR <= FilteredModes[h265_log2table[width - 4] - 2] ? PredPel : PredPelFilt, pred_ptr, width, width);
    intra_cost[0] = h265_tu_had(pSrc, pred_ptr, pitch_src, width, width, width);
    pred_ptr += width * width;

    MFX_HEVC_PP::h265_PredictIntra_DC_8u(PredPel, pred_ptr, width, width, 1);
    intra_cost[1] = h265_tu_had(pSrc, pred_ptr, pitch_src, width, width, width);
    pred_ptr += width * width;

    MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_All_8u)(PredPel, PredPelFilt, pred_ptr, width);

    for (Ipp8u mode = 2; mode < 35; mode++) {
        if (mode < 18) {
            intra_cost[mode] = h265_tu_had(tu_src_transposed, pred_ptr, width, width, width, width);
        } else {
            intra_cost[mode] = h265_tu_had(pSrc, pred_ptr, pitch_src, width, width, width);
        }
        pred_ptr += width * width;
    }
}

Ipp8u H265CU::GetTRSplitMode(Ipp32s abs_part_idx, Ipp8u depth, Ipp8u tr_depth, Ipp8u part_size, Ipp8u is_luma)
{
    Ipp32s width = par->MaxCUSize >> (depth + tr_depth);
    Ipp8u split_mode = SPLIT_NONE;

    if (width > 32) {
        split_mode = SPLIT_MUST;
    } else if ((par->MaxCUSize >> (depth + tr_depth)) > 4 &&
            (par->Log2MaxCUSize - depth - tr_depth >
                getQuadtreeTULog2MinSizeInCU(abs_part_idx, par->MaxCUSize >> depth, part_size, MODE_INTRA)))
    {
                Ipp32u lpel_x   = ctb_pelx +
                    ((h265_scan_z2r[par->MaxCUDepth][abs_part_idx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
                Ipp32u tpel_y   = ctb_pely +
                    ((h265_scan_z2r[par->MaxCUDepth][abs_part_idx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);

                if (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MaxSize ||
                    lpel_x + width >= par->Width || tpel_y + width >= par->Height) {
                    split_mode = SPLIT_MUST;
                } else {
                    split_mode = SPLIT_TRY;
                }
    }
    return split_mode;
}

void H265CU::IntraPred(Ipp32u abs_part_idx, Ipp8u depth) {
    Ipp32s depth_max = data[abs_part_idx].depth + data[abs_part_idx].tr_idx;
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) )>>2;
    Ipp32u i;

    if (depth == depth_max - 1 && (data[abs_part_idx].size >> data[abs_part_idx].tr_idx) == 4) {
        for (i = 0; i < 4; i++) {
            Ipp32s abs_part_idx_tmp = abs_part_idx + num_parts * i;
            IntraPredTU(abs_part_idx_tmp, data[abs_part_idx_tmp].size >> data[abs_part_idx_tmp].tr_idx,
                data[abs_part_idx_tmp].intra_luma_dir, 1);
        }
        IntraPredTU(abs_part_idx, data[abs_part_idx].size >> data[abs_part_idx].tr_idx << 1, data[abs_part_idx].intra_chroma_dir, 0);
    } else if (depth == depth_max) {
        IntraPredTU(abs_part_idx, data[abs_part_idx].size >> data[abs_part_idx].tr_idx, data[abs_part_idx].intra_luma_dir, 1);
        IntraPredTU(abs_part_idx, data[abs_part_idx].size >> data[abs_part_idx].tr_idx, data[abs_part_idx].intra_chroma_dir, 0);
    } else {
        for (i = 0; i < 4; i++) {
            IntraPred(abs_part_idx + num_parts * i, depth+1);
        }
    }
}


void H265CU::GetInitAvailablity()
{
    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp8u *RasterToZscanTab = (Ipp8u*)h265_scan_r2z[maxDepth];

    Ipp32s rasterIdx, zScanIdx;
    Ipp32s i;
    Ipp8u constrained_intra_flag = par->cpps->constrained_intra_pred_flag;

    for (i = 0; i < 3 * numMinTUInLCU + 1; i++)
    {
        inNeighborFlags[i] = 0;
    }

    /* Left */
    if (left_addr >= cslice->slice_segment_address /* and in one tile */)
    {
        for (i = 0; i < numMinTUInLCU; i++)
        {
            rasterIdx = (numMinTUInLCU - i) * numMinTUInLCU - 1;
            zScanIdx = RasterToZscanTab[rasterIdx];

            if (constrained_intra_flag)
            {
                if (p_left[zScanIdx].pred_mode == MODE_INTRA)
                {
                    inNeighborFlags[i] = 1;
                }
            }
            else
            {
                inNeighborFlags[i] = 1;
            }
        }
    }

    /* Above Left */
    if (above_left_addr >= cslice->slice_segment_address /* and in one tile */)
    {
        rasterIdx = numMinTUInLCU * numMinTUInLCU - 1;
        zScanIdx = RasterToZscanTab[rasterIdx];

        if (constrained_intra_flag)
        {
            if (p_above_left[zScanIdx].pred_mode == MODE_INTRA)
            {
                inNeighborFlags[numMinTUInLCU] = 1;
            }
        }
        else
        {
            inNeighborFlags[numMinTUInLCU] = 1;
        }
    }

    /* Above */
    if (above_addr >= cslice->slice_segment_address /* and in one tile */)
    {
        for (i = 0; i < numMinTUInLCU; i++)
        {
            rasterIdx = (numMinTUInLCU - 1) * numMinTUInLCU + i;
            zScanIdx = RasterToZscanTab[rasterIdx];

            if (constrained_intra_flag)
            {
                if (p_above[zScanIdx].pred_mode == MODE_INTRA)
                {
                    inNeighborFlags[numMinTUInLCU+1+i] = 1;
                }
            }
            else
            {
                inNeighborFlags[numMinTUInLCU+1+i] = 1;
            }
        }
    }

    /* Above Right*/
    if (above_right_addr >= cslice->slice_segment_address /* and in one tile */)
    {
        for (i = 0; i < numMinTUInLCU; i++)
        {
            rasterIdx = (numMinTUInLCU - 1) * numMinTUInLCU + i;
            zScanIdx = RasterToZscanTab[rasterIdx];

            if (constrained_intra_flag)
            {
                if (p_above_right[zScanIdx].pred_mode == MODE_INTRA)
                {
                    inNeighborFlags[2*numMinTUInLCU+1+i] = 1;
                }
            }
            else
            {
                inNeighborFlags[2*numMinTUInLCU+1+i] = 1;
            }
        }
    }
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
