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

Ipp32u H265CU::getCtxSkipFlag(Ipp32u absPartIdx)
{
    H265CUPtr pcTempCU;
    Ipp32u    uiCtx = 0;

    // Get BCBP of left PU
    getPuLeft(&pcTempCU, m_uiAbsIdxInLCU + absPartIdx );
    uiCtx    = ( pcTempCU.ctbData ) ? isSkipped(pcTempCU.ctbData, pcTempCU.absPartIdx ) : 0;

    // Get BCBP of above PU
    getPuAbove(&pcTempCU, m_uiAbsIdxInLCU + absPartIdx );
    uiCtx   += ( pcTempCU.ctbData ) ? isSkipped(pcTempCU.ctbData, pcTempCU.absPartIdx ) : 0;

    return uiCtx;
}

Ipp32u H265CU::getCtxSplitFlag(Ipp32u absPartIdx, Ipp32u depth)
{
    H265CUPtr tempCU;
    Ipp32u    ctx;
    // Get left split flag
    getPuLeft( &tempCU, m_uiAbsIdxInLCU + absPartIdx );
    ctx  = ( tempCU.ctbData ) ? ( ( tempCU.ctbData[tempCU.absPartIdx].depth > depth ) ? 1 : 0 ) : 0;

    // Get above split flag
    getPuAbove(&tempCU, m_uiAbsIdxInLCU + absPartIdx );
    ctx += ( tempCU.ctbData ) ? ( ( tempCU.ctbData[tempCU.absPartIdx].depth > depth ) ? 1 : 0 ) : 0;

    return ctx;
}

Ipp8s H265CU::getRefQp( Ipp32u currAbsIdxInLcu )
{
    Ipp32u        lPartIdx, aPartIdx;
    H265CUData* cULeft  = getQpMinCuLeft ( lPartIdx, m_uiAbsIdxInLCU + currAbsIdxInLcu );
    H265CUData* cUAbove = getQpMinCuAbove( aPartIdx, m_uiAbsIdxInLCU + currAbsIdxInLcu );
    return (((cULeft? cULeft[lPartIdx].qp: getLastCodedQP( currAbsIdxInLcu )) + (cUAbove? cUAbove[aPartIdx].qp: getLastCodedQP( currAbsIdxInLcu )) + 1) >> 1);
}

Ipp32u H265CU::getCoefScanIdx(Ipp32u absPartIdx, Ipp32u width, Ipp32s isLuma, Ipp32s isIntra)
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
        dirMode = data[absPartIdx].intraLumaDir;
        scanIdx = COEFF_SCAN_ZIGZAG;
        if (ctxIdx >3 && ctxIdx < 6) //if multiple scans supported for transform size
        {
            scanIdx = abs((Ipp32s) dirMode - INTRA_VER) < 5 ? 1 : (abs((Ipp32s)dirMode - INTRA_HOR) < 5 ? 2 : 0);
        }
    }
    else {
        dirMode = data[absPartIdx].intraChromaDir;
        if (dirMode == INTRA_DM_CHROMA)
        {
            // get number of partitions in current CU
            Ipp32u depth = data[absPartIdx].depth;
            Ipp32u numParts = par->NumPartInCU >> (2 * depth);

            // get luma mode from upper-left corner of current CU
            dirMode = data[(absPartIdx/numParts)*numParts].intraLumaDir;
        }
        scanIdx = COEFF_SCAN_ZIGZAG;
        if (ctxIdx > 4 && ctxIdx < 7) //if multiple scans supported for transform size
        {
            scanIdx = abs((Ipp32s) dirMode - INTRA_VER) < 5 ? 1 : (abs((Ipp32s)dirMode - INTRA_HOR) < 5 ? 2 : 0);
        }
    }

    return scanIdx;
}

Ipp32u H265CU::getCtxQtCbf(Ipp32u/* absPartIdx*/, EnumTextType type, Ipp32u trDepth)
{
  if (type) {
      return trDepth;
  }
  else {
      return trDepth == 0 ? 1 : 0;
  }
}

Ipp32u H265CU::getQuadtreeTULog2MinSizeInCU(Ipp32u absPartIdx )
{
    return getQuadtreeTULog2MinSizeInCU(absPartIdx, data[absPartIdx].size,
        data[absPartIdx].partSize, data[absPartIdx].predMode);
}

