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

#include "mfx_h265_ctb.h"
#include "mfx_h265_enc.h"
#include "ippi.h"
#include "ippvc.h"

namespace H265Enc {

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

Ipp32u H265CU::GetQuadtreeTuLog2MinSizeInCu(Ipp32u absPartIdx )
{
    return GetQuadtreeTuLog2MinSizeInCu(absPartIdx, m_data[absPartIdx].size,
        m_data[absPartIdx].partSize, m_data[absPartIdx].predMode);
}

Ipp32u H265CU::GetQuadtreeTuLog2MinSizeInCu(Ipp32u absPartIdx, Ipp32s size, Ipp8u partSize, Ipp8u pred_mode)
{
    Ipp32u log2CbSize = h265_log2m2[size] + 2;
    Ipp32u quadtreeTUMaxDepth = pred_mode == MODE_INTRA ?
        m_par->QuadtreeTUMaxDepthIntra: m_par->QuadtreeTUMaxDepthInter;
    Ipp32s intraSplitFlag = ( pred_mode == MODE_INTRA && partSize == PART_SIZE_NxN ) ? 1 : 0;
    Ipp32s interSplitFlag = ((quadtreeTUMaxDepth == 1) && (pred_mode == MODE_INTER) && (partSize != PART_SIZE_2Nx2N) );

    Ipp32u log2MinTUSizeInCU = 0;
    if (log2CbSize < (m_par->QuadtreeTULog2MinSize + quadtreeTUMaxDepth - 1 + interSplitFlag + intraSplitFlag) )
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is < QuadtreeTULog2MinSize
        log2MinTUSizeInCU = m_par->QuadtreeTULog2MinSize;
    }
    else
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still >= QuadtreeTULog2MinSize
        log2MinTUSizeInCU = log2CbSize - ( quadtreeTUMaxDepth - 1 + interSplitFlag + intraSplitFlag); // stop when trafoDepth == hierarchy_depth = splitFlag
        if ( log2MinTUSizeInCU > m_par->QuadtreeTULog2MaxSize)
        {
            // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still > QuadtreeTULog2MaxSize
            log2MinTUSizeInCU = m_par->QuadtreeTULog2MaxSize;
        }
    }
    return log2MinTUSizeInCU;
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

