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

extern int cmMvW[PU_MAX];
extern int cmMvH[PU_MAX];
extern Ipp32u dist32x32Pitch;
extern Ipp32u dist32x16Pitch;
extern Ipp32u dist16x32Pitch;
extern Ipp32u dist16x16Pitch;
extern Ipp32u dist16x8Pitch;
extern Ipp32u dist8x16Pitch;
extern Ipp32u dist8x8Pitch;
extern Ipp32u dist8x4Pitch;
extern Ipp32u dist4x8Pitch;
extern CmMbDist32 ** cmDist32x32[2];
extern CmMbDist32 ** cmDist32x16[2];
extern CmMbDist32 ** cmDist16x32[2];
extern CmMbDist16 ** cmDist16x16[2];
extern CmMbDist16 ** cmDist16x8[2];
extern CmMbDist16 ** cmDist8x16[2];
extern CmMbDist16 ** cmDist8x8[2];
extern CmMbDist16 ** cmDist8x4[2];
extern CmMbDist16 ** cmDist4x8[2];

extern Ipp32u intraPitch[2];
extern CmMbIntraDist * cmMbIntraDist[2];
extern Ipp32u mv32x32Pitch;
extern mfxI16Pair ** cmMv32x32[2];
extern Ipp32u mv32x16Pitch;
extern mfxI16Pair ** cmMv32x16[2];
extern Ipp32u mv16x32Pitch;
extern mfxI16Pair ** cmMv16x32[2];
extern Ipp32u mv16x16Pitch;
extern mfxI16Pair ** cmMv16x16[2];
extern Ipp32u mv16x8Pitch;
extern mfxI16Pair ** cmMv16x8[2];
extern Ipp32u mv8x16Pitch;
extern mfxI16Pair ** cmMv8x16[2];
extern Ipp32u mv8x8Pitch;
extern mfxI16Pair ** cmMv8x8[2];
extern Ipp32u mv8x4Pitch;
extern mfxI16Pair ** cmMv8x4[2];
extern Ipp32u mv4x8Pitch;
extern mfxI16Pair ** cmMv4x8[2];
extern Ipp32s cmCurIdx;
extern Ipp32s cmNextIdx;

extern CmEvent * ready16x16Event;
extern CmEvent * readyMV32x32Event;
extern CmEvent * readyRefine32x32Event;
extern CmEvent * readyRefine32x16Event;
extern CmEvent * readyRefine16x32Event;

#define NO_TRANSFORM_SPLIT_INTRAPRED_STAGE1 0

Ipp32s GetLumaOffset(H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch) {
    Ipp32s maxDepth = par->MaxTotalDepth;
    Ipp32s puRasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
    Ipp32s puStartRow = puRasterIdx >> maxDepth;
    Ipp32s puStartColumn = puRasterIdx & (par->NumMinTUInMaxCU - 1);

    return (puStartRow * pitch + puStartColumn) << par->Log2MinTUSize;
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
    return (((cULeft? cULeft[lPartIdx].qp: GetLastCodedQP( currAbsIdxInLcu )) + (cUAbove? cUAbove[aPartIdx].qp: GetLastCodedQP( currAbsIdxInLcu )) + 1) >> 1);
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
    // not implemented
    return NULL;
}

H265CUData* H265CU::GetQpMinCuAbove( Ipp32u& aPartUnitIdx, Ipp32u currAbsIdxInLCU, bool enforceSliceRestriction, bool enforceDependentSliceRestriction )
{
    // not implemented
    return NULL;
}

Ipp8s H265CU::GetLastCodedQP( Ipp32u absPartIdx )
{
    // not implemented
    return 0;
}

