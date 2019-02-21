// Copyright (c) 2004-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_common_mvdiff_tbl.h"
#include "umc_vc1_huffman.h"

static uint8_t size_table[6] = {0, 2, 3, 4, 5, 8};
static uint8_t offset_table[6] = {0, 1, 3, 7, 15, 31};

uint16_t DecodeMVDiff(VC1Context* pContext,int32_t hpelfl,
                  int16_t* pdmv_x, int16_t* pdmv_y)
{
    int ret;
    int32_t data;
    int16_t index;
    int16_t dmv_x = 0;
    int16_t dmv_y = 0;
    uint16_t last_intra = 0;
    VC1PictureLayerHeader* picHeader = pContext->m_picLayerHeader;

    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                     &pContext->m_bitstream.bitOffset,
                                     &data,
                                     picHeader->m_pCurrMVDifftbl);
    VM_ASSERT(ret == 0);

    index = (int16_t)(data & 0x000000FF);
    data = data>>8;
    last_intra = (uint16_t)(data >> 8);

#ifdef VC1_DEBUG_ON
    if(index!=37)
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
                                VM_STRING("MVDiff index = %d\n"), index);
    else
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,
                                VC1_MV,VM_STRING("MVDiff index = %d\n"), 0);
#endif

    if(index < 35)
    {
        int32_t hpel = 0, sign, val;

        if(data&0x0f)
        {
            uint32_t index1 = (data & 0x0000000f) % 6;   // = index%6
            if (hpelfl + index1 == 6)
                hpel = 1;

            VC1_GET_BITS((size_table[index1] - hpel), val);
            sign = 0 - (val & 1);
            dmv_x = (int16_t)(sign ^ ((val >> 1) + offset_table[index1]));
            dmv_x = dmv_x - (int16_t)sign;
        }


        if(index > 5)
        {
            data = data >> 4;
            hpel = 0;
            uint32_t index2 = (data & 0x0000000f) % 6;   // = index/6

            if (hpelfl + index2 == 6)
                hpel = 1;

            VC1_GET_BITS((size_table[index2] - hpel), val);
            sign = 0 - (val & 1);
            dmv_y = (int16_t)(sign ^ ((val >> 1) + offset_table[index2]));
            dmv_y = dmv_y - (int16_t)sign;
        }
    }
    else  if (index == 35)
    {
        int32_t k_x = picHeader->m_pCurrMVRangetbl->k_x - hpelfl;
        int32_t k_y = picHeader->m_pCurrMVRangetbl->k_y - hpelfl;

        int32_t tmp_dmv_x = 0;
        int32_t tmp_dmv_y = 0;

        VC1_GET_BITS(k_x, tmp_dmv_x);
        VC1_GET_BITS(k_y, tmp_dmv_y);

        dmv_x = (int16_t)tmp_dmv_x;
        dmv_y = (int16_t)tmp_dmv_y;
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
                    VM_STRING("DMV_X = %d, DMV_Y = %d\n"), dmv_x, dmv_y);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
                    VM_STRING("intra = %d\n"), last_intra & 0x000F);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
                    VM_STRING("last = %d\n"), (last_intra & 0x00F0)>>4);
#endif

    * pdmv_x = dmv_x;
    * pdmv_y = dmv_y;
    return last_intra;
}

void PullBack_PPred(VC1Context* pContext, int16_t *pMVx, int16_t* pMVy, int32_t blk_num)
{
    int32_t Min;
    int32_t X = *pMVx;
    int32_t Y = *pMVy;

    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    int32_t IX = (sMB->m_currMBXpos<<6 ) + X;
    int32_t IY = (sMB->m_currMBYpos<<6 ) + Y;
    int32_t Width  = (sMB->widthMB<<6 )  - 4;
    int32_t Height = (sMB->heightMB<<6 ) - 4;

    Min = -60;

    if (VC1_GET_MBTYPE(pContext->m_pCurrMB->mbType)== VC1_MB_4MV_INTER)
    {
        Min = -28;

        if (blk_num==1 || blk_num==3)
        {
            IX += 32;
        }
        if (blk_num==2 || blk_num==3)
        {
            IY += 32;
        }
    }

    if (IX < Min)
    {
        X = Min-(sMB->m_currMBXpos<<6);
    }
    else if (IX > Width)
    {
        X = Width - (sMB->m_currMBXpos<<6);
    }

    if (IY < Min)
    {
        Y = Min - (sMB->m_currMBYpos<<6);
    }
    else if (IY > Height)
    {
        Y = Height - (pContext->m_pSingleMB->m_currMBYpos<<6);
    }

    (*pMVx) = (int16_t)X;
    (*pMVy) = (int16_t)Y;
}