Ipp32s H265CU::GetIntradirLumaPred(Ipp32u absPartIdx, Ipp32s* intraDirPred, Ipp32s* mode)
{
    H265CUPtr tempCU;
    Ipp32s leftIntraDir, aboveIntraDir;
    Ipp32s predNum = 0;

    // Get intra direction of left PU
    GetPuLeft(&tempCU, m_absIdxInLcu + absPartIdx );

    leftIntraDir  = tempCU.ctbData ? ( IS_INTRA(tempCU.ctbData, tempCU.absPartIdx ) ? tempCU.ctbData[tempCU.absPartIdx].intraLumaDir : INTRA_DC ) : INTRA_DC;

    // Get intra direction of above PU
    GetPuAbove(&tempCU, m_absIdxInLcu + absPartIdx, true, true, false, true );

    aboveIntraDir = tempCU.ctbData ? ( IS_INTRA(tempCU.ctbData, tempCU.absPartIdx ) ? tempCU.ctbData[tempCU.absPartIdx].intraLumaDir : INTRA_DC ) : INTRA_DC;

    predNum = 3;
    if (leftIntraDir == aboveIntraDir)
    {
        if (mode)
        {
            *mode = 1;
        }

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
        if (mode)
        {
            *mode = 2;
        }
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

    return predNum;
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
                    PixType *_y, PixType *_uv, Ipp32s _pitch,
                    PixType *_ySrc, PixType *_uvSrc, Ipp32s _pitchSrc, H265BsFake *_bsf, H265Slice *_cslice,
                    Ipp32s initializeDataFlag) {
    m_par = _par;

    m_cslice = _cslice;
    m_bsf = _bsf;
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
    m_pitchSrc = _pitchSrc;
    m_yRec = _y + m_ctbPelX + m_ctbPelY * m_pitchRec;
    m_uvRec = _uv + m_ctbPelX + (m_ctbPelY * m_pitchRec >> 1);

    m_ySrc = _ySrc + m_ctbPelX + m_ctbPelY * m_pitchSrc;

    // aya: may be used to provide performance gain for SAD calculation
    /*{
    IppiSize blkSize = {m_par->MaxCUSize, m_par->MaxCUSize};
    ippiCopy_8u_C1R(_m_ySrc + m_ctbPelX + m_ctbPelY * m_pitchSrc, m_pitchSrc, m_src_aligned_block, MAX_CU_SIZE, blkSize);
    }*/

    m_uvSrc = _uvSrc + m_ctbPelX + (m_ctbPelY * m_pitchSrc >> 1);
    m_depthMin = MAX_TOTAL_DEPTH;

    if (initializeDataFlag) {
        m_rdOptFlag = m_cslice->rd_opt_flag;
        m_rdLambda = m_cslice->rd_lambda;
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
    }
    //int myseed = 12345;

static double rand_val = 1.239847855923875843957;

static unsigned int myrand()
{
    rand_val *= 1.204839573950674;
    if (rand_val > 1024) rand_val /= 1024;
    return ((Ipp32s *)&rand_val)[0];
}


static void genCBF(Ipp8u *cbfy, Ipp8u *cbfu, Ipp8u *cbfv, Ipp8u size, Ipp8u len, Ipp8u depth, Ipp8u max_depth) {
    if (depth > max_depth)
        return;

    Ipp8u y = cbfy[0], u = cbfu[0], v = cbfv[0];

    if (depth == 0 || size > 4) {
        if (depth == 0 || ((u >> (depth - 1)) & 1))
            u |= ((myrand() & 1) << depth);
        else if (depth > 0 && size == 4)
            u |= (((u >> (depth - 1)) & 1) << depth);

        if (depth == 0 || ((v >> (depth - 1)) & 1))
            v |= ((myrand() & 1) << depth);
        else if (depth > 0 && size == 4)
            v |= (((v >> (depth - 1)) & 1) << depth);
    } else {
            u |= (((u >> (depth - 1)) & 1) << depth);
            v |= (((v >> (depth - 1)) & 1) << depth);
    }
    if (depth > 0 || ((u >> depth) & 1) || ((v >> depth) & 1))
        y |= ((myrand() & 1) << depth);
    else
        y |= (1 << depth);

    memset(cbfy, y, len);
    memset(cbfu, u, len);
    memset(cbfv, v, len);

    for (Ipp32u i = 0; i < 4; i++)
        genCBF(cbfy + len/4 * i,cbfu + len/4 * i,cbfv + len/4 * i,
        size >> 1, len/4, depth + 1, max_depth);
}

void H265CU::FillRandom(Ipp32u absPartIdx, Ipp8u depth)
{
    Ipp32u num_parts = ( m_par->NumPartInCU >> (depth<<1) );
    Ipp32u i;
  Ipp32u lpel_x   = m_ctbPelX +
      ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
  Ipp32u rpel_x   = lpel_x + (m_par->MaxCUSize>>depth)  - 1;
  Ipp32u tpel_y   = m_ctbPelY +
      ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
  Ipp32u bpel_y   = tpel_y + (m_par->MaxCUSize>>depth) - 1;
/*
  if (depth == 0) {
      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE; i++)
          m_trCoeffY[i] = (Ipp16s)((myrand() & 31) - 16);
      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE/4; i++) {
          m_trCoeffU[i] = (Ipp16s)((myrand() & 31) - 16);
          m_trCoeffV[i] = (Ipp16s)((myrand() & 31) - 16);
      }
  }
*/
    if ((depth < m_par->MaxCUDepth - m_par->AddCUDepth) &&
        (((myrand() & 1) && (m_par->Log2MaxCUSize - depth > m_par->QuadtreeTULog2MinSize)) ||
        rpel_x >= m_par->Width  || bpel_y >= m_par->Height ||
        m_par->Log2MaxCUSize - depth -
            (m_cslice->slice_type == I_SLICE ? m_par->QuadtreeTUMaxDepthIntra :
            MIN(m_par->QuadtreeTUMaxDepthIntra, m_par->QuadtreeTUMaxDepthInter)) > m_par->QuadtreeTULog2MaxSize)) {
        // split
        for (i = 0; i < 4; i++)
            FillRandom(absPartIdx + (num_parts >> 2) * i, depth+1);
    } else {
        Ipp8u pred_mode = MODE_INTRA;
        if ( m_cslice->slice_type != I_SLICE && (myrand() & 15)) {
            pred_mode = MODE_INTER;
        }
        Ipp8u size = (Ipp8u)(m_par->MaxCUSize >> depth);
        Ipp32u MaxDepth = pred_mode == MODE_INTRA ? m_par->QuadtreeTUMaxDepthIntra : m_par->QuadtreeTUMaxDepthInter;
        Ipp8u part_size;

        if (pred_mode == MODE_INTRA) {
            part_size = (Ipp8u)
                (depth == m_par->MaxCUDepth - m_par->AddCUDepth &&
                depth + 1 <= m_par->MaxCUDepth &&
                ((m_par->Log2MaxCUSize - depth - MaxDepth == m_par->QuadtreeTULog2MaxSize) ||
                (myrand() & 1)) ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
        } else {
            if (depth == m_par->MaxCUDepth - m_par->AddCUDepth) {
                if (size == 8) {
                    part_size = (Ipp8u)(myrand() % 3);
                } else {
                    part_size = (Ipp8u)(myrand() % 4);
                }
            } else {
                if (m_par->csps->amp_enabled_flag) {
                    part_size = (Ipp8u)(myrand() % 7);
                    if (part_size == 3) part_size = 7;
                } else {
                    part_size = (Ipp8u)(myrand() % 3);
                }
            }
        }
        Ipp8u intra_split = ((pred_mode == MODE_INTRA && (part_size == PART_SIZE_NxN)) ? 1 : 0);
        Ipp8u inter_split = ((MaxDepth == 1) && (pred_mode == MODE_INTER) && (part_size != PART_SIZE_2Nx2N) ) ? 1 : 0;

        Ipp8u tr_depth = intra_split + inter_split;
        while ((m_par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
        while (tr_depth < (MaxDepth - 1 + intra_split + inter_split) &&
            (m_par->MaxCUSize >> (depth + tr_depth)) > 4 &&
            (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MinSize) &&
            ((myrand() & 1) ||
                (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MaxSize))) tr_depth++;

        for (i = 0; i < num_parts; i++) {
            m_data[absPartIdx + i].cbf[0] = 0;
            m_data[absPartIdx + i].cbf[1] = 0;
            m_data[absPartIdx + i].cbf[2] = 0;
        }

        if (tr_depth > m_par->QuadtreeTUMaxDepthIntra) {
            printf("FillRandom err 1\n"); exit(1);
        }
        if (depth + tr_depth > m_par->MaxCUDepth) {
            printf("FillRandom err 2\n"); exit(1);
        }
        if (m_par->Log2MaxCUSize - (depth + tr_depth) < m_par->QuadtreeTULog2MinSize) {
            printf("FillRandom err 3\n"); exit(1);
        }
        if (m_par->Log2MaxCUSize - (depth + tr_depth) > m_par->QuadtreeTULog2MaxSize) {
            printf("FillRandom err 4\n"); exit(1);
        }
        if (pred_mode == MODE_INTRA) {
            Ipp8u luma_dir = (m_left && m_above) ? (Ipp8u) (myrand() % 35) : INTRA_DC;
            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
                m_data[i].predMode = pred_mode;
                m_data[i].depth = depth;
                m_data[i].size = size;
                m_data[i].partSize = part_size;
                m_data[i].intraLumaDir = luma_dir;
                m_data[i].intraChromaDir = INTRA_DM_CHROMA;
                m_data[i].qp = m_par->QP;
                m_data[i].trIdx = tr_depth;
                m_data[i].interDir = 0;
            }
        } else {
            Ipp16s mvmax = (Ipp16s)(10 + myrand()%100);
            Ipp16s mvx0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvx1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            T_RefIdx ref_idx0 = (T_RefIdx)(myrand() % m_cslice->num_ref_idx_l0_active);
            Ipp8u inter_dir;
            if (m_cslice->slice_type == B_SLICE && part_size != PART_SIZE_2Nx2N && size == 8) {
                inter_dir = (Ipp8u)(1 + myrand()%2);
            } else if (m_cslice->slice_type == B_SLICE) {
                inter_dir = (Ipp8u)(1 + myrand()%3);
            } else {
                inter_dir = 1;
            }
            T_RefIdx ref_idx1 = (T_RefIdx)((inter_dir & INTER_DIR_PRED_L1) ? (myrand() % m_cslice->num_ref_idx_l1_active) : -1);
            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
                m_data[i].predMode = pred_mode;
                m_data[i].depth = depth;
                m_data[i].size = size;
                m_data[i].partSize = part_size;
                m_data[i].intraLumaDir = 0;//luma_dir;
                m_data[i].qp = m_par->QP;
                m_data[i].trIdx = tr_depth;
                m_data[i].interDir = inter_dir;
                m_data[i].refIdx[0] = -1;
                m_data[i].refIdx[1] = -1;
                m_data[i].mv[0].mvx = 0;
                m_data[i].mv[0].mvy = 0;
                m_data[i].mv[1].mvx = 0;
                m_data[i].mv[1].mvy = 0;
                if (inter_dir & INTER_DIR_PRED_L0) {
                    m_data[i].refIdx[0] = ref_idx0;
                    m_data[i].mv[0].mvx = mvx0;
                    m_data[i].mv[0].mvy = mvy0;
                }
                if (inter_dir & INTER_DIR_PRED_L1) {
                    m_data[i].refIdx[1] = ref_idx1;
                    m_data[i].mv[1].mvx = mvx1;
                    m_data[i].mv[1].mvy = mvy1;
                }
                m_data[i].flags.mergeFlag = 0;
                m_data[i].mvd[0].mvx = m_data[i].mvd[0].mvy =
                    m_data[i].mvd[1].mvx = m_data[i].mvd[1].mvy = 0;
                m_data[i].mvpIdx[0] = m_data[i].mvpIdx[1] = 0;
                m_data[i].mvpNum[0] = m_data[i].mvpNum[1] = 0;
            }
            for (Ipp32s i = 0; i < GetNumPartInter(absPartIdx); i++)
            {
                Ipp32s PartAddr;
                Ipp32s PartX, PartY, Width, Height;
                GetPartOffsetAndSize(i, m_data[absPartIdx].partSize,
                    m_data[absPartIdx].size, PartX, PartY, Width, Height);
                GetPartAddr(i, m_data[absPartIdx].partSize, num_parts, PartAddr);

                GetPuMvInfo(absPartIdx, PartAddr, m_data[absPartIdx].partSize, i);
            }

            if ((num_parts < 4 && part_size != PART_SIZE_2Nx2N) ||
                (num_parts < 16 && part_size > 3)) {
                printf("FillRandom err 5\n"); exit(1);
            }
        }
    }
}

void H265CU::FillZero(Ipp32u absPartIdx, Ipp8u depth)
{
    Ipp32u num_parts = ( m_par->NumPartInCU >> (depth<<1) );
    Ipp32u i;
    Ipp32u lpel_x   = m_ctbPelX +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
    Ipp32u rpel_x   = lpel_x + (m_par->MaxCUSize>>depth)  - 1;
    Ipp32u tpel_y   = m_ctbPelY +
        ((h265_scan_z2r[m_par->MaxCUDepth][absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
    Ipp32u bpel_y   = tpel_y + (m_par->MaxCUSize>>depth) - 1;
/*
  if (depth == 0) {
      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE; i++)
          m_trCoeffY[i] = (Ipp16s)((myrand() & 31) - 16);
      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE/4; i++) {
          m_trCoeffU[i] = (Ipp16s)((myrand() & 31) - 16);
          m_trCoeffV[i] = (Ipp16s)((myrand() & 31) - 16);
      }
  }
*/
    if ((depth < m_par->MaxCUDepth - m_par->AddCUDepth) &&
        (((/*myrand() & 1*/0) && (m_par->Log2MaxCUSize - depth > m_par->QuadtreeTULog2MinSize)) ||
        rpel_x >= m_par->Width  || bpel_y >= m_par->Height ||
        m_par->Log2MaxCUSize - depth -
            (m_cslice->slice_type == I_SLICE ? m_par->QuadtreeTUMaxDepthIntra :
            MIN(m_par->QuadtreeTUMaxDepthIntra, m_par->QuadtreeTUMaxDepthInter)) > m_par->QuadtreeTULog2MaxSize)) {
        // split
        for (i = 0; i < 4; i++)
            FillZero(absPartIdx + (num_parts >> 2) * i, depth+1);
    } else {
        Ipp8u pred_mode = MODE_INTRA;
        if ( m_cslice->slice_type != I_SLICE && (myrand() & 15)) {
            pred_mode = MODE_INTER;
        }
        Ipp8u size = (Ipp8u)(m_par->MaxCUSize >> depth);
        Ipp32u MaxDepth = pred_mode == MODE_INTRA ? m_par->QuadtreeTUMaxDepthIntra : m_par->QuadtreeTUMaxDepthInter;
        Ipp8u part_size;
        /*
        if (pred_mode == MODE_INTRA) {
            part_size = (Ipp8u)
                (depth == m_par->MaxCUDepth - m_par->AddCUDepth &&
                depth + 1 <= m_par->MaxCUDepth &&
                ((m_par->Log2MaxCUSize - depth - MaxDepth == m_par->QuadtreeTULog2MaxSize) ||
                (myrand() & 1)) ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
        } else {
            if (depth == m_par->MaxCUDepth - m_par->AddCUDepth) {
                if (size == 8) {
                    part_size = (Ipp8u)(myrand() % 3);
                } else {
                    part_size = (Ipp8u)(myrand() % 4);
                }
            } else {
                if (m_par->csps->amp_enabled_flag) {
                    part_size = (Ipp8u)(myrand() % 7);
                    if (part_size == 3) part_size = 7;
                } else {
                    part_size = (Ipp8u)(myrand() % 3);
                }
            }
        }*/
        part_size = PART_SIZE_2Nx2N;

        Ipp8u intra_split = ((pred_mode == MODE_INTRA && (part_size == PART_SIZE_NxN)) ? 1 : 0);
        Ipp8u inter_split = ((MaxDepth == 1) && (pred_mode == MODE_INTER) && (part_size != PART_SIZE_2Nx2N) ) ? 1 : 0;

        Ipp8u tr_depth = intra_split + inter_split;
        while ((m_par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
        while (tr_depth < (MaxDepth - 1 + intra_split + inter_split) &&
            (m_par->MaxCUSize >> (depth + tr_depth)) > 4 &&
            (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MinSize) &&
            ((0/*myrand() & 1*/) ||
                (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MaxSize))) tr_depth++;

        for (i = 0; i < num_parts; i++) {
            m_data[absPartIdx + i].cbf[0] = 0;
            m_data[absPartIdx + i].cbf[1] = 0;
            m_data[absPartIdx + i].cbf[2] = 0;
        }

        if (tr_depth > m_par->QuadtreeTUMaxDepthIntra) {
            printf("FillZero err 1\n"); exit(1);
        }
        if (depth + tr_depth > m_par->MaxCUDepth) {
            printf("FillZero err 2\n"); exit(1);
        }
        if (m_par->Log2MaxCUSize - (depth + tr_depth) < m_par->QuadtreeTULog2MinSize) {
            printf("FillZero err 3\n"); exit(1);
        }
        if (m_par->Log2MaxCUSize - (depth + tr_depth) > m_par->QuadtreeTULog2MaxSize) {
            printf("FillZero err 4\n"); exit(1);
        }
        if (pred_mode == MODE_INTRA) {
            Ipp8u luma_dir =/* (p_left && p_above) ? (Ipp8u) (myrand() % 35) : */INTRA_DC;
            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
                m_data[i].predMode = pred_mode;
                m_data[i].depth = depth;
                m_data[i].size = size;
                m_data[i].partSize = part_size;
                m_data[i].intraLumaDir = luma_dir;
                m_data[i].intraChromaDir = INTRA_DM_CHROMA;
                m_data[i].qp = m_par->QP;
                m_data[i].trIdx = tr_depth;
                m_data[i].interDir = 0;
            }
        } else {
/*            Ipp16s mvmax = (Ipp16s)(10 + myrand()%100);
            Ipp16s mvx0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvx1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);*/
            T_RefIdx ref_idx0 = 0;//(T_RefIdx)(myrand() % m_cslice->num_ref_idx_l0_active);
            Ipp8u inter_dir;
/*            if (m_cslice->slice_type == B_SLICE && part_size != PART_SIZE_2Nx2N && size == 8) {
                inter_dir = (Ipp8u)(1 + myrand()%2);
            } else if (m_cslice->slice_type == B_SLICE) {
                inter_dir = (Ipp8u)(1 + myrand()%3);
            } else*/ {
                inter_dir = 1;
            }
            T_RefIdx ref_idx1 = (T_RefIdx)((inter_dir & INTER_DIR_PRED_L1) ? (myrand() % m_cslice->num_ref_idx_l1_active) : -1);
            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
                m_data[i].predMode = pred_mode;
                m_data[i].depth = depth;
                m_data[i].size = size;
                m_data[i].partSize = part_size;
                m_data[i].intraLumaDir = 0;//luma_dir;
                m_data[i].qp = m_par->QP;
                m_data[i].trIdx = tr_depth;
                m_data[i].interDir = inter_dir;
                m_data[i].refIdx[0] = -1;
                m_data[i].refIdx[1] = -1;
                m_data[i].mv[0].mvx = 0;
                m_data[i].mv[0].mvy = 0;
                m_data[i].mv[1].mvx = 0;
                m_data[i].mv[1].mvy = 0;
                if (inter_dir & INTER_DIR_PRED_L0) {
                    m_data[i].refIdx[0] = ref_idx0;
//                    m_data[i].mv[0].mvx = mvx0;
//                    m_data[i].mv[0].mvy = mvy0;
                }
                if (inter_dir & INTER_DIR_PRED_L1) {
                    m_data[i].refIdx[1] = ref_idx1;
//                    m_data[i].mv[1].mvx = mvx1;
//                    m_data[i].mv[1].mvy = mvy1;
                }
                m_data[i]._flags = 0;
                m_data[i].flags.mergeFlag = 0;
                m_data[i].mvd[0].mvx = m_data[i].mvd[0].mvy =
                    m_data[i].mvd[1].mvx = m_data[i].mvd[1].mvy = 0;
                m_data[i].mvpIdx[0] = m_data[i].mvpIdx[1] = 0;
                m_data[i].mvpNum[0] = m_data[i].mvpNum[1] = 0;
            }
            for (Ipp32s i = 0; i < GetNumPartInter(absPartIdx); i++)
            {
                Ipp32s PartAddr;
                Ipp32s PartX, PartY, Width, Height;
                GetPartOffsetAndSize(i, m_data[absPartIdx].partSize,
                    m_data[absPartIdx].size, PartX, PartY, Width, Height);
                GetPartAddr(i, m_data[absPartIdx].partSize, num_parts, PartAddr);

                GetPuMvInfo(absPartIdx, PartAddr, m_data[absPartIdx].partSize, i);
            }

            if ((num_parts < 4 && part_size != PART_SIZE_2Nx2N) ||
                (num_parts < 16 && part_size > 3)) {
                printf("FillZero err 5\n"); exit(1);
            }
        }
    }
}

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
            m_depthMin = depth;

        m_data = m_dataTemp + (depth << m_par->Log2NumPartInCU);

        Ipp32s tryIntra = true;
        if (tryIntra)
        {
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
                    IntraLumaModeDecisionRDO(absPartIdxTu, offset + subsize * i, depth, trDepth, ctxSave[1]);
                    costBestPu[i] = m_intraBestCosts[0];

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
                } else if (trDepth == 1) {
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

                for (Ipp8u chromaDir = 0; chromaDir < NUM_CHROMA_MODE; chromaDir++)
                {
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
            }
        }

        // Try Skip mode
        if (m_cslice->slice_type != I_SLICE) {
            CostType costSkip;
            m_data = m_dataSave;

            if (m_rdOptFlag)
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);

            costSkip = CalcCostSkip(absPartIdx, depth);
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
            } else {
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
            } else {
                m_data = m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU);
                m_bsf->CtxRestore(ctxSave[2], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(m_recLumaSaveCu[depth], widthCu, m_yRec + offsetLumaCu, m_pitchRec, roiSizeCu);
            }
        }

        if (splitMode == SPLIT_TRY) {
            ippiCopy_8u_C1R(m_yRec + offsetLumaCu, m_pitchRec, m_recLumaSaveCu[depth], widthCu, roiSizeCu);
        }
    }

    Ipp8u skippedFlag = 0;
    if (splitMode != SPLIT_MUST)
        skippedFlag = (m_dataBest + ((depth + 0) << m_par->Log2NumPartInCU))[absPartIdx].flags.skippedFlag;

    CostType cuSplitThresholdCu = m_cslice->slice_type == I_SLICE ? m_par->cu_split_threshold_cu_intra[depth] : m_par->cu_split_threshold_cu_inter[depth];
    if (costBest >= cuSplitThresholdCu && splitMode != SPLIT_NONE && !(skippedFlag && m_par->cuSplit == 2)) {
        // restore ctx
        if (m_rdOptFlag)
            m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        // split
        CostType costSplit = 0;
        for (Ipp32s i = 0; i < 4; i++) {
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
        } else {
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
        EncAndRecLumaTu(absPartIdx, offset, width, NULL, &costBest, costPredFlag, intraPredOpt);
        if (splitMode == SPLIT_TRY) {
            if (m_rdOptFlag)
                m_bsf->CtxSave(ctxSave[1], tab_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);
            ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth+trDepth], width, roiSize);
        }
    }

    if (costBest >= m_par->cu_split_threshold_tu_intra[depth+trDepth] && splitMode != SPLIT_NONE) {
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

    me_info->details[0] = dx >> 4;
    me_info->details[1] = dy >> 4;

}

Ipp32s H265CU::MvCost( H265MV MV[2], T_RefIdx curRefIdx[2], MVPInfo *pInfo, MVPInfo& mergeInfo) const
{
    if (curRefIdx[0] == -1 || curRefIdx[1] == -1) {
        Ipp32s numRefLists = (m_cslice->slice_type == B_SLICE) ? 2 : 1;
        Ipp32s mincost = 0, minind = 0, minlist = 0;
        CostType lamb = (CostType)(m_rdLambdaInterMv)/2;

        for (Ipp32s rlist = 0; rlist < numRefLists; rlist++) {
            if (curRefIdx[rlist] == -1)
                continue;
            for (Ipp32s i=0; i<mergeInfo.numCand; i++)
                if(curRefIdx[rlist] == mergeInfo.refIdx[2*i+rlist] && mergeInfo.mvCand[2*i+rlist] == MV[rlist])
                    return (Ipp32s)(lamb * (2+i));
        }
        for (Ipp32s rlist = 0; rlist < numRefLists; rlist++) {
            if (curRefIdx[rlist] == -1)
                continue;
            pInfo += 2 * curRefIdx[rlist];
            for (Ipp32s i=0; i<pInfo[rlist].numCand; i++) {
                if(curRefIdx[rlist] == pInfo[rlist].refIdx[i]) {
                    Ipp32s cost = (0+i) + qdist(&MV[rlist], &pInfo[rlist].mvCand[i])*4;
                    if(i==0 || mincost > cost) {
                        minlist = rlist;
                        mincost = cost;
                        minind = i;
                    }

                }
            }
        }
        return (Ipp32s)(lamb * ((2+minind) + qdiff(&MV[minlist], &pInfo[minlist].mvCand[minind])*4));
    } else {
        // B-pred
        T_RefIdx refidx[2][2] = {{curRefIdx[0], -1}, {-1, curRefIdx[1]}};
        return MvCost(MV, refidx[0], pInfo, mergeInfo) + MvCost(MV, refidx[1], pInfo, mergeInfo);
    }
}

// absPartIdx - for minimal TU
Ipp32s H265CU::MatchingMetricPu(PixType *pSrc, H265MEInfo* me_info, H265MV* MV, H265Frame *PicYUVRef) const
{
    Ipp32s cost = 0;
    Ipp32s refOffset = m_ctbPelX + me_info->posx + (MV->mvx >> 2) + (m_ctbPelY + me_info->posy + (MV->mvy >> 2)) * PicYUVRef->pitch_luma;
    PixType *pRec = PicYUVRef->y + refOffset;
    Ipp32s recPitch = PicYUVRef->pitch_luma;
    ALIGN_DECL(32) PixType pred_buf_y[MAX_CU_SIZE*MAX_CU_SIZE];

    if ((MV->mvx | MV->mvy) & 3)
    {
        MeInterpolate(me_info, MV, PicYUVRef->y, PicYUVRef->pitch_luma, pred_buf_y, MAX_CU_SIZE);
        pRec = pred_buf_y;
        recPitch = MAX_CU_SIZE;

        cost = MFX_HEVC_PP::h265_SAD_MxN_special_8u(pSrc, pRec, m_pitchSrc, me_info->width, me_info->height);
    }
    else
    {
        cost = MFX_HEVC_PP::h265_SAD_MxN_general_8u(pRec, recPitch, pSrc, m_pitchSrc, me_info->width, me_info->height);
        //Ipp8u* pSrcAligned = (Ipp8u*)m_src_aligned_block + me_info->posx + me_info->posy * MAX_CU_SIZE;
        //cost = MFX_HEVC_PP::h265_SAD_MxN_special_8u(pRec, pSrcAligned, recPitch, me_info->width, me_info->height);
    }

    return cost;
}

Ipp32s H265CU::MatchingMetricBipredPu(PixType *pSrc, H265MEInfo* meInfo, PixType *yFwd, Ipp32u pitchFwd, PixType *yBwd, Ipp32u pitchBwd, H265MV fullMV[2])
{
    Ipp32s width = meInfo->width;
    Ipp32s height = meInfo->height;
    H265MV MV[2] = {fullMV[0], fullMV[1]};

    Ipp16s* predBufY[2];
    PixType *pY[2] = {yFwd, yBwd};
    Ipp32u pitch[2] = {pitchFwd, pitchBwd};

    for(Ipp8u dir=0; dir<2; dir++) {

        ClipMV(MV[dir]); // store clipped mv in buffer

        Ipp8u idx;
        for (idx = m_interpIdxFirst; idx !=m_interpIdxLast; idx = (idx+1)&(INTERP_BUF_SZ-1)) {
            if(pY[dir] == m_interpBuf[idx].pY && MV[dir] == m_interpBuf[idx].mv) {
                predBufY[dir] = m_interpBuf[idx].predBufY;
                break;
            }
        }

        if (idx == m_interpIdxLast) {
            predBufY[dir] = m_interpBuf[idx].predBufY;
            m_interpBuf[idx].pY = pY[dir];
            m_interpBuf[idx].mv = MV[dir];

            MeInterpolateOld(meInfo, &MV[dir], pY[dir], pitch[dir], predBufY[dir], MAX_CU_SIZE);
            m_interpIdxLast = (m_interpIdxLast+1)&(INTERP_BUF_SZ-1); // added to end
            if (m_interpIdxFirst == m_interpIdxLast) // replaced oldest
                m_interpIdxFirst = (m_interpIdxFirst+1)&(INTERP_BUF_SZ-1);
        }
    }

    Ipp32s cost = 0;
    for (Ipp32s y=0; y<height; y++) {
        for (Ipp32s x=0; x<width; x++) {
            Ipp32s var = predBufY[0][x + y*MAX_CU_SIZE] + predBufY[1][x + y*MAX_CU_SIZE] - 2*pSrc[x + y*m_pitchSrc]; // ok for estimation
            //Ipp32s var = ((predBufY[0][x + y*MAX_CU_SIZE] + predBufY[1][x + y*MAX_CU_SIZE] + 1) >> 1) - pSrc[x + y*m_pitchSrc]; // precise
            cost += abs(var);
            //cost += (var*var >> 2);
        }
    }
    cost = (cost + 1) >> 1;

    return cost;
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
    MePu(&meInfo); // 2Nx2N

    m_interPredPtr = m_interPredBest;
    bestCost = CuCost(absPartIdx, depth, &meInfo, offset, m_par->fastPUDecision);
    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
    bestMeInfo[0] = meInfo;
    m_interPredPtr = m_interPred;

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
            MePu(&bestInfo[i]);
            allthesame = allthesame && SamePrediction(&bestInfo[i], &meInfo);
            //cost += bestInfo[i].cost_inter;
        }
        if (!allthesame) {
            m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
            cost = CuCost(absPartIdx, depth, bestInfo, offset, m_par->fastPUDecision);
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

        MePu(&partsInfo[0]);
        MePu(&partsInfo[1]);
        //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
        if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
            m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
            cost = CuCost(absPartIdx, depth, partsInfo, offset, m_par->fastPUDecision);
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

        MePu(&partsInfo[0]);
        MePu(&partsInfo[1]);
        //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
        if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
            m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
            cost = CuCost(absPartIdx, depth, partsInfo, offset, m_par->fastPUDecision);
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

            MePu(&partsInfo[0]);
            MePu(&partsInfo[1]);
            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo, offset, m_par->fastPUDecision);
                if (bestCost > cost) {
                    bestSplitMode = PART_SIZE_2NxnU;
                    bestCost = cost;
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
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

            MePu(&partsInfo[0]);
            MePu(&partsInfo[1]);
            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo, offset, m_par->fastPUDecision);
                if (bestCost > cost) {
                    bestSplitMode = PART_SIZE_2NxnD;
                    bestCost = cost;
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
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

            MePu(&partsInfo[0]);
            MePu(&partsInfo[1]);
            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo, offset, m_par->fastPUDecision);
                if (bestCost > cost) {
                    bestSplitMode = PART_SIZE_nRx2N;
                    bestCost = cost;
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
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

            MePu(&partsInfo[0]);
            MePu(&partsInfo[1]);
            //cost = partsInfo[0].cost_inter + partsInfo[1].cost_inter;
            if (!SamePrediction(&partsInfo[0], &meInfo) || !SamePrediction(&partsInfo[1], &meInfo)) {
                m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
                cost = CuCost(absPartIdx, depth, partsInfo, offset, m_par->fastPUDecision);
                if (bestCost > cost) {
                    bestSplitMode = PART_SIZE_nLx2N;
                    bestCost = cost;
                    m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(m_yRec + offsetLuma, m_pitchRec, m_recLumaSaveTu[depth], meInfo.width, roiSize);
                    small_memcpy(m_dataTemp2, m_data + absPartIdx, sizeof(H265CUData) * numParts);
                    ippiCopy_8u_C1R(m_interPredPtr[depth] + offsetPred, MAX_CU_SIZE,
                                    m_interPredBest[depth] + offsetPred, MAX_CU_SIZE, roiSize);
                }
            }
        }
    }

    m_interPredPtr = m_interPredBest;

    if (m_par->fastPUDecision)
    {
        m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        small_memcpy(m_data + absPartIdx, m_dataTemp2, sizeof(H265CUData) * numParts);
        return CuCost(absPartIdx, depth, bestMeInfo, offset, 0);
    }
    else
    {
        m_bsf->CtxRestore(ctxSave[1], 0, NUM_CABAC_CONTEXT);
        ippiCopy_8u_C1R(m_recLumaSaveTu[depth], meInfo.width, m_yRec + offsetLuma, m_pitchRec, roiSize);
        small_memcpy(m_data + absPartIdx, m_dataTemp2, sizeof(H265CUData) * numParts);
        return bestCost;
    }
}