Ipp32u H265CU::getQuadtreeTULog2MinSizeInCU(Ipp32u absPartIdx, Ipp32s size, Ipp8u partSize, Ipp8u pred_mode)
{
    Ipp32u log2CbSize = h265_log2m2[size] + 2;
    Ipp32u quadtreeTUMaxDepth = pred_mode == MODE_INTRA ?
        par->QuadtreeTUMaxDepthIntra: par->QuadtreeTUMaxDepthInter;
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

void H265CU::convertTransIdx( Ipp32u /*absPartIdx*/, Ipp32u trIdx, Ipp32u& rlumaTrMode, Ipp32u& rchromaTrMode )
{
    rlumaTrMode   = trIdx;
    rchromaTrMode = trIdx;
    return;
}

Ipp32u H265CU::getSCUAddr()
{
    return ctb_addr*(1<<(par->MaxCUDepth<<1))+m_uiAbsIdxInLCU;
}

Ipp32s H265CU::getIntradirLumaPred(Ipp32u absPartIdx, Ipp32s* intraDirPred, Ipp32s* mode)
{
    H265CUPtr tempCU;
    Ipp32s leftIntraDir, aboveIntraDir;
    Ipp32s predNum = 0;

    // Get intra direction of left PU
    getPuLeft(&tempCU, m_uiAbsIdxInLCU + absPartIdx );

    leftIntraDir  = tempCU.ctbData ? ( IS_INTRA(tempCU.ctbData, tempCU.absPartIdx ) ? tempCU.ctbData[tempCU.absPartIdx].intraLumaDir : INTRA_DC ) : INTRA_DC;

    // Get intra direction of above PU
    getPuAbove(&tempCU, m_uiAbsIdxInLCU + absPartIdx, true, true, false, true );

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


void H265CU::getAllowedChromaDir(Ipp32u absPartIdx, Ipp8u* modeList)
{
    modeList[0] = INTRA_PLANAR;
    modeList[1] = INTRA_VER;
    modeList[2] = INTRA_HOR;
    modeList[3] = INTRA_DC;
    modeList[4] = INTRA_DM_CHROMA;

    Ipp8u lumaMode = data[absPartIdx].intraLumaDir;

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

void H265CU::getPuAbove(H265CUPtr *cu,
                        Ipp32u currPartUnitIdx,
                        Ipp32s enforceSliceRestriction,
                        Ipp32s /*bEnforceDependentSliceRestriction*/,
                        Ipp32s /*MotionDataCompresssion*/,
                        Ipp32s planarAtLcuBoundary,
                        Ipp32s /*bEnforceTileRestriction*/ )
{
    Ipp32u absPartIdx       = h265_scan_z2r[par->MaxCUDepth][currPartUnitIdx];
    Ipp32u absZorderCuIdx   = h265_scan_z2r[par->MaxCUDepth][m_uiAbsIdxInLCU];
    Ipp32u numPartInCuWidth = par->NumPartInCUSize;

    if ( !isZeroRow( absPartIdx, numPartInCuWidth ) )
    {
        cu->absPartIdx = h265_scan_r2z[par->MaxCUDepth][ absPartIdx - numPartInCuWidth ];
        cu->ctbData = data;
        cu->ctbAddr = ctb_addr;
        if ( !isEqualRow( absPartIdx, absZorderCuIdx, numPartInCuWidth ) )
        {
            cu->absPartIdx -= m_uiAbsIdxInLCU;
        }
        return;
    }

    if(planarAtLcuBoundary)
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->absPartIdx = h265_scan_r2z[par->MaxCUDepth][ absPartIdx + par->NumPartInCU - numPartInCuWidth ];

    if (enforceSliceRestriction && (p_above == NULL || above_addr < cslice->slice_segment_address))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }
    cu->ctbData = p_above;
    cu->ctbAddr = above_addr;
}

void H265CU::getPuLeft(H265CUPtr *cu,
                       Ipp32u currPartUnitIdx,
                       Ipp32s enforceSliceRestriction,
                       Ipp32s /*bEnforceDependentSliceRestriction*/,
                       Ipp32s /*bEnforceTileRestriction*/ )
{
    Ipp32u absPartIdx       = h265_scan_z2r[par->MaxCUDepth][currPartUnitIdx];
    Ipp32u absZorderCUIdx   = h265_scan_z2r[par->MaxCUDepth][m_uiAbsIdxInLCU];
    Ipp32u numPartInCuWidth = par->NumPartInCUSize;

    if ( !isZeroCol( absPartIdx, numPartInCuWidth ) )
    {
        cu->absPartIdx = h265_scan_r2z[par->MaxCUDepth][ absPartIdx - 1 ];
        cu->ctbData = data;
        cu->ctbAddr = ctb_addr;
        if ( !isEqualCol( absPartIdx, absZorderCUIdx, numPartInCuWidth ) )
        {
            cu->absPartIdx -= m_uiAbsIdxInLCU;
        }
        return;
    }

    cu->absPartIdx = h265_scan_r2z[par->MaxCUDepth][ absPartIdx + numPartInCuWidth - 1 ];

    if (enforceSliceRestriction && (p_left == NULL || left_addr < cslice->slice_segment_address))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->ctbData = p_left;
    cu->ctbAddr = left_addr;
}


H265CUData* H265CU::getQpMinCuLeft( Ipp32u& uiLPartUnitIdx, Ipp32u uiCurrAbsIdxInLCU, bool bEnforceSliceRestriction, bool bEnforceDependentSliceRestriction)
{
    // not implemented
    return NULL;
}

H265CUData* H265CU::getQpMinCuAbove( Ipp32u& aPartUnitIdx, Ipp32u currAbsIdxInLCU, bool enforceSliceRestriction, bool enforceDependentSliceRestriction )
{
    // not implemented
    return NULL;
}

Ipp8s H265CU::getLastCodedQP( Ipp32u absPartIdx )
{
    // not implemented
    return 0;
}

void H265CU::InitCu(H265VideoParam *_par, H265CUData *_data, H265CUData *_dataTemp, Ipp32s cuAddr,
                    PixType *_y, PixType *_uv, Ipp32s _pitch,
                    PixType *_ySrc, PixType *_uvSrc, Ipp32s _pitchSrc, H265BsFake *_bsf, H265Slice *_cslice,
                    Ipp32s initializeDataFlag) {
    par = _par;

    cslice = _cslice;
    bsf = _bsf;
    data_save = data = _data;
    data_best = _dataTemp;
    data_temp = _dataTemp + (MAX_TOTAL_DEPTH << par->Log2NumPartInCU);
    data_temp2 = _dataTemp + ((MAX_TOTAL_DEPTH*2) << par->Log2NumPartInCU);
    ctb_addr           = cuAddr;
    ctb_pelx           = ( cuAddr % par->PicWidthInCtbs ) * par->MaxCUSize;
    ctb_pely           = ( cuAddr / par->PicWidthInCtbs ) * par->MaxCUSize;
    m_uiAbsIdxInLCU      = 0;
    num_partition     = par->NumPartInCU;
    pitch_rec = _pitch;
    pitch_src = _pitchSrc;
    y_rec = _y + ctb_pelx + ctb_pely * pitch_rec;
    uv_rec = _uv + ctb_pelx + (ctb_pely * pitch_rec >> 1);

    y_src = _ySrc + ctb_pelx + ctb_pely * pitch_src;

    // aya: may be used to provide performance gain for SAD calculation
    /*{
    IppiSize blkSize = {par->MaxCUSize, par->MaxCUSize};
    ippiCopy_8u_C1R(_y_src + ctb_pelx + ctb_pely * pitch_src, pitch_src, m_src_aligned_block, MAX_CU_SIZE, blkSize);
    }*/

    uv_src = _uvSrc + ctb_pelx + (ctb_pely * pitch_src >> 1);
    depth_min = MAX_TOTAL_DEPTH;

    if (initializeDataFlag) {
        rd_opt_flag = cslice->rd_opt_flag;
        rd_lambda = cslice->rd_lambda;
        rd_lambda_inter = cslice->rd_lambda_inter;
        rd_lambda_inter_mv = cslice->rd_lambda_inter_mv;

        if ( num_partition > 0 )
        {
            memset (data, 0, sizeof(H265CUData));
            data->partSize = PART_SIZE_NONE;
            data->predMode = MODE_NONE;
            data->size = (Ipp8u)par->MaxCUSize;
            data->mvpIdx[0] = -1;
            data->mvpIdx[1] = -1;
            data->mvpNum[0] = -1;
            data->mvpNum[1] = -1;
            data->qp = par->QP;
            data->_flags = 0;
            data->intraLumaDir = INTRA_DC;
            data->intraChromaDir = INTRA_DM_CHROMA;
            data->cbf[0] = data->cbf[1] = data->cbf[2] = 0;

            for (Ipp32u i = 1; i < num_partition; i++)
                small_memcpy(data+i,data,sizeof(H265CUData));
        }
    }

    // Setting neighbor CU
    p_left        = NULL;
    p_above       = NULL;
    p_above_left   = NULL;
    p_above_right  = NULL;
    left_addr = - 1;
    above_addr = -1;
    above_left_addr = -1;
    above_right_addr = -1;

    Ipp32u widthInCU = par->PicWidthInCtbs;
    if ( ctb_addr % widthInCU )
    {
        p_left = data - par->NumPartInCU;
        left_addr = ctb_addr - 1;
    }

    if ( ctb_addr / widthInCU )
    {
        p_above = data - (widthInCU << par->Log2NumPartInCU);
        above_addr = ctb_addr - widthInCU;
    }

    if ( p_left && p_above )
    {
        p_above_left = data - (widthInCU << par->Log2NumPartInCU) - par->NumPartInCU;
        above_left_addr = ctb_addr - widthInCU - 1;
    }

    if ( p_above && ( (ctb_addr%widthInCU) < (widthInCU-1) )  )
    {
        p_above_right = data - (widthInCU << par->Log2NumPartInCU) + par->NumPartInCU;
        above_right_addr = ctb_addr - widthInCU + 1;
    }

    bakAbsPartIdxCu = 0;
    bakAbsPartIdx = 0;
    bakChromaOffset = 0;
    m_isRDOQ = par->RDOQFlag ? true : false;
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
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );
    Ipp32u i;
  Ipp32u lpel_x   = ctb_pelx +
      ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
  Ipp32u rpel_x   = lpel_x + (par->MaxCUSize>>depth)  - 1;
  Ipp32u tpel_y   = ctb_pely +
      ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);
  Ipp32u bpel_y   = tpel_y + (par->MaxCUSize>>depth) - 1;
/*
  if (depth == 0) {
      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE; i++)
          tr_coeff_y[i] = (Ipp16s)((myrand() & 31) - 16);
      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE/4; i++) {
          tr_coeff_u[i] = (Ipp16s)((myrand() & 31) - 16);
          tr_coeff_v[i] = (Ipp16s)((myrand() & 31) - 16);
      }
  }
*/
    if ((depth < par->MaxCUDepth - par->AddCUDepth) &&
        (((myrand() & 1) && (par->Log2MaxCUSize - depth > par->QuadtreeTULog2MinSize)) ||
        rpel_x >= par->Width  || bpel_y >= par->Height ||
        par->Log2MaxCUSize - depth -
            (cslice->slice_type == I_SLICE ? par->QuadtreeTUMaxDepthIntra :
            MIN(par->QuadtreeTUMaxDepthIntra, par->QuadtreeTUMaxDepthInter)) > par->QuadtreeTULog2MaxSize)) {
        // split
        for (i = 0; i < 4; i++)
            FillRandom(absPartIdx + (num_parts >> 2) * i, depth+1);
    } else {
        Ipp8u pred_mode = MODE_INTRA;
        if ( cslice->slice_type != I_SLICE && (myrand() & 15)) {
            pred_mode = MODE_INTER;
        }
        Ipp8u size = (Ipp8u)(par->MaxCUSize >> depth);
        Ipp32u MaxDepth = pred_mode == MODE_INTRA ? par->QuadtreeTUMaxDepthIntra : par->QuadtreeTUMaxDepthInter;
        Ipp8u part_size;

        if (pred_mode == MODE_INTRA) {
            part_size = (Ipp8u)
                (depth == par->MaxCUDepth - par->AddCUDepth &&
                depth + 1 <= par->MaxCUDepth &&
                ((par->Log2MaxCUSize - depth - MaxDepth == par->QuadtreeTULog2MaxSize) ||
                (myrand() & 1)) ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
        } else {
            if (depth == par->MaxCUDepth - par->AddCUDepth) {
                if (size == 8) {
                    part_size = (Ipp8u)(myrand() % 3);
                } else {
                    part_size = (Ipp8u)(myrand() % 4);
                }
            } else {
                if (par->csps->amp_enabled_flag) {
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
        while ((par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
        while (tr_depth < (MaxDepth - 1 + intra_split + inter_split) &&
            (par->MaxCUSize >> (depth + tr_depth)) > 4 &&
            (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MinSize) &&
            ((myrand() & 1) ||
                (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MaxSize))) tr_depth++;

        for (i = 0; i < num_parts; i++) {
            data[absPartIdx + i].cbf[0] = 0;
            data[absPartIdx + i].cbf[1] = 0;
            data[absPartIdx + i].cbf[2] = 0;
        }

        if (tr_depth > par->QuadtreeTUMaxDepthIntra) {
            printf("FillRandom err 1\n"); exit(1);
        }
        if (depth + tr_depth > par->MaxCUDepth) {
            printf("FillRandom err 2\n"); exit(1);
        }
        if (par->Log2MaxCUSize - (depth + tr_depth) < par->QuadtreeTULog2MinSize) {
            printf("FillRandom err 3\n"); exit(1);
        }
        if (par->Log2MaxCUSize - (depth + tr_depth) > par->QuadtreeTULog2MaxSize) {
            printf("FillRandom err 4\n"); exit(1);
        }
        if (pred_mode == MODE_INTRA) {
            Ipp8u luma_dir = (p_left && p_above) ? (Ipp8u) (myrand() % 35) : INTRA_DC;
            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
                data[i].predMode = pred_mode;
                data[i].depth = depth;
                data[i].size = size;
                data[i].partSize = part_size;
                data[i].intraLumaDir = luma_dir;
                data[i].intraChromaDir = INTRA_DM_CHROMA;
                data[i].qp = par->QP;
                data[i].trIdx = tr_depth;
                data[i].interDir = 0;
            }
        } else {
            Ipp16s mvmax = (Ipp16s)(10 + myrand()%100);
            Ipp16s mvx0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvx1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            T_RefIdx ref_idx0 = (T_RefIdx)(myrand() % cslice->num_ref_idx_l0_active);
            Ipp8u inter_dir;
            if (cslice->slice_type == B_SLICE && part_size != PART_SIZE_2Nx2N && size == 8) {
                inter_dir = (Ipp8u)(1 + myrand()%2);
            } else if (cslice->slice_type == B_SLICE) {
                inter_dir = (Ipp8u)(1 + myrand()%3);
            } else {
                inter_dir = 1;
            }
            T_RefIdx ref_idx1 = (T_RefIdx)((inter_dir & INTER_DIR_PRED_L1) ? (myrand() % cslice->num_ref_idx_l1_active) : -1);
            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
                data[i].predMode = pred_mode;
                data[i].depth = depth;
                data[i].size = size;
                data[i].partSize = part_size;
                data[i].intraLumaDir = 0;//luma_dir;
                data[i].qp = par->QP;
                data[i].trIdx = tr_depth;
                data[i].interDir = inter_dir;
                data[i].refIdx[0] = -1;
                data[i].refIdx[1] = -1;
                data[i].mv[0].mvx = 0;
                data[i].mv[0].mvy = 0;
                data[i].mv[1].mvx = 0;
                data[i].mv[1].mvy = 0;
                if (inter_dir & INTER_DIR_PRED_L0) {
                    data[i].refIdx[0] = ref_idx0;
                    data[i].mv[0].mvx = mvx0;
                    data[i].mv[0].mvy = mvy0;
                }
                if (inter_dir & INTER_DIR_PRED_L1) {
                    data[i].refIdx[1] = ref_idx1;
                    data[i].mv[1].mvx = mvx1;
                    data[i].mv[1].mvy = mvy1;
                }
                data[i].flags.mergeFlag = 0;
                data[i].mvd[0].mvx = data[i].mvd[0].mvy =
                    data[i].mvd[1].mvx = data[i].mvd[1].mvy = 0;
                data[i].mvpIdx[0] = data[i].mvpIdx[1] = 0;
                data[i].mvpNum[0] = data[i].mvpNum[1] = 0;
            }
            for (Ipp32s i = 0; i < getNumPartInter(absPartIdx); i++)
            {
                Ipp32s PartAddr;
                Ipp32s PartX, PartY, Width, Height;
                GetPartOffsetAndSize(i, data[absPartIdx].partSize,
                    data[absPartIdx].size, PartX, PartY, Width, Height);
                GetPartAddr(i, data[absPartIdx].partSize, num_parts, PartAddr);

                GetPUMVInfo(absPartIdx, PartAddr, data[absPartIdx].partSize, i);
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
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );
    Ipp32u i;
    Ipp32u lpel_x   = ctb_pelx +
        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
    Ipp32u rpel_x   = lpel_x + (par->MaxCUSize>>depth)  - 1;
    Ipp32u tpel_y   = ctb_pely +
        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);
    Ipp32u bpel_y   = tpel_y + (par->MaxCUSize>>depth) - 1;
/*
  if (depth == 0) {
      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE; i++)
          tr_coeff_y[i] = (Ipp16s)((myrand() & 31) - 16);
      for (i = 0; i < MAX_CU_SIZE * MAX_CU_SIZE/4; i++) {
          tr_coeff_u[i] = (Ipp16s)((myrand() & 31) - 16);
          tr_coeff_v[i] = (Ipp16s)((myrand() & 31) - 16);
      }
  }
*/
    if ((depth < par->MaxCUDepth - par->AddCUDepth) &&
        (((/*myrand() & 1*/0) && (par->Log2MaxCUSize - depth > par->QuadtreeTULog2MinSize)) ||
        rpel_x >= par->Width  || bpel_y >= par->Height ||
        par->Log2MaxCUSize - depth -
            (cslice->slice_type == I_SLICE ? par->QuadtreeTUMaxDepthIntra :
            MIN(par->QuadtreeTUMaxDepthIntra, par->QuadtreeTUMaxDepthInter)) > par->QuadtreeTULog2MaxSize)) {
        // split
        for (i = 0; i < 4; i++)
            FillZero(absPartIdx + (num_parts >> 2) * i, depth+1);
    } else {
        Ipp8u pred_mode = MODE_INTRA;
        if ( cslice->slice_type != I_SLICE && (myrand() & 15)) {
            pred_mode = MODE_INTER;
        }
        Ipp8u size = (Ipp8u)(par->MaxCUSize >> depth);
        Ipp32u MaxDepth = pred_mode == MODE_INTRA ? par->QuadtreeTUMaxDepthIntra : par->QuadtreeTUMaxDepthInter;
        Ipp8u part_size;
        /*
        if (pred_mode == MODE_INTRA) {
            part_size = (Ipp8u)
                (depth == par->MaxCUDepth - par->AddCUDepth &&
                depth + 1 <= par->MaxCUDepth &&
                ((par->Log2MaxCUSize - depth - MaxDepth == par->QuadtreeTULog2MaxSize) ||
                (myrand() & 1)) ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
        } else {
            if (depth == par->MaxCUDepth - par->AddCUDepth) {
                if (size == 8) {
                    part_size = (Ipp8u)(myrand() % 3);
                } else {
                    part_size = (Ipp8u)(myrand() % 4);
                }
            } else {
                if (par->csps->amp_enabled_flag) {
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
        while ((par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
        while (tr_depth < (MaxDepth - 1 + intra_split + inter_split) &&
            (par->MaxCUSize >> (depth + tr_depth)) > 4 &&
            (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MinSize) &&
            ((0/*myrand() & 1*/) ||
                (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MaxSize))) tr_depth++;

        for (i = 0; i < num_parts; i++) {
            data[absPartIdx + i].cbf[0] = 0;
            data[absPartIdx + i].cbf[1] = 0;
            data[absPartIdx + i].cbf[2] = 0;
        }

        if (tr_depth > par->QuadtreeTUMaxDepthIntra) {
            printf("FillZero err 1\n"); exit(1);
        }
        if (depth + tr_depth > par->MaxCUDepth) {
            printf("FillZero err 2\n"); exit(1);
        }
        if (par->Log2MaxCUSize - (depth + tr_depth) < par->QuadtreeTULog2MinSize) {
            printf("FillZero err 3\n"); exit(1);
        }
        if (par->Log2MaxCUSize - (depth + tr_depth) > par->QuadtreeTULog2MaxSize) {
            printf("FillZero err 4\n"); exit(1);
        }
        if (pred_mode == MODE_INTRA) {
            Ipp8u luma_dir =/* (p_left && p_above) ? (Ipp8u) (myrand() % 35) : */INTRA_DC;
            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
                data[i].predMode = pred_mode;
                data[i].depth = depth;
                data[i].size = size;
                data[i].partSize = part_size;
                data[i].intraLumaDir = luma_dir;
                data[i].intraChromaDir = INTRA_DM_CHROMA;
                data[i].qp = par->QP;
                data[i].trIdx = tr_depth;
                data[i].interDir = 0;
            }
        } else {
/*            Ipp16s mvmax = (Ipp16s)(10 + myrand()%100);
            Ipp16s mvx0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvx1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);*/
            T_RefIdx ref_idx0 = 0;//(T_RefIdx)(myrand() % cslice->num_ref_idx_l0_active);
            Ipp8u inter_dir;
/*            if (cslice->slice_type == B_SLICE && part_size != PART_SIZE_2Nx2N && size == 8) {
                inter_dir = (Ipp8u)(1 + myrand()%2);
            } else if (cslice->slice_type == B_SLICE) {
                inter_dir = (Ipp8u)(1 + myrand()%3);
            } else*/ {
                inter_dir = 1;
            }
            T_RefIdx ref_idx1 = (T_RefIdx)((inter_dir & INTER_DIR_PRED_L1) ? (myrand() % cslice->num_ref_idx_l1_active) : -1);
            for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
                data[i].predMode = pred_mode;
                data[i].depth = depth;
                data[i].size = size;
                data[i].partSize = part_size;
                data[i].intraLumaDir = 0;//luma_dir;
                data[i].qp = par->QP;
                data[i].trIdx = tr_depth;
                data[i].interDir = inter_dir;
                data[i].refIdx[0] = -1;
                data[i].refIdx[1] = -1;
                data[i].mv[0].mvx = 0;
                data[i].mv[0].mvy = 0;
                data[i].mv[1].mvx = 0;
                data[i].mv[1].mvy = 0;
                if (inter_dir & INTER_DIR_PRED_L0) {
                    data[i].refIdx[0] = ref_idx0;
//                    data[i].mv[0].mvx = mvx0;
//                    data[i].mv[0].mvy = mvy0;
                }
                if (inter_dir & INTER_DIR_PRED_L1) {
                    data[i].refIdx[1] = ref_idx1;
//                    data[i].mv[1].mvx = mvx1;
//                    data[i].mv[1].mvy = mvy1;
                }
                data[i]._flags = 0;
                data[i].flags.mergeFlag = 0;
                data[i].mvd[0].mvx = data[i].mvd[0].mvy =
                    data[i].mvd[1].mvx = data[i].mvd[1].mvy = 0;
                data[i].mvpIdx[0] = data[i].mvpIdx[1] = 0;
                data[i].mvpNum[0] = data[i].mvpNum[1] = 0;
            }
            for (Ipp32s i = 0; i < getNumPartInter(absPartIdx); i++)
            {
                Ipp32s PartAddr;
                Ipp32s PartX, PartY, Width, Height;
                GetPartOffsetAndSize(i, data[absPartIdx].partSize,
                    data[absPartIdx].size, PartX, PartY, Width, Height);
                GetPartAddr(i, data[absPartIdx].partSize, num_parts, PartAddr);

                GetPUMVInfo(absPartIdx, PartAddr, data[absPartIdx].partSize, i);
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
    Ipp8u size = (Ipp8u)(par->MaxCUSize >> depthCu);
    Ipp32s depth = depthCu + trDepth;
    Ipp32u numParts = ( par->NumPartInCU >> (depth<<1) );
    Ipp32u i;

// uncomment to hide false uninitialized memory read warning
//    memset(data + absPartIdx, 0, num_parts * sizeof(data[0]));
    for (i = absPartIdx; i < absPartIdx + numParts; i++) {
        data[i].depth = depthCu;
        data[i].size = size;
        data[i].partSize = partSize;
        data[i].predMode = MODE_INTRA;
        data[i].qp = qp;

        data[i].trIdx = trDepth;
        data[i].intraLumaDir = lumaDir;
        data[i].intraChromaDir = INTRA_DM_CHROMA;
        data[i].cbf[0] = data[i].cbf[1] = data[i].cbf[2] = 0;
        data[i].flags.skippedFlag = 0;
    }
}

void H265CU::FillSubPartIntraLumaDir(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth, Ipp8u lumaDir)
{
    Ipp32u numParts = (par->NumPartInCU >> ((depthCu + trDepth) << 1));
    for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
        data[i].intraLumaDir = lumaDir;
    }
}

void H265CU::CopySubPartTo(H265CUData *data_copy, Ipp32s absPartIdx, Ipp8u depth_cu, Ipp8u tr_depth)
{
    Ipp32s depth = depth_cu + tr_depth;
    Ipp32u numParts = ( par->NumPartInCU >> (depth<<1) );
    small_memcpy(data_copy + absPartIdx, data + absPartIdx, numParts * sizeof(H265CUData));
}

void H265CU::ModeDecision(Ipp32u absPartIdx, Ipp32u offset, Ipp8u depth, CostType *cost)
{
    CABAC_CONTEXT_H265 ctx_save[4][NUM_CABAC_CONTEXT];
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );
    Ipp32u lpel_x   = ctb_pelx +
        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
    Ipp32u rpel_x   = lpel_x + (par->MaxCUSize>>depth)  - 1;
    Ipp32u tpel_y   = ctb_pely +
        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);
    Ipp32u bpel_y   = tpel_y + (par->MaxCUSize>>depth) - 1;

    CostType cost_best = COST_MAX;

    Ipp32s subsize = par->MaxCUSize >> (depth + 1);
    subsize *= subsize;
    Ipp32s width_cu = par->MaxCUSize >> depth;
    IppiSize roiSize_cu = {width_cu, width_cu};
    Ipp32s offset_luma_cu = GetLumaOffset(par, absPartIdx, pitch_rec);

    Ipp8u split_mode = SPLIT_NONE;

    if (depth < par->MaxCUDepth - par->AddCUDepth) {
        if (rpel_x >= par->Width  || bpel_y >= par->Height ||
            par->Log2MaxCUSize - depth - (par->QuadtreeTUMaxDepthIntra - 1) > par->QuadtreeTULog2MaxSize) {
                split_mode = SPLIT_MUST;
        } else if (par->Log2MaxCUSize - depth > par->QuadtreeTULog2MinSize) {
            split_mode = SPLIT_TRY;
        }
    }

    if (rd_opt_flag) {
        bsf->CtxSave(ctx_save[0], 0, NUM_CABAC_CONTEXT);
        bsf->CtxSave(ctx_save[2], 0, NUM_CABAC_CONTEXT);
    }

    if (split_mode != SPLIT_MUST) {
        if (depth_min == MAX_TOTAL_DEPTH)
            depth_min = depth;

        data = data_temp + (depth << par->Log2NumPartInCU);

        bool tryIntra = true;
        if (tryIntra)
        {
            // Try Intra mode
            Ipp32s intra_split = (depth == par->MaxCUDepth - par->AddCUDepth &&
                depth + 1 <= par->MaxCUDepth) ? 1 : 0;
            for (Ipp8u tr_depth = 0; tr_depth < intra_split + 1; tr_depth++) {
                Ipp8u part_size = (Ipp8u)(tr_depth == 1 ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
                CostType cost_best_pu[4] = { COST_MAX, COST_MAX, COST_MAX, COST_MAX };
                CostType cost_best_pu_sum = 0;

                if (rd_opt_flag && tr_depth)
                    bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);

                for (Ipp32s i = 0; i < ((tr_depth == 1) ? 4 : 1); i++) {
                    Ipp32s absPartIdx_tu = absPartIdx + (num_parts >> 2) * i;
                    Ipp32u lpel_x_tu = ctb_pelx +
                        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx_tu] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
                    Ipp32u tpel_y_tu = ctb_pely +
                        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx_tu] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);

                    if (lpel_x_tu >= par->Width || tpel_y_tu >= par->Height) {
                        data = data_best + ((depth + tr_depth) << par->Log2NumPartInCU);
                        FillSubPart(absPartIdx_tu, depth, tr_depth, part_size, 0, par->QP);
                        CopySubPartTo(data_save, absPartIdx_tu, depth, tr_depth);
                        continue;
                    }

                    if (rd_opt_flag)
                        bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);

                    IntraLumaModeDecision(absPartIdx_tu, offset + subsize * i, depth, tr_depth);
                    IntraLumaModeDecisionRDO(absPartIdx_tu, offset + subsize * i, depth, tr_depth, ctx_save[1]);
                    cost_best_pu[i] = intra_best_costs[0];

                    data = data_best + ((depth + tr_depth) << par->Log2NumPartInCU);
                    CopySubPartTo(data_save, absPartIdx_tu, depth, tr_depth);

                    cost_best_pu_sum += cost_best_pu[i];
                }

                // add cost of split cu and pred mode to cost_best_pu_sum
                if (rd_opt_flag) {
                    bsf->Reset();
                    data = data_save;
                    xEncodeCU(bsf, absPartIdx, depth, RD_CU_MODES);
                    cost_best_pu_sum += BIT_COST(bsf->GetNumBits());
                }

                if (cost_best > cost_best_pu_sum) {
                    bsf->CtxSave(ctx_save[2], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(y_rec + offset_luma_cu, pitch_rec, rec_luma_save_cu[depth], width_cu, roiSize_cu);
                    cost_best = cost_best_pu_sum;
                    if (tr_depth == 1) {
                        small_memcpy(data_best + ((depth + 0) << par->Log2NumPartInCU) + absPartIdx,
                            data_best + ((depth + 1) << par->Log2NumPartInCU) + absPartIdx,
                            sizeof(H265CUData) * num_parts);
                    }
                } else if (tr_depth == 1) {
                    data = data_best + ((depth + 0) << par->Log2NumPartInCU);
                    bsf->CtxRestore(ctx_save[2], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(rec_luma_save_cu[depth], width_cu, y_rec + offset_luma_cu, pitch_rec, roiSize_cu);
                }
            }

            Ipp8u chroma_dir_best = INTRA_DM_CHROMA;
            data = data_best + (depth << par->Log2NumPartInCU);

            if (cslice->slice_type == I_SLICE && (par->AnalyseFlags & HEVC_ANALYSE_CHROMA)) {
                Ipp8u allowed_chroma_dir[NUM_CHROMA_MODE];
                CopySubPartTo(data_save, absPartIdx, depth, 0);
                data = data_save;
                getAllowedChromaDir(absPartIdx, allowed_chroma_dir);
                H265CUData *data_b = data_best + (depth << par->Log2NumPartInCU);

                CostType cost_chroma_best = COST_MAX, cost_temp;

                for (Ipp8u chroma_dir = 0; chroma_dir < NUM_CHROMA_MODE; chroma_dir++)
                {
                    if (allowed_chroma_dir[chroma_dir] == 34) continue;
                    if (rd_opt_flag && chroma_dir)
                        bsf->CtxRestore(ctx_save[2], 0, NUM_CABAC_CONTEXT);
                    for(Ipp32u i = absPartIdx; i < absPartIdx + num_parts; i++)
                        data_b[i].intraChromaDir = data[i].intraChromaDir =
                            allowed_chroma_dir[chroma_dir];
                    EncAndRecChroma(absPartIdx, offset >> 2, data[absPartIdx].depth, NULL, &cost_temp);
                    if (cost_chroma_best >= cost_temp) {
                        cost_chroma_best = cost_temp;
                        chroma_dir_best = allowed_chroma_dir[chroma_dir];
                    }
                }
                data = data_save;
                for(Ipp32u i = absPartIdx; i < absPartIdx + num_parts; i++)
                    data_b[i].intraChromaDir = data[i].intraChromaDir = chroma_dir_best;
                if (rd_opt_flag)
                    bsf->CtxRestore(ctx_save[2], 0, NUM_CABAC_CONTEXT);
                EncAndRecChroma(absPartIdx, offset >> 2, data[absPartIdx].depth, NULL, &cost_temp);
                if (rd_opt_flag)
                    bsf->CtxSave(ctx_save[2], 0, NUM_CABAC_CONTEXT);
            }
        }

        // Try Skip mode
        if (cslice->slice_type != I_SLICE) {
            CostType cost_skip;
            data = data_save;

            if (rd_opt_flag)
                bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);

            cost_skip = CalcCostSkip(absPartIdx, depth);
            if (rd_opt_flag) {
                bsf->Reset();
                data = data_save;
                xEncodeCU(bsf, absPartIdx, depth, RD_CU_MODES);
                cost_skip += BIT_COST_INTER(bsf->GetNumBits());
            }

            if (cost_best > cost_skip) {
                small_memcpy(data_best + ((depth + 0) << par->Log2NumPartInCU) + absPartIdx,
                    data_save + absPartIdx,
                    num_parts * sizeof(H265CUData));
                cost_best = cost_skip;
                ippiCopy_8u_C1R(y_rec + offset_luma_cu, pitch_rec, rec_luma_save_cu[depth], width_cu, roiSize_cu);
                if (rd_opt_flag)
                    bsf->CtxSave(ctx_save[2], 0, NUM_CABAC_CONTEXT);
            } else {
                data = data_best + ((depth + 0) << par->Log2NumPartInCU);
                bsf->CtxRestore(ctx_save[2], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(rec_luma_save_cu[depth], width_cu, y_rec + offset_luma_cu, pitch_rec, roiSize_cu);
            }
        }

        // Try inter mode
        if (cslice->slice_type != I_SLICE) {
            CostType cost_inter;
            data = data_save;

            if (rd_opt_flag)
                bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);

            cost_inter = ME_CU(absPartIdx, depth, offset) * 1.;

            if (cost_best > cost_inter) {
                small_memcpy(data_best + ((depth + 0) << par->Log2NumPartInCU) + absPartIdx,
                    data_save + absPartIdx,
                    num_parts * sizeof(H265CUData));
                cost_best = cost_inter;

               ippiCopy_8u_C1R(y_rec + offset_luma_cu, pitch_rec, rec_luma_save_cu[depth], width_cu, roiSize_cu);

                if (rd_opt_flag)
                    bsf->CtxSave(ctx_save[2], 0, NUM_CABAC_CONTEXT);
            } else {
                data = data_best + ((depth + 0) << par->Log2NumPartInCU);
                bsf->CtxRestore(ctx_save[2], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(rec_luma_save_cu[depth], width_cu, y_rec + offset_luma_cu, pitch_rec, roiSize_cu);
            }
        }

        if (split_mode == SPLIT_TRY) {
            ippiCopy_8u_C1R(y_rec + offset_luma_cu, pitch_rec, rec_luma_save_cu[depth], width_cu, roiSize_cu);
        }
    }

    Ipp8u skipped_flag = 0;
    if (split_mode != SPLIT_MUST)
        skipped_flag = (data_best + ((depth + 0) << par->Log2NumPartInCU))[absPartIdx].flags.skippedFlag;

    CostType cu_split_threshold_cu = cslice->slice_type == I_SLICE ? par->cu_split_threshold_cu_intra[depth] : par->cu_split_threshold_cu_inter[depth];
    if (cost_best >= cu_split_threshold_cu && split_mode != SPLIT_NONE && !(skipped_flag && par->cuSplit == 2)) {
        // restore ctx
        if (rd_opt_flag)
            bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
        // split
        CostType cost_split = 0;
        for (Ipp32s i = 0; i < 4; i++) {
            CostType cost_temp;
            ModeDecision(absPartIdx + (num_parts >> 2) * i, offset + subsize * i, depth+1, &cost_temp);
            cost_split += cost_temp;
        }
        if (rd_opt_flag && split_mode != SPLIT_MUST) {
            bsf->Reset();
            data = data_best + ((depth + 1) << par->Log2NumPartInCU);
            CopySubPartTo(data_save, absPartIdx, depth, 0);
            data = data_save;
            xEncodeCU(bsf, absPartIdx, depth, RD_CU_SPLITFLAG);
            if(cslice->slice_type == I_SLICE)
                cost_split += BIT_COST(bsf->GetNumBits());
            else
                cost_split += BIT_COST_INTER(bsf->GetNumBits());
        }

        // add cost of cu split flag to cost_split
        if (cost_best > cost_split) {
            cost_best = cost_split;
            small_memcpy(data_best + (depth << par->Log2NumPartInCU) + absPartIdx,
                data_best + ((depth + 1) << par->Log2NumPartInCU) + absPartIdx,
                sizeof(H265CUData) * num_parts);
        } else {
            ippiCopy_8u_C1R(rec_luma_save_cu[depth], width_cu, y_rec + offset_luma_cu, pitch_rec, roiSize_cu);
            bsf->CtxRestore(ctx_save[2], 0, NUM_CABAC_CONTEXT);
        }
    }

    small_memcpy(data_save + absPartIdx,
        data_best + ((depth) << par->Log2NumPartInCU) + absPartIdx,
        sizeof(H265CUData) * num_parts);

    if (cost)
        *cost = cost_best;

    if (depth == 0) {
        small_memcpy(data_save, data_best, sizeof(H265CUData) << par->Log2NumPartInCU);
        data = data_save;
        for (Ipp32u i = absPartIdx; i < absPartIdx + num_parts; i++) {
            data[i].cbf[0] = data[i].cbf[1] = data[i].cbf[2] = 0;
        }
    }
}

