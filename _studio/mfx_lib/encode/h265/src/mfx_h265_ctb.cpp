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
#include <math.h>
#include <assert.h>
#include "ippi.h"
#include "ippvc.h"
//#include "ippdc.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"

namespace H265Enc {

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

static bool IsCandFound(const Ipp8s *curRefIdx, const H265MV *curMV, const MvPredInfo<5> *mergeInfo,
                        Ipp32s candIdx, Ipp32s numRefLists);

static Ipp32s TuSse(const Ipp8u *src, Ipp32s pitchSrc,
                    const Ipp8u *rec, Ipp32s pitchRec,
                    Ipp32s width, Ipp32s height, Ipp32s shift)
{
    return h265_Sse(src, pitchSrc, rec, pitchRec, width, height, shift);
}

static Ipp32s TuSse(const Ipp16u *src, Ipp32s pitchSrc,
                    const Ipp16u *rec, Ipp32s pitchRec,
                    Ipp32s width, Ipp32s height, Ipp32s shift)
{
    return h265_Sse(src, pitchSrc, rec, pitchRec, width, height, shift);
#if 0
    // Sq Err is defined on truncated Sqr Pixel Diff for uniform results across blocks sizes.
    // Shifting after Sqr Pixel Diff accumulation causes,
    // truncation error to integrate for larger block sizes causing mismatch and large loss in bdrate. (see 10b WP Test set Tu1)
    // return h265_Sse(src, pitchSrc, rec, pitchRec, width, height)>>shift;
    // Disable Opt
    Ipp32s s = 0;
    for (Ipp32s j = 0; j < height; j++) {
        for (Ipp32s i = 0; i < width; i++) {
            s += (((CoeffsType)src[i] - rec[i]) * (Ipp32s)((CoeffsType)src[i] - rec[i])) >> shift;
        }
        src += pitchSrc;
        rec += pitchRec;
    }
    return s;
#endif
}

static inline int h265_SAD_MxN_special(const Ipp8u *image,  const Ipp8u *ref, int stride, int SizeX, int SizeY)
{
    return MFX_HEVC_PP::h265_SAD_MxN_special_8u(image, ref, stride, SizeX, SizeY);
}

template <class PixType>
Ipp32s h265_SAD_MxN_general(const PixType *image, Ipp32s stride_img, const PixType *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY);

template <>
Ipp32s h265_SAD_MxN_general<Ipp8u>(const Ipp8u *image, Ipp32s stride_img, const Ipp8u *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY)
{
    return MFX_HEVC_PP::h265_SAD_MxN_general_8u(ref, stride_ref, image, stride_img, SizeX, SizeY);
}

template <>
Ipp32s h265_SAD_MxN_general<Ipp16u>(const Ipp16u *image, Ipp32s stride_img, const Ipp16u *ref, Ipp32s stride_ref, Ipp32s SizeX, Ipp32s SizeY)
{
    return MFX_HEVC_PP::NAME(h265_SAD_MxN_general_16s)((Ipp16s*)ref, stride_ref, (Ipp16s*)image, stride_img, SizeX, SizeY);
}

static inline int h265_SAD_MxN_special(const Ipp16u *image,  const Ipp16u *ref, int stride, int SizeX, int SizeY)
{
    return MFX_HEVC_PP::NAME(h265_SAD_MxN_16s)((Ipp16s*)image, stride, (Ipp16s*)ref, SizeX, SizeY);
}

template <typename PixType>
static void TuDiff(CoeffsType *residual, Ipp32s pitchDst, const PixType *src, Ipp32s pitchSrc,
                   const PixType *pred, Ipp32s pitchPred, Ipp32s size);

template <typename PixType>
Ipp32s tuHad_Kernel(const PixType *src, Ipp32s pitchSrc, const PixType *rec, Ipp32s pitchRec,
             Ipp32s width, Ipp32s height);


Ipp32s GetLumaOffset(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch)
{
    Ipp32s puRasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s puStartRow = puRasterIdx >> 4;
    Ipp32s puStartColumn = puRasterIdx & 15;

    return (puStartRow * pitch + puStartColumn) << par->QuadtreeTULog2MinSize;
}

// chroma offset for NV12
Ipp32s GetChromaOffset(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch)
{
    Ipp32s puRasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s puStartRow = puRasterIdx >> 4;
    Ipp32s puStartColumn = puRasterIdx & 15;

    return ((puStartRow * pitch >> par->chromaShiftH) + (puStartColumn << par->chromaShiftWInv)) << par->QuadtreeTULog2MinSize;
}

Ipp32s GetChromaOffset1(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch)
{
    Ipp32s puRasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s puStartRow = puRasterIdx >> 4;
    Ipp32s puStartColumn = puRasterIdx & 15;

    return ((puStartRow * pitch >> par->chromaShiftH) + (puStartColumn >> par->chromaShiftW)) << par->QuadtreeTULog2MinSize;
}

// propagate data[0] thru data[1..numParts-1] where numParts=2^n
void PropagateSubPart(H265CUData *data, Ipp32s numParts)
{
    VM_ASSERT((numParts & (numParts - 1)) == 0); // check that numParts=2^n
    for (Ipp32s i = 1; i < numParts; i <<= 1)
        small_memcpy(data + i, data, i * sizeof(H265CUData));
}

template <class T>
bool IsZero(const T *arr, Ipp32s size)
{
    VM_ASSERT(reinterpret_cast<Ipp64u>(arr) % sizeof(Ipp64u) == 0);
    VM_ASSERT(size * sizeof(T) % sizeof(Ipp64u) == 0);

    const Ipp64u *arr64 = reinterpret_cast<const Ipp64u *>(arr);
    const Ipp64u *arr64end = reinterpret_cast<const Ipp64u *>(arr + size);

    for (; arr64 != arr64end; arr64++)
        if (*arr64)
            return false;
    return true;
}

Ipp32s IsZeroCbf(const H265CUData *data, Ipp32s idx422)
{
    Ipp8u cbf = data->cbf[0] | data->cbf[1] | data->cbf[2];
    if (idx422) {
        cbf |= (data+idx422)->cbf[1] | (data+idx422)->cbf[2];
    }
    return cbf == 0;
}

template <typename PixType>
void H265CU<PixType>::GetPartOffsetAndSize(Ipp32s idx,
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


void GetPartAddr(Ipp32s partIdx,
                 Ipp32s partMode,
                 Ipp32s numPartition,
                 Ipp32s& partAddr)
{
    if (partIdx == 0) {
        partAddr = 0;
        return;
    }
    switch (partMode)
    {
    case PART_SIZE_2NxN:
        partAddr = numPartition >> 1;
        break;
    case PART_SIZE_Nx2N:
        partAddr = numPartition >> 2;
        break;
    case PART_SIZE_NxN:
        partAddr = (numPartition >> 2) * partIdx;
        break;
    case PART_SIZE_2NxnU:
        partAddr = numPartition >> 3;
        break;
    case PART_SIZE_2NxnD:
        partAddr = (numPartition >> 1) + (numPartition >> 3);
        break;
    case PART_SIZE_nLx2N:
        partAddr = numPartition >> 4;
        break;
    case PART_SIZE_nRx2N:
        partAddr = (numPartition >> 2) + (numPartition >> 4);
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

template <typename PixType>
Ipp8u H265CU<PixType>::GetQtRootCbf(Ipp32u idx)
{ 
    Ipp8u cbf = m_data[idx].cbf[0] | m_data[idx].cbf[1] | m_data[idx].cbf[2];
    //Ipp8u cbf = GetCbf( idx, TEXT_LUMA, 0 ) || GetCbf( idx, TEXT_CHROMA_U, 0 ) || GetCbf( idx, TEXT_CHROMA_V, 0 );
    if (m_par->chroma422) {
        Ipp32s idx_offset = m_par->NumPartInCU >> ((m_data[idx].depth<<1) + 1);
        cbf |= m_data[idx+idx_offset].cbf[1];
        cbf |= m_data[idx+idx_offset].cbf[2];
        //cbf |= GetCbf( idx + idx_offset, TEXT_CHROMA_U, 0 ) || GetCbf( idx + idx_offset, TEXT_CHROMA_V, 0 );
    }
    return cbf & 1;
}

template <typename PixType>
Ipp32u H265CU<PixType>::GetCtxSkipFlag(Ipp32s absPartIdx)
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

template <typename PixType>
Ipp32u H265CU<PixType>::GetCtxSplitFlag(Ipp32s absPartIdx, Ipp32u depth)
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

template <typename PixType>
Ipp8s H265CU<PixType>::GetRefQp( Ipp32u currAbsIdxInLcu )
{
    Ipp32u        lPartIdx, aPartIdx;
    H265CUData* cULeft  = GetQpMinCuLeft ( lPartIdx, m_absIdxInLcu + currAbsIdxInLcu );
    H265CUData* cUAbove = GetQpMinCuAbove( aPartIdx, m_absIdxInLcu + currAbsIdxInLcu );
    return (((cULeft? cULeft[lPartIdx].qp: GetLastCodedQP( this, currAbsIdxInLcu )) + (cUAbove? cUAbove[aPartIdx].qp: GetLastCodedQP( this, currAbsIdxInLcu )) + 1) >> 1);
}

template <typename PixType>
void H265CU<PixType>::SetQpSubCUs(int qp, int absPartIdx, int depth, bool &foundNonZeroCbf)
{

    if (!foundNonZeroCbf)
    {
        if(m_data[absPartIdx].depth > depth)
        {
            Ipp32u curPartNumb = m_par->NumPartInCU >> (depth << 1);
            Ipp32u curPartNumQ = curPartNumb >> 2;
            for (Ipp32u partUnitIdx = 0; partUnitIdx < 4; partUnitIdx++)
            {
                SetQpSubCUs(qp, absPartIdx + partUnitIdx * curPartNumQ, depth + 1, foundNonZeroCbf);
            }
        }
        else
        {
            Ipp32u numParts = m_par->NumPartInCU >> (depth << 1);
            if(m_data[absPartIdx].cbf[0] 
            || m_data[absPartIdx].cbf[1] || m_data[absPartIdx].cbf[2]
            || (m_par->chroma422 && (m_data[absPartIdx+(numParts>>1)].cbf[1] || m_data[absPartIdx+(numParts>>1)].cbf[2]))
                )
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

template <typename PixType>
void H265CU<PixType>::SetQpSubParts(int qp, int absPartIdx, int depth)
{
    int partNum = m_par->NumPartInCU >> (depth << 1);

    for (int idx = absPartIdx; idx < absPartIdx + partNum; idx++)
    {
        m_data[idx].qp = qp;
    }
}


template <typename PixType>
void H265CU<PixType>::CheckDeltaQp(void)
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

template <typename PixType>
Ipp32u H265CU<PixType>::GetCoefScanIdx(Ipp32s absPartIdx, Ipp32u log2Width, Ipp32s isLuma, Ipp32s isIntra) const
{
    Ipp32s ctxIdx;
    Ipp32u scanIdx;
    Ipp32u dirMode;

    if (!isIntra) {
        scanIdx = COEFF_SCAN_ZIGZAG;
        return scanIdx;
    }

    ctxIdx = 7 - log2Width; // 2(4)->5 and 6(64)->1; so 1 <= ctxIdx <= 5

    //switch(width)
    //{
    //case  2: ctxIdx = 6; break;
    //case  4: ctxIdx = 5; break;
    //case  8: ctxIdx = 4; break;
    //case 16: ctxIdx = 3; break;
    //case 32: ctxIdx = 2; break;
    //case 64: ctxIdx = 1; break;
    //default: ctxIdx = 0; break;
    //}

    if (isLuma) {
        dirMode = m_data[absPartIdx].intraLumaDir;
        scanIdx = COEFF_SCAN_ZIGZAG;
        if (ctxIdx >3 /*&& ctxIdx < 6*/) //if multiple scans supported for transform size
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
            Ipp32s mask = (-1) << (2 * (m_par->MaxCUDepth-depth));

            // get luma mode from upper-left corner of current CU
            dirMode = m_data[absPartIdx & mask].intraLumaDir;
        }

        if (m_par->chroma422)
        {
            dirMode = h265_chroma422IntraAngleMappingTable[dirMode];
        }

        scanIdx = COEFF_SCAN_ZIGZAG;
        if (ctxIdx > 4 /*&& ctxIdx < 7*/) //if multiple scans supported for transform size
        {
            scanIdx = abs((Ipp32s) dirMode - INTRA_VER) < 5 ? 1 : (abs((Ipp32s)dirMode - INTRA_HOR) < 5 ? 2 : 0);
        }
    }

    return scanIdx;
}


Ipp32u GetQuadtreeTuLog2MinSizeInCu(const H265VideoParam *par, Ipp32u log2CbSize, Ipp8u partSize, Ipp8u pred_mode)
{
    Ipp32u SplitFlag;
    Ipp32u quadtreeTUMaxDepth;
    if (pred_mode == MODE_INTRA) {
        quadtreeTUMaxDepth = par->QuadtreeTUMaxDepthIntra;
        SplitFlag = ( partSize == PART_SIZE_NxN ) ? 1 : 0;
    }
    else {
        quadtreeTUMaxDepth = par->QuadtreeTUMaxDepthInter;
        SplitFlag = ( partSize != PART_SIZE_2Nx2N && quadtreeTUMaxDepth == 1 ) ? 1 : 0;
    }

    Ipp32u log2MinTUSizeInCU = 0;
    if (log2CbSize < par->QuadtreeTULog2MinSize + (quadtreeTUMaxDepth - 1 + SplitFlag) )
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is < QuadtreeTULog2MinSize
        log2MinTUSizeInCU = par->QuadtreeTULog2MinSize;
    }
    else
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still >= QuadtreeTULog2MinSize
        log2MinTUSizeInCU = log2CbSize - (quadtreeTUMaxDepth - 1 + SplitFlag); // stop when trafoDepth == hierarchy_depth = splitFlag
        if (log2MinTUSizeInCU > par->QuadtreeTULog2MaxSize)
        {
            // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still > QuadtreeTULog2MaxSize
            log2MinTUSizeInCU = par->QuadtreeTULog2MaxSize;
        }
    }
    return log2MinTUSizeInCU;
}

void GetTrDepthMinMax(const H265VideoParam *par, Ipp32s depth, Ipp32s partMode,
                      Ipp8u *trDepthMin, Ipp8u *trDepthMax)
{
    Ipp8u trDepth = (par->QuadtreeTUMaxDepthInter == 1) && (partMode != PART_SIZE_2Nx2N);
    Ipp32u tuLog2MinSize = GetQuadtreeTuLog2MinSizeInCu(par, par->Log2MaxCUSize - depth, (Ipp8u)partMode, MODE_INTER);
    if ((int)par->Log2MaxCUSize - depth - 5 > trDepth) trDepth = (int)par->Log2MaxCUSize - depth - 5;
    int parmin1, parmin2;
    parmin1 = IPP_MIN((int)par->QuadtreeTUMaxDepthInter - 1 + (par->QuadtreeTUMaxDepthInter == 1),
        (int)par->Log2MaxCUSize - depth - 2);
    parmin1 = IPP_MIN(parmin1, (int)par->Log2MaxCUSize - depth - (int)tuLog2MinSize);
    parmin2 = IPP_MIN(parmin1, (int)par->Log2MaxCUSize - depth - (int)par->QuadtreeTULog2MaxSize);
    trDepth = IPP_MAX(trDepth, parmin2);
    *trDepthMin = trDepth;
    trDepth = IPP_MAX(trDepth, parmin1);
    *trDepthMax = trDepth;
}

Ipp8u GetTrDepthMinIntra(const H265VideoParam *par, Ipp32s depth)
{
    Ipp8u trDepth = 0;
    if((int)par->Log2MaxCUSize - depth - (int)par->QuadtreeTULog2MaxSize > trDepth) 
        trDepth = (int)par->Log2MaxCUSize - depth - (int)par->QuadtreeTULog2MaxSize;
    return trDepth;
}

template <typename PixType>
Ipp32s H265CU<PixType>::GetIntradirLumaPred(Ipp32s absPartIdx, Ipp8u* intraDirPred)
{
    H265CUPtr tempCU;
    Ipp8u leftIntraDir, aboveIntraDir;
    Ipp32s iModes = 3; //for encoder side only //kolya

    // Get intra direction of left PU
    GetPuLeft(&tempCU, m_absIdxInLcu + absPartIdx );

    leftIntraDir  = tempCU.ctbData ? ( IS_INTRA(tempCU.ctbData, tempCU.absPartIdx ) ? tempCU.ctbData[tempCU.absPartIdx].intraLumaDir : INTRA_DC ) : INTRA_DC;

    // Get intra direction of above PU
    GetPuAbove(&tempCU, m_absIdxInLcu + absPartIdx, true, /*true, false,*/ true );

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


template <typename PixType>
void H265CU<PixType>::GetAllowedChromaDir(Ipp32s absPartIdx, Ipp8u* modeList)
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

//static inline Ipp8u isZeroRow(Ipp32s addr, Ipp32s numUnitsPerRow)
//{
//// addr / numUnitsPerRow == 0
//    return ( addr &~ ( numUnitsPerRow - 1 ) ) == 0;
//}
//
//static inline Ipp8u isEqualRow(Ipp32s addrA, Ipp32s addrB, Ipp32s numUnitsPerRow)
//{
//// addrA / numUnitsPerRow == addrB / numUnitsPerRow
//    return (( addrA ^ addrB ) &~ ( numUnitsPerRow - 1 ) ) == 0;
//}
//
//static inline Ipp8u isZeroCol(Ipp32s addr, Ipp32s numUnitsPerRow)
//{
//// addr % numUnitsPerRow == 0
//    return ( addr & ( numUnitsPerRow - 1 ) ) == 0;
//}
//
//static inline Ipp8u isEqualCol(Ipp32s addrA, Ipp32s addrB, Ipp32s numUnitsPerRow)
//{
//// addrA % numUnitsPerRow == addrB % numUnitsPerRow
//    return (( addrA ^ addrB ) &  ( numUnitsPerRow - 1 ) ) == 0;
//}

template <typename PixType>
void H265CU<PixType>::GetPuAbove(H265CUPtr *cu,
                        Ipp32u currPartUnitIdx,
                        Ipp32s enforceSliceRestriction,
                        Ipp32s planarAtLcuBoundary,
                        Ipp32s enforceTileRestriction
                        )
{
    Ipp32s absPartIdx       = h265_scan_z2r4[currPartUnitIdx]; // raster with PITCH_PU
    Ipp32u absZorderCuIdx   = h265_scan_z2r4[m_absIdxInLcu];
    Ipp32u numPartInCuWidth = m_par->NumPartInCUSize;

    //if ( !isZeroRow( absPartIdx, numPartInCuWidth ) )
    if ( absPartIdx > 15  )
    {
        cu->absPartIdx = h265_scan_r2z4[ absPartIdx - PITCH_TU ];
        cu->ctbData = m_data;
        cu->ctbAddr = m_ctbAddr;
        //if ( !isEqualRow( absPartIdx, absZorderCuIdx, numPartInCuWidth ) )
        if ( (absPartIdx ^ absZorderCuIdx) > 15 )
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

    cu->absPartIdx = h265_scan_r2z4[ absPartIdx + PITCH_TU * (numPartInCuWidth-1) ];

    if ((enforceSliceRestriction && (m_above == NULL || m_aboveAddr < m_cslice->slice_segment_address)) ||
        (enforceTileRestriction && (m_above == NULL || !m_aboveSameTile)))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }
    cu->ctbData = m_above;
    cu->ctbAddr = m_aboveAddr;
}

template <typename PixType>
void H265CU<PixType>::GetPuAboveOf(H265CUPtr *cu,
                                   H265CUPtr *cuBase, 
                                   Ipp32u currPartUnitIdx,
                                   Ipp32s enforceSliceRestriction,
                                   Ipp32s planarAtLcuBoundary,
                                   Ipp32s enforceTileRestriction)
{
    Ipp32s absPartIdx       = h265_scan_z2r4[currPartUnitIdx]; // raster with PITCH_PU
    Ipp32u absZorderCuIdx   = h265_scan_z2r4[m_absIdxInLcu];
    Ipp32u numPartInCuWidth = m_par->NumPartInCUSize;
    Ipp32s widthInCU = m_par->PicWidthInCtbs;

    if ( absPartIdx > 15  )
    {
        cu->absPartIdx = h265_scan_r2z4[ absPartIdx - PITCH_TU ];
        cu->ctbData = cuBase->ctbData;
        cu->ctbAddr = cuBase->ctbAddr;
        if ( (absPartIdx ^ absZorderCuIdx) > 15 )
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

    cu->absPartIdx = h265_scan_r2z4[ absPartIdx + PITCH_TU * (numPartInCuWidth-1) ];

    if (cuBase->ctbAddr < widthInCU)
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->ctbData = cuBase->ctbData - (widthInCU << m_par->Log2NumPartInCU);
    cu->ctbAddr = cuBase->ctbAddr - widthInCU;

    if ((enforceSliceRestriction && (m_par->m_slice_ids[cu->ctbAddr] != m_par->m_slice_ids[cuBase->ctbAddr])) ||
        (enforceTileRestriction && (m_par->m_tile_ids[cu->ctbAddr] != m_par->m_tile_ids[cuBase->ctbAddr])))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }
}

template <typename PixType>
void H265CU<PixType>::GetPuLeft(H265CUPtr *cu,
                       Ipp32u currPartUnitIdx,
                       Ipp32s enforceSliceRestriction,
                       //,Ipp32s /*bEnforceDependentSliceRestriction*/
                       Ipp32s enforceTileRestriction
                       )
{
    Ipp32s absPartIdx       = h265_scan_z2r4[currPartUnitIdx];
    Ipp32u absZorderCUIdx   = h265_scan_z2r4[m_absIdxInLcu];
    Ipp32u numPartInCuWidth = m_par->NumPartInCUSize;

    //if ( !isZeroCol( absPartIdx, numPartInCuWidth ) )
    if ( absPartIdx & 15 )
    {
        cu->absPartIdx = h265_scan_r2z4[ absPartIdx - 1 ];
        cu->ctbData = m_data;
        cu->ctbAddr = m_ctbAddr;
        //if ( !isEqualCol( absPartIdx, absZorderCUIdx, numPartInCuWidth ) )
        if ( (absPartIdx ^ absZorderCUIdx) & 15 )
        {
            cu->absPartIdx -= m_absIdxInLcu;
        }
        return;
    }

    cu->absPartIdx = h265_scan_r2z4[ absPartIdx + numPartInCuWidth - 1 ];

    if ((enforceSliceRestriction && (m_left == NULL || m_leftAddr < m_cslice->slice_segment_address)) ||
        (enforceTileRestriction && (m_left == NULL || !m_leftSameTile)))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->ctbData = m_left;
    cu->ctbAddr = m_leftAddr;
}

template <typename PixType>
void H265CU<PixType>::GetPuLeftOf(H265CUPtr *cu,
                                  H265CUPtr *cuBase, 
                                  Ipp32u currPartUnitIdx,
                                  Ipp32s enforceSliceRestriction,
                                  Ipp32s enforceTileRestriction)
{
    Ipp32s absPartIdx       = h265_scan_z2r4[currPartUnitIdx];
    Ipp32u absZorderCUIdx   = h265_scan_z2r4[m_absIdxInLcu];
    Ipp32u numPartInCuWidth = m_par->NumPartInCUSize;
    Ipp32s widthInCU = m_par->PicWidthInCtbs;

    if ( absPartIdx & 15 )
    {
        cu->absPartIdx = h265_scan_r2z4[ absPartIdx - 1 ];
        cu->ctbData = cuBase->ctbData;
        cu->ctbAddr = cuBase->ctbAddr;
        if ( (absPartIdx ^ absZorderCUIdx) & 15 )
        {
            cu->absPartIdx -= m_absIdxInLcu;
        }
        return;
    }

    cu->absPartIdx = h265_scan_r2z4[ absPartIdx + numPartInCuWidth - 1 ];

    if (!(cuBase->ctbAddr % widthInCU))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->ctbData = cuBase->ctbData - m_par->NumPartInCU;
    cu->ctbAddr = cuBase->ctbAddr - 1;

    if ((enforceSliceRestriction && (m_par->m_slice_ids[cu->ctbAddr] != m_par->m_slice_ids[cuBase->ctbAddr])) ||
        (enforceTileRestriction && (m_par->m_tile_ids[cu->ctbAddr] != m_par->m_tile_ids[cuBase->ctbAddr])))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }
}

template <typename PixType>
void H265CU<PixType>::GetCtuBelow(H265CUPtr *cu,
                        Ipp32s enforceSliceRestriction,
                        Ipp32s enforceTileRestriction
                        )
{
    H265CUData *below = NULL;
    Ipp32s belowAddr = -1, belowSameTile = 0;
    Ipp32u widthInCU = m_par->PicWidthInCtbs;

    if ((m_ctbPelY >> m_par->Log2MaxCUSize) < m_par->PicHeightInCtbs - 1)
    {
        below = m_data + (widthInCU << m_par->Log2NumPartInCU);
        belowAddr = m_ctbAddr + widthInCU;
        if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[belowAddr])
            belowSameTile = 1;
    }

    cu->absPartIdx = 0;

    if ((enforceSliceRestriction && (below == NULL || belowAddr > (Ipp32s)m_cslice->slice_address_last_ctb)) ||
        (enforceTileRestriction && (below == NULL || !belowSameTile)))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }
    cu->ctbData = below;
    cu->ctbAddr = belowAddr;
}

template <typename PixType>
void H265CU<PixType>::GetCtuRight(H265CUPtr *cu,
                       Ipp32s enforceSliceRestriction,
                       Ipp32s enforceTileRestriction
                       )
{
    H265CUData *right = NULL;
    Ipp32s rightAddr = -1, rightSameTile = 0;
    Ipp32u widthInCU = m_par->PicWidthInCtbs;

    if ((m_ctbPelX >> m_par->Log2MaxCUSize) < widthInCU - 1)
    {
        right = m_data + m_par->NumPartInCU;
        rightAddr = m_ctbAddr + 1;
        if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[rightAddr])
            rightSameTile = 1;
    }

    cu->absPartIdx = 0;

    if ((enforceSliceRestriction && (right == NULL || rightAddr > (Ipp32s)m_cslice->slice_address_last_ctb)) ||
        (enforceTileRestriction && (right == NULL || !rightSameTile)))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->ctbData = right;
    cu->ctbAddr = rightAddr;
}

template <typename PixType>
void H265CU<PixType>::GetCtuBelowRight(H265CUPtr *cu,
                        Ipp32s enforceSliceRestriction,
                        Ipp32s enforceTileRestriction)
{
    H265CUData *belowRight = NULL;
    Ipp32s belowRightAddr = -1, belowRightSameTile = 0;
    Ipp32u widthInCU = m_par->PicWidthInCtbs;

    if ((m_ctbPelY >> m_par->Log2MaxCUSize) < m_par->PicHeightInCtbs - 1 &&
        (m_ctbPelX >> m_par->Log2MaxCUSize) < widthInCU - 1)
    {
        belowRight = m_data + ((widthInCU + 1) << m_par->Log2NumPartInCU);
        belowRightAddr = m_ctbAddr + widthInCU + 1;
        if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[belowRightAddr])
            belowRightSameTile = 1;
    }

    cu->absPartIdx = 0;

    if ((enforceSliceRestriction && (belowRight == NULL || belowRightAddr > (Ipp32s)m_cslice->slice_address_last_ctb)) ||
        (enforceTileRestriction && (belowRight == NULL || !belowRightSameTile)))
    {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->ctbData = belowRight;
    cu->ctbAddr = belowRightAddr;
}

template <typename PixType>
H265CUData* H265CU<PixType>::GetQpMinCuLeft( Ipp32u& uiLPartUnitIdx, Ipp32u uiCurrAbsIdxInLCU, bool bEnforceSliceRestriction, bool bEnforceDependentSliceRestriction)
{
    Ipp32u shift = ((m_par->MaxCUDepth - m_par->MaxCuDQPDepth) << 1);
    Ipp32u absZorderQpMinCUIdx = (uiCurrAbsIdxInLCU >> shift) << shift;
    Ipp32u absRorderQpMinCUIdx = h265_scan_z2r4[absZorderQpMinCUIdx];

    // check for left LCU boundary
    //if ( isZeroCol(absRorderQpMinCUIdx, unumPartInCUWidth) )
    if ( (absRorderQpMinCUIdx & 15) == 0 )
    {
        return NULL;
    }

    // get index of left-CU relative to top-left corner of current quantization group
    uiLPartUnitIdx = h265_scan_r2z4[absRorderQpMinCUIdx - 1];

    // return pointer to current LCU
    return m_data;
}

template <typename PixType>
H265CUData* H265CU<PixType>::GetQpMinCuAbove( Ipp32u& aPartUnitIdx, Ipp32u currAbsIdxInLCU, bool enforceSliceRestriction, bool enforceDependentSliceRestriction )
{
    Ipp32u shift = ((m_par->MaxCUDepth - m_par->MaxCuDQPDepth) << 1);
    Ipp32u absZorderQpMinCUIdx = (currAbsIdxInLCU >> shift) << shift;
    Ipp32u absRorderQpMinCUIdx = h265_scan_z2r4[absZorderQpMinCUIdx];

    // check for top LCU boundary
    //if ( isZeroRow( absRorderQpMinCUIdx, unumPartInCUWidth) )
    if ( absRorderQpMinCUIdx < 15 )
    {
        return NULL;
    }

    // get index of top-CU relative to top-left corner of current quantization group
    aPartUnitIdx = h265_scan_r2z4[absRorderQpMinCUIdx - PITCH_TU];

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
        Ipp8s       sliceQpY,
        H265CUData* data,
        H265CUData* ctbData,
        Ipp32u absIdxInLcu)
    :m_ctbAddr(ctbAddr)
    ,m_par(par)
    ,m_sliceQpY(sliceQpY)
    ,m_data(data)
    ,m_ctbData(ctbData)
    ,m_absIdxInLcu(absIdxInLcu)
    {};

    ~H265CUFake(){};

    Ipp32u m_ctbAddr;
    H265VideoParam*  m_par;
    Ipp8s m_sliceQpY;
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

    // kla
    // MODE_NONE is NOWHERE set! Does it work as expected?

    while (iLastValidPartIdx >= 0 && cu->m_data[iLastValidPartIdx].predMode == MODE_NONE)
    {
        int depth = cu->m_data[iLastValidPartIdx].depth;
        //iLastValidPartIdx--;
        iLastValidPartIdx -= cu->m_par->NumPartInCU >> (depth << 1);
    }

    return iLastValidPartIdx;
}

template <class H265CuBase>
Ipp8s GetLastCodedQP(H265CuBase* cu, Ipp32s absPartIdx )
{
    Ipp32u uiQUPartIdxMask = ~((1 << ((cu->m_par->MaxCUDepth - cu->m_par->MaxCuDQPDepth) << 1)) - 1);
    Ipp32s iLastValidPartIdx = GetLastValidPartIdx(cu, absPartIdx & uiQUPartIdxMask);

    if (iLastValidPartIdx >= 0)
    {
        return cu->m_data[iLastValidPartIdx].qp;
    }
    else  /* assume TilesOrEntropyCodingSyncIdc == 0 */
    {
        if( cu->m_absIdxInLcu > 0 ) {
            //H265CU tmpCtb;
            //memset(&tmpCtb, 0, sizeof(H265CU));
            //tmpCtb.m_ctbAddr = m_ctbAddr;
            //tmpCtb.m_par = m_par;
            //tmpCtb.m_data = m_ctbData + (m_ctbAddr << m_par->Log2NumPartInCU);
            ////return GetLastCodedQP(m_absIdxInLcu );
            //return tmpCtb.GetLastCodedQP(m_absIdxInLcu);

            VM_ASSERT(!"NOT IMPLEMENTED");
            return 0;
        } else if (cu->m_ctbAddr > 0 && !(cu->m_par->WPPFlag && cu->m_ctbAddr % cu->m_par->PicWidthInCtbs == 0)) {
            
            int curAddr = cu->m_ctbAddr;

            // slice segmentation
            Ipp8u sameSlice = cu->m_par->m_slice_ids[curAddr] == cu->m_par->m_slice_ids[curAddr-1];
            if(cu->m_par->NumTiles == 1 && sameSlice) {
               H265CUFake tmpCtb(
                    cu->m_ctbAddr-1, 
                    cu->m_par,
                    cu->m_sliceQpY,
                    cu->m_ctbData + ((cu->m_ctbAddr-1) << cu->m_par->Log2NumPartInCU),
                    cu->m_ctbData,
                    cu->m_absIdxInLcu);

                Ipp8s qp = GetLastCodedQP(&tmpCtb, cu->m_par->NumPartInCU);
                return qp;
            }

            // tile segmentation
            else if (cu->m_par->NumTiles > 1) {
                Ipp32u tile_id = cu->m_par->m_tile_ids[curAddr];
                if (curAddr != cu->m_par->m_tiles[tile_id].first_ctb_addr) {
                    int prevCtbAddr = curAddr - 1;
                    Ipp32s tile_col = tile_id % cu->m_par->NumTileCols;
                    int regionCtbColFirst = cu->m_par->tileColStart[tile_col];
                    int currCtbCol = (curAddr % cu->m_par->PicWidthInCtbs);
                    if (regionCtbColFirst == currCtbCol) {
                        prevCtbAddr = curAddr - cu->m_par->PicWidthInCtbs + cu->m_par->tileColWidth[tile_col] - 1;
                    }
                    H265CUFake tmpCtb(
                        prevCtbAddr, 
                        cu->m_par,
                        cu->m_sliceQpY,
                        cu->m_ctbData + ((prevCtbAddr) << cu->m_par->Log2NumPartInCU),
                        cu->m_ctbData,
                        cu->m_absIdxInLcu);

                    Ipp8s qp = GetLastCodedQP(&tmpCtb, cu->m_par->NumPartInCU);
                    return qp;
                }
            }
        }
    }

    return cu->m_sliceQpY;
}


template <typename PixType>
void H265CU<PixType>::UpdateCuQp(void)
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
template <typename PixType>
void H265CU<PixType>::InitCu(
                    H265VideoParam *_par, 
                    H265CUData *_data, 
                    H265CUData *_dataTemp, 
                    Ipp32s cuAddr,
                    H265BsFake *_bsf,
                    H265Slice *_cslice, 
                    ThreadingTaskSpecifier stage, 
                    costStat* _costStat,
                    const Frame* frame,
                    CoeffsType *m_coeffWork) 
{
    m_par = _par;
    m_sliceQpY = frame->m_sliceQpY;
    m_lcuQps = (Ipp8s*)&frame->m_lcuQps[0];

    m_cslice = _cslice;
    m_costStat = _costStat;

    m_bsf = _bsf;
    m_data = _data;
    m_dataStored = _dataTemp;
    m_ctbAddr = cuAddr;
    m_ctbPelX = (cuAddr % m_par->PicWidthInCtbs) * m_par->MaxCUSize;
    m_ctbPelY = (cuAddr / m_par->PicWidthInCtbs) * m_par->MaxCUSize;
    m_absIdxInLcu = 0;
    m_numPartition = m_par->NumPartInCU;

    m_currFrame = (Frame*)frame;

    FrameData* recon = m_currFrame->m_recon;
    m_pitchRecLuma = recon->pitch_luma_pix;
    m_pitchRecChroma = recon->pitch_chroma_pix;
    m_yRec = (PixType*)recon->y + m_ctbPelX + m_ctbPelY * m_pitchRecLuma;
    m_uvRec = (PixType*)recon->uv + (m_ctbPelX << _par->chromaShiftWInv) + (m_ctbPelY * m_pitchRecChroma >> _par->chromaShiftH);

    FrameData* origin = m_currFrame->m_origin;
    m_pitchSrcLuma = origin->pitch_luma_pix;
    m_pitchSrcChroma = origin->pitch_chroma_pix;
    m_ySrc = (PixType*)origin->y + m_ctbPelX + m_ctbPelY * m_pitchSrcLuma;
    m_uvSrc = (PixType*)origin->uv + (m_ctbPelX << _par->chromaShiftWInv) + (m_ctbPelY * m_pitchSrcChroma >> _par->chromaShiftH);

    m_coeffWorkY = m_coeffWork;
    m_coeffWorkU = m_coeffWorkY + m_par->MaxCUSize * m_par->MaxCUSize;
    m_coeffWorkV = m_coeffWorkU + (m_par->MaxCUSize * m_par->MaxCUSize >> m_par->chromaShift);

    // limits to clip MV allover the CU
    const Ipp32s MvShift = 2;
    const Ipp32s offset = 8;
    HorMax = (m_par->Width + offset - m_ctbPelX - 1) << MvShift;
    HorMin = ( - (Ipp32s) m_par->MaxCUSize - offset - (Ipp32s) m_ctbPelX + 1) << MvShift;
    VerMax = (m_par->Height + offset - m_ctbPelY - 1) << MvShift;
    VerMin = ( - (Ipp32s) m_par->MaxCUSize - offset - (Ipp32s) m_ctbPelY + 1) << MvShift;

    //m_uvSrc = (PixType*)currFrame->uv + (m_ctbPelX << _par->chromaShiftWInv) + (m_ctbPelY * m_pitchSrcChroma >> _par->chromaShiftH);

    if (stage == TT_ENCODE_CTU) {
        m_rdOptFlag = m_cslice->rd_opt_flag;
        m_rdLambda = m_cslice->rd_lambda_slice;

        //kolya
        m_rdLambdaSqrt = m_cslice->rd_lambda_sqrt_slice;
        m_ChromaDistWeight = m_cslice->ChromaDistWeight_slice;
        m_rdLambdaChroma = m_rdLambda*(3.0+1.0/m_ChromaDistWeight)/4.0;

        m_rdLambdaInter = m_cslice->rd_lambda_inter_slice;
        m_rdLambdaInterMv = m_cslice->rd_lambda_inter_mv_slice;

        for (Ipp32u i = 0; i < 4; i++) {
            m_costStat[m_ctbAddr].cost[i] = m_costStat[m_ctbAddr].num[i] = 0;
        }

        // zero initialize
        memset(m_data, 0, sizeof(*m_data));
        m_data->qp = m_lcuQps[m_ctbAddr];
        PropagateSubPart(m_data, m_par->NumPartInCU);
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
    m_leftSameTile = m_aboveSameTile = m_aboveLeftSameTile = m_aboveRightSameTile = 0;

    Ipp32u tile_id = m_par->m_tile_ids[m_ctbAddr];

    Ipp32u widthInCU = m_par->PicWidthInCtbs;
    if (m_ctbAddr % widthInCU)
    {
        m_left = m_data - m_par->NumPartInCU;
        m_leftAddr = m_ctbAddr - 1;
        if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_leftAddr])
            m_leftSameTile = 1;
    }

    if (m_ctbAddr >= widthInCU)
    {
        m_above = m_data - (widthInCU << m_par->Log2NumPartInCU);
        m_aboveAddr = m_ctbAddr - widthInCU;
        if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_aboveAddr])
            m_aboveSameTile = 1;
    }

    if ( m_left && m_above )
    {
        m_aboveLeft = m_data - (widthInCU << m_par->Log2NumPartInCU) - m_par->NumPartInCU;
        m_aboveLeftAddr = m_ctbAddr - widthInCU - 1;
        if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_aboveLeftAddr])
            m_aboveLeftSameTile = 1;
    }

    if ( m_above && ( (m_ctbAddr%widthInCU) < (widthInCU-1) )/* && tile_id == m_par->m_tile_ids[m_ctbAddr - widthInCU + 1]*/)
    {
        m_aboveRight = m_data - (widthInCU << m_par->Log2NumPartInCU) + m_par->NumPartInCU;
        m_aboveRightAddr = m_ctbAddr - widthInCU + 1;
        if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_aboveRightAddr])
            m_aboveRightSameTile = 1;
    }

    Ipp32s tile_row = tile_id / m_par->NumTileCols;
    Ipp32s tile_col = tile_id % m_par->NumTileCols;
    
    if ((m_par->NumTiles == 1 && m_par->NumSlices == 1) || m_par->deblockBordersFlag) {
        m_region_border_right = m_par->Width;
        m_region_border_bottom = m_par->Height;
        m_region_border_left = 0;
        m_region_border_top = 0;
    } else {
        m_region_border_left = m_par->tileColStart[tile_col] << m_par->Log2MaxCUSize;
        m_region_border_right = (m_par->tileColStart[tile_col] + m_par->tileColWidth[tile_col]) << m_par->Log2MaxCUSize;
        if (m_region_border_right > m_par->Width)
            m_region_border_right = m_par->Width;

        m_region_border_top = m_par->NumTiles > 1 ? (m_par->tileRowStart[tile_row] << m_par->Log2MaxCUSize) :
            (m_cslice->row_first << m_par->Log2MaxCUSize);
        m_region_border_bottom = m_par->NumTiles > 1 ? ((m_par->tileRowStart[tile_row] + m_par->tileRowHeight[tile_row]) << m_par->Log2MaxCUSize) :
            ((m_cslice->row_last + 1)  << m_par->Log2MaxCUSize);
        if (m_region_border_bottom > m_par->Height)
            m_region_border_bottom = m_par->Height;
    }
    m_SlicePixRowFirst = m_cslice->row_first << m_par->Log2MaxCUSize;
    m_SlicePixRowLast = (m_cslice->row_last + 1) << m_par->Log2MaxCUSize;

    m_bakAbsPartIdxCu = 0;
    m_isRdoq = m_par->RDOQFlag ? true : false;
    
    m_ctbData = _data - (m_ctbAddr << m_par->Log2NumPartInCU);
    if (stage == TT_ENCODE_CTU) {
#ifdef AMT_ICRA_OPT
        m_SCid[0][0]    = 5;
        m_SCpp[0][0]    = 1764.0;
        m_STC[0][0]     = 2;
        m_mvdMax  = 128;
        m_mvdCost = 60;
        GetSpatialComplexity(0, 0, 0, 0);
#endif
        m_intraMinDepth = 0;
        m_adaptMaxDepth = MAX_TOTAL_DEPTH;
        m_projMinDepth = 0;
        m_adaptMinDepth = 0;
        if(m_par->minCUDepthAdapt) 
            GetAdaptiveMinDepth(0, 0);
        m_isHistBuilt = false;

        m_cuIntraAngMode = m_cslice->sliceIntraAngMode;
        /*EnumIntraAngMode((m_currFrame->m_picCodeType == MFX_FRAMETYPE_B && !m_currFrame->m_isRef)
            ? m_par->intraAngModes[B_NONREF]
        : ((m_currFrame->m_picCodeType == MFX_FRAMETYPE_P)
            ? m_par->intraAngModes[SliceTypeIndex(P_SLICE)]
        : m_par->intraAngModes[SliceTypeIndex(m_cslice->slice_type)]));*/

#ifndef AMT_ALT_ENCODE
        m_interPredY = m_interPredBufsY[0];
        m_interPredC = m_interPredBufsC[0];
        m_interResidY = m_interResidBufsY[0];
        m_interResidU = m_interResidBufsU[0];
        m_interResidV = m_interResidBufsV[0];

        m_interPredBestY = m_interPredBufsY[1];
        m_interPredBestC = m_interPredBufsC[1];
        m_interResidBestY = m_interResidBufsY[1];
        m_interResidBestU = m_interResidBufsU[1];
        m_interResidBestV = m_interResidBufsV[1];
#endif

#ifdef MEMOIZE_SUBPEL
        MemoizeInit();
#endif
    }
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
//      ((m_scan_z2r[absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
//  Ipp32u rpel_x   = lpel_x + (m_par->MaxCUSize>>depth)  - 1;
//  Ipp32u tpel_y   = m_ctbPelY +
//      ((m_scan_z2r[absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
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
//        ((m_scan_z2r[absPartIdx] & (m_par->NumMinTUInMaxCU - 1)) << m_par->QuadtreeTULog2MinSize);
//    Ipp32u rpel_x   = lpel_x + (m_par->MaxCUSize>>depth)  - 1;
//    Ipp32u tpel_y   = m_ctbPelY +
//        ((m_scan_z2r[absPartIdx] >> m_par->MaxCUDepth) << m_par->QuadtreeTULog2MinSize);
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
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
template <typename PixType>
Ipp8u H265CU<PixType>::GetAdaptiveIntraMinDepth(Ipp32s absPartIdx, Ipp32s depth, Ipp32s& maxSC)
{
    Ipp8u intraMinDepth = 0;
    Ipp32f delta_sc[10]   =   {12.0, 39.0, 81.0, 168.0, 268.0, 395.0, 553.0, 744.0, 962.0, 962.0};
    Ipp32s subSC[4];
    Ipp32f subSCpp[4];
    intraMinDepth = 0;
    //Ipp32s maxSC = 0;
    maxSC = 0;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    for (Ipp32s i = 0; i < 4; i++) {
        Ipp32s partAddr = (numParts >> 2) * i;
        subSC[i] = GetSpatialComplexity(absPartIdx, depth, partAddr, depth+1, subSCpp[i]);
        if(subSC[i]>maxSC) maxSC = subSC[i];
        for(Ipp32s j=i-1; j>=0; j--) {
            if(fabsf(subSCpp[i]-subSCpp[j])>delta_sc[IPP_MIN(subSC[i], subSC[j])]*(float)(1<<(m_par->bitDepthLumaShift*2))) {
                intraMinDepth = 1;
            }
        }
    }
    // absolutes & overrides
    if(IsTuSplitIntra() && ((m_par->Log2MaxCUSize-depth) <= 5 || m_par->QuadtreeTUMaxDepthIntra>2)) {
        if(maxSC<5) intraMinDepth = 0;
    } else 
    {
        if(maxSC>=m_par->IntraMinDepthSC)  intraMinDepth = 1;
        if(maxSC<2)                        intraMinDepth = 0;
    }
    return intraMinDepth+=depth;
}
#endif

template <typename PixType>
void H265CU<PixType>::GetAdaptiveMinDepth(Ipp32s absPartIdx, Ipp32s depth)
{
    Ipp32u width  = (Ipp8u)(m_par->MaxCUSize>>depth);
    Ipp32u height = (Ipp8u)(m_par->MaxCUSize>>depth);
    Ipp32u posx  = ((h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u posy  = ((h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize);
    m_intraMinDepth = 0;
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    Ipp32s maxSC=m_SCid[depth][absPartIdx];
    if ((m_ctbPelX + posx + width) > m_par->Width || (m_ctbPelY + posy + height) > m_par->Height) {
        m_intraMinDepth = 1;
    } else {
        m_intraMinDepth = GetAdaptiveIntraMinDepth(absPartIdx, depth, maxSC);
    }
#endif
    if (m_cslice->num_ref_idx[0] == 0 && m_cslice->num_ref_idx[1] == 0) {
        m_adaptMinDepth = m_intraMinDepth;
#ifdef AMT_ADAPTIVE_INTER_MIN_DEPTH
        if (m_adaptMinDepth == 0 && (m_par->Log2MaxCUSize-depth) > 5) {
            m_adaptMaxDepth = m_SCid[depth][absPartIdx]+((maxSC!=m_SCid[depth][absPartIdx])?1:0)+(m_currFrame->m_isRef);
        }
#endif
        return;
    }
    Ipp8u interMinDepthColSTC = 0;
    Ipp32s STC = 2;
#ifdef AMT_ADAPTIVE_INTER_MIN_DEPTH
    if((m_par->Log2MaxCUSize-depth)>5) {
        FrameData *ref0 = m_currFrame->m_refPicList[0].m_refFrames[0]->m_recon;
        STC = GetSpatioTemporalComplexityColocated(absPartIdx, depth, 0, depth, ref0);
        if(STC>=m_par->InterMinDepthSTC && m_intraMinDepth>depth) interMinDepthColSTC=1;
    }
#endif
    Ipp8u interMinDepthColCu = 0;
    RefPicList *refLists = m_currFrame->m_refPicList;
    Ipp32s cuDataColOffset = (m_ctbAddr << m_par->Log2NumPartInCU);

    Ipp8s qpCur = m_lcuQps[m_ctbAddr];
    Ipp8s qpPrev = qpCur;
    Ipp32s xLimit = (m_par->Width  - m_ctbPelX) >> m_par->QuadtreeTULog2MinSize;
    Ipp32s yLimit = (m_par->Height - m_ctbPelY) >> m_par->QuadtreeTULog2MinSize;

    Ipp8u depthMin = 4;
    Ipp32s depthSum = 0;
    Ipp32s numParts = 0;

    for (Ipp32s list = 1; list >= 0; list--) {
        if (m_cslice->num_ref_idx[list] > 0) {
            Frame *ref0 = refLists[list].m_refFrames[0];
            const H265CUData *ctbCol = ref0->cu_data + cuDataColOffset;
            qpPrev = ctbCol->qp;
            if(ref0->m_picCodeType==MFX_FRAMETYPE_I) continue;   // Spatial complexity does not equal temporal complexity -NG
            for (Ipp32u i = 0; i < m_numPartition; i += 4, ctbCol += 4) {
                if ((h265_scan_z2r4[i] & 15) >= xLimit || (h265_scan_z2r4[i] >> 4) >= yLimit)
                    continue; // out of picture

                numParts++;
                depthSum += ctbCol->depth;
                if (depthMin > ctbCol->depth)
                    depthMin = ctbCol->depth;
            }
        }
    }
    if (!numParts) {
        interMinDepthColCu = 0;
    } else {
        Ipp64f depthAvg = (Ipp64f)depthSum / numParts;

        Ipp8u depthDelta = 1;
        if (qpCur - qpPrev < 0 || (qpCur - qpPrev >= 0 && depthAvg - depthMin > 0.5))
            depthDelta = 0;

        interMinDepthColCu = (depthMin > 0) ? Ipp8u(depthMin - depthDelta) : depthMin;
    }

#ifdef AMT_ADAPTIVE_INTER_MIN_DEPTH
    if (STC < 4) {
        m_adaptMinDepth = IPP_MAX(interMinDepthColCu, interMinDepthColSTC);   // Copy
    } else {
        m_adaptMinDepth = interMinDepthColSTC;                                // Reset
    }
    if (m_adaptMinDepth == 0 && (m_par->Log2MaxCUSize-depth) > 5) {
        m_adaptMaxDepth = m_SCid[depth][absPartIdx]+((maxSC!=m_SCid[depth][absPartIdx])?1:0)+(m_currFrame->m_isRef);
    }
#else
    m_adaptMinDepth = interMinDepthColCu;
#endif
}

#ifdef AMT_ICRA_OPT
template <typename PixType>
H265CUData* H265CU<PixType>::GetCuDataXY(Ipp32s x, Ipp32s y, Frame *ref)
{
    H265CUData *cu = NULL;
    if(x < 0 || y < 0 || x >= (Ipp32s) m_par->Width  || y >= (Ipp32s) m_par->Height) return cu;
    if (!ref)                                                                        return cu;
    Ipp32s lcux     = x>>m_par->Log2MaxCUSize;
    Ipp32s lcuy     = y>>m_par->Log2MaxCUSize;
    Ipp32u ctbAddr  = lcuy*m_par->PicWidthInCtbs+lcux;
    Ipp32s posx     = x - (lcux<<m_par->Log2MaxCUSize);
    Ipp32s posy     = y - (lcuy<<m_par->Log2MaxCUSize);
    Ipp32s absPartIdx = h265_scan_r2z4[(posy>>m_par->QuadtreeTULog2MinSize)*PITCH_TU+(posx>>m_par->QuadtreeTULog2MinSize)];
    cu = ref->cu_data + (ctbAddr << m_par->Log2NumPartInCU) + absPartIdx;
    return cu;
}

template <typename PixType>
void H265CU<PixType>::GetProjectedDepth(Ipp32s absPartIdx, Ipp32s depth, Ipp8u splitMode)
{
    if (m_cslice->num_ref_idx[0] == 0 && m_cslice->num_ref_idx[1] == 0)
        return;
    
    const H265CUData *dataBestInter = GetBestCuDecision(absPartIdx, depth);
    Ipp8u layer = (m_par->BiPyramidLayers > 1) ? m_currFrame->m_pyramidLayer : 0;
    bool highestLayerBiFrame = (m_par->BiPyramidLayers > 1 && layer == m_par->BiPyramidLayers - 1);
    Ipp32s numParts = ( m_par->NumPartInCU >> (depth<<1) );

    if (depth==0 && !highestLayerBiFrame && dataBestInter->flags.skippedFlag!=1 && splitMode == SPLIT_TRY) 
    {
        Ipp32s minDepth0 = MAX_TOTAL_DEPTH;
        Ipp32s minDepth1 = MAX_TOTAL_DEPTH;
        Ipp32s foundDepth0 = 0;
        Ipp32s foundDepth1 = 0;
        Ipp8s qpCur = m_lcuQps[m_ctbAddr];
        Ipp8s qp0 = 52;
        Ipp8s qp1 = 52;
        RefPicList *refPicList = m_currFrame->m_refPicList;

        for(Ipp32s i=0;i<numParts;i+=4) {
            Ipp32s posx = (Ipp8u)((h265_scan_z2r4[absPartIdx+i] & 15) << m_par->QuadtreeTULog2MinSize);
            Ipp32s posy = (Ipp8u)((h265_scan_z2r4[absPartIdx+i] >> 4) << m_par->QuadtreeTULog2MinSize);
            if (dataBestInter[i].interDir & INTER_DIR_PRED_L0) {
                Frame *ref0 = refPicList[0].m_refFrames[dataBestInter[i].refIdx[0]];
                if(ref0->m_picCodeType!=MFX_FRAMETYPE_I) {
                    posx += dataBestInter[i].mv[0].mvx/4;
                    posy += dataBestInter[i].mv[0].mvy/4;
                    H265CUData *cu0 = GetCuDataXY(m_ctbPelX+posx, m_ctbPelY+posy, ref0);
                    if(cu0) {
                        if(cu0->depth<minDepth0) minDepth0 = cu0->depth;
                        foundDepth0++;
                        if(cu0->qp<qp0) qp0=cu0->qp;
                    }
                }
            }
        }
        if(minDepth0==MAX_TOTAL_DEPTH || foundDepth0<numParts/5) minDepth0 = 0;
        if(layer==0 && qp0<qpCur) minDepth0 = 0;

        if(m_cslice->slice_type == B_SLICE) {
            for(Ipp32s i=0;i<numParts;i+=4) {
                Ipp32s posx = (Ipp8u)((h265_scan_z2r4[absPartIdx+i] & 15) << m_par->QuadtreeTULog2MinSize);
                Ipp32s posy = (Ipp8u)((h265_scan_z2r4[absPartIdx+i] >> 4) << m_par->QuadtreeTULog2MinSize);
                if (dataBestInter[i].interDir & INTER_DIR_PRED_L1) {
                    Frame *ref1 = refPicList[1].m_refFrames[dataBestInter[i].refIdx[1]];
                    if(ref1->m_picCodeType!=MFX_FRAMETYPE_I) {
                        posx += dataBestInter[i].mv[1].mvx/4;
                        posy += dataBestInter[i].mv[1].mvy/4;
                        H265CUData *cu1 = GetCuDataXY(m_ctbPelX+posx, m_ctbPelY+posy, ref1);
                        if(cu1) {
                            if(cu1->depth<minDepth1) minDepth1 = cu1->depth;
                            foundDepth1++;
                            if(cu1->qp<qp1) qp1=cu1->qp;
                        }
                    }
                }
            }
        }
        if(minDepth1==MAX_TOTAL_DEPTH || foundDepth1<numParts/5) minDepth1 = 0;
        if(layer==0 && qp1<qpCur) minDepth0 = 0;

        if(foundDepth0 && foundDepth1) {
            m_projMinDepth = (Ipp8u)(IPP_MIN(minDepth0, minDepth1));
        } else if(foundDepth0) {
            m_projMinDepth = (Ipp8u)minDepth0;
        } else {
            m_projMinDepth = (Ipp8u)minDepth1;
        }
        if(m_projMinDepth && (layer || m_STC[depth][absPartIdx]<1 || m_STC[depth][absPartIdx]>3)) m_projMinDepth--; // Projected from diff layer or change in complexity or very high complexity
    }

    if (depth==0 && dataBestInter->flags.skippedFlag!=1 && splitMode == SPLIT_TRY && m_STC[depth][absPartIdx]<2)
    {
        Ipp32s maxDepth0 = 0;
        Ipp32s maxDepth1 = 0;
        Ipp32s foundDepth0 = 0;
        Ipp32s foundDepth1 = 0;
        Ipp8s qpCur = m_lcuQps[m_ctbAddr];
        Ipp8s qp0 = 0;
        Ipp8s qp1 = 0;
        RefPicList *refPicList = m_currFrame->m_refPicList;

        for(Ipp32s i=0;i<numParts;i+=4) {
            Ipp32s posx = (Ipp8u)((h265_scan_z2r4[absPartIdx+i] & 15) << m_par->QuadtreeTULog2MinSize);
            Ipp32s posy = (Ipp8u)((h265_scan_z2r4[absPartIdx+i] >> 4) << m_par->QuadtreeTULog2MinSize);
            if ((dataBestInter[i].interDir & INTER_DIR_PRED_L0) && dataBestInter[i].refIdx[0]==0) {
                Frame *ref0 = refPicList[0].m_refFrames[dataBestInter[i].refIdx[0]];
                posx += dataBestInter[i].mv[0].mvx/4;
                posy += dataBestInter[i].mv[0].mvy/4;
                H265CUData *cu0 = GetCuDataXY(m_ctbPelX+posx, m_ctbPelY+posy, ref0);
                if(cu0) {
                    if(cu0->depth>maxDepth0) maxDepth0 = cu0->depth;
                    foundDepth0++;
                    if(cu0->qp>qp0) qp0 = cu0->qp;
                }
            }
        }
        if(maxDepth0==0 || foundDepth0<numParts/5 || qp0>qpCur) maxDepth0 = MAX_TOTAL_DEPTH;
        if(qp0==qpCur) maxDepth0++;

        if(m_cslice->slice_type == B_SLICE) {
            for(Ipp32s i=0;i<numParts;i+=4) {
                Ipp32s posx = (Ipp8u)((h265_scan_z2r4[absPartIdx+i] & 15) << m_par->QuadtreeTULog2MinSize);
                Ipp32s posy = (Ipp8u)((h265_scan_z2r4[absPartIdx+i] >> 4) << m_par->QuadtreeTULog2MinSize);
                if ((dataBestInter[i].interDir & INTER_DIR_PRED_L1) && dataBestInter[i].refIdx[1]==0) {
                    Frame *ref1 = refPicList[1].m_refFrames[dataBestInter[i].refIdx[1]];
                    posx += dataBestInter[i].mv[1].mvx/4;
                    posy += dataBestInter[i].mv[1].mvy/4;
                    H265CUData *cu1 = GetCuDataXY(m_ctbPelX+posx, m_ctbPelY+posy, ref1);
                    if(cu1) {
                        if(cu1->depth>maxDepth1) maxDepth1 = cu1->depth;
                        foundDepth1++;
                        if(cu1->qp>qp1) qp1 = cu1->qp;
                    }
                }
            }
        }
        if(maxDepth1==0 || foundDepth1<numParts/5 || qp1>qpCur) maxDepth1 = MAX_TOTAL_DEPTH;
        if(qp1==qpCur) maxDepth1++;

        m_adaptMaxDepth = IPP_MIN(m_adaptMaxDepth, IPP_MIN(maxDepth0, maxDepth1));
    }
}
#endif
template <typename PixType>
void H265CU<PixType>::FillSubPart(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth,
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

template <typename PixType>
void H265CU<PixType>::FillSubPartIntraLumaDir(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth, Ipp8u lumaDir)
{
    Ipp32u numParts = (m_par->NumPartInCU >> ((depthCu + trDepth) << 1));
    for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++)
        m_data[i].intraLumaDir = lumaDir;
}

template <typename PixType>
void H265CU<PixType>::CopySubPartTo(H265CUData *dataCopy, Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth)
{
    Ipp32s depth = depthCu + trDepth;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    small_memcpy(dataCopy + absPartIdx, m_data + absPartIdx, numParts * sizeof(H265CUData));
}

void FillSubPartIntraCuModes_(H265CUData *data, Ipp32s numParts, Ipp8u cuWidth, Ipp8u cuDepth, Ipp8u partMode)
{
    data->depth = cuDepth;
    data->size = cuWidth;
    data->partSize = partMode;
    data->predMode = MODE_INTRA;
    data->cbf[0] = 0;
    data->cbf[1] = 0;
    data->cbf[2] = 0;
    data->_flags = 0;
    PropagateSubPart(data, numParts);
}

void FillSubPartIntraPartMode_(H265CUData *data, Ipp32s numParts, Ipp8u partMode)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->partSize = partMode;
}

void FillSubPartCuQp_(H265CUData *data, Ipp32s numParts, Ipp8u qp)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->qp = qp;
}

void FillSubPartIntraPredModeY_(H265CUData *data, Ipp32s numParts, Ipp8u mode)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->intraLumaDir = mode;
}

void FillSubPartIntraPredModeC_(H265CUData *data, Ipp32s numParts, Ipp8u mode)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->intraChromaDir = mode;
}

void FillSubPartTrDepth_(H265CUData *data, Ipp32s numParts, Ipp8u trDepth)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->trIdx = trDepth;
}

void FillSubPartSkipFlag_(H265CUData *data, Ipp32s numParts, Ipp8u skipFlag)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->flags.skippedFlag = skipFlag;
}

void FillSubPartCbfY_(H265CUData *data, Ipp32s numParts, Ipp8u cbf)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->cbf[0] = cbf;
}

void FillSubPartCbfU_(H265CUData *data, Ipp32s numParts, Ipp8u cbf)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->cbf[1] = cbf;
}

void FillSubPartCbfV_(H265CUData *data, Ipp32s numParts, Ipp8u cbf)
{
    for (Ipp32s i = 0; i < numParts; i++, data++)
        data->cbf[2] = cbf;
}

void CopySubPartTo_(H265CUData *dst, const H265CUData *src, Ipp32s absPartIdx, Ipp32u numParts)
{
    small_memcpy(dst + absPartIdx, src + absPartIdx, numParts * sizeof(H265CUData));
}

#if defined(AMT_ICRA_OPT)
template <typename PixType>
void H265CU<PixType>::GetSpatialComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth)
{
    float SCpp = 0;
    m_SCid[depth][absPartIdx] = GetSpatialComplexity(absPartIdx, depth, partAddr, partDepth, SCpp);
    m_SCpp[depth][absPartIdx] = SCpp;
}

template <typename PixType>
Ipp32s H265CU<PixType>::GetSpatialComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth, Ipp32f& SCpp) const
{
    Ipp32u width  = (Ipp8u)(m_par->MaxCUSize>>partDepth);
    Ipp32u height = (Ipp8u)(m_par->MaxCUSize>>partDepth);
    Ipp32u posx  = ((h265_scan_z2r4[absPartIdx+partAddr] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u posy  = ((h265_scan_z2r4[absPartIdx+partAddr] >> 4) << m_par->QuadtreeTULog2MinSize);
    
    if (m_ctbPelX + posx + width > m_par->Width)
        width = m_par->Width - posx - m_ctbPelX;
    if (m_ctbPelY + posy + height > m_par->Height)
        height = m_par->Height - posy - m_ctbPelY;

    Ipp32s log2BlkSize = m_par->Log2MaxCUSize - partDepth;
    Ipp32s pitchRsCs = m_currFrame->m_stats[0]->m_pitchRsCs4 >> (log2BlkSize-2);
    Ipp32s idx = ((m_ctbPelY+posy)>>log2BlkSize)*pitchRsCs + ((m_ctbPelX+posx)>>log2BlkSize);
    Ipp32s Rs2 = m_currFrame->m_stats[0]->m_rs[log2BlkSize-2][idx];
    Ipp32s Cs2 = m_currFrame->m_stats[0]->m_cs[log2BlkSize-2][idx];
    float Rs2pp=(float)Rs2;
    float Cs2pp=(float)Cs2;
    Rs2pp/=((width/4)*(height/4));
    Cs2pp/=((width/4)*(height/4));

    SCpp = Rs2pp+Cs2pp;
    static float lmt_sc2[10]   =   {16.0, 81.0, 225.0, 529.0, 1024.0, 1764.0, 2809.0, 4225.0, 6084.0, (float)INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
    Ipp32s scVal = 5;
    for(Ipp32s i = 0;i<10;i++) {
        if(SCpp < lmt_sc2[i]*(float)(1<<(m_par->bitDepthLumaShift*2)))  {
            scVal   =   i;
            break;
        }
    }
    return scVal;
}
template <typename PixType>
Ipp32s H265CU<PixType>::GetSpatioTemporalComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth)
{
    Ipp32u width  = (Ipp8u)(m_par->MaxCUSize>>partDepth);
    Ipp32u height = (Ipp8u)(m_par->MaxCUSize>>partDepth);
    Ipp32u posx  = ((h265_scan_z2r4[absPartIdx+partAddr] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u posy  = ((h265_scan_z2r4[absPartIdx+partAddr] >> 4) << m_par->QuadtreeTULog2MinSize);

    if (m_ctbPelX + posx + width > m_par->Width)
        width = m_par->Width - posx - m_ctbPelX;
    if (m_ctbPelY + posy + height > m_par->Height)
        height = m_par->Height - posy - m_ctbPelY;

    float SCpp = m_SCpp[depth][absPartIdx];

    Ipp32s offsetLuma = posx + posy * m_pitchSrcLuma;
    PixType *pSrc = m_ySrc + offsetLuma;
    Ipp32s offsetPred = posx + posy * MAX_CU_SIZE;
    const PixType *predY = m_interPredY + offsetPred;
    Ipp32s sad = h265_SAD_MxN_special(pSrc, predY, m_pitchSrcLuma, width, height);
    float sadpp=(float)sad/(float)(width*height);
    sadpp=(sadpp*sadpp);

    if     (sadpp < 0.09*SCpp )  return 0; // Very Low
    else if(sadpp < 0.36*SCpp )  return 1; // Low
    else if(sadpp < 1.44*SCpp )  return 2; // Medium
    else if(sadpp < 3.24*SCpp )  return 3; // High
    else                         return 4; // VeryHigh
}
template <typename PixType>
Ipp32s H265CU<PixType>::GetSpatioTemporalComplexityColocated(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth, FrameData *ref) const
{
    Ipp32u width  = (Ipp8u)(m_par->MaxCUSize>>partDepth);
    Ipp32u height = (Ipp8u)(m_par->MaxCUSize>>partDepth);
    Ipp32u posx  = ((h265_scan_z2r4[absPartIdx+partAddr] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u posy  = ((h265_scan_z2r4[absPartIdx+partAddr] >> 4) << m_par->QuadtreeTULog2MinSize);


    if (m_ctbPelX + posx + width > m_par->Width)
        width = m_par->Width - posx - m_ctbPelX;
    if (m_ctbPelY + posy + height > m_par->Height)
        height = m_par->Height - posy - m_ctbPelY;

    Ipp32s log2BlkSize = m_par->Log2MaxCUSize - partDepth;
    Ipp32s pitchRsCs = m_currFrame->m_stats[0]->m_pitchRsCs4 >> (log2BlkSize-2);
    Ipp32s idx = ((m_ctbPelY+posy)>>log2BlkSize)*pitchRsCs + ((m_ctbPelX+posx)>>log2BlkSize);
    Ipp32s Rs2 = m_currFrame->m_stats[0]->m_rs[log2BlkSize-2][idx];
    Ipp32s Cs2 = m_currFrame->m_stats[0]->m_cs[log2BlkSize-2][idx];
    float Rs2pp=(float)Rs2;
    float Cs2pp=(float)Cs2;
    Rs2pp/=((width/4)*(height/4));
    Cs2pp/=((width/4)*(height/4));

    float SCpp = Rs2pp+Cs2pp;
    
    Ipp32s offsetLuma = posx + posy * m_pitchSrcLuma;
    PixType *pSrc = m_ySrc + offsetLuma;
    
    Ipp32s refOffset = m_ctbPelX + posx + (m_ctbPelY + posy) * ref->pitch_luma_pix;
    const PixType *predY = (PixType*)ref->y + refOffset;
    Ipp32s sad = h265_SAD_MxN_general(pSrc, m_pitchSrcLuma, predY, ref->pitch_luma_pix, width, height);
    float sadpp=(float)sad/(float)(width*height);
    sadpp=(sadpp*sadpp);

    if     (sadpp < 0.09*SCpp )  return 0; // Very Low
    else if(sadpp < 0.36*SCpp )  return 1; // Low
    else if(sadpp < 1.44*SCpp )  return 2; // Medium
    else if(sadpp < 3.24*SCpp )  return 3; // High
    else                         return 4; // VeryHigh
}
template <typename PixType>
Ipp32s H265CU<PixType>::GetSpatioTemporalComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth, Ipp32s& scVal)
{
    Ipp32u width  = (Ipp8u)(m_par->MaxCUSize>>partDepth);
    Ipp32u height = (Ipp8u)(m_par->MaxCUSize>>partDepth);
    Ipp32u posx  = ((h265_scan_z2r4[absPartIdx+partAddr] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u posy  = ((h265_scan_z2r4[absPartIdx+partAddr] >> 4) << m_par->QuadtreeTULog2MinSize);

    if (m_ctbPelX + posx + width > m_par->Width)
        width = m_par->Width - posx - m_ctbPelX;
    if (m_ctbPelY + posy + height > m_par->Height)
        height = m_par->Height - posy - m_ctbPelY;

    Ipp32s log2BlkSize = m_par->Log2MaxCUSize - partDepth;
    Ipp32s pitchRsCs = m_currFrame->m_stats[0]->m_pitchRsCs4 >> (log2BlkSize-2);
    Ipp32s idx = ((m_ctbPelY+posy)>>log2BlkSize)*pitchRsCs + ((m_ctbPelX+posx)>>log2BlkSize);
    Ipp32s Rs2 = m_currFrame->m_stats[0]->m_rs[log2BlkSize-2][idx];
    Ipp32s Cs2 = m_currFrame->m_stats[0]->m_cs[log2BlkSize-2][idx];
    float Rs2pp=(float)Rs2;
    float Cs2pp=(float)Cs2;
    Rs2pp/=((width/4)*(height/4));
    Cs2pp/=((width/4)*(height/4));
    float SCpp = Rs2pp+Cs2pp;

    static float lmt_sc2[10]   =   {16.0, 81.0, 225.0, 529.0, 1024.0, 1764.0, 2809.0, 4225.0, 6084.0, (float)INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
    for(Ipp32s i = 0;i<10;i++) {
        if(SCpp < lmt_sc2[i]*(float)(1<<(m_par->bitDepthLumaShift*2))) {
            scVal   =   i;
            break;
        }
    }

    Ipp32s offsetLuma = posx + posy*m_pitchSrcLuma;
    PixType *pSrc = m_ySrc + offsetLuma;
    Ipp32s offsetPred = posx + posy * MAX_CU_SIZE;
    const PixType *predY = m_interPredY + offsetPred;
    Ipp32s sad = h265_SAD_MxN_special(pSrc, predY, m_pitchSrcLuma, width, height);
    float sadpp=(float)sad/(float)(width*height);
    sadpp=sadpp*sadpp;

    if     (sadpp < 0.09*SCpp )  return 0; // Very Low
    else if(sadpp < 0.36*SCpp )  return 1; // Low
    else if(sadpp < 1.44*SCpp )  return 2; // Medium
    else if(sadpp < 3.24*SCpp )  return 3; // High
    else                         return 4; // VeryHigh
}
template <typename PixType>
Ipp8u H265CU<PixType>::EncInterLumaTuGetBaseCBF(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width)
{
    Ipp32s isRdoq = m_isRdoq;
    m_isRdoq = false;

    Ipp32s PURasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s PUStartRow = PURasterIdx >> 4;
    Ipp32s PUStartColumn = PURasterIdx & 15;
    Ipp8u cbf = 0;

    IppiSize roiSize = {width, width};
    Ipp16s *resid = m_interResidY;
    resid += (PUStartRow * MAX_CU_SIZE + PUStartColumn) << m_par->QuadtreeTULog2MinSize;

    TransformFwd(resid, MAX_CU_SIZE, m_residualsY+offset, width, m_par->bitDepthLuma, 0);
    QuantFwdTuBase(m_residualsY+offset, m_coeffWorkY+offset, absPartIdx, width, m_data[absPartIdx].qp, 1);
    cbf = !IsZero(m_coeffWorkY+offset, width * width);
    
    m_isRdoq = isRdoq;
    return cbf;
}

template <typename PixType>
void H265CU<PixType>::EncInterChromaTuGetBaseCBF(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz)
{
    Ipp32s isRdoq = m_isRdoq;
    m_isRdoq = false;
    nz[0] = nz[1] = 0;

    Ipp32s PURasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s PUStartRow = PURasterIdx >> 4;
    Ipp32s PUStartColumn = PURasterIdx & 15;
    const Ipp8u depth = m_data[absPartIdx].depth;

    IppiSize roiSize = { width, width };
    Ipp32s offsetCh = GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);

    TransformFwd(m_interResidU+offsetCh, MAX_CU_SIZE>>m_par->chromaShiftW, m_residualsU+offset, width, m_par->bitDepthChroma, 0);
    QuantFwdTu(m_residualsU+offset, m_coeffWorkU+offset, absPartIdx, width, m_data[absPartIdx].qp, 0, 0);
    nz[0] = !IsZero(m_coeffWorkU+offset, width * width);

    TransformFwd(m_interResidV+offsetCh, MAX_CU_SIZE>>m_par->chromaShiftW, m_residualsV+offset, width, m_par->bitDepthChroma, 0);
    QuantFwdTu(m_residualsV+offset, m_coeffWorkV+offset, absPartIdx, width, m_data[absPartIdx].qp, 0, 0);
    nz[1] = !IsZero(m_coeffWorkV+offset, width * width); 

    m_isRdoq = isRdoq;
}
template <typename PixType>
bool H265CU<PixType>::TuMaxSplitInterHasNonZeroCoeff(Ipp32u absPartIdx, Ipp8u trIdxMax)
{
    Ipp8u depth = m_data[absPartIdx].depth;
    Ipp32u numParts = ( m_par->NumPartInCU >> ((depth + 0)<<1) ); // in TU
    Ipp32u num_tr_parts = ( m_par->NumPartInCU >> ((depth + trIdxMax)<<1) );
    Ipp32s width = m_data[absPartIdx].size >>trIdxMax;

    Ipp8u nz[3] = {0};
    Ipp8u nzt[3];

    //for (Ipp32u i = 0; i < numParts; i++) m_data[absPartIdx + i].trIdx = trIdxMax;
    for (Ipp32u pos = 0; pos < numParts; ) {
        nzt[0] = EncInterLumaTuGetBaseCBF(absPartIdx + pos, (absPartIdx + pos)*16, width);
        nz[0] |= nzt[0];
        pos += num_tr_parts;
        if(nz[0]) return true;
    }

    if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && !m_par->chroma422) {
        // doesnot handle anything other than 4:2:0 for now -NG
        if (width == 4) {
            num_tr_parts <<= 2;
            width <<= 1;
        }
        for (Ipp32u pos = 0; pos < numParts; ) {
            EncInterChromaTuGetBaseCBF(absPartIdx + pos, ((absPartIdx + pos)*16)>>2, width>>1, &nzt[1]);
            nz[1] |= nzt[1];
            nz[2] |= nzt[2];
            pos += num_tr_parts;
            if(nz[1] || nz[2]) return true;
        }
    } else {
        nz[1] = nz[2] = 0;
    }
    return false;
}


inline Ipp32s MvCost(Ipp16s mvd)
{
#if defined(_WIN32) || defined(_WIN64)
    unsigned long index;
#else
    unsigned int index;
#endif
    unsigned char dst = _BitScanReverse(&index, abs(mvd));
    return (mvd == 0) ? 1 : 2 * index + 3;
}


template <typename PixType>
bool H265CU<PixType>::tryIntraICRA(Ipp32s absPartIdx, Ipp32s depth)
{
    if(m_cslice->slice_type == I_SLICE) return true;
    // Inter
    Ipp32s widthCu = (m_par->MaxCUSize >> depth);
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );
    const H265CUData *dataBestInter = GetBestCuDecision(absPartIdx, depth);
    Ipp32s numPu = h265_numPu[dataBestInter->partSize];
    // Motion Analysis
    Ipp32s mvdMax=0, mvdCost=0;
    
    for (Ipp32s i = 0; i < numPu; i++) {
        Ipp32s partAddr;
        GetPartAddr(i, dataBestInter->partSize, numParts, partAddr);
        if(!dataBestInter[partAddr].flags.mergeFlag && !dataBestInter[partAddr].flags.skippedFlag)
        {
            if(dataBestInter[partAddr].interDir & INTER_DIR_PRED_L0) {
                mvdCost += MvCost(dataBestInter[partAddr].mvd[0].mvx);
                mvdCost += MvCost(dataBestInter[partAddr].mvd[0].mvy);
            }
            if(dataBestInter[partAddr].interDir & INTER_DIR_PRED_L1) {
                mvdCost += MvCost(dataBestInter[partAddr].mvd[1].mvx);
                mvdCost += MvCost(dataBestInter[partAddr].mvd[1].mvy);
            }

            if(abs(dataBestInter[partAddr].mvd[0].mvx)>mvdMax)
                mvdMax = abs(dataBestInter[partAddr].mvd[0].mvx);
            if(abs(dataBestInter[partAddr].mvd[0].mvy)>mvdMax)
                mvdMax = abs(dataBestInter[partAddr].mvd[0].mvy);
            if(abs(dataBestInter[partAddr].mvd[1].mvx)>mvdMax)
                mvdMax = abs(dataBestInter[partAddr].mvd[1].mvx);
            if(abs(dataBestInter[partAddr].mvd[1].mvy)>mvdMax)
                mvdMax = abs(dataBestInter[partAddr].mvd[1].mvy);
        }
    }
    m_mvdMax = mvdMax;
    m_mvdCost = mvdCost;
    // CBF Condition
    bool tryIntra = true;
    if(m_par->AnalyseFlags & HEVC_COST_CHROMA) {
        Ipp32s idx422 = m_par->chroma422 ? (m_par->NumPartInCU >> (2 * depth + 1)) : 0;
        tryIntra = !IsZeroCbf(dataBestInter, idx422);
    } else {
        tryIntra = dataBestInter->cbf[0] !=0;
    }
    // CBF Check
    if(!tryIntra && m_isRdoq && m_SCid[depth][absPartIdx]<2)
    {
        // check if all cbf are really zero for low scene complexity
        Ipp8u tr_depth_min, tr_depth_max;

        GetTrDepthMinMax(m_par, depth, PART_SIZE_2Nx2N, &tr_depth_min, &tr_depth_max);

        if(dataBestInter->flags.skippedFlag==1) {
            CoeffsType *residY = m_interResidY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
            const PixType *srcY = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchRecLuma);
            const PixType *predY = m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
            TuDiff(residY, MAX_CU_SIZE, srcY, m_pitchSrcLuma, predY, MAX_CU_SIZE, widthCu);
            if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && !m_par->chroma422) {
                const PixType *srcC = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchRecChroma);
                const PixType *predC = m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
                CoeffsType *residU = m_interResidU + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
                CoeffsType *residV = m_interResidV + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
                h265_DiffNv12(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv,
                              residU, MAX_CU_SIZE >> m_par->chromaShiftW, residV, MAX_CU_SIZE >> m_par->chromaShiftW,
                              widthCu >> m_par->chromaShiftW, widthCu >> m_par->chromaShiftH);
            }
        }
        tryIntra = TuMaxSplitInterHasNonZeroCoeff(absPartIdx, tr_depth_max);
    }
    // ICRA and Motion analysis
    if(tryIntra) 
    {
        Ipp32s SCid = m_SCid[depth][absPartIdx];
        Ipp32s STC  = m_STC[depth][absPartIdx];
        Ipp32s subSC[4] ={SCid, SCid, SCid, SCid};
        Ipp32s subSTC[4]={STC, STC, STC, STC};
        Ipp32s minSC    =  SCid;
        bool goodPred = false;
        bool tryProj = false;
        // 8x8 is also 4x4 intra
        if(widthCu==8) {
            for (Ipp32s i = 0; i < 4; i++) {
                Ipp32s partAddr = (numParts >> 2) * i;
                subSTC[i] = GetSpatioTemporalComplexity(absPartIdx, depth, partAddr, depth+1, subSC[i]);
                if(subSC[i]<minSC) minSC=subSC[i];
            }
            goodPred = subSTC[0]<1 && subSTC[1]<1 && subSTC[2]<1 && subSTC[3]<1;
            tryProj  = subSTC[0]<2 && subSTC[1]<2 && subSTC[2]<2 && subSTC[3]<2;
        } else {
            goodPred = STC<1;
            tryProj  = STC<2;
        }

        if(       minSC<5 && mvdMax>15)  {
            tryIntra = true;
        } else if(minSC<6 && mvdMax>31)  {
            tryIntra = true;
        } else if(minSC<7 && mvdMax>63)  {
            tryIntra = true;
        } else if(           mvdMax>127) {
            tryIntra = true;
        } else if(goodPred)  {
            tryIntra = false;
        } else if(tryProj)   {
            // Projection Analysis
            bool foundIntra = false;

            RefPicList *refPicList = m_currFrame->m_refPicList;
            Frame *ref0 = refPicList[0].m_refFrames[0];
            Ipp32s cuDataColOffset = (m_ctbAddr << m_par->Log2NumPartInCU);
            const H265CUData *col0 = refPicList[0].m_refFrames[0]->cu_data + cuDataColOffset;
            Frame *ref1 = (m_cslice->slice_type == B_SLICE)? refPicList[1].m_refFrames[0] : NULL;
            const H265CUData *col1 = (m_cslice->slice_type == B_SLICE)? refPicList[1].m_refFrames[0]->cu_data + cuDataColOffset : NULL;
            if(ref0 == ref1) {
                if(ref0->m_picCodeType==MFX_FRAMETYPE_I) {
                    foundIntra = true;
                } else {
                    for (Ipp32s i = absPartIdx; i < absPartIdx+(Ipp32s)numParts && !foundIntra; i ++) {
                        if (col0 && col0[i].predMode==MODE_INTRA) foundIntra = true; 
                    }
                }
            } else if((ref0&&ref0->m_picCodeType!=MFX_FRAMETYPE_I) || (ref1&&ref1->m_picCodeType!=MFX_FRAMETYPE_I)) {
                for (Ipp32s i = absPartIdx; i < absPartIdx+(Ipp32s)numParts && !foundIntra; i ++) {
                    if (ref0->m_picCodeType!=MFX_FRAMETYPE_I && col0 && col0[i].predMode==MODE_INTRA) foundIntra = true; 
                    if (ref1 && ref1->m_picCodeType!=MFX_FRAMETYPE_I && col1 && col1[i].predMode==MODE_INTRA) foundIntra = true;
                }
            } else {
                foundIntra = true;
            }
            if(foundIntra==false) tryIntra = false;
        } 
    } else {
        if(mvdMax>15) tryIntra = true;
    }
    return tryIntra;
}
#endif

#ifdef MEMOIZE_SUBPEL
template <typename PixType>
void H265CU<PixType>::MemoizeInit() 
{
    Ipp32s j,k,l;
    for(j=0;j<m_currFrame->m_numRefUnique;j++) {
        m_memBestMV[3][j].Init();
        m_memBestMV[2][j].Init();
        m_memBestMV[1][j].Init();
        for(k=0;k<4;k++) {
            for(l=0;l<4;l++) {
                m_memSubpel[3][j][k][l].Init(&(m_predBufHi3[j][k][l][0]), &(m_predBuf3[j][k][l][0]));
                m_memSubpel[2][j][k][l].Init(&(m_predBufHi2[j][k][l][0]), &(m_predBuf2[j][k][l][0]));
                m_memSubpel[1][j][k][l].Init(&(m_predBufHi1[j][k][l][0]), &(m_predBuf1[j][k][l][0]));
                m_memSubpel[0][j][k][l].Init(&(m_predBufHi0[j][k][l][0]), &(m_predBuf0[j][k][l][0]));
                if(m_par->hadamardMe>=2) {
                    m_memSubHad[3][j][k][l].Init(&(m_satd8Buf3[j][k][l][0][0]), &(m_satd8Buf3[j][k][l][1][0]), &(m_satd8Buf3[j][k][l][2][0]), &(m_satd8Buf3[j][k][l][3][0]));
                    m_memSubHad[2][j][k][l].Init(&(m_satd8Buf2[j][k][l][0][0]), &(m_satd8Buf2[j][k][l][1][0]), &(m_satd8Buf2[j][k][l][2][0]), &(m_satd8Buf2[j][k][l][3][0]));
                    m_memSubHad[1][j][k][l].Init(&(m_satd8Buf1[j][k][l][0][0]), &(m_satd8Buf1[j][k][l][1][0]), &(m_satd8Buf1[j][k][l][2][0]), &(m_satd8Buf1[j][k][l][3][0]));
                }
            }
        }
    }
#ifdef MEMOIZE_CAND
    m_memCandSubHad[0].Init();
    m_memCandSubHad[1].Init();
    m_memCandSubHad[2].Init();
    m_memCandSubHad[3].Init();
    for(k=0;k<MEMOIZE_NUMCAND;k++) {
#ifdef MEMOIZE_CAND_SUBPEL
        m_memCandSubHad[0].Init(k, &(m_satd8CandBuf0[k][0]), &(m_predBufCand0[k][0]));
        m_memCandSubHad[1].Init(k, &(m_satd8CandBuf1[k][0]), &(m_predBufCand1[k][0]));
        m_memCandSubHad[2].Init(k, &(m_satd8CandBuf2[k][0]), &(m_predBufCand2[k][0]));
        m_memCandSubHad[3].Init(k, &(m_satd8CandBuf3[k][0]), &(m_predBufCand3[k][0]));
#else
        m_memCandSubHad[0].Init(k, &(m_satd8CandBuf0[k][0]));
        m_memCandSubHad[1].Init(k, &(m_satd8CandBuf1[k][0]));
        m_memCandSubHad[2].Init(k, &(m_satd8CandBuf2[k][0]));
        m_memCandSubHad[3].Init(k, &(m_satd8CandBuf3[k][0]));
#endif
    }
#endif

}
template <typename PixType>
void H265CU<PixType>::MemoizeClear(Ipp8u depth) 
{
    Ipp32s memSize = (m_par->Log2MaxCUSize-3-depth);
    for(Ipp32s j=0;j<m_currFrame->m_numRefUnique;j++) {
        m_memBestMV[memSize][j].Init();
        for(Ipp32s k=0;k<4;k++) {
            for(Ipp32s l=0;l<4;l++) {
                m_memSubpel[memSize][j][k][l].Init();
                if(m_par->hadamardMe>=2 && memSize>0) { 
                    m_memSubHad[memSize][j][k][l].Init();
                }
            }
        }
    }
#ifdef MEMOIZE_CAND
    m_memCandSubHad[memSize].Init();
#endif
}
#endif


#ifdef AMT_ADAPTIVE_INTER_MIN_DEPTH
template <typename PixType>
bool H265CU<PixType>::JoinCU(Ipp32s absPartIdx, Ipp32s depth) 
{
    Ipp8u skipped = (m_data[absPartIdx].depth==depth+1 && m_data[absPartIdx].predMode==MODE_INTER && m_data[absPartIdx].flags.skippedFlag)?1:0;
    Ipp8s refIdx[2] = {m_data[absPartIdx].refIdx[0], m_data[absPartIdx].refIdx[1]};
    H265MV mv[2]    = {m_data[absPartIdx].mv[0]    , m_data[absPartIdx].mv[1]    };
    Ipp32s candBest = -1;
    Ipp32u numParts = (m_par->NumPartInCU >> (2 * depth)) >> 2;
    Ipp32s absPartIdxSplit = absPartIdx + numParts;
    if (refIdx[0] >= 0 && refIdx[1] >= 0) {
        for (Ipp32s i = 1; i < 4 && skipped; i++, absPartIdxSplit += numParts) {
            skipped &= (m_data[absPartIdxSplit].depth==depth+1 && m_data[absPartIdxSplit].predMode==MODE_INTER && m_data[absPartIdxSplit].flags.skippedFlag)?1:0;
            if(!skipped                                        ||
                refIdx[0] != m_data[absPartIdxSplit].refIdx[0] ||
                refIdx[1] != m_data[absPartIdxSplit].refIdx[1] ||
                mv[0]     != m_data[absPartIdxSplit].mv[0]     ||
                mv[1]     != m_data[absPartIdxSplit].mv[1]) {
                    skipped=0;
            }
        }
    } else {
        Ipp32s listIdx = (refIdx[1] >= 0);
        for (Ipp32s i = 1; i < 4 && skipped; i++, absPartIdxSplit += numParts) {
            skipped &= (m_data[absPartIdxSplit].depth==depth+1 && m_data[absPartIdxSplit].predMode==MODE_INTER && m_data[absPartIdxSplit].flags.skippedFlag)?1:0;
            if(!skipped                                        ||
                refIdx[0] != m_data[absPartIdxSplit].refIdx[0] ||
                refIdx[1] != m_data[absPartIdxSplit].refIdx[1] ||
                mv[listIdx]     != m_data[absPartIdxSplit].mv[listIdx]     ) {
                    skipped=0;
            }
        }
    }
    if(skipped) {
        Ipp32s cuWidth = (m_par->MaxCUSize >> depth);
        Ipp32s cuWidthInMinTU = (cuWidth >> m_par->QuadtreeTULog2MinSize);
        GetMergeCand(absPartIdx, PART_SIZE_2Nx2N, 0, cuWidthInMinTU, m_mergeCand);
        Ipp32s numRefLists = 1 + (m_cslice->slice_type == B_SLICE);
        
        // setup merge flag and index
        for (Ipp8u i = 0; i < m_mergeCand[0].numCand; i++) {
            if (IsCandFound(refIdx, mv, m_mergeCand, i, numRefLists)) {
                candBest = i;
                break;
            }
        }
        if(candBest<0) skipped = 0;
    }
    if(skipped) {
        Ipp8u interDir = m_data[absPartIdx].interDir;
        memset(m_data + absPartIdx, 0, sizeof(H265CUData));
        m_data[absPartIdx].mergeIdx  = candBest;
        m_data[absPartIdx].interDir  = interDir;
        m_data[absPartIdx].refIdx[0] = refIdx[0];
        m_data[absPartIdx].refIdx[1] = refIdx[1];
        m_data[absPartIdx].mv[0]     = mv[0];
        m_data[absPartIdx].mv[1]     = mv[1];
        m_data[absPartIdx].depth = depth;
        m_data[absPartIdx].size = (Ipp8u)(m_par->MaxCUSize >> depth);
        m_data[absPartIdx].qp = m_lcuQps[m_ctbAddr];
        m_data[absPartIdx].flags.mergeFlag = 1;
        m_data[absPartIdx].flags.skippedFlag = 1;
        PropagateSubPart(m_data + absPartIdx, numParts*4);
        return true;
    }
    return false;
}
#endif

template <typename PixType>
CostType H265CU<PixType>::ModeDecision(Ipp32s absPartIdx, Ipp8u depth)
{
    if (absPartIdx == 0 && depth == 0) {
        if (m_par->UseDQP)
            setdQPFlag(true);
        std::fill_n(m_costStored, sizeof(m_costStored) / sizeof(m_costStored[0]), COST_MAX);
        m_costCurr = 0.0;
        // setup CU QP - moved on InitCu stage
        //FillSubPartCuQp_(m_data, m_par->NumPartInCU, m_lcuQps[m_ctbAddr]);
    }

    Ipp32u left = m_ctbPelX + ((h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u top  = m_ctbPelY + ((h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize);
    if (left >= m_par->Width || top >= m_par->Height)
        return CostType(0); // CU is out of picture

    // get split mode
    Ipp8u splitMode = SPLIT_NONE;
    if (depth < m_par->MaxCUDepth - m_par->AddCUDepth) {
        Ipp32u right = left + (m_par->MaxCUSize >> depth) - 1;
        Ipp32u bottom = top + (m_par->MaxCUSize >> depth) - 1;
        if (right >= m_par->Width || bottom >= m_par->Height ||
            m_par->Log2MaxCUSize - depth - (m_par->QuadtreeTUMaxDepthIntra - 1) > m_par->QuadtreeTULog2MaxSize) {
            splitMode = SPLIT_MUST;
        }
        else if (m_par->Log2MaxCUSize - depth > m_par->QuadtreeTULog2MinSize) {
            splitMode = SPLIT_TRY;
        }
    }
    Ipp8u splitModeOrig = splitMode;

#if defined(AMT_ICRA_OPT)
    if(depth) GetSpatialComplexity(absPartIdx, depth, 0, depth);

#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    if(depth && m_par->minCUDepthAdapt && splitMode == SPLIT_TRY && (m_par->Log2MaxCUSize - depth)==5) {
        Ipp32s maxSC;
        m_intraMinDepth = GetAdaptiveIntraMinDepth(absPartIdx, depth, maxSC);
    }
#endif
#endif
    
    // adaptively bypass non-split case
    if (splitMode == SPLIT_TRY && depth < m_adaptMinDepth)
        splitMode = SPLIT_MUST;
    
    if (depth == m_adaptMinDepth)
        m_projMinDepth = m_adaptMinDepth;
    
    if (splitMode == SPLIT_TRY && depth < m_projMinDepth)
        splitMode = SPLIT_MUST;

#ifdef AMT_ICRA_OPT
    if (splitMode == SPLIT_TRY && depth >= m_adaptMaxDepth)
        splitMode = SPLIT_NONE;
#endif


#ifdef MEMOIZE_SUBPEL
    MemoizeClear(depth);
#endif
    // before checking something at current level save initial states
    CostType costInitial = m_costCurr;
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
    m_bsf->CtxSave(ctxInitial);
    // and initialize storage
    m_costStored[depth] = COST_MAX;
#ifdef AMT_ALT_ENCODE
    m_interPredY = m_interPredBufsY[depth][0];
    m_interPredC = m_interPredBufsC[depth][0];
    m_interResidY = m_interResidBufsY[depth][0];
    m_interResidU = m_interResidBufsU[depth][0];
    m_interResidV = m_interResidBufsV[depth][0];

    m_interPredBestY = m_interPredBufsY[depth][1];
    m_interPredBestC = m_interPredBufsC[depth][1];
    m_interResidBestY = m_interResidBufsY[depth][1];
    m_interResidBestU = m_interResidBufsU[depth][1];
    m_interResidBestV = m_interResidBufsV[depth][1];
#endif

    bool tryIntra = true;
    CostType costChroma = 0;
    // try non-split INTER and INTRA
    if (splitMode != SPLIT_MUST) {
        if (m_cslice->slice_type != I_SLICE) {

            MeCu(absPartIdx, depth); // Inter modes are first to check for P and B slices

            LoadBestInterPredAndResid(absPartIdx, depth);

            const H265CUData *dataBestInter = GetBestCuDecision(absPartIdx, depth);
            Ipp32s idx422 = m_par->chroma422 ? (m_par->NumPartInCU >> (2 * depth + 1)) : 0;
#ifdef AMT_VQ_TUNE
            bool exitOnCbf = false;
            m_STC[depth][absPartIdx] = GetSpatioTemporalComplexity(absPartIdx, depth, 0, depth);
            Ipp32s goodSTC=1+m_currFrame->m_isRef?0:1;
            if(m_STC[depth][absPartIdx]<goodSTC || m_SCid[depth][absPartIdx]<2) exitOnCbf = true;       // Good Pred

            if ((m_par->fastSkip && dataBestInter->flags.skippedFlag) || (m_par->fastCbfMode && IsZeroCbf(dataBestInter, idx422) && exitOnCbf))
#else
            if (m_par->fastCbfMode && IsZeroCbf(dataBestInter, idx422) ||
                m_par->fastSkip && dataBestInter->flags.skippedFlag) 
#endif
            {
                // early exit: found good enough decision
                // no Intra and further split
#ifdef AMT_CHROMA_GUIDED_INTER
                if(!HaveChromaRec()) {
                    LoadWorkingCuDecision(absPartIdx, depth);
                    //CostType costChroma;
                    EncAndRecChroma(absPartIdx, absPartIdx << (4 - m_par->chromaShift), depth, &costChroma, INTER_PRED_IN_BUF);
                    m_costCurr += costChroma;
                    m_costStored[depth] = COST_MAX;
                }
                LoadFinalCuDecision(absPartIdx, depth, true);
#else
                LoadFinalCuDecision(absPartIdx, depth, false);
#endif
                return m_costCurr - costInitial;
            }
            
#if defined(AMT_ICRA_OPT)
#ifndef AMT_VQ_TUNE
#ifndef AMT_THRESHOLDS
            if(m_par->TryIntra>=2 || m_par->maxCUDepthAdapt)
#endif
            {
                Ipp32s STC =  GetSpatioTemporalComplexity(absPartIdx, depth, 0, depth);
                m_STC[depth][absPartIdx] = STC;
            }
#endif
            if(m_par->maxCUDepthAdapt) {
                GetProjectedDepth(absPartIdx, depth, splitMode);
            }
#endif
        }

        if(tryIntra) tryIntra = (m_cuIntraAngMode != INTRA_ANG_MODE_DISABLE) && CheckGpuIntraCost(absPartIdx, depth);

#ifdef AMT_ICRA_OPT
        if(tryIntra && m_par->TryIntra>=2) tryIntra = tryIntraICRA(absPartIdx, depth);
#endif
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
        if (m_cslice->slice_type != I_SLICE) {
            const H265CUData *dataBestNonSplit = GetBestCuDecision(absPartIdx, depth);
            Ipp32s qp_best = dataBestNonSplit->qp;

            if(m_par->DeltaQpMode) {
                qp_best = m_currFrame->m_sliceQpY;
            }

            CostType cuSplitThresholdCu = m_par->cu_split_threshold_cu[Saturate(0,51,qp_best)][m_cslice->slice_type != I_SLICE][depth];
            CostType costBestNonSplit = IPP_MIN(m_costStored[depth], m_costCurr) - costInitial;
            if(tryIntra && splitMode == SPLIT_TRY && depth < m_intraMinDepth && costBestNonSplit>=cuSplitThresholdCu) tryIntra = false;
        }
#endif

        // hint from lookahead: (marked) RASL (due to scenecut) w/o intra could introduce strong artifact
        if (m_currFrame->m_forceTryIntra && !tryIntra) {
            tryIntra = true;
        }

        if (tryIntra) {

            if (!m_par->enableCmFlag && m_cuIntraAngMode == INTRA_ANG_MODE_GRADIENT && !m_isHistBuilt) { 
                h265_AnalyzeGradient(m_ySrc, m_pitchSrcLuma, m_hist4, m_hist8, m_par->MaxCUSize, m_par->MaxCUSize);
                m_isHistBuilt = true;
            }

            if (m_cslice->slice_type != I_SLICE) {
                // Inter modes decision is already made (for P and B slices)
                // save the decision and restore initial states
                SaveFinalCuDecision(absPartIdx, depth, false);
                m_bsf->CtxRestore(ctxInitial);
                m_costCurr = costInitial;
            }

            CheckIntra(absPartIdx, depth);
        }
#ifdef AMT_CHROMA_GUIDED_INTER
        if(!HaveChromaRec()) {
            LoadWorkingCuDecision(absPartIdx, depth);
            //CostType costChroma;
            EncAndRecChroma(absPartIdx, absPartIdx << (4 - m_par->chromaShift), depth, &costChroma, INTER_PRED_IN_BUF);
            m_costCurr += costChroma;
            m_costStored[depth] = COST_MAX;
        }
#endif
    }

    Ipp8u skippedFlag = 0;
    CostType cuSplitThresholdCu = 0;
    CostType costBestNonSplit = IPP_MIN(m_costStored[depth], m_costCurr) - costInitial;
#ifdef AMT_CHROMA_GUIDED_INTER
    if(m_par->chroma422) costBestNonSplit-= costChroma/2; // Not Tuned for excessive chroma cost
#endif
    if (splitMode == SPLIT_TRY) {
        const H265CUData *dataBestNonSplit = GetBestCuDecision(absPartIdx, depth);
        skippedFlag = dataBestNonSplit->flags.skippedFlag;
        Ipp32s qp_best = dataBestNonSplit->qp;

        if(m_par->DeltaQpMode) {
            qp_best = m_currFrame->m_sliceQpY;
        }

#ifdef AMT_THRESHOLDS
        if(m_cslice->slice_type == I_SLICE && m_par->SplitThresholdStrengthCUIntra > m_par->SplitThresholdStrengthTUIntra) {
            if(m_SCid[depth][absPartIdx]<5) {
                cuSplitThresholdCu = m_par->cu_split_threshold_cu[Saturate(0,51,qp_best-2)][m_cslice->slice_type != I_SLICE][depth];
            } else {
                cuSplitThresholdCu = m_par->cu_split_threshold_cu[Saturate(0,51,qp_best)][m_cslice->slice_type != I_SLICE][depth];
            }
        } else if(m_cslice->slice_type != I_SLICE && m_par->SplitThresholdStrengthCUInter > m_par->SplitThresholdStrengthTUIntra) {
            if(m_SCid[depth][absPartIdx]<5 && tryIntra) {
                cuSplitThresholdCu = m_par->cu_split_threshold_cu[Saturate(0,51,qp_best-2)][m_cslice->slice_type != I_SLICE][depth];
            } else if(m_STC[depth][absPartIdx]<1 || m_SCid[depth][absPartIdx]<5) {
                cuSplitThresholdCu = m_par->cu_split_threshold_cu[Saturate(0,51,qp_best-1)][m_cslice->slice_type != I_SLICE][depth];
            } else if(m_STC[depth][absPartIdx]>3)    {
                cuSplitThresholdCu = m_par->cu_split_threshold_cu[Saturate(0,51,qp_best+1)][m_cslice->slice_type != I_SLICE][depth];
            } else      {
                cuSplitThresholdCu = m_par->cu_split_threshold_cu[Saturate(0,51,qp_best)][m_cslice->slice_type != I_SLICE][depth];
            }
        } else
#endif
        {
        cuSplitThresholdCu = m_par->cu_split_threshold_cu[Saturate(0,51,qp_best)][m_cslice->slice_type != I_SLICE][depth];
        }

        if (m_par->cuSplitThreshold > 0) {
            Ipp64u costNeigh = 0, costCur = 0, numNeigh = 0, numCur = 0;
            Ipp64f costAvg = 0;
            costStat* cur = m_costStat + m_ctbAddr;

            costCur += cur->cost[depth] * cur->num[depth];
            numCur += cur->num[depth];
            if (m_aboveAddr >= m_cslice->slice_segment_address && m_aboveSameTile) {
                costStat* above = m_costStat + m_aboveAddr;
                costNeigh += above->cost[depth] * above->num[depth];
                numNeigh += above->num[depth];
            }
            if (m_aboveLeftAddr >= m_cslice->slice_segment_address && m_aboveLeftSameTile) {
                costStat* aboveLeft = m_costStat + m_aboveLeftAddr;
                costNeigh += aboveLeft->cost[depth] * aboveLeft->num[depth];
                numNeigh += aboveLeft->num[depth];
            }
            if (m_aboveRightAddr >= m_cslice->slice_segment_address && m_aboveRightSameTile) {
                costStat* aboveRight =  m_costStat + m_aboveRightAddr;
                costNeigh += aboveRight->cost[depth] * aboveRight->num[depth];
                numNeigh += aboveRight->num[depth];
            }
            if (m_leftAddr >= m_cslice->slice_segment_address && m_leftSameTile) {
                costStat* left =  m_costStat + m_leftAddr;
                costNeigh += left->cost[depth] * left->num[depth];
                numNeigh += left->num[depth];
            }

            if (numNeigh + numCur)
                costAvg = ((0.61 * costCur) + (0.39 * costNeigh)) / ((0.61 * numCur) + (0.39 * numNeigh));

            if (costBestNonSplit < (m_par->cuSplitThreshold / 256.0) * costAvg && costAvg != 0 && depth != 0)
                splitMode = SPLIT_NONE;
        }
    }

    bool trySplit = (costBestNonSplit >= cuSplitThresholdCu) &&
                    (splitMode != SPLIT_NONE) &&
                   !(skippedFlag && m_par->cuSplit == 2);

    if (trySplit) {
        if (splitMode != SPLIT_MUST) {
            // some decision is already made, save it and restore the initial states
#ifdef AMT_CHROMA_GUIDED_INTER
            SaveFinalCuDecision(absPartIdx, depth, true);
#else
            SaveFinalCuDecision(absPartIdx, depth, false);
#endif
            m_bsf->CtxRestore(ctxInitial);
            m_costCurr = costInitial;
        }

        // split
        CostType costSplit = 0;
        Ipp32s absPartIdxSplit = absPartIdx;
        Ipp32u numParts = (m_par->NumPartInCU >> (2 * depth)) >> 2;
        for (Ipp32s i = 0; i < 4; i++, absPartIdxSplit += numParts) {
            CostType costPart = ModeDecision(absPartIdxSplit, depth + 1);
            costSplit += costPart;

            if (m_par->cuSplitThreshold > 0 && m_data[absPartIdxSplit].predMode != MODE_INTRA && costPart > 0.0) {
                costStat *cur = m_costStat + m_ctbAddr;
                Ipp64u costTotal = cur->cost[depth + 1] * cur->num[depth + 1];
                cur->num[depth + 1]++;
                cur->cost[depth + 1] = (costTotal + costPart) / cur->num[depth + 1];
            }

        }
#ifdef AMT_ADAPTIVE_INTER_MIN_DEPTH
        if(splitMode==SPLIT_MUST && splitModeOrig==SPLIT_TRY) {
            if(JoinCU(absPartIdx, depth)) {
                return m_costCurr - costInitial;
            }
        }
#endif
        // add cost of cu split flag to costSplit
        if (splitMode != SPLIT_MUST)
            m_costCurr += GetCuSplitFlagCost(m_data, absPartIdx, depth);

#ifdef DUMP_COSTS_CU
        if (splitMode != SPLIT_MUST) {
            printCostStat(fp_cu, m_par->QP,m_cslice->slice_type != I_SLICE, widthCu,m_costStored[depth],costSplit);
        }
#endif

        if (m_par->cuSplitThreshold > 0 && splitMode == SPLIT_TRY && depth == 0 && costBestNonSplit > 0.0) {
            costStat *cur = m_costStat + m_ctbAddr;
            Ipp64u costTotal = cur->cost[depth] * cur->num[depth];
            cur->num[depth]++;
            cur->cost[depth] = (costTotal + costBestNonSplit) / cur->num[depth];
        }

    }


    // keep best decision
#ifdef AMT_CHROMA_GUIDED_INTER
    LoadFinalCuDecision(absPartIdx, depth, true);
#else
    LoadFinalCuDecision(absPartIdx, depth, false);
#endif
    return m_costCurr - costInitial;
}


template <typename PixType>
void H265CU<PixType>::CalcCostLuma(Ipp32s absPartIdx, Ipp8u depth, Ipp8u trDepth, CostOpt costOpt, IntraPredOpt intraPredOpt)
{
    Ipp32s widthCu = m_par->MaxCUSize >> depth;
    Ipp32s width = widthCu >> trDepth;
    Ipp32u numParts = m_par->NumPartInCU >> (2 * (depth + trDepth));

    Ipp8u splitMode = GetTrSplitMode(absPartIdx, depth, trDepth, m_data[absPartIdx].partSize);
    if ((costOpt == COST_PRED_TR_0 || costOpt == COST_REC_TR_0) && splitMode != SPLIT_MUST)
        splitMode = SPLIT_NONE;

    // save initial states if we are going to test split
    CostType costInitial = m_costCurr;
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
    if (splitMode != SPLIT_NONE)
        m_bsf->CtxSave(ctxInitial);
    // init storage for trDepth>initTrDepth
    // when trDepth=initTrDepth storage is occupied by current best intra luma decision
    Ipp32s initTrDepth = (m_data[absPartIdx].partSize == PART_SIZE_NxN);
    if (trDepth > initTrDepth)
        m_costStored[depth + trDepth + 1] = COST_MAX;

    CostType costNoSplit = COST_MAX;
    if (splitMode != SPLIT_MUST) {
        if(intraPredOpt!=INTRA_PRED_CALC) intraPredOpt = trDepth == initTrDepth ? INTRA_PRED_IN_BUF : INTRA_PRED_CALC;
        Ipp8u costPredFlag = (costOpt == COST_PRED_TR_0);

        FillSubPartTrDepth_(m_data + absPartIdx, numParts, trDepth);
        if(width>4) {
            m_data[absPartIdx].cbf[1] = m_data[absPartIdx].cbf[2] = 0; // clear chroma cbf since it affects PutTransform for Luma
            if(m_par->chroma422) m_data[absPartIdx+(numParts>>1)].cbf[1] = m_data[absPartIdx+(numParts>>1)].cbf[2] = 0;
        }
        EncAndRecLumaTu(absPartIdx, absPartIdx * 16, width, &costNoSplit, costPredFlag, intraPredOpt);
        FillSubPartCbfY_(m_data + absPartIdx + 1, numParts - 1, m_data[absPartIdx].cbf[0]);
        if (m_data[absPartIdx].cbf[0] == 0)
            splitMode = SPLIT_NONE;
        m_costCurr += costNoSplit;
    }

    CostType cuSplitThresholdTu = 0;

    if (splitMode == SPLIT_TRY) {
        cuSplitThresholdTu = m_par->cu_split_threshold_tu[m_data[absPartIdx].qp][0][depth + trDepth];
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
        // 8x8 thresholds set too high, in dc planar mode split is prefferable than NXN pu
        if (m_cuIntraAngMode==INTRA_ANG_MODE_DC_PLANAR_ONLY && width==8) cuSplitThresholdTu = 0;
#endif
    }

    if (costNoSplit >= cuSplitThresholdTu && splitMode != SPLIT_NONE) {
        if (splitMode != SPLIT_MUST) {
            // if trying split after non-split
            // save decision made for non-split and restore initial states
            SaveIntraLumaDecision(absPartIdx, depth + trDepth);
            m_bsf->CtxRestore(ctxInitial);
            m_costCurr = costInitial;
        }

        // try transform split
        if (splitMode != SPLIT_MUST)
            m_costCurr += GetTransformSubdivFlagCost(depth, trDepth);

        Ipp32s absPartIdxSplit = absPartIdx;
        Ipp32s nz = 0;
        for (Ipp32s i = 0; i < 4; i++, absPartIdxSplit += (numParts >> 2)) {
            CalcCostLuma(absPartIdxSplit, depth, trDepth + 1, costOpt, intraPredOpt);
            nz |= m_data[absPartIdxSplit].cbf[0];
        }

        if (nz)
            for (Ipp32u i = 0; i < 4; i++)
                SetCbfBit<0>(m_data + absPartIdx + (numParts * i >> 2), trDepth);

#ifdef DUMP_COSTS_TU
        if (splitMode != SPLIT_MUST) {
            printCostStat(fp_tu, m_par->m_lcuQps[m_ctbAddr], 0, width,costBest,costSplit);
        }
#endif
        // keep the best decision
        LoadIntraLumaDecision(absPartIdx, depth + trDepth);
    }
}

// absPartIdx - for minimal TU
template <typename PixType>
void H265CU<PixType>::DetailsXY(H265MEInfo* me_info) const
{
    Ipp32s width = me_info->width;
    Ipp32s height = me_info->height;
    Ipp32s ctbOffset = me_info->posx + me_info->posy * m_pitchSrcLuma;
    Ipp32s dx = 0, dy = 0;

    PixType *pSrc = m_ySrc + ctbOffset;
    for(Ipp32s y=0; y<height; y++) {
        for(Ipp32s x=0; x<width-1; x++)
            dx += (pSrc[x]-pSrc[x+1]) * (pSrc[x]-pSrc[x+1]);
        pSrc += m_pitchSrcLuma;
    }

    pSrc = m_ySrc + ctbOffset;
    for(Ipp32s y=0; y<height-1; y++) {
        for(Ipp32s x=0; x<width; x++)
            dy += (pSrc[x]-pSrc[x+m_pitchSrcLuma]) * (pSrc[x]-pSrc[x+m_pitchSrcLuma]);
        pSrc += m_pitchSrcLuma;
    }
}


template <typename PixType>
Ipp32s H265CU<PixType>::MvCost1RefLog(H265MV mv, const MvPredInfo<2> *predInfo) const
{
    return MvCost1RefLog(mv.mvx, mv.mvy, predInfo);
}

template <typename PixType>
Ipp32s H265CU<PixType>::MvCost1RefLog(Ipp16s mvx, Ipp16s mvy, const MvPredInfo<2> *predInfo) const
{
    assert(predInfo->numCand >= 1);
    Ipp32s cost = MvCost(Ipp16s(mvx - predInfo->mvCand[0].mvx)) + MvCost(Ipp16s(mvy - predInfo->mvCand[0].mvy));
    if (predInfo->numCand > 1) {
        Ipp32s cost2 = MvCost(Ipp16s(mvx - predInfo->mvCand[1].mvx)) + MvCost(Ipp16s(mvy - predInfo->mvCand[1].mvy));
        if (cost > cost2)
            cost = cost2;
    }

    return Ipp32s(cost * m_rdLambdaSqrt + 0.5);
}

#ifdef MEMOIZE_SUBPEL
static const Ipp32s MemSubpelExtW[4][4][4]={
    { // 8x8/
        { 0, 0, 0, 0}, // 00 01 02 03
        { 0, 0, 0, 0}, // 10 11 12 13
        { 8, 0, 8, 0}, // 20 21 22 23
        { 0, 0, 0, 0}  // 30 31 32 33
    }, 
    { // 16x16
        { 0, 0, 0, 0}, // 00 01 02 03
        { 0, 0, 0, 0}, // 10 11 12 13
        { 8, 0, 8, 0}, // 20 21 22 23
        { 0, 0, 0, 0}  // 30 31 32 33
    },
    { // 32x32
        { 0, 0, 0, 0}, // 00 01 02 03
        { 0, 0, 0, 0}, // 10 11 12 13
        { 8, 0, 8, 0}, // 20 21 22 23
        { 0, 0, 0, 0}  // 30 31 32 33
    },
    { // 64x64
        { 0, 0, 0, 0}, // 00 01 02 03
        { 0, 0, 0, 0}, // 10 11 12 13
        { 8, 0, 8, 0}, // 20 21 22 23
        { 0, 0, 0, 0}  // 30 31 32 33
    }
};
static const Ipp32s MemSubpelExtH[4][4][4]={
    { // 8x8
        { 0, 0, 2, 0}, // 00 01 02 03
        { 8, 2, 2, 2}, // 10 11 12 13
        { 8, 2, 2, 2}, // 20 21 22 23
        { 8, 2, 2, 2}  // 30 31 32 33
    }, 
    { // 16x16
        { 0, 0, 2, 0}, // 00 01 02 03
        { 8, 2, 2, 2}, // 10 11 12 13
        { 8, 2, 2, 2}, // 20 21 22 23
        { 8, 2, 2, 2}  // 30 31 32 33
    },
    { // 32x32
        { 0, 0, 2, 0}, // 00 01 02 03
        { 8, 2, 2, 2}, // 10 11 12 13
        { 8, 2, 2, 2}, // 20 21 22 23
        { 8, 2, 2, 2}  // 30 31 32 33
    },
    { // 64x64
        { 0, 0, 2, 0}, // 00 01 02 03
        { 8, 2, 2, 2}, // 10 11 12 13
        { 8, 2, 2, 2}, // 20 21 22 23
        { 8, 2, 2, 2}  // 30 31 32 33
    }
};

template <typename PixType>
Ipp32s H265CU<PixType>::tuHadSave(const PixType* src, Ipp32s pitchSrc, const PixType* rec, Ipp32s pitchRec,
             Ipp32s width, Ipp32s height, Ipp32s *satd, Ipp32s memPitch)
{

    Ipp32u satdTotal = 0;
    Ipp32s sp = memPitch>>3;
    /* assume height and width are multiple of 8 */
    VM_ASSERT(!(width & 0x07) && !(height & 0x07));
    /* test with optimized SATD source code */
    if (width == 8 && height == 8) {
        if(satd) {
            if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)src, pitchSrc, (const Ipp16u *)rec, pitchRec);
            else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u) ((const Ipp8u  *)src, pitchSrc, (const Ipp8u  *)rec, pitchRec);
            satd[0] = (satd[0]+2)>>2;
            satdTotal += satd[0];
        } else {
            Ipp32s had;
            if (sizeof(PixType) == 2)   had = MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)src, pitchSrc, (const Ipp16u *)rec, pitchRec);
            else                        had = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u) ((const Ipp8u  *)src, pitchSrc, (const Ipp8u  *)rec, pitchRec);
            had = (had+2)>>2;
            satdTotal += had;
        }
    } else {
        /* multiple 8x8 blocks - do as many pairs as possible */
        Ipp32s widthPair = width & ~0x0f;
        Ipp32s widthRem = width - widthPair;
        for (Ipp32s j = 0; j < height; j += 8, src += pitchSrc * 8, rec += pitchRec * 8, satd += sp) {
            Ipp32s i = 0, k = 0;
            for (; i < widthPair; i += 8*2, k+=2) {
                if (sizeof(PixType) == 2)   MFX_HEVC_PP::NAME(h265_SATD_8x8_Pair_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec, &satd[k]);
                else                        MFX_HEVC_PP::NAME(h265_SATD_8x8_Pair_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec, &satd[k]);
                satd[k] = (satd[k]+2)>>2;
                satd[k+1] = (satd[k+1]+2)>>2;
                satdTotal += (satd[k] + satd[k+1]);
            }
            if (widthRem) {
                if (sizeof(PixType) == 2)   satd[k] = MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)src+i, pitchSrc, (const Ipp16u *)rec+i, pitchRec);
                else                        satd[k] = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u) ((const Ipp8u  *)src+i, pitchSrc, (const Ipp8u  *)rec+i, pitchRec);
                satd[k] = (satd[k]+2)>>2;
                satdTotal += satd[k];
            }
        }
    }

    return satdTotal;
}

template <typename PixType>
Ipp32s H265CU<PixType>::tuHadUse(Ipp32s width, Ipp32s height, Ipp32s *satd8, Ipp32s memPitch) const
{
    Ipp32u satdTotal = 0;

    /* assume height and width are multiple of 4 */
    VM_ASSERT(!(width & 0x03) && !(height & 0x03));

    if ( (height | width) & 0x07 ) {
        //assert(0);
    } else {
        Ipp32s s8 = memPitch>>3;
        /* multiple 8x8 blocks */
        for (Ipp32s j = 0; j < height>>3; j ++) {
            for (Ipp32s i = 0; i < width>>3; i ++) {
                satdTotal += satd8[j*s8+i];
            }
        }
    }
    return satdTotal;
}

template <typename PixType>
inline void  H265CU<PixType>::MemSubpelGetBufSetMv(Ipp32s size, H265MV *mv, Ipp32s refIdxMem, PixType*& predBuf, Ipp16s *&predBufHi)
{
    Ipp32s hPh = mv->mvx&0x3;
    Ipp32s vPh = mv->mvy&0x3;
    predBuf = m_memSubpel[size][refIdxMem][hPh][vPh].predBuf;
    predBufHi = m_memSubpel[size][refIdxMem][hPh][vPh].predBufHi;
    m_memSubpel[size][refIdxMem][hPh][vPh].done = true;
    m_memSubpel[size][refIdxMem][hPh][vPh].mv = *mv;
}

template <typename PixType>
inline bool  H265CU<PixType>::MemSubpelSave(Ipp32s size, Ipp32s hPh, Ipp32s vPh, 
                                            const H265MEInfo* meInfo, Ipp32s refIdxMem, 
                                            const H265MV *mv, PixType*& predBuf, Ipp32s& memPitch,
                                            PixType*& hpredBuf, Ipp16s*& predBufHi, Ipp16s*& hpredBufHi)
{    
    if(meInfo->width != meInfo->height) return false;
    hpredBuf = NULL;
    hpredBufHi = NULL;

    if(!m_memSubpel[size][refIdxMem][hPh][vPh].done) {
        memPitch = (MAX_CU_SIZE>>(3-size))+8;
        predBuf = m_memSubpel[size][refIdxMem][hPh][vPh].predBuf;
        predBufHi = m_memSubpel[size][refIdxMem][hPh][vPh].predBufHi;
        m_memSubpel[size][refIdxMem][hPh][vPh].done = true;
        m_memSubpel[size][refIdxMem][hPh][vPh].mv   = *mv;

        if(hPh && vPh && MemSubpelExtW[size][hPh][vPh]==MemSubpelExtW[size][hPh][0] 
        && MemSubpelExtH[size][hPh][0]==MemSubpelExtH[size][hPh][vPh]+6)
        {
            hpredBuf = m_memSubpel[size][refIdxMem][hPh][0].predBuf;
            hpredBufHi = m_memSubpel[size][refIdxMem][hPh][0].predBufHi;
            m_memSubpel[size][refIdxMem][hPh][0].done = true;
            m_memSubpel[size][refIdxMem][hPh][0].mv   = *mv;
            m_memSubpel[size][refIdxMem][hPh][0].mv.mvy -= vPh;
        }
        return true;
    }

    return false;
}
template <typename PixType>
inline bool  H265CU<PixType>::MemSubpelInRange(Ipp32s size, const H265MEInfo* meInfo, Ipp32s refIdxMem, const H265MV *cmv)
{   
    const Ipp32s hPh = 2;
    const Ipp32s vPh = 0;
    const Ipp32s MaxSubpelExtW = 8;
    const Ipp32s MaxSubpelExtH = 2;
    H265MV tmv = *cmv;
    tmv.mvx -= 2;
    tmv.mvy -= 0;
    
    switch(size)
    {
    case 0:
        if(m_memSubpel[0][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x07;
            Ipp32s relx = meInfo->posx & 0x07;
            Ipp32s dMVx = (tmv.mvx - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (tmv.mvy - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MaxSubpelExtW/2);
            Ipp32s rangeRx = (8+(MaxSubpelExtW/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MaxSubpelExtH/2);
            Ipp32s rangeRy = (8+(MaxSubpelExtH/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                return true;
            }
        }
    case 1:
        if(m_memSubpel[1][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x0f;
            Ipp32s relx = meInfo->posx & 0x0f;
            Ipp32s dMVx = (tmv.mvx - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (tmv.mvy - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MaxSubpelExtW/2);
            Ipp32s rangeRx = (16+(MaxSubpelExtW/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MaxSubpelExtH/2);
            Ipp32s rangeRy = (16+(MaxSubpelExtH/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                return true;
            }
        }
    case 2:
        if(m_memSubpel[2][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x1f;
            Ipp32s relx = meInfo->posx & 0x1f;
            Ipp32s dMVx = (tmv.mvx - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (tmv.mvy - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MaxSubpelExtW/2);
            Ipp32s rangeRx = (32+(MaxSubpelExtW/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MaxSubpelExtH/2);
            Ipp32s rangeRy = (32+(MaxSubpelExtH/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                return true;
            }
        }
    case 3:
        if(m_memSubpel[3][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x3f;
            Ipp32s relx = meInfo->posx & 0x3f;
            Ipp32s dMVx = (tmv.mvx - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (tmv.mvy - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MaxSubpelExtW/2);
            Ipp32s rangeRx = (64+(MaxSubpelExtW/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MaxSubpelExtH/2);
            Ipp32s rangeRy = (64+(MaxSubpelExtH/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                return true;
            }
        }
    } 

    return false;
}
template <typename PixType>
inline bool  H265CU<PixType>::MemSubpelUse(Ipp32s size, Ipp32s hPh, Ipp32s vPh, const H265MEInfo* meInfo, Ipp32s refIdxMem, const H265MV *mv, const PixType*& predBuf, Ipp32s& memPitch)
{   
    switch(size)
    {
    case 0:
        if(m_memSubpel[0][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x07;
            Ipp32s relx = meInfo->posx & 0x07;
            Ipp32s dMVx = (mv->mvx - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (mv->mvy - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MemSubpelExtW[0][hPh][vPh]/2);
            Ipp32s rangeRx = (8+(MemSubpelExtW[0][hPh][vPh]/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MemSubpelExtH[0][hPh][vPh]/2);
            Ipp32s rangeRy = (8+(MemSubpelExtH[0][hPh][vPh]/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                memPitch = (MAX_CU_SIZE>>3)+8;
                predBuf = &(m_memSubpel[0][refIdxMem][hPh][vPh].predBuf[(rely+dMVy+(MemSubpelExtH[0][hPh][vPh]/2))*memPitch+(relx+dMVx+(MemSubpelExtW[0][hPh][vPh]/2))]);
                return true;
            }
        }
    case 1:
        if(m_memSubpel[1][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x0f;
            Ipp32s relx = meInfo->posx & 0x0f;
            Ipp32s dMVx = (mv->mvx - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (mv->mvy - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MemSubpelExtW[1][hPh][vPh]/2);
            Ipp32s rangeRx = (16+(MemSubpelExtW[1][hPh][vPh]/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MemSubpelExtH[1][hPh][vPh]/2);
            Ipp32s rangeRy = (16+(MemSubpelExtH[1][hPh][vPh]/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                memPitch = (MAX_CU_SIZE>>2)+8;
                predBuf = &(m_memSubpel[1][refIdxMem][hPh][vPh].predBuf[(rely+dMVy+(MemSubpelExtH[1][hPh][vPh]/2))*memPitch+(relx+dMVx+(MemSubpelExtW[1][hPh][vPh]/2))]);
                return true;
            }
        }
    case 2:
        if(m_memSubpel[2][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x1f;
            Ipp32s relx = meInfo->posx & 0x1f;
            Ipp32s dMVx = (mv->mvx - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (mv->mvy - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MemSubpelExtW[2][hPh][vPh]/2);
            Ipp32s rangeRx = (32+(MemSubpelExtW[2][hPh][vPh]/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MemSubpelExtH[2][hPh][vPh]/2);
            Ipp32s rangeRy = (32+(MemSubpelExtH[2][hPh][vPh]/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                memPitch = (MAX_CU_SIZE>>1)+8;
                predBuf = &(m_memSubpel[2][refIdxMem][hPh][vPh].predBuf[(rely+dMVy+(MemSubpelExtH[2][hPh][vPh]/2))*memPitch+(relx+dMVx+(MemSubpelExtW[2][hPh][vPh]/2))]);
                return true;
            }
        }
    case 3:
        if(m_memSubpel[3][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x3f;
            Ipp32s relx = meInfo->posx & 0x3f;
            Ipp32s dMVx = (mv->mvx - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (mv->mvy - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MemSubpelExtW[3][hPh][vPh]/2);
            Ipp32s rangeRx = (64+(MemSubpelExtW[3][hPh][vPh]/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MemSubpelExtH[3][hPh][vPh]/2);
            Ipp32s rangeRy = (64+(MemSubpelExtH[3][hPh][vPh]/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                memPitch = MAX_CU_SIZE+8;
                predBuf = &(m_memSubpel[3][refIdxMem][hPh][vPh].predBuf[(rely+dMVy+(MemSubpelExtH[3][hPh][vPh]/2))*memPitch+(relx+dMVx+(MemSubpelExtW[3][hPh][vPh]/2))]);
                return true;
            }
        }
    } 
    return false;
}

#ifdef AMT_INT_ME_SEED
template <typename PixType>
inline bool  H265CU<PixType>::MemBestMV(Ipp32s size, Ipp32s refIdxMem, H265MV *mv)
{
    switch(size)
    {
    case 0:
        if(m_memBestMV[0][refIdxMem].done) {
            *mv = m_memBestMV[0][refIdxMem].mv;
            return true;
        }
    case 1:
        if(m_memBestMV[1][refIdxMem].done) {
            *mv = m_memBestMV[1][refIdxMem].mv;
            return true;
        }
    case 2:
        if(m_memBestMV[2][refIdxMem].done) {
            *mv = m_memBestMV[2][refIdxMem].mv;
            return true;
        }
    case 3:
        if(m_memBestMV[3][refIdxMem].done) {
            *mv = m_memBestMV[3][refIdxMem].mv;
            return true;
        }
    } 
    return false;
}

template <typename PixType>
inline void  H265CU<PixType>::SetMemBestMV(Ipp32s size, const H265MEInfo* meInfo, Ipp32s refIdxMem, H265MV mv)
{
    if(meInfo->width != meInfo->height) return;
    if(!m_memBestMV[size][refIdxMem].done) {
        m_memBestMV[size][refIdxMem].mv = mv;
        m_memBestMV[size][refIdxMem].done = true;
    }
}
#endif

template <typename PixType>
inline bool  H265CU<PixType>::MemSubpelUse(Ipp32s size, Ipp32s hPh, Ipp32s vPh, const H265MEInfo* meInfo, Ipp32s refIdxMem, const H265MV *mv, Ipp16s*& predBuf, Ipp32s& memPitch)
{
    switch(size)
    {
    case 0:
        if(m_memSubpel[0][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x07;
            Ipp32s relx = meInfo->posx & 0x07;
            Ipp32s dMVx = (mv->mvx - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (mv->mvy - m_memSubpel[0][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MemSubpelExtW[0][hPh][vPh]/2);
            Ipp32s rangeRx = (8+(MemSubpelExtW[0][hPh][vPh]/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MemSubpelExtH[0][hPh][vPh]/2);
            Ipp32s rangeRy = (8+(MemSubpelExtH[0][hPh][vPh]/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                memPitch = (MAX_CU_SIZE>>3)+8;
                predBuf = &(m_memSubpel[0][refIdxMem][hPh][vPh].predBufHi[(rely+dMVy+(MemSubpelExtH[0][hPh][vPh]/2))*memPitch+(relx+dMVx+(MemSubpelExtW[0][hPh][vPh]/2))]);
                return true;
            }
        }
    case 1:
        if(m_memSubpel[1][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x0f;
            Ipp32s relx = meInfo->posx & 0x0f;
            Ipp32s dMVx = (mv->mvx - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (mv->mvy - m_memSubpel[1][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MemSubpelExtW[1][hPh][vPh]/2);
            Ipp32s rangeRx = (16+(MemSubpelExtW[1][hPh][vPh]/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MemSubpelExtH[1][hPh][vPh]/2);
            Ipp32s rangeRy = (16+(MemSubpelExtH[1][hPh][vPh]/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                memPitch = (MAX_CU_SIZE>>2)+8;
                predBuf = &(m_memSubpel[1][refIdxMem][hPh][vPh].predBufHi[(rely+dMVy+(MemSubpelExtH[1][hPh][vPh]/2))*memPitch+(relx+dMVx+(MemSubpelExtW[1][hPh][vPh]/2))]);
                return true;
            }
        }
    case 2:
        if(m_memSubpel[2][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x1f;
            Ipp32s relx = meInfo->posx & 0x1f;
            Ipp32s dMVx = (mv->mvx - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (mv->mvy - m_memSubpel[2][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MemSubpelExtW[2][hPh][vPh]/2);
            Ipp32s rangeRx = (32+(MemSubpelExtW[2][hPh][vPh]/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MemSubpelExtH[2][hPh][vPh]/2);
            Ipp32s rangeRy = (32+(MemSubpelExtH[2][hPh][vPh]/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                memPitch = (MAX_CU_SIZE>>1)+8;
                predBuf = &(m_memSubpel[2][refIdxMem][hPh][vPh].predBufHi[(rely+dMVy+(MemSubpelExtH[2][hPh][vPh]/2))*memPitch+(relx+dMVx+(MemSubpelExtW[2][hPh][vPh]/2))]);
                return true;
            }
        }
    case 3:
        if(m_memSubpel[3][refIdxMem][hPh][vPh].done) {
            Ipp32s rely = meInfo->posy & 0x3f;
            Ipp32s relx = meInfo->posx & 0x3f;
            Ipp32s dMVx = (mv->mvx - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvx)>>2;   // same phase, always integer pixel num
            Ipp32s dMVy = (mv->mvy - m_memSubpel[3][refIdxMem][hPh][vPh].mv.mvy)>>2;
            Ipp32s rangeLx = relx+1+(MemSubpelExtW[3][hPh][vPh]/2);
            Ipp32s rangeRx = (64+(MemSubpelExtW[3][hPh][vPh]/2) -(relx+meInfo->width));
            Ipp32s rangeLy = rely+1+(MemSubpelExtH[3][hPh][vPh]/2);
            Ipp32s rangeRy = (64+(MemSubpelExtH[3][hPh][vPh]/2) -(rely+meInfo->height));
            if( dMVx > -rangeLx && dMVx <= rangeRx && dMVy > -rangeLy && dMVy <= rangeRy) {
                memPitch = MAX_CU_SIZE+8;
                predBuf = &(m_memSubpel[3][refIdxMem][hPh][vPh].predBufHi[(rely+dMVy+(MemSubpelExtH[3][hPh][vPh]/2))*memPitch+(relx+dMVx+(MemSubpelExtW[3][hPh][vPh]/2))]);
                return true;
            }
        }
    } 
    return false;
}

template <typename PixType>
inline bool  H265CU<PixType>::MemHadFirst(Ipp32s size, const H265MEInfo* meInfo, Ipp32s refIdxMem)
{
    if((meInfo->height | meInfo->width) & 0x07) return false;
    switch(size)
    {
    case 0:
    case 1:
        if(m_memSubHad[1][refIdxMem][0][0].count) return false;
    case 2:
        if(m_memSubHad[2][refIdxMem][0][0].count) return false;
    case 3:
        if(m_memSubHad[3][refIdxMem][0][0].count) return false;
    }

    return true;
}

template <typename PixType>
inline bool  H265CU<PixType>::MemHadUse(Ipp32s size, Ipp32s hPh, Ipp32s vPh, const H265MEInfo* meInfo, Ipp32s refIdxMem, const H265MV *mv, Ipp32s& satd, Ipp32s& foundSize)
{
    if((meInfo->height | meInfo->width) & 0x07) return false;
    if(foundSize<size) return false;
    if(foundSize>size) size = foundSize;
    switch(size)
    {
    case 0:
    case 1:
        for(Ipp32s testPos=0;testPos<m_memSubHad[1][refIdxMem][hPh][vPh].count;testPos++) {
            if(m_memSubHad[1][refIdxMem][hPh][vPh].mv[testPos] == *mv) {
                Ipp32s offset = ((meInfo->posy&0x0f)>>3)*((MAX_CU_SIZE>>2)>>3)+((meInfo->posx&0x0f)>>3);
                Ipp32s *satd8 = &(m_memSubHad[1][refIdxMem][hPh][vPh].satd8[testPos][offset]);
                Ipp32s memPitch = MAX_CU_SIZE>>2;
                satd = (tuHadUse(meInfo->width, meInfo->height, satd8, memPitch)) >> m_par->bitDepthLumaShift;
                foundSize = 1;
                return true;
            }
        }
    case 2:
        for(Ipp32s testPos=0;testPos<m_memSubHad[2][refIdxMem][hPh][vPh].count;testPos++) {
            if(m_memSubHad[2][refIdxMem][hPh][vPh].mv[testPos] == *mv) {
                Ipp32s offset = ((meInfo->posy&0x1f)>>3)*((MAX_CU_SIZE>>1)>>3)+((meInfo->posx&0x1f)>>3);
                Ipp32s *satd8 = &(m_memSubHad[2][refIdxMem][hPh][vPh].satd8[testPos][offset]);  
                Ipp32s memPitch = MAX_CU_SIZE>>1;
                satd = (tuHadUse(meInfo->width, meInfo->height, satd8, memPitch)) >> m_par->bitDepthLumaShift;
                foundSize = 2;
                return true;
            }
        }        
    case 3:
        for(Ipp32s testPos=0;testPos<m_memSubHad[3][refIdxMem][hPh][vPh].count;testPos++) {
            if(m_memSubHad[3][refIdxMem][hPh][vPh].mv[testPos] == *mv) {
                Ipp32s offset = ((meInfo->posy&0x3f)>>3)*(MAX_CU_SIZE>>3)+((meInfo->posx&0x3f)>>3);
                Ipp32s *satd8 = &(m_memSubHad[3][refIdxMem][hPh][vPh].satd8[testPos][offset]);
                Ipp32s memPitch = MAX_CU_SIZE;
                satd = (tuHadUse(meInfo->width, meInfo->height, satd8, memPitch)) >> m_par->bitDepthLumaShift;
                foundSize = 3;
                return true;
            }
        }
    default:
        break;
    } 

    return false;
}

template <typename PixType>
inline bool  H265CU<PixType>::MemHadSave(Ipp32s size, Ipp32s hPh, Ipp32s vPh, const H265MEInfo* meInfo, Ipp32s refIdxMem, const H265MV *mv, Ipp32s*& satd8, Ipp32s& memPitch)
{
    if(meInfo->width != meInfo->height) return false;
    if(size == 0) { satd8= NULL; return false; }
    if(m_memSubHad[size][refIdxMem][hPh][vPh].count<4) {
        satd8 = &(m_memSubHad[size][refIdxMem][hPh][vPh].satd8[m_memSubHad[size][refIdxMem][hPh][vPh].count][0]);
        m_memSubHad[size][refIdxMem][hPh][vPh].mv[m_memSubHad[size][refIdxMem][hPh][vPh].count]   = *mv;
        m_memSubHad[size][refIdxMem][hPh][vPh].count++;
        memPitch = MAX_CU_SIZE>>(3-size);
        return true;
    }
    return false;
}

template <typename PixType>
inline void  H265CU<PixType>::MemHadGetBuf(Ipp32s size, Ipp32s hPh, Ipp32s vPh, Ipp32s refIdxMem, const H265MV *mv, Ipp32s*& satd8)
{
    if(size == 0) { satd8= NULL; return; }
    satd8 = &(m_memSubHad[size][refIdxMem][hPh][vPh].satd8[m_memSubHad[size][refIdxMem][hPh][vPh].count][0]);
    m_memSubHad[size][refIdxMem][hPh][vPh].mv[m_memSubHad[size][refIdxMem][hPh][vPh].count]   = *mv;
    m_memSubHad[size][refIdxMem][hPh][vPh].count++;
}

template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricPuMem(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                         const FrameData *refPic, Ipp32s useHadamard, Ipp32s refIdxMem, Ipp32s size, Ipp32s& hadFoundSize)
{
    Ipp32s refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma_pix;
    const PixType *rec = (PixType*)refPic->y + refOffset;
    Ipp32s pitchRec = refPic->pitch_luma_pix;
    PixType *predPtr, *hpredPtr;
    Ipp16s  *predPtrHi, *hpredPtrHi;
    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE*MAX_CU_SIZE];

    Ipp32s isFast = m_par->FastInterp;

    Ipp32s hPh = (mv->mvx&0x3);
    Ipp32s vPh = (mv->mvy&0x3);

    if (hPh || vPh) {
        if(useHadamard) {
            Ipp32s satd=0;
            if(!MemHadUse(size, hPh, vPh, meInfo, refIdxMem, mv, satd, hadFoundSize)) {
                if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, rec, pitchRec)) {
                    if(MemSubpelSave(size, hPh, vPh, meInfo, refIdxMem, mv, predPtr, pitchRec, hpredPtr, predPtrHi, hpredPtrHi)) {
                        H265MEInfo ext_meInfo = *meInfo;
                        H265MV ext_mv = *mv;
                        ext_mv.mvx -= ((MemSubpelExtW[size][hPh][vPh]/2)<<2);
                        ext_mv.mvy -= ((MemSubpelExtH[size][hPh][vPh]/2)<<2);
                        ext_meInfo.width +=MemSubpelExtW[size][hPh][vPh];
                        ext_meInfo.height+=MemSubpelExtH[size][hPh][vPh];
                        MeInterpolateSave(&ext_meInfo, &ext_mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predPtr, pitchRec, hpredPtr, predPtrHi, hpredPtrHi);
                        predPtr+= (MemSubpelExtH[size][hPh][vPh]/2)*pitchRec+(MemSubpelExtW[size][hPh][vPh]/2);
                        rec = predPtr;
                    } else {
                        MeInterpolate(meInfo, mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE, isFast);
                        rec = predBuf;
                        pitchRec = MAX_CU_SIZE;
                    }
                }
                Ipp32s *satd8 = NULL;
                Ipp32s memPitch=0;
                if(MemHadSave(size, hPh, vPh, meInfo, refIdxMem, mv, satd8, memPitch)) {
                    satd =  (tuHadSave(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height, satd8, memPitch)) >> m_par->bitDepthLumaShift;
                } else {
                    satd =  (tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
                }
            }
            return satd;
        } else {
            if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, rec, pitchRec))  {
                if(MemSubpelSave(size, hPh, vPh, meInfo, refIdxMem, mv, predPtr, pitchRec, hpredPtr, predPtrHi, hpredPtrHi)) {
                    H265MEInfo ext_meInfo = *meInfo;
                    H265MV ext_mv = *mv;
                    ext_mv.mvx -= ((MemSubpelExtW[size][hPh][vPh]/2)<<2);
                    ext_mv.mvy -= ((MemSubpelExtH[size][hPh][vPh]/2)<<2);
                    ext_meInfo.width +=MemSubpelExtW[size][hPh][vPh];
                    ext_meInfo.height+=MemSubpelExtH[size][hPh][vPh];
                    MeInterpolateSave(&ext_meInfo, &ext_mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predPtr, pitchRec, hpredPtr, predPtrHi, hpredPtrHi);
                    predPtr+= (MemSubpelExtH[size][hPh][vPh]/2)*pitchRec+(MemSubpelExtW[size][hPh][vPh]/2);
                    rec = predPtr;
                } else {
                    MeInterpolate(meInfo, mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE, isFast);
                    rec = predBuf;
                    pitchRec = MAX_CU_SIZE;
                }
            }
            //return (h265_SAD_MxN_general(rec, pitchRec, src, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
            return (h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
    } else {
        if(useHadamard) {
            Ipp32s satd=0;
            if(!MemHadUse(size, hPh, vPh, meInfo, refIdxMem, mv, satd, hadFoundSize)) {
                Ipp32s *satd8 = NULL;
                Ipp32s memPitch=0;
                if(MemHadSave(size, hPh, vPh, meInfo, refIdxMem, mv, satd8, memPitch)) {
                    satd =  (tuHadSave(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height, satd8, memPitch)) >> m_par->bitDepthLumaShift;
                } else {
                    satd =  (tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
                }
            }
            return satd;
        } else {
            //return (h265_SAD_MxN_general(rec, pitchRec, src, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
            return (h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
    }
}

template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricPuMemSubpelUse(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                         const FrameData *refPic, Ipp32s useHadamard, Ipp32s refIdxMem, Ipp32s size)
{
    Ipp32s refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma_pix;
    const PixType *rec = (PixType*)refPic->y + refOffset;
    Ipp32s pitchRec = refPic->pitch_luma_pix;
    PixType *predPtr, *hpredPtr;
    Ipp16s  *predPtrHi, *hpredPtrHi;
    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE*MAX_CU_SIZE];

    Ipp32s isFast = m_par->FastInterp;

    Ipp32s hPh = (mv->mvx&0x3);
    Ipp32s vPh = (mv->mvy&0x3);

    if (hPh || vPh) {
        if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, rec, pitchRec))  {
            MeInterpolate(meInfo, mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE, isFast);
            rec = predBuf;
            pitchRec = MAX_CU_SIZE;
        }
        if(useHadamard) {
            return (tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        } else {
            return (h265_SAD_MxN_general(rec, pitchRec, src, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
    } else {
        if(useHadamard) {
            return (tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        } else {
            return (h265_SAD_MxN_general(rec, pitchRec, src, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
    }
}


#ifdef MEMOIZE_CAND
template <typename PixType>
#ifdef MEMOIZE_CAND_SUBPEL
bool  H265CU<PixType>::MemSCandSave(const H265MEInfo* meInfo, Ipp32s listIdx, const Ipp8s *refIdx, const H265MV *omv, Ipp32s **satd8, PixType **predBuf, Ipp32s *memPitch, bool useHadamard)
#else
bool  H265CU<PixType>::MemSCandSave(const H265MEInfo* meInfo, Ipp32s listIdx, const Ipp8s *refIdx, const H265MV *omv, Ipp32s **satd8, Ipp32s *memPitch, bool useHadamard)
#endif
{    
#ifndef MEMOIZE_CAND_SUBPEL
    if(!useHadamard) return false;
#endif
    if(meInfo->width != meInfo->height) return false;
    
    Ipp32s test = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   test = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) test = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) test = 2;
    else                                             test = 3;
    
    H265MV mv = *omv;
    ClipMV(mv);

    if(m_memCandSubHad[test].count<MEMOIZE_NUMCAND) {
        Ipp32s size =  (MAX_CU_SIZE>>(3-test));
        *memPitch = size;
        *satd8 = &(m_memCandSubHad[test].satd8[m_memCandSubHad[test].count][0]);
#ifdef MEMOIZE_CAND_SUBPEL
        *predBuf   =  &(m_memCandSubHad[test].predBuf[m_memCandSubHad[test].count][0]);
#endif
        m_memCandSubHad[test].list[m_memCandSubHad[test].count]             = listIdx;
        m_memCandSubHad[test].mv[m_memCandSubHad[test].count][listIdx]      = mv;
        m_memCandSubHad[test].refIdx[m_memCandSubHad[test].count][listIdx]   = refIdx[listIdx];
        m_memCandSubHad[test].count++;
        return true;
    }

    return false;
}

template <typename PixType>
#ifdef MEMOIZE_CAND_SUBPEL
bool  H265CU<PixType>::MemBiCandSave(const H265MEInfo* meInfo, const Ipp8s *refIdx, const H265MV *omv, Ipp32s **satd8, PixType **predBuf, Ipp32s *memPitch, bool useHadamard)
#else
bool  H265CU<PixType>::MemBiCandSave(const H265MEInfo* meInfo, const Ipp8s *refIdx, const H265MV *omv, Ipp32s **satd8, Ipp32s *memPitch, bool useHadamard)
#endif
{
#ifndef MEMOIZE_CAND_SUBPEL
    if(!useHadamard) return false;
#endif
    if(meInfo->width != meInfo->height) return false;
    
    Ipp32s test = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   test = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) test = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) test = 2;
    else                                             test = 3;

    H265MV mv[2] = { omv[0], omv[1] };

    ClipMV(mv[0]);
    ClipMV(mv[1]);

    if(m_memCandSubHad[test].count<MEMOIZE_NUMCAND) {
        Ipp32s size =  (MAX_CU_SIZE>>(3-test));
        *memPitch = size;
        *satd8 = &(m_memCandSubHad[test].satd8[m_memCandSubHad[test].count][0]);
#ifdef MEMOIZE_CAND_SUBPEL
        *predBuf = &(m_memCandSubHad[test].predBuf[m_memCandSubHad[test].count][0]);
#endif
        m_memCandSubHad[test].list[m_memCandSubHad[test].count]         = 2;
        m_memCandSubHad[test].mv[m_memCandSubHad[test].count][0]       = mv[0];
        m_memCandSubHad[test].refIdx[m_memCandSubHad[test].count][0]   = refIdx[0];
        m_memCandSubHad[test].mv[m_memCandSubHad[test].count][1]       = mv[1];
        m_memCandSubHad[test].refIdx[m_memCandSubHad[test].count][1]   = refIdx[1];
        
        m_memCandSubHad[test].count++;
        return true;
    }

    return false;
}

template <typename PixType>
#ifndef MEMOIZE_CAND_SUBPEL
bool  H265CU<PixType>::MemSCandUse(const H265MEInfo* meInfo, Ipp32s listIdx, const Ipp8s *refIdx, const H265MV *omv,  Ipp32s **satd8, Ipp32s *memPitch, bool useHadamard)
#else
bool  H265CU<PixType>::MemSCandUse(const H265MEInfo* meInfo, Ipp32s listIdx, const Ipp8s *refIdx, const H265MV *omv,  const PixType*& predBuf, Ipp32s **satd8, Ipp32s *memPitch, bool useHadamard)
#endif
{
    if (!useHadamard) return false;
    if((meInfo->height | meInfo->width) & 0x07) return false;
    Ipp32s test = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   test = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) test = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) test = 2;
    else                                             test = 3;

    H265MV mv = *omv;
    ClipMV(mv);

    switch(test)
    {
    case 0:
        for(Ipp32s testPos=0;testPos<m_memCandSubHad[0].count;testPos++) {
            if(m_memCandSubHad[0].list[testPos]==listIdx && m_memCandSubHad[0].mv[testPos][listIdx] == mv && m_memCandSubHad[0].refIdx[testPos][listIdx]==refIdx[listIdx]) {
                *memPitch = (MAX_CU_SIZE>>3);
                Ipp32s offset = ((meInfo->posy&0x07)>>3)*((MAX_CU_SIZE>>3)>>3)+((meInfo->posx&0x07)>>3);
                *satd8 = &(m_memCandSubHad[0].satd8[testPos][offset]);
#ifdef MEMOIZE_CAND_SUBPEL
                offset = (meInfo->posy&0x07)*(MAX_CU_SIZE>>3)+(meInfo->posx&0x07);
                predBuf = &(m_memCandSubHad[0].predBuf[testPos][offset]);
#endif
                return true;
            }
        }
    case 1:
        for(Ipp32s testPos=0;testPos<m_memCandSubHad[1].count;testPos++) {
            if(m_memCandSubHad[1].list[testPos]==listIdx && m_memCandSubHad[1].mv[testPos][listIdx] == mv && m_memCandSubHad[1].refIdx[testPos][listIdx]==refIdx[listIdx]) {
                *memPitch = (MAX_CU_SIZE>>2);
                Ipp32s offset = ((meInfo->posy&0x0f)>>3)*((MAX_CU_SIZE>>2)>>3)+((meInfo->posx&0x0f)>>3);
                *satd8 = &(m_memCandSubHad[1].satd8[testPos][offset]);
#ifdef MEMOIZE_CAND_SUBPEL
                offset = (meInfo->posy&0x0f)*(MAX_CU_SIZE>>2)+(meInfo->posx&0x0f);
                predBuf = &(m_memCandSubHad[1].predBuf[testPos][offset]);
#endif
                return true;
            }
        }
    case 2:
        for(Ipp32s testPos=0;testPos<m_memCandSubHad[2].count;testPos++) {
            if(m_memCandSubHad[2].list[testPos]==listIdx && m_memCandSubHad[2].mv[testPos][listIdx] == mv && m_memCandSubHad[2].refIdx[testPos][listIdx]==refIdx[listIdx]) {
                *memPitch = (MAX_CU_SIZE>>1);
                Ipp32s offset = ((meInfo->posy&0x1f)>>3)*((MAX_CU_SIZE>>1)>>3)+((meInfo->posx&0x1f)>>3);
                *satd8 = &(m_memCandSubHad[2].satd8[testPos][offset]);
#ifdef MEMOIZE_CAND_SUBPEL
                offset = (meInfo->posy&0x1f)*(MAX_CU_SIZE>>1)+(meInfo->posx&0x1f);
                predBuf = &(m_memCandSubHad[2].predBuf[testPos][offset]);
#endif
                return true;
            }
        }
    case 3:
        for(Ipp32s testPos=0;testPos<m_memCandSubHad[3].count;testPos++) {
            if(m_memCandSubHad[3].list[testPos]==listIdx && m_memCandSubHad[3].mv[testPos][listIdx] == mv && m_memCandSubHad[3].refIdx[testPos][listIdx]==refIdx[listIdx]) {
                *memPitch = (MAX_CU_SIZE);
                Ipp32s offset = ((meInfo->posy&0x3f)>>3)*((MAX_CU_SIZE)>>3)+((meInfo->posx&0x3f)>>3);
                *satd8 = &(m_memCandSubHad[3].satd8[testPos][offset]);
#ifdef MEMOIZE_CAND_SUBPEL
                offset = (meInfo->posy&0x3f)*(MAX_CU_SIZE)+(meInfo->posx&0x3f);
                predBuf = &(m_memCandSubHad[3].predBuf[testPos][offset]);
#endif
                return true;
            }
        }
    } 
    return false;
}
#ifdef MEMOIZE_CAND_SUBPEL
template <typename PixType>
bool  H265CU<PixType>::MemCandUseSubpel(const H265MEInfo* meInfo, const Ipp8s listIdx, const Ipp8s *refIdx, const H265MV *mv,  const PixType*& predBuf, Ipp32s& memPitch)
{
    if((meInfo->height | meInfo->width) & 0x07) return false;
    Ipp32s test = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   test = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) test = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) test = 2;
    else                                             test = 3;
    Ipp32s offset;
    if(listIdx==2) {
        H265MV cmv[2] = { mv[0], mv[1] };
        ClipMV(cmv[0]);
        ClipMV(cmv[1]);

        if(((cmv[0].mvx|cmv[0].mvy|cmv[1].mvx|cmv[1].mvy)&0x3)==0) return false;
        switch(test)
        {
        case 0:
            for(Ipp32s testPos=0;testPos<m_memCandSubHad[0].count;testPos++) {
                if(m_memCandSubHad[0].list[testPos]==2 
                    && m_memCandSubHad[0].mv[testPos][0] == cmv[0] && m_memCandSubHad[0].mv[testPos][1] == cmv[1]
                    && m_memCandSubHad[0].refIdx[testPos][0]==refIdx[0] && m_memCandSubHad[0].refIdx[testPos][1]==refIdx[1]) {
                    memPitch = (MAX_CU_SIZE>>3);
                    offset = (meInfo->posy&0x07)*(MAX_CU_SIZE>>3)+(meInfo->posx&0x07);
                    predBuf = &(m_memCandSubHad[0].predBuf[testPos][offset]);
                    return true;
                }
            }
        case 1:
            for(Ipp32s testPos=0;testPos<m_memCandSubHad[1].count;testPos++) {
                if(m_memCandSubHad[1].list[testPos]==2 
                    && m_memCandSubHad[1].mv[testPos][0] == cmv[0] && m_memCandSubHad[1].mv[testPos][1] == cmv[1]
                    && m_memCandSubHad[1].refIdx[testPos][0]==refIdx[0] && m_memCandSubHad[1].refIdx[testPos][1]==refIdx[1]) {
                    memPitch = (MAX_CU_SIZE>>2);
                    offset = (meInfo->posy&0x0f)*(MAX_CU_SIZE>>2)+(meInfo->posx&0x0f);
                    predBuf = &(m_memCandSubHad[1].predBuf[testPos][offset]);
                    return true;
                }
            }
        case 2:
            for(Ipp32s testPos=0;testPos<m_memCandSubHad[2].count;testPos++) {
                if(m_memCandSubHad[2].list[testPos]==2 
                    && m_memCandSubHad[2].mv[testPos][0] == cmv[0] && m_memCandSubHad[2].mv[testPos][1] == cmv[1]
                    && m_memCandSubHad[2].refIdx[testPos][0]==refIdx[0] && m_memCandSubHad[2].refIdx[testPos][1] ==refIdx[1]) {
                    memPitch = (MAX_CU_SIZE>>1);
                    offset = (meInfo->posy&0x1f)*(MAX_CU_SIZE>>1)+(meInfo->posx&0x1f);
                    predBuf = &(m_memCandSubHad[2].predBuf[testPos][offset]);
                    return true;
                }
            }
        case 3:
            for(Ipp32s testPos=0;testPos<m_memCandSubHad[3].count;testPos++) {
                if(m_memCandSubHad[3].list[testPos]==2 
                    && m_memCandSubHad[3].mv[testPos][0] == cmv[0] && m_memCandSubHad[3].mv[testPos][1] == cmv[1]
                    && m_memCandSubHad[3].refIdx[testPos][0] ==refIdx[0] && m_memCandSubHad[3].refIdx[testPos][1] ==refIdx[1]) {
                    memPitch = (MAX_CU_SIZE);
                    offset = (meInfo->posy&0x3f)*(MAX_CU_SIZE)+(meInfo->posx&0x3f);
                    predBuf = &(m_memCandSubHad[3].predBuf[testPos][offset]);
                    return true;
                }
            }
        } 
    } else {
        H265MV cmv =  mv[0];
        ClipMV(cmv);

        if(((cmv.mvx|cmv.mvy)&0x3)==0) return false;
        switch(test)
        {
        case 0:
            for(Ipp32s testPos=0;testPos<m_memCandSubHad[0].count;testPos++) {
                if(m_memCandSubHad[0].list[testPos]==listIdx && m_memCandSubHad[0].mv[testPos][listIdx] == cmv && m_memCandSubHad[0].refIdx[testPos][listIdx]==refIdx[listIdx]) {
                    memPitch = (MAX_CU_SIZE>>3);
                    offset = (meInfo->posy&0x07)*(MAX_CU_SIZE>>3)+(meInfo->posx&0x07);
                    predBuf = &(m_memCandSubHad[0].predBuf[testPos][offset]);
                    return true;
                }
            }
        case 1:
            for(Ipp32s testPos=0;testPos<m_memCandSubHad[1].count;testPos++) {
                if(m_memCandSubHad[1].list[testPos]==listIdx && m_memCandSubHad[1].mv[testPos][listIdx] == cmv && m_memCandSubHad[1].refIdx[testPos][listIdx]==refIdx[listIdx]) {
                    memPitch = (MAX_CU_SIZE>>2);
                    offset = (meInfo->posy&0x0f)*(MAX_CU_SIZE>>2)+(meInfo->posx&0x0f);
                    predBuf = &(m_memCandSubHad[1].predBuf[testPos][offset]);
                    return true;
                }
            }
        case 2:
            for(Ipp32s testPos=0;testPos<m_memCandSubHad[2].count;testPos++) {
                if(m_memCandSubHad[2].list[testPos]==listIdx && m_memCandSubHad[2].mv[testPos][listIdx] == cmv && m_memCandSubHad[2].refIdx[testPos][listIdx]==refIdx[listIdx]) {
                    memPitch = (MAX_CU_SIZE>>1);
                    offset = (meInfo->posy&0x1f)*(MAX_CU_SIZE>>1)+(meInfo->posx&0x1f);
                    predBuf = &(m_memCandSubHad[2].predBuf[testPos][offset]);
                    return true;
                }
            }
        case 3:
            for(Ipp32s testPos=0;testPos<m_memCandSubHad[3].count;testPos++) {
                if(m_memCandSubHad[3].list[testPos]==listIdx && m_memCandSubHad[3].mv[testPos][listIdx] == cmv && m_memCandSubHad[3].refIdx[testPos][listIdx]==refIdx[listIdx]) {
                    memPitch = (MAX_CU_SIZE);
                    offset = (meInfo->posy&0x3f)*(MAX_CU_SIZE)+(meInfo->posx&0x3f);
                    predBuf = &(m_memCandSubHad[3].predBuf[testPos][offset]);
                    return true;
                }
            }
        } 
    } 
    return false;
}
#endif

template <typename PixType>
#ifdef MEMOIZE_CAND_SUBPEL
bool  H265CU<PixType>::MemBiCandUse(const H265MEInfo* meInfo, const Ipp8s *refIdx, const H265MV *omv,  const PixType*& predBuf, Ipp32s **satd8, Ipp32s *memPitch, bool useHadamard)
#else
bool  H265CU<PixType>::MemBiCandUse(const H265MEInfo* meInfo, const Ipp8s *refIdx, const H265MV *omv,  Ipp32s **satd8, Ipp32s *memPitch, bool useHadamard)
#endif
{
    if (!useHadamard) return false;
    if((meInfo->height | meInfo->width) & 0x07) return false;
    Ipp32s test = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   test = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) test = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) test = 2;
    else                                             test = 3;
    H265MV mv[2] = { omv[0], omv[1] };
    ClipMV(mv[0]);
    ClipMV(mv[1]);

    switch(test)
    {
    case 0:
        for(Ipp32s testPos=0;testPos<m_memCandSubHad[0].count;testPos++) {
            if(m_memCandSubHad[0].list[testPos]==2 
                && m_memCandSubHad[0].mv[testPos][0] == mv[0] && m_memCandSubHad[0].mv[testPos][1] == mv[1]
                && m_memCandSubHad[0].refIdx[testPos][0]==refIdx[0] && m_memCandSubHad[0].refIdx[testPos][1]==refIdx[1]) {
                *memPitch = (MAX_CU_SIZE>>3);
                Ipp32s offset = ((meInfo->posy&0x07)>>3)*((MAX_CU_SIZE>>3)>>3)+((meInfo->posx&0x07)>>3);
                *satd8 = &(m_memCandSubHad[0].satd8[testPos][offset]);
#ifdef MEMOIZE_CAND_SUBPEL
                offset = (meInfo->posy&0x07)*(MAX_CU_SIZE>>3)+(meInfo->posx&0x07);
                predBuf = &(m_memCandSubHad[0].predBuf[testPos][offset]);
#endif
                return true;
            }
        }
    case 1:
        for(Ipp32s testPos=0;testPos<m_memCandSubHad[1].count;testPos++) {
            if(m_memCandSubHad[1].list[testPos]==2 
                && m_memCandSubHad[1].mv[testPos][0] == mv[0] && m_memCandSubHad[1].mv[testPos][1] == mv[1]
                && m_memCandSubHad[1].refIdx[testPos][0]==refIdx[0] && m_memCandSubHad[1].refIdx[testPos][1]==refIdx[1]) {
                *memPitch = (MAX_CU_SIZE>>2);
                Ipp32s offset = ((meInfo->posy&0x0f)>>3)*((MAX_CU_SIZE>>2)>>3)+((meInfo->posx&0x0f)>>3);
                *satd8 = &(m_memCandSubHad[1].satd8[testPos][offset]);
#ifdef MEMOIZE_CAND_SUBPEL
                offset = (meInfo->posy&0x0f)*(MAX_CU_SIZE>>2)+(meInfo->posx&0x0f);
                predBuf = &(m_memCandSubHad[1].predBuf[testPos][offset]);
#endif
                return true;
            }
        }
    case 2:
        for(Ipp32s testPos=0;testPos<m_memCandSubHad[2].count;testPos++) {
            if(m_memCandSubHad[2].list[testPos]==2 
                && m_memCandSubHad[2].mv[testPos][0] == mv[0] && m_memCandSubHad[2].mv[testPos][1] == mv[1]
                && m_memCandSubHad[2].refIdx[testPos][0]==refIdx[0] && m_memCandSubHad[2].refIdx[testPos][1] ==refIdx[1]) {
                *memPitch = (MAX_CU_SIZE>>1);
                Ipp32s offset = ((meInfo->posy&0x1f)>>3)*((MAX_CU_SIZE>>1)>>3)+((meInfo->posx&0x1f)>>3);
                *satd8 = &(m_memCandSubHad[2].satd8[testPos][offset]);
#ifdef MEMOIZE_CAND_SUBPEL
                offset = (meInfo->posy&0x1f)*(MAX_CU_SIZE>>1)+(meInfo->posx&0x1f);
                predBuf = &(m_memCandSubHad[2].predBuf[testPos][offset]);
#endif
                return true;
            }
        }
    case 3:
        for(Ipp32s testPos=0;testPos<m_memCandSubHad[3].count;testPos++) {
            if(m_memCandSubHad[3].list[testPos]==2 
                && m_memCandSubHad[3].mv[testPos][0] == mv[0] && m_memCandSubHad[3].mv[testPos][1] == mv[1]
                && m_memCandSubHad[3].refIdx[testPos][0] ==refIdx[0] && m_memCandSubHad[3].refIdx[testPos][1] ==refIdx[1]) {
                *memPitch = (MAX_CU_SIZE);
                Ipp32s offset = ((meInfo->posy&0x3f)>>3)*((MAX_CU_SIZE)>>3)+((meInfo->posx&0x3f)>>3);
                *satd8 = &(m_memCandSubHad[3].satd8[testPos][offset]);
#ifdef MEMOIZE_CAND_SUBPEL
                offset = (meInfo->posy&0x3f)*(MAX_CU_SIZE)+(meInfo->posx&0x3f);
                predBuf = &(m_memCandSubHad[3].predBuf[testPos][offset]);
#endif
                return true;
            }
        }
    } 
    return false;
}

#ifdef MEMOIZE_CAND_SUBPEL
template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricPuUse(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                         const FrameData *refPic, Ipp32s useHadamard, const PixType *predBuf, Ipp32s memPitch)
{
    Ipp32s refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma_pix;
    const PixType *rec = (PixType*)refPic->y + refOffset;
    Ipp32s pitchRec = refPic->pitch_luma_pix;

    if ((mv->mvx | mv->mvy) & 3)
    {
        rec = predBuf;
        pitchRec = memPitch;

        return ((useHadamard)
            ? tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)
            : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
    }
    else
    {
        return ((useHadamard)
            ? tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)
            : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
    }
}
#endif

template <typename PixType>
#ifdef MEMOIZE_CAND_SUBPEL
Ipp32s H265CU<PixType>::MatchingMetricPuSave(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                         const FrameData *refPic, Ipp32s useHadamard, Ipp32s *satd8, PixType *predBuf, Ipp32s memPitch)
#else
Ipp32s H265CU<PixType>::MatchingMetricPuSave(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                         const Frame *refPic, Ipp32s useHadamard, Ipp32s *satd8, Ipp32s memPitch)
#endif
{
    Ipp32s refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma_pix;
    const PixType *rec = (PixType*)refPic->y + refOffset;
    Ipp32s pitchRec = refPic->pitch_luma_pix;
    
#ifndef MEMOIZE_CAND_SUBPEL
    Ipp32s predPitch = MAX_CU_SIZE;
    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE*MAX_CU_SIZE];
#else
    Ipp32s predPitch = memPitch;
#endif
    Ipp32s isFast = m_par->FastInterp;

    if ((mv->mvx | mv->mvy) & 3)
    {
        MeInterpolate(meInfo, mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predBuf, predPitch, isFast);
        rec = predBuf;
        pitchRec = predPitch;
        if(!useHadamard && m_par->hadamardMe>=2 && !m_par->fastSkip) {
             // do both, as merge will use it later, except when fastSkip early exit is used
             tuHadSave(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height, satd8, memPitch);
             return h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height) >> m_par->bitDepthLumaShift;
         } else {
            return ((useHadamard)
                ? tuHadSave(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height, satd8, memPitch)
                : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
         }
    }
    else
    {
        if(!useHadamard && m_par->hadamardMe>=2 && !m_par->fastSkip) {
            tuHadSave(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height, satd8, memPitch);
            return h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height) >> m_par->bitDepthLumaShift;
        } else {
            return ((useHadamard)
                ? tuHadSave(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height, satd8, memPitch)
                : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
    }
}

#ifdef MEMOIZE_CAND_SUBPEL
template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricBipredPuUse(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], Ipp32s useHadamard, const PixType *predBuf, Ipp32s memPitch)
{
    H265MV mvsClipped[2] = { mvs[0], mvs[1] };
    Frame *refFrame[2] = { m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]],
                               m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]] };

    Ipp32s isFast = m_par->FastInterp;

    if (CheckIdenticalMotion(refIdx, mvsClipped)) {
        Ipp32s listIdx = !!(refIdx[0] < 0);
        H265MV *mv = mvsClipped + listIdx;
        if (!((mv->mvx | mv->mvy) & 3)) {
            // unique case; int pel from single ref
            Ipp32s refOffset = (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refFrame[listIdx]->m_recon->pitch_luma_pix;
            refOffset += m_ctbPelX + meInfo->posx + (mv->mvx >> 2);
            PixType *rec = (PixType *)refFrame[listIdx]->m_recon->y + refOffset;
            Ipp32s pitchRec = refFrame[listIdx]->m_recon->pitch_luma_pix;

            return ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)
                : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
    }
    
    return ((useHadamard)
        ? tuHad(src, m_pitchSrcLuma, predBuf, memPitch, meInfo->width, meInfo->height)
        : h265_SAD_MxN_general(src, m_pitchSrcLuma, predBuf, memPitch, meInfo->width, meInfo->height)) >> (m_par->bitDepthLumaShift);

}
#endif

template <typename PixType>
#ifdef MEMOIZE_CAND_SUBPEL
Ipp32s H265CU<PixType>::MatchingMetricBipredPuSave(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], Ipp32s useHadamard, Ipp32s *satd8, PixType *predBuf, Ipp32s memPitch)
#else
Ipp32s H265CU<PixType>::MatchingMetricBipredPuSave(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], Ipp32s useHadamard, Ipp32s *satd8, Ipp32s memPitch)
#endif
{
    H265MV mvsClipped[2] = { mvs[0], mvs[1] };
    Frame *refFrame[2] = { m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]],
                               m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]] };
#ifndef MEMOIZE_CAND_SUBPEL
    Ipp32s predPitch = MAX_CU_SIZE;
    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE * MAX_CU_SIZE];
#else
    Ipp32s predPitch = memPitch;
#endif

    Ipp32s isFast = m_par->FastInterp;

    if (CheckIdenticalMotion(refIdx, mvsClipped)) {
        Ipp32s listIdx = !!(refIdx[0] < 0);

        ClipMV(mvsClipped[listIdx]);
        H265MV *mv = mvsClipped + listIdx;
        if ((mv->mvx | mv->mvy) & 3) {
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, listIdx,
                                    refIdx, mvsClipped, predBuf, predPitch, false, AVERAGE_NO, isFast);
        }
        else {
            // unique case; int pel from single ref
            Ipp32s refOffset = (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refFrame[listIdx]->m_recon->pitch_luma_pix;
            refOffset += m_ctbPelX + meInfo->posx + (mv->mvx >> 2);
            PixType *rec = (PixType *)refFrame[listIdx]->m_recon->y + refOffset;
            Ipp32s pitchRec = refFrame[listIdx]->m_recon->pitch_luma_pix;

            if(!useHadamard && m_par->hadamardMe>=2 && !m_par->fastSkip) {
                tuHadSave(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height, satd8, memPitch);
                return h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height) >> m_par->bitDepthLumaShift;
            } else {
                return ((useHadamard)
                    ? tuHadSave(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height, satd8, memPitch)
                    : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
            }
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
                                    refIdx, mvsClipped, predBuf, predPitch, true, AVERAGE_FROM_PIC, isFast);
        }
        else 
        {
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, REF_PIC_LIST_0,
                                    refIdx, mvsClipped, predBuf, predPitch, true, AVERAGE_NO, isFast);
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, REF_PIC_LIST_1,
                                    refIdx, mvsClipped, predBuf, predPitch, true, AVERAGE_FROM_BUF, isFast);
        }
    }

    if(!useHadamard && m_par->hadamardMe>=2 && !m_par->fastSkip) {
        tuHadSave(src, m_pitchSrcLuma, predBuf, predPitch, meInfo->width, meInfo->height, satd8, memPitch);
        return h265_SAD_MxN_general(src, m_pitchSrcLuma, predBuf, memPitch, meInfo->width, meInfo->height) >> (m_par->bitDepthLumaShift);
    } else {
        return ((useHadamard)
            ? tuHadSave(src, m_pitchSrcLuma, predBuf, predPitch, meInfo->width, meInfo->height, satd8, memPitch)
            : h265_SAD_MxN_general(src, m_pitchSrcLuma, predBuf, memPitch, meInfo->width, meInfo->height)) >> (m_par->bitDepthLumaShift);
    }
}

template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricBiPredPuMemCand(const PixType *src, const H265MEInfo *meInfo, const Ipp8s *refIdx, const H265MV *mv,
                                            Ipp32s useHadamard)
{
    Ipp32s  *satd8   = NULL;
    Ipp32s  memPitch = MAX_CU_SIZE;
#ifdef MEMOIZE_CAND_SUBPEL
    const PixType *predBuf;
    PixType *predPtr;
#endif
    Ipp32s cost;
#ifdef MEMOIZE_CAND_SUBPEL
    if(!m_par->fastSkip && MemBiCandUse(meInfo, refIdx, mv, predBuf, &satd8, &memPitch, useHadamard)) {
#else
    if(MemBiCandUse(meInfo, refIdx, mv, &satd8, &memPitch, useHadamard)) {
#endif
        cost = tuHadUse(meInfo->width, meInfo->height, satd8, memPitch) >> m_par->bitDepthLumaShift;
#ifdef MEMOIZE_CAND_TEST
        Ipp32s testcost = MatchingMetricBipredPu(src, meInfo, refIdx, mv, useHadamard);
        assert(testcost == cost);
#endif
#ifdef MEMOIZE_CAND_SUBPEL
    } else if(MemCandUseSubpel(meInfo, 2, refIdx, mv, predBuf, memPitch)) {
        cost = MatchingMetricBipredPuUse(src, meInfo, refIdx, mv, useHadamard, predBuf, memPitch);
#ifdef MEMOIZE_CAND_SUBPEL_TEST
        Ipp32s testcost = MatchingMetricBipredPu(src, meInfo, refIdx, mv, useHadamard);
        assert(testcost == cost);
#endif
    } else if(MemBiCandSave(meInfo, refIdx, mv, &satd8, &predPtr, &memPitch, useHadamard)) {
        cost = MatchingMetricBipredPuSave(src, meInfo, refIdx, mv, useHadamard, satd8, predPtr, memPitch);
#ifdef MEMOIZE_CAND_SUBPEL_TEST
        Ipp32s testcost = MatchingMetricBipredPu(src, meInfo, refIdx, mv, useHadamard);
        assert(testcost == cost);
#endif
#else
    } else if(MemBiCandSave(meInfo, refIdx, mv, &satd8, &memPitch, useHadamard)) {
        cost = MatchingMetricBipredPuSave(src, meInfo, refIdx, mv, useHadamard, satd8, memPitch);
#endif
    } else {
        cost = MatchingMetricBipredPu(src, meInfo, refIdx, mv, useHadamard);
    }
    return cost;
}

template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricPuMemSCand(const PixType *src, const H265MEInfo *meInfo, Ipp32s listIdx, const Ipp8s *refIdx, const H265MV *mv,
                                         const FrameData *refPic, Ipp32s useHadamard)
{
    Ipp32s  *satd8   = NULL;
    Ipp32s  memPitch = MAX_CU_SIZE;
#ifdef MEMOIZE_CAND_SUBPEL
    const PixType *predBuf;
    PixType *predPtr;
#endif
    Ipp32s cost;
#ifdef MEMOIZE_CAND_SUBPEL
    if(!m_par->fastSkip && MemSCandUse(meInfo, listIdx, refIdx, mv, predBuf, &satd8, &memPitch, useHadamard)) {
#else
    if(MemSCandUse(meInfo, listIdx, refIdx, mv, &satd8, &memPitch, useHadamard)) {
#endif
        cost = tuHadUse(meInfo->width, meInfo->height, satd8, memPitch) >> m_par->bitDepthLumaShift;
#ifdef MEMOIZE_CAND_TEST
        Ipp32s testcost = MatchingMetricPu(src, meInfo, mv, refPic, useHadamard);
        assert(testcost == cost);
#endif
#ifdef MEMOIZE_CAND_SUBPEL
    } else if(MemCandUseSubpel(meInfo, listIdx, refIdx, mv, predBuf, memPitch)) {
        cost = MatchingMetricPuUse(src, meInfo, mv, refPic, useHadamard, predBuf, memPitch);
#ifdef MEMOIZE_CAND_SUBPEL_TEST
        Ipp32s testcost = MatchingMetricPu(src, meInfo, mv, refPic, useHadamard);
        assert(testcost == cost);
#endif
    } else if(MemSCandSave(meInfo, listIdx, refIdx, mv, &satd8, &predPtr, &memPitch, useHadamard)) {
        cost = MatchingMetricPuSave(src, meInfo, mv, refPic, useHadamard, satd8, predPtr, memPitch);
#ifdef MEMOIZE_CAND_SUBPEL_TEST
        Ipp32s testcost = MatchingMetricPu(src, meInfo, mv, refPic, useHadamard);
        assert(testcost == cost);
#endif
#else
    } else if(MemSCandSave(meInfo, listIdx, refIdx, mv, &satd8, &memPitch, useHadamard)) {
        cost = MatchingMetricPuSave(src, meInfo, mv, refPic, useHadamard, satd8, memPitch);
#endif
    } else {
        cost = MatchingMetricPu(src, meInfo, mv, refPic, useHadamard);
    }
    return cost;
}
#endif
#endif


// absPartIdx - for minimal TU
template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricPu(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                         const FrameData *refPic, Ipp32s useHadamard) const
{
    Ipp32s refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma_pix;
    const PixType *rec = (PixType*)refPic->y + refOffset;
    Ipp32s pitchRec = refPic->pitch_luma_pix;
    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE*MAX_CU_SIZE];

    Ipp32s isFast = m_par->FastInterp;

    if ((mv->mvx | mv->mvy) & 3)
    {
        MeInterpolate(meInfo, mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE, isFast);
        rec = predBuf;
        pitchRec = MAX_CU_SIZE;
        
        return ((useHadamard)
            ? tuHad(src, m_pitchSrcLuma, rec, MAX_CU_SIZE, meInfo->width, meInfo->height)
            : h265_SAD_MxN_special(src, rec, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
    }
    else
    {
        return ((useHadamard)
            ? tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)
            : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
    }
}

// absPartIdx - for minimal TU
template <typename PixType>
void H265CU<PixType>::MatchingMetricPuCombine(Ipp32s *had, const PixType *src, const H265MEInfo *meInfo, const H265MV *mv, const H265MV *mvB,
                                              const Ipp8s refIdx[2], const Ipp8s refIdxB[2], Ipp32s useHadamard) const
{
    Frame *refFrame[2] = { m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]],
                           m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]] };
    Frame *refFrameB[2] = { m_currFrame->m_refPicList[0].m_refFrames[refIdxB[0]],
                            m_currFrame->m_refPicList[1].m_refFrames[refIdxB[1]] };
    Ipp32s pitchRec0 = refFrame[0]->m_recon->pitch_luma_pix;
    Ipp32s pitchRec1 = refFrame[1]->m_recon->pitch_luma_pix;
    Ipp32s refOffset0 = m_ctbPelX + meInfo->posx + (mv[0].mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv[0].mvy >> 2)) * pitchRec0;
    Ipp32s refOffset1 = m_ctbPelX + meInfo->posx + (mv[1].mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv[1].mvy >> 2)) * pitchRec1;
    const PixType *rec0 = (PixType*)refFrame[0]->m_recon->y + refOffset0;
    const PixType *rec1 = (PixType*)refFrame[1]->m_recon->y + refOffset1;
    ALIGN_DECL(32) PixType predBuf0[MAX_CU_SIZE*MAX_CU_SIZE];
    ALIGN_DECL(32) PixType predBuf1[MAX_CU_SIZE*MAX_CU_SIZE];
    ALIGN_DECL(32) Ipp16s predBufHi0[MAX_CU_SIZE*MAX_CU_SIZE];
    ALIGN_DECL(32) Ipp16s predBufHi1[MAX_CU_SIZE*MAX_CU_SIZE];
    Ipp16s *pBufHi0 = predBufHi0;
    Ipp16s *pBufHi1 = predBufHi1;
    Ipp32s bitDepth = m_par->bitDepthLuma;

    if ((mv[0].mvx | mv[0].mvy) & 3)
    {
        MeInterpolateCombine(meInfo, &mv[0], (PixType*)refFrame[0]->m_recon->y, refFrame[0]->m_recon->pitch_luma_pix, pBufHi0, MAX_CU_SIZE);
        h265_InterpLumaPack(pBufHi0, MAX_CU_SIZE, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height, bitDepth);
        rec0 = predBuf0;
        pitchRec0 = MAX_CU_SIZE;
        
        had[0] =  ((useHadamard)
            ? tuHad(src, m_pitchSrcLuma, rec0, pitchRec0, meInfo->width, meInfo->height)
            : h265_SAD_MxN_special(src, rec0, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
    }
    else
    {
        had[0] =  ((useHadamard)
            ? tuHad(src, m_pitchSrcLuma, rec0, pitchRec0, meInfo->width, meInfo->height)
            : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec0, pitchRec0, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
    }
 
    if (refIdx[0] == m_currFrame->m_mapListRefUnique[1][refIdx[1]]&& mv[0] == mv[1]) {
        had[1] = had[0];
        pBufHi1 = pBufHi0;
        pBufHi0 = predBufHi1;
        rec1 = rec0;
        pitchRec1 = pitchRec0;
    } else {
        if ((mv[1].mvx | mv[1].mvy) & 3)
        {
            MeInterpolateCombine(meInfo, &mv[1], (PixType*)refFrame[1]->m_recon->y, refFrame[1]->m_recon->pitch_luma_pix, pBufHi1, MAX_CU_SIZE);
            h265_InterpLumaPack(pBufHi1, MAX_CU_SIZE, predBuf1, MAX_CU_SIZE, meInfo->width, meInfo->height, bitDepth);
            rec1 = predBuf1;
            pitchRec1 = MAX_CU_SIZE;
        
            had[1] =  ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, rec1, pitchRec1, meInfo->width, meInfo->height)
                : h265_SAD_MxN_special(src, rec1, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
        else
        {
            had[1] =  ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, rec1, pitchRec1, meInfo->width, meInfo->height)
                : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec1, pitchRec1, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
    }

    // for Bi-pred either L0 or L1 or both are from Uni-pred
    if (m_currFrame->m_mapListRefUnique[0][refIdxB[0]] == m_currFrame->m_mapListRefUnique[1][refIdxB[1]]) {
        had[2] = had[0];    // useless case from encoder's standpoint if there're more ref frames in dpb
    } else {

        Ipp32s interpFlag0 = (mvB[0].mvx | mvB[0].mvy) & 3;
        Ipp32s interpFlag1 = (mvB[1].mvx | mvB[1].mvy) & 3;
        bool onlyOneIterp = !(interpFlag0 && interpFlag1);
        Ipp32s isFast = false;

        if (refIdx[0] != refIdxB[0]) { // refIdxB[0] can be changed (see MePuGacc)
            if (interpFlag0)
            {
                MeInterpolateCombine(meInfo, &mvB[0], (PixType*)refFrameB[0]->m_recon->y, refFrameB[0]->m_recon->pitch_luma_pix, pBufHi0, MAX_CU_SIZE);
            } else {
                pitchRec0 = refFrameB[0]->m_recon->pitch_luma_pix;
                refOffset0 = m_ctbPelX + meInfo->posx + (mvB[0].mvx >> 2) + (m_ctbPelY + meInfo->posy + (mvB[0].mvy >> 2)) * pitchRec0;
                rec0 = (PixType*)refFrameB[0]->m_recon->y + refOffset0;
            }
        }/* else {
            assert(mv[0]==mvB[0]);
            assert(mv[1]==mvB[1]);
        }*/

        if(!interpFlag0 && !interpFlag1) {
            WriteAverageToPic(rec0, pitchRec0, rec1, pitchRec1, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height);

            had[2] = ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height)
                : h265_SAD_MxN_special(src, predBuf0, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> (m_par->bitDepthLumaShift);
        } else if(!interpFlag0) {
            WriteBiAverageToPicP(pBufHi1, MAX_CU_SIZE, (PixType*)rec0, pitchRec0, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height, m_par->bitDepthLuma);

            had[2] = ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height)
                : h265_SAD_MxN_special(src, predBuf0, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> (m_par->bitDepthLumaShift);
        } else if(!interpFlag1) {
            WriteBiAverageToPicP(pBufHi0, MAX_CU_SIZE, (PixType*)rec1, pitchRec1, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height, m_par->bitDepthLuma);

            had[2] = ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height)
                : h265_SAD_MxN_special(src, predBuf0, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> (m_par->bitDepthLumaShift);
        } else {
            WriteBiAverageToPicB(pBufHi0, MAX_CU_SIZE, pBufHi1, MAX_CU_SIZE, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height, m_par->bitDepthLuma);

            had[2] = ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, predBuf0, MAX_CU_SIZE, meInfo->width, meInfo->height)
                : h265_SAD_MxN_special(src, predBuf0, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> (m_par->bitDepthLumaShift);
        }
    }

}


template <typename PixType>
void WriteAverageToPic(
    const PixType * pSrc0,
    Ipp32u        in_Src0Pitch,      // in samples
    const PixType * pSrc1,
    Ipp32u        in_Src1Pitch,      // in samples
    PixType * H265_RESTRICT pDst,
    Ipp32u        in_DstPitch,       // in samples
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
        for (int i = 0; i < width; i++)
             pDst[i] = (((Ipp16u)pSrc0[i] + (Ipp16u)pSrc1[i] + 1) >> 1);

        pSrc0 += in_Src0Pitch;
        pSrc1 += in_Src1Pitch;
        pDst += in_DstPitch;
    }
}

template <typename PixType>
void WriteAverageToPic1(
    const Ipp16s *src0,
    Ipp32u        pitchSrc0,      // in samples
    const Ipp16s *src1,
    Ipp32u        pitchSrc1,      // in samples
    PixType * H265_RESTRICT dst,
    Ipp32u        pitchDst,       // in samples
    Ipp32s        width,
    Ipp32s        height,
    Ipp32s        bitDepth)
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
            Ipp32s val = ((src0[i] + src1[i] + 1) >> 1);
            dst[i] = (PixType)Saturate(0, (1 << bitDepth) - 1, val);
        }

        src0 += pitchSrc0;
        src1 += pitchSrc1;
        dst  += pitchDst;
    }
}

#ifdef MEMOIZE_SUBPEL

void WriteBiAverageToPicP(
    const Ipp16s *src0,
    Ipp32u        pitchSrc0,      // in samples
    unsigned char *src1,
    Ipp32u        pitchSrc1,      // in samples
    Ipp8u * H265_RESTRICT dst,
    Ipp32u        pitchDst,       // in samples
    Ipp32s        width,
    Ipp32s        height,
    Ipp32s        bitDepth)
{
    MFX_HEVC_PP::NAME(h265_AverageModeP)((short*)src0, pitchSrc0, src1, pitchSrc1, dst, pitchDst, width, height);
}

void WriteBiAverageToPicP(
    const Ipp16s *src0,
    Ipp32u        pitchSrc0,      // in samples
    Ipp16u * H265_RESTRICT src1,
    Ipp32u        pitchSrc1,      // in samples
    Ipp16u * H265_RESTRICT dst,
    Ipp32u        pitchDst,       // in samples
    Ipp32s        width,
    Ipp32s        height,
    Ipp32s        bitDepth)
{
    MFX_HEVC_PP::NAME(h265_AverageModeP_U16)((short*)src0, pitchSrc0, src1, pitchSrc1, dst, pitchDst, width, height, bitDepth);
}

void WriteBiAverageToPicB(
    const Ipp16s *src0,
    Ipp32u        pitchSrc0,      // in samples
    const Ipp16s *src1,
    Ipp32u        pitchSrc1,      // in samples
    Ipp8u * H265_RESTRICT dst,
    Ipp32u        pitchDst,       // in samples
    Ipp32s        width,
    Ipp32s        height,
    Ipp32s        bitDepth)
{
    MFX_HEVC_PP::NAME(h265_AverageModeB)((short*)src0, pitchSrc0, (short*)src1, pitchSrc1, dst, pitchDst, width, height);
}

void WriteBiAverageToPicB(
    const Ipp16s *src0,
    Ipp32u        pitchSrc0,      // in samples
    const Ipp16s *src1,
    Ipp32u        pitchSrc1,      // in samples
    Ipp16u * H265_RESTRICT dst,
    Ipp32u        pitchDst,       // in samples
    Ipp32s        width,
    Ipp32s        height,
    Ipp32s        bitDepth)
{
    MFX_HEVC_PP::NAME(h265_AverageModeB_U16)((short*)src0, pitchSrc0, (short*)src1, pitchSrc1, dst, pitchDst, width, height, bitDepth);
}

template <typename PixType>
void H265CU<PixType>::MeInterpolateUseSaveHi(const H265MEInfo* meInfo, const H265MV *mv, PixType *src,
                                    Ipp32s srcPitch, Ipp16s*& dst, Ipp32s& dstPitch, Ipp32s size, Ipp32s refIdxMem)
{
    Ipp32s w = meInfo->width;
    Ipp32s h = meInfo->height;
    Ipp32s dx = mv->mvx & 3;
    Ipp32s dy = mv->mvy & 3;
    Ipp32s hPh = dx;
    Ipp32s vPh = dy;
    Ipp32s bitDepth = m_par->bitDepthLuma;
    Ipp16s *interpBuf;
    Ipp32s interpPitch;
    Ipp32s refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * srcPitch;
    src += refOffset;
    
    bool isFast = m_par->FastInterp;

    VM_ASSERT (!(dx == 0 && dy == 0));
    if (dy == 0)
    {
        if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, interpBuf, interpPitch)) {
            dst = m_interpBufBi[m_interpIdxLast].predBufY;
            m_interpBufBi[m_interpIdxLast].refIdx = refIdxMem;
            m_interpBufBi[m_interpIdxLast].mv = *mv;
            m_interpIdxLast = (m_interpIdxLast + 1) & (INTERP_BUF_SZ - 1); // added to end
            if (m_interpIdxFirst == m_interpIdxLast) // replaced oldest
                m_interpIdxFirst = (m_interpIdxFirst + 1) & (INTERP_BUF_SZ - 1);
            dstPitch = MAX_CU_SIZE;
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src, srcPitch, dst, dstPitch, dx, w, h, bitDepth - 8, 0, bitDepth, isFast);
        } else {
            dst = interpBuf;
            dstPitch = interpPitch;
        }
    }
    else if (dx == 0)
    {
        if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, interpBuf, interpPitch)) {
            PixType *predPtr, *hpredPtr;
            Ipp32s pitchRec;
            Ipp16s *predPtrHi, *hpredPtrHi;
            if(MemSubpelSave(size, hPh, vPh, meInfo, refIdxMem, mv, predPtr, pitchRec, hpredPtr, predPtrHi, hpredPtrHi)) {
                Ipp32s ext_offset = (MemSubpelExtW[size][hPh][vPh]/2);
                ext_offset += (MemSubpelExtH[size][hPh][vPh]/2)*srcPitch;
                Ipp32s w2 = w+MemSubpelExtW[size][hPh][vPh];
                Ipp32s h2 = h+MemSubpelExtH[size][hPh][vPh];
                Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, src-ext_offset, srcPitch, predPtrHi, pitchRec, dy, w2, h2, bitDepth - 8, 0, bitDepth, isFast);
                h265_InterpLumaPack(predPtrHi, pitchRec, predPtr, pitchRec, w2, h2, bitDepth);
                dst = predPtrHi + (MemSubpelExtH[size][hPh][vPh]/2)*pitchRec+(MemSubpelExtW[size][hPh][vPh]/2);
                dstPitch = pitchRec;
            } else {
                dst = m_interpBufBi[m_interpIdxLast].predBufY;
                m_interpBufBi[m_interpIdxLast].refIdx = refIdxMem;
                m_interpBufBi[m_interpIdxLast].mv = *mv;
                m_interpIdxLast = (m_interpIdxLast + 1) & (INTERP_BUF_SZ - 1); // added to end
                if (m_interpIdxFirst == m_interpIdxLast) // replaced oldest
                    m_interpIdxFirst = (m_interpIdxFirst + 1) & (INTERP_BUF_SZ - 1);
                dstPitch = MAX_CU_SIZE;
                Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, src, srcPitch, dst, dstPitch, dy, w, h, bitDepth - 8, 0, bitDepth, isFast);
            }
        } else {
            dst = interpBuf;
            dstPitch = interpPitch;
        }
    }
    else
    {
        if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, interpBuf, interpPitch)) {
            dst = m_interpBufBi[m_interpIdxLast].predBufY;
            m_interpBufBi[m_interpIdxLast].refIdx = refIdxMem;
            m_interpBufBi[m_interpIdxLast].mv = *mv;
            m_interpIdxLast = (m_interpIdxLast + 1) & (INTERP_BUF_SZ - 1); // added to end
            if (m_interpIdxFirst == m_interpIdxLast) // replaced oldest
                m_interpIdxFirst = (m_interpIdxFirst + 1) & (INTERP_BUF_SZ - 1);
            dstPitch = MAX_CU_SIZE;

            Ipp16s tmpBuf[((64+8)+8+8) * ((64+8)+8+8)];
            Ipp16s *tmp = tmpBuf + ((64+8)+8+8) * 8 + 8;
            Ipp32s tmpPitch = ((64+8)+8+8);

            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 3 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 8, bitDepth - 8, 0, bitDepth, isFast);
            Ipp32s shift  = 6;
            Ipp16s offset = 0;
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth, isFast);
        } else {
            dst = interpBuf;
            dstPitch = interpPitch;
        }
    }
    return;
} 

template <typename PixType>
void H265CU<PixType>::MeInterpolateUseHi(const H265MEInfo* meInfo, const H265MV *mv, PixType *src,
                                    Ipp32s srcPitch, Ipp16s*& dst, Ipp32s& dstPitch, Ipp32s size, Ipp32s refIdxMem)
{
    Ipp32s w = meInfo->width;
    Ipp32s h = meInfo->height;
    Ipp32s dx = mv->mvx & 3;
    Ipp32s dy = mv->mvy & 3;
    Ipp32s hPh = dx;
    Ipp32s vPh = dy;
    Ipp32s bitDepth = m_par->bitDepthLuma;
    Ipp16s *interpBuf;
    Ipp32s interpPitch;
    Ipp32s refOffset = m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * srcPitch;
    src += refOffset;
    
    bool isFast = m_par->FastInterp;

    VM_ASSERT (!(dx == 0 && dy == 0));
    if (dy == 0)
    {
        if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, interpBuf, interpPitch)) {
            dst = m_interpBufBi[m_interpIdxLast].predBufY;
            m_interpBufBi[m_interpIdxLast].refIdx = refIdxMem;
            m_interpBufBi[m_interpIdxLast].mv = *mv;
            m_interpIdxLast = (m_interpIdxLast + 1) & (INTERP_BUF_SZ - 1); // added to end
            if (m_interpIdxFirst == m_interpIdxLast) // replaced oldest
                m_interpIdxFirst = (m_interpIdxFirst + 1) & (INTERP_BUF_SZ - 1);
            dstPitch = MAX_CU_SIZE;
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src, srcPitch, dst, dstPitch, dx, w, h, bitDepth - 8, 0, bitDepth, isFast);
        } else {
            dst = interpBuf;
            dstPitch = interpPitch;
        }
    }
    else if (dx == 0)
    {
        if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, interpBuf, interpPitch)) {
            dst = m_interpBufBi[m_interpIdxLast].predBufY;
            m_interpBufBi[m_interpIdxLast].refIdx = refIdxMem;
            m_interpBufBi[m_interpIdxLast].mv = *mv;
            m_interpIdxLast = (m_interpIdxLast + 1) & (INTERP_BUF_SZ - 1); // added to end
            if (m_interpIdxFirst == m_interpIdxLast) // replaced oldest
                m_interpIdxFirst = (m_interpIdxFirst + 1) & (INTERP_BUF_SZ - 1);
            dstPitch = MAX_CU_SIZE;
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, src, srcPitch, dst, dstPitch, dy, w, h, bitDepth - 8, 0, bitDepth, isFast);
        } else {
            dst = interpBuf;
            dstPitch = interpPitch;
        }
    }
    else
    {
        if(!MemSubpelUse(size, hPh, vPh, meInfo, refIdxMem, mv, interpBuf, interpPitch)) {  
            dst = m_interpBufBi[m_interpIdxLast].predBufY;
            m_interpBufBi[m_interpIdxLast].refIdx = refIdxMem;
            m_interpBufBi[m_interpIdxLast].mv = *mv;
            m_interpIdxLast = (m_interpIdxLast + 1) & (INTERP_BUF_SZ - 1); // added to end
            if (m_interpIdxFirst == m_interpIdxLast) // replaced oldest
                m_interpIdxFirst = (m_interpIdxFirst + 1) & (INTERP_BUF_SZ - 1);
            dstPitch = MAX_CU_SIZE;

            Ipp16s tmpBuf[((64+8)+8+8) * ((64+8)+8+8)];
            Ipp16s *tmp = tmpBuf + ((64+8)+8+8) * 8 + 8;
            Ipp32s tmpPitch = ((64+8)+8+8);

            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 3 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 8, bitDepth - 8, 0, bitDepth, isFast);
            Ipp32s shift  = 6;
            Ipp16s offset = 0;
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth, isFast);
        } else {
            dst = interpBuf;
            dstPitch = interpPitch;
        }
    }
    return;
} 


#ifdef MEMOIZE_SUBPEL
template <typename PixType>
void H265CU<PixType>::InterploateBipredUseHi(Ipp32s size, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], PixType* predBuf)
{
    H265MV mvsClipped[2] = { mvs[0], mvs[1] };
    Frame *refFrame[2] = { m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]],
                               m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]] };
    
    Ipp32s isFast = false;

    IppiSize roiSize = {meInfo->width, meInfo->height};
    if (CheckIdenticalMotion(refIdx, mvsClipped)) {
        Ipp32s listIdx = !!(refIdx[0] < 0);
        ClipMV(mvsClipped[listIdx]);
        H265MV *mv = mvsClipped + listIdx;
        if ((mv->mvx | mv->mvy) & 3) {
            const PixType* predPtr;
            Ipp32s predPitch;
            if(MemSubpelUse(size, mv->mvx&3, mv->mvy&3, meInfo, m_currFrame->m_mapListRefUnique[listIdx][refIdx[listIdx]],
                &mvsClipped[listIdx], predPtr, predPitch)) 
            {
                _ippiCopy_C1R((PixType*)predPtr, predPitch, predBuf, MAX_CU_SIZE, roiSize);
            } else {
                    PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, listIdx,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, false, AVERAGE_NO, isFast);
            }
        }
        else {
            // unique case; int pel from single ref
            Ipp32s refOffset = (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refFrame[listIdx]->m_recon->pitch_luma_pix;
            refOffset += m_ctbPelX + meInfo->posx + (mv->mvx >> 2);
            PixType *rec = (PixType *)refFrame[listIdx]->m_recon->y + refOffset;
            Ipp32s pitchRec = refFrame[listIdx]->m_recon->pitch_luma_pix;
            _ippiCopy_C1R(rec, pitchRec, predBuf, MAX_CU_SIZE, roiSize);
        }
    }
    else {
        ClipMV(mvsClipped[0]);
        ClipMV(mvsClipped[1]);
        Ipp32s interpFlag0 = (mvsClipped[0].mvx | mvsClipped[0].mvy) & 3;
        Ipp32s interpFlag1 = (mvsClipped[1].mvx | mvsClipped[1].mvy) & 3;
        bool onlyOneIterp = !(interpFlag0 && interpFlag1);
#if 1
        if(!interpFlag0 && !interpFlag1) {
            Ipp32s listIdx = !interpFlag0;
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, listIdx,
                refIdx, mvsClipped, predBuf, MAX_CU_SIZE, true, AVERAGE_FROM_PIC, isFast);
        } else
#endif
        if (onlyOneIterp) {
            Ipp32s listIdx = !interpFlag0;
            Ipp32s listIdx2 = !listIdx;
            Ipp16s *predBufY = NULL;
            Ipp32s predPitch = MAX_CU_SIZE;
            
            H265MV *ref_mv = mvsClipped + listIdx2;
            Ipp32s refOffset = (m_ctbPelY + meInfo->posy + (ref_mv->mvy >> 2)) * refFrame[listIdx2]->m_recon->pitch_luma_pix;
            refOffset += m_ctbPelX + meInfo->posx + (ref_mv->mvx >> 2);
            PixType *refY = (PixType *)refFrame[listIdx2]->m_recon->y + refOffset;
            Ipp32s pitchRef = refFrame[listIdx2]->m_recon->pitch_luma_pix;
            m_interpIdxFirst = 0;
            m_interpIdxLast = 0;
            MeInterpolateUseHi(meInfo, &mvsClipped[listIdx], (PixType *)refFrame[listIdx]->m_recon->y, 
                refFrame[listIdx]->m_recon->pitch_luma_pix, predBufY, predPitch, size, 
                m_currFrame->m_mapListRefUnique[listIdx][refIdx[listIdx]]);

            WriteBiAverageToPicP(predBufY, predPitch, refY, pitchRef, predBuf, MAX_CU_SIZE, meInfo->width, meInfo->height, m_par->bitDepthLuma);
        }
        else {
            Ipp16s *predBufY[2] = {NULL, NULL};
            Ipp32s predPitch[2] = {MAX_CU_SIZE, MAX_CU_SIZE};
            m_interpIdxFirst = 0;
            m_interpIdxLast = 0;
            
            MeInterpolateUseHi(meInfo, &mvsClipped[0], (PixType *)refFrame[0]->m_recon->y, refFrame[0]->m_recon->pitch_luma_pix, 
                predBufY[0], predPitch[0], size, m_currFrame->m_mapListRefUnique[0][refIdx[0]]);

            MeInterpolateUseHi(meInfo, &mvsClipped[1], (PixType *)refFrame[1]->m_recon->y, refFrame[1]->m_recon->pitch_luma_pix, 
                predBufY[1], predPitch[1], size, m_currFrame->m_mapListRefUnique[1][refIdx[1]]);

            WriteBiAverageToPicB(predBufY[0], predPitch[0], predBufY[1], predPitch[1], predBuf, MAX_CU_SIZE, 
                meInfo->width, meInfo->height, m_par->bitDepthLuma);
        }
    }
}
#endif

template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricBipredPuSearch(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], Ipp32s useHadamard)
{
    H265MV mvsClipped[2] = { mvs[0], mvs[1] };
    Frame *refFrame[2] = { m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]],
                           m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]] };
    
    Ipp32s isFast = m_par->FastInterp;

    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE * MAX_CU_SIZE];

    Ipp32s size = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   size = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) size = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) size = 2;
    else                                             size = 3;

    if (CheckIdenticalMotion(refIdx, mvsClipped)) {
        Ipp32s listIdx = !!(refIdx[0] < 0);
        ClipMV(mvsClipped[listIdx]);
        H265MV *mv = mvsClipped + listIdx;
        if ((mv->mvx | mv->mvy) & 3) {

            const PixType* predPtr;
            Ipp32s predPitch;
            if(MemSubpelUse(size, mv->mvx&3, mv->mvy&3, meInfo, m_currFrame->m_mapListRefUnique[listIdx][refIdx[listIdx]],
                &mvsClipped[listIdx], predPtr, predPitch)) 
            {
                IppiSize roiSize = {meInfo->width, meInfo->height};
                _ippiCopy_C1R((PixType*)predPtr, predPitch, predBuf, MAX_CU_SIZE, roiSize);
            } else {
                    PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, listIdx,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, false, AVERAGE_NO, isFast);
            }
        }
        else {
            // unique case; int pel from single ref
            Ipp32s refOffset = (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refFrame[listIdx]->m_recon->pitch_luma_pix;
            refOffset += m_ctbPelX + meInfo->posx + (mv->mvx >> 2);
            PixType *rec = (PixType *)refFrame[listIdx]->m_recon->y + refOffset;
            Ipp32s pitchRec = refFrame[listIdx]->m_recon->pitch_luma_pix;
            return ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)
                : h265_SAD_MxN_general(rec, pitchRec, src, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
        }
    }
    else {
        ClipMV(mvsClipped[0]);
        ClipMV(mvsClipped[1]);
        Ipp32s interpFlag0 = (mvsClipped[0].mvx | mvsClipped[0].mvy) & 3;
        Ipp32s interpFlag1 = (mvsClipped[1].mvx | mvsClipped[1].mvy) & 3;
        bool onlyOneIterp = !(interpFlag0 && interpFlag1);
#if 1
        if(!interpFlag0 && !interpFlag1) {
            Ipp32s listIdx = !interpFlag0;
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, listIdx,
                refIdx, mvsClipped, predBuf, MAX_CU_SIZE, true, AVERAGE_FROM_PIC, isFast);
        } else
#endif
        if (onlyOneIterp) {
            //if(!interpFlag0 && !interpFlag1) assert(0);
            Ipp32s listIdx = !interpFlag0;
            Ipp32s listIdx2 = !listIdx;
            Ipp16s *predBufY = NULL;
            Ipp32s predPitch = MAX_CU_SIZE;
            bool found=false;
            
            H265MV *ref_mv = mvsClipped + listIdx2;
            Ipp32s refOffset = (m_ctbPelY + meInfo->posy + (ref_mv->mvy >> 2)) * refFrame[listIdx2]->m_recon->pitch_luma_pix;
            refOffset += m_ctbPelX + meInfo->posx + (ref_mv->mvx >> 2);
            PixType *refY = (PixType *)refFrame[listIdx2]->m_recon->y + refOffset;
            Ipp32s pitchRef = refFrame[listIdx2]->m_recon->pitch_luma_pix;
            Ipp8u idx;
            for (idx = m_interpIdxFirst; idx !=m_interpIdxLast; idx = (idx + 1) & (INTERP_BUF_SZ - 1)) {
                if (m_currFrame->m_mapListRefUnique[listIdx][refIdx[listIdx]] == m_interpBufBi[idx].refIdx 
                    && mvsClipped[listIdx] == m_interpBufBi[idx].mv) {
                    predBufY = m_interpBufBi[idx].predBufY;
                    found=true;
                    break;
                }
            }
            
            if(!found) {
#ifdef MEMOIZE_BIPRED_HI_SAVE
                MeInterpolateUseSaveHi(meInfo, &mvsClipped[listIdx], (PixType *)refFrame[listIdx]->m_recon->y, 
                    refFrame[listIdx]->m_recon->pitch_luma_pix, predBufY, predPitch, size, 
                    m_currFrame->m_mapListRefUnique[listIdx][refIdx[listIdx]]);
#else
                MeInterpolateUseHi(meInfo, &mvsClipped[listIdx], (PixType *)refFrame[listIdx]->y, 
                    refFrame[listIdx]->pitch_luma_pix, predBufY, predPitch, size, 
                    m_currFrame->m_mapListRefUnique[listIdx][refIdx[listIdx]]);
#endif
            }
            WriteBiAverageToPicP(predBufY, predPitch, refY, pitchRef, predBuf, MAX_CU_SIZE, meInfo->width, meInfo->height, m_par->bitDepthLuma);
        }
        else {
            Ipp16s *predBufY[2] = {NULL, NULL};
            Ipp32s predPitch[2] = {MAX_CU_SIZE, MAX_CU_SIZE};
            bool found[2]={false,false};
            //for (Ipp8u dir = 0; dir < 2; dir++) 
            Ipp8u dir =0;
            Ipp8u idx;
            for (idx = m_interpIdxFirst; idx !=m_interpIdxLast; idx = (idx + 1) & (INTERP_BUF_SZ - 1)) {
                if (m_currFrame->m_mapListRefUnique[dir][refIdx[dir]] == m_interpBufBi[idx].refIdx && mvsClipped[dir] == m_interpBufBi[idx].mv) {
                    predBufY[dir] = m_interpBufBi[idx].predBufY;
                    found[dir]=true;
                    break;
                }
            }
            
            if(!found[0]) {
#ifdef MEMOIZE_BIPRED_HI_SAVE
                MeInterpolateUseSaveHi(meInfo, &mvsClipped[0], (PixType *)refFrame[0]->m_recon->y, refFrame[0]->m_recon->pitch_luma_pix, 
                    predBufY[0], predPitch[0], size, m_currFrame->m_mapListRefUnique[0][refIdx[0]]);
#else
                MeInterpolateUseHi(meInfo, &mvsClipped[0], (PixType *)refFrame[0]->y, refFrame[0]->pitch_luma_pix, 
                    predBufY[0], predPitch[0], size, m_currFrame->m_mapListRefUnique[0][refIdx[0]]);
#endif
            }
            dir = 1;
            for (idx = m_interpIdxFirst; idx !=m_interpIdxLast; idx = (idx + 1) & (INTERP_BUF_SZ - 1)) {
                if (m_currFrame->m_mapListRefUnique[dir][refIdx[dir]] == m_interpBufBi[idx].refIdx && mvsClipped[dir] == m_interpBufBi[idx].mv) {
                    predBufY[dir] = m_interpBufBi[idx].predBufY;
                    found[dir]=true;
                    break;
                }
            }
            if(!found[1]) {
#ifdef MEMOIZE_BIPRED_HI_SAVE
                MeInterpolateUseSaveHi(meInfo, &mvsClipped[1], (PixType *)refFrame[1]->m_recon->y, refFrame[1]->m_recon->pitch_luma_pix,
                    predBufY[1], predPitch[1], size, m_currFrame->m_mapListRefUnique[1][refIdx[1]]);
#else
                MeInterpolateUseHi(meInfo, &mvsClipped[1], (PixType *)refFrame[1]->y, refFrame[1]->pitch_luma_pix, 
                    predBufY[1], predPitch[1], size, m_currFrame->m_mapListRefUnique[1][refIdx[1]]);
#endif
            }

            WriteBiAverageToPicB(predBufY[0], predPitch[0], predBufY[1], predPitch[1], predBuf, MAX_CU_SIZE, 
                meInfo->width, meInfo->height, m_par->bitDepthLuma);
        }
    }

    return ((useHadamard)
        ? tuHad(src, m_pitchSrcLuma, predBuf, MAX_CU_SIZE, meInfo->width, meInfo->height)
        : h265_SAD_MxN_special(src, predBuf, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> (m_par->bitDepthLumaShift);
}
#endif

template <typename PixType>
Ipp32s H265CU<PixType>::MatchingMetricBipredPu(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], Ipp32s useHadamard)
{
    H265MV mvsClipped[2] = { mvs[0], mvs[1] };
    Frame *refFrame[2] = { m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]],
                               m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]] };

    ALIGN_DECL(32) PixType predBuf[MAX_CU_SIZE * MAX_CU_SIZE];

    Ipp32s isFast = m_par->FastInterp;

    if (CheckIdenticalMotion(refIdx, mvsClipped)) {
        Ipp32s listIdx = !!(refIdx[0] < 0);

        ClipMV(mvsClipped[listIdx]);
        H265MV *mv = mvsClipped + listIdx;
        if ((mv->mvx | mv->mvy) & 3) {
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, listIdx,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, false, AVERAGE_NO, isFast);
        }
        else {
            // unique case; int pel from single ref
            Ipp32s refOffset = (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * refFrame[listIdx]->m_recon->pitch_luma_pix;
            refOffset += m_ctbPelX + meInfo->posx + (mv->mvx >> 2);
            PixType *rec = (PixType *)refFrame[listIdx]->m_recon->y + refOffset;
            Ipp32s pitchRec = refFrame[listIdx]->m_recon->pitch_luma_pix;

            return ((useHadamard)
                ? tuHad(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)
                : h265_SAD_MxN_general(src, m_pitchSrcLuma, rec, pitchRec, meInfo->width, meInfo->height)) >> m_par->bitDepthLumaShift;
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
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, true, AVERAGE_FROM_PIC, isFast);
        }
        else {
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, REF_PIC_LIST_0,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, true, AVERAGE_NO, isFast);
            PredInterUni<TEXT_LUMA>(meInfo->posx, meInfo->posy, meInfo->width, meInfo->height, REF_PIC_LIST_1,
                                    refIdx, mvsClipped, predBuf, MAX_CU_SIZE, true, AVERAGE_FROM_BUF, isFast);
        }
    }

    return ((useHadamard)
        ? tuHad(src, m_pitchSrcLuma, predBuf, MAX_CU_SIZE, meInfo->width, meInfo->height)
        : h265_SAD_MxN_special(src, predBuf, m_pitchSrcLuma, meInfo->width, meInfo->height)) >> (m_par->bitDepthLumaShift);
}


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


template <typename PixType>
bool H265CU<PixType>::CheckFrameThreadingSearchRange(const H265MEInfo *meInfo, const H265MV *mv) const
{
    int searchRangeY = m_par->m_meSearchRangeY;//pixel
    int magic_wa = 1; //tmp wa for TU7 mismatch

    if (meInfo->posy + mv->mvy / 4 + meInfo->height + 4 + magic_wa > searchRangeY) { // +4 for interpolation 
        return false;//skip
    }
    
    return true;
}

template <typename PixType>
bool H265CU<PixType>::CheckIndepRegThreadingSearchRange(const H265MEInfo *meInfo, const H265MV *mv, Ipp8u subpel) const
{
    Ipp32s shiftVer = 2 + m_par->chromaShiftH;
    Ipp32s maskVer = (1 << shiftVer) - 1;
    Ipp32s mv_add = 0;

    if (subpel | (mv->mvy & maskVer)) {
        mv_add = 4;
    }

    if (((Ipp32s)m_ctbPelY + meInfo->posy + mv->mvy / 4 + (meInfo->height - 1) + mv_add) >= m_SlicePixRowLast) { // +mv_add for interpolation 
        return false;//skip
    }
    if (((Ipp32s)m_ctbPelY + meInfo->posy + mv->mvy / 4 - mv_add) < m_SlicePixRowFirst) { // +mv_add for interpolation 
        return false;//skip
    }
    
    return true;
}

template <typename PixType>
Ipp32s H265CU<PixType>::CheckMergeCand(const H265MEInfo *meInfo, const MvPredInfo<5> *mergeCand,
                                       Ipp32s useHadamard, Ipp32s *mergeCandIdx)
{
    Ipp32s costBest = INT_MAX;
    Ipp32s candBest = -1;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;

    for (Ipp32s i = 0; i < mergeCand->numCand; i++) {
        const Ipp8s *refIdx = mergeCand->refIdx + 2 * i;
        if (refIdx[0] < 0 && refIdx[1] < 0)
            continue; // skip duplicate

        const H265MV *mv = mergeCand->mvCand + 2 * i;

//#ifdef AMT_REF_SCALABLE
//        if(refIdx[0]>0) {
//            Frame *ref0 = m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]];
//            if(m_par->BiPyramidLayers == 4 && ref0->m_pyramidLayer>m_par->refLayerLimit[m_currFrame->m_pyramidLayer]) 
//                continue;
//        }
//        if(refIdx[1]>0) {
//            Frame *ref1 = m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]];
//            if(m_par->BiPyramidLayers == 4 && ref1->m_pyramidLayer>m_par->refLayerLimit[m_currFrame->m_pyramidLayer]) 
//                continue;
//        }
//#endif
        //-------------------------------------------------
        // FRAME THREADING PATCH
        if ((m_par->m_framesInParallel > 1) &&
            (refIdx[0] >= 0 && CheckFrameThreadingSearchRange(meInfo, mv + 0) == false ||
             refIdx[1] >= 0 && CheckFrameThreadingSearchRange(meInfo, mv + 1) == false)) {
            continue;
        }
        //------------------------------------------------
        if ((m_par->RegionIdP1 > 0) &&
            (refIdx[0] >= 0 && CheckIndepRegThreadingSearchRange(meInfo, mv + 0) == false ||
             refIdx[1] >= 0 && CheckIndepRegThreadingSearchRange(meInfo, mv + 1) == false)) {
            continue;
        }
        //------------------------------------------------

        Ipp32s cost;
        if (refIdx[0] >= 0 && refIdx[1] >= 0) {
#ifdef MEMOIZE_CAND
            cost = MatchingMetricBiPredPuMemCand(src, meInfo, refIdx, mv, useHadamard);
#else
            cost = MatchingMetricBipredPu(src, meInfo, refIdx, mv, useHadamard);
#endif
        }
        else {
            Ipp32s listIdx = (refIdx[1] >= 0);
            FrameData *ref = m_currFrame->m_refPicList[listIdx].m_refFrames[refIdx[listIdx]]->m_recon;
            H265MV mvc = mv[listIdx];
            ClipMV(mvc);
#ifdef MEMOIZE_CAND
            cost = MatchingMetricPuMemSCand(src, meInfo, listIdx, refIdx, &mvc, ref, useHadamard);
#else
            cost = MatchingMetricPu(src, meInfo, &mvc, ref, useHadamard);
#endif
        }

        if (costBest > cost) {
            cost += (Ipp32s)(GetFlBits(i, mergeCand->numCand) * m_rdLambdaSqrt + 0.5);
            if (costBest > cost) {
                costBest = cost;
                candBest = i;
            }
        }
    }

    *mergeCandIdx = candBest;
    return costBest;
}


// Tries different PU modes for CU
// m_data assumed to dataSave, not dataBest(depth)
template <typename PixType>
void H265CU<PixType>::MeCu(Ipp32s absPartIdx, Ipp8u depth)
{
    Ipp32s idx422 = (m_par->chroma422 && (m_par->AnalyseFlags & HEVC_COST_CHROMA)) ? (m_par->NumPartInCU >> (2 * depth + 1)) : 0;
    CostType costInitial = m_costCurr;
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
    m_bsf->CtxSave(ctxInitial);

    Ipp32s cuWidth = (m_par->MaxCUSize >> depth);
    Ipp32s cuWidthInMinTU = (cuWidth >> m_par->QuadtreeTULog2MinSize);
    H265CUData *dataCu = m_data + absPartIdx;

    // check skip/merge 2Nx2N
    GetMergeCand(absPartIdx, PART_SIZE_2Nx2N, 0, cuWidthInMinTU, m_mergeCand);
#ifdef AMT_ALT_FAST_SKIP
    m_skipCandBest = -1;
    bool bestIsSkip = CheckMerge2Nx2N(absPartIdx, depth);
    if (m_par->fastSkip && bestIsSkip)  return;
#else
    CheckMerge2Nx2N(absPartIdx, depth);
    // FRAME THREADING PATCH
    if (m_costCurr < COST_MAX && m_par->fastSkip && !dataCu->flags.skippedFlag && IsZeroCbf(dataCu, idx422)) {
        Ipp32u numParts = m_par->NumPartInCU >> (2 * depth);
        FillSubPartSkipFlag_(dataCu, numParts, 1);
        return;
    }
#endif

    // save the decision and restore initial states
    SaveBestInterPredAndResid(absPartIdx, depth);
    SaveFinalCuDecision(absPartIdx, depth, false);
    m_bsf->CtxRestore(ctxInitial);
    m_costCurr = costInitial;

    // 2Nx2N
    CheckInter(absPartIdx, depth, PART_SIZE_2Nx2N, NULL);
    if (m_par->fastCbfMode && IsZeroCbf(dataCu, idx422))
        return;
#ifdef AMT_FIX_CHROMA_SKIP_ORIGINAL
    if(!(m_par->AnalyseFlags & HEVC_COST_CHROMA) && m_costStored[depth] < m_costCurr) {
        // Skip is better
        H265CUData *cur_data = m_data;
        m_data = StoredCuData(depth);
        InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredC, MAX_CU_SIZE << m_par->chromaShiftWInv);
        // chroma distortion of Forced SKIP
        Ipp32s chromaCostShift = (m_par->bitDepthChromaShift << 1);
        const PixType *srcC = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchRecChroma);
        const PixType *predC = m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
        CostType costChroma =
            TuSse(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv, cuWidth << m_par->chromaShiftWInv, cuWidth >> m_par->chromaShiftH, chromaCostShift);
        costChroma /= 4; // add chroma with little weight
        m_costStored[depth]+=costChroma;
        m_data = cur_data;
    }
#endif

#ifdef AMT_ICRA_OPT
    // Last not best, and zero coeff for skip pred
    if(m_par->SkipMotionPartition >= 2 && m_par->partModes > 1 && m_costStored[depth] < m_costCurr) {
        bool continueMotionPartition = true;
        if(!m_isRdoq) {
            continueMotionPartition = false;
        } else {
            Ipp8u tr_depth_min, tr_depth_max;
            CoeffsType *residY = m_interResidY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
            const PixType *srcY = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchRecLuma);
            const PixType *predY = m_interPredBestY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
            TuDiff(residY, MAX_CU_SIZE, srcY, m_pitchSrcLuma, predY, MAX_CU_SIZE, cuWidth);
            if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && !m_par->chroma422) {
                const PixType *srcC = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchRecChroma);
                const PixType *predC = m_interPredBestC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
                CoeffsType *residU = m_interResidU + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
                CoeffsType *residV = m_interResidV + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
                h265_DiffNv12(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv,
                              residU, MAX_CU_SIZE >> m_par->chromaShiftW, residV, MAX_CU_SIZE >> m_par->chromaShiftW,
                              cuWidth >> m_par->chromaShiftW, cuWidth >> m_par->chromaShiftH);
            }

            GetTrDepthMinMax(m_par, depth, PART_SIZE_2Nx2N, &tr_depth_min, &tr_depth_max);
            continueMotionPartition = TuMaxSplitInterHasNonZeroCoeff(absPartIdx, tr_depth_max);
        }
        if(!continueMotionPartition) return;
    }
#endif

    H265MEInfo meInfo2Nx2N;
    meInfo2Nx2N.interDir = dataCu->interDir;
    meInfo2Nx2N.MV[0] = dataCu->mv[0];
    meInfo2Nx2N.MV[1] = dataCu->mv[1];
    meInfo2Nx2N.refIdx[0] = dataCu->refIdx[0];
    meInfo2Nx2N.refIdx[1] = dataCu->refIdx[1];

    // NxN
    if (depth == m_par->MaxCUDepth - m_par->AddCUDepth && cuWidth > 8) {
        // save the decision and restore initial states
        SaveBestInterPredAndResid(absPartIdx, depth);
        SaveFinalCuDecision(absPartIdx, depth, false);
        m_bsf->CtxRestore(ctxInitial);
        m_costCurr = costInitial;

        CheckInter(absPartIdx, depth, PART_SIZE_NxN, &meInfo2Nx2N);
        if (m_par->fastCbfMode && IsZeroCbf(dataCu, idx422))
            return;
    }

    if (m_par->partModes < 2 || (depth>=m_par->MotionPartitionDepth) || m_par->enableCmFlag && cuWidth == 8)
        return; // rectangular modes are not allowed

    // save the decision and restore initial states
    SaveBestInterPredAndResid(absPartIdx, depth);
    SaveFinalCuDecision(absPartIdx, depth, false);
    m_bsf->CtxRestore(ctxInitial);
    m_costCurr = costInitial;

    // Nx2N
    CheckInter(absPartIdx, depth, PART_SIZE_Nx2N, &meInfo2Nx2N);
    if (m_par->fastCbfMode && IsZeroCbf(dataCu, idx422))
        return;

    // save the decision and restore initial states
    SaveBestInterPredAndResid(absPartIdx, depth);
    SaveFinalCuDecision(absPartIdx, depth, false);
    m_bsf->CtxRestore(ctxInitial);
    m_costCurr = costInitial;

    // 2NxN
    CheckInter(absPartIdx, depth, PART_SIZE_2NxN, &meInfo2Nx2N);
    if (m_par->fastCbfMode && IsZeroCbf(dataCu, idx422))
        return;

    if (!m_par->AMPAcc[depth])
        return; // asymmetric modes are not allowed

    // save the decision and restore initial states
    SaveBestInterPredAndResid(absPartIdx, depth);
    SaveFinalCuDecision(absPartIdx, depth, false);
    m_bsf->CtxRestore(ctxInitial);
    m_costCurr = costInitial;

#ifdef AMT_ICRA_OPT
    const H265CUData *dataBestInterPreAMP = StoredCuData(depth)+absPartIdx;
    if(m_par->FastAMPRD>=2 && !m_par->enableCmFlag && (dataBestInterPreAMP->flags.skippedFlag!=1 && dataBestInterPreAMP->cbf[0]))
    {
        FastCheckAMP(absPartIdx, depth, &meInfo2Nx2N);
    } else
#endif
    {
        // 2NxnU
        CheckInter(absPartIdx, depth, PART_SIZE_2NxnU, &meInfo2Nx2N);
        if (m_par->fastCbfMode && IsZeroCbf(dataCu, idx422))
            return;

        // save the decision and restore initial states
        SaveBestInterPredAndResid(absPartIdx, depth);
        SaveFinalCuDecision(absPartIdx, depth, false);
        m_bsf->CtxRestore(ctxInitial);
        m_costCurr = costInitial;

        // 2NxnD
        CheckInter(absPartIdx, depth, PART_SIZE_2NxnD, &meInfo2Nx2N);
        if (m_par->fastCbfMode && IsZeroCbf(dataCu, idx422))
            return;

        // save the decision and restore initial states
        SaveBestInterPredAndResid(absPartIdx, depth);
        SaveFinalCuDecision(absPartIdx, depth, false);
        m_bsf->CtxRestore(ctxInitial);
        m_costCurr = costInitial;

        // nRx2N
        CheckInter(absPartIdx, depth, PART_SIZE_nRx2N, &meInfo2Nx2N);
        if (m_par->fastCbfMode && IsZeroCbf(dataCu, idx422))
            return;

        // save the decision and restore initial states
        SaveBestInterPredAndResid(absPartIdx, depth);
        SaveFinalCuDecision(absPartIdx, depth, false);
        m_bsf->CtxRestore(ctxInitial);
        m_costCurr = costInitial;

        // nLx2N
        CheckInter(absPartIdx, depth, PART_SIZE_nLx2N, &meInfo2Nx2N);
    }
}

void PropagateChromaCbf(H265CUData *data, Ipp32s numParts, Ipp32s num, Ipp32s trDepth, Ipp8u setBitFlag) {
    Ipp32s i;
    if (data->cbf[1]) {
        for (i = 0; i < num; i++)
            SetCbf<1>(data + numParts * i, trDepth);
        if (setBitFlag)
            for (i = 0; i < num; i++)
                SetCbfBit<1>(data + numParts * i, trDepth + 1);
    }
    else {
        for (i = 1; i < num; i++)
            ResetCbf<1>(data + numParts * i);
    }

    if (data->cbf[2]) {
        for (i = 0; i < num; i++)
            SetCbf<2>(data + numParts * i, trDepth);
        if (setBitFlag)
            for (i = 0; i < num; i++)
                SetCbfBit<2>(data + numParts * i, trDepth + 1);
    }
    else {
        for (i = 1; i < num; i++)
            ResetCbf<2>(data + numParts * i);
    }
}
template <typename PixType>
void H265CU<PixType>::TuGetSplitInter(Ipp32s absPartIdx, Ipp8u trDepth, Ipp8u trDepthMax, CostType *cost)
{
    H265CUData *data = m_data + absPartIdx;
    Ipp8u cuDepth = data->depth;
    Ipp32s numParts = m_par->NumPartInCU >> (2 * (cuDepth + trDepth));
    Ipp32s idx422 = 0;
    Ipp32s width = data->size >> trDepth;
    Ipp32s offset = absPartIdx * 16;

    // save initial states if we are going to test split
    CostType costInitial = m_costCurr;
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
    if (trDepth < trDepthMax)
        m_bsf->CtxSave(ctxInitial);
    // init storage
    m_costStored[cuDepth + trDepth + 1] = COST_MAX;

    CostType costBest = COST_MAX;
    FillSubPartTrDepth_(data, numParts, trDepth);
    data->cbf[1] = data->cbf[2] = 0; // clear chroma cbf since it affects PutTransform for Luma
    if(m_par->chroma422) data[numParts>>1].cbf[1] = data[numParts>>1].cbf[2] = 0;
    EncAndRecLumaTu(absPartIdx, offset, width, &costBest, 0, INTER_PRED_IN_BUF);
    FillSubPartCbfY_(data + 1, numParts - 1, data->cbf[0]);

    Ipp8u nonSplitCbfU[4] = {};
    Ipp8u nonSplitCbfV[4] = {};
    CostType costNonSplitChroma = 0;
    if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && width > 4) {
        if (m_par->chroma422) idx422 = (numParts >> 1);

        Ipp32s absPartIdx422 = 0;
        for (Ipp32s i = 0; i <= m_par->chroma422; i++) {
            CostType costChromaTemp;
            EncAndRecChromaTu(absPartIdx, absPartIdx422, offset >> m_par->chromaShift, width>>1, &costChromaTemp, INTER_PRED_IN_BUF, 0);
            PropagateChromaCbf(data + absPartIdx422, numParts >> 2, (2 - m_par->chroma422) << 1, trDepth, width == 8 && trDepth != trDepthMax);
            costNonSplitChroma += costChromaTemp;
            absPartIdx422 += (numParts >> 1);
        }
        for (Ipp32s i = 0; i < 4; i++) {
            nonSplitCbfU[i] = (data + (numParts >> 2) * i)->cbf[1];
            nonSplitCbfV[i] = (data + (numParts >> 2) * i)->cbf[2];
        }
        costBest += costNonSplitChroma;
    }

    m_costCurr += costBest;
#ifdef AMT_ADAPTIVE_TU_DEPTH
    CostType cuSplitThresholdTu = 0;
#else
    CostType cuSplitThresholdTu = m_par->cu_split_threshold_tu[data->qp][1][cuDepth + trDepth];
#endif
    if (costBest >= cuSplitThresholdTu && trDepth < trDepthMax && !IsZeroCbf(data, idx422)) {
        // before trying split
        // save decision made for non-split and restore initial states
        SaveInterTuDecision(absPartIdx, cuDepth + trDepth);
        m_bsf->CtxRestore(ctxInitial);
        m_costCurr = costInitial;

        m_costCurr += GetTransformSubdivFlagCost(cuDepth, trDepth);

        // try transform split
        Ipp8u cbfAcc[3] = {};
        numParts >>= 2;
        for (Ipp32s i = 0, absPartIdxSplit = absPartIdx; i < 4; i++, absPartIdxSplit += numParts) {
            CostType costTemp;
            TuGetSplitInter(absPartIdxSplit, trDepth + 1, trDepthMax, &costTemp);
            cbfAcc[0] |= m_data[absPartIdxSplit].cbf[0];
            cbfAcc[1] |= m_data[absPartIdxSplit].cbf[1];
            cbfAcc[2] |= m_data[absPartIdxSplit].cbf[2];
            if (m_par->chroma422) {
                cbfAcc[1] |= m_data[absPartIdxSplit + (numParts >> 1)].cbf[1];
                cbfAcc[2] |= m_data[absPartIdxSplit + (numParts >> 1)].cbf[2];
            }
        }
        if ((m_par->AnalyseFlags & HEVC_COST_CHROMA) && width == 8) {
            // no chroma for TU4x4
            // get chroma cost and cbf from non-split decision
            m_costCurr += costNonSplitChroma;
            for (Ipp32s i = 0; i < 4; i++) {
                (data + numParts * i)->cbf[1] = nonSplitCbfU[i];
                (data + numParts * i)->cbf[2] = nonSplitCbfV[i];
            }

            // when testing TU4x4 chroma-related contexts are not updated
            // we should take them from TU8x8 decision
            // which are stored in m_ctxStored[cuDepth + trDepth + 1]
            m_bsf->CtxRestore(m_ctxStored[cuDepth + trDepth + 1], tab_ctxIdxOffset[SIG_COEFF_GROUP_FLAG_HEVC] + 2, 2);
            m_bsf->CtxRestore(m_ctxStored[cuDepth + trDepth + 1], tab_ctxIdxOffset[SIG_FLAG_HEVC] + 27, 15);
            m_bsf->CtxRestore(m_ctxStored[cuDepth + trDepth + 1], tab_ctxIdxOffset[ONE_FLAG_HEVC] + 16, 8);
            m_bsf->CtxRestore(m_ctxStored[cuDepth + trDepth + 1], tab_ctxIdxOffset[ABS_FLAG_HEVC] + 4, 2);
            m_bsf->CtxRestore(m_ctxStored[cuDepth + trDepth + 1], tab_ctxIdxOffset[LAST_X_HEVC] + 15, 15);
            m_bsf->CtxRestore(m_ctxStored[cuDepth + trDepth + 1], tab_ctxIdxOffset[LAST_Y_HEVC] + 15, 15);
        } 
        else
        {
            if (cbfAcc[1])
                for (Ipp32s i = 0; i < 4; i++)
                    SetCbfBit<1>(data + numParts * i, trDepth);
            if (cbfAcc[2])
                for (Ipp32s i = 0; i < 4; i++)
                    SetCbfBit<2>(data + numParts * i, trDepth);
        }
        if (cbfAcc[0])
            for (Ipp32s i = 0; i < 4; i++)
                SetCbfBit<0>(data + numParts * i, trDepth);

#ifdef DUMP_COSTS_TU
        printCostStat(fp_tu, m_par->QP, 1, width,costBest,cost_split);
#endif

        // keep the best decision
        LoadInterTuDecision(absPartIdx, cuDepth + trDepth);
    }

    if (cost)
        *cost = m_costCurr - costInitial;
}


template <typename PixType>
void H265CU<PixType>::TuMaxSplitInter(Ipp32s absPartIdx, Ipp8u trIdxMax)
{
    Ipp8u depth = m_data[absPartIdx].depth;
    Ipp32u numParts = m_par->NumPartInCU >> (2 * depth);
    Ipp32u numTrParts = (m_par->NumPartInCU >> ((depth + trIdxMax) << 1));
    Ipp32s width = m_data[absPartIdx].size >> trIdxMax;

    FillSubPartTrDepth_(m_data + absPartIdx, numParts, trIdxMax);

    Ipp8u cbfAcc[3] = {};
    CostType costSum = 0;
    for (Ipp32u pos = 0; pos < numParts; pos += numTrParts) {
        CostType costTmp;
        EncAndRecLumaTu(absPartIdx + pos, (absPartIdx + pos) * 16, width, &costTmp, 0, INTER_PRED_IN_BUF);
        costSum += costTmp;
        cbfAcc[0] |= m_data[absPartIdx + pos].cbf[0];
    }
    if (cbfAcc[0]) {
        FillSubPartCbfY_(m_data + absPartIdx, numParts, cbfAcc[0]);
        if (trIdxMax > 0)
            m_data[absPartIdx].cbf[0] |= (1 << trIdxMax) - 1;
    }

    if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
        if (width == 4) {
            numTrParts <<= 2;
            width <<= 1;
        }
        for (Ipp32u pos = 0; pos < numParts; pos += numTrParts) {
            CostType costTmp;
            Ipp32s absPartIdx422 = 0;
            for (Ipp32s i = 0; i <= m_par->chroma422; i++) {
                EncAndRecChromaTu(absPartIdx + pos, absPartIdx422, ((absPartIdx + pos) * 16) >> m_par->chromaShift, width>>1, &costTmp, INTER_PRED_IN_BUF, 0);
                PropagateChromaCbf(m_data + absPartIdx + pos + absPartIdx422, numTrParts >> 2, (2 - m_par->chroma422) << 1, m_data->trIdx, 0);
                costSum += costTmp;
                cbfAcc[1] |= m_data[absPartIdx + pos].cbf[1];
                cbfAcc[2] |= m_data[absPartIdx + pos].cbf[2];
                absPartIdx422 += (numTrParts >> 1);
            }
        }
        if (cbfAcc[1]) {
            FillSubPartCbfU_(m_data + absPartIdx, numParts, cbfAcc[1]);
            if (trIdxMax > 0)
                m_data[absPartIdx].cbf[1] |= (1 << trIdxMax) - 1;
        }
        if (cbfAcc[2]) {
            FillSubPartCbfV_(m_data + absPartIdx, numParts, cbfAcc[2]);
            if (trIdxMax > 0)
                m_data[absPartIdx].cbf[2] |= (1 << trIdxMax) - 1;
        }
    }

    m_costCurr += costSum;
}

template <typename PixType>
void H265CU<PixType>::CuCost(Ipp32s absPartIdx, Ipp8u depth, const H265MEInfo *bestInfo)
{
    Ipp8u bestSplitMode = bestInfo[0].splitMode;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) );

    Ipp32u xborder = (bestInfo[0].posx + bestInfo[0].width) >> m_par->QuadtreeTULog2MinSize;
    Ipp32u yborder = (bestInfo[0].posy + bestInfo[0].height) >> m_par->QuadtreeTULog2MinSize;

    for (Ipp32u i = absPartIdx; i < absPartIdx + numParts; i++) {
        Ipp32u posx = h265_scan_z2r4[i] & 15;
        Ipp32u posy = h265_scan_z2r4[i] >> 4;
        Ipp32s partNxN = (posx<xborder ? 0 : 1) + (posy<yborder ? 0 : 2);
        Ipp32s part = (bestSplitMode != PART_SIZE_NxN) ? (partNxN ? 1 : 0) : partNxN;
        const H265MEInfo* mei = &bestInfo[part];

        VM_ASSERT(mei->interDir >= 1 && mei->interDir <= 3);
        m_data[i].partSize = bestSplitMode;
        m_data[i].cbf[0] = 0;
        m_data[i].cbf[1] = 0;
        m_data[i].cbf[2] = 0;
        m_data[i].flags.skippedFlag = 0;
        m_data[i].interDir = mei->interDir;
        m_data[i].refIdx[0] = mei->refIdx[0];
        m_data[i].refIdx[1] = mei->refIdx[1];
        m_data[i].mv[0] = mei->MV[0];
        m_data[i].mv[1] = mei->MV[1];
    }

    // set mv
    Ipp32s numPu = h265_numPu[m_data[absPartIdx].partSize];
    for (Ipp32s i = 0; i < numPu; i++) {
        Ipp32s partAddr;
        GetPartAddr(i, m_data[absPartIdx].partSize, numParts, partAddr);
        SetupMvPredictionInfo(absPartIdx + partAddr, m_data[absPartIdx].partSize, i);
    }

    // get residual pels
    Ipp32s width = m_par->MaxCUSize >> depth;
    IppiSize roiY = { width, width };
    IppiSize roiUv = { width, width >> 1 };
#ifdef MEMOIZE_SUBPEL_RECON
    const PixType *predBuf;
    Ipp32s memPitch;
    Ipp32s memSize = (m_par->Log2MaxCUSize-3-depth);
    if(m_data[absPartIdx].partSize==PART_SIZE_2Nx2N && m_data[absPartIdx].flags.mergeFlag==1 &&
        MemCandUseSubpel(bestInfo, m_data[absPartIdx].interDir-1, bestInfo->refIdx, (m_data[absPartIdx].interDir==2)?&bestInfo->MV[1]:bestInfo->MV, predBuf, memPitch)) {
            _ippiCopy_C1R(predBuf, memPitch, m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE), MAX_CU_SIZE, roiY);
    } else if(m_data[absPartIdx].partSize==PART_SIZE_2Nx2N && m_data[absPartIdx].interDir!=3 
        && (bestInfo->MV[bestInfo->interDir-1].mvx|bestInfo->MV[bestInfo->interDir-1].mvy)&0x3 &&
        MemSubpelUse(memSize, bestInfo->MV[bestInfo->interDir-1].mvx&0x3, bestInfo->MV[bestInfo->interDir-1].mvy&0x3, 
        bestInfo, m_currFrame->m_mapListRefUnique[bestInfo->interDir-1][bestInfo->refIdx[bestInfo->interDir-1]], 
        &(bestInfo->MV[bestInfo->interDir-1]), predBuf, memPitch))  {
            _ippiCopy_C1R(predBuf, memPitch, m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE), MAX_CU_SIZE, roiY);
#ifdef MEMOIZE_SUBPEL_RECON_TEST
            const PixType *srcY = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchSrcLuma);
            Ipp32s sse0 = TuSse(srcY, m_pitchSrcLuma, predBuf, memPitch, width, width, 0);
            InterPredCu<TEXT_LUMA>(absPartIdx, depth, m_interPredY, MAX_CU_SIZE);
            Ipp32s sse1 = TuSse(srcY, m_pitchSrcLuma, m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE), MAX_CU_SIZE, width, width, 0);
            assert(sse0==sse1);
#endif
    } else if(m_data[absPartIdx].partSize==PART_SIZE_2Nx2N && m_data[absPartIdx].interDir==3
        && (bestInfo->MV[0].mvx|bestInfo->MV[0].mvy|bestInfo->MV[1].mvx|bestInfo->MV[1].mvy)&0x3) {
        InterploateBipredUseHi(memSize, bestInfo, bestInfo->refIdx, bestInfo->MV, m_interPredY+ GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE));
#ifdef MEMOIZE_SUBPEL_RECON_TEST
            const PixType *srcY = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchSrcLuma);
            Ipp32s sse0 = TuSse(srcY, m_pitchSrcLuma, m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE), MAX_CU_SIZE, width, width, 0);
            InterPredCu<TEXT_LUMA>(absPartIdx, depth, m_interPredY, MAX_CU_SIZE);
            Ipp32s sse1 = TuSse(srcY, m_pitchSrcLuma, m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE), MAX_CU_SIZE, width, width, 0);
            assert(sse0==sse1);
#endif
    } else
#endif
    {
    InterPredCu<TEXT_LUMA>(absPartIdx, depth, m_interPredY, MAX_CU_SIZE);
    }
    if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
        InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredC, MAX_CU_SIZE);

    const PixType *srcY = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchSrcLuma);
    const PixType *predY = m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
    CoeffsType *residY = m_interResidY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
    TuDiff(residY, MAX_CU_SIZE, srcY, m_pitchSrcLuma, predY, MAX_CU_SIZE, width);

    if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
        const PixType *srcC = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchSrcChroma);
        const PixType *predC = m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
        CoeffsType *residU = m_interResidU + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
        CoeffsType *residV = m_interResidV + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
        h265_DiffNv12(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv,
                      residU, MAX_CU_SIZE >> m_par->chromaShiftW, residV, MAX_CU_SIZE >> m_par->chromaShiftW,
                      width >> m_par->chromaShiftW, width >> m_par->chromaShiftH);
    }

    Ipp8u trDepthMin, trDepthMax;
    GetTrDepthMinMax(m_par, depth, bestSplitMode, &trDepthMin, &trDepthMax);

#ifdef AMT_ADAPTIVE_TU_DEPTH
    if(trDepthMax>trDepthMin) {
        Ipp32s STC = GetSpatioTemporalComplexity(absPartIdx, depth, 0, depth);
        if(m_par->QuadtreeTUMaxDepthInter>m_par->QuadtreeTUMaxDepthInterRD) {
            if(STC>0) trDepthMax--;                                     // Not Very Low SpatioTemporal Complexity
        } else {
            if(STC>2 || (STC==1 && m_SCid[depth][absPartIdx]>4)) trDepthMax--;           // Medium SpatioTemporal Complexity
        }
        if(trDepthMax>trDepthMin) {
            if(STC>3 || (STC==2 && m_SCid[depth][absPartIdx]>4)) trDepthMax--;           // High SpatioTemporal Complexity
        }
    }
#endif

    //CostType bestCost = m_costStored[depth];

    Ipp32u numTrParts = (m_par->NumPartInCU >> ((depth + trDepthMin) << 1));
    Ipp8u cbfAcc[3] = {};
    for (Ipp32u pos = 0; pos < numParts; pos += numTrParts) {
        CostType costTemp;
        TuGetSplitInter(absPartIdx + pos, trDepthMin, trDepthMax, &costTemp);
        //if (m_costCurr >= m_costStored[depth]) {
        //    m_costCurr = COST_MAX;
        //    return;
        //}
        cbfAcc[0] |= m_data[absPartIdx + pos].cbf[0];
        cbfAcc[1] |= m_data[absPartIdx + pos].cbf[1];
        cbfAcc[2] |= m_data[absPartIdx + pos].cbf[2];
        if (m_par->chroma422) {
            cbfAcc[1] |= m_data[absPartIdx + pos + (numTrParts >> 1)].cbf[1];
            cbfAcc[2] |= m_data[absPartIdx + pos + (numTrParts >> 1)].cbf[2];
        }
    }

    if (trDepthMin > 0) {
        if (cbfAcc[0])
            for (Ipp32s i = 0; i < 4; i++)
                SetCbfBit<0>(m_data + absPartIdx + numParts * i / 4, 0);
        if (cbfAcc[1])
            for (Ipp32s i = 0; i < 4; i++)
                SetCbfBit<1>(m_data + absPartIdx + numParts * i / 4, 0);
        if (cbfAcc[2])
            for (Ipp32s i = 0; i < 4; i++)
                SetCbfBit<2>(m_data + absPartIdx + numParts * i / 4, 0);
    }

    m_costCurr += GetCuModesCost(m_data, absPartIdx, depth);
}

#ifdef AMT_INT_ME_SEED
template <typename PixType>
Ipp16s H265CU<PixType>::MeIntSeed(const H265MEInfo *meInfo, const MvPredInfo<2> *amvp, Ipp32s list, 
                                  Ipp32s refIdx, H265MV mvRefBest0, H265MV mvRefBest00, 
                                  H265MV &mvBest, Ipp32s &costBest, Ipp32s &mvCostBest, H265MV &mvBestSub)
{
    Ipp32s costOrig = costBest;
    Ipp16s meStepMax = MAX(MIN(meInfo->width, meInfo->height), 16) * 4;
    Ipp32s size = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   size = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) size = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) size = 2;
    else                                             size = 3;
    Ipp32s hadFoundSize = size;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;
    FrameData *ref = m_currFrame->m_refPicList[list].m_refFrames[refIdx]->m_recon;
    Ipp32s useHadamard = (m_par->hadamardMe == 3);
    // Helps ME quality, memoization & reduced dyn range helps speed.
    H265MV mvSeed;

    Ipp32s mvCostMax = (Ipp32s)(MvCost(meStepMax) * m_rdLambdaSqrt + 0.5);
    mvCostMax*=2;

    if(MemBestMV(size, m_currFrame->m_mapListRefUnique[list][refIdx], &mvSeed)) {
        H265MV mvSeedInt = mvSeed;
        if ((mvSeed.mvx | mvSeed.mvy) & 3) {
            mvSeedInt.mvx = (mvSeed.mvx + 1) & ~3;
            mvSeedInt.mvy = (mvSeed.mvy + 1) & ~3;
        }
        // PATCH
        bool checkSeed = true;
        if(m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, (const H265MV*)&mvSeedInt) == false ) {
            checkSeed = false;
        }
        if(checkSeed && m_par->RegionIdP1 > 0 && CheckIndepRegThreadingSearchRange(meInfo, (const H265MV*)&mvSeedInt) == false ) {
            checkSeed = false;
        }
        if(checkSeed && mvSeedInt!=mvBest) {
            Ipp32s hadFoundSize = size;
            Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mvSeedInt, ref, useHadamard, m_currFrame->m_mapListRefUnique[list][refIdx], size, hadFoundSize);
            if(cost < costBest) {
                Ipp32s mvCostSeed = MvCost1RefLog(mvSeedInt, amvp);
                if (costBest > cost+mvCostSeed) {
                    costBest = cost+mvCostSeed;
                    mvBest = mvSeedInt;
                    mvBestSub = mvSeed;
                    mvCostBest = mvCostSeed;
                }
            }
        }
    } 

    if(refIdx) {
        Ipp32s zeroPocDiff = m_currFrame->m_refPicList[list].m_deltaPoc[0];
        Ipp32s currPocDiff = m_currFrame->m_refPicList[list].m_deltaPoc[refIdx];
        Ipp32s scale = GetDistScaleFactor(currPocDiff, zeroPocDiff);
        H265MV mvProj = mvRefBest0;
        mvProj.mvx = (Ipp16s)Saturate(-32768, 32767, (scale * mvProj.mvx + 127 + (scale * mvProj.mvx < 0)) >> 8);
        mvProj.mvy = (Ipp16s)Saturate(-32768, 32767, (scale * mvProj.mvy + 127 + (scale * mvProj.mvy < 0)) >> 8);
        ClipMV(mvProj);
        bool checkProj = true;
        H265MV mvSeedInt = mvProj;
        if ((mvProj.mvx | mvProj.mvy) & 3) {
            mvSeedInt.mvx = (mvProj.mvx + 1) & ~3;
            mvSeedInt.mvy = (mvProj.mvy + 1) & ~3;
        }
        // PATCH
        if(m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, (const H265MV*)&mvSeedInt) == false ) {
            checkProj = false;
        }
        if(checkProj && m_par->RegionIdP1 > 0 && CheckIndepRegThreadingSearchRange(meInfo, (const H265MV*)&mvSeedInt) == false ) {
            checkProj = false;
        }
        if(checkProj && mvSeedInt!=mvBest) {
            Ipp32s hadFoundSize = size;
            Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mvSeedInt, ref, useHadamard, m_currFrame->m_mapListRefUnique[list][refIdx], size, hadFoundSize);
            if(cost < costBest) {
                Ipp32s mvCostSeed = MvCost1RefLog(mvSeedInt, amvp);
                if (costBest > cost+mvCostSeed) {
                    costBest = cost+mvCostSeed;
                    mvBest = mvSeedInt;
                    mvBestSub = mvProj;
                    mvCostBest = mvCostSeed;
                }
            }
        }
    } 

    if(list) {
        Ipp32s zeroPocDiff = m_currFrame->m_refPicList[0].m_deltaPoc[0];
        Ipp32s currPocDiff = m_currFrame->m_refPicList[list].m_deltaPoc[refIdx];
        Ipp32s scale = GetDistScaleFactor(currPocDiff, zeroPocDiff);
        H265MV mvProj = mvRefBest00;

        mvProj.mvx = (Ipp16s)Saturate(-32768, 32767, (scale * mvProj.mvx + 127 + (scale * mvProj.mvx < 0)) >> 8);
        mvProj.mvy = (Ipp16s)Saturate(-32768, 32767, (scale * mvProj.mvy + 127 + (scale * mvProj.mvy < 0)) >> 8);
        ClipMV(mvProj);
        bool checkProj = true;
        
        H265MV mvSeedInt = mvProj;
        if ((mvProj.mvx | mvProj.mvy) & 3) {
            mvSeedInt.mvx = (mvProj.mvx + 1) & ~3;
            mvSeedInt.mvy = (mvProj.mvy + 1) & ~3;
        }
        // PATCH
        if(m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, (const H265MV*)&mvSeedInt) == false ) {
            checkProj = false;
        }
        if(checkProj && m_par->RegionIdP1 > 0 && CheckIndepRegThreadingSearchRange(meInfo, (const H265MV*)&mvSeedInt) == false ) {
            checkProj = false;
        }
        if(checkProj && mvSeedInt!=mvBest) {
            Ipp32s hadFoundSize = size;
            Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mvSeedInt, ref, useHadamard, m_currFrame->m_mapListRefUnique[list][refIdx], size, hadFoundSize);
            if(cost < costBest) {
                Ipp32s mvCostSeed = MvCost1RefLog(mvSeedInt, amvp);
                if (costBest > cost+mvCostSeed) {
                    costBest = cost+mvCostSeed;
                    mvBest = mvSeedInt;
                    mvBestSub = mvProj;
                    mvCostBest = mvCostSeed;
                }
            }
        }
    }

    // Range by seed
    if (mvCostMax>mvCostBest && costBest-mvCostBest+mvCostMax < costOrig) {
        meStepMax/=2;
        mvCostMax = (Ipp32s)(MvCost(meStepMax) * m_rdLambdaSqrt + 0.5);
        mvCostMax*=2;
    }
    // Range by cost
    if((costBest-mvCostBest)<mvCostMax) {
        Ipp32s stepCost=mvCostMax;
        Ipp32s stepMax=meStepMax;
        while(stepCost>(costBest-mvCostBest) && stepCost>mvCostBest && stepMax>4) {
            stepMax/=2;
            stepCost = (Ipp32s)(MvCost(stepMax) * m_rdLambdaSqrt + 0.5);
            stepCost*=2;
        }
        meStepMax = stepMax;
    }
    return meStepMax;
}
#endif

#ifdef AMT_INT_ME_TRANSITION
const Ipp16s tab_mePattern[1 + 8 + 6][2] = {
    {0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}, {1, 0}, {1,1}, {0,1}
};
const struct TransitionTable {
    Ipp8u start;
    Ipp8u len;
} tab_meTransition[1 + 8 + 6] = {
    {1,8}, {7,5}, {1,3}, {1,5}, {3,3}, {3,5}, {5,3}, {5,5}, {7,3}, {7,5}, {1,3}, {1,5}, {3,3}, {3,5}, {5,3}
};
const struct TransitionTableStart {
    Ipp8u start;
    Ipp8u len;
} tab_meTransitionStart[1 + 8] = {
    {1,8}, {6,7}, {8,5}, {8,7}, {2,5}, {2,7}, {4,5}, {4,7}, {6,5}
};
const struct TransitionTableStart2 {
    Ipp8u start;
    Ipp8u len;
} tab_meTransitionStart2[1 + 8] = {
    {1,8}, {8,3}, {1,3}, {2,3}, {3,3}, {4,3}, {5,3}, {6,3}, {7,3}
};
const Ipp16s mePosToLoc[1 + 8 + 6] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6
};
const Ipp16s refineTransition[9][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 0, 0},
    {0, 0, 0, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 1, 1, 1},
    {1, 1, 0, 0, 0, 1, 1, 1},
    {1, 1, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 0, 0, 0, 1},
    {0, 1, 1, 1, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 0, 0}
};
#else
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
#endif

template <typename PixType>
#ifdef AMT_INT_ME_SEED
void H265CU<PixType>::MeIntPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                               const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s meStepRange) const
#else
void H265CU<PixType>::MeIntPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                               const Frame *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
#endif
{
    if (m_par->patternIntPel == 1) {
#ifdef AMT_INT_ME_SEED
        return MeIntPelLog(meInfo, predInfo, ref, mv, cost, mvCost, meStepRange);
#else
        return MeIntPelLog(meInfo, predInfo, ref, mv, cost, mvCost);
#endif
    }
    else if (m_par->patternIntPel == 100) {
        return MeIntPelFullSearch(meInfo, predInfo, ref, mv, cost, mvCost);
    }
    else {
        VM_ASSERT(0);
    }
}


template <typename PixType>
void H265CU<PixType>::MeIntPelFullSearch(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, 
                                         const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    const Ipp32s RANGE = 4 * 64;

    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;
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
            Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
            if (costBest > cost) {
                Ipp32s mvCost = MvCost1RefLog(mv, predInfo);
                cost += mvCost;
                if (costBest > cost) {
                    costBest = cost;
                    mvCostBest = mvCost;
                    mvBest = mv;
                }
            }
        }
    }

    *mv = mvBest;
    *cost = costBest;
    *mvCost = mvCostBest;
}

#ifdef AMT_FAST_SUBPEL_SEARCH
const Ipp16s tab_mePatternSelector[5][12][2] = {
    {{0,0}, {0,-1},  {1,0},  {0,1},  {-1,0}, {0,0}, {0,0}, {0,0},  {0,0},  {0,0},   {0,0},  {0,0}}, //diamond
    {{0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0},  {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}}, //box
    {{0,0}, {-1,-1}, {1,-1}, {1,1}, {-1,1},  {0,0}, {0,0}, {0,0},  {0,0},  {0,0},   {0,0},  {0,0}},  //Sq
    {{0,0}, {-1, 0}, {1, 0}, {0,0}, {0,0},  {0,0},  {0,0}, {0,0},  {0,0},  {0,0},   {0,0},  {0,0}},  //H
    {{0,0}, { 0,-1}, {0, 1}, {0,0}, {0,0},  {0,0},  {0,0}, {0,0},  {0,0},  {0,0},   {0,0},  {0,0}},  //V
};
#else
const Ipp16s tab_mePatternSelector[2][12][2] = {
    {{0,0}, {0,-1},  {1,0},  {0,1},  {-1,0}, {0,0}, {0,0}, {0,0},  {0,0},  {0,0},   {0,0},  {0,0}}, //diamond
    {{0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0},  {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}} //box
};
#endif

template <typename PixType>
#ifdef AMT_INT_ME_SEED
void H265CU<PixType>::MeIntPelLog(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                                  const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s meStepRange) const
#else
void H265CU<PixType>::MeIntPelLog(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                                  const Frame *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
#endif
{
    // Log2( step ) is used, except meStepMax
    Ipp16s meStepBest = 2;
#ifdef AMT_INT_ME_SEED
    Ipp16s meStepOrig = MAX(MIN(meInfo->width, meInfo->height), 16) * 4;
    Ipp16s meStepMax = MIN(meStepRange, meStepOrig);
#else
    Ipp16s meStepMax = MAX(MIN(meInfo->width, meInfo->height), 16) * 4;
#endif
    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;

    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;
    Ipp32s useHadamard = (m_par->hadamardMe == 3);
    Ipp16s mePosBest = 0;
    // expanding search
    H265MV mvCenter = mvBest;
    for (Ipp16s meStep = meStepBest; (1<<meStep) <= meStepMax; meStep ++) {
        for (Ipp16s mePos = 1; mePos < 9; mePos++) {
            H265MV mv = {
                Ipp16s(mvCenter.mvx + (tab_mePattern[mePos][0] << meStep)),
                Ipp16s(mvCenter.mvy + (tab_mePattern[mePos][1] << meStep))
            };
            if (ClipMV(mv))
                continue;

            //-------------------------------------------------
            // FRAME THREADING PATCH
            //searchRangeY = FrameHeight / numFrameEncoders;
            //if (mv->mvy / 4 + 4 > searchRangeY) // +4 for interpolation
            //    continue;

            if(m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, (const H265MV*)&mv) == false ) {
                continue;
            }
            //-------------------------------------------------
            if(m_par->RegionIdP1 > 0 && CheckIndepRegThreadingSearchRange(meInfo, (const H265MV*)&mv) == false ) {
                continue;
            }
            //-------------------------------------------------

            Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);

            if (costBest > cost) {
                Ipp32s mvCost = MvCost1RefLog(mv, predInfo);
                cost += mvCost;
                if (costBest > cost) {
                    costBest = cost;
                    mvCostBest = mvCost;
                    mvBest = mv;
                    meStepBest = meStep;
                    mePosBest = mePos;
                }
            }
        }
    }

    // then logarithm from best
    // for Cpattern
    Ipp32s transPos = mePosBest;
#ifdef AMT_INT_ME_TRANSITION
    Ipp8u start = 0, len = 1;
    Ipp16s meStep = meStepBest;
    if(meStepBest>2) {
        start = tab_meTransitionStart[mePosBest].start;
        len   = tab_meTransitionStart[mePosBest].len;
        meStep--;
    } else {
#ifdef NO_ME_2
        meStep = 0;
#else
        if(mvBest!=*mv) {
            start = tab_meTransitionStart2[mePosBest].start;
            len   = tab_meTransitionStart2[mePosBest].len;
        } else {
            meStep=0;
        }
#endif
    }
#else
    Ipp8u start = 0, len = 1;
    Ipp16s meStep = meStepBest;
#endif
    mvCenter = mvBest;
    Ipp32s iterbest = costBest;
    while (meStep >= 2) {
        Ipp32s refine = 1;
        Ipp32s bestPos = 0;
        for (Ipp16s mePos = start; mePos < start + len; mePos++) {
            H265MV mv = {
                Ipp16s(mvCenter.mvx + (tab_mePattern[mePos][0] << meStep)),
                Ipp16s(mvCenter.mvy + (tab_mePattern[mePos][1] << meStep))
            };
            if (ClipMV(mv))
                continue;

            //-------------------------------------------------
            // FRAME THREADING PATCH
            //searchRangeY = FrameHeight / numFrameEncoders;
            //if (mv->mvy / 4 + 4 > searchRangeY) // +4 for interpolation
            //    continue;

            if(m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, (const H265MV*)&mv) == false ) {
                continue;
            }
            //-------------------------------------------------
            if(m_par->RegionIdP1 > 0 && CheckIndepRegThreadingSearchRange(meInfo, (const H265MV*)&mv) == false ) {
                continue;
            }
            //-------------------------------------------------

            Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);

            if (costBest > cost) {
                Ipp32s mvCost = MvCost1RefLog(mv, predInfo);
                cost += mvCost;
                if (costBest > cost) {
                    costBest = cost;
                    mvCostBest = mvCost;
                    mvBest = mv;
                    refine = 0;
                    bestPos = mePos;
                }
            }
        }

        if (refine) {
            meStep --;
        } else {
#if defined(AMT_INT_ME_TRANSITION)
            if(refineTransition[mePosToLoc[transPos]][mePosToLoc[bestPos]-1]) {
                meStep--;
                bestPos = 0;
            } else
#endif
            if((costBest+m_rdLambdaSqrt)>iterbest) {
                meStep--;
                bestPos = 0;
            }
#ifndef NO_ME_2
            else if(meStep<=4) {
                meStep++;
            }
#endif
            mvCenter = mvBest;
        }
        start = tab_meTransition[bestPos].start;
        len   = tab_meTransition[bestPos].len;
        iterbest = costBest;
        transPos = bestPos;
    }

    *mv = mvBest;
    *cost = costBest;
    *mvCost = mvCostBest;
}

const UMC_HEVC_DECODER::EnumTextType LUMA = (UMC_HEVC_DECODER::EnumTextType)TEXT_LUMA;

template <typename TSrc, typename TDst>
void InterpHor(const TSrc *src, Ipp32s pitchSrc, TDst *dst, Ipp32s pitchDst, Ipp32s dx,
               Ipp32s width, Ipp32s height, Ipp32u shift, Ipp16s offset, Ipp32u bitDepth)
{
    Interpolate<LUMA>(INTERP_HOR, src, pitchSrc, dst, pitchDst, dx, width, height, shift, offset, bitDepth, 0);
}

template <typename TSrc, typename TDst>
void InterpVer(const TSrc *src, Ipp32s pitchSrc, TDst *dst, Ipp32s pitchDst, Ipp32s dy,
               Ipp32s width, Ipp32s height, Ipp32u shift, Ipp16s offset, Ipp32u bitDepth)
{
    Interpolate<LUMA>(INTERP_VER, src, pitchSrc, dst, pitchDst, dy, width, height, shift, offset, bitDepth, 0);
}


template <typename PixType>
void H265CU<PixType>::AddMvCost(const MvPredInfo<2> *predInfo, Ipp32s log2Step, const Ipp32s *dists,
                                H265MV *mv, Ipp32s *costBest, Ipp32s *mvCostBest, Ipp32s patternSubPel) const
{
    const Ipp16s (*pattern)[2] = tab_mePatternSelector[Ipp32s(patternSubPel == SUBPEL_BOX)] + 1;
    Ipp32s numMvs = (patternSubPel == SUBPEL_BOX) ? 8 : 4; // BOX or DIA

    H265MV centerMv = *mv;
    for (Ipp32s i = 0; i < numMvs; i++) {
        Ipp32s cost = dists[i];
        if (*costBest > cost) {
            Ipp16s mvx = centerMv.mvx + (pattern[i][0] << log2Step);
            Ipp16s mvy = centerMv.mvy + (pattern[i][1] << log2Step);
            Ipp32s mvCost = MvCost1RefLog(mvx, mvy, predInfo);
            cost += mvCost;
            if (*costBest > cost) {
                *costBest = cost;
                *mvCostBest = mvCost;
                mv->mvx = mvx;
                mv->mvy = mvy;
            }
        }
    }
}


template <typename PixType>
void H265CU<PixType>::MeSubPelBatchedDia(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                                         const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    VM_ASSERT(m_par->patternSubPel == SUBPEL_DIA);
    using MFX_HEVC_PP::h265_InterpLumaPack;

    Ipp32s w = meInfo->width;
    Ipp32s h = meInfo->height;
    Ipp32s bitDepth = m_par->bitDepthLuma;
    Ipp32s costShift = m_par->bitDepthLumaShift;
    Ipp32s shift = 20 - bitDepth;
    Ipp32s offset = 1 << (19 - bitDepth);
    Ipp32s useHadamard = (m_par->hadamardMe >= 2);

    Ipp32s pitchSrc = m_pitchSrcLuma;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * pitchSrc;

    Ipp32s pitchRef = ref->pitch_luma_pix;
    const PixType *refY = (const PixType *)ref->y;
    refY += (Ipp32s)(m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * pitchRef);

    Ipp32s (*costFunc)(const PixType*, Ipp32s, const PixType*, Ipp32s, Ipp32s, Ipp32s);
    costFunc = (useHadamard ? tuHad_Kernel<PixType> : h265_SAD_MxN_general<PixType>);
    Ipp32s costs[4];

    Ipp32s pitchTmp2 = (w + 1 + 15) & ~15;
    Ipp32s pitchHpel = (w + 1 + 15) & ~15;
    ALIGN_DECL(32) Ipp16s tmpPels[(MAX_CU_SIZE + 16) * (MAX_CU_SIZE + 8)];
    ALIGN_DECL(32) PixType subpel[(MAX_CU_SIZE + 16) * (MAX_CU_SIZE + 2)];

    // intermediate halfpels
    InterpHor(refY - 1 - 4 * pitchRef, pitchRef, tmpPels, pitchTmp2, 2, (w + 8) & ~7, h + 8, bitDepth - 8, 0, bitDepth); // (intermediate)
    Ipp16s *tmpHor2 = tmpPels + 3 * pitchTmp2;

    // interpolate horizontal halfpels
    h265_InterpLumaPack(tmpHor2 + pitchTmp2, pitchTmp2, subpel, pitchHpel, w + 1, h, bitDepth);
    costs[1] = costFunc(src, pitchSrc, subpel + 1, pitchHpel, w, h) >> costShift;
    costs[3] = costFunc(src, pitchSrc, subpel,     pitchHpel, w, h) >> costShift;

    // interpolate vertical halfpels
    pitchHpel = (w + 15) & ~15;
    InterpVer(refY - pitchRef, pitchRef, subpel, pitchHpel, 2, w, h + 2, 6, 32, bitDepth);
    costs[0] = costFunc(src, pitchSrc, subpel,             pitchHpel, w, h) >> costShift;
    costs[2] = costFunc(src, pitchSrc, subpel + pitchHpel, pitchHpel, w, h) >> costShift;

    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;
    if (m_par->hadamardMe == 2) {
        // when satd is for subpel only need to recalculate cost for intpel motion vector
        costBest = tuHad(src, pitchSrc, refY, pitchRef, w, h) >> costShift;
        costBest += mvCostBest;
    }

    AddMvCost(predInfo, 1, costs, &mvBest, &costBest, &mvCostBest, m_par->patternSubPel);

    Ipp32s hpelx = mvBest.mvx - mv->mvx + 4; // can be 2, 4 or 6
    Ipp32s hpely = mvBest.mvy - mv->mvy + 4; // can be 2, 4 or 6
    Ipp32s dx = hpelx & 3; // can be 0 or 2
    Ipp32s dy = hpely & 3; // can be 0 or 2

    Ipp32s pitchQpel = (w + 15) & ~15;

    // interpolate vertical quater-pels
    if (dx == 0) // best halfpel is intpel or ver-halfpel
        InterpVer(refY + (hpely - 5 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, 3 - dy, w, h, 6, 32, bitDepth);                             // hpx+0 hpy-1/4
    else // best halfpel is hor-halfpel
        InterpVer(tmpHor2 + (hpelx >> 2) + (hpely - 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth); // hpx+0 hpy-1/4
    costs[0] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    // interpolate vertical qpels
    if (dx == 0) // best halfpel is intpel or ver-halfpel
        InterpVer(refY + (hpely - 3 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, dy + 1, w, h, 6, 32, bitDepth);                             // hpx+0 hpy+1/4
    else // best halfpel is hor-halfpel or diag-halfpel
        InterpVer(tmpHor2 + (hpelx >> 2) + (hpely + 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth); // hpx+0 hpy+1/4
    costs[2] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    // interpolate horizontal quater-pels (left of best half-pel)
    if (dy == 0) { // best halfpel is intpel or hor-halfpel
        InterpHor(refY + (hpelx - 5 >> 2), pitchRef, subpel, pitchHpel, 3 - dx, w, h, 6, 32, bitDepth);                                 // hpx-1/4 hpy+0
    }
    else { // best halfpel is vert-halfpel or diag-halfpel
        // intermediate horizontal quater-pels (left of best half-pel)
        Ipp32s pitchTmp1 = (w + 15) & ~15;
        Ipp16s *tmpHor1 = tmpPels + 3 * pitchTmp1;
        InterpHor(refY - 4 * pitchRef + (hpelx - 5 >> 2), pitchRef, tmpPels, pitchTmp1, 3 - dx, w, h + 8, bitDepth - 8, 0, bitDepth);  // hpx-1/4 hpy+0 (intermediate)
        // final horizontal quater-pels
        InterpVer(tmpHor1 + (hpely >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth);                // hpx-1/4 hpy+0
    }
    costs[3] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    // interpolate horizontal quater-pels (right of best half-pel)
    if (dy == 0) { // best halfpel is intpel or hor-halfpel
        InterpHor(refY + (hpelx - 3 >> 2), pitchRef, subpel, pitchHpel, dx + 1, w, h, 6, 32, bitDepth);                                 // hp+1/4 hpy+0
    }
    else { // best halfpel is vert-halfpel or diag-halfpel
        // intermediate horizontal quater-pels (right of best half-pel)
        Ipp32s pitchTmp3 = (w + 15) & ~15;
        Ipp16s *tmpHor3 = tmpPels + 3 * pitchTmp3;
        InterpHor(refY - 4 * pitchRef + (hpelx - 3 >> 2), pitchRef, tmpPels, pitchTmp3, dx + 1, w, h + 8, bitDepth - 8, 0, bitDepth);  // hpx+1/4 hpy+0 (intermediate)
        // final horizontal quater-pels
        InterpVer(tmpHor3 + (hpely >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth);                // hpx+1/4 hpy+0
    }
    costs[1] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    AddMvCost(predInfo, 0, costs, &mvBest, &costBest, &mvCostBest, m_par->patternSubPel);

    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;
}

#ifdef MEMOIZE_SUBPEL
#define CompareFastCosts(costFast, bestFastCost, testPos, shift, bestPos, bestmvCost, bestmv) { \
   if(costFast<bestFastCost) {                                                                  \
        H265MV tmpMv = {mvCenter.mvx+((Ipp16s)tab_mePatternSelector[1][testPos+1][0]<<shift),   \
                        mvCenter.mvy+((Ipp16s)tab_mePatternSelector[1][testPos+1][1]<<shift)};  \
        Ipp32s tmpMvCost = MvCost1RefLog(tmpMv.mvx, tmpMv.mvy, predInfo);                       \
        costFast += tmpMvCost;                                                                  \
        if(costFast<bestFastCost) {                                                             \
            bestFastCost = costFast;                                                            \
            bestPos = testPos;                                                                  \
            bestmvCost = tmpMvCost;                                                             \
            bestmv = tmpMv;                                                                     \
        }                                                                                       \
    }                                                                                           \
}

template <typename PixType>
void H265CU<PixType>::MemSubPelBatchedFastBoxDiaOrth(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                                         const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s refIdxMem, Ipp32s size)
{
    VM_ASSERT(m_par->patternSubPel == SUBPEL_FASTBOX_DIA_ORTH);
    using MFX_HEVC_PP::h265_InterpLumaPack;

    Ipp32s w = meInfo->width;
    Ipp32s h = meInfo->height;
    Ipp32s bitDepth = m_par->bitDepthLuma;
    Ipp32s costShift = m_par->bitDepthLumaShift;
    Ipp32s shift = 20 - bitDepth;
    Ipp32s offset = 1 << (19 - bitDepth);
    Ipp32s useHadamard = (m_par->hadamardMe >= 2);

    Ipp32s (*costFunc)(const PixType*, Ipp32s, const PixType*, Ipp32s, Ipp32s, Ipp32s);
    costFunc = h265_SAD_MxN_general<PixType>;

    Ipp32s isFast = m_par->FastInterp;

    Ipp32s pitchSrc = m_pitchSrcLuma;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * pitchSrc;
    Ipp32s pitchRef = ref->pitch_luma_pix;
    const PixType *refY = (const PixType *)ref->y;
    refY += (Ipp32s)(m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * pitchRef);

    Ipp32s memPitch = (MAX_CU_SIZE>>(3-size))+8;
    PixType *tmpSubMem[8];

    Ipp16s *predBufHi20;
    PixType *predBuf20;
    H265MV tmv;
    tmv =*mv;
    tmv.mvx +=2;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf20, predBufHi20);
    // intermediate halfpels Hi[2,0]: change (-1, -4) (w+8, h+8) to (-4,-4) (w+8, h+8)
    InterpHor(refY - 4 - 4 * pitchRef, pitchRef, predBufHi20, memPitch, 2, (w + 8) & ~7, h + 10, bitDepth - 8, 0, bitDepth);    // (intermediate, save to predBufHi[2][0])
    Ipp16s *tmpHor2 = predBufHi20 + 3 * memPitch + 3;                                                                           // Save For Quarter Pel

    // interpolate horizontal halfpels: Lo (2,0) (-4, -4) (w+8, h+8)
    h265_InterpLumaPack(predBufHi20, memPitch, predBuf20, memPitch, w + 8, h + 8, bitDepth);                                    // save to predBuf[2][0]
    
    tmpSubMem[7] = predBuf20 + 4 * memPitch + 3;
    tmpSubMem[3] = tmpSubMem[7]+1;

    Ipp32s *satd8=NULL;
    Ipp32s satdPitch = MAX_CU_SIZE>>(3-size);
    H265MV mvCenter = *mv;
    Ipp32s costFast = 0;
    Ipp32s bestFastCost = *cost; 
    Ipp32s bestHalf = -1;
    Ipp32s bestmvCostHalf = *mvCost;
    H265MV bestmvHalf = *mv;

    costFast = costFunc(src, pitchSrc, tmpSubMem[3], memPitch, w, h)>> costShift;
    CompareFastCosts(costFast, bestFastCost, 3, 1, bestHalf, bestmvCostHalf, bestmvHalf);

    costFast = costFunc(src, pitchSrc, tmpSubMem[7],     memPitch, w, h)>> costShift;
    CompareFastCosts(costFast, bestFastCost, 7, 1, bestHalf, bestmvCostHalf, bestmvHalf);

    // interpolate vertical halfpels
    Ipp16s *predBufHi02;
    PixType *predBuf02;
    tmv = *mv;
    tmv.mvy +=2;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf02, predBufHi02);
    // Hi [0,2] (0,-1) (w, h+2)
    InterpVer(refY - pitchRef, pitchRef, predBufHi02, memPitch, 2, w, h + 2,  bitDepth - 8, 0, bitDepth);                    // save to predBufHi[0][2]
    // Lo [0,2] (0,-1) (w, h+2)
    h265_InterpLumaPack(predBufHi02, memPitch, predBuf02, memPitch, w, h + 2, bitDepth);                                    // save to predBuf[0][2]

    tmpSubMem[1] = predBuf02;
    tmpSubMem[5] = predBuf02 + memPitch;

    costFast = costFunc(src, pitchSrc, tmpSubMem[1],             memPitch, w, h)>> costShift;
    CompareFastCosts(costFast, bestFastCost, 1, 1, bestHalf, bestmvCostHalf, bestmvHalf);

    costFast = costFunc(src, pitchSrc, tmpSubMem[5],  memPitch, w, h)>> costShift;
    CompareFastCosts(costFast, bestFastCost, 5, 1, bestHalf, bestmvCostHalf, bestmvHalf);

    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;
    if (m_par->hadamardMe >= 2) { // always
        // when satd is for subpel only need to recalculate cost for intpel motion vector
        MemHadGetBuf(size, mv->mvx&3, mv->mvy&3, refIdxMem, mv, satd8);
        costBest = tuHadSave(src, pitchSrc, refY, pitchRef, w, h, satd8, satdPitch) >> costShift;                           // Save to Had[0][0]
        costBest += mvCostBest;
    }

    if(bestHalf>0) 
    {
        // interpolate diagonal halfpels Hi (2,2) (-4,-1) (w+8, h+2)
        Ipp16s *tmpHor22 = predBufHi20 + 3 * memPitch;
        Ipp16s *predBufHi22;
        PixType *predBuf22;
        tmv.mvx = mv->mvx + 2;
        tmv.mvy = mv->mvy + 2;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf22, predBufHi22);
        InterpVer(tmpHor22, memPitch, predBufHi22, memPitch, 2, (w + 8) & ~7, h + 2, 6, 0, bitDepth);                         // save to predBufHi[2][2]
        // Lo (2,2) (-4,-1) (w+8, h+2)
        h265_InterpLumaPack(predBufHi22, memPitch, predBuf22, memPitch, w + 8, h + 2, bitDepth);                                // save to predBuf[2][2]

        tmpSubMem[0] = predBuf22 + 3;
        tmpSubMem[2] = tmpSubMem[0] + 1;
        tmpSubMem[4] = tmpSubMem[0] + 1 + memPitch;
        tmpSubMem[6] = tmpSubMem[0] + memPitch;

        costFast = costFunc(src, pitchSrc, tmpSubMem[0],                 memPitch, w, h)>> costShift;
        CompareFastCosts(costFast, bestFastCost, 0, 1, bestHalf, bestmvCostHalf, bestmvHalf);

        costFast = costFunc(src, pitchSrc, tmpSubMem[2],             memPitch, w, h)>> costShift;
        CompareFastCosts(costFast, bestFastCost, 2, 1, bestHalf, bestmvCostHalf, bestmvHalf);

        costFast = costFunc(src, pitchSrc, tmpSubMem[4],  memPitch, w, h)>> costShift;
        CompareFastCosts(costFast, bestFastCost, 4, 1, bestHalf, bestmvCostHalf, bestmvHalf);

        costFast = costFunc(src, pitchSrc, tmpSubMem[6],  memPitch, w, h)>> costShift;
        CompareFastCosts(costFast, bestFastCost, 6, 1, bestHalf, bestmvCostHalf, bestmvHalf);

        MemHadGetBuf(size, bestmvHalf.mvx&3, bestmvHalf.mvy&3, refIdxMem, &bestmvHalf, satd8);
        bestFastCost = tuHadSave(src, pitchSrc, tmpSubMem[bestHalf], memPitch, w, h, satd8, satdPitch) >> costShift;                     // save to Had

        bestFastCost += bestmvCostHalf;
        if (costBest > bestFastCost) {
            costBest = bestFastCost;
            mvCostBest = bestmvCostHalf;
            mvBest = bestmvHalf;
        }
    }

    Ipp32s hpelx = mvBest.mvx - mv->mvx + 4; // can be 2, 4 or 6
    Ipp32s hpely = mvBest.mvy - mv->mvy + 4; // can be 2, 4 or 6
    Ipp32s dx = hpelx & 3; // can be 0 or 2
    Ipp32s dy = hpely & 3; // can be 0 or 2

    mvCenter = mvBest;
    costFast = 0;
    bestFastCost = INT_MAX; 
    Ipp32s bestQuarterDia = 1;
    Ipp32s bestQuarterDiaMvCost = 0;
    H265MV bestmvQuarter;

    // interpolate vertical quater-pels 
    if (dx == 0) // best halfpel is intpel or ver-halfpel [0,1/3] (0,0)(w,h)
    {
        Ipp16s *predBufHi0q1;
        PixType *predBuf0q1;
        tmv = mvBest;
        tmv.mvy -=1;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf0q1, predBufHi0q1);
        InterpVer(refY + (hpely - 5 >> 2) * pitchRef, pitchRef, predBufHi0q1, memPitch, 3 - dy, w, h, bitDepth - 8, 0, bitDepth);                   // hpx+0 hpy-1/4 predBufHi[0][1/3]
        h265_InterpLumaPack(predBufHi0q1, memPitch, predBuf0q1, memPitch, w, h, bitDepth);                                                          // predBuf[0][1/3]
        tmpSubMem[1] = predBuf0q1;
    }
    else // best halfpel is hor-halfpel or diag-halfpel
    {
        Ipp16s *predBufHi2q1;
        PixType *predBuf2q1;
        tmv.mvx = mvBest.mvx;
        tmv.mvy = mv->mvy + 3 - dy;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf2q1, predBufHi2q1);
        //[2,1/3] (0,0) (w,h+2)
        InterpVer(tmpHor2 + (hpelx >> 2), memPitch, predBufHi2q1, memPitch, 3 - dy, w, h+2, 6, 0, bitDepth);                                     // hpx+0 hpy-1/4 predBufHi[2][1/3]
        h265_InterpLumaPack(predBufHi2q1, memPitch, predBuf2q1, memPitch, w, h+2, bitDepth);                                                      // predBuf[2][1/3]
        tmpSubMem[1] = predBuf2q1+(hpely - 1 >> 2) * memPitch;
    }

    costFast = costFunc(src, pitchSrc, tmpSubMem[1], memPitch, w, h) >> costShift;                                                               // sad[0][1/3]
    CompareFastCosts(costFast, bestFastCost, 1, 0, bestQuarterDia, bestQuarterDiaMvCost, bestmvQuarter);

    // interpolate vertical qpels
    if (dx == 0) // best halfpel is intpel or ver-halfpel [0,1/3] (0,0) (w,h)
    {
        Ipp16s *predBufHi0q5;
        PixType *predBuf0q5;
        tmv = mvBest;
        tmv.mvy +=1;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf0q5, predBufHi0q5);
        InterpVer(refY + (hpely - 3 >> 2) * pitchRef, pitchRef, predBufHi0q5, memPitch, dy + 1, w, h, bitDepth - 8, 0, bitDepth);                   // hpx+0 hpy+1/4 predBufHi[2][1/3]
        h265_InterpLumaPack(predBufHi0q5, memPitch, predBuf0q5, memPitch, w, h, bitDepth);                                                          // predBuf[0][1/3]
        tmpSubMem[5] = predBuf0q5;
    }
    else // best halfpel is hor-halfpel or diag-halfpel
    {
        Ipp16s *predBufHi2q5;
        PixType *predBuf2q5;
        tmv.mvx = mvBest.mvx;
        tmv.mvy = mv->mvy + dy + 1;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf2q5, predBufHi2q5);
        //[2,1/3] (0,0) (w,h+2)
        InterpVer(tmpHor2 + (hpelx >> 2), memPitch, predBufHi2q5, memPitch, dy + 1, w, h+2, 6, 0, bitDepth);                                        // hpx+0 hpy+1/4 predBufHi[2][1/3]
        h265_InterpLumaPack(predBufHi2q5, memPitch, predBuf2q5, memPitch, w, h+2, bitDepth);                                                          // predBuf[2][1/3]
        tmpSubMem[5] = predBuf2q5+ (hpely + 1 >> 2) * memPitch;
    }
    
    costFast = costFunc(src, pitchSrc, tmpSubMem[5], memPitch, w, h) >> costShift;
    CompareFastCosts(costFast, bestFastCost, 5, 0, bestQuarterDia, bestQuarterDiaMvCost, bestmvQuarter);

    // intermediate horizontal quater-pels (left of best half-pel) [1/3,0] (0,-4) (w,h+8)
    Ipp16s *predBufHiq07;
    PixType *predBufq07;
    tmv.mvx = mvBest.mvx - 1;
    tmv.mvy = mv->mvy;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufq07, predBufHiq07);
    Ipp16s *tmpHor1 = predBufHiq07 + 3 * memPitch;
    InterpHor(refY - 4 * pitchRef + (hpelx - 5 >> 2), pitchRef, predBufHiq07, memPitch, 3 - dx, w, h + 10, bitDepth - 8, 0, bitDepth);  // hpx-1/4 hpy+0 (intermediate) predBufHi[1/3][0]
    // Lo [1/3,0] (0,-4) (w, h+8)
    h265_InterpLumaPack(predBufHiq07, memPitch, predBufq07, memPitch, w, h + 8, bitDepth);                                               // save to predBuf[1/3][0]

    // interpolate horizontal quater-pels (left of best half-pel)
    if (dy == 0) // best halfpel is intpel or hor-halfpel [1/3, 0] (0,0) (w,h)
    {
        tmpSubMem[7] = predBufq07+4*memPitch;
    }
    else // best halfpel is vert-halfpel or diag-halfpel [1/3,2] (0,0) (w,h+2)
    {
        Ipp16s *predBufHiq27;
        PixType *predBufq27;
        tmv.mvx = mvBest.mvx - 1;
        tmv.mvy = mv->mvy + dy;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufq27, predBufHiq27);
        InterpVer(tmpHor1, memPitch, predBufHiq27, memPitch, dy, w, h+2, 6, 0, bitDepth);                                           // hpx-1/4 hpy+0 save to predBufHi[1/3][2]
        h265_InterpLumaPack(predBufHiq27, memPitch, predBufq27, memPitch, w, h+2, bitDepth);                                        // predBuf[1/3][2]
        tmpSubMem[7] = predBufq27+ (hpely >> 2) * memPitch;
    }

    costFast = costFunc(src, pitchSrc, tmpSubMem[7], memPitch, w, h) >> costShift;
    CompareFastCosts(costFast, bestFastCost, 7, 0, bestQuarterDia, bestQuarterDiaMvCost, bestmvQuarter);

    Ipp16s *predBufHiq03;
    PixType *predBufq03;
    // intermediate horizontal quater-pels (right of best half-pel) [1/3,0] (0,-4) (w,h+8)
    tmv.mvx = mvBest.mvx + 1;
    tmv.mvy = mv->mvy;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufq03, predBufHiq03);
    Ipp16s *tmpHor3 = predBufHiq03 + 3 * memPitch;
    InterpHor(refY - 4 * pitchRef + (hpelx - 3 >> 2), pitchRef, predBufHiq03, memPitch, dx + 1, w, h + 10, bitDepth - 8, 0, bitDepth);  // hpx+1/4 hpy+0 (intermediate) predBufHi[1/3][0]
    // Lo (1/3,0) (0,-4) (w, h+8)
    h265_InterpLumaPack(predBufHiq03, memPitch, predBufq03, memPitch, w, h + 8, bitDepth);                                               // save to predBuf[1/3][0]

    // interpolate horizontal quater-pels (right of best half-pel)
    if (dy == 0) // best halfpel is intpel or hor-halfpel [1/3, 0] (0,0) (w,h)
    {
        tmpSubMem[3] = predBufq03+4*memPitch;
    }
    else // best halfpel is vert-halfpel or diag-halfpel [1/3,2] (0,0) (w,h+2)
    {
        Ipp16s *predBufHiq23;
        PixType *predBufq23;
        tmv.mvx = mvBest.mvx + 1;
        tmv.mvy = mv->mvy + dy;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufq23, predBufHiq23);
        InterpVer(tmpHor3, memPitch, predBufHiq23, memPitch, dy, w, h+2, 6, 0, bitDepth);                                         // hpx+1/4 hpy+0 predBufHi[1/3][2]
        h265_InterpLumaPack(predBufHiq23, memPitch, predBufq23, memPitch, w, h+2, bitDepth);                                           // predBuf[1/3][2]
        tmpSubMem[3] = predBufq23+ (hpely >> 2) * memPitch;
    }

    costFast = costFunc(src, pitchSrc, tmpSubMem[3], memPitch, w, h) >> costShift;
    CompareFastCosts(costFast, bestFastCost, 3, 0, bestQuarterDia, bestQuarterDiaMvCost, bestmvQuarter);

    MemHadGetBuf(size, bestmvQuarter.mvx&3, bestmvQuarter.mvy&3, refIdxMem, &bestmvQuarter, satd8);
    bestFastCost = tuHadSave(src, pitchSrc, tmpSubMem[bestQuarterDia], memPitch, w, h, satd8, satdPitch) >> costShift;                                       // Had
    bestFastCost += bestQuarterDiaMvCost;

    if (costBest > bestFastCost) {
        mvCenter = mvBest;
        costBest = bestFastCost;
        mvCostBest = bestQuarterDiaMvCost;
        mvBest = bestmvQuarter;

        bestFastCost = INT_MAX; 
        Ipp32s bestQuarter = 0;

        if(bestQuarterDia==1 || bestQuarterDia==7)
        {
            Ipp16s *predBufHiqq0;
            PixType *predBufqq0;
            tmv.mvx = mvCenter.mvx - 1;
            tmv.mvy = mv->mvy + 3 - dy;
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufqq0, predBufHiqq0);
            // interpolate 2 diagonal quater-pels (left-top and left-bottom of best half-pel) [1/3,1/3] (0,0) (w,h+2)
            InterpVer(tmpHor1, memPitch, predBufHiqq0, memPitch, 3 - dy, w, h+2, 6, 0, bitDepth);                                        // hpx-1/4 hpy-1/4  predBufHi[1/3][1/3]
            h265_InterpLumaPack(predBufHiqq0, memPitch, predBufqq0, memPitch, w, h+2, bitDepth);                                         // predBuf[1/3][1/3]

            tmpSubMem[0] = predBufqq0+ (hpely - 1 >> 2) * memPitch;
            
            costFast = costFunc(src, pitchSrc, tmpSubMem[0], memPitch, w, h) >> costShift;                // sad[1/3][1/3]
            CompareFastCosts(costFast, bestFastCost, 0, 0, bestQuarter, bestQuarterDiaMvCost, bestmvQuarter);
        }

        if(bestQuarterDia==7 || bestQuarterDia==5)
        {
            Ipp16s *predBufHiqq6;
            PixType *predBufqq6;
            tmv.mvx = mvCenter.mvx - 1;
            tmv.mvy = mv->mvy + dy + 1;
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufqq6, predBufHiqq6);
            InterpVer(tmpHor1, memPitch, predBufHiqq6, memPitch, dy + 1, w, h+2, 6, 0, bitDepth);                                          // hpx-1/4 hpy+1/4  predBufHi[1/3][1/3]
            h265_InterpLumaPack(predBufHiqq6, memPitch, predBufqq6, memPitch, w, h+2, bitDepth);                                           // predBuf[1/3][1/3]

            tmpSubMem[6] = predBufqq6+ (hpely + 1 >> 2) * memPitch;
            costFast = costFunc(src, pitchSrc, tmpSubMem[6], memPitch, w, h) >> costShift;       // sad[1/3][1/3]
            CompareFastCosts(costFast, bestFastCost, 6, 0, bestQuarter, bestQuarterDiaMvCost, bestmvQuarter);
        }

        if(bestQuarterDia==1 || bestQuarterDia==3)
        {
            Ipp16s *predBufHiqq2;
            PixType *predBufqq2;
            // interpolate 2 diagonal quater-pels (right-top and right-bottom of best half-pel) [1/3,1/3] (0,0) (w,h+2)
            tmv.mvx = mvCenter.mvx + 1;
            tmv.mvy = mv->mvy + 3 - dy;
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufqq2, predBufHiqq2);
            InterpVer(tmpHor3, memPitch, predBufHiqq2, memPitch, 3 - dy, w, h+2, 6, 0, bitDepth);                                        // hpx+1/4 hpy-1/4 predBufHi[1/3][1/3]
            h265_InterpLumaPack(predBufHiqq2, memPitch, predBufqq2, memPitch, w, h+2, bitDepth);                                         // predBuf[1/3][1/3]

            tmpSubMem[2] = predBufqq2+ (hpely - 1 >> 2) * memPitch;
            costFast = costFunc(src, pitchSrc, tmpSubMem[2], memPitch, w, h) >> costShift;                                              // sad[1/3][1/3]
            CompareFastCosts(costFast, bestFastCost, 2, 0, bestQuarter, bestQuarterDiaMvCost, bestmvQuarter);
        }

        if(bestQuarterDia==3 || bestQuarterDia==5)
        {
            Ipp16s *predBufHiqq4;
            PixType *predBufqq4;
            tmv.mvx = mvCenter.mvx + 1;
            tmv.mvy = mv->mvy + dy + 1;
            MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufqq4, predBufHiqq4);
            InterpVer(tmpHor3, memPitch, predBufHiqq4, memPitch, dy + 1, w, h+2, 6, 0, bitDepth);                                        // hpx+1/4 hpy+1/4 predBufHi[1/3][1/3]
            h265_InterpLumaPack(predBufHiqq4, memPitch, predBufqq4, memPitch, w, h+2, bitDepth);                                         // predBuf[1/3][1/3]

            tmpSubMem[4] = predBufqq4+ (hpely + 1 >> 2) * memPitch;
            costFast = costFunc(src, pitchSrc, tmpSubMem[4], memPitch, w, h) >> costShift;                                               // sad[1/3][1/3]
            CompareFastCosts(costFast, bestFastCost, 4, 0, bestQuarter, bestQuarterDiaMvCost, bestmvQuarter);
        }
        MemHadGetBuf(size, bestmvQuarter.mvx&3, bestmvQuarter.mvy&3, refIdxMem, &bestmvQuarter, satd8);
        bestFastCost = tuHadSave(src, pitchSrc, tmpSubMem[bestQuarter], memPitch, w, h, satd8, satdPitch) >> costShift;                  // had
        bestFastCost += bestQuarterDiaMvCost;

        if (costBest > bestFastCost) {
            costBest = bestFastCost;
            mvCostBest = bestQuarterDiaMvCost;
            mvBest = bestmvQuarter;
        }
    }
    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;
}

template <typename PixType>
void H265CU<PixType>::MemSubPelBatchedBox(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                                         const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s refIdxMem, Ipp32s size)
{
    VM_ASSERT(m_par->patternSubPel == SUBPEL_BOX);
    using MFX_HEVC_PP::h265_InterpLumaPack;

    Ipp32s w = meInfo->width;
    Ipp32s h = meInfo->height;
    Ipp32s bitDepth = m_par->bitDepthLuma;
    Ipp32s costShift = m_par->bitDepthLumaShift;
    Ipp32s shift = 20 - bitDepth;
    Ipp32s offset = 1 << (19 - bitDepth);
    Ipp32s useHadamard = (m_par->hadamardMe >= 2);

    Ipp32s isFast = m_par->FastInterp;

    Ipp32s pitchSrc = m_pitchSrcLuma;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * pitchSrc;

    Ipp32s pitchRef = ref->pitch_luma_pix;
    const PixType *refY = (const PixType *)ref->y;
    refY += (Ipp32s)(m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * pitchRef);

    Ipp32s costs[8];

    Ipp32s memPitch = (MAX_CU_SIZE>>(3-size))+8;
    Ipp16s *predBufHi20;
    PixType *predBuf20;
    H265MV tmv;
    tmv =*mv;
    tmv.mvx +=2;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf20, predBufHi20);
    // intermediate halfpels Hi[2,0]: change (-1, -4) (w+8, h+8) to (-4,-4) (w+8, h+8)
    InterpHor(refY - 4 - 4 * pitchRef, pitchRef, predBufHi20, memPitch, 2, (w + 8) & ~7, h + 10, bitDepth - 8, 0, bitDepth); // (intermediate, save to predBufHi[2][0])
    Ipp16s *tmpHor2 = predBufHi20 + 3 * memPitch + 3;
    // interpolate horizontal halfpels: Lo (2,0) (-4, -4) (w+8, h+8)
    h265_InterpLumaPack(predBufHi20, memPitch, predBuf20, memPitch, w + 8, h + 8, bitDepth);                            // save to predBuf[2][0]

    PixType *tmpSub2 = predBuf20 + 4 * memPitch + 3;
    Ipp32s *satd8 = NULL;
    Ipp32s satdPitch = (MAX_CU_SIZE>>(3-size));
    tmv = *mv;
    tmv.mvx +=2;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[3] = tuHadSave(src, pitchSrc, tmpSub2 + 1, memPitch, w, h, satd8, satdPitch) >> costShift;                    // save to Had[2][0]

    tmv = *mv;
    tmv.mvx -=2;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[7] = tuHadSave(src, pitchSrc, tmpSub2,     memPitch, w, h, satd8, satdPitch) >> costShift;                     // save to Had[2][0]

    // interpolate diagonal halfpels Hi (2,2) (-4,-1) (w+8, h+2)
    Ipp16s *tmpHor22 = predBufHi20 + 3 * memPitch;
    Ipp16s *predBufHi22;
    PixType *predBuf22;
    tmv = *mv;
    tmv.mvx +=2;
    tmv.mvy +=2;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf22, predBufHi22);
    InterpVer(tmpHor22, memPitch, predBufHi22, memPitch, 2, (w + 8) & ~7, h + 2, 6, 0, bitDepth);                         // save to predBufHi[2][2]
    // Lo (2,2) (-4,-1) (w+8, h+2)
    h265_InterpLumaPack(predBufHi22, memPitch, predBuf22, memPitch, w + 8, h + 2, bitDepth);                                // save to predBuf[2][2]

    tmpSub2 = predBuf22 + 3;
    tmv = *mv;
    tmv.mvx -=2;
    tmv.mvy -=2;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[0] = tuHadSave(src, pitchSrc, tmpSub2,                 memPitch, w, h, satd8, satdPitch) >> costShift;       // save to Had[2][2] ++

    tmv = *mv;
    tmv.mvx +=2;
    tmv.mvy -=2;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[2] = tuHadSave(src, pitchSrc, tmpSub2 + 1,             memPitch, w, h, satd8, satdPitch) >> costShift;       // save to Had[2][2] ++

    tmv = *mv;
    tmv.mvx +=2;
    tmv.mvy +=2;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[4] = tuHadSave(src, pitchSrc, tmpSub2 + 1 + memPitch,  memPitch, w, h, satd8, satdPitch) >> costShift;       // save to Had[2][2] ++

    tmv = *mv;
    tmv.mvx -=2;
    tmv.mvy +=2;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[6] = tuHadSave(src, pitchSrc, tmpSub2     + memPitch,  memPitch, w, h, satd8, satdPitch) >> costShift;       // save to Had[2][2] ++

    // interpolate vertical halfpels
    Ipp16s *predBufHi02;
    PixType *predBuf02;
    tmv = *mv;
    tmv.mvy +=2;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf02, predBufHi02);
    // Hi [0,2] (0,-1) (w, h+2)
    InterpVer(refY - pitchRef, pitchRef, predBufHi02, memPitch, 2, w, h + 2, bitDepth - 8, 0, bitDepth);                    // save to predBufHi[0][2]
    // Lo [0,2] (0,-1) (w, h+2)
    h265_InterpLumaPack(predBufHi02, memPitch, predBuf02, memPitch, w, h + 2, bitDepth);                                    // save to predBuf[0][2]
    
    tmv = *mv;
    tmv.mvy -=2;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[1] = tuHadSave(src, pitchSrc, predBuf02,             memPitch, w, h, satd8, satdPitch) >> costShift;               // Save to Had[0][2]++

    tmv = *mv;
    tmv.mvy +=2;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[5] = tuHadSave(src, pitchSrc, predBuf02 + memPitch,  memPitch, w, h, satd8, satdPitch) >> costShift;              // Save to Had[0][2]++

    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;
    if (m_par->hadamardMe >= 2) { // always
        // when satd is for subpel only need to recalculate cost for intpel motion vector
        MemHadGetBuf(size, mv->mvx&3, mv->mvy&3, refIdxMem, mv, satd8);
        costBest = tuHadSave(src, pitchSrc, refY, pitchRef, w, h, satd8, satdPitch) >> costShift;                           // Save to Had[0][0]
        costBest += mvCostBest;
    }

    AddMvCost(predInfo, 1, costs, &mvBest, &costBest, &mvCostBest, m_par->patternSubPel);

    Ipp32s hpelx = mvBest.mvx - mv->mvx + 4; // can be 2, 4 or 6
    Ipp32s hpely = mvBest.mvy - mv->mvy + 4; // can be 2, 4 or 6
    Ipp32s dx = hpelx & 3; // can be 0 or 2
    Ipp32s dy = hpely & 3; // can be 0 or 2

    // interpolate vertical quater-pels 
    if (dx == 0) // best halfpel is intpel or ver-halfpel [0,1/3] (0,0)(w,h)
    {
        Ipp16s *predBufHi0q;
        PixType *predBuf0q;
        tmv = mvBest;
        tmv.mvy -=1;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf0q, predBufHi0q);
        InterpVer(refY + (hpely - 5 >> 2) * pitchRef, pitchRef, predBufHi0q, memPitch, 3 - dy, w, h, bitDepth -8, 0, bitDepth);                   // hpx+0 hpy-1/4 predBufHi[0][1/3]
        h265_InterpLumaPack(predBufHi0q, memPitch, predBuf0q, memPitch, w, h, bitDepth);                                                          // predBuf[0][1/3]

        MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
        costs[1] = tuHadSave(src, pitchSrc, predBuf0q, memPitch, w, h, satd8, satdPitch) >> costShift;                                            // Had[0][1/3]
    }
    else // best halfpel is hor-halfpel or diag-halfpel
    {
        Ipp16s *predBufHi2q;
        PixType *predBuf2q;
        tmv.mvx = mvBest.mvx;
        tmv.mvy = mv->mvy + 3 - dy;

        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf2q, predBufHi2q);
        //[2,1/3] (0,0) (w,h+2)
        InterpVer(tmpHor2 + (hpelx >> 2), memPitch, predBufHi2q, memPitch, 3 - dy, w, h+2, 6, 0, bitDepth);                                     // hpx+0 hpy-1/4 predBufHi[2][1/3]
        h265_InterpLumaPack(predBufHi2q, memPitch, predBuf2q, memPitch, w, h+2, bitDepth);                                                      // predBuf[2][1/3]
        
        tmv = mvBest;
        tmv.mvy -=1;
        MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
        costs[1] = tuHadSave(src, pitchSrc, predBuf2q+(hpely - 1 >> 2) * memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;              // Had[2][1/3]
    }

    // interpolate vertical qpels
    if (dx == 0) // best halfpel is intpel or ver-halfpel [0,1/3] (0,0) (w,h)
    {
        Ipp16s *predBufHi0q;
        PixType *predBuf0q;
        tmv = mvBest;
        tmv.mvy +=1;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf0q, predBufHi0q);
        InterpVer(refY + (hpely - 3 >> 2) * pitchRef, pitchRef, predBufHi0q, memPitch, dy + 1, w, h, bitDepth -8, 0, bitDepth);                   // hpx+0 hpy+1/4 predBufHi[2][1/3]
        h265_InterpLumaPack(predBufHi0q, memPitch, predBuf0q, memPitch, w, h, bitDepth);                                                          // predBuf[0][1/3]

        MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
        costs[5] = tuHadSave(src, pitchSrc, predBuf0q, memPitch, w, h, satd8, satdPitch) >> costShift;                                            // Had[0/2][1/3]
    }
    else // best halfpel is hor-halfpel or diag-halfpel
    {
        Ipp16s *predBufHi2q;
        PixType *predBuf2q;
        tmv.mvx = mvBest.mvx;
        tmv.mvy = mv->mvy + dy + 1;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBuf2q, predBufHi2q);
        //[2,1/3] (0,0) (w,h+2)
        InterpVer(tmpHor2 + (hpelx >> 2), memPitch, predBufHi2q, memPitch, dy + 1, w, h+2, 6, 0, bitDepth);                                        // hpx+0 hpy+1/4 predBufHi[2][1/3]
        h265_InterpLumaPack(predBufHi2q, memPitch, predBuf2q, memPitch, w, h+2, bitDepth);                                                          // predBuf[2][1/3]
        
        tmv = mvBest;
        tmv.mvy +=1;
        MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
        costs[5] = tuHadSave(src, pitchSrc, predBuf2q+ (hpely + 1 >> 2) * memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;                // Had[0/2][1/3]
    }
    

    // intermediate horizontal quater-pels (left of best half-pel) [1/3,0] (0,-4) (w,h+8)
    Ipp16s *predBufHiq0;
    PixType *predBufq0;
    tmv.mvx = mvBest.mvx - 1;
    tmv.mvy = mv->mvy;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufq0, predBufHiq0);
    Ipp16s *tmpHor1 = predBufHiq0 + 3 * memPitch;
    InterpHor(refY - 4 * pitchRef + (hpelx - 5 >> 2), pitchRef, predBufHiq0, memPitch, 3 - dx, w, h + 10, bitDepth - 8, 0, bitDepth);  // hpx-1/4 hpy+0 (intermediate) predBufHi[1/3][0]
    // Lo [1/3,0] (0,-4) (w, h+8)
    h265_InterpLumaPack(predBufHiq0, memPitch, predBufq0, memPitch, w, h + 8, bitDepth);                                               // save to predBuf[1/3][0]

    // interpolate horizontal quater-pels (left of best half-pel)
    if (dy == 0) // best halfpel is intpel or hor-halfpel [1/3, 0] (0,0) (w,h)
    {
        //h265_InterpLumaPack(tmpHor1 + pitchTmp1, pitchTmp1, subpel, pitchHpel, w, h, bitDepth);                                 // hpx-1/4 hpy+0, already saved to predBuf[1/3][0]
        tmv = mvBest;
        tmv.mvx -=1;
        MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
        costs[7] = tuHadSave(src, pitchSrc, predBufq0+4*memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;                 // Had[1/3][0]
    }
    else // best halfpel is vert-halfpel or diag-halfpel [1/3,2] (0,0) (w,h+2)
    {
        Ipp16s *predBufHiq2;
        PixType *predBufq2;
        tmv.mvx = mvBest.mvx - 1;
        tmv.mvy = mv->mvy + dy;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufq2, predBufHiq2);
        InterpVer(tmpHor1, memPitch, predBufHiq2, memPitch, dy, w, h+2, 6, 0, bitDepth);                                         // hpx-1/4 hpy+0 save to predBufHi[1/3][2]
        h265_InterpLumaPack(predBufHiq2, memPitch, predBufq2, memPitch, w, h+2, bitDepth);                                           // predBuf[1/3][2]
        
        tmv = mvBest;
        tmv.mvx -=1;
        MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
        costs[7] = tuHadSave(src, pitchSrc, predBufq2+ (hpely >> 2) * memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;     // Had[1/3][2]
    }
    
    Ipp16s *predBufHiqq;
    PixType *predBufqq;
    tmv.mvx = mvBest.mvx - 1;
    tmv.mvy = mv->mvy + 3 - dy;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufqq, predBufHiqq);
    // interpolate 2 diagonal quater-pels (left-top and left-bottom of best half-pel) [1/3,1/3] (0,0) (w,h+2)
    InterpVer(tmpHor1, memPitch, predBufHiqq, memPitch, 3 - dy, w, h+2, 6, 0, bitDepth);                                        // hpx-1/4 hpy-1/4  predBufHi[1/3][1/3]
    h265_InterpLumaPack(predBufHiqq, memPitch, predBufqq, memPitch, w, h+2, bitDepth);                                              // predBuf[1/3][1/3]

    tmv = mvBest;
    tmv.mvx -=1;
    tmv.mvy -=1;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[0] = tuHadSave(src, pitchSrc, predBufqq+ (hpely - 1 >> 2) * memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;       // had[1/3][1/3]

    tmv.mvx = mvBest.mvx - 1;
    tmv.mvy = mv->mvy + dy + 1;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufqq, predBufHiqq);
    InterpVer(tmpHor1, memPitch, predBufHiqq, memPitch, dy + 1, w, h+2, 6, 0, bitDepth);                                          // hpx-1/4 hpy+1/4  predBufHi[1/3][1/3]
    h265_InterpLumaPack(predBufHiqq, memPitch, predBufqq, memPitch, w, h+2, bitDepth);                                                // predBuf[1/3][1/3]

    tmv = mvBest;
    tmv.mvx -=1;
    tmv.mvy +=1;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[6] = tuHadSave(src, pitchSrc, predBufqq+ (hpely + 1 >> 2) * memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;      // had[1/3][1/3]

    // intermediate horizontal quater-pels (right of best half-pel) [1/3,0] (0,-4) (w,h+8)
    tmv.mvx = mvBest.mvx + 1;
    tmv.mvy = mv->mvy;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufq0, predBufHiq0);
    Ipp16s *tmpHor3 = predBufHiq0 + 3 * memPitch;
    InterpHor(refY - 4 * pitchRef + (hpelx - 3 >> 2), pitchRef, predBufHiq0, memPitch, dx + 1, w, h + 10, bitDepth - 8, 0, bitDepth);  // hpx+1/4 hpy+0 (intermediate) predBufHi[1/3][0]
    // Lo (1/3,0) (0,-4) (w, h+8)
    h265_InterpLumaPack(predBufHiq0, memPitch, predBufq0, memPitch, w, h + 8, bitDepth);                                               // save to predBuf[1/3][0]

    // interpolate horizontal quater-pels (right of best half-pel)
    if (dy == 0) // best halfpel is intpel or hor-halfpel [1/3, 0] (0,0) (w,h)
    {
        //h265_InterpLumaPack(tmpHor3 + pitchTmp3, pitchTmp3, subpel, pitchHpel, w, h, bitDepth);                                  // hpx+1/4 hpy+0 already saved to predBuf[1/3][0]
        tmv = mvBest;
        tmv.mvx +=1;
        MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
        costs[3] = tuHadSave(src, pitchSrc, predBufq0+4*memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;                  // Had[1/3][0]
    }
    else // best halfpel is vert-halfpel or diag-halfpel [1/3,2] (0,0) (w,h+2)
    {
        Ipp16s *predBufHiq2;
        PixType *predBufq2;
        tmv.mvx = mvBest.mvx + 1;
        tmv.mvy = mv->mvy + dy;
        MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufq2, predBufHiq2);
        InterpVer(tmpHor3, memPitch, predBufHiq2, memPitch, dy, w, h+2, 6, 0, bitDepth);                                         // hpx+1/4 hpy+0 predBufHi[1/3][2]
        h265_InterpLumaPack(predBufHiq2, memPitch, predBufq2, memPitch, w, h+2, bitDepth);                                           // predBuf[1/3][2]

        tmv = mvBest;
        tmv.mvx +=1;
        MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
        costs[3] = tuHadSave(src, pitchSrc, predBufq2+ (hpely >> 2) * memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;      // Had[1/3][1/3]
    }

    // interpolate 2 diagonal quater-pels (right-top and right-bottom of best half-pel) [1/3,1/3] (0,0) (w,h+2)
    tmv.mvx = mvBest.mvx + 1;
    tmv.mvy = mv->mvy + 3 - dy;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufqq, predBufHiqq);
    InterpVer(tmpHor3, memPitch, predBufHiqq, memPitch, 3 - dy, w, h+2, 6, 0, bitDepth);                                        // hpx+1/4 hpy-1/4 predBufHi[1/3][1/3]
    h265_InterpLumaPack(predBufHiqq, memPitch, predBufqq, memPitch, w, h+2, bitDepth);                                              // predBuf[1/3][1/3]

    tmv = mvBest;
    tmv.mvx +=1;
    tmv.mvy -=1;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[2] = tuHadSave(src, pitchSrc, predBufqq+ (hpely - 1 >> 2) * memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;    // had[1/3][1/3]

    tmv.mvx = mvBest.mvx + 1;
    tmv.mvy = mv->mvy + dy + 1;
    MemSubpelGetBufSetMv(size, &tmv, refIdxMem, predBufqq, predBufHiqq);
    InterpVer(tmpHor3, memPitch, predBufHiqq, memPitch, dy + 1, w, h+2, 6, 0, bitDepth);                                        // hpx+1/4 hpy+1/4 predBufHi[1/3][1/3]
    h265_InterpLumaPack(predBufHiqq, memPitch, predBufqq, memPitch, w, h+2, bitDepth);                                              // predBuf[1/3][1/3]

    tmv = mvBest;
    tmv.mvx +=1;
    tmv.mvy +=1;
    MemHadGetBuf(size, tmv.mvx&3, tmv.mvy&3, refIdxMem, &tmv, satd8);
    costs[4] = tuHadSave(src, pitchSrc, predBufqq+ (hpely + 1 >> 2) * memPitch, memPitch, w, h, satd8, satdPitch) >> costShift;    // had[1/3][1/3]

    AddMvCost(predInfo, 0, costs, &mvBest, &costBest, &mvCostBest, m_par->patternSubPel);

    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;
}
#endif

template <typename PixType>
void H265CU<PixType>::MeSubPelBatchedBox(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                                         const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
{
    VM_ASSERT(m_par->patternSubPel == SUBPEL_BOX);
    using MFX_HEVC_PP::h265_InterpLumaPack;

    Ipp32s w = meInfo->width;
    Ipp32s h = meInfo->height;
    Ipp32s bitDepth = m_par->bitDepthLuma;
    Ipp32s costShift = m_par->bitDepthLumaShift;
    Ipp32s shift = 20 - bitDepth;
    Ipp32s offset = 1 << (19 - bitDepth);
    Ipp32s useHadamard = (m_par->hadamardMe >= 2);

    Ipp32s pitchSrc = m_pitchSrcLuma;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * pitchSrc;

    Ipp32s pitchRef = ref->pitch_luma_pix;
    const PixType *refY = (const PixType *)ref->y;
    refY += (Ipp32s)(m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * pitchRef);

    Ipp32s (*costFunc)(const PixType*, Ipp32s, const PixType*, Ipp32s, Ipp32s, Ipp32s);
    costFunc = (useHadamard ? tuHad_Kernel<PixType> : h265_SAD_MxN_general<PixType>);
    Ipp32s costs[8];

    Ipp32s pitchTmp2 = (w + 1 + 15) & ~15;
    Ipp32s pitchHpel = (w + 1 + 15) & ~15;
    ALIGN_DECL(32) Ipp16s tmpPels[(MAX_CU_SIZE + 16) * (MAX_CU_SIZE + 8)];
    ALIGN_DECL(32) PixType subpel[(MAX_CU_SIZE + 16) * (MAX_CU_SIZE + 2)];

    // intermediate halfpels
    InterpHor(refY - 1 - 4 * pitchRef, pitchRef, tmpPels, pitchTmp2, 2, (w + 8) & ~7, h + 8, bitDepth - 8, 0, bitDepth); // (intermediate)
    Ipp16s *tmpHor2 = tmpPels + 3 * pitchTmp2;

    // interpolate horizontal halfpels
    h265_InterpLumaPack(tmpHor2 + pitchTmp2, pitchTmp2, subpel, pitchHpel, w + 1, h, bitDepth);
    costs[3] = costFunc(src, pitchSrc, subpel + 1, pitchHpel, w, h) >> costShift;
    costs[7] = costFunc(src, pitchSrc, subpel,     pitchHpel, w, h) >> costShift;

    // interpolate diagonal halfpels
    InterpVer(tmpHor2, pitchTmp2, subpel, pitchHpel, 2, (w + 8) & ~7, h + 2, shift, offset, bitDepth);
    costs[0] = costFunc(src, pitchSrc, subpel,                 pitchHpel, w, h) >> costShift;
    costs[2] = costFunc(src, pitchSrc, subpel + 1,             pitchHpel, w, h) >> costShift;
    costs[4] = costFunc(src, pitchSrc, subpel + 1 + pitchHpel, pitchHpel, w, h) >> costShift;
    costs[6] = costFunc(src, pitchSrc, subpel     + pitchHpel, pitchHpel, w, h) >> costShift;

    // interpolate vertical halfpels
    pitchHpel = (w + 15) & ~15;
    InterpVer(refY - pitchRef, pitchRef, subpel, pitchHpel, 2, w, h + 2, 6, 32, bitDepth);
    costs[1] = costFunc(src, pitchSrc, subpel,             pitchHpel, w, h) >> costShift;
    costs[5] = costFunc(src, pitchSrc, subpel + pitchHpel, pitchHpel, w, h) >> costShift;

    H265MV mvBest = *mv;
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;
    if (m_par->hadamardMe == 2) {
        // when satd is for subpel only need to recalculate cost for intpel motion vector
        costBest = tuHad(src, pitchSrc, refY, pitchRef, w, h) >> costShift;
        costBest += mvCostBest;
    }

    AddMvCost(predInfo, 1, costs, &mvBest, &costBest, &mvCostBest, m_par->patternSubPel);

    Ipp32s hpelx = mvBest.mvx - mv->mvx + 4; // can be 2, 4 or 6
    Ipp32s hpely = mvBest.mvy - mv->mvy + 4; // can be 2, 4 or 6
    Ipp32s dx = hpelx & 3; // can be 0 or 2
    Ipp32s dy = hpely & 3; // can be 0 or 2

    Ipp32s pitchQpel = (w + 15) & ~15;

    // interpolate vertical quater-pels
    if (dx == 0) // best halfpel is intpel or ver-halfpel
        InterpVer(refY + (hpely - 5 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, 3 - dy, w, h, 6, 32, bitDepth);                             // hpx+0 hpy-1/4
    else // best halfpel is hor-halfpel or diag-halfpel
        InterpVer(tmpHor2 + (hpelx >> 2) + (hpely - 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth); // hpx+0 hpy-1/4
    costs[1] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    // interpolate vertical qpels
    if (dx == 0) // best halfpel is intpel or ver-halfpel
        InterpVer(refY + (hpely - 3 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, dy + 1, w, h, 6, 32, bitDepth);                             // hpx+0 hpy+1/4
    else // best halfpel is hor-halfpel or diag-halfpel
        InterpVer(tmpHor2 + (hpelx >> 2) + (hpely + 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth); // hpx+0 hpy+1/4
    costs[5] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    // intermediate horizontal quater-pels (left of best half-pel)
    Ipp32s pitchTmp1 = (w + 15) & ~15;
    Ipp16s *tmpHor1 = tmpPels + 3 * pitchTmp1;
    InterpHor(refY - 4 * pitchRef + (hpelx - 5 >> 2), pitchRef, tmpPels, pitchTmp1, 3 - dx, w, h + 8, bitDepth - 8, 0, bitDepth);  // hpx-1/4 hpy+0 (intermediate)

    // interpolate horizontal quater-pels (left of best half-pel)
    if (dy == 0) // best halfpel is intpel or hor-halfpel
        h265_InterpLumaPack(tmpHor1 + pitchTmp1, pitchTmp1, subpel, pitchHpel, w, h, bitDepth); // hpx-1/4 hpy+0
    else // best halfpel is vert-halfpel or diag-halfpel
        InterpVer(tmpHor1 + (hpely >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth); // hpx-1/4 hpy+0
    costs[7] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    // interpolate 2 diagonal quater-pels (left-top and left-bottom of best half-pel)
    InterpVer(tmpHor1 + (hpely - 1 >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth); // hpx-1/4 hpy-1/4
    costs[0] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;
    InterpVer(tmpHor1 + (hpely + 1 >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth); // hpx-1/4 hpy+1/4
    costs[6] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    // intermediate horizontal quater-pels (right of best half-pel)
    Ipp32s pitchTmp3 = (w + 15) & ~15;
    Ipp16s *tmpHor3 = tmpPels + 3 * pitchTmp3;
    InterpHor(refY - 4 * pitchRef + (hpelx - 3 >> 2), pitchRef, tmpPels, pitchTmp3, dx + 1, w, h + 8, bitDepth - 8, 0, bitDepth);  // hpx+1/4 hpy+0 (intermediate)

    // interpolate horizontal quater-pels (right of best half-pel)
    if (dy == 0) // best halfpel is intpel or hor-halfpel
        h265_InterpLumaPack(tmpHor3 + pitchTmp3, pitchTmp3, subpel, pitchHpel, w, h, bitDepth); // hpx+1/4 hpy+0
    else // best halfpel is vert-halfpel or diag-halfpel
        InterpVer(tmpHor3 + (hpely >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth); // hpx+1/4 hpy+0
    costs[3] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    // interpolate 2 diagonal quater-pels (right-top and right-bottom of best half-pel)
    InterpVer(tmpHor3 + (hpely - 1 >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth); // hpx+1/4 hpy-1/4
    costs[2] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;
    InterpVer(tmpHor3 + (hpely + 1 >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth); // hpx+1/4 hpy+1/4
    costs[4] = costFunc(src, pitchSrc, subpel, pitchQpel, w, h) >> costShift;

    AddMvCost(predInfo, 0, costs, &mvBest, &costBest, &mvCostBest, m_par->patternSubPel);

    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;
}
#ifdef AMT_FAST_SUBPEL_SEED
template <typename PixType>
bool H265CU<PixType>::MeSubPelSeed(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                               const FrameData *ref, H265MV &mvBest, Ipp32s &costBest, Ipp32s &mvCostBest, Ipp32s refIdxMem, 
                               H265MV &mvInt, H265MV &mvBestSub)
{
    bool subpelSeeded = false;
    if(mvBestSub.mvx&3 || mvBestSub.mvy&3)
    {
        H265MV mvInt1, mvInt2, mvInt3, mvInt4;
        mvInt1 = mvInt2 = mvInt3 = mvInt4 = mvInt;
        // bounding ints
        mvInt1.mvx = mvBestSub.mvx & ~3;
        mvInt1.mvy = mvBestSub.mvy & ~3;
        if(mvBestSub.mvx&3) {
            mvInt2 = mvInt1;
            mvInt2.mvx+=4;
        }
        if(mvBestSub.mvy&3) {
            mvInt3 = mvInt1;
            mvInt3.mvy+=4;
            if(m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, (const H265MV*)&mvInt3) == false ) {
               return subpelSeeded;
            }
        }
        if(mvBestSub.mvx&3 && mvBestSub.mvy&3) {
            mvInt4.mvx = mvInt2.mvx;
            mvInt4.mvy = mvInt3.mvy;
        }

        if(mvBest==mvInt1 || mvBest==mvInt2 || mvBest==mvInt3 || mvBest==mvInt4) {
            Ipp32s size = 0;
            if     (meInfo->width<=8 && meInfo->height<=8)   size = 0;
            else if(meInfo->width<=16 && meInfo->height<=16) size = 1;
            else if(meInfo->width<=32 && meInfo->height<=32) size = 2;
            else                                             size = 3;
            Ipp32s hadFoundSize = size;
            Ipp32s useHadamard = (m_par->hadamardMe == 3);
            PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;
            Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mvBestSub, ref, useHadamard, refIdxMem, size, hadFoundSize);
            cost+=MvCost1RefLog(mvBestSub, predInfo);
            if(cost<costBest) {
                subpelSeeded = true;
                useHadamard = (m_par->hadamardMe >= 2);
                mvBest = mvBestSub;
                costBest = MatchingMetricPuMem(src, meInfo, &mvBest, ref, useHadamard, refIdxMem, size, hadFoundSize);
                mvCostBest = MvCost1RefLog(mvBest, predInfo);
                costBest += mvCostBest;
                H265MV mvCenter = mvBest;
                Ipp32s bestPos = 0;
                for (Ipp32s i = 0; i < 4; i++) {
                    H265MV mv = mvCenter;
                    if (i & 2) mv.mvx += (i & 1) ? 1 : -1;
                    else       mv.mvy += (i & 1) ? 1 : -1;
                    Ipp32s hadFoundSize = size;
                    Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
                    if(cost < costBest) {
                        Ipp32s mvCost = MvCost1RefLog(mv, predInfo);
                        if (costBest > cost+mvCost) {
                            costBest = cost+mvCost;
                            mvBest = mv;
                            mvCostBest = mvCost;
                            bestPos=i;
                        }
                    }
                }
                if(mvBest!=mvCenter) {
                    H265MV mvCenter = mvBest;
                    for (Ipp32s i = 0; i < 2; i++) {
                        H265MV mv = mvCenter;
                        if (bestPos & 2) mv.mvy += (i & 1) ? 1 : -1;
                        else             mv.mvx += (i & 1) ? 1 : -1;
                        Ipp32s hadFoundSize = size;
                        Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
                        if(cost < costBest) {
                            Ipp32s mvCost = MvCost1RefLog(mv, predInfo);
                            if (costBest > cost+mvCost) {
                                costBest = cost+mvCost;
                                mvBest = mv;
                                mvCostBest = mvCost;
                            }
                        }
                    }
                }
            }
        }
    }
    return subpelSeeded;
}
#endif

#ifdef MEMOIZE_SUBPEL
template <typename PixType>
void H265CU<PixType>::MeSubPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                               const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s refIdxMem)
#else
template <typename PixType>
void H265CU<PixType>::MeSubPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                               const Frame *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const
#endif
{
    H265MV mvCenter = *mv;
    H265MV mvBest = *mv;
    Ipp32s startPos = 1;
    Ipp32s meStep = 1;
    Ipp32s costBest = *cost;
    Ipp32s costBestSAD = *cost;
    Ipp32s mvCostBest = *mvCost;

    static Ipp32s endPos[5] = {5, 9, 5, 2, 2};  // based on index
    Ipp16s pattern_index[2][2];    // meStep, iter
    Ipp32s iterNum[2] = {1, 1};

#ifdef MEMOIZE_SUBPEL
    Ipp32s size = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   size = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) size = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) size = 2;
    else                                             size = 3;
    Ipp32s hadFoundSize = size;
#endif
    // select subMe pattern
    switch (m_par->patternSubPel)
    {
        case SUBPEL_NO:             // int pel only
            return; 
        case SUBPEL_BOX_HPEL_ONLY:  // more points with square patterns, no quarter-pel
            pattern_index[0][1] = pattern_index[0][0] =  pattern_index[0][1] = pattern_index[1][0] = 1;
            break;
        case SUBPEL_BOX:            // more points with square patterns
#ifdef MEMOIZE_SUBPEL
            if(m_par->hadamardMe >= 2) {
                if(meInfo->width == meInfo->height && (meInfo->width == m_par->MaxCUSize || MemHadFirst(size, meInfo, refIdxMem))) {
                    return MemSubPelBatchedBox(meInfo, predInfo, ref, mv, cost, mvCost, refIdxMem, size);
                } else if(MemSubpelInRange(size, meInfo, refIdxMem, mv)) {
                    pattern_index[0][1] = pattern_index[0][0] =  pattern_index[0][1] = pattern_index[1][0] = 1;
                } else if(meInfo->width == meInfo->height) {
                    return MemSubPelBatchedBox(meInfo, predInfo, ref, mv, cost, mvCost, refIdxMem, size);
                } else {
                    return MeSubPelBatchedBox(meInfo, predInfo, ref, mv, cost, mvCost);
                }
            } else
#endif
            {
            return MeSubPelBatchedBox(meInfo, predInfo, ref, mv, cost, mvCost);
            }
            break;
        case SUBPEL_DIA_2STEP:      // quarter pel with simplified diamond pattern - double
            pattern_index[0][1] = pattern_index[0][0] =  pattern_index[0][1] = pattern_index[1][0] = 0;
            iterNum[1] = iterNum[0] = 2;
            break;
#if defined(AMT_FAST_SUBPEL_SEARCH)
        case SUBPEL_FASTBOX_DIA_ORTH:
            iterNum[1] = iterNum[0] = 2;
            pattern_index[1][0] = pattern_index[0][0] = 0;
            pattern_index[1][1] = 2;
            if(m_par->hadamardMe == 2) {
                if(meInfo->width == meInfo->height && (meInfo->width == m_par->MaxCUSize || MemHadFirst(size, meInfo, refIdxMem))) {
                    return MemSubPelBatchedFastBoxDiaOrth(meInfo, predInfo, ref, mv, cost, mvCost, refIdxMem, size);
                } else if(!MemSubpelInRange(size, meInfo, refIdxMem, mv) && meInfo->width == meInfo->height) {
                    return MemSubPelBatchedFastBoxDiaOrth(meInfo, predInfo, ref, mv, cost, mvCost, refIdxMem, size);
                }
            }
            break;
#endif
        case SUBPEL_DIA:            // quarter pel with simplified diamond pattern - single
        default:
            return MeSubPelBatchedDia(meInfo, predInfo, ref, mv, cost, mvCost);
            break;
     }

    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;

    Ipp32s useHadamard = (m_par->hadamardMe >= 2);
    if (m_par->hadamardMe == 2) {
        // when satd is for subpel only need to recalculate cost for intpel motion vector
#ifdef MEMOIZE_SUBPEL
        costBest = MatchingMetricPuMem(src, meInfo, &mvBest, ref, useHadamard, refIdxMem, size, hadFoundSize) + mvCostBest;
        if(m_par->partModes==1 && hadFoundSize<=size) hadFoundSize = -1;
#ifdef MEMOIZE_SUBPEL_TEST
        Ipp32s testcostBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard) + mvCostBest;
        assert(testcostBest == costBest);
#endif
#else
        costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard) + mvCostBest;
#endif
    }

#if defined(AMT_FAST_SUBPEL_SEARCH)
    if(m_par->patternSubPel==SUBPEL_FASTBOX_DIA_ORTH) {
        meStep = 1;
        useHadamard = (m_par->hadamardMe == 3);
        CostType costBestHalf = costBestSAD;    //or HAD if hadamardMe==3
        Ipp32s mvCostBestHalf = *mvCost;
        H265MV mvBestHalf = mvCenter;
        
        for (Ipp32s iter = 0; iter < iterNum[meStep]; iter++) {
            for (Ipp32s mePos = startPos; mePos < endPos[pattern_index[meStep][iter]]; mePos++) {
                H265MV mv = {
                    Ipp16s(mvCenter.mvx + (tab_mePatternSelector[pattern_index[meStep][iter]][mePos][0] << meStep)),
                    Ipp16s(mvCenter.mvy + (tab_mePatternSelector[pattern_index[meStep][iter]][mePos][1] << meStep))
                };
                Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                Ipp32s testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                assert(testcost == cost);
#endif
                if (costBestHalf > cost) {
                    Ipp32s mvCost = MvCost1RefLog(mv, predInfo);
                    cost += mvCost;
                    if (costBestHalf > cost) {
                        costBestHalf = cost;
                        mvCostBestHalf = mvCost;
                        mvBestHalf = mv;
                    }
                }
            }
            if(mvBestHalf==mvCenter) break;
        }
        
        if(mvBestHalf!=mvCenter)  {
            if(m_par->hadamardMe == 2) {
                costBestHalf = MatchingMetricPuMem(src, meInfo, &mvBestHalf, ref, true, refIdxMem, size, hadFoundSize);
                costBestHalf += mvCostBestHalf;
            } 
            if (costBest > costBestHalf) {
                costBest = costBestHalf;
                mvCostBest = mvCostBestHalf;
                mvBest = mvBestHalf;
            }
        }

        hadFoundSize = size;
        mvCenter = mvBest;
        meStep--;
        CostType costBestQuarter = COST_MAX;
        Ipp32s mvCostBestQuarter = 0;
        H265MV mvBestQuarter = mvCenter;
        Ipp32s bestQPos = 1;
        for (Ipp32s iter = 0; iter < iterNum[meStep]; iter++) {
            for (Ipp32s mePos = startPos; mePos < endPos[pattern_index[meStep][iter]]; mePos++) {
                H265MV mv = {
                    Ipp16s(mvCenter.mvx + (tab_mePatternSelector[pattern_index[meStep][iter]][mePos][0] << meStep)),
                    Ipp16s(mvCenter.mvy + (tab_mePatternSelector[pattern_index[meStep][iter]][mePos][1] << meStep))
                };
                Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
#ifdef MEMOIZE_SUBPEL_TEST
                Ipp32s testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                assert(testcost == cost);
#endif
                if (costBestQuarter > cost) {
                    Ipp32s mvCost = MvCost1RefLog(mv, predInfo);
                    cost += mvCost;
                    if (costBestQuarter > cost) {
                        costBestQuarter = cost;
                        mvCostBestQuarter = mvCost;
                        mvBestQuarter = mv;
                        bestQPos = mePos;
                    }
                }
            }

            if(m_par->hadamardMe == 2) {
                costBestQuarter = MatchingMetricPuMem(src, meInfo, &mvBestQuarter, ref, true, refIdxMem, size, hadFoundSize);
                costBestQuarter += mvCostBestQuarter;
            }
            if (costBest > costBestQuarter) {
                costBest = costBestQuarter;
                mvCostBest = mvCostBestQuarter;
                mvBest = mvBestQuarter;
            }
            if(mvBestQuarter==mvCenter) break;
            mvCenter = mvBest;
            costBestQuarter = COST_MAX;
            mvCostBestQuarter = 0;
            mvBestQuarter = mvCenter;
            if(bestQPos==1 || bestQPos==3) pattern_index[0][1] = 3;
            else                           pattern_index[0][1] = 4;
        }
    } else
#endif
    {
        while (meStep >= 0) {
            H265MV bestMv = mvCenter;
            for (Ipp32s iter = 0; iter < iterNum[meStep]; iter++) {
                for (Ipp32s mePos = startPos; mePos < endPos[pattern_index[meStep][iter]]; mePos++) {
                    H265MV mv = {
                        Ipp16s(bestMv.mvx + (tab_mePatternSelector[pattern_index[meStep][iter]][mePos][0] << meStep)),
                        Ipp16s(bestMv.mvy + (tab_mePatternSelector[pattern_index[meStep][iter]][mePos][1] << meStep))
                    };
#ifdef MEMOIZE_SUBPEL
                    Ipp32s cost = MatchingMetricPuMem(src, meInfo, &mv, ref, useHadamard, refIdxMem, size, hadFoundSize);
                    if(m_par->partModes==1 && hadFoundSize<=size) hadFoundSize = -1;
#ifdef MEMOIZE_SUBPEL_TEST
                    Ipp32s testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                    assert(testcost == cost);
#endif
#else
                    Ipp32s cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
#endif
                    if (costBest > cost) {
                        Ipp32s mvCost = MvCost1RefLog(mv, predInfo);
                        cost += mvCost;
                        if (costBest > cost) {
                            costBest = cost;
                            mvCostBest = mvCost;
                            mvBest = mv;
                        }
                    }
                }

                bestMv = mvBest;
                if(bestMv == mvCenter) break;
            }

            if (m_par->patternSubPel == SUBPEL_BOX_HPEL_ONLY)
                break; //no quarter pel
#ifdef MEMOIZE_SUBPEL
            hadFoundSize = size;
#endif
            mvCenter = mvBest;
            meStep--;
            //startPos = 1;
        }
    }

    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;
}

static Ipp32s GetFlBits(Ipp32s val, Ipp32s maxVal)
{
    return val + (val != maxVal - 1);
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

template <typename PixType>
Ipp32s H265CU<PixType>::PuCost(H265MEInfo *meInfo) {
    RefPicList *refPicList = m_currFrame->m_refPicList;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;
    Ipp32s cost = INT_MAX;
    if (meInfo->refIdx[0] >= 0 && meInfo->refIdx[1] >= 0) {
        cost = MatchingMetricBipredPu(src, meInfo, meInfo->refIdx, meInfo->MV, 1);
    }
    else {
        Ipp32s list = (meInfo->refIdx[1] >= 0);
        FrameData *ref = refPicList[list].m_refFrames[meInfo->refIdx[list]]->m_recon;
        cost = MatchingMetricPu(src, meInfo, meInfo->MV + list, ref, 1);
    }
    return cost;
}


template <typename PixType>
void H265CU<PixType>::RefineBiPred(const H265MEInfo *meInfo, const Ipp8s refIdxs[2], Ipp32s curPUidx,
                                   H265MV mvs[2], Ipp32s *cost, Ipp32s *mvCost)
{
    H265MV mvBest[2] = { mvs[0], mvs[1] };
    Ipp32s costBest = *cost;
    Ipp32s mvCostBest = *mvCost;

    Ipp32s useHadamard = (m_par->hadamardMe >= 2);

    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;
    Frame *refF = m_currFrame->m_refPicList[0].m_refFrames[refIdxs[0]];
    Frame *refB = m_currFrame->m_refPicList[1].m_refFrames[refIdxs[1]];
#ifdef AMT_BIREFINE_CONVERGE
    Ipp32s iterbest = costBest;
#endif
    bool changed = true;
    for (Ipp32s iter = 0; iter < m_par->numBiRefineIter - 1 && changed; iter++) {
        H265MV mvCenter[2] = { mvBest[0], mvBest[1] };
        changed = false;
        Ipp32s count = (refF == refB && mvBest[0] == mvBest[1]) ? 4 : 8;

        for (Ipp32s i = 0; i < count; i++) {

            H265MV mv[2] = { mvCenter[0], mvCenter[1] };
            if (i & 2)
                mv[i >> 2].mvx += (i & 1) ? 1 : -1;
            else
                mv[i >> 2].mvy += (i & 1) ? 1 : -1;

            //-------------------------------------------------
            // FRAME THREADING PATCH
            if ((m_par->m_framesInParallel > 1) &&
                (/*refIdx[0] >= 0 && */CheckFrameThreadingSearchRange(meInfo, mv + 0) == false ||
                 /*refIdx[1] >= 0 && */CheckFrameThreadingSearchRange(meInfo, mv + 1) == false)) {
                    continue;
            }
            //-------------------------------------------------
            if ((m_par->RegionIdP1 > 0) &&
                (/*refIdx[0] >= 0 && */CheckIndepRegThreadingSearchRange(meInfo, mv + 0) == false ||
                 /*refIdx[1] >= 0 && */CheckIndepRegThreadingSearchRange(meInfo, mv + 1) == false)) {
                    continue;
            }

            Ipp32s mvCost = MvCost1RefLog(mv[0], m_amvpCand[curPUidx] + 2 * refIdxs[0] + 0) +
                            MvCost1RefLog(mv[1], m_amvpCand[curPUidx] + 2 * refIdxs[1] + 1);
            Ipp32s cost = 0;
#ifdef MEMOIZE_SUBPEL
            if(m_par->hadamardMe>=2) {
                // current limitation of MemSubpelBatchedBox (always uses hadamard)
                cost = MatchingMetricBipredPuSearch(src, meInfo, refIdxs, mv, useHadamard);
#ifdef MEMOIZE_BIPRED_TEST
                Ipp32s testcost = MatchingMetricBipredPu(src, meInfo, refIdxs, mv, useHadamard);
                assert(testcost==cost);
#endif
            } else
#endif
            {
                cost = MatchingMetricBipredPu(src, meInfo, refIdxs, mv, useHadamard);
            }

            cost += mvCost;

            if (costBest > cost) {
                costBest = cost;
                mvCostBest = mvCost;
                mvBest[0] = mv[0];
                mvBest[1] = mv[1];
                changed = true;
            }
        }
#ifdef AMT_BIREFINE_CONVERGE
        // converge  bit by bit
        if(changed && (costBest+m_rdLambdaSqrt)>iterbest) {
            changed = false;
        }
        iterbest = costBest;
#endif
    }

    mvs[0] = mvBest[0];
    mvs[1] = mvBest[1];
    *cost = costBest;
    *mvCost = mvCostBest;
}
#ifdef AMT_ICRA_OPT
template <typename PixType>
void H265CU<PixType>::CheckSkipCandFullRD(const H265MEInfo *meInfo, const MvPredInfo<5> *mergeCand, Ipp32s *mergeCandIdx)
{
    Ipp32s absPartIdx = meInfo->absPartIdx;
    Ipp32s depth = meInfo->depth;
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
    m_bsf->CtxSave(ctxInitial);
    H265CUData *dataCu = m_data + absPartIdx;
    Ipp8u cuWidth = (Ipp8u)(m_par->MaxCUSize >> depth);
    Ipp32s lumaCostShift = (m_par->bitDepthLumaShift << 1);
    Ipp32s chromaCostShift = (m_par->bitDepthChromaShift << 1);
    Ipp32u numParts = m_par->NumPartInCU >> (2 * depth);
    Ipp32s idx422 = (m_par->chroma422 && (m_par->AnalyseFlags & HEVC_COST_CHROMA)) ? (numParts >> 1) : 0;
    const PixType *srcY = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchRecLuma);
    const PixType *srcC = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchRecChroma);
    const PixType *predY = m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
    const PixType *predC = m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
    // setup CU data
    memset(dataCu, 0, sizeof(H265CUData));
    dataCu->depth = depth;
    dataCu->size = cuWidth;
    dataCu->qp = m_lcuQps[m_ctbAddr];
    dataCu->flags.mergeFlag = 1;
    dataCu->flags.skippedFlag = 1;
    CostType bestCost = COST_MAX;
    Ipp32s candBest = -1;

    for (Ipp32s cand = 0; cand < mergeCand->numCand; cand++) {
        CostType cost = 0;
        const H265MV *mergeMv = mergeCand->mvCand + 2 * cand;
        const Ipp8s  *mergeRefIdx = mergeCand->refIdx + 2 * cand;

        if (mergeRefIdx[0] < 0 && mergeRefIdx[1] < 0)
            continue; // skip duplicate

//#ifdef AMT_REF_SCALABLE
//        if(mergeRefIdx[0]>0) {
//            Frame *ref0 = m_currFrame->m_refPicList[0].m_refFrames[mergeRefIdx[0]];
//            if(m_par->BiPyramidLayers == 4 && ref0->m_pyramidLayer>m_par->refLayerLimit[m_currFrame->m_pyramidLayer]) 
//                continue;
//        }
//        if(mergeRefIdx[1]>0) {
//            Frame *ref1 = m_currFrame->m_refPicList[1].m_refFrames[mergeRefIdx[1]];
//            if(m_par->BiPyramidLayers == 4 && ref1->m_pyramidLayer>m_par->refLayerLimit[m_currFrame->m_pyramidLayer]) 
//                continue;
//        }
//#endif
        //-------------------------------------------------
        // FRAME THREADING PATCH
        if ((m_par->m_framesInParallel > 1) &&
            (mergeRefIdx[0] >= 0 && CheckFrameThreadingSearchRange(meInfo, mergeMv + 0) == false ||
             mergeRefIdx[1] >= 0 && CheckFrameThreadingSearchRange(meInfo, mergeMv + 1) == false)) {
            continue;
        }
        //-------------------------------------------------
        if ((m_par->RegionIdP1 > 0) &&
            (mergeRefIdx[0] >= 0 && CheckIndepRegThreadingSearchRange(meInfo, mergeMv + 0) == false ||
             mergeRefIdx[1] >= 0 && CheckIndepRegThreadingSearchRange(meInfo, mergeMv + 1) == false)) {
            continue;
        }
        //-------------------------------------------------

        // fill CU data
        dataCu->interDir = (mergeRefIdx[0] >= 0) + 2 * (mergeRefIdx[1] >= 0);
        dataCu->mergeIdx = (Ipp8u)cand;
        dataCu->mv[0] = mergeMv[0];
        dataCu->mv[1] = mergeMv[1];
        dataCu->refIdx[0] = mergeRefIdx[0];
        dataCu->refIdx[1] = mergeRefIdx[1];

        PropagateSubPart(dataCu, numParts);

        InterPredCu<TEXT_LUMA>(absPartIdx, depth, m_interPredY, MAX_CU_SIZE);
        if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
            InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredC, MAX_CU_SIZE << m_par->chromaShiftWInv);

        // bits for SKIP
        if(cand) m_bsf->CtxRestore(ctxInitial);
        CostType skipBitCost = GetCuModesCost(m_data, absPartIdx, depth);
        cost += skipBitCost;
        // luma distortion of SKIP
        cost += TuSse(srcY, m_pitchSrcLuma, predY, MAX_CU_SIZE, cuWidth, cuWidth, lumaCostShift);

        if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
            //InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredC, MAX_CU_SIZE << m_par->chromaShiftWInv);
            // chroma distortion of SKIP
            CostType costChroma = TuSse(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv,
                                             cuWidth << m_par->chromaShiftWInv, cuWidth >> m_par->chromaShiftH, chromaCostShift);
            //kolya //WEIGHTED_CHROMA_DISTORTION (JCTVC-F386)
            if (m_par->IntraChromaRDO)
                costChroma *= m_ChromaDistWeight;
            cost += costChroma;
        }
        if(cost<bestCost) {
            bestCost = cost;
            candBest = cand;
        }
    }
    *mergeCandIdx = candBest;
    m_bsf->CtxRestore(ctxInitial);
}
#endif

template <typename PixType>
#ifdef AMT_ALT_FAST_SKIP
bool H265CU<PixType>::CheckMerge2Nx2N(Ipp32s absPartIdx, Ipp8u depth)
#else
void H265CU<PixType>::CheckMerge2Nx2N(Ipp32s absPartIdx, Ipp8u depth)
#endif
{
    bool retVal = false;
    CostType costInitial = m_costCurr;
    CABAC_CONTEXT_H265 ctxForcedSkip[NUM_CABAC_CONTEXT];
    CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
#ifndef AMT_ALT_FAST_SKIP
    if (m_par->fastSkip)
        m_bsf->CtxSave(ctxInitial);
#endif
    H265MEInfo meInfo;
    meInfo.absPartIdx = absPartIdx;
    meInfo.depth = depth;
    meInfo.splitMode = 0;
    meInfo.posx = (h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize;
    meInfo.posy = (h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize;
    meInfo.width = (m_par->MaxCUSize >> depth);
    meInfo.height = (m_par->MaxCUSize >> depth);

    // FRAME THREADING PATCH 
    Ipp32u numParts = m_par->NumPartInCU >> (2 * depth);
    Ipp32s idx422 = (m_par->chroma422 && (m_par->AnalyseFlags & HEVC_COST_CHROMA)) ? (numParts >> 1) : 0;
    H265CUData *dataCu = m_data + absPartIdx;
    Ipp8u cuWidth = (Ipp8u)(m_par->MaxCUSize >> depth);
    memset(dataCu, 0, sizeof(H265CUData));
    dataCu->depth = depth;
    dataCu->size = cuWidth;
    dataCu->qp = m_lcuQps[m_ctbAddr];
    dataCu->flags.mergeFlag = 1;
    dataCu->flags.skippedFlag = 1;
    PropagateSubPart(dataCu, numParts);

    Ipp32s candBest = -1;

#ifdef AMT_ICRA_OPT
    if(m_par->SkipCandRD) {
        CheckSkipCandFullRD(&meInfo, m_mergeCand, &candBest);
    } else
#endif
    {
        CheckMergeCand(&meInfo, m_mergeCand, 0, &candBest);
        m_skipCandBest = candBest;
    }

    if (candBest < 0) {
        m_costCurr = COST_MAX;
#ifndef AMT_ALT_FAST_SKIP
        return;
#else
        return false;
#endif
    }

    const H265MV *mergeMv = m_mergeCand->mvCand + 2 * candBest;
    const Ipp8s  *mergeRefIdx = m_mergeCand->refIdx + 2 * candBest;
    Ipp32s lumaCostShift = (m_par->bitDepthLumaShift << 1);
    Ipp32s chromaCostShift = (m_par->bitDepthChromaShift << 1);

    // setup CU data
    dataCu->interDir = (mergeRefIdx[0] >= 0) + 2 * (mergeRefIdx[1] >= 0);
    dataCu->mergeIdx = (Ipp8u)candBest;
    dataCu->mv[0] = mergeMv[0];
    dataCu->mv[1] = mergeMv[1];
    dataCu->refIdx[0] = mergeRefIdx[0];
    dataCu->refIdx[1] = mergeRefIdx[1];
    PropagateSubPart(dataCu, numParts);

    const PixType *srcY = m_ySrc + GetLumaOffset(m_par, absPartIdx, m_pitchRecLuma);
    const PixType *srcC = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchRecChroma);
    const PixType *predY = m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
    const PixType *predC = m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);

#ifdef MEMOIZE_CAND_SUBPEL
    const PixType *predBuf;
    Ipp32s memPitch;
    if(MemCandUseSubpel(&meInfo, dataCu->interDir-1, mergeRefIdx, (dataCu->interDir==2)?mergeMv+1:mergeMv, predBuf, memPitch)) {
        IppiSize roiY = { cuWidth, cuWidth };
        _ippiCopy_C1R(predBuf, memPitch, m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE), MAX_CU_SIZE, roiY);
#ifdef MEMOIZE_CAND_SUBPEL_TEST
        Ipp32s sse0 = TuSse(srcY, m_pitchSrcLuma, predBuf, memPitch, cuWidth, cuWidth, lumaCostShift);
        InterPredCu<TEXT_LUMA>(absPartIdx, depth, m_interPredY, MAX_CU_SIZE);
        Ipp32s sse1 = TuSse(srcY, m_pitchSrcLuma, m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE), MAX_CU_SIZE, cuWidth, cuWidth, lumaCostShift);
        assert(sse0==sse1);
#endif
    } else
#endif
    {
    InterPredCu<TEXT_LUMA>(absPartIdx, depth, m_interPredY, MAX_CU_SIZE);
    }
    if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
        InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredC, MAX_CU_SIZE << m_par->chromaShiftWInv);

    // bits for SKIP
    CostType skipBitCost = GetCuModesCost(m_data, absPartIdx, depth);
    m_costCurr += skipBitCost;
#ifdef AMT_ALT_FAST_SKIP
    bool fastSkip = false;
    if(m_par->fastSkip) {
        if(m_SCid[depth][absPartIdx]<2) fastSkip=true;
        else {
            Ipp32s STC = GetSpatioTemporalComplexity(absPartIdx, depth, 0, depth);
            Ipp8u layer = (m_par->BiPyramidLayers > 1) ? m_currFrame->m_pyramidLayer : 4;
            if(STC<layer+depth+1) fastSkip = true;
        }
    }
    if(fastSkip) {
#else
    if (m_par->fastSkip) {
#endif
        Ipp8u trDepthMin, trDepthMax;
        GetTrDepthMinMax(m_par, depth, PART_SIZE_2Nx2N, &trDepthMin, &trDepthMax);

        CoeffsType *residY = m_interResidY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
        TuDiff(residY, MAX_CU_SIZE, srcY, m_pitchSrcLuma, predY, MAX_CU_SIZE, cuWidth);

        if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
            CoeffsType *residU = m_interResidU + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
            CoeffsType *residV = m_interResidV + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
            h265_DiffNv12(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv,
                          residU, MAX_CU_SIZE >> m_par->chromaShiftW, residV, MAX_CU_SIZE >> m_par->chromaShiftW,
                          cuWidth >> m_par->chromaShiftW, cuWidth >> m_par->chromaShiftH);
        }
#ifdef AMT_ALT_FAST_SKIP
        if(!TuMaxSplitInterHasNonZeroCoeff(absPartIdx, trDepthMax)) 
            retVal =  true;
#else
        // current CABAC context corresponds to forced SKIP state
        // save it before coding MERGE 2Nx2N coeffs
        m_bsf->CtxSave(ctxForcedSkip);

        FillSubPartSkipFlag_(dataCu, numParts, 0);
#ifdef AMT_ADAPTIVE_TU_DEPTH
        TuMaxSplitInter(absPartIdx, trDepthMin);
#else
        TuMaxSplitInter(absPartIdx, trDepthMax);
#endif

        if (IsZeroCbf(dataCu, idx422)) {
            return; // early exit if MERGE 2Nx2N gets zero cbf
        }
        else {
            m_bsf->CtxRestore(ctxInitial, tab_ctxIdxOffset[SPLIT_CODING_UNIT_FLAG_HEVC], 12); // [SPLIT_CODING_UNIT_FLAG_HEVC..PART_SIZE_HEVC]
            m_costCurr -= skipBitCost;
            m_costCurr += GetCuModesCost(m_data, absPartIdx, depth); // bit cost of MERGE 2Nx2N CU modes

            // no early exit by fastSkip, so save MERGE 2Nx2N decision before trying forced SKIP
            // save the decision and restore initial states
            // Need to copy back pred not just swap, cannot use SaveBestInterPredAndResid(absPartIdx, depth);
            if (m_costStored[depth] > m_costCurr) 
            {
                // save Inter predicted pixels and residual simply by swapping pointers
                std::swap(m_interPredY, m_interPredBestY);
                std::swap(m_interPredC, m_interPredBestC);
                std::swap(m_interResidY, m_interResidBestY);
                std::swap(m_interResidU, m_interResidBestU);
                std::swap(m_interResidV, m_interResidBestV);
                IppiSize roiY = { cuWidth, cuWidth };
                IppiSize roiC = { cuWidth << m_par->chromaShiftWInv, cuWidth >> m_par->chromaShiftH };
                _ippiCopy_C1R(predY, MAX_CU_SIZE, m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE), MAX_CU_SIZE, roiY);
                if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
                    _ippiCopy_C1R(predC, MAX_CU_SIZE << m_par->chromaShiftWInv, 
                        m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv),
                        MAX_CU_SIZE << m_par->chromaShiftWInv, roiC);
                }
                predY = m_interPredY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
                predC = m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
            }

            SaveFinalCuDecision(absPartIdx, depth, false);
            m_bsf->CtxRestore(ctxForcedSkip);
            m_costCurr = costInitial + skipBitCost; // bit cost of forced SKIP CU modes
        }
#endif
    }

    // luma distortion of SKIP
    m_costCurr += TuSse(srcY, m_pitchSrcLuma, predY, MAX_CU_SIZE, cuWidth, cuWidth, lumaCostShift);

    CostType costChroma = 0;
#ifdef AMT_FIX_CHROMA_SKIP
    if (m_par->AnalyseFlags & HEVC_COST_CHROMA) {
        // chroma distortion of SKIP
        costChroma = TuSse(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv,
                                cuWidth << m_par->chromaShiftWInv, cuWidth >> m_par->chromaShiftH, chromaCostShift);
        //kolya //WEIGHTED_CHROMA_DISTORTION (JCTVC-F386)
        if (m_par->IntraChromaRDO)
            costChroma *= m_ChromaDistWeight;
    }
#else
    // predict chroma for forced SKIP check if didn't before for MERGE 2Nx2N
    if (!(m_par->AnalyseFlags & HEVC_COST_CHROMA))
        InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredC, MAX_CU_SIZE << m_par->chromaShiftWInv);

    // chroma distortion of SKIP
    costChroma =
        TuSse(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv, cuWidth << m_par->chromaShiftWInv, cuWidth >> m_par->chromaShiftH, chromaCostShift);
    if (!(m_par->AnalyseFlags & HEVC_COST_CHROMA))
        costChroma /= 4; // add chroma with little weight
#endif
    m_costCurr += costChroma;

    if (m_costCurr < m_costStored[depth]) {
        // prediction pels are a reconstruction pels for SKIP
        IppiSize roiY = { cuWidth, cuWidth };
        IppiSize roiC = { cuWidth << m_par->chromaShiftWInv, cuWidth >> m_par->chromaShiftH };
        PixType *recY = m_interRecWorkY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
        PixType *recC = m_interRecWorkC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
        _ippiCopy_C1R(predY, MAX_CU_SIZE, recY, MAX_CU_SIZE, roiY);
        if (m_par->AnalyseFlags & HEVC_COST_CHROMA)
            _ippiCopy_C1R(predC, MAX_CU_SIZE << m_par->chromaShiftWInv, recC, MAX_CU_SIZE << m_par->chromaShiftWInv, roiC);
        // set skip flag
        dataCu->flags.skippedFlag = 1;
        dataCu->cbf[0] = 0;
        dataCu->cbf[1] = 0;
        dataCu->cbf[2] = 0;
        PropagateSubPart(dataCu, numParts);
    }
#ifdef AMT_ALT_FAST_SKIP
    return retVal;
#endif
}
#if defined(AMT_ICRA_OPT)
template <typename PixType>
void H265CU<PixType>::FastCheckAMP(Ipp32s absPartIdx, Ipp8u depth, const H265MEInfo *meInfo2Nx2N)
{
    H265CUData *dataCu = m_data + absPartIdx;
    Ipp32u numParts = (m_par->NumPartInCU >> (depth << 1));
    Ipp32s cuSizeInMinTU = (m_par->MaxCUSize >> depth) >> m_par->QuadtreeTULog2MinSize;
    
    Ipp32s cuX = (h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize;
    Ipp32s cuY = (h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize;

    H265MEInfo meInfo[2];
    H265MEInfo bestMeInfo[2]={*meInfo2Nx2N, *meInfo2Nx2N};
    Ipp32s bestAMPCost = INT_MAX;

    for(Ipp32s partMode = PART_SIZE_2NxnU; partMode<=PART_SIZE_nRx2N; partMode++) 
    {
        Ipp32s numPu = h265_numPu[partMode];
        Ipp32s cost = 0;
        for (Ipp32s partIdx = 0; partIdx < numPu; partIdx++) {
            Ipp32s puX, puY, puW, puH, partAddr;
            GetPartOffsetAndSize(partIdx, partMode, dataCu->size, puX, puY, puW, puH);
            GetPartAddr(partIdx, partMode, numParts, partAddr);

            meInfo[partIdx].absPartIdx = absPartIdx + partAddr;
            meInfo[partIdx].depth = depth;
            meInfo[partIdx].width  = (Ipp8u)puW;
            meInfo[partIdx].height = (Ipp8u)puH;
            meInfo[partIdx].posx = (Ipp8u)(cuX + puX);
            meInfo[partIdx].posy = (Ipp8u)(cuY + puY);
            meInfo[partIdx].splitMode = partMode;

            GetMergeCand(absPartIdx, partMode, partIdx, cuSizeInMinTU, m_mergeCand + partIdx);
            GetAmvpCand(absPartIdx, partMode, partIdx, m_amvpCand[partIdx]);
#ifdef AMT_ICRA_OPT
            if( m_par->partModes>2
                && m_par->FastAMPSkipME >= 2
                && (StoredCuData(depth)+absPartIdx)->flags.skippedFlag==1
                && (    ((partMode==PART_SIZE_2NxnU || partMode==PART_SIZE_2NxnD) && puH>dataCu->size/2) 
                || ((partMode==PART_SIZE_nLx2N || partMode==PART_SIZE_nRx2N) && puW>dataCu->size/2) )
                )
            {
                //cost += MePu(meInfo, partIdx, false);
                Ipp32s retCost = MePu(meInfo, partIdx, false);
                if( retCost == INT_MAX ) {
                    m_costCurr = COST_MAX;
                    return; // same mv/refidx as for 2Nx2N skip RDO part
                }
                cost += retCost;
            } else 
#endif
            {
                cost += MePu(meInfo, partIdx);
            }
        }

        if(cost < bestAMPCost) {
            bestAMPCost = cost;
            bestMeInfo[0] = meInfo[0];
            bestMeInfo[1] = meInfo[1];
        }
    }
    if (meInfo2Nx2N) {
        // check if all PUs have the same mv/refidx as what was found for 2Nx2N
        bool sameAs2Nx2N = true;
        for (Ipp32s partIdx = 0; partIdx < 2; partIdx++)
            sameAs2Nx2N = sameAs2Nx2N && SamePrediction(bestMeInfo + partIdx, meInfo2Nx2N);
        if (sameAs2Nx2N) {
            m_costCurr = COST_MAX;
            return; // same mv/refidx as for 2Nx2N skip RDO part
        }
    }
    GetMergeCand(absPartIdx, bestMeInfo[0].splitMode, 0, cuSizeInMinTU, m_mergeCand + 0);
    GetAmvpCand(absPartIdx, bestMeInfo[0].splitMode, 0, m_amvpCand[0]);
    {
        Ipp32s xstart = bestMeInfo[0].posx >> m_par->QuadtreeTULog2MinSize;
        Ipp32s ystart = bestMeInfo[0].posy >> m_par->QuadtreeTULog2MinSize;
        Ipp32s xsize = bestMeInfo[0].width >> m_par->QuadtreeTULog2MinSize;
        Ipp32s ysize = bestMeInfo[0].height >> m_par->QuadtreeTULog2MinSize;
        for (Ipp32s y = ystart; y < ystart + ysize; y++) {
            for (Ipp32s x = xstart; x < xstart + xsize; x++) {
                Ipp32s rorder = y * PITCH_TU + x;
                Ipp32s zorder = h265_scan_r2z4[rorder];
                m_data[zorder].mv[0] = bestMeInfo[0].MV[0];
                m_data[zorder].mv[1] = bestMeInfo[0].MV[1];
                m_data[zorder].refIdx[0] = bestMeInfo[0].refIdx[0];
                m_data[zorder].refIdx[1] = bestMeInfo[0].refIdx[1];
            }
        }
    }
    GetMergeCand(absPartIdx, bestMeInfo[0].splitMode, 1, cuSizeInMinTU, m_mergeCand + 1);
    GetAmvpCand(absPartIdx, bestMeInfo[0].splitMode, 1, m_amvpCand[1]);
    {
        Ipp32s xstart = bestMeInfo[1].posx >> m_par->QuadtreeTULog2MinSize;
        Ipp32s ystart = bestMeInfo[1].posy >> m_par->QuadtreeTULog2MinSize;
        Ipp32s xsize = bestMeInfo[1].width >> m_par->QuadtreeTULog2MinSize;
        Ipp32s ysize = bestMeInfo[1].height >> m_par->QuadtreeTULog2MinSize;
        for (Ipp32s y = ystart; y < ystart + ysize; y++) {
            for (Ipp32s x = xstart; x < xstart + xsize; x++) {
                Ipp32s rorder = y * PITCH_TU + x;
                Ipp32s zorder = h265_scan_r2z4[rorder];
                m_data[zorder].mv[0] = bestMeInfo[1].MV[0];
                m_data[zorder].mv[1] = bestMeInfo[1].MV[1];
                m_data[zorder].refIdx[0] = bestMeInfo[1].refIdx[0];
                m_data[zorder].refIdx[1] = bestMeInfo[1].refIdx[1];
            }
        }
    }

    CuCost(absPartIdx, depth, bestMeInfo);
}
#endif
template <typename PixType>
void H265CU<PixType>::CheckInter(Ipp32s absPartIdx, Ipp8u depth, Ipp32s partMode, const H265MEInfo *meInfo2Nx2N)
{
    H265CUData *dataCu = m_data + absPartIdx;
    Ipp32u numParts = (m_par->NumPartInCU >> (depth << 1));
    Ipp32s cuSizeInMinTU = (m_par->MaxCUSize >> depth) >> m_par->QuadtreeTULog2MinSize;
    Ipp32s numPu = h265_numPu[partMode];
    Ipp32s cuX = (h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize;
    Ipp32s cuY = (h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize;

    H265MEInfo meInfo[4];
    for (Ipp32s partIdx = 0; partIdx < numPu; partIdx++) {
        Ipp32s puX, puY, puW, puH, partAddr;
        GetPartOffsetAndSize(partIdx, partMode, dataCu->size, puX, puY, puW, puH);
        GetPartAddr(partIdx, partMode, numParts, partAddr);

        meInfo[partIdx].absPartIdx = absPartIdx + partAddr;
        meInfo[partIdx].depth = depth;
        meInfo[partIdx].width  = (Ipp8u)puW;
        meInfo[partIdx].height = (Ipp8u)puH;
        meInfo[partIdx].posx = (Ipp8u)(cuX + puX);
        meInfo[partIdx].posy = (Ipp8u)(cuY + puY);
        meInfo[partIdx].splitMode = partMode;

        if (partMode != PART_SIZE_2Nx2N) // merge candidates for 2Nx2N are already there
            GetMergeCand(absPartIdx, partMode, partIdx, cuSizeInMinTU, m_mergeCand + partIdx);
        GetAmvpCand(absPartIdx, partMode, partIdx, m_amvpCand[partIdx]);
#ifdef AMT_ICRA_OPT
        if( m_par->partModes>2
            && m_par->FastAMPSkipME >= 2
            && (StoredCuData(depth)+absPartIdx)->flags.skippedFlag==1
            && (    ((partMode==PART_SIZE_2NxnU || partMode==PART_SIZE_2NxnD) && puH>dataCu->size/2) 
                 || ((partMode==PART_SIZE_nLx2N || partMode==PART_SIZE_nRx2N) && puW>dataCu->size/2) )
          )
        {
            Ipp32s retCost = MePu(meInfo, partIdx, false);
            if ( retCost == INT_MAX ) { // no valid mv here (due to frame threading specific)
                m_costCurr = COST_MAX;
                return; 
            }

        } else 
#endif
        {
            MePu(meInfo, partIdx);
        }
    }

    if (meInfo2Nx2N) {
        // check if all PUs have the same mv/refidx as what was found for 2Nx2N
        bool sameAs2Nx2N = true;
        for (Ipp32s partIdx = 0; partIdx < numPu; partIdx++)
            sameAs2Nx2N = sameAs2Nx2N && SamePrediction(meInfo + partIdx, meInfo2Nx2N);
        if (sameAs2Nx2N) {
            m_costCurr = COST_MAX;
            return; // same mv/refidx as for 2Nx2N skip RDO part
        }
    }

    CuCost(absPartIdx, depth, meInfo);
}


template <typename PixType>
#ifdef AMT_ICRA_OPT
Ipp32s H265CU<PixType>::MePu(H265MEInfo *meInfos, Ipp32s partIdx, bool doME)
#else
void H265CU<PixType>::MePu(H265MEInfo *meInfos, Ipp32s partIdx)
#endif
{
    if (m_par->enableCmFlag)
        return MePuGacc(meInfos, partIdx);

    H265MEInfo *meInfo = meInfos + partIdx;
#ifdef MEMOIZE_SUBPEL
    Ipp32s size = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   size = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) size = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) size = 2;
    else                                             size = 3;
#endif
    Ipp32s lastPredIdx = (partIdx == 0) ? 0 : meInfos[partIdx - 1].interDir - 1;

    RefPicList *refPicList = m_currFrame->m_refPicList;
    Ipp32u numParts = (m_par->NumPartInCU >> (meInfo->depth << 1));
    Ipp32s partAddr = meInfo->absPartIdx &  (numParts - 1);
    Ipp32s curPUidx = (meInfo->splitMode == PART_SIZE_NxN) ? (partAddr / (numParts >> 2)) : !!partAddr;

    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;

    H265MV mvRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s costRefBest[2][MAX_NUM_REF_IDX] = { { INT_MAX }, { INT_MAX } };
    Ipp32s mvCostRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s bitsRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp8s  refIdxBest[2] = {};
    Ipp8s  refIdxBestB[2] = {};

    for (Ipp32s i = 0; i < 2; i++)
        for (Ipp32s j = 0; j < MAX_NUM_REF_IDX; j++)
            costRefBest[i][j] = INT_MAX;

    Ipp32s predIdxBits[3];
    GetPredIdxBits(meInfo->splitMode, (m_cslice->slice_type == P_SLICE), curPUidx, lastPredIdx, predIdxBits);

    Ipp32s numLists = (m_cslice->slice_type == B_SLICE) + 1;

#ifdef AMT_ICRA_OPT
    for (Ipp32s list = 0; list < numLists && doME; list++) {
#else
    for (Ipp32s list = 0; list < numLists; list++) {
#endif
        Ipp32s ref0PyramidLayer = refPicList[list].m_refFrames[0]->m_pyramidLayer;
        Ipp32s numRefIdx = m_currFrame->m_refPicList[list].m_refFramesCount;
        bool searchRef = true;

        for (Ipp8s refIdx = 0; refIdx < numRefIdx && searchRef; refIdx++) {
            const MvPredInfo<2> *amvp = m_amvpCand[curPUidx] + 2 * refIdx + list;

            Ipp32s refPyramidLayer = refPicList[list].m_refFrames[refIdx]->m_pyramidLayer;

//#ifdef AMT_REF_SCALABLE
//            if (refIdx && m_par->BiPyramidLayers==4 && refPyramidLayer > m_par->refLayerLimit[m_currFrame->m_pyramidLayer]) 
//                continue;
//#endif

            if (list == 1) { 
                Ipp32s idx0 = m_currFrame->m_mapRefIdxL1ToL0[refIdx];
                if (idx0 >= 0 && costRefBest[0][idx0]!=INT_MAX) {
                    // don't do ME just re-calc costs
                    mvRefBest[1][refIdx] = mvRefBest[0][idx0];
                    mvCostRefBest[1][refIdx] = MvCost1RefLog(mvRefBest[0][idx0], amvp);
                    bitsRefBest[1][refIdx] = MVP_LX_FLAG_BITS;
                    bitsRefBest[1][refIdx] += GetFlBits(refIdx, numRefIdx);
                    bitsRefBest[1][refIdx] += predIdxBits[1];
                    costRefBest[1][refIdx] = costRefBest[0][idx0] - mvCostRefBest[0][idx0];
                    costRefBest[1][refIdx] -= (Ipp32s)(bitsRefBest[0][idx0] * m_rdLambdaSqrt + 0.5);
                    costRefBest[1][refIdx] += mvCostRefBest[1][refIdx];
                    costRefBest[1][refIdx] += (Ipp32s)(bitsRefBest[1][refIdx] * m_rdLambdaSqrt + 0.5);
                    if (costRefBest[1][refIdxBest[1]] > costRefBest[1][refIdx]) {
                        refIdxBest[1] = refIdx;
                    } else if (refIdx && refPyramidLayer > ref0PyramidLayer) {
                        searchRef = false;
                    }
                    continue;
                }
            }

            // use satd for all candidates even if satd is for subpel only
            Ipp32s useHadamard = (m_par->hadamardMe == 3);

            Ipp32s costBest = INT_MAX;
            H265MV mvBest = { 0, 0 };
            FrameData *ref = refPicList[list].m_refFrames[refIdx]->m_recon;
            for (Ipp32s i = 0; i < amvp->numCand; i++) {
                H265MV mv = amvp->mvCand[i];
                if (std::find(amvp->mvCand, amvp->mvCand + i, amvp->mvCand[i]) != amvp->mvCand + i)
                    continue; // skip duplicate
                ClipMV(mv);
                
                // PATCH
                if(m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, (const H265MV*)&mv) == false ) {
                    continue;
                }
                if(m_par->RegionIdP1 > 0 && CheckIndepRegThreadingSearchRange(meInfo, (const H265MV*)&mv) == false ) {
                    continue;
                }

                //notFound = false;
                Ipp32s cost;
#ifdef MEMOIZE_CAND_SUBPEL
                const PixType *predBuf;
                Ipp32s memPitch;
                Ipp8s refsArr[2] = {refIdx, refIdx};
                if(MemCandUseSubpel(meInfo, list, refsArr, &mv, predBuf, memPitch)) {
                    cost = MatchingMetricPuUse(src, meInfo, &mv, ref, useHadamard, predBuf, memPitch);
#ifdef MEMOIZE_CAND_SUBPEL_TEST
                    Ipp32s testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                    assert(testcost == cost);
#endif
                } else if(((mv.mvx | mv.mvy) & 3) && MemSubpelUse(size, mv.mvx&0x3, mv.mvy&0x3, meInfo, 
                    m_currFrame->m_mapListRefUnique[list][refIdx], &mv, predBuf, memPitch)) {
                    cost = MatchingMetricPuUse(src, meInfo, &mv, ref, useHadamard, predBuf, memPitch);
#ifdef MEMOIZE_CAND_SUBPEL_TEST
                    Ipp32s testcost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                    assert(testcost == cost);
#endif
                } else
#endif
                {
                cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
                }

                if (costBest > cost) {
                    costBest = cost;
                    mvBest = mv;
                }
            }
            H265MV mvBestSub = mvBest;
            Ipp32s mvCostBest = 0;
            if (costBest == INT_MAX) {
                costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard);
                mvCostBest = MvCost1RefLog(mvBest, amvp);
                costBest += mvCostBest;
            }
            else {
            if ((mvBest.mvx | mvBest.mvy) & 3) {
                // recalculate cost for nearest int-pel
                mvBest.mvx = (mvBest.mvx + 1) & ~3;
                mvBest.mvy = (mvBest.mvy + 1) & ~3;
                if(m_par->m_framesInParallel > 1 && CheckFrameThreadingSearchRange(meInfo, (const H265MV*)&mvBest) == false ) {
                    // best subpel amvp is inside range, but rounded intpel isn't 
                    mvBest.mvy -= 4;
                }
                costBest = MatchingMetricPu(src, meInfo, &mvBest, ref, useHadamard);
                mvCostBest = MvCost1RefLog(mvBest, amvp);
                costBest += mvCostBest;
            }
            else {
                // add cost of zero mvd
                mvCostBest = 2 * (Ipp32s)(1 * m_rdLambdaSqrt + 0.5);
                costBest += mvCostBest;
            }
            }
#ifdef AMT_INT_ME_SEED
            Ipp16s meStepMax = MeIntSeed(meInfo, amvp, list, refIdx, mvRefBest[list][0],
                                         mvRefBest[0][0], mvBest, costBest, mvCostBest, mvBestSub);
#endif

            H265MV mvInt = mvBest;
#ifdef AMT_INT_ME_SEED
            MeIntPel(meInfo, amvp, ref, &mvBest, &costBest, &mvCostBest, meStepMax);
#else
            MeIntPel(meInfo, amvp, ref, &mvBest, &costBest, &mvCostBest);
#endif

            if (m_par->RegionIdP1 == 0 || CheckIndepRegThreadingSearchRange(meInfo, &mvBest, 1))
            {
#ifdef MEMOIZE_SUBPEL
#ifdef AMT_FAST_SUBPEL_SEED
                if((m_par->patternSubPel == SUBPEL_BOX) ||
                    !MeSubPelSeed(meInfo, amvp, ref, mvBest, costBest, mvCostBest, m_currFrame->m_mapListRefUnique[list][refIdx], mvInt, mvBestSub))
#endif
                {
                    MeSubPel(meInfo, amvp, ref, &mvBest, &costBest, &mvCostBest, m_currFrame->m_mapListRefUnique[list][refIdx]);
                }
#else
                MeSubPel(meInfo, amvp, ref, &mvBest, &costBest, &mvCostBest);
#endif
            }

            mvRefBest[list][refIdx] = mvBest;
            SetMemBestMV(size, meInfo, m_currFrame->m_mapListRefUnique[list][refIdx], mvBest);
            mvCostRefBest[list][refIdx] = mvCostBest;
            costRefBest[list][refIdx] = costBest;
            bitsRefBest[list][refIdx] = MVP_LX_FLAG_BITS + predIdxBits[list];
            bitsRefBest[list][refIdx] += GetFlBits(refIdx, numRefIdx);
            costRefBest[list][refIdx] += (Ipp32s)(bitsRefBest[list][refIdx] * m_rdLambdaSqrt + 0.5);
            if (costRefBest[list][refIdxBest[list]] > costRefBest[list][refIdx]) {
                refIdxBest[list] = refIdx;
            } else if (refIdx && refPyramidLayer > ref0PyramidLayer) {
                searchRef = false;
            }
        }
    }

    Ipp8s  idxL0 = refIdxBestB[0] = refIdxBest[0];
    Ipp8s  idxL1 = refIdxBestB[1] = refIdxBest[1];
    Ipp32s costList[2] = { costRefBest[0][idxL0], costRefBest[1][idxL1] };

#ifdef AMT_REF_SCALABLE_REF_SELECT
    if(m_cslice->slice_type == B_SLICE && idxL1==idxL0
        && m_par->BiPyramidLayers>1 && m_currFrame->m_pyramidLayer==0 && m_par->NumRefLayers>2) 
    {
        Ipp32s numRefIdx = m_cslice->num_ref_idx[0];
        for (Ipp8s refIdx = 0; refIdx < numRefIdx; refIdx++) {
            if(refIdx!=refIdxBest[0] && costRefBest[0][refIdx]!=INT_MAX && costRefBest[0][refIdx]*8 < costRefBest[1][refIdxBest[1]]*9) {
                idxL0 = refIdxBestB[0] = refIdx;
                break;
            }
        }
    }
#else
    if(m_cslice->slice_type == B_SLICE && refPicList[1].m_refFrames[idxL1]==refPicList[0].m_refFrames[idxL0]) 
    {
        Ipp32s bestCostAlt = INT_MAX;
        Ipp8s bestList = (idxL0<=idxL1)?1:0;
        Ipp8s bestAlt = refIdxBest[bestList];
        Ipp32s numRefIdx = m_cslice->num_ref_idx[bestList];
        for (Ipp8s refIdx = 0; refIdx < numRefIdx; refIdx++) {
            if(refIdx!=refIdxBest[bestList] && costRefBest[bestList][refIdx]!=INT_MAX && costRefBest[bestList][refIdx]<bestCostAlt) {
                bestCostAlt = costRefBest[bestList][refIdx];
                bestAlt = refIdx;
            }
        }
        Ipp8s oList = (bestList)?0:1;
        numRefIdx = m_cslice->num_ref_idx[oList];
        for (Ipp8s refIdx = 0; refIdx < numRefIdx; refIdx++) {
            if(refIdx!=refIdxBest[oList] && costRefBest[oList][refIdx]!=INT_MAX && costRefBest[oList][refIdx]<bestCostAlt) {
                bestCostAlt = costRefBest[oList][refIdx];
                bestAlt = refIdx;
                bestList = oList;
            }
        }
        if(bestCostAlt!=INT_MAX && bestCostAlt*8<costRefBest[bestList][refIdxBest[bestList]]*9) {
            if(bestList==0) {
                idxL0 = refIdxBestB[0] = bestAlt;
            } else {
                idxL1 = refIdxBestB[1] = bestAlt;
            }
        }
    }
#endif

    Ipp32s costBiBest = INT_MAX;
    H265MV mvBiBest[2] = { mvRefBest[0][idxL0], mvRefBest[1][idxL1] };

    Frame *refF = refPicList[0].m_refFrames[idxL0];
    Frame *refB = refPicList[1].m_refFrames[idxL1];
#ifdef AMT_ICRA_OPT
    if (m_cslice->slice_type == B_SLICE && meInfo->width + meInfo->height != 12 && refF && refB && doME) {
#else
    if (m_cslice->slice_type == B_SLICE && meInfo->width + meInfo->height != 12 && refF && refB) {
#endif
        // use satd for bidir refine because it is always sub-pel
        Ipp32s useHadamard = (m_par->hadamardMe >= 2);

        Ipp32s mvCostBiBest = mvCostRefBest[0][idxL0] + mvCostRefBest[1][idxL1];
        costBiBest = mvCostBiBest;

#ifdef MEMOIZE_SUBPEL
        if(useHadamard) {
            // current limitation of MemSubpelBatchedBox (always uses hadamard)
            m_interpIdxFirst = m_interpIdxLast = 0; // for MatchingMetricBipredPu optimization
            costBiBest += MatchingMetricBipredPuSearch(src, meInfo, refIdxBestB, mvBiBest, useHadamard);
#ifdef MEMOIZE_BIPRED_TEST
            Ipp32s testcost = MatchingMetricBipredPu(src, meInfo, refIdxBest, mvBiBest, useHadamard) + mvCostBiBest;
            assert(testcost==costBiBest);
#endif
        } else
#endif
        {
        costBiBest += MatchingMetricBipredPu(src, meInfo, refIdxBestB, mvBiBest, useHadamard);
        }

        // refine Bidir
        if (IPP_MIN(costList[0], costList[1]) * 9 > 8 * costBiBest) {
            RefineBiPred(meInfo, refIdxBestB, curPUidx, mvBiBest, &costBiBest, &mvCostBiBest);

            Ipp32s bitsBiBest = MVP_LX_FLAG_BITS + predIdxBits[2];
            bitsBiBest += GetFlBits(idxL0, m_cslice->num_ref_idx[0]);
            bitsBiBest += GetFlBits(idxL1, m_cslice->num_ref_idx[1]);
            costBiBest += (Ipp32s)(bitsBiBest * m_rdLambdaSqrt + 0.5);
        }
    }

    Ipp32s useHadamard = (m_par->hadamardMe >= 2);

    Ipp32s bestMergeCost = INT_MAX;
    H265MV bestMergeMv[2] = {};
    Ipp8s  bestMergeRef[2] = { -1, -1 };

#ifdef AMT_ALT_FAST_SKIP
    if (!m_par->fastSkip || meInfo->splitMode != PART_SIZE_2Nx2N || m_skipCandBest<0) 
#else
    if (!(m_par->fastSkip && meInfo->splitMode == PART_SIZE_2Nx2N)) 
#endif
    {
        Ipp32s mergeIdx = -1;
        bestMergeCost = CheckMergeCand(meInfo, m_mergeCand + curPUidx, useHadamard, &mergeIdx);
        if (mergeIdx >= 0) {
            bestMergeRef[0] = m_mergeCand[curPUidx].refIdx[2 * mergeIdx + 0];
            bestMergeRef[1] = m_mergeCand[curPUidx].refIdx[2 * mergeIdx + 1];
            bestMergeMv[0] = m_mergeCand[curPUidx].mvCand[2 * mergeIdx + 0];
            bestMergeMv[1] = m_mergeCand[curPUidx].mvCand[2 * mergeIdx + 1];
        }
    }
#ifdef AMT_ALT_FAST_SKIP
    else {
        Ipp32s mergeIdx = m_skipCandBest;
        const MvPredInfo<5> *mergeCand = m_mergeCand + curPUidx;
        const Ipp8s *refIdx = mergeCand->refIdx + 2 * mergeIdx;
        const H265MV *mv = mergeCand->mvCand + 2 * mergeIdx;
        if (refIdx[0] >= 0 && refIdx[1] >= 0) {
            bestMergeCost = MatchingMetricBiPredPuMemCand(src, meInfo, refIdx, mv, useHadamard);
        } else {
            Ipp32s listIdx = (refIdx[1] >= 0);
            FrameData *ref = m_currFrame->m_refPicList[listIdx].m_refFrames[refIdx[listIdx]]->m_recon;
            H265MV mvc = mv[listIdx];
            ClipMV(mvc);
            bestMergeCost = MatchingMetricPuMemSCand(src, meInfo, listIdx, refIdx, &mvc, ref, useHadamard);
        }

        bestMergeCost += (Ipp32s)(GetFlBits(mergeIdx, mergeCand->numCand) * m_rdLambdaSqrt + 0.5);
        bestMergeRef[0] = m_mergeCand[curPUidx].refIdx[2 * mergeIdx + 0];
        bestMergeRef[1] = m_mergeCand[curPUidx].refIdx[2 * mergeIdx + 1];
        bestMergeMv[0]  = m_mergeCand[curPUidx].mvCand[2 * mergeIdx + 0];
        bestMergeMv[1]  = m_mergeCand[curPUidx].mvCand[2 * mergeIdx + 1];
    }
#endif

    Ipp32s retCost=INT_MAX;
    if (bestMergeCost <= costList[0] && bestMergeCost <= costList[1] && bestMergeCost <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L0 * (bestMergeRef[0] >= 0) + INTER_DIR_PRED_L1 * (bestMergeRef[1] >= 0);
        meInfo->refIdx[0] = bestMergeRef[0];
        meInfo->refIdx[1] = bestMergeRef[1];
        meInfo->MV[0] = bestMergeMv[0];
        meInfo->MV[1] = bestMergeMv[1];
        retCost = bestMergeCost;
    }
    else if (costList[0] <= costList[1] && costList[0] <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L0;
        meInfo->refIdx[0] = (Ipp8s)refIdxBest[0];
        meInfo->refIdx[1] = -1;
        meInfo->MV[0] = mvRefBest[0][refIdxBest[0]];
        meInfo->MV[1] = MV_ZERO;
        retCost = costList[0];
    }
    else if (costList[1] <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L1;
        meInfo->refIdx[0] = -1;
        meInfo->refIdx[1] = (Ipp8s)refIdxBest[1];
        meInfo->MV[0] = MV_ZERO;
        meInfo->MV[1] = mvRefBest[1][refIdxBest[1]];
        retCost = costList[1];
    }
    else {
        meInfo->interDir = INTER_DIR_PRED_L0 + INTER_DIR_PRED_L1;
        meInfo->refIdx[0] = (Ipp8s)refIdxBestB[0];
        meInfo->refIdx[1] = (Ipp8s)refIdxBestB[1];
        meInfo->MV[0] = mvBiBest[0];
        meInfo->MV[1] = mvBiBest[1];
        retCost = costBiBest;
    }

    Ipp32s xstart = meInfo->posx >> m_par->QuadtreeTULog2MinSize;
    Ipp32s ystart = meInfo->posy >> m_par->QuadtreeTULog2MinSize;
    Ipp32s xsize = meInfo->width >> m_par->QuadtreeTULog2MinSize;
    Ipp32s ysize = meInfo->height >> m_par->QuadtreeTULog2MinSize;
    for (Ipp32s y = ystart; y < ystart + ysize; y++) {
        for (Ipp32s x = xstart; x < xstart + xsize; x++) {
            Ipp32s rorder = y * PITCH_TU + x;
            Ipp32s zorder = h265_scan_r2z4[rorder];
            m_data[zorder].mv[0] = meInfo->MV[0];
            m_data[zorder].mv[1] = meInfo->MV[1];
            m_data[zorder].refIdx[0] = meInfo->refIdx[0];
            m_data[zorder].refIdx[1] = meInfo->refIdx[1];
        }
    }

#ifdef AMT_ICRA_OPT
    return retCost;
#endif
}


namespace {
    /* helper function */
    /* should match fei part */
    Ipp32s GetPuSize(Ipp32s puw, Ipp32s puh)
    {
        if ((puw | puh) == 8)       return 0;
        else if ((puw | puh) == 16) return 1;
        else if ((puw | puh) == 32) return 2;
        else if ((puw | puh) == 64) return 3;
        assert("unsupported PU size");
        return 0;
    }
};


template <typename PixType>
#ifdef AMT_ICRA_OPT
Ipp32s H265CU<PixType>::MePuGacc(H265MEInfo *meInfos, Ipp32s partIdx)
#else
void H265CU<PixType>::MePuGacc(H265MEInfo *meInfos, Ipp32s partIdx)
#endif
{
    H265MEInfo *meInfo = meInfos + partIdx;
#ifdef MEMOIZE_SUBPEL
    Ipp32s size = 0;
    if     (meInfo->width<=8 && meInfo->height<=8)   size = 0;
    else if(meInfo->width<=16 && meInfo->height<=16) size = 1;
    else if(meInfo->width<=32 && meInfo->height<=32) size = 2;
    else                                             size = 3;
#endif
    Ipp32s lastPredIdx = (partIdx == 0) ? 0 : meInfos[partIdx - 1].interDir - 1;

    Ipp32s x = (m_ctbPelX + meInfo->posx) / meInfo->width;
    Ipp32s y = (m_ctbPelY + meInfo->posy) / meInfo->height;
    Ipp32s puSize = GetPuSize(meInfo->width, meInfo->height);

    Ipp32u numParts = (m_par->NumPartInCU >> (meInfo->depth << 1));
    Ipp32s partAddr = meInfo->absPartIdx &  (numParts - 1);
    Ipp32s curPUidx = (meInfo->splitMode == PART_SIZE_NxN) ? (partAddr / (numParts >> 2)) : !!partAddr;

    H265MV mvRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s costRefBest[2][MAX_NUM_REF_IDX];
    Ipp32s mvCostRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp32s bitsRefBest[2][MAX_NUM_REF_IDX] = {};
    Ipp8s  refIdxBest[2] = {};
    Ipp8s  refIdxBestB[2] = {};

    Ipp32s predIdxBits[3];
    GetPredIdxBits(meInfo->splitMode, (m_cslice->slice_type == P_SLICE), curPUidx, lastPredIdx, predIdxBits);

    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * m_pitchSrcLuma;
    Ipp32s numLists = (m_cslice->slice_type == B_SLICE) + 1;
    for (Ipp32s list = 0; list < numLists; list++) {
        Ipp32s numRefIdx = m_cslice->num_ref_idx[list];

        for (Ipp8s refIdx = 0; refIdx < numRefIdx; refIdx++) {

            costRefBest[list][refIdx] = INT_MAX;
            if (list == 1) { // TODO: use pre-build table of duplicates instead of std::find
                Ipp32s idx0 = m_currFrame->m_mapRefIdxL1ToL0[refIdx];
                if (idx0 >= 0) {
                    if (costRefBest[0][idx0] != INT_MAX) { // in case of 64x64 there may be INT_MAX cost
                        // don't do ME just re-calc costs
                        mvRefBest[1][refIdx] = mvRefBest[0][idx0];
                        mvCostRefBest[1][refIdx] = MvCost1RefLog(mvRefBest[0][idx0], m_amvpCand[curPUidx] + 2 * refIdx + list);
                        bitsRefBest[1][refIdx] = MVP_LX_FLAG_BITS;
                        bitsRefBest[1][refIdx] += GetFlBits(refIdx, numRefIdx);
                        bitsRefBest[1][refIdx] += predIdxBits[1];
                        costRefBest[1][refIdx] = costRefBest[0][idx0] - mvCostRefBest[0][idx0];
                        costRefBest[1][refIdx] -= (Ipp32s)(bitsRefBest[0][idx0] * m_rdLambdaSqrt + 0.5);
                        costRefBest[1][refIdx] += mvCostRefBest[1][refIdx];
                        costRefBest[1][refIdx] += (Ipp32s)(bitsRefBest[1][refIdx] * m_rdLambdaSqrt + 0.5);
                        if (costRefBest[1][refIdxBest[1]] > costRefBest[1][refIdx])
                            refIdxBest[1] = refIdx;
                    }
                    continue;
                }
            }

            Ipp32s uniqRefIdx = m_currFrame->m_mapListRefUnique[list][refIdx];
            Ipp32s pitchData = m_currFrame->m_feiInterData[uniqRefIdx][puSize]->m_pitch;

            Ipp32u *cmData = (Ipp32u *)(m_currFrame->m_feiInterData[uniqRefIdx][puSize]->m_sysmem + y * pitchData);

            H265MV mvBest = { 0, 0 };
            Ipp32s costBest = INT_MAX;
            Ipp32s mvCostBest = 0;

            if (meInfo->width > 32 || meInfo->height > 32) {
                {
                    Ipp32s puSize32x32 = GetPuSize(32, 32);
                    Ipp32s x32 = x * 2;
                    Ipp32s y32 = y * 2;
                    Ipp32s pitchData32 = m_currFrame->m_feiInterData[uniqRefIdx][puSize32x32]->m_pitch;
                    Ipp32u *cmData32_0 = (Ipp32u *)(m_currFrame->m_feiInterData[uniqRefIdx][puSize32x32]->m_sysmem + (y32 + 0) * pitchData32);
                    Ipp32u *cmData32_1 = (Ipp32u *)(m_currFrame->m_feiInterData[uniqRefIdx][puSize32x32]->m_sysmem + (y32 + 1) * pitchData32);

                    mfxI16Pair cmMv32[4] = {
                        *((mfxI16Pair *)(cmData32_0 + 16 * (x32+0))),
                        *((mfxI16Pair *)(cmData32_0 + 16 * (x32+1))),
                        *((mfxI16Pair *)(cmData32_1 + 16 * (x32+0))),
                        *((mfxI16Pair *)(cmData32_1 + 16 * (x32+1)))
                    };
                    Ipp32s *cmDist32[4] = {
                        (Ipp32s *)(cmData32_0 + 16 * (x32+0) + 1),
                        (Ipp32s *)(cmData32_0 + 16 * (x32+1) + 1),
                        (Ipp32s *)(cmData32_1 + 16 * (x32+0) + 1),
                        (Ipp32s *)(cmData32_1 + 16 * (x32+1) + 1)
                    };

                    Ipp32s cmDist64[9];
                    for (Ipp16s sadIdx0 = 0, dy = -1; dy <= 1; dy++) {
                        for (Ipp16s dx = -1; dx <= 1; dx++, sadIdx0++) {
                            H265MV mv0 = {cmMv32[0].x + dx, cmMv32[0].y + dy};
                            cmDist64[sadIdx0] = cmDist32[0][sadIdx0];

                            for (Ipp32s i = 1; i < 4; i++) {
                                H265MV mv = {cmMv32[i].x - 1, cmMv32[i].y - 1};
                                Ipp32s dmvx = mv0.mvx - mv.mvx;
                                Ipp32s dmvy = mv0.mvy - mv.mvy;
                                if (dmvx > 2 || dmvx < 0 || dmvy > 2 || dmvy < 0) {
                                    cmDist64[sadIdx0] = INT_MAX;
                                    break;
                                }
                                cmDist64[sadIdx0] += cmDist32[i][dmvy * 3 + dmvx];
                            }

                            if (cmDist64[sadIdx0] != INT_MAX) {
                                Ipp32s mvCost = MvCost1RefLog(mv0, m_amvpCand[curPUidx] + 2 * refIdx + list);
                                cmDist64[sadIdx0] += mvCost;
                                if (costBest > cmDist64[sadIdx0]) {
                                    costBest = cmDist64[sadIdx0];
                                    mvBest = mv0;
                                    mvCostBest = mvCost;
                                }
                            }
                        }
                    }
                }

                mfxI16Pair cmMv = *((mfxI16Pair *)(cmData + 16 * x));
                Ipp32u *cmDist = cmData + 16 * x + 1;   // 9 halfpel distortions for 64x64
                for (Ipp16s sadIdx = 0, dy = -2; dy <= 2; dy += 2) {
                    for (Ipp16s dx = -2; dx <= 2; dx += 2, sadIdx++) {
                        H265MV mv = { cmMv.x + dx, cmMv.y + dy };
                        Ipp32s mvCost = MvCost1RefLog(mv, m_amvpCand[curPUidx] + 2 * refIdx + list);
                        Ipp32s cost = cmDist[sadIdx] + mvCost;
                        if (costBest > cost) {
                            costBest = cost;
                            mvBest = mv;
                            mvCostBest = mvCost;
                        }
                    }
                }

            } else if (meInfo->width > 16 || meInfo->height > 16) {
                mfxI16Pair cmMv = *((mfxI16Pair *)(cmData + 16 * x));
                Ipp32u *cmDist = cmData + 16 * x + 1;
                for (Ipp16s sadIdx = 0, dy = -1; dy <= 1; dy++) {
                    for (Ipp16s dx = -1; dx <= 1; dx++, sadIdx++) {
                        H265MV mv = { cmMv.x + dx, cmMv.y + dy };
                        Ipp32s mvCost = MvCost1RefLog(mv, m_amvpCand[curPUidx] + 2 * refIdx + list);
                        Ipp32s cost = cmDist[sadIdx] + mvCost;
                        if (costBest > cost) {
                            costBest = cost;
                            mvBest = mv;
                            mvCostBest = mvCost;
                        }
                    }
                }
            } else {
                mfxI16Pair cmMv = *((mfxI16Pair *)(cmData + 2 * x));
                Ipp32u *cmDist = cmData + 2 * x + 1;
                mvBest.mvx = cmMv.x;
                mvBest.mvy = cmMv.y;
                mvCostBest = MvCost1RefLog(mvBest, m_amvpCand[curPUidx] + 2 * refIdx + list);
                costBest = *cmDist + mvCostBest;
            }

            mvRefBest[list][refIdx] = mvBest;
            mvCostRefBest[list][refIdx] = mvCostBest;
            costRefBest[list][refIdx] = costBest;
            if (costBest != INT_MAX) {
                bitsRefBest[list][refIdx] = MVP_LX_FLAG_BITS + predIdxBits[list];
                bitsRefBest[list][refIdx] += GetFlBits(refIdx, numRefIdx);
                costRefBest[list][refIdx] += (Ipp32s)(bitsRefBest[list][refIdx] *m_rdLambdaSqrt + 0.5);

                if (costRefBest[list][refIdxBest[list]] > costRefBest[list][refIdx])
                    refIdxBest[list] = refIdx;
            }
        }
    }
    
    Ipp32s useHadamard = (m_par->hadamardMe >= 2);
    const Ipp8s idx0 = refIdxBest[0];
    const Ipp8s idx1 = refIdxBest[1];

    refIdxBestB[0] = refIdxBest[0];
    refIdxBestB[1] = refIdxBest[1];
#ifdef AMT_REF_SCALABLE
    if(m_cslice->slice_type == B_SLICE && refIdxBestB[1] == refIdxBestB[0]
        && m_par->BiPyramidLayers > 1 && m_currFrame->m_pyramidLayer == 0 && m_par->NumRefLayers > 2) 
    {
        Ipp32s numRefIdx = m_cslice->num_ref_idx[0];
        for (Ipp8s i = 0; i < numRefIdx; i++) {
            if (i != refIdxBestB[0] && costRefBest[0][i] != INT_MAX && costRefBest[0][i] * 8 < costRefBest[1][refIdxBestB[1]] * 9) {
                refIdxBestB[0] = i;
                break;
            }
        }
    }
#endif

    Ipp32s metric[3];  // L0, L1 and Bi
    if (m_cslice->slice_type == B_SLICE && (costRefBest[0][idx0] != INT_MAX) && (costRefBest[1][idx1] != INT_MAX)) {
        H265MV mvBest[2] = { mvRefBest[0][idx0], mvRefBest[1][idx1]};
        H265MV mvBestB[2] = { mvRefBest[0][refIdxBestB[0]], mvRefBest[1][refIdxBestB[1]]};
        ClipMV(mvBest[0]);
        ClipMV(mvBest[1]);
        ClipMV(mvBestB[0]);
        ClipMV(mvBestB[1]);
        MatchingMetricPuCombine(metric, src, meInfo, mvBest, mvBestB, refIdxBest, refIdxBestB, useHadamard);
    }
    //else {
    //    // 64x64 only can come here
    //    if (costRefBest[0][idx0] != INT_MAX) {
    //        H265MV mvc = mvRefBest[0][idx0];
    //        ClipMV(mvc);
    //        metric[0] = MatchingMetricPu(src, meInfo, &mvc, m_currFrame->m_refPicList[0].m_refFrames[idx0]->m_recon, useHadamard);
    //    } else if (costRefBest[1][idx1] != INT_MAX) {
    //        H265MV mvc = mvRefBest[1][idx1];
    //        ClipMV(mvc);
    //        metric[1] = MatchingMetricPu(src, meInfo, &mvc, m_currFrame->m_refPicList[1].m_refFrames[idx1]->m_recon, useHadamard);
    //    }
    //}

    Ipp32s costList[2] = { costRefBest[0][idx0], costRefBest[1][idx1] };

    if (costRefBest[0][idx0] != INT_MAX) {
//        H265MV mvc = mvRefBest[0][idx0];
//        ClipMV(mvc);
//        metric0 = MatchingMetricPu(src, meInfo, &mvc, m_currFrame->m_refPicList[0].m_refFrames[idx0]->m_recon, useHadamard);
        costList[0] = metric[0] + mvCostRefBest[0][idx0] + (Ipp32s)(bitsRefBest[0][idx0] * m_rdLambdaSqrt + 0.5);
    }

    if (costRefBest[1][idx1] != INT_MAX) {
//            H265MV mvc = mvRefBest[1][idx1];
//            ClipMV(mvc);
//            metric1 = MatchingMetricPu(src, meInfo, &mvc, m_currFrame->m_refPicList[1].m_refFrames[idx1]->m_recon, useHadamard);
        costList[1] = metric[1] + mvCostRefBest[1][idx1] + (Ipp32s)(bitsRefBest[1][idx1] * m_rdLambdaSqrt + 0.5);
    }

    Ipp32s costBiBest = INT_MAX;
    H265MV mvBiBest[2] = { mvRefBest[0][refIdxBestB[0]], mvRefBest[1][refIdxBestB[1]] };
    Ipp32s costBiL0 = costRefBest[0][refIdxBestB[0]];
    Ipp32s costBiL1 = costRefBest[1][refIdxBestB[1]];

    if (m_cslice->slice_type == B_SLICE && meInfo->width + meInfo->height != 12 && costBiL0 != INT_MAX && costBiL1 != INT_MAX) {
        Frame *refF = m_currFrame->m_refPicList[0].m_refFrames[refIdxBestB[0]];
        Frame *refB = m_currFrame->m_refPicList[1].m_refFrames[refIdxBestB[1]];

        Ipp32s mvCostBiBest = mvCostRefBest[0][refIdxBestB[0]] + mvCostRefBest[1][refIdxBestB[1]];
        costBiBest = mvCostBiBest;
        //costBiBest += MatchingMetricBipredPu(src, meInfo, refIdxBestB, mvBiBest, useHadamard);
        costBiBest += metric[2];

        // refine Bidir
        if (IPP_MIN(costList[0], costList[1]) * 9 > 8 * costBiBest) {
            if (m_par->numBiRefineIter > 1) { 
                ClipMV(mvBiBest[0]);
                ClipMV(mvBiBest[1]);
                m_interpIdxFirst = m_interpIdxLast = 0; // for MatchingMetricBipredPuSearch inside RefineBiPred
                RefineBiPred(meInfo, refIdxBestB, curPUidx, mvBiBest, &costBiBest, &mvCostBiBest);
            }

            Ipp32s bitsBiBest = MVP_LX_FLAG_BITS + predIdxBits[2];
            bitsBiBest += GetFlBits(refIdxBestB[0], m_cslice->num_ref_idx[0]);
            bitsBiBest += GetFlBits(refIdxBestB[1], m_cslice->num_ref_idx[1]);
            costBiBest += (Ipp32s)(bitsBiBest * m_rdLambdaSqrt + 0.5);
        }
    }


    Ipp32s bestMergeCost = INT_MAX;
    H265MV bestMergeMv[2] = {};
    Ipp8s  bestMergeRef[2] = { -1, -1 };
#ifdef AMT_ALT_FAST_SKIP
    if (!m_par->fastSkip || meInfo->splitMode != PART_SIZE_2Nx2N || m_skipCandBest<0) 
#else
    if (!(m_par->fastSkip && meInfo->splitMode == PART_SIZE_2Nx2N))
#endif
    {
        Ipp32s mergeIdx = -1;
        bestMergeCost = CheckMergeCand(meInfo, m_mergeCand + curPUidx, useHadamard, &mergeIdx);
        if (mergeIdx >= 0) {
            bestMergeRef[0] = m_mergeCand[curPUidx].refIdx[2 * mergeIdx + 0];
            bestMergeRef[1] = m_mergeCand[curPUidx].refIdx[2 * mergeIdx + 1];
            bestMergeMv[0] = m_mergeCand[curPUidx].mvCand[2 * mergeIdx + 0];
            bestMergeMv[1] = m_mergeCand[curPUidx].mvCand[2 * mergeIdx + 1];
        }
    }
#ifdef AMT_ALT_FAST_SKIP
    else {
        Ipp32s mergeIdx = m_skipCandBest;
        const MvPredInfo<5> *mergeCand = m_mergeCand + curPUidx;
        const Ipp8s *refIdx = mergeCand->refIdx + 2 * mergeIdx;
        const H265MV *mv = mergeCand->mvCand + 2 * mergeIdx;
        if (refIdx[0] >= 0 && refIdx[1] >= 0) {
            bestMergeCost = MatchingMetricBiPredPuMemCand(src, meInfo, refIdx, mv, useHadamard);
        } else {
            Ipp32s listIdx = (refIdx[1] >= 0);
            FrameData *ref = m_currFrame->m_refPicList[listIdx].m_refFrames[refIdx[listIdx]]->m_recon;
            H265MV mvc = mv[listIdx];
            ClipMV(mvc);
            bestMergeCost = MatchingMetricPuMemSCand(src, meInfo, listIdx, refIdx, &mvc, ref, useHadamard);
        }

        bestMergeCost += (Ipp32s)(GetFlBits(mergeIdx, mergeCand->numCand) * m_rdLambdaSqrt + 0.5);
        bestMergeRef[0] = m_mergeCand[curPUidx].refIdx[2 * mergeIdx + 0];
        bestMergeRef[1] = m_mergeCand[curPUidx].refIdx[2 * mergeIdx + 1];
        bestMergeMv[0]  = m_mergeCand[curPUidx].mvCand[2 * mergeIdx + 0];
        bestMergeMv[1]  = m_mergeCand[curPUidx].mvCand[2 * mergeIdx + 1];
    }
#endif

    Ipp32s retCost = INT_MAX;
    if (bestMergeCost <= costList[0] && bestMergeCost <= costList[1] && bestMergeCost <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L0 * (bestMergeRef[0] >= 0) + INTER_DIR_PRED_L1 * (bestMergeRef[1] >= 0);
        meInfo->refIdx[0] = bestMergeRef[0];
        meInfo->refIdx[1] = bestMergeRef[1];
        meInfo->MV[0] = bestMergeMv[0];
        meInfo->MV[1] = bestMergeMv[1];
        retCost = bestMergeCost;
    }
    else if (costList[0] <= costList[1] && costList[0] <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L0;
        meInfo->refIdx[0] = (Ipp8s)idx0;
        meInfo->refIdx[1] = -1;
        meInfo->MV[0] = mvRefBest[0][idx0];
        meInfo->MV[1] = MV_ZERO;
        retCost = costList[0];
    }
    else if (costList[1] <= costBiBest) {
        meInfo->interDir = INTER_DIR_PRED_L1;
        meInfo->refIdx[0] = -1;
        meInfo->refIdx[1] = (Ipp8s)idx1;
        meInfo->MV[0] = MV_ZERO;
        meInfo->MV[1] = mvRefBest[1][idx1];
        retCost = costList[1];
    }
    else {
        assert(costBiBest != INT_MAX);
        meInfo->interDir = INTER_DIR_PRED_L0 + INTER_DIR_PRED_L1;
        meInfo->refIdx[0] = (Ipp8s)refIdxBestB[0];
        meInfo->refIdx[1] = (Ipp8s)refIdxBestB[1];
        meInfo->MV[0] = mvBiBest[0];
        meInfo->MV[1] = mvBiBest[1];
        retCost = costBiBest;
    }

    if (meInfo->splitMode != PART_SIZE_2Nx2N) {
        Ipp32s xstart = meInfo->posx >> m_par->QuadtreeTULog2MinSize;
        Ipp32s ystart = meInfo->posy >> m_par->QuadtreeTULog2MinSize;
        Ipp32s xsize = meInfo->width >> m_par->QuadtreeTULog2MinSize;
        Ipp32s ysize = meInfo->height >> m_par->QuadtreeTULog2MinSize;
        for (Ipp32s y = ystart; y < ystart + ysize; y++) {
            for (Ipp32s x = xstart; x < xstart + xsize; x++) {
                Ipp32s rorder = y * PITCH_TU + x;
                Ipp32s zorder = h265_scan_r2z4[rorder];
                m_data[zorder].mv[0] = meInfo->MV[0];
                m_data[zorder].mv[1] = meInfo->MV[1];
                m_data[zorder].refIdx[0] = meInfo->refIdx[0];
                m_data[zorder].refIdx[1] = meInfo->refIdx[1];
            }
        }
    }

#ifdef AMT_ICRA_OPT
    return retCost;
#endif
}


template <typename PixType>
bool H265CU<PixType>::CheckGpuIntraCost(Ipp32s absPartIdx, Ipp32s depth) const
{
    bool tryIntra = true;
    
#ifdef MFX_VA_BUG
    if (m_par->enableCmFlag && m_par->cmIntraThreshold != 0 && m_cslice->slice_type != I_SLICE) {
        mfxU32 cmIntraCost = 0;
        mfxU32 cmInterCost = 0;
        mfxU32 cmInterCostMin = IPP_MAX_32U;
        Ipp32s cuSize = m_par->MaxCUSize >> depth;

        Ipp32s x = (m_ctbPelX + ((h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize)) >> 4;
        Ipp32s y = (m_ctbPelY + ((h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize)) >> 4;

        Ipp32s x_num = (cuSize == 32) ? 2 : 1;
        Ipp32s y_num = (cuSize == 32) ? 2 : 1;

        for (Ipp32s j = y; j < y + y_num; j++) {
            for (Ipp32s i = x; i < x + x_num; i++) {
                mfxFEIH265IntraDist *intraDistLine = feiOut->IntraDist + j*feiOut->IntraPitch;
                cmIntraCost += intraDistLine[i].Dist;
            }
        }

        for (Ipp32s refList = 0; refList < ((m_cslice->slice_type == B_SLICE) ? 2 : 1); refList++) {
            cmInterCost = 0;
            for (Ipp32s j = y; j < y + y_num; j++) {
                for (Ipp32s i = x; i < x + x_num; i++) {
                    //mfxFEIH265IntraDist *intraDistLine = feiOut->IntraDist + j*feiOut->IntraPitch;
                    //cmIntraCost += intraDistLine[i].Dist;

                    mfxU32 cmInterCost16x16 = IPP_MAX_32U;
                    for (Ipp8s refIdx = 0; refIdx < m_cslice->num_ref_idx[refList]; refIdx++) {

                        Ipp8s feiRefIdx = refIdx;

                        // use m_mapRefIdxL1ToL0 if L0 ref is in L1 (e.g. in case of GPB) 
                        if (refList == 1) {
                            Ipp32s idx0 = m_currFrame->m_mapRefIdxL1ToL0[refIdx];
                            if (idx0 >= 0) {
                                continue;   // this cost was already checked in L0 
                            } else {
                                // refIdxes from L0 and L1 are in common array 
                                feiRefIdx += m_cslice->num_ref_idx[0];
                            }
                        }

                        mfxU32 *dist16x16line = feiOut->Dist[feiRefIdx][MFX_FEI_H265_BLK_16x16] + (j        ) * feiOut->PitchDist[MFX_FEI_H265_BLK_16x16];

                        mfxU32 *dist8x8line0  = feiOut->Dist[feiRefIdx][MFX_FEI_H265_BLK_8x8]   + (j * 2    ) * feiOut->PitchDist[MFX_FEI_H265_BLK_8x8];
                        mfxU32 *dist8x8line1  = feiOut->Dist[feiRefIdx][MFX_FEI_H265_BLK_8x8]   + (j * 2 + 1) * feiOut->PitchDist[MFX_FEI_H265_BLK_8x8];

                        Ipp32u dist16x16 = dist16x16line[i];

                        Ipp32u dist8x8 = dist8x8line0[i * 2] + dist8x8line0[i * 2 + 1] +
                                         dist8x8line1[i * 2] + dist8x8line1[i * 2 + 1];

                        if (dist16x16 < cmInterCost16x16)
                            cmInterCost16x16 = dist16x16; 

                        if (dist8x8 < cmInterCost16x16)
                            cmInterCost16x16 = dist8x8; 
                    }
                    cmInterCost += cmInterCost16x16;
                }
            }
            if (cmInterCost < cmInterCostMin)
                cmInterCostMin = cmInterCost;
        }

        tryIntra = cmIntraCost < (m_par->cmIntraThreshold / 256.0) * cmInterCostMin;
    }
#endif // MFX_VA

    return tryIntra;
}

template <typename PixType>
bool H265CU<PixType>::EncAndRecLuma(Ipp32s absPartIdx, Ipp32s offset, Ipp8u depth, CostType *cost) {
    bool interMod = false;
    Ipp32u lpel_x = m_ctbPelX + ((h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u tpel_y = m_ctbPelY + ((h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize);
    if (lpel_x >= m_par->Width || tpel_y >= m_par->Height)
        return interMod;

    Ipp32s depthMax = m_data[absPartIdx].depth + m_data[absPartIdx].trIdx;
    Ipp32u numParts = (m_par->NumPartInCU >> (depth << 1));
    //Ipp32s width = m_data[absPartIdx].size >> m_data[absPartIdx].trIdx;
    
    VM_ASSERT(depth <= depthMax);
#ifdef AMT_ALT_ENCODE
    if (depth == m_data[absPartIdx].depth && m_data[absPartIdx].predMode == MODE_INTER) {
        m_interPredY = m_interPredSavedY[depth][absPartIdx];
        m_interResidY = m_interResidSavedY[depth][absPartIdx];
        if(m_par->AnalyseFlags & HEVC_COST_CHROMA) {
            m_interPredC = m_interPredSavedC[depth][absPartIdx];
            m_interResidU = m_interResidSavedU[depth][absPartIdx];
            m_interResidV = m_interResidSavedV[depth][absPartIdx];
        }

#if defined(AMT_ADAPTIVE_INTER_DEPTH)
        if(depth<2 && m_data[absPartIdx].cbf[0] && m_data[absPartIdx].trIdx==0 &&
            m_par->QuadtreeTUMaxDepthInter>m_par->QuadtreeTUMaxDepthInterRD) {
            Ipp8u trDepthMin, trDepthMax;
            GetTrDepthMinMax(m_par, depth, m_data[absPartIdx].partSize, &trDepthMin, &trDepthMax);
            bool trySplit = false;
            if(trDepthMin==0 && trDepthMax>trDepthMin) {
                //m_STC = GetSpatioTemporalComplexity(absPartIdx, depth, 0, depth, m_SCid);
                if(m_STC[depth][absPartIdx]>0) trySplit=true;                                     // Recreate internal condition
            }
            if(trySplit) {
                Ipp8u cbfAcc[5] = {m_data[absPartIdx].cbf[0],m_data[absPartIdx].cbf[1],m_data[absPartIdx].cbf[2]};
                if (m_par->chroma422) {
                    Ipp32s idx422 = numParts >> 1;
                    cbfAcc[3] = m_data[absPartIdx + idx422].cbf[1];
                    cbfAcc[4] = m_data[absPartIdx + idx422].cbf[2];
                }
                CostType costTemp;
                TuGetSplitInter(absPartIdx, trDepthMin, trDepthMax, &costTemp); // does luma and chroma
                depthMax = m_data[absPartIdx].depth + m_data[absPartIdx].trIdx;
                Ipp32s widthY = m_par->MaxCUSize >> depth;
                IppiSize roiY = { widthY, widthY };
                PixType *dstY = m_yRec  + GetLumaOffset(m_par, absPartIdx, m_pitchRecLuma);
                PixType *srcY = m_interRecWorkY + GetLumaOffset(m_par, absPartIdx, MAX_CU_SIZE);
                _ippiCopy_C1R(srcY, MAX_CU_SIZE, dstY, m_pitchRecLuma, roiY);      // block of reconstructed Y pels
                if(m_par->AnalyseFlags & HEVC_COST_CHROMA) {
                    Ipp32s widthC = widthY >> m_par->chromaShiftW;
                    Ipp32s heightC = widthY >> m_par->chromaShiftH;
                    IppiSize roiC = { 2 * widthC, heightC };
                    PixType *dstC = m_uvRec + GetChromaOffset(m_par, absPartIdx, m_pitchRecChroma);
                    Ipp32s pitchSrcC = MAX_CU_SIZE << m_par->chromaShiftWInv;
                    PixType *srcC = m_interRecWorkC + GetChromaOffset(m_par, absPartIdx, pitchSrcC);
                    _ippiCopy_C1R(srcC, pitchSrcC, dstC, m_pitchRecChroma, roiC);  // block of reconstructed UV pels
                }
                if(depth!=depthMax) interMod=true;
                else if(!(m_par->AnalyseFlags & HEVC_COST_CHROMA)) {
                    m_data[absPartIdx].cbf[1] = cbfAcc[1];
                    m_data[absPartIdx].cbf[2] = cbfAcc[2];
                    if (m_par->chroma422) {
                        Ipp32s idx422 = numParts >> 1;
                        m_data[absPartIdx + idx422].cbf[1] = cbfAcc[3];
                        m_data[absPartIdx + idx422].cbf[2] = cbfAcc[4];
                    }
                }
                return interMod;
            }
        }
#endif
    }
#endif
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    if (depth == m_data[absPartIdx].depth && m_data[absPartIdx].predMode == MODE_INTRA && m_data[absPartIdx].cbf[0]) {
        if(!IsTuSplitIntra() && m_par->QuadtreeTUMaxDepthIntra>1 && m_data[absPartIdx].partSize==PART_SIZE_2Nx2N) {
            Ipp8u splitMode = GetTrSplitMode(absPartIdx, depth, 0, m_data[absPartIdx].partSize);
            if(splitMode==SPLIT_TRY) {
                m_costStored[depth + 1] = COST_MAX;
                CalcCostLuma(absPartIdx, depth, 0, COST_REC_TR_ALL, INTRA_PRED_CALC);
                depthMax = m_data[absPartIdx].depth + m_data[absPartIdx].trIdx;
                return interMod;
            }
        }
    }
#endif
    if (depth == depthMax) {
        if (m_data[absPartIdx].predMode == MODE_INTRA || m_data[absPartIdx].cbf[0])
        {
            Ipp32s width = m_data[absPartIdx].size >> m_data[absPartIdx].trIdx;
            EncAndRecLumaTu(absPartIdx, offset, width, cost, 0, INTRA_PRED_CALC);
            FillSubPartCbfY_(m_data + absPartIdx + 1, numParts - 1, m_data[absPartIdx].cbf[0]);
        }
    }
    else {
        //Ipp32s subsize = width << (depthMax - depth) >> 1;
        //subsize *= subsize;
        numParts >>= 2;
        if (cost) *cost = 0;
         Ipp32s nz = 0;
        for (Ipp32u i = 0; i < 4; i++) {
            CostType cost_temp = COST_MAX;
            interMod |= EncAndRecLuma(absPartIdx + numParts * i, (absPartIdx + numParts * i)*16, depth+1, cost ? &cost_temp : 0);
            if (cost) *cost += cost_temp;
            //offset += subsize;
            nz |= m_data[absPartIdx + numParts * i].cbf[0];
        }
        if (nz)
            for (Ipp32u i = 0; i < 4; i++)
                SetCbfBit<0>(m_data + absPartIdx + (numParts * i >> 2), depth - m_data[absPartIdx].depth);
        /*
        if (m_data[absPartIdx].cbf[0] && depth >= m_data[absPartIdx].depth)
            for (Ipp32u i = 0; i < 4; i++)
                SetCbfOne(absPartIdx + numParts * i, TEXT_LUMA, depth - m_data[absPartIdx].depth);
                */
    }
    return interMod;
}

template <typename PixType>
void H265CU<PixType>::EncAndRecChromaUpdate(Ipp32s absPartIdx, Ipp32s offset, Ipp8u depth, bool interMod) {
    Ipp32u lPel = m_ctbPelX + ((h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u tPel = m_ctbPelY + ((h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize);
    if (lPel >= m_par->Width || tPel >= m_par->Height)
        return;

    H265CUData *data = m_data + absPartIdx;
    Ipp32s depthMax = data->depth + data->trIdx;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) )>>2;
    Ipp32s width = data->size >> data->trIdx << (depthMax - depth);
    Ipp32s cuWidth = width;
    if (m_par->chromaFormatIdc != MFX_CHROMAFORMAT_YUV444)
        width >>= 1;

    if (depth == data->depth && data->predMode == MODE_INTER) {
        if(!interMod || (m_par->AnalyseFlags & HEVC_COST_CHROMA)) return;
        m_interPredC = m_interPredSavedC[depth][absPartIdx];
        m_interResidU = m_interResidSavedU[depth][absPartIdx];
        m_interResidV = m_interResidSavedV[depth][absPartIdx];
    }

#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    if (depth == data->depth && data->predMode == MODE_INTRA && !HaveChromaRec())
    {
        Ipp8u allowedChromaDir[NUM_CHROMA_MODE];
        GetAllowedChromaDir(absPartIdx, allowedChromaDir);
        Ipp8u trDepthMin=GetTrDepthMinIntra(m_par, depth);
        Ipp32s tuwidth = width >> trDepthMin;
        Ipp32u numTrParts = (m_par->NumPartInCU >> ((depth + trDepthMin) << 1));
        // need to save initial state
        CostType bestCost   = INT_MAX;
        Ipp8u bestMode = 0;
        CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
        m_bsf->CtxSave(ctxInitial);
        // ignoring tr depth
        for (Ipp8u i = 0; i < NUM_CHROMA_MODE; i++) {
            CostType chromaCost = 0;
            Ipp8u chromaDir = allowedChromaDir[i];
            FillSubPartIntraPredModeC_(m_data + absPartIdx, numParts<<2, chromaDir);
            for (Ipp32u pos = 0; pos < numParts<<2; pos += numTrParts) {
                Ipp32s absPartIdx422 = 0;
                for (Ipp32s idx422 = 0; idx422 <= m_par->chroma422; idx422++) {
                    CostType cost_temp;
                    EncAndRecChromaTu(absPartIdx+pos, absPartIdx422, ((absPartIdx + pos) * 16) >> m_par->chromaShift, tuwidth, &cost_temp, INTRA_PRED_CALC, 1);
                    chromaCost += cost_temp;
                    absPartIdx422 += (numTrParts>>1);
                }
            }
            chromaCost += GetIntraChromaModeCost(absPartIdx);
            if(chromaCost<bestCost) {
                bestCost = chromaCost;
                bestMode = i;
            }
        }
        m_bsf->CtxRestore(ctxInitial);
        FillSubPartIntraPredModeC_(m_data + absPartIdx, numParts<<2, allowedChromaDir[bestMode]);
    }
#endif

    if (depth == depthMax || width == 4) {
        Ipp32s absPartIdx422 = 0;

        for (Ipp32s idx422 = 0; idx422 <= m_par->chroma422; idx422++) {
            Ipp8u setBitFlag = depth != depthMax && data->trIdx > 0 ? 1 : 0;
            EncAndRecChromaTu(absPartIdx, absPartIdx422, offset, width, NULL, INTRA_PRED_CALC, 0);
            PropagateChromaCbf(data + absPartIdx422, numParts, (2-m_par->chroma422) << 1,
                setBitFlag ? data->trIdx - 1 : data->trIdx, setBitFlag);
            absPartIdx422 += numParts * 2;
        }
    }
    else {
        Ipp32s subsize = width >> 1;
        subsize *= subsize;
        if (m_par->chroma422)
            subsize <<= 1;

        Ipp8u cbf[2] = {};
        for (Ipp32u i = 0; i < 4; i++) {
            EncAndRecChromaUpdate(absPartIdx + numParts * i, offset, depth + 1, interMod);
            offset += subsize;

            cbf[0] |= data[numParts * i].cbf[1];
            cbf[1] |= data[numParts * i].cbf[2];
            if (m_par->chroma422) {
                cbf[0] |= data[numParts * i + (numParts >> 1)].cbf[1];
                cbf[1] |= data[numParts * i + (numParts >> 1)].cbf[2];
            }
        }
        if (depth >= data->depth) {

            if (cbf[0])
                for(Ipp32u i = 0; i < 4; i++)
                   SetCbfBit<1>(data + numParts * i, depth - data->depth);
            if (cbf[1])
                for(Ipp32u i = 0; i < 4; i++)
                   SetCbfBit<2>(data + numParts * i, depth - data->depth);
        }
    }
}

template <typename PixType>
void H265CU<PixType>::EncAndRecChroma(Ipp32s absPartIdx, Ipp32s offset, Ipp8u depth, CostType *cost, IntraPredOpt pred_opt) {
    H265CUData *data = m_data + absPartIdx;
    Ipp32s depthMax = data->depth + data->trIdx;
    Ipp32u numParts = ( m_par->NumPartInCU >> (depth<<1) )>>2;
    Ipp32s width = data->size >> data->trIdx << (depthMax - depth);
    Ipp32s cuWidth = width;
    if (m_par->chromaFormatIdc != MFX_CHROMAFORMAT_YUV444)
        width >>= 1;    

    Ipp32u lPel = m_ctbPelX + ((h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u tPel = m_ctbPelY + ((h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize);
    if (lPel >= m_par->Width || tPel >= m_par->Height)
        return;
    
    if (depth == data->depth && data->predMode == MODE_INTER) {
        // predict chroma
        InterPredCu<TEXT_CHROMA>(absPartIdx, depth, m_interPredC, MAX_CU_SIZE << m_par->chromaShiftWInv);

        // calc residual chroma
        const PixType *srcC = m_uvSrc + GetChromaOffset(m_par, absPartIdx, m_pitchSrcChroma);
        const PixType *predC = m_interPredC + GetChromaOffset(m_par, absPartIdx, MAX_CU_SIZE << m_par->chromaShiftWInv);
        CoeffsType *residU = m_interResidU + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
        CoeffsType *residV = m_interResidV + GetChromaOffset1(m_par, absPartIdx, MAX_CU_SIZE >> m_par->chromaShiftW);
        h265_DiffNv12(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv,
                      residU, MAX_CU_SIZE >> m_par->chromaShiftW, residV, MAX_CU_SIZE >> m_par->chromaShiftW,
                      width, width * 2 >> m_par->chromaShiftH);
#ifdef AMT_FIX_CHROMA_SKIP
        Ipp8u trDepthMin, trDepthMax;
        GetTrDepthMinMax(m_par, depth, data->partSize, &trDepthMin, &trDepthMax);
        if(data->flags.skippedFlag && !(m_par->AnalyseFlags & HEVC_COST_CHROMA) && trDepthMin==0) {
            CostType skipCost=0, mergeCost=0, lumaMergeCost=0;
            CABAC_CONTEXT_H265 *ctx0 = CTX(m_bsf,QT_CBF_HEVC) + GetCtxQtCbfLuma(0);
            Ipp32s code0 = 0;
            Ipp32s bits = tab_cabacPBits[(*ctx0) ^ (code0 << 6)];
            // Sub Opt to promote chroma (if not intra chroma rdo)
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
            CABAC_CONTEXT_H265 *ctx1 = CTX(m_bsf,PRED_MODE_HEVC);
            Ipp32s code1 = 0;
            bits += tab_cabacPBits[(*ctx1) ^ (code1 << 6)];
            CABAC_CONTEXT_H265 *ctx2 = CTX(m_bsf,PART_SIZE_HEVC);
            Ipp32s code2 = 1;
            bits += tab_cabacPBits[(*ctx2) ^ (code2 << 6)];
            CABAC_CONTEXT_H265 *ctx3 = CTX(m_bsf,MERGE_FLAG_HEVC);
            Ipp32s code3 = 1;
            bits += tab_cabacPBits[(*ctx3) ^ (code3 << 6)];
#endif            
            bits>>= (8 - BIT_COST_SHIFT);
            lumaMergeCost += (bits*m_rdLambda);

            skipCost = TuSse(srcC, m_pitchSrcChroma, predC, MAX_CU_SIZE << m_par->chromaShiftWInv, 
                            cuWidth << m_par->chromaShiftWInv, cuWidth >> m_par->chromaShiftH, (m_par->bitDepthChromaShift << 1));
            //kolya //WEIGHTED_CHROMA_DISTORTION (JCTVC-F386)
            if (m_par->IntraChromaRDO)
                skipCost *= m_ChromaDistWeight;

            if(skipCost>lumaMergeCost) {
                ////////////////////////////
                data->flags.skippedFlag = 0;
                Ipp32s absPartIdx422 = 0;
                Ipp8u cbf = 0;
                for (Ipp32s idx422 = 0; idx422 <= m_par->chroma422; idx422++) {
                    CostType cost_temp;
                    EncAndRecChromaTu(absPartIdx, absPartIdx422, offset, width, &cost_temp, pred_opt, 0);
                    cbf |= m_data[absPartIdx+absPartIdx422].cbf[1];
                    cbf |= m_data[absPartIdx+absPartIdx422].cbf[2];
                    mergeCost += cost_temp;
                    absPartIdx422 += numParts * 2;
                }
                data->flags.skippedFlag = 1;
                ////////////////////////////
                if(cbf) {
#ifndef AMT_ADAPTIVE_INTRA_DEPTH
                    PixType *recC = m_uvRec + GetChromaOffset(m_par, absPartIdx, m_pitchRecChroma);
                    mergeCost += TuSse(srcC, m_pitchSrcChroma, recC, m_pitchRecChroma, 
                                        cuWidth << m_par->chromaShiftWInv, cuWidth >> m_par->chromaShiftH, 
                                        (m_par->bitDepthChromaShift << 1));
#endif
                    if((mergeCost+lumaMergeCost)<skipCost) {
                        data->flags.skippedFlag = 0;
                        if(depth == depthMax) {             // Weird!!
                            Ipp32s absPartIdx422 = 0;
                            *ctx0 = tab_cabacTransTbl[code0][*ctx0];
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
                            *ctx1 = tab_cabacTransTbl[code1][*ctx1];
                            *ctx2 = tab_cabacTransTbl[code2][*ctx2];
                            *ctx3 = tab_cabacTransTbl[code3][*ctx3];
#endif
                            for (Ipp32s idx422 = 0; idx422 <= m_par->chroma422; idx422++) {
                                CostType cost_temp;
                                Ipp8u setBitFlag = depth != depthMax && data->trIdx > 0 ? 1 : 0;
                                PropagateChromaCbf(data + absPartIdx422, numParts, (2-m_par->chroma422) << 1,
                                    setBitFlag ? data->trIdx - 1 : data->trIdx, setBitFlag);
                                absPartIdx422 += numParts * 2;
                            }
                            if (cost) *cost = mergeCost;
                            return;
                        }
                    }
                }
            }
        }
#endif
    }
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
#ifdef AMT_CHROMA_GUIDED_INTER
    if (depth == data->depth && data->predMode == MODE_INTRA && !HaveChromaRec() && pred_opt==INTRA_PRED_CALC) 
#else
    if (depth == data->depth && data->predMode == MODE_INTRA && !HaveChromaRec()) 
#endif
    {
        Ipp8u allowedChromaDir[NUM_CHROMA_MODE];
        GetAllowedChromaDir(absPartIdx, allowedChromaDir);
        Ipp8u trDepthMin=GetTrDepthMinIntra(m_par, depth);
        Ipp32s tuwidth = width >> trDepthMin;
        Ipp32u numTrParts = (m_par->NumPartInCU >> ((depth + trDepthMin) << 1));
        // need to save initial state
        CostType bestCost   = INT_MAX;
        Ipp8u bestMode = 0;
        CABAC_CONTEXT_H265 ctxInitial[NUM_CABAC_CONTEXT];
        m_bsf->CtxSave(ctxInitial);
        // ignoring tr depth
        for (Ipp8u i = 0; i < NUM_CHROMA_MODE; i++) {
            CostType chromaCost = 0;
            Ipp8u chromaDir = allowedChromaDir[i];
            FillSubPartIntraPredModeC_(m_data + absPartIdx, numParts<<2, chromaDir);
            for (Ipp32u pos = 0; pos < numParts<<2; pos += numTrParts) {
                Ipp32s absPartIdx422 = 0;
                for (Ipp32s idx422 = 0; idx422 <= m_par->chroma422; idx422++) {
                    CostType cost_temp;
                    EncAndRecChromaTu(absPartIdx+pos, absPartIdx422, ((absPartIdx + pos) * 16) >> m_par->chromaShift, tuwidth, &cost_temp, INTRA_PRED_CALC, 1);
                    chromaCost += cost_temp;
                    absPartIdx422 += (numTrParts >>1);
                }
            }
            chromaCost += GetIntraChromaModeCost(absPartIdx);
            if(chromaCost<bestCost) {
                bestCost = chromaCost;
                bestMode = i;
            }
        }
        m_bsf->CtxRestore(ctxInitial);
        FillSubPartIntraPredModeC_(m_data + absPartIdx, numParts<<2, allowedChromaDir[bestMode]);
    }
#endif
    if (depth == depthMax || width == 4) {
        Ipp32s absPartIdx422 = 0;
        if (cost) *cost = 0;

        for (Ipp32s idx422 = 0; idx422 <= m_par->chroma422; idx422++) {
            CostType cost_temp;
            Ipp8u setBitFlag = depth != depthMax && data->trIdx > 0 ? 1 : 0;
            EncAndRecChromaTu(absPartIdx, absPartIdx422, offset, width, cost ? &cost_temp : NULL, pred_opt, 0);
            PropagateChromaCbf(data + absPartIdx422, numParts, (2-m_par->chroma422) << 1,
                setBitFlag ? data->trIdx - 1 : data->trIdx, setBitFlag);
            if (cost) *cost += cost_temp;
            absPartIdx422 += numParts * 2;
        }
    }
    else {
        Ipp32s subsize = width >> 1;
        subsize *= subsize;
        if (m_par->chroma422)
            subsize <<= 1;

        if (cost) *cost = 0;
        Ipp8u cbf[2] = {};
        for (Ipp32u i = 0; i < 4; i++) {
            CostType cost_temp;
            EncAndRecChroma(absPartIdx + numParts * i, offset, depth + 1, cost ? &cost_temp : 0, pred_opt);
            if (cost) *cost += cost_temp;
            offset += subsize;

            cbf[0] |= data[numParts * i].cbf[1];
            cbf[1] |= data[numParts * i].cbf[2];
            if (m_par->chroma422) {
                cbf[0] |= data[numParts * i + (numParts >> 1)].cbf[1];
                cbf[1] |= data[numParts * i + (numParts >> 1)].cbf[2];
            }
        }
        if (depth >= data->depth) {

            if (cbf[0])
                for(Ipp32u i = 0; i < 4; i++)
                   SetCbfBit<1>(data + numParts * i, depth - data->depth);
            if (cbf[1])
                for(Ipp32u i = 0; i < 4; i++)
                   SetCbfBit<2>(data + numParts * i, depth - data->depth);
        }
    }
}

template <typename PixType>
static void tuAddClipNv12(PixType *dstNv12, Ipp32s pitchDst, 
                          const PixType *src1Nv12, Ipp32s pitchSrc1, 
                          const CoeffsType *src2Yv12, Ipp32s pitchSrc2,
                          Ipp32s size, Ipp32s bitDepth)
{
    Ipp32s maxval = (1 << bitDepth) - 1;
    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            dstNv12[2*i] = (PixType)Saturate(0, maxval, src1Nv12[2*i] + src2Yv12[i]);
        }
        dstNv12  += pitchDst;
        src1Nv12 += pitchSrc1;
        src2Yv12 += pitchSrc2;
    }
}

template <typename PixType>
static void tuAddClipNv12UV(PixType *dstNv12, Ipp32s pitchDst, 
                          const PixType *src1Nv12, Ipp32s pitchSrc1, 
                          const CoeffsType *src2Yv12U, const CoeffsType *src2Yv12V, Ipp32s pitchSrc2,
                          Ipp32s size, Ipp32s bitDepth)
{
    if(sizeof(PixType)==1) {
        MFX_HEVC_PP::NAME(h265_AddClipNv12UV_8u)((Ipp8u*)dstNv12, pitchDst, (Ipp8u*)src1Nv12, pitchSrc1, src2Yv12U, src2Yv12V, size, size);
    } else {
        tuAddClipNv12(dstNv12, pitchDst, src1Nv12, pitchSrc1, src2Yv12U, pitchSrc2, size, bitDepth);
        tuAddClipNv12(dstNv12+1, pitchDst, src1Nv12+1, pitchSrc1, src2Yv12V, pitchSrc2, size, bitDepth);
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

template <typename PixType>
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

template <typename PixType>
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

template <typename PixType>
static void TuDiff(CoeffsType *residual, Ipp32s pitchDst,
                   const PixType *src, Ipp32s pitchSrc,
                   const PixType *pred, Ipp32s pitchPred,
                   Ipp32s size)
{
    for (Ipp32s j = 0; j < size; j++) {
        for (Ipp32s i = 0; i < size; i++) {
            residual[i] = (CoeffsType)src[i] - pred[i];
        }
        residual += pitchDst;
        src += pitchSrc;
        pred += pitchPred;
    }
}

template <typename PixType>
Ipp32s tuHad_Kernel(const PixType *src, Ipp32s pitchSrc, const PixType *rec, Ipp32s pitchRec,
             Ipp32s width, Ipp32s height)
{
    Ipp32u satdTotal = 0;
    Ipp32s satd[2] = {0, 0};

    /* assume height and width are multiple of 4 */
    VM_ASSERT(!(width & 0x03) && !(height & 0x03));

    /* test with optimized SATD source code */
    if (width == 4 && height == 4) {
        /* single 4x4 block */
        if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_16u)((const Ipp16u *)src, pitchSrc, (const Ipp16u *)rec, pitchRec);
        else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_8u) ((const Ipp8u  *)src, pitchSrc, (const Ipp8u  *)rec, pitchRec);
        satdTotal += (satd[0] + 1) >> 1;
    } else if ( (height | width) & 0x07 ) {
        /* multiple 4x4 blocks - do as many pairs as possible */
        Ipp32s widthPair = width & ~0x07;
        Ipp32s widthRem = width - widthPair;
        for (Ipp32s j = 0; j < height; j += 4, src += pitchSrc * 4, rec += pitchRec * 4) {
            Ipp32s i = 0;
            for (; i < widthPair; i += 4*2) {
                if (sizeof(PixType) == 2)   MFX_HEVC_PP::NAME(h265_SATD_4x4_Pair_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec, satd);
                else                        MFX_HEVC_PP::NAME(h265_SATD_4x4_Pair_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec, satd);
                satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
            }

            if (widthRem) {
                if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec);
                else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_4x4_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec);
                satdTotal += (satd[0] + 1) >> 1;
            }
        }
    } 
    else if (width == 8 && height == 8) {
        /* single 8x8 block */
        if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)src, pitchSrc, (const Ipp16u *)rec, pitchRec);
        else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u) ((const Ipp8u  *)src, pitchSrc, (const Ipp8u  *)rec, pitchRec);
        satdTotal += (satd[0] + 2) >> 2;
    } else {
        /* multiple 8x8 blocks - do as many pairs as possible */
        Ipp32s widthPair = width & ~0x0f;
        Ipp32s widthRem = width - widthPair;
        for (Ipp32s j = 0; j < height; j += 8, src += pitchSrc * 8, rec += pitchRec * 8) {
            Ipp32s i = 0;
            for (; i < widthPair; i += 8*2) {
                if (sizeof(PixType) == 2)   MFX_HEVC_PP::NAME(h265_SATD_8x8_Pair_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec, satd);
                else                        MFX_HEVC_PP::NAME(h265_SATD_8x8_Pair_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec, satd);
                satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
            }

            if (widthRem) {
                if (sizeof(PixType) == 2)   satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)src + i, pitchSrc, (const Ipp16u *)rec + i, pitchRec);
                else                        satd[0] = MFX_HEVC_PP::NAME(h265_SATD_8x8_8u) ((const Ipp8u  *)src + i, pitchSrc, (const Ipp8u  *)rec + i, pitchRec);
                satdTotal += (satd[0] + 2) >> 2;
            }
        }
    }

    return satdTotal;
}

Ipp32s tuHad(const Ipp8u *src, Ipp32s pitchSrc, const Ipp8u *rec, Ipp32s pitchRec, Ipp32s width, Ipp32s height)
{
    return tuHad_Kernel<Ipp8u>(src, pitchSrc, rec, pitchRec, width, height);

}

Ipp32s tuHad(const Ipp16u *src, Ipp32s pitchSrc, const Ipp16u *rec, Ipp32s pitchRec, Ipp32s width, Ipp32s height)
{
    return tuHad_Kernel<Ipp16u>(src, pitchSrc, rec, pitchRec, width, height);
}


template <typename PixType>
void H265CU<PixType>::EncAndRecLumaTu(Ipp32s absPartIdx, Ipp32s offset, Ipp32s width, CostType *cost,
                                      Ipp8u costPredFlag, IntraPredOpt predOpt)
{
    CostType cost_pred;
#ifndef AMT_ALT_ENCODE_OPT
    CostType cost_rdoq;
#endif
    Ipp32u lpel_x = m_ctbPelX + ((h265_scan_z2r4[absPartIdx] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u tpel_y = m_ctbPelY + ((h265_scan_z2r4[absPartIdx] >> 4) << m_par->QuadtreeTULog2MinSize);
    H265CUData *dataTu = m_data + absPartIdx;
    Ipp8u isIntra = (dataTu->predMode == MODE_INTRA);

    if (cost)
        *cost = 0;
#ifndef AMT_ALT_ENCODE_OPT
    else if (m_isRdoq)
        cost = &cost_rdoq;
#endif
    if (lpel_x >= m_par->Width || tpel_y >= m_par->Height)
        return;

    Ipp32s PURasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s PUStartRow = PURasterIdx >> 4;
    Ipp32s PUStartColumn = PURasterIdx & 15;

    PixType *rec;
    Ipp32s pitch_rec;
    PixType *src = m_ySrc + ((PUStartRow * m_pitchSrcLuma + PUStartColumn) << m_par->QuadtreeTULog2MinSize);

    PixType *pred = NULL;
    Ipp32s  pitch_pred = 0;
    Ipp32s  is_pred_transposed = 0;
    const Ipp8u depth = dataTu->depth;
    const Ipp32s intraLumaDir = dataTu->intraLumaDir;
    if (isIntra) {
        rec = m_yRec + ((PUStartRow * m_pitchRecLuma + PUStartColumn) << m_par->QuadtreeTULog2MinSize);
        pitch_rec = m_pitchRecLuma;
        if (predOpt == INTRA_PRED_IN_BUF) {
            pred = m_predIntraAll + intraLumaDir * width * width;
            pitch_pred = width;
            is_pred_transposed = (intraLumaDir >= 2 && intraLumaDir < 18);
            if (m_cuIntraAngMode == INTRA_ANG_MODE_GRADIENT && intraLumaDir == 10)
                is_pred_transposed = 0;
        }
        else {
            pred = rec;
            pitch_pred = m_pitchRecLuma;
        }
    }
    else {
        pitch_pred = MAX_CU_SIZE;
        pred = m_interPredY;
        pred += (PUStartRow * pitch_pred + PUStartColumn) << m_par->QuadtreeTULog2MinSize;
        if (predOpt == INTER_PRED_IN_BUF) {
            rec = m_interRecWorkY;
            rec += (PUStartRow * MAX_CU_SIZE + PUStartColumn) << m_par->QuadtreeTULog2MinSize;
            pitch_rec = MAX_CU_SIZE;
        }
        else {
            rec = m_yRec + ((PUStartRow * m_pitchRecLuma + PUStartColumn) << m_par->QuadtreeTULog2MinSize);
            pitch_rec = m_pitchRecLuma;
        }
    }

    if (isIntra && predOpt == INTRA_PRED_CALC) {
        IntraPredTu(absPartIdx, 0, width, intraLumaDir, 1);
    }

    if (cost && costPredFlag) {
        cost_pred = tuHad(src, m_pitchSrcLuma, rec, pitch_rec, width, width) >> m_par->bitDepthLumaShift;
        *cost = cost_pred;
        return;
    }

    IppiSize roi = { width, width };
    Ipp8u cbf = 0;
    if (!dataTu->flags.skippedFlag) {
        if (isIntra) {
            if (is_pred_transposed) {
                _ippiTranspose_C1R(pred, pitch_pred, rec, pitch_rec, roi);
                TuDiff(m_residualsY+offset, width, src, m_pitchSrcLuma, rec, pitch_rec, width);
            } else {
                TuDiff(m_residualsY+offset, width, src, m_pitchSrcLuma, pred, pitch_pred, width);
            }
            TransformFwd(m_residualsY+offset, width, m_residualsY+offset, width, m_par->bitDepthLuma, isIntra);
        }
        else {
            Ipp16s *resid = m_interResidY;
            resid += (PUStartRow * MAX_CU_SIZE + PUStartColumn) << m_par->QuadtreeTULog2MinSize;
            //ippiCopy_16s_C1R(resid, MAX_CU_SIZE * sizeof(CoeffsType), m_residualsY + offset, width * sizeof(CoeffsType), roi);
            TransformFwd(resid, MAX_CU_SIZE, m_residualsY+offset, width, m_par->bitDepthLuma, isIntra);
        }

        QuantFwdTu(m_residualsY+offset, m_coeffWorkY+offset, absPartIdx, width, dataTu->qp, 1, isIntra);

        cbf = !IsZero(m_coeffWorkY+offset, width * width);

        if (cbf) {
            QuantInvTu(m_coeffWorkY+offset, m_residualsY+offset, width, dataTu->qp, 1);
            if (is_pred_transposed)
                TransformInv(rec, pitch_rec, m_residualsY+offset, width, rec, pitch_rec, 1, isIntra, m_par->bitDepthLuma);
            else
                TransformInv(pred, pitch_pred, m_residualsY+offset, width, rec, pitch_rec, 1, isIntra, m_par->bitDepthLuma);
        } else if (!is_pred_transposed)
            _ippiCopy_C1R(pred, pitch_pred, rec, pitch_rec, roi);
    }

    if (cost)
        *cost = TuSse(src, m_pitchSrcLuma, rec, pitch_rec, width, width, (m_par->bitDepthLumaShift << 1));

    if (dataTu->flags.skippedFlag)
        return;

    if (m_isRdoq || cost) {
        Ipp8u code_dqp = getdQPFlag();

        m_bsf->Reset();
        if (cbf) {
            SetCbf<0>(dataTu, dataTu->trIdx);
            if (cost)
                PutTransform(m_bsf, offset, offset >> 2, absPartIdx, absPartIdx, depth + dataTu->trIdx, 
                             width, dataTu->trIdx, code_dqp);
        }
        else {
            ResetCbf<0>(dataTu);
            if (cost)
                m_bsf->EncodeSingleBin_CABAC(CTX(m_bsf,QT_CBF_HEVC) + GetCtxQtCbfLuma(dataTu->trIdx), 0);
        }

        if(cost) *cost += BIT_COST(m_bsf->GetNumBits());
    } else {
        if (cbf) {
            SetCbf<0>(dataTu, dataTu->trIdx);
        } else {
            ResetCbf<0>(dataTu);
        }
    }
}


template <typename PixType>
void H265CU<PixType>::EncAndRecChromaTu(Ipp32s absPartIdx, Ipp32s idx422, Ipp32s offset, Ipp32s width, CostType *cost, IntraPredOpt pred_opt, Ipp8u costPredFlag)
{
    if (cost) *cost = 0;

    Ipp32s PURasterIdx = h265_scan_z2r4[absPartIdx + idx422];
    Ipp32s PUStartRow = PURasterIdx >> 4;
    if (idx422) {
        offset += width * width;
    }
    Ipp32s PUStartColumn = PURasterIdx & 15;
    PixType *pPred;
    Ipp32s pitchPred;
    PixType *pSrc = m_uvSrc + (((PUStartRow * m_pitchSrcChroma >> m_par->chromaShiftH) + (PUStartColumn << m_par->chromaShiftWInv)) << m_par->QuadtreeTULog2MinSize);
    PixType *pRec;
    Ipp32s pitchRec;
    H265CUData *data = m_data + absPartIdx;
    Ipp8u depth = data->depth;
    Ipp32s isIntra = (data->predMode == MODE_INTRA);

    if (isIntra) {
        Ipp8u intra_pred_mode = data->intraChromaDir;
        if (intra_pred_mode == INTRA_DM_CHROMA) {
            Ipp32s mask = (m_par->NumPartInCU >> (depth << 1)) - 1;
            Ipp32s absPartIdx_0 = absPartIdx & ~mask;
            intra_pred_mode = m_data[absPartIdx_0].intraLumaDir;
        }

        IntraPredTu(absPartIdx, idx422, width, intra_pred_mode, 0);

        pRec = m_uvRec + (((PUStartRow * m_pitchRecChroma >> m_par->chromaShiftH) + (PUStartColumn << m_par->chromaShiftWInv)) << m_par->QuadtreeTULog2MinSize);
        pitchRec = m_pitchRecChroma;
        pPred = pRec;
        pitchPred = m_pitchRecChroma;
    }
    else {
        if (pred_opt == INTER_PRED_IN_BUF) {
            pitchPred = MAX_CU_SIZE << m_par->chromaShiftWInv;
            pPred = m_interPredC + (((PUStartRow * pitchPred >> m_par->chromaShiftH) + (PUStartColumn << m_par->chromaShiftWInv)) << m_par->QuadtreeTULog2MinSize);
            pitchRec = MAX_CU_SIZE << m_par->chromaShiftWInv;
            pRec = m_interRecWorkC + (((PUStartRow * pitchRec >> m_par->chromaShiftH) + (PUStartColumn << m_par->chromaShiftWInv)) << m_par->QuadtreeTULog2MinSize);
        }
        else {
            pitchPred = MAX_CU_SIZE << m_par->chromaShiftWInv;
            pPred = m_interPredC + (((PUStartRow * pitchPred >> m_par->chromaShiftH) + (PUStartColumn << m_par->chromaShiftWInv)) << m_par->QuadtreeTULog2MinSize);
            pitchRec = m_pitchRecChroma;
            pRec = m_uvRec + (((PUStartRow * m_pitchRecChroma >> m_par->chromaShiftH) + (PUStartColumn << m_par->chromaShiftWInv)) << m_par->QuadtreeTULog2MinSize);
        }
    }

    if (cost && costPredFlag) {
        *cost += TuSse(pSrc, m_pitchSrcChroma, pRec, pitchRec, width<<1, width, (m_par->bitDepthChromaShift << 1));
        return;
    }

    Ipp8u cbf[2] = {0, 0};
    if (!data->flags.skippedFlag) {
        CoeffsType *residU = m_residualsU + offset;
        CoeffsType *residV = m_residualsV + offset;
        Ipp32s pitchResidU = width;
        Ipp32s pitchResidV = width;

        if (isIntra) {
            h265_DiffNv12(pSrc, m_pitchSrcChroma, pRec, pitchRec, m_residualsU + offset, width, m_residualsV + offset, width, width, width);
        } else {
            IppiSize roiSize = { width, width };
            Ipp32s offsetCh = GetChromaOffset1(m_par, absPartIdx + idx422, MAX_CU_SIZE >> m_par->chromaShiftW);
            residU = m_interResidU + offsetCh;
            residV = m_interResidV + offsetCh;
            pitchResidU = MAX_CU_SIZE >> m_par->chromaShiftW;
            pitchResidV = MAX_CU_SIZE >> m_par->chromaShiftW;
        }

        TransformFwd(residU, pitchResidU, m_residualsU+offset, width, m_par->bitDepthChroma, 0);
        QuantFwdTu(m_residualsU+offset, m_coeffWorkU+offset, absPartIdx, width, data->qp, 0, isIntra);
        cbf[0] = !IsZero(m_coeffWorkU + offset, width * width);
        if (cbf[0]) {
            QuantInvTu(m_coeffWorkU+offset, m_residualsU+offset, width, data->qp, 0);
            TransformInv(NULL, 0, m_residualsU+offset, width, m_residualsU+offset, width, 0, 0, m_par->bitDepthChroma); 
        }

        TransformFwd(residV, pitchResidV, m_residualsV+offset, width, m_par->bitDepthChroma, 0);
        QuantFwdTu(m_residualsV+offset, m_coeffWorkV+offset, absPartIdx, width, data->qp, 0, isIntra);
        cbf[1] = !IsZero(m_coeffWorkV + offset, width * width);
        if (cbf[1]) {
            QuantInvTu(m_coeffWorkV+offset, m_residualsV+offset, width, data->qp, 0);
            TransformInv(NULL, 0, m_residualsV+offset, width, m_residualsV+offset, width, 0, 0, m_par->bitDepthChroma); 
        }
    }

    if (cbf[0] | cbf[1]) {
        CoeffsType *residU = cbf[0] ? m_residualsU : m_coeffWorkU; // if cbf==0 coeffWork contains zeroes
        CoeffsType *residV = cbf[1] ? m_residualsV : m_coeffWorkV; // if cbf==0 coeffWork contains zeroes
        tuAddClipNv12UV(pRec, pitchRec, pPred, pitchPred, residU+offset, residV+offset, width, width, m_par->bitDepthChroma);
    }
    else if (!isIntra) {
        IppiSize roiSize = {width * 2, width};
        _ippiCopy_C1R(pPred, pitchPred, pRec, pitchRec, roiSize);
    }

    if (cost) {
        *cost += TuSse(pSrc, m_pitchSrcChroma, pRec, pitchRec, width<<1, width, (m_par->bitDepthChromaShift << 1));
        if (m_par->IntraChromaRDO)
            *cost *= m_ChromaDistWeight;
    }

    cbf[0] ? SetCbf<1>(data+idx422, data->trIdx) : ResetCbf<1>(data+idx422);
    cbf[1] ? SetCbf<2>(data+idx422, data->trIdx) : ResetCbf<2>(data+idx422);

    if (cost) {
        m_bsf->Reset();
        if (cbf[0])
            CodeCoeffNxN(m_bsf, this, m_coeffWorkU + offset, absPartIdx, width, TEXT_CHROMA_U);
        if (cbf[1])
            CodeCoeffNxN(m_bsf, this, m_coeffWorkV + offset, absPartIdx, width, TEXT_CHROMA_V);
        *cost += BIT_COST(m_bsf->GetNumBits());
    }
}

/* ****************************************************************************** *\
FUNCTION: AddMvpCand
DESCRIPTION:
\* ****************************************************************************** */

template <typename PixType>
bool H265CU<PixType>::AddMvpCand(MvPredInfo<2> *info, H265CUData *data, Ipp32s blockZScanIdx,
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

template <typename PixType>
void H265CU<PixType>::GetMergeCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx,
                          Ipp32s cuSize, MvPredInfo<5> *mergeInfo)
{
    mergeInfo->numCand = 0;
    if (m_par->log2ParallelMergeLevel > 2 && m_data[topLeftCUBlockZScanIdx].size == 8) {
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
    Ipp32s minTUSize = m_par->MinTUSize;

    for (Ipp32s i = 0; i < 5; i++) {
        abCandIsInter[i] = false;
        isCandAvailable[i] = false;
    }

    Ipp32s partWidth, partHeight, partX, partY;
    GetPartOffsetAndSize(partIdx, partMode, cuSize, partX, partY, partWidth, partHeight);

    Ipp32s topLeftRasterIdx = h265_scan_z2r4[topLeftCUBlockZScanIdx] + partX + PITCH_TU * partY;
    Ipp32s topLeftRow = topLeftRasterIdx >> 4;
    Ipp32s topLeftColumn = topLeftRasterIdx & 15;
    Ipp32s topLeftBlockZScanIdx = h265_scan_r2z4[topLeftRasterIdx];

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

            if (candLCU[i] && !IsDiffMER(canXP[i], canYP[i], xP, yP, m_par->log2ParallelMergeLevel))
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
    Ipp32s numRefIdx = m_cslice->num_ref_idx[0];
    if (m_cslice->slice_type == B_SLICE && m_cslice->num_ref_idx[1] < m_cslice->num_ref_idx[0])
        numRefIdx = m_cslice->num_ref_idx[1];

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

    Ipp32s noBpred = (m_cslice->slice_type != B_SLICE) || (partWidth + partHeight == 3);
    // check bi-pred availability and mark duplicates
    for (Ipp32s i = 0; i < MAX_NUM_MERGE_CANDS; i++) {
        H265MV *mv = mergeInfo->mvCand + 2 * i;
        Ipp8s *refIdx = mergeInfo->refIdx + 2 * i;
        if (noBpred && refIdx[0] >= 0)
            refIdx[1] = -1;
        for (Ipp32s j = 0; j < i; j++) {
            H265MV *omv = mergeInfo->mvCand + 2 * j;
            Ipp8s *orefIdx = mergeInfo->refIdx + 2 * j;
            if (*(Ipp16u*)refIdx == *(Ipp16s*)orefIdx && *(Ipp64u*)mv == *(Ipp64u*)omv) {
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

template <typename PixType>
void H265CU<PixType>::SetupMvPredictionInfo(Ipp32s blockZScanIdx, Ipp32s partMode, Ipp32s curPUidx)
{
    H265CUData *data = m_data + blockZScanIdx;

    Ipp32s numRefLists = 1 + (m_cslice->slice_type == B_SLICE);
    if ((data->size == 8) && (partMode != PART_SIZE_2Nx2N) &&
        (data->interDir & INTER_DIR_PRED_L0)) {
        numRefLists = 1;
    }

    data->flags.mergeFlag = 0;

    const Ipp8s *refIdx = data->refIdx;
    const H265MV *mv = data->mv;

    // setup merge flag and index
    for (Ipp8u i = 0; i < m_mergeCand[curPUidx].numCand; i++) {
        if (IsCandFound(refIdx, mv, m_mergeCand + curPUidx, i, numRefLists)) {
            data->flags.mergeFlag = 1;
            data->mergeIdx = i;
            break;
        }
    }

    if (!data->flags.mergeFlag) {
        // setup mvp_idx and mvd
        for (Ipp32s listIdx = 0; listIdx < 2; listIdx++) {
            if (refIdx[listIdx] < 0)
                continue;
            MvPredInfo<2> *amvpCand = m_amvpCand[curPUidx] + 2 * refIdx[listIdx] + listIdx;

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

template <typename PixType>
H265CUData *H265CU<PixType>::GetNeighbour(Ipp32s &neighbourBlockZScanIdx, Ipp32s neighbourBlockColumn,
                                 Ipp32s neighbourBlockRow, Ipp32s  curBlockZScanIdx,
                                 bool isNeedTocheckCurLCU)
{
    Ipp32s numMinTUInLCU = m_par->NumPartInCUSize;
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

    neighbourBlockZScanIdx = h265_scan_r2z4[(tmpNeighbourBlockRow * PITCH_TU) + tmpNeighbourBlockColumn];

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
        if (neighbourLCUAddr < sliceStartAddr || m_par->m_tile_ids[neighbourLCUAddr] != m_par->m_tile_ids[m_ctbAddr])
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

template <typename PixType>
bool H265CU<PixType>::GetTempMvPred(const H265CUData *currPb, Ipp32s xPb, Ipp32s yPb, Ipp32s nPbW,
                           Ipp32s nPbH, Ipp32s listIdx, Ipp32s refIdx, H265MV *mvLxCol)
{
    Frame *colPic = (m_cslice->slice_type == P_SLICE || m_cslice->collocated_from_l0_flag)
        ? m_currFrame->m_refPicList[0].m_refFrames[m_cslice->collocated_ref_idx]
        : m_currFrame->m_refPicList[1].m_refFrames[m_cslice->collocated_ref_idx];

    H265CUData *colBr = NULL;
    Ipp32s xColBr = xPb + nPbW;
    Ipp32s yColBr = yPb + nPbH;
    Ipp32s numMinTUInLCU = m_par->NumPartInCUSize;
    Ipp32s compressionShift = (m_par->QuadtreeTULog2MinSize < 4) ? 4 - m_par->QuadtreeTULog2MinSize : 0;

    if ((Ipp32s)m_ctbPelX + xColBr * m_par->MinTUSize < m_par->Width &&
        (Ipp32s)m_ctbPelY + yColBr * m_par->MinTUSize < m_par->Height &&
        yColBr < numMinTUInLCU) {

        xColBr = (xColBr >> compressionShift) << compressionShift;
        yColBr = (yColBr >> compressionShift) << compressionShift;

        if (xColBr < numMinTUInLCU) {
            colBr = colPic->cu_data + (m_ctbAddr << m_par->Log2NumPartInCU);
            colBr += h265_scan_r2z4[(yColBr * PITCH_TU) + xColBr];
        }
        else {
            colBr = colPic->cu_data + ((m_ctbAddr + 1) << m_par->Log2NumPartInCU);
            colBr += h265_scan_r2z4[(yColBr * PITCH_TU) + xColBr - numMinTUInLCU];
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
    colCtr += h265_scan_r2z4[(yColCtr * PITCH_TU) + xColCtr];

    return GetColMv(currPb, listIdx, refIdx, colPic, colCtr, mvLxCol);
}


template <typename PixType>
bool H265CU<PixType>::GetColMv(const H265CUData *currPb, Ipp32s listIdxCurr, Ipp32s refIdxCurr,
                      const Frame *colPic, const H265CUData *colPb, H265MV *mvLxCol)
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

template <typename PixType>
void H265CU<PixType>::GetMvpCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s refPicListIdx, Ipp32s refIdx,
                        Ipp32s partMode, Ipp32s partIdx, Ipp32s cuSize, MvPredInfo<2> *info)
{
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

    topLeftRasterIdx = h265_scan_z2r4[topLeftCUBlockZScanIdx] + partX + PITCH_TU * partY;
    topLeftRow = topLeftRasterIdx >> 4;
    topLeftColumn = topLeftRasterIdx & 15;
    topLeftBlockZScanIdx = h265_scan_r2z4[topLeftRasterIdx];

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

template <typename PixType>
void H265CU<PixType>::GetAmvpCand(Ipp32s topLeftBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx,
                         MvPredInfo<2> amvpInfo[2 * MAX_NUM_REF_IDX])
{
    Ipp32s cuSizeInMinTu = m_data[topLeftBlockZScanIdx].size >> m_par->QuadtreeTULog2MinSize;
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

template <typename PixType>
bool H265CU<PixType>::HaveChromaRec() const
{
    return (m_par->AnalyseFlags & HEVC_COST_CHROMA) ||
           (m_par->AnalyseFlags & HEVC_ANALYSE_CHROMA) && m_cslice->slice_type == I_SLICE;
}

template <typename PixType>
bool H265CU<PixType>::IsTuSplitIntra() const
{
#ifndef AMT_ADAPTIVE_INTRA_DEPTH
    Ipp32s tuSplitIntra = (m_par->tuSplitIntra == 1 ||
                           m_par->tuSplitIntra == 3 && m_cslice->slice_type == I_SLICE);
#else
    Ipp32s tuSplitIntra = (m_par->tuSplitIntra == 1 ||
                           m_par->tuSplitIntra == 3 && (m_cslice->slice_type == I_SLICE || 
                           (m_par->BiPyramidLayers > 1 && m_currFrame->m_pyramidLayer==0)));
#endif

    return tuSplitIntra;
}

template <typename PixType>
bool H265CU<PixType>::UpdateChromaRec() const
{
    bool intraUpdate = false;
#ifdef AMT_CHROMA_GUIDED_INTER
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    Ipp32s tuSplitIntra = IsTuSplitIntra();
#else
    Ipp32s tuSplitIntra = false;
#endif
    if(!tuSplitIntra
#ifdef AMT_ADAPTIVE_INTER_DEPTH
        || (m_cslice->slice_type != I_SLICE
        && m_par->QuadtreeTUMaxDepthInter>m_par->QuadtreeTUMaxDepthInterRD)
#endif
        ) 
    {
        intraUpdate = true;
    }
#endif
    return intraUpdate;
}


template <typename PixType>
H265CUData *H265CU<PixType>::StoredCuData(Ipp32s storageIndex) const
{
    VM_ASSERT(storageIndex < NUM_DATA_STORED_LAYERS);
    return m_dataStored + (storageIndex << m_par->Log2NumPartInCU);
}

template <typename PixType>
void H265CU<PixType>::SaveFinalCuDecision(Ipp32s absPartIdx, Ipp32s depth, bool Chroma)
{
    Ipp32s storageIndex = depth;
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

    if (m_costStored[storageIndex] > m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthY = m_par->MaxCUSize >> depth;
        Ipp32s widthC = widthY >> m_par->chromaShiftW;
        Ipp32s heightC = widthY >> m_par->chromaShiftH;
        Ipp32s sizeY = widthY * widthY;
        Ipp32s sizeC = widthC * heightC;
        IppiSize roiY = { widthY, widthY };
        IppiSize roiC = { 2 * widthC, heightC };
        Ipp32s coeffOffsetY = absPartIdx << 4;
        Ipp32s coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);

        Ipp32s pitchRecY;
        Ipp32s pitchRecC;
        PixType *recY;
        PixType *recC;

        if (m_data[absPartIdx].predMode == MODE_INTRA) { // intra keeps its reconstruct in frame-sized plane
            pitchRecY = m_pitchRecLuma;
            pitchRecC = m_pitchRecChroma;
            recY = m_yRec  + GetLumaOffset(m_par, absPartIdx, pitchRecY);
            recC = m_uvRec + GetChromaOffset(m_par, absPartIdx, pitchRecC);
        }
        else { // inter keeps its reconstruct in small buffer
            pitchRecY = MAX_CU_SIZE;
            pitchRecC = MAX_CU_SIZE << m_par->chromaShiftWInv;
            recY = m_interRecWorkY + GetLumaOffset(m_par, absPartIdx, pitchRecY);
            recC = m_interRecWorkC + GetChromaOffset(m_par, absPartIdx, pitchRecC);
        }

        m_costStored[storageIndex] = m_costCurr;                                            // full RD cost
        m_bsf->CtxSave(m_ctxStored[storageIndex]);                                          // all cabac contexts
        CopySubPartTo_(StoredCuData(storageIndex), m_data, absPartIdx, numParts);           // block of CU data
        _ippiCopy_C1R(recY, pitchRecY, m_recStoredY[storageIndex], MAX_CU_SIZE, roiY);      // block of reconstructed Y pels
        _ippsCopy(m_coeffWorkY + coeffOffsetY, m_coeffStoredY[storageIndex], sizeY);        // block of Y coefficients

        if (HaveChromaRec() || Chroma) {
            _ippiCopy_C1R(recC, pitchRecC, m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, roiC);  // block of reconstructed UV pels
            _ippsCopy(m_coeffWorkU + coeffOffsetC, m_coeffStoredU[storageIndex], sizeC);    // block of U coefficients
            _ippsCopy(m_coeffWorkV + coeffOffsetC, m_coeffStoredV[storageIndex], sizeC);    // block of V coefficients
        }
    }
}

template <typename PixType>
void H265CU<PixType>::LoadWorkingCuDecision(Ipp32s absPartIdx, Ipp32s depth)
{
    Ipp32s storageIndex = depth;
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

    if (m_costStored[storageIndex] <= m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthY = m_par->MaxCUSize >> depth;
        Ipp32s widthC = widthY >> m_par->chromaShiftW;
        Ipp32s heightC = widthY >> m_par->chromaShiftH;
        Ipp32s sizeY = widthY * widthY;
        Ipp32s sizeC = widthC * heightC;
        IppiSize roiY = { widthY, widthY };
        IppiSize roiC = { 2 * widthC, heightC };
        Ipp32s coeffOffsetY = absPartIdx << 4;
        Ipp32s coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);

        m_costCurr = m_costStored[storageIndex];                                            // full RD cost
        m_bsf->CtxRestore(m_ctxStored[storageIndex]);                                       // all cabac contexts
        CopySubPartTo_(m_data, StoredCuData(storageIndex), absPartIdx, numParts);           // block of CU data

        Ipp32s pitchRecY;
        Ipp32s pitchRecC;
        PixType *recY;
        PixType *recC;

        if (m_data[absPartIdx].predMode == MODE_INTRA) { // intra keeps its reconstruct in frame-sized plane
            pitchRecY = m_pitchRecLuma;
            pitchRecC = m_pitchRecChroma;
            recY = m_yRec  + GetLumaOffset(m_par, absPartIdx, pitchRecY);
            recC = m_uvRec + GetChromaOffset(m_par, absPartIdx, pitchRecC);
        }
        else { // inter keeps its reconstruct in small buffer
            pitchRecY = MAX_CU_SIZE;
            pitchRecC = MAX_CU_SIZE << m_par->chromaShiftWInv;
            recY = m_interRecWorkY + GetLumaOffset(m_par, absPartIdx, pitchRecY);
            recC = m_interRecWorkC + GetChromaOffset(m_par, absPartIdx, pitchRecC);
        }

        _ippiCopy_C1R(m_recStoredY[storageIndex], MAX_CU_SIZE, recY, pitchRecY, roiY);      // block of reconstructed Y pels
        _ippsCopy(m_coeffStoredY[storageIndex], m_coeffWorkY + coeffOffsetY, sizeY);        // block of Y coefficients

        if (HaveChromaRec()) {
            _ippiCopy_C1R(m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, recC, pitchRecC, roiC);  // block of reconstructed UV pels
            _ippsCopy(m_coeffStoredU[storageIndex], m_coeffWorkU + coeffOffsetC, sizeC);    // block of U coefficients
            _ippsCopy(m_coeffStoredV[storageIndex], m_coeffWorkV + coeffOffsetC, sizeC);    // block of V coefficients
        }
    }
}


template <typename PixType>
void H265CU<PixType>::LoadFinalCuDecision(Ipp32s absPartIdx, Ipp32s depth, bool Chroma)
{
    Ipp32s storageIndex = depth;
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

    if (m_costStored[storageIndex] <= m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthY = m_par->MaxCUSize >> depth;
        Ipp32s widthC = widthY >> m_par->chromaShiftW;
        Ipp32s heightC = widthY >> m_par->chromaShiftH;
        Ipp32s sizeY = widthY * widthY;
        Ipp32s sizeC = widthC * heightC;
        IppiSize roiY = { widthY, widthY };
        IppiSize roiC = { 2 * widthC, heightC };
        Ipp32s coeffOffsetY = absPartIdx << 4;
        Ipp32s coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);

        Ipp32s pitchRecY = m_pitchRecLuma;
        Ipp32s pitchRecC = m_pitchRecChroma;
        PixType *recY = m_yRec  + GetLumaOffset(m_par, absPartIdx, pitchRecY);
        PixType *recC = m_uvRec + GetChromaOffset(m_par, absPartIdx, pitchRecC);

        m_costCurr = m_costStored[storageIndex];                                            // full RD cost
        m_bsf->CtxRestore(m_ctxStored[storageIndex]);                                       // all cabac contexts
        CopySubPartTo_(m_data, StoredCuData(storageIndex), absPartIdx, numParts);           // block of CU data
        _ippiCopy_C1R(m_recStoredY[storageIndex], MAX_CU_SIZE, recY, pitchRecY, roiY);      // block of reconstructed Y pels
        _ippsCopy(m_coeffStoredY[storageIndex], m_coeffWorkY + coeffOffsetY, sizeY);        // block of Y coefficients

        if (HaveChromaRec() || Chroma) {
            _ippiCopy_C1R(m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, recC, pitchRecC, roiC);  // block of reconstructed UV pels
            _ippsCopy(m_coeffStoredU[storageIndex], m_coeffWorkU + coeffOffsetC, sizeC);    // block of U coefficients
            _ippsCopy(m_coeffStoredV[storageIndex], m_coeffWorkV + coeffOffsetC, sizeC);    // block of V coefficients
        }
    }
    else if (m_data[absPartIdx].predMode == MODE_INTER && m_data[absPartIdx].depth == depth) {
        // when current decision is the best one and it is INTER mode (non split though)
        // then reconstructed pels are in temporal buffer
        // need to copy it from there to common place
        Ipp32s widthY = m_par->MaxCUSize >> depth;
        Ipp32s widthC = widthY >> m_par->chromaShiftW;
        Ipp32s heightC = widthY >> m_par->chromaShiftH;
        IppiSize roiY = { widthY, widthY };
        IppiSize roiC = { 2 * widthC, heightC };

        Ipp32s pitchDstY = m_pitchRecLuma;
        Ipp32s pitchDstC = m_pitchRecChroma;
        PixType *dstY = m_yRec  + GetLumaOffset(m_par, absPartIdx, pitchDstY);
        PixType *dstC = m_uvRec + GetChromaOffset(m_par, absPartIdx, pitchDstC);

        Ipp32s pitchSrcY = MAX_CU_SIZE;
        Ipp32s pitchSrcC = MAX_CU_SIZE << m_par->chromaShiftWInv;
        PixType *srcY = m_interRecWorkY + GetLumaOffset(m_par, absPartIdx, pitchSrcY);
        PixType *srcC = m_interRecWorkC + GetChromaOffset(m_par, absPartIdx, pitchSrcC);

        _ippiCopy_C1R(srcY, pitchSrcY, dstY, pitchDstY, roiY);      // block of reconstructed Y pels
        if (HaveChromaRec() || Chroma)
            _ippiCopy_C1R(srcC, pitchSrcC, dstC, pitchDstC, roiC);  // block of reconstructed UV pels
    }
}


template <typename PixType>
void H265CU<PixType>::SaveInterTuDecision(Ipp32s absPartIdx, Ipp32s depth)
{
    VM_ASSERT(m_data[absPartIdx].predMode == MODE_INTER);

    Ipp32s storageIndex = depth + 1;
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

    if (m_costStored[storageIndex] > m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthY = m_par->MaxCUSize >> depth;
        Ipp32s widthC = widthY >> m_par->chromaShiftW;
        Ipp32s heightC = widthY >> m_par->chromaShiftH;
        Ipp32s sizeY = widthY * widthY;
        Ipp32s sizeC = widthC * heightC;
        IppiSize roiY = { widthY, widthY };
        IppiSize roiC = { 2 * widthC, heightC };
        Ipp32s coeffOffsetY = absPartIdx << 4;
        Ipp32s coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);

        Ipp32s pitchRecY = MAX_CU_SIZE;
        Ipp32s pitchRecC = MAX_CU_SIZE << m_par->chromaShiftWInv;
        PixType *recY = m_interRecWorkY + GetLumaOffset(m_par, absPartIdx, pitchRecY);
        PixType *recC = m_interRecWorkC + GetChromaOffset(m_par, absPartIdx, pitchRecC);

        m_costStored[storageIndex] = m_costCurr;                                            // full RD cost
        m_bsf->CtxSave(m_ctxStored[storageIndex]);                                          // all cabac contexts
        CopySubPartTo_(StoredCuData(storageIndex), m_data, absPartIdx, numParts);           // block of CU data
        _ippiCopy_C1R(recY, pitchRecY, m_recStoredY[storageIndex], MAX_CU_SIZE, roiY);      // block of reconstructed Y pels
        _ippsCopy(m_coeffWorkY + coeffOffsetY, m_coeffStoredY[storageIndex], sizeY);        // block of Y coefficients

        if (HaveChromaRec()) {
            _ippiCopy_C1R(recC, pitchRecC, m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, roiC);  // block of reconstructed UV pels
            _ippsCopy(m_coeffWorkU + coeffOffsetC, m_coeffStoredU[storageIndex], sizeC);    // block of U coefficients
            _ippsCopy(m_coeffWorkV + coeffOffsetC, m_coeffStoredV[storageIndex], sizeC);    // block of V coefficients
        }
    }
}


template <typename PixType>
void H265CU<PixType>::LoadInterTuDecision(Ipp32s absPartIdx, Ipp32s depth)
{
    VM_ASSERT(m_data[absPartIdx].predMode == MODE_INTER);

    Ipp32s storageIndex = depth + 1;
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

    if (m_costStored[storageIndex] <= m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthY = m_par->MaxCUSize >> depth;
        Ipp32s widthC = widthY >> m_par->chromaShiftW;
        Ipp32s heightC = widthY >> m_par->chromaShiftH;
        Ipp32s sizeY = widthY * widthY;
        Ipp32s sizeC = widthC * heightC;
        IppiSize roiY = { widthY, widthY };
        IppiSize roiC = { 2 * widthC, heightC };
        Ipp32s coeffOffsetY = absPartIdx << 4;
        Ipp32s coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);

        Ipp32s pitchRecY = MAX_CU_SIZE;
        Ipp32s pitchRecC = MAX_CU_SIZE << m_par->chromaShiftWInv;
        PixType *recY = m_interRecWorkY + GetLumaOffset(m_par, absPartIdx, pitchRecY);
        PixType *recC = m_interRecWorkC + GetChromaOffset(m_par, absPartIdx, pitchRecC);

        m_costCurr = m_costStored[storageIndex];                                            // full RD cost
        m_bsf->CtxRestore(m_ctxStored[storageIndex]);                                       // all cabac contexts
        CopySubPartTo_(m_data, StoredCuData(storageIndex), absPartIdx, numParts);           // block of CU data
        _ippiCopy_C1R(m_recStoredY[storageIndex], MAX_CU_SIZE, recY, pitchRecY, roiY);      // block of reconstructed Y pels
        _ippsCopy(m_coeffStoredY[storageIndex], m_coeffWorkY + coeffOffsetY, sizeY);        // block of Y coefficients

        if (HaveChromaRec()) {
            _ippiCopy_C1R(m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, recC, pitchRecC, roiC);  // block of reconstructed UV pels
            _ippsCopy(m_coeffStoredU[storageIndex], m_coeffWorkU + coeffOffsetC, sizeC);    // block of U coefficients
            _ippsCopy(m_coeffStoredV[storageIndex], m_coeffWorkV + coeffOffsetC, sizeC);    // block of V coefficients
        }
    }
}


template <typename PixType>
void H265CU<PixType>::SaveIntraLumaDecision(Ipp32s absPartIdx, Ipp32s depth)
{
    // designed for partial states
    // which can be compared only during Intra mode decision
    VM_ASSERT(m_data[absPartIdx].predMode == MODE_INTRA);

    Ipp32s storageIndex = depth + 1; // [depth] is occupied by full RD cost
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));

    if (m_costStored[storageIndex] > m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthY = m_par->MaxCUSize >> depth;
        Ipp32s sizeY = widthY * widthY;
        IppiSize roiY = { widthY, widthY };
        Ipp32s coeffOffsetY = absPartIdx << 4;

        Ipp32s pitchRecY = m_pitchRecLuma;
        PixType *recY = m_yRec + GetLumaOffset(m_par, absPartIdx, pitchRecY);

        m_costStored[storageIndex] = m_costCurr;                                        // partial RD cost (luma only)
        m_bsf->CtxSave(m_ctxStored[storageIndex]);                                      // all cabac contexts
        CopySubPartTo_(StoredCuData(storageIndex), m_data, absPartIdx, numParts);       // block of CU data
        _ippiCopy_C1R(recY, pitchRecY, m_recStoredY[storageIndex], MAX_CU_SIZE, roiY);  // block of reconstructed Y pels
        _ippsCopy(m_coeffWorkY + coeffOffsetY, m_coeffStoredY[storageIndex], sizeY);    // block of Y coefficients
    }
}


template <typename PixType>
void H265CU<PixType>::LoadIntraLumaDecision(Ipp32s absPartIdx, Ipp32s depth)
{
    // designed for partial states
    // which can be compared only during Intra mode decision
    VM_ASSERT(m_data[absPartIdx].predMode == MODE_INTRA);

    Ipp32s storageIndex = depth + 1; // [depth] is occupied by full RD cost
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_recStoredY) / sizeof(m_recStoredY[0]));
    VM_ASSERT((Ipp32u)storageIndex < sizeof(m_coeffStoredY) / sizeof(m_coeffStoredY[0]));

    if (m_costStored[storageIndex] <= m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthY = m_par->MaxCUSize >> depth;
        Ipp32s sizeY = widthY * widthY;
        IppiSize roiY = { widthY, widthY };
        Ipp32s coeffOffsetY = absPartIdx << 4;

        Ipp32s pitchRecY = m_pitchRecLuma;
        PixType *recY = m_yRec + GetLumaOffset(m_par, absPartIdx, pitchRecY);

        m_costCurr = m_costStored[storageIndex];                                        // partial RD cost (luma only)
        m_bsf->CtxRestore(m_ctxStored[storageIndex]);                                   // all cabac contexts
        CopySubPartTo_(m_data, StoredCuData(storageIndex), absPartIdx, numParts);       // block of CU data
        _ippiCopy_C1R(m_recStoredY[storageIndex], MAX_CU_SIZE, recY, pitchRecY, roiY);  // block of reconstructed Y pels
        _ippsCopy(m_coeffStoredY[storageIndex], m_coeffWorkY + coeffOffsetY, sizeY);    // block of Y coefficients
    }
}


template <typename PixType>
void H265CU<PixType>::SaveIntraChromaDecision(Ipp32s absPartIdx, Ipp32s depth)
{
    // designed for partial states
    // which can be compared only during Intra mode decision
    VM_ASSERT(m_data[absPartIdx].predMode == MODE_INTRA);

    Ipp32s storageIndex = depth + 1; // [depth] is occupied by full RD cost
    VM_ASSERT((Ipp32u)depth < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)depth < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)depth < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
    VM_ASSERT((Ipp32u)depth < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
    VM_ASSERT((Ipp32u)depth < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

    if (m_costStored[storageIndex] > m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthC = (m_par->MaxCUSize >> depth) >> m_par->chromaShiftW;
        Ipp32s heightC = (m_par->MaxCUSize >> depth) >> m_par->chromaShiftH;
        Ipp32s sizeC = widthC * heightC;
        IppiSize roiC = { 2 * widthC, heightC };
        Ipp32s coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);

        Ipp32s pitchRecC = m_pitchRecChroma;
        PixType *recC = m_uvRec + GetChromaOffset(m_par, absPartIdx, pitchRecC);

        m_costStored[storageIndex] = m_costCurr;                                        // full RD cost
        m_bsf->CtxSave(m_ctxStored[storageIndex]);                                      // all cabac contexts
        CopySubPartTo_(StoredCuData(storageIndex), m_data, absPartIdx, numParts);       // block of CU data
        _ippiCopy_C1R(recC, pitchRecC, m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, roiC);  // block of reconstructed UV pels
        _ippsCopy(m_coeffWorkU + coeffOffsetC, m_coeffStoredU[storageIndex], sizeC);    // block of U coefficients
        _ippsCopy(m_coeffWorkV + coeffOffsetC, m_coeffStoredV[storageIndex], sizeC);    // block of V coefficients
    }
}


template <typename PixType>
void H265CU<PixType>::LoadIntraChromaDecision(Ipp32s absPartIdx, Ipp32s depth)
{
    // designed for partial states
    // which can be compared only during Intra mode decision
    VM_ASSERT(m_data[absPartIdx].predMode == MODE_INTRA);

    Ipp32s storageIndex = depth + 1; // [depth] is occupied by full RD cost
    VM_ASSERT((Ipp32u)depth < sizeof(m_costStored) / sizeof(m_costStored[0]));
    VM_ASSERT((Ipp32u)depth < sizeof(m_ctxStored) / sizeof(m_ctxStored[0]));
    VM_ASSERT((Ipp32u)depth < sizeof(m_recStoredC) / sizeof(m_recStoredC[0]));
    VM_ASSERT((Ipp32u)depth < sizeof(m_coeffStoredU) / sizeof(m_coeffStoredU[0]));
    VM_ASSERT((Ipp32u)depth < sizeof(m_coeffStoredV) / sizeof(m_coeffStoredV[0]));

    if (m_costStored[storageIndex] <= m_costCurr) {
        Ipp32s numParts = m_par->NumPartInCU >> (2 * depth);
        Ipp32s widthC = (m_par->MaxCUSize >> depth) >> m_par->chromaShiftW;
        Ipp32s heightC = (m_par->MaxCUSize >> depth) >> m_par->chromaShiftH;
        Ipp32s sizeC = widthC * heightC;
        IppiSize roiC = { 2 * widthC, heightC };
        Ipp32s coeffOffsetC = absPartIdx << (4 - m_par->chromaShift);

        Ipp32s pitchRecC = m_pitchRecChroma;
        PixType *recC = m_uvRec + GetChromaOffset(m_par, absPartIdx, pitchRecC);

        m_costCurr = m_costStored[storageIndex];                                        // full RD cost
        m_bsf->CtxRestore(m_ctxStored[storageIndex]);                                   // all cabac contexts
        CopySubPartTo_(m_data, StoredCuData(storageIndex), absPartIdx, numParts);       // block of CU data
        _ippiCopy_C1R(m_recStoredC[storageIndex], MAX_CU_SIZE << m_par->chromaShiftWInv, recC, pitchRecC, roiC);  // block of reconstructed UV pels
        _ippsCopy(m_coeffStoredU[storageIndex], m_coeffWorkU + coeffOffsetC, sizeC);    // block of U coefficients
        _ippsCopy(m_coeffStoredV[storageIndex], m_coeffWorkV + coeffOffsetC, sizeC);    // block of V coefficients
    }
}

template <typename PixType>
void H265CU<PixType>::SaveBestInterPredAndResid(Ipp32s absPartIdx, Ipp32s depth)
{
    Ipp32s storageIndex = depth;
    VM_ASSERT((Ipp32u)depth < sizeof(m_costStored) / sizeof(m_costStored[0]));
 
    if (m_costStored[storageIndex] > m_costCurr) {
        // save Inter predicted pixels and residual simply by swapping pointers
        // to temporal buffers (there are only 2 of them which is enough)
        std::swap(m_interPredY, m_interPredBestY);
        std::swap(m_interPredC, m_interPredBestC);
        std::swap(m_interResidY, m_interResidBestY);
        std::swap(m_interResidU, m_interResidBestU);
        std::swap(m_interResidV, m_interResidBestV);
    }
}

template <typename PixType>
void H265CU<PixType>::LoadBestInterPredAndResid(Ipp32s absPartIdx, Ipp32s depth)
{
    Ipp32s storageIndex = depth;
    VM_ASSERT((Ipp32u)depth < sizeof(m_costStored) / sizeof(m_costStored[0]));

    const H265CUData *dataStored = StoredCuData(storageIndex) + absPartIdx;
    if (dataStored->predMode == MODE_INTER && m_costStored[storageIndex] <= m_costCurr) {
        // stored decision is INTER and its cost is better than current
        // get predicted pixels and residual from there
        std::swap(m_interPredY, m_interPredBestY);
        std::swap(m_interPredC, m_interPredBestC);
        std::swap(m_interResidY, m_interResidBestY);
        std::swap(m_interResidU, m_interResidBestU);
        std::swap(m_interResidV, m_interResidBestV);
    }
#ifdef AMT_ALT_ENCODE
    m_interPredSavedY[depth][absPartIdx] = m_interPredY;
    m_interPredSavedC[depth][absPartIdx] = m_interPredC;
    m_interResidSavedY[depth][absPartIdx] = m_interResidY;
    m_interResidSavedU[depth][absPartIdx] = m_interResidU;
    m_interResidSavedV[depth][absPartIdx] = m_interResidV;
#endif
}

template <typename PixType>
void H265CU<PixType>::SetCuLambda(Frame* frame)
{
    int deltaQP = frame->m_stats[0]->qp_mask[m_ctbAddr];//use "0" (original) only here
    deltaQP = Saturate(-MAX_DQP, MAX_DQP, deltaQP);
    int idxDqp = 2*abs(deltaQP)-((deltaQP<0)?1:0);

    H265Slice* curSlice = &(frame->m_dqpSlice[idxDqp]);
    m_rdLambda = curSlice->rd_lambda_slice;
    m_rdLambdaSqrt = curSlice->rd_lambda_sqrt_slice;
    m_ChromaDistWeight = curSlice->ChromaDistWeight_slice;
    if( (m_par->DeltaQpMode&AMT_DQP_CAQ) &&
        ((curSlice->slice_type==I_SLICE) ||
        (curSlice->slice_type==P_SLICE && m_par->BiPyramidLayers > 1) ||
        (curSlice->slice_type==B_SLICE && m_par->BiPyramidLayers > 1 && frame->m_pyramidLayer==0))
      )
    {
        m_rdLambdaChroma = m_rdLambda*(2.0+2.0/(m_ChromaDistWeight*m_par->LambdaCorrection))/4.0;
        // to match previous results
    } else 
    {
        m_rdLambdaChroma = m_rdLambda*(3.0+1.0/(m_ChromaDistWeight))/4.0;
    }
    m_rdLambdaInter = curSlice->rd_lambda_inter_slice;
    m_rdLambdaInterMv = curSlice->rd_lambda_inter_mv_slice;
}

template <typename PixType>
void H265CU<PixType>::SetCuLambdaRoi(Frame* frame)
{
    Ipp32s indx = frame->m_lcuQps[m_ctbAddr] + (frame->m_bitDepthLuma - 8)*6;
    H265Slice* curSlice = &(frame->m_roiSlice[indx]);
    m_rdLambda = curSlice->rd_lambda_slice;
    m_rdLambdaSqrt = curSlice->rd_lambda_sqrt_slice;
    m_ChromaDistWeight = curSlice->ChromaDistWeight_slice;
    m_rdLambdaChroma = m_rdLambda*(3.0+1.0/m_ChromaDistWeight)/4.0;
    m_rdLambdaInter = curSlice->rd_lambda_inter_slice;
    m_rdLambdaInterMv = curSlice->rd_lambda_inter_mv_slice;
}

template <typename PixType>
const H265CUData *H265CU<PixType>::GetBestCuDecision(Ipp32s absPartIdx, Ipp32s depth) const
{
    VM_ASSERT((Ipp32u)depth < sizeof(m_costStored) / sizeof(m_costStored[0]));
    return (m_costStored[depth] <= m_costCurr ? StoredCuData(depth) : m_data) + absPartIdx;
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

template class H265CU<Ipp8u>;
template class H265CU<Ipp16u>;

template<>
Ipp32u CheckSum<Ipp8u>(const Ipp8u *buf, Ipp32s size, Ipp32u initialSum)
{
    Ipp32u sum = initialSum;
    //ippsCRC32_8u(buf, size, &sum);
    return sum;
}

template<>
Ipp32u CheckSum<Ipp8u>(const Ipp8u *buf, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32u initialSum)
{
    Ipp32u sum = initialSum;
    for (Ipp32s i = 0; i < height; i++, buf += pitch)
        sum = CheckSum(buf, width, sum);
    return sum;
}

Ipp32u CheckSumCabacCtx(const Ipp8u buf[188])
{
    return CheckSum(buf, 188);
}

Ipp32u CheckSumCabacCtxChroma(const Ipp8u buf[188])
{
    Ipp32s sum = 0;
    sum += CheckSum(buf + tab_ctxIdxOffset[SIG_COEFF_GROUP_FLAG_HEVC] + 2, 2, sum);
    sum += CheckSum(buf + tab_ctxIdxOffset[SIG_FLAG_HEVC] + 27, 15, sum);
    sum += CheckSum(buf + tab_ctxIdxOffset[ONE_FLAG_HEVC] + 16, 8, sum);
    sum += CheckSum(buf + tab_ctxIdxOffset[ABS_FLAG_HEVC] + 4, 2, sum);
    sum += CheckSum(buf + tab_ctxIdxOffset[LAST_X_HEVC] + 15, 15, sum);
    sum += CheckSum(buf + tab_ctxIdxOffset[LAST_Y_HEVC] + 15, 15, sum);
    return sum;
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE

