//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <limits.h> /* for INT_MAX on Linux/Android */
#include <algorithm>

#include "ippi.h"
#include "ippvc.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_enc.h"

#include "mfx_h265_enc_cm_defs.h"
#include "mfx_h265_enc_cm.h"

namespace H265Enc {

#if defined (MFX_ENABLE_CM)

extern Ipp32s cmMvW[PU_MAX];
extern Ipp32s cmMvH[PU_MAX];

extern CmMbIntraDist * cmMbIntraDist[2];
extern Ipp32u pitchIntra[2];

extern mfxU32 *distCpu[2][2][CM_REF_BUF_LEN][PU_MAX];
extern Ipp32u pitchDist[PU_MAX];

extern mfxI16Pair * mvCpu[2][2][CM_REF_BUF_LEN][PU_MAX];
extern Ipp32u pitchMv[PU_MAX];

extern mfxU32 ** pDistCpu[2][2][CM_REF_BUF_LEN];
extern mfxI16Pair ** pMvCpu[2][2][CM_REF_BUF_LEN];

extern Ipp32u intraPitch[2];

extern Ipp32s cmCurIdx;
extern Ipp32s cmNextIdx;

extern H265RefMatchData * recBufData;

#endif // MFX_ENABLE_CM

#define NO_TRANSFORM_SPLIT_INTRAPRED_STAGE1 0

#define SWAP_REC_IDX(a,b) {Ipp8u tmp=a; a=b; b=tmp;}

#if defined(DUMP_COSTS_CU) || defined (DUMP_COSTS_TU)
FILE *fp_cu = NULL, *fp_tu = NULL;
// QP; slice I/P or TU I/P; width; costNonSplit; costSplit
typedef struct {
    Ipp8u QP;
    Ipp8u isNotI;
    Ipp8u width;
    Ipp8u reserved;
    Ipp32f costNotSplit;
    Ipp32f costSplit;
} costStat;

static void inline printCostStat(FILE*fp, Ipp8u QP, Ipp8u isNotI, Ipp8u width, Ipp32f costNotSplit, Ipp32f costSplit)
{
    costStat stat;
    stat.QP = QP;
    stat.isNotI = isNotI;
    stat.width = width;
    stat.costNotSplit = costNotSplit;
    stat.costSplit = costSplit;

    fwrite(&stat, sizeof(stat), 1, fp);
}
#endif

Ipp32s GetLumaOffset(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch)
{
    Ipp32s maxDepth = par->MaxTotalDepth;
    Ipp32s puRasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
    Ipp32s puStartRow = puRasterIdx >> maxDepth;
    Ipp32s puStartColumn = puRasterIdx & (par->NumMinTUInMaxCU - 1);

    return (puStartRow * pitch + puStartColumn) << par->Log2MinTUSize;
}

// chroma offset for NV12
Ipp32s GetChromaOffset(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch)
{
    Ipp32s maxDepth = par->MaxTotalDepth;
    Ipp32s puRasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
    Ipp32s puStartRow = puRasterIdx >> maxDepth;
    Ipp32s puStartColumn = puRasterIdx & (par->NumMinTUInMaxCU - 1);

    return ((puStartRow * pitch >> 1) + puStartColumn) << par->Log2MinTUSize;
}

// propagate data[0] thru data[1..numParts-1] where numParts=2^n
void PropagateSubPart(H265CUData *data, Ipp32s numParts)
{
    VM_ASSERT(numParts ^ (numParts - 1) == 0); // check that numParts=2^n
    for (Ipp32s i = 1; i < numParts; i <<= 1)
        small_memcpy(data + i, data, i * sizeof(H265CUData));
}

void H265CU::GetPartOffsetAndSize(Ipp32s idx,
                                  Ipp32s partMode,
                                  Ipp32s cuSize,
                                  Ipp32s& partX,
                                  Ipp32s& partY,
                                  Ipp32s& partWidth,
                                  Ipp32s& partHeight)
{
    switch (partMode)
    {
    case PART_SIZE_2NxN:
        partWidth = cuSize;
        partHeight = cuSize >> 1;
        partX = 0;
        partY = idx * (cuSize >> 1);
        break;
    case PART_SIZE_Nx2N:
        partWidth = cuSize >> 1;
        partHeight = cuSize;
        partX = idx * (cuSize >> 1);
        partY = 0;
        break;
    case PART_SIZE_NxN:
        partWidth = cuSize >> 1;
        partHeight = cuSize >> 1;
        partX = (idx & 1) * (cuSize >> 1);
        partY = (idx >> 1) * (cuSize >> 1);
        break;
    case PART_SIZE_2NxnU:
        partWidth = cuSize;
        partHeight = (cuSize >> 2) + idx * (cuSize >> 1);
        partX = 0;
        partY = idx * (cuSize >> 2);
        break;
    case PART_SIZE_2NxnD:
        partWidth = cuSize;
        partHeight = (cuSize >> 2) + (1 - idx) * (cuSize >> 1);
        partX = 0;
        partY = idx * ((cuSize >> 1) + (cuSize >> 2));
        break;
    case PART_SIZE_nLx2N:
        partWidth = (cuSize >> 2) + idx * (cuSize >> 1);
        partHeight = cuSize;
        partX = idx * (cuSize >> 2);
        partY = 0;
        break;
    case PART_SIZE_nRx2N:
        partWidth = (cuSize >> 2) + (1 - idx) * (cuSize >> 1);
        partHeight = cuSize;
        partX = idx * ((cuSize >> 1) + (cuSize >> 2));
        partY = 0;
        break;
    case PART_SIZE_2Nx2N:
    default:
        partWidth = cuSize;
        partHeight = cuSize;
        partX = 0;
        partY = 0;
        break;
    }
}

void H265CU::GetPartAddr(Ipp32s partIdx,
                         Ipp32s partMode,
                         Ipp32s numPartition,
                         Ipp32s& partAddr)
{
    switch (partMode)
    {
    case PART_SIZE_2NxN:
        partAddr = (partIdx == 0) ? 0 : numPartition >> 1;
        break;
    case PART_SIZE_Nx2N:
        partAddr = (partIdx == 0) ? 0 : numPartition >> 2;
        break;
    case PART_SIZE_NxN:
        partAddr = (numPartition >> 2) * partIdx;
        break;
    case PART_SIZE_2NxnU:
        partAddr = (partIdx == 0) ? 0 : numPartition >> 3;
        break;
    case PART_SIZE_2NxnD:
        partAddr = (partIdx == 0) ? 0 : (numPartition >> 1) + (numPartition >> 3);
        break;
    case PART_SIZE_nLx2N:
        partAddr = (partIdx == 0) ? 0 : numPartition >> 4;
        break;
    case PART_SIZE_nRx2N:
        partAddr = (partIdx == 0) ? 0 : (numPartition >> 2) + (numPartition >> 4);
        break;
    case PART_SIZE_2Nx2N:
    default:
        partAddr = 0;
        break;
    }
}

static Ipp32s GetDistScaleFactor(Ipp32s currRefTb,
                                 Ipp32s colRefTb)
{
    if (currRefTb == colRefTb)
    {
        return 4096;
    }
    else
    {
        Ipp32s tb = Saturate(-128, 127, currRefTb);
        Ipp32s td = Saturate(-128, 127, colRefTb);
        Ipp32s tx = (16384 + abs(td/2))/td;
        Ipp32s distScaleFactor = Saturate(-4096, 4095, (tb * tx + 32) >> 6);
        return distScaleFactor;
    }
}

Ipp32u H265CU::GetCtxSkipFlag(Ipp32u absPartIdx)
{
    H265CUPtr pcTempCU;
    Ipp32u    uiCtx = 0;

    // Get BCBP of left PU
    GetPuLeft(&pcTempCU, m_absIdxInLcu + absPartIdx );
    uiCtx    = ( pcTempCU.ctbData ) ? isSkipped(pcTempCU.ctbData, pcTempCU.absPartIdx ) : 0;

    // Get BCBP of above PU
    GetPuAbove(&pcTempCU, m_absIdxInLcu + absPartIdx );
    uiCtx   += ( pcTempCU.ctbData ) ? isSkipped(pcTempCU.ctbData, pcTempCU.absPartIdx ) : 0;

    return uiCtx;
}

Ipp32u H265CU::GetCtxSplitFlag(Ipp32u absPartIdx, Ipp32u depth)
{
    H265CUPtr tempCU;
    Ipp32u    ctx;
    // Get left split flag
    GetPuLeft( &tempCU, m_absIdxInLcu + absPartIdx );
    ctx  = ( tempCU.ctbData ) ? ( ( tempCU.ctbData[tempCU.absPartIdx].depth > depth ) ? 1 : 0 ) : 0;

    // Get above split flag
    GetPuAbove(&tempCU, m_absIdxInLcu + absPartIdx );
    ctx += ( tempCU.ctbData ) ? ( ( tempCU.ctbData[tempCU.absPartIdx].depth > depth ) ? 1 : 0 ) : 0;

    return ctx;
}

Ipp8s H265CU::GetRefQp( Ipp32u currAbsIdxInLcu )
{
    Ipp32u        lPartIdx, aPartIdx;
    H265CUData* cULeft  = GetQpMinCuLeft ( lPartIdx, m_absIdxInLcu + currAbsIdxInLcu );
    H265CUData* cUAbove = GetQpMinCuAbove( aPartIdx, m_absIdxInLcu + currAbsIdxInLcu );
    return (((cULeft? cULeft[lPartIdx].qp: GetLastCodedQP( this, currAbsIdxInLcu )) + (cUAbove? cUAbove[aPartIdx].qp: GetLastCodedQP( this, currAbsIdxInLcu )) + 1) >> 1);
}

void H265CU::SetQpSubCUs(int qp, int absPartIdx, int depth, bool &foundNonZeroCbf)
{
    Ipp32u curPartNumb = m_par->NumPartInCU >> (depth << 1);
    Ipp32u curPartNumQ = curPartNumb >> 2;

    if (!foundNonZeroCbf)
    {
        if(m_data[absPartIdx].depth > depth)
        {
            for (Ipp32u partUnitIdx = 0; partUnitIdx < 4; partUnitIdx++)
            {
                SetQpSubCUs(qp, absPartIdx + partUnitIdx * curPartNumQ, depth + 1, foundNonZeroCbf);
            }
        }
        else
        {
            if(m_data[absPartIdx].cbf[0] || m_data[absPartIdx].cbf[1] || m_data[absPartIdx].cbf[2])
            {
                foundNonZeroCbf = true;
            }
            else
            {
                SetQpSubParts(qp, absPartIdx, depth);
            }
        }
    }
}

void H265CU::SetQpSubParts(int qp, int absPartIdx, int depth)
{
    int partNum = m_par->NumPartInCU >> (depth << 1);

    for (int idx = absPartIdx; idx < absPartIdx + partNum; idx++)
    {
        m_data[idx].qp = qp;
    }
}


void H265CU::CheckDeltaQp(void)
{
    int depth = m_data[0].depth;
    
    if( m_par->UseDQP && (m_par->MaxCUSize >> depth) >= m_par->MinCuDQPSize )
    {
        if( ! (m_data[0].cbf[0] & 0x1) && !(m_data[0].cbf[1] & 0x1) && !(m_data[0].cbf[2] & 0x1) )
        {
            SetQpSubParts(GetRefQp(0), 0, depth); // set QP to default QP
        }
    }
}

Ipp32u H265CU::GetCoefScanIdx(Ipp32u absPartIdx, Ipp32u width, Ipp32s isLuma, Ipp32s isIntra)
{
    Ipp32u ctxIdx;
    Ipp32u scanIdx;
    Ipp32u dirMode;

    if (!isIntra) {
        scanIdx = COEFF_SCAN_ZIGZAG;
        return scanIdx;
    }

    switch(width)
    {
    case  2: ctxIdx = 6; break;
    case  4: ctxIdx = 5; break;
    case  8: ctxIdx = 4; break;
    case 16: ctxIdx = 3; break;
    case 32: ctxIdx = 2; break;
    case 64: ctxIdx = 1; break;
    default: ctxIdx = 0; break;
    }

    if (isLuma) {
        dirMode = m_data[absPartIdx].intraLumaDir;
        scanIdx = COEFF_SCAN_ZIGZAG;
        if (ctxIdx >3 && ctxIdx < 6) //if multiple scans supported for transform size
        {
            scanIdx = abs((Ipp32s) dirMode - INTRA_VER) < 5 ? 1 : (abs((Ipp32s)dirMode - INTRA_HOR) < 5 ? 2 : 0);
        }
    }
    else {
        dirMode = m_data[absPartIdx].intraChromaDir;
        if (dirMode == INTRA_DM_CHROMA)
        {
            // get number of partitions in current CU
            Ipp32u depth = m_data[absPartIdx].depth;
            Ipp32u numParts = m_par->NumPartInCU >> (2 * depth);

            // get luma mode from upper-left corner of current CU
            dirMode = m_data[(absPartIdx/numParts)*numParts].intraLumaDir;
        }
        scanIdx = COEFF_SCAN_ZIGZAG;
        if (ctxIdx > 4 && ctxIdx < 7) //if multiple scans supported for transform size
        {
            scanIdx = abs((Ipp32s) dirMode - INTRA_VER) < 5 ? 1 : (abs((Ipp32s)dirMode - INTRA_HOR) < 5 ? 2 : 0);
        }
    }

    return scanIdx;
}

Ipp32u H265CU::GetCtxQtCbf(Ipp32u/* absPartIdx*/, EnumTextType type, Ipp32u trDepth)
{
  if (type) {
      return trDepth;
  }
  else {
      return trDepth == 0 ? 1 : 0;
  }
}

Ipp32u H265CU::GetQuadtreeTuLog2MinSizeInCu(Ipp32u absPartIdx)
{
    return H265Enc::GetQuadtreeTuLog2MinSizeInCu(m_par, absPartIdx, m_data[absPartIdx].size,
        m_data[absPartIdx].partSize, m_data[absPartIdx].predMode);
}

Ipp32u GetQuadtreeTuLog2MinSizeInCu(const H265VideoParam *par, Ipp32u absPartIdx, Ipp32s size,
                                    Ipp8u partSize, Ipp8u pred_mode)
{
    Ipp32u log2CbSize = h265_log2m2[size] + 2;
    Ipp32u quadtreeTUMaxDepth = (pred_mode == MODE_INTRA)
        ? par->QuadtreeTUMaxDepthIntra
        : par->QuadtreeTUMaxDepthInter;
    Ipp32s intraSplitFlag = ( pred_mode == MODE_INTRA && partSize == PART_SIZE_NxN ) ? 1 : 0;
    Ipp32s interSplitFlag = ((quadtreeTUMaxDepth == 1) && (pred_mode == MODE_INTER) && (partSize != PART_SIZE_2Nx2N) );

    Ipp32u log2MinTUSizeInCU = 0;
    if (log2CbSize < (par->QuadtreeTULog2MinSize + quadtreeTUMaxDepth - 1 + interSplitFlag + intraSplitFlag) )
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is < QuadtreeTULog2MinSize
        log2MinTUSizeInCU = par->QuadtreeTULog2MinSize;
    }
    else
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still >= QuadtreeTULog2MinSize
        log2MinTUSizeInCU = log2CbSize - ( quadtreeTUMaxDepth - 1 + interSplitFlag + intraSplitFlag); // stop when trafoDepth == hierarchy_depth = splitFlag
        if ( log2MinTUSizeInCU > par->QuadtreeTULog2MaxSize)
        {
            // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still > QuadtreeTULog2MaxSize
            log2MinTUSizeInCU = par->QuadtreeTULog2MaxSize;
        }
    }
    return log2MinTUSizeInCU;
}


void GetTrDepthMinMax(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s depth, Ipp32s partMode,
                      Ipp8u *trDepthMin, Ipp8u *trDepthMax)
{
    Ipp8u trDepth = (par->QuadtreeTUMaxDepthInter == 1) && (partMode != PART_SIZE_2Nx2N);
    Ipp32u tuLog2MinSize = GetQuadtreeTuLog2MinSizeInCu(par, absPartIdx, par->MaxCUSize >> depth, (Ipp8u)partMode, MODE_INTER);
    while ((par->MaxCUSize >> (depth + trDepth)) > 32) trDepth++;
    while (trDepth < (par->QuadtreeTUMaxDepthInter - 1 + (par->QuadtreeTUMaxDepthInter == 1)) &&
        (par->MaxCUSize >> (depth + trDepth)) > 4 &&
        (par->Log2MaxCUSize - depth - trDepth > tuLog2MinSize) &&
        (0 || // biggest TU
        (par->Log2MaxCUSize - depth - trDepth > par->QuadtreeTULog2MaxSize)))
        trDepth++;
    *trDepthMin = trDepth;
    while (trDepth < (par->QuadtreeTUMaxDepthInter - 1 + (par->QuadtreeTUMaxDepthInter == 1)) &&
        (par->MaxCUSize >> (depth + trDepth)) > 4 &&
        (par->Log2MaxCUSize - depth - trDepth > tuLog2MinSize) &&
        (1 || // smallest TU
        (par->Log2MaxCUSize - depth - trDepth > par->QuadtreeTULog2MaxSize)))
        trDepth++;
    *trDepthMax = trDepth;
}


void H265CU::ConvertTransIdx( Ipp32u /*absPartIdx*/, Ipp32u trIdx, Ipp32u& rlumaTrMode, Ipp32u& rchromaTrMode )
{
    rlumaTrMode   = trIdx;
    rchromaTrMode = trIdx;
    return;
}

Ipp32u H265CU::GetSCuAddr()
{
    return m_ctbAddr*(1<<(m_par->MaxCUDepth<<1))+m_absIdxInLcu;
}

Ipp32s H265CU::GetIntradirLumaPred(Ipp32u absPartIdx, Ipp8u* intraDirPred)
{
    H265CUPtr tempCU;
    Ipp8u leftIntraDir, aboveIntraDir;
    Ipp32s iModes = 3; //for encoder side only //kolya

    // Get intra direction of left PU
    GetPuLeft(&tempCU, m_absIdxInLcu + absPartIdx );

    leftIntraDir  = tempCU.ctbData ? ( IS_INTRA(tempCU.ctbData, tempCU.absPartIdx ) ? tempCU.ctbData[tempCU.absPartIdx].intraLumaDir : INTRA_DC ) : INTRA_DC;

    // Get intra direction of above PU
    GetPuAbove(&tempCU, m_absIdxInLcu + absPartIdx, true, true, false, true );

    aboveIntraDir = tempCU.ctbData ? ( IS_INTRA(tempCU.ctbData, tempCU.absPartIdx ) ? tempCU.ctbData[tempCU.absPartIdx].intraLumaDir : INTRA_DC ) : INTRA_DC;

    if (leftIntraDir == aboveIntraDir)
    {
        iModes = 1;

        if (leftIntraDir > 1) // angular modes
        {
            intraDirPred[0] = leftIntraDir;
            intraDirPred[1] = ((leftIntraDir + 29) % 32) + 2;
            intraDirPred[2] = ((leftIntraDir - 1 ) % 32) + 2;
        }
        else //non-angular
        {
            intraDirPred[0] = INTRA_PLANAR;
            intraDirPred[1] = INTRA_DC;
            intraDirPred[2] = INTRA_VER;
        }
    }
    else
    {
        iModes = 2;

        intraDirPred[0] = leftIntraDir;
        intraDirPred[1] = aboveIntraDir;

        if (leftIntraDir && aboveIntraDir ) //both modes are non-planar
        {
            intraDirPred[2] = INTRA_PLANAR;
        }
        else
        {
            intraDirPred[2] =  (leftIntraDir+aboveIntraDir)<2? INTRA_VER : INTRA_DC;
        }
    }

    return iModes;
}


void H265CU::GetAllowedChromaDir(Ipp32u absPartIdx, Ipp8u* modeList)
{
    modeList[0] = INTRA_PLANAR;
    modeList[1] = INTRA_VER;
    modeList[2] = INTRA_HOR;
    modeList[3] = INTRA_DC;
    modeList[4] = INTRA_DM_CHROMA;

    Ipp8u lumaMode = m_data[absPartIdx].intraLumaDir;

    for (Ipp32s i = 0; i < NUM_CHROMA_MODE - 1; i++)
    {
        if (lumaMode == modeList[i])
        {
            modeList[i] = 34; // VER+8 mode
            break;
        }
    }
}

static inline Ipp8u isZeroRow(Ipp32s addr, Ipp32s numUnitsPerRow)
{
// addr / numUnitsPerRow == 0
    return ( addr &~ ( numUnitsPerRow - 1 ) ) == 0;
}

static inline Ipp8u isEqualRow(Ipp32s addrA, Ipp32s addrB, Ipp32s numUnitsPerRow)
{
// addrA / numUnitsPerRow == addrB / numUnitsPerRow
    return (( addrA ^ addrB ) &~ ( numUnitsPerRow - 1 ) ) == 0;
}

static inline Ipp8u isZeroCol(Ipp32s addr, Ipp32s numUnitsPerRow)
{
// addr % numUnitsPerRow == 0
    return ( addr & ( numUnitsPerRow - 1 ) ) == 0;
}

static inline Ipp8u isEqualCol(Ipp32s addrA, Ipp32s addrB, Ipp32s numUnitsPerRow)
{
// addrA % numUnitsPerRow == addrB % numUnitsPerRow
    return (( addrA ^ addrB ) &  ( numUnitsPerRow - 1 ) ) == 0;
}

void H265CU::GetPuAbove(H265CUPtr *cu,
                        Ipp32u currPartUnitIdx,
                        Ipp32s enforceSliceRestriction,
                        Ipp32s /*bEnforceDependentSliceRestriction*/,
                        Ipp32s /*MotionDataCompresssion*/,
                        Ipp32s planarAtLcuBoundary,
                        Ipp32s /*bEnforceTileRestriction*/ )
{
    Ipp32u absPartIdx       = h265_scan_z2r[m_par->MaxCUDepth][currPartUnitIdx];
    Ipp32u absZorderCuIdx   = h265_scan_z2r[m_par->MaxCUDepth][m_absIdxInLcu];
    Ipp32u numPartInCuWidth = m_par->NumPartInCUSize;

    if ( !isZeroRow( absPartIdx, numPartInCuWidth ) )
    {
        cu->absPartIdx = h265_scan_r2z[m_par->MaxCUDepth][ absPartIdx - numPartInCuWidth ];
        cu->ctbData = m_data;
        cu->ctbAddr = m_ctbAddr;
        if ( !isEqualRow( absPartIdx, absZorderCuIdx, numPartInCuWidth ) )
        {
            cu->absPartIdx -= m_absIdxInLcu;
        }
        return;
    }

    if(planarAtLcuBoundary)
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->absPartIdx = h265_scan_r2z[m_par->MaxCUDepth][ absPartIdx + m_par->NumPartInCU - numPartInCuWidth ];

    if (enforceSliceRestriction && (m_above == NULL || m_aboveAddr < m_cslice->slice_segment_address))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }
    cu->ctbData = m_above;
    cu->ctbAddr = m_aboveAddr;
}

void H265CU::GetPuLeft(H265CUPtr *cu,
                       Ipp32u currPartUnitIdx,
                       Ipp32s enforceSliceRestriction,
                       Ipp32s /*bEnforceDependentSliceRestriction*/,
                       Ipp32s /*bEnforceTileRestriction*/ )
{
    Ipp32u absPartIdx       = h265_scan_z2r[m_par->MaxCUDepth][currPartUnitIdx];
    Ipp32u absZorderCUIdx   = h265_scan_z2r[m_par->MaxCUDepth][m_absIdxInLcu];
    Ipp32u numPartInCuWidth = m_par->NumPartInCUSize;

    if ( !isZeroCol( absPartIdx, numPartInCuWidth ) )
    {
        cu->absPartIdx = h265_scan_r2z[m_par->MaxCUDepth][ absPartIdx - 1 ];
        cu->ctbData = m_data;
        cu->ctbAddr = m_ctbAddr;
        if ( !isEqualCol( absPartIdx, absZorderCUIdx, numPartInCuWidth ) )
        {
            cu->absPartIdx -= m_absIdxInLcu;
        }
        return;
    }

    cu->absPartIdx = h265_scan_r2z[m_par->MaxCUDepth][ absPartIdx + numPartInCuWidth - 1 ];

    if (enforceSliceRestriction && (m_left == NULL || m_leftAddr < m_cslice->slice_segment_address))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->ctbData = m_left;
    cu->ctbAddr = m_leftAddr;
}


H265CUData* H265CU::GetQpMinCuLeft( Ipp32u& uiLPartUnitIdx, Ipp32u uiCurrAbsIdxInLCU, bool bEnforceSliceRestriction, bool bEnforceDependentSliceRestriction)
{
    Ipp32u unumPartInCUWidth = m_par->NumPartInCUSize;
    Ipp32u shift = ((m_par->MaxCUDepth - m_par->MaxCuDQPDepth) << 1);
    Ipp32u absZorderQpMinCUIdx = (uiCurrAbsIdxInLCU >> shift) << shift;
    Ipp32u absRorderQpMinCUIdx = h265_scan_z2r[m_par->MaxCUDepth][absZorderQpMinCUIdx];

    // check for left LCU boundary
    if ( isZeroCol(absRorderQpMinCUIdx, unumPartInCUWidth) )
    {
        return NULL;
    }

    // get index of left-CU relative to top-left corner of current quantization group
    uiLPartUnitIdx = h265_scan_r2z[m_par->MaxCUDepth][absRorderQpMinCUIdx - 1];

    // return pointer to current LCU
    return m_data;

    // not implemented
    //return NULL;
}

H265CUData* H265CU::GetQpMinCuAbove( Ipp32u& aPartUnitIdx, Ipp32u currAbsIdxInLCU, bool enforceSliceRestriction, bool enforceDependentSliceRestriction )
{
    // not implemented
    //return NULL;

    Ipp32u unumPartInCUWidth = m_par->NumPartInCUSize;
    Ipp32u shift = ((m_par->MaxCUDepth - m_par->MaxCuDQPDepth) << 1);
    Ipp32u absZorderQpMinCUIdx = (currAbsIdxInLCU >> shift) << shift;
    Ipp32u absRorderQpMinCUIdx = h265_scan_z2r[m_par->MaxCUDepth][absZorderQpMinCUIdx];

    // check for top LCU boundary
    if ( isZeroRow( absRorderQpMinCUIdx, unumPartInCUWidth) )
    {
        return NULL;
    }

    // get index of top-CU relative to top-left corner of current quantization group
    aPartUnitIdx = h265_scan_r2z[m_par->MaxCUDepth][absRorderQpMinCUIdx - unumPartInCUWidth];

    return m_data;
}

//-----------------------------------------------------------------------------
// aya: temporal solution to provide GetLastValidPartIdx() and GetLastCodedQP()
//-----------------------------------------------------------------------------
class H265CUFake
{
public:
    H265CUFake(
        Ipp32u ctbAddr,
        H265VideoParam*  par,
        H265CUData* data,
        H265CUData* ctbData,
        Ipp32u absIdxInLcu)
    :m_ctbAddr(ctbAddr)
    ,m_par(par)
    ,m_data(data)
    ,m_ctbData(ctbData)
    ,m_absIdxInLcu(absIdxInLcu)
    {};

    ~H265CUFake(){};

    Ipp32u m_ctbAddr;
    H265VideoParam*  m_par;
    H265CUData* m_data;
    H265CUData* m_ctbData;
    Ipp32u m_absIdxInLcu;

private:
    // copy, etc
};
//---------------------------------------------------------

template <class H265CuBase>
Ipp32s GetLastValidPartIdx(H265CuBase* cu, Ipp32s iAbsPartIdx)
{
    Ipp32s iLastValidPartIdx = iAbsPartIdx - 1;

    while (iLastValidPartIdx >= 0 && cu->m_data[iLastValidPartIdx].predMode == MODE_NONE)
    {
        int depth = cu->m_data[iLastValidPartIdx].depth;
        //iLastValidPartIdx--;
        iLastValidPartIdx -= cu->m_par->NumPartInCU >> (depth << 1);
    }

    return iLastValidPartIdx;
}


template <class H265CuBase>
Ipp8s GetLastCodedQP(H265CuBase* cu, Ipp32u absPartIdx )
{
    Ipp32u uiQUPartIdxMask = ~((1 << ((cu->m_par->MaxCUDepth - cu->m_par->MaxCuDQPDepth) << 1)) - 1);
    Ipp32s iLastValidPartIdx = GetLastValidPartIdx(cu, absPartIdx & uiQUPartIdxMask);

    if (iLastValidPartIdx >= 0)
    {
        return cu->m_data[iLastValidPartIdx].qp;
    }
    else  /* assume TilesOrEntropyCodingSyncIdc == 0 */
    {
        if( cu->m_absIdxInLcu > 0 )
        {
            //H265CU tmpCtb;
            //memset(&tmpCtb, 0, sizeof(H265CU));
            //tmpCtb.m_ctbAddr = m_ctbAddr;
            //tmpCtb.m_par = m_par;
            //tmpCtb.m_data = m_ctbData + (m_ctbAddr << m_par->Log2NumPartInCU);
            ////return GetLastCodedQP(m_absIdxInLcu );
            //return tmpCtb.GetLastCodedQP(m_absIdxInLcu);

            VM_ASSERT(!"NOT IMPLEMENTED");
            return 0;
        } 
        else if( cu->m_ctbAddr > 0  && !(cu->m_par->cpps->entropy_coding_sync_enabled_flag && 
            cu->m_ctbAddr % cu->m_par->PicWidthInCtbs == 0))
        {
            int curAddr = cu->m_ctbAddr;
            //aya: quick patcj for multi-slice mode
            if(cu->m_par->m_slice_ids[curAddr] == cu->m_par->m_slice_ids[curAddr-1] )
            {
               H265CUFake tmpCtb(
                    cu->m_ctbAddr-1, 
                    cu->m_par,
                    cu->m_ctbData + ((cu->m_ctbAddr-1) << cu->m_par->Log2NumPartInCU),
                    cu->m_ctbData,
                    cu->m_absIdxInLcu);

                Ipp8s qp = GetLastCodedQP(&tmpCtb, cu->m_par->NumPartInCU);
                return qp;
            }
            else
            {
                return cu->m_par->m_sliceQpY;
            }
        }
        else
        {
            return cu->m_par->m_sliceQpY;
        }
    }

} // Ipp8s H265CU::GetLastCodedQP( Ipp32u absPartIdx )