void H265CU::InitCu(H265VideoParam *_par, H265CUData *_data, H265CUData *_dataTemp, Ipp32s cuAddr,
                    PixType *_y, PixType *_uv, Ipp32s _pitch, H265Frame *currFrame, H265BsFake *_bsf,
                    H265Slice *_cslice, Ipp32s initializeDataFlag, const Ipp8u *logMvCostTable) 
{
    m_par = _par;

    m_cslice = _cslice;
    m_bsf = _bsf;
    m_logMvCostTable = logMvCostTable + (1 << 15);
    m_dataSave = m_data = _data;
    m_dataBest = _dataTemp;
    m_dataTemp = _dataTemp + (MAX_TOTAL_DEPTH << m_par->Log2NumPartInCU);
    m_dataTemp2 = _dataTemp + ((MAX_TOTAL_DEPTH*2) << m_par->Log2NumPartInCU);
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

    if (initializeDataFlag) {
        m_rdOptFlag = m_cslice->rd_opt_flag;
        m_rdLambda = m_cslice->rd_lambda;

        //kolya
        m_rdLambdaSqrt = m_cslice->rd_lambda_sqrt;

        m_rdLambdaInter = m_cslice->rd_lambda_inter;
        m_rdLambdaInterMv = m_cslice->rd_lambda_inter_mv;

        if ( m_numPartition > 0 )
        {
            memset (m_data, 0, sizeof(H265CUData));
            m_data->partSize = PART_SIZE_NONE;
            m_data->predMode = MODE_NONE;
            m_data->size = (Ipp8u)m_par->MaxCUSize;
            m_data->mvpIdx[0] = -1;
            m_data->mvpIdx[1] = -1;
            m_data->mvpNum[0] = -1;
            m_data->mvpNum[1] = -1;
            m_data->qp = m_par->QP;
            m_data->_flags = 0;
            m_data->intraLumaDir = INTRA_DC;
            m_data->intraChromaDir = INTRA_DM_CHROMA;
            m_data->cbf[0] = m_data->cbf[1] = m_data->cbf[2] = 0;

            for (Ipp32u i = 1; i < m_numPartition; i++)
                small_memcpy(m_data+i,m_data,sizeof(H265CUData));
        }
    }

    // Setting neighbor CU
    m_left        = NULL;
    m_above       = NULL;
    m_aboveLeft   = NULL;
    m_aboveRight  = NULL;
    m_leftAddr = - 1;
    m_aboveAddr = -1;
    m_aboveLeftAddr = -1;
    m_aboveRightAddr = -1;

    Ipp32u widthInCU = m_par->PicWidthInCtbs;
    if ( m_ctbAddr % widthInCU )
    {
        m_left = m_data - m_par->NumPartInCU;
        m_leftAddr = m_ctbAddr - 1;
    }

    if ( m_ctbAddr / widthInCU )
    {
        m_above = m_data - (widthInCU << m_par->Log2NumPartInCU);
        m_aboveAddr = m_ctbAddr - widthInCU;
    }

    if ( m_left && m_above )
    {
        m_aboveLeft = m_data - (widthInCU << m_par->Log2NumPartInCU) - m_par->NumPartInCU;
        m_aboveLeftAddr = m_ctbAddr - widthInCU - 1;
    }

    if ( m_above && ( (m_ctbAddr%widthInCU) < (widthInCU-1) )  )
    {
        m_aboveRight = m_data - (widthInCU << m_par->Log2NumPartInCU) + m_par->NumPartInCU;
        m_aboveRightAddr = m_ctbAddr - widthInCU + 1;
    }

    m_bakAbsPartIdxCu = 0;
    m_bakAbsPartIdx = 0;
    m_bakChromaOffset = 0;
    m_isRdoq = m_par->RDOQFlag ? true : false;

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
//            for (Ipp32s i = 0; i < GetNumPartInter(absPartIdx); i++)
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
//            for (Ipp32s i = 0; i < GetNumPartInter(absPartIdx); i++)
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
    CABAC_CONTEXT_H265 ctxSave[4][NUM_CABAC_CONTEXT];
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    Ipp32u lPelX   = m_ctbPelX +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
    Ipp32u rPelX   = lPelX + (m_par->MaxCUSize>>depth)  - 1;
    Ipp32u tPelY   = m_ctbPelY +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
    Ipp32u bPelY   = tPelY + (m_par->MaxCUSize>>depth) - 1;

    CostType costBest = COST_MAX;

    Ipp32s subsize = m_par->MaxCUSize >> (depth + 1);
    subsize *= subsize;
    Ipp32s widthCu = m_par->MaxCUSize >> depth;
    IppiSize roiSizeCu = {widthCu, widthCu};
    Ipp32s offsetLumaCu = GetLumaOffset(m_par, absPartIdx, m_pitchRec);

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

    if (splitMode != SPLIT_MUST) {
        if (m_depthMin == MAX_TOTAL_DEPTH)
            m_depthMin = depth; // lowest depth for branch where not SPLIT_MUST

        m_data = m_dataTemp + (depth << m_par->Log2NumPartInCU);

        bool tryIntra = CheckGpuIntraCost(lPelX, tPelY, depth);

        if (tryIntra) {
            // Try Intra mode
            Ipp32s intraSplit = (depth == m_par->MaxCUDepth - m_par->AddCUDepth &&
                depth + 1 <= m_par->MaxCUDepth) ? 1 : 0;
            for (Ipp8u trDepth = 0; trDepth < intraSplit + 1; trDepth++) {
                Ipp8u partSize = (Ipp8u)(trDepth == 1 ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
                CostType costBestPu[4] = { COST_MAX, COST_MAX, COST_MAX, COST_MAX };
                CostType costBestPuSum = 0;

                if (m_rdOptFlag && trDepth)
                    m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);

                for (Ipp32s i = 0; i < ((trDepth == 1) ? 4 : 1); i++) {
                    Ipp32s absPartIdxTu = absPartIdx + (numParts >> 2) * i;
                    Ipp32u lPelXTu = m_ctbPelX +
                        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdxTu] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
                    Ipp32u tPelYTu = m_ctbPelY +
                        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdxTu] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);

                    if (lPelXTu >= m_par->Width || tPelYTu >= m_par->Height) {
                        m_data = m_dataBest + ((depth + trDepth) << m_par->Log2NumPartInCU);
                        FillSubPart(absPartIdxTu, depth, trDepth, partSize, 0, m_par->QP);
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

            Ipp8u chromaDirBest = INTRA_DM_CHROMA;
            m_data = m_dataBest + (depth << m_par->Log2NumPartInCU);

            if (m_cslice->slice_type == I_SLICE && (m_par->AnalyseFlags & HEVC_ANALYSE_CHROMA)) {
                Ipp8u allowedChromaDir[NUM_CHROMA_MODE];
                CopySubPartTo(m_dataSave, absPartIdx, depth, 0);
                m_data = m_dataSave;
                GetAllowedChromaDir(absPartIdx, allowedChromaDir);
                H265CUData *data_b = m_dataBest + (depth << m_par->Log2NumPartInCU);

                CostType costChromaBest = COST_MAX, costTemp;

                for (Ipp8u chromaDir = 0; chromaDir < NUM_CHROMA_MODE; chromaDir++) {
                    if (allowedChromaDir[chromaDir] == 34) continue;
                    if (m_rdOptFlag && chromaDir)
                        m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                    for(Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++)
                        data_b[i].intraChromaDir = m_data[i].intraChromaDir =
                            allowedChromaDir[chromaDir];
                    EncAndRecChroma(absPartIdx, offset >> 2, m_data[absPartIdx].depth, NULL, &costTemp);
                    if (costChromaBest >= costTemp) {
                        costChromaBest = costTemp;
                        chromaDirBest = allowedChromaDir[chromaDir];
                    }
                }
                m_data = m_dataSave;
                for(Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++)
                    data_b[i].intraChromaDir = m_data[i].intraChromaDir = chromaDirBest;
                if (m_rdOptFlag)
                    m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                EncAndRecChroma(absPartIdx, offset >> 2, m_data[absPartIdx].depth, NULL, &costTemp);
                if (m_rdOptFlag)
                    m_bsf->CtxSave(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
                    costBest += costTemp;
            }
        }

        // Try Skip mode
        if (m_cslice->slice_type != I_SLICE) {
            m_data = m_dataSave;

            if (m_rdOptFlag)
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);

            //EXPERIMENTAL SKIP CHECK: CostType costSkip = CalcCostSkipExperimental(absPartIdx, depth);

            CostType costSkip = CalcCostSkip(absPartIdx, depth);
            if (m_rdOptFlag) {
                m_bsf->Reset();
                m_data = m_dataSave;
                EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
                costSkip += BIT_COST_INTER(m_bsf->GetNumBits());
            }

            if (costBest > costSkip) {
                small_memcpy(m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU) + absPartIdx,
                    m_dataSave + absPartIdx,
                    numParts * sizeof(H265CUData));
                costBest = costSkip;
                ippiCopy_8u_C1R(m_yRec + offsetLumaCu, m_pitchRec, m_recLumaSaveCu[depth], widthCu, roiSizeCu);
                if (m_rdOptFlag)
                    m_bsf->CtxSave(ctxSave[2], 0, NUM_CABAC_CONTEXT);
            }
            else {
                m_data = m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU);
                m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(m_recLumaSaveCu[depth], widthCu, m_yRec + offsetLumaCu, m_pitchRec, roiSizeCu);
            }
        }

        // Try inter mode
        if (m_cslice->slice_type != I_SLICE) {
            CostType costInter;
            m_data = m_dataSave;

            if (m_rdOptFlag)
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);

            costInter = MeCu(absPartIdx, depth, offset) * 1.;

            if (costBest > costInter) {
                small_memcpy(m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU) + absPartIdx,
                    m_dataSave + absPartIdx,
                    numParts * sizeof(H265CUData));
                costBest = costInter;

               ippiCopy_8u_C1R(m_yRec + offsetLumaCu, m_pitchRec, m_recLumaSaveCu[depth], widthCu, roiSizeCu);

                if (m_rdOptFlag)
                    m_bsf->CtxSave(ctxSave[2], 0, NUM_CABAC_CONTEXT);
            }
            else {
                m_data = m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU);
                m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(m_recLumaSaveCu[depth], widthCu, m_yRec + offsetLumaCu, m_pitchRec, roiSizeCu);

                //EXPERIMENTAL SKIP CHECK:
                //Ipp32s offsetPred = GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
                //if (m_data[absPartIdx].flags.skippedFlag) {
                //    ippiCopy_8u_C1R(m_interPredMergeBest[depth] + offsetPred, MAX_CU_SIZE,
                //                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSizeCu);
                //}
                //else if (m_data[absPartIdx].flags.mergeFlag && m_data[absPartIdx].partSize == PART_SIZE_2Nx2N) {
                //    ippiCopy_16s_C1R(m_interResidualsYMergeBest[depth] + offsetPred, MAX_CU_SIZE * 2,
                //                     m_interResidualsYBest[depth] + offsetPred, MAX_CU_SIZE * 2, roiSizeCu);
                //    ippiCopy_8u_C1R(m_interPredMergeBest[depth] + offsetPred, MAX_CU_SIZE,
                //                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSizeCu);
                //}
            }
        }

        if (splitMode == SPLIT_TRY)
            ippiCopy_8u_C1R(m_yRec + offsetLumaCu, m_pitchRec, m_recLumaSaveCu[depth], widthCu, roiSizeCu);
    }

    Ipp8u skippedFlag = 0;
    if (splitMode != SPLIT_MUST)
        skippedFlag = (m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU))[absPartIdx].flags.skippedFlag;

    CostType cuSplitThresholdCu = (m_cslice->slice_type == I_SLICE)
        ? m_par->cu_split_threshold_cu_intra[depth]
        : m_par->cu_split_threshold_cu_inter[depth];
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

        // add cost of cu split flag to costSplit
        if (costBest > costSplit) {
            costBest = costSplit;
            small_memcpy(m_dataBest + (depth << m_par->Log2NumPartInCU) + absPartIdx,
                m_dataBest + ((depth + 1) << m_par->Log2NumPartInCU) + absPartIdx,
                sizeof(H265CUData) * numParts);
        }
        else {
            ippiCopy_8u_C1R(m_recLumaSaveCu[depth], widthCu, m_yRec + offsetLumaCu, m_pitchRec, roiSizeCu);
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
        FillSubPart(absPartIdx, depth, trDepth, partSize, lumaDir, m_par->QP);
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

    if (costBest >= m_par->cu_split_threshold_tu_intra[depth+trDepth] && splitMode != SPLIT_NONE && nz) {
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


Ipp32s H265CU::MvCost1RefLog(H265MV mv, Ipp8s refIdx, const MVPInfo *predInfo, Ipp32s rlist) const
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



Ipp32s H265CU::MatchingMetricBipredPu(PixType *src, H265MEInfo *meInfo, PixType *fwd, Ipp32u pitchFwd,
                                      PixType *bwd, Ipp32u pitchBwd, H265MV fullMV[2], Ipp32s useHadamard)
{
    H265MV MV[2] = { fullMV[0], fullMV[1] };

    Ipp16s *predBufY[2];
    PixType *pY[2] = { fwd, bwd };
    Ipp32u pitch[2] = { pitchFwd, pitchBwd };

    for (Ipp8u dir = 0; dir < 2; dir++) {

        ClipMV(MV[dir]); // store clipped mv in buffer

        Ipp8u idx;
        for (idx = m_interpIdxFirst; idx !=m_interpIdxLast; idx = (idx + 1) & (INTERP_BUF_SZ - 1)) {
            if (pY[dir] == m_interpBuf[idx].pY && MV[dir] == m_interpBuf[idx].mv) {
                predBufY[dir] = m_interpBuf[idx].predBufY;
                break;
            }
        }

        if (idx == m_interpIdxLast) {
            predBufY[dir] = m_interpBuf[idx].predBufY;
            m_interpBuf[idx].pY = pY[dir];
            m_interpBuf[idx].mv = MV[dir];

            MeInterpolateOld(meInfo, &MV[dir], pY[dir], pitch[dir], predBufY[dir], MAX_CU_SIZE);
            m_interpIdxLast = (m_interpIdxLast + 1) & (INTERP_BUF_SZ - 1); // added to end
            if (m_interpIdxFirst == m_interpIdxLast) // replaced oldest
                m_interpIdxFirst = (m_interpIdxFirst + 1) & (INTERP_BUF_SZ - 1);
        }
    }

    ALIGN_DECL(32) PixType biPredBuf[MAX_CU_SIZE * MAX_CU_SIZE];
    WriteAverageToPic(predBufY[0], MAX_CU_SIZE, predBufY[1], MAX_CU_SIZE, biPredBuf, MAX_CU_SIZE, meInfo->width, meInfo->height);

    return (useHadamard)
        ? tuHad(src, m_pitchSrc, biPredBuf, MAX_CU_SIZE, meInfo->width, meInfo->height)
        : MFX_HEVC_PP::h265_SAD_MxN_special_8u(src, biPredBuf, m_pitchSrc, meInfo->width, meInfo->height);
}

static Ipp32s SamePrediction(const H265MEInfo* a, const H265MEInfo* b)
{
    if (a->interDir != b->interDir ||
        (a->interDir | INTER_DIR_PRED_L0) && (a->MV[0] != b->MV[0] || a->refIdx[0] != b->refIdx[0]) ||
        (a->interDir | INTER_DIR_PRED_L1) && (a->MV[1] != b->MV[1] || a->refIdx[1] != b->refIdx[1]) )
        return false;
    else
        return true;
}


// Tries different PU modes for CU
CostType H265CU::MeCu(Ipp32u absPartIdx, Ipp8u depth, Ipp32s offset)
{
    Ipp32u i;
    H265MEInfo meInfo, bestMeInfo[4];
    meInfo.absPartIdx = absPartIdx;
    meInfo.depth = depth;
    meInfo.width  = (Ipp8u)(m_par->MaxCUSize>>depth);
    meInfo.height = (Ipp8u)(m_par->MaxCUSize>>depth);
    meInfo.posx = (Ipp8u)((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
    meInfo.posy = (Ipp8u)((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);

    IppiSize roiSize = {meInfo.width, meInfo.width};
    Ipp32s offsetLuma = GetLumaOffset(m_par, absPartIdx, m_pitchRec);
    Ipp32s offsetPred = meInfo.posx + meInfo.posy * MAX_CU_SIZE;

    CABAC_CONTEXT_H265 ctxSave[2][NUM_CABAC_CONTEXT];
    m_bsf->CtxSave(ctxSave[0], 0, NUM_CABAC_CONTEXT);

    Ipp8u bestSplitMode = PART_SIZE_2Nx2N;
    CostType bestCost;
    //Ipp32u xborder = meInfo.width;
    //Ipp32u yborder = meInfo.height;

    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    CostType cost = 0;

    meInfo.splitMode = PART_SIZE_2Nx2N;
    MePu(&meInfo, 0);

    m_interPredPtr = m_interPredBest;
    m_interResidualsYPtr = m_interResidualsYBest;
    m_interPredReady = false;
    bestCost = CuCost(absPartIdx, depth, &meInfo, m_par->fastPUDecision);
    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
    bestMeInfo[0] = meInfo;
    m_interPredPtr = m_interPred;
    m_interResidualsYPtr = m_interResidualsY;

    // NxN prediction
    if (depth == m_par->MaxCUDepth - m_par->AddCUDepth && meInfo.width > 8)
    {
        H265MEInfo bestInfo[4];
        Ipp32s allthesame = true;
        for (i=0; i<4; i++) {
            bestInfo[i] = meInfo;
            bestInfo[i].width  = meInfo.width / 2;
            bestInfo[i].height = meInfo.height / 2;
            bestInfo[i].posx  = (Ipp8u)(meInfo.posx + bestInfo[i].width * (i & 1));
            bestInfo[i].posy  = (Ipp8u)(meInfo.posy + bestInfo[i].height * ((i>>1) & 1));
            bestInfo[i].absPartIdx = meInfo.absPartIdx + (numParts >> 2)*i;
            bestInfo[i].splitMode = PART_SIZE_NxN;
            Ipp32s interPredIdx = i == 0 ? 0 : (bestInfo[i - 1].refIdx[0] < 0) ? 1 : (bestInfo[i - 1].refIdx[1] < 0) ? 0 : 2;
            MePu(bestInfo + i, interPredIdx);
            allthesame = allthesame && SamePrediction(&bestInfo[i], &meInfo);
        }
        if (!allthesame) {
            m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
            cost = CuCost(absPartIdx, depth, bestInfo, m_par->fastPUDecision);
            if (bestCost > cost) {
                bestMeInfo[0] = bestInfo[0];
                bestMeInfo[1] = bestInfo[1];
                bestMeInfo[2] = bestInfo[2];
                bestMeInfo[3] = bestInfo[3];
                bestSplitMode = PART_SIZE_NxN;
                bestCost = cost;
                m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
                ippiCopy_16s_C1R(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE * 2,
                                 m_interResidualsYBest[depth] + offsetPred, MAX_CU_SIZE * 2, roiSize);
            }
        }
    }

    // Nx2N
    {
        H265MEInfo partsInfo[2] = {meInfo, meInfo};
        partsInfo[0].width = meInfo.width / 2;
        partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 2)*1;
        partsInfo[1].width = meInfo.width / 2;
        partsInfo[1].posx  = meInfo.posx + partsInfo[0].width;
        partsInfo[0].splitMode = PART_SIZE_Nx2N;
        partsInfo[1].splitMode = PART_SIZE_Nx2N;

        MePu(partsInfo + 0, 0);
        Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
        MePu(partsInfo + 1, interPredIdx);

        if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
            m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
            cost = CuCost(absPartIdx, depth, partsInfo, m_par->fastPUDecision);
            if (bestCost > cost) {
                bestMeInfo[0] = partsInfo[0];
                bestMeInfo[1] = partsInfo[1];
                bestSplitMode = PART_SIZE_Nx2N;
                bestCost = cost;
                m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
                ippiCopy_16s_C1R(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE*2,
                                 m_interResidualsYBest[depth] + offsetPred, MAX_CU_SIZE*2, roiSize);
            }
        }
    }

    // 2NxN
    {
        H265MEInfo partsInfo[2] = {meInfo, meInfo};
        partsInfo[0].height = meInfo.height / 2;
        partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 2)*2;
        partsInfo[1].height = meInfo.height / 2;
        partsInfo[1].posy  = meInfo.posy + partsInfo[0].height;
        partsInfo[0].splitMode = PART_SIZE_2NxN;
        partsInfo[1].splitMode = PART_SIZE_2NxN;

        MePu(partsInfo + 0, 0);
        Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
        MePu(partsInfo + 1, interPredIdx);

        //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
        if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
            m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
            cost = CuCost(absPartIdx, depth, partsInfo, m_par->fastPUDecision);
            if (bestCost > cost) {
                bestMeInfo[0] = partsInfo[0];
                bestMeInfo[1] = partsInfo[1];
                bestSplitMode = PART_SIZE_2NxN;
                bestCost = cost;
                m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
                ippiCopy_16s_C1R(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE*2,
                                 m_interResidualsYBest[depth] + offsetPred, MAX_CU_SIZE*2, roiSize);
            }
        }
    }

    // advanced prediction modes
    //if (m_par->AMPFlag && meInfo.width > 8)
    if (m_par->AMPAcc[meInfo.depth])
    {
        // PART_SIZE_2NxnU
        {
            H265MEInfo partsInfo[2] = {meInfo, meInfo};
            partsInfo[0].height = meInfo.height / 4;
            partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 3);
            partsInfo[1].height = meInfo.height * 3 / 4;
            partsInfo[1].posy  = meInfo.posy + partsInfo[0].height;
            partsInfo[0].splitMode = PART_SIZE_2NxnU;
            partsInfo[1].splitMode = PART_SIZE_2NxnU;

            MePu(partsInfo + 0, 0);
            Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
            MePu(partsInfo + 1, interPredIdx);

            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo, m_par->fastPUDecision);
                if (bestCost > cost) {
                    bestSplitMode = PART_SIZE_2NxnU;
                    bestCost = cost;
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
                    ippiCopy_16s_C1R(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE*2,
                                     m_interResidualsYBest[depth] + offsetPred, MAX_CU_SIZE*2, roiSize);
                }
            }
        }
        // PART_SIZE_2NxnD
        {
            H265MEInfo partsInfo[2] = {meInfo, meInfo};
            partsInfo[0].height = meInfo.height * 3 / 4;
            partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 3) * 5;
            partsInfo[1].height = meInfo.height / 4;
            partsInfo[1].posy  = meInfo.posy + partsInfo[0].height;
            partsInfo[0].splitMode = PART_SIZE_2NxnD;
            partsInfo[1].splitMode = PART_SIZE_2NxnD;

            MePu(partsInfo + 0, 0);
            Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
            MePu(partsInfo + 1, interPredIdx);

            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo, m_par->fastPUDecision);
                if (bestCost > cost) {
                    bestSplitMode = PART_SIZE_2NxnD;
                    bestCost = cost;
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
                    ippiCopy_16s_C1R(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE*2,
                                     m_interResidualsYBest[depth] + offsetPred, MAX_CU_SIZE*2, roiSize);
                }
            }
        }
        // PART_SIZE_nRx2N
        {
            H265MEInfo partsInfo[2] = {meInfo, meInfo};
            partsInfo[0].width = meInfo.width * 3 / 4;
            partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 4) * 5;
            partsInfo[1].width = meInfo.width / 4;
            partsInfo[1].posx  = meInfo.posx + partsInfo[0].width;
            partsInfo[0].splitMode = PART_SIZE_nRx2N;
            partsInfo[1].splitMode = PART_SIZE_nRx2N;

            MePu(partsInfo + 0, 0);
            Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
            MePu(partsInfo + 1, interPredIdx);

            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo, m_par->fastPUDecision);
                if (bestCost > cost) {
                    bestSplitMode = PART_SIZE_nRx2N;
                    bestCost = cost;
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
                    ippiCopy_16s_C1R(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE*2,
                                     m_interResidualsYBest[depth] + offsetPred, MAX_CU_SIZE*2, roiSize);
                }
            }
        }
        // PART_SIZE_nLx2N
        {
            H265MEInfo partsInfo[2] = {meInfo, meInfo};
            partsInfo[0].width = meInfo.width / 4;
            partsInfo[1].absPartIdx = meInfo.absPartIdx + (numParts >> 4);
            partsInfo[1].width = meInfo.width * 3 / 4;
            partsInfo[1].posx  = meInfo.posx + partsInfo[0].width;
            partsInfo[0].splitMode = PART_SIZE_nLx2N;
            partsInfo[1].splitMode = PART_SIZE_nLx2N;

            MePu(partsInfo + 0, 0);
            Ipp32s interPredIdx = (partsInfo[0].refIdx[0] < 0) ? 1 : (partsInfo[0].refIdx[1] < 0) ? 0 : 2;
            MePu(partsInfo + 1, interPredIdx);

            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo, m_par->fastPUDecision);
                if (bestCost > cost) {
                    bestSplitMode = PART_SIZE_nLx2N;
                    bestCost = cost;
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
                    ippiCopy_16s_C1R(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE*2,
                                     m_interResidualsYBest[depth] + offsetPred, MAX_CU_SIZE*2, roiSize);
                }
            }
        }
    }

    m_interPredPtr = m_interPredBest;
    m_interResidualsYPtr = m_interResidualsYBest;

    if (m_par->fastPUDecision)
    {
        m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        small_memcpy(m_data + absPartIdx, m_dataTemp2, sizeof(H265CUData) * numParts);
        m_interPredReady = true;
        return CuCost(absPartIdx, depth, bestMeInfo, 0);
    }
    else
    {
        m_bsf->CtxRestore(ctxSave[1], 0, NUM_CABAC_CONTEXT);
        ippiCopy_8u_C1R(m_recLumaSaveTu[depth], meInfo.width, m_yRec + offsetLuma, m_pitchRec, roiSize);
        small_memcpy(m_data + absPartIdx, m_dataTemp2, sizeof(H265CUData) * numParts);
        return bestCost;
    }
}

