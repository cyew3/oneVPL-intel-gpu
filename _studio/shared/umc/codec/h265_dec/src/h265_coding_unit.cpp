/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder_mt.h"
#include "h265_coding_unit.h"
#include "umc_h265_frame.h"
#include "umc_h265_frame_info.h"
#include "h265_global_rom.h"

#include <cstdarg>

namespace UMC_HEVC_DECODER
{

// Constructor, destructor, create, destroy -------------------------------------------------------------
H265CodingUnit::H265CodingUnit()
{
    m_Frame = 0;
    m_SliceHeader = 0;
    m_SliceIdx = -1;

    m_lumaIntraDir = 0;
    m_chromaIntraDir = 0;
    // ML: FIXED: changed the last 3 into 2

    m_cbf[0] = 0;
    m_cbf[1] = 0;
    m_cbf[2] = 0;
}

H265CodingUnit::~H265CodingUnit()
{
}

void H265CodingUnit::create (H265FrameCodingData * frameCD, Ipp32s cuAddr)
{
    m_Frame = NULL;
    m_SliceHeader = NULL;
    m_NumPartition = frameCD->m_NumPartitions;

    m_cuData = &frameCD->m_cuData[m_NumPartition*cuAddr];

    m_rasterToPelX = frameCD->m_partitionInfo.m_rasterToPelX;
    m_rasterToPelY = frameCD->m_partitionInfo.m_rasterToPelY;

    m_zscanToRaster = frameCD->m_partitionInfo.m_zscanToRaster;
    m_rasterToZscan = frameCD->m_partitionInfo.m_rasterToZscan;
}

void H265CodingUnit::destroy()
{
    m_Frame = NULL;
    m_SliceHeader = NULL;
}


// Public member functions --------------------------------------------------------------------------------------
// Initialization -----------------------------------------------------------------------------------------------
/*
 - initialize top-level CU
 - internal buffers are already created
 - set values before encoding a CU
 \param  pcPic     picture (TComPic) class pointer
 \param  iCUAddr   CU address
 */
void H265CodingUnit::initCU(H265SegmentDecoderMultiThreaded* sd, Ipp32u iCUAddr)
{
    const H265SeqParamSet* sps = sd->m_pSeqParamSet;
    H265DecoderFrame *Pic = sd->m_pCurrentFrame;

    m_Frame = Pic;
    m_SliceIdx = sd->m_pSlice->GetSliceNum();
    m_SliceHeader = sd->m_pSliceHeader;
    CUAddr = iCUAddr;
    m_CUPelX = (iCUAddr % sps->WidthInCU) * sps->MaxCUSize;
    m_CUPelY = (iCUAddr / sps->WidthInCU) * sps->MaxCUSize;
    m_NumPartition = sps->NumPartitionsInCU;

    memset(m_cuData, 0, m_NumPartition * sizeof(H265CodingUnitData));
}

void H265CodingUnit::setOutsideCUPart(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp8u Width = (Ipp8u) (m_Frame->getCD()->m_MaxCUWidth >> Depth);

    m_cuData[AbsPartIdx].depth = (Ipp8u)Depth;
    m_cuData[AbsPartIdx].width = (Ipp8u)Width;
    m_cuData[AbsPartIdx].predMode = MODE_NONE;
    SetCUDataSubParts(AbsPartIdx, Depth);

}

Ipp32s H265CodingUnit::getLastValidPartIdx(Ipp32s AbsPartIdx)
{
    Ipp32s LastValidPartIdx = AbsPartIdx - 1;
    while (LastValidPartIdx >= 0
        && GetPredictionMode(LastValidPartIdx) == MODE_NONE)
    {
        Ipp32u Depth = GetDepth(LastValidPartIdx);
        LastValidPartIdx -= m_NumPartition >> (Depth << 1);
    }
    return LastValidPartIdx;
}

/** Check whether the CU is coded in lossless coding mode
 * \param   uiAbsPartIdx
 * \returns true if the CU is coded in lossless coding mode; false if otherwise
 */
bool H265CodingUnit::isLosslessCoded(Ipp32u absPartIdx)
{
  return (m_SliceHeader->m_PicParamSet->transquant_bypass_enabled_flag && GetCUTransquantBypass(absPartIdx));
}

/** Get allowed chroma intra modes
*\param   uiAbsPartIdx
*\param   uiModeList  pointer to chroma intra modes array
*\returns
*- fill uiModeList with chroma intra modes
*/
void H265CodingUnit::getAllowedChromaDir(Ipp32u AbsPartIdx, Ipp32u* ModeList)
{
    ModeList[0] = INTRA_LUMA_PLANAR_IDX;
    ModeList[1] = INTRA_LUMA_VER_IDX;
    ModeList[2] = INTRA_LUMA_HOR_IDX;
    ModeList[3] = INTRA_LUMA_DC_IDX;
    ModeList[4] = INTRA_DM_CHROMA_IDX;

    Ipp32u LumaMode = GetLumaIntra(AbsPartIdx);

    for (Ipp32s i = 0; i < INTRA_NUM_CHROMA_MODE - 1; i++)
    {
        if (LumaMode == ModeList[i])
        {
            ModeList[i] = 34; // VER+8 mode
            break;
        }
    }
}

Ipp32u H265CodingUnit::getQuadtreeTULog2MinSizeInCU(Ipp32u Idx)
{
    Ipp32u Log2CbSize = g_ConvertToBit[GetWidth(Idx)] + 2;
    EnumPartSize partSize = GetPartitionSize(Idx);
    const H265SeqParamSet *sps = m_SliceHeader->m_SeqParamSet;
    Ipp32u QuadtreeTUMaxDepth = GetPredictionMode(Idx) == MODE_INTRA ? sps->max_transform_hierarchy_depth_intra : sps->max_transform_hierarchy_depth_inter;
    Ipp32s IntraSplitFlag = GetPredictionMode(Idx) == MODE_INTRA && partSize == PART_SIZE_NxN;
    Ipp32s InterSplitFlag = GetPredictionMode(Idx) == MODE_INTER && sps->max_transform_hierarchy_depth_inter == 0 && partSize != PART_SIZE_2Nx2N;

    Ipp32u Log2MinTUSizeInCU = 0;

    if (Log2CbSize < sps->log2_min_transform_block_size + QuadtreeTUMaxDepth + InterSplitFlag + IntraSplitFlag)
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is < log2_min_transform_block_size
        Log2MinTUSizeInCU = sps->log2_min_transform_block_size;
    }
    else
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still >= log2_min_transform_block_size
        Log2MinTUSizeInCU = Log2CbSize - (QuadtreeTUMaxDepth + InterSplitFlag + IntraSplitFlag);

        if (Log2MinTUSizeInCU > sps->log2_max_transform_block_size)
            // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still > log2_max_transform_block_size
            Log2MinTUSizeInCU = sps->log2_max_transform_block_size;
    }

    return Log2MinTUSizeInCU;
}

void H265CodingUnit::setCbfSubParts(Ipp32u CbfY, Ipp32u CbfU, Ipp32u CbfV, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_NumPartition >> (Depth << 1);
    memset(m_cbf[0] + AbsPartIdx, CbfY, sizeof(Ipp8u) * CurrPartNumb);
    memset(m_cbf[1] + AbsPartIdx, CbfU, sizeof(Ipp8u) * CurrPartNumb);
    memset(m_cbf[2] + AbsPartIdx, CbfV, sizeof(Ipp8u) * CurrPartNumb);
}

void H265CodingUnit::setCbfSubParts(Ipp32u uCbf, ComponentPlane plane, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_NumPartition >> (Depth << 1);
    memset(m_cbf[plane] + AbsPartIdx, uCbf, sizeof(Ipp8u) * CurrPartNumb);
}

void H265CodingUnit::setDepth(Ipp32u Depth, Ipp32u AbsPartIdx)
{
    m_cuData[AbsPartIdx].depth = (Ipp8u)Depth;
}

void H265CodingUnit::setPartSizeSubParts(EnumPartSize Mode, Ipp32u AbsPartIdx)
{
    m_cuData[AbsPartIdx].partSize = (Ipp8u)Mode;
}

void H265CodingUnit::setCUTransquantBypass(bool flag, Ipp32u AbsPartIdx)
{
    m_cuData[AbsPartIdx].cu_transform_bypass = flag;
}

void H265CodingUnit::setPredMode(EnumPredMode Mode, Ipp32u AbsPartIdx)
{
    m_cuData[AbsPartIdx].predMode = (Ipp8u)Mode;
}