void H265CU::UpdateCuQp(void)
{
    int depth = 0;
    if( (m_par->MaxCUSize >> depth) == m_par->MinCuDQPSize && m_par->UseDQP )
    {
        //printf("\n updateDQP \n");fflush(stderr);

        bool hasResidual = false;
        for (Ipp32u blkIdx = 0; blkIdx < m_numPartition; blkIdx++)
        {
            if(m_data[blkIdx].cbf[0] || m_data[blkIdx].cbf[1] || m_data[blkIdx].cbf[2])
            {
                hasResidual = true;
                break;
            }
        }

        Ipp32u targetPartIdx;
        targetPartIdx = 0;
        if (hasResidual)
        {
            bool foundNonZeroCbf = false;
            SetQpSubCUs(GetRefQp(targetPartIdx), 0, depth, foundNonZeroCbf);
            VM_ASSERT(foundNonZeroCbf);
        }
        else
        {
            SetQpSubParts(GetRefQp(targetPartIdx), 0, depth); // set QP to default QP
        }
    }
}

//---------------------------------------------------------
void H265CU::InitCu(H265VideoParam *_par, H265CUData *_data, H265CUData *_dataTemp, Ipp32s cuAddr,
                    PixType *_y, PixType *_uv, Ipp32s _pitch, H265Frame *currFrame, H265BsFake *_bsf,
                    H265Slice *_cslice, Ipp32s initializeDataFlag, const Ipp8u *logMvCostTable) 
{
    m_par = _par;

    m_cslice = _cslice;
    m_costStat = m_par->m_costStat;
    m_bsf = _bsf;
    m_logMvCostTable = logMvCostTable + (1 << 15);
    m_dataSave = _data;
    m_data = _data;
    m_dataBest = _dataTemp;
    m_dataTemp = _dataTemp + (MAX_TOTAL_DEPTH << m_par->Log2NumPartInCU);
    m_dataTemp2 = _dataTemp + ((MAX_TOTAL_DEPTH*2) << m_par->Log2NumPartInCU);
    m_dataInter = _dataTemp + ((MAX_TOTAL_DEPTH*2 + 1) << m_par->Log2NumPartInCU);
    m_ctbAddr           = cuAddr;
    m_ctbPelX           = ( cuAddr % m_par->PicWidthInCtbs ) * m_par->MaxCUSize;
    m_ctbPelY           = ( cuAddr / m_par->PicWidthInCtbs ) * m_par->MaxCUSize;
    m_absIdxInLcu      = 0;
    m_numPartition     = m_par->NumPartInCU;
    m_pitchRec = _pitch;
    m_currFrame = currFrame;
    m_pitchSrc = currFrame->pitch_luma;
    m_yRec = _y + m_ctbPelX + m_ctbPelY * m_pitchRec;
    m_uvRec = _uv + m_ctbPelX + (m_ctbPelY * m_pitchRec >> 1);

    m_ySrc = currFrame->y + m_ctbPelX + m_ctbPelY * m_pitchSrc;

    // limits to clip MV allover the CU
    const Ipp32s MvShift = 2;
    const Ipp32s offset = 8;
    HorMax = (m_par->Width + offset - m_ctbPelX - 1) << MvShift;
    HorMin = ( - (Ipp32s) m_par->MaxCUSize - offset - (Ipp32s) m_ctbPelX + 1) << MvShift;
    VerMax = (m_par->Height + offset - m_ctbPelY - 1) << MvShift;
    VerMin = ( - (Ipp32s) m_par->MaxCUSize - offset - (Ipp32s) m_ctbPelY + 1) << MvShift;


    // aya: may be used to provide performance gain for SAD calculation
    /*{
    IppiSize blkSize = {m_par->MaxCUSize, m_par->MaxCUSize};
    ippiCopy_8u_C1R(_m_ySrc + m_ctbPelX + m_ctbPelY * m_pitchSrc, m_pitchSrc, m_src_aligned_block, MAX_CU_SIZE, blkSize);
    }*/

    m_uvSrc = currFrame->uv + m_ctbPelX + (m_ctbPelY * m_pitchSrc >> 1);
    m_depthMin = MAX_TOTAL_DEPTH;
    m_depthMinCollocated = MAX_TOTAL_DEPTH;

    if (initializeDataFlag) {
        m_rdOptFlag = m_cslice->rd_opt_flag;
        m_rdLambda = m_cslice->rd_lambda;

        //kolya
        m_rdLambdaSqrt = m_cslice->rd_lambda_sqrt;
        m_ChromaDistWeight = m_cslice->ChromaDistWeight;

        m_rdLambdaInter = m_cslice->rd_lambda_inter;
        m_rdLambdaInterMv = m_cslice->rd_lambda_inter_mv;

        if ( m_numPartition > 0 )
        {
            memset (m_dataSave, 0, sizeof(H265CUData));
            m_dataSave->partSize = PART_SIZE_NONE;
            m_dataSave->predMode = MODE_NONE;
            m_dataSave->size = (Ipp8u)m_par->MaxCUSize;
            m_dataSave->mvpIdx[0] = -1;
            m_dataSave->mvpIdx[1] = -1;
            m_dataSave->mvpNum[0] = -1;
            m_dataSave->mvpNum[1] = -1;
            //m_data->qp = m_par->QP;
            m_data->qp = m_par->m_lcuQps[m_ctbAddr];
            m_dataSave->_flags = 0;
            m_dataSave->intraLumaDir = INTRA_DC;
            m_dataSave->intraChromaDir = INTRA_DM_CHROMA;
            m_dataSave->cbf[0] = m_dataSave->cbf[1] = m_dataSave->cbf[2] = 0;

            PropagateSubPart(m_dataSave, m_numPartition);
        }
        for (Ipp32u i = 0; i < 4; i++) {
            m_costStat[m_ctbAddr].cost[i] = m_costStat[m_ctbAddr].num[i] = 0;
        }
    }

    // Setting neighbor CU
    m_left        = NULL;
    m_above       = NULL;
    m_aboveLeft   = NULL;
    m_aboveRight  = NULL;
    m_leftAddr = -1;
    m_aboveAddr = -1;
    m_aboveLeftAddr = -1;
    m_aboveRightAddr = -1;

    Ipp32u widthInCU = m_par->PicWidthInCtbs;
    if ( m_ctbAddr % widthInCU )
    {
        m_left = m_dataSave - m_par->NumPartInCU;
        m_leftAddr = m_ctbAddr - 1;
    }

    if ( m_ctbAddr / widthInCU )
    {
        m_above = m_dataSave - (widthInCU << m_par->Log2NumPartInCU);
        m_aboveAddr = m_ctbAddr - widthInCU;
    }

    if ( m_left && m_above )
    {
        m_aboveLeft = m_dataSave - (widthInCU << m_par->Log2NumPartInCU) - m_par->NumPartInCU;
        m_aboveLeftAddr = m_ctbAddr - widthInCU - 1;
    }

    if ( m_above && ( (m_ctbAddr%widthInCU) < (widthInCU-1) )  )
    {
        m_aboveRight = m_dataSave - (widthInCU << m_par->Log2NumPartInCU) + m_par->NumPartInCU;
        m_aboveRightAddr = m_ctbAddr - widthInCU + 1;
    }

    m_bakAbsPartIdxCu = 0;
    m_bakAbsPartIdx = 0;
    m_bakChromaOffset = 0;
    m_isRdoq = m_par->RDOQFlag ? true : false;

    m_colocatedCu[0] = m_colocatedCu[1] = NULL;
    RefPicList *refPicList = m_currFrame->m_refPicList;
    H265Frame *ref0 = m_cslice->slice_type != I_SLICE ? refPicList[0].m_refFrames[0] : NULL;
    H265Frame *ref1 = m_cslice->slice_type == B_SLICE ? refPicList[1].m_refFrames[0] : NULL;
    if (ref0) m_colocatedCu[0] = ref0->cu_data + (m_ctbAddr << m_par->Log2NumPartInCU);
    if (ref1) m_colocatedCu[1] = ref1->cu_data + (m_ctbAddr << m_par->Log2NumPartInCU);    

    //aya:quick fix for MB BRC!!!    
    m_ctbData = _data - (m_ctbAddr << m_par->Log2NumPartInCU);

    m_interPredReady = false;
}

////// random generation code, for development purposes
//
//static double rand_val = 1.239847855923875843957;
//
//static unsigned int myrand()
//{
//    rand_val *= 1.204839573950674;
//    if (rand_val > 1024) rand_val /= 1024;
//    return ((Ipp32s *)&rand_val)[0];
//}
//
//
//static void genCBF(Ipp8u *cbfy, Ipp8u *cbfu, Ipp8u *cbfv, Ipp8u size, Ipp8u len, Ipp8u depth, Ipp8u max_depth) {
//    if (depth > max_depth)
//        return;
//
//    Ipp8u y = cbfy[0], u = cbfu[0], v = cbfv[0];
//
//    if (depth == 0 || size > 4) {
//        if (depth == 0 || ((u >> (depth - 1)) & 1))
//            u |= ((myrand() & 1) << depth);
//        else if (depth > 0 && size == 4)
//            u |= (((u >> (depth - 1)) & 1) << depth);
//
//        if (depth == 0 || ((v >> (depth - 1)) & 1))
//            v |= ((myrand() & 1) << depth);
//        else if (depth > 0 && size == 4)
//            v |= (((v >> (depth - 1)) & 1) << depth);
//    } else {
//            u |= (((u >> (depth - 1)) & 1) << depth);
//            v |= (((v >> (depth - 1)) & 1) << depth);
//    }
//    if (depth > 0 || ((u >> depth) & 1) || ((v >> depth) & 1))
//        y |= ((myrand() & 1) << depth);
//    else
//        y |= (1 << depth);
//
//    memset(cbfy, y, len);
//    memset(cbfu, u, len);
//    memset(cbfv, v, len);
//
//    for (Ipp32u i = 0; i < 4; i++)
//        genCBF(cbfy + len/4 * i,cbfu + len/4 * i,cbfv + len/4 * i,
//        size >> 1, len/4, depth + 1, max_depth);
//}
//
//void H265CU::FillRandom(Ipp32u absPartIdx, Ipp8u depth)
//{
//    Ipp32u num_parts = ( m_par->NumPartInCU >> (depth<<1) );
//    Ipp32u i;
//  Ipp32u lpel_x   = m_ctbPelX +
//      ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
//  Ipp32u rpel_x   = lpel_x + (m_par->MaxCUSize>>depth)  - 1;
//  Ipp32u tpel_y   = m_ctbPelY +
//      ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
//  Ipp32u bpel_y   = tpel_y + (m_par->MaxCUSize>>depth) - 1;
///*
//  if (depth == 0) {
//      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE; i++)
//          m_trCoeffY[i] = (Ipp16s)((myrand() & 31) - 16);
//      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE/4; i++) {
//          m_trCoeffU[i] = (Ipp16s)((myrand() & 31) - 16);
//          m_trCoeffV[i] = (Ipp16s)((myrand() & 31) - 16);
//      }
//  }
//*/
//    if ((depth < m_par->MaxCUDepth - m_par->AddCUDepth) &&
//        (((myrand() & 1) && (m_par->Log2MaxCUSize - depth > m_par->QuadtreeTULog2MinSize)) ||
//        rpel_x >= m_par->Width  || bpel_y >= m_par->Height ||
//        m_par->Log2MaxCUSize - depth -
//            (m_cslice->slice_type == I_SLICE ? m_par->QuadtreeTUMaxDepthIntra :
//            MIN(m_par->QuadtreeTUMaxDepthIntra, m_par->QuadtreeTUMaxDepthInter)) > m_par->QuadtreeTULog2MaxSize)) {
//        // split
//        for (i = 0; i < 4; i++)
//            FillRandom(absPartIdx + (num_parts >> 2) * i, depth+1);
//    } else {
//        Ipp8u pred_mode = MODE_INTRA;
//        if ( m_cslice->slice_type != I_SLICE && (myrand() & 15)) {
//            pred_mode = MODE_INTER;
//        }
//        Ipp8u size = (Ipp8u)(m_par->MaxCUSize >> depth);
//        Ipp32u MaxDepth = pred_mode == MODE_INTRA ? m_par->QuadtreeTUMaxDepthIntra : m_par->QuadtreeTUMaxDepthInter;
//        Ipp8u part_size;
//
//        if (pred_mode == MODE_INTRA) {
//            part_size = (Ipp8u)
//                (depth == m_par->MaxCUDepth - m_par->AddCUDepth &&
//                depth + 1 <= m_par->MaxCUDepth &&
//                ((m_par->Log2MaxCUSize - depth - MaxDepth == m_par->QuadtreeTULog2MaxSize) ||
//                (myrand() & 1)) ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
//        } else {
//            if (depth == m_par->MaxCUDepth - m_par->AddCUDepth) {
//                if (size == 8) {
//                    part_size = (Ipp8u)(myrand() % 3);
//                } else {
//                    part_size = (Ipp8u)(myrand() % 4);
//                }
//            } else {
//                if (m_par->csps->amp_enabled_flag) {
//                    part_size = (Ipp8u)(myrand() % 7);
//                    if (part_size == 3) part_size = 7;
//                } else {
//                    part_size = (Ipp8u)(myrand() % 3);
//                }
//            }
//        }
//        Ipp8u intra_split = ((pred_mode == MODE_INTRA && (part_size == PART_SIZE_NxN)) ? 1 : 0);
//        Ipp8u inter_split = ((MaxDepth == 1) && (pred_mode == MODE_INTER) && (part_size != PART_SIZE_2Nx2N) ) ? 1 : 0;
//
//        Ipp8u tr_depth = intra_split + inter_split;
//        while ((m_par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
//        while (tr_depth < (MaxDepth - 1 + intra_split + inter_split) &&
//            (m_par->MaxCUSize >> (depth + tr_depth)) > 4 &&
//            (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MinSize) &&
//            ((myrand() & 1) ||
//                (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MaxSize))) tr_depth++;
//
//        for (i = 0; i < num_parts; i++) {
//            m_data[absPartIdx + i].cbf[0] = 0;
//            m_data[absPartIdx + i].cbf[1] = 0;
//            m_data[absPartIdx + i].cbf[2] = 0;
//        }
//
//        if (tr_depth > m_par->QuadtreeTUMaxDepthIntra) {
//            printf("FillRandom err 1\n"); exit(1);
//        }
//        if (depth + tr_depth > m_par->MaxCUDepth) {
//            printf("FillRandom err 2\n"); exit(1);
//        }
//        if (m_par->Log2MaxCUSize - (depth + tr_depth) < m_par->QuadtreeTULog2MinSize) {
//            printf("FillRandom err 3\n"); exit(1);
//        }
//        if (m_par->Log2MaxCUSize - (depth + tr_depth) > m_par->QuadtreeTULog2MaxSize) {
//            printf("FillRandom err 4\n"); exit(1);
//        }
//        if (pred_mode == MODE_INTRA) {
//            Ipp8u luma_dir = (m_left && m_above) ? (Ipp8u) (myrand() % 35) : INTRA_DC;
//            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
//                m_data[i].predMode = pred_mode;
//                m_data[i].depth = depth;
//                m_data[i].size = size;
//                m_data[i].partSize = part_size;
//                m_data[i].intraLumaDir = luma_dir;
//                m_data[i].intraChromaDir = INTRA_DM_CHROMA;
//                m_data[i].qp = m_par->QP;
//                m_data[i].trIdx = tr_depth;
//                m_data[i].interDir = 0;
//            }
//        } else {
//            Ipp16s mvmax = (Ipp16s)(10 + myrand()%100);
//            Ipp16s mvx0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
//            Ipp16s mvy0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
//            Ipp16s mvx1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
//            Ipp16s mvy1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
//            Ipp8s ref_idx0 = (Ipp8s)(myrand() % m_cslice->num_ref_idx_l0_active);
//            Ipp8u inter_dir;
//            if (m_cslice->slice_type == B_SLICE && part_size != PART_SIZE_2Nx2N && size == 8) {
//                inter_dir = (Ipp8u)(1 + myrand()%2);
//            } else if (m_cslice->slice_type == B_SLICE) {
//                inter_dir = (Ipp8u)(1 + myrand()%3);
//            } else {
//                inter_dir = 1;
//            }
//            Ipp8s ref_idx1 = (Ipp8s)((inter_dir & INTER_DIR_PRED_L1) ? (myrand() % m_cslice->num_ref_idx_l1_active) : -1);
//            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
//                m_data[i].predMode = pred_mode;
//                m_data[i].depth = depth;
//                m_data[i].size = size;
//                m_data[i].partSize = part_size;
//                m_data[i].intraLumaDir = 0;//luma_dir;
//                m_data[i].qp = m_par->QP;
//                m_data[i].trIdx = tr_depth;
//                m_data[i].interDir = inter_dir;
//                m_data[i].refIdx[0] = -1;
//                m_data[i].refIdx[1] = -1;
//                m_data[i].mv[0].mvx = 0;
//                m_data[i].mv[0].mvy = 0;
//                m_data[i].mv[1].mvx = 0;
//                m_data[i].mv[1].mvy = 0;
//                if (inter_dir & INTER_DIR_PRED_L0) {
//                    m_data[i].refIdx[0] = ref_idx0;
//                    m_data[i].mv[0].mvx = mvx0;
//                    m_data[i].mv[0].mvy = mvy0;
//                }
//                if (inter_dir & INTER_DIR_PRED_L1) {
//                    m_data[i].refIdx[1] = ref_idx1;
//                    m_data[i].mv[1].mvx = mvx1;
//                    m_data[i].mv[1].mvy = mvy1;
//                }
//                m_data[i].flags.mergeFlag = 0;
//                m_data[i].mvd[0].mvx = m_data[i].mvd[0].mvy =
//                    m_data[i].mvd[1].mvx = m_data[i].mvd[1].mvy = 0;
//                m_data[i].mvpIdx[0] = m_data[i].mvpIdx[1] = 0;
//                m_data[i].mvpNum[0] = m_data[i].mvpNum[1] = 0;
//            }
//            Ipp32s numPu = h265_numPu[m_data[absPartIdx].partSize];
//            for (Ipp32s i = 0; i < numPu; i++)
//            {
//                Ipp32s PartAddr;
//                Ipp32s PartX, PartY, Width, Height;
//                GetPartOffsetAndSize(i, m_data[absPartIdx].partSize,
//                    m_data[absPartIdx].size, PartX, PartY, Width, Height);
//                GetPartAddr(i, m_data[absPartIdx].partSize, num_parts, PartAddr);
//
//                GetPuMvInfo(absPartIdx, PartAddr, m_data[absPartIdx].partSize, i);
//            }
//
//            if ((num_parts < 4 && part_size != PART_SIZE_2Nx2N) ||
//                (num_parts < 16 && part_size > 3)) {
//                printf("FillRandom err 5\n"); exit(1);
//            }
//        }
//    }
//}
//
//void H265CU::FillZero(Ipp32u absPartIdx, Ipp8u depth)
//{
//    Ipp32u num_parts = ( m_par->NumPartInCU >> (depth<<1) );
//    Ipp32u i;
//    Ipp32u lpel_x   = m_ctbPelX +
//        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
//    Ipp32u rpel_x   = lpel_x + (m_par->MaxCUSize>>depth)  - 1;
//    Ipp32u tpel_y   = m_ctbPelY +
//        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
//    Ipp32u bpel_y   = tpel_y + (m_par->MaxCUSize>>depth) - 1;
///*
//  if (depth == 0) {
//      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE; i++)
//          m_trCoeffY[i] = (Ipp16s)((myrand() & 31) - 16);
//      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE/4; i++) {
//          m_trCoeffU[i] = (Ipp16s)((myrand() & 31) - 16);
//          m_trCoeffV[i] = (Ipp16s)((myrand() & 31) - 16);
//      }
//  }
//*/
//    if ((depth < m_par->MaxCUDepth - m_par->AddCUDepth) &&
//        (((/*myrand() & 1*/0) && (m_par->Log2MaxCUSize - depth > m_par->QuadtreeTULog2MinSize)) ||
//        rpel_x >= m_par->Width  || bpel_y >= m_par->Height ||
//        m_par->Log2MaxCUSize - depth -
//            (m_cslice->slice_type == I_SLICE ? m_par->QuadtreeTUMaxDepthIntra :
//            MIN(m_par->QuadtreeTUMaxDepthIntra, m_par->QuadtreeTUMaxDepthInter)) > m_par->QuadtreeTULog2MaxSize)) {
//        // split
//        for (i = 0; i < 4; i++)
//            FillZero(absPartIdx + (num_parts >> 2) * i, depth+1);
//    } else {
//        Ipp8u pred_mode = MODE_INTRA;
//        if ( m_cslice->slice_type != I_SLICE && (myrand() & 15)) {
//            pred_mode = MODE_INTER;
//        }
//        Ipp8u size = (Ipp8u)(m_par->MaxCUSize >> depth);
//        Ipp32u MaxDepth = pred_mode == MODE_INTRA ? m_par->QuadtreeTUMaxDepthIntra : m_par->QuadtreeTUMaxDepthInter;
//        Ipp8u part_size;
//        /*
//        if (pred_mode == MODE_INTRA) {
//            part_size = (Ipp8u)
//                (depth == m_par->MaxCUDepth - m_par->AddCUDepth &&
//                depth + 1 <= m_par->MaxCUDepth &&
//                ((m_par->Log2MaxCUSize - depth - MaxDepth == m_par->QuadtreeTULog2MaxSize) ||
//                (myrand() & 1)) ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
//        } else {
//            if (depth == m_par->MaxCUDepth - m_par->AddCUDepth) {
//                if (size == 8) {
//                    part_size = (Ipp8u)(myrand() % 3);
//                } else {
//                    part_size = (Ipp8u)(myrand() % 4);
//                }
//            } else {
//                if (m_par->csps->amp_enabled_flag) {
//                    part_size = (Ipp8u)(myrand() % 7);
//                    if (part_size == 3) part_size = 7;
//                } else {
//                    part_size = (Ipp8u)(myrand() % 3);
//                }
//            }
//        }*/
//        part_size = PART_SIZE_2Nx2N;
//
//        Ipp8u intra_split = ((pred_mode == MODE_INTRA && (part_size == PART_SIZE_NxN)) ? 1 : 0);
//        Ipp8u inter_split = ((MaxDepth == 1) && (pred_mode == MODE_INTER) && (part_size != PART_SIZE_2Nx2N) ) ? 1 : 0;
//
//        Ipp8u tr_depth = intra_split + inter_split;
//        while ((m_par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
//        while (tr_depth < (MaxDepth - 1 + intra_split + inter_split) &&
//            (m_par->MaxCUSize >> (depth + tr_depth)) > 4 &&
//            (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MinSize) &&
//            ((0/*myrand() & 1*/) ||
//                (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MaxSize))) tr_depth++;
//
//        for (i = 0; i < num_parts; i++) {
//            m_data[absPartIdx + i].cbf[0] = 0;
//            m_data[absPartIdx + i].cbf[1] = 0;
//            m_data[absPartIdx + i].cbf[2] = 0;
//        }
//
//        if (tr_depth > m_par->QuadtreeTUMaxDepthIntra) {
//            printf("FillZero err 1\n"); exit(1);
//        }
//        if (depth + tr_depth > m_par->MaxCUDepth) {
//            printf("FillZero err 2\n"); exit(1);
//        }
//        if (m_par->Log2MaxCUSize - (depth + tr_depth) < m_par->QuadtreeTULog2MinSize) {
//            printf("FillZero err 3\n"); exit(1);
//        }
//        if (m_par->Log2MaxCUSize - (depth + tr_depth) > m_par->QuadtreeTULog2MaxSize) {
//            printf("FillZero err 4\n"); exit(1);
//        }
//        if (pred_mode == MODE_INTRA) {
//            Ipp8u luma_dir =/* (p_left && p_above) ? (Ipp8u) (myrand() % 35) : */INTRA_DC;
//            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
//                m_data[i].predMode = pred_mode;
//                m_data[i].depth = depth;
//                m_data[i].size = size;
//                m_data[i].partSize = part_size;
//                m_data[i].intraLumaDir = luma_dir;
//                m_data[i].intraChromaDir = INTRA_DM_CHROMA;
//                m_data[i].qp = m_par->QP;
//                m_data[i].trIdx = tr_depth;
//                m_data[i].interDir = 0;
//            }
//        } else {
///*            Ipp16s mvmax = (Ipp16s)(10 + myrand()%100);
//            Ipp16s mvx0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
//            Ipp16s mvy0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
//            Ipp16s mvx1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
//            Ipp16s mvy1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);*/
//            Ipp8s ref_idx0 = 0;//(Ipp8s)(myrand() % m_cslice->num_ref_idx_l0_active);
//            Ipp8u inter_dir;
///*            if (m_cslice->slice_type == B_SLICE && part_size != PART_SIZE_2Nx2N && size == 8) {
//                inter_dir = (Ipp8u)(1 + myrand()%2);
//            } else if (m_cslice->slice_type == B_SLICE) {
//                inter_dir = (Ipp8u)(1 + myrand()%3);
//            } else*/ {
//                inter_dir = 1;
//            }
//            Ipp8s ref_idx1 = (Ipp8s)((inter_dir & INTER_DIR_PRED_L1) ? (myrand() % m_cslice->num_ref_idx_l1_active) : -1);
//            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
//                m_data[i].predMode = pred_mode;
//                m_data[i].depth = depth;
//                m_data[i].size = size;
//                m_data[i].partSize = part_size;
//                m_data[i].intraLumaDir = 0;//luma_dir;
//                m_data[i].qp = m_par->QP;
//                m_data[i].trIdx = tr_depth;
//                m_data[i].interDir = inter_dir;
//                m_data[i].refIdx[0] = -1;
//                m_data[i].refIdx[1] = -1;
//                m_data[i].mv[0].mvx = 0;
//                m_data[i].mv[0].mvy = 0;
//                m_data[i].mv[1].mvx = 0;
//                m_data[i].mv[1].mvy = 0;
//                if (inter_dir & INTER_DIR_PRED_L0) {
//                    m_data[i].refIdx[0] = ref_idx0;
////                    m_data[i].mv[0].mvx = mvx0;
////                    m_data[i].mv[0].mvy = mvy0;
//                }
//                if (inter_dir & INTER_DIR_PRED_L1) {
//                    m_data[i].refIdx[1] = ref_idx1;
////                    m_data[i].mv[1].mvx = mvx1;
////                    m_data[i].mv[1].mvy = mvy1;
//                }
//                m_data[i]._flags = 0;
//                m_data[i].flags.mergeFlag = 0;
//                m_data[i].mvd[0].mvx = m_data[i].mvd[0].mvy =
//                    m_data[i].mvd[1].mvx = m_data[i].mvd[1].mvy = 0;
//                m_data[i].mvpIdx[0] = m_data[i].mvpIdx[1] = 0;
//                m_data[i].mvpNum[0] = m_data[i].mvpNum[1] = 0;
//            }
//            Ipp32s numPu = h265_numPu[m_data[absPartIdx].partSize];
//            for (Ipp32s i = 0; i < numPu(absPartIdx); i++)
//            {
//                Ipp32s PartAddr;
//                Ipp32s PartX, PartY, Width, Height;
//                GetPartOffsetAndSize(i, m_data[absPartIdx].partSize,
//                    m_data[absPartIdx].size, PartX, PartY, Width, Height);
//                GetPartAddr(i, m_data[absPartIdx].partSize, num_parts, PartAddr);
//
//                GetPuMvInfo(absPartIdx, PartAddr, m_data[absPartIdx].partSize, i);
//            }
//
//            if ((num_parts < 4 && part_size != PART_SIZE_2Nx2N) ||
//                (num_parts < 16 && part_size > 3)) {
//                printf("FillZero err 5\n"); exit(1);
//            }
//        }
//    }
//}

void H265CU::FillSubPart(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth,
                         Ipp8u partSize, Ipp8u lumaDir, Ipp8u qp)
{
    Ipp8u size = (Ipp8u)(m_par->MaxCUSize >> depthCu);
    Ipp32s depth = depthCu + trDepth;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    Ipp32u i;

// uncomment to hide false uninitialized memory read warning
//    memset(data + absPartIdx, 0, num_parts * sizeof(m_data[0]));
    for (i = absPartIdx; i < absPartIdx + numParts; i++) {
        m_data[i].depth = depthCu;
        m_data[i].size = size;
        m_data[i].partSize = partSize;
        m_data[i].predMode = MODE_INTRA;
        m_data[i].qp = qp;

        m_data[i].trIdx = trDepth;
        m_data[i].intraLumaDir = lumaDir;
        m_data[i].intraChromaDir = INTRA_DM_CHROMA;
        m_data[i].cbf[0] = m_data[i].cbf[1] = m_data[i].cbf[2] = 0;
        m_data[i].flags.skippedFlag = 0;
    }
}

void H265CU::FillSubPartIntraLumaDir(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth, Ipp8u lumaDir)
{
    Ipp32u numParts = (m_par->NumPartInCU >> ((depthCu + trDepth) << 1));
    for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
        m_data[i].intraLumaDir = lumaDir;
    }
}

void H265CU::CopySubPartTo(H265CUData *dataCopy, Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth)
{
    Ipp32s depth = depthCu + trDepth;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    small_memcpy(dataCopy + absPartIdx, m_data + absPartIdx, numParts * sizeof(H265CUData));
}