void H265CU::CalcCostLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth,
                          Ipp8u tr_depth, CostOpt cost_opt,
                          Ipp8u part_size, Ipp8u luma_dir, CostType *cost)
{
    CABAC_CONTEXT_H265 ctx_save[2][NUM_CABAC_CONTEXT];
    H265CUData *data_t;
    data_t = data_temp + ((depth + tr_depth) << par->Log2NumPartInCU);

    Ipp32u num_parts = ( par->NumPartInCU >> ((depth + tr_depth)<<1) );
    Ipp32s width = par->MaxCUSize >> (depth + tr_depth);
    CostType cost_split, cost_best = COST_MAX, cost_temp;
    Ipp32s subsize = width >> 1;
    subsize *= subsize;
    Ipp8u split_mode = SPLIT_NONE;
    IppiSize roiSize = {width, width};
    Ipp32s offset_luma = GetLumaOffset(par, absPartIdx, pitch_rec);

    Ipp8u tr_depth_zero_only = cost_opt == COST_PRED_TR_0 || cost_opt == COST_REC_TR_0;
    Ipp8u cost_pred_flag = cost_opt == COST_PRED_TR_0;
    IntraPredOpt intra_pred_opt;

    split_mode = GetTRSplitMode(absPartIdx, depth, tr_depth, part_size, 1);

    if (tr_depth_zero_only && split_mode != SPLIT_MUST) {
        split_mode = SPLIT_NONE;
    } else if (cost_opt == COST_REC_TR_MAX) {
        if (/*pred_mode == MODE_INTRA && */
            (width == 32 || (width < 32 && tr_depth == 0))) {
            data = data_t;
            IntraPredTU(absPartIdx, width, luma_dir, 1);
        }
        if (split_mode != SPLIT_NONE)
            split_mode = SPLIT_MUST;
    }

    if (cost_opt == COST_REC_TR_MAX) {
        intra_pred_opt = INTRA_PRED_IN_REC;
    } else if (pred_intra_all_width == width) {
        intra_pred_opt = INTRA_PRED_IN_BUF;
    } else {
        intra_pred_opt = INTRA_PRED_CALC;
    }

    // save ctx

    if (rd_opt_flag)
        bsf->CtxSave(ctx_save[0], h265_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);

    if (split_mode != SPLIT_MUST) {
        data = data_t;
        FillSubPart(absPartIdx, depth, tr_depth, part_size, luma_dir, par->QP);
        if (rd_opt_flag) {
        // cp data subpart to main
            CopySubPartTo(data_save, absPartIdx, depth, tr_depth);
            data = data_save;
        }
        EncAndRecLumaTU(absPartIdx, offset, width, NULL, &cost_best, cost_pred_flag, intra_pred_opt);
        if (split_mode == SPLIT_TRY) {
            if (rd_opt_flag)
                bsf->CtxSave(ctx_save[1], h265_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);
            ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth+tr_depth], width, roiSize);
        }
    }

    if (cost_best >= par->cu_split_threshold_tu_intra[depth+tr_depth] && split_mode != SPLIT_NONE) {
        // restore ctx
        if (rd_opt_flag)
            bsf->CtxRestore(ctx_save[0], h265_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);

        cost_split = 0;
        if (rd_opt_flag && split_mode != SPLIT_MUST) {
            bsf->Reset();
            Ipp32s tr_size = h265_log2m2[par->MaxCUSize] + 2 - depth - tr_depth;
            bsf->EncodeSingleBin_CABAC(CTX(bsf,TRANS_SUBDIV_FLAG_HEVC) + 5 - tr_size, 1);
            cost_split += BIT_COST(bsf->GetNumBits());
        }
        for (Ipp32u i = 0; i < 4; i++) {
            CalcCostLuma(absPartIdx + (num_parts >> 2) * i, offset + subsize * i,
                depth, tr_depth + 1, cost_opt, part_size, luma_dir, &cost_temp);
            cost_split += cost_temp;
        }
        data = data_save;

        if (cost_best > cost_split) {
            cost_best = cost_split;
            small_memcpy(data_t + absPartIdx, data_t + ((size_t)1 << (size_t)(par->Log2NumPartInCU)) + absPartIdx,
                sizeof(H265CUData) * num_parts);
        } else {
    // restore ctx
    // cp data subpart to main
            if (rd_opt_flag)
                bsf->CtxRestore(ctx_save[1], h265_ctxIdxOffset[QT_CBF_HEVC], NUM_CTX_TU);
            ippiCopy_8u_C1R(rec_luma_save_tu[depth+tr_depth], width, y_rec + offset_luma, pitch_rec, roiSize);
        }
    }

    *cost = cost_best;
}


// absPartIdx - for minimal TU
void H265CU::DetailsXY(H265MEInfo* me_info) const
{
    Ipp32s width = me_info->width;
    Ipp32s height = me_info->height;

    Ipp32s ctbOffset = me_info->posx + me_info->posy * pitch_src;

    Ipp32s dx = 0, dy = 0;
    Ipp32s x, y;

    PixType *pSrc = y_src + ctbOffset;
    for(y=0; y<height; y++) {
        for(x=0; x<width-1; x++)
            dx += (pSrc[x]-pSrc[x+1]) * (pSrc[x]-pSrc[x+1]);
        pSrc += pitch_src;
    }

    pSrc = y_src + ctbOffset;
    for(y=0; y<height-1; y++) {
        for(x=0; x<width; x++)
            dy += (pSrc[x]-pSrc[x+pitch_src]) * (pSrc[x]-pSrc[x+pitch_src]);
        pSrc += pitch_src;
    }

    me_info->details[0] = dx >> 4;
    me_info->details[1] = dy >> 4;

}

Ipp32s H265CU::MVCost( H265MV MV[2], T_RefIdx curRefIdx[2], MVPInfo *pInfo, MVPInfo& mergeInfo) const
{
    CostType mincost = 0, cost;
    Ipp32s i, rlist;
    Ipp32s numRefLists = (cslice->slice_type == B_SLICE) ? 2 : 1;
    CostType lamb = (CostType)(rd_lambda_inter_mv);

    if (curRefIdx[0] == -1 || curRefIdx[1] == -1) {
        for (rlist = 0; rlist < numRefLists; rlist++) {
            if (curRefIdx[rlist] == -1)
                continue;
            for (i=0; i<mergeInfo.numCand; i++)
                if(curRefIdx[rlist] == mergeInfo.refIdx[2*i+rlist] && mergeInfo.mvCand[2*i+rlist] == MV[rlist])
                    return (Ipp32s)(lamb * (2+i)/2);
        }
        for (rlist = 0; rlist < numRefLists; rlist++) {
            if (curRefIdx[rlist] == -1)
                continue;
            pInfo += 2 * curRefIdx[rlist];
            for (i=0; i<pInfo[rlist].numCand; i++) {
                if(curRefIdx[rlist] == pInfo[rlist].refIdx[i]) {
                    cost = lamb * (2+i)/2 +  lamb * qdiff(&MV[rlist], &pInfo[rlist].mvCand[i])*2;
                    if(i==0 || mincost > cost)
                        mincost = cost;
                }
            }
        }
        return (Ipp32s)mincost;
    } else {
        // B-pred
        T_RefIdx refidx[2][2] = {{curRefIdx[0], -1}, {-1, curRefIdx[1]}};
        return MVCost(MV, refidx[0], pInfo, mergeInfo) + MVCost(MV, refidx[1], pInfo, mergeInfo);
    }
}

