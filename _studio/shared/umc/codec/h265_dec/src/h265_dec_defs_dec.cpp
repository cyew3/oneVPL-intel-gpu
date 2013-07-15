/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "h265_global_rom.h"
#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_dec_defs_yuv.h"
#include "umc_h265_frame.h"
#include "umc_h265_frame_info.h"
#include "umc_h265_slice_decoding.h"
#include "assert.h"
#include "math.h"

namespace UMC_HEVC_DECODER
{

/** get the sign of input variable
 * \param   x
 */
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

H265SampleAdaptiveOffset::H265SampleAdaptiveOffset()
{
    m_ClipTable = NULL;
    m_ClipTableBase = NULL;
    m_OffsetBo = NULL;
    m_lumaTableBo = NULL;

    m_TmpU1 = NULL;
    m_TmpU2 = NULL;

    m_isInitialized = false;
}

void H265SampleAdaptiveOffset::init(Ipp32s Width, Ipp32s Height, Ipp32s MaxCUWidth, Ipp32s MaxCUHeight)
{
    Ipp32u i;

    m_PicWidth  = Width;
    m_PicHeight = Height;

    m_MaxCUWidth  = MaxCUWidth;
    m_MaxCUHeight = MaxCUHeight;

    Ipp32u uiPixelRangeY = 1 << g_bitDepthY;
    Ipp32u uiBoRangeShiftY = g_bitDepthY - SAO_BO_BITS;

    m_lumaTableBo = new H265PlaneYCommon[uiPixelRangeY];
    for (Ipp32u k2 = 0; k2 < uiPixelRangeY; k2++)
    {
        m_lumaTableBo[k2] = (H265PlaneYCommon)(1 + (k2>>uiBoRangeShiftY));
    }

    Ipp32u uiMaxY  = (1 << g_bitDepthY) - 1;
    Ipp32u uiMinY  = 0;

    Ipp32u iCRangeExt = uiMaxY>>1;

    m_ClipTableBase = new H265PlaneYCommon[uiMaxY+2*iCRangeExt];
    m_OffsetBo      = new H265PlaneYCommon[uiMaxY+2*iCRangeExt];

    for (i = 0; i < (uiMinY + iCRangeExt);i++)
    {
        m_ClipTableBase[i] = (H265PlaneYCommon)uiMinY;
    }

    for (i = uiMinY + iCRangeExt; i < (uiMaxY + iCRangeExt); i++)
    {
        m_ClipTableBase[i] = (H265PlaneYCommon)(i - iCRangeExt);
    }

    for (i= uiMaxY + iCRangeExt; i < (uiMaxY + 2 * iCRangeExt); i++)
    {
        m_ClipTableBase[i] = (H265PlaneYCommon)uiMaxY;
    }

    m_ClipTable = &(m_ClipTableBase[iCRangeExt]);

    m_TmpU1 = new H265PlaneYCommon [m_PicWidth];
    m_TmpU2 = new H265PlaneYCommon [m_PicWidth];

    m_isInitialized = true;
}

void H265SampleAdaptiveOffset::destroy()
{
    if (m_ClipTableBase)
    {
        delete [] m_ClipTableBase; m_ClipTableBase = NULL;
    }
    if (m_OffsetBo)
    {
        delete [] m_OffsetBo; m_OffsetBo = NULL;
    }
    if (m_lumaTableBo)
    {
        delete[] m_lumaTableBo; m_lumaTableBo = NULL;
    }
    if (m_TmpU1)
    {
        delete [] m_TmpU1; m_TmpU1 = NULL;
    }
    if (m_TmpU2)
    {
        delete [] m_TmpU2; m_TmpU2 = NULL;
    }

    m_isInitialized = false;
}

void H265SampleAdaptiveOffset::init(H265SeqParamSet* pSPS)
{
    if (pSPS->sample_adaptive_offset_enabled_flag)
    {
        this->init(pSPS->pic_width_in_luma_samples, pSPS->pic_height_in_luma_samples, pSPS->MaxCUWidth, pSPS->MaxCUHeight);
    }
}

void H265PicParamSetBase::getColumnWidth(Ipp16u *columnWidth) const
{
    for (unsigned i = 0; i < num_tile_columns - 1; i++)
        columnWidth[i] = (Ipp16u)m_ColumnWidthArray[i];
}

void H265PicParamSetBase::getColumnWidthMinus1(Ipp16u *columnWidth) const
{
    for (unsigned i = 0; i < num_tile_columns - 1; i++)
        columnWidth[i] = (Ipp16u)(m_ColumnWidthArray[i] - 1);
}

void H265PicParamSetBase::setColumnWidth(Ipp32u* columnWidth)
{
    if (uniform_spacing_flag == 0)
    {
        m_ColumnWidthArray = new Ipp32u[num_tile_columns];
        if (NULL == m_ColumnWidthArray)
            return;

        for (Ipp32u i = 0; i < num_tile_columns - 1; i++)
        {
            m_ColumnWidthArray[i] = columnWidth[i];
        }
    }
}

void H265PicParamSetBase::getRowHeight(Ipp16u* rowHeight) const
{
    for (unsigned i = 0; i < num_tile_rows - 1; i++)
        rowHeight[i] = (Ipp16u)m_RowHeightArray[i];
}

void H265PicParamSetBase::getRowHeightMinus1(Ipp16u* rowHeight) const
{
    for (unsigned i = 0; i < num_tile_rows - 1; i++)
        rowHeight[i] = (Ipp16u)(m_RowHeightArray[i] - 1);
}

void H265PicParamSetBase::setRowHeight(Ipp32u* rowHeight)
{
    if (uniform_spacing_flag == 0)
    {
        m_RowHeightArray = new Ipp32u[num_tile_rows];

        for (Ipp32u i = 0; i < num_tile_rows - 1; i++)
        {
            m_RowHeightArray[i] = rowHeight[i];
        }
    }
}

ReferencePictureSet::ReferencePictureSet()
{
  ::memset(this, 0, sizeof(*this));
}

void ReferencePictureSet::sortDeltaPOC()
{
    Ipp32s temp;
    Ipp32s deltaPOC;
    bool Used;
    // sort in increasing order (smallest first)
    for (Ipp32s j = 1; j < m_NumberOfPictures; j++)
    {
        deltaPOC = m_DeltaPOC[j];
        Used = m_Used[j];
        for (Ipp32s k = j - 1; k >= 0; k--)
        {
            temp = m_DeltaPOC[k]; //modify sort
            if (deltaPOC < temp)
            {
                m_DeltaPOC[k + 1] = temp;
                m_Used[k + 1] = m_Used[k];
                m_DeltaPOC[k] = deltaPOC;
                m_Used[k] = Used;
            }
        }
    }
    // flip the negative values to largest first
    Ipp32s NumNegPics = (Ipp32s) m_NumberOfNegativePictures;
    for (Ipp32s j = 0, k = NumNegPics - 1; j < NumNegPics >> 1; j++, k--)
    {
        deltaPOC = m_DeltaPOC[j];
        Used = m_Used[j];
        m_DeltaPOC[j] = m_DeltaPOC[k];
        m_Used[j] = m_Used[k];
        m_DeltaPOC[k] = deltaPOC;
        m_Used[k] = Used;
    }
}

ReferencePictureSetList::ReferencePictureSetList()
    : m_NumberOfReferencePictureSets(0)
{
}

void ReferencePictureSetList::allocate(unsigned NumberOfReferencePictureSets)
{
    if (m_NumberOfReferencePictureSets == NumberOfReferencePictureSets)
        return;

    m_NumberOfReferencePictureSets = NumberOfReferencePictureSets;
    referencePictureSet.resize(NumberOfReferencePictureSets);
}

void H265SampleAdaptiveOffset::processSaoCuOrgLuma(Ipp32s addr, Ipp32s saoType, H265PlanePtrYCommon tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    H265PlanePtrYCommon pRec;
    Ipp32s tmpUpBuff1[65];
    Ipp32s tmpUpBuff2[65];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeft;
    Ipp32s signRight;
    Ipp32s signDown;
    Ipp32s signDown1;
    Ipp32u edgeType;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    H265PlanePtrYCommon tmpU;

    picWidthTmp  = m_PicWidth;
    picHeightTmp = m_PicHeight;
    LCUWidth     = LCUWidth;
    LCUHeight    = LCUHeight;
    LPelX        = LPelX;
    TPelY        = TPelY;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    pRec      = m_Frame->GetLumaAddr(addr);
    stride    = m_Frame->pitch_luma();

    tmpU = &(m_TmpU1[LPelX]);

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            startX = (LPelX == 0) ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth-1 : LCUWidth;

            for (y = 0; y < LCUHeight; y++)
            {
                signLeft = getSign(pRec[startX] - tmpL[y]);

                for (x = startX; x < endX; x++)
                {
                    signRight =  getSign(pRec[x] - pRec[x+1]);
                    edgeType  =  signRight + signLeft + 2;
                    signLeft  = -signRight;

                    pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];
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
                for (x = 0; x < LCUWidth; x++)
                {
                    signDown    = getSign(pRec[x] - pRec[x+stride]);
                    edgeType    = signDown + tmpUpBuff1[x] + 2;
                    tmpUpBuff1[x]= -signDown;

                    pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];
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

            startX = (LPelX == 0)           ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 1 : LCUWidth;

            startY = (TPelY == 0) ?            1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0)
            {
                pRec += stride;
            }

            for (x = startX; x < endX; x++)
            {
                pUpBuff[x] = getSign(pRec[x] - tmpU[x-1]);
            }

            for (y = startY; y < endY; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride+startX] - tmpL[y]);