void H265CU::ModeDecision(Ipp32u absPartIdx, Ipp32u offset, Ipp8u depth, CostType *cost)
{
    if( m_par->UseDQP )
    {
        if(0 == absPartIdx && 0 == depth)
        {
            setdQPFlag(true);
        }
    }

    CABAC_CONTEXT_H265 ctxSave[4][NUM_CABAC_CONTEXT];
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    Ipp32u lPelX   = m_ctbPelX +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
    Ipp32u rPelX   = lPelX + (m_par->MaxCUSize>>depth)  - 1;
    Ipp32u tPelY   = m_ctbPelY +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
    Ipp32u bPelY   = tPelY + (m_par->MaxCUSize>>depth) - 1;

    CostType costBest = COST_MAX;
    CostType costInter = COST_MAX;

    Ipp32s subsize = m_par->MaxCUSize >> (depth + 1);
    subsize *= subsize;
    Ipp32s widthCu = m_par->MaxCUSize >> depth;
    IppiSize roiSizeCu = {widthCu, widthCu};
    Ipp32s offsetLumaCu = GetLumaOffset(m_par, absPartIdx, m_pitchRec);
    IppiSize roiSizeCuChr = {widthCu, widthCu>>1};
    Ipp32s offsetChromaCu = GetChromaOffset(m_par, absPartIdx, m_pitchRec);
    Ipp32s offsetLumaBuf = GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
    Ipp32s offsetChromaBuf = GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE);

    Ipp8u splitMode = SPLIT_NONE;

    if (depth < m_par->MaxCUDepth - m_par->AddCUDepth) {
        if (rPelX >= m_par->Width  || bPelY >= m_par->Height ||
            m_par->Log2MaxCUSize - depth - (m_par->QuadtreeTUMaxDepthIntra - 1) > m_par->QuadtreeTULog2MaxSize) {
                splitMode = SPLIT_MUST;
        } else if (m_par->Log2MaxCUSize - depth > m_par->QuadtreeTULog2MinSize) {
            splitMode = SPLIT_TRY;
        }
    }

    if (m_rdOptFlag) {
        m_bsf->CtxSave(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        m_bsf->CtxSave(ctxSave[2], 0, NUM_CABAC_CONTEXT);
    }

    if (m_par->minCUDepthAdapt && m_cslice->slice_type != I_SLICE) {
        H265CUData* col0 = m_colocatedCu[0];
        H265CUData* col1 = m_colocatedCu[1];
        //Ipp8s qpCur = m_par->QP;
        Ipp8s qpCur = m_par->m_lcuQps[m_ctbAddr];
        Ipp8s qpPrev = col0 ? col0[0].qp : (col1 ? col1[0].qp : qpCur);
        Ipp8u depthDelta = 0, depthMin0 = 4, depthMin1 = 4;
        if (depth == 0)
        {
            Ipp64f depthSum0 = 0, depthSum1 = 0, depthAvg0 = 0, depthAvg1 = 0, depthAvg = 0;
            for (Ipp32u i = 0; i < m_numPartition; i += 4)
            {
                if (col0 && col0[i].depth < depthMin0)
                    depthMin0 = col0[i].depth;
                if (col1 && col1[i].depth < depthMin1)
                    depthMin1 = col1[i].depth;
                if (col0)
                    depthSum0 += (col0[i].depth * 4);
                if (col1)
                    depthSum1 += (col1[i].depth * 4);
            }

            depthAvg0 = depthSum0 / m_numPartition;
            depthAvg1 = depthSum1 / m_numPartition;
            depthAvg = (depthAvg0 + depthAvg1) / 2;

            m_depthMinCollocated = MIN(depthMin0, depthMin1);
            if (((qpCur - qpPrev) < 0) || (((qpCur - qpPrev) >= 0) && ((depthAvg - m_depthMinCollocated) > 0.5)))
                depthDelta = 0;
            else
                depthDelta = 1;
            if (m_depthMinCollocated > 0)
                m_depthMinCollocated -= depthDelta;
        }
        if (splitMode == SPLIT_TRY && depth < m_depthMinCollocated)
            splitMode = SPLIT_MUST;
    }

    if (splitMode != SPLIT_MUST) {
        if (m_depthMin == MAX_TOTAL_DEPTH)
            m_depthMin = depth; // lowest depth for branch where not SPLIT_MUST

        //m_data = m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU);

        // Inter mode
        m_data = m_dataSave;
        if (m_cslice->slice_type != I_SLICE) {

            costInter = MeCu(absPartIdx, depth, offset);

            small_memcpy(m_dataBest + (depth << m_par->Log2NumPartInCU) + absPartIdx, m_dataSave + absPartIdx, numParts * sizeof(H265CUData));
            small_memcpy(m_dataInter, m_dataSave + absPartIdx, numParts * sizeof(H265CUData));
            if (!m_data[absPartIdx].cbf[0] && !m_data[absPartIdx].cbf[1] && !m_data[absPartIdx].cbf[2] &&
                (m_par->fastCbfMode || m_par->fastSkip && m_data[absPartIdx].flags.skippedFlag) ) {
                if (cost)
                    *cost = costInter;
                if (depth == 0) {
                    //small_memcpy(m_dataSave, m_dataBest, sizeof(H265CUData) << m_par->Log2NumPartInCU);
                    m_data = m_dataSave;
                    for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
                        m_data[i].cbf[0] = m_data[i].cbf[1] = m_data[i].cbf[2] = 0;
                    }
                }
                ippiCopy_8u_C1R(m_interRec[m_data[absPartIdx].curRecIdx][ depth ] + offsetLumaBuf, MAX_CU_SIZE, m_yRec + offsetLumaCu, m_pitchRec, roiSizeCu);
                if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
                    ippiCopy_8u_C1R(m_interRecChroma[m_data[absPartIdx].curRecIdx][ depth ] + offsetChromaBuf, MAX_CU_SIZE, m_uvRec + offsetChromaCu, m_pitchRec, roiSizeCuChr);
                return;
            }

            { // TODO avoid what possible
                //small_memcpy(m_dataSave + absPartIdx, m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU) + absPartIdx, numParts * sizeof(H265CUData));
                //ippiCopy_8u_C1R(m_yRec + offsetLumaCu, m_pitchRec, m_recLumaSaveCu[depth], widthCu, roiSizeCu);
                //if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
                //    ippiCopy_8u_C1R(m_uvRec + offsetChromaCu, m_pitchRec, m_recChromaSaveCu[depth], widthCu, roiSizeCuChr);
                if (m_rdOptFlag)
                    m_bsf->CtxSave(ctxSave[3], 0, NUM_CABAC_CONTEXT);
            }
        }

        bool tryIntra = true;

#if defined (MFX_ENABLE_CM)
        tryIntra = CheckGpuIntraCost(lPelX, tPelY, depth);
#endif // MFX_ENABLE_CM

        if (tryIntra) {
            // Try Intra mode
            Ipp32s intraSplit = (depth == m_par->MaxCUDepth - m_par->AddCUDepth &&
                depth + 1 <= m_par->MaxCUDepth) ? 1 : 0;
            for (Ipp8u trDepth = 0; trDepth < intraSplit + 1; trDepth++) {
                Ipp8u partSize = (Ipp8u)(trDepth == 1 ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
                CostType costBestPu[4] = { COST_MAX, COST_MAX, COST_MAX, COST_MAX };
                CostType costBestPuSum = 0;

                //if (m_rdOptFlag && trDepth)
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);

                for (Ipp32s i = 0; i < ((trDepth == 1) ? 4 : 1); i++) {
                    Ipp32s absPartIdxTu = absPartIdx + (numParts >> 2) * i;
                    Ipp32u lPelXTu = m_ctbPelX +
                        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdxTu] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
                    Ipp32u tPelYTu = m_ctbPelY +
                        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdxTu] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);

                    if (lPelXTu >= m_par->Width || tPelYTu >= m_par->Height) {
                        m_data = m_dataBest + ((depth + trDepth) << m_par->Log2NumPartInCU);
                        FillSubPart(absPartIdxTu, depth, trDepth, partSize, 0, m_par->m_lcuQps[m_ctbAddr]);
                        CopySubPartTo(m_dataSave, absPartIdxTu, depth, trDepth);
                        continue;
                    }

                    if (m_rdOptFlag)
                        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);

                    IntraLumaModeDecision(absPartIdxTu, offset + subsize * i, depth, trDepth);

                    IntraLumaModeDecisionRDO(absPartIdxTu, offset + subsize * i, depth, trDepth);
                    costBestPu[i] = m_intraCosts[0];

                    m_data = m_dataBest + ((depth + trDepth) << m_par->Log2NumPartInCU);
                    CopySubPartTo(m_dataSave, absPartIdxTu, depth, trDepth);

                    costBestPuSum += costBestPu[i];
                }

                // add cost of split cu and pred mode to costBestPuSum
                if (m_rdOptFlag) {
                    m_bsf->Reset();
                    m_data = m_dataSave;
                    EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
                    costBestPuSum += BIT_COST(m_bsf->GetNumBits());
                }

                if (costBest > costBestPuSum) {
                    m_bsf->CtxSave(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLumaCu, m_pitchRec, m_recLumaSaveCu[depth], widthCu, roiSizeCu);
                    costBest = costBestPuSum;
                    if (trDepth == 1) {
                        small_memcpy(m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU) + absPartIdx,
                            m_dataBest + ((depth + 1) << m_par->Log2NumPartInCU) + absPartIdx,
                            sizeof(H265CUData) * numParts);
                    }
                }
                else if (trDepth == 1) {
                    m_data = m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU);
                    m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_recLumaSaveCu[depth], widthCu, m_yRec + offsetLumaCu, m_pitchRec, roiSizeCu);
                }
            }

            Ipp8u chromaDirBest = INTRA_DM_CHROMA, chromaDirLast = 34;
            CostType costTemp;
            m_data = m_dataBest + (depth << m_par->Log2NumPartInCU);

            if (costBest < costInter) {

                if (m_cslice->slice_type == I_SLICE && (m_par->AnalyseFlags & HEVC_ANALYSE_CHROMA)) {
                    CopySubPartTo(m_dataSave, absPartIdx, depth, 0);
                    m_data = m_dataSave;

                    Ipp8u allowedChromaDir[NUM_CHROMA_MODE];
                    GetAllowedChromaDir(absPartIdx, allowedChromaDir);
                    H265CUData *data_b = m_dataBest + (depth << m_par->Log2NumPartInCU);
                    CostType costChromaBest = COST_MAX;

                    for (Ipp8u chromaDir = 0; chromaDir < NUM_CHROMA_MODE; chromaDir++) {
                        if (allowedChromaDir[chromaDir] == 34) continue;
                        if (m_rdOptFlag && chromaDir)
                            m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                        for(Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++)
                            data_b[i].intraChromaDir = m_data[i].intraChromaDir = allowedChromaDir[chromaDir];
                        chromaDirLast = allowedChromaDir[chromaDir];
                        EncAndRecChroma(absPartIdx, offset >> 2, m_data[absPartIdx].depth, NULL, &costTemp);

                        //kolya
                        //HM_MATCH
                        //add bits spent on chromaDir - commented out, gives loss
                        //m_bsf->Reset();
                        //xEncIntraHeaderChroma(m_bsf);
                        //costTemp += BIT_COST(m_bsf->GetNumBits());

                        if (costChromaBest >= costTemp) {
                            costChromaBest = costTemp;
                            chromaDirBest = allowedChromaDir[chromaDir];
                        }
                    }
                    for(Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++)
                        data_b[i].intraChromaDir = m_data[i].intraChromaDir = chromaDirBest;
                    m_data = data_b;
                }
                if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) ||
                    m_cslice->slice_type == I_SLICE && (m_par->AnalyseFlags & HEVC_ANALYSE_CHROMA) )
                {
                    if (chromaDirLast != chromaDirBest) {
                        if (m_rdOptFlag)
                            m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                        EncAndRecChroma(absPartIdx, offset >> 2, m_data[absPartIdx].depth, NULL, &costTemp);
                    }
                    if (m_rdOptFlag)
                        m_bsf->CtxSave(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_uvRec + offsetChromaCu, m_pitchRec, m_recChromaSaveCu[depth], widthCu, roiSizeCuChr);
                    if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
                        costBest += costTemp;
                }
            } //  
        } // if (tryIntra)

        if (costBest >= costInter) { // load inter results
            costBest = costInter;
            small_memcpy(m_dataBest + ((depth) << m_par->Log2NumPartInCU) + absPartIdx, m_dataInter /*+ absPartIdx*/, sizeof(H265CUData) * numParts); // or below?
            small_memcpy(m_dataSave + absPartIdx, m_dataInter /*+ absPartIdx*/, sizeof(H265CUData) * numParts);
            //small_memcpy(ctxSave[2], ctxSave[3], sizeof(H265CUData) * numParts);
            m_bsf->CtxRestore(ctxSave[3], 0, NUM_CABAC_CONTEXT);
            m_bsf->CtxSave(ctxSave[2], 0, NUM_CABAC_CONTEXT);
            ippiCopy_8u_C1R(m_interRec[m_data[absPartIdx].curRecIdx][ depth ] + offsetLumaBuf, MAX_CU_SIZE, m_yRec + offsetLumaCu, m_pitchRec, roiSizeCu);
            if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
                ippiCopy_8u_C1R(m_interRecChroma[m_data[absPartIdx].curRecIdx][ depth ] + offsetChromaBuf, MAX_CU_SIZE, m_uvRec + offsetChromaCu, m_pitchRec, roiSizeCuChr);
            ippiCopy_8u_C1R(m_interRec[m_data[absPartIdx].curRecIdx][ depth ] + offsetLumaBuf, MAX_CU_SIZE, m_recLumaSaveCu[depth], widthCu, roiSizeCu);
            if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
                ippiCopy_8u_C1R(m_interRecChroma[m_data[absPartIdx].curRecIdx][ depth ] + offsetChromaBuf, MAX_CU_SIZE, m_recChromaSaveCu[depth], widthCu, roiSizeCuChr);
        }

        //// TODO: check is needed
        //if (splitMode == SPLIT_TRY) {
        //    ippiCopy_8u_C1R(m_yRec + offsetLumaCu, m_pitchRec, m_recLumaSaveCu[depth], widthCu, roiSizeCu);
        //    if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
        //        ippiCopy_8u_C1R(m_uvRec + offsetChromaCu, m_pitchRec, m_recChromaSaveCu[depth], widthCu, roiSizeCuChr);
        //}
    } // if (splitMode != SPLIT_MUST)

    Ipp8u skippedFlag = 0;
    CostType cuSplitThresholdCu = 0;
    if (splitMode == SPLIT_TRY) {
        skippedFlag = (m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU))[absPartIdx].flags.skippedFlag;
        Ipp32s qp_best = (m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU))[absPartIdx].qp;
        cuSplitThresholdCu = m_par->cu_split_threshold_cu[qp_best][m_cslice->slice_type != I_SLICE][depth];

        if (m_par->cuSplitThreshold > 0) {
            Ipp64u costNeigh = 0, costCur = 0, numNeigh = 0, numCur = 0;
            Ipp64f costAvg = 0;
            costStat* cur = m_costStat + m_ctbAddr;

            costCur += cur->cost[depth] * cur->num[depth];
            numCur += cur->num[depth];
            if (m_aboveAddr >= m_cslice->slice_segment_address) {
                costStat* above = m_costStat + m_aboveAddr;
                costNeigh += above->cost[depth] * above->num[depth];
                numNeigh += above->num[depth];
            }
            if (m_aboveLeftAddr >= m_cslice->slice_segment_address) {
                costStat* aboveLeft = m_costStat + m_aboveLeftAddr;
                costNeigh += aboveLeft->cost[depth] * aboveLeft->num[depth];
                numNeigh += aboveLeft->num[depth];
            }
            if (m_aboveRightAddr >= m_cslice->slice_segment_address) {
                costStat* aboveRight =  m_costStat + m_aboveRightAddr;
                costNeigh += aboveRight->cost[depth] * aboveRight->num[depth];
                numNeigh += aboveRight->num[depth];
            }
            if (m_leftAddr >= m_cslice->slice_segment_address) {
                costStat* left =  m_costStat + m_leftAddr;
                costNeigh += left->cost[depth] * left->num[depth];
                numNeigh += left->num[depth];
            }

            if (numNeigh + numCur)
                costAvg = ((0.61 * costCur) + (0.39 * costNeigh)) / ((0.61 * numCur) + (0.39 * numNeigh));

            if (costBest < (m_par->cuSplitThreshold / 256.0) * costAvg && costAvg != 0 && depth != 0)
            {
                splitMode = SPLIT_NONE;
            }
        }
    }

    if (costBest >= cuSplitThresholdCu && splitMode != SPLIT_NONE && !(skippedFlag && m_par->cuSplit == 2)) {
        // restore ctx
        if (m_rdOptFlag)
            m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);

        // split
        CostType costSplit = 0;
        for (Ipp32s i = 0; i < 4; i++) {

            if (splitMode == SPLIT_MUST)
                m_depthMin = MAX_TOTAL_DEPTH; // to be computed in sub-CU

            CostType costTemp;
            ModeDecision(absPartIdx + (numParts >> 2) * i, offset + subsize * i, depth+1, &costTemp);
            costSplit += costTemp;
            if (m_par->cuSplitThreshold > 0 && (m_dataBest + ((depth + 1) << m_par->Log2NumPartInCU) + absPartIdx + (numParts >> 2) * i)->predMode != MODE_INTRA)
            {
                costStat* cur = m_costStat + m_ctbAddr;
                Ipp64u costTotal = cur->cost[depth + 1] * cur->num[depth + 1];
                cur->num[depth + 1] ++;
                cur->cost[depth + 1] = (costTotal + costTemp) / cur->num[depth + 1];
            }
        }
        if (m_rdOptFlag && splitMode != SPLIT_MUST) {
            m_bsf->Reset();
            m_data = m_dataBest + ((depth + 1) << m_par->Log2NumPartInCU);
            CopySubPartTo(m_dataSave, absPartIdx, depth, 0);
            m_data = m_dataSave;
            EncodeCU(m_bsf, absPartIdx, depth, RD_CU_SPLITFLAG);
            if(m_cslice->slice_type == I_SLICE)
                costSplit += BIT_COST(m_bsf->GetNumBits());
            else
                costSplit += BIT_COST_INTER(m_bsf->GetNumBits());
        }

#ifdef DUMP_COSTS_CU
        if (splitMode != SPLIT_MUST) {
            printCostStat(fp_cu, m_par->QP,m_cslice->slice_type != I_SLICE, widthCu,costBest,costSplit);
        }
#endif
        if (m_par->cuSplitThreshold > 0 && splitMode == SPLIT_TRY && depth == 0) {
            costStat* cur = m_costStat + m_ctbAddr;
            Ipp64u costTotal = cur->cost[depth] * cur->num[depth];
            cur->num[depth] ++;
            cur->cost[depth] = (costTotal + costBest) / cur->num[depth];
        }

        // add cost of cu split flag to costSplit
        if (costBest > costSplit) {
            costBest = costSplit;
            small_memcpy(m_dataBest + (depth << m_par->Log2NumPartInCU) + absPartIdx,
                m_dataBest + ((depth + 1) << m_par->Log2NumPartInCU) + absPartIdx,
                sizeof(H265CUData) * numParts);
        }
        else {
            ippiCopy_8u_C1R(m_recLumaSaveCu[depth], widthCu, m_yRec + offsetLumaCu, m_pitchRec, roiSizeCu);
            if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) ||
                m_cslice->slice_type == I_SLICE && (m_par->AnalyseFlags & HEVC_ANALYSE_CHROMA) )
                ippiCopy_8u_C1R(m_recChromaSaveCu[depth], widthCu, m_uvRec + offsetChromaCu, m_pitchRec, roiSizeCuChr);
            m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
        }
    }

    small_memcpy(m_dataSave + absPartIdx,
        m_dataBest + ((depth) << m_par->Log2NumPartInCU) + absPartIdx,
        sizeof(H265CUData) * numParts);

    if (cost)
        *cost = costBest;

    if (depth == 0) {
        small_memcpy(m_dataSave, m_dataBest, sizeof(H265CUData) << m_par->Log2NumPartInCU);
        m_data = m_dataSave;
        for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
            m_data[i].cbf[0] = m_data[i].cbf[1] = m_data[i].cbf[2] = 0;
        }
    }
}

void H265CU::CalcCostLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth,
                          Ipp8u trDepth, CostOpt costOpt,
                          Ipp8u partSize, Ipp8u lumaDir, CostType *cost)
{
    CABAC_CONTEXT_H265 ctxSave[2][NUM_CABAC_CONTEXT];
    H265CUData *data_t;
    data_t = m_dataTemp + ((depth + trDepth) << m_par->Log2NumPartInCU);

    Ipp32s width = m_par->MaxCUSize >> (depth + trDepth);
    CostType costBest = COST_MAX;
    IppiSize roiSize = {width, width};
    Ipp32s offsetLuma = GetLumaOffset(m_par, absPartIdx, m_pitchRec);
    Ipp8u trDepthZeroOnly = costOpt == COST_PRED_TR_0 || costOpt == COST_REC_TR_0;
    Ipp8u nz = 1;

    Ipp8u splitMode = GetTrSplitMode(absPartIdx, depth, trDepth, partSize, 1);

    if (trDepthZeroOnly && splitMode != SPLIT_MUST) {
        splitMode = SPLIT_NONE;
    } else if (costOpt == COST_REC_TR_MAX) {
        if (/*pred_mode == MODE_INTRA && */
            (width == 32 || (width < 32 && trDepth == 0))) {
            m_data = data_t;
            IntraPredTu(absPartIdx, width, lumaDir, 1);
        }
        if (splitMode != SPLIT_NONE)
            splitMode = SPLIT_MUST;
    }

    // save ctx
    if (m_rdOptFlag)
        m_bsf->CtxSave(ctxSave[0], tab_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);

    if (splitMode != SPLIT_MUST) {
        IntraPredOpt intraPredOpt;
        if (costOpt == COST_REC_TR_MAX) {
            intraPredOpt = INTRA_PRED_IN_REC;
        } else if (m_predIntraAllWidth == width) {
            intraPredOpt = INTRA_PRED_IN_BUF;
        } else {
            intraPredOpt = INTRA_PRED_CALC;
        }
        Ipp8u costPredFlag = costOpt == COST_PRED_TR_0;

        m_data = data_t;
        FillSubPart(absPartIdx, depth, trDepth, partSize, lumaDir,m_par->m_lcuQps[m_ctbAddr]);
        if (m_rdOptFlag) {
            // cp data subpart to main
            CopySubPartTo(m_dataSave, absPartIdx, depth, trDepth);
            m_data = m_dataSave;
        }
        EncAndRecLumaTu(absPartIdx, offset, width, &nz, &costBest, costPredFlag, intraPredOpt);
        if (splitMode == SPLIT_TRY) {
            if (m_rdOptFlag)
                m_bsf->CtxSave(ctxSave[1], tab_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);
            ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth+trDepth], width, roiSize);
        }
    }

    CostType cuSplitThresholdTu = 0;
    if (splitMode == SPLIT_TRY) {
        cuSplitThresholdTu = m_par->cu_split_threshold_tu[m_data[absPartIdx].qp][0][depth+trDepth];
    }

    if (costBest >= cuSplitThresholdTu && splitMode != SPLIT_NONE && nz) {
        Ipp32u numParts = ( m_par->NumPartInCU >> ((depth + trDepth)<<1) );
        Ipp32s subsize = width >> 1;
        subsize *= subsize;

        // restore ctx
        if (m_rdOptFlag)
            m_bsf->CtxRestore(ctxSave[0], tab_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);

        CostType costSplit = 0;
        if (m_rdOptFlag && splitMode != SPLIT_MUST) {
            m_bsf->Reset();
            Ipp32s tr_size = h265_log2m2[m_par->MaxCUSize] + 2 - depth - trDepth;
            m_bsf->EncodeSingleBin_CABAC(CTX(m_bsf,TRANS_SUBDIV_FLAG_HEVC) + 5 - tr_size, 1);
            costSplit += BIT_COST(m_bsf->GetNumBits());
        }
        for (Ipp32u i = 0; i < 4; i++) {
            CostType costTemp;
            CalcCostLuma(absPartIdx + (numParts >> 2) * i, offset + subsize * i,
                depth, trDepth + 1, costOpt, partSize, lumaDir, &costTemp);
            costSplit += costTemp;
        }
        m_data = m_dataSave;

#ifdef DUMP_COSTS_TU
        if (splitMode != SPLIT_MUST) {
            printCostStat(fp_tu, m_par->m_lcuQps[m_ctbAddr], 0, width,costBest,costSplit);
        }
#endif

        if (costBest > costSplit) {
            costBest = costSplit;
            small_memcpy(data_t + absPartIdx, data_t + ((size_t)1 << (size_t)(m_par->Log2NumPartInCU)) + absPartIdx,
                sizeof(H265CUData) * numParts);
        } else {
            // restore ctx
            // cp data subpart to main
            if (m_rdOptFlag)
                m_bsf->CtxRestore(ctxSave[1], tab_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);
            ippiCopy_8u_C1R(m_recLumaSaveTu[depth+trDepth], width, m_yRec + offsetLuma, m_pitchRec, roiSize);
        }
    }

    *cost = costBest;
}

// absPartIdx - for minimal TU
void H265CU::DetailsXY(H265MEInfo* me_info) const
{
    Ipp32s width = me_info->width;
    Ipp32s height = me_info->height;
    Ipp32s ctbOffset = me_info->posx + me_info->posy * m_pitchSrc;
    Ipp32s dx = 0, dy = 0;

    PixType *pSrc = m_ySrc + ctbOffset;
    for(Ipp32s y=0; y<height; y++) {
        for(Ipp32s x=0; x<width-1; x++)
            dx += (pSrc[x]-pSrc[x+1]) * (pSrc[x]-pSrc[x+1]);
        pSrc += m_pitchSrc;
    }

    pSrc = m_ySrc + ctbOffset;
    for(Ipp32s y=0; y<height-1; y++) {
        for(Ipp32s x=0; x<width; x++)
            dy += (pSrc[x]-pSrc[x+m_pitchSrc]) * (pSrc[x]-pSrc[x+m_pitchSrc]);
        pSrc += m_pitchSrc;
    }
}


Ipp32s H265CU::MvCost1RefLog(H265MV mv, Ipp8s refIdx, const MvPredInfo<2> *predInfo, Ipp32s rlist) const
{
    Ipp32s costMin = INT_MAX;
    Ipp32s mpvIdx = INT_MAX;
    predInfo += 2 * refIdx + rlist;
    for (Ipp32s i = 0; i < predInfo->numCand; i++) {
        Ipp32s cost = m_logMvCostTable[mv.mvx - predInfo->mvCand[i].mvx] +
                      m_logMvCostTable[mv.mvy - predInfo->mvCand[i].mvy];
        if (costMin > cost) {
            costMin = cost;
            mpvIdx = i;
        }
    }

    return
        (Ipp32s)(m_logMvCostTable[mv.mvx - predInfo->mvCand[mpvIdx].mvx] * m_cslice->rd_lambda_sqrt + 0.5) +
        (Ipp32s)(m_logMvCostTable[mv.mvy - predInfo->mvCand[mpvIdx].mvy] * m_cslice->rd_lambda_sqrt + 0.5);
}


// absPartIdx - for minimal TU
Ipp32s H265CU::MatchingMetricPu(PixType *src, const H265MEInfo* meInfo, H265MV* mv,
                                H265Frame *refPic, Ipp32s useHadamard) const
{
    Ipp32s refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma;
    PixType *rec = refPic->y + refOffset;
    Ipp32s pitchRec = refPic->pitch_luma;
    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE*MAX_CU_SIZE];

    if ((mv->mvx | mv->mvy) & 3)
    {
        MeInterpolate(meInfo, mv, refPic->y, refPic->pitch_luma, predBuf, MAX_CU_SIZE);
        rec = predBuf;
        pitchRec = MAX_CU_SIZE;
        
        return (useHadamard)
            ? tuHad(src, m_pitchSrc, rec, MAX_CU_SIZE, meInfo->width, meInfo->height)
            : MFX_HEVC_PP::h265_SAD_MxN_special_8u(src, rec, m_pitchSrc, meInfo->width, meInfo->height);
    }
    else
    {
        return (useHadamard)
            ? tuHad(src, m_pitchSrc, rec, pitchRec, meInfo->width, meInfo->height)
            : MFX_HEVC_PP::h265_SAD_MxN_general_8u(rec, pitchRec, src, m_pitchSrc, meInfo->width, meInfo->height);
    }
}

void WriteAverageToPic(
    const Ipp16s *src0,
    Ipp32u        pitchSrc0,      // in samples
    const Ipp16s *src1,
    Ipp32u        pitchSrc1,      // in samples
    Ipp8u * H265_RESTRICT dst,
    Ipp32u        pitchDst,       // in samples
    Ipp32s        width,
    Ipp32s        height)
{
#ifdef __INTEL_COMPILER
    #pragma ivdep
#endif // __INTEL_COMPILER
    for (int j = 0; j < height; j++)
    {
#ifdef __INTEL_COMPILER
        #pragma ivdep
        #pragma vector always
#endif // __INTEL_COMPILER
        for (int i = 0; i < width; i++) {
            Ipp16s val = ((src0[i] + src1[i] + 1) >> 1);
            dst[i] = (Ipp8u)Saturate(0, 255, val);
        }

        src0 += pitchSrc0;
        src1 += pitchSrc1;
        dst  += pitchDst;
    }
}


Ipp32s H265CU::MatchingMetricBipredPu(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], Ipp32s useHadamard)
{
    H265MV mvsClipped[2] = { mvs[0], mvs[1] };
    H265Frame *refFrame[2] = { m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]],
                               m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]] };

    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE * MAX_CU_SIZE];

    if (CheckIdenticalMotion(refIdx, mvsClipped)) {
        Ipp32s listIdx = !!(refIdx[0] < 0);

        ClipMV(mvsClipped[listIdx]);
        H265MV *mv = mvsClipped + listIdx;
        if ((mv->mvx | mv->mvy) & 3) {
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, listIdx,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, false, AVERAGE_NO);
        }
        else {
            // unique case; int pel from single ref
            Ipp32s refOffset = (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refFrame[listIdx]->pitch_luma;
            refOffset += m_ctbPelX + meInfo->posx + (mv->mvx >> 2);
            PixType *rec = refFrame[listIdx]->y + refOffset;
            Ipp32s pitchRec = refFrame[listIdx]->pitch_luma;

            return (useHadamard)
                ? tuHad(src, m_pitchSrc, rec, pitchRec, meInfo->width, meInfo->height)
                : MFX_HEVC_PP::h265_SAD_MxN_general_8u(rec, pitchRec, src, m_pitchSrc, meInfo->width, meInfo->height);
        }
    }
    else {
        ClipMV(mvsClipped[0]);
        ClipMV(mvsClipped[1]);
        Ipp32s interpFlag0 = (mvsClipped[0].mvx | mvsClipped[0].mvy) & 3;
        Ipp32s interpFlag1 = (mvsClipped[1].mvx | mvsClipped[1].mvy) & 3;
        bool onlyOneIterp = !(interpFlag0 && interpFlag1);

        if (onlyOneIterp) {
            Ipp32s listIdx = !interpFlag0;
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, listIdx,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, true, AVERAGE_FROM_PIC);
        }
        else {
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, REF_PIC_LIST_0,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, true, AVERAGE_NO);
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, REF_PIC_LIST_1,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, true, AVERAGE_FROM_BUF);
        }
    }

    return (useHadamard)
        ? tuHad(src, m_pitchSrc, predBuf, MAX_CU_SIZE, meInfo->width, meInfo->height)
        : MFX_HEVC_PP::h265_SAD_MxN_special_8u(src, predBuf, m_pitchSrc, meInfo->width, meInfo->height);
}