void CropLumaPullBack(VC1Context* pContext, int16_t* xMV, int16_t* yMV)
{
    int32_t X = *xMV;
    int32_t Y = *yMV;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    int32_t IX = sMB->m_currMBXpos<<4;
    int32_t IY = sMB->m_currMBYpos<<4;
    int32_t Width  = (sMB->widthMB<<4);
    int32_t Height = (sMB->heightMB<<4);

    int32_t XPos = IX + (X>>2);
    int32_t YPos = IY + (Y>>2);

    if (XPos < -16)
    {
        X = (X&3)+((-IX-16)<<2);
    }
    else if (XPos > Width)
    {
        X = (X&3)+((Width-IX)<<2);
    }

    if (YPos < -16)
    {
        Y =(Y&3)+((-IY-16)<<2);
    }
    else if (YPos > Height)
    {
        Y= (Y&3)+((Height - IY)<<2);

    }

    (*xMV) = (int16_t)X;
    (*yMV) = (int16_t)Y;

}


void CropChromaPullBack(VC1Context* pContext, int16_t* xMV, int16_t* yMV)
{
    int32_t X = *xMV;
    int32_t Y = *yMV;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;

    int32_t IX = sMB->m_currMBXpos<<3;
    int32_t IY = sMB->m_currMBYpos<<3;
    int32_t Width  = (sMB->widthMB<<3);
    int32_t Height = (sMB->heightMB<<3);

    int32_t XPos = IX + (X >> 2);
    int32_t YPos = IY + (Y >> 2);

    if (XPos < -8)
    {
        X =(X&3)+((-8 - IX)<<2);
    }
    else if (XPos > Width)
    {
        X = (X&3)+((Width - IX)<<2);
    }

    if (YPos < -8)
    {
        Y =(Y&3)+((-8 - IY)<<2);;
    }
    else if (YPos > Height)
    {
        Y = (Y&3)+((Height -IY)<<2);

    }

    *xMV = (int16_t)X;
    *yMV = (int16_t)Y;

    pCurrMB->m_pBlocks[4].mv[0][0] = *xMV;
    pCurrMB->m_pBlocks[4].mv[0][1] = *yMV;
    pCurrMB->m_pBlocks[5].mv[0][0] = *xMV;
    pCurrMB->m_pBlocks[5].mv[0][1] = *yMV;
}

void CalculateProgressive1MV(VC1Context* pContext, int16_t *pPredMVx,int16_t *pPredMVy)
{
    int16_t x=0,y=0;

    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MVPredictors* MVPred = &pContext->MVPred;

    GetPredictProgressiveMV(MVPred->AMVPred[0],
                            MVPred->BMVPred[0],
                            MVPred->CMVPred[0],
                            &x,&y,0);
    x = PullBack_PredMV(&x,(sMB->m_currMBXpos<<6), -60,(sMB->widthMB<<6)-4);
    y = PullBack_PredMV(&y,(sMB->m_currMBYpos<<6), -60,(sMB->heightMB<<6)-4);

    HybridMV(pContext,MVPred->AMVPred[0],MVPred->CMVPred[0], &x,&y,0);
    *pPredMVx=x;
    *pPredMVy=y;

}

void CalculateProgressive1MV_B(VC1Context* pContext, int16_t *pPredMVx,int16_t *pPredMVy,
                            int32_t Back)
{
    int16_t x=0,y=0;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MVPredictors* MVPred = &pContext->MVPred;

    GetPredictProgressiveMV(MVPred->AMVPred[0],
                            MVPred->BMVPred[0],
                            MVPred->CMVPred[0],
                            &x, &y,
                            Back);

     x = PullBack_PredMV(&x,(sMB->m_currMBXpos<<5), -28,(sMB->widthMB<<5)-4);
     y = PullBack_PredMV(&y,(sMB->m_currMBYpos<<5), -28,(sMB->heightMB<<5)-4);

    *pPredMVx=x;
    *pPredMVy=y;
}

