//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

//#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 
#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_TARGET_OPTIMIZATION_PX) || \
    defined(MFX_TARGET_OPTIMIZATION_ATOM) || \
    defined(MFX_TARGET_OPTIMIZATION_SSSE3)

namespace MFX_HEVC_PP
{
    enum SAOType
    {
        SAO_EO_0 = 0,
        SAO_EO_1,
        SAO_EO_2,
        SAO_EO_3,
        SAO_BO,
        MAX_NUM_SAO_TYPE
    };

    /** get the sign of input variable
    * \param   x
    */
    inline Ipp32s getSign(Ipp32s x)
    {
        return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
    }

    typedef Ipp16u PixType;

#undef MAKE_NAME

#if defined(MFX_TARGET_OPTIMIZATION_AUTO)
    #define MAKE_NAME( func ) func ## _px
#else
    #define MAKE_NAME( func ) func ## _px
#endif

    void MAKE_NAME(h265_ProcessSaoCuOrg_Luma_16u)(SAOCU_ORG_PARAMETERS_LIST_U16)
    {
        Ipp32s tmpUpBuff1[65];
        Ipp32s tmpUpBuff2[65];
        Ipp32s LCUWidth  = maxCUWidth;
        Ipp32s LCUHeight = maxCUHeight;
        Ipp32s LPelX     = CUPelX;
        Ipp32s TPelY     = CUPelY;
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

        picWidthTmp  = picWidth;
        picHeightTmp = picHeight;
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

                        pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
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

                        pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
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
                        pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
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
                    pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];

                    for (x = startX + 1; x < endX; x++)
                    {
                        signDown1      =  getSign(pRec[x] - pRec[x+stride-1]) ;
                        edgeType       =  signDown1 + tmpUpBuff1[x+1] + 2;
                        tmpUpBuff1[x]  = -signDown1;
                        pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
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
                        pRec[x] = pOffsetBo[pRec[x]];
                    }
                    pRec += stride;
                }
                break;
            }
        default: break;
        }

    }

#if defined MFX_TARGET_OPTIMIZATION_ATOM || defined MFX_TARGET_OPTIMIZATION_SSSE3
    void h265_ProcessSaoCu_Luma_16u(SAOCU_PARAMETERS_LIST_U16)
#else
    void MAKE_NAME(h265_ProcessSaoCu_Luma_16u)(SAOCU_PARAMETERS_LIST_U16)