// absPartIdx - for minimal TU
Ipp32s H265CU::MatchingMetric_PU(PixType *pSrc, H265MEInfo* me_info, H265MV* MV, H265Frame *PicYUVRef) const
{
    Ipp32s cost = 0;
    Ipp32s refOffset = ctb_pelx + me_info->posx + (MV->mvx >> 2) + (ctb_pely + me_info->posy + (MV->mvy >> 2)) * PicYUVRef->pitch_luma;
    PixType *pRec = PicYUVRef->y + refOffset;
    Ipp32s recPitch = PicYUVRef->pitch_luma;
    ALIGN_DECL(32) PixType pred_buf_y[MAX_CU_SIZE*MAX_CU_SIZE];

    if ((MV->mvx | MV->mvy) & 3)
    {
        ME_Interpolate(me_info, MV, PicYUVRef->y, PicYUVRef->pitch_luma, pred_buf_y, MAX_CU_SIZE);
        pRec = pred_buf_y;
        recPitch = MAX_CU_SIZE;

        cost = MFX_HEVC_PP::h265_SAD_MxN_special_8u(pSrc, pRec, pitch_src, me_info->width, me_info->height);
    }
    else
    {
        cost = MFX_HEVC_PP::h265_SAD_MxN_general_8u(pRec, recPitch, pSrc, pitch_src, me_info->width, me_info->height);
        //Ipp8u* pSrcAligned = (Ipp8u*)m_src_aligned_block + me_info->posx + me_info->posy * MAX_CU_SIZE;
        //cost = MFX_HEVC_PP::h265_SAD_MxN_special_8u(pRec, pSrcAligned, recPitch, me_info->width, me_info->height);
    }

    return cost;
}

Ipp32s H265CU::MatchingMetricBipred_PU(PixType *pSrc, H265MEInfo* me_info, PixType *y_fwd, Ipp32u pitch_fwd, PixType *y_bwd, Ipp32u pitch_bwd, H265MV fullMV[2])
{
    Ipp32s cost;
    Ipp32s width = me_info->width;
    Ipp32s height = me_info->height;
    Ipp32s x, y;
    H265MV MV[2] = {fullMV[0], fullMV[1]};

    Ipp16s* pred_buf_y[2];
    PixType *pY[2] = {y_fwd, y_bwd};
    Ipp32u pitch[2] = {pitch_fwd, pitch_bwd};
    Ipp8u dir, idx;
    for(dir=0; dir<2; dir++) {

        clipMV(MV[dir]); // store clipped mv in buffer

        for (idx = interp_idx_first; idx !=interp_idx_last; idx = (idx+1)&(INTERP_BUF_SZ-1)) {
            if(pY[dir] == interp_buf[idx].pY && MV[dir] == interp_buf[idx].MV) {
                pred_buf_y[dir] = interp_buf[idx].pred_buf_y;
                break;
            }
        }

        if (idx == interp_idx_last) {
            pred_buf_y[dir] = interp_buf[idx].pred_buf_y;
            interp_buf[idx].pY = pY[dir];
            interp_buf[idx].MV = MV[dir];

            ME_Interpolate_old(me_info, &MV[dir], pY[dir], pitch[dir], pred_buf_y[dir], MAX_CU_SIZE);
            interp_idx_last = (interp_idx_last+1)&(INTERP_BUF_SZ-1); // added to end
            if (interp_idx_first == interp_idx_last) // replaced oldest
                interp_idx_first = (interp_idx_first+1)&(INTERP_BUF_SZ-1);
        }
    }


    cost = 0;
    for (y=0; y<height; y++) {
        for (x=0; x<width; x++) {
            Ipp32s var = pred_buf_y[0][x + y*MAX_CU_SIZE] + pred_buf_y[1][x + y*MAX_CU_SIZE] - 2*pSrc[x + y*pitch_src]; // ok for estimation
            //Ipp32s var = ((pred_buf_y[0][x + y*MAX_CU_SIZE] + pred_buf_y[1][x + y*MAX_CU_SIZE] + 1) >> 1) - pSrc[x + y*pitch_src]; // precise
            cost += abs(var);
            //cost += (var*var >> 2);
        }
    }
    cost = (cost + 1) >> 1;

    return cost;
}

static bool SamePrediction(const H265MEInfo* a, const H265MEInfo* b)
{
    if (a->inter_dir != b->inter_dir ||
        (a->inter_dir | INTER_DIR_PRED_L0) && (a->MV[0] != b->MV[0] || a->ref_idx[0] != b->ref_idx[0]) ||
        (a->inter_dir | INTER_DIR_PRED_L1) && (a->MV[1] != b->MV[1] || a->ref_idx[1] != b->ref_idx[1]) )
        return false;
    else
        return true;
}

// Tries different PU modes for CU
CostType H265CU::ME_CU(Ipp32u absPartIdx, Ipp8u depth, Ipp32s offset)
{
    Ipp32u i;
    H265MEInfo me_info, best_me_info[4];
    me_info.absPartIdx = absPartIdx;
    me_info.depth = depth;
    me_info.width  = (Ipp8u)(par->MaxCUSize>>depth);
    me_info.height = (Ipp8u)(par->MaxCUSize>>depth);
    me_info.posx = (Ipp8u)((h265_scan_z2r[par->MaxCUDepth][absPartIdx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
    me_info.posy = (Ipp8u)((h265_scan_z2r[par->MaxCUDepth][absPartIdx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);
    IppiSize roiSize = {me_info.width, me_info.width};
    Ipp32s offset_luma = GetLumaOffset(par, absPartIdx, pitch_rec);
    Ipp32s offset_pred = me_info.posx + me_info.posy * MAX_CU_SIZE;

    CABAC_CONTEXT_H265 ctx_save[2][NUM_CABAC_CONTEXT];
    bsf->CtxSave(ctx_save[0], 0, NUM_CABAC_CONTEXT);

    H265MEInfo best_info[4];
    Ipp8u best_split_mode = PART_SIZE_2Nx2N;
    CostType best_cost;
    //Ipp32u xborder = me_info.width;
    //Ipp32u yborder = me_info.height;

    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );
    CostType cost = 0;

    me_info.split_mode = PART_SIZE_2Nx2N;
    ME_PU(&me_info); // 2Nx2N

    inter_pred_ptr = inter_pred_best;
    best_cost = CU_cost(absPartIdx, depth, &me_info, offset, par->fastPUDecision);
    bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
    ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth], me_info.width, roiSize);
    small_memcpy(data_temp2, data + absPartIdx, sizeof(H265CUData) * num_parts);
    best_me_info[0] = me_info;
    inter_pred_ptr = inter_pred;

    // NxN prediction
    if (depth == par->MaxCUDepth - par->AddCUDepth && me_info.width > 8)
    {
        bool allthesame = true;
        for (i=0; i<4; i++) {
            best_info[i] = me_info;
            best_info[i].width  = me_info.width / 2;
            best_info[i].height = me_info.height / 2;
            best_info[i].posx  = (Ipp8u)(me_info.posx + best_info[i].width * (i & 1));
            best_info[i].posy  = (Ipp8u)(me_info.posy + best_info[i].height * ((i>>1) & 1));
            best_info[i].absPartIdx = me_info.absPartIdx + (num_parts >> 2)*i;
            best_info[i].split_mode = PART_SIZE_NxN;
            ME_PU(&best_info[i]);
            allthesame = allthesame && SamePrediction(&best_info[i], &me_info);
            //cost += best_info[i].cost_inter;
        }
        if (!allthesame) {
            bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
            cost = CU_cost(absPartIdx, depth, best_info, offset, par->fastPUDecision);
            if (best_cost > cost) {
                best_me_info[0] = best_info[0];
                best_me_info[1] = best_info[1];
                best_me_info[2] = best_info[2];
                best_me_info[3] = best_info[3];
                best_split_mode = PART_SIZE_NxN;
                best_cost = cost;
                bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth], me_info.width, roiSize);
                small_memcpy(data_temp2, data + absPartIdx, sizeof(H265CUData) * num_parts);
                ippiCopy_8u_C1R(inter_pred_ptr[depth] + offset_pred, MAX_CU_SIZE,
                                inter_pred_best[depth] + offset_pred, MAX_CU_SIZE, roiSize);
            }
        }
    }

    // Nx2N
    {
        H265MEInfo parts_info[2] = {me_info, me_info};
        parts_info[0].width = me_info.width / 2;
        parts_info[1].absPartIdx = me_info.absPartIdx + (num_parts >> 2)*1;
        parts_info[1].width = me_info.width / 2;
        parts_info[1].posx  = me_info.posx + parts_info[0].width;
        parts_info[0].split_mode = PART_SIZE_Nx2N;
        parts_info[1].split_mode = PART_SIZE_Nx2N;

        ME_PU(&parts_info[0]);
        ME_PU(&parts_info[1]);
        //cost = parts_info[0].cost_inter + parts_info[1].cost_inter;
        if (!SamePrediction(&parts_info[0], &me_info) || !SamePrediction(&parts_info[1], &me_info)) {
            bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
            cost = CU_cost(absPartIdx, depth, parts_info, offset, par->fastPUDecision);
            if (best_cost > cost) {
                best_me_info[0] = parts_info[0];
                best_me_info[1] = parts_info[1];
                best_split_mode = PART_SIZE_Nx2N;
                best_cost = cost;
                bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth], me_info.width, roiSize);
                small_memcpy(data_temp2, data + absPartIdx, sizeof(H265CUData) * num_parts);
                ippiCopy_8u_C1R(inter_pred_ptr[depth] + offset_pred, MAX_CU_SIZE,
                                inter_pred_best[depth] + offset_pred, MAX_CU_SIZE, roiSize);
            }
        }
    }

    // 2NxN
    {
        H265MEInfo parts_info[2] = {me_info, me_info};
        parts_info[0].height = me_info.height / 2;
        parts_info[1].absPartIdx = me_info.absPartIdx + (num_parts >> 2)*2;
        parts_info[1].height = me_info.height / 2;
        parts_info[1].posy  = me_info.posy + parts_info[0].height;
        parts_info[0].split_mode = PART_SIZE_2NxN;
        parts_info[1].split_mode = PART_SIZE_2NxN;

        ME_PU(&parts_info[0]);
        ME_PU(&parts_info[1]);
        //cost = parts_info[0].cost_inter + parts_info[1].cost_inter;
        if (!SamePrediction(&parts_info[0], &me_info) || !SamePrediction(&parts_info[1], &me_info)) {
            bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
            cost = CU_cost(absPartIdx, depth, parts_info, offset, par->fastPUDecision);
            if (best_cost > cost) {
                best_me_info[0] = parts_info[0];
                best_me_info[1] = parts_info[1];
                best_split_mode = PART_SIZE_2NxN;
                best_cost = cost;
                bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
                ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth], me_info.width, roiSize);
                small_memcpy(data_temp2, data + absPartIdx, sizeof(H265CUData) * num_parts);
                ippiCopy_8u_C1R(inter_pred_ptr[depth] + offset_pred, MAX_CU_SIZE,
                                inter_pred_best[depth] + offset_pred, MAX_CU_SIZE, roiSize);
            }
        }
    }

    // advanced prediction modes
    //if (par->AMPFlag && me_info.width > 8)
    if (par->AMPAcc[me_info.depth])
    {
        // PART_SIZE_2NxnU
        {
            H265MEInfo parts_info[2] = {me_info, me_info};
            parts_info[0].height = me_info.height / 4;
            parts_info[1].absPartIdx = me_info.absPartIdx + (num_parts >> 3);
            parts_info[1].height = me_info.height * 3 / 4;
            parts_info[1].posy  = me_info.posy + parts_info[0].height;
            parts_info[0].split_mode = PART_SIZE_2NxnU;
            parts_info[1].split_mode = PART_SIZE_2NxnU;

            ME_PU(&parts_info[0]);
            ME_PU(&parts_info[1]);
            //cost = parts_info[0].cost_inter + parts_info[1].cost_inter;
            if (!SamePrediction(&parts_info[0], &me_info) || !SamePrediction(&parts_info[1], &me_info)) {
                bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
                cost = CU_cost(absPartIdx, depth, parts_info, offset, par->fastPUDecision);
                if (best_cost > cost) {
                    best_split_mode = PART_SIZE_2NxnU;
                    best_cost = cost;
                    bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth], me_info.width, roiSize);
                    small_memcpy(data_temp2, data + absPartIdx, sizeof(H265CUData) * num_parts);
                    ippiCopy_8u_C1R(inter_pred_ptr[depth] + offset_pred, MAX_CU_SIZE,
                                    inter_pred_best[depth] + offset_pred, MAX_CU_SIZE, roiSize);
                }
            }
        }
        // PART_SIZE_2NxnD
        {
            H265MEInfo parts_info[2] = {me_info, me_info};
            parts_info[0].height = me_info.height * 3 / 4;
            parts_info[1].absPartIdx = me_info.absPartIdx + (num_parts >> 3) * 5;
            parts_info[1].height = me_info.height / 4;
            parts_info[1].posy  = me_info.posy + parts_info[0].height;
            parts_info[0].split_mode = PART_SIZE_2NxnD;
            parts_info[1].split_mode = PART_SIZE_2NxnD;

            ME_PU(&parts_info[0]);
            ME_PU(&parts_info[1]);
            //cost = parts_info[0].cost_inter + parts_info[1].cost_inter;
            if (!SamePrediction(&parts_info[0], &me_info) || !SamePrediction(&parts_info[1], &me_info)) {
                bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
                cost = CU_cost(absPartIdx, depth, parts_info, offset, par->fastPUDecision);
                if (best_cost > cost) {
                    best_split_mode = PART_SIZE_2NxnD;
                    best_cost = cost;
                    bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth], me_info.width, roiSize);
                    small_memcpy(data_temp2, data + absPartIdx, sizeof(H265CUData) * num_parts);
                    ippiCopy_8u_C1R(inter_pred_ptr[depth] + offset_pred, MAX_CU_SIZE,
                                    inter_pred_best[depth] + offset_pred, MAX_CU_SIZE, roiSize);
                }
            }
        }
        // PART_SIZE_nRx2N
        {
            H265MEInfo parts_info[2] = {me_info, me_info};
            parts_info[0].width = me_info.width * 3 / 4;
            parts_info[1].absPartIdx = me_info.absPartIdx + (num_parts >> 4) * 5;
            parts_info[1].width = me_info.width / 4;
            parts_info[1].posx  = me_info.posx + parts_info[0].width;
            parts_info[0].split_mode = PART_SIZE_nRx2N;
            parts_info[1].split_mode = PART_SIZE_nRx2N;

            ME_PU(&parts_info[0]);
            ME_PU(&parts_info[1]);
            //cost = parts_info[0].cost_inter + parts_info[1].cost_inter;
            if (!SamePrediction(&parts_info[0], &me_info) || !SamePrediction(&parts_info[1], &me_info)) {
                bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
                cost = CU_cost(absPartIdx, depth, parts_info, offset, par->fastPUDecision);
                if (best_cost > cost) {
                    best_split_mode = PART_SIZE_nRx2N;
                    best_cost = cost;
                    bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth], me_info.width, roiSize);
                    small_memcpy(data_temp2, data + absPartIdx, sizeof(H265CUData) * num_parts);
                    ippiCopy_8u_C1R(inter_pred_ptr[depth] + offset_pred, MAX_CU_SIZE,
                                    inter_pred_best[depth] + offset_pred, MAX_CU_SIZE, roiSize);
                }
            }
        }
        // PART_SIZE_nLx2N
        {
            H265MEInfo parts_info[2] = {me_info, me_info};
            parts_info[0].width = me_info.width / 4;
            parts_info[1].absPartIdx = me_info.absPartIdx + (num_parts >> 4);
            parts_info[1].width = me_info.width * 3 / 4;
            parts_info[1].posx  = me_info.posx + parts_info[0].width;
            parts_info[0].split_mode = PART_SIZE_nLx2N;
            parts_info[1].split_mode = PART_SIZE_nLx2N;

            ME_PU(&parts_info[0]);
            ME_PU(&parts_info[1]);
            //cost = parts_info[0].cost_inter + parts_info[1].cost_inter;
            if (!SamePrediction(&parts_info[0], &me_info) || !SamePrediction(&parts_info[1], &me_info)) {
                bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
                cost = CU_cost(absPartIdx, depth, parts_info, offset, par->fastPUDecision);
                if (best_cost > cost) {
                    best_split_mode = PART_SIZE_nLx2N;
                    best_cost = cost;
                    bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
                    ippiCopy_8u_C1R(y_rec + offset_luma, pitch_rec, rec_luma_save_tu[depth], me_info.width, roiSize);
                    small_memcpy(data_temp2, data + absPartIdx, sizeof(H265CUData) * num_parts);
                    ippiCopy_8u_C1R(inter_pred_ptr[depth] + offset_pred, MAX_CU_SIZE,
                                    inter_pred_best[depth] + offset_pred, MAX_CU_SIZE, roiSize);
                }
            }
        }
    }

    inter_pred_ptr = inter_pred_best;

    if (par->fastPUDecision)
    {
        bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
        small_memcpy(data + absPartIdx, data_temp2, sizeof(H265CUData) * num_parts);
        return CU_cost(absPartIdx, depth, best_me_info, offset, 0);
    }
    else
    {
        bsf->CtxRestore(ctx_save[1], 0, NUM_CABAC_CONTEXT);
        ippiCopy_8u_C1R(rec_luma_save_tu[depth], me_info.width, y_rec + offset_luma, pitch_rec, roiSize);
        small_memcpy(data + absPartIdx, data_temp2, sizeof(H265CUData) * num_parts);
        return best_cost;
    }
}