void H265CU::TuGetSplitInter(Ipp32u absPartIdx, Ipp32s offset, Ipp8u tr_idx, Ipp8u trIdxMax, Ipp8u *nz, CostType *cost) {
    Ipp8u depth = m_data[absPartIdx].depth;
    Ipp32s numParts = ( m_par->NumPartInCU >> ((depth + tr_idx)<<1) ); // in TU
    Ipp32s width = m_data[absPartIdx].size >>tr_idx;
    Ipp8u nzt = 0;

    H265CUData *data_t = m_dataTemp + ((depth + tr_idx) << m_par->Log2NumPartInCU);
    CostType costBest;
    CABAC_CONTEXT_H265 ctxSave[2][NUM_CABAC_CONTEXT];

    for (Ipp32s i = 0; i < numParts; i++)
        m_data[absPartIdx + i].trIdx = tr_idx;
    m_bsf->CtxSave(ctxSave[0], 0, NUM_CABAC_CONTEXT);
    EncAndRecLumaTu(absPartIdx, offset, width, &nzt, &costBest, 0, INTRA_PRED_CALC);
    if (nz) *nz = nzt;

    if (tr_idx  < trIdxMax && nzt) { // don't try if all zero
        m_bsf->CtxSave(ctxSave[1], 0, NUM_CABAC_CONTEXT);
        m_bsf->CtxRestore(ctxSave[0], 0, NUM_CABAC_CONTEXT);
        // keep not split
        small_memcpy(data_t + absPartIdx, m_data + absPartIdx, sizeof(H265CUData) * numParts);

        CostType cost_temp, cost_split = 0;
        Ipp32s subsize = width /*<< (trIdxMax - tr_idx)*/ >> 1;
        subsize *= subsize;
        numParts >>= 2;
        nzt = 0;
        for (Ipp32s i = 0; i < 4; i++) {
            Ipp8u nz_loc;
            TuGetSplitInter(absPartIdx + numParts * i, offset + subsize * i, tr_idx + 1,
                             trIdxMax, &nz_loc, &cost_temp);
            nzt |= nz_loc;
            cost_split += cost_temp;
        }

        m_bsf->Reset();
        Ipp8u code_dqp;
        PutTransform(m_bsf, offset, 0, absPartIdx, absPartIdx,
                      m_data[absPartIdx].depth + m_data[absPartIdx].trIdx,
                      width, width, m_data[absPartIdx].trIdx, code_dqp, 1);

        cost_split += BIT_COST_INTER(m_bsf->GetNumBits());

        if (costBest > cost_split) {
            costBest = cost_split;
            if (nz) *nz = nzt;
        } else {
            // restore not split
            small_memcpy(m_data + absPartIdx, data_t + absPartIdx, sizeof(H265CUData) * numParts * 4);
            m_bsf->CtxRestore(ctxSave[1], 0, NUM_CABAC_CONTEXT);
        }

    }

    if(cost) *cost = costBest;
}