void CalculateProgressive4MV(VC1Context* pContext,
                                int16_t *pPredMVx,int16_t *pPredMVy,
                                int32_t blk_num)
{
    int16_t x,y;
    VC1MVPredictors* MVPred = &pContext->MVPred;

    GetPredictProgressiveMV(MVPred->AMVPred[blk_num],
                            MVPred->BMVPred[blk_num],
                            MVPred->CMVPred[blk_num],
                            &x,&y,0);
    PullBack_PPred4MV(pContext->m_pSingleMB,&x,&y, blk_num);
    HybridMV(pContext,MVPred->AMVPred[blk_num],MVPred->CMVPred[blk_num], &x,&y,0);
    *pPredMVx=x;
    *pPredMVy=y;
}


void Scale_Direct_MV(VC1PictureLayerHeader* picHeader, int16_t X, int16_t Y,
                     int16_t* Xf, int16_t* Yf,
                     int16_t* Xb, int16_t* Yb)
{
    int32_t hpelfl = (int32_t)((picHeader->MVMODE==VC1_MVMODE_HPEL_1MV) ||
                            (picHeader->MVMODE==VC1_MVMODE_HPELBI_1MV));
    int32_t ScaleFactor = picHeader->ScaleFactor;

    if (hpelfl)
    {
        * Xf = (int16_t)(2*((X*ScaleFactor + 255)>>9));
        * Yf = (int16_t)(2*((Y*ScaleFactor + 255)>>9));

        ScaleFactor -=256;
        * Xb = (int16_t)(2*((X*ScaleFactor + 255)>>9));
        * Yb = (int16_t)(2*((Y*ScaleFactor + 255)>>9));
    }
    else
    {
        * Xf =(int16_t)((X*ScaleFactor + 128)>>8);
        * Yf =(int16_t)((Y*ScaleFactor + 128)>>8);

        ScaleFactor -=256;
        * Xb =(int16_t)((X*ScaleFactor + 128)>>8);
        * Yb =(int16_t)((Y*ScaleFactor + 128)>>8);
    }
}
void Scale_Direct_MV_Interlace(VC1PictureLayerHeader* picHeader,
                               int16_t X, int16_t Y,
                               int16_t* Xf, int16_t* Yf,
                               int16_t* Xb, int16_t* Yb)
{
    int32_t ScaleFactor = picHeader->ScaleFactor;
    * Xf =(int16_t)((X*ScaleFactor+128)>>8);
    * Yf =(int16_t)((Y*ScaleFactor+128)>>8);

    ScaleFactor -=256;

    * Xb =(int16_t)((X*ScaleFactor+128)>>8);
    * Yb =(int16_t)((Y*ScaleFactor+128)>>8);
}

void DeriveSecondStageChromaMV(VC1Context* pContext, int16_t* xMV, int16_t* yMV)
{
    VC1MB *pMB = pContext->m_pCurrMB;
    int16_t IX, IY;

    if(((*xMV) == VC1_MVINTRA) || ((*yMV) == VC1_MVINTRA))
    {
        return;
    }
    else
    {
        const int16_t Round[4] = {0, 0, 0, 1};
        int16_t CMV_X, CMV_Y;

        IX = *xMV;
        IY = *yMV;

        CMV_X = (IX + Round[IX & 3]) >> 1;
        CMV_Y = (IY + Round[IY & 3]) >> 1;

        if (pContext->m_seqLayerHeader.FASTUVMC)
        {
            const int16_t RndTbl[3] = {1, 0, -1};
            CMV_X = CMV_X + RndTbl[1 + (VC1_SIGN(CMV_X) * (vc1_abs_16s(CMV_X) % 2))];
            CMV_Y = CMV_Y + RndTbl[1 + (VC1_SIGN(CMV_Y) * (vc1_abs_16s(CMV_Y) % 2))];
        }

        *xMV = CMV_X;
        *yMV = CMV_Y;

        pMB->m_pBlocks[4].mv[0][0] = *xMV;
        pMB->m_pBlocks[4].mv[0][1] = *yMV;
        pMB->m_pBlocks[5].mv[0][0] = *xMV;
        pMB->m_pBlocks[5].mv[0][1] = *yMV;
    }
}

#endif //MFX_ENABLE_VC1_VIDEO_DECODE