static void TuDiff(CoeffsType *residual, Ipp32s pitchDst,
                   const PixType *src, Ipp32s pitchSrc,
                   const PixType *pred, Ipp32s pitchPred, Ipp32s size);
static Ipp32s TuSse(const PixType *src, Ipp32s pitchSrc,
                    const PixType *rec, Ipp32s pitchRec, Ipp32s size);
static void TuDiffNv12(CoeffsType *residual, Ipp32s pitchDst,  const PixType *src, Ipp32s pitchSrc,
                       const PixType *pred, Ipp32s pitchPred, Ipp32s size);
static bool IsCandFound(const Ipp8s *curRefIdx, const H265MV *curMV, const MvPredInfo<5> *mergeInfo,
                        Ipp32s candIdx, Ipp32s numRefLists);
static Ipp32s GetFlBits(Ipp32s val, Ipp32s maxVal);

static Ipp32s SamePrediction(const H265MEInfo* a, const H265MEInfo* b)
{
    // TODO fix for GPB: same ref for different indices
    if (a->interDir != b->interDir ||
        (a->interDir & INTER_DIR_PRED_L0) && (a->MV[0] != b->MV[0] || a->refIdx[0] != b->refIdx[0]) ||
        (a->interDir & INTER_DIR_PRED_L1) && (a->MV[1] != b->MV[1] || a->refIdx[1] != b->refIdx[1]) )
        return false;
    else
        return true;
}


// Tries different PU modes for CU
// m_data assumed to dataSave, not dataBest(depth)
CostType H265CU::MeCu(Ipp32u absPartIdx, Ipp8u depth, Ipp32s offset)
{
    H265MEInfo meInfo, bestMeInfo[4];
    meInfo.absPartIdx = absPartIdx;
    meInfo.depth = depth;
    meInfo.width  = (Ipp8u)(m_par->MaxCUSize>>depth);
    meInfo.height = (Ipp8u)(m_par->MaxCUSize>>depth);
    meInfo.posx = (Ipp8u)((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
    meInfo.posy = (Ipp8u)((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
    meInfo.splitMode = PART_SIZE_2Nx2N;

    IppiSize roiSize = {meInfo.width, meInfo.width};
    IppiSize roiSizeChr = {meInfo.width, meInfo.width / 2};
    Ipp32s offsetLuma = GetLumaOffset(m_par, absPartIdx, m_pitchRec);
    Ipp32s offsetChroma = GetChromaOffset(m_par, absPartIdx, m_pitchRec);
    Ipp32s offsetPred = meInfo.posx + meInfo.posy * MAX_CU_SIZE;
    Ipp32s offsetPredChroma = meInfo.posx + meInfo.posy * (MAX_CU_SIZE / 2); //for NV12 chroma; 1/2 for separated chroma

    CABAC_CONTEXT_H265 ctxSave[2][NUM_CABAC_CONTEXT]; // 0-initial, 1-best
    m_bsf->CtxSave(ctxSave[0], 0, NUM_CABAC_CONTEXT);

    CostType bestCost = COST_MAX, bestCostMergeSkip = COST_MAX;

    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    CostType cost = 0;

    // Try skips
    Ipp32s candBest = 0;
    Ipp8u  size = (Ipp8u)(m_par->MaxCUSize >> depth);
    Ipp32s CUSizeInMinTU = size >> m_par->Log2MinTUSize;

    GetMergeCand(absPartIdx, PART_SIZE_2Nx2N, 0, CUSizeInMinTU, m_mergeCand);

    PixType *srcY = m_ySrc + offsetLuma;
    PixType *recY = m_yRec + offsetLuma;
    PixType *srcUv = m_uvSrc + offsetChroma;
    PixType *recUv = m_uvRec + offsetChroma;

    for (Ipp32s i = 0; i < m_mergeCand->numCand; i++) {
        Ipp8s *refIdx = m_mergeCand->refIdx + 2 * i;
        if (refIdx[0] < 0 && refIdx[1] < 0)
            continue; // skip duplicate

        H265MV *mv = m_mergeCand->mvCand + 2 * i;
        CostType cost = COST_MAX;
        if (refIdx[0] >= 0 && refIdx[1] >= 0) {
            cost = MatchingMetricBipredPu(srcY, &meInfo, refIdx, mv, 0);
        }
        else {
            Ipp32s listIdx = (refIdx[1] >= 0);
            H265Frame *ref = m_currFrame->m_refPicList[listIdx].m_refFrames[refIdx[listIdx]];
            cost = MatchingMetricPu(srcY, &meInfo, mv + listIdx, ref, 0);
        }

        if (bestCost > cost) {
            cost += (Ipp32s)(GetFlBits(i, m_mergeCand->numCand) * m_cslice->rd_lambda_sqrt + 0.5);
            if (bestCost > cost) {
                bestCost = cost;
                candBest = i;
            }
        }
    }

    Ipp8u interDir = (m_mergeCand->refIdx[2 * candBest + 0] >= 0) * INTER_DIR_PRED_L0 +
                     (m_mergeCand->refIdx[2 * candBest + 1] >= 0) * INTER_DIR_PRED_L1;
    memset(m_data + absPartIdx, 0, sizeof(H265CUData));
    m_data[absPartIdx].depth = depth;
    m_data[absPartIdx].size = (Ipp8u)(m_par->MaxCUSize >> depth);
    m_data[absPartIdx].qp = m_par->m_lcuQps[m_ctbAddr];
    m_data[absPartIdx].interDir = interDir;
    m_data[absPartIdx].mergeIdx = (Ipp8u)candBest;
    m_data[absPartIdx].mv[0] = m_mergeCand->mvCand[2 * candBest + 0];
    m_data[absPartIdx].mv[1] = m_mergeCand->mvCand[2 * candBest + 1];
    m_data[absPartIdx].refIdx[0] = m_mergeCand->refIdx[2 * candBest + 0];
    m_data[absPartIdx].refIdx[1] = m_mergeCand->refIdx[2 * candBest + 1];
    m_data[absPartIdx].flags.mergeFlag = 1;
    m_data[absPartIdx].flags.skippedFlag = 1;
    m_data[absPartIdx].bestRecIdx = 1;
    PropagateSubPart(m_data + absPartIdx, numParts);

    m_interPredReady = false;
    const Ipp8u curIdx = 0; // idx: current, idx^1: best (== m_data[absPartIdx].curIdx)
    PixType* pred = m_interPred[curIdx][depth];
    PixType* predChr = m_interPredChroma[curIdx][depth];
    CoeffsType *residY = m_interResidualsY[curIdx][depth] + offsetPred;
    CoeffsType *residU = m_interResidualsU[curIdx][depth] + offsetPredChroma / 2;
    CoeffsType *residV = m_interResidualsV[curIdx][depth] + offsetPredChroma / 2;

    InterPredCu<TEXT_LUMA>(absPartIdx, depth, pred, MAX_CU_SIZE);
    InterPredCu<TEXT_CHROMA>(absPartIdx, depth, predChr, MAX_CU_SIZE);

    m_bsf->Reset();
    EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
    bestCost = BIT_COST_INTER(m_bsf->GetNumBits()); // bits for skipped

    ippiCopy_8u_C1R(pred + offsetPred, MAX_CU_SIZE, m_interRec[m_data[absPartIdx].curRecIdx][ depth ] + offsetPred, MAX_CU_SIZE, roiSize);
    if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
        ippiCopy_8u_C1R(predChr + offsetPredChroma, MAX_CU_SIZE, m_interRecChroma[m_data[absPartIdx].curRecIdx][ depth ] + offsetPredChroma, MAX_CU_SIZE, roiSizeChr);
    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
    small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);

    CostType fastCost = COST_MAX;

    if (m_par->fastSkip) { // check if all cbf are zero
        Ipp8u cbf[256][3]; // use from [absPartIdx]

        Ipp8u tr_depth_min, tr_depth_max;
        GetTrDepthMinMax(m_par, absPartIdx, depth, PART_SIZE_2Nx2N, &tr_depth_min, &tr_depth_max);
        tr_depth_min = tr_depth_max; // will try with minimal transform size

        TuDiff(residY, MAX_CU_SIZE, srcY, m_pitchSrc, pred + offsetPred, MAX_CU_SIZE, size);
        TuDiffNv12(residU, MAX_CU_SIZE / 2, srcUv,     m_pitchSrc, predChr + offsetPredChroma,     MAX_CU_SIZE, size / 2);
        TuDiffNv12(residV, MAX_CU_SIZE / 2, srcUv + 1, m_pitchSrc, predChr + offsetPredChroma + 1, MAX_CU_SIZE, size / 2);

        for (Ipp32u i = 0; i < numParts; i++)
            m_data[absPartIdx + i].flags.skippedFlag = 0;
        memset(&cbf[absPartIdx], 0, numParts * sizeof(cbf[0]));

        TuMaxSplitInter(absPartIdx, tr_depth_max, &fastCost, cbf); // min TU size

        if (!cbf[absPartIdx][0] && !cbf[absPartIdx][1] && !cbf[absPartIdx][2])
            return fastCost + bestCost; // diff + skipped cost

        for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
            m_data[i].cbf[0] = cbf[i][0];
            m_data[i].cbf[1] = cbf[i][1];
            m_data[i].cbf[2] = cbf[i][2];
        }

        m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        m_bsf->Reset();
        EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
        fastCost += BIT_COST_INTER(m_bsf->GetNumBits()); // + not skipped merge cost
    }

    // check forced skip
    bestCost += TuSse(srcY, m_pitchSrc, pred + offsetPred, MAX_CU_SIZE, size); // skip flag cost +=
    CostType costChroma = TuSse(srcUv, m_pitchSrc, predChr + offsetPredChroma, MAX_CU_SIZE, size >> 1);
    costChroma += TuSse(srcUv + (size >> 1), m_pitchSrc, predChr + offsetPredChroma + (size >> 1), MAX_CU_SIZE, size >> 1);
    if (!(m_par->AnalyseFlags & HEVC_COST_CHROMA))
        costChroma /= 4; // add chroma with little weight
    bestCost += costChroma;

    if (fastCost < bestCost) {
        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
        small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
        bestCost = fastCost;
    }
    else if (!m_par->fastSkip) { // TODO check if needed
        TuDiff(residY, MAX_CU_SIZE, srcY, m_pitchSrc, pred + offsetPred, MAX_CU_SIZE, size);
        if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
            TuDiffNv12(residU, MAX_CU_SIZE / 2, srcUv,     m_pitchSrc, predChr + offsetPredChroma,     MAX_CU_SIZE, size / 2);
            TuDiffNv12(residV, MAX_CU_SIZE / 2, srcUv + 1, m_pitchSrc, predChr + offsetPredChroma + 1, MAX_CU_SIZE, size / 2);
        }
    } else {
        ippiCopy_8u_C1R(pred + offsetPred, MAX_CU_SIZE, m_interRec[m_data[absPartIdx].curRecIdx][ depth ] + offsetPred, MAX_CU_SIZE, roiSize);
        if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
            ippiCopy_8u_C1R(predChr + offsetPredChroma, MAX_CU_SIZE, m_interRecChroma[m_data[absPartIdx].curRecIdx][ depth ] + offsetPredChroma, MAX_CU_SIZE, roiSizeChr);
    }

    m_data[absPartIdx].curIdx = 1;
    m_data[absPartIdx].curRecIdx = 1;
    m_data[absPartIdx].bestRecIdx = 0;

    GetAmvpCand(absPartIdx, PART_SIZE_2Nx2N, 0, m_amvpCand[0]);
    MePu(&meInfo, 0);

    Ipp32u nonZeroCbf = 1;
    bestMeInfo[0] = meInfo;

    bestCostMergeSkip = bestCost;

    if (m_par->puDecisionSatd) {
        bestCost = PuCost(&meInfo);
        nonZeroCbf = bestCost != 0;
    } else {
        m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        cost = CuCost(absPartIdx, depth, &meInfo);
        if (bestCost > cost) {
            bestCost = cost;
            if (m_par->fastCbfMode)
                nonZeroCbf = m_data[absPartIdx].cbf[0] | m_data[absPartIdx].cbf[1] | m_data[absPartIdx].cbf[2];
            m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
            small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
            m_data[absPartIdx].curIdx ^= 1;
            SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)
        }
    }

    bestMeInfo[0] = meInfo;

    // NxN prediction
    if (nonZeroCbf && depth == m_par->MaxCUDepth - m_par->AddCUDepth && meInfo.width > 8) {
        H265MEInfo bestInfo[4];
        Ipp32s allthesame = true;
        for (Ipp32s i = 0; i < 4; i++) {
            bestInfo[i] = meInfo;
            bestInfo[i].width  = meInfo.width / 2;
            bestInfo[i].height = meInfo.height / 2;
            bestInfo[i].posx  = (Ipp8u)(meInfo.posx + bestInfo[i].width * (i & 1));
            bestInfo[i].posy  = (Ipp8u)(meInfo.posy + bestInfo[i].height * ((i>>1) & 1));
            bestInfo[i].absPartIdx = meInfo.absPartIdx + (numParts >> 2)*i;
            bestInfo[i].splitMode = PART_SIZE_NxN;

            GetMergeCand(absPartIdx, PART_SIZE_NxN, i, CUSizeInMinTU, m_mergeCand + i);
            GetAmvpCand(absPartIdx, PART_SIZE_NxN, i, m_amvpCand[i]);
            Ipp32s interPredIdx = i == 0 ? 0 : (bestInfo[i - 1].refIdx[0] < 0) ? 1 : (bestInfo[i - 1].refIdx[1] < 0) ? 0 : 2;
            MePu(bestInfo + i, interPredIdx);
            allthesame = allthesame && SamePrediction(&bestInfo[i], &meInfo);
        }
        if (!allthesame) {
            if (m_par->puDecisionSatd) {
                cost = PuCost(bestInfo + 0) + PuCost(bestInfo + 1) + PuCost(bestInfo + 2) + PuCost(bestInfo + 3);
            } else {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, bestInfo);
            }
            if (bestCost > cost) {
                bestMeInfo[0] = bestInfo[0];
                bestMeInfo[1] = bestInfo[1];
                bestMeInfo[2] = bestInfo[2];
                bestMeInfo[3] = bestInfo[3];
                bestCost = cost;

                if (m_par->puDecisionSatd) {
                    nonZeroCbf = bestCost != 0;
                } else {
                    if (m_par->fastCbfMode)
                        nonZeroCbf = m_data[absPartIdx].cbf[0] | m_data[absPartIdx].cbf[1] | m_data[absPartIdx].cbf[2];
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    m_data[absPartIdx].curIdx ^= 1;
                    SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)
                }
            }
        }
    }

    // Nx2N
    if (nonZeroCbf && m_par->partModes > 1) {
        H265MEInfo partsInfo[2] = {meInfo, meInfo};
        partsInfo[0].width = meInfo.width / 2;
        partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 2)*1;
        partsInfo[1].width = meInfo.width / 2;
        partsInfo[1].posx  = meInfo.posx + partsInfo[0].width;
        partsInfo[0].splitMode = PART_SIZE_Nx2N;
        partsInfo[1].splitMode = PART_SIZE_Nx2N;

        GetMergeCand(absPartIdx, PART_SIZE_Nx2N, 0, CUSizeInMinTU, m_mergeCand + 0);
        GetAmvpCand(absPartIdx, PART_SIZE_Nx2N, 0, m_amvpCand[0]);
        MePu(partsInfo + 0, 0);
        Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
        GetMergeCand(absPartIdx, PART_SIZE_Nx2N, 1, CUSizeInMinTU, m_mergeCand + 1);
        GetAmvpCand(absPartIdx, PART_SIZE_Nx2N, 1, m_amvpCand[1]);
        MePu(partsInfo + 1, interPredIdx);

        if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
            if (m_par->puDecisionSatd) {
                cost = PuCost(partsInfo + 0) + PuCost(partsInfo + 1);
            }
            else {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo);
            }
            if (bestCost > cost) {
                bestMeInfo[0] = partsInfo[0];
                bestMeInfo[1] = partsInfo[1];
                bestCost = cost;

                if (m_par->puDecisionSatd) {
                    nonZeroCbf = bestCost != 0;
                }
                else {
                    if (m_par->fastCbfMode)
                        nonZeroCbf = m_data[absPartIdx].cbf[0] | m_data[absPartIdx].cbf[1] | m_data[absPartIdx].cbf[2];
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    m_data[absPartIdx].curIdx ^= 1;
                    SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)
                }
            }
        }
    }

    // 2NxN
    if (nonZeroCbf && m_par->partModes > 1) {
        H265MEInfo partsInfo[2] = {meInfo, meInfo};
        partsInfo[0].height = meInfo.height / 2;
        partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 2)*2;
        partsInfo[1].height = meInfo.height / 2;
        partsInfo[1].posy  = meInfo.posy + partsInfo[0].height;
        partsInfo[0].splitMode = PART_SIZE_2NxN;
        partsInfo[1].splitMode = PART_SIZE_2NxN;

        GetMergeCand(absPartIdx, PART_SIZE_2NxN, 0, CUSizeInMinTU, m_mergeCand + 0);
        GetAmvpCand(absPartIdx, PART_SIZE_2NxN, 0, m_amvpCand[0]);
        MePu(partsInfo + 0, 0);
        Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
        GetMergeCand(absPartIdx, PART_SIZE_2NxN, 1, CUSizeInMinTU, m_mergeCand + 1);
        GetAmvpCand(absPartIdx, PART_SIZE_2NxN, 1, m_amvpCand[1]);
        MePu(partsInfo + 1, interPredIdx);

        //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
        if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
            if (m_par->puDecisionSatd) {
                cost = PuCost(partsInfo + 0) + PuCost(partsInfo + 1);
            }
            else {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo);
            }
            if (bestCost > cost) {
                bestMeInfo[0] = partsInfo[0];
                bestMeInfo[1] = partsInfo[1];
                bestCost = cost;

                if (m_par->puDecisionSatd) {
                    nonZeroCbf = bestCost != 0;
                }
                else {
                    if (m_par->fastCbfMode)
                        nonZeroCbf = m_data[absPartIdx].cbf[0] | m_data[absPartIdx].cbf[1] | m_data[absPartIdx].cbf[2];
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    m_data[absPartIdx].curIdx ^= 1;
                    SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)
                }
            }
        }
    }

    // advanced prediction modes
    if (m_par->AMPAcc[meInfo.depth]) {
        // PART_SIZE_2NxnU
        if (nonZeroCbf) {
            H265MEInfo partsInfo[2] = {meInfo, meInfo};
            partsInfo[0].height = meInfo.height / 4;
            partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 3);
            partsInfo[1].height = meInfo.height * 3 / 4;
            partsInfo[1].posy  = meInfo.posy + partsInfo[0].height;
            partsInfo[0].splitMode = PART_SIZE_2NxnU;
            partsInfo[1].splitMode = PART_SIZE_2NxnU;

            GetMergeCand(absPartIdx, PART_SIZE_2NxnU, 0, CUSizeInMinTU, m_mergeCand + 0);
            GetAmvpCand(absPartIdx, PART_SIZE_2NxnU, 0, m_amvpCand[0]);
            MePu(partsInfo + 0, 0);
            Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
            GetMergeCand(absPartIdx, PART_SIZE_2NxnU, 1, CUSizeInMinTU, m_mergeCand + 1);
            GetAmvpCand(absPartIdx, PART_SIZE_2NxnU, 1, m_amvpCand[1]);
            MePu(partsInfo + 1, interPredIdx);

            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                if (m_par->puDecisionSatd) {
                    cost = PuCost(partsInfo + 0) + PuCost(partsInfo + 1);
                } else {
                    m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                    cost = CuCost(absPartIdx, depth, partsInfo);
                }
                if (bestCost > cost) {
                    bestMeInfo[0] = partsInfo[0];
                    bestMeInfo[1] = partsInfo[1];
                    bestCost = cost;

                    if (m_par->puDecisionSatd) {
                        nonZeroCbf = bestCost != 0;
                    }
                    else {
                        if (m_par->fastCbfMode)
                            nonZeroCbf = m_data[absPartIdx].cbf[0] | m_data[absPartIdx].cbf[1] | m_data[absPartIdx].cbf[2];
                        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                        small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                        m_data[absPartIdx].curIdx ^= 1;
                        SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)
                    }
                }
            }
        }

        // PART_SIZE_2NxnD
        if (nonZeroCbf) {
            H265MEInfo partsInfo[2] = {meInfo, meInfo};
            partsInfo[0].height = meInfo.height * 3 / 4;
            partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 3) * 5;
            partsInfo[1].height = meInfo.height / 4;
            partsInfo[1].posy  = meInfo.posy + partsInfo[0].height;
            partsInfo[0].splitMode = PART_SIZE_2NxnD;
            partsInfo[1].splitMode = PART_SIZE_2NxnD;

            GetMergeCand(absPartIdx, PART_SIZE_2NxnD, 0, CUSizeInMinTU, m_mergeCand + 0);
            GetAmvpCand(absPartIdx, PART_SIZE_2NxnD, 0, m_amvpCand[0]);
            MePu(partsInfo + 0, 0);
            Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
            GetMergeCand(absPartIdx, PART_SIZE_2NxnD, 1, CUSizeInMinTU, m_mergeCand + 1);
            GetAmvpCand(absPartIdx, PART_SIZE_2NxnD, 1, m_amvpCand[1]);
            MePu(partsInfo + 1, interPredIdx);

            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                if (m_par->puDecisionSatd) {
                    cost = PuCost(partsInfo + 0) + PuCost(partsInfo + 1);
                } else {
                    m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                    cost = CuCost(absPartIdx, depth, partsInfo);
                }
                if (bestCost > cost) {
                    bestMeInfo[0] = partsInfo[0];
                    bestMeInfo[1] = partsInfo[1];
                    bestCost = cost;

                    if (m_par->puDecisionSatd) {
                        nonZeroCbf = bestCost != 0;
                    }
                    else {
                        if (m_par->fastCbfMode)
                            nonZeroCbf = m_data[absPartIdx].cbf[0] | m_data[absPartIdx].cbf[1] | m_data[absPartIdx].cbf[2];
                        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                        small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                        m_data[absPartIdx].curIdx ^= 1;
                        SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)
                    }
                }
            }
        }
        // PART_SIZE_nRx2N
        if (nonZeroCbf) {
            H265MEInfo partsInfo[2] = {meInfo, meInfo};
            partsInfo[0].width = meInfo.width * 3 / 4;
            partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 4) * 5;
            partsInfo[1].width = meInfo.width / 4;
            partsInfo[1].posx  = meInfo.posx + partsInfo[0].width;
            partsInfo[0].splitMode = PART_SIZE_nRx2N;
            partsInfo[1].splitMode = PART_SIZE_nRx2N;

            GetMergeCand(absPartIdx, PART_SIZE_nRx2N, 0, CUSizeInMinTU, m_mergeCand + 0);
            GetAmvpCand(absPartIdx, PART_SIZE_nRx2N, 0, m_amvpCand[0]);
            MePu(partsInfo + 0, 0);
            Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
            GetMergeCand(absPartIdx, PART_SIZE_nRx2N, 1, CUSizeInMinTU, m_mergeCand + 1);
            GetAmvpCand(absPartIdx, PART_SIZE_nRx2N, 1, m_amvpCand[1]);
            MePu(partsInfo + 1, interPredIdx);

            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                if (m_par->puDecisionSatd) {
                    cost = PuCost(partsInfo + 0) + PuCost(partsInfo + 1);
                } else {
                    m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                    cost = CuCost(absPartIdx, depth, partsInfo);
                }
                if (bestCost > cost) {
                    bestMeInfo[0] = partsInfo[0];
                    bestMeInfo[1] = partsInfo[1];
                    bestCost = cost;

                    if (m_par->puDecisionSatd) {
                        nonZeroCbf = bestCost != 0;
                    }
                    else {
                        if (m_par->fastCbfMode)
                            nonZeroCbf = m_data[absPartIdx].cbf[0] | m_data[absPartIdx].cbf[1] | m_data[absPartIdx].cbf[2];
                        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                        small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                        m_data[absPartIdx].curIdx ^= 1;
                        SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)
                    }
                }
            }
        }
        // PART_SIZE_nLx2N
        if (nonZeroCbf) {
            H265MEInfo partsInfo[2] = {meInfo, meInfo};
            partsInfo[0].width = meInfo.width / 4;
            partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 4);
            partsInfo[1].width = meInfo.width * 3 / 4;
            partsInfo[1].posx  = meInfo.posx + partsInfo[0].width;
            partsInfo[0].splitMode = PART_SIZE_nLx2N;
            partsInfo[1].splitMode = PART_SIZE_nLx2N;

            GetMergeCand(absPartIdx, PART_SIZE_nLx2N, 0, CUSizeInMinTU, m_mergeCand + 0);
            GetAmvpCand(absPartIdx, PART_SIZE_nLx2N, 0, m_amvpCand[0]);
            MePu(partsInfo + 0, 0);
            Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
            GetMergeCand(absPartIdx, PART_SIZE_nLx2N, 1, CUSizeInMinTU, m_mergeCand + 1);
            GetAmvpCand(absPartIdx, PART_SIZE_nLx2N, 1, m_amvpCand[1]);
            MePu(partsInfo + 1, interPredIdx);

            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                if (m_par->puDecisionSatd) {
                    cost = PuCost(partsInfo + 0) + PuCost(partsInfo + 1);
                }
                else {
                    m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                    cost = CuCost(absPartIdx, depth, partsInfo);
                }
                if (bestCost > cost) {
                    bestMeInfo[0] = partsInfo[0];
                    bestMeInfo[1] = partsInfo[1];
                    bestCost = cost;

                    if (m_par->puDecisionSatd) {
                        nonZeroCbf = bestCost != 0;
                    } else {
                        if (m_par->fastCbfMode)
                            nonZeroCbf = m_data[absPartIdx].cbf[0] | m_data[absPartIdx].cbf[1] | m_data[absPartIdx].cbf[2];
                        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                        small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                        m_data[absPartIdx].curIdx ^= 1;
                        SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)
                    }
                }
            }
        }
    }

    m_data[absPartIdx].curIdx ^= 1; // set to best
    SWAP_REC_IDX( m_data[absPartIdx].curRecIdx,  m_data[absPartIdx].bestRecIdx)

    if (m_par->puDecisionSatd) {
        bestCost = bestCostMergeSkip;
        m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        cost = CuCost(absPartIdx, depth, bestMeInfo);
        if (bestCost > cost) {
            bestCost = cost;
            small_memcpy(m_dataInter, m_data + absPartIdx, sizeof(H265CUData) * numParts);
        }
        else {
            m_bsf->CtxRestore(ctxSave[1], 0, NUM_CABAC_CONTEXT);
            small_memcpy(m_data + absPartIdx, m_dataInter, sizeof(H265CUData) * numParts);
        }
        return bestCost;
    }
    else {
        m_bsf->CtxRestore(ctxSave[1], 0, NUM_CABAC_CONTEXT);
        small_memcpy(m_data + absPartIdx, m_dataInter, sizeof(H265CUData) * numParts);
        return bestCost;
    }
}

void H265CU::TuGetSplitInter(Ipp32u absPartIdx, Ipp32s offset, Ipp8u tr_idx, Ipp8u trIdxMax, Ipp8u nz[3], CostType *cost, Ipp8u cbf[256][3])
{
    Ipp8u depth = m_data[absPartIdx].depth;
    Ipp32s numParts = ( m_par->NumPartInCU >> ((depth + tr_idx)<<1) ); // in TU
    Ipp32s width = m_data[absPartIdx].size >>tr_idx;

    CostType costBest;
    CABAC_CONTEXT_H265 ctxSave[2][NUM_CABAC_CONTEXT];

    for (Ipp32s i = 0; i < numParts; i++) m_data[absPartIdx + i].trIdx = tr_idx;
    m_bsf->CtxSave(ctxSave[0], 0, NUM_CABAC_CONTEXT);
    EncAndRecLumaTu(absPartIdx, offset, width, &nz[0], &costBest, 0, INTER_PRED_IN_BUF);
    Ipp8u hasNz = nz[0];
    CostType costChroma = 0;
    if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && width > 4) {
        EncAndRecChromaTu(absPartIdx, offset>>2, width>>1, &nz[1], &costChroma, INTER_PRED_IN_BUF);
        costBest += costChroma;
        hasNz |= (nz[1] | nz[2]);
    } else
        nz[1] = nz[2] = 0;

    CostType cuSplitThresholdTu = m_par->cu_split_threshold_tu[m_data[absPartIdx].qp][1][depth+tr_idx];

    if (costBest >= cuSplitThresholdTu && tr_idx  < trIdxMax && hasNz) { // don't try if all zero
        Ipp32s offsetLuma = GetLumaOffset(m_par, absPartIdx, m_pitchRec);
        Ipp32s offsetChroma = GetChromaOffset(m_par, absPartIdx, m_pitchRec);
        Ipp32s offsetPred = GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
        Ipp32s offsetPredChroma = GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE);
        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
        m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        // keep not split
        H265CUData *data_t = m_dataTemp + ((depth + tr_idx) << m_par->Log2NumPartInCU);
        small_memcpy(data_t + absPartIdx, m_data + absPartIdx, sizeof(H265CUData) * numParts);
        IppiSize roi = { width, width };
        ippiCopy_8u_C1R(m_interRec[m_data[absPartIdx].curRecIdx][ depth ] + offsetPred, MAX_CU_SIZE, m_interRecBest[depth + tr_idx], MAX_CU_SIZE, roi);
        if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
            IppiSize roi = { width, width>>1 };
            ippiCopy_8u_C1R(m_interRecChroma[m_data[absPartIdx].curRecIdx][ depth ] + offsetPredChroma, MAX_CU_SIZE, m_interRecBestChroma[depth + tr_idx], MAX_CU_SIZE, roi);
        }

        CostType cost_temp, cost_split = 0;
        Ipp32s subsize = width /*<< (trIdxMax - tr_idx)*/ >> 1;
        subsize *= subsize;
        Ipp32s numParts4 = numParts >> 2;
        Ipp8u nzt[3] = {0,};
        for (Ipp32s i = 0; i < 4; i++) {
            Ipp8u nz_loc[3];
            TuGetSplitInter(absPartIdx + numParts4 * i, offset + subsize * i, tr_idx + 1,
                             trIdxMax, nz_loc, &cost_temp, cbf);
            nzt[0] |= nz_loc[0];
            nzt[1] |= nz_loc[1];
            nzt[2] |= nz_loc[2];
            cost_split += cost_temp;
        }

        if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && width == 8) {
            cost_split += costChroma; // take from lower depth
            nzt[1] |= nz[1];
            nzt[2] |= nz[2];
        }

        if (costBest > cost_split) { // count split flag if split is still better
            m_bsf->Reset();
            Ipp8u code_dqp = getdQPFlag();
            PutTransform(m_bsf, offset, offset>>2, absPartIdx, absPartIdx,
                          m_data[absPartIdx].depth + m_data[absPartIdx].trIdx,
                          width, width, m_data[absPartIdx].trIdx, code_dqp, 1);

            cost_split += BIT_COST_INTER(m_bsf->GetNumBits());
        }