void H265CU::TU_GetSplitInter(Ipp32u absPartIdx, Ipp32s offset, Ipp8u tr_idx, Ipp8u tr_idx_max, Ipp8u *nz, CostType *cost) {
    Ipp8u depth = data[absPartIdx].depth;
    Ipp32s num_parts = ( par->NumPartInCU >> ((depth + tr_idx)<<1) ); // in TU
    Ipp32s width = data[absPartIdx].size >>tr_idx;
    Ipp8u nzt = 0;

    H265CUData *data_t = data_temp + ((depth + tr_idx) << par->Log2NumPartInCU);
    CostType cost_best;
    CABAC_CONTEXT_H265 ctx_save[2][NUM_CABAC_CONTEXT];

    for (Ipp32s i = 0; i < num_parts; i++)
        data[absPartIdx + i].trIdx = tr_idx;
    bsf->CtxSave(ctx_save[0], 0, NUM_CABAC_CONTEXT);
    EncAndRecLumaTU(absPartIdx, offset, width, &nzt, &cost_best, 0, INTRA_PRED_CALC);
    if (nz) *nz = nzt;

    if (tr_idx  < tr_idx_max && nzt) { // don't try if all zero
        bsf->CtxSave(ctx_save[1], 0, NUM_CABAC_CONTEXT);
        bsf->CtxRestore(ctx_save[0], 0, NUM_CABAC_CONTEXT);
        // keep not split
        small_memcpy(data_t + absPartIdx, data + absPartIdx, sizeof(H265CUData) * num_parts);

        CostType cost_temp, cost_split = 0;
        Ipp32s subsize = width /*<< (tr_idx_max - tr_idx)*/ >> 1;
        subsize *= subsize;
        num_parts >>= 2;
        nzt = 0;
        for (Ipp32s i = 0; i < 4; i++) {
            Ipp8u nz_loc;
            TU_GetSplitInter(absPartIdx + num_parts * i, offset + subsize * i, tr_idx + 1,
                             tr_idx_max, &nz_loc, &cost_temp);
            nzt |= nz_loc;
            cost_split += cost_temp;
        }

        bsf->Reset();
        Ipp8u code_dqp;
        put_transform(bsf, offset, 0, absPartIdx, absPartIdx,
                      data[absPartIdx].depth + data[absPartIdx].trIdx,
                      width, width, data[absPartIdx].trIdx, code_dqp, 1);

        cost_split += BIT_COST_INTER(bsf->GetNumBits());

        if (cost_best > cost_split) {
            cost_best = cost_split;
            if (nz) *nz = nzt;
        } else {
            // restore not split
            small_memcpy(data + absPartIdx, data_t + absPartIdx, sizeof(H265CUData) * num_parts * 4);
            bsf->CtxRestore(ctx_save[1], 0, NUM_CABAC_CONTEXT);
        }

    }

    if(cost) *cost = cost_best;
}


// Cost for CU
CostType H265CU::CU_cost(Ipp32u absPartIdx, Ipp8u depth, const H265MEInfo* best_info, Ipp32s offset, Ipp32s fastPUDecision)
{
    Ipp32u i;
    Ipp8u best_split_mode = best_info[0].split_mode;
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );
    CostType cost, best_cost = 0;


    Ipp32u xborder = best_info[0].posx + best_info[0].width;
    Ipp32u yborder = best_info[0].posy + best_info[0].height;

    Ipp8u tr_depth_min, tr_depth_max;
    Ipp8u tr_depth = (par->QuadtreeTUMaxDepthInter == 1) && (best_split_mode != PART_SIZE_2Nx2N);
    while ((par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
    while (tr_depth < (par->QuadtreeTUMaxDepthInter - 1 + (par->QuadtreeTUMaxDepthInter == 1)) &&
        (par->MaxCUSize >> (depth + tr_depth)) > 4 &&
        (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MinSize) &&
        (0 || // biggest TU
        (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MaxSize)))
        tr_depth++;
    tr_depth_min = tr_depth;
    while (tr_depth < (par->QuadtreeTUMaxDepthInter - 1 + (par->QuadtreeTUMaxDepthInter == 1)) &&
        (par->MaxCUSize >> (depth + tr_depth)) > 4 &&
        (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MinSize) &&
        (1 || // smallest TU
        (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MaxSize)))
        tr_depth++;
    tr_depth_max = tr_depth;


    memset(&data[absPartIdx], 0, num_parts * sizeof(H265CUData));
    for (i = absPartIdx; i < absPartIdx + num_parts; i++) {
        Ipp32u posx = (h265_scan_z2r[par->MaxCUDepth][i] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize;
        Ipp32u posy = (h265_scan_z2r[par->MaxCUDepth][i] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize;
        Ipp32s partNxN = (posx<xborder ? 0 : 1) + (posy<yborder ? 0 : 2);
        Ipp32s part = (best_split_mode != PART_SIZE_NxN) ? (partNxN ? 1 : 0) : partNxN;
        const H265MEInfo* mei = &best_info[part];

        VM_ASSERT(mei->inter_dir >= 1 && mei->inter_dir <= 3);
        data[i].predMode = MODE_INTER;
        data[i].depth = depth;
        data[i].size = (Ipp8u)(par->MaxCUSize>>depth);
        data[i].partSize = best_split_mode;
        data[i].qp = par->QP;
        data[i].flags.skippedFlag = 0;
        //data[i].tr_idx = tr_depth; // not decided thoroughly here
        data[i].interDir = mei->inter_dir;
        data[i].refIdx[0] = -1;
        data[i].refIdx[1] = -1;
        if (data[i].interDir & INTER_DIR_PRED_L0) {
            data[i].refIdx[0] = mei->ref_idx[0];
            data[i].mv[0] = mei->MV[0];
        }
        if (data[i].interDir & INTER_DIR_PRED_L1) {
            data[i].refIdx[1] = mei->ref_idx[1];
            data[i].mv[1] = mei->MV[1];
        }
    }
    // set mv
    for (i = 0; i < getNumPartInter(absPartIdx); i++)
    {
        Ipp32s PartAddr;
        Ipp32s PartX, PartY, Width, Height;
        GetPartOffsetAndSize(i, data[absPartIdx].partSize,
            data[absPartIdx].size, PartX, PartY, Width, Height);
        GetPartAddr(i, data[absPartIdx].partSize, num_parts, PartAddr);
        GetPUMVInfo(absPartIdx, PartAddr, data[absPartIdx].partSize, i);
    }

    if (rd_opt_flag) {
        Ipp8u nz[2];
        Ipp32u pos;
        CABAC_CONTEXT_H265 ctx_save[NUM_CABAC_CONTEXT];
        if (IsRDOQ())
            bsf->CtxSave(ctx_save, 0, NUM_CABAC_CONTEXT);

        InterPredCU(absPartIdx, depth, inter_pred_ptr[depth], MAX_CU_SIZE, 1);

        for (pos = 0; pos < num_parts; ) {
            Ipp32u num_tr_parts = ( par->NumPartInCU >> ((depth + tr_depth_min)<<1) );

            TU_GetSplitInter(absPartIdx + pos, (absPartIdx + pos)*16, tr_depth_min, fastPUDecision ? tr_depth_min : tr_depth_max, nz, &cost);

            best_cost += cost;
            pos += num_tr_parts;
        }

        if (IsRDOQ())
            bsf->CtxRestore(ctx_save, 0, NUM_CABAC_CONTEXT);
        EncAndRecLuma(absPartIdx, offset, depth, nz, NULL);

        bsf->Reset();
        xEncodeCU(bsf, absPartIdx, depth, RD_CU_MODES);
        best_cost += BIT_COST_INTER(bsf->GetNumBits());
    }

    return best_cost;
}

// Uses simple algorithm for now
void H265CU::ME_PU(H265MEInfo* me_info)
{
    //Ipp32u num_parts = ( par->NumPartInCU >> ((depth + 0)<<1) );
    //const Ipp16s ME_pattern[][2] = {{0,0},{0,-1},{-1,0},{1,0},{0,1}};
    //const Ipp16s ME_pattern[][2] = {{0,0},{-1,-1},{-1,1},{1,1},{1,-1},{0,-2},{-2,0},{2,0},{0,2}};
    //const Ipp16s ME_pattern[][2] = {{0,0},{0,-1},{-1,0},{1,0},{0,1},{-1,-1},{-1,1},{1,1},{1,-1}};
    //const Ipp16s ME_pattern_sz = sizeof(ME_pattern)/sizeof(ME_pattern[0]);
    Ipp16s ME_step, ME_step_max, ME_step_best, ME_pos, ME_dir;

    // Cyclic pattern to avoid trying already checked positions
    const Ipp16s ME_Cpattern[1+8+3][2] =
        {{0,0},{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},
               {-1,-1},{0,-1},{1,-1}};
    const struct Cp_tab {
        Ipp8u start;
        Ipp8u len;
    } Cpattern_tab[1+8+3] = {{1,8},{7,5},{1,3},{1,5},{3,3},{3,5},{5,3},{5,5},{7,3},   {7,5},{1,3},{1,5}};

    // TODO add use
    //DetailsXY(me_info);

    EncoderRefPicListStruct *pList[2];
    pList[0] = &(cslice->m_pRefPicList[0].m_RefPicListL0);
    pList[1] = &(cslice->m_pRefPicList[0].m_RefPicListL1);

    data[me_info->absPartIdx].refIdx[0] = 0;
    data[me_info->absPartIdx].refIdx[1] = 0;
    data[me_info->absPartIdx].size = (Ipp8u)(par->MaxCUSize>>me_info->depth);
    data[me_info->absPartIdx].flags.skippedFlag = 0;
    data[me_info->absPartIdx].interDir = INTER_DIR_PRED_L0;
    if (cslice->slice_type == B_SLICE)
        data[me_info->absPartIdx].interDir |= INTER_DIR_PRED_L1;

    MVPInfo pInfo[2*H265_MAXNUMREF];
    MVPInfo mergeInfo;
    Ipp32u num_parts = ( par->NumPartInCU >> (me_info->depth<<1) );
    Ipp32s blockIdx = me_info->absPartIdx &~ (num_parts-1);
    Ipp32s partAddr = me_info->absPartIdx &  (num_parts-1);
    Ipp32s curPUidx = (partAddr==0) ? 0 : ((me_info->split_mode!=PART_SIZE_NxN) ? 1 : (partAddr / (num_parts>>2)) ); // TODO remove division

    GetPUMVPredictorInfo(blockIdx, partAddr, me_info->split_mode, curPUidx, pInfo, mergeInfo);
    PixType *pSrc = y_src + me_info->posx + me_info->posy * pitch_src;

    H265MV MV_cur = {0, 0};
//    H265MV MV_pred = {0, 0}; // predicted MV, now use zero MV
    T_RefIdx curRefIdx[2] = {-1, -1};
    H265MV MV_best_ref[2][H265_MAXNUMREF];
    Ipp32s cost_best_ref[2][H265_MAXNUMREF];

    me_info->ref_idx[0] = me_info->ref_idx[1] = -1;
    for (ME_dir = 0; ME_dir <= (cslice->slice_type == B_SLICE ? 1 : 0); ME_dir++) {
        H265MV MV_last;
        for (T_RefIdx ref_idx = 0; ref_idx < cslice->num_ref_idx[ME_dir]; ref_idx++)
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
                for (T_RefIdx ref_idx_0 = 0; ref_idx_0 < cslice->num_ref_idx[0]; ref_idx_0++) {
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
                        clipMV(MVtried[num_tried]);
                    }
                    num_tried++;
                }
            }

            for (i=0; i<mergeInfo.numCand; i++) {
                if(curRefIdx[ME_dir] == mergeInfo.refIdx[2*i+ME_dir]) {
                    MVtried[num_tried++] = mergeInfo.mvCand[2*i+ME_dir];
                    clipMV( MVtried[num_tried-1]);
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
                    clipMV( MVtried[num_tried-1]);
                    for(j=0; j<num_tried-1; j++)
                        if (MVtried[j] == MVtried[num_tried-1]) {
                            num_tried --;
                            break;
                        }
                }
            }
            // add from top level
            if(me_info->depth>0 && me_info->depth > depth_min) {
                H265CUData* topdata = data_best + ((me_info->depth -1) << par->Log2NumPartInCU);
                if(curRefIdx[ME_dir] == topdata[me_info->absPartIdx].refIdx[ME_dir] &&
                    topdata[me_info->absPartIdx].predMode == MODE_INTER ) { // TODO also check same ref
                        MVtried[num_tried++] = topdata[me_info->absPartIdx].mv[ME_dir];
                        clipMV( MVtried[num_tried-1]);
                        for(j=0; j<num_tried-1; j++)
                            if (MVtried[j] ==  MVtried[num_tried-1]) {
                                num_tried --;
                                break;
                            }
                }
            }

            for (i=0; i<num_tried; i++) {
                cost_temp = MatchingMetric_PU( pSrc, me_info, &MVtried[i], PicYUVRef) + i; // approx MVcost
                if (cost_best > cost_temp) {
                    MV_best = MVtried[i];
                    cost_best = cost_temp;
                }
            }
            cost_best = INT_MAX; // to be properly updated
            MV_cur.mvx = (MV_best.mvx + 1) & ~3;
            MV_cur.mvy = (MV_best.mvy + 1) & ~3;
            cost_best = MatchingMetric_PU( pSrc, me_info, &MV_cur, PicYUVRef);
            MV_best = MV_cur;
            ME_step_best = 4;

            ME_step = MIN(me_info->width, me_info->height) * 4; // enable clipMV below if step > MaxCUSize
            ME_step = MAX(ME_step, 16*4); // enable clipMV below if step > MaxCUSize
            ME_step_max = ME_step;

            //         // trying fullsearch
            //         Ipp32s x, y;
            //         for(y = -16; y < 16; y++)
            //             for(x = -16; x < 16; x++) {
            //                 MV[ME_dir].mvx = x * 8;
            //                 MV[ME_dir].mvy = y * 8;
            //                 clipMV(MV[ME_dir]);
            //                 cost_temp = MatchingMetric_PU( pSrc, me_info, &MV[ME_dir], PicYUVRef) + //MV_pred.qdiff(MV, par->QP);
            //                     MVCost( MV, curRefIdx, pInfo, mergeInfo);
            //                 if (cost_best[ME_dir] > cost_temp) {
            //                     MV_best[ME_dir] = MV[ME_dir];
            //                     cost_best[ME_dir] = cost_temp;
            //                 }
            //             }
            //         ME_step = 4;

{
            // expanding search
            for (ME_step = ME_step_best; ME_step <= ME_step_max; ME_step *= 2) {
                for (ME_pos = 1; ME_pos < 9; ME_pos++) {
                    MV[ME_dir].mvx = MV_cur.mvx + ME_Cpattern[ME_pos][0] * ME_step * 1;
                    MV[ME_dir].mvy = MV_cur.mvy + ME_Cpattern[ME_pos][1] * ME_step * 1;
                    clipMV(MV[ME_dir]);
                    cost_temp = MatchingMetric_PU( pSrc, me_info, &MV[ME_dir], PicYUVRef) + //MV_pred.qdiff(MV, par->QP);
                        MVCost( MV, curRefIdx, pInfo, mergeInfo);
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
                //    ////clipMV(MV[ME_dir]);
                //    cost_temp = MatchingMetric_PU( pSrc, me_info, &MV[ME_dir], PicYUVRef) + //MV_pred.qdiff(MV, par->QP);
                //        MVCost( MV, curRefIdx, pInfo, mergeInfo);
                //    if (cost_best[ME_dir] > cost_temp) {
                //        MV_best[ME_dir] = MV[ME_dir];
                //        cost_best[ME_dir] = cost_temp;
                //        refine = 0;
                //    }
                //}

                for (ME_pos = start; ME_pos < start+len; ME_pos++) {
                    MV[ME_dir].mvx = MV_cur.mvx + ME_Cpattern[ME_pos][0] * ME_step * 1;
                    MV[ME_dir].mvy = MV_cur.mvy + ME_Cpattern[ME_pos][1] * ME_step * 1;
                    H265MV MVorig = { MV[ME_dir].mvx, MV[ME_dir].mvy };
                    clipMV(MV[ME_dir]);
                    cost_temp = MatchingMetric_PU( pSrc, me_info, &MV[ME_dir], PicYUVRef) + //MV_pred.qdiff(MV, par->QP);
                        MVCost( MV, curRefIdx, pInfo, mergeInfo);
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
        for (T_RefIdx ref_idx = 0; ref_idx < cslice->num_ref_idx[ME_dir]; ref_idx++) {
            if (cost_best_allref > cost_best_ref[ME_dir][ref_idx]) {
                cost_best_allref = cost_best_ref[ME_dir][ref_idx];
                me_info->cost_1dir[ME_dir] = cost_best_ref[ME_dir][ref_idx];
                me_info->MV[ME_dir] = MV_best_ref[ME_dir][ref_idx];
                me_info->ref_idx[ME_dir] = ref_idx;
            }
        }
    }

    if (cslice->slice_type == B_SLICE && (me_info->width + me_info->height != 12)) {
        // bidirectional
        H265Frame *PicYUVRefF = pList[0]->m_RefPicList[me_info->ref_idx[0]];
        H265Frame *PicYUVRefB = pList[1]->m_RefPicList[me_info->ref_idx[1]];
//        T_RefIdx curRefIdx[2] = {0, 0};
        if(PicYUVRefF && PicYUVRefB) {
            interp_idx_first = interp_idx_last = 0; // for MatchingMetricBipred_PU optimization
            me_info->cost_bidir = MatchingMetricBipred_PU(pSrc, me_info, PicYUVRefF->y, PicYUVRefF->pitch_luma, PicYUVRefB->y, PicYUVRefB->pitch_luma, me_info->MV) +
                MVCost( me_info->MV, me_info->ref_idx, pInfo, mergeInfo);

            // refine Bidir
            if (IPP_MIN(me_info->cost_1dir[0], me_info->cost_1dir[1]) * 9 > 8 * me_info->cost_bidir) {
                bool changed;
                H265MV MV2_best[2] = {me_info->MV[0], me_info->MV[1]};
                Ipp32s cost2_best = me_info->cost_bidir;
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

                        Ipp32s cost_temp = MatchingMetricBipred_PU(pSrc, me_info, PicYUVRefF->y, PicYUVRefF->pitch_luma, PicYUVRefB->y, PicYUVRefB->pitch_luma, MV2_tmp) +
                            MVCost( MV2_tmp, me_info->ref_idx, pInfo, mergeInfo);
                        if (cost_temp < cost2_best) {
                            MV2_best[0] = MV2_tmp[0]; MV2_best[1] = MV2_tmp[1];
                            cost2_best = cost_temp;
                            changed = true;
                        }
                    }
                } while (changed);
                me_info->MV[0] = MV2_best[0]; me_info->MV[1] = MV2_best[1];
                me_info->cost_bidir = cost2_best;
            }


            if (me_info->cost_1dir[0] <= me_info->cost_1dir[1] && me_info->cost_1dir[0] <= me_info->cost_bidir) {
                me_info->inter_dir = INTER_DIR_PRED_L0;
                me_info->cost_inter = me_info->cost_1dir[0];
                me_info->ref_idx[1] = -1;
            } else if (me_info->cost_1dir[1] < me_info->cost_1dir[0] && me_info->cost_1dir[1] <= me_info->cost_bidir) {
                me_info->inter_dir = INTER_DIR_PRED_L1;
                me_info->cost_inter = me_info->cost_1dir[1];
                me_info->ref_idx[0] = -1;
            } else {
                me_info->inter_dir = INTER_DIR_PRED_L0 + INTER_DIR_PRED_L1;
                me_info->cost_inter = me_info->cost_bidir;
            }
        } else {
            me_info->inter_dir = INTER_DIR_PRED_L0;
            me_info->cost_inter = me_info->cost_1dir[0];
            me_info->cost_bidir = INT_MAX;
        }
    } else {
        me_info->inter_dir = INTER_DIR_PRED_L0;
        me_info->cost_inter = me_info->cost_1dir[0];
    }
}

void H265CU::EncAndRecLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost) {
    Ipp32s depth_max = data[absPartIdx].depth + data[absPartIdx].trIdx;
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );
    Ipp32u i;
    Ipp32s width = data[absPartIdx].size >> data[absPartIdx].trIdx;
    Ipp8u nzt;
    CostType cost_temp = COST_MAX;

    if (nz) *nz = 0;

    //if (depth == data[absPartIdx].depth && data[absPartIdx].pred_mode == MODE_INTER) {
    //    InterPredCU(absPartIdx, depth, y_rec, pitch_rec, 1);
    //}

    if (depth == depth_max) {
        EncAndRecLumaTU( absPartIdx, offset, width, nz, cost, 0, INTRA_PRED_CALC);
        if (nz && *nz) {
            setCbfOne(absPartIdx, TEXT_LUMA, data[absPartIdx].trIdx);
            for (i = 1; i < num_parts; i++)
                data[absPartIdx + i].cbf[0] = data[absPartIdx].cbf[0];
        }
    } else {
        Ipp32s subsize = width << (depth_max - depth) >> 1;
        subsize *= subsize;
        num_parts >>= 2;
        if (cost) *cost = 0;
        for (i = 0; i < 4; i++) {
            EncAndRecLuma(absPartIdx + num_parts * i, offset, depth+1, &nzt, cost ? &cost_temp : 0);
            if (cost) *cost += cost_temp;
            if (nz && nzt) *nz = 1;
            offset += subsize;
        }
        if (nz && *nz && depth >= data[absPartIdx].depth) {
            for (i = 0; i < 4; i++) {
                setCbfOne(absPartIdx + num_parts * i, TEXT_LUMA, depth - data[absPartIdx].depth);
            }
        }
    }
}

void H265CU::EncAndRecChroma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost) {
    Ipp32s depth_max = data[absPartIdx].depth + data[absPartIdx].trIdx;
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) )>>2;
    Ipp32u i;
    Ipp32s width = data[absPartIdx].size >> data[absPartIdx].trIdx << (depth_max - depth);
    width >>= 1;
    Ipp8u nzt[2];
    CostType cost_temp;

    if(nz) nz[0] = nz[1] = 0;

    if (depth == data[absPartIdx].depth && data[absPartIdx].predMode == MODE_INTER) {
        InterPredCU(absPartIdx, depth, uv_rec, pitch_rec, 0);
    }

    if (depth == depth_max || width == 4) {
        EncAndRecChromaTU(absPartIdx, offset, width, nz, cost);
        if (nz) {
            if (nz[0]) {
                for (i = 0; i < 4; i++)
                   setCbfOne(absPartIdx + num_parts * i, TEXT_CHROMA_U, data[absPartIdx].trIdx);
                if (depth != depth_max && data[absPartIdx].trIdx > 0) {
                    for (i = 0; i < 4; i++)
                       setCbfOne(absPartIdx + num_parts * i, TEXT_CHROMA_U, data[absPartIdx].trIdx - 1);
                }
            }
            if (nz[1]) {
                for (i = 0; i < 4; i++)
                   setCbfOne(absPartIdx + num_parts * i, TEXT_CHROMA_V, data[absPartIdx].trIdx);
                if (depth != depth_max && data[absPartIdx].trIdx > 0) {
                    for (i = 0; i < 4; i++)
                       setCbfOne(absPartIdx + num_parts * i, TEXT_CHROMA_V, data[absPartIdx].trIdx - 1);
                }
            }
        }
    } else {
        Ipp32s subsize = width >> 1;
        subsize *= subsize;
        if (cost) *cost = 0;
        for (i = 0; i < 4; i++) {
            EncAndRecChroma(absPartIdx + num_parts * i, offset, depth+1, nz ? nzt : NULL, cost ? &cost_temp : 0);
            if (cost) *cost += cost_temp;
            offset += subsize;

            if (nz) {
                if (nzt[0]) nz[0] = 1;
                if (nzt[1]) nz[1] = 1;
            }
        }
        if (depth >= data[absPartIdx].depth && nz) {
            if (nz[0]) {
                for(i = 0; i < 4; i++) {
                   setCbfOne(absPartIdx + num_parts * i, TEXT_CHROMA_U, depth - data[absPartIdx].depth);
                }
            }
            if (nz[1]) {
                for(i = 0; i < 4; i++) {
                   setCbfOne(absPartIdx + num_parts * i, TEXT_CHROMA_V, depth - data[absPartIdx].depth);
                }
            }
        }
    }
}