void H265CU::TuGetSplitInter(Ipp32u absPartIdx, Ipp32s offset, Ipp8u tr_idx, Ipp8u trIdxMax, Ipp8u nz[3], CostType *cost, Ipp8u cbf[256][3])
{
    Ipp8u depth = m_data[absPartIdx].depth;
    Ipp32s numParts = ( m_par->NumPartInCU >> ((depth + tr_idx)<<1) ); // in TU
    Ipp32s width = m_data[absPartIdx].size >>tr_idx;
    Ipp32s yRecOffset = GetLumaOffset(m_par, absPartIdx, m_pitchRec);

    H265CUData *data_t = m_dataTemp + ((depth + tr_idx) << m_par->Log2NumPartInCU);
    CostType costBest;
    CABAC_CONTEXT_H265 ctxSave[2][NUM_CABAC_CONTEXT];

    for (Ipp32s i = 0; i < numParts; i++) m_data[absPartIdx + i].trIdx = tr_idx;
    m_bsf->CtxSave(ctxSave[0], 0, NUM_CABAC_CONTEXT);
    EncAndRecLumaTu(absPartIdx, offset, width, &nz[0], &costBest, 0, INTRA_PRED_CALC);
    Ipp8u hasNz = nz[0];
    CostType costChroma = 0;
    if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && width > 4) {
        EncAndRecChromaTu(absPartIdx, offset>>2, width>>1, &nz[1], &costChroma);
        costBest += costChroma;
        hasNz |= (nz[1] | nz[2]);
    } else
        nz[1] = nz[2] = 0;

    if (tr_idx  < trIdxMax && hasNz) { // don't try if all zero
        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
        m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        // keep not split
        small_memcpy(data_t + absPartIdx, m_data + absPartIdx, sizeof(H265CUData) * numParts);
        IppiSize roi = { width, width };
        ippiCopy_8u_C1R(m_yRec + yRecOffset, m_pitchRec, m_interRecBest[depth + tr_idx], MAX_CU_SIZE, roi);
        if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
            IppiSize roi = { width, width>>1 };
            ippiCopy_8u_C1R(m_uvRec + (yRecOffset>>1), m_pitchRec, m_interRecBestChroma[depth + tr_idx], MAX_CU_SIZE, roi);
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

        m_bsf->Reset();
        Ipp8u code_dqp;
        PutTransform(m_bsf, offset, 0, absPartIdx, absPartIdx,
                      m_data[absPartIdx].depth + m_data[absPartIdx].trIdx,
                      width, width, m_data[absPartIdx].trIdx, code_dqp, 1);

        cost_split += BIT_COST_INTER(m_bsf->GetNumBits());

        if (costBest > cost_split) {
            costBest = cost_split;
            nz[0] = nzt[0];
            nz[1] = nzt[1];
            nz[2] = nzt[2];
        } else {
            // restore not split
            small_memcpy(m_data + absPartIdx, data_t + absPartIdx, sizeof(H265CUData) * numParts);
            m_bsf->CtxRestore(ctxSave[1], 0, NUM_CABAC_CONTEXT);
            ippiCopy_8u_C1R(m_interRecBest[depth + tr_idx], MAX_CU_SIZE, m_yRec + yRecOffset, m_pitchRec, roi);
            if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
                IppiSize roi = { width, width>>1 };
                ippiCopy_8u_C1R(m_interRecBestChroma[depth + tr_idx], MAX_CU_SIZE, m_uvRec + (yRecOffset>>1), m_pitchRec, roi);
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

// Cost for CU
CostType H265CU::CuCost(Ipp32u absPartIdx, Ipp8u depth, const H265MEInfo *bestInfo,
                        Ipp32s fastPuDecision)
{
    Ipp8u bestSplitMode = bestInfo[0].splitMode;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    CostType bestCost = 0;

    Ipp32u xborder = bestInfo[0].posx + bestInfo[0].width;
    Ipp32u yborder = bestInfo[0].posy + bestInfo[0].height;

    Ipp8u tr_depth_min, tr_depth_max;
    GetTrDepthMinMax(m_par, absPartIdx, depth, bestSplitMode, &tr_depth_min, &tr_depth_max);

    memset(&m_data[absPartIdx], 0, numParts * sizeof(H265CUData));
    for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
        Ipp32u posx = (h265_scan_z2r[m_par->MaxCUDepth][i] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize;
        Ipp32u posy = (h265_scan_z2r[m_par->MaxCUDepth][i] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize;
        Ipp32s partNxN = (posx<xborder ? 0 : 1) + (posy<yborder ? 0 : 2);
        Ipp32s part = (bestSplitMode != PART_SIZE_NxN) ? (partNxN ? 1 : 0) : partNxN;
        const H265MEInfo* mei = &bestInfo[part];

        VM_ASSERT(mei->interDir >= 1 && mei->interDir <= 3);
        m_data[i].predMode = MODE_INTER;
        m_data[i].depth = depth;
        m_data[i].size = (Ipp8u)(m_par->MaxCUSize>>depth);
        m_data[i].partSize = bestSplitMode;
        m_data[i].qp = m_par->QP;
        m_data[i].flags.skippedFlag = 0;
        //m_data[i].tr_idx = tr_depth; // not decided thoroughly here
        m_data[i].interDir = mei->interDir;
        m_data[i].refIdx[0] = -1;
        m_data[i].refIdx[1] = -1;
        if (m_data[i].interDir & INTER_DIR_PRED_L0) {
            m_data[i].refIdx[0] = mei->refIdx[0];
            m_data[i].mv[0] = mei->MV[0];
        }
        if (m_data[i].interDir & INTER_DIR_PRED_L1) {
            m_data[i].refIdx[1] = mei->refIdx[1];
            m_data[i].mv[1] = mei->MV[1];
        }
    }
    // set mv
    for (Ipp32u i = 0; i < GetNumPartInter(absPartIdx); i++)
    {
        Ipp32s PartAddr;
        Ipp32s PartX, PartY, Width, Height;
        GetPartOffsetAndSize(i, m_data[absPartIdx].partSize,
            m_data[absPartIdx].size, PartX, PartY, Width, Height);
        GetPartAddr(i, m_data[absPartIdx].partSize, numParts, PartAddr);
        GetPuMvInfo(absPartIdx, PartAddr, m_data[absPartIdx].partSize, i);
    }

    if (m_rdOptFlag) {
        Ipp8u nzt[3], nz[3] = {0};
        CABAC_CONTEXT_H265 ctxSave[NUM_CABAC_CONTEXT];
        Ipp8u cbf[256][3];
        ALIGN_DECL(32) PixType predBufChr[MAX_CU_SIZE*MAX_CU_SIZE/2]; // nv12?

        if (m_isRdoq)
            m_bsf->CtxSave(ctxSave, 0, NUM_CABAC_CONTEXT);

        if (!m_interPredReady)
            InterPredCu(absPartIdx, depth, m_interPredPtr[depth], MAX_CU_SIZE, 1);

        Ipp32s offsetPred = GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
        PixType *src = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchSrc);

        TuDiff(m_interResidualsYPtr[depth] + offsetPred, MAX_CU_SIZE, src, m_pitchSrc, m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE, m_data[absPartIdx].size);

        if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
            InterPredCu(absPartIdx, depth, predBufChr, MAX_CU_SIZE, 0);
        }

        memset(&cbf[absPartIdx], 0, numParts*sizeof(cbf[0]));
        for (Ipp32u pos = 0; pos < numParts; ) {
            CostType cost;
            Ipp32u num_tr_parts = ( m_par->NumPartInCU >> ((depth + tr_depth_min)<<1) );

            TuGetSplitInter(absPartIdx + pos, (absPartIdx + pos)*16, tr_depth_min, fastPuDecision ? tr_depth_min : tr_depth_max, nzt, &cost, cbf);
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

Ipp32s H265CU::GetMvPredictors(H265MV * mvPred, const H265MEInfo* meInfo, const MVPInfo *predInfo,
                               const MVPInfo *mergeInfo, H265MV mvLast, Ipp32s meDir, Ipp32s refIdx) const
{
    Ipp32s numMvPred = 0;
    RefPicList *refList = m_currFrame->m_refPicList + meDir;

    if (refIdx > 0) {
        mvPred[numMvPred++] = mvLast; // add best mv from previous reference

        Ipp32s scale =
            GetDistScaleFactor(refList->m_deltaPoc[refIdx], refList->m_deltaPoc[refIdx - 1]);
        if (scale < 4096) {
            // add scaled best mv from previous reference
            mvPred[numMvPred].mvx = (Ipp16s)Saturate(-32768, 32767, (scale * mvLast.mvx + 127 + (scale * mvLast.mvx < 0)) >> 8);
            mvPred[numMvPred].mvy = (Ipp16s)Saturate(-32768, 32767, (scale * mvLast.mvy + 127 + (scale * mvLast.mvy < 0)) >> 8);
            ClipMV(mvPred[numMvPred]);
            numMvPred++;
        }
    }

    for (Ipp32s i = 0; i < mergeInfo->numCand; i++) {
        if (refIdx == mergeInfo->refIdx[2 * i + meDir]) {
            H265MV mv = mergeInfo->mvCand[2 * i + meDir];
            ClipMV(mv);
            if (std::find(mvPred, mvPred + numMvPred, mv) == mvPred + numMvPred)
                mvPred[numMvPred++] = mv;
        }
    }

    for (Ipp32s i = 0; i < predInfo[refIdx * 2 + meDir].numCand; i++) {
        if(refIdx == predInfo[refIdx * 2 + meDir].refIdx[i]) {
            H265MV mv = predInfo[refIdx * 2 + meDir].mvCand[i];
            ClipMV(mv);
            if (std::find(mvPred, mvPred + numMvPred, mv) == mvPred + numMvPred)
                mvPred[numMvPred++] = mv;
        }
    }

    // add from top level
    if (meInfo->depth > 0 && meInfo->depth > m_depthMin) {
        H265CUData *topdata = m_dataBest + ((meInfo->depth - 1) << m_par->Log2NumPartInCU);
        if (refIdx == topdata[meInfo->absPartIdx].refIdx[meDir] &&
            topdata[meInfo->absPartIdx].predMode == MODE_INTER)
        {
            H265MV mv = topdata[meInfo->absPartIdx].mv[meDir];
            if (std::find(mvPred, mvPred + numMvPred, mv) == mvPred + numMvPred)
                mvPred[numMvPred++] = mv;
        }
    }

    return numMvPred;
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

void H265CU::MeIntPel(const H265MEInfo *meInfo, const MVPInfo *predInfo, Ipp32s list,
                      Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    if (m_par->patternIntPel == 1) {
        return MeIntPelLog(meInfo, predInfo, list, refIdx, mv, cost, mvCost);
    }
    else if (m_par->patternIntPel == 2) {
        return MeIntPelFullSearch(meInfo, predInfo, list, refIdx, mv, cost, mvCost);
    }
    else {
        VM_ASSERT(0);
    }
}


void H265CU::MeIntPelFullSearch(const H265MEInfo *meInfo, const MVPInfo *predInfo, Ipp32s list,
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
            H265MV mv = { mvCenter.mvx + dx, mvCenter.mvy + dy };
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


void H265CU::MeIntPelLog(const H265MEInfo *meInfo, const MVPInfo *predInfo, Ipp32s list,
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
                mvCenter.mvx + tab_mePattern[mePos][0] * meStep,
                mvCenter.mvy + tab_mePattern[mePos][1] * meStep
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
                mvCenter.mvx + tab_mePattern[mePos][0] * meStep,
                mvCenter.mvy + tab_mePattern[mePos][1] * meStep
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


void H265CU::MeSubPel(const H265MEInfo *meInfo, const MVPInfo *predInfo, Ipp32s meDir,
                      Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    H265MV mvCenter = *mv;
    H265MV mvBest = *mv;
    Ipp32s startPos = 1;
    Ipp32s meStep = 2;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;

    H265Frame *ref = m_currFrame->m_refPicList[meDir].m_refFrames[refIdx];
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;

    Ipp32s useHadamard = (m_par->hadamardMe >= 2);
    if (m_par->hadamardMe == 2) {
        // when satd is for subpel only need to recalculate cost for intpel motion vector
        costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard) + mvCostBest;
    }

    while (meStep) {
        Ipp32s bestPos = 0;
        for (Ipp32s mePos = startPos; mePos < 9; mePos++) {
            H265MV mv = {
                mvCenter.mvx + tab_mePattern[mePos][0] * (Ipp16s)meStep,
                mvCenter.mvy + tab_mePattern[mePos][1] * (Ipp16s)meStep
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

        mvCenter = mvBest;
        meStep >>= 1;
        startPos = 1;
    }

    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;
}

Ipp32s GetFlBits(Ipp32s val, Ipp32s maxVal)
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

static bool IsCandFound(const Ipp8s *curRefIdx, const H265MV *curMV, const MVPInfo *mergeInfo,
                        Ipp32s candIdx, Ipp32s numRefLists);

Ipp32s IsDuplicatedMergeCand(const MVPInfo * mergeInfo, Ipp32s mergeIdxCur) // FIXME: try using IsCandFound()
{
    const H265MV *curMv = mergeInfo->mvCand + 2 * mergeIdxCur;
    const Ipp8s *curRefIdx = mergeInfo->refIdx + 2 * mergeIdxCur;

    for (Ipp32s i = 0; i < mergeIdxCur; i++) {
        if (IsCandFound(curRefIdx, curMv, mergeInfo, i, 2))
            return 1;
    }

    return 0;
}


void H265CU::MePu(H265MEInfo *meInfo, Ipp32s lastPredIdx)
{
    if (m_par->enableCmFlag)
        return MePuGacc(meInfo, lastPredIdx);

    RefPicList *refPicList = m_currFrame->m_refPicList;
    Ipp32u numParts = (m_par->NumPartInCU >> (meInfo->depth << 1));
    Ipp32s blockIdx = meInfo->absPartIdx &~ (numParts - 1);
    Ipp32s partAddr = meInfo->absPartIdx &  (numParts - 1);
    Ipp32s curPUidx = (meInfo->splitMode == PART_SIZE_NxN) ? (partAddr / (numParts >> 2)) : !!partAddr;

    MVPInfo predInfo[2 * MAX_NUM_REF_IDX];
    MVPInfo mergeInfo;
    GetPuMvPredictorInfo(blockIdx, partAddr, meInfo->splitMode, curPUidx, predInfo, mergeInfo);

    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;

    H265MV mvRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s costRefBest[2][MAX_NUM_REF_IDX] = { { INT_MAX }, { INT_MAX } };
    Ipp32s mvCostRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s bitsRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s refIdxBest[2] = {};

    Ipp32s predIdxBits[3];
    GetPredIdxBits(meInfo->splitMode, (m_cslice->slice_type == P_SLICE), curPUidx, lastPredIdx, predIdxBits);

    Ipp32s numLists = (m_cslice->slice_type == B_SLICE) + 1;
    for (Ipp32s list = 0; list < numLists; list++) {
        Ipp32s numRefIdx = m_cslice->num_ref_idx[list];

        for (Ipp32s refIdx = 0; refIdx < numRefIdx; refIdx++) {
            costRefBest[list][refIdx] = INT_MAX;

            if (list == 1) { // TODO: use pre-build table of duplicates instead of std::find
                Ipp32s idx0 = m_currFrame->m_mapRefIdxL1ToL0[refIdx];
                if (idx0 >= 0) {
                    // don't do ME just re-calc costs
                    mvRefBest[1][refIdx] = mvRefBest[0][idx0];
                    mvCostRefBest[1][refIdx] = MvCost1RefLog(mvRefBest[0][idx0], (Ipp8s)refIdx, predInfo, list);
                    bitsRefBest[1][refIdx] = MVP_LX_FLAG_BITS;
                    bitsRefBest[1][refIdx] += GetFlBits(refIdx, numRefIdx);
                    bitsRefBest[1][refIdx] += predIdxBits[list];
                    costRefBest[1][refIdx] = costRefBest[0][idx0] - mvCostRefBest[0][idx0];
                    costRefBest[1][refIdx] -= (Ipp32s)(bitsRefBest[0][idx0] * m_cslice->rd_lambda_sqrt + 0.5);
                    costRefBest[1][refIdx] += mvCostRefBest[1][refIdx];
                    costRefBest[1][refIdx] += (Ipp32s)(bitsRefBest[1][refIdx] * m_cslice->rd_lambda_sqrt + 0.5);
                    if (costRefBest[1][refIdxBest[1]] > costRefBest[list][1])
                        refIdxBest[1] = refIdx;
                    continue;
                }
            }

            // use satd for all candidates even if satd is for subpel only
            Ipp32s useHadamard = (m_par->hadamardMe == 3);

            Ipp32s costBest = INT_MAX;
            H265MV mvBest = { 0, 0 };
            MVPInfo *mvp = predInfo + refIdx * 2 + list;
            H265Frame *ref = refPicList[list].m_refFrames[refIdx];
            for (Ipp32s i = 0; i < predInfo[refIdx * 2 + list].numCand; i++) {
                H265MV mv = mvp->mvCand[i];
                if (std::find(mvp->mvCand, mvp->mvCand + i, mvp->mvCand[i]) != mvp->mvCand + i)
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
                mvCostBest = MvCost1RefLog(mvBest, (Ipp8s)refIdx, predInfo, list);
                costBest += mvCostBest;
            }
            else {
                // add cost of zero mvd
                mvCostBest = 2 * (Ipp32s)(1 * m_cslice->rd_lambda_sqrt + 0.5);
                costBest += mvCostBest;
            }

            MeIntPel(meInfo, predInfo, list, refIdx, &mvBest, &costBest, &mvCostBest);
            MeSubPel(meInfo, predInfo, list, refIdx, &mvBest, &costBest, &mvCostBest);

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

    Ipp32s idxL0 = refIdxBest[0];
    Ipp32s idxL1 = refIdxBest[1];
    Ipp32s costList[2] = { costRefBest[0][idxL0], costRefBest[1][idxL1] };
    Ipp32s costBiBest = INT_MAX;
    H265MV mvBiBest[2] = { mvRefBest[0][idxL0], mvRefBest[1][idxL1] };

    H265Frame *refF = refPicList[0].m_refFrames[idxL0];
    H265Frame *refB = refPicList[1].m_refFrames[idxL1];
    if (m_cslice->slice_type == B_SLICE && meInfo->width + meInfo->height != 12 && refF && refB) {
        // use satd for bidir refine because it is always sub-pel
        Ipp32s useHadamard = (m_par->hadamardMe >= 2);
        m_interpIdxFirst = m_interpIdxLast = 0; // for MatchingMetricBipredPu optimization

        Ipp32s mvCostBiBest = mvCostRefBest[0][idxL0] + mvCostRefBest[1][idxL1];
        costBiBest = mvCostBiBest;
        costBiBest += MatchingMetricBipredPu(src, meInfo, refF->y, refF->pitch_luma, refB->y,
            refB->pitch_luma, mvBiBest, useHadamard);

        // refine Bidir
        if (IPP_MIN(costList[0], costList[1]) * 9 > 8 * costBiBest) {
            bool changed = false;

            do {
                H265MV mvCenter[2] = { mvBiBest[0], mvBiBest[1] };
                changed = false;
                Ipp32s count = (refF == refB && mvBiBest[0] == mvBiBest[1]) ? 2 * 2 : 2 * 2 * 2;
                for (Ipp32s i = 0; i < count; i++) {
                    H265MV mv[2] = { mvCenter[0], mvCenter[1] };
                    if (i & 2)
                        mv[i >> 2].mvx += (i & 1) ? 1 : -1;
                    else
                        mv[i >> 2].mvy += (i & 1) ? 1 : -1;

                    Ipp32s mvCost = MvCost1RefLog(mv[0], (Ipp8s)idxL0, predInfo, 0) +
                        MvCost1RefLog(mv[1], (Ipp8s)idxL1, predInfo, 1);
                    Ipp32s cost = MatchingMetricBipredPu(src, meInfo, refF->y, refF->pitch_luma,
                        refB->y, refB->pitch_luma, mv, useHadamard);
                    cost += mvCost;

                    if (costBiBest > cost) {
                        costBiBest = cost;
                        mvCostBiBest = mvCost;
                        mvBiBest[0] = mv[0];
                        mvBiBest[1] = mv[1];
                        changed = true;
                    }
                }
            } while (changed);

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
    /*EXPERIMENTAL SKIP CHECK: if (meInfo->splitMode != PART_SIZE_2Nx2N)*/ {
        for (Ipp32s mergeIdx = 0; mergeIdx < mergeInfo.numCand; mergeIdx++) {
            H265MV *mv = &mergeInfo.mvCand[2 * mergeIdx];
            Ipp8s *refIdx = &mergeInfo.refIdx[2 * mergeIdx];
            if (m_cslice->slice_type != B_SLICE || (refIdx[0] >= 0 && meInfo->width + meInfo->height == 12))
                refIdx[1] = -1;
            if (IsDuplicatedMergeCand(&mergeInfo, mergeIdx))
                continue;

            Ipp32s cost = INT_MAX;
            Ipp8u interDir = 0;
            interDir += (refIdx[0] >= 0) ? INTER_DIR_PRED_L0 : 0;
            interDir += (refIdx[1] >= 0) ? INTER_DIR_PRED_L1 : 0;
            if (refIdx[0] >= 0 && refIdx[1] >= 0) {
                H265Frame *refF = refPicList[0].m_refFrames[refIdx[0]];
                H265Frame *refB = refPicList[1].m_refFrames[refIdx[1]];
                cost = MatchingMetricBipredPu(src, meInfo, refF->y, refF->pitch_luma, refB->y,
                    refB->pitch_luma, mv, useHadamard);
            }
            else {
                Ipp32s list = (refIdx[1] >= 0);
                H265Frame *ref = refPicList[list].m_refFrames[refIdx[list]];
                cost = MatchingMetricPu(src, meInfo, mv + list, ref, useHadamard);
            }

            cost += (Ipp32s)(GetFlBits(mergeIdx, mergeInfo.numCand) * m_cslice->rd_lambda_sqrt + 0.5);

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
}


void H265CU::MePuGacc(H265MEInfo *meInfo, Ipp32s lastPredIdx)
{
    if (m_par->enableCmFlag) {
        int x = (m_ctbPelX + meInfo->posx) / meInfo->width;
        int y = (m_ctbPelY + meInfo->posy) / meInfo->height;
        PuSize puSize = PU_MAX;
        mfxI16Pair **cmMv = NULL;
        CmMbDist32 **cmDist32 = NULL;
        CmMbDist16 **cmDist16 = NULL;
        Ipp32s pitchDist = 0;
        Ipp32s pitchMv = 0;

        if (meInfo->width == 32 && meInfo->height == 32)
            cmMv = cmMv32x32[cmCurIdx], cmDist32 = cmDist32x32[cmCurIdx], pitchDist = (Ipp32s)dist32x32Pitch / sizeof(CmMbDist32), pitchMv = (Ipp32s)mv32x32Pitch / sizeof(mfxI16Pair);
        else if (meInfo->width == 32 && meInfo->height == 16)
            cmMv = cmMv32x16[cmCurIdx], cmDist32 = cmDist32x16[cmCurIdx], pitchDist = (Ipp32s)dist32x16Pitch / sizeof(CmMbDist32), pitchMv = (Ipp32s)mv32x16Pitch / sizeof(mfxI16Pair);
        else if (meInfo->width == 16 && meInfo->height == 32)
            cmMv = cmMv16x32[cmCurIdx], cmDist32 = cmDist16x32[cmCurIdx], pitchDist = (Ipp32s)dist16x32Pitch / sizeof(CmMbDist32), pitchMv = (Ipp32s)mv16x32Pitch / sizeof(mfxI16Pair);
        else if (meInfo->width == 16 && meInfo->height == 16)
            cmMv = cmMv16x16[cmCurIdx], cmDist16 = cmDist16x16[cmCurIdx], pitchDist = (Ipp32s)dist16x16Pitch / sizeof(CmMbDist16), pitchMv = (Ipp32s)mv16x16Pitch / sizeof(mfxI16Pair);
        else if (meInfo->width == 16 && meInfo->height ==  8)
            cmMv = cmMv16x8[cmCurIdx], cmDist16 = cmDist16x8[cmCurIdx], pitchDist = (Ipp32s)dist16x8Pitch / sizeof(CmMbDist16), pitchMv = (Ipp32s)mv16x8Pitch / sizeof(mfxI16Pair);
        else if (meInfo->width ==  8 && meInfo->height == 16)
            cmMv = cmMv8x16[cmCurIdx], cmDist16 = cmDist8x16[cmCurIdx], pitchDist = (Ipp32s)dist8x16Pitch / sizeof(CmMbDist16), pitchMv = (Ipp32s)mv8x16Pitch / sizeof(mfxI16Pair);
        else if (meInfo->width ==  8 && meInfo->height ==  8)
            cmMv = cmMv8x8[cmCurIdx], cmDist16 = cmDist8x8[cmCurIdx], pitchDist = (Ipp32s)dist8x8Pitch / sizeof(CmMbDist16), pitchMv = (Ipp32s)mv8x8Pitch / sizeof(mfxI16Pair);
        else if (meInfo->width ==  8 && meInfo->height ==  4)
            cmMv = cmMv8x4[cmCurIdx], cmDist16 = cmDist8x4[cmCurIdx], pitchDist = (Ipp32s)dist8x4Pitch / sizeof(CmMbDist16), pitchMv = (Ipp32s)mv8x4Pitch / sizeof(mfxI16Pair);
        else if (meInfo->width ==  4 && meInfo->height ==  8)
            cmMv = cmMv4x8[cmCurIdx], cmDist16 = cmDist4x8[cmCurIdx], pitchDist = (Ipp32s)dist4x8Pitch / sizeof(CmMbDist16), pitchMv = (Ipp32s)mv4x8Pitch / sizeof(mfxI16Pair);

        Ipp32u numParts = (m_par->NumPartInCU >> (meInfo->depth << 1));
        Ipp32s blockIdx = meInfo->absPartIdx &~ (numParts - 1);
        Ipp32s partAddr = meInfo->absPartIdx &  (numParts - 1);
        Ipp32s curPUidx = (meInfo->splitMode == PART_SIZE_NxN) ? (partAddr / (numParts >> 2)) : !!partAddr;

        MVPInfo predInfo[2 * MAX_NUM_REF_IDX];
        MVPInfo mergeInfo;
        GetPuMvPredictorInfo(blockIdx, partAddr, meInfo->splitMode, curPUidx, predInfo, mergeInfo);

        H265MV mvRefBest[2][MAX_NUM_REF_IDX] = {};
        Ipp32s costRefBest[2][MAX_NUM_REF_IDX] = { { INT_MAX }, { INT_MAX } };
        Ipp32s mvCostRefBest[2][MAX_NUM_REF_IDX] = {};
        Ipp32s bitsRefBest[2][MAX_NUM_REF_IDX] = {};
        Ipp32s refIdxBest[2] = {};

        Ipp32s predIdxBits[3];
        GetPredIdxBits(meInfo->splitMode, (m_cslice->slice_type == P_SLICE), curPUidx, lastPredIdx, predIdxBits);

        Ipp32s numRefIdx = m_cslice->num_ref_idx[0];
        for (Ipp32s refIdx = 0; refIdx < numRefIdx; refIdx++) {

            H265MV mvBest;
            Ipp32s costBest = INT_MAX;
            Ipp32s mvCostBest = 0;

            if (meInfo->width > 16 || meInfo->height > 16) {
                H265MV mvCenter = { cmMv[refIdx][y * pitchMv + x].x, cmMv[refIdx][y * pitchMv + x].y };
                for (Ipp16s sadIdx = 0, dy = -1; dy <= 1; dy++) {
                    for (Ipp16s dx = -1; dx <= 1; dx++, sadIdx++) {
                        H265MV mv = { mvCenter.mvx + dx, mvCenter.mvy + dy };
                        Ipp32s mvCost = MvCost1RefLog(mv, refIdx, predInfo, 0);
                        Ipp32s cost = cmDist32[refIdx][y * pitchDist + x].dist[sadIdx] + mvCost;
                        if (costBest > cost) {
                            costBest = cost;
                            mvBest = mv;
                            mvCostBest = mvCost;
                        }
                    }
                }
            }
            else {
                mvBest.mvx = cmMv[refIdx][y * pitchMv + x].x;
                mvBest.mvy = cmMv[refIdx][y * pitchMv + x].y;
                mvCostBest = MvCost1RefLog(mvBest, refIdx, predInfo, 0);
                costBest = cmDist16[refIdx][y * pitchDist + x].dist + mvCostBest;
            }

            mvRefBest[0][refIdx] = mvBest;
            mvCostRefBest[0][refIdx] = 0;
            costRefBest[0][refIdx] = costBest;
            bitsRefBest[0][refIdx] = MVP_LX_FLAG_BITS + predIdxBits[0];
            bitsRefBest[0][refIdx] += GetFlBits(refIdx, numRefIdx);
            costRefBest[0][refIdx] += (Ipp32s)(bitsRefBest[0][refIdx] * m_cslice->rd_lambda_sqrt + 0.5);

            if (costRefBest[0][refIdxBest[0]] > costRefBest[0][refIdx])
                refIdxBest[0] = refIdx;
        }

        meInfo->interDir = INTER_DIR_PRED_L0;
        meInfo->refIdx[0] = (Ipp8s)refIdxBest[0];
        meInfo->refIdx[1] = -1;
        meInfo->MV[0] = mvRefBest[0][refIdxBest[0]];
        meInfo->MV[1] = MV_ZERO;
    }
}


void H265CU::EncAndRecLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost) {
    Ipp32s depthMax = m_data[absPartIdx].depth + m_data[absPartIdx].trIdx;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    Ipp32s width = m_data[absPartIdx].size >> m_data[absPartIdx].trIdx;

    if (nz) *nz = 0;

    //if (depth == data[absPartIdx].depth && data[absPartIdx].pred_mode == MODE_INTER) {
    //    InterPredCu(absPartIdx, depth, m_yRec, m_pitchRec, 1);
    //}

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
    Ipp8u nzt[2];

    if(nz) nz[0] = nz[1] = 0;

    if (depth == m_data[absPartIdx].depth && m_data[absPartIdx].predMode == MODE_INTER) {
        InterPredCu(absPartIdx, depth, m_uvRec, m_pitchRec, 0);
    }

    if (depth == depthMax || width == 4) {
        EncAndRecChromaTu(absPartIdx, offset, width, nz, cost);
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

    PixType *rec = m_yRec + ((PUStartRow * m_pitchRec + PUStartColumn) << m_par->Log2MinTUSize);
    PixType *src = m_ySrc + ((PUStartRow * m_pitchSrc + PUStartColumn) << m_par->Log2MinTUSize);

    PixType *pred = NULL;
    Ipp32s  pitch_pred = 0;
    Ipp32s  is_pred_transposed = 0;
    if (m_data[absPartIdx].predMode == MODE_INTRA) {
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
        pred = m_interPredPtr[ m_data[absPartIdx].depth ];
        pred += (PUStartRow * pitch_pred + PUStartColumn) << m_par->Log2MinTUSize;
    }

    if (m_data[absPartIdx].predMode == MODE_INTRA && pred_opt == INTRA_PRED_CALC) {
        IntraPredTu(absPartIdx, width, m_data[absPartIdx].intraLumaDir, 1);
    }

    if (cost && cost_pred_flag) {
        cost_pred = tuHad(src, m_pitchSrc, rec, m_pitchRec, width, width);
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
            Ipp16s *resid = m_interResidualsYPtr[ m_data[absPartIdx].depth ];
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
            ? ippiTranspose_8u_C1R(pred, pitch_pred, rec, m_pitchRec, roi)
            : ippiCopy_8u_C1R(pred, pitch_pred, rec, m_pitchRec, roi);

        if (!m_rdOptFlag || cbf)
            TransformInv2(rec, m_pitchRec, offset, width, 1, m_data[absPartIdx].predMode == MODE_INTRA);

    } else {
        memset(m_trCoeffY + offset, 0, sizeof(CoeffsType) * width * width);
    }

    if (cost) {
        CostType cost_rec = TuSse(src, m_pitchSrc, rec, m_pitchRec, width);
        *cost = cost_rec;
    }

    if (m_data[absPartIdx].flags.skippedFlag) {
        if (nz) *nz = 0;
        return;
    }

    if (m_rdOptFlag && cost) {
        Ipp8u code_dqp;
        m_bsf->Reset();
        if (cbf) {
            SetCbfOne(absPartIdx, TEXT_LUMA, m_data[absPartIdx].trIdx);
            PutTransform(m_bsf, offset, 0, absPartIdx, absPartIdx,
                m_data[absPartIdx].depth + m_data[absPartIdx].trIdx,
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

void H265CU::EncAndRecChromaTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost)
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

    if (m_data[absPartIdx].predMode == MODE_INTRA) {
        Ipp8u intra_pred_mode = m_data[absPartIdx].intraChromaDir;
        if (intra_pred_mode == INTRA_DM_CHROMA) {
            Ipp32s shift = h265_log2m2[m_par->NumPartInCU >> (m_data[absPartIdx].depth<<1)] + 2;
            Ipp32s absPartIdx_0 = absPartIdx >> shift << shift;
            intra_pred_mode = m_data[absPartIdx_0].intraLumaDir;
        }
        IntraPredTu(absPartIdx, width, intra_pred_mode, 0);
    }

    if (!m_data[absPartIdx].flags.skippedFlag) {
        for (Ipp32s c_idx = 0; c_idx < 2; c_idx++) {
            PixType *pSrc = m_uvSrc + (((PUStartRow * m_pitchSrc >> 1) + PUStartColumn) << m_par->Log2MinTUSize) + c_idx;
            PixType *pRec = m_uvRec + (((PUStartRow * m_pitchRec >> 1) + PUStartColumn) << m_par->Log2MinTUSize) + c_idx;
            CoeffsType *residuals = c_idx ? m_residualsV : m_residualsU;

            TuDiffNv12(residuals + offset, width, pSrc, m_pitchSrc, pRec, m_pitchRec, width);
        }

        TransformFwd(offset, width, 0, 0);
        QuantFwdTu(absPartIdx, offset, width, 0);

        QuantInvTu(absPartIdx, offset, width, 0);
        TransformInv(offset, width, 0, 0);
    } else {
        memset(m_trCoeffU + offset, 0, sizeof(CoeffsType) * width * width);
        memset(m_trCoeffV + offset, 0, sizeof(CoeffsType) * width * width);
    }

    Ipp8u cbf[2] = {0, 0};
    for (Ipp32s c_idx = 0; c_idx < 2; c_idx++) {
        PixType *pSrc = m_uvSrc + (((PUStartRow * m_pitchSrc >> 1) + PUStartColumn) << (m_par->Log2MinTUSize)) + c_idx;
        PixType *pRec = m_uvRec + ((PUStartRow * m_pitchRec + PUStartColumn * 2) << (m_par->Log2MinTUSize - 1)) + c_idx;
        CoeffsType *residuals = c_idx ? m_residualsV : m_residualsU;
        CoeffsType *coeff = c_idx ? m_trCoeffV : m_trCoeffU;

        if (!m_data[absPartIdx].flags.skippedFlag) {
            tuAddClipNv12(pRec, m_pitchRec, pRec, m_pitchRec, residuals + offset, width, width);
        }

        // use only bit cost for chroma
        if (cost /*&& !m_rdOptFlag*/) {
            *cost += TuSseNv12(pSrc, m_pitchSrc, pRec, m_pitchRec, width);
        }
        if (m_data[absPartIdx].flags.skippedFlag && nz) {
            nz[c_idx] = 0;
            continue;
        }
        if (m_rdOptFlag || nz) {
            for (int i = 0; i < width * width; i++) {
                if (coeff[i + offset]) {
                    if (nz) nz[c_idx] = 1;
                    cbf[c_idx] = 1;
                    break;
                }
            }
        }
    }
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

bool H265CU::AddMvpCand(MVPInfo* pInfo,
                        H265CUData *p_data,
                        Ipp32s blockZScanIdx,
                        Ipp32s refPicListIdx,
                        Ipp32s refIdx,
                        bool order)
{
    RefPicList *refPicList = m_currFrame->m_refPicList;
    Ipp8u isCurrRefLongTerm;
    Ipp32s  currRefTb;
    Ipp32s  i;

    if (!p_data || (p_data[blockZScanIdx].predMode == MODE_INTRA))
    {
        return false;
    }

    if (!order)
    {
        if (p_data[blockZScanIdx].refIdx[refPicListIdx] == refIdx)
        {
            pInfo->mvCand[pInfo->numCand] = p_data[blockZScanIdx].mv[refPicListIdx];
            pInfo->numCand++;

            return true;
        }

        currRefTb = refPicList[refPicListIdx].m_deltaPoc[refIdx];
        refPicListIdx = 1 - refPicListIdx;

        if (p_data[blockZScanIdx].refIdx[refPicListIdx] >= 0)
        {
            Ipp32s neibRefTb = refPicList[refPicListIdx].m_deltaPoc[p_data[blockZScanIdx].refIdx[refPicListIdx]];

            if (currRefTb == neibRefTb)
            {
                pInfo->mvCand[pInfo->numCand] = p_data[blockZScanIdx].mv[refPicListIdx];
                pInfo->numCand++;

                return true;
            }
        }
    }
    else
    {
        currRefTb = refPicList[refPicListIdx].m_deltaPoc[refIdx];
        isCurrRefLongTerm = refPicList[refPicListIdx].m_isLongTermRef[refIdx];

        for (i = 0; i < 2; i++)
        {
            if (p_data[blockZScanIdx].refIdx[refPicListIdx] >= 0)
            {
                Ipp32s neibRefTb = refPicList[refPicListIdx].m_deltaPoc[p_data[blockZScanIdx].refIdx[refPicListIdx]];
                Ipp8u isNeibRefLongTerm = refPicList[refPicListIdx].m_isLongTermRef[p_data[blockZScanIdx].refIdx[refPicListIdx]];

                H265MV cMvPred, rcMv;

                cMvPred = p_data[blockZScanIdx].mv[refPicListIdx];

                if (isCurrRefLongTerm || isNeibRefLongTerm)
                {
                    rcMv = cMvPred;
                }
                else
                {
                    Ipp32s scale = GetDistScaleFactor(currRefTb, neibRefTb);

                    if (scale == 4096)
                    {
                        rcMv = cMvPred;
                    }
                    else
                    {
                        rcMv.mvx = (Ipp16s)Saturate(-32768, 32767, (scale * cMvPred.mvx + 127 + (scale * cMvPred.mvx < 0)) >> 8);
                        rcMv.mvy = (Ipp16s)Saturate(-32768, 32767, (scale * cMvPred.mvy + 127 + (scale * cMvPred.mvy < 0)) >> 8);
                    }
                }

                pInfo->mvCand[pInfo->numCand] = rcMv;
                pInfo->numCand++;

                return true;
            }

            refPicListIdx = 1 - refPicListIdx;
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

static void GetMergeCandInfo(MVPInfo * pInfo, bool *abCandIsInter, Ipp32s *puhInterDirNeighbours,
                             Ipp32s blockZScanIdx, H265CUData* blockLCU)
{
    Ipp32s iCount = pInfo->numCand;

    abCandIsInter[iCount] = true;

    pInfo->mvCand[2 * iCount] = blockLCU[blockZScanIdx].mv[0];
    pInfo->mvCand[2 * iCount + 1] = blockLCU[blockZScanIdx].mv[1];

    pInfo->refIdx[2 * iCount] = blockLCU[blockZScanIdx].refIdx[0];
    pInfo->refIdx[2 * iCount + 1] = blockLCU[blockZScanIdx].refIdx[1];

    puhInterDirNeighbours[iCount] = 0;

    if (pInfo->refIdx[2 * iCount] >= 0)
        puhInterDirNeighbours[iCount] += 1;

    if (pInfo->refIdx[2 * iCount + 1] >= 0)
        puhInterDirNeighbours[iCount] += 2;

    pInfo->numCand++;
}

/* ****************************************************************************** *\
FUNCTION: IsDiffMER
DESCRIPTION:
\* ****************************************************************************** */

inline Ipp32s IsDiffMER(Ipp32s xN, Ipp32s yN, Ipp32s xP, Ipp32s yP, Ipp32s log2ParallelMergeLevel)
{
    //if ((xN >> log2ParallelMergeLevel) != (xP >> log2ParallelMergeLevel))
    //    return true;
    //if ((yN >> log2ParallelMergeLevel) != (yP >> log2ParallelMergeLevel))
    //    return true;
    //return false;
    return ( ((xN ^ xP) | (yN ^ yP)) >> log2ParallelMergeLevel );
}

/* ****************************************************************************** *\
FUNCTION: GetInterMergeCandidates
DESCRIPTION:
\* ****************************************************************************** */

void H265CU::GetInterMergeCandidates(Ipp32s topLeftCUBlockZScanIdx,
                                     Ipp32s partMode,
                                     Ipp32s partIdx,
                                     Ipp32s cuSize,
                                     MVPInfo* pInfo)
{
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
    Ipp32s topLeftBlockZScanIdx;
    Ipp32s topLeftRasterIdx;
    Ipp32s topLeftRow;
    Ipp32s topLeftColumn;
    Ipp32s partWidth, partHeight, partX, partY;
    Ipp32s xP, yP, nPSW, nPSH;
    Ipp32s i;

    for (i = 0; i < 5; i++) {
        abCandIsInter[i] = false;
        isCandAvailable[i] = false;
    }

    GetPartOffsetAndSize(partIdx, partMode, cuSize, partX, partY, partWidth, partHeight);

    topLeftRasterIdx = h265_scan_z2r[maxDepth][topLeftCUBlockZScanIdx] + partX + numMinTUInLCU * partY;
    topLeftRow = topLeftRasterIdx >> maxDepth;
    topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
    topLeftBlockZScanIdx = h265_scan_r2z[maxDepth][topLeftRasterIdx];

    xP = m_ctbPelX + topLeftColumn * minTUSize;
    yP = m_ctbPelY + topLeftRow * minTUSize;
    nPSW = partWidth * minTUSize;
    nPSH = partHeight * minTUSize;

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

    pInfo->numCand = 0;

    for (i = 0; i < 5; i++) {
        if (pInfo->numCand < MAX_NUM_MERGE_CANDS-1)  {
            candLCU[i] = GetNeighbour(candZScanIdx[i], candColumn[i], candRow[i], topLeftBlockZScanIdx, checkCurLCU[i]);

            if (candLCU[i] && !IsDiffMER(canXP[i], canYP[i], xP, yP, m_par->cpps->log2_parallel_merge_level))
                candLCU[i] = NULL;

            if ((candLCU[i] == NULL) || (candLCU[i][candZScanIdx[i]].predMode == MODE_INTRA))
                isCandInter[i] = false;
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
                    GetMergeCandInfo(pInfo, abCandIsInter, puhInterDirNeighbours, candZScanIdx[i], candLCU[i]);
            }
        }
    }

    if (m_par->TMVPFlag) {
        H265CUData *currPb = m_data + topLeftBlockZScanIdx;
        H265MV mvCol;

        puhInterDirNeighbours[pInfo->numCand] = 0;
        if (GetTempMvPred(currPb, topLeftColumn, topLeftRow, partWidth, partHeight, 0, 0, &mvCol)) {
            abCandIsInter[pInfo->numCand] = true;
            puhInterDirNeighbours[pInfo->numCand] |= 1;
            pInfo->mvCand[2 * pInfo->numCand + 0] = mvCol;
            pInfo->refIdx[2 * pInfo->numCand + 0] = 0;
        }
        if (m_cslice->slice_type == B_SLICE) {
            if (GetTempMvPred(currPb, topLeftColumn, topLeftRow, partWidth, partHeight, 1, 0, &mvCol)) {
                abCandIsInter[pInfo->numCand] = true;
                puhInterDirNeighbours[pInfo->numCand] |= 2;
                pInfo->mvCand[2 * pInfo->numCand + 1] = mvCol;
                pInfo->refIdx[2 * pInfo->numCand + 1] = 0;
            }
        }
        if (abCandIsInter[pInfo->numCand])
            pInfo->numCand++;
    }

    /* combined bi-predictive merging candidates */
    if (m_cslice->slice_type == B_SLICE) {
        Ipp32s uiPriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
        Ipp32s uiPriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};
        Ipp32s limit = pInfo->numCand * (pInfo->numCand - 1);

        for (i = 0; i < limit && pInfo->numCand < MAX_NUM_MERGE_CANDS; i++) {
            Ipp32s l0idx = uiPriorityList0[i];
            Ipp32s l1idx = uiPriorityList1[i];

            if (abCandIsInter[l0idx] && (puhInterDirNeighbours[l0idx] & 1) &&
                abCandIsInter[l1idx] && (puhInterDirNeighbours[l1idx] & 2))
            {
                abCandIsInter[pInfo->numCand] = true;
                puhInterDirNeighbours[pInfo->numCand] = 3;

                pInfo->mvCand[2*pInfo->numCand] = pInfo->mvCand[2*l0idx];
                pInfo->refIdx[2*pInfo->numCand] = pInfo->refIdx[2*l0idx];

                pInfo->mvCand[2*pInfo->numCand+1] = pInfo->mvCand[2*l1idx+1];
                pInfo->refIdx[2*pInfo->numCand+1] = pInfo->refIdx[2*l1idx+1];

                if ((m_currFrame->m_refPicList[0].m_deltaPoc[pInfo->refIdx[2 * pInfo->numCand + 0]] ==
                     m_currFrame->m_refPicList[1].m_deltaPoc[pInfo->refIdx[2 * pInfo->numCand + 1]]) &&
                    (pInfo->mvCand[2 * pInfo->numCand].mvx == pInfo->mvCand[2 * pInfo->numCand + 1].mvx) &&
                    (pInfo->mvCand[2 * pInfo->numCand].mvy == pInfo->mvCand[2 * pInfo->numCand + 1].mvy))
                {
                    abCandIsInter[pInfo->numCand] = false;
                }
                else
                    pInfo->numCand++;
            }
        }
    }

    /* zero motion vector merging candidates */
    {
        Ipp32s numRefIdx = m_cslice->num_ref_idx_l0_active;
        Ipp8s r = 0;
        Ipp8s refCnt = 0;

        if (m_cslice->slice_type == B_SLICE)
            if (m_cslice->num_ref_idx_l1_active < m_cslice->num_ref_idx_l0_active)
                numRefIdx = m_cslice->num_ref_idx_l1_active;

        while (pInfo->numCand < MAX_NUM_MERGE_CANDS) {
            r = (refCnt < numRefIdx) ? refCnt : 0;
            pInfo->mvCand[2*pInfo->numCand].mvx = 0;
            pInfo->mvCand[2*pInfo->numCand].mvy = 0;
            pInfo->refIdx[2*pInfo->numCand]   = r;

            pInfo->mvCand[2*pInfo->numCand+1].mvx = 0;
            pInfo->mvCand[2*pInfo->numCand+1].mvy = 0;
            pInfo->refIdx[2*pInfo->numCand+1]   = r;

            pInfo->numCand++;
            refCnt++;
        }
    }
}

/* ****************************************************************************** *\
FUNCTION: IsCandFound
DESCRIPTION:
\* ****************************************************************************** */

static bool IsCandFound(const Ipp8s *curRefIdx, const H265MV *curMV, const MVPInfo *mergeInfo,
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
FUNCTION: GetPuMvInfo
DESCRIPTION:
\* ****************************************************************************** */

void H265CU::GetPuMvInfo(Ipp32s blockZScanIdx,
                         Ipp32s partAddr,
                         Ipp32s partMode,
                         Ipp32s curPUidx)
{
    Ipp32s CUSizeInMinTU = m_data[blockZScanIdx].size >> m_par->Log2MinTUSize;
    MVPInfo pInfo;
    Ipp8s curRefIdx[2];
    H265MV   curMV[2];
    Ipp32s numRefLists = 1;
    Ipp8u i;
    MVPInfo mergeInfo;
    Ipp32s topLeftBlockZScanIdx = blockZScanIdx;
    blockZScanIdx += partAddr;

    pInfo.numCand = 0;
    mergeInfo.numCand = 0;

    if (m_cslice->slice_type == B_SLICE)
    {
        numRefLists = 2;

        if (BIPRED_RESTRICT_SMALL_PU)
        {
            if ((m_data[blockZScanIdx].size == 8) && (partMode != PART_SIZE_2Nx2N) &&
                (m_data[blockZScanIdx].interDir & INTER_DIR_PRED_L0))
            {
                numRefLists = 1;
            }
        }
    }

    m_data[blockZScanIdx].flags.mergeFlag = 0;
    m_data[blockZScanIdx].mvpNum[0] = m_data[blockZScanIdx].mvpNum[1] = 0;

    curRefIdx[0] = -1;
    curRefIdx[1] = -1;

    curMV[0].mvx = m_data[blockZScanIdx].mv[0].mvx;//   curCUData->MVxl0[curPUidx];
    curMV[0].mvy = m_data[blockZScanIdx].mv[0].mvy;//   curCUData->MVyl0[curPUidx];
    curMV[1].mvx = m_data[blockZScanIdx].mv[1].mvx;//   curCUData->MVxl1[curPUidx];
    curMV[1].mvy = m_data[blockZScanIdx].mv[1].mvy;//   curCUData->MVyl1[curPUidx];

    if (m_data[blockZScanIdx].interDir & INTER_DIR_PRED_L0)
    {
        curRefIdx[0] = m_data[blockZScanIdx].refIdx[0];
    }

    if ((m_cslice->slice_type == B_SLICE) && (m_data[blockZScanIdx].interDir & INTER_DIR_PRED_L1))
    {
        curRefIdx[1] = m_data[blockZScanIdx].refIdx[1];
    }

    if ((m_par->cpps->log2_parallel_merge_level > 2) && (m_data[blockZScanIdx].size == 8))
    {
        if (curPUidx == 0)
        {
            GetInterMergeCandidates(topLeftBlockZScanIdx, PART_SIZE_2Nx2N, curPUidx, CUSizeInMinTU, &mergeInfo);
        }
    }
    else
    {
        GetInterMergeCandidates(topLeftBlockZScanIdx, partMode, curPUidx, CUSizeInMinTU, &mergeInfo);
    }

    for (i = 0; i < mergeInfo.numCand; i++)
    {
        if (IsCandFound(curRefIdx, curMV, &mergeInfo, i, numRefLists))
        {
            m_data[blockZScanIdx].flags.mergeFlag = 1;
            m_data[blockZScanIdx].mergeIdx = i;
            break;
        }
    }

    if (!m_data[blockZScanIdx].flags.mergeFlag)
    {
        Ipp32s refList;

        for (refList = 0; refList < 2; refList++)
        {
            if (curRefIdx[refList] >= 0)
            {
                Ipp32s minDist = 0;

                GetMvpCand(topLeftBlockZScanIdx, refList, curRefIdx[refList], partMode, curPUidx, CUSizeInMinTU, &pInfo);

                m_data[blockZScanIdx].mvpNum[refList] = (Ipp8s)pInfo.numCand;

                /* find the best MV cand */

                minDist = abs(curMV[refList].mvx - pInfo.mvCand[0].mvx) +
                          abs(curMV[refList].mvy - pInfo.mvCand[0].mvy);
                m_data[blockZScanIdx].mvpIdx[refList] = 0;

                for (i = 1; i < pInfo.numCand; i++)
                {
                    Ipp32s dist = abs(curMV[refList].mvx - pInfo.mvCand[i].mvx) +
                               abs(curMV[refList].mvy - pInfo.mvCand[i].mvy);

                    if (dist < minDist)
                    {
                        minDist = dist;
                        m_data[blockZScanIdx].mvpIdx[refList] = i;
                    }
                }

                m_data[blockZScanIdx].mvd[refList].mvx = curMV[refList].mvx - pInfo.mvCand[m_data[blockZScanIdx].mvpIdx[refList]].mvx;
                m_data[blockZScanIdx].mvd[refList].mvy = curMV[refList].mvy - pInfo.mvCand[m_data[blockZScanIdx].mvpIdx[refList]].mvy;
            }
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

void H265CU::GetMvpCand(Ipp32s topLeftCUBlockZScanIdx,
                        Ipp32s refPicListIdx,
                        Ipp32s refIdx,
                        Ipp32s partMode,
                        Ipp32s partIdx,
                        Ipp32s cuSize,
                        MVPInfo* pInfo)
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

    pInfo->numCand = 0;

    if (refIdx < 0)
    {
        return;
    }

    GetPartOffsetAndSize(partIdx, partMode, cuSize, partX, partY, partWidth, partHeight);

    topLeftRasterIdx = h265_scan_z2r[maxDepth][topLeftCUBlockZScanIdx] + partX + numMinTUInLCU * partY;
    topLeftRow = topLeftRasterIdx >> maxDepth;
    topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
    topLeftBlockZScanIdx = h265_scan_r2z[maxDepth][topLeftRasterIdx];

    /* Get Spatial MV */

    /* Left predictor search */

    belowLeftCandLCU = GetNeighbour(belowLeftCandBlockZScanIdx, topLeftColumn - 1,
                                    topLeftRow + partHeight, topLeftBlockZScanIdx, true);
    bAdded = AddMvpCand(pInfo, belowLeftCandLCU, belowLeftCandBlockZScanIdx, refPicListIdx, refIdx, false);

    if (!bAdded)
    {
        leftCandLCU = GetNeighbour(leftCandBlockZScanIdx, topLeftColumn - 1,
                                   topLeftRow + partHeight - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMvpCand(pInfo, leftCandLCU, leftCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (!bAdded)
    {
        bAdded = AddMvpCand(pInfo, belowLeftCandLCU, belowLeftCandBlockZScanIdx, refPicListIdx, refIdx, true);

        if (!bAdded)
        {
            bAdded = AddMvpCand(pInfo, leftCandLCU, leftCandBlockZScanIdx, refPicListIdx, refIdx, true);
        }
    }

    /* Above predictor search */

    aboveRightCandLCU = GetNeighbour(aboveRightCandBlockZScanIdx, topLeftColumn + partWidth,
                                     topLeftRow - 1, topLeftBlockZScanIdx, true);
    bAdded = AddMvpCand(pInfo, aboveRightCandLCU, aboveRightCandBlockZScanIdx, refPicListIdx, refIdx, false);

    if (!bAdded)
    {
        aboveCandLCU = GetNeighbour(aboveCandBlockZScanIdx, topLeftColumn + partWidth - 1,
                                    topLeftRow - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMvpCand(pInfo, aboveCandLCU, aboveCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (!bAdded)
    {
        aboveLeftCandLCU = GetNeighbour(aboveLeftCandBlockZScanIdx, topLeftColumn - 1,
                                        topLeftRow - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMvpCand(pInfo, aboveLeftCandLCU, aboveLeftCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (pInfo->numCand == 2)
    {
        bAdded = true;
    }
    else
    {
        bAdded = ((belowLeftCandLCU != NULL) &&
                  (belowLeftCandLCU[belowLeftCandBlockZScanIdx].predMode != MODE_INTRA));

        if (!bAdded)
        {
            bAdded = ((leftCandLCU != NULL) &&
                      (leftCandLCU[leftCandBlockZScanIdx].predMode != MODE_INTRA));
        }
    }

    if (!bAdded)
    {
        bAdded = AddMvpCand(pInfo, aboveRightCandLCU, aboveRightCandBlockZScanIdx, refPicListIdx, refIdx, true);
        if (!bAdded)
        {
            bAdded = AddMvpCand(pInfo, aboveCandLCU, aboveCandBlockZScanIdx, refPicListIdx, refIdx, true);
        }

        if (!bAdded)
        {
            bAdded = AddMvpCand(pInfo, aboveLeftCandLCU, aboveLeftCandBlockZScanIdx, refPicListIdx, refIdx, true);
        }
    }

    if (pInfo->numCand == 2 && pInfo->mvCand[0] == pInfo->mvCand[1])
        pInfo->numCand = 1;

    if (pInfo->numCand < 2) {
        if (m_par->TMVPFlag) {
            H265CUData *currPb = m_data + topLeftBlockZScanIdx;
            H265MV mvCol;
            bool availFlagCol = GetTempMvPred(currPb, topLeftColumn, topLeftRow, partWidth,
                                              partHeight, refPicListIdx, refIdx, &mvCol);
            if (availFlagCol)
                pInfo->mvCand[pInfo->numCand++] = mvCol;
        }
    }

    while (pInfo->numCand < 2) {
        pInfo->mvCand[pInfo->numCand].mvx = 0;
        pInfo->mvCand[pInfo->numCand].mvy = 0;
        pInfo->numCand++;
    }

    return;
}

static Ipp8u isEqualRef(RefPicList *ref_list, Ipp8s idx1, Ipp8s idx2)
{
    if (idx1 < 0 || idx2 < 0) return 0;
    return ref_list->m_refFrames[idx1] == ref_list->m_refFrames[idx2];
}

Ipp8u H265CU::GetNumPartInter(Ipp32s absPartIdx)
{
    Ipp8u iNumPart = 0;

    switch (m_data[absPartIdx].partSize)
    {
        case PART_SIZE_2Nx2N:
            iNumPart = 1;
            break;
        case PART_SIZE_2NxN:
            iNumPart = 2;
            break;
        case PART_SIZE_Nx2N:
            iNumPart = 2;
            break;
        case PART_SIZE_NxN:
            iNumPart = 4;
            break;
        case PART_SIZE_2NxnU:
            iNumPart = 2;
            break;
        case PART_SIZE_2NxnD:
            iNumPart = 2;
            break;
        case PART_SIZE_nLx2N:
            iNumPart = 2;
            break;
        case PART_SIZE_nRx2N:
            iNumPart = 2;
            break;
        default:
            VM_ASSERT(0);
            break;
    }

    return  iNumPart;
}

/* ****************************************************************************** *\
FUNCTION: GetPuMvPredictorInfo
DESCRIPTION: collects MV predictors.
    Merge MV refidx are interleaved,
    amvp refidx are separated.
\* ****************************************************************************** */

void H265CU::GetPuMvPredictorInfo(Ipp32s blockZScanIdx,
                                  Ipp32s partAddr,
                                  Ipp32s partMode,
                                  Ipp32s curPUidx,
                                  MVPInfo *pInfo,
                                  MVPInfo& mergeInfo)
{
    Ipp32s CUSizeInMinTU = m_data[blockZScanIdx].size >> m_par->Log2MinTUSize;
    Ipp32s numRefLists = 1;
    Ipp32s topLeftBlockZScanIdx = blockZScanIdx;
    blockZScanIdx += partAddr;

    if (m_cslice->slice_type == B_SLICE)
    {
        numRefLists = 2;

        if (BIPRED_RESTRICT_SMALL_PU)
        {
            if ((m_data[blockZScanIdx].size == 8) && (partMode != PART_SIZE_2Nx2N) &&
                (m_data[blockZScanIdx].interDir & INTER_DIR_PRED_L0))
            {
                numRefLists = 1;
            }
        }
    }

    m_data[blockZScanIdx].flags.mergeFlag = 0;
    m_data[blockZScanIdx].mvpNum[0] = m_data[blockZScanIdx].mvpNum[1] = 0;


    if ((m_par->cpps->log2_parallel_merge_level > 2) && (m_data[blockZScanIdx].size == 8))
    {
        if (curPUidx == 0)
        {
            GetInterMergeCandidates(topLeftBlockZScanIdx, PART_SIZE_2Nx2N, curPUidx, CUSizeInMinTU, &mergeInfo);
        }
    }
    else
    {
        GetInterMergeCandidates(topLeftBlockZScanIdx, partMode, curPUidx, CUSizeInMinTU, &mergeInfo);
    }

    //if (!m_data[blockZScanIdx].flags.merge_flag)
    {
        Ipp32s refList, i;

        for (refList = 0; refList < ((m_cslice->slice_type == B_SLICE) ? 2 : 1); refList++)
        {
            for (Ipp8s cur_ref = 0; cur_ref < m_cslice->num_ref_idx[refList]; cur_ref++) {

                pInfo[cur_ref*2+refList].numCand = 0;
//                if (curRefIdx[refList] >= 0)
                {
                    GetMvpCand(topLeftBlockZScanIdx, refList, cur_ref, partMode, curPUidx, CUSizeInMinTU, &pInfo[cur_ref*2+refList]);
                    m_data[blockZScanIdx].mvpNum[refList] = (Ipp8s)pInfo[cur_ref*2+refList].numCand;
                    for (i=0; i<pInfo[cur_ref*2+refList].numCand; i++) {
                        pInfo[cur_ref*2+refList].refIdx[i] = cur_ref;
                    }
                }
            }
        }
    }
}

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
//    GetInterMergeCandidates(absPartIdx, PART_SIZE_2Nx2N, 0, CUSizeInMinTU, &mergeInfo);
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
//        InterPredCu(absPartIdx, depth, m_interPredPtr[depth], MAX_CU_SIZE, 1);
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

CostType H265CU::CalcCostSkip(Ipp32u absPartIdx, Ipp8u depth)
{
    CostType cost_temp, cost_best = COST_MAX;
    Ipp32s cand_best = 0;
    Ipp8u inter_dir_best = 0;
    Ipp8u size = (Ipp8u)(m_par->MaxCUSize>>depth);
    Ipp32u num_parts = ( m_par->NumPartInCU >> (depth<<1) );

    Ipp32s maxDepth = m_par->Log2MaxCUSize - m_par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    MVPInfo mergeInfo;

    H265MEInfo me_info;
    me_info.absPartIdx = absPartIdx;
    me_info.depth = depth;
    me_info.width  = size;
    me_info.height = size;
    me_info.posx = (Ipp8u)((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
    me_info.posy = (Ipp8u)((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
    me_info.splitMode = PART_SIZE_2Nx2N;

    m_data[absPartIdx].mvpNum[0] = m_data[absPartIdx].mvpNum[1] = 0;
    m_data[absPartIdx].flags.skippedFlag = 1;
    m_data[absPartIdx].flags.mergeFlag = 1;
    m_data[absPartIdx].size = size;
    m_data[absPartIdx].predMode = MODE_INTER;
    m_data[absPartIdx].partSize = PART_SIZE_2Nx2N;
    m_data[absPartIdx].qp = m_par->QP;
    m_data[absPartIdx].depth = depth;
    m_data[absPartIdx].trIdx = 0;
    m_data[absPartIdx].cbf[0] = m_data[absPartIdx].cbf[1] = m_data[absPartIdx].cbf[2] = 0;

    Ipp32s CUSizeInMinTU = m_data[absPartIdx].size >> m_par->Log2MinTUSize;

    GetInterMergeCandidates(absPartIdx, PART_SIZE_2Nx2N, 0, CUSizeInMinTU, &mergeInfo);

    PixType *pSrc = m_ySrc + me_info.posx + me_info.posy * m_pitchSrc;
    m_interpIdxFirst = m_interpIdxLast = 0; // for MatchingMetricBipredPu optimization

    if (mergeInfo.numCand > 0) {
        for (Ipp32s i = 0; i < mergeInfo.numCand; i++) {
            Ipp8s *ref_idx = &(mergeInfo.refIdx[2*i]);
            if (m_cslice->slice_type != B_SLICE)
                ref_idx[1] = -1;

            if (i > 0) {
                Ipp8u same_flag = 0;
                for (Ipp32s j = 0; j < i; j++) {
                    if (mergeInfo.mvCand[2*i] == mergeInfo.mvCand[2*j] &&
                        mergeInfo.mvCand[2*i+1] == mergeInfo.mvCand[2*j+1] &&
                        mergeInfo.refIdx[2*i] == mergeInfo.refIdx[2*j] &&
                        mergeInfo.refIdx[2*i+1] == mergeInfo.refIdx[2*j+1]) {
                            same_flag = 1;
                            break;
                    }
                }
                if (same_flag)
                    continue;
            }

            Ipp8u inter_dir = 0;
            if (ref_idx[0] >= 0)
                inter_dir |= INTER_DIR_PRED_L0;
            if (ref_idx[1] >= 0)
                inter_dir |= INTER_DIR_PRED_L1;

            H265MV *mv = &(mergeInfo.mvCand[2*i]);
            if (inter_dir == (INTER_DIR_PRED_L0 | INTER_DIR_PRED_L1)) {
                H265Frame *PicYUVRefF = m_currFrame->m_refPicList[0].m_refFrames[ref_idx[0]];
                H265Frame *PicYUVRefB = m_currFrame->m_refPicList[1].m_refFrames[ref_idx[1]];
                if (PicYUVRefF && PicYUVRefB)
                    cost_temp = MatchingMetricBipredPu(pSrc, &me_info, PicYUVRefF->y, PicYUVRefF->pitch_luma, PicYUVRefB->y, PicYUVRefB->pitch_luma, mv, 0);
                else
                    cost_temp = COST_MAX;
            } else {
                EnumRefPicList dir = inter_dir == INTER_DIR_PRED_L0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
                H265Frame *PicYUVRef = m_currFrame->m_refPicList[dir].m_refFrames[ref_idx[dir]];
                cost_temp = MatchingMetricPu(pSrc, &me_info, mv+dir, PicYUVRef, 0); // approx MvCost
            }

            if (cost_best > cost_temp) {
                cost_best = cost_temp;
                cand_best = i;
                inter_dir_best = inter_dir;
            }
        }
        for (Ipp32s dir = 0; dir < 2; dir++) {
            m_data[absPartIdx].mv[dir] = mergeInfo.mvCand[2*cand_best+dir];
            m_data[absPartIdx].refIdx[dir] = mergeInfo.refIdx[2*cand_best+dir];
        }
        m_data[absPartIdx].mergeIdx = (Ipp8u)cand_best;
        m_data[absPartIdx].interDir = inter_dir_best;
        InterPredCu(absPartIdx, depth, m_yRec, m_pitchRec, 1);
        PixType *pRec = m_yRec + ((PUStartRow * m_pitchRec + PUStartColumn) << m_par->Log2MinTUSize);
        PixType *pSrc = m_ySrc + ((PUStartRow * m_pitchSrc + PUStartColumn) << m_par->Log2MinTUSize);
        cost_best = TuSse(pSrc, m_pitchSrc, pRec, m_pitchRec, size);
        // add chroma with little weight
        InterPredCu(absPartIdx, depth, m_uvRec, m_pitchRec, 0);
        pRec = m_uvRec + (((PUStartRow>>1) * m_pitchRec + PUStartColumn) << m_par->Log2MinTUSize);
        pSrc = m_uvSrc + (((PUStartRow>>1) * m_pitchSrc + PUStartColumn) << m_par->Log2MinTUSize);
        cost_temp = TuSse(pSrc, m_pitchSrc, pRec, m_pitchRec, size>>1);
        pRec += size>>1;
        pSrc += size>>1;
        cost_temp += TuSse(pSrc, m_pitchSrc, pRec, m_pitchRec, size>>1);
        if (!(m_par->AnalyseFlags & HEVC_COST_CHROMA))
            cost_temp /= 4;
        cost_best += cost_temp;
    }

    if (cost_best < COST_MAX) {
        for (Ipp32u i = 1; i < num_parts; i++)
            m_data[absPartIdx+i] = m_data[absPartIdx];
    }

    return cost_best;
}

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
    H265MV mvdiff = {mv1->mvx - mv2->mvx, mv1->mvy - mv2->mvy};
    return qcost(&mvdiff);
}

inline Ipp32s qdist(const H265MV *mv1, const H265MV *mv2)
{
    Ipp32s dx = ABS(mv1->mvx - mv2->mvx);
    Ipp32s dy = ABS(mv1->mvy - mv2->mvy);
    return (dx + dy);
}


bool H265CU::CheckGpuIntraCost(Ipp32s leftPelX, Ipp32s topPelY, Ipp32s depth) const
{
    bool tryIntra = true;

    if (m_par->enableCmFlag && m_par->cmIntraThreshold != 0 && m_cslice->slice_type != I_SLICE) {
        mfxU32 cmIntraCost = 0;
        mfxU32 cmInterCost = 0;
        Ipp32s cuSize = m_par->MaxCUSize >> depth;
        Ipp32s x = leftPelX / 16;
        Ipp32s y = topPelY / 16;
        Ipp32s x_num = (cuSize == 32) ? 2 : 1;
        Ipp32s y_num = (cuSize == 32) ? 2 : 1;
            
        for (Ipp32s j = y; j < y + y_num; j++) {
            for (Ipp32s i = x; i < x + x_num; i++) {
                CmMbIntraDist *intraDistLine = (CmMbIntraDist *)((Ipp8u *)cmMbIntraDist[cmCurIdx] + j * intraPitch[cmCurIdx]);
                cmIntraCost += intraDistLine[i].intraDist;
                
                mfxU32 cmInterCost16x16 = IPP_MAX_32U;
                for (Ipp8s ref_idx = 0; ref_idx < m_cslice->num_ref_idx[0]; ref_idx++) {
                    CmMbDist16 *dist16x16line = (CmMbDist16 *)((Ipp8u *)(cmDist16x16[cmCurIdx][ref_idx]) + j * dist16x16Pitch);
                    CmMbDist16 *dist16x8line0 = (CmMbDist16 *)((Ipp8u *)(cmDist16x8[cmCurIdx][ref_idx]) + (j * 2) * dist16x8Pitch);
                    CmMbDist16 *dist16x8line1 = (CmMbDist16 *)((Ipp8u *)(cmDist16x8[cmCurIdx][ref_idx]) + (j * 2 + 1) * dist16x8Pitch);
                    CmMbDist16 *dist8x16line = (CmMbDist16 *)((Ipp8u *)(cmDist8x16[cmCurIdx][ref_idx]) + j * dist8x16Pitch);
                    CmMbDist16 *dist8x8line0 = (CmMbDist16 *)((Ipp8u *)(cmDist8x8[cmCurIdx][ref_idx]) + (j * 2) * dist8x8Pitch);
                    CmMbDist16 *dist8x8line1 = (CmMbDist16 *)((Ipp8u *)(cmDist8x8[cmCurIdx][ref_idx]) + (j * 2 + 1) * dist8x8Pitch);
                    CmMbDist16 *dist8x4line0 = (CmMbDist16 *)((Ipp8u *)(cmDist8x4[cmCurIdx][ref_idx]) + (j * 4) * dist8x4Pitch);
                    CmMbDist16 *dist8x4line1 = (CmMbDist16 *)((Ipp8u *)(cmDist8x4[cmCurIdx][ref_idx]) + (j * 4 + 1) * dist8x4Pitch);
                    CmMbDist16 *dist8x4line2 = (CmMbDist16 *)((Ipp8u *)(cmDist8x4[cmCurIdx][ref_idx]) + (j * 4 + 2) * dist8x4Pitch);
                    CmMbDist16 *dist8x4line3 = (CmMbDist16 *)((Ipp8u *)(cmDist8x4[cmCurIdx][ref_idx]) + (j * 4 + 3) * dist8x4Pitch);
                    CmMbDist16 *dist4x8line0 = (CmMbDist16 *)((Ipp8u *)(cmDist4x8[cmCurIdx][ref_idx]) + (j * 2) * dist4x8Pitch);
                    CmMbDist16 *dist4x8line1 = (CmMbDist16 *)((Ipp8u *)(cmDist4x8[cmCurIdx][ref_idx]) + (j * 2 + 1) * dist4x8Pitch);
                    Ipp32u dist16x16 = dist16x16line[i].dist;
                    Ipp32u dist16x8 = dist16x8line0[i].dist + dist16x8line1[i].dist;
                    Ipp32u dist8x16 = dist8x16line[i * 2].dist + dist8x16line[i * 2 + 1].dist;
                    Ipp32u dist8x8 = dist8x8line0[i * 2].dist + dist8x8line0[i * 2 + 1].dist +
                                        dist8x8line1[i * 2].dist + dist8x8line1[i * 2 + 1].dist;
                    Ipp32u dist8x4 = dist8x4line0[i * 2].dist + dist8x4line0[i * 2 + 1].dist +
                                        dist8x4line1[i * 2].dist + dist8x4line1[i * 2 + 1].dist +
                                        dist8x4line2[i * 2].dist + dist8x4line2[i * 2 + 1].dist +
                                        dist8x4line3[i * 2].dist + dist8x4line3[i * 2 + 1].dist;
                    Ipp32u dist4x8 = dist4x8line0[i * 4 + 0].dist + dist4x8line0[i * 4 + 1].dist +
                                        dist4x8line0[i * 4 + 2].dist + dist4x8line0[i * 4 + 3].dist +
                                        dist4x8line1[i * 4 + 0].dist + dist4x8line1[i * 4 + 1].dist +
                                        dist4x8line1[i * 4 + 2].dist + dist4x8line1[i * 4 + 3].dist;
                    if (dist16x16 < cmInterCost16x16)
                        cmInterCost16x16 = dist16x16; 
                    if (dist16x8 < cmInterCost16x16)
                        cmInterCost16x16 = dist16x8; 
                    if (dist8x16 < cmInterCost16x16)
                        cmInterCost16x16 = dist8x16; 
                    if (dist8x8 < cmInterCost16x16)
                        cmInterCost16x16 = dist8x8; 
                    if (dist8x4 < cmInterCost16x16)
                        cmInterCost16x16 = dist8x4; 
                    if (dist4x8 < cmInterCost16x16)
                        cmInterCost16x16 = dist4x8;
                }
                cmInterCost += cmInterCost16x16;
            }
        }

        tryIntra = cmIntraCost < (m_par->cmIntraThreshold / 256.0) * cmInterCost;
    }

    return tryIntra;
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
