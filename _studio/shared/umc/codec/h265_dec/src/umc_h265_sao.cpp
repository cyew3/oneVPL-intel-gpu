//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#ifndef MFX_VA

#include "umc_h265_dec_defs.h"
#include "umc_h265_yuv.h"
#include "umc_h265_frame.h"
#include "umc_h265_frame_info.h"
#include "mfx_h265_optimization.h"

namespace UMC_HEVC_DECODER
{

enum
{
    SAO_PRED_SIZE = 66
};

// Get the sign of input variable
inline Ipp32s getSign(Ipp32s x)
{
  return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
}

static const Ipp8u EoTable[9] =
{
    1, //0
    2, //1
    0, //2
    3, //3
    4, //4
    0, //5
    0, //6
    0, //7
    0
};

// SAO context data structure constructor
template<typename PlaneType>
H265SampleAdaptiveOffsetTemplate<PlaneType>::H265SampleAdaptiveOffsetTemplate()
{
    m_ClipTable = 0;
    m_ClipTableBase = 0;
    m_OffsetBo = 0;
    m_OffsetBoChroma = 0;
    m_OffsetBo2Chroma = 0;
    m_lumaTableBo = 0;
    m_chromaTableBo = 0;
    m_bitdepth_luma = 0;
    m_bitdepth_chroma = 0;

    m_TmpU[0] = 0;
    m_TmpU[1] = 0;

    m_TmpL[0] = 0;
    m_TmpL[1] = 0;

    m_tempPCMBuffer = 0;

    m_isInitialized = false;
    m_UseNIF = false;
    m_Frame = 0;
    m_ClipTableChroma = 0;
    m_SaoBitIncreaseC = 0;
    m_SaoBitIncreaseY = 0;
    m_sps = 0;
    m_needPCMRestoration = 0;
    m_PicHeight = 0;
    m_slice_sao_chroma_flag = 0;
    m_MaxCUSize = 0;
    m_PicWidth = 0;
    m_slice_sao_luma_flag = 0;
}

// SAO context data structure deallocator
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::destroy()
{
    delete [] m_ClipTableBase; m_ClipTableBase = 0;
    delete [] m_OffsetBo; m_OffsetBo = 0;
    delete [] m_OffsetBoChroma; m_OffsetBoChroma = 0;
    delete [] m_OffsetBo2Chroma; m_OffsetBo2Chroma = 0;
    delete[] m_lumaTableBo; m_lumaTableBo = 0;
    delete[] m_chromaTableBo; m_chromaTableBo = 0;
    delete [] m_TmpU[0]; m_TmpU[0] = 0;
    delete [] m_TmpU[1]; m_TmpU[1] = 0;
    delete [] m_TmpL[0]; m_TmpL[0] = 0;
    delete [] m_TmpL[1]; m_TmpL[1] = 0;

    delete [] m_tempPCMBuffer; m_tempPCMBuffer = 0;

    m_PicWidth  = 0;
    m_PicHeight = 0;
    m_isInitialized = false;
}

// Returns whethher it is necessary to reallocate SAO context
template<typename PlaneType>
bool H265SampleAdaptiveOffsetTemplate<PlaneType>::isNeedReInit(const H265SeqParamSet* sps)
{
    if (m_isInitialized && m_PicWidth  == sps->pic_width_in_luma_samples && m_PicHeight == sps->pic_height_in_luma_samples &&
        m_bitdepth_luma == sps->bit_depth_luma && m_bitdepth_chroma == sps->bit_depth_chroma)
        return false;

    return true;
}

// SAO context initalization
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::init(const H265SeqParamSet* sps)
{
    if (!sps->sample_adaptive_offset_enabled_flag)
        return;

    if (!isNeedReInit(sps))
        return;

    m_PicWidth  = sps->pic_width_in_luma_samples;
    m_PicHeight = sps->pic_height_in_luma_samples;

    m_MaxCUSize  = sps->MaxCUSize;

    m_bitdepth_luma = sps->bit_depth_luma;
    m_bitdepth_chroma = sps->bit_depth_chroma;

    Ipp32u uiPixelRangeY = 1 << sps->bit_depth_luma;
    Ipp32u uiBoRangeShiftY = sps->bit_depth_luma - SAO_BO_BITS;

    // Initialize lookup tables for band offset for luma and NV12 chroma
    m_lumaTableBo = h265_new_array_throw<PlaneType>(uiPixelRangeY);
    for (Ipp32u k2 = 0; k2 < uiPixelRangeY; k2++)
    {
        m_lumaTableBo[k2] = (PlaneType)(1 + (k2>>uiBoRangeShiftY));
    }

    uiPixelRangeY = 1 << sps->bit_depth_chroma;
    uiBoRangeShiftY = sps->bit_depth_chroma - SAO_BO_BITS;

    m_chromaTableBo = h265_new_array_throw<PlaneType>(uiPixelRangeY);
    for (Ipp32u k2 = 0; k2 < uiPixelRangeY; k2++)
    {
        m_chromaTableBo[k2] = (PlaneType)(1 + (k2>>uiBoRangeShiftY));
    }

    Ipp32u uiMaxY  = (1 << sps->bit_depth_luma) - 1;
    Ipp32u uiMinY  = 0;

    Ipp32u iCRangeExt = uiMaxY>>1;

    Ipp32s doubleClipTable = sps->bit_depth_luma != sps->bit_depth_chroma ? 1 : 0;

    Ipp32s clipTableSize = uiMaxY+2*iCRangeExt;

    if (doubleClipTable)
    {
        Ipp32u uiMaxUV  = (1 << sps->bit_depth_chroma) - 1;
        Ipp32u iCRangeExtUV = uiMaxUV>>1;
        clipTableSize += uiMaxUV+2*iCRangeExtUV;
    }

    m_ClipTableBase = h265_new_array_throw<PlaneType>(clipTableSize);
    m_OffsetBo      = h265_new_array_throw<PlaneType>(uiMaxY+2*iCRangeExt);

    // Initialize lookup table to clip sample values beyond bit depth allowed range
    for (Ipp32u i = 0; i < (uiMinY + iCRangeExt);i++)
    {
        m_ClipTableBase[i] = (PlaneType)uiMinY;
    }

    for (Ipp32u i = uiMinY + iCRangeExt; i < (uiMaxY + iCRangeExt); i++)
    {
        m_ClipTableBase[i] = (PlaneType)(i - iCRangeExt);
    }

    for (Ipp32u i = uiMaxY + iCRangeExt; i < (uiMaxY + 2 * iCRangeExt); i++)
    {
        m_ClipTableBase[i] = (PlaneType)uiMaxY;
    }

    m_ClipTable = &(m_ClipTableBase[iCRangeExt]);
    m_ClipTableChroma = &(m_ClipTableBase[iCRangeExt]);

    Ipp32u uiMaxUV  = (1 << sps->bit_depth_chroma) - 1;
    Ipp32u iCRangeExtUV = uiMaxUV>>1;

    if (doubleClipTable)
    {
        PlaneType * clipBase = m_ClipTableBase + uiMaxY+2*iCRangeExt;

        // Initialize lookup table to clip sample values beyond bit depth allowed range
        for (Ipp32u i = 0; i < (uiMinY + iCRangeExtUV);i++)
        {
            clipBase[i] = (PlaneType)uiMinY;
        }

        for (Ipp32u i = uiMinY + iCRangeExtUV; i < (uiMaxUV + iCRangeExtUV); i++)
        {
            clipBase[i] = (PlaneType)(i - iCRangeExtUV);
        }

        for (Ipp32u i = uiMaxUV + iCRangeExtUV; i < (uiMaxUV + 2 * iCRangeExtUV); i++)
        {
            clipBase[i] = (PlaneType)uiMaxUV;
        }

        m_ClipTableChroma = &(clipBase[iCRangeExtUV]);
    }

    m_OffsetBoChroma   = h265_new_array_throw<PlaneType>(uiMaxUV+2*iCRangeExtUV);
    m_OffsetBo2Chroma  = h265_new_array_throw<PlaneType>(uiMaxUV+2*iCRangeExtUV);

    m_TmpU[0] = h265_new_array_throw<PlaneType>(2*m_PicWidth);
    m_TmpU[1] = h265_new_array_throw<PlaneType>(2*m_PicWidth);

    m_TmpL[0] = h265_new_array_throw<PlaneType>(3*SAO_PRED_SIZE);
    m_TmpL[1] = h265_new_array_throw<PlaneType>(3*SAO_PRED_SIZE);

    // Temporary buffer to store PCM and tranquant bypass samples which may require to be skipped in filtering
    m_tempPCMBuffer = h265_new_array_throw<PlaneType>((64*64 *3) /2); // one CU data

    m_isInitialized = true;
}

// Apply SAO filtering to NV12 chroma plane. Filter is enabled across slices and
// tiles so only frame boundaries are taken into account.
// HEVC spec 8.7.3.
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::processSaoCuOrgChroma(Ipp32s addr, Ipp32s saoType, PlaneType* tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    PlaneType* pRec;
    Ipp32s tmpUpBuff1[66];
    Ipp32s tmpUpBuff2[66];
    Ipp32s stride;
    Ipp32s signLeftCb, signLeftCr;
    Ipp32s signRightCb, signRightCr;
    Ipp32s signDownCb, signDownCr;
    Ipp32u edgeTypeCb, edgeTypeCr;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    PlaneType*  tmpU;
    Ipp32s *pOffsetEoCb = m_OffsetEoChroma;
    Ipp32s *pOffsetEoCr = m_OffsetEo2Chroma;
    PlaneType* pOffsetBoCb = m_OffsetBoChroma;
    PlaneType* pOffsetBoCr = m_OffsetBo2Chroma;

    Ipp32s picWidthTmp  = m_PicWidth;
    Ipp32s picHeightTmp = m_PicHeight >> m_sps->chromaShiftH;
    Ipp32s LCUWidth     = m_MaxCUSize;
    Ipp32s LCUHeight    = m_MaxCUSize   >> m_sps->chromaShiftH;
    Ipp32s LPelX        = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY        = (Ipp32s)pTmpCu->m_CUPelY >> m_sps->chromaShiftH;
    Ipp32s RPelX        = LPelX + LCUWidth;
    Ipp32s BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    pRec      = (PlaneType* )m_Frame->GetCbCrAddr(addr);
    stride    = m_Frame->pitch_chroma();

    tmpU = ((PlaneType*)m_TmpU[0]) + LPelX + picWidthTmp;

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            startX = (LPelX == 0) ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 2 : LCUWidth;

            for (y = 0; y < LCUHeight; y++)
            {
                signLeftCb = getSign(pRec[startX] - tmpL[y * 2]);
                signLeftCr = getSign(pRec[startX + 1] - tmpL[y * 2 + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    signRightCb = getSign(pRec[x] - pRec[x + 2]);
                    signRightCr = getSign(pRec[x + 1] - pRec[x + 3]);
                    edgeTypeCb = signRightCb + signLeftCb + 2;
                    edgeTypeCr = signRightCr + signLeftCr + 2;
                    signLeftCb = -signRightCb;
                    signLeftCr = -signRightCr;

                    pRec[x] = m_ClipTableChroma[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = m_ClipTableChroma[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }

                pRec += stride;
            }
            break;
        }
    case SAO_EO_1: // dir: |
        {
            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0)
            {
                pRec += stride;
            }

            for (x = 0; x < LCUWidth; x++)
            {
                tmpUpBuff1[x] = getSign(pRec[x] - tmpU[x]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + 1 + stride]);
                    edgeTypeCb = signDownCb + tmpUpBuff1[x] + 2;
                    edgeTypeCr = signDownCr + tmpUpBuff1[x + 1] + 2;
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;

                    pRec[x] = m_ClipTableChroma[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = m_ClipTableChroma[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
                pRec += stride;
            }
            break;
        }
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            startX = (LPelX == 0)           ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 2 : LCUWidth;

            startY = (TPelY == 0) ?            1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0)
            {
                pRec += stride;
            }

            for (x = startX; x < endX; x++)
            {
                pUpBuff[x] = getSign(pRec[x] - tmpU[x - 2]);
            }

            for (y = startY; y < endY; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride + startX] - tmpL[y * 2]);
                pUpBufft[startX + 1] = getSign(pRec[stride + startX + 1] - tmpL[y * 2 + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride + 2]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + stride + 3]);
                    edgeTypeCb = signDownCb + pUpBuff[x] + 2;
                    edgeTypeCr = signDownCr + pUpBuff[x + 1] + 2;
                    pUpBufft[x + 2] = -signDownCb;
                    pUpBufft[x + 3] = -signDownCr;
                    pRec[x] = m_ClipTableChroma[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = m_ClipTableChroma[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }

                swapPtr  = pUpBuff;
                pUpBuff  = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }
            break;
        }
    case SAO_EO_3: // dir: 45
        {
            startX = (LPelX == 0) ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 2 : LCUWidth;

            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (startY == 1)
            {
                pRec += stride;
            }

            for (x = startX - 2; x < endX; x++)
            {
                tmpUpBuff1[x + 2] = getSign(pRec[x] - tmpU[x + 2]);
            }

            for (y = startY; y < endY; y++)
            {
                x = startX;
                signDownCb = getSign(pRec[x] - tmpL[y * 2 + 2]);
                signDownCr = getSign(pRec[x + 1] - tmpL[y * 2 + 3]);
                edgeTypeCb =  signDownCb + tmpUpBuff1[x + 2] + 2;
                edgeTypeCr =  signDownCr + tmpUpBuff1[x + 3] + 2;
                tmpUpBuff1[x] = -signDownCb;
                tmpUpBuff1[x + 1] = -signDownCr;
                pRec[x] = m_ClipTableChroma[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                pRec[x + 1] = m_ClipTableChroma[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];

                for (x = startX + 2; x < endX; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride - 2]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + stride - 1]);
                    edgeTypeCb =  signDownCb + tmpUpBuff1[x + 2] + 2;
                    edgeTypeCr =  signDownCr + tmpUpBuff1[x + 3] + 2;
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;
                    pRec[x] = m_ClipTableChroma[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = m_ClipTableChroma[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }

                tmpUpBuff1[endX] = getSign(pRec[endX - 2 + stride] - pRec[endX]);
                tmpUpBuff1[endX + 1] = getSign(pRec[endX - 1 + stride] - pRec[endX + 1]);

                pRec += stride;
            }
            break;
        }
    case SAO_BO: // band offset
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x += 2)
                {
                    pRec[x] = pOffsetBoCb[pRec[x]];
                    pRec[x + 1] = pOffsetBoCr[pRec[x + 1]];
                }
                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