                for (x = startX; x < endX; x++)
                {
                    signDown1      =  getSign(pRec[x] - pRec[x+stride+1]) ;
                    edgeType       =  signDown1 + pUpBuff[x] + 2;
                    pUpBufft[x+1]  = -signDown1;
                    pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];
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
            startX = (LPelX == 0) ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 1 : LCUWidth;

            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (startY == 1)
            {
                pRec += stride;
            }

            for (x = startX - 1; x < endX; x++)
            {
                tmpUpBuff1[x+1] = getSign(pRec[x] - tmpU[x+1]);
            }

            for (y = startY; y < endY; y++)
            {
                x = startX;
                signDown1      =  getSign(pRec[x] - tmpL[y+1]) ;
                edgeType       =  signDown1 + tmpUpBuff1[x+1] + 2;
                tmpUpBuff1[x] = -signDown1;
                pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];

                for (x = startX + 1; x < endX; x++)
                {
                    signDown1      =  getSign(pRec[x] - pRec[x+stride-1]) ;
                    edgeType       =  signDown1 + tmpUpBuff1[x+1] + 2;
                    tmpUpBuff1[x]  = -signDown1;
                    pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];
                }

                tmpUpBuff1[endX] = getSign(pRec[endX-1 + stride] - pRec[endX]);

                pRec += stride;
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    pRec[x] = m_OffsetBo[pRec[x]];
                }
                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