void H265CodingUnit::SetCUDataSubParts(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_NumPartition >> (Depth << 1);
    for (Ipp32u i = 1; i < CurrPartNumb; i++)
    {
        m_cuData[AbsPartIdx + i] = m_cuData[AbsPartIdx];
    }
}

void H265CodingUnit::setTransformSkip(Ipp32u useTransformSkip, ComponentPlane plane, Ipp32u AbsPartIdx)
{
    m_cuData[AbsPartIdx].transform_skip &= ~(1 << plane);
    m_cuData[AbsPartIdx].transform_skip |= useTransformSkip << plane;
}

void H265CodingUnit::setLumaIntraDirSubParts(Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_NumPartition >> (Depth << 1);

    memset(m_lumaIntraDir + AbsPartIdx, Dir, sizeof(Ipp8u) * CurrPartNumb);
}

void H265CodingUnit::setChromIntraDirSubParts(Ipp32u uDir, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uCurrPartNumb = m_NumPartition >> (Depth << 1);

    memset(m_chromaIntraDir + AbsPartIdx, uDir, sizeof(Ipp8u) * uCurrPartNumb);
}

void H265CodingUnit::setTrIdx(Ipp32u uTrIdx, Ipp32u AbsPartIdx, Ipp32s Depth)
{
    Ipp32u uCurrPartNumb = m_NumPartition >> (Depth << 1);
    m_cuData[AbsPartIdx].trIndex = (Ipp8u)uTrIdx;

    for (Ipp32u i = 1; i < uCurrPartNumb; i++)
    {
        m_cuData[AbsPartIdx + i].trIndex = (Ipp8u)uTrIdx;
    }
}

void H265CodingUnit::UpdateTUInfo (Ipp32u AbsPartIdx, Ipp32u uTrIdx, Ipp32s Depth)
{
    Ipp32u uCurrPartNumb = m_NumPartition >> (Depth << 1);

    for (Ipp32u i = 0; i < uCurrPartNumb; i++)
    {
        m_cuData[AbsPartIdx + i].trIndex = (Ipp8u)uTrIdx;
    }
}

void H265CodingUnit::UpdateTUQpInfo (Ipp32u AbsPartIdx, Ipp32s qp, Ipp32s Depth)
{
    Ipp32u uCurrPartNumb = m_NumPartition >> (Depth << 1);

    for (Ipp32u i = 0; i < uCurrPartNumb; i++)
    {
        m_cuData[AbsPartIdx + i].qp = (Ipp8s)qp;
    }
}

void H265CodingUnit::setSize(Ipp32u Width, Ipp32u AbsPartIdx)
{
    m_cuData[AbsPartIdx].width = (Ipp8u)Width;
}