// Apply SAO filtering to NV12 chroma plane. Filter may be disabled across slices or
// tiles so neighbour availability has to be checked for every CTB.
// HEVC spec 8.7.3.
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::processSaoCuChroma(Ipp32s addr, Ipp32s saoType, PlaneType* tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    MFX_HEVC_PP::CTBBorders pbBorderAvail = pTmpCu->m_AvailBorder;
    PlaneType* pRec;
    Ipp32s tmpUpBuff1[66];
    Ipp32s tmpUpBuff2[66];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUSize;
    Ipp32s LCUHeight = m_MaxCUSize;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeftCb, signLeftCr;
    Ipp32s signRightCb, signRightCr;
    Ipp32s signDownCb, signDownCr;
    Ipp32u edgeTypeCb, edgeTypeCr;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    PlaneType* startPtr;
    PlaneType* tmpU;
    PlaneType* pClipTbl = m_ClipTableChroma;
    Ipp32s *pOffsetEoCb = m_OffsetEoChroma;
    Ipp32s *pOffsetEoCr = m_OffsetEo2Chroma;
    PlaneType* pOffsetBoCb = m_OffsetBoChroma;
    PlaneType* pOffsetBoCr = m_OffsetBo2Chroma;
    Ipp32s startStride;

    picWidthTmp  = m_PicWidth;
    picHeightTmp = m_PicHeight >> m_sps->chromaShiftH;
    LCUWidth     = LCUWidth;
    LCUHeight    = LCUHeight   >> m_sps->chromaShiftH;
    LPelX        = LPelX;
    TPelY        = TPelY       >> m_sps->chromaShiftH;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    pRec      = (PlaneType* )m_Frame->GetCbCrAddr(addr);
    stride    = m_Frame->pitch_chroma();

    tmpU = m_TmpU[0] + LPelX + picWidthTmp;

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            if (pbBorderAvail.m_left)
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 2;
            }
            else
            {
                startX = 2;
                startPtr = pRec;
                startStride = stride;
            }

            endX = pbBorderAvail.m_right ? LCUWidth : LCUWidth - 2;

            for (y = 0; y < LCUHeight; y++)
            {
                signLeftCb = getSign(pRec[startX] - startPtr[y * startStride]);
                signLeftCr = getSign(pRec[startX + 1] - startPtr[y * startStride + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    signRightCb = getSign(pRec[x] - pRec[x + 2]);
                    signRightCr = getSign(pRec[x + 1] - pRec[x + 3]);
                    edgeTypeCb = signRightCb + signLeftCb + 2;
                    edgeTypeCr = signRightCr + signLeftCr + 2;
                    signLeftCb = -signRightCb;
                    signLeftCr = -signRightCr;

                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
                pRec  += stride;
            }
            break;
        }
    case SAO_EO_1: // dir: |
        {
            if (pbBorderAvail.m_top)
            {
                startY = 0;
                startPtr = tmpU;
            }
            else
            {
                startY = 1;
                startPtr = pRec;
                pRec += stride;
            }

            endY = pbBorderAvail.m_bottom ? LCUHeight : LCUHeight - 1;

            for (x = 0; x < LCUWidth; x++)
            {
                tmpUpBuff1[x] = getSign(pRec[x] - startPtr[x]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + 1 + stride]);
                    edgeTypeCb = signDownCb + tmpUpBuff1[x] + 2;
                    edgeTypeCr = signDownCr + tmpUpBuff1[x + 1] + 2;
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;

                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
                pRec += stride;
            }
            break;
        }
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            if (pbBorderAvail.m_left)
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 2;
            }
            else
            {
                startX = 2;
                startPtr = pRec;
                startStride = stride;
            }

            endX = pbBorderAvail.m_right ? LCUWidth : LCUWidth - 2;

            //prepare 2nd line upper sign
            pUpBuff[startX] = getSign(pRec[startX + stride] - startPtr[0]);
            pUpBuff[startX + 1] = getSign(pRec[startX + 1 + stride] - startPtr[1]);
            for (x = startX; x < endX; x++)
            {
                pUpBuff[x + 2] = getSign(pRec[x + stride + 2] - pRec[x]);
            }

            //1st line
            if (pbBorderAvail.m_top_left)
            {
                edgeTypeCb = getSign(pRec[0] - tmpU[-2]) - pUpBuff[2] + 2;
                edgeTypeCr = getSign(pRec[1] - tmpU[-1]) - pUpBuff[3] + 2;
                pRec[0] = pClipTbl[pRec[0] + pOffsetEoCb[edgeTypeCb]];
                pRec[1] = pClipTbl[pRec[1] + pOffsetEoCr[edgeTypeCr]];
            }

            if (pbBorderAvail.m_top)
            {
                for (x = 2; x < endX; x += 2)
                {
                    edgeTypeCb = getSign(pRec[x] - tmpU[x - 2]) - pUpBuff[x + 2] + 2;
                    edgeTypeCr = getSign(pRec[x + 1] - tmpU[x - 1]) - pUpBuff[x + 3] + 2;
                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride + startX] - startPtr[y * startStride]);
                pUpBufft[startX + 1] = getSign(pRec[stride + startX + 1] - startPtr[y * startStride + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride + 2]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + 1 + stride + 2]);
                    edgeTypeCb = signDownCb + pUpBuff[x] + 2;
                    edgeTypeCr = signDownCr + pUpBuff[x + 1] + 2;
                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];

                    pUpBufft[x + 2] = -signDownCb;
                    pUpBufft[x + 3] = -signDownCr;
                }

                swapPtr = pUpBuff;
                pUpBuff = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }

            //last line
            if (pbBorderAvail.m_bottom)
            {
                for (x = startX; x < LCUWidth - 2; x += 2)
                {
                    edgeTypeCb = getSign(pRec[x] - pRec[x + stride + 2]) + pUpBuff[x] + 2;
                    edgeTypeCr = getSign(pRec[x + 1] - pRec[x + 1 + stride + 2]) + pUpBuff[x + 1] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1]  = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
            }

            if (pbBorderAvail.m_bottom_right)
            {
                x = LCUWidth - 2;
                edgeTypeCb = getSign(pRec[x] - pRec[x + stride + 2]) + pUpBuff[x] + 2;
                edgeTypeCr = getSign(pRec[x + 1] - pRec[x + 1 + stride + 2]) + pUpBuff[x + 1] + 2;
                pRec[x]  = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                pRec[x + 1]  = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
            }
            break;
        }
    case SAO_EO_3: // dir: 45
        {
            if (pbBorderAvail.m_left)
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 2;
            }
            else
            {
                startX = 2;
                startPtr = pRec;
                startStride = stride;
            }

            endX = pbBorderAvail.m_right ? LCUWidth : LCUWidth - 2;

            //prepare 2nd line upper sign
            tmpUpBuff1[startX] = getSign(startPtr[startStride] - pRec[startX]);
            tmpUpBuff1[startX + 1] = getSign(startPtr[startStride + 1] - pRec[startX + 1]);
            for (x = startX; x < endX; x++)
            {
                tmpUpBuff1[x + 2] = getSign(pRec[x + stride] - pRec[x + 2]);
            }

            //first line
            if (pbBorderAvail.m_top)
            {
                for (x = startX; x < LCUWidth - 2; x += 2)
                {
                    edgeTypeCb = getSign(pRec[x] - tmpU[x + 2]) - tmpUpBuff1[x] + 2;
                    edgeTypeCr = getSign(pRec[x + 1] - tmpU[x + 3]) - tmpUpBuff1[x + 1] + 2;
                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
            }

            if (pbBorderAvail.m_top_right)
            {
                x = LCUWidth - 2;
                edgeTypeCb = getSign(pRec[x] - tmpU[x + 2]) - tmpUpBuff1[x] + 2;
                edgeTypeCr = getSign(pRec[x + 1] - tmpU[x + 3]) - tmpUpBuff1[x + 1] + 2;
                pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                signDownCb = getSign(pRec[startX] - startPtr[(y + 1) * startStride]);
                signDownCr = getSign(pRec[startX + 1] - startPtr[(y + 1) * startStride + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    edgeTypeCb = signDownCb + tmpUpBuff1[x + 2] + 2;
                    edgeTypeCr = signDownCr + tmpUpBuff1[x + 3] + 2;
                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;
                    signDownCb = getSign(pRec[x + 2] - pRec[x + stride]);
                    signDownCr = getSign(pRec[x + 3] - pRec[x + 1 + stride]);
                }

                tmpUpBuff1[endX] = -signDownCb;
                tmpUpBuff1[endX + 1] = -signDownCr;

                pRec  += stride;
            }

            //last line
            if (pbBorderAvail.m_bottom_left)
            {
                edgeTypeCb = getSign(pRec[0] - pRec[stride - 2]) + tmpUpBuff1[2] + 2;
                edgeTypeCr = getSign(pRec[1] - pRec[stride - 1]) + tmpUpBuff1[3] + 2;
                pRec[0] = pClipTbl[pRec[0] + pOffsetEoCb[edgeTypeCb]];
                pRec[1] = pClipTbl[pRec[1] + pOffsetEoCr[edgeTypeCr]];
            }

            if (pbBorderAvail.m_bottom)
            {
                for (x = 2; x < endX; x += 2)
                {
                    edgeTypeCb = getSign(pRec[x] - pRec[x + stride - 2]) + tmpUpBuff1[x + 2] + 2;
                    edgeTypeCr = getSign(pRec[x + 1] - pRec[x + stride - 1]) + tmpUpBuff1[x + 3] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1]  = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
            }
            break;
        }
    case SAO_BO: // band offset
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x += 2)
                {
                    pRec[x] = pOffsetBoCb[pRec[x]];
                    pRec[x + 1] = pOffsetBoCr[pRec[x + 1]];
                }

                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