void H265SampleAdaptiveOffset::processSaoCuLuma(Ipp32s addr, Ipp32s saoType, H265PlanePtrYCommon tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    bool* pbBorderAvail = pTmpCu->m_AvailBorder;
    H265PlanePtrYCommon pRec;
    Ipp32s tmpUpBuff1[65];
    Ipp32s tmpUpBuff2[65];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeft;
    Ipp32s signRight;
    Ipp32s signDown;
    Ipp32s signDown1;
    Ipp32u edgeType;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    H265PlanePtrYCommon startPtr;
    H265PlanePtrYCommon tmpU;
    H265PlanePtrYCommon pClipTbl = m_ClipTable;
    H265PlanePtrYCommon pOffsetBo = m_OffsetBo;
    Ipp32s startStride;

    picWidthTmp  = m_PicWidth;
    picHeightTmp = m_PicHeight;
    LCUWidth     = LCUWidth;
    LCUHeight    = LCUHeight;
    LPelX        = LPelX;
    TPelY        = TPelY;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    pRec      = m_Frame->GetLumaAddr(addr);
    stride    = m_Frame->pitch_luma();

    tmpU = &(m_TmpU1[LPelX]);

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX   = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth - 1);

            for (y = 0; y < LCUHeight; y++)
            {
                signLeft = getSign(pRec[startX] - startPtr[y*startStride]);

                for (x = startX; x < endX; x++)
                {
                    signRight =  getSign(pRec[x] - pRec[x+1]);
                    edgeType  =  signRight + signLeft + 2;
                    signLeft  = -signRight;

                    pRec[x]   = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
                pRec  += stride;
            }
            break;
        }
    case SAO_EO_1: // dir: |
        {
            if (pbBorderAvail[SGU_T])
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

            endY = (pbBorderAvail[SGU_B]) ? LCUHeight : LCUHeight - 1;

            for (x = 0; x < LCUWidth; x++)
            {
                tmpUpBuff1[x] = getSign(pRec[x] - startPtr[x]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    signDown      = getSign(pRec[x] - pRec[x+stride]);
                    edgeType      = signDown + tmpUpBuff1[x] + 2;
                    tmpUpBuff1[x] = -signDown;

                    pRec[x]      = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
                pRec  += stride;
            }
            break;
        }
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth-1);

            //prepare 2nd line upper sign
            pUpBuff[startX] = getSign(pRec[startX+stride] - startPtr[0]);
            for (x = startX; x < endX; x++)
            {
                pUpBuff[x+1] = getSign(pRec[x+stride+1] - pRec[x]);
            }

            //1st line
            if (pbBorderAvail[SGU_TL])
            {
                edgeType = getSign(pRec[0] - tmpU[-1]) - pUpBuff[1] + 2;
                pRec[0]  = pClipTbl[pRec[0] + m_OffsetEo[edgeType]];
            }

            if (pbBorderAvail[SGU_T])
            {
                for (x = 1; x < endX; x++)
                {
                    edgeType = getSign(pRec[x] - tmpU[x-1]) - pUpBuff[x+1] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride+startX] - startPtr[y*startStride]);

                for (x = startX; x < endX; x++)
                {
                    signDown1 = getSign(pRec[x] - pRec[x+stride+1]);
                    edgeType  =  signDown1 + pUpBuff[x] + 2;
                    pRec[x]   = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];

                    pUpBufft[x+1] = -signDown1;
                }

                swapPtr  = pUpBuff;
                pUpBuff  = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }

            //last line
            if (pbBorderAvail[SGU_B])
            {
                for (x = startX; x < LCUWidth - 1; x++)
                {
                    edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
            }

            if (pbBorderAvail[SGU_BR])
            {
                x = LCUWidth - 1;
                edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                pRec[x]  = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
            }
            break;
        }
    case SAO_EO_3: // dir: 45
        {
            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX   = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth -1);

            //prepare 2nd line upper sign
            tmpUpBuff1[startX] = getSign(startPtr[startStride] - pRec[startX]);
            for (x = startX; x < endX; x++)
            {
                tmpUpBuff1[x+1] = getSign(pRec[x+stride] - pRec[x+1]);
            }

            //first line
            if (pbBorderAvail[SGU_T])
            {
                for (x = startX; x < LCUWidth - 1; x++)
                {
                    edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                    pRec[x] = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
            }

            if (pbBorderAvail[SGU_TR])
            {
                x= LCUWidth - 1;
                edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                pRec[x] = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                signDown1 = getSign(pRec[startX] - startPtr[(y+1)*startStride]) ;

                for (x = startX; x < endX; x++)
                {
                    edgeType       = signDown1 + tmpUpBuff1[x+1] + 2;
                    pRec[x]        = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                    tmpUpBuff1[x] = -signDown1;
                    signDown1      = getSign(pRec[x+1] - pRec[x+stride]) ;
                }

                tmpUpBuff1[endX] = -signDown1;

                pRec  += stride;
            }

            //last line
            if (pbBorderAvail[SGU_BL])
            {
                edgeType = getSign(pRec[0] - pRec[stride-1]) + tmpUpBuff1[1] + 2;
                pRec[0] = pClipTbl[pRec[0] + m_OffsetEo[edgeType]];

            }

            if (pbBorderAvail[SGU_B])
            {
                for (x = 1; x < endX; x++)
                {
                    edgeType = getSign(pRec[x] - pRec[x+stride-1]) + tmpUpBuff1[x+1] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    pRec[x] = pOffsetBo[pRec[x]];
                }

                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

void H265SampleAdaptiveOffset::processSaoCuOrgChroma(Ipp32s addr, Ipp32s saoType, Ipp32s YCbCr, H265PlanePtrUVCommon tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    H265PlanePtrUVCommon pRec;
    Ipp32s tmpUpBuff1[65];
    Ipp32s tmpUpBuff2[65];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeft;
    Ipp32s signRight;
    Ipp32s signDown;
    Ipp32s signDown1;
    Ipp32u edgeType;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    H265PlanePtrUVCommon tmpU;

    picWidthTmp  = m_PicWidth  >> 1;
    picHeightTmp = m_PicHeight >> 1;
    LCUWidth     = LCUWidth    >> 1;
    LCUHeight    = LCUHeight   >> 1;
    LPelX        = LPelX       >> 1;
    TPelY        = TPelY       >> 1;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    if (YCbCr == 1)
    {
        pRec      = m_Frame->GetCbCrAddr(addr);
    }
    else
    {
        pRec      = m_Frame->GetCbCrAddr(addr) + 1;
    }
    stride    = m_Frame->pitch_chroma();

    tmpU = &(m_TmpU1[LPelX]);

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            startX = (LPelX == 0) ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth-1 : LCUWidth;

            for (y = 0; y < LCUHeight; y++)
            {
                signLeft = getSign(pRec[startX * 2] - tmpL[y]);

                for (x = startX; x < endX; x++)
                {
                    signRight =  getSign(pRec[x * 2] - pRec[x * 2 + 2]);
                    edgeType  =  signRight + signLeft + 2;
                    signLeft  = -signRight;

                    pRec[x * 2] = m_ClipTable[pRec[x * 2] + m_OffsetEo[edgeType]];
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
                tmpUpBuff1[x] = getSign(pRec[x * 2] - tmpU[x]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    signDown    = getSign(pRec[x * 2] - pRec[x * 2 + stride]);
                    edgeType    = signDown + tmpUpBuff1[x] + 2;
                    tmpUpBuff1[x]= -signDown;

                    pRec[x * 2] = m_ClipTable[pRec[x * 2] + m_OffsetEo[edgeType]];
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

            startX = (LPelX == 0)           ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 1 : LCUWidth;

            startY = (TPelY == 0) ?            1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0)
            {
                pRec += stride;
            }

            for (x = startX; x < endX; x++)
            {
                pUpBuff[x] = getSign(pRec[x * 2] - tmpU[x-1]);
            }

            for (y = startY; y < endY; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride + startX * 2] - tmpL[y]);

                for (x = startX; x < endX; x++)
                {
                    signDown1      =  getSign(pRec[x * 2] - pRec[x * 2 + stride + 2]) ;
                    edgeType       =  signDown1 + pUpBuff[x] + 2;
                    pUpBufft[x+1]  = -signDown1;
                    pRec[x * 2] = m_ClipTable[pRec[x * 2] + m_OffsetEo[edgeType]];
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
            startX = (LPelX == 0) ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 1 : LCUWidth;

            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (startY == 1)
            {
                pRec += stride;
            }

            for (x = startX - 1; x < endX; x++)
            {
                tmpUpBuff1[x+1] = getSign(pRec[x * 2] - tmpU[x+1]);
            }

            for (y = startY; y < endY; y++)
            {
                x = startX;
                signDown1      =  getSign(pRec[x * 2] - tmpL[y+1]) ;
                edgeType       =  signDown1 + tmpUpBuff1[x+1] + 2;
                tmpUpBuff1[x] = -signDown1;
                pRec[x * 2] = m_ClipTable[pRec[x * 2] + m_OffsetEo[edgeType]];

                for (x = startX + 1; x < endX; x++)
                {
                    signDown1      =  getSign(pRec[x * 2] - pRec[x * 2 + stride - 2]) ;
                    edgeType       =  signDown1 + tmpUpBuff1[x+1] + 2;
                    tmpUpBuff1[x]  = -signDown1;
                    pRec[x * 2] = m_ClipTable[pRec[x * 2] + m_OffsetEo[edgeType]];
                }

                tmpUpBuff1[endX] = getSign(pRec[endX * 2 - 2 + stride] - pRec[endX * 2]);

                pRec += stride;
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    pRec[x * 2] = m_OffsetBo[pRec[x * 2]];
                }
                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

void H265SampleAdaptiveOffset::processSaoCuChroma(Ipp32s addr, Ipp32s saoType, Ipp32s YCbCr, H265PlanePtrUVCommon tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    bool* pbBorderAvail = pTmpCu->m_AvailBorder;
    H265PlanePtrUVCommon pRec;
    Ipp32s tmpUpBuff1[65];
    Ipp32s tmpUpBuff2[65];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeft;
    Ipp32s signRight;
    Ipp32s signDown;
    Ipp32s signDown1;
    Ipp32u edgeType;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    H265PlanePtrUVCommon startPtr;
    H265PlanePtrUVCommon tmpU;
    H265PlanePtrUVCommon pClipTbl = m_ClipTable;
    H265PlanePtrUVCommon pOffsetBo = m_OffsetBo;
    Ipp32s startStride;

    picWidthTmp  = m_PicWidth  >> 1;
    picHeightTmp = m_PicHeight >> 1;
    LCUWidth     = LCUWidth    >> 1;
    LCUHeight    = LCUHeight   >> 1;
    LPelX        = LPelX       >> 1;
    TPelY        = TPelY       >> 1;
    RPelX        = LPelX + LCUWidth  ;
    BPelY        = TPelY + LCUHeight ;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    if (YCbCr == 1)
    {
        pRec      = m_Frame->GetCbCrAddr(addr);
    }
    else
    {
        pRec      = m_Frame->GetCbCrAddr(addr) + 1;
    }
    stride    = m_Frame->pitch_chroma();

    tmpU = &(m_TmpU1[LPelX]);

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX   = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth - 1);

            for (y = 0; y < LCUHeight; y++)
            {
                signLeft = getSign(pRec[startX * 2] - startPtr[y * startStride]);

                for (x = startX; x < endX; x++)
                {
                    signRight =  getSign(pRec[x * 2] - pRec[x * 2 + 2]);
                    edgeType  =  signRight + signLeft + 2;
                    signLeft  = -signRight;

                    pRec[x * 2]   = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
                }
                pRec  += stride;
            }
            break;
        }
    case SAO_EO_1: // dir: |
        {
            if (pbBorderAvail[SGU_T])
            {
                startY = 0;
                startPtr = tmpU;
                startStride = 1;
            }
            else
            {
                startY = 1;
                startPtr = pRec;
                startStride = 2;
                pRec += stride;
            }

            endY = (pbBorderAvail[SGU_B]) ? LCUHeight : LCUHeight - 1;

            for (x = 0; x < LCUWidth; x++)
            {
                tmpUpBuff1[x] = getSign(pRec[x * 2] - startPtr[x * startStride]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    signDown      = getSign(pRec[x * 2] - pRec[x * 2 + stride]);
                    edgeType      = signDown + tmpUpBuff1[x] + 2;
                    tmpUpBuff1[x] = -signDown;

                    pRec[x * 2]   = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
                }
                pRec  += stride;
            }
            break;
        }
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth-1);

            //prepare 2nd line upper sign
            pUpBuff[startX] = getSign(pRec[startX * 2 + stride] - startPtr[0]);
            for (x = startX; x < endX; x++)
            {
                pUpBuff[x+1] = getSign(pRec[x * 2 + stride + 2] - pRec[x * 2]);
            }

            //1st line
            if (pbBorderAvail[SGU_TL])
            {
                edgeType = getSign(pRec[0] - tmpU[-1]) - pUpBuff[1] + 2;
                pRec[0]  = pClipTbl[pRec[0] + m_OffsetEo[edgeType]];
            }

            if (pbBorderAvail[SGU_T])
            {
                for (x = 1; x < endX; x++)
                {
                    edgeType = getSign(pRec[x * 2] - tmpU[x-1]) - pUpBuff[x+1] + 2;
                    pRec[x * 2]  = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
                }
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride + startX * 2] - startPtr[y * startStride]);

                for (x = startX; x < endX; x++)
                {
                    signDown1 = getSign(pRec[x * 2] - pRec[x * 2 + stride + 2]);
                    edgeType  =  signDown1 + pUpBuff[x] + 2;
                    pRec[x * 2]   = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];

                    pUpBufft[x+1] = -signDown1;
                }

                swapPtr  = pUpBuff;
                pUpBuff  = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }

            //last line
            if (pbBorderAvail[SGU_B])
            {
                for (x = startX; x < LCUWidth - 1; x++)
                {
                    edgeType = getSign(pRec[x * 2] - pRec[x * 2 + stride + 2]) + pUpBuff[x] + 2;
                    pRec[x * 2]  = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
                }
            }

            if (pbBorderAvail[SGU_BR])
            {
                x = LCUWidth - 1;
                edgeType = getSign(pRec[x * 2] - pRec[x * 2 + stride + 2]) + pUpBuff[x] + 2;
                pRec[x * 2]  = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
            }
            break;
        }
    case SAO_EO_3: // dir: 45
        {
            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX   = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth -1);

            //prepare 2nd line upper sign
            tmpUpBuff1[startX] = getSign(startPtr[startStride] - pRec[startX * 2]);
            for (x = startX; x < endX; x++)
            {
                tmpUpBuff1[x+1] = getSign(pRec[x * 2 + stride] - pRec[x * 2 + 2]);
            }

            //first line
            if (pbBorderAvail[SGU_T])
            {
                for (x = startX; x < LCUWidth - 1; x++)
                {
                    edgeType = getSign(pRec[x * 2] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                    pRec[x * 2] = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
                }
            }

            if (pbBorderAvail[SGU_TR])
            {
                x= LCUWidth - 1;
                edgeType = getSign(pRec[x * 2] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                pRec[x * 2] = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                signDown1 = getSign(pRec[startX * 2] - startPtr[(y + 1) * startStride]);

                for (x = startX; x < endX; x++)
                {
                    edgeType       = signDown1 + tmpUpBuff1[x+1] + 2;
                    pRec[x * 2]    = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
                    tmpUpBuff1[x]  = -signDown1;
                    signDown1      = getSign(pRec[x * 2 + 2] - pRec[x * 2 + stride]) ;
                }

                tmpUpBuff1[endX] = -signDown1;

                pRec  += stride;
            }

            //last line
            if (pbBorderAvail[SGU_BL])
            {
                edgeType = getSign(pRec[0] - pRec[stride - 2]) + tmpUpBuff1[1] + 2;
                pRec[0] = pClipTbl[pRec[0] + m_OffsetEo[edgeType]];
            }

            if (pbBorderAvail[SGU_B])
            {
                for (x = 1; x < endX; x++)
                {
                    edgeType = getSign(pRec[x * 2] - pRec[x * 2 + stride - 2]) + tmpUpBuff1[x+1] + 2;
                    pRec[x * 2]  = pClipTbl[pRec[x * 2] + m_OffsetEo[edgeType]];
                }
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    pRec[x * 2] = pOffsetBo[pRec[x * 2]];
                }

                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

void H265SampleAdaptiveOffset::SAOProcess(H265DecoderFrame* pFrame, SAOParams* pSAOParam)
{
    if (pSAOParam->m_bSaoFlag[0] || pSAOParam->m_bSaoFlag[1])
    {
        m_SaoBitIncreaseY = IPP_MAX(g_bitDepthY - 10, 0);
        m_SaoBitIncreaseC = IPP_MAX(g_bitDepthC - 10, 0);

        if (m_saoLcuBasedOptimization)
        {
            pSAOParam->m_oneUnitFlag[0] = 0;
            pSAOParam->m_oneUnitFlag[1] = 0;
            pSAOParam->m_oneUnitFlag[2] = 0;
        }

        if (pSAOParam->m_bSaoFlag[0])
        {
            processSaoUnitAllLuma(pFrame->m_saoLcuParam[0], pSAOParam->m_oneUnitFlag[0]);
        }

        if (pSAOParam->m_bSaoFlag[1])
        {
            VM_ASSERT(pSAOParam->m_oneUnitFlag[1] == pSAOParam->m_oneUnitFlag[2]);
            processSaoUnitAllChroma(pFrame->m_saoLcuParam[1], pSAOParam->m_oneUnitFlag[1], 1);//Cb
            processSaoUnitAllChroma(pFrame->m_saoLcuParam[2], pSAOParam->m_oneUnitFlag[2], 2);//Cr
        }
    }
}

void H265SampleAdaptiveOffset::processSaoUnitAllLuma(SAOLCUParam* saoLCUParam, bool oneUnitFlag)
{
    H265PlaneYCommon tmpLeftBuff1[65];
    H265PlaneYCommon tmpLeftBuff2[65];
    H265PlaneYCommon *tmpL1;
    H265PlaneYCommon *tmpL2;
    H265PlaneYCommon *pRec;
    H265PlaneYCommon *ppLumaTable = NULL;
    H265PlaneYCommon *tmpUSwap;
    static Ipp32s offset[LUMA_GROUP_NUM + 1];
    Ipp32s frameWidthInCU = m_Frame->getFrameWidthInCU();
    Ipp32s frameHeightInCU = m_Frame->getFrameHeightInCU();
    Ipp32u edgeType;
    Ipp32s m_typeIdx;
    Ipp32s idxX;
    Ipp32s idxY;
    Ipp32s addr;
    Ipp32s stride;
    Ipp32s LCUWidth = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s picWidthTmp = m_PicWidth;
    Ipp32s picHeightTmp = m_PicHeight;
    bool m_mergeLeftFlag;
    Ipp32u saoBitIncrease = m_SaoBitIncreaseY;

    tmpL1 = tmpLeftBuff1;
    tmpL2 = tmpLeftBuff2;

    pRec   = m_Frame->m_pYPlane;
    stride = m_Frame->pitch_luma();

    memcpy(m_TmpU1, pRec, sizeof(H265PlaneYCommon) * picWidthTmp);

    offset[0] = 0;
    for (idxY = 0; idxY < frameHeightInCU; idxY++)
    {
        Ipp32s CUHeightTmp = LCUHeight + 1;
        Ipp32s i;

        if ((idxY * LCUHeight + CUHeightTmp) > picHeightTmp)
        {
            CUHeightTmp = picHeightTmp - idxY * LCUHeight;
        }

        addr = idxY * frameWidthInCU;
        pRec  = m_Frame->GetLumaAddr(addr);

        for (i = 0; i < CUHeightTmp; i++)
        {
            tmpL1[i] = pRec[0];
            pRec += stride;
        }

        if (idxY != (frameHeightInCU - 1))
        {
            pRec -= 2*stride;
            memcpy(m_TmpU2, pRec, sizeof(H265PlaneYCommon) * picWidthTmp);
        }

        for (idxX = 0; idxX < frameWidthInCU; idxX++)
        {
            addr = idxY * frameWidthInCU + idxX;

            if (idxX != (frameWidthInCU - 1))
            {
                pRec  = m_Frame->GetLumaAddr(addr);

                for (i = 0; i < CUHeightTmp; i++)
                {
                    tmpL2[i] = pRec[i*stride+LCUWidth-1];
                }
            }

            if (oneUnitFlag)
            {
                m_typeIdx = saoLCUParam[0].m_typeIdx;
                m_mergeLeftFlag = (addr == 0) ? 0 : 1;
            }
            else
            {
                m_typeIdx = saoLCUParam[addr].m_typeIdx;
                m_mergeLeftFlag = saoLCUParam[addr].m_mergeLeftFlag;
            }
            if (m_typeIdx >= 0)
            {
                if (!m_mergeLeftFlag)
                {
                    if (m_typeIdx == SAO_BO)
                    {
                        for (i = 0; i < SAO_MAX_BO_CLASSES + 1; i++)
                        {
                            offset[i] = 0;
                        }
                        for (i = 0; i < saoLCUParam[addr].m_length; i++)
                        {
                            offset[(saoLCUParam[addr].m_subTypeIdx +i) % SAO_MAX_BO_CLASSES + 1] = saoLCUParam[addr].m_offset[i] << saoBitIncrease;
                        }
                        ppLumaTable = m_lumaTableBo;
                        for (i = 0; i < (1 << g_bitDepthY); i++)
                        {
                            m_OffsetBo[i] = m_ClipTable[i + offset[ppLumaTable[i]]];
                        }
                    }

                    if (m_typeIdx == SAO_EO_0 || m_typeIdx == SAO_EO_1 || m_typeIdx == SAO_EO_2 || m_typeIdx == SAO_EO_3)
                    {
                        for (i = 0; i < saoLCUParam[addr].m_length; i++)
                        {
                            offset[i + 1] = saoLCUParam[addr].m_offset[i] << saoBitIncrease;
                        }
                        for (edgeType = 0; edgeType < 6; edgeType++)
                        {
                            m_OffsetEo[edgeType] = offset[EoTable[edgeType]];
                        }
                    }
                }

                if (!m_UseNIF)
                {
                    processSaoCuOrgLuma(addr, m_typeIdx, tmpL1);
                }
                else
                {
                    processSaoCuLuma(addr, m_typeIdx, tmpL1);
                }
            }

            tmpUSwap = tmpL1;
            tmpL1  = tmpL2;
            tmpL2  = tmpUSwap;
        }
        tmpUSwap = m_TmpU1;
        m_TmpU1 = m_TmpU2;
        m_TmpU2 = tmpUSwap;
    }
}

void H265SampleAdaptiveOffset::processSaoUnitAllChroma(SAOLCUParam* saoLCUParam, bool oneUnitFlag, Ipp32s yCbCr)
{
    H265PlaneUVCommon tmpLeftBuff1[65];
    H265PlaneUVCommon tmpLeftBuff2[65];
    H265PlanePtrUVCommon tmpL1;
    H265PlanePtrUVCommon tmpL2;
    H265PlanePtrUVCommon pRec;
    H265PlanePtrUVCommon ppLumaTable = NULL;
    H265PlanePtrUVCommon tmpUSwap;
    static Ipp32s offset[LUMA_GROUP_NUM + 1];
    Ipp32s frameWidthInCU = m_Frame->getFrameWidthInCU();
    Ipp32s frameHeightInCU = m_Frame->getFrameHeightInCU();
    Ipp32u edgeType;
    Ipp32s m_typeIdx;
    Ipp32s idxX;
    Ipp32s idxY;
    Ipp32s addr;
    Ipp32s stride;
    Ipp32s LCUWidth = (m_MaxCUWidth >> 1);
    Ipp32s LCUHeight = (m_MaxCUHeight >> 1);
    Ipp32s picWidthTmp = (m_PicWidth >> 1);
    Ipp32s picHeightTmp = (m_PicHeight >> 1);
    bool m_mergeLeftFlag;
    Ipp32u saoBitIncrease = (yCbCr == 0) ? m_SaoBitIncreaseY : m_SaoBitIncreaseC;
    Ipp32s i;

    tmpL1 = tmpLeftBuff1;
    tmpL2 = tmpLeftBuff2;

    if (yCbCr == 1)
    {
        pRec   = m_Frame->m_pUVPlane;
    }
    else
    {
        pRec   = m_Frame->m_pUVPlane + 1;
    }
    stride = m_Frame->pitch_chroma();

    for (i = 0; i < picWidthTmp; i++)
        m_TmpU1[i] = pRec[i * 2];

    offset[0] = 0;
    for (idxY = 0; idxY < frameHeightInCU; idxY++)
    {
        Ipp32s CUHeightTmp = LCUHeight + 1;

        if ((idxY * LCUHeight + CUHeightTmp) > picHeightTmp)
        {
            CUHeightTmp = picHeightTmp - idxY * LCUHeight;
        }

        addr = idxY * frameWidthInCU;
        if (yCbCr == 1)
        {
            pRec  = m_Frame->GetCbCrAddr(addr);
        }
        else
        {
            pRec  = m_Frame->GetCbCrAddr(addr) + 1;
        }

        for (i = 0; i < CUHeightTmp; i++)
        {
            tmpL1[i] = pRec[0];
            pRec += stride;
        }

        if (idxY != (frameHeightInCU - 1))
        {
            pRec -= 2 * stride;

            for (i = 0; i < picWidthTmp; i++)
                m_TmpU2[i] = pRec[i * 2];
        }

        for (idxX = 0; idxX < frameWidthInCU; idxX++)
        {
            addr = idxY * frameWidthInCU + idxX;

            if (idxX != (frameWidthInCU - 1))
            {
                if (yCbCr == 1)
                {
                    pRec  = m_Frame->GetCbCrAddr(addr);
                }
                else
                {
                    pRec  = m_Frame->GetCbCrAddr(addr) + 1;
                }

                for (i = 0; i < CUHeightTmp; i++)
                {
                    tmpL2[i] = pRec[i * stride + (LCUWidth - 1) * 2];
                }
            }

            if (oneUnitFlag)
            {
                m_typeIdx = saoLCUParam[0].m_typeIdx;
                m_mergeLeftFlag = (addr == 0) ? 0 : 1;
            }
            else
            {
                m_typeIdx = saoLCUParam[addr].m_typeIdx;
                m_mergeLeftFlag = saoLCUParam[addr].m_mergeLeftFlag;
            }
            if (m_typeIdx >= 0)
            {
                if (!m_mergeLeftFlag)
                {
                    if (m_typeIdx == SAO_BO)
                    {
                        for (i = 0; i < SAO_MAX_BO_CLASSES + 1; i++)
                        {
                            offset[i] = 0;
                        }
                        for (i = 0; i < saoLCUParam[addr].m_length; i++)
                        {
                            offset[(saoLCUParam[addr].m_subTypeIdx +i) % SAO_MAX_BO_CLASSES + 1] = saoLCUParam[addr].m_offset[i] << saoBitIncrease;
                        }
                        ppLumaTable = m_lumaTableBo;
                        for (i = 0; i < (1 << g_bitDepthY); i++)
                        {
                            m_OffsetBo[i] = m_ClipTable[i + offset[ppLumaTable[i]]];
                        }
                    }

                    if (m_typeIdx == SAO_EO_0 || m_typeIdx == SAO_EO_1 || m_typeIdx == SAO_EO_2 || m_typeIdx == SAO_EO_3)
                    {
                        for (i = 0; i < saoLCUParam[addr].m_length; i++)
                        {
                            offset[i + 1] = saoLCUParam[addr].m_offset[i] << saoBitIncrease;
                        }
                        for (edgeType = 0; edgeType < 6; edgeType++)
                        {
                            m_OffsetEo[edgeType] = offset[EoTable[edgeType]];
                        }
                    }
                }

                if (!m_UseNIF)
                {
                    processSaoCuOrgChroma(addr, m_typeIdx, yCbCr, tmpL1);
                }
                else
                {
                    processSaoCuChroma(addr, m_typeIdx, yCbCr, tmpL1);
                }
            }

            tmpUSwap = tmpL1;
            tmpL1  = tmpL2;
            tmpL2  = tmpUSwap;
        }
        tmpUSwap = m_TmpU1;
        m_TmpU1 = m_TmpU2;
        m_TmpU2 = tmpUSwap;
    }
}

void H265SampleAdaptiveOffset::createNonDBFilterInfo()
{
    H265Slice* pSlice;
    bool independentSliceBoundaryForNDBFilter = false;
    bool independentTileBoundaryForNDBFilter = false;

    pSlice = m_Frame->GetAU()->GetSlice(0);

    if ((pSlice->getPPS()->getNumTiles() > 1) &&
        (!pSlice->getPPS()->getLoopFilterAcrossTilesEnabledFlag()))
    {
        independentTileBoundaryForNDBFilter = true;
    }

    if (m_Frame->m_iNumberOfSlices > 1)
    {
        for (Ipp32s i = 0; i < m_Frame->m_iNumberOfSlices; i++)
        {
            pSlice = m_Frame->GetAU()->GetSlice(i);

            if (pSlice->getLFCrossSliceBoundaryFlag() == false)
            {
                independentSliceBoundaryForNDBFilter = true;
            }

            if ((pSlice->getPPS()->getNumTiles() > 1) &&
                (!pSlice->getPPS()->getLoopFilterAcrossTilesEnabledFlag()))
            {
                independentTileBoundaryForNDBFilter = true;
            }
        }
    }

    m_UseNIF = false;

    if (independentSliceBoundaryForNDBFilter || independentTileBoundaryForNDBFilter)
    {
        m_UseNIF = true;
    }

    if (m_UseNIF)
    {
        for (Ipp32s i = 0; i < m_Frame->m_iNumberOfSlices; i++)
        {
            pSlice = m_Frame->GetAU()->GetSlice(i);
            independentTileBoundaryForNDBFilter = false;

            if ((pSlice->getPPS()->getNumTiles() > 1) &&
                (!pSlice->getPPS()->getLoopFilterAcrossTilesEnabledFlag()))
            {
                independentTileBoundaryForNDBFilter = true;
            }

            Ipp32u firstCU = pSlice->GetCurrentFrame()->m_CodingData->GetInverseCUOrderMap(pSlice->m_iFirstMB);
            Ipp32u lastCU = pSlice->GetCurrentFrame()->m_CodingData->GetInverseCUOrderMap(pSlice->m_iMaxMB);
            for (Ipp32u j = firstCU; j < lastCU; j++)
            {
                m_Frame->getCU(pSlice->GetCurrentFrame()->m_CodingData->getCUOrderMap(j))->setNDBFilterBlockBorderAvailability(independentTileBoundaryForNDBFilter);
            }
        }
    }
}

void H265SampleAdaptiveOffset::PCMRestoration()
{
    H265Slice *pSlice = m_Frame->GetAU()->GetSlice(0);
    bool PCMFilter = (pSlice->GetSliceHeader()->m_SeqParamSet->getUsePCM() && pSlice->GetSliceHeader()->m_SeqParamSet->getPCMFilterDisableFlag()) ? true : false;

    if(PCMFilter || pSlice->GetSliceHeader()->m_PicParamSet->getTransquantBypassEnableFlag())
    {
        for(Ipp32u CUAddr = 0; CUAddr < m_Frame->getNumCUsInFrame(); CUAddr++)
        {
            H265CodingUnit* pcCU = m_Frame->getCU(CUAddr);

            PCMCURestoration(pcCU, 0, 0);
        }
    }
}

/** PCM CU restoration.
 * \param pcCU pointer to current CU
 * \param uiAbsPartIdx part index
 * \param uiDepth CU depth
 * \returns Void
 */
void H265SampleAdaptiveOffset::PCMCURestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth)
{
    Ipp32u CurNumParts = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u QNumParts   = CurNumParts >> 2;

    // go to sub-CU
    if(pcCU->GetDepth(AbsZorderIdx) > Depth)
    {
        for (Ipp32u PartIdx = 0; PartIdx < 4; PartIdx++, AbsZorderIdx += QNumParts)
        {
            Ipp32u LPelX = pcCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[AbsZorderIdx]];
            Ipp32u TPelY = pcCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[AbsZorderIdx]];
            if((LPelX < pcCU->m_SliceHeader->m_SeqParamSet->getPicWidthInLumaSamples()) && (TPelY < pcCU->m_SliceHeader->m_SeqParamSet->getPicHeightInLumaSamples()))
                PCMCURestoration(pcCU, AbsZorderIdx, Depth + 1);
        }
        return;
    }

    // restore PCM samples
    if ((pcCU->GetIPCMFlag(AbsZorderIdx) && pcCU->m_SliceHeader->m_SeqParamSet->getPCMFilterDisableFlag()) || pcCU->isLosslessCoded(AbsZorderIdx))
    {
        PCMSampleRestoration(pcCU, AbsZorderIdx, Depth, TEXT_LUMA);
        PCMSampleRestoration(pcCU, AbsZorderIdx, Depth, TEXT_CHROMA);
    }
}