// Cost for CU
CostType H265CU::CuCost(Ipp32u absPartIdx, Ipp8u depth, const H265MEInfo* bestInfo, Ipp32s offset, Ipp32s fastPuDecision)
{
    Ipp8u bestSplitMode = bestInfo[0].splitMode;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    CostType bestCost = 0;

    Ipp32u xborder = bestInfo[0].posx + bestInfo[0].width;
    Ipp32u yborder = bestInfo[0].posy + bestInfo[0].height;

    Ipp8u tr_depth_min, tr_depth_max;
    Ipp8u tr_depth = (m_par->QuadtreeTUMaxDepthInter == 1) && (bestSplitMode != PART_SIZE_2Nx2N);
    while ((m_par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
    while (tr_depth < (m_par->QuadtreeTUMaxDepthInter - 1 + (m_par->QuadtreeTUMaxDepthInter == 1)) &&
        (m_par->MaxCUSize >> (depth + tr_depth)) > 4 &&
        (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MinSize) &&
        (0 || // biggest TU
        (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MaxSize)))
        tr_depth++;
    tr_depth_min = tr_depth;
    while (tr_depth < (m_par->QuadtreeTUMaxDepthInter - 1 + (m_par->QuadtreeTUMaxDepthInter == 1)) &&
        (m_par->MaxCUSize >> (depth + tr_depth)) > 4 &&
        (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MinSize) &&
        (1 || // smallest TU
        (m_par->Log2MaxCUSize - depth - tr_depth > m_par->QuadtreeTULog2MaxSize)))
        tr_depth++;
    tr_depth_max = tr_depth;

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
        Ipp8u nz[2];
        CABAC_CONTEXT_H265 ctxSave[NUM_CABAC_CONTEXT];
        if (m_isRdoq)
            m_bsf->CtxSave(ctxSave, 0, NUM_CABAC_CONTEXT);

        InterPredCu(absPartIdx, depth, m_interPredPtr[depth], MAX_CU_SIZE, 1);

        for (Ipp32u pos = 0; pos < numParts; ) {
            CostType cost;
            Ipp32u num_tr_parts = ( m_par->NumPartInCU >> ((depth + tr_depth_min)<<1) );

            TuGetSplitInter(absPartIdx + pos, (absPartIdx + pos)*16, tr_depth_min, fastPuDecision ? tr_depth_min : tr_depth_max, nz, &cost);

            bestCost += cost;
            pos += num_tr_parts;
        }

        if (m_isRdoq)
            m_bsf->CtxRestore(ctxSave, 0, NUM_CABAC_CONTEXT);
        EncAndRecLuma(absPartIdx, offset, depth, nz, NULL);

        m_bsf->Reset();
        EncodeCU(m_bsf, absPartIdx, depth, RD_CU_MODES);
        bestCost += BIT_COST_INTER(m_bsf->GetNumBits());
    }

    return bestCost;
}

// Uses simple algorithm for now
void H265CU::MePu(H265MEInfo* meInfo)
{
    //Ipp32u num_parts = ( m_par->NumPartInCU >> ((depth + 0)<<1) );
    //const Ipp16s ME_pattern[][2] = {{0,0},{0,-1},{-1,0},{1,0},{0,1}};
    //const Ipp16s ME_pattern[][2] = {{0,0},{-1,-1},{-1,1},{1,1},{1,-1},{0,-2},{-2,0},{2,0},{0,2}};
    //const Ipp16s ME_pattern[][2] = {{0,0},{0,-1},{-1,0},{1,0},{0,1},{-1,-1},{-1,1},{1,1},{1,-1}};
    //const Ipp16s ME_pattern_sz = sizeof(ME_pattern)/sizeof(ME_pattern[0]);

    // Cyclic pattern to avoid trying already checked positions
    const Ipp16s ME_Cpattern[1+8+3][2] =
        {{0,0},{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},
               {-1,-1},{0,-1},{1,-1}};
    const struct Cp_tab {
        Ipp8u start;
        Ipp8u len;
    } Cpattern_tab[1+8+3] = {{1,8},{7,5},{1,3},{1,5},{3,3},{3,5},{5,3},{5,5},{7,3},   {7,5},{1,3},{1,5}};

    // TODO add use
    //DetailsXY(meInfo);

    EncoderRefPicListStruct *pList[2];
    pList[0] = &(m_cslice->m_pRefPicList[0].m_RefPicListL0);
    pList[1] = &(m_cslice->m_pRefPicList[0].m_RefPicListL1);

    m_data[meInfo->absPartIdx].refIdx[0] = 0;
    m_data[meInfo->absPartIdx].refIdx[1] = 0;
    m_data[meInfo->absPartIdx].size = (Ipp8u)(m_par->MaxCUSize>>meInfo->depth);
    m_data[meInfo->absPartIdx].flags.skippedFlag = 0;
    m_data[meInfo->absPartIdx].interDir = INTER_DIR_PRED_L0;
    if (m_cslice->slice_type == B_SLICE)
        m_data[meInfo->absPartIdx].interDir |= INTER_DIR_PRED_L1;

    MVPInfo pInfo[2*H265_MAXNUMREF];
    MVPInfo mergeInfo;
    Ipp32u numParts = ( m_par->NumPartInCU >> (meInfo->depth<<1) );
    Ipp32s blockIdx = meInfo->absPartIdx &~ (numParts-1);
    Ipp32s partAddr = meInfo->absPartIdx &  (numParts-1);
    Ipp32s curPUidx = (partAddr==0) ? 0 : ((meInfo->splitMode!=PART_SIZE_NxN) ? 1 : (partAddr / (numParts>>2)) ); // TODO remove division

    GetPuMvPredictorInfo(blockIdx, partAddr, meInfo->splitMode, curPUidx, pInfo, mergeInfo);
    PixType *pSrc = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrc;

    H265MV MV_cur = {0, 0};
//    H265MV MV_pred = {0, 0}; // predicted MV, now use zero MV
    T_RefIdx curRefIdx[2] = {-1, -1};
    H265MV MV_best_ref[2][H265_MAXNUMREF];
    Ipp32s cost_best_ref[2][H265_MAXNUMREF];

    meInfo->refIdx[0] = meInfo->refIdx[1] = -1;
    for (Ipp16s ME_dir = 0; ME_dir <= (m_cslice->slice_type == B_SLICE ? 1 : 0); ME_dir++) {
        H265MV MV_last;
        for (T_RefIdx ref_idx = 0; ref_idx < m_cslice->num_ref_idx[ME_dir]; ref_idx++)
        {
            H265MV MV_best;
            Ipp32s cost_temp;
            Ipp32s cost_best = INT_MAX;

            curRefIdx[ME_dir] = ref_idx;
            curRefIdx[1-ME_dir] = -1;

            H265Frame *PicYUVRef = pList[ME_dir]->m_RefPicList[curRefIdx[ME_dir]];
            H265MV MV[2];

            if (ME_dir == 1) {
                Ipp8u found_same = 0;
                for (T_RefIdx ref_idx_0 = 0; ref_idx_0 < m_cslice->num_ref_idx[0]; ref_idx_0++) {
                    if (PicYUVRef == pList[0]->m_RefPicList[ref_idx_0]) {
                        cost_best_ref[ME_dir][ref_idx] = cost_best_ref[0][ref_idx_0];
                        MV_best_ref[ME_dir][ref_idx] = MV_best_ref[0][ref_idx_0];
                        MV_last = MV_best_ref[ME_dir][ref_idx];
                        found_same = 1;
                        break;
                    }
                }
                if (found_same) continue;
            }

            // Choose start point using predictors
            H265MV MVtried[7+1+H265_MAXNUMREF+2];
            Ipp32s i, j, num_tried = 0;

            if (curRefIdx[ME_dir] > 0) {
                MVtried[num_tried++] = MV_last;
                {
                    MVtried[num_tried] = MV_last;

                    Ipp32s scale = GetDistScaleFactor(pList[ME_dir]->m_Tb[curRefIdx[ME_dir]],
                        pList[ME_dir]->m_Tb[curRefIdx[ME_dir]-1]);

                    if (scale == 4096)
                    {
                        MVtried[num_tried] = MV_last;
                    }
                    else
                    {
                        MVtried[num_tried].mvx = (Ipp16s)Saturate(-32768, 32767, (scale * MV_last.mvx + 127 + (scale * MV_last.mvx < 0)) >> 8);
                        MVtried[num_tried].mvy = (Ipp16s)Saturate(-32768, 32767, (scale * MV_last.mvy + 127 + (scale * MV_last.mvy < 0)) >> 8);
                        ClipMV(MVtried[num_tried]);
                    }
                    num_tried++;
                }
            }

            for (i=0; i<mergeInfo.numCand; i++) {
                if(curRefIdx[ME_dir] == mergeInfo.refIdx[2*i+ME_dir]) {
                    MVtried[num_tried++] = mergeInfo.mvCand[2*i+ME_dir];
                    ClipMV( MVtried[num_tried-1]);
                    for(j=0; j<num_tried-1; j++)
                        if (MVtried[j] == MVtried[num_tried-1]) {
                            num_tried --;
                            break;
                        }
                }
            }
            for (i=0; i<pInfo[ref_idx*2+ME_dir].numCand; i++) {
                if(curRefIdx[ME_dir] == pInfo[ref_idx*2+ME_dir].refIdx[i]) {
                    MVtried[num_tried++] = pInfo[ref_idx*2+ME_dir].mvCand[i];
                    ClipMV( MVtried[num_tried-1]);
                    for(j=0; j<num_tried-1; j++)
                        if (MVtried[j] == MVtried[num_tried-1]) {
                            num_tried --;
                            break;
                        }
                }
            }
            // add from top level
            if(meInfo->depth>0 && meInfo->depth > m_depthMin) {
                H265CUData* topdata = m_dataBest + ((meInfo->depth -1) << m_par->Log2NumPartInCU);
                if(curRefIdx[ME_dir] == topdata[meInfo->absPartIdx].refIdx[ME_dir] &&
                    topdata[meInfo->absPartIdx].predMode == MODE_INTER ) { // TODO also check same ref
                        MVtried[num_tried++] = topdata[meInfo->absPartIdx].mv[ME_dir];
                        ClipMV( MVtried[num_tried-1]);
                        for(j=0; j<num_tried-1; j++)
                            if (MVtried[j] ==  MVtried[num_tried-1]) {
                                num_tried --;
                                break;
                            }
                }
            }

            for (i=0; i<num_tried; i++) {
                cost_temp = MatchingMetricPu( pSrc, meInfo, &MVtried[i], PicYUVRef) + i; // approx MvCost
                if (cost_best > cost_temp) {
                    MV_best = MVtried[i];
                    cost_best = cost_temp;
                }
            }
            cost_best = INT_MAX; // to be properly updated
            MV_cur.mvx = (MV_best.mvx + 1) & ~3;
            MV_cur.mvy = (MV_best.mvy + 1) & ~3;
            cost_best = MatchingMetricPu( pSrc, meInfo, &MV_cur, PicYUVRef);
            MV_best = MV_cur;
            Ipp16s ME_step_best = 4;

            Ipp16s ME_step = MIN(meInfo->width, meInfo->height) * 4; // enable ClipMV below if step > MaxCUSize
            ME_step = MAX(ME_step, 16*4); // enable ClipMV below if step > MaxCUSize
            Ipp16s ME_step_max = ME_step;

            //         // trying fullsearch
            //         Ipp32s x, y;
            //         for(y = -16; y < 16; y++)
            //             for(x = -16; x < 16; x++) {
            //                 MV[ME_dir].mvx = x * 8;
            //                 MV[ME_dir].mvy = y * 8;
            //                 ClipMV(MV[ME_dir]);
            //                 cost_temp = MatchingMetricPu( pSrc, meInfo, &MV[ME_dir], PicYUVRef) + //MV_pred.qdiff(MV, m_par->QP);
            //                     MvCost( MV, curRefIdx, pInfo, mergeInfo);
            //                 if (cost_best[ME_dir] > cost_temp) {
            //                     MV_best[ME_dir] = MV[ME_dir];
            //                     cost_best[ME_dir] = cost_temp;
            //                 }
            //             }
            //         ME_step = 4;

{
            // expanding search
            for (ME_step = ME_step_best; ME_step <= ME_step_max; ME_step *= 2) {
                for (Ipp16s ME_pos = 1; ME_pos < 9; ME_pos++) {
                    MV[ME_dir].mvx = MV_cur.mvx + ME_Cpattern[ME_pos][0] * ME_step * 1;
                    MV[ME_dir].mvy = MV_cur.mvy + ME_Cpattern[ME_pos][1] * ME_step * 1;
                    ClipMV(MV[ME_dir]);
                    cost_temp = MatchingMetricPu( pSrc, meInfo, &MV[ME_dir], PicYUVRef) + //MV_pred.qdiff(MV, m_par->QP);
                        MvCost( MV, curRefIdx, pInfo, mergeInfo);
                    if (cost_best > cost_temp) {
                        MV_best = MV[ME_dir];
                        cost_best = cost_temp;
                        ME_step_best = ME_step;
                    }
                }
            }
            ME_step = ME_step_best;
            MV_cur = MV_best;
            // then logarithm from best
}

            // for Cpattern
            Ipp8u start = 0, len = 1;

            do {
                Ipp32s refine = 1;
                Ipp32s best_pos = 0;
                //for (ME_pos = 0; ME_pos < ME_pattern_sz; ME_pos++) {
                //    MV[ME_dir].mvx = MV_cur.mvx + ME_pattern[ME_pos][0] * ME_step * 1;
                //    MV[ME_dir].mvy = MV_cur.mvy + ME_pattern[ME_pos][1] * ME_step * 1;
                //    ////ClipMV(MV[ME_dir]);
                //    cost_temp = MatchingMetricPu( pSrc, meInfo, &MV[ME_dir], PicYUVRef) + //MV_pred.qdiff(MV, m_par->QP);
                //        MvCost( MV, curRefIdx, pInfo, mergeInfo);
                //    if (cost_best[ME_dir] > cost_temp) {
                //        MV_best[ME_dir] = MV[ME_dir];
                //        cost_best[ME_dir] = cost_temp;
                //        refine = 0;
                //    }
                //}

                for (Ipp16s ME_pos = start; ME_pos < start+len; ME_pos++) {
                    MV[ME_dir].mvx = MV_cur.mvx + ME_Cpattern[ME_pos][0] * ME_step * 1;
                    MV[ME_dir].mvy = MV_cur.mvy + ME_Cpattern[ME_pos][1] * ME_step * 1;
                    H265MV MVorig = { MV[ME_dir].mvx, MV[ME_dir].mvy };
                    ClipMV(MV[ME_dir]);
                    cost_temp = MatchingMetricPu( pSrc, meInfo, &MV[ME_dir], PicYUVRef) + //MV_pred.qdiff(MV, m_par->QP);
                        MvCost( MV, curRefIdx, pInfo, mergeInfo);
                    if (cost_best > cost_temp) {
                        MV_best = MV[ME_dir];
                        cost_best = cost_temp;
                        refine = 0;
                        best_pos = ME_pos;
                        if (MV[ME_dir] != MVorig)
                            best_pos = 0;
                    }
                }
                start = Cpattern_tab[best_pos].start;
                len   = Cpattern_tab[best_pos].len;

                if(!refine)
                    MV_cur = MV_best;
                else
                    ME_step >>= 1;
                //ME_step = (ME_step * 181) >> 8;

            } while (ME_step);
            cost_best_ref[ME_dir][ref_idx] = cost_best;
            MV_best_ref[ME_dir][ref_idx] = MV_best;
            MV_last = MV_best;
        }
        Ipp32s cost_best_allref = INT_MAX;
        for (T_RefIdx ref_idx = 0; ref_idx < m_cslice->num_ref_idx[ME_dir]; ref_idx++) {
            if (cost_best_allref > cost_best_ref[ME_dir][ref_idx]) {
                cost_best_allref = cost_best_ref[ME_dir][ref_idx];
                meInfo->costOneDir[ME_dir] = cost_best_ref[ME_dir][ref_idx];
                meInfo->MV[ME_dir] = MV_best_ref[ME_dir][ref_idx];
                meInfo->refIdx[ME_dir] = ref_idx;
            }
        }
    }

    if (m_cslice->slice_type == B_SLICE && (meInfo->width + meInfo->height != 12)) {
        // bidirectional
        H265Frame *PicYUVRefF = pList[0]->m_RefPicList[meInfo->refIdx[0]];
        H265Frame *PicYUVRefB = pList[1]->m_RefPicList[meInfo->refIdx[1]];
//        T_RefIdx curRefIdx[2] = {0, 0};
        if(PicYUVRefF && PicYUVRefB) {
            m_interpIdxFirst = m_interpIdxLast = 0; // for MatchingMetricBipredPu optimization
            meInfo->costBiDir = MatchingMetricBipredPu(pSrc, meInfo, PicYUVRefF->y, PicYUVRefF->pitch_luma, PicYUVRefB->y, PicYUVRefB->pitch_luma, meInfo->MV) +
                MvCost( meInfo->MV, meInfo->refIdx, pInfo, mergeInfo);

            // refine Bidir
            if (IPP_MIN(meInfo->costOneDir[0], meInfo->costOneDir[1]) * 9 > 8 * meInfo->costBiDir) {
                bool changed;
                H265MV MV2_best[2] = {meInfo->MV[0], meInfo->MV[1]};
                Ipp32s cost2_best = meInfo->costBiDir;
                do {
                    Ipp32s i, count;
                    H265MV MV2_cur[2], MV2_tmp[2];
                    changed = false;
                    MV2_cur[0] = MV2_best[0]; MV2_cur[1] = MV2_best[1];
                    count = (PicYUVRefF == PicYUVRefB && MV2_best[0] == MV2_best[1]) ? 2*2 : 2*2*2;
                    for(i = 0; i < count; i++) {
                        MV2_tmp[0] = MV2_cur[0]; MV2_tmp[1] = MV2_cur[1];
                        if (i&2)
                            MV2_tmp[i>>2].mvx += (i&1)?1:-1;
                        else
                            MV2_tmp[i>>2].mvy += (i&1)?1:-1;

                        Ipp32s cost_temp = MatchingMetricBipredPu(pSrc, meInfo, PicYUVRefF->y, PicYUVRefF->pitch_luma, PicYUVRefB->y, PicYUVRefB->pitch_luma, MV2_tmp) +
                            MvCost( MV2_tmp, meInfo->refIdx, pInfo, mergeInfo);
                        if (cost_temp < cost2_best) {
                            MV2_best[0] = MV2_tmp[0]; MV2_best[1] = MV2_tmp[1];
                            cost2_best = cost_temp;
                            changed = true;
                        }
                    }
                } while (changed);
                meInfo->MV[0] = MV2_best[0]; meInfo->MV[1] = MV2_best[1];
                meInfo->costBiDir = cost2_best;
            }


            if (meInfo->costOneDir[0] <= meInfo->costOneDir[1] && meInfo->costOneDir[0] <= meInfo->costBiDir) {
                meInfo->interDir = INTER_DIR_PRED_L0;
                meInfo->costInter = meInfo->costOneDir[0];
                meInfo->refIdx[1] = -1;
            } else if (meInfo->costOneDir[1] < meInfo->costOneDir[0] && meInfo->costOneDir[1] <= meInfo->costBiDir) {
                meInfo->interDir = INTER_DIR_PRED_L1;
                meInfo->costInter = meInfo->costOneDir[1];
                meInfo->refIdx[0] = -1;
            } else {
                meInfo->interDir = INTER_DIR_PRED_L0 + INTER_DIR_PRED_L1;
                meInfo->costInter = meInfo->costBiDir;
            }
        } else {
            meInfo->interDir = INTER_DIR_PRED_L0;
            meInfo->costInter = meInfo->costOneDir[0];
            meInfo->costBiDir = INT_MAX;
        }
    } else {
        meInfo->interDir = INTER_DIR_PRED_L0;
        meInfo->costInter = meInfo->costOneDir[0];
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
static void tuDiff(CoeffsType *residual, Ipp32s pitchDst, 
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

static void tuDiffTransp(CoeffsType *residual, Ipp32s pitchDst, const PixType *src, Ipp32s pitchSrc,
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

static Ipp32s tuSse(const PixType *src, Ipp32s pitchSrc,
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
  Ipp32u s = 0;

  Ipp32s satd = 0;
  if (!((width | height) & 7))
  {
      for (Ipp32s j = 0; j < height; j += 8)
      {
          for (Ipp32s i = 0; i < width; i += 8)
          {
              //s += calc_had8x8(&src[i], &rec[i], m_pitchSrc, m_pitchRec);
              ippiSAT8x8D_8u32s_C1R(&src[i], pitchSrc, &rec[i], pitchRec, &satd);
              s += ((satd + 2) >> 2);
          }
          src += pitchSrc * 8;
          rec += pitchRec * 8;
      }
  }
  else if (!((width | height) & 3))
  {
      for (Ipp32s j = 0; j < height; j += 4)
      {
          for (Ipp32s i = 0; i < width; i += 4)
          {
              //s += calc_had4x4(&src[i], &rec[i], m_pitchSrc, m_pitchRec);
              ippiSATD4x4_8u32s_C1R(&src[i], pitchSrc, &rec[i], pitchRec, &satd);
              s += ((satd+1) >> 1);
          }
          src += pitchSrc * 4;
          rec += pitchRec * 4;
      }
  }
  else
  {
      VM_ASSERT(0);
  }

  return s;
}

static void tuDiffNv12(CoeffsType *residual, Ipp32s pitchDst,  const PixType *src, Ipp32s pitchSrc,
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
            s += abs((CoeffsType)src[i*2] - rec[i]);
        }
        src += pitchSrc;
        rec += pitchRec;
    }
    return s;
}

static Ipp32s tuSseNv12(const PixType *src, Ipp32s pitchSrc,
                        const PixType *rec, Ipp32s pitchRec, Ipp32s size)
{
    Ipp32s i, j;
    Ipp32s s = 0;

    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            s += ((CoeffsType)src[i*2] - rec[i]) * ((CoeffsType)src[i*2] - rec[i]);
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
        if (is_pred_transposed)
            tuDiffTransp(m_residualsY + offset, width, src, m_pitchSrc, pred, pitch_pred, width);
        else
            tuDiff(m_residualsY + offset, width, src, m_pitchSrc, pred, pitch_pred, width);

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
        CostType cost_rec = tuSse(src, m_pitchSrc, rec, m_pitchRec, width);
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

int in_decision = 0;

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

            tuDiffNv12(residuals + offset, width, pSrc, m_pitchSrc, pRec, m_pitchRec, width);
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
        if (cost && !m_rdOptFlag) {
            *cost += tuSseNv12(pSrc, m_pitchSrc, pRec, m_pitchRec, width);
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
            *cost = BIT_COST(m_bsf->GetNumBits());
        else
            *cost = BIT_COST_INTER(m_bsf->GetNumBits());
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
    EncoderRefPicListStruct* refPicList[2];
    Ipp8u isCurrRefLongTerm;
    Ipp32s  currRefTb;
    Ipp32s  i;

    refPicList[0] = &m_cslice->m_pRefPicList->m_RefPicListL0;
    refPicList[1] = &m_cslice->m_pRefPicList->m_RefPicListL1;

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

        currRefTb = refPicList[refPicListIdx]->m_Tb[refIdx];
        refPicListIdx = 1 - refPicListIdx;

        if (p_data[blockZScanIdx].refIdx[refPicListIdx] >= 0)
        {
            Ipp32s neibRefTb = refPicList[refPicListIdx]->m_Tb[p_data[blockZScanIdx].refIdx[refPicListIdx]];

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
        currRefTb = refPicList[refPicListIdx]->m_Tb[refIdx];
        isCurrRefLongTerm = refPicList[refPicListIdx]->m_IsLongTermRef[refIdx];

        for (i = 0; i < 2; i++)
        {
            if (p_data[blockZScanIdx].refIdx[refPicListIdx] >= 0)
            {
                Ipp32s neibRefTb = refPicList[refPicListIdx]->m_Tb[p_data[blockZScanIdx].refIdx[refPicListIdx]];
                Ipp8u isNeibRefLongTerm = refPicList[refPicListIdx]->m_IsLongTermRef[p_data[blockZScanIdx].refIdx[refPicListIdx]];

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

static bool HasEqualMotion(Ipp32s blockZScanIdx,
                           H265CUData* blockLCU,
                           Ipp32s candZScanIdx,
                           H265CUData* candLCU)
{
    Ipp32s i;

    for (i = 0; i < 2; i++)
    {
        if (blockLCU[blockZScanIdx].refIdx[i] != candLCU[candZScanIdx].refIdx[i])
        {
            return false;
        }

        if (blockLCU[blockZScanIdx].refIdx[i] >= 0)
        {
            if ((blockLCU[blockZScanIdx].mv[i].mvx != candLCU[candZScanIdx].mv[i].mvx) ||
                (blockLCU[blockZScanIdx].mv[i].mvy != candLCU[candZScanIdx].mv[i].mvy))
            {
                return false;
            }
        }
    }

    return true;
}

/* ****************************************************************************** *\
FUNCTION: GetMergeCandInfo
DESCRIPTION:
\* ****************************************************************************** */

static void GetMergeCandInfo(MVPInfo* pInfo,
                             bool *abCandIsInter,
                             Ipp32s *puhInterDirNeighbours,
                             Ipp32s blockZScanIdx,
                             H265CUData* blockLCU)
{
    Ipp32s iCount = pInfo->numCand;

    abCandIsInter[iCount] = true;

    pInfo->mvCand[2*iCount] = blockLCU[blockZScanIdx].mv[0];
    pInfo->mvCand[2*iCount+1] = blockLCU[blockZScanIdx].mv[1];

    pInfo->refIdx[2*iCount] = blockLCU[blockZScanIdx].refIdx[0];
    pInfo->refIdx[2*iCount+1] = blockLCU[blockZScanIdx].refIdx[1];

    puhInterDirNeighbours[iCount] = 0;

    if (pInfo->refIdx[2*iCount] >= 0)
        puhInterDirNeighbours[iCount] += 1;

    if (pInfo->refIdx[2*iCount+1] >= 0)
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
        if (pInfo->numCand < 4)  {
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

    if (pInfo->numCand > MRG_MAX_NUM_CANDS-1)
        pInfo->numCand = MRG_MAX_NUM_CANDS-1;

    /* temporal candidate */
    if (m_par->TMVPFlag) {
        Ipp32s frameWidthInSamples = m_par->Width;
        Ipp32s frameHeightInSamples = m_par->Height;
        Ipp32s bottomRightCandRow = topLeftRow + partHeight;
        Ipp32s bottomRightCandColumn = topLeftColumn + partWidth;
        Ipp32s centerCandRow = topLeftRow + (partHeight >> 1);
        Ipp32s centerCandColumn = topLeftColumn + (partWidth >> 1);
        T_RefIdx refIdx = 0;
        H265MV colCandMv;

        if ((((Ipp32s)m_ctbPelX + bottomRightCandColumn * minTUSize) >= frameWidthInSamples) ||
            (((Ipp32s)m_ctbPelY + bottomRightCandRow * minTUSize) >= frameHeightInSamples) ||
             (bottomRightCandRow >= numMinTUInLCU))
        {
            candLCU[0] = NULL;
        }
        else if (bottomRightCandColumn < numMinTUInLCU) {
            candLCU[0] = m_colocatedLcu[0];
            candZScanIdx[0] = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn];
        }
        else {
            candLCU[0] = m_colocatedLcu[1];
            candZScanIdx[0] = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn - numMinTUInLCU];
        }

        candLCU[1] = m_colocatedLcu[0];
        candZScanIdx[1] = h265_scan_r2z[maxDepth][(centerCandRow << maxDepth) + centerCandColumn];

        if ((candLCU[0] != NULL) && GetColMvp(candLCU[0], candZScanIdx[0], 0, refIdx, colCandMv) ||
            GetColMvp(candLCU[1], candZScanIdx[1], 0, refIdx, colCandMv))
        {
            abCandIsInter[pInfo->numCand] = true;

            pInfo->mvCand[2*pInfo->numCand].mvx = colCandMv.mvx;
            pInfo->mvCand[2*pInfo->numCand].mvy = colCandMv.mvy;

            pInfo->refIdx[2*pInfo->numCand] = refIdx;
            puhInterDirNeighbours[pInfo->numCand] = 1;

            if (m_cslice->slice_type == B_SLICE) {
                if (candLCU[0] != NULL && GetColMvp(candLCU[0], candZScanIdx[0], 1, refIdx, colCandMv) ||
                     GetColMvp(candLCU[1], candZScanIdx[1], 1, refIdx, colCandMv) )
                {
                    pInfo->mvCand[2*pInfo->numCand+1].mvx = colCandMv.mvx;
                    pInfo->mvCand[2*pInfo->numCand+1].mvy = colCandMv.mvy;

                    pInfo->refIdx[2*pInfo->numCand+1] = refIdx;
                    puhInterDirNeighbours[pInfo->numCand] += 2;
                }
            }

            pInfo->numCand++;
        }
    }

    /* combined bi-predictive merging candidates */
    if (m_cslice->slice_type == B_SLICE) {
        Ipp32s uiPriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
        Ipp32s uiPriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};
        Ipp32s limit = pInfo->numCand * (pInfo->numCand - 1);

        for (i = 0; i < limit && pInfo->numCand != MRG_MAX_NUM_CANDS; i++) {
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

                if ((m_cslice->m_pRefPicList->m_RefPicListL0.m_Tb[pInfo->refIdx[2*pInfo->numCand]] ==
                     m_cslice->m_pRefPicList->m_RefPicListL1.m_Tb[pInfo->refIdx[2*pInfo->numCand+1]]) &&
                    (pInfo->mvCand[2*pInfo->numCand].mvx == pInfo->mvCand[2*pInfo->numCand+1].mvx) &&
                    (pInfo->mvCand[2*pInfo->numCand].mvy == pInfo->mvCand[2*pInfo->numCand+1].mvy))
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
        T_RefIdx r = 0;
        T_RefIdx refCnt = 0;

        if (m_cslice->slice_type == B_SLICE)
            if (m_cslice->num_ref_idx_l1_active < m_cslice->num_ref_idx_l0_active)
                numRefIdx = m_cslice->num_ref_idx_l1_active;

        while (pInfo->numCand < MRG_MAX_NUM_CANDS) {
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

static bool IsCandFound(T_RefIdx *curRefIdx,
                        H265MV* curMV,
                        MVPInfo* pInfo,
                        Ipp32s candIdx,
                        Ipp32s numRefLists)
{
    Ipp32s i;

    for (i = 0; i < numRefLists; i++)
    {
        if (curRefIdx[i] != pInfo->refIdx[2*candIdx+i])
        {
            return false;
        }

        if (curRefIdx[i] >= 0)
        {
            if ((curMV[i].mvx != pInfo->mvCand[2*candIdx+i].mvx) ||
                (curMV[i].mvy != pInfo->mvCand[2*candIdx+i].mvy))
            {
                return false;
            }
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
    T_RefIdx curRefIdx[2];
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

H265CUData* H265CU::GetNeighbour(Ipp32s& neighbourBlockZScanIdx,
                                 Ipp32s  neighbourBlockColumn,
                                 Ipp32s  neighbourBlockRow,
                                 Ipp32s  curBlockZScanIdx,
                                 bool isNeedTocheckCurLCU)
{
    Ipp32s maxDepth = m_par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
//    Ipp32s maxCUSize = m_par->MaxCUSize;
    Ipp32s minCUSize = m_par->MinCUSize;
    Ipp32s minTUSize = m_par->MinTUSize;
    Ipp32s frameWidthInSamples = m_par->Width;
    Ipp32s frameHeightInSamples = m_par->Height;
//    Ipp32s frameWidthInLCUs = m_par->PicWidthInCtbs;
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
if(0)        if ((minCUSize == 8) && (minTUSize == 4))
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

bool H265CU::GetColMvp(H265CUData* colLCU,
                       Ipp32s blockZScanIdx,
                       Ipp32s refPicListIdx,
                       Ipp32s refIdx,
                       H265MV& rcMv)
{
    EncoderRefPicListStruct* refPicList[2];
    Ipp32s colPicListIdx, colRefIdx;
    Ipp8u isCurrRefLongTerm, isColRefLongTerm;
    Ipp32s  currRefTb, colRefTb;
    H265MV cMvPred;
    Ipp32s minTUSize = m_par->MinTUSize;

    /* MV compression */
    if (m_par->MinTUSize < 2)
    {
        Ipp32s shift = 2 * (2 - minTUSize);
        blockZScanIdx = (blockZScanIdx >> shift) << shift;
    }

    refPicList[0] = &m_cslice->m_pRefPicList->m_RefPicListL0;
    refPicList[1] = &m_cslice->m_pRefPicList->m_RefPicListL1;

    if ((colLCU == NULL) || (colLCU[blockZScanIdx].predMode == MODE_INTRA))
    {
        return false;
    }

    if (m_cslice->slice_type != B_SLICE)
    {
        colPicListIdx = 1;
    }
    else
    {
/*        if (m_SliceInf.IsLowDelay)
        {
            colPicListIdx = refPicListIdx;
        }
        else */if (m_cslice->collocated_from_l0_flag)
        {
            colPicListIdx = 1;
        }
        else
        {
            colPicListIdx = 0;
        }
    }

    colRefIdx = colLCU[blockZScanIdx].refIdx[colPicListIdx];

    if (colRefIdx < 0 )
    {
        colPicListIdx = 1 - colPicListIdx;
        colRefIdx = colLCU[blockZScanIdx].refIdx[colPicListIdx];

        if (colRefIdx < 0 )
        {
            return false;
        }
    }

    // Scale the vector.
    currRefTb = refPicList[refPicListIdx]->m_Tb[refIdx];
    isCurrRefLongTerm = refPicList[refPicListIdx]->m_IsLongTermRef[refIdx];
    colRefTb = m_colFrmRefFramesTbList[colPicListIdx][colRefIdx];
    isColRefLongTerm = m_colFrmRefIsLongTerm[colPicListIdx][colRefIdx];

    cMvPred = colLCU[blockZScanIdx].mv[colPicListIdx];

    if (isCurrRefLongTerm || isColRefLongTerm)
    {
        rcMv = cMvPred;
    }
    else
    {
        Ipp32s scale = GetDistScaleFactor(currRefTb, colRefTb);

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

    return true;
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

    if (pInfo->numCand == 2)
    {
        if ((pInfo->mvCand[0].mvx == pInfo->mvCand[1].mvx) &&
            (pInfo->mvCand[0].mvy == pInfo->mvCand[1].mvy))
        {
            pInfo->numCand = 1;
        }
    }

    if (m_par->TMVPFlag)
    {
        Ipp32s frameWidthInSamples = m_par->Width;
        Ipp32s frameHeightInSamples = m_par->Height;
        Ipp32s topLeftRow = topLeftRasterIdx >> maxDepth;
        Ipp32s topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
        Ipp32s bottomRightCandRow = topLeftRow + partHeight;
        Ipp32s bottomRightCandColumn = topLeftColumn + partWidth;
        Ipp32s minTUSize = m_par->MinTUSize;
        Ipp32s colCandBlockZScanIdx = 0;
        H265CUData* colCandLCU;
        H265MV colCandMv;

        if ((((Ipp32s)m_ctbPelX + bottomRightCandColumn * minTUSize) >= frameWidthInSamples) ||
            (((Ipp32s)m_ctbPelY + bottomRightCandRow * minTUSize) >= frameHeightInSamples) ||
            (bottomRightCandRow >= numMinTUInLCU))
        {
            colCandLCU = NULL;
        }
        else if (bottomRightCandColumn < numMinTUInLCU)
        {
            colCandLCU = m_colocatedLcu[0];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn];
        }
        else
        {
            colCandLCU = m_colocatedLcu[1];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn - numMinTUInLCU];
        }

        if ((colCandLCU != NULL) &&
            GetColMvp(colCandLCU, colCandBlockZScanIdx, refPicListIdx, refIdx, colCandMv))
        {
            pInfo->mvCand[pInfo->numCand] = colCandMv;
            pInfo->numCand++;
        }
        else
        {
            Ipp32s centerCandRow = topLeftRow + (partHeight >> 1);
            Ipp32s centerCandColumn = topLeftColumn + (partWidth >> 1);

            colCandLCU = m_colocatedLcu[0];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(centerCandRow << maxDepth) + centerCandColumn];

            if (GetColMvp(colCandLCU, colCandBlockZScanIdx, refPicListIdx, refIdx, colCandMv))
            {
                pInfo->mvCand[pInfo->numCand] = colCandMv;
                pInfo->numCand++;
            }
        }
    }

    if (pInfo->numCand > 2)
    {
        pInfo->numCand = 2;
    }

    while (pInfo->numCand < 2)
    {
        pInfo->mvCand[pInfo->numCand].mvx = 0;
        pInfo->mvCand[pInfo->numCand].mvy = 0;
        pInfo->numCand++;
    }

    return;
}

static inline Ipp8u isEqualRef(EncoderRefPicListStruct *ref_list, T_RefIdx idx1, T_RefIdx idx2)
{
    if (idx1 < 0 || idx2 < 0) return 0;
    return ref_list->m_RefPicList[idx1] == ref_list->m_RefPicList[idx2];
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
            for (T_RefIdx cur_ref = 0; cur_ref < m_cslice->num_ref_idx[refList]; cur_ref++) {

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
            T_RefIdx *ref_idx = &(mergeInfo.refIdx[2*i]);
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
                H265Frame *PicYUVRefF = m_cslice->GetRefFrame(REF_PIC_LIST_0, ref_idx[0]);
                H265Frame *PicYUVRefB = m_cslice->GetRefFrame(REF_PIC_LIST_1, ref_idx[1]);
                if (PicYUVRefF && PicYUVRefB)
                    cost_temp = MatchingMetricBipredPu(pSrc, &me_info, PicYUVRefF->y, PicYUVRefF->pitch_luma, PicYUVRefB->y, PicYUVRefB->pitch_luma, mv);
                else
                    cost_temp = COST_MAX;
            } else {
                EnumRefPicList dir = inter_dir == INTER_DIR_PRED_L0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
                H265Frame *PicYUVRef = m_cslice->GetRefFrame(dir, ref_idx[dir]);
                cost_temp = MatchingMetricPu(pSrc, &me_info, mv+dir, PicYUVRef); // approx MvCost
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
        cost_best = tuSse(pSrc, m_pitchSrc, pRec, m_pitchRec, size);
        // add chroma with little weight
        InterPredCu(absPartIdx, depth, m_uvRec, m_pitchRec, 0);
        pRec = m_uvRec + (((PUStartRow>>1) * m_pitchRec + PUStartColumn) << m_par->Log2MinTUSize);
        pSrc = m_uvSrc + (((PUStartRow>>1) * m_pitchSrc + PUStartColumn) << m_par->Log2MinTUSize);
        cost_best += tuSse(pSrc, m_pitchSrc, pRec, m_pitchRec, size>>1)/4;
        pRec += size>>1;
        pSrc += size>>1;
        cost_best += tuSse(pSrc, m_pitchSrc, pRec, m_pitchRec, size>>1)/4;
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


} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