// Apply SAO filter to a frame
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::SAOProcess(H265DecoderFrame* pFrame, Ipp32s startCU, Ipp32s toProcessCU)
{
    if (!pFrame->GetAU()->IsNeedSAO())
        return;

    m_Frame = pFrame;
    H265Slice * slice = m_Frame->GetAU()->GetAnySlice();
    m_sps = slice->GetSeqParam();

    processSaoUnits(startCU, toProcessCU);
}

// Apply SAO filter to a line or part of line of CTBs in a frame
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::processSaoLine(SAOLCUParam* saoLCUParam, Ipp32s firstCU, Ipp32s endCU)
{
    Ipp32s frameWidthInCU = m_Frame->getFrameWidthInCU();
    Ipp32s LCUWidth = m_MaxCUSize;
    Ipp32s LCUHeight = m_MaxCUSize;
    Ipp32s picHeightTmp = m_PicHeight;

    Ipp32s CUHeight = LCUHeight + 1;
    Ipp32s CUHeightChroma = (LCUHeight >> m_sps->chromaShiftH) + 1;
    Ipp32s idxY = firstCU / frameWidthInCU;

    if ((idxY * LCUHeight + CUHeight) > picHeightTmp)
    {
        CUHeight = picHeightTmp - idxY * LCUHeight;
        CUHeightChroma = CUHeight >> m_sps->chromaShiftH;
    }

    PlaneType* rec = (PlaneType*)m_Frame->GetLumaAddr(firstCU);
    PlaneType* recChroma = (PlaneType*)m_Frame->GetCbCrAddr(firstCU);
    Ipp32s stride = m_Frame->pitch_luma();

    // Save a column of left samples
    if ((firstCU % frameWidthInCU) == 0)
    {
        for (Ipp32s i = 0; i < CUHeight; i++)
        {
            m_TmpL[0][i] = rec[0];
            rec += stride;
        }

        if (m_sps->ChromaArrayType != CHROMA_FORMAT_400)
        {
            for (Ipp32s i = 0; i < CUHeightChroma; i++)
            {
                m_TmpL[0][SAO_PRED_SIZE + i*2] = recChroma[0];
                m_TmpL[0][SAO_PRED_SIZE + i*2 + 1] = recChroma[1];
                recChroma += stride;
            }
        }
    }

    for (Ipp32s addr = firstCU; addr < endCU; addr++)
    {
        // Save a column of right most samples of this CTB
        if ((addr % frameWidthInCU) != (frameWidthInCU - 1))
        {
            rec  = (PlaneType*)m_Frame->GetLumaAddr(addr);
            recChroma = (PlaneType*)m_Frame->GetCbCrAddr(addr);
            for (Ipp32s i = 0; i < CUHeight; i++)
            {
                m_TmpL[1][i] = rec[i*stride+LCUWidth-1];
            }

            if (m_sps->ChromaArrayType != CHROMA_FORMAT_400)
            {
                for (Ipp32s i = 0; i < CUHeightChroma; i++)
                {
                    m_TmpL[1][SAO_PRED_SIZE + i*2] = recChroma[i * stride + LCUWidth - 2];
                    m_TmpL[1][SAO_PRED_SIZE + i*2 + 1] = recChroma[i * stride + LCUWidth - 1];
                }
            }
        }

        H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
        if (!pTmpCu->m_Frame)
            continue;

        bool isNeedSAOLuma = pTmpCu->m_SliceHeader->slice_sao_luma_flag && saoLCUParam[addr].m_typeIdx[0] >= 0;
        bool isNeedSAOChroma = pTmpCu->m_SliceHeader->slice_sao_chroma_flag && saoLCUParam[addr].m_typeIdx[1] >= 0;

        // Save samples that may require to skip filtering
        if (m_needPCMRestoration && (isNeedSAOLuma || isNeedSAOChroma))
            PCMCURestoration(pTmpCu, 0, 0, false);

        if (isNeedSAOLuma)
        {
            Ipp32s typeIdx = saoLCUParam[addr].m_typeIdx[0];
            if (!saoLCUParam[addr].sao_merge_left_flag)
            {
                SetOffsetsLuma(saoLCUParam[addr], typeIdx);
            }

            if (!m_UseNIF)
            {
                MFX_HEVC_PP::h265_ProcessSaoCuOrg_Luma(
                    (PlaneType*)m_Frame->GetLumaAddr(addr),
                    m_Frame->pitch_luma(),
                    typeIdx,
                    m_TmpL[0],
                    &(m_TmpU[0][pTmpCu->m_CUPelX]),
                    m_MaxCUSize,
                    m_MaxCUSize,
                    m_PicWidth,
                    m_PicHeight,
                    m_OffsetEo,
                    m_OffsetBo,
                    m_ClipTable,
                    pTmpCu->m_CUPelX,
                    pTmpCu->m_CUPelY);
            }
            else
            {
                h265_ProcessSaoCu_Luma(
                    (PlaneType*)m_Frame->GetLumaAddr(addr),
                    m_Frame->pitch_luma(),
                    typeIdx,
                    m_TmpL[0],
                    &(m_TmpU[0][pTmpCu->m_CUPelX]),
                    m_MaxCUSize,
                    m_MaxCUSize,
                    m_PicWidth,
                    m_PicHeight,
                    m_OffsetEo,
                    m_OffsetBo,
                    m_ClipTable,
                    pTmpCu->m_CUPelX,
                    pTmpCu->m_CUPelY,
                    pTmpCu->m_AvailBorder);
            }
        }

        if (isNeedSAOChroma)
        {
            Ipp32s typeIdx = saoLCUParam[addr].m_typeIdx[1];

            if (!saoLCUParam[addr].sao_merge_left_flag)
            {
                SetOffsetsChroma(saoLCUParam[addr], typeIdx);
            }

            if (!m_UseNIF)
                processSaoCuOrgChroma(addr, typeIdx, m_TmpL[0] + SAO_PRED_SIZE);
            else
                processSaoCuChroma(addr, typeIdx, m_TmpL[0] + SAO_PRED_SIZE);
        }

        // Restore samples that require to skip filtering
        if (m_needPCMRestoration && (isNeedSAOLuma || isNeedSAOChroma))
            PCMCURestoration(pTmpCu, 0, 0, true);

        std::swap(m_TmpL[0], m_TmpL[1]);
    }
}