#ifdef DUMP_COSTS_TU
        printCostStat(fp_tu, m_par->QP, 1, width,costBest,cost_split);
#endif

        if (costBest > cost_split) {
            costBest = cost_split;
            nz[0] = nzt[0];
            nz[1] = nzt[1];
            nz[2] = nzt[2];
        } else {
            // restore not split
            small_memcpy(m_data + absPartIdx, data_t + absPartIdx, sizeof(H265CUData) * numParts);
            m_bsf->CtxRestore(ctxSave[1], 0, NUM_CABAC_CONTEXT);
            ippiCopy_8u_C1R(m_interRecBest[depth + tr_idx], MAX_CU_SIZE, m_interRec[m_data[absPartIdx].curRecIdx][ depth ] + offsetPred, MAX_CU_SIZE, roi);
            if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
                IppiSize roi = { width, width>>1 };
                ippiCopy_8u_C1R(m_interRecBestChroma[depth + tr_idx], MAX_CU_SIZE, m_interRecChroma[m_data[absPartIdx].curRecIdx][ depth ] + offsetPredChroma, MAX_CU_SIZE, roi);
            }
        }

    }
    for (Ipp32s i = 0; i < 3; i++) {
        if (nz[i])
            for (Ipp32s n = 0; n < numParts; n++)
                cbf[absPartIdx+n][i] |= (1 << tr_idx);
        else
            for (Ipp32s n = 0; n < numParts; n++)
                cbf[absPartIdx+n][i] &= ~(1 << tr_idx);
    }
    if(cost) *cost = costBest;
}

void H265CU::TuMaxSplitInter(Ipp32u absPartIdx, Ipp8u trIdxMax, CostType *cost, Ipp8u cbf[256][3])
{
    Ipp8u depth = m_data[absPartIdx].depth;
    Ipp32s offset = absPartIdx * 16;
    Ipp32u numParts = ( m_par->NumPartInCU >> ((depth + 0)<<1) ); // in TU
    Ipp32u num_tr_parts = ( m_par->NumPartInCU >> ((depth + trIdxMax)<<1) );
    Ipp32s width = m_data[absPartIdx].size >>trIdxMax;

    CABAC_CONTEXT_H265 ctxSave[2][NUM_CABAC_CONTEXT];
    CostType costSum = 0;
    Ipp8u nz[3] = {0,0,0};
    Ipp8u nzt[3];

    for (Ipp32u i = 0; i < numParts; i++) m_data[absPartIdx + i].trIdx = trIdxMax;
    m_bsf->CtxSave(ctxSave[0], 0, NUM_CABAC_CONTEXT);

    for (Ipp32u pos = 0; pos < numParts; ) {
        CostType costTmp;

        EncAndRecLumaTu(absPartIdx + pos, (absPartIdx + pos)*16, width, &nzt[0], &costTmp, 0, INTER_PRED_IN_BUF);
        nz[0] |= nzt[0];

        costSum += costTmp;
        pos += num_tr_parts;
    }

    if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
        if (width == 4) {
            num_tr_parts <<= 2;
            width <<= 1;
        }
        for (Ipp32u pos = 0; pos < numParts; ) {
            CostType costTmp;

            EncAndRecChromaTu(absPartIdx + pos, ((absPartIdx + pos)*16)>>2, width>>1, &nzt[1], &costTmp, INTER_PRED_IN_BUF);
            nz[1] |= nzt[1];
            nz[2] |= nzt[2];

            costSum += costTmp;
            pos += num_tr_parts;
        }
    } else
        nz[1] = nz[2] = 0;

    for (Ipp32s i = 0; i < 3; i++) {
        if (nz[i])
            for (Ipp32u n = 0; n < numParts; n++)
                cbf[absPartIdx+n][i] |= (1 << trIdxMax);
        else
            for (Ipp32u n = 0; n < numParts; n++)
                cbf[absPartIdx+n][i] &= ~(1 << trIdxMax);
    }

    if (trIdxMax > 0) {
        Ipp8u val = (1 << trIdxMax) - 1;
        for (Ipp32s i = 0; i < 3; i++) {
            nz[i] ? cbf[absPartIdx][i] |= val
                    : cbf[absPartIdx][i] = 0;
        }
    }

    if(cost) *cost = costSum;
}

// Cost for CU
CostType H265CU::CuCost(Ipp32u absPartIdx, Ipp8u depth, const H265MEInfo *bestInfo)
{
    Ipp8u bestSplitMode = bestInfo[0].splitMode;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    CostType bestCost = 0;

    Ipp32u xborder = bestInfo[0].posx + bestInfo[0].width;
    Ipp32u yborder = bestInfo[0].posy + bestInfo[0].height;

    Ipp8u tr_depth_min, tr_depth_max;
    const Ipp8u curIdx = m_data[absPartIdx].curIdx; 
    const Ipp8u curRecIdx = m_data[absPartIdx].curRecIdx; 
    const Ipp8u bestRecIdx = m_data[absPartIdx].bestRecIdx; 
    GetTrDepthMinMax(m_par, absPartIdx, depth, bestSplitMode, &tr_depth_min, &tr_depth_max);

    for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
        Ipp32u posx = (h265_scan_z2r[m_par->MaxCUDepth][i] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize;
        Ipp32u posy = (h265_scan_z2r[m_par->MaxCUDepth][i] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize;
        Ipp32s partNxN = (posx<xborder ? 0 : 1) + (posy<yborder ? 0 : 2);
        Ipp32s part = (bestSplitMode != PART_SIZE_NxN) ? (partNxN ? 1 : 0) : partNxN;
        const H265MEInfo* mei = &bestInfo[part];

        VM_ASSERT(mei->interDir >= 1 && mei->interDir <= 3);
        m_data[i].partSize = bestSplitMode;
        m_data[i].cbf[0] =  m_data[i].cbf[1] =  m_data[i].cbf[2] = 0;
        m_data[i].flags.skippedFlag = 0;
        m_data[i].interDir = mei->interDir;
        m_data[i].refIdx[0] = mei->refIdx[0];
        m_data[i].refIdx[1] = mei->refIdx[1];
        m_data[i].mv[0] = mei->MV[0];
        m_data[i].mv[1] = mei->MV[1];
        m_data[i].curIdx = curIdx;
        m_data[i].curRecIdx = curRecIdx; 
        m_data[i].bestRecIdx = bestRecIdx;
    }

    // set mv
    Ipp32s numPu = h265_numPu[m_data[absPartIdx].partSize];
    for (Ipp32s i = 0; i < numPu; i++) {
        Ipp32s partAddr;
        GetPartAddr(i, m_data[absPartIdx].partSize, numParts, partAddr);
        SetupMvPredictionInfo(absPartIdx + partAddr, m_data[absPartIdx].partSize, i);
    }

    if (m_rdOptFlag) {
        Ipp8u nzt[3], nz[3] = {0};
        Ipp8u cbf[256][3];

        if (!m_interPredReady) {
            InterPredCu<TEXT_LUMA>(absPartIdx, depth, m_interPred[m_data[absPartIdx].curIdx][depth], MAX_CU_SIZE);
            if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
                InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredChroma[m_data[absPartIdx].curIdx][depth], MAX_CU_SIZE);

            Ipp32s offsetPred = GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
            Ipp32s offsetPredChroma = GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE);
            PixType *pSrc = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchSrc);
            PixType *pSrcUv = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchSrc);

            TuDiff(m_interResidualsY[m_data[absPartIdx].curIdx][depth] + offsetPred, MAX_CU_SIZE, pSrc, m_pitchSrc, m_interPred[m_data[absPartIdx].curIdx][depth] + offsetPred, MAX_CU_SIZE, m_data[absPartIdx].size);
            if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
                TuDiffNv12(m_interResidualsU[m_data[absPartIdx].curIdx][depth] + offsetPredChroma/2, MAX_CU_SIZE/2, pSrcUv, m_pitchSrc, m_interPredChroma[m_data[absPartIdx].curIdx][depth] + offsetPredChroma, MAX_CU_SIZE, m_data[absPartIdx].size/2);
                TuDiffNv12(m_interResidualsV[m_data[absPartIdx].curIdx][depth] + offsetPredChroma/2, MAX_CU_SIZE/2, pSrcUv+1, m_pitchSrc, m_interPredChroma[m_data[absPartIdx].curIdx][depth] + offsetPredChroma + 1, MAX_CU_SIZE, m_data[absPartIdx].size/2);
            }
        }

        memset(&cbf[absPartIdx], 0, numParts*sizeof(cbf[0]));
        for (Ipp32u pos = 0; pos < numParts; ) {
            CostType cost;
            Ipp32u num_tr_parts = ( m_par->NumPartInCU >> ((depth + tr_depth_min)<<1) );

            TuGetSplitInter(absPartIdx + pos, (absPartIdx + pos)*16, tr_depth_min, tr_depth_max, nzt, &cost, cbf);
            nz[0] |= nzt[0];
            nz[1] |= nzt[1];
            nz[2] |= nzt[2];

            bestCost += cost;
            pos += num_tr_parts;
        }

        if (tr_depth_min > 0) {
            Ipp8u val = (1 << tr_depth_min) - 1;
            for (Ipp32s i = 0; i < 3; i++) {
                nz[i] ? cbf[absPartIdx][i] |= val
                      : cbf[absPartIdx][i] = 0;
            }
        }

        for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
            m_data[i].cbf[0] = cbf[i][0];
            m_data[i].cbf[1] = cbf[i][1];
            m_data[i].cbf[2] = cbf[i][2];
        }

        m_bsf->Reset();
        EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
        bestCost += BIT_COST_INTER(m_bsf->GetNumBits());
    }

    return bestCost;
}


// Cyclic pattern to avoid trying already checked positions
const Ipp16s tab_mePattern[1 + 8 + 3][2] = {
    {0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}
};
const struct TransitionTable {
    Ipp8u start;
    Ipp8u len;
} tab_meTransition[1 + 8 + 3] = {
    {1,8}, {7,5}, {1,3}, {1,5}, {3,3}, {3,5}, {5,3}, {5,5}, {7,3}, {7,5}, {1,3}, {1,5}
};

void H265CU::MeIntPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, Ipp32s list,
                      Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    if (m_par->patternIntPel == 1) {
        return MeIntPelLog(meInfo, predInfo, list, refIdx, mv, cost, mvCost);
    }
    else if (m_par->patternIntPel == 100) {
        return MeIntPelFullSearch(meInfo, predInfo, list, refIdx, mv, cost, mvCost);
    }
    else {
        VM_ASSERT(0);
    }
}


void H265CU::MeIntPelFullSearch(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, Ipp32s list,
                                Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    const Ipp32s RANGE = 4 * 64;

    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;
    H265Frame *ref = m_currFrame->m_refPicList[list].m_refFrames[refIdx];
    Ipp32s useHadamard = (m_par->hadamardMe == 3);

    H265MV mvCenter = mvBest;
    for (Ipp32s dy = -RANGE; dy <= RANGE; dy += 4) {
        for (Ipp32s dx = -RANGE; dx <= RANGE; dx += 4) {
            H265MV mv = {
                static_cast<Ipp16s>(mvCenter.mvx + dx),
                static_cast<Ipp16s>(mvCenter.mvy + dy)
            };
            if (ClipMV(mv))
                continue;
            Ipp32s mvCost = MvCost1RefLog(mv, (Ipp8s)refIdx, predInfo, list);
            Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard) + mvCost;
            if (costBest > cost) {
                costBest = cost;
                mvCostBest = mvCost;
                mvBest = mv;
            }
        }
    }

    *mv = mvBest;
    *cost = costBest;
    *mvCost = mvCostBest;
}


const Ipp16s tab_mePatternSelector[2][12][2] = {
    {{0,0}, {0,-1},  {1,0},  {0,1},  {-1,0}, {0,0}, {0,0}, {0,0},  {0,0},  {0,0},   {0,0},  {0,0}}, //diamond
    {{0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0},  {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}} //box
};

void H265CU::MeIntPelLog(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, Ipp32s list,
                         Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    Ipp16s meStepBest = 4;
    Ipp16s meStepMax = MAX(MIN(meInfo->width, meInfo->height), 16) * 4;
    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;
    H265Frame *ref = m_currFrame->m_refPicList[list].m_refFrames[refIdx];
    Ipp32s useHadamard = (m_par->hadamardMe == 3);

    // expanding search
    H265MV mvCenter = mvBest;
    for (Ipp16s meStep = meStepBest; meStep <= meStepMax; meStep *= 2) {
        for (Ipp16s mePos = 1; mePos < 9; mePos++) {
            H265MV mv = {
                static_cast<Ipp16s>(mvCenter.mvx + tab_mePattern[mePos][0] * meStep),
                static_cast<Ipp16s>(mvCenter.mvy + tab_mePattern[mePos][1] * meStep)
            };
            if (ClipMV(mv))
                continue;
            Ipp32s mvCost = MvCost1RefLog(mv, (Ipp8s)refIdx, predInfo, list);
            Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard) + mvCost;
            if (costBest > cost) {
                costBest = cost;
                mvCostBest = mvCost;
                mvBest = mv;
                meStepBest = meStep;
            }
        }
    }

    // then logarithm from best
    // for Cpattern
    Ipp8u start = 0, len = 1;
    Ipp16s meStep = meStepBest;
    mvCenter = mvBest;
    while (meStep >= 4) {
        Ipp32s refine = 1;
        Ipp32s bestPos = 0;
        for (Ipp16s mePos = start; mePos < start + len; mePos++) {
            H265MV mv = {
                static_cast<Ipp16s>(mvCenter.mvx + tab_mePattern[mePos][0] * meStep),
                static_cast<Ipp16s>(mvCenter.mvy + tab_mePattern[mePos][1] * meStep)
            };
            if (ClipMV(mv))
                continue;
            Ipp32s mvCost = MvCost1RefLog(mv, (Ipp8s)refIdx, predInfo, list);
            Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard) + mvCost;
            if (costBest > cost) {
                costBest = cost;
                mvCostBest = mvCost;
                mvBest = mv;
                refine = 0;
                bestPos = mePos;
            }
        }

        if (refine) 
            meStep >>= 1;
        else
            mvCenter = mvBest;

        start = tab_meTransition[bestPos].start;
        len   = tab_meTransition[bestPos].len;
    }

    *mv = mvBest;
    *cost = costBest;
    *mvCost = mvCostBest;
}


void H265CU::MeSubPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, Ipp32s meDir,
                      Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    H265MV mvCenter = *mv;
    H265MV mvBest = *mv;
    Ipp32s startPos = 1;
    Ipp32s meStep = 2;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;

    Ipp32s endPos;
    Ipp16s pattern_index;
    Ipp32s iterNum = 1;

    // select subMe pattern
    switch(m_par->patternSubPel)
    {
        case 1:               // int pel only
            return; 
        case 2:               // more points with square patterns, no quarter-pel
            endPos = 9;
            pattern_index = 1;
            break;
        case 3:               // more points with square patterns
            endPos = 9;
            pattern_index = 1;
            break;
        case 4:               // quarter pel with simplified diamond pattern - single
            endPos = 5;
            pattern_index = 0;
            break;
        case 5:               // quarter pel with simplified diamond pattern - double
            endPos = 5;
            pattern_index = 0;
            iterNum = 2;
            break;
        default:
            endPos = 5;
            pattern_index = 0;
            break;
     }

    H265Frame *ref = m_currFrame->m_refPicList[meDir].m_refFrames[refIdx];
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;

    Ipp32s useHadamard = (m_par->hadamardMe >= 2);
    if (m_par->hadamardMe == 2) {
        // when satd is for subpel only need to recalculate cost for intpel motion vector
        costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard) + mvCostBest;
    }

    while (meStep) {

        H265MV bestMv = mvCenter;

        for( Ipp32s iter = 0; iter < iterNum; iter++) {

            Ipp32s bestPos = 0;
            for (Ipp32s mePos = startPos; mePos < endPos; mePos++) {
                H265MV mv = {
                    static_cast<Ipp16s>(bestMv.mvx + tab_mePatternSelector[pattern_index][mePos][0] * meStep),
                    static_cast<Ipp16s>(bestMv.mvy + tab_mePatternSelector[pattern_index][mePos][1] * meStep)
                };
                if (ClipMV(mv))
                    continue;
                Ipp32s mvCost = MvCost1RefLog(mv, (Ipp8s)refIdx, predInfo, meDir);
                Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard) + mvCost;
                if (costBest > cost) {
                    costBest = cost;
                    mvCostBest = mvCost;
                    mvBest = mv;
                    bestPos = mePos;
                }
            }

            bestMv = mvBest;
        }
        if(m_par->patternSubPel== 2) break; //no quarter pel
        mvCenter = mvBest;
        meStep >>= 1;
        startPos = 1;
    }

    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;
}

static Ipp32s GetFlBits(Ipp32s val, Ipp32s maxVal)
{
    if (maxVal < 2)
        return 0;
    else if (val == maxVal - 1)
        return val;
    else
        return val + 1;
}

void GetPredIdxBits(Ipp32s partMode, Ipp32s isP, int partIdx, Ipp32s lastPredIdx, Ipp32s predIdxBits[3])
{
    VM_ASSERT(partMode <= PART_SIZE_nRx2N);

    if (isP) {
        if (partMode == PART_SIZE_2Nx2N || partMode == PART_SIZE_NxN) {
            predIdxBits[0] = 1;
            predIdxBits[1] = 3;
            predIdxBits[2] = 5;
        }
        else {
            predIdxBits[0] = 3;
            predIdxBits[1] = 0;
            predIdxBits[2] = 0;
        }
    }
    else {
        if (partMode == PART_SIZE_2Nx2N) {
            predIdxBits[0] = 3;
            predIdxBits[1] = 3;
            predIdxBits[2] = 5;
        }
        else if (partMode == PART_SIZE_2NxN || partMode == PART_SIZE_2NxnU || partMode == PART_SIZE_2NxnD) {
            static const Ipp32s tab_predIdxBits[2][3][3] = {
                { { 0, 0, 3 }, { 0, 0, 0 }, { 0, 0, 0 } },
                { { 5, 7, 7 }, { 7, 5, 7 }, { 9 - 3, 9 - 3, 9 - 3 } }
            };
            small_memcpy(predIdxBits, tab_predIdxBits[partIdx][lastPredIdx], sizeof(Ipp32s) * 3);
        }
        else if (partMode == PART_SIZE_Nx2N || partMode == PART_SIZE_nLx2N || partMode == PART_SIZE_nRx2N) {
            static const Ipp32s tab_predIdxBits[2][3][3] = {
                { { 0, 2, 3 }, { 0, 0, 0 }, { 0, 0, 0 } },
                { { 5, 7, 7 }, { 7 - 2, 7 - 2, 9 - 2 }, { 9 - 3, 9 - 3, 9 - 3 } }
            };
            small_memcpy(predIdxBits, tab_predIdxBits[partIdx][lastPredIdx], sizeof(Ipp32s) * 3);
        }
        else if (partMode == PART_SIZE_NxN) {
            predIdxBits[0] = 3;
            predIdxBits[1] = 3;
            predIdxBits[2] = 5;
        }
    }
}

Ipp32s H265CU::PuCost(H265MEInfo *meInfo) {
    RefPicList *refPicList = m_currFrame->m_refPicList;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;
    Ipp32s cost = INT_MAX;
    if (meInfo->refIdx[0] >= 0 && meInfo->refIdx[1] >= 0) {
        cost = MatchingMetricBipredPu(src, meInfo, meInfo->refIdx, meInfo->MV, 1);
    }
    else {
        Ipp32s list = (meInfo->refIdx[1] >= 0);
        H265Frame *ref = refPicList[list].m_refFrames[meInfo->refIdx[list]];
        cost = MatchingMetricPu(src, meInfo, meInfo->MV + list, ref, 1);
    }
    return cost;
}

void H265CU::MePu(H265MEInfo *meInfo, Ipp32s lastPredIdx)
{
#if defined (MFX_ENABLE_CM)
    if (m_par->enableCmFlag) {
        return MePuGacc(meInfo, lastPredIdx);
    }
#endif // MFX_ENABLE_CM

    RefPicList *refPicList = m_currFrame->m_refPicList;
    Ipp32u numParts = (m_par->NumPartInCU >> (meInfo->depth << 1));
    Ipp32s blockIdx = meInfo->absPartIdx &~ (numParts - 1);
    Ipp32s partAddr = meInfo->absPartIdx &  (numParts - 1);
    Ipp32s curPUidx = (meInfo->splitMode == PART_SIZE_NxN) ? (partAddr / (numParts >> 2)) : !!partAddr;

    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;

    H265MV mvRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s costRefBest[2][MAX_NUM_REF_IDX] = { { INT_MAX }, { INT_MAX } };
    Ipp32s mvCostRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s bitsRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp8s  refIdxBest[2] = {};

    Ipp32s predIdxBits[3];
    GetPredIdxBits(meInfo->splitMode, (m_cslice->slice_type == P_SLICE), curPUidx, lastPredIdx, predIdxBits);

    Ipp32s numLists = (m_cslice->slice_type == B_SLICE) + 1;
    for (Ipp32s list = 0; list < numLists; list++) {
        Ipp32s numRefIdx = m_cslice->num_ref_idx[list];

        for (Ipp8s refIdx = 0; refIdx < numRefIdx; refIdx++) {
            costRefBest[list][refIdx] = INT_MAX;

            if (list == 1) { // TODO: use pre-build table of duplicates instead of std::find
                Ipp32s idx0 = m_currFrame->m_mapRefIdxL1ToL0[refIdx];
                if (idx0 >= 0) {
                    // don't do ME just re-calc costs
                    mvRefBest[1][refIdx] = mvRefBest[0][idx0];
                    mvCostRefBest[1][refIdx] = MvCost1RefLog(mvRefBest[0][idx0], refIdx, m_amvpCand[curPUidx], list);
                    bitsRefBest[1][refIdx] = MVP_LX_FLAG_BITS;
                    bitsRefBest[1][refIdx] += GetFlBits(refIdx, numRefIdx);
                    bitsRefBest[1][refIdx] += predIdxBits[1];
                    costRefBest[1][refIdx] = costRefBest[0][idx0] - mvCostRefBest[0][idx0];
                    costRefBest[1][refIdx] -= (Ipp32s)(bitsRefBest[0][idx0] * m_cslice->rd_lambda_sqrt + 0.5);
                    costRefBest[1][refIdx] += mvCostRefBest[1][refIdx];
                    costRefBest[1][refIdx] += (Ipp32s)(bitsRefBest[1][refIdx] * m_cslice->rd_lambda_sqrt + 0.5);
                    if (costRefBest[1][refIdxBest[1]] > costRefBest[1][refIdx])
                        refIdxBest[1] = refIdx;
                    continue;
                }
            }

            // use satd for all candidates even if satd is for subpel only
            Ipp32s useHadamard = (m_par->hadamardMe == 3);

            Ipp32s costBest = INT_MAX;
            H265MV mvBest = { 0, 0 };
            MvPredInfo<2> *amvp = m_amvpCand[curPUidx] + refIdx * 2 + list;
            H265Frame *ref = refPicList[list].m_refFrames[refIdx];
            for (Ipp32s i = 0; i < amvp->numCand; i++) {
                H265MV mv = amvp->mvCand[i];
                if (std::find(amvp->mvCand, amvp->mvCand + i, amvp->mvCand[i]) != amvp->mvCand + i)
                    continue; // skip duplicate
                ClipMV(mv);
                Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                if (costBest > cost) {
                    costBest = cost;
                    mvBest = mv;
                }
            }

            Ipp32s mvCostBest = 0;
            if ((mvBest.mvx | mvBest.mvy) & 3) {
                // recalculate cost for nearest int-pel
                mvBest.mvx = (mvBest.mvx + 1) & ~3;
                mvBest.mvy = (mvBest.mvy + 1) & ~3;
                costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard);
                mvCostBest = MvCost1RefLog(mvBest, refIdx, m_amvpCand[curPUidx], list);
                costBest += mvCostBest;
            }
            else {
                // add cost of zero mvd
                mvCostBest = 2 * (Ipp32s)(1 * m_cslice->rd_lambda_sqrt + 0.5);
                costBest += mvCostBest;
            }

            MeIntPel(meInfo, m_amvpCand[curPUidx], list, refIdx, &mvBest, &costBest, &mvCostBest);
            MeSubPel(meInfo, m_amvpCand[curPUidx], list, refIdx, &mvBest, &costBest, &mvCostBest);

            mvRefBest[list][refIdx] = mvBest;
            mvCostRefBest[list][refIdx] = mvCostBest;
            costRefBest[list][refIdx] = costBest;
            bitsRefBest[list][refIdx] = MVP_LX_FLAG_BITS + predIdxBits[list];
            bitsRefBest[list][refIdx] += GetFlBits(refIdx, numRefIdx);
            costRefBest[list][refIdx] += (Ipp32s)(bitsRefBest[list][refIdx] * m_cslice->rd_lambda_sqrt + 0.5);

            if (costRefBest[list][refIdxBest[list]] > costRefBest[list][refIdx])
                refIdxBest[list] = refIdx;
        }
    }

    Ipp8s  idxL0 = refIdxBest[0];
    Ipp8s  idxL1 = refIdxBest[1];
    Ipp32s costList[2] = { costRefBest[0][idxL0], costRefBest[1][idxL1] };
    Ipp32s costBiBest = INT_MAX;

    H265MV mvBiBest[2] = { mvRefBest[0][idxL0], mvRefBest[1][idxL1] };

    H265Frame *refF = refPicList[0].m_refFrames[idxL0];
    H265Frame *refB = refPicList[1].m_refFrames[idxL1];
    if (m_cslice->slice_type == B_SLICE && meInfo->width + meInfo->height != 12 && refF && refB) {
        // use satd for bidir refine because it is always sub-pel
        Ipp32s useHadamard = (m_par->hadamardMe >= 2);

        Ipp32s mvCostBiBest = mvCostRefBest[0][idxL0] + mvCostRefBest[1][idxL1];
        costBiBest = mvCostBiBest;
        costBiBest += MatchingMetricBipredPu(src, meInfo, refIdxBest, mvBiBest, useHadamard);

        // refine Bidir
        if (IPP_MIN(costList[0], costList[1]) * 9 > 8 * costBiBest) {

            bool changed = true;
            for (Ipp32s iter = 0; iter < m_par->numBiRefineIter - 1 && changed; iter++) {
                H265MV mvCenter[2] = { mvBiBest[0], mvBiBest[1] };
                changed = false;
                Ipp32s count = (refF == refB && mvBiBest[0] == mvBiBest[1]) ? 2 * 2 : 2 * 2 * 2;
                for (Ipp32s i = 0; i < count; i++) {
                    H265MV mv[2] = { mvCenter[0], mvCenter[1] };
                    if (i & 2)
                        mv[i >> 2].mvx += (i & 1) ? 1 : -1;
                    else
                        mv[i >> 2].mvy += (i & 1) ? 1 : -1;

                    Ipp32s mvCost = MvCost1RefLog(mv[0], idxL0, m_amvpCand[curPUidx], 0) +
                                    MvCost1RefLog(mv[1], idxL1, m_amvpCand[curPUidx], 1);
                    Ipp32s cost = MatchingMetricBipredPu(src, meInfo, refIdxBest, mv, useHadamard);
                    cost += mvCost;

                    if (costBiBest > cost) {
                        costBiBest = cost;
                        mvCostBiBest = mvCost;
                        mvBiBest[0] = mv[0];
                        mvBiBest[1] = mv[1];
                        changed = true;
                    }
                }
            }

            Ipp32s bitsBiBest = MVP_LX_FLAG_BITS + predIdxBits[2];
            bitsBiBest += GetFlBits(idxL0, m_cslice->num_ref_idx[0]);
            bitsBiBest += GetFlBits(idxL1, m_cslice->num_ref_idx[1]);
            costBiBest += (Ipp32s)(bitsBiBest * m_cslice->rd_lambda_sqrt + 0.5);
        }
    }

    Ipp32s useHadamard = (m_par->hadamardMe >= 2);

    Ipp32s bestMergeCost = INT_MAX;
    H265MV bestMergeMv[2] = {};
    Ipp8s  bestMergeRef[2] = { -1, -1 };
    if (!(m_par->fastSkip && meInfo->splitMode == PART_SIZE_2Nx2N)) {
        for (Ipp32s mergeIdx = 0; mergeIdx < m_mergeCand[curPUidx].numCand; mergeIdx++) {
            Ipp8s *refIdx = m_mergeCand[curPUidx].refIdx + 2 * mergeIdx;
            if (refIdx[0] < 0 && refIdx[1] < 0)
                continue;
            H265MV *mv = m_mergeCand[curPUidx].mvCand + 2 * mergeIdx;

            Ipp32s cost = INT_MAX;
            Ipp8u interDir = 0;
            interDir += (refIdx[0] >= 0) ? INTER_DIR_PRED_L0 : 0;
            interDir += (refIdx[1] >= 0) ? INTER_DIR_PRED_L1 : 0;
            if (refIdx[0] >= 0 && refIdx[1] >= 0) {
                H265Frame *refF = refPicList[0].m_refFrames[refIdx[0]];
                H265Frame *refB = refPicList[1].m_refFrames[refIdx[1]];
                cost = MatchingMetricBipredPu(src, meInfo, refIdx, mv, useHadamard);
            }
            else {
                Ipp32s list = (refIdx[1] >= 0);
                H265Frame *ref = refPicList[list].m_refFrames[refIdx[list]];
                cost = MatchingMetricPu(src, meInfo, mv + list, ref, useHadamard);
            }

            cost += (Ipp32s)(GetFlBits(mergeIdx, m_mergeCand[curPUidx].numCand) * m_cslice->rd_lambda_sqrt + 0.5);

            if (bestMergeCost >= cost) {
                bestMergeCost = cost;
                bestMergeRef[0] = refIdx[0];
                bestMergeRef[1] = refIdx[1];
                bestMergeMv[0] = mv[0];
                bestMergeMv[1] = mv[1];
            }
        }
    }

    if (bestMergeCost <= costList[0] && bestMergeCost <= costList[1] && bestMergeCost <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L0 * (bestMergeRef[0] >= 0) + INTER_DIR_PRED_L1 * (bestMergeRef[1] >= 0);
        meInfo->refIdx[0] = bestMergeRef[0];
        meInfo->refIdx[1] = bestMergeRef[1];
        meInfo->MV[0] = bestMergeMv[0];
        meInfo->MV[1] = bestMergeMv[1];
    }
    else if (costList[0] <= costList[1] && costList[0] <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L0;
        meInfo->refIdx[0] = (Ipp8s)refIdxBest[0];
        meInfo->refIdx[1] = -1;
        meInfo->MV[0] = mvRefBest[0][idxL0];
        meInfo->MV[1] = MV_ZERO;
    }
    else if (costList[1] <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L1;
        meInfo->refIdx[0] = -1;
        meInfo->refIdx[1] = (Ipp8s)refIdxBest[1];
        meInfo->MV[0] = MV_ZERO;
        meInfo->MV[1] = mvRefBest[1][idxL1];
    }
    else {
        meInfo->interDir = INTER_DIR_PRED_L0 + INTER_DIR_PRED_L1;
        meInfo->refIdx[0] = (Ipp8s)refIdxBest[0];
        meInfo->refIdx[1] = (Ipp8s)refIdxBest[1];
        meInfo->MV[0] = mvBiBest[0];
        meInfo->MV[1] = mvBiBest[1];
    }

    Ipp32s xstart = meInfo->posx >> m_par->QuadtreeTULog2MinSize;
    Ipp32s ystart = meInfo->posy >> m_par->QuadtreeTULog2MinSize;
    Ipp32s xsize = meInfo->width >> m_par->QuadtreeTULog2MinSize;
    Ipp32s ysize = meInfo->height >> m_par->QuadtreeTULog2MinSize;
    for (Ipp32s y = ystart; y < ystart + ysize; y++) {
        for (Ipp32s x = xstart; x < xstart + xsize; x++) {
            Ipp32s rorder = y * m_par->NumMinTUInMaxCU + x;
            Ipp32s zorder = h265_scan_r2z[m_par->MaxCUDepth][rorder];
            m_data[zorder].mv[0] = meInfo->MV[0];
            m_data[zorder].mv[1] = meInfo->MV[1];
            m_data[zorder].refIdx[0] = meInfo->refIdx[0];
            m_data[zorder].refIdx[1] = meInfo->refIdx[1];
        }
    }
}