/** PCM sample restoration.
* \param pcCU pointer to current CU
* \param uiAbsPartIdx part index
* \param uiDepth CU depth
* \param ttText texture component type
* \returns Void
*/
void H265SampleAdaptiveOffset::PCMSampleRestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth, EnumTextType Text)
{
    H265PlaneYCommon* Src;
    Ipp32u Stride;
    Ipp32u Width;
    Ipp32u Height;
    Ipp32u PcmLeftShiftBit;
    Ipp32u X, Y;
    Ipp32u MinCoeffSize = m_Frame->getMinCUWidth() * m_Frame->getMinCUWidth();
    Ipp32u LumaOffset   = MinCoeffSize * AbsZorderIdx;
    Ipp32u ChromaOffset = LumaOffset >> 2;

    H265FrameCodingData * cd = pcCU->m_Frame->getCD();

    if(Text == TEXT_LUMA)
    {
        H265PlaneYCommon* Pcm;

        Src = m_Frame->GetLumaAddr(pcCU->CUAddr, AbsZorderIdx);
        Pcm = pcCU->m_IPCMSampleY + LumaOffset;
        Stride  = m_Frame->pitch_luma();
        Width  = (cd->m_MaxCUWidth >> Depth);
        Height = (cd->m_MaxCUWidth >> Depth);

        if (pcCU->isLosslessCoded(AbsZorderIdx) && !pcCU->GetIPCMFlag(AbsZorderIdx))
        {
            PcmLeftShiftBit = 0;
        }
        else
        {
            PcmLeftShiftBit = g_bitDepthY - pcCU->m_SliceHeader->m_SeqParamSet->getPCMBitDepthLuma();
        }

        for(Y = 0; Y < Height; Y++)
        {
            for(X = 0; X < Width; X++)
            {
                Src[X] = (Pcm[X] << PcmLeftShiftBit);
            }
            Pcm += Width;
            Src += Stride;
        }
    }
    else
    {
        H265PlaneUVCommon *PcmCb, *PcmCr;

        Src = m_Frame->GetCbCrAddr(pcCU->CUAddr, AbsZorderIdx);
        PcmCb = pcCU->m_IPCMSampleCb + ChromaOffset;
        PcmCr = pcCU->m_IPCMSampleCr + ChromaOffset;

        Stride = m_Frame->pitch_chroma();
        Width  = ((cd->m_MaxCUWidth >> Depth) / 2);
        Height = ((cd->m_MaxCUWidth >> Depth) / 2);
        if (pcCU->isLosslessCoded(AbsZorderIdx) && !pcCU->GetIPCMFlag(AbsZorderIdx))
        {
            PcmLeftShiftBit = 0;
        }
        else
        {
            PcmLeftShiftBit = g_bitDepthC - pcCU->m_SliceHeader->m_SeqParamSet->getPCMBitDepthChroma();
        }

        for(Y = 0; Y < Height; Y++)
        {
            for(X = 0; X < Width; X++)
            {
                Src[X * 2] = (PcmCb[X] << PcmLeftShiftBit);
                Src[X * 2 + 1] = (PcmCr[X] << PcmLeftShiftBit);
            }
            PcmCb += Width;
            PcmCr += Width;
            Src += Stride;
        }
    }
}

void H265ScalingList::init()
{
    // TODO something w/ this.........
    //m_scalingListCoef[SCALING_LIST_32x32][3] = &m_scalingListCoef[SCALING_LIST_32x32][1][0]; // copy address for 32x32
}

void H265ScalingList::destroy()
{
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