// Apply SAO filter to a range of CTBs
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::processSaoUnits(Ipp32s firstCU, Ipp32s toProcessCU)
{
    H265Slice * slice = m_Frame->GetAU()->GetAnySlice();
    SAOLCUParam* saoLCUParam = m_Frame->getCD()->m_saoLcuParam;

    Ipp32s frameWidthInCU = m_Frame->getFrameWidthInCU();
    Ipp32s frameHeightInCU = m_Frame->getFrameHeightInCU();
    Ipp32s LCUHeight = m_MaxCUSize;
    Ipp32s picWidthTmp = m_PicWidth;

    m_slice_sao_luma_flag = slice->GetSliceHeader()->slice_sao_luma_flag != 0;
    m_slice_sao_chroma_flag = slice->GetSliceHeader()->slice_sao_chroma_flag != 0;

    m_needPCMRestoration = (slice->GetSeqParam()->pcm_enabled_flag && slice->GetSeqParam()->pcm_loop_filter_disabled_flag) ||
        slice->GetPicParam()->transquant_bypass_enabled_flag;

    m_SaoBitIncreaseY = slice->GetPicParam()->log2_sao_offset_scale_luma;
    m_SaoBitIncreaseC = slice->GetPicParam()->log2_sao_offset_scale_chroma;

    createNonDBFilterInfo();

    Ipp32s endCU = firstCU + toProcessCU;

    // Save top line samples
    if (!firstCU)// / frameWidthInCU == 0)
    {
        Ipp32s width = picWidthTmp;
        MFX_INTERNAL_CPY(m_TmpU[0] + firstCU*LCUHeight, m_Frame->m_pYPlane + firstCU*LCUHeight, sizeof(PlaneType) * width);
        MFX_INTERNAL_CPY(m_TmpU[0] + picWidthTmp + firstCU*LCUHeight, m_Frame->m_pUVPlane + firstCU*LCUHeight, sizeof(PlaneType) * width);
    }

    bool independentTileBoundaryForNDBFilter = false;
    if (slice->GetPicParam()->getNumTiles() > 1 && !slice->GetPicParam()->loop_filter_across_tiles_enabled_flag)
    {
        independentTileBoundaryForNDBFilter = true;
    }

    for (; ; )
    {
        Ipp32s endAddr = IPP_MIN(firstCU + toProcessCU,
                      firstCU -
                      (firstCU % frameWidthInCU) +
                      frameWidthInCU);

        // Save samples at the bottom of CTB row
        if ((firstCU % frameWidthInCU) == 0 && (firstCU / frameWidthInCU) != (frameHeightInCU - 1))
        {
            PlaneType* pRec = (PlaneType*)m_Frame->GetLumaAddr(firstCU) + (LCUHeight - 1)*m_Frame->pitch_luma();
            MFX_INTERNAL_CPY(m_TmpU[1], pRec, sizeof(PlaneType) * picWidthTmp);

            pRec = (PlaneType*)m_Frame->GetCbCrAddr(firstCU) + ((LCUHeight >> m_sps->chromaShiftH)- 1)*m_Frame->pitch_chroma();
            MFX_INTERNAL_CPY(m_TmpU[1] + picWidthTmp, pRec, sizeof(PlaneType) * picWidthTmp);
        }

        // Get slice and tile boundaries if needed
        if (m_UseNIF)
        {
            for (Ipp32s j = firstCU; j < endAddr; j++)
            {
                m_Frame->getCU(j)->setNDBFilterBlockBorderAvailability(independentTileBoundaryForNDBFilter);
            }
        }

        // Filter CTB line
        processSaoLine(saoLCUParam, firstCU, endAddr);

        firstCU = endAddr;

        if ((firstCU % frameWidthInCU) == 0)
            std::swap(m_TmpU[0], m_TmpU[1]);

        if (firstCU >= endCU)
        {
            break;
        }
    }
}