#if defined (MFX_ENABLE_CM)

void H265CU::MePuGacc(H265MEInfo *meInfo, Ipp32s lastPredIdx)
{
    Ipp32s x = (m_ctbPelX + meInfo->posx) / meInfo->width;
    Ipp32s y = (m_ctbPelY + meInfo->posy) / meInfo->height;
    Ipp32s puSize = GetPuSize(meInfo->width, meInfo->height);
    Ipp32s pitchDist = (Ipp32s)H265Enc::pitchDist[puSize] / sizeof(mfxU32);
    Ipp32s pitchMv = (Ipp32s)H265Enc::pitchMv[puSize] / sizeof(mfxI16Pair);
    
    Ipp32u numParts = (m_par->NumPartInCU >> (meInfo->depth << 1));
    Ipp32s blockIdx = meInfo->absPartIdx &~ (numParts - 1);
    Ipp32s partAddr = meInfo->absPartIdx &  (numParts - 1);
    Ipp32s curPUidx = (meInfo->splitMode == PART_SIZE_NxN) ? (partAddr / (numParts >> 2)) : !!partAddr;

    H265MV mvRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s costRefBest[2][MAX_NUM_REF_IDX] = { { INT_MAX }, { INT_MAX } };
    Ipp32s mvCostRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s bitsRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp8s  refIdxBest[2] = {};

    Ipp32s predIdxBits[3];
    GetPredIdxBits(meInfo->splitMode, (m_cslice->slice_type == P_SLICE), curPUidx, lastPredIdx, predIdxBits);

    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;
    Ipp32s numLists = (m_cslice->slice_type == B_SLICE) + 1;
    for (Ipp32s list = 0; list < numLists; list++) {
        Ipp32s numRefIdx = m_cslice->num_ref_idx[list];
        mfxI16Pair *** cmMvs = pMvCpu[cmCurIdx][list];
        mfxU32 *** cmDists = pDistCpu[cmCurIdx][list];

        //mfxI16Pair *(* cmMvs)[PU_MAX] = mvCpu[cmCurIdx][list];
        //mfxU32 *(* cmDists)[PU_MAX] = distCpu[cmCurIdx][list];

        for (Ipp8s refIdx = 0; refIdx < numRefIdx; refIdx++) {
            costRefBest[list][refIdx] = INT_MAX;
            if (list == 1) { // TODO: use pre-build table of duplicates instead of std::find
                Ipp32s idx0 = m_currFrame->m_mapRefIdxL1ToL0[refIdx];
                if (idx0 >= 0) {
                    // don't do ME just re-calc costs
                    mvRefBest[1][refIdx] = mvRefBest[0][idx0];
                    mvCostRefBest[1][refIdx] = MvCost1RefLog(mvRefBest[0][idx0], (Ipp8s)refIdx, m_amvpCand[curPUidx], list);
                    bitsRefBest[1][refIdx] = MVP_LX_FLAG_BITS;
                    bitsRefBest[1][refIdx] += GetFlBits(refIdx, numRefIdx);
                    bitsRefBest[1][refIdx] += predIdxBits[1];
                    costRefBest[1][refIdx] = costRefBest[0][idx0] - mvCostRefBest[0][idx0];
                    costRefBest[1][refIdx] -= (Ipp32s)(bitsRefBest[0][idx0] * m_cslice->rd_lambda_sqrt + 0.5);
                    costRefBest[1][refIdx] += mvCostRefBest[1][refIdx];
                    costRefBest[1][refIdx] += (Ipp32s)(bitsRefBest[1][refIdx] * m_cslice->rd_lambda_sqrt + 0.5);
                    if (costRefBest[1][refIdxBest[1]] > costRefBest[1][refIdx])
                        refIdxBest[1] = refIdx;
                    continue;
                }
            }

            mfxI16Pair cmMv = cmMvs[refIdx][puSize][y * pitchMv + x];
            mfxU32 *cmDist = cmDists[refIdx][puSize] + y * pitchDist;

            H265MV mvBest = { 0, 0 };
            Ipp32s costBest = INT_MAX;
            Ipp32s mvCostBest = 0;

            if (meInfo->width > 16 || meInfo->height > 16) {
                cmDist += 16 * x;
                for (Ipp16s sadIdx = 0, dy = -1; dy <= 1; dy++) {
                    for (Ipp16s dx = -1; dx <= 1; dx++, sadIdx++) {
                        H265MV mv = { cmMv.x + dx, cmMv.y + dy };
                        Ipp32s mvCost = MvCost1RefLog(mv, refIdx, m_amvpCand[curPUidx], list);
                        Ipp32s cost = cmDist[sadIdx] + mvCost;
                        if (costBest > cost) {
                            costBest = cost;
                            mvBest = mv;
                            mvCostBest = mvCost;
                        }
                    }
                }
            }
            else {
                cmDist += 1 * x;
                mvBest.mvx = cmMv.x;
                mvBest.mvy = cmMv.y;
                mvCostBest = MvCost1RefLog(mvBest, refIdx, m_amvpCand[curPUidx], list);
                costBest = *cmDist + mvCostBest;
            }

            mvRefBest[list][refIdx] = mvBest;
            mvCostRefBest[list][refIdx] = mvCostBest;
            costRefBest[list][refIdx] = costBest;
            bitsRefBest[list][refIdx] = MVP_LX_FLAG_BITS + predIdxBits[list];
            bitsRefBest[list][refIdx] += GetFlBits(refIdx, numRefIdx);
            costRefBest[list][refIdx] += (Ipp32s)(bitsRefBest[list][refIdx] * m_cslice->rd_lambda_sqrt + 0.5);

            if (costRefBest[list][refIdxBest[list]] > costRefBest[list][refIdx])
                refIdxBest[list] = refIdx;
        }
    }

    if (m_cslice->slice_type == P_SLICE) {
        meInfo->interDir = INTER_DIR_PRED_L0;
        meInfo->refIdx[0] = (Ipp8s)refIdxBest[0];
        meInfo->refIdx[1] = -1;
        meInfo->MV[0] = mvRefBest[0][refIdxBest[0]];
        meInfo->MV[1] = MV_ZERO;
        return;
    }
    
    Ipp32s idxL0 = refIdxBest[0];
    Ipp32s idxL1 = refIdxBest[1];
    Ipp32s costList[2] = { costRefBest[0][idxL0], costRefBest[1][idxL1] };

    Ipp32s costBiBest = INT_MAX;
    H265MV mvBiBest[2] = { mvRefBest[0][idxL0], mvRefBest[1][idxL1] };

    H265Frame *refF = m_currFrame->m_refPicList[0].m_refFrames[idxL0];
    H265Frame *refB = m_currFrame->m_refPicList[1].m_refFrames[idxL1];
    if (m_cslice->slice_type == B_SLICE && meInfo->width + meInfo->height != 12 && refF && refB) {
        // use satd for bidir refine because it is always sub-pel
        Ipp32s useHadamard = (m_par->hadamardMe >= 2);

        Ipp32s mvCostBiBest = mvCostRefBest[0][idxL0] + mvCostRefBest[1][idxL1];
        costBiBest = mvCostBiBest;
        costBiBest += MatchingMetricBipredPu(src, meInfo, refIdxBest, mvBiBest, useHadamard);

        // refine Bidir
        if (IPP_MIN(costList[0], costList[1]) * 9 > 8 * costBiBest) {

            bool changed = true;
            for (Ipp32s iter = 0; iter < m_par->numBiRefineIter - 1 && changed; iter++) {
                H265MV mvCenter[2] = { mvBiBest[0], mvBiBest[1] };
                changed = false;
                Ipp32s count = (refF == refB && mvBiBest[0] == mvBiBest[1]) ? 2 * 2 : 2 * 2 * 2;
                for (Ipp32s i = 0; i < count; i++) {
                    H265MV mv[2] = { mvCenter[0], mvCenter[1] };
                    if (i & 2)
                        mv[i >> 2].mvx += (i & 1) ? 1 : -1;
                    else
                        mv[i >> 2].mvy += (i & 1) ? 1 : -1;

                    Ipp32s mvCost = MvCost1RefLog(mv[0], idxL0, m_amvpCand[curPUidx], 0) +
                                    MvCost1RefLog(mv[1], idxL1, m_amvpCand[curPUidx], 1);
                    Ipp32s cost = MatchingMetricBipredPu(src, meInfo, refIdxBest, mv, useHadamard);
                    cost += mvCost;

                    if (costBiBest > cost) {
                        costBiBest = cost;
                        mvCostBiBest = mvCost;
                        mvBiBest[0] = mv[0];
                        mvBiBest[1] = mv[1];
                        changed = true;
                    }
                }
            }

            Ipp32s bitsBiBest = MVP_LX_FLAG_BITS + predIdxBits[2];
            bitsBiBest += GetFlBits(idxL0, m_cslice->num_ref_idx[0]);
            bitsBiBest += GetFlBits(idxL1, m_cslice->num_ref_idx[1]);
            costBiBest += (Ipp32s)(bitsBiBest * m_cslice->rd_lambda_sqrt + 0.5);
        }
    }

    Ipp32s useHadamard = (m_par->hadamardMe >= 2);

    Ipp32s bestMergeCost = INT_MAX;
    H265MV bestMergeMv[2] = {};
    Ipp8s  bestMergeRef[2] = { -1, -1 };
    if (!(m_par->fastSkip && meInfo->splitMode == PART_SIZE_2Nx2N)) {
        for (Ipp32s mergeIdx = 0; mergeIdx < m_mergeCand[curPUidx].numCand; mergeIdx++) {
            Ipp8s *refIdx = m_mergeCand[curPUidx].refIdx + 2 * mergeIdx;
            if (refIdx[0] < 0 && refIdx[1] < 0)
                continue;
            H265MV *mv = m_mergeCand[curPUidx].mvCand + 2 * mergeIdx;

            Ipp32s cost = INT_MAX;
            Ipp8u interDir = 0;
            interDir += (refIdx[0] >= 0) ? INTER_DIR_PRED_L0 : 0;
            interDir += (refIdx[1] >= 0) ? INTER_DIR_PRED_L1 : 0;
            if (refIdx[0] >= 0 && refIdx[1] >= 0) {
                H265Frame *refF = m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]];
                H265Frame *refB = m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]];
                cost = MatchingMetricBipredPu(src, meInfo, refIdx, mv, useHadamard);
            }
            else {
                Ipp32s list = (refIdx[1] >= 0);
                H265Frame *ref = m_currFrame->m_refPicList[list].m_refFrames[refIdx[list]];
                cost = MatchingMetricPu(src, meInfo, mv + list, ref, useHadamard);
            }

            cost += (Ipp32s)(GetFlBits(mergeIdx, m_mergeCand[curPUidx].numCand) * m_cslice->rd_lambda_sqrt + 0.5);

            if (bestMergeCost >= cost) {
                bestMergeCost = cost;
                bestMergeRef[0] = refIdx[0];
                bestMergeRef[1] = refIdx[1];
                bestMergeMv[0] = mv[0];
                bestMergeMv[1] = mv[1];
            }
        }
    }

    if (bestMergeCost <= costList[0] && bestMergeCost <= costList[1] && bestMergeCost <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L0 * (bestMergeRef[0] >= 0) + INTER_DIR_PRED_L1 * (bestMergeRef[1] >= 0);
        meInfo->refIdx[0] = bestMergeRef[0];
        meInfo->refIdx[1] = bestMergeRef[1];
        meInfo->MV[0] = bestMergeMv[0];
        meInfo->MV[1] = bestMergeMv[1];
    }
    else if (costList[0] <= costList[1] && costList[0] <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L0;
        meInfo->refIdx[0] = (Ipp8s)refIdxBest[0];
        meInfo->refIdx[1] = -1;
        meInfo->MV[0] = mvRefBest[0][idxL0];
        meInfo->MV[1] = MV_ZERO;
    }
    else if (costList[1] <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L1;
        meInfo->refIdx[0] = -1;
        meInfo->refIdx[1] = (Ipp8s)refIdxBest[1];
        meInfo->MV[0] = MV_ZERO;
        meInfo->MV[1] = mvRefBest[1][idxL1];
    }
    else {
        meInfo->interDir = INTER_DIR_PRED_L0 + INTER_DIR_PRED_L1;
        meInfo->refIdx[0] = (Ipp8s)refIdxBest[0];
        meInfo->refIdx[1] = (Ipp8s)refIdxBest[1];
        meInfo->MV[0] = mvBiBest[0];
        meInfo->MV[1] = mvBiBest[1];
    }

    Ipp32s xstart = meInfo->posx >> m_par->QuadtreeTULog2MinSize;
    Ipp32s ystart = meInfo->posy >> m_par->QuadtreeTULog2MinSize;
    Ipp32s xsize = meInfo->width >> m_par->QuadtreeTULog2MinSize;
    Ipp32s ysize = meInfo->height >> m_par->QuadtreeTULog2MinSize;
    for (Ipp32s y = ystart; y < ystart + ysize; y++) {
        for (Ipp32s x = xstart; x < xstart + xsize; x++) {
            Ipp32s rorder = y * m_par->NumMinTUInMaxCU + x;
            Ipp32s zorder = h265_scan_r2z[m_par->MaxCUDepth][rorder];
            m_data[zorder].mv[0] = meInfo->MV[0];
            m_data[zorder].mv[1] = meInfo->MV[1];
            m_data[zorder].refIdx[0] = meInfo->refIdx[0];
            m_data[zorder].refIdx[1] = meInfo->refIdx[1];
        }
    }
}

bool H265CU::CheckGpuIntraCost(Ipp32s leftPelX, Ipp32s topPelY, Ipp32s depth) const
{
    bool tryIntra = true;

    if (m_par->enableCmFlag && m_par->cmIntraThreshold != 0 && m_cslice->slice_type != I_SLICE) {
        mfxU32 cmIntraCost = 0;
        mfxU32 cmInterCost = 0;
        mfxU32 cmInterCostMin = IPP_MAX_32U;
        Ipp32s cuSize = m_par->MaxCUSize >> depth;
        Ipp32s x = leftPelX / 16;
        Ipp32s y = topPelY / 16;
        Ipp32s x_num = (cuSize == 32) ? 2 : 1;
        Ipp32s y_num = (cuSize == 32) ? 2 : 1;

        for (Ipp32s refList = 0; refList < ((m_cslice->slice_type == B_SLICE) ? 2 : 1); refList++) {
            cmInterCost = 0;
            for (Ipp32s j = y; j < y + y_num; j++) {
                for (Ipp32s i = x; i < x + x_num; i++) {
                    CmMbIntraDist *intraDistLine = (CmMbIntraDist *)((Ipp8u *)cmMbIntraDist[cmCurIdx] + j * intraPitch[cmCurIdx]);
                    cmIntraCost += intraDistLine[i].intraDist;

                    mfxU32 cmInterCost16x16 = IPP_MAX_32U;
                    for (Ipp8s refIdx = 0; refIdx < m_cslice->num_ref_idx[0]; refIdx++) {
                        mfxU32 *dist16x16line = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU16x16]) + (j        ) * pitchDist[PU16x16]);
                        mfxU32 *dist16x8line0 = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU16x8])  + (j * 2    ) * pitchDist[PU16x8]);
                        mfxU32 *dist16x8line1 = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU16x8])  + (j * 2 + 1) * pitchDist[PU16x8]);
                        mfxU32 *dist8x16line  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU8x16])  + (j        ) * pitchDist[PU8x16]);
                        mfxU32 *dist8x8line0  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU8x8])   + (j * 2    ) * pitchDist[PU8x8]);
                        mfxU32 *dist8x8line1  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU8x8])   + (j * 2 + 1) * pitchDist[PU8x8]);
                        mfxU32 *dist8x4line0  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU8x4])   + (j * 4    ) * pitchDist[PU8x4]);
                        mfxU32 *dist8x4line1  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU8x4])   + (j * 4 + 1) * pitchDist[PU8x4]);
                        mfxU32 *dist8x4line2  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU8x4])   + (j * 4 + 2) * pitchDist[PU8x4]);
                        mfxU32 *dist8x4line3  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU8x4])   + (j * 4 + 3) * pitchDist[PU8x4]);
                        mfxU32 *dist4x8line0  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU4x8])   + (j * 2    ) * pitchDist[PU4x8]);
                        mfxU32 *dist4x8line1  = (mfxU32 *)((Ipp8u *)(pDistCpu[cmCurIdx][refList][refIdx][PU4x8])   + (j * 2 + 1) * pitchDist[PU4x8]);
                        Ipp32u dist16x16 = dist16x16line[i];
                        //Ipp32u dist16x8 = dist16x8line0[i] + dist16x8line1[i];
                        //Ipp32u dist8x16 = dist8x16line[i * 2] + dist8x16line[i * 2 + 1];
                        Ipp32u dist8x8 = dist8x8line0[i * 2] + dist8x8line0[i * 2 + 1] +
                                         dist8x8line1[i * 2] + dist8x8line1[i * 2 + 1];
                        //Ipp32u dist8x4 = dist8x4line0[i * 2] + dist8x4line0[i * 2 + 1] +
                        //                 dist8x4line1[i * 2] + dist8x4line1[i * 2 + 1] +
                        //                 dist8x4line2[i * 2] + dist8x4line2[i * 2 + 1] +
                        //                 dist8x4line3[i * 2] + dist8x4line3[i * 2 + 1];
                        //Ipp32u dist4x8 = dist4x8line0[i * 4 + 0] + dist4x8line0[i * 4 + 1] +
                        //                 dist4x8line0[i * 4 + 2] + dist4x8line0[i * 4 + 3] +
                        //                 dist4x8line1[i * 4 + 0] + dist4x8line1[i * 4 + 1] +
                        //                 dist4x8line1[i * 4 + 2] + dist4x8line1[i * 4 + 3];
                        if (dist16x16 < cmInterCost16x16)
                            cmInterCost16x16 = dist16x16; 
                        //if (dist16x8 < cmInterCost16x16)
                        //    cmInterCost16x16 = dist16x8; 
                        //if (dist8x16 < cmInterCost16x16)
                        //    cmInterCost16x16 = dist8x16; 
                        if (dist8x8 < cmInterCost16x16)
                            cmInterCost16x16 = dist8x8; 
                        //if (dist8x4 < cmInterCost16x16)
                        //    cmInterCost16x16 = dist8x4; 
                        //if (dist4x8 < cmInterCost16x16)
                        //    cmInterCost16x16 = dist4x8;
                    }
                    cmInterCost += cmInterCost16x16;
                }
            }
            if (cmInterCost < cmInterCostMin)
                cmInterCostMin = cmInterCost;
        }

        tryIntra = cmIntraCost < (m_par->cmIntraThreshold / 256.0) * cmInterCostMin;
    }

    return tryIntra;
}

#endif // MFX_ENABLE_CM

void H265CU::EncAndRecLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost) {
    Ipp32s depthMax = m_data[absPartIdx].depth + m_data[absPartIdx].trIdx;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    Ipp32s width = m_data[absPartIdx].size >> m_data[absPartIdx].trIdx;

    if (nz) *nz = 0;

    VM_ASSERT(depth <= depthMax);

    if (depth == depthMax) {
        EncAndRecLumaTu( absPartIdx, offset, width, nz, cost, 0, INTRA_PRED_CALC);
        if (nz && *nz) {
            SetCbfOne(absPartIdx, TEXT_LUMA, m_data[absPartIdx].trIdx);
            for (Ipp32u i = 1; i < numParts; i++)
                m_data[absPartIdx + i].cbf[0] = m_data[absPartIdx].cbf[0];
        }
    } else {
        Ipp32s subsize = width << (depthMax - depth) >> 1;
        subsize *= subsize;
        numParts >>= 2;
        if (cost) *cost = 0;
        for (Ipp32u i = 0; i < 4; i++) {
            Ipp8u nzt;
            CostType cost_temp = COST_MAX;

            EncAndRecLuma(absPartIdx + numParts * i, offset, depth+1, &nzt, cost ? &cost_temp : 0);
            if (cost) *cost += cost_temp;
            if (nz && nzt) *nz = 1;
            offset += subsize;
        }
        if (nz && *nz && depth >= m_data[absPartIdx].depth) {
            for (Ipp32u i = 0; i < 4; i++) {
                SetCbfOne(absPartIdx + numParts * i, TEXT_LUMA, depth - m_data[absPartIdx].depth);
            }
        }
    }
}

void H265CU::EncAndRecChroma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost) {
    Ipp32s depthMax = m_data[absPartIdx].depth + m_data[absPartIdx].trIdx;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) )>>2;
    Ipp32s width = m_data[absPartIdx].size >> m_data[absPartIdx].trIdx << (depthMax - depth);
    width >>= 1;

    if(nz) nz[0] = nz[1] = 0;

    if (depth == m_data[absPartIdx].depth && m_data[absPartIdx].predMode == MODE_INTER) {
        if (!(m_par->AnalyseFlags & HEVC_COST_CHROMA)) {
            InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredChroma[m_data[absPartIdx].curIdx][depth], MAX_CU_SIZE);
            Ipp32s offsetPredChroma = GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE);
            PixType *pSrcUv = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchSrc);
            TuDiffNv12(m_interResidualsU[m_data[absPartIdx].curIdx][depth] + offsetPredChroma/2, MAX_CU_SIZE/2, pSrcUv, m_pitchSrc, m_interPredChroma[m_data[absPartIdx].curIdx][depth] + offsetPredChroma, MAX_CU_SIZE, m_data[absPartIdx].size/2);
            TuDiffNv12(m_interResidualsV[m_data[absPartIdx].curIdx][depth] + offsetPredChroma/2, MAX_CU_SIZE/2, pSrcUv+1, m_pitchSrc, m_interPredChroma[m_data[absPartIdx].curIdx][depth] + offsetPredChroma + 1, MAX_CU_SIZE, m_data[absPartIdx].size/2);
        }
    }

    if (depth == depthMax || width == 4) {
        EncAndRecChromaTu(absPartIdx, offset, width, nz, cost, INTRA_PRED_CALC);
        if (nz) {
            if (nz[0]) {
                for (Ipp32u i = 0; i < 4; i++)
                   SetCbfOne(absPartIdx + numParts * i, TEXT_CHROMA_U, m_data[absPartIdx].trIdx);
                if (depth != depthMax && m_data[absPartIdx].trIdx > 0) {
                    for (Ipp32u i = 0; i < 4; i++)
                       SetCbfOne(absPartIdx + numParts * i, TEXT_CHROMA_U, m_data[absPartIdx].trIdx - 1);
                }
            }
            if (nz[1]) {
                for (Ipp32u i = 0; i < 4; i++)
                   SetCbfOne(absPartIdx + numParts * i, TEXT_CHROMA_V, m_data[absPartIdx].trIdx);
                if (depth != depthMax && m_data[absPartIdx].trIdx > 0) {
                    for (Ipp32u i = 0; i < 4; i++)
                       SetCbfOne(absPartIdx + numParts * i, TEXT_CHROMA_V, m_data[absPartIdx].trIdx - 1);
                }
            }
        }
    } else {
        Ipp32s subsize = width >> 1;
        subsize *= subsize;
        if (cost) *cost = 0;
        for (Ipp32u i = 0; i < 4; i++) {
            CostType cost_temp;
            Ipp8u nzt[2];
            EncAndRecChroma(absPartIdx + numParts * i, offset, depth+1, nz ? nzt : NULL, cost ? &cost_temp : 0);
            if (cost) *cost += cost_temp;
            offset += subsize;

            if (nz) {
                if (nzt[0]) nz[0] = 1;
                if (nzt[1]) nz[1] = 1;
            }
        }
        if (depth >= m_data[absPartIdx].depth && nz) {
            if (nz[0]) {
                for(Ipp32u i = 0; i < 4; i++) {
                   SetCbfOne(absPartIdx + numParts * i, TEXT_CHROMA_U, depth - m_data[absPartIdx].depth);
                }
            }
            if (nz[1]) {
                for(Ipp32u i = 0; i < 4; i++) {
                   SetCbfOne(absPartIdx + numParts * i, TEXT_CHROMA_V, depth - m_data[absPartIdx].depth);
                }
            }
        }
    }
}
/*
static void tuAddClip(PixType *dst, Ipp32s pitchDst, 
                      const PixType *src1, Ipp32s pitchSrc1, 
                      const CoeffsType *src2, Ipp32s pitchSrc2,
                      Ipp32s size)
{
    Ipp32s i, j;
    Ipp32s maxval = 255;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            dst[i] = (PixType)Saturate(0, maxval, src1[i] + src2[i]);
        }
        dst += pitchDst;
        src1 += pitchSrc1;
        src2 += pitchSrc2;
    }
}
*/
static void tuAddClipNv12(PixType *dstNv12, Ipp32s pitchDst, 
                          const PixType *src1Nv12, Ipp32s pitchSrc1, 
                          const CoeffsType *src2Yv12, Ipp32s pitchSrc2,
                          Ipp32s size)
{
    Ipp32s maxval = 255;
    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            dstNv12[2*i] = (PixType)Saturate(0, maxval, src1Nv12[2*i] + src2Yv12[i]);
        }
        dstNv12  += pitchDst;
        src1Nv12 += pitchSrc1;
        src2Yv12 += pitchSrc2;
    }
}
/*
static void tu_add_clip_transp(PixType *dst, PixType *src1, CoeffsType *src2,
                     Ipp32s pitch_dst, Ipp32s m_pitchSrc1, Ipp32s m_pitchSrc2, Ipp32s size)
{
    Ipp32s i, j;
    Ipp32s maxval = 255;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            dst[i] = (PixType)Saturate(0, maxval, src1[m_pitchSrc1*i] + src2[i]);
        }
        dst += pitch_dst;
        src1 ++;
        src2 += m_pitchSrc2;
    }
}
*/

static void TuDiffTransp(CoeffsType *residual, Ipp32s pitchDst, const PixType *src, Ipp32s pitchSrc,
                         const PixType *pred, Ipp32s pitchPred, Ipp32s size)
{
    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            residual[i] = (CoeffsType)src[i] - pred[pitchPred*i];
        }
        residual += pitchDst;
        src += pitchSrc;
        pred ++;
    }
}

static Ipp32s tuSad(const PixType *src, Ipp32s pitchSrc,
                    const PixType *rec, Ipp32s pitchRec, Ipp32s size)
{
    Ipp32s s = 0;
    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            s += abs((CoeffsType)src[i] - rec[i]);
        }
        src += pitchSrc;
        rec += pitchRec;
    }
    return s;
}

static void TuDiff(CoeffsType *residual, Ipp32s pitchDst,
                   const PixType *src, Ipp32s pitchSrc,
                   const PixType *pred, Ipp32s pitchPred,
                   Ipp32s size)
{
    //Ipp32s step_dst = pitch_dst * sizeof(CoeffsType);
    //if (size == 4) {
    //    ippiSub4x4_8u16s_C1R(src, m_pitchSrc, pred, pitch_pred, residual, step_dst);
    //} else if (size == 8) {
    //    ippiSub8x8_8u16s_C1R(src, m_pitchSrc, pred, pitch_pred, residual, step_dst);
    //} else if (size == 16) {
    //    ippiSub16x16_8u16s_C1R(src, m_pitchSrc, pred, pitch_pred, residual, step_dst);
    //} else if (size == 32) {
    //    for (Ipp32s y = 0; y < 32; y += 16) {
    //        for (Ipp32s x = 0; x < 32; x += 16) {
    //            ippiSub16x16_8u16s_C1R(src + x + y * m_pitchSrc, m_pitchSrc,
    //                                   pred + x + y * pitch_pred, pitch_pred,
    //                                   residual + x + y * pitch_dst, step_dst);
    //        }
    //    }
    //}

    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            residual[i] = (CoeffsType)src[i] - pred[i];
        }
        residual += pitchDst;
        src += pitchSrc;
        pred += pitchPred;
    }
}

static Ipp32s TuSse(const PixType *src, Ipp32s pitchSrc,
                    const PixType *rec, Ipp32s pitchRec, Ipp32s size)
{
    Ipp32s s = 0;
    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            s += ((CoeffsType)src[i] - rec[i]) * (Ipp32s)((CoeffsType)src[i] - rec[i]);
        }
        src += pitchSrc;
        rec += pitchRec;
    }
    return s;
}

Ipp32s tuHad(const PixType *src, Ipp32s pitchSrc, const PixType *rec, Ipp32s pitchRec,
             Ipp32s width, Ipp32s height)
{
    Ipp32u satdTotal = 0;
    Ipp32s satd[2] = {0, 0};

    /* assume height and width are multiple of 4 */
    VM_ASSERT(!(width & 0x03) && !(height & 0x03));

    /* test with optimized SATD source code */
    if (width == 4 && height == 4) {
        /* single 4x4 block */
        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_8u)(src, pitchSrc, rec, pitchRec);
        satdTotal += (satd[0] + 1) >> 1;
    } else if ( (height | width) & 0x07 ) {
        /* multiple 4x4 blocks - do as many pairs as possible */
        Ipp32s widthPair = width & ~0x07;
        Ipp32s widthRem = width - widthPair;
        for (Ipp32s j = 0; j < height; j += 4, src += pitchSrc * 4, rec += pitchRec * 4) {
            Ipp32s i = 0;
            for (; i < widthPair; i += 4*2) {
                MFX_HEVC_PP::NAME(h265_SATD_4x4_Pair_8u)(src + i, pitchSrc, rec + i, pitchRec, satd);
                satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
            }

            if (widthRem) {
                satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_8u)(src + i, pitchSrc, rec + i, pitchRec);
                satdTotal += (satd[0] + 1) >> 1;
            }
        }
    } else if (width == 8 && height == 8) {
        /* single 8x8 block */
        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u)(src, pitchSrc, rec, pitchRec);
        satdTotal += (satd[0] + 2) >> 2;
    } else {
        /* multiple 8x8 blocks - do as many pairs as possible */
        Ipp32s widthPair = width & ~0x0f;
        Ipp32s widthRem = width - widthPair;
        for (Ipp32s j = 0; j < height; j += 8, src += pitchSrc * 8, rec += pitchRec * 8) {
            Ipp32s i = 0;
            for (; i < widthPair; i += 8*2) {
                MFX_HEVC_PP::NAME(h265_SATD_8x8_Pair_8u)(src + i, pitchSrc, rec + i, pitchRec, satd);
                satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
            }

            if (widthRem) {
                satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u)(src + i, pitchSrc, rec + i, pitchRec);
                satdTotal += (satd[0] + 2) >> 2;
            }
        }
    }

    return satdTotal;
}

static void TuDiffNv12(CoeffsType *residual, Ipp32s pitchDst,  const PixType *src, Ipp32s pitchSrc,
                       const PixType *pred, Ipp32s pitchPred, Ipp32s size)
{
    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            residual[i] = (CoeffsType)src[i*2] - pred[i*2];
        }
        residual += pitchDst;
        src += pitchSrc;
        pred += pitchPred;
    }
}

static Ipp32s tuSadNv12(const PixType *src, Ipp32s pitchSrc,
                        const PixType *rec, Ipp32s pitchRec, Ipp32s size)
{
    Ipp32s s = 0;

    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            s += abs((CoeffsType)src[i*2] - rec[i*2]);
        }
        src += pitchSrc;
        rec += pitchRec;
    }
    return s;
}
// for uv can use ordinary sse twice instead
static Ipp32s TuSseNv12(const PixType *src, Ipp32s pitchSrc,
                        const PixType *rec, Ipp32s pitchRec, Ipp32s size)
{
    Ipp32s i, j;
    Ipp32s s = 0;

    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            s += ((CoeffsType)src[i*2] - rec[i*2]) * ((CoeffsType)src[i*2] - rec[i*2]);
        }
        src += pitchSrc;
        rec += pitchRec;
    }
    return s;
}

void H265CU::EncAndRecLumaTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost,
                             Ipp8u cost_pred_flag, IntraPredOpt pred_opt)
{
    CostType cost_pred, cost_rdoq;
    Ipp32u lpel_x   = m_ctbPelX +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
    Ipp32u tpel_y   = m_ctbPelY +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);

    if (nz) *nz = 0;
    if (cost) *cost = 0;
    else if (m_isRdoq) cost = &cost_rdoq;

    if (lpel_x >= m_par->Width || tpel_y >= m_par->Height)
        return;

    Ipp32s maxDepth = m_par->Log2MaxCUSize - m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    PixType *rec;
    Ipp32s pitch_rec;
    PixType *src = m_ySrc + ((PUStartRow * m_pitchSrc + PUStartColumn) << m_par->Log2MinTUSize);

    PixType *pred = NULL;
    Ipp32s  pitch_pred = 0;
    Ipp32s  is_pred_transposed = 0;
    const Ipp8u depth = m_data[absPartIdx].depth;
    if (m_data[absPartIdx].predMode == MODE_INTRA) {
        rec = m_yRec + ((PUStartRow * m_pitchRec + PUStartColumn) << m_par->Log2MinTUSize);
        pitch_rec = m_pitchRec;
        if (pred_opt == INTRA_PRED_IN_BUF) {
            pred = m_predIntraAll + m_data[absPartIdx].intraLumaDir * width * width;
            pitch_pred = width;
            is_pred_transposed = (m_data[absPartIdx].intraLumaDir >= 2 &&
                m_data[absPartIdx].intraLumaDir < 18);
        } else {
            pred = rec;
            pitch_pred = m_pitchRec;
        }
    } else {
        pitch_pred = MAX_CU_SIZE;
        pred = m_interPred[m_data[absPartIdx].curIdx][ depth ];
        pred += (PUStartRow * pitch_pred + PUStartColumn) << m_par->Log2MinTUSize;
        if (pred_opt == INTER_PRED_IN_BUF) {
            rec = m_interRec[m_data[absPartIdx].curRecIdx][ depth ];
            rec += (PUStartRow * MAX_CU_SIZE + PUStartColumn) << m_par->Log2MinTUSize;
            pitch_rec = MAX_CU_SIZE;
        } else {
            rec = m_yRec + ((PUStartRow * m_pitchRec + PUStartColumn) << m_par->Log2MinTUSize);
            pitch_rec = m_pitchRec;
        }
    }

    if (m_data[absPartIdx].predMode == MODE_INTRA && pred_opt == INTRA_PRED_CALC) {
        IntraPredTu(absPartIdx, width, m_data[absPartIdx].intraLumaDir, 1);
    }

    if (cost && cost_pred_flag) {
        cost_pred = tuHad(src, m_pitchSrc, rec, pitch_rec, width, width);
        *cost = cost_pred;
        return;
    }

    Ipp8u cbf = 0;
    if (!m_data[absPartIdx].flags.skippedFlag) {
        if (m_data[absPartIdx].predMode == MODE_INTRA) {
            if (is_pred_transposed)
                TuDiffTransp(m_residualsY + offset, width, src, m_pitchSrc, pred, pitch_pred, width);
            else
                TuDiff(m_residualsY + offset, width, src, m_pitchSrc, pred, pitch_pred, width);
        } else {
            Ipp16s *resid = m_interResidualsY[m_data[absPartIdx].curIdx][ depth ];
            resid += (PUStartRow * MAX_CU_SIZE + PUStartColumn) << m_par->Log2MinTUSize;
            IppiSize roiSize = {width, width};
            ippiCopy_16s_C1R(resid, MAX_CU_SIZE*2, m_residualsY + offset, width*2, roiSize);
        }

        TransformFwd(offset, width, 1, m_data[absPartIdx].predMode == MODE_INTRA);
        QuantFwdTu(absPartIdx, offset, width, 1);

        if (m_rdOptFlag || nz) {
            for (Ipp32s i = 0; i < width * width; i++) {
                if (m_trCoeffY[i + offset]) {
                    cbf = 1;
                    if (nz) *nz = 1;
                    break;
                }
            }
        }

        if (!m_rdOptFlag || cbf)
            QuantInvTu(absPartIdx, offset, width, 1);

        IppiSize roi = { width, width };
        (is_pred_transposed)
            ? ippiTranspose_8u_C1R(pred, pitch_pred, rec, pitch_rec, roi)
            : ippiCopy_8u_C1R(pred, pitch_pred, rec, pitch_rec, roi);

        if (!m_rdOptFlag || cbf)
            TransformInv2(rec, pitch_rec, offset, width, 1, m_data[absPartIdx].predMode == MODE_INTRA);

    } else {
        memset(m_trCoeffY + offset, 0, sizeof(CoeffsType) * width * width);
    }

    //if (m_data[absPartIdx].flags.skippedFlag) { // TODO check if needed
    //    IppiSize roi = { width, width };
    //    ippiCopy_8u_C1R(pred, pitch_pred, rec, m_pitchRec, roi);
    //}

    if (cost) {
        CostType cost_rec = TuSse(src, m_pitchSrc, rec, pitch_rec, width);
        *cost = cost_rec;
    }

    if (m_data[absPartIdx].flags.skippedFlag) {
        if (nz) *nz = 0;
        return;
    }

    if (m_rdOptFlag && cost) {
        Ipp8u code_dqp = getdQPFlag();
        m_bsf->Reset();
        if (cbf) {
            SetCbfOne(absPartIdx, TEXT_LUMA, m_data[absPartIdx].trIdx);
            PutTransform(m_bsf, offset, offset>>2, absPartIdx, absPartIdx,
                depth + m_data[absPartIdx].trIdx,
                width, width, m_data[absPartIdx].trIdx, code_dqp);
        }
        else {
            m_bsf->EncodeSingleBin_CABAC(CTX(m_bsf,QT_CBF_HEVC) +
                GetCtxQtCbf(absPartIdx, TEXT_LUMA, m_data[absPartIdx].trIdx),
                0);
        }
        if(IS_INTRA(m_data, absPartIdx))
            *cost += BIT_COST(m_bsf->GetNumBits());
        else
            *cost += BIT_COST_INTER(m_bsf->GetNumBits());
    }
}

void H265CU::EncAndRecChromaTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost, IntraPredOpt pred_opt)
{
    Ipp32u lpel_x   = m_ctbPelX +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
    Ipp32u tpel_y   = m_ctbPelY +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);

    if (nz) nz[0] = nz[1] = 0;
    if (cost) *cost = 0;

    if (lpel_x >= m_par->Width || tpel_y >= m_par->Height)
        return;

    Ipp32s maxDepth = m_par->Log2MaxCUSize - m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);
    PixType *pPred;
    Ipp32s pitchPred;
    PixType *pSrc = m_uvSrc + (((PUStartRow * m_pitchSrc >> 1) + PUStartColumn) << m_par->Log2MinTUSize);
    PixType *pRec;
    Ipp32s pitchRec;
    const Ipp8u depth = m_data[absPartIdx].depth;

    if (m_data[absPartIdx].predMode == MODE_INTRA) {
        Ipp8u intra_pred_mode = m_data[absPartIdx].intraChromaDir;
        if (intra_pred_mode == INTRA_DM_CHROMA) {
            Ipp32s shift = h265_log2m2[m_par->NumPartInCU >> (depth<<1)] + 2;
            Ipp32s absPartIdx_0 = absPartIdx >> shift << shift;
            intra_pred_mode = m_data[absPartIdx_0].intraLumaDir;
        }
        IntraPredTu(absPartIdx, width, intra_pred_mode, 0);
        pRec = m_uvRec + (((PUStartRow * m_pitchRec >> 1) + PUStartColumn) << m_par->Log2MinTUSize);
        pitchRec = m_pitchRec;
        pPred = pRec;
        pitchPred = m_pitchRec;
    } else {
        pPred = m_interPredChroma[m_data[absPartIdx].curIdx][depth] + ((PUStartRow * (MAX_CU_SIZE >> 1) + PUStartColumn) << m_par->Log2MinTUSize);
        pitchPred = MAX_CU_SIZE;
        if (pred_opt == INTER_PRED_IN_BUF) {
            pRec = m_interRecChroma[m_data[absPartIdx].curRecIdx][ depth ];
            pRec += (PUStartRow *  (MAX_CU_SIZE >> 1) + PUStartColumn) << m_par->Log2MinTUSize;
            pitchRec = MAX_CU_SIZE;
        } else {
            pRec = m_uvRec + (((PUStartRow * m_pitchRec >> 1) + PUStartColumn) << m_par->Log2MinTUSize);
            pitchRec = m_pitchRec;
        }
    }

    Ipp8u cbf[2] = {0, 0};
    if (!m_data[absPartIdx].flags.skippedFlag) {
        if (m_data[absPartIdx].predMode == MODE_INTRA) {
            TuDiffNv12(m_residualsU + offset, width, pSrc, m_pitchSrc, pRec, pitchRec, width);
            TuDiffNv12(m_residualsV + offset, width, pSrc+1, m_pitchSrc, pRec+1, pitchRec, width);
        } else {
            IppiSize roiSize = {width, width};
            Ipp32s offsetU = (PUStartRow * (MAX_CU_SIZE>>1) + PUStartColumn) << (m_par->Log2MinTUSize-1);
            ippiCopy_16s_C1R( m_interResidualsU[m_data[absPartIdx].curIdx][depth] + offsetU, MAX_CU_SIZE, m_residualsU + offset, width*2, roiSize);
            ippiCopy_16s_C1R( m_interResidualsV[m_data[absPartIdx].curIdx][depth] + offsetU, MAX_CU_SIZE, m_residualsV + offset, width*2, roiSize);
        }

        TransformFwd(offset, width, 0, 0);
        QuantFwdTu(absPartIdx, offset, width, 0);

        for (int i = 0; i < width * width; i++) {
            if (m_trCoeffU[i + offset]) {
                cbf[0] = 1;
                break;
            }
        }
        for (int i = 0; i < width * width; i++) {
            if (m_trCoeffV[i + offset]) {
                cbf[1] = 1;
                break;
            }
        }

        if (cbf[0] | cbf[1]) {
            QuantInvTu(absPartIdx, offset, width, 0);
            TransformInv(offset, width, 0, 0);
        }
    } else { // TODO check if needed
        memset(m_trCoeffU + offset, 0, sizeof(CoeffsType) * width * width);
        memset(m_trCoeffV + offset, 0, sizeof(CoeffsType) * width * width);
    }

    if (cbf[0] | cbf[1]) {
        tuAddClipNv12(pRec + 0, pitchRec, pPred + 0, pitchPred, m_residualsU + offset, width, width);
        tuAddClipNv12(pRec + 1, pitchRec, pPred + 1, pitchPred, m_residualsV + offset, width, width);
        if (nz) {
            nz[0] = cbf[0];
            nz[1] = cbf[1];
        }
    } else if (m_data[absPartIdx].predMode != MODE_INTRA) { // TODO check if needed
        IppiSize roiSize = {width*2, width};
        ippiCopy_8u_C1R(pPred, pitchPred, pRec, pitchRec, roiSize);
    }

    if (cost && (m_par->AnalyseFlags & HEVC_COST_CHROMA)) {
        *cost += TuSseNv12(pSrc + 0, m_pitchSrc, pRec + 0, pitchRec, width);
        *cost += TuSseNv12(pSrc + 1, m_pitchSrc, pRec + 1, pitchRec, width);
    }

    //kolya //WEIGHTED_CHROMA_DISTORTION (JCTVC-F386)
    if (0 && cost)
        (*cost) *= (this->m_ChromaDistWeight);

    if (m_rdOptFlag && cost) {
        m_bsf->Reset();
        if (cbf[0])
        {
            CodeCoeffNxN(m_bsf, this, (m_trCoeffU+offset), absPartIdx, width, width, TEXT_CHROMA_U );
        }
        if (cbf[1])
        {
            CodeCoeffNxN(m_bsf, this, (m_trCoeffV+offset), absPartIdx, width, width, TEXT_CHROMA_V );
        }
        if(IS_INTRA(m_data, absPartIdx))
            *cost += BIT_COST(m_bsf->GetNumBits());
        else
            *cost += BIT_COST_INTER(m_bsf->GetNumBits());
    }
}

/* ****************************************************************************** *\
FUNCTION: AddMvpCand
DESCRIPTION:
\* ****************************************************************************** */

bool H265CU::AddMvpCand(MvPredInfo<2> *info, H265CUData *data, Ipp32s blockZScanIdx,
                        Ipp32s listIdx, Ipp32s refIdx, bool order)
{
    RefPicList *refPicList = m_currFrame->m_refPicList;

    if (!data || data[blockZScanIdx].predMode == MODE_INTRA)
        return false;

    if (!order) {
        if (data[blockZScanIdx].refIdx[listIdx] == refIdx) {
            info->mvCand[info->numCand] = data[blockZScanIdx].mv[listIdx];
            info->numCand++;
            return true;
        }

        Ipp32s currRefTb = refPicList[listIdx].m_deltaPoc[refIdx];
        listIdx = 1 - listIdx;

        if (data[blockZScanIdx].refIdx[listIdx] >= 0) {
            Ipp32s neibRefTb = refPicList[listIdx].m_deltaPoc[data[blockZScanIdx].refIdx[listIdx]];
            if (currRefTb == neibRefTb) {
                info->mvCand[info->numCand] = data[blockZScanIdx].mv[listIdx];
                info->numCand++;
                return true;
            }
        }
    }
    else {
        Ipp32s currRefTb = refPicList[listIdx].m_deltaPoc[refIdx];
        Ipp8u isCurrRefLongTerm = refPicList[listIdx].m_isLongTermRef[refIdx];

        for (Ipp32s i = 0; i < 2; i++) {
            if (data[blockZScanIdx].refIdx[listIdx] >= 0) {
                Ipp32s neibRefTb = refPicList[listIdx].m_deltaPoc[data[blockZScanIdx].refIdx[listIdx]];
                Ipp8u isNeibRefLongTerm = refPicList[listIdx].m_isLongTermRef[data[blockZScanIdx].refIdx[listIdx]];

                H265MV cMvPred, rcMv;

                cMvPred = data[blockZScanIdx].mv[listIdx];

                if (isCurrRefLongTerm || isNeibRefLongTerm) {
                    rcMv = cMvPred;
                }
                else {
                    Ipp32s scale = GetDistScaleFactor(currRefTb, neibRefTb);

                    if (scale == 4096) {
                        rcMv = cMvPred;
                    }
                    else {
                        rcMv.mvx = (Ipp16s)Saturate(-32768, 32767, (scale * cMvPred.mvx + 127 + (scale * cMvPred.mvx < 0)) >> 8);
                        rcMv.mvy = (Ipp16s)Saturate(-32768, 32767, (scale * cMvPred.mvy + 127 + (scale * cMvPred.mvy < 0)) >> 8);
                    }
                }

                info->mvCand[info->numCand] = rcMv;
                info->numCand++;
                return true;
            }

            listIdx = 1 - listIdx;
        }
    }

    return false;
}


/* ****************************************************************************** *\
FUNCTION: HasEqualMotion
DESCRIPTION:
\* ****************************************************************************** */

static bool HasEqualMotion(Ipp32s blockZScanIdx, H265CUData *blockLCU, Ipp32s candZScanIdx,
                           H265CUData *candLCU)
{
    for (Ipp32s i = 0; i < 2; i++) {
        if (blockLCU[blockZScanIdx].refIdx[i] != candLCU[candZScanIdx].refIdx[i])
            return false;

        if (blockLCU[blockZScanIdx].refIdx[i] >= 0 &&
            blockLCU[blockZScanIdx].mv[i] != candLCU[candZScanIdx].mv[i])
            return false;
    }

    return true;
}

/* ****************************************************************************** *\
FUNCTION: GetMergeCandInfo
DESCRIPTION:
\* ****************************************************************************** */

static void GetMergeCandInfo(MvPredInfo<5> *info, bool *candIsInter, Ipp32s *interDirNeighbours,
                             Ipp32s blockZScanIdx, H265CUData* blockLCU)
{
    Ipp32s count = info->numCand;

    candIsInter[count] = true;

    info->mvCand[2 * count + 0] = blockLCU[blockZScanIdx].mv[0];
    info->mvCand[2 * count + 1] = blockLCU[blockZScanIdx].mv[1];

    info->refIdx[2 * count + 0] = blockLCU[blockZScanIdx].refIdx[0];
    info->refIdx[2 * count + 1] = blockLCU[blockZScanIdx].refIdx[1];

    interDirNeighbours[count] = 0;

    if (info->refIdx[2 * count] >= 0)
        interDirNeighbours[count] += 1;

    if (info->refIdx[2 * count + 1] >= 0)
        interDirNeighbours[count] += 2;

    info->numCand++;
}

/* ****************************************************************************** *\
FUNCTION: IsDiffMER
DESCRIPTION:
\* ****************************************************************************** */

inline Ipp32s IsDiffMER(Ipp32s xN, Ipp32s yN, Ipp32s xP, Ipp32s yP, Ipp32s log2ParallelMergeLevel)
{
    return ( ((xN ^ xP) | (yN ^ yP)) >> log2ParallelMergeLevel );
}

/* ****************************************************************************** *\
FUNCTION: GetMergeCand
DESCRIPTION:
\* ****************************************************************************** */

void H265CU::GetMergeCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx,
                          Ipp32s cuSize, MvPredInfo<5> *mergeInfo)
{
    mergeInfo->numCand = 0;
    if (m_par->cpps->log2_parallel_merge_level > 2 && m_data[topLeftCUBlockZScanIdx].size == 8) {
        if (partIdx > 0)
            return;
        partMode = PART_SIZE_2Nx2N;
    }

    bool abCandIsInter[5];
    bool isCandInter[5];
    bool isCandAvailable[5];
    Ipp32s  puhInterDirNeighbours[5];
    Ipp32s  candColumn[5], candRow[5], canXP[5], canYP[5], candZScanIdx[5];
    bool checkCurLCU[5];
    H265CUData* candLCU[5];
    Ipp32s maxDepth = m_par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s minTUSize = m_par->MinTUSize;

    for (Ipp32s i = 0; i < 5; i++) {
        abCandIsInter[i] = false;
        isCandAvailable[i] = false;
    }

    Ipp32s partWidth, partHeight, partX, partY;
    GetPartOffsetAndSize(partIdx, partMode, cuSize, partX, partY, partWidth, partHeight);

    Ipp32s topLeftRasterIdx = h265_scan_z2r[maxDepth][topLeftCUBlockZScanIdx] + partX + numMinTUInLCU * partY;
    Ipp32s topLeftRow = topLeftRasterIdx >> maxDepth;
    Ipp32s topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
    Ipp32s topLeftBlockZScanIdx = h265_scan_r2z[maxDepth][topLeftRasterIdx];

    Ipp32s xP = m_ctbPelX + topLeftColumn * minTUSize;
    Ipp32s yP = m_ctbPelY + topLeftRow * minTUSize;
    Ipp32s nPSW = partWidth * minTUSize;
    Ipp32s nPSH = partHeight * minTUSize;

    /* spatial candidates */

    /* left */
    candColumn[0] = topLeftColumn - 1;
    candRow[0] = topLeftRow + partHeight - 1;
    canXP[0] = xP - 1;
    canYP[0] = yP + nPSH - 1;
    checkCurLCU[0] = false;

    /* above */
    candColumn[1] = topLeftColumn + partWidth - 1;
    candRow[1] = topLeftRow - 1;
    canXP[1] = xP + nPSW - 1;
    canYP[1] = yP - 1;
    checkCurLCU[1] = false;

    /* above right */
    candColumn[2] = topLeftColumn + partWidth;
    candRow[2] = topLeftRow - 1;
    canXP[2] = xP + nPSW;
    canYP[2] = yP - 1;
    checkCurLCU[2] = true;

    /* below left */
    candColumn[3] = topLeftColumn - 1;
    candRow[3] = topLeftRow + partHeight;
    canXP[3] = xP - 1;
    canYP[3] = yP + nPSH;
    checkCurLCU[3] = true;

    /* above left */
    candColumn[4] = topLeftColumn - 1;
    candRow[4] = topLeftRow - 1;
    canXP[4] = xP - 1;
    canYP[4] = yP - 1;
    checkCurLCU[4] = false;

    for (Ipp32s i = 0; i < 5; i++) {
        if (mergeInfo->numCand < MAX_NUM_MERGE_CANDS - 1)  {
            candLCU[i] = GetNeighbour(candZScanIdx[i], candColumn[i], candRow[i], topLeftBlockZScanIdx, checkCurLCU[i]);

            if (candLCU[i] && !IsDiffMER(canXP[i], canYP[i], xP, yP, m_par->cpps->log2_parallel_merge_level))
                candLCU[i] = NULL;

            if ((candLCU[i] == NULL) || (candLCU[i][candZScanIdx[i]].predMode == MODE_INTRA)) {
                isCandInter[i] = false;
            }
            else {
                isCandInter[i] = true;

                bool getInfo = false;
                /* getMergeCandInfo conditions */
                switch (i)
                {
                case 0: /* left */
                    if (!((partIdx == 1) && ((partMode == PART_SIZE_Nx2N) ||
                                                (partMode == PART_SIZE_nLx2N) ||
                                                (partMode == PART_SIZE_nRx2N))))
                    {
                        getInfo = true;
                        isCandAvailable[0] = true;
                    }
                    break;
                case 1: /* above */
                    if (!((partIdx == 1) && ((partMode == PART_SIZE_2NxN) ||
                                                (partMode == PART_SIZE_2NxnU) ||
                                                (partMode == PART_SIZE_2NxnD))))
                    {
                        isCandAvailable[1] = true;
                        if (!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0], candZScanIdx[1], candLCU[1]))
                            getInfo = true;
                    }
                    break;
                case 2: /* above right */
                    isCandAvailable[2] = true;
                    if (!isCandAvailable[1] || !HasEqualMotion(candZScanIdx[1], candLCU[1], candZScanIdx[2], candLCU[2]))
                        getInfo = true;
                    break;
                case 3: /* below left */
                    isCandAvailable[3] = true;
                    if (!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0], candZScanIdx[3], candLCU[3]))
                        getInfo = true;
                    break;
                case 4: /* above left */
                default:
                    isCandAvailable[4] = true;
                    if ((!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0], candZScanIdx[4], candLCU[4])) &&
                        (!isCandAvailable[1] || !HasEqualMotion(candZScanIdx[1], candLCU[1], candZScanIdx[4], candLCU[4])))
                    {
                        getInfo = true;
                    }
                    break;
                }

                if (getInfo)
                    GetMergeCandInfo(mergeInfo, abCandIsInter, puhInterDirNeighbours, candZScanIdx[i], candLCU[i]);
            }
        }
    }

    if (m_par->TMVPFlag) {
        H265CUData *currPb = m_data + topLeftBlockZScanIdx;
        H265MV mvCol;

        puhInterDirNeighbours[mergeInfo->numCand] = 0;
        if (GetTempMvPred(currPb, topLeftColumn, topLeftRow, partWidth, partHeight, 0, 0, &mvCol)) {
            abCandIsInter[mergeInfo->numCand] = true;
            puhInterDirNeighbours[mergeInfo->numCand] |= 1;
            mergeInfo->mvCand[2 * mergeInfo->numCand + 0] = mvCol;
            mergeInfo->refIdx[2 * mergeInfo->numCand + 0] = 0;
        }
        if (m_cslice->slice_type == B_SLICE) {
            if (GetTempMvPred(currPb, topLeftColumn, topLeftRow, partWidth, partHeight, 1, 0, &mvCol)) {
                abCandIsInter[mergeInfo->numCand] = true;
                puhInterDirNeighbours[mergeInfo->numCand] |= 2;
                mergeInfo->mvCand[2 * mergeInfo->numCand + 1] = mvCol;
                mergeInfo->refIdx[2 * mergeInfo->numCand + 1] = 0;
            }
        }
        if (abCandIsInter[mergeInfo->numCand])
            mergeInfo->numCand++;
    }

    /* combined bi-predictive merging candidates */
    if (m_cslice->slice_type == B_SLICE) {
        Ipp32s uiPriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
        Ipp32s uiPriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};
        Ipp32s limit = mergeInfo->numCand * (mergeInfo->numCand - 1);

        for (Ipp32s i = 0; i < limit && mergeInfo->numCand < MAX_NUM_MERGE_CANDS; i++) {
            Ipp32s l0idx = uiPriorityList0[i];
            Ipp32s l1idx = uiPriorityList1[i];

            if (abCandIsInter[l0idx] && (puhInterDirNeighbours[l0idx] & 1) &&
                abCandIsInter[l1idx] && (puhInterDirNeighbours[l1idx] & 2))
            {
                abCandIsInter[mergeInfo->numCand] = true;
                puhInterDirNeighbours[mergeInfo->numCand] = 3;

                mergeInfo->mvCand[2*mergeInfo->numCand] = mergeInfo->mvCand[2*l0idx];
                mergeInfo->refIdx[2*mergeInfo->numCand] = mergeInfo->refIdx[2*l0idx];

                mergeInfo->mvCand[2*mergeInfo->numCand+1] = mergeInfo->mvCand[2*l1idx+1];
                mergeInfo->refIdx[2*mergeInfo->numCand+1] = mergeInfo->refIdx[2*l1idx+1];

                if ((m_currFrame->m_refPicList[0].m_deltaPoc[mergeInfo->refIdx[2 * mergeInfo->numCand + 0]] ==
                        m_currFrame->m_refPicList[1].m_deltaPoc[mergeInfo->refIdx[2 * mergeInfo->numCand + 1]]) &&
                    (mergeInfo->mvCand[2 * mergeInfo->numCand].mvx == mergeInfo->mvCand[2 * mergeInfo->numCand + 1].mvx) &&
                    (mergeInfo->mvCand[2 * mergeInfo->numCand].mvy == mergeInfo->mvCand[2 * mergeInfo->numCand + 1].mvy))
                {
                    abCandIsInter[mergeInfo->numCand] = false;
                }
                else
                    mergeInfo->numCand++;
            }
        }
    }

    /* zero motion vector merging candidates */
    Ipp32s numRefIdx = m_cslice->num_ref_idx_l0_active;
    if (m_cslice->slice_type == B_SLICE && m_cslice->num_ref_idx_l1_active < m_cslice->num_ref_idx_l0_active)
        numRefIdx = m_cslice->num_ref_idx_l1_active;

    Ipp8s r = 0;
    Ipp8s refCnt = 0;
    while (mergeInfo->numCand < MAX_NUM_MERGE_CANDS) {
        r = (refCnt < numRefIdx) ? refCnt : 0;
        mergeInfo->mvCand[2 * mergeInfo->numCand].mvx = 0;
        mergeInfo->mvCand[2 * mergeInfo->numCand].mvy = 0;
        mergeInfo->refIdx[2 * mergeInfo->numCand]     = r;

        mergeInfo->mvCand[2 * mergeInfo->numCand + 1].mvx = 0;
        mergeInfo->mvCand[2 * mergeInfo->numCand + 1].mvy = 0;
        mergeInfo->refIdx[2 * mergeInfo->numCand + 1]     = r;

        mergeInfo->numCand++;
        refCnt++;
    }

    // check bi-pred availability and mark duplicates
    for (Ipp32s i = 0; i < mergeInfo->numCand; i++) {
        H265MV *mv = mergeInfo->mvCand + 2 * i;
        Ipp8s *refIdx = mergeInfo->refIdx + 2 * i;
        if (m_cslice->slice_type != B_SLICE || (partWidth + partHeight == 3 && refIdx[0] >= 0))
            refIdx[1] = -1;
        for (Ipp32s j = 0; j < i; j++) {
            if (IsCandFound(refIdx, mv, mergeInfo, j, 2)) {
                refIdx[0] = refIdx[1] = -1;
                break;
            }
        }
    }
}