static void tu_add_clip(PixType *dst, PixType *src1, CoeffsType *src2,
                     Ipp32s pitch_dst, Ipp32s pitch_src1, Ipp32s pitch_src2, Ipp32s size/*, Ipp32s maxval*/)
{
    Ipp32s i, j;
    Ipp32s maxval = 255;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            dst[i] = (PixType)Saturate(0, maxval, src1[i] + src2[i]);
        }
        dst += pitch_dst;
        src1 += pitch_src1;
        src2 += pitch_src2;
    }
}

static void tu_add_clip_nv12(PixType *dst_nv12, PixType *src1_nv12, CoeffsType *src2_yv12,
                     Ipp32s pitch_dst, Ipp32s pitch_src1, Ipp32s pitch_src2, Ipp32s size/*, Ipp32s maxval*/)
{
    Ipp32s i, j;
    Ipp32s maxval = 255;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            dst_nv12[2*i] = (PixType)Saturate(0, maxval, src1_nv12[2*i] + src2_yv12[i]);
        }
        dst_nv12  += pitch_dst;
        src1_nv12 += pitch_src1;
        src2_yv12 += pitch_src2;
    }
}

static void tu_add_clip_transp(PixType *dst, PixType *src1, CoeffsType *src2,
                     Ipp32s pitch_dst, Ipp32s pitch_src1, Ipp32s pitch_src2, Ipp32s size/*, Ipp32s maxval*/)
{
    Ipp32s i, j;
    Ipp32s maxval = 255;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            dst[i] = (PixType)Saturate(0, maxval, src1[pitch_src1*i] + src2[i]);
        }
        dst += pitch_dst;
        src1 ++;
        src2 += pitch_src2;
    }
}

static void tu_diff(CoeffsType *residual, PixType *src, PixType *pred,
                     Ipp32s pitch_dst, Ipp32s pitch_src, Ipp32s pitch_pred, Ipp32s size)
{
    //Ipp32s step_dst = pitch_dst * sizeof(CoeffsType);
    //if (size == 4) {
    //    ippiSub4x4_8u16s_C1R(src, pitch_src, pred, pitch_pred, residual, step_dst);
    //} else if (size == 8) {
    //    ippiSub8x8_8u16s_C1R(src, pitch_src, pred, pitch_pred, residual, step_dst);
    //} else if (size == 16) {
    //    ippiSub16x16_8u16s_C1R(src, pitch_src, pred, pitch_pred, residual, step_dst);
    //} else if (size == 32) {
    //    for (Ipp32s y = 0; y < 32; y += 16) {
    //        for (Ipp32s x = 0; x < 32; x += 16) {
    //            ippiSub16x16_8u16s_C1R(src + x + y * pitch_src, pitch_src,
    //                                   pred + x + y * pitch_pred, pitch_pred,
    //                                   residual + x + y * pitch_dst, step_dst);
    //        }
    //    }
    //}

    Ipp32s i, j;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            residual[i] = (CoeffsType)src[i] - pred[i];
        }
        residual += pitch_dst;
        src += pitch_src;
        pred += pitch_pred;
    }
}

static void tu_diff_transp(CoeffsType *residual, PixType *src, PixType *pred,
                     Ipp32s pitch_dst, Ipp32s pitch_src, Ipp32s pitch_pred, Ipp32s size)
{
    Ipp32s i, j;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            residual[i] = (CoeffsType)src[i] - pred[pitch_pred*i];
        }
        residual += pitch_dst;
        src += pitch_src;
        pred ++;
    }
}

static Ipp32s tu_sad(PixType *src, PixType *rec,
                   Ipp32s pitch_src, Ipp32s pitch_rec, Ipp32s size)
{
    Ipp32s i, j;
    Ipp32s s = 0;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            s += abs((CoeffsType)src[i] - rec[i]);
        }
        src += pitch_src;
        rec += pitch_rec;
    }
    return s;
}

static Ipp32s tu_sse(PixType *src, PixType *rec,
                   Ipp32s pitch_src, Ipp32s pitch_rec, Ipp32s size)
{
    Ipp32s i, j;
    Ipp32s s = 0;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            s += ((CoeffsType)src[i] - rec[i]) * (Ipp32s)((CoeffsType)src[i] - rec[i]);
        }
        src += pitch_src;
        rec += pitch_rec;
    }
    return s;
}

Ipp32s h265_tu_had(PixType *src, PixType *rec,
                   Ipp32s pitch_src, Ipp32s pitch_rec, Ipp32s width, Ipp32s height)
{
  Ipp32u s = 0;
  Ipp32s i, j;

  Ipp32s satd = 0;
  if (!((width | height) & 7))
  {
      for (j = 0; j < height; j += 8)
      {
          for (i = 0; i < width; i += 8)
          {
              //s += calc_had8x8(&src[i], &rec[i], pitch_src, pitch_rec);
              ippiSAT8x8D_8u32s_C1R(&src[i], pitch_src, &rec[i], pitch_rec, &satd);
              s += ((satd + 2) >> 2);
          }
          src += pitch_src * 8;
          rec += pitch_rec * 8;
      }
  }
  else if (!((width | height) & 3))
  {
      for (j = 0; j < height; j += 4)
      {
          for (i = 0; i < width; i += 4)
          {
              //s += calc_had4x4(&src[i], &rec[i], pitch_src, pitch_rec);
              ippiSATD4x4_8u32s_C1R(&src[i], pitch_src, &rec[i], pitch_rec, &satd);
              s += ((satd+1) >> 1);
          }
          src += pitch_src * 4;
          rec += pitch_rec * 4;
      }
  }
  else
  {
      VM_ASSERT(0);
  }

  return s;
}

static void tu_diff_nv12(CoeffsType *residual, PixType *src, PixType *pred,
                     Ipp32s pitch_dst, Ipp32s pitch_src, Ipp32s pitch_pred, Ipp32s size)
{
    Ipp32s i, j;
    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            residual[i] = (CoeffsType)src[i*2] - pred[i*2];
        }
        residual += pitch_dst;
        src += pitch_src;
        pred += pitch_pred;
    }
}

static Ipp32s tu_sad_nv12(PixType *src, PixType *rec,
                      Ipp32s pitch_src, Ipp32s pitch_rec, Ipp32s size)
{
    Ipp32s i, j;
    Ipp32s s = 0;

    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            s += abs((CoeffsType)src[i*2] - rec[i]);
        }
        src += pitch_src;
        rec += pitch_rec;
    }
    return s;
}

static Ipp32s tu_sse_nv12(PixType *src, PixType *rec,
                      Ipp32s pitch_src, Ipp32s pitch_rec, Ipp32s size)
{
    Ipp32s i, j;
    Ipp32s s = 0;

    for (j = 0; j < size; j++) {
        for (i = 0; i < size; i++) {
            s += ((CoeffsType)src[i*2] - rec[i]) * ((CoeffsType)src[i*2] - rec[i]);
        }
        src += pitch_src;
        rec += pitch_rec;
    }
    return s;
}

void H265CU::EncAndRecLumaTU(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost,
                             Ipp8u cost_pred_flag, IntraPredOpt pred_opt)
{
    CostType cost_pred, cost_rdoq;
    Ipp32u lpel_x   = ctb_pelx +
        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
    Ipp32u tpel_y   = ctb_pely +
        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);

    if (nz) *nz = 0;
    if (cost) *cost = 0;
    else if (IsRDOQ()) cost = &cost_rdoq;

    if (lpel_x >= par->Width || tpel_y >= par->Height)
        return;

    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    PixType *rec = y_rec + ((PUStartRow * pitch_rec + PUStartColumn) << par->Log2MinTUSize);
    PixType *src = y_src + ((PUStartRow * pitch_src + PUStartColumn) << par->Log2MinTUSize);

    PixType *pred = NULL;
    Ipp32s  pitch_pred = 0;
    Ipp32s  is_pred_transposed = 0;
    if (data[absPartIdx].predMode == MODE_INTRA) {
        if (pred_opt == INTRA_PRED_IN_BUF) {
            pred = pred_intra_all + data[absPartIdx].intraLumaDir * width * width;
            pitch_pred = width;
            is_pred_transposed = (data[absPartIdx].intraLumaDir >= 2 &&
                data[absPartIdx].intraLumaDir < 18);
        } else {
            pred = rec;
            pitch_pred = pitch_rec;
        }
    } else {
        pitch_pred = MAX_CU_SIZE;
        pred = inter_pred_ptr[ data[absPartIdx].depth ];
        pred += (PUStartRow * pitch_pred + PUStartColumn) << par->Log2MinTUSize;
    }

    if (data[absPartIdx].predMode == MODE_INTRA && pred_opt == INTRA_PRED_CALC) {
        IntraPredTU(absPartIdx, width, data[absPartIdx].intraLumaDir, 1);
    }

    if (cost && cost_pred_flag) {
        cost_pred = h265_tu_had(src, rec, pitch_src, pitch_rec, width, width);
        *cost = cost_pred;
        return;
    }

    Ipp8u cbf = 0;
    if (!data[absPartIdx].flags.skippedFlag) {
        if (is_pred_transposed)
            tu_diff_transp(residuals_y + offset, src, pred, width, pitch_src, pitch_pred, width);
        else
            tu_diff(residuals_y + offset, src, pred, width, pitch_src, pitch_pred, width);

        TransformFwd(offset, width, 1, data[absPartIdx].predMode == MODE_INTRA);
        QuantFwdTU(absPartIdx, offset, width, 1);

        if (rd_opt_flag || nz) {
            for (Ipp32s i = 0; i < width * width; i++) {
                if (tr_coeff_y[i + offset]) {
                    cbf = 1;
                    if (nz) *nz = 1;
                    break;
                }
            }
        }

        if (!rd_opt_flag || cbf)
            QuantInvTU(absPartIdx, offset, width, 1);

        IppiSize roi = { width, width };
        (is_pred_transposed)
            ? ippiTranspose_8u_C1R(pred, pitch_pred, rec, pitch_rec, roi)
            : ippiCopy_8u_C1R(pred, pitch_pred, rec, pitch_rec, roi);

        if (!rd_opt_flag || cbf)
            TransformInv2(rec, pitch_rec, offset, width, 1, data[absPartIdx].predMode == MODE_INTRA);

    } else {
        memset(tr_coeff_y + offset, 0, sizeof(CoeffsType) * width * width);
    }

    if (cost) {
        CostType cost_rec = tu_sse(src, rec, pitch_src, pitch_rec, width);
        *cost = cost_rec;
    }

    if (data[absPartIdx].flags.skippedFlag) {
        if (nz) *nz = 0;
        return;
    }

    if (rd_opt_flag && cost) {
        Ipp8u code_dqp;
        bsf->Reset();
        if (cbf) {
            setCbfOne(absPartIdx, TEXT_LUMA, data[absPartIdx].trIdx);
            put_transform(bsf, offset, 0, absPartIdx, absPartIdx,
                data[absPartIdx].depth + data[absPartIdx].trIdx,
                width, width, data[absPartIdx].trIdx, code_dqp);
        }
        else {
            bsf->EncodeSingleBin_CABAC(CTX(bsf,QT_CBF_HEVC) +
                getCtxQtCbf(absPartIdx, TEXT_LUMA, data[absPartIdx].trIdx),
                0);
        }
        if(IS_INTRA(data, absPartIdx))
            *cost += BIT_COST(bsf->GetNumBits());
        else
            *cost += BIT_COST_INTER(bsf->GetNumBits());
    }
}

int in_decision = 0;