// Calculate SAO lookup tables for luma offsets from bitstream data
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::SetOffsetsLuma(SAOLCUParam  &saoLCUParam, Ipp32s typeIdx)
{
    Ipp32s offset[LUMA_GROUP_NUM + 1];

    Ipp32u saoBitIncrease = m_SaoBitIncreaseY;

    if (typeIdx == SAO_BO)
    {
        for (Ipp32s i = 0; i < SAO_MAX_BO_CLASSES + 1; i++)
        {
            offset[i] = 0;
        }
        for (Ipp32s i = 0; i < SAO_OFFSETS_LEN; i++)
        {
            offset[(saoLCUParam.m_subTypeIdx[0] + i) % SAO_MAX_BO_CLASSES + 1] = saoLCUParam.m_offset[0][i] << saoBitIncrease;
        }

        PlaneType *ppLumaTable = m_lumaTableBo;
        for (Ipp32s i = 0; i < (1 << m_sps->bit_depth_luma); i++)
        {
            m_OffsetBo[i] = (PlaneType)m_ClipTable[i + offset[ppLumaTable[i]]];
        }
    }
    else
    {
        offset[0] = 0;
        for (Ipp32s i = 0; i < SAO_OFFSETS_LEN; i++)
        {
            offset[i + 1] = saoLCUParam.m_offset[0][i] << saoBitIncrease;
        }
        for (Ipp32s edgeType = 0; edgeType < 6; edgeType++)
        {
            m_OffsetEo[edgeType] = offset[EoTable[edgeType]];
        }
    }
}