/* ****************************************************************************** *\
FUNCTION: IsCandFound
DESCRIPTION:
\* ****************************************************************************** */

static bool IsCandFound(const Ipp8s *curRefIdx, const H265MV *curMV, const MvPredInfo<5> *mergeInfo,
                        Ipp32s candIdx, Ipp32s numRefLists)
{
    for (Ipp32s i = 0; i < numRefLists; i++) {
        if (curRefIdx[i] != mergeInfo->refIdx[2 * candIdx + i])
            return false;

        if (curRefIdx[i] >= 0)
            if ((curMV[i].mvx != mergeInfo->mvCand[2 * candIdx + i].mvx) ||
                (curMV[i].mvy != mergeInfo->mvCand[2 * candIdx + i].mvy))
            {
                return false;
            }
    }

    return true;
}

/* ****************************************************************************** *\
FUNCTION: SetupMvPredictionInfo
DESCRIPTION:
\* ****************************************************************************** */

void H265CU::SetupMvPredictionInfo(Ipp32s blockZScanIdx, Ipp32s partMode, Ipp32s curPUidx)
{
    Ipp32s numRefLists = 1 + (m_cslice->slice_type == B_SLICE);
    if ((m_data[blockZScanIdx].size == 8) && (partMode != PART_SIZE_2Nx2N) &&
        (m_data[blockZScanIdx].interDir & INTER_DIR_PRED_L0)) {
        numRefLists = 1;
    }

    m_data[blockZScanIdx].flags.mergeFlag = 0;
    m_data[blockZScanIdx].mvpNum[0] = m_data[blockZScanIdx].mvpNum[1] = 0;

    const Ipp8s *refIdx = m_data[blockZScanIdx].refIdx;
    const H265MV *mv = m_data[blockZScanIdx].mv;

    // setup merge flag and index
    for (Ipp8u i = 0; i < m_mergeCand[curPUidx].numCand; i++) {
        if (IsCandFound(refIdx, mv, m_mergeCand + curPUidx, i, numRefLists)) {
            m_data[blockZScanIdx].flags.mergeFlag = 1;
            m_data[blockZScanIdx].mergeIdx = i;
            break;
        }
    }

    if (!m_data[blockZScanIdx].flags.mergeFlag) {
        // setup mvp_idx and mvd
        for (Ipp32s listIdx = 0; listIdx < 2; listIdx++) {
            if (refIdx[listIdx] < 0)
                continue;
            MvPredInfo<2> *amvpCand = m_amvpCand[curPUidx] + 2 * refIdx[listIdx] + listIdx;
            m_data[blockZScanIdx].mvpNum[listIdx] = (Ipp8s)amvpCand->numCand;

            // choose best mv predictor
            H265MV mvd[2];
            mvd[0].mvx = mv[listIdx].mvx - amvpCand->mvCand[0].mvx;
            mvd[0].mvy = mv[listIdx].mvy - amvpCand->mvCand[0].mvy;
            Ipp32s dist0 = abs(mvd[0].mvx) + abs(mvd[0].mvy);
            Ipp32s mvpIdx = 0;
            
            if (amvpCand->numCand > 1) {
                mvd[1].mvx = mv[listIdx].mvx - amvpCand->mvCand[1].mvx;
                mvd[1].mvy = mv[listIdx].mvy - amvpCand->mvCand[1].mvy;
                Ipp32s dist1 = abs(mvd[1].mvx) + abs(mvd[1].mvy);
                if (dist1 < dist0)
                    mvpIdx = 1;
            }

            m_data[blockZScanIdx].mvpIdx[listIdx] = mvpIdx;
            m_data[blockZScanIdx].mvd[listIdx] = mvd[mvpIdx];
        }
    }
}

/* ****************************************************************************** *\
FUNCTION: GetNeighbour
DESCRIPTION:
\* ****************************************************************************** */

H265CUData *H265CU::GetNeighbour(Ipp32s &neighbourBlockZScanIdx, Ipp32s neighbourBlockColumn,
                                 Ipp32s neighbourBlockRow, Ipp32s  curBlockZScanIdx,
                                 bool isNeedTocheckCurLCU)
{
    Ipp32s maxDepth = m_par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    //Ipp32s maxCUSize = m_par->MaxCUSize;
    Ipp32s minCUSize = m_par->MinCUSize;
    Ipp32s minTUSize = m_par->MinTUSize;
    Ipp32s frameWidthInSamples = m_par->Width;
    Ipp32s frameHeightInSamples = m_par->Height;
    //Ipp32s frameWidthInLCUs = m_par->PicWidthInCtbs;
    Ipp32s sliceStartAddr = m_cslice->slice_segment_address;
    Ipp32s tmpNeighbourBlockRow = neighbourBlockRow;
    Ipp32s tmpNeighbourBlockColumn = neighbourBlockColumn;
    Ipp32s neighbourLCUAddr = 0;
    H265CUData* neighbourLCU;


    if ((((Ipp32s)m_ctbPelX + neighbourBlockColumn * minTUSize) >= frameWidthInSamples) ||
        (((Ipp32s)m_ctbPelY + neighbourBlockRow * minTUSize) >= frameHeightInSamples))
    {
        neighbourBlockZScanIdx = 256; /* doesn't matter */
        return NULL;
    }

    if ((((Ipp32s)m_ctbPelX + neighbourBlockColumn * minTUSize) < 0) ||
        (((Ipp32s)m_ctbPelY + neighbourBlockRow * minTUSize) < 0))
    {
        neighbourBlockZScanIdx = 256; /* doesn't matter */
        return NULL;
    }

    if (neighbourBlockColumn < 0)
    {
        tmpNeighbourBlockColumn += numMinTUInLCU;
    }
    else if (neighbourBlockColumn >= numMinTUInLCU)
    {
        tmpNeighbourBlockColumn -= numMinTUInLCU;
    }

    if (neighbourBlockRow < 0)
    {
        /* Constrained motion data compression */
        if(0) if ((minCUSize == 8) && (minTUSize == 4))
        {
            tmpNeighbourBlockColumn = ((tmpNeighbourBlockColumn >> 1) << 1) + ((tmpNeighbourBlockColumn >> 1) & 1);
        }

        tmpNeighbourBlockRow += numMinTUInLCU;
    }
    else if (neighbourBlockRow >= numMinTUInLCU)
    {
        tmpNeighbourBlockRow -= numMinTUInLCU;
    }

    neighbourBlockZScanIdx = h265_scan_r2z[maxDepth][(tmpNeighbourBlockRow << maxDepth) + tmpNeighbourBlockColumn];

    if (neighbourBlockRow < 0)
    {
        if (neighbourBlockColumn < 0)
        {
            neighbourLCU = m_aboveLeft;
            neighbourLCUAddr = m_aboveLeftAddr;
        }
        else if (neighbourBlockColumn >= numMinTUInLCU)
        {
            neighbourLCU = m_aboveRight;
            neighbourLCUAddr = m_aboveRightAddr;
        }
        else
        {
            neighbourLCU = m_above;
            neighbourLCUAddr = m_aboveAddr;
        }
    }
    else if (neighbourBlockRow >= numMinTUInLCU)
    {
        neighbourLCU = NULL;
    }
    else
    {
        if (neighbourBlockColumn < 0)
        {
            neighbourLCU = m_left;
            neighbourLCUAddr = m_leftAddr;
        }
        else if (neighbourBlockColumn >= numMinTUInLCU)
        {
            neighbourLCU = NULL;
        }
        else
        {
            neighbourLCU = m_data;
            neighbourLCUAddr = m_ctbAddr;

            if ((isNeedTocheckCurLCU) && (curBlockZScanIdx <= neighbourBlockZScanIdx))
            {
                neighbourLCU = NULL;
            }
        }
    }

    if (neighbourLCU != NULL)
    {
        if (neighbourLCUAddr < sliceStartAddr)
        {
            neighbourLCU = NULL;
        }
    }

    return neighbourLCU;
}

/* ****************************************************************************** *\
FUNCTION: GetColMvp
DESCRIPTION:
\* ****************************************************************************** */

bool H265CU::GetTempMvPred(const H265CUData *currPb, Ipp32s xPb, Ipp32s yPb, Ipp32s nPbW,
                           Ipp32s nPbH, Ipp32s listIdx, Ipp32s refIdx, H265MV *mvLxCol)
{
    H265Frame *colPic = (m_cslice->slice_type == P_SLICE || m_cslice->collocated_from_l0_flag)
        ? m_currFrame->m_refPicList[0].m_refFrames[m_cslice->collocated_ref_idx]
        : m_currFrame->m_refPicList[1].m_refFrames[m_cslice->collocated_ref_idx];

    H265CUData *colBr = NULL;
    Ipp32s xColBr = xPb + nPbW;
    Ipp32s yColBr = yPb + nPbH;
    Ipp32s maxDepth = m_par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << m_par->MaxCUDepth;
    Ipp32s compressionShift = (m_par->Log2MinTUSize < 4) ? 4 - m_par->Log2MinTUSize : 0;

    if ((Ipp32s)m_ctbPelX + xColBr * m_par->MinTUSize < m_par->Width &&
        (Ipp32s)m_ctbPelY + yColBr * m_par->MinTUSize < m_par->Height &&
        yColBr < numMinTUInLCU) {

        xColBr = (xColBr >> compressionShift) << compressionShift;
        yColBr = (yColBr >> compressionShift) << compressionShift;

        if (xColBr < numMinTUInLCU) {
            colBr = colPic->cu_data + (m_ctbAddr << m_par->Log2NumPartInCU);
            colBr += h265_scan_r2z[maxDepth][(yColBr << maxDepth) + xColBr];
        }
        else {
            colBr = colPic->cu_data + ((m_ctbAddr + 1) << m_par->Log2NumPartInCU);
            colBr += h265_scan_r2z[maxDepth][(yColBr << maxDepth) + xColBr - numMinTUInLCU];
        }

        if (!GetColMv(currPb, listIdx, refIdx, colPic, colBr, mvLxCol))
            colBr = NULL;
    }

    if (colBr)
        return true;

    Ipp32s xColCtr = xPb + (nPbW >> 1);
    Ipp32s yColCtr = yPb + (nPbH >> 1);
    xColCtr = (xColCtr >> compressionShift) << compressionShift;
    yColCtr = (yColCtr >> compressionShift) << compressionShift;

    H265CUData *colCtr = colPic->cu_data + (m_ctbAddr << m_par->Log2NumPartInCU);
    colCtr += h265_scan_r2z[maxDepth][(yColCtr << maxDepth) + xColCtr];

    return GetColMv(currPb, listIdx, refIdx, colPic, colCtr, mvLxCol);
}


bool H265CU::GetColMv(const H265CUData *currPb, Ipp32s listIdxCurr, Ipp32s refIdxCurr,
                      const H265Frame *colPic, const H265CUData *colPb, H265MV *mvLxCol)
{
    //Ipp8u isCurrRefLongTerm, isColRefLongTerm;
    //Ipp32s currRefTb, colRefTb;
    //H265MV cMvPred;
    //Ipp32s minTUSize = m_par->MinTUSize;

    if (colPb->predMode == MODE_INTRA)
        return false; // availableFlagLXCol=0

    Ipp32s listCol;
    if (colPb->refIdx[0] < 0)
        listCol = 1;
    else if (colPb->refIdx[1] < 0)
        listCol = 0;
    else if (m_currFrame->m_allRefFramesAreFromThePast)
        listCol = listIdxCurr;
    else
        listCol = m_cslice->collocated_from_l0_flag;

    H265MV mvCol = colPb->mv[listCol];
    Ipp8s refIdxCol = colPb->refIdx[listCol];

    Ipp32s isLongTermCurr = m_currFrame->m_refPicList[listIdxCurr].m_isLongTermRef[refIdxCurr];
    Ipp32s isLongTermCol  = colPic->m_refPicList[listCol].m_isLongTermRef[refIdxCol];

    if (isLongTermCurr != isLongTermCol)
        return false; // availableFlagLXCol=0

    Ipp32s colPocDiff  = colPic->m_refPicList[listCol].m_deltaPoc[refIdxCol];
    Ipp32s currPocDiff = m_currFrame->m_refPicList[listIdxCurr].m_deltaPoc[refIdxCurr];

    if (isLongTermCurr || currPocDiff == colPocDiff) {
        *mvLxCol = mvCol;
    }
    else {
        Ipp32s scale = GetDistScaleFactor(currPocDiff, colPocDiff);
        mvLxCol->mvx = (Ipp16s)Saturate(-32768, 32767, (scale * mvCol.mvx + 127 + (scale * mvCol.mvx < 0)) >> 8);
        mvLxCol->mvy = (Ipp16s)Saturate(-32768, 32767, (scale * mvCol.mvy + 127 + (scale * mvCol.mvy < 0)) >> 8);
    }

    return true; // availableFlagLXCol=1
}

void H265CU::GetMvpCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s refPicListIdx, Ipp32s refIdx,
                        Ipp32s partMode, Ipp32s partIdx, Ipp32s cuSize, MvPredInfo<2> *info)
{
    Ipp32s maxDepth = m_par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s topLeftBlockZScanIdx;
    Ipp32s topLeftRasterIdx;
    Ipp32s topLeftRow;
    Ipp32s topLeftColumn;
    Ipp32s belowLeftCandBlockZScanIdx;
    Ipp32s leftCandBlockZScanIdx = 0;
    Ipp32s aboveRightCandBlockZScanIdx;
    Ipp32s aboveCandBlockZScanIdx = 0;
    Ipp32s aboveLeftCandBlockZScanIdx = 0;
    H265CUData* belowLeftCandLCU = NULL;
    H265CUData* leftCandLCU = NULL;
    H265CUData* aboveRightCandLCU = NULL;
    H265CUData* aboveCandLCU = NULL;
    H265CUData* aboveLeftCandLCU = NULL;
    bool bAdded = false;
    Ipp32s partWidth, partHeight, partX, partY;

    info->numCand = 0;

    if (refIdx < 0)
        return;

    GetPartOffsetAndSize(partIdx, partMode, cuSize, partX, partY, partWidth, partHeight);

    topLeftRasterIdx = h265_scan_z2r[maxDepth][topLeftCUBlockZScanIdx] + partX + numMinTUInLCU * partY;
    topLeftRow = topLeftRasterIdx >> maxDepth;
    topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
    topLeftBlockZScanIdx = h265_scan_r2z[maxDepth][topLeftRasterIdx];

    /* Get Spatial MV */

    /* Left predictor search */

    belowLeftCandLCU = GetNeighbour(belowLeftCandBlockZScanIdx, topLeftColumn - 1,
                                    topLeftRow + partHeight, topLeftBlockZScanIdx, true);
    bAdded = AddMvpCand(info, belowLeftCandLCU, belowLeftCandBlockZScanIdx, refPicListIdx, refIdx, false);

    if (!bAdded) {
        leftCandLCU = GetNeighbour(leftCandBlockZScanIdx, topLeftColumn - 1,
                                   topLeftRow + partHeight - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMvpCand(info, leftCandLCU, leftCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (!bAdded) {
        bAdded = AddMvpCand(info, belowLeftCandLCU, belowLeftCandBlockZScanIdx, refPicListIdx, refIdx, true);
        if (!bAdded)
            bAdded = AddMvpCand(info, leftCandLCU, leftCandBlockZScanIdx, refPicListIdx, refIdx, true);
    }

    /* Above predictor search */

    aboveRightCandLCU = GetNeighbour(aboveRightCandBlockZScanIdx, topLeftColumn + partWidth,
                                     topLeftRow - 1, topLeftBlockZScanIdx, true);
    bAdded = AddMvpCand(info, aboveRightCandLCU, aboveRightCandBlockZScanIdx, refPicListIdx, refIdx, false);

    if (!bAdded) {
        aboveCandLCU = GetNeighbour(aboveCandBlockZScanIdx, topLeftColumn + partWidth - 1,
                                    topLeftRow - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMvpCand(info, aboveCandLCU, aboveCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (!bAdded) {
        aboveLeftCandLCU = GetNeighbour(aboveLeftCandBlockZScanIdx, topLeftColumn - 1,
                                        topLeftRow - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMvpCand(info, aboveLeftCandLCU, aboveLeftCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (info->numCand == 2) {
        bAdded = true;
    }
    else {
        bAdded = ((belowLeftCandLCU != NULL) &&
                  (belowLeftCandLCU[belowLeftCandBlockZScanIdx].predMode != MODE_INTRA));
        if (!bAdded)
            bAdded = ((leftCandLCU != NULL) &&
                      (leftCandLCU[leftCandBlockZScanIdx].predMode != MODE_INTRA));
    }

    if (!bAdded) {
        bAdded = AddMvpCand(info, aboveRightCandLCU, aboveRightCandBlockZScanIdx, refPicListIdx, refIdx, true);
        if (!bAdded)
            bAdded = AddMvpCand(info, aboveCandLCU, aboveCandBlockZScanIdx, refPicListIdx, refIdx, true);

        if (!bAdded)
            bAdded = AddMvpCand(info, aboveLeftCandLCU, aboveLeftCandBlockZScanIdx, refPicListIdx, refIdx, true);
    }

    if (info->numCand == 2 && info->mvCand[0] == info->mvCand[1])
        info->numCand = 1;

    if (info->numCand < 2) {
        if (m_par->TMVPFlag) {
            H265CUData *currPb = m_data + topLeftBlockZScanIdx;
            H265MV mvCol;
            bool availFlagCol = GetTempMvPred(currPb, topLeftColumn, topLeftRow, partWidth,
                                              partHeight, refPicListIdx, refIdx, &mvCol);
            if (availFlagCol)
                info->mvCand[info->numCand++] = mvCol;
        }
    }

    while (info->numCand < 2) {
        info->mvCand[info->numCand].mvx = 0;
        info->mvCand[info->numCand].mvy = 0;
        info->numCand++;
    }

    return;
}

static Ipp8u isEqualRef(RefPicList *ref_list, Ipp8s idx1, Ipp8s idx2)
{
    if (idx1 < 0 || idx2 < 0) return 0;
    return ref_list->m_refFrames[idx1] == ref_list->m_refFrames[idx2];
}


/* ****************************************************************************** *\
FUNCTION: GetPuMvPredictorInfo
DESCRIPTION: collects MV predictors.
    Merge MV refidx are interleaved,
    amvp refidx are separated.
\* ****************************************************************************** */

void H265CU::GetAmvpCand(Ipp32s topLeftBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx,
                         MvPredInfo<2> amvpInfo[2 * MAX_NUM_REF_IDX])
{
    Ipp32s cuSizeInMinTu = m_data[topLeftBlockZScanIdx].size >> m_par->Log2MinTUSize;
    Ipp32s numRefLists = 1 + (m_cslice->slice_type == B_SLICE);

    for (Ipp32s refList = 0; refList < numRefLists; refList++) {
        for (Ipp8s refIdx = 0; refIdx < m_cslice->num_ref_idx[refList]; refIdx++) {
            amvpInfo[refIdx * 2 + refList].numCand = 0;
            GetMvpCand(topLeftBlockZScanIdx, refList, refIdx, partMode, partIdx,
                       cuSizeInMinTu, amvpInfo + refIdx * 2 + refList);

            for (Ipp32s i = 0; i < amvpInfo[refIdx * 2 + refList].numCand; i++)
                amvpInfo[refIdx * 2 + refList].refIdx[i] = refIdx;
        }
    }
}

// OBSOLETE after COST_CHROMA was added
//CostType H265CU::CalcCostSkipExperimental(Ipp32u absPartIdx, Ipp8u depth)
//{
//    Ipp8u size = (Ipp8u)(m_par->MaxCUSize>>depth);
////    Ipp32u num_parts = ( m_par->NumPartInCU >> (depth<<1) );
//
////    Ipp32s maxDepth = m_par->Log2MaxCUSize - m_par->Log2MinTUSize;
////    Ipp32s numMinTUInLCU = 1 << maxDepth;
////    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
////    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
////    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);
//    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
//    Ipp32s offsetRec  = GetLumaOffset(m_par, absPartIdx, m_pitchRec);
//    Ipp32s offsetSrc  = GetLumaOffset(m_par, absPartIdx, m_pitchSrc);
//    Ipp32s offsetPred = GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
//    IppiSize roiSize = { size, size };
//
//    Ipp32s CUSizeInMinTU = size >> m_par->Log2MinTUSize;
//
//    MVPInfo mergeInfo;
//    GetMergeCand(absPartIdx, PART_SIZE_2Nx2N, 0, CUSizeInMinTU, &mergeInfo);
//
//    PixType *src = m_ySrc + offsetSrc;
//
//    CABAC_CONTEXT_H265 bestCtx[NUM_CABAC_CONTEXT];
//    CABAC_CONTEXT_H265 initCtx[NUM_CABAC_CONTEXT];
//    m_bsf->CtxSave(initCtx, 0, NUM_CABAC_CONTEXT);
//
//    m_interPredPtr = m_interPredMerge;
//    m_interResidualsYPtr = m_interResidualsYMerge;
//
//    Ipp8u trDepthMin, trDepthMax;
//    GetTrDepthMinMax(m_par, absPartIdx, depth, PART_SIZE_2Nx2N, &trDepthMin, &trDepthMax);
//
//    H265CUData *dataBest = m_dataTemp2;
//    H265CUData *dataTest = m_data + absPartIdx;
//    dataTest->depth = depth;
//    dataTest->size = size;
//    dataTest->partSize = PART_SIZE_2Nx2N;
//    dataTest->predMode = MODE_INTER;
//    dataTest->trIdx = 0;
//    dataTest->qp = m_par->QP;
//    dataTest->cbf[0] = 0;
//    dataTest->cbf[1] = 0;
//    dataTest->cbf[2] = 0;
//    dataTest->intraLumaDir = 0;
//    dataTest->intraChromaDir = 0;
//    dataTest->mvpIdx[2];
//    dataTest->mvpNum[2];
//    dataTest->mvd[0] = MV_ZERO;
//    dataTest->mvd[1] = MV_ZERO;
//    dataTest->transformSkipFlag[0] = 0;
//    dataTest->transformSkipFlag[1] = 0;
//    dataTest->transformSkipFlag[2] = 0;
//    dataTest->flags.mergeFlag = 1;
//    dataTest->flags.ipcmFlag = 0;
//    dataTest->flags.transquantBypassFlag = 0;
//    dataTest->flags.skippedFlag = 0;
//
//    CostType costBest = COST_MAX;
//    Ipp32s mergeIdxBest = 0;
//    Ipp8u interPredIdcBest = 0;
//    Ipp8u skipFlagBest = 0;
//
//    for (Ipp32s i = 0; i < mergeInfo.numCand; i++) {
//        Ipp8s *refIdx = mergeInfo.refIdx + 2 * i;
//        if (m_cslice->slice_type != B_SLICE)
//            refIdx[1] = -1;
//        H265MV *mv = mergeInfo.mvCand + 2 * i;
//
//        if (IsDuplicatedMergeCand(&mergeInfo, i))
//            continue;
//
//        Ipp8u interPredIdc = 0;
//        if (refIdx[0] >= 0)
//            interPredIdc |= INTER_DIR_PRED_L0;
//        if (refIdx[1] >= 0)
//            interPredIdc |= INTER_DIR_PRED_L1;
//
//        // update CU dataTest fields
//        dataTest->cbf[0] = 0;
//        dataTest->interDir = interPredIdc;
//        dataTest->mergeIdx = (Ipp8u)i;
//        dataTest->mv[0] = mv[0];
//        dataTest->mv[1] = mv[1];
//        dataTest->refIdx[0] = refIdx[0];
//        dataTest->refIdx[1] = refIdx[1];
//        dataTest->flags.skippedFlag = 1;
//        for (Ipp32u j = 1; j < numParts; j++) small_memcpy(dataTest + j, dataTest, sizeof(*dataTest));
//
//        InterPredCu<TEXT_LUMA>(absPartIdx, depth, m_interPredPtr[depth], MAX_CU_SIZE);
//
//        CostType costSkip = TuSse(src, m_pitchSrc, m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE, size);
//        m_bsf->Reset();
//        EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
//        costSkip += BIT_COST_INTER(m_bsf->GetNumBits());
//
//        if (costBest > costSkip) {
//            costBest = costSkip;
//            mergeIdxBest = i;
//            interPredIdcBest = interPredIdc;
//            skipFlagBest = 1;
//            m_bsf->CtxSave(bestCtx, 0, NUM_CABAC_CONTEXT);
//            small_memcpy(dataBest, dataTest, numParts * sizeof(*dataTest));
//            ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE, m_recLumaSaveTu[depth], size, roiSize);
//            ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE, m_interPredMergeBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
//        }
//
//        TuDiff(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE, src, m_pitchSrc,
//               m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE, size);
//
//        Ipp8u nz = 0;
//        CostType costMerge = 0;
//        Ipp32u numTrParts = (m_par->NumPartInCU >> ((depth + trDepthMin) << 1));
//        Ipp8u cbf[256];
//        memset(cbf + absPartIdx, 0, numParts);
//        for (Ipp32u j = 0; j < numParts; j++) dataTest[j].flags.skippedFlag = 0;
//
//        for (Ipp32u pos = 0; pos < numParts; pos += numTrParts) {
//            Ipp8u nzPart;
//            CostType costPart;
//            TuGetSplitInter(absPartIdx + pos, (absPartIdx + pos) * 16, trDepthMin, trDepthMax,
//                            &nzPart, &costPart, cbf);
//            nz |= nzPart;
//            costMerge += costPart;
//        }
//
//        if (trDepthMin > 0) {
//            Ipp8u val = (1 << trDepthMin) - 1;
//            (nz)
//                ? (cbf[absPartIdx] |= val)
//                : (cbf[absPartIdx] = 0);
//        }
//
//        for (Ipp32u j = absPartIdx; j < absPartIdx + numParts; j++) dataTest[j].cbf[0] = cbf[j];
//        
//        m_bsf->Reset();
//        m_bsf->CtxRestore(initCtx, 0, NUM_CABAC_CONTEXT);
//        EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
//        costMerge += BIT_COST_INTER(m_bsf->GetNumBits());
//
//        if (costBest > costMerge) {
//            costBest = costMerge;
//            mergeIdxBest = i;
//            interPredIdcBest = interPredIdc;
//            skipFlagBest = 0;
//            m_bsf->CtxSave(bestCtx, 0, NUM_CABAC_CONTEXT);
//            small_memcpy(dataBest, dataTest, numParts * sizeof(*dataTest));
//            ippiCopy_8u_C1R(m_yRec + offsetRec, m_pitchRec, m_recLumaSaveTu[depth], size, roiSize);
//            ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE, m_interPredMergeBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
//            ippiCopy_16s_C1R(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE * 2,
//                             m_interResidualsYMergeBest[depth] + offsetPred, MAX_CU_SIZE * 2, roiSize);
//        }
//
//        m_bsf->CtxRestore(initCtx, 0, NUM_CABAC_CONTEXT);
//    }
//
//    small_memcpy(dataTest, dataBest, numParts * sizeof(*dataBest));
//    m_bsf->CtxRestore(bestCtx, 0, NUM_CABAC_CONTEXT);
//    ippiCopy_8u_C1R(m_recLumaSaveTu[depth], size, m_yRec + offsetRec, m_pitchRec, roiSize);
//
//    return costBest;
//}



// MV functions
Ipp32s operator == (const H265MV &mv1, const H265MV &mv2)
{ return mv1.mvx == mv2.mvx && mv1.mvy == mv2.mvy; }

Ipp32s operator != (const H265MV &mv1, const H265MV &mv2)
{ return !(mv1.mvx == mv2.mvx && mv1.mvy == mv2.mvy); }

inline Ipp32s qcost(const H265MV *mv)
{
    Ipp32s dx = ABS(mv->mvx), dy = ABS(mv->mvy);
    //return dx*dx + dy*dy;
    //Ipp32s i;
    //for(i=0; (dx>>i)!=0; i++);
    //dx = i;
    //for(i=0; (dy>>i)!=0; i++);
    //dy = i;
    return (dx + dy);
}

inline Ipp32s qdiff(const H265MV *mv1, const H265MV *mv2)
{
    H265MV mvdiff = {
        static_cast<Ipp16s>(mv1->mvx - mv2->mvx),
        static_cast<Ipp16s>(mv1->mvy - mv2->mvy)
};
    return qcost(&mvdiff);
}

inline Ipp32s qdist(const H265MV *mv1, const H265MV *mv2)
{
    Ipp32s dx = ABS(mv1->mvx - mv2->mvx);
    Ipp32s dy = ABS(mv1->mvy - mv2->mvy);
    return (dx + dy);
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