void H265CU::EncAndRecChromaTU(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost)
{
    Ipp32u lpel_x   = ctb_pelx +
        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
    Ipp32u tpel_y   = ctb_pely +
        ((h265_scan_z2r[par->MaxCUDepth][absPartIdx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);

    if (nz) nz[0] = nz[1] = 0;
    if (cost) *cost = 0;

    if (lpel_x >= par->Width || tpel_y >= par->Height)
        return;

    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][absPartIdx];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    if (data[absPartIdx].predMode == MODE_INTRA) {
        Ipp8u intra_pred_mode = data[absPartIdx].intraChromaDir;
        if (intra_pred_mode == INTRA_DM_CHROMA) {
            Ipp32s shift = h265_log2m2[par->NumPartInCU >> (data[absPartIdx].depth<<1)] + 2;
            Ipp32s absPartIdx_0 = absPartIdx >> shift << shift;
            intra_pred_mode = data[absPartIdx_0].intraLumaDir;
        }
        IntraPredTU(absPartIdx, width, intra_pred_mode, 0);
    }

    if (!data[absPartIdx].flags.skippedFlag) {
        for (Ipp32s c_idx = 0; c_idx < 2; c_idx++) {
            PixType *pSrc = uv_src + (((PUStartRow * pitch_src >> 1) + PUStartColumn) << par->Log2MinTUSize) + c_idx;
            PixType *pRec = uv_rec + (((PUStartRow * pitch_rec >> 1) + PUStartColumn) << par->Log2MinTUSize) + c_idx;
            CoeffsType *residuals = c_idx ? residuals_v : residuals_u;

            tu_diff_nv12(residuals + offset, pSrc, pRec, width, pitch_src, pitch_rec, width);
        }

        TransformFwd(offset, width, 0, 0);
        QuantFwdTU(absPartIdx, offset, width, 0);

        QuantInvTU(absPartIdx, offset, width, 0);
        TransformInv(offset, width, 0, 0);
    } else {
        memset(tr_coeff_u + offset, 0, sizeof(CoeffsType) * width * width);
        memset(tr_coeff_v + offset, 0, sizeof(CoeffsType) * width * width);
    }

    Ipp8u cbf[2] = {0, 0};
    for (Ipp32s c_idx = 0; c_idx < 2; c_idx++) {
        PixType *pSrc = uv_src + (((PUStartRow * pitch_src >> 1) + PUStartColumn) << (par->Log2MinTUSize)) + c_idx;
        PixType *pRec = uv_rec + ((PUStartRow * pitch_rec + PUStartColumn * 2) << (par->Log2MinTUSize - 1)) + c_idx;
        CoeffsType *residuals = c_idx ? residuals_v : residuals_u;
        CoeffsType *coeff = c_idx ? tr_coeff_v : tr_coeff_u;

        if (!data[absPartIdx].flags.skippedFlag) {
            tu_add_clip_nv12(pRec, pRec, residuals + offset, pitch_rec, pitch_rec, width, width);
        }

        // use only bit cost for chroma
        if (cost && !rd_opt_flag) {
            *cost += tu_sse_nv12(pSrc, pRec, pitch_src, pitch_rec, width);
        }
        if (data[absPartIdx].flags.skippedFlag && nz) {
            nz[c_idx] = 0;
            continue;
        }
        if (rd_opt_flag || nz) {
            for (int i = 0; i < width * width; i++) {
                if (coeff[i + offset]) {
                    if (nz) nz[c_idx] = 1;
                    cbf[c_idx] = 1;
                    break;
                }
            }
        }
    }
    if (rd_opt_flag && cost) {
        bsf->Reset();
        if (cbf[0])
        {
            h265_code_coeff_NxN(bsf, this, (tr_coeff_u+offset), absPartIdx, width, width, TEXT_CHROMA_U );
        }
        if (cbf[1])
        {
            h265_code_coeff_NxN(bsf, this, (tr_coeff_v+offset), absPartIdx, width, width, TEXT_CHROMA_V );
        }
        if(IS_INTRA(data, absPartIdx))
            *cost = BIT_COST(bsf->GetNumBits());
        else
            *cost = BIT_COST_INTER(bsf->GetNumBits());
    }
}

/* ****************************************************************************** *\
FUNCTION: AddMVPCand
DESCRIPTION:
\* ****************************************************************************** */

bool H265CU::AddMVPCand(MVPInfo* pInfo,
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

    refPicList[0] = &cslice->m_pRefPicList->m_RefPicListL0;
    refPicList[1] = &cslice->m_pRefPicList->m_RefPicListL1;

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

static bool IsDiffMER(Ipp32s xN,
                      Ipp32s yN,
                      Ipp32s xP,
                      Ipp32s yP,
                      Ipp32s log2ParallelMergeLevel)
{
    if ((xN >> log2ParallelMergeLevel) != (xP >> log2ParallelMergeLevel))
    {
        return true;
    }
    if ((yN >> log2ParallelMergeLevel) != (yP >> log2ParallelMergeLevel))
    {
        return true;
    }
    return false;
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
    Ipp32s maxDepth = par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s minTUSize = par->MinTUSize;
    Ipp32s topLeftBlockZScanIdx;
    Ipp32s topLeftRasterIdx;
    Ipp32s topLeftRow;
    Ipp32s topLeftColumn;
    Ipp32s partWidth, partHeight, partX, partY;
    Ipp32s xP, yP, nPSW, nPSH;
    Ipp32s i;

    for (i = 0; i < 5; i++)
    {
        abCandIsInter[i] = false;
        isCandAvailable[i] = false;
    }

    GetPartOffsetAndSize(partIdx, partMode, cuSize, partX, partY, partWidth, partHeight);

    topLeftRasterIdx = h265_scan_z2r[maxDepth][topLeftCUBlockZScanIdx] + partX + numMinTUInLCU * partY;
    topLeftRow = topLeftRasterIdx >> maxDepth;
    topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
    topLeftBlockZScanIdx = h265_scan_r2z[maxDepth][topLeftRasterIdx];

    xP = ctb_pelx + topLeftColumn * minTUSize;
    yP = ctb_pely + topLeftRow * minTUSize;
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

    for (i = 0; i < 5; i++)
    {
        if (pInfo->numCand < 4)
        {
            candLCU[i] = GetNeighbour(candZScanIdx[i], candColumn[i], candRow[i], topLeftBlockZScanIdx, checkCurLCU[i]);

            if (candLCU[i])
            {
                if (!IsDiffMER(canXP[i], canYP[i], xP, yP, par->cpps->log2_parallel_merge_level))
                {
                    candLCU[i] = NULL;
                }
            }

            isCandInter[i] = true;

            if ((candLCU[i] == NULL) || (candLCU[i][candZScanIdx[i]].predMode == MODE_INTRA))
            {
                isCandInter[i] = false;
            }

            if (isCandInter[i])
            {
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
                        if (!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0],
                                                               candZScanIdx[1], candLCU[1]))
                        {
                            getInfo = true;
                        }
                    }
                    break;
                case 2: /* above right */
                    isCandAvailable[2] = true;
                    if (!isCandAvailable[1] || !HasEqualMotion(candZScanIdx[1], candLCU[1],
                                                           candZScanIdx[2], candLCU[2]))
                    {
                        getInfo = true;
                    }
                    break;
                case 3: /* below left */
                    isCandAvailable[3] = true;
                    if (!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0],
                                                           candZScanIdx[3], candLCU[3]))
                    {
                        getInfo = true;
                    }
                    break;
                case 4: /* above left */
                default:
                    isCandAvailable[4] = true;
                    if ((!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0],
                                                            candZScanIdx[4], candLCU[4])) &&
                        (!isCandAvailable[1] || !HasEqualMotion(candZScanIdx[1], candLCU[1],
                                                            candZScanIdx[4], candLCU[4])))
                    {
                        getInfo = true;
                    }
                    break;
                }

                if (getInfo)
                {
                    GetMergeCandInfo(pInfo, abCandIsInter, puhInterDirNeighbours,
                                     candZScanIdx[i], candLCU[i]);

                }
            }
        }
    }

    if (pInfo->numCand > MRG_MAX_NUM_CANDS-1)
        pInfo->numCand = MRG_MAX_NUM_CANDS-1;

    /* temporal candidate */
    if (par->TMVPFlag)
    {
        Ipp32s frameWidthInSamples = par->Width;
        Ipp32s frameHeightInSamples = par->Height;
        Ipp32s bottomRightCandRow = topLeftRow + partHeight;
        Ipp32s bottomRightCandColumn = topLeftColumn + partWidth;
        Ipp32s centerCandRow = topLeftRow + (partHeight >> 1);
        Ipp32s centerCandColumn = topLeftColumn + (partWidth >> 1);
        T_RefIdx refIdx = 0;
        bool existMV = false;
        H265MV colCandMv;

        if ((((Ipp32s)ctb_pelx + bottomRightCandColumn * minTUSize) >= frameWidthInSamples) ||
            (((Ipp32s)ctb_pely + bottomRightCandRow * minTUSize) >= frameHeightInSamples) ||
             (bottomRightCandRow >= numMinTUInLCU))
        {
            candLCU[0] = NULL;
        }
        else if (bottomRightCandColumn < numMinTUInLCU)
        {
            candLCU[0] = m_colocatedLCU[0];
            candZScanIdx[0] = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn];
        }
        else
        {
            candLCU[0] = m_colocatedLCU[1];
            candZScanIdx[0] = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn - numMinTUInLCU];
        }

        candLCU[1] = m_colocatedLCU[0];
        candZScanIdx[1] = h265_scan_r2z[maxDepth][(centerCandRow << maxDepth) + centerCandColumn];

        if ((candLCU[0] != NULL) &&
            GetColMVP(candLCU[0], candZScanIdx[0], 0, refIdx, colCandMv))
        {
            existMV = true;
        }

        if (!existMV)
        {
            existMV = GetColMVP(candLCU[1], candZScanIdx[1], 0, refIdx, colCandMv);
        }

        if (existMV)
        {
            abCandIsInter[pInfo->numCand] = true;

            pInfo->mvCand[2*pInfo->numCand].mvx = colCandMv.mvx;
            pInfo->mvCand[2*pInfo->numCand].mvy = colCandMv.mvy;

            pInfo->refIdx[2*pInfo->numCand] = refIdx;
            puhInterDirNeighbours[pInfo->numCand] = 1;

            if (cslice->slice_type == B_SLICE)
            {
                existMV = false;

                if ((candLCU[0] != NULL) &&
                    GetColMVP(candLCU[0], candZScanIdx[0], 1, refIdx, colCandMv))
                {
                    existMV = true;
                }

                if (!existMV)
                {
                    existMV = GetColMVP(candLCU[1], candZScanIdx[1], 1, refIdx, colCandMv);
                }

                if (existMV)
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
    if (cslice->slice_type == B_SLICE)
    {
        Ipp32s uiPriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
        Ipp32s uiPriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};
        Ipp32s limit = pInfo->numCand * (pInfo->numCand - 1);

        for (i = 0; i < limit && pInfo->numCand != MRG_MAX_NUM_CANDS; i++)
        {
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

                if ((cslice->m_pRefPicList->m_RefPicListL0.m_Tb[pInfo->refIdx[2*pInfo->numCand]] ==
                     cslice->m_pRefPicList->m_RefPicListL1.m_Tb[pInfo->refIdx[2*pInfo->numCand+1]]) &&
                    (pInfo->mvCand[2*pInfo->numCand].mvx == pInfo->mvCand[2*pInfo->numCand+1].mvx) &&
                    (pInfo->mvCand[2*pInfo->numCand].mvy == pInfo->mvCand[2*pInfo->numCand+1].mvy))
                {
                    abCandIsInter[pInfo->numCand] = false;
                }
                else
                {
                    pInfo->numCand++;
                }
            }
        }
    }

    /* zero motion vector merging candidates */
    {
        Ipp32s numRefIdx = cslice->num_ref_idx_l0_active;
        T_RefIdx r = 0;
        T_RefIdx refCnt = 0;

        if (cslice->slice_type == B_SLICE)
        {
            if (cslice->num_ref_idx_l1_active < cslice->num_ref_idx_l0_active)
            {
                numRefIdx = cslice->num_ref_idx_l1_active;
            }
        }

        while (pInfo->numCand < MRG_MAX_NUM_CANDS)
        {
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
FUNCTION: GetPUMVInfo
DESCRIPTION:
\* ****************************************************************************** */

void H265CU::GetPUMVInfo(Ipp32s blockZScanIdx,
                         Ipp32s partAddr,
                         Ipp32s partMode,
                         Ipp32s curPUidx)
{
    Ipp32s CUSizeInMinTU = data[blockZScanIdx].size >> par->Log2MinTUSize;
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

    if (cslice->slice_type == B_SLICE)
    {
        numRefLists = 2;

        if (BIPRED_RESTRICT_SMALL_PU)
        {
            if ((data[blockZScanIdx].size == 8) && (partMode != PART_SIZE_2Nx2N) &&
                (data[blockZScanIdx].interDir & INTER_DIR_PRED_L0))
            {
                numRefLists = 1;
            }
        }
    }

    data[blockZScanIdx].flags.mergeFlag = 0;
    data[blockZScanIdx].mvpNum[0] = data[blockZScanIdx].mvpNum[1] = 0;

    curRefIdx[0] = -1;
    curRefIdx[1] = -1;

    curMV[0].mvx = data[blockZScanIdx].mv[0].mvx;//   curCUData->MVxl0[curPUidx];
    curMV[0].mvy = data[blockZScanIdx].mv[0].mvy;//   curCUData->MVyl0[curPUidx];
    curMV[1].mvx = data[blockZScanIdx].mv[1].mvx;//   curCUData->MVxl1[curPUidx];
    curMV[1].mvy = data[blockZScanIdx].mv[1].mvy;//   curCUData->MVyl1[curPUidx];

    if (data[blockZScanIdx].interDir & INTER_DIR_PRED_L0)
    {
        curRefIdx[0] = data[blockZScanIdx].refIdx[0];
    }

    if ((cslice->slice_type == B_SLICE) && (data[blockZScanIdx].interDir & INTER_DIR_PRED_L1))
    {
        curRefIdx[1] = data[blockZScanIdx].refIdx[1];
    }

    if ((par->cpps->log2_parallel_merge_level > 2) && (data[blockZScanIdx].size == 8))
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
            data[blockZScanIdx].flags.mergeFlag = 1;
            data[blockZScanIdx].mergeIdx = i;
            break;
        }
    }

    if (!data[blockZScanIdx].flags.mergeFlag)
    {
        Ipp32s refList;

        for (refList = 0; refList < 2; refList++)
        {
            if (curRefIdx[refList] >= 0)
            {
                Ipp32s minDist = 0;

                GetMvpCand(topLeftBlockZScanIdx, refList, curRefIdx[refList], partMode, curPUidx, CUSizeInMinTU, &pInfo);

                data[blockZScanIdx].mvpNum[refList] = (Ipp8s)pInfo.numCand;

                /* find the best MV cand */

                minDist = abs(curMV[refList].mvx - pInfo.mvCand[0].mvx) +
                          abs(curMV[refList].mvy - pInfo.mvCand[0].mvy);
                data[blockZScanIdx].mvpIdx[refList] = 0;

                for (i = 1; i < pInfo.numCand; i++)
                {
                    Ipp32s dist = abs(curMV[refList].mvx - pInfo.mvCand[i].mvx) +
                               abs(curMV[refList].mvy - pInfo.mvCand[i].mvy);

                    if (dist < minDist)
                    {
                        minDist = dist;
                        data[blockZScanIdx].mvpIdx[refList] = i;
                    }
                }

                data[blockZScanIdx].mvd[refList].mvx = curMV[refList].mvx - pInfo.mvCand[data[blockZScanIdx].mvpIdx[refList]].mvx;
                data[blockZScanIdx].mvd[refList].mvy = curMV[refList].mvy - pInfo.mvCand[data[blockZScanIdx].mvpIdx[refList]].mvy;
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
    Ipp32s maxDepth = par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
//    Ipp32s maxCUSize = par->MaxCUSize;
    Ipp32s minCUSize = par->MinCUSize;
    Ipp32s minTUSize = par->MinTUSize;
    Ipp32s frameWidthInSamples = par->Width;
    Ipp32s frameHeightInSamples = par->Height;
//    Ipp32s frameWidthInLCUs = par->PicWidthInCtbs;
    Ipp32s sliceStartAddr = cslice->slice_segment_address;
    Ipp32s tmpNeighbourBlockRow = neighbourBlockRow;
    Ipp32s tmpNeighbourBlockColumn = neighbourBlockColumn;
    Ipp32s neighbourLCUAddr = 0;
    H265CUData* neighbourLCU;


    if ((((Ipp32s)ctb_pelx + neighbourBlockColumn * minTUSize) >= frameWidthInSamples) ||
        (((Ipp32s)ctb_pely + neighbourBlockRow * minTUSize) >= frameHeightInSamples))
    {
        neighbourBlockZScanIdx = 256; /* doesn't matter */
        return NULL;
    }

    if ((((Ipp32s)ctb_pelx + neighbourBlockColumn * minTUSize) < 0) ||
        (((Ipp32s)ctb_pely + neighbourBlockRow * minTUSize) < 0))
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
            neighbourLCU = p_above_left;
            neighbourLCUAddr = above_left_addr;
        }
        else if (neighbourBlockColumn >= numMinTUInLCU)
        {
            neighbourLCU = p_above_right;
            neighbourLCUAddr = above_right_addr;
        }
        else
        {
            neighbourLCU = p_above;
            neighbourLCUAddr = above_addr;
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
            neighbourLCU = p_left;
            neighbourLCUAddr = left_addr;
        }
        else if (neighbourBlockColumn >= numMinTUInLCU)
        {
            neighbourLCU = NULL;
        }
        else
        {
            neighbourLCU = data;
            neighbourLCUAddr = ctb_addr;

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
FUNCTION: GetColMVP
DESCRIPTION:
\* ****************************************************************************** */

bool H265CU::GetColMVP(H265CUData* colLCU,
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
    Ipp32s minTUSize = par->MinTUSize;

    /* MV compression */
    if (par->MinTUSize < 2)
    {
        Ipp32s shift = 2 * (2 - minTUSize);
        blockZScanIdx = (blockZScanIdx >> shift) << shift;
    }

    refPicList[0] = &cslice->m_pRefPicList->m_RefPicListL0;
    refPicList[1] = &cslice->m_pRefPicList->m_RefPicListL1;

    if ((colLCU == NULL) || (colLCU[blockZScanIdx].predMode == MODE_INTRA))
    {
        return false;
    }

    if (cslice->slice_type != B_SLICE)
    {
        colPicListIdx = 1;
    }
    else
    {
/*        if (m_SliceInf.IsLowDelay)
        {
            colPicListIdx = refPicListIdx;
        }
        else */if (cslice->collocated_from_l0_flag)
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
    colRefTb = m_ColFrmRefFramesTbList[colPicListIdx][colRefIdx];
    isColRefLongTerm = m_ColFrmRefIsLongTerm[colPicListIdx][colRefIdx];

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
    Ipp32s maxDepth = par->MaxCUDepth;
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
    bAdded = AddMVPCand(pInfo, belowLeftCandLCU, belowLeftCandBlockZScanIdx, refPicListIdx, refIdx, false);

    if (!bAdded)
    {
        leftCandLCU = GetNeighbour(leftCandBlockZScanIdx, topLeftColumn - 1,
                                   topLeftRow + partHeight - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMVPCand(pInfo, leftCandLCU, leftCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (!bAdded)
    {
        bAdded = AddMVPCand(pInfo, belowLeftCandLCU, belowLeftCandBlockZScanIdx, refPicListIdx, refIdx, true);

        if (!bAdded)
        {
            bAdded = AddMVPCand(pInfo, leftCandLCU, leftCandBlockZScanIdx, refPicListIdx, refIdx, true);
        }
    }

    /* Above predictor search */

    aboveRightCandLCU = GetNeighbour(aboveRightCandBlockZScanIdx, topLeftColumn + partWidth,
                                     topLeftRow - 1, topLeftBlockZScanIdx, true);
    bAdded = AddMVPCand(pInfo, aboveRightCandLCU, aboveRightCandBlockZScanIdx, refPicListIdx, refIdx, false);

    if (!bAdded)
    {
        aboveCandLCU = GetNeighbour(aboveCandBlockZScanIdx, topLeftColumn + partWidth - 1,
                                    topLeftRow - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMVPCand(pInfo, aboveCandLCU, aboveCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (!bAdded)
    {
        aboveLeftCandLCU = GetNeighbour(aboveLeftCandBlockZScanIdx, topLeftColumn - 1,
                                        topLeftRow - 1, topLeftBlockZScanIdx, false);
        bAdded = AddMVPCand(pInfo, aboveLeftCandLCU, aboveLeftCandBlockZScanIdx, refPicListIdx, refIdx, false);
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
        bAdded = AddMVPCand(pInfo, aboveRightCandLCU, aboveRightCandBlockZScanIdx, refPicListIdx, refIdx, true);
        if (!bAdded)
        {
            bAdded = AddMVPCand(pInfo, aboveCandLCU, aboveCandBlockZScanIdx, refPicListIdx, refIdx, true);
        }

        if (!bAdded)
        {
            bAdded = AddMVPCand(pInfo, aboveLeftCandLCU, aboveLeftCandBlockZScanIdx, refPicListIdx, refIdx, true);
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

    if (par->TMVPFlag)
    {
        Ipp32s frameWidthInSamples = par->Width;
        Ipp32s frameHeightInSamples = par->Height;
        Ipp32s topLeftRow = topLeftRasterIdx >> maxDepth;
        Ipp32s topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
        Ipp32s bottomRightCandRow = topLeftRow + partHeight;
        Ipp32s bottomRightCandColumn = topLeftColumn + partWidth;
        Ipp32s minTUSize = par->MinTUSize;
        Ipp32s colCandBlockZScanIdx = 0;
        H265CUData* colCandLCU;
        H265MV colCandMv;

        if ((((Ipp32s)ctb_pelx + bottomRightCandColumn * minTUSize) >= frameWidthInSamples) ||
            (((Ipp32s)ctb_pely + bottomRightCandRow * minTUSize) >= frameHeightInSamples) ||
            (bottomRightCandRow >= numMinTUInLCU))
        {
            colCandLCU = NULL;
        }
        else if (bottomRightCandColumn < numMinTUInLCU)
        {
            colCandLCU = m_colocatedLCU[0];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn];
        }
        else
        {
            colCandLCU = m_colocatedLCU[1];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn - numMinTUInLCU];
        }

        if ((colCandLCU != NULL) &&
            GetColMVP(colCandLCU, colCandBlockZScanIdx, refPicListIdx, refIdx, colCandMv))
        {
            pInfo->mvCand[pInfo->numCand] = colCandMv;
            pInfo->numCand++;
        }
        else
        {
            Ipp32s centerCandRow = topLeftRow + (partHeight >> 1);
            Ipp32s centerCandColumn = topLeftColumn + (partWidth >> 1);

            colCandLCU = m_colocatedLCU[0];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(centerCandRow << maxDepth) + centerCandColumn];

            if (GetColMVP(colCandLCU, colCandBlockZScanIdx, refPicListIdx, refIdx, colCandMv))
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

Ipp8u H265CU::getNumPartInter(Ipp32s absPartIdx)
{
    Ipp8u iNumPart = 0;

    switch (data[absPartIdx].partSize)
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
FUNCTION: GetPUMVPredictorInfo
DESCRIPTION: collects MV predictors.
    Merge MV refidx are interleaved,
    amvp refidx are separated.
\* ****************************************************************************** */

void H265CU::GetPUMVPredictorInfo(Ipp32s blockZScanIdx,
                                  Ipp32s partAddr,
                                  Ipp32s partMode,
                                  Ipp32s curPUidx,
                                  MVPInfo *pInfo,
                                  MVPInfo& mergeInfo)
{
    Ipp32s CUSizeInMinTU = data[blockZScanIdx].size >> par->Log2MinTUSize;
    Ipp32s numRefLists = 1;
    Ipp32s topLeftBlockZScanIdx = blockZScanIdx;
    blockZScanIdx += partAddr;

    if (cslice->slice_type == B_SLICE)
    {
        numRefLists = 2;

        if (BIPRED_RESTRICT_SMALL_PU)
        {
            if ((data[blockZScanIdx].size == 8) && (partMode != PART_SIZE_2Nx2N) &&
                (data[blockZScanIdx].interDir & INTER_DIR_PRED_L0))
            {
                numRefLists = 1;
            }
        }
    }

    data[blockZScanIdx].flags.mergeFlag = 0;
    data[blockZScanIdx].mvpNum[0] = data[blockZScanIdx].mvpNum[1] = 0;


    if ((par->cpps->log2_parallel_merge_level > 2) && (data[blockZScanIdx].size == 8))
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

    //if (!data[blockZScanIdx].flags.merge_flag)
    {
        Ipp32s refList, i;

        for (refList = 0; refList < ((cslice->slice_type == B_SLICE) ? 2 : 1); refList++)
        {
            for (T_RefIdx cur_ref = 0; cur_ref < cslice->num_ref_idx[refList]; cur_ref++) {

                pInfo[cur_ref*2+refList].numCand = 0;
//                if (curRefIdx[refList] >= 0)
                {
                    GetMvpCand(topLeftBlockZScanIdx, refList, cur_ref, partMode, curPUidx, CUSizeInMinTU, &pInfo[cur_ref*2+refList]);
                    data[blockZScanIdx].mvpNum[refList] = (Ipp8s)pInfo[cur_ref*2+refList].numCand;
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
    Ipp8u size = (Ipp8u)(par->MaxCUSize>>depth);
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );

    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
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
    me_info.posx = (Ipp8u)((h265_scan_z2r[par->MaxCUDepth][absPartIdx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
    me_info.posy = (Ipp8u)((h265_scan_z2r[par->MaxCUDepth][absPartIdx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);
    me_info.split_mode = PART_SIZE_2Nx2N;

    data[absPartIdx].mvpNum[0] = data[absPartIdx].mvpNum[1] = 0;
    data[absPartIdx].flags.skippedFlag = 1;
    data[absPartIdx].flags.mergeFlag = 1;
    data[absPartIdx].size = size;
    data[absPartIdx].predMode = MODE_INTER;
    data[absPartIdx].partSize = PART_SIZE_2Nx2N;
    data[absPartIdx].qp = par->QP;
    data[absPartIdx].depth = depth;
    data[absPartIdx].trIdx = 0;
    data[absPartIdx].cbf[0] = data[absPartIdx].cbf[1] = data[absPartIdx].cbf[2] = 0;

    Ipp32s CUSizeInMinTU = data[absPartIdx].size >> par->Log2MinTUSize;

    GetInterMergeCandidates(absPartIdx, PART_SIZE_2Nx2N, 0, CUSizeInMinTU, &mergeInfo);

    PixType *pSrc = y_src + me_info.posx + me_info.posy * pitch_src;
    interp_idx_first = interp_idx_last = 0; // for MatchingMetricBipred_PU optimization

    if (mergeInfo.numCand > 0) {
        for (Ipp32s i = 0; i < mergeInfo.numCand; i++) {
            T_RefIdx *ref_idx = &(mergeInfo.refIdx[2*i]);
            if (cslice->slice_type != B_SLICE)
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
                H265Frame *PicYUVRefF = cslice->GetRefFrame(REF_PIC_LIST_0, ref_idx[0]);
                H265Frame *PicYUVRefB = cslice->GetRefFrame(REF_PIC_LIST_1, ref_idx[1]);
                if (PicYUVRefF && PicYUVRefB)
                    cost_temp = MatchingMetricBipred_PU(pSrc, &me_info, PicYUVRefF->y, PicYUVRefF->pitch_luma, PicYUVRefB->y, PicYUVRefB->pitch_luma, mv);
                else
                    cost_temp = COST_MAX;
            } else {
                EnumRefPicList dir = inter_dir == INTER_DIR_PRED_L0 ? REF_PIC_LIST_0 : REF_PIC_LIST_1;
                H265Frame *PicYUVRef = cslice->GetRefFrame(dir, ref_idx[dir]);
                cost_temp = MatchingMetric_PU(pSrc, &me_info, mv+dir, PicYUVRef); // approx MVcost
            }

            if (cost_best > cost_temp) {
                cost_best = cost_temp;
                cand_best = i;
                inter_dir_best = inter_dir;
            }
        }
        for (Ipp32s dir = 0; dir < 2; dir++) {
            data[absPartIdx].mv[dir] = mergeInfo.mvCand[2*cand_best+dir];
            data[absPartIdx].refIdx[dir] = mergeInfo.refIdx[2*cand_best+dir];
        }
        data[absPartIdx].mergeIdx = (Ipp8u)cand_best;
        data[absPartIdx].interDir = inter_dir_best;
        InterPredCU(absPartIdx, depth, y_rec, pitch_rec, 1);
        PixType *pRec = y_rec + ((PUStartRow * pitch_rec + PUStartColumn) << par->Log2MinTUSize);
        PixType *pSrc = y_src + ((PUStartRow * pitch_src + PUStartColumn) << par->Log2MinTUSize);
        cost_best = tu_sse(pSrc, pRec, pitch_src, pitch_rec, size);
        // add chroma with little weight
        InterPredCU(absPartIdx, depth, uv_rec, pitch_rec, 0);
        pRec = uv_rec + (((PUStartRow>>1) * pitch_rec + PUStartColumn) << par->Log2MinTUSize);
        pSrc = uv_src + (((PUStartRow>>1) * pitch_src + PUStartColumn) << par->Log2MinTUSize);
        cost_best += tu_sse(pSrc, pRec, pitch_src, pitch_rec, size>>1)/4;
        pRec += size>>1;
        pSrc += size>>1;
        cost_best += tu_sse(pSrc, pRec, pitch_src, pitch_rec, size>>1)/4;
    }

    if (cost_best < COST_MAX) {
        for (Ipp32u i = 1; i < num_parts; i++)
            data[absPartIdx+i] = data[absPartIdx];
    }

    return cost_best;
}

// MV functions
Ipp32s operator == (const H265MV &mv1, const H265MV &mv2)
{ return mv1.mvx == mv2.mvx && mv1.mvy == mv2.mvy; }

Ipp32s operator != (const H265MV &mv1, const H265MV &mv2)
{ return !(mv1.mvx == mv2.mvx && mv1.mvy == mv2.mvy); }

Ipp32s qdiff(const H265MV *mv1, const H265MV *mv2)
{
    H265MV mvdiff = {mv1->mvx - mv2->mvx, mv1->mvy - mv2->mvy};
    return qcost(&mvdiff);
}

Ipp32s qcost(const H265MV *mv)
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


} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