// Calculate SAO lookup tables for chroma offsets from bitstream data
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::SetOffsetsChroma(SAOLCUParam &saoLCUParam, Ipp32s typeIdx)
{
    Ipp32s offsetCb[LUMA_GROUP_NUM + 1];
    Ipp32s offsetCr[LUMA_GROUP_NUM + 1];

    Ipp32u saoBitIncrease = m_SaoBitIncreaseC;

    if (typeIdx == SAO_BO)
    {
        for (Ipp32s i = 0; i < SAO_MAX_BO_CLASSES + 1; i++)
        {
            offsetCb[i] = 0;
            offsetCr[i] = 0;
        }
        for (Ipp32s i = 0; i < SAO_OFFSETS_LEN; i++)
        {
            offsetCb[(saoLCUParam.m_subTypeIdx[1] + i) % SAO_MAX_BO_CLASSES + 1] = saoLCUParam.m_offset[1][i] << saoBitIncrease;
            offsetCr[(saoLCUParam.m_subTypeIdx[2] + i) % SAO_MAX_BO_CLASSES + 1] = saoLCUParam.m_offset[2][i] << saoBitIncrease;
        }

        PlaneType*ppLumaTable = m_chromaTableBo;
        for (Ipp32s i = 0; i < (1 << m_sps->bit_depth_chroma); i++)
        {
            m_OffsetBoChroma[i] = (PlaneType)m_ClipTableChroma[i + offsetCb[ppLumaTable[i]]];
            m_OffsetBo2Chroma[i] = (PlaneType)m_ClipTableChroma[i + offsetCr[ppLumaTable[i]]];
        }
    }
    else
    {
        offsetCb[0] = 0;
        offsetCr[0] = 0;

        for (Ipp32s i = 0; i < SAO_OFFSETS_LEN; i++)
        {
            offsetCb[i + 1] = saoLCUParam.m_offset[1][i] << saoBitIncrease;
            offsetCr[i + 1] = saoLCUParam.m_offset[2][i] << saoBitIncrease;
        }
        for (Ipp32s edgeType = 0; edgeType < 6; edgeType++)
        {
            m_OffsetEoChroma[edgeType] = offsetCb[EoTable[edgeType]];
            m_OffsetEo2Chroma[edgeType] = offsetCr[EoTable[edgeType]];
        }
    }
}