#endif
    {
        Ipp32s tmpUpBuff1[65];
        Ipp32s tmpUpBuff2[65];
        Ipp32s LCUWidth  = maxCUWidth;
        Ipp32s LCUHeight = maxCUHeight;
        Ipp32s LPelX     = CUPelX;
        Ipp32s TPelY     = CUPelY;
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
        PixType*  startPtr;
        Ipp32s startStride;

        picWidthTmp  = picWidth;
        picHeightTmp = picHeight;
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

        switch (saoType)
        {
        case SAO_EO_0: // dir: -
            {
                if (pbBorderAvail.m_left)
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

                endX   = (pbBorderAvail.m_right) ? LCUWidth : (LCUWidth - 1);

                for (y = 0; y < LCUHeight; y++)
                {
                    signLeft = getSign(pRec[startX] - startPtr[y*startStride]);

                    for (x = startX; x < endX; x++)
                    {
                        signRight =  getSign(pRec[x] - pRec[x+1]);
                        edgeType  =  signRight + signLeft + 2;
                        signLeft  = -signRight;

                        pRec[x]   = pClipTable[pRec[x] + pOffsetEo[edgeType]];
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

                endY = (pbBorderAvail.m_bottom) ? LCUHeight : LCUHeight - 1;

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

                        pRec[x]      = pClipTable[pRec[x] + pOffsetEo[edgeType]];
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

                if (pbBorderAvail.m_left)
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

                endX = (pbBorderAvail.m_right) ? LCUWidth : (LCUWidth-1);

                //prepare 2nd line upper sign
                pUpBuff[startX] = getSign(pRec[startX+stride] - startPtr[0]);
                for (x = startX; x < endX; x++)
                {
                    pUpBuff[x+1] = getSign(pRec[x+stride+1] - pRec[x]);
                }

                //1st line
                if (pbBorderAvail.m_top_left)
                {
                    edgeType = getSign(pRec[0] - tmpU[-1]) - pUpBuff[1] + 2;
                    pRec[0]  = pClipTable[pRec[0] + pOffsetEo[edgeType]];
                }

                if (pbBorderAvail.m_top)
                {
                    for (x = 1; x < endX; x++)
                    {
                        edgeType = getSign(pRec[x] - tmpU[x-1]) - pUpBuff[x+1] + 2;
                        pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
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
                        pRec[x]   = pClipTable[pRec[x] + pOffsetEo[edgeType]];

                        pUpBufft[x+1] = -signDown1;
                    }

                    swapPtr  = pUpBuff;
                    pUpBuff  = pUpBufft;
                    pUpBufft = swapPtr;

                    pRec += stride;
                }

                //last line
                if (pbBorderAvail.m_bottom)
                {
                    for (x = startX; x < LCUWidth - 1; x++)
                    {
                        edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                        pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                }

                if (pbBorderAvail.m_bottom_right)
                {
                    x = LCUWidth - 1;
                    edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                    pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                }
                break;
            }
        case SAO_EO_3: // dir: 45
            {
                if (pbBorderAvail.m_left)
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

                endX   = (pbBorderAvail.m_right) ? LCUWidth : (LCUWidth -1);

                //prepare 2nd line upper sign
                tmpUpBuff1[startX] = getSign(startPtr[startStride] - pRec[startX]);
                for (x = startX; x < endX; x++)
                {
                    tmpUpBuff1[x+1] = getSign(pRec[x+stride] - pRec[x+1]);
                }

                //first line
                if (pbBorderAvail.m_top)
                {
                    for (x = startX; x < LCUWidth - 1; x++)
                    {
                        edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                        pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                }

                if (pbBorderAvail.m_top_right)
                {
                    x= LCUWidth - 1;
                    edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                    pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                }

                pRec += stride;

                //middle lines
                for (y = 1; y < LCUHeight - 1; y++)
                {
                    signDown1 = getSign(pRec[startX] - startPtr[(y+1)*startStride]) ;

                    for (x = startX; x < endX; x++)
                    {
                        edgeType       = signDown1 + tmpUpBuff1[x+1] + 2;
                        pRec[x]        = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                        tmpUpBuff1[x] = -signDown1;
                        signDown1      = getSign(pRec[x+1] - pRec[x+stride]) ;
                    }

                    tmpUpBuff1[endX] = -signDown1;

                    pRec  += stride;
                }

                //last line
                if (pbBorderAvail.m_bottom_left)
                {
                    edgeType = getSign(pRec[0] - pRec[stride-1]) + tmpUpBuff1[1] + 2;
                    pRec[0] = pClipTable[pRec[0] + pOffsetEo[edgeType]];

                }

                if (pbBorderAvail.m_bottom)
                {
                    for (x = 1; x < endX; x++)
                    {
                        edgeType = getSign(pRec[x] - pRec[x+stride-1]) + tmpUpBuff1[x+1] + 2;
                        pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
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

    const int   g_skipLinesR[3] = {1, 1, 1};//YCbCr
    const int   g_skipLinesB[3] = {1, 1, 1};//YCbCr

    void MAKE_NAME(h265_GetCtuStatistics_16u)(SAOCU_ENCODE_PARAMETERS_LIST_16U)
        //(
        //int compIdx,
        //const PixType* recBlk,
        //int recStride,
        //const PixType* orgBlk,
        //int orgStride,

        //int width, // correct LCU size
        //int height,

        //int shift,// to switch btw NV12/YV12 Chroma

        //const MFX_HEVC_PP::CTBBorders& borders,
        //MFX_HEVC_PP::SaoCtuStatistics* statsDataTypes)
    {
        Ipp16s signLineBuf1[64+1];
        Ipp16s signLineBuf2[64+1];

        int x, y, startX, startY, endX, endY;
        int firstLineStartX, firstLineEndX;
        int edgeType;
        Ipp16s signLeft, signRight, signDown;
        Ipp64s *diff, *count;

        //const int compIdx = SAO_Y;
        int skipLinesR = g_skipLinesR[compIdx];
        int skipLinesB = g_skipLinesB[compIdx];

        const PixType *recLine, *orgLine;
        const PixType* recLineAbove;
        const PixType* recLineBelow;


        for(int typeIdx=0; typeIdx< numSaoModes; typeIdx++)
        {
            SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
            statsData.Reset();

            recLine = recBlk;
            orgLine = orgBlk;
            diff    = statsData.diff;
            count   = statsData.count;
            switch(typeIdx)
            {
            case SAO_EO_0://SAO_TYPE_EO_0:
                {
                    diff +=2;
                    count+=2;
                    endY   = height - skipLinesB;

                    startX = borders.m_left ? 0 : 1;
                    endX   = width - skipLinesR;

                    for (y=0; y<endY; y++)
                    {
                        signLeft = getSign(recLine[startX] - recLine[startX-1]);
                        for (x=startX; x<endX; x++)
                        {
                            signRight =  getSign(recLine[x] - recLine[x+1]);
                            edgeType  =  signRight + signLeft;
                            signLeft  = -signRight;

                            diff [edgeType] += (orgLine[x << shift] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;

            case SAO_EO_1: //SAO_TYPE_EO_90:
                {
                    diff +=2;
                    count+=2;
                    endX   = width - skipLinesR;

                    startY = borders.m_top ? 0 : 1;
                    endY   = height - skipLinesB;
                    if ( 0 == borders.m_top )
                    {
                        recLine += recStride;
                        orgLine += orgStride;
                    }

                    recLineAbove = recLine - recStride;
                    Ipp16s *signUpLine = signLineBuf1;
                    for (x=0; x< endX; x++) 
                    {
                        signUpLine[x] = getSign(recLine[x] - recLineAbove[x]);
                    }

                    for (y=startY; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for (x=0; x<endX; x++)
                        {
                            signDown  = getSign(recLine[x] - recLineBelow[x]); 
                            edgeType  = signDown + signUpLine[x];
                            signUpLine[x]= -signDown;

                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            case SAO_EO_2: //SAO_TYPE_EO_135:
                {
                    diff +=2;
                    count+=2;
                    startX = borders.m_left ? 0 : 1 ;
                    endX   = width - skipLinesR;
                    endY   = height - skipLinesB;

                    //prepare 2nd line's upper sign
                    Ipp16s *signUpLine, *signDownLine, *signTmpLine;
                    signUpLine  = signLineBuf1;
                    signDownLine= signLineBuf2;
                    recLineBelow = recLine + recStride;
                    for (x=startX; x<endX+1; x++)
                    {
                        signUpLine[x] = getSign(recLineBelow[x] - recLine[x-1]);
                    }

                    //1st line
                    recLineAbove = recLine - recStride;
                    firstLineStartX = borders.m_top_left ? 0 : 1;
                    firstLineEndX   = borders.m_top      ? endX : 1;

                    for(x=firstLineStartX; x<firstLineEndX; x++)
                    {
                        edgeType = getSign(recLine[x] - recLineAbove[x-1]) - signUpLine[x+1];
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine  += recStride;
                    orgLine  += orgStride;


                    //middle lines
                    for (y=1; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for (x=startX; x<endX; x++)
                        {
                            signDown = getSign(recLine[x] - recLineBelow[x+1]);
                            edgeType = signDown + signUpLine[x];
                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;

                            signDownLine[x+1] = -signDown; 
                        }
                        signDownLine[startX] = getSign(recLineBelow[startX] - recLine[startX-1]);

                        signTmpLine  = signUpLine;
                        signUpLine   = signDownLine;
                        signDownLine = signTmpLine;

                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            case SAO_EO_3: //SAO_TYPE_EO_45:
                {
                    diff +=2;
                    count+=2;
                    endY   = height - skipLinesB;

                    startX = borders.m_left ? 0 : 1;
                    endX   = width - skipLinesR;

                    //prepare 2nd line upper sign
                    recLineBelow = recLine + recStride;
                    Ipp16s *signUpLine = signLineBuf1+1;
                    for (x=startX-1; x<endX; x++)
                    {
                        signUpLine[x] = getSign(recLineBelow[x] - recLine[x+1]);
                    }

                    //first line
                    recLineAbove = recLine - recStride;

                    firstLineStartX = borders.m_top ? startX : endX;
                    firstLineEndX   = endX;

                    for(x=firstLineStartX; x<firstLineEndX; x++)
                    {
                        edgeType = getSign(recLine[x] - recLineAbove[x+1]) - signUpLine[x-1];
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }

                    recLine += recStride;
                    orgLine += orgStride;

                    //middle lines
                    for (y=1; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for(x=startX; x<endX; x++)
                        {
                            signDown = getSign(recLine[x] - recLineBelow[x-1]) ;
                            edgeType = signDown + signUpLine[x];

                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;

                            signUpLine[x-1] = -signDown; 
                        }
                        signUpLine[endX-1] = getSign(recLineBelow[endX-1] - recLine[endX]);
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;

            case SAO_BO: //SAO_TYPE_BO:
                {
                    endX = width- skipLinesR;
                    endY = height- skipLinesB;
                    const int shiftBits = 5;
                    for (y=0; y<endY; y++)
                    {
                        for (x=0; x<endX; x++)
                        {
                            int bandIdx= recLine[x] >> shiftBits; 
                            diff [bandIdx] += (orgLine[x<<shift] - recLine[x]);
                            count[bandIdx]++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            default:
                {
                    VM_ASSERT(!"Not a supported SAO types\n");
                }
            }
        }

    } // void GetCtuStatistics_Internal(...)
}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) 
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