Ipp8u H265CodingUnit::getNumPartInter(Ipp32u AbsPartIdx)
{
    Ipp8u iNumPart = 0;

    // ML: OPT: TODO: Change switch to table lookup instead
    switch (GetPartitionSize(AbsPartIdx))
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

void H265CodingUnit::getPartIndexAndSize(Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u uPartIdx, Ipp32u &Width, Ipp32u &Height)
{
    Ipp32u cuWidth = GetWidth(AbsPartIdx);

    switch (GetPartitionSize(AbsPartIdx))
    {
        case PART_SIZE_2NxN:
            Width = cuWidth;
            Height = cuWidth >> 1;
            break;
        case PART_SIZE_Nx2N:
            Width = cuWidth >> 1;
            Height = cuWidth;
            break;
        case PART_SIZE_NxN:
            Width = cuWidth >> 1;
            Height = cuWidth >> 1;
            break;
        case PART_SIZE_2NxnU:
            Width    = cuWidth;
            Height   = (uPartIdx == 0) ? cuWidth >> 2 : (cuWidth >> 2) + (cuWidth >> 1);
            break;
        case PART_SIZE_2NxnD:
            Width    = cuWidth;
            Height   = (uPartIdx == 0) ?  (cuWidth >> 2) + (cuWidth>> 1) : cuWidth >> 2;
            break;
        case PART_SIZE_nLx2N:
            Width    = (uPartIdx == 0) ? cuWidth >> 2 : (cuWidth >> 2) + (cuWidth >> 1);
            Height   = cuWidth;
            break;
        case PART_SIZE_nRx2N:
            Width    = (uPartIdx == 0) ? (cuWidth >> 2) + (cuWidth >> 1) : cuWidth >> 2;
            Height   = cuWidth;
            break;
        default:
            VM_ASSERT(GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N);
            Width = cuWidth;
            Height = cuWidth;
            break;
    }
}

void H265CodingUnit::getPartSize(Ipp32u AbsPartIdx, Ipp32u partIdx, Ipp32s &nPSW, Ipp32s &nPSH)
{
    Ipp32u cuWidth = GetWidth(AbsPartIdx);

    switch (GetPartitionSize(AbsPartIdx))
    {
    case PART_SIZE_2NxN:
        nPSW = cuWidth;
        nPSH = cuWidth >> 1;
        break;
    case PART_SIZE_Nx2N:
        nPSW = cuWidth >> 1;
        nPSH = cuWidth;
        break;
    case PART_SIZE_NxN:
        nPSW = cuWidth >> 1;
        nPSH = cuWidth >> 1;
        break;
    case PART_SIZE_2NxnU:
        nPSW = cuWidth;
        nPSH = (partIdx == 0) ? cuWidth >> 2 : (cuWidth >> 2) + (cuWidth >> 1);
        break;
    case PART_SIZE_2NxnD:
        nPSW = cuWidth;
        nPSH = (partIdx == 0) ?  (cuWidth >> 2) + (cuWidth >> 1) : cuWidth >> 2;
        break;
    case PART_SIZE_nLx2N:
        nPSW = (partIdx == 0) ? cuWidth >> 2 : (cuWidth >> 2) + (cuWidth >> 1);
        nPSH = cuWidth;
        break;
    case PART_SIZE_nRx2N:
        nPSW = (partIdx == 0) ? (cuWidth >> 2) + (cuWidth >> 1) : cuWidth >> 2;
        nPSH = cuWidth;
        break;
    default:
        VM_ASSERT(GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N);
        nPSW = cuWidth;
        nPSH = cuWidth;
        break;
    }
}

/** Set a I_PCM flag for all sub-partitions of a partition.
 * \param bIpcmFlag I_PCM flag
 * \param uiAbsPartIdx patition index
 * \param uiDepth CU depth
 * \returns Void
 */
void H265CodingUnit::setIPCMFlag (bool IpcmFlag, Ipp32u AbsPartIdx)
{
    m_cuData[AbsPartIdx].pcm_flag = IpcmFlag;
}

bool H265CodingUnit::isBipredRestriction(Ipp32u AbsPartIdx, Ipp32u PartIdx)
{
    Ipp32s width = 0;
    Ipp32s height = 0;

    getPartSize(AbsPartIdx, PartIdx, width, height);
    if (GetWidth(AbsPartIdx) == 8 && (width < 8 || height < 8))
    {
        return true;
    }
    return false;
}

Ipp32u H265CodingUnit::getCoefScanIdx(Ipp32u AbsPartIdx, Ipp32u L2Width, bool IsLuma, bool IsIntra)
{
    Ipp32u uiScanIdx = SCAN_DIAG;
    Ipp32u uiDirMode;

    if(IsIntra) {
        if ( IsLuma )
        {
            uiDirMode = GetLumaIntra(AbsPartIdx);
            if (L2Width > 1 && L2Width < 4 )
            {
                uiScanIdx = abs((Ipp32s) uiDirMode - INTRA_LUMA_VER_IDX) < 5 ? SCAN_HOR : (abs((Ipp32s)uiDirMode - INTRA_LUMA_HOR_IDX) < 5 ? SCAN_VER : SCAN_DIAG);
            }
        }
        else
        {
            uiDirMode = GetChromaIntra(AbsPartIdx);
            if (uiDirMode == INTRA_DM_CHROMA_IDX)
            {
                Ipp32u depth = GetDepth(AbsPartIdx);
                Ipp32u numParts = m_NumPartition >> (depth << 1);
                uiDirMode = GetLumaIntra((AbsPartIdx / numParts) * numParts);
            }
            if (L2Width < 3)
            {
                uiScanIdx = abs((Ipp32s)uiDirMode - INTRA_LUMA_VER_IDX) < 5 ? SCAN_HOR : (abs((Ipp32s)uiDirMode - INTRA_LUMA_HOR_IDX) < 5 ? SCAN_VER : SCAN_DIAG);
            }
        }
    }

    return uiScanIdx;
}

Ipp32u H265CodingUnit::getSCUAddr()
{
    return m_Frame->m_CodingData->GetInverseCUOrderMap(CUAddr) * (1 << (m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1));
}

#define SET_BORDER_AVAILABILITY(borderIndex, noPicBoundary, refCUAddr)                         \
{                                                                                              \
    if (noPicBoundary)                                                                         \
    {                                                                                          \
        if (m_Frame->GetAU()->GetSliceCount() == 1)                                            \
        {                                                                                      \
            m_AvailBorder.data |= 1 << borderIndex;                                            \
        }                                                                                      \
        else                                                                                   \
        {                                                                                      \
            H265CodingUnit* refCU = m_Frame->getCU(refCUAddr);                                 \
            Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;                 \
            Ipp32u          SA = m_SliceHeader->SliceCurStartCUAddr;                           \
                                                                                               \
            Ipp8u borderFlag = (refSA == SA) ? 1 : ((refSA > SA) ?                             \
                refCU->m_SliceHeader->slice_loop_filter_across_slices_enabled_flag :           \
                m_SliceHeader->slice_loop_filter_across_slices_enabled_flag);                  \
            m_AvailBorder.data |= borderFlag << borderIndex;                                   \
        }                                                                                      \
    }                                                                                          \
}

void H265CodingUnit::setNDBFilterBlockBorderAvailability(bool independentTileBoundaryEnabled)
{
    bool noPicLBoundary = ((m_CUPelX == 0) ? false : true);
    bool noPicTBoundary = ((m_CUPelY == 0) ? false : true);
    bool noPicRBoundary = ((m_CUPelX + m_SliceHeader->m_SeqParamSet->MaxCUSize >= m_SliceHeader->m_SeqParamSet->pic_width_in_luma_samples)  ? false : true);
    bool noPicBBoundary = ((m_CUPelY + m_SliceHeader->m_SeqParamSet->MaxCUSize >= m_SliceHeader->m_SeqParamSet->pic_height_in_luma_samples) ? false : true);
    Ipp32s numLCUInPicWidth = m_Frame->m_CodingData->m_WidthInCU;
    Ipp32u tileID = m_Frame->m_CodingData->getTileIdxMap(CUAddr);

    m_AvailBorder.data = 0;
    SET_BORDER_AVAILABILITY(SGU_L, noPicLBoundary, CUAddr - 1);
    SET_BORDER_AVAILABILITY(SGU_R, noPicRBoundary, CUAddr + 1);
    SET_BORDER_AVAILABILITY(SGU_T, noPicTBoundary, CUAddr - numLCUInPicWidth);
    SET_BORDER_AVAILABILITY(SGU_B, noPicBBoundary, CUAddr + numLCUInPicWidth);
    SET_BORDER_AVAILABILITY(SGU_TL, noPicTBoundary && noPicLBoundary, CUAddr - numLCUInPicWidth - 1);
    SET_BORDER_AVAILABILITY(SGU_TR, noPicTBoundary && noPicRBoundary, CUAddr - numLCUInPicWidth + 1);
    SET_BORDER_AVAILABILITY(SGU_BL, noPicBBoundary && noPicLBoundary, CUAddr + numLCUInPicWidth - 1);
    SET_BORDER_AVAILABILITY(SGU_BR, noPicBBoundary && noPicRBoundary, CUAddr + numLCUInPicWidth + 1);

    if (independentTileBoundaryEnabled)
    {
        if (noPicLBoundary && m_Frame->m_CodingData->getTileIdxMap(CUAddr - 1) != tileID)
        {
            m_AvailBorder.data &= ~((1 << SGU_L) | (1 << SGU_TL) | (1 << SGU_BL));
        }

        if (noPicRBoundary && m_Frame->m_CodingData->getTileIdxMap(CUAddr + 1) != tileID)
        {
            m_AvailBorder.data &= ~((1 << SGU_R) | (1 << SGU_TR) | (1 << SGU_BR));
        }

        if (noPicTBoundary && m_Frame->m_CodingData->getTileIdxMap(CUAddr - numLCUInPicWidth) !=  tileID)
        {
            m_AvailBorder.data &= ~((1 << SGU_T) | (1 << SGU_TL) | (1 << SGU_TR));
        }

        if (noPicBBoundary && m_Frame->m_CodingData->getTileIdxMap(CUAddr + numLCUInPicWidth) !=  tileID)
        {
            m_AvailBorder.data &= ~((1 << SGU_B) | (1 << SGU_BL) | (1 << SGU_BR));
        }
    }
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