// Calculate whether it is necessary to check slice and tile boundaries because of
// filtering restrictions in some slice of the frame.
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::createNonDBFilterInfo()
{
    bool independentSliceBoundaryForNDBFilter = false;
    bool independentTileBoundaryForNDBFilter = false;

    H265Slice* pSlice = m_Frame->GetAU()->GetSlice(0);

    if (pSlice->GetPicParam()->getNumTiles() > 1 && !pSlice->GetPicParam()->loop_filter_across_tiles_enabled_flag)
    {
        independentTileBoundaryForNDBFilter = true;
    }

    if (m_Frame->GetAU()->GetSliceCount() > 1)
    {
        for (Ipp32u i = 0; i < m_Frame->GetAU()->GetSliceCount(); i++)
        {
            H265Slice* slice = m_Frame->GetAU()->GetSlice(i);

            if (!slice->GetSliceHeader()->slice_loop_filter_across_slices_enabled_flag)
            {
                independentSliceBoundaryForNDBFilter = true;
            }

            if (slice->GetPicParam()->getNumTiles() > 1 && !slice->GetPicParam()->loop_filter_across_tiles_enabled_flag)
            {
                independentTileBoundaryForNDBFilter = true;
            }
        }
    }

    m_UseNIF = independentSliceBoundaryForNDBFilter || independentTileBoundaryForNDBFilter;
}

// Restore parts of CTB encoded with PCM or transquant bypass if filtering should be disabled for them
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::PCMCURestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth, bool restore)
{
    Ipp32u CurNumParts = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u QNumParts   = CurNumParts >> 2;

    // go to sub-CU
    if(pcCU->GetDepth(AbsZorderIdx) > Depth)
    {
        for (Ipp32u PartIdx = 0; PartIdx < 4; PartIdx++, AbsZorderIdx += QNumParts)
        {
            Ipp32u LPelX = pcCU->m_CUPelX + pcCU->m_rasterToPelX[AbsZorderIdx];
            Ipp32u TPelY = pcCU->m_CUPelY + pcCU->m_rasterToPelY[AbsZorderIdx];
            if((LPelX < m_sps->pic_width_in_luma_samples) && (TPelY < m_sps->pic_height_in_luma_samples))
                PCMCURestoration(pcCU, AbsZorderIdx, Depth + 1, restore);
        }
        return;
    }

    // restore PCM samples
    if ((pcCU->GetIPCMFlag(AbsZorderIdx) && m_sps->pcm_loop_filter_disabled_flag) || pcCU->isLosslessCoded(AbsZorderIdx))
    {
        PCMSampleRestoration(pcCU, AbsZorderIdx, Depth, restore);
    }
}

Ipp32s GetAddrOffset(H265FrameCodingData * cd, Ipp32u PartUnitIdx, Ipp32u width)
{
    Ipp32s blkX = cd->m_partitionInfo.m_rasterToPelX[PartUnitIdx];
    Ipp32s blkY = cd->m_partitionInfo.m_rasterToPelY[PartUnitIdx];

    return blkX + blkY * width;
}

// Save/restore losless coded samples from temporary buffer back to frame
template<typename PlaneType>
void H265SampleAdaptiveOffsetTemplate<PlaneType>::PCMSampleRestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth, bool restore)
{
    H265FrameCodingData * cd = m_Frame->getCD();

    PlaneType *src = (PlaneType *)m_Frame->GetLumaAddr(pcCU->CUAddr, AbsZorderIdx);

    Ipp32u pitch  = m_Frame->pitch_luma();
    Ipp32u width = (cd->m_MaxCUWidth >> Depth);

    PlaneType *pcm = m_tempPCMBuffer + GetAddrOffset(cd, AbsZorderIdx, 64);

    if (restore)
    {
        for (Ipp32u Y = 0; Y < width; Y++)
        {
            for (Ipp32u X = 0; X < width; X++)
                src[X] = pcm[X];
            pcm += 64;
            src += pitch;
        }
    }
    else
    {
        for (Ipp32u Y = 0; Y < width; Y++)
        {
            for (Ipp32u X = 0; X < width; X++)
                pcm[X] = src[X];
            pcm += 64;
            src += pitch;
        }
    }

    src = (PlaneType *)m_Frame->GetCbCrAddr(pcCU->CUAddr, AbsZorderIdx);
    PlaneType * pcmChroma = m_tempPCMBuffer + 64*64 + GetAddrOffset(cd, AbsZorderIdx, 32);

    pitch = m_Frame->pitch_chroma();
    width >>= 1;

    if (restore)
    {
        for(Ipp32u Y = 0; Y < width; Y++)
        {
            for(Ipp32u X = 0; X < 2*width; X++)
                src[X] = pcmChroma[X];

            pcmChroma += 64;
            src += pitch;
        }
    }
    else
    {
        for(Ipp32u Y = 0; Y < width; Y++)
        {
            for(Ipp32u X = 0; X < 2*width; X++)
                pcmChroma[X] = src[X];

            pcmChroma += 64;
            src += pitch;
        }
    }
}

// SAO context constructor
H265SampleAdaptiveOffset::H265SampleAdaptiveOffset()
{
    m_base = 0;
    m_is10bits = false;
    m_isInitialized = false;
}

// SAO context cleanup
void H265SampleAdaptiveOffset::destroy()
{
    delete m_base;
    m_base = 0;
    m_isInitialized = false;
    m_is10bits = false;
}

// Initialize SAO context
void H265SampleAdaptiveOffset::init(const H265SeqParamSet* sps)
{
    if (!sps->sample_adaptive_offset_enabled_flag)
        return;

    bool is10bits = sps->need16bitOutput != 0;
    if (m_is10bits == is10bits && m_isInitialized)
    {
        if (m_base->isNeedReInit(sps))
        {
            m_base->init(sps);
        }
        return;
    }

    destroy();
    m_is10bits = is10bits;

    if (m_is10bits)
    {
        m_base = new H265SampleAdaptiveOffsetTemplate<Ipp16u>();
    }
    else
    {
        m_base = new H265SampleAdaptiveOffsetTemplate<Ipp8u>();
    }

    m_base->init(sps);
    m_isInitialized = true;
}

// Apply SAO filter to a target frame CTB range
void H265SampleAdaptiveOffset::SAOProcess(H265DecoderFrame* pFrame, Ipp32s startCU, Ipp32s toProcessCU)
{
    m_base->SAOProcess(pFrame, startCU, toProcessCU);
}

} // end namespace UMC_HEVC_DECODER

#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
