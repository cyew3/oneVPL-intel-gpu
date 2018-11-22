// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_common_mvdiff_tbl.h"
#include "umc_vc1_huffman.h"

void ApplyMVPredictionCalculate( VC1Context* pContext,
                                int16_t* pMVx,
                                int16_t* pMVy,
                                int32_t dmv_x,
                                int32_t dmv_y)
{
    const VC1MVRange *pMVRange = pContext->m_picLayerHeader->m_pCurrMVRangetbl;
    int32_t RangeX, RangeY;
    int32_t DMV_X, DMV_Y;
    int32_t PredictorX, PredictorY;
    int16_t MVx, MVy;

    RangeX = pMVRange->r_x;
    RangeY = pMVRange->r_y;

    DMV_X = dmv_x;
    DMV_Y = dmv_y;

    PredictorX = *pMVx;
    PredictorY = *pMVy;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("DMV_X  = %d, DMV_Y  = %d, PredictorX = %d, PredictorY = %d\n"),
        DMV_X, DMV_Y, PredictorX,PredictorY);
#endif
    DMV_X += PredictorX;
    DMV_Y += PredictorY;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("DMV_X  = %d, DMV_Y  = %d, RangeX = %d, RangeY = %d\n"),
        DMV_X, DMV_Y, RangeX,RangeY);
#endif
    MVx = (int16_t)( ((DMV_X + RangeX) & ( (RangeX << 1) - 1)) - RangeX );
    MVy = (int16_t)( ((DMV_Y + RangeY) & ( (RangeY << 1) - 1)) - RangeY);

    *pMVx = MVx;
    *pMVy = MVy;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),MVx, MVy);
#endif
}


void ApplyMVPredictionCalculateOneReference( VC1PictureLayerHeader* picLayerHeader,
                                             int16_t* pMVx, int16_t* pMVy,
                                             int32_t dmv_x,  int32_t dmv_y, uint8_t same_polatity)
{
    const VC1MVRange *pMVRange = picLayerHeader->m_pCurrMVRangetbl;
    int32_t RangeX, RangeY;
    int32_t DMV_X, DMV_Y;
    int32_t PredictorX, PredictorY;
    int16_t MVx, MVy;

    RangeX = pMVRange->r_x;
    RangeY = pMVRange->r_y;

    DMV_X = dmv_x;
    DMV_Y = dmv_y;

    PredictorX = *pMVx;
    PredictorY = *pMVy;

    if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV
        || picLayerHeader->MVMODE==VC1_MVMODE_HPEL_1MV)
    {
        RangeX  = RangeX << 1;
        RangeY  = RangeY << 1;
    }

    if (picLayerHeader->NUMREF == 1)
    {
        RangeY  = RangeY >> 1;
    }

    #ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("DMV_X  = %d, DMV_Y  = %d, PredictorX = %d, PredictorY = %d\n"),
        DMV_X, DMV_Y, PredictorX,PredictorY);
    #endif
    DMV_X += PredictorX;
    DMV_Y += PredictorY;
    #ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("DMV_X  = %d, DMV_Y  = %d, RangeX = %d, RangeY = %d\n"),
        DMV_X, DMV_Y, RangeX,RangeY);
    #endif

    //MVx = (int16_t)( ((DMV_X + RangeX) & (2 * RangeX - 1)) - RangeX );
    //MVy = (int16_t)( ((DMV_Y + RangeY) & (2 * RangeY - 1)) - RangeY);
    MVx = (int16_t)( ((DMV_X + RangeX) & ( (RangeX << 1) - 1)) - RangeX );
    MVy = (int16_t)( ((DMV_Y + RangeY) & ( (RangeY << 1) - 1)) - RangeY);

    if ( same_polatity && (picLayerHeader->BottomField))
        //MVy = (int16_t)( ( ((DMV_Y + RangeY - 1) & (2 * RangeY - 1)) - RangeY )+1);
        MVy = (int16_t)( ( ((DMV_Y + RangeY - 1) & ( (RangeY << 1) - 1)) - RangeY )+1);

    *pMVx = MVx;
    *pMVy = MVy;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),MVx, MVy);
#endif
}

void ApplyMVPredictionCalculateTwoReference( VC1PictureLayerHeader* picLayerHeader,
                                             int16_t* pMVx,
                                             int16_t* pMVy,
                                             int32_t dmv_x,
                                             int32_t dmv_y,
                                             uint8_t same_polatity)
{
    const VC1MVRange *pMVRange = picLayerHeader->m_pCurrMVRangetbl;
    int32_t RangeX, RangeY;
    int32_t DMV_X, DMV_Y;
    int32_t PredictorX, PredictorY;
    int16_t MVx, MVy;

    RangeX = pMVRange->r_x;
    RangeY = pMVRange->r_y;

    DMV_X = dmv_x;
    DMV_Y = dmv_y;

    PredictorX = *pMVx;
    PredictorY = *pMVy;


    if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV
        || picLayerHeader->MVMODE==VC1_MVMODE_HPEL_1MV)
    {
        RangeX  = RangeX << 1;
        RangeY  = RangeY << 1;
    }

    if (picLayerHeader->NUMREF == 1)
    {
        RangeY  = RangeY >> 1;
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("DMV_X  = %d, DMV_Y  = %d, PredictorX = %d, PredictorY = %d\n"),
        DMV_X, DMV_Y, PredictorX,PredictorY);
#endif
    DMV_X += PredictorX;
    DMV_Y += PredictorY;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("DMV_X  = %d, DMV_Y  = %d, RangeX = %d, RangeY = %d\n"),
        DMV_X, DMV_Y, RangeX,RangeY);
#endif

    MVx = (int16_t)( ((DMV_X + RangeX) & ((RangeX << 1) - 1)) - RangeX );
    MVy = (int16_t)( ((DMV_Y + RangeY) & ((RangeY << 1) - 1)) - RangeY);

    if ( same_polatity && (picLayerHeader->BottomField))
        MVy = (int16_t)( ( ((DMV_Y - 1 + RangeY ) & ((RangeY << 1) - 1)) - RangeY )+1);

    *pMVx = MVx;
    *pMVy = MVy;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV,
        VM_STRING("ApplyPred : MV_X  = %d, MV_Y  = %d\n"),MVx, MVy);
#endif
}

void CropLumaPullBack_Adv(VC1Context* pContext, int16_t* xMV, int16_t* yMV)
{
    int32_t X = *xMV;
    int32_t Y = *yMV;
    int32_t xNum;
    int32_t yNum;
    int32_t IX = pContext->m_pSingleMB->m_currMBXpos;
    int32_t IY = pContext->m_pSingleMB->m_currMBYpos;

    int32_t Width  = (2*(pContext->m_seqLayerHeader.CODED_WIDTH+1));
    int32_t Height = (2*(pContext->m_seqLayerHeader.CODED_HEIGHT+1) >> 1);

    xNum = (IX<<4) + (X >> 2);
    yNum = (IY<<3) + (Y >> 3);

    if (xNum < -17)
    {
        X -= ((xNum +17) << 2);
    }
    else if (xNum > Width)
    {
        X -= ((xNum -Width) << 2);

    }

    if (yNum < -18)
    {
        Y -= ((yNum+18) << 3);
    }
    else if (yNum > Height+1)
    {
        Y -= ((yNum-Height-1) << 3);
    }

    (*xMV) = (int16_t)X;
    (*yMV) = (int16_t)Y;

}
void CropLumaPullBackField_Adv(VC1Context* pContext, int16_t* xMV, int16_t* yMV)
{
    int32_t X = *xMV;
    int32_t Y = *yMV;
    int32_t xNum;
    int32_t yNum;
    int32_t IX = pContext->m_pSingleMB->m_currMBXpos;
    int32_t IY = pContext->m_pSingleMB->m_currMBYpos;

    int32_t Width  = ((pContext->m_seqLayerHeader.CODED_WIDTH+1) << 1);
    int32_t Height = (pContext->m_seqLayerHeader.CODED_HEIGHT+1);


    if (pContext->m_picLayerHeader->CurrField)
        IY  -= (pContext->m_pSingleMB->heightMB >> 1);

    IY = IY << 1;
    Y  = Y << 1;

    xNum = (IX<<4) + (X >> 2);
    yNum = (IY<<3) + (Y >> 3);

    if (xNum < -17)
    {
        X -= 4*(xNum +17);
    }
    else if (xNum > Width)
    {
        X -= 4*(xNum -Width);
    }

    if (yNum < -18)
    {
        Y -= 8*(yNum+18);
    }
    else if (yNum > Height+1)
    {
        Y -= 8*(yNum-Height-1);
    }
    Y = Y >> 1;

    (*xMV) = (int16_t)X;
    (*yMV) = (int16_t)Y;

}
void CropChromaPullBack_Adv(VC1Context* pContext, int16_t* xMV, int16_t* yMV)
{
    int32_t X = *xMV;
    int32_t Y = *yMV;
    int32_t XPos;
    int32_t YPos;
    int32_t IX = pContext->m_pSingleMB->m_currMBXpos;
    int32_t IY = pContext->m_pSingleMB->m_currMBYpos;

    int32_t Width  = (pContext->m_seqLayerHeader.CODED_WIDTH+1);
    int32_t Height = ((pContext->m_seqLayerHeader.CODED_HEIGHT+1) >> 1);


    int32_t MinY = -8;
    int32_t MaxY = Height;

    if (pContext->m_picLayerHeader->CurrField)
        IY  -= (pContext->m_pSingleMB->heightMB >> 1);

    if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
    {
        --MinY;
        ++MaxY;
        IY = IY << 1;
        Y  = Y  << 1;
    }

    XPos = (IX<<3) + (X >> 2);
    YPos = (IY<<2) + (Y >> 3);

    if (XPos < -8)
    {
        X -= 4*(XPos+8);
    }
    else if (XPos > Width)
    {
        X -= ((XPos-Width) << 2);
    }

    if (YPos < MinY)
    {
        Y -= 8*(YPos - MinY);
    }
    else if (YPos > MaxY)
    {
        Y -= ((YPos-MaxY) << 3);
    }

    if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
    {
       Y = Y >> 1;
    }
    *xMV = (int16_t)X;
    *yMV = (int16_t)Y;

    //pContext->m_pCurrMB->m_pBlocks[4].mv[0][0] = *xMV;
    //pContext->m_pCurrMB->m_pBlocks[4].mv[0][1] = *yMV;
    //pContext->m_pCurrMB->m_pBlocks[5].mv[0][0] = *xMV;
    //pContext->m_pCurrMB->m_pBlocks[5].mv[0][1] = *yMV;
}

void CalculateProgressive1MV_B_Adv  (VC1Context* pContext,
                                int16_t *pPredMVx,int16_t *pPredMVy,
                                int32_t Back)
{
    int16_t x=0,y=0;

    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MVPredictors* MVPred = &pContext->MVPred;

    GetPredictProgressiveMV(MVPred->AMVPred[0],
                            MVPred->BMVPred[0],
                            MVPred->CMVPred[0],
                            &x,&y,Back);

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_BFRAMES,
        VM_STRING("predict MV (%d,%d), back = %d\n"),x,y,Back);
#endif

    x = PullBack_PredMV(&x,(sMB->m_currMBXpos<<6), -60,(sMB->widthMB<<6)-4);
    y = PullBack_PredMV(&y,(sMB->m_currMBYpos<<6), -60,(sMB->heightMB<<6)-4);
    *pPredMVx=x;
    *pPredMVy=y;
}

void CalculateProgressive4MV_Adv(VC1Context* pContext,
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

void CalculateInterlaceFrame1MV_P(VC1MVPredictors* MVPredictors,
                              int16_t *pPredMVx,int16_t *pPredMVy)
{
    VC1MVPredictors MVPred;

    uint32_t validPredictors = 0;
    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    MVPred = *MVPredictors;

    if(MVPred.AMVPred[0])
    {
        if(MVPred.FieldMB[0][0])
        {
            MV_px[0] = (MVPred.AMVPred[0]->mv[0][0] + MVPred.AMVPred[0]->mv_bottom[0][0] + 1)>>1;
            MV_py[0] = (MVPred.AMVPred[0]->mv[0][1] + MVPred.AMVPred[0]->mv_bottom[0][1] + 1)>>1;
        }
        else
        {
            MV_px[0] = MVPred.AMVPred[0]->mv[0][0];
            MV_py[0] = MVPred.AMVPred[0]->mv[0][1];
        }
        validPredictors++;
    }

    if(MVPred.BMVPred[0])
    {
        if(MVPred.FieldMB[0][1])
        {
            MV_px[1] = (MVPred.BMVPred[0]->mv[0][0] + MVPred.BMVPred[0]->mv_bottom[0][0] + 1)>>1;
            MV_py[1] = (MVPred.BMVPred[0]->mv[0][1] + MVPred.BMVPred[0]->mv_bottom[0][1] + 1)>>1;
        }
        else
        {
            MV_px[1] = MVPred.BMVPred[0]->mv[0][0];
            MV_py[1] = MVPred.BMVPred[0]->mv[0][1];
        }
        validPredictors++;
    }

    if(MVPred.CMVPred[0])
    {
        if(MVPred.FieldMB[0][2])
        {
            MV_px[2] = (MVPred.CMVPred[0]->mv[0][0] + MVPred.CMVPred[0]->mv_bottom[0][0] + 1)>>1;
            MV_py[2] = (MVPred.CMVPred[0]->mv[0][1] + MVPred.CMVPred[0]->mv_bottom[0][1] + 1)>>1;
        }
        else
        {
            MV_px[2] = MVPred.CMVPred[0]->mv[0][0];
            MV_py[2] = MVPred.CMVPred[0]->mv[0][1];
        }
        validPredictors++;
    }

    //computing frame predictors
    if (validPredictors > 1)
    {
        // 2 or 3 predictors are available
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else
    {
        // 1 or 0 predictor is available
        *pPredMVx = MV_px[0]+MV_px[1]+MV_px[2];
        *pPredMVy = MV_py[0]+MV_py[1]+MV_py[2];
    }
}

void CalculateInterlaceFrame1MV_B(VC1MVPredictors* MVPredictors,
                                    int16_t *f_x,int16_t *f_y,
                                    int16_t *b_x,int16_t *b_y,
                                    uint32_t back)
{
    VC1MVPredictors MVPred;
    uint32_t f_back = back;
    uint32_t b_back = 1 - back;
    uint32_t same = 0;
    uint32_t opposite = 0;

    //uint32_t validPredictors = 0;
    int16_t f_MV_px[] = {0,0,0};
    int16_t f_MV_py[] = {0,0,0};

    int16_t b_MV_px[] = {0,0,0};
    int16_t b_MV_py[] = {0,0,0};

    int16_t MV_px_sameField[] = {0,0,0};
    int16_t MV_py_sameField[] = {0,0,0};

    int16_t MV_px_oppField[] = {0,0,0};
    int16_t MV_py_oppField[] = {0,0,0};

    MVPred = *MVPredictors;

    if(MVPred.AMVPred[0])
    {
        if(MVPred.FieldMB[0][0])
        {
            f_MV_px[0] = (MVPred.AMVPred[0]->mv[f_back][0]
                                + MVPred.AMVPred[0]->mv_bottom[f_back][0] + 1)>>1;
            f_MV_py[0] = (MVPred.AMVPred[0]->mv[f_back][1]
                                + MVPred.AMVPred[0]->mv_bottom[f_back][1] + 1)>>1;
        }
        else
        {
            f_MV_px[0] = MVPred.AMVPred[0]->mv[f_back][0];
            f_MV_py[0] = MVPred.AMVPred[0]->mv[f_back][1];
        }

        //validPredictors++;

        b_MV_px[0] = MVPred.AMVPred[0]->mv[b_back][0];
        b_MV_py[0] = MVPred.AMVPred[0]->mv[b_back][1];

        //classifying candidate mv
        if(b_MV_py[0] & 4)
        {
            MV_px_oppField[opposite] = b_MV_px[0];
            MV_py_oppField[opposite] = b_MV_py[0];
            ++opposite;
        }
        else
        {
            MV_px_sameField[same] = b_MV_px[0];
            MV_py_sameField[same] = b_MV_py[0];
            ++same;
        }
    }

    if(MVPred.BMVPred[0])
    {
        if(MVPred.FieldMB[0][1])
        {
            f_MV_px[1] = (MVPred.BMVPred[0]->mv[f_back][0]
                            + MVPred.BMVPred[0]->mv_bottom[f_back][0] + 1)>>1;
            f_MV_py[1] = (MVPred.BMVPred[0]->mv[f_back][1]
                            + MVPred.BMVPred[0]->mv_bottom[f_back][1] + 1)>>1;
        }
        else
        {
            f_MV_px[1] = MVPred.BMVPred[0]->mv[f_back][0];
            f_MV_py[1] = MVPred.BMVPred[0]->mv[f_back][1];
        }

        //validPredictors++;

        b_MV_px[1] = MVPred.BMVPred[0]->mv[b_back][0];
        b_MV_py[1] = MVPred.BMVPred[0]->mv[b_back][1];

        //classifying candidate mv
        if(b_MV_py[1] & 4)
        {
            MV_px_oppField[opposite] = b_MV_px[1];
            MV_py_oppField[opposite] = b_MV_py[1];
            ++opposite;
        }
        else
        {
            MV_px_sameField[same] = b_MV_px[1];
            MV_py_sameField[same] = b_MV_py[1];
            ++same;
        }
    }

    if(MVPred.CMVPred[0])
    {
        if(MVPred.FieldMB[0][2])
        {
            f_MV_px[2] = (MVPred.CMVPred[0]->mv[f_back][0]
                                    + MVPred.CMVPred[0]->mv_bottom[f_back][0] + 1)>>1;
            f_MV_py[2] = (MVPred.CMVPred[0]->mv[f_back][1]
                                    + MVPred.CMVPred[0]->mv_bottom[f_back][1] + 1)>>1;
        }
        else
        {
            f_MV_px[2] = MVPred.CMVPred[0]->mv[f_back][0];
            f_MV_py[2] = MVPred.CMVPred[0]->mv[f_back][1];
        }
        //validPredictors++;
        b_MV_px[2] = MVPred.CMVPred[0]->mv[b_back][0];
        b_MV_py[2] = MVPred.CMVPred[0]->mv[b_back][1];

        //classifying candidate mv
        if(b_MV_py[2] & 4)
        {
            MV_px_oppField[opposite] = b_MV_px[2];
            MV_py_oppField[opposite] = b_MV_py[2];
            ++opposite;
        }
        else
        {
            MV_px_sameField[same] = b_MV_px[2];
            MV_py_sameField[same] = b_MV_py[2];
            ++same;
        }
    }

    //computing frame predictors
    if (same + opposite > 1)
    {
        // 2 or 3 predictors are available
        *f_x = (int16_t)median3(f_MV_px);
        *f_y = (int16_t)median3(f_MV_py);
    }
    else
    {
        // 1 or 0 predictor is available
        *f_x = f_MV_px[0]+f_MV_px[1]+f_MV_px[2];
        *f_y = f_MV_py[0]+f_MV_py[1]+f_MV_py[2];
    }

    //computation of Field MV predictors from candidate mv
    //3 predictors are available
    if((same == 3) || (opposite == 3))
    {
        *b_x = (int16_t)median3(b_MV_px);
        *b_y = (int16_t)median3(b_MV_py);
    }
    else if(same >= opposite)
    {
        *b_x = MV_px_sameField[0];
        *b_y = MV_py_sameField[0];
    }
    else
    {
        *b_x = MV_px_oppField[0];
        *b_y = MV_py_oppField[0];
    }
}

void CalculateInterlaceFrame1MV_B_Interpolate(VC1MVPredictors* MVPredictors,
                                              int16_t *f_x,int16_t *f_y,
                                              int16_t *b_x,int16_t *b_y)
{
    VC1MVPredictors MVPred;

    uint32_t validPredictors = 0;
    int16_t f_MV_px[] = {0,0,0};
    int16_t f_MV_py[] = {0,0,0};

    int16_t b_MV_px[] = {0,0,0};
    int16_t b_MV_py[] = {0,0,0};

    MVPred = *MVPredictors;

    if(MVPred.AMVPred[0])
    {
        if(MVPred.FieldMB[0][0])
        {
            f_MV_px[0] = (MVPred.AMVPred[0]->mv[0][0]
                                + MVPred.AMVPred[0]->mv_bottom[0][0] + 1)>>1;
            f_MV_py[0] = (MVPred.AMVPred[0]->mv[0][1]
                                + MVPred.AMVPred[0]->mv_bottom[0][1] + 1)>>1;

            b_MV_px[0] = (MVPred.AMVPred[0]->mv[1][0]
                                + MVPred.AMVPred[0]->mv_bottom[1][0] + 1)>>1;
            b_MV_py[0] = (MVPred.AMVPred[0]->mv[1][1]
                                + MVPred.AMVPred[0]->mv_bottom[1][1] + 1)>>1;
        }
        else
        {
            f_MV_px[0] = MVPred.AMVPred[0]->mv[0][0];
            f_MV_py[0] = MVPred.AMVPred[0]->mv[0][1];
            b_MV_px[0] = MVPred.AMVPred[0]->mv[1][0];
            b_MV_py[0] = MVPred.AMVPred[0]->mv[1][1];
        }

        validPredictors++;
    }

    if(MVPred.BMVPred[0])
    {
        if(MVPred.FieldMB[0][1])
        {
            f_MV_px[1] = (MVPred.BMVPred[0]->mv[0][0]
                            + MVPred.BMVPred[0]->mv_bottom[0][0] + 1)>>1;
            f_MV_py[1] = (MVPred.BMVPred[0]->mv[0][1]
                            + MVPred.BMVPred[0]->mv_bottom[0][1] + 1)>>1;
            b_MV_px[1] = (MVPred.BMVPred[0]->mv[1][0]
                            + MVPred.BMVPred[0]->mv_bottom[1][0] + 1)>>1;
            b_MV_py[1] = (MVPred.BMVPred[0]->mv[1][1]
                            + MVPred.BMVPred[0]->mv_bottom[1][1] + 1)>>1;
        }
        else
        {
            f_MV_px[1] = MVPred.BMVPred[0]->mv[0][0];
            f_MV_py[1] = MVPred.BMVPred[0]->mv[0][1];
            b_MV_px[1] = MVPred.BMVPred[0]->mv[1][0];
            b_MV_py[1] = MVPred.BMVPred[0]->mv[1][1];
        }

        validPredictors++;
    }

    if(MVPred.CMVPred[0])
    {
        if(MVPred.FieldMB[0][2])
        {
            f_MV_px[2] = (MVPred.CMVPred[0]->mv[0][0]
                                    + MVPred.CMVPred[0]->mv_bottom[0][0] + 1)>>1;
            f_MV_py[2] = (MVPred.CMVPred[0]->mv[0][1]
                                    + MVPred.CMVPred[0]->mv_bottom[0][1] + 1)>>1;
            b_MV_px[2] = (MVPred.CMVPred[0]->mv[1][0]
                                    + MVPred.CMVPred[0]->mv_bottom[1][0] + 1)>>1;
            b_MV_py[2] = (MVPred.CMVPred[0]->mv[1][1]
                                    + MVPred.CMVPred[0]->mv_bottom[1][1] + 1)>>1;
        }
        else
        {
            f_MV_px[2] = MVPred.CMVPred[0]->mv[0][0];
            f_MV_py[2] = MVPred.CMVPred[0]->mv[0][1];
            b_MV_px[2] = MVPred.CMVPred[0]->mv[1][0];
            b_MV_py[2] = MVPred.CMVPred[0]->mv[1][1];
        }
        validPredictors++;
    }

    //computing frame predictors
    if (validPredictors > 1)
    {
        // 2 or 3 predictors are available
        *f_x = (int16_t)median3(f_MV_px);
        *f_y = (int16_t)median3(f_MV_py);

        *b_x = (int16_t)median3(b_MV_px);
        *b_y = (int16_t)median3(b_MV_py);
    }
    else
    {
        // 1 or 0 predictor is available
        *f_x = f_MV_px[0]+f_MV_px[1]+f_MV_px[2];
        *f_y = f_MV_py[0]+f_MV_py[1]+f_MV_py[2];

        *b_x = b_MV_px[0]+b_MV_px[1]+b_MV_px[2];
        *b_y = b_MV_py[0]+b_MV_py[1]+b_MV_py[2];
    }
}

void Calculate4MVFrame_Adv(VC1MVPredictors* MVPredictors,
                              int16_t *pPredMVx,int16_t *pPredMVy,
                              uint32_t blk_num)
{
   VC1MVPredictors MVPred;

    uint32_t validPredictors = 0;
    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    MVPred = *MVPredictors;

    if(MVPred.AMVPred[blk_num])
    {
        if(MVPred.FieldMB[blk_num][0])
        {
            MV_px[0] = (MVPred.AMVPred[blk_num]->mv[0][0] + MVPred.AMVPred[blk_num]->mv_bottom[0][0] + 1)>>1;
            MV_py[0] = (MVPred.AMVPred[blk_num]->mv[0][1] + MVPred.AMVPred[blk_num]->mv_bottom[0][1] + 1)>>1;
        }
        else
        {
            MV_px[0] = MVPred.AMVPred[blk_num]->mv[0][0];
            MV_py[0] = MVPred.AMVPred[blk_num]->mv[0][1];
        }
        validPredictors++;
    }

    if(MVPred.BMVPred[blk_num])
    {
        if(MVPred.FieldMB[blk_num][1])
        {
            MV_px[1] = (MVPred.BMVPred[blk_num]->mv[0][0] + MVPred.BMVPred[blk_num]->mv_bottom[0][0] + 1)>>1;
            MV_py[1] = (MVPred.BMVPred[blk_num]->mv[0][1] + MVPred.BMVPred[blk_num]->mv_bottom[0][1] + 1)>>1;
        }
        else
        {
            MV_px[1] = MVPred.BMVPred[blk_num]->mv[0][0];
            MV_py[1] = MVPred.BMVPred[blk_num]->mv[0][1];
        }
        validPredictors++;
    }

    if(MVPred.CMVPred[blk_num])
    {
        if(MVPred.FieldMB[blk_num][2])
        {
            MV_px[2] = (MVPred.CMVPred[blk_num]->mv[0][0] + MVPred.CMVPred[blk_num]->mv_bottom[0][0] + 1)>>1;
            MV_py[2] = (MVPred.CMVPred[blk_num]->mv[0][1] + MVPred.CMVPred[blk_num]->mv_bottom[0][1] + 1)>>1;
        }
        else
        {
            MV_px[2] = MVPred.CMVPred[blk_num]->mv[0][0];
            MV_py[2] = MVPred.CMVPred[blk_num]->mv[0][1];
        }
        validPredictors++;
    }

    //computing frame predictors
    if (validPredictors > 1)
    {
        // 2 or 3 predictors are available
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else
    {
        // 1 or 0 predictor is available
        *pPredMVx = MV_px[0]+MV_px[1]+MV_px[2];
        *pPredMVy = MV_py[0]+MV_py[1]+MV_py[2];
    }
}


//       ---- ----            ---- ----
//      | B  | C  |          | C  | B  | (LAST MB in row)
//  ---- ---- ----     or     ---- ----
// | A  |Cur |               | A  |Cur |
//  ---- ----                 ---- ----
void PredictInterlaceFrame1MV(VC1Context* pContext)
{
    VC1MVPredictors MVPred;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    uint32_t LeftTopRightPositionFlag = pCurrMB->LeftTopRightPositionFlag;
    int32_t width = pContext->m_pSingleMB->MaxWidthMB;
    VC1MB *pA = NULL, *pB = NULL, *pC = NULL;

    memset(&MVPred,0,sizeof(VC1MVPredictors));

    //A predictor
    if(VC1_IS_NO_LEFT_MB(LeftTopRightPositionFlag))
    {
        pA=(pContext->m_pCurrMB - 1);

        if(!pA->IntraFlag)
        {
            if(VC1_GET_MBTYPE(pA->mbType) == VC1_MB_1MV_INTER ||   VC1_GET_MBTYPE(pA->mbType) == VC1_MB_2MV_INTER)
                MVPred.AMVPred[0] = &pA->m_pBlocks[0];
            else
                MVPred.AMVPred[0] = &pA->m_pBlocks[1];

            MVPred.FieldMB[0][0] = VC1_IS_MVFIELD(pA->mbType);
        }
    }

    if (VC1_IS_NO_TOP_MB(LeftTopRightPositionFlag))
    {
        //B predictor
        pB=(pContext->m_pCurrMB - width);

        if(!pB->IntraFlag)
        {
            if(VC1_GET_MBTYPE(pB->mbType) == VC1_MB_4MV_INTER)
                MVPred.BMVPred[0] = &pB->m_pBlocks[2];
            else
                MVPred.BMVPred[0] = &pB->m_pBlocks[0];

            MVPred.FieldMB[0][1] = VC1_IS_MVFIELD(pB->mbType);
        }

        //C predictor
        if (VC1_IS_NO_RIGHT_MB(LeftTopRightPositionFlag))
        {
            //this block is not last in row
            pC=(pContext->m_pCurrMB - width + 1);
            if(!pC->IntraFlag)
            {
                if(VC1_GET_MBTYPE(pC->mbType) == VC1_MB_4MV_INTER)
                    MVPred.CMVPred[0] = &pC->m_pBlocks[2];
                else
                    MVPred.CMVPred[0] = &pC->m_pBlocks[0];

                MVPred.FieldMB[0][2] = VC1_IS_MVFIELD(pC->mbType);
            }
        }
        else
        {
            //this block is last in row
            pC=(pContext->m_pCurrMB - width -1);
            if(!pC->IntraFlag)
            {
                if(VC1_GET_MBTYPE(pC->mbType) == VC1_MB_4MV_INTER)
                    MVPred.CMVPred[0] = &pC->m_pBlocks[3];
                else if (VC1_GET_MBTYPE(pC->mbType) == VC1_MB_4MV_FIELD_INTER)
                    MVPred.CMVPred[0] = &pC->m_pBlocks[1];
                else
                    MVPred.CMVPred[0] = &pC->m_pBlocks[0];
                MVPred.FieldMB[0][2] = VC1_IS_MVFIELD(pC->mbType);
            }
        }
    }

    pContext->MVPred = MVPred;
}


void PredictInterlace4MVFrame_Adv(VC1Context* pContext)
{
    VC1MVPredictors MVPred;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    uint32_t LeftTopRightPositionFlag = pCurrMB->LeftTopRightPositionFlag;
    int32_t width = pContext->m_pSingleMB->MaxWidthMB;
    VC1MB *pA = NULL, *pB = NULL, *pC = NULL;

    memset(&MVPred,0,sizeof(VC1MVPredictors));

    // A predictor
    if(VC1_IS_NO_LEFT_MB(LeftTopRightPositionFlag))
    {
        pA=(pCurrMB - 1); //candidate for blocks 0 and 2

        if(!pA->IntraFlag)
        {
            if(VC1_GET_MBTYPE(pA->mbType) == VC1_MB_1MV_INTER || VC1_GET_MBTYPE(pA->mbType) == VC1_MB_2MV_INTER)
            {
                //block 0
                MVPred.AMVPred[0] = &pA->m_pBlocks[0];
                //block 2
                MVPred.AMVPred[2] = &pA->m_pBlocks[0];
            }
            else if(VC1_GET_MBTYPE(pA->mbType) == VC1_MB_4MV_INTER)
            {
                //block 0
                MVPred.AMVPred[0] = &pA->m_pBlocks[1];
                //block 2
                MVPred.AMVPred[2] = &pA->m_pBlocks[3];
            }
            else
            {
                //block 0
                MVPred.AMVPred[0] = &pA->m_pBlocks[1];
                //block 2
                MVPred.AMVPred[2] = &pA->m_pBlocks[1];
            }

            MVPred.FieldMB[0][0] = VC1_IS_MVFIELD(pA->mbType);
            MVPred.FieldMB[2][0] = MVPred.FieldMB[0][0];
        }
    }

    if(VC1_IS_NO_TOP_MB(LeftTopRightPositionFlag))
    {
        // B predictor
        pB=(pCurrMB - width);
        if(!pB->IntraFlag)
        {
            if(VC1_GET_MBTYPE(pB->mbType) == VC1_MB_1MV_INTER || VC1_GET_MBTYPE(pB->mbType) == VC1_MB_2MV_INTER)
            {
                //block 0
                MVPred.BMVPred[0] = &pB->m_pBlocks[0];
                //block 1
                MVPred.BMVPred[1] = &pB->m_pBlocks[0];
            }
            else if(VC1_GET_MBTYPE(pB->mbType) == VC1_MB_4MV_INTER)
            {
                //block 0
                MVPred.BMVPred[0] = &pB->m_pBlocks[2];
                //block 1
                MVPred.BMVPred[1] = &pB->m_pBlocks[3];
            }
            else
            {
                //block 0
                MVPred.BMVPred[0] = &pB->m_pBlocks[0];
                //block 1
                MVPred.BMVPred[1] = &pB->m_pBlocks[1];
            }
            MVPred.FieldMB[0][1] = VC1_IS_MVFIELD(pB->mbType);
            MVPred.FieldMB[1][1] = MVPred.FieldMB[0][1];
        }

        // C predictor
        if (VC1_IS_NO_RIGHT_MB(LeftTopRightPositionFlag))
        {
            //this block is not last in row
            pC=(pCurrMB - width + 1);
            if(!pC->IntraFlag)
            {
                if(VC1_GET_MBTYPE(pC->mbType) != VC1_MB_4MV_INTER)
                {
                    //block 0
                    MVPred.CMVPred[0] = &pC->m_pBlocks[0];
                    //block 1
                    MVPred.CMVPred[1] = &pC->m_pBlocks[0];
                }
                else
                {
                    //block 0
                    MVPred.CMVPred[0] = &pC->m_pBlocks[2];
                    //block 1
                    MVPred.CMVPred[1] = &pC->m_pBlocks[2];
                }
                MVPred.FieldMB[0][2] = VC1_IS_MVFIELD(pC->mbType);
                MVPred.FieldMB[1][2] = MVPred.FieldMB[0][2];
            }
        }
        else
        {
            //this block is last in row
            pC=(pCurrMB - width -1);
            if(!pC->IntraFlag)
            {
                if(VC1_GET_MBTYPE(pC->mbType) == VC1_MB_1MV_INTER || VC1_GET_MBTYPE(pC->mbType) == VC1_MB_2MV_INTER)
                {
                    //block 0
                    MVPred.CMVPred[0] = &pC->m_pBlocks[0];
                    //block 1
                    MVPred.CMVPred[1] = &pC->m_pBlocks[0];
                }
                else if(VC1_GET_MBTYPE(pC->mbType) == VC1_MB_4MV_INTER)
                {
                    //block 0
                    MVPred.CMVPred[0] = &pC->m_pBlocks[3];
                    //block 1
                    MVPred.CMVPred[1] = &pC->m_pBlocks[3];
                }
                else
                {
                    //block 0
                    MVPred.CMVPred[0] = &pC->m_pBlocks[1];
                    //block 1
                    MVPred.CMVPred[1] = &pC->m_pBlocks[1];
                }
                MVPred.FieldMB[0][2] = VC1_IS_MVFIELD(pC->mbType);
                MVPred.FieldMB[1][2] = MVPred.FieldMB[0][2];
            }
        }
    }

    MVPred.AMVPred[1] = &pCurrMB->m_pBlocks[0];
    MVPred.BMVPred[2] = &pCurrMB->m_pBlocks[0];
    MVPred.CMVPred[2] = &pCurrMB->m_pBlocks[1];
    MVPred.AMVPred[3] = &pCurrMB->m_pBlocks[2];
    MVPred.BMVPred[3] = &pCurrMB->m_pBlocks[0];
    MVPred.CMVPred[3] = &pCurrMB->m_pBlocks[1];

    pContext->MVPred = MVPred;
}

void PredictInterlace4MVField_Adv(VC1Context* pContext)
{
    VC1MVPredictors MVPred;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    uint32_t LeftTopRightPositionFlag = pCurrMB->LeftTopRightPositionFlag;
    int32_t width = pContext->m_pSingleMB->MaxWidthMB;
    VC1MB *pA = NULL, *pB = NULL, *pC = NULL;

    memset(&MVPred,0,sizeof(VC1MVPredictors));

    // A predictor
    if(VC1_IS_NO_LEFT_MB(LeftTopRightPositionFlag))
    {
        pA=(pCurrMB - 1);

        if(!pA->IntraFlag)
        {
            switch(VC1_GET_MBTYPE(pA->mbType))
            {
            case VC1_MB_1MV_INTER:
                {
                    MVPred.AMVPred[0] = &pA->m_pBlocks[0];
                    MVPred.AMVPred[2] = &pA->m_pBlocks[0];
                }
                break;
            case VC1_MB_2MV_INTER:
                {
                    MVPred.AMVPred[0] = &pA->m_pBlocks[0];
                    MVPred.AMVPred[2] = &pA->m_pBlocks[0];
                    MVPred.FieldMB[2][0] = 1;
                }
                break;
            case VC1_MB_4MV_INTER:
                {
                    MVPred.AMVPred[0] = &pA->m_pBlocks[1];
                    MVPred.AMVPred[2] = &pA->m_pBlocks[3];
                }
                break;
            case VC1_MB_4MV_FIELD_INTER:
                {
                    MVPred.AMVPred[0] = &pA->m_pBlocks[1];
                    MVPred.AMVPred[2] = &pA->m_pBlocks[1];
                    MVPred.FieldMB[2][0] = 1;
                }
                break;
            default:
                pA=NULL;
                break;
            }
        }
    }

    if(VC1_IS_NO_TOP_MB(LeftTopRightPositionFlag))
    {
        // B predictor
        pB=(pCurrMB - width);
        if(!pB->IntraFlag)
        {
            switch(VC1_GET_MBTYPE(pB->mbType))
            {
            case VC1_MB_1MV_INTER:
                {
                    MVPred.BMVPred[0] = &pB->m_pBlocks[0];
                    MVPred.BMVPred[1] = &pB->m_pBlocks[0];
                    MVPred.BMVPred[2] = &pB->m_pBlocks[0];
                    MVPred.BMVPred[3] = &pB->m_pBlocks[0];
                }
                break;
            case VC1_MB_2MV_INTER:
                {
                    MVPred.BMVPred[0] = &pB->m_pBlocks[0];
                    MVPred.BMVPred[1] = &pB->m_pBlocks[0];
                    MVPred.BMVPred[2] = &pB->m_pBlocks[0];
                    MVPred.BMVPred[3] = &pB->m_pBlocks[0];
                    MVPred.FieldMB[2][1] = 1;
                    MVPred.FieldMB[3][1] = 1;
                }
                break;
            case VC1_MB_4MV_INTER:
                {
                    MVPred.BMVPred[0] = &pB->m_pBlocks[2];
                    MVPred.BMVPred[1] = &pB->m_pBlocks[3];
                    MVPred.BMVPred[2] = &pB->m_pBlocks[2];
                    MVPred.BMVPred[3] = &pB->m_pBlocks[3];
                }
                break;
            case VC1_MB_4MV_FIELD_INTER:
                {
                    MVPred.BMVPred[0] = &pB->m_pBlocks[0];
                    MVPred.BMVPred[1] = &pB->m_pBlocks[1];
                    MVPred.BMVPred[2] = &pB->m_pBlocks[0];
                    MVPred.BMVPred[3] = &pB->m_pBlocks[1];
                    MVPred.FieldMB[2][1] = 1;
                    MVPred.FieldMB[3][1] = 1;
                }
                break;
            default:
                pB=NULL;
                break;
            }
        }

        // C predictor
        if (VC1_IS_NO_RIGHT_MB(LeftTopRightPositionFlag))
        {
            //this block is not last in row
            pC=(pCurrMB - width + 1);
            if(!pC->IntraFlag)
            {
                switch(VC1_GET_MBTYPE(pC->mbType))
                {
                case VC1_MB_1MV_INTER:
                    {
                        MVPred.CMVPred[0] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[1] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[2] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[3] = &pC->m_pBlocks[0];
                    }
                    break;
                case VC1_MB_2MV_INTER:
                    {
                        MVPred.CMVPred[0] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[1] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[2] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[3] = &pC->m_pBlocks[0];
                        MVPred.FieldMB[2][2] = 1;
                        MVPred.FieldMB[3][2] = 1;
                    }
                    break;
                case VC1_MB_4MV_INTER:
                    {
                        MVPred.CMVPred[0] = &pC->m_pBlocks[2];
                        MVPred.CMVPred[1] = &pC->m_pBlocks[2];
                        MVPred.CMVPred[2] = &pC->m_pBlocks[2];
                        MVPred.CMVPred[3] = &pC->m_pBlocks[2];
                    }
                    break;
                case VC1_MB_4MV_FIELD_INTER:
                    {
                        MVPred.CMVPred[0] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[1] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[2] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[3] = &pC->m_pBlocks[0];
                        MVPred.FieldMB[2][2] = 1;
                        MVPred.FieldMB[3][2] = 1;
                    }
                    break;
                default:
                    pC=NULL;
                    break;
                }
            }
        }
        else
        {
            //this block is last in row
            pC=(pCurrMB - width -1);
            if(!pC->IntraFlag)
            {
                switch(VC1_GET_MBTYPE(pC->mbType))
                {
                case VC1_MB_1MV_INTER:
                    {
                        MVPred.CMVPred[0] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[1] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[2] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[3] = &pC->m_pBlocks[0];
                    }
                    break;
                case VC1_MB_2MV_INTER:
                    {
                        MVPred.CMVPred[0] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[1] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[2] = &pC->m_pBlocks[0];
                        MVPred.CMVPred[3] = &pC->m_pBlocks[0];
                        MVPred.FieldMB[2][2] = 1;
                        MVPred.FieldMB[3][2] = 1;
                    }
                    break;
                case VC1_MB_4MV_INTER:
                    {
                        MVPred.CMVPred[0] = &pC->m_pBlocks[3];
                        MVPred.CMVPred[1] = &pC->m_pBlocks[3];
                        MVPred.CMVPred[2] = &pC->m_pBlocks[3];
                        MVPred.CMVPred[3] = &pC->m_pBlocks[3];
                    }
                    break;
                case VC1_MB_4MV_FIELD_INTER:
                    {
                        MVPred.CMVPred[0] = &pC->m_pBlocks[1];
                        MVPred.CMVPred[1] = &pC->m_pBlocks[1];
                        MVPred.CMVPred[2] = &pC->m_pBlocks[1];
                        MVPred.CMVPred[3] = &pC->m_pBlocks[1];
                        MVPred.FieldMB[2][2] = 1;
                        MVPred.FieldMB[3][2] = 1;
                    }
                    break;
                default:
                    pC=NULL;
                    break;
                }
            }
        }
    }


    MVPred.AMVPred[1] = &pCurrMB->m_pBlocks[0];
    MVPred.AMVPred[3] = &pCurrMB->m_pBlocks[0];
    MVPred.FieldMB[3][0] = 1;

    pContext->MVPred = MVPred;

}
//2 Field MV candidate MV derivation
void PredictInterlace2MV_Field_Adv(VC1MB* pCurrMB,
                                   int16_t pPredMVx[2],int16_t pPredMVy[2],
                                   int16_t backTop, int16_t backBottom,
                                   uint32_t widthMB)
{
    uint32_t LeftTopRightPositionFlag = pCurrMB->LeftTopRightPositionFlag;
    uint32_t OppositeTopField = 0;
    uint32_t OppositeBottomField = 0;
    uint32_t SameTopField = 0;
    uint32_t SameBottomField = 0;

    int16_t MV_px[2][3] = { {0,0,0}, {0,0,0} };
    int16_t MV_py[2][3] = { {0,0,0}, {0,0,0} };

    int16_t MV_px_sameField[2][3] = { {0,0,0}, {0,0,0} };
    int16_t MV_py_sameField[2][3] = { {0,0,0}, {0,0,0} };

    int16_t MV_px_oppField[2][3] = { {0,0,0}, {0,0,0} };
    int16_t MV_py_oppField[2][3] = { {0,0,0}, {0,0,0} };

    VC1MB *pA = NULL, *pB = NULL, *pC = NULL;

    // A predictor
    if(VC1_IS_NO_LEFT_MB(LeftTopRightPositionFlag))
    {
        pA=(pCurrMB - 1);

        if(!pA->IntraFlag)
        {
            switch(VC1_GET_MBTYPE(pA->mbType))
            {
            case VC1_MB_1MV_INTER:
                {
                    //add mv of A
                    //top field
                    MV_px[0][0] = pA->m_pBlocks[0].mv[backTop][0];
                    MV_py[0][0] = pA->m_pBlocks[0].mv[backTop][1];

                    //bottom field
                    MV_px[1][0] = pA->m_pBlocks[0].mv[backBottom][0];
                    MV_py[1][0] = pA->m_pBlocks[0].mv[backBottom][1];
                }
                break;
            case VC1_MB_2MV_INTER:
                {
                    //top field
                    //add the top field mv
                    MV_px[0][0] = pA->m_pBlocks[0].mv[backTop][0];
                    MV_py[0][0] = pA->m_pBlocks[0].mv[backTop][1];

                    //bottom field
                    //add the bottom field block mv
                    MV_px[1][0] = pA->m_pBlocks[0].mv_bottom[backBottom][0];
                    MV_py[1][0] = pA->m_pBlocks[0].mv_bottom[backBottom][1];
                }
                break;
            case VC1_MB_4MV_INTER:
                {
                    //top field
                    // add the top right block mv
                    MV_px[0][0] = pA->m_pBlocks[1].mv[backTop][0];
                    MV_py[0][0] = pA->m_pBlocks[1].mv[backTop][1];

                    //bottom field
                    //add the bottom right block mv
                    MV_px[1][0] = pA->m_pBlocks[3].mv[backBottom][0];
                    MV_py[1][0] = pA->m_pBlocks[3].mv[backBottom][1];
                }
                break;
            case VC1_MB_4MV_FIELD_INTER:
                {
                    //top field
                    //add the top right field mv
                    MV_px[0][0] = pA->m_pBlocks[1].mv[backTop][0];
                    MV_py[0][0] = pA->m_pBlocks[1].mv[backTop][1];

                    //bottom feild
                    //add the bottom right field mv
                    MV_px[1][0] = pA->m_pBlocks[1].mv_bottom[backBottom][0];
                    MV_py[1][0] = pA->m_pBlocks[1].mv_bottom[backBottom][1];
                }
                break;
            default:
                pA=NULL;
                break;
            }

            //classifying top candidate mv
            if(MV_py[0][0] & 4)
            {
                MV_px_oppField[0][OppositeTopField] = MV_px[0][0];
                MV_py_oppField[0][OppositeTopField] = MV_py[0][0];
                ++OppositeTopField;
            }
            else
            {
                MV_px_sameField[0][SameTopField] = MV_px[0][0];
                MV_py_sameField[0][SameTopField] = MV_py[0][0];
                ++SameTopField;
            }

            //classifying bottom candidate mv
            if(MV_py[1][0] & 4)
            {
                MV_px_oppField[1][OppositeBottomField] = MV_px[1][0];
                MV_py_oppField[1][OppositeBottomField] = MV_py[1][0];
                ++OppositeBottomField;
            }
            else
            {
                MV_px_sameField[1][SameBottomField] = MV_px[1][0];
                MV_py_sameField[1][SameBottomField] = MV_py[1][0];
                ++SameBottomField;
            }
        }
    }

    if(VC1_IS_NO_TOP_MB(LeftTopRightPositionFlag))
    {
        // B predictor
        pB=(pCurrMB - widthMB);
        if(!pB->IntraFlag)
        {
            switch(VC1_GET_MBTYPE(pB->mbType))
            {
            case VC1_MB_1MV_INTER:
                {
                    //top field
                    MV_px[0][1] = pB->m_pBlocks[0].mv[backTop][0];
                    MV_py[0][1] = pB->m_pBlocks[0].mv[backTop][1];

                    //bottom field
                    MV_px[1][1] = pB->m_pBlocks[0].mv[backBottom][0];
                    MV_py[1][1] = pB->m_pBlocks[0].mv[backBottom][1];
                }
                break;
            case VC1_MB_2MV_INTER:
                {
                    //top field
                    //add the top field mv
                    MV_px[0][1] = pB->m_pBlocks[0].mv[backTop][0];
                    MV_py[0][1] = pB->m_pBlocks[0].mv[backTop][1];

                    //bottom field
                    //add the bottom field mv
                    MV_px[1][1] = pB->m_pBlocks[0].mv_bottom[backBottom][0];
                    MV_py[1][1] = pB->m_pBlocks[0].mv_bottom[backBottom][1];
                }
                break;
            case VC1_MB_4MV_INTER:
                {
                    //top field
                    //add the bottom left block mv
                    MV_px[0][1] = pB->m_pBlocks[2].mv[backTop][0];
                    MV_py[0][1] = pB->m_pBlocks[2].mv[backTop][1];

                    //bottom field
                    //add the bottom left block mv
                    MV_px[1][1] = pB->m_pBlocks[2].mv[backBottom][0];
                    MV_py[1][1] = pB->m_pBlocks[2].mv[backBottom][1];
                }
                break;
            case VC1_MB_4MV_FIELD_INTER:
                {
                    //top field
                    //add the top left field mv
                    MV_px[0][1] = pB->m_pBlocks[0].mv[backTop][0];
                    MV_py[0][1] = pB->m_pBlocks[0].mv[backTop][1];

                    //bottom field
                    //add the bottom left field mv
                    MV_px[1][1] = pB->m_pBlocks[0].mv_bottom[backBottom][0];
                    MV_py[1][1] = pB->m_pBlocks[0].mv_bottom[backBottom][1];
                }
                break;
            default:
                pB=NULL;
                break;
            }

            //classifying top candidate mv
            if(MV_py[0][1] & 4)
            {
                MV_px_oppField[0][OppositeTopField] = MV_px[0][1];
                MV_py_oppField[0][OppositeTopField] = MV_py[0][1];
                ++OppositeTopField;
            }
            else
            {
                MV_px_sameField[0][SameTopField] = MV_px[0][1];
                MV_py_sameField[0][SameTopField] = MV_py[0][1];
                ++SameTopField;
            }

            //classifying bottom candidate mv
            if(MV_py[1][1] & 4)
            {
                MV_px_oppField[1][OppositeBottomField] = MV_px[1][1];
                MV_py_oppField[1][OppositeBottomField] = MV_py[1][1];
                ++OppositeBottomField;
            }
            else
            {
                MV_px_sameField[1][SameBottomField] = MV_px[1][1];
                MV_py_sameField[1][SameBottomField] = MV_py[1][1];
                ++SameBottomField;
            }
        }

        // C predictor
        if (VC1_IS_NO_RIGHT_MB(LeftTopRightPositionFlag))
        {
            //this block is not last in row
            pC=(pCurrMB - widthMB + 1);
            if(!pC->IntraFlag)
            {
                switch(VC1_GET_MBTYPE(pC->mbType))
                {
                case VC1_MB_1MV_INTER:
                    {
                        //top field
                        MV_px[0][2] = pC->m_pBlocks[0].mv[backTop][0];
                        MV_py[0][2] = pC->m_pBlocks[0].mv[backTop][1];

                        //bottom field
                        MV_px[1][2] = pC->m_pBlocks[0].mv[backBottom][0];
                        MV_py[1][2] = pC->m_pBlocks[0].mv[backBottom][1];
                    }
                    break;
                case VC1_MB_2MV_INTER:
                    {
                        //top field
                        //add the top field mv
                        MV_px[0][2] = pC->m_pBlocks[0].mv[backTop][0];
                        MV_py[0][2] = pC->m_pBlocks[0].mv[backTop][1];

                        //bottom field
                        //add the bottom field block mv
                        MV_px[1][2] = pC->m_pBlocks[0].mv_bottom[backBottom][0];
                        MV_py[1][2] = pC->m_pBlocks[0].mv_bottom[backBottom][1];
                    }
                    break;
                case VC1_MB_4MV_INTER:
                    {
                        //top field
                        //add the bottom left block mv
                        MV_px[0][2] = pC->m_pBlocks[2].mv[backTop][0];
                        MV_py[0][2] = pC->m_pBlocks[2].mv[backTop][1];

                        //bottom filed
                        MV_px[1][2] = pC->m_pBlocks[2].mv[backBottom][0];
                        MV_py[1][2] = pC->m_pBlocks[2].mv[backBottom][1];
                    }
                    break;
                case VC1_MB_4MV_FIELD_INTER:
                    {
                        //top field
                        //add the top left field block mv
                        MV_px[0][2] = pC->m_pBlocks[0].mv[backTop][0];
                        MV_py[0][2] = pC->m_pBlocks[0].mv[backTop][1];

                        //bottom field
                        MV_px[1][2] = pC->m_pBlocks[0].mv_bottom[backBottom][0];
                        MV_py[1][2] = pC->m_pBlocks[0].mv_bottom[backBottom][1];
                    }
                    break;
                default:
                    pC=NULL;
                    break;
                }

                //classifying top candidate mv
                if(MV_py[0][2] & 4)
                {
                    MV_px_oppField[0][OppositeTopField] = MV_px[0][2];
                    MV_py_oppField[0][OppositeTopField] = MV_py[0][2];
                    ++OppositeTopField;
                }
                else
                {
                    MV_px_sameField[0][SameTopField] = MV_px[0][2];
                    MV_py_sameField[0][SameTopField] = MV_py[0][2];
                    ++SameTopField;
                }

                //classifying bottom candidate mv
                if(MV_py[1][2] & 4)
                {
                    MV_px_oppField[1][OppositeBottomField] = MV_px[1][2];
                    MV_py_oppField[1][OppositeBottomField] = MV_py[1][2];
                    ++OppositeBottomField;
                }
                else
                {
                    MV_px_sameField[1][SameBottomField] = MV_px[1][2];
                    MV_py_sameField[1][SameBottomField] = MV_py[1][2];
                    ++SameBottomField;
                }
            }
        }
        else
        {
            //this block is last in row
            pC=(pCurrMB - widthMB -1);
            if(!pC->IntraFlag)
            {
                switch(VC1_GET_MBTYPE(pC->mbType))
                {
                case VC1_MB_1MV_INTER:
                    {
                        //top field
                        MV_px[0][2] = pC->m_pBlocks[0].mv[backTop][0];
                        MV_py[0][2] = pC->m_pBlocks[0].mv[backTop][1];

                        //bottom field
                        MV_px[1][2] = pC->m_pBlocks[0].mv[backBottom][0];
                        MV_py[1][2] = pC->m_pBlocks[0].mv[backBottom][1];
                    }
                    break;
                case VC1_MB_2MV_INTER:
                    {
                        //top field
                        //add the top field mv
                        MV_px[0][2] = pC->m_pBlocks[0].mv[backTop][0];
                        MV_py[0][2] = pC->m_pBlocks[0].mv[backTop][1];

                        //bottom field
                        MV_px[1][2] = pC->m_pBlocks[0].mv_bottom[backBottom][0];
                        MV_py[1][2] = pC->m_pBlocks[0].mv_bottom[backBottom][1];
                    }
                    break;
                case VC1_MB_4MV_INTER:
                    {
                        //top field
                        //add the bottom right block mv
                        MV_px[0][2] = pC->m_pBlocks[3].mv[backTop][0];
                        MV_py[0][2] = pC->m_pBlocks[3].mv[backTop][1];

                        //bottom field
                        MV_px[1][2] = pC->m_pBlocks[3].mv[backBottom][0];
                        MV_py[1][2] = pC->m_pBlocks[3].mv[backBottom][1];
                    }
                    break;
                case VC1_MB_4MV_FIELD_INTER:
                    {
                        //top field
                        //add the top right field mv
                        MV_px[0][2] = pC->m_pBlocks[1].mv[backTop][0];
                        MV_py[0][2] = pC->m_pBlocks[1].mv[backTop][1];

                        //bottom field
                        MV_px[1][2] = pC->m_pBlocks[1].mv_bottom[backBottom][0];
                        MV_py[1][2] = pC->m_pBlocks[1].mv_bottom[backBottom][1];
                    }
                    break;
                default:
                    pC=NULL;
                    break;
                }
                //classifying top candidate mv
                if(MV_py[0][2] & 4)
                {
                    MV_px_oppField[0][OppositeTopField] = MV_px[0][2];
                    MV_py_oppField[0][OppositeTopField] = MV_py[0][2];
                    ++OppositeTopField;
                }
                else
                {
                    MV_px_sameField[0][SameTopField] = MV_px[0][2];
                    MV_py_sameField[0][SameTopField] = MV_py[0][2];
                    ++SameTopField;
                }

                //classifying bottom candidate mv
                if(MV_py[1][2] & 4)
                {
                    MV_px_oppField[1][OppositeBottomField] = MV_px[1][2];
                    MV_py_oppField[1][OppositeBottomField] = MV_py[1][2];
                    ++OppositeBottomField;
                }
                else
                {
                    MV_px_sameField[1][SameBottomField] = MV_px[1][2];
                    MV_py_sameField[1][SameBottomField] = MV_py[1][2];
                    ++SameBottomField;
                }
            }
        }
    }

    //computation of Field MV predictors from candidate mv
    //3 or 2 predictors are available
    if((SameTopField == 3) || (OppositeTopField == 3))
    {
        pPredMVx[0] = (int16_t)median3(MV_px[0]);
        pPredMVy[0] = (int16_t)median3(MV_py[0]);
    }
    else if(SameTopField >= OppositeTopField)
    {
        pPredMVx[0] = MV_px_sameField[0][0];
        pPredMVy[0] = MV_py_sameField[0][0];
    }
    else
    {
        pPredMVx[0] = MV_px_oppField[0][0];
        pPredMVy[0] = MV_py_oppField[0][0];
    }

    //computation of Field MV predictors from candidate mv
    //3 or 2 predictors are available
    if((SameBottomField == 3) || (OppositeBottomField == 3))
    {
        pPredMVx[1] = (int16_t)median3(MV_px[1]);
        pPredMVy[1] = (int16_t)median3(MV_py[1]);
    }
    else if(SameBottomField >= OppositeBottomField)
    {
        pPredMVx[1] = MV_px_sameField[1][0];
        pPredMVy[1] = MV_py_sameField[1][0];
    }
    else
    {
        pPredMVx[1] = MV_px_oppField[1][0];
        pPredMVy[1] = MV_py_oppField[1][0];
    }
}

void CalculateInterlace4MV_TopField_Adv(VC1MVPredictors* MVPredictors,
                                      int16_t *pPredMVx,int16_t *pPredMVy,
                                      uint32_t blk_num)
{
    uint32_t validPredictorsOppositeField = 0;
    uint32_t validPredictorsSameField = 0;

    int16_t MV_px[3] = {0,0,0}; //first index - top/bottom
    int16_t MV_py[3] = {0,0,0}; //second - A,B,C predictors

    int16_t MV_px_sameField[3] = {0,0,0};
    int16_t MV_py_sameField[3] = {0,0,0};

    int16_t MV_px_oppField[3] = {0,0,0};
    int16_t MV_py_oppField[3] = {0,0,0};

    VC1MVPredictors MVPred;

    MVPred = *MVPredictors;

    if(MVPred.AMVPred[blk_num])
    {
        MV_px[0] = MVPred.AMVPred[blk_num]->mv[0][0];
        MV_py[0] = MVPred.AMVPred[blk_num]->mv[0][1];
        //classifying top candidate mv
        if(MV_py[0] & 4)
        {
            MV_px_oppField[validPredictorsOppositeField] = MV_px[0];
            MV_py_oppField[validPredictorsOppositeField] = MV_py[0];
            ++validPredictorsOppositeField;
        }
        else
        {
            MV_px_sameField[validPredictorsSameField] = MV_px[0];
            MV_py_sameField[validPredictorsSameField] = MV_py[0];
            ++validPredictorsSameField;
        }
    }

    if(MVPred.BMVPred[blk_num])
    {
        MV_px[1] = MVPred.BMVPred[blk_num]->mv[0][0];
        MV_py[1] = MVPred.BMVPred[blk_num]->mv[0][1];
        //classifying top candidate mv
        if(MV_py[1] & 4)
        {
            MV_px_oppField[validPredictorsOppositeField] = MV_px[1];
            MV_py_oppField[validPredictorsOppositeField] = MV_py[1];
            ++validPredictorsOppositeField;
        }
        else
        {
            MV_px_sameField[validPredictorsSameField] = MV_px[1];
            MV_py_sameField[validPredictorsSameField] = MV_py[1];
            ++validPredictorsSameField;
        }
    }


    if(MVPred.CMVPred[blk_num])
    {
        MV_px[2] = MVPred.CMVPred[blk_num]->mv[0][0];
        MV_py[2] = MVPred.CMVPred[blk_num]->mv[0][1];
        //classifying top candidate mv
        if(MV_py[2] & 4)
        {
            MV_px_oppField[validPredictorsOppositeField] = MV_px[2];
            MV_py_oppField[validPredictorsOppositeField] = MV_py[2];
            ++validPredictorsOppositeField;
        }
        else
        {
            MV_px_sameField[validPredictorsSameField] = MV_px[2];
            MV_py_sameField[validPredictorsSameField] = MV_py[2];
            ++validPredictorsSameField;
        }
    }

    //computation of top Field MV predictors from candidate mv
    //3 or 2 predictors are available
    if((validPredictorsSameField == 3) || (validPredictorsOppositeField == 3))
    {
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else if(validPredictorsSameField >= validPredictorsOppositeField)
    {
        *pPredMVx = MV_px_sameField[0];
        *pPredMVy = MV_py_sameField[0];
    }
    else
    {
        *pPredMVx = MV_px_oppField[0];
        *pPredMVy = MV_py_oppField[0];
    }
 }

void CalculateInterlace4MV_BottomField_Adv(VC1MVPredictors* MVPredictors,
                                      int16_t *pPredMVx,int16_t *pPredMVy,
                                      uint32_t blk_num)
{
    uint32_t validPredictorsOppositeField = 0;
    uint32_t validPredictorsSameField = 0;

    int16_t MV_px[3] = {0,0,0}; //first index - top/bottom
    int16_t MV_py[3] = {0,0,0}; //second - A,B,C predictors

    int16_t MV_px_sameField[3] = {0,0,0};
    int16_t MV_py_sameField[3] = {0,0,0};

    int16_t MV_px_oppField[3] = {0,0,0};
    int16_t MV_py_oppField[3] = {0,0,0};

    VC1MVPredictors MVPred;

    MVPred = *MVPredictors;

    if(MVPred.AMVPred[blk_num])
    {
        if(!MVPred.FieldMB[blk_num][0])
        {
            MV_px[0] = MVPred.AMVPred[blk_num]->mv[0][0];
            MV_py[0] = MVPred.AMVPred[blk_num]->mv[0][1];
        }
        else
        {
            MV_px[0] = MVPred.AMVPred[blk_num]->mv_bottom[0][0];
            MV_py[0] = MVPred.AMVPred[blk_num]->mv_bottom[0][1];
        }

        //classifying top candidate mv
        if(MV_py[0] & 4)
        {
            MV_px_oppField[validPredictorsOppositeField] = MV_px[0];
            MV_py_oppField[validPredictorsOppositeField] = MV_py[0];
            ++validPredictorsOppositeField;
        }
        else
        {
            MV_px_sameField[validPredictorsSameField] = MV_px[0];
            MV_py_sameField[validPredictorsSameField] = MV_py[0];
            ++validPredictorsSameField;
        }
    }

    if(MVPred.BMVPred[blk_num])
    {
        if(!MVPred.FieldMB[blk_num][1])
        {
            MV_px[1] = MVPred.BMVPred[blk_num]->mv[0][0];
            MV_py[1] = MVPred.BMVPred[blk_num]->mv[0][1];
        }
        else
        {
            MV_px[1] = MVPred.BMVPred[blk_num]->mv_bottom[0][0];
            MV_py[1] = MVPred.BMVPred[blk_num]->mv_bottom[0][1];
        }

        //classifying top candidate mv
        if(MV_py[1] & 4)
        {
            MV_px_oppField[validPredictorsOppositeField] = MV_px[1];
            MV_py_oppField[validPredictorsOppositeField] = MV_py[1];
            ++validPredictorsOppositeField;
        }
        else
        {
            MV_px_sameField[validPredictorsSameField] = MV_px[1];
            MV_py_sameField[validPredictorsSameField] = MV_py[1];
            ++validPredictorsSameField;
        }
    }


    if(MVPred.CMVPred[blk_num])
    {
        if(!MVPred.FieldMB[blk_num][2])
        {
            MV_px[2] = MVPred.CMVPred[blk_num]->mv[0][0];
            MV_py[2] = MVPred.CMVPred[blk_num]->mv[0][1];
        }
        else
        {
            MV_px[2] = MVPred.CMVPred[blk_num]->mv_bottom[0][0];
            MV_py[2] = MVPred.CMVPred[blk_num]->mv_bottom[0][1];

        }

        //classifying top candidate mv
        if(MV_py[2] & 4)
        {
            MV_px_oppField[validPredictorsOppositeField] = MV_px[2];
            MV_py_oppField[validPredictorsOppositeField] = MV_py[2];
            ++validPredictorsOppositeField;
        }
        else
        {
            MV_px_sameField[validPredictorsSameField] = MV_px[2];
            MV_py_sameField[validPredictorsSameField] = MV_py[2];
            ++validPredictorsSameField;
        }
    }

    //computation of top Field MV predictors from candidate mv
    //3 or 2 predictors are available
    if((validPredictorsSameField == 3) || (validPredictorsOppositeField == 3))
    {
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else if(validPredictorsSameField >= validPredictorsOppositeField)
    {
        *pPredMVx = MV_px_sameField[0];
        *pPredMVy = MV_py_sameField[0];
    }
    else
    {
        *pPredMVx = MV_px_oppField[0];
        *pPredMVy = MV_py_oppField[0];
    }
 }

void DecodeMVDiff_Adv(VC1Context* pContext,int16_t* pdmv_x, int16_t* pdmv_y)
{
    int ret;
    int32_t index;
    int16_t dmv_x = 0;
    int16_t dmv_y = 0;
    int32_t sign;
    int32_t val;
    VC1PictureLayerHeader* picHeader = pContext->m_picLayerHeader;

    static const uint8_t offset_table[2][9] = { {0, 1, 2, 4, 8, 16, 32, 64, 128},
                                       {0, 1, 3, 7, 15, 31, 63, 127, 255}};
    static const uint8_t* curr_offset;

    ret = DecodeHuffmanOne(
        &pContext->m_bitstream.pBitstream,
        &pContext->m_bitstream.bitOffset,
        &val,
        picHeader->m_pCurrMVDifftbl
        );

    VM_ASSERT(ret == 0);

    index = val & 0x000000FF;

    if (index != 72)
    {
        val = val>>8;
        int32_t index1 = val & 0x000000FF; //index%9
        val = val>>8;
        int32_t index2 = val & 0x000000FF; //index/9

        int32_t extend_x = (picHeader->DMVRANGE & VC1_DMVRANGE_HORIZONTAL_RANGE)?1:0;
        int32_t extend_y = (picHeader->DMVRANGE & VC1_DMVRANGE_VERTICAL_RANGE)?1:0;

        curr_offset = offset_table[extend_x];

        if (index1)
        {
            VC1_GET_BITS((index1 + extend_x),val);
            sign = -(val & 1);
            dmv_x = (int16_t)((sign ^ ( (val >> 1) + curr_offset[index1])) - sign);
        }
        else
            dmv_x = 0;

        curr_offset = offset_table[extend_y];

        if (index2)
        {
            VC1_GET_BITS((index2 + extend_y),val);
            sign = -(val & 1);
            dmv_y = (int16_t)((sign ^ ( (val >> 1) + curr_offset[index2])) - sign);
        }
        else
            dmv_y =0;
    }
    else
    {
        uint8_t k_x = picHeader->m_pCurrMVRangetbl->k_x;
        uint8_t k_y = picHeader->m_pCurrMVRangetbl->k_y;

        int32_t tmp_dmv_x = 0;
        int32_t tmp_dmv_y = 0;

        VC1_GET_BITS(k_x, tmp_dmv_x);
        VC1_GET_BITS(k_y, tmp_dmv_y);

        dmv_x = (int16_t)tmp_dmv_x;
        dmv_y = (int16_t)tmp_dmv_y;
    }

    //dMV scaling in case of fields and Half pel resolution
    if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
    {
        if (pContext->m_picLayerHeader->MVMODE == VC1_MVMODE_HPELBI_1MV ||
            pContext->m_picLayerHeader->MVMODE == VC1_MVMODE_HPEL_1MV)
        {
            dmv_x <<=1;
            dmv_y <<=1;
        }
    }

    *pdmv_x=dmv_x;
    *pdmv_y=dmv_y;
}

uint8_t DecodeMVDiff_TwoReferenceField_Adv(VC1Context* pContext,
                                         int16_t* pdmv_x, int16_t* pdmv_y)
{
    int ret;
    int32_t index;
    int16_t dmv_x = 0;
    int16_t dmv_y = 0;
    int32_t sign;
    int32_t val;
    uint8_t predictor_flag;

    VC1PictureLayerHeader* picHeader = pContext->m_picLayerHeader;


    static uint8_t offset_table[2][9] = {{0, 1, 2, 4, 8, 16, 32, 64, 128},
                                             {0, 1, 3, 7, 15, 31, 63, 127, 255}};
    static uint8_t size_table[16]   = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7};
    uint8_t* curr_offset;

    ret = DecodeHuffmanOne(
        &pContext->m_bitstream.pBitstream,
        &pContext->m_bitstream.bitOffset,
        &val,
        picHeader->m_pCurrMVDifftbl
        );

    VM_ASSERT(ret == 0);

    index = val & 0x000000FF;

    if (index != 126)
    {
        int32_t extend_x = (picHeader->DMVRANGE & VC1_DMVRANGE_HORIZONTAL_RANGE)? 1:0;
        int32_t extend_y = (picHeader->DMVRANGE & VC1_DMVRANGE_VERTICAL_RANGE)  ? 1:0;

        val = val>>8;
        int32_t index1 = val & 0x000000FF; //index%9
        val = val>>8;
        int32_t index2 = val & 0x000000FF; //index/9

        curr_offset = offset_table[extend_x];

        if (index1)
        {
            VC1_GET_BITS((index1 + extend_x),val);
            sign = -(val & 1);
            dmv_x = (int16_t)((sign ^ ( (val >> 1) + curr_offset[index1])) - sign);
        }
        else
            dmv_x = 0;

        curr_offset = offset_table[extend_y];

        if (index2 > 1)
        {
            VC1_GET_BITS(size_table[(index2 + 2*extend_y)&0xF],val);
            sign = -(val & 1);
            dmv_y = (int16_t)((sign ^ ( (val >> 1) + curr_offset[index2>>1])) - sign);
        }
        else
            dmv_y =0;

        predictor_flag = (uint8_t)(index2 & 1);
    }
    else
    {
        int32_t k_x = picHeader->m_pCurrMVRangetbl->k_x;
        int32_t k_y = picHeader->m_pCurrMVRangetbl->k_y;

        int32_t tmp_dmv_x = 0;
        int32_t tmp_dmv_y = 0;

        VC1_GET_BITS(k_x, tmp_dmv_x);
        VC1_GET_BITS(k_y, tmp_dmv_y);

        dmv_x = (int16_t)tmp_dmv_x;
        dmv_y = (int16_t)tmp_dmv_y;

        predictor_flag = (uint8_t)(dmv_y & 1);
        dmv_y = (dmv_y + 1) >> 1; // differ from standard dmv_y = (dmv_y + predictor_flag) >> 1;
    }

    if (picHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picHeader->MVMODE==VC1_MVMODE_HPEL_1MV)
    {
        dmv_x <<=1;
        dmv_y <<=1;
    }

    * pdmv_x = dmv_x;
    * pdmv_y = dmv_y;

    return predictor_flag;
}
void DeriveSecondStageChromaMV_Interlace(VC1Context* pContext, int16_t* xMV, int16_t* yMV)
{
    VC1MB *pMB = pContext->m_pCurrMB;
    int32_t IX, IY;
    static const uint8_t RndTbl[4] = {0, 0, 0, 1};
    static const uint8_t RndTblField[16]   = {0, 0, 1, 2, 4, 4, 5, 6, 2, 2, 3, 8, 6, 6, 7, 12};

    if(((*xMV) == VC1_MVINTRA) || ((*yMV) == VC1_MVINTRA))
    {
        return;
    }
    else
    {
        int32_t CMV_X, CMV_Y;

        IX = *xMV;
        IY = *yMV;

        CMV_X = (IX + RndTbl[IX & 3]) >> 1;

        if ( (VC1_GET_MBTYPE(pContext->m_pCurrMB->mbType) == VC1_MB_2MV_INTER)
            || (VC1_GET_MBTYPE(pContext->m_pCurrMB->mbType) == VC1_MB_4MV_FIELD_INTER) )
            CMV_Y = (IY >> 4)*8 + RndTblField [IY & 0xF];
        else
            CMV_Y = (IY + RndTbl[IY & 3]) >> 1;

        *xMV              = (int16_t)CMV_X;
        *yMV              = (int16_t)CMV_Y;

        pMB->m_pBlocks[4].mv[0][0] = *xMV;
        pMB->m_pBlocks[4].mv[0][1] = *yMV;
        pMB->m_pBlocks[5].mv[0][0] = *xMV;
        pMB->m_pBlocks[5].mv[0][1] = *yMV;
    }
}

void Decode_InterlaceFrame_BMVTYPE(VC1Context* pContext)
{
    int32_t value=0;
    VC1_GET_BITS(1, value);
    if (value)
    {
        VC1_GET_BITS(1, value);
        if (value)
        {
            pContext->m_pCurrMB->mbType=(uint8_t)(pContext->m_pCurrMB->mbType|VC1_MB_INTERP);
        }
        else
        {
            pContext->m_pCurrMB->mbType=(pContext->m_picLayerHeader->BFRACTION)?
                (uint8_t)(pContext->m_pCurrMB->mbType|VC1_MB_FORWARD)
                :(uint8_t)(pContext->m_pCurrMB->mbType|VC1_MB_BACKWARD);
        }
    }
    else
    {
        pContext->m_pCurrMB->mbType=(pContext->m_picLayerHeader->BFRACTION)?
            (uint8_t)(pContext->m_pCurrMB->mbType|VC1_MB_BACKWARD)
            :(uint8_t)(pContext->m_pCurrMB->mbType|VC1_MB_FORWARD);
    }
}

void Decode_InterlaceField_BMVTYPE(VC1Context* pContext)
{
    int32_t value=0;

    VC1_GET_BITS(1, value);
    if (value)
    {
        VC1_GET_BITS(1, value);
        if (value)
        {
            //11
            pContext->m_pCurrMB->mbType = (uint8_t)(pContext->m_pCurrMB->mbType|VC1_MB_INTERP);
        }
        else
        {
            //10
            pContext->m_pCurrMB->mbType = (uint8_t)(pContext->m_pCurrMB->mbType|VC1_MB_DIRECT);
        }
    }
    else
    {
        //0
        pContext->m_pCurrMB->mbType = (uint8_t)(pContext->m_pCurrMB->mbType|VC1_MB_BACKWARD);
    }
}

void ScaleOppositePredPPic(VC1PictureLayerHeader* picLayerHeader,int16_t *x, int16_t *y)
{
    int16_t scaleX = *x;
    int16_t scaleY = *y;

    const VC1PredictScaleValuesPPic* ScaleValuesTable = picLayerHeader->m_pCurrPredScaleValuePPictbl;
    uint16_t scaleopp = ScaleValuesTable->scaleopp;

    if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE == VC1_MVMODE_HPEL_1MV)
    {
        scaleX  = scaleX >> 1;
        scaleY  = scaleY >> 1;

        scaleX = ((scaleX*scaleopp)>>8);
        scaleY = ((scaleY*scaleopp)>>8);

        scaleX  = scaleX << 1;
        scaleY  = scaleY << 1;
    }
    else
    {
        scaleX = ((scaleX*scaleopp)>>8);
        scaleY = ((scaleY*scaleopp)>>8);
    }

    (*x) = scaleX;
    (*y) = scaleY;
}

void ScaleSamePredPPic(VC1PictureLayerHeader* picLayerHeader, int16_t *x, int16_t *y, int32_t dominant, int32_t fieldFlag)
{
    int16_t scaleX = *x;
    int16_t scaleY = *y;

    const VC1PredictScaleValuesPPic* ScaleValuesTable = picLayerHeader->m_pCurrPredScaleValuePPictbl;

    uint16_t range_x = picLayerHeader->m_pCurrMVRangetbl->r_x;
    uint16_t range_y = picLayerHeader->m_pCurrMVRangetbl->r_y;

    range_y  = range_y >> 1;

    if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE == VC1_MVMODE_HPEL_1MV)
    {
        scaleX  = scaleX >> 1;
        scaleY  = scaleY >> 1;
    }

   // X
    if(vc1_abs_16s(scaleX) <= 255)
    {
        if(vc1_abs_16s(scaleX) < ScaleValuesTable->scalezone1_x)
           scaleX = ((scaleX*ScaleValuesTable->scalesame1)>>8);
        else
        {
            if(scaleX < 0)
                scaleX = ((scaleX * ScaleValuesTable->scalesame2)>>8) - (ScaleValuesTable->zone1offset_x);
            else
                scaleX = ( (scaleX * ScaleValuesTable->scalesame2)>>8 ) + (ScaleValuesTable->zone1offset_x);
        }
    }
    if(scaleX > range_x - 1)
        scaleX = range_x - 1;

    if(scaleX < - range_x)
      scaleX = -range_x;

    // Y
    if(vc1_abs_16s(scaleY) <= 63)
    {
        if(vc1_abs_16s(scaleY) < ScaleValuesTable->scalezone1_y)
            scaleY = ((scaleY*ScaleValuesTable->scalesame1)>>8);
        else
        {
            if(scaleY<0)
                scaleY = ((scaleY*ScaleValuesTable->scalesame2)>>8) - ScaleValuesTable->zone1offset_y;
            else
                scaleY = ((scaleY*ScaleValuesTable->scalesame2)>>8) + ScaleValuesTable->zone1offset_y;
        }
    }

    if(dominant && fieldFlag)
    {
        if(scaleY > (range_y))
        {
            scaleY = (range_y);
        }
        else if(scaleY < -(range_y) + 1)
        {
            scaleY = -(range_y) + 1;
        }
    }
    else
    {
        if(scaleY > range_y-1)
        {
            scaleY = range_y-1;
        }
        else if(scaleY < -(range_y))
        {
            scaleY = -(range_y);
        }
    }

    if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE==VC1_MVMODE_HPEL_1MV)
    {
        scaleX  = scaleX << 1;
        scaleY  = scaleY << 1;
    }

    (*x) = scaleX;
    (*y) = scaleY;
}


void ScaleOppositePredBPic(VC1PictureLayerHeader* picLayerHeader, int16_t *x, int16_t *y,
                           int32_t dominant, int32_t fieldFlag, int32_t back)
{
    int16_t scaleX = *x;
    int16_t scaleY = *y;

    if(back && (picLayerHeader->CurrField == 0))
    {
        const VC1PredictScaleValuesBPic* ScaleValuesTable = picLayerHeader->m_pCurrPredScaleValueB_BPictbl;

        int16_t range_x = picLayerHeader->m_pCurrMVRangetbl->r_x;
        int16_t range_y = picLayerHeader->m_pCurrMVRangetbl->r_y;

        range_y  = range_y >> 1;

        if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE == VC1_MVMODE_HPEL_1MV)
        {
            scaleX  = scaleX >> 1;
            scaleY  = scaleY >> 1;
        }

        // X
        if(vc1_abs_16s(scaleX) <= 255)
        {
            if(vc1_abs_16s(scaleX) < ScaleValuesTable->scalezone1_x)
                scaleX = (scaleX*ScaleValuesTable->scaleopp1)>>8;
            else
            {
                if(scaleX < 0)
                    scaleX = ((scaleX * ScaleValuesTable->scaleopp2)>>8) - (ScaleValuesTable->zone1offset_x);
                else
                    scaleX = ( (scaleX * ScaleValuesTable->scaleopp2)>>8 ) + (ScaleValuesTable->zone1offset_x);
            }
        }

        if(scaleX > range_x - 1)
            scaleX = range_x - 1;

        if(scaleX < - range_x)
            scaleX = -range_x;

        // Y
        if(vc1_abs_16s(scaleY) <= 63)
        {
            if(vc1_abs_16s(scaleY) < ScaleValuesTable->scalezone1_y)
                scaleY = ((scaleY*ScaleValuesTable->scaleopp1)>>8);
            else
            {
                if(scaleY<0)
                    scaleY = ((scaleY*ScaleValuesTable->scaleopp2)>>8) - ScaleValuesTable->zone1offset_y;
                else
                    scaleY = ((scaleY*ScaleValuesTable->scaleopp2)>>8) + ScaleValuesTable->zone1offset_y;
            }
        }

        if(dominant && fieldFlag)
        {
            if(scaleY > range_y - 1)
            {
                scaleY = range_y - 1;
            }
            else if(scaleY < -(range_y))
            {
                scaleY = -(range_y);
            }
        }
        else
        {
            if(scaleY > (range_y))
            {
                scaleY = (range_y);
            }
            else if(scaleY < -(range_y) + 1)
            {
                scaleY = -(range_y) + 1;
            }
        }

        if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE==VC1_MVMODE_HPEL_1MV)
        {
            scaleX  = scaleX << 1;
            scaleY  = scaleY << 1;
        }
    }
    else
    {
        //forward
        const VC1PredictScaleValuesPPic* ScaleValuesTable = picLayerHeader->m_pCurrPredScaleValueP_BPictbl[back];
        int16_t scaleopp = ScaleValuesTable->scaleopp;

        if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE == VC1_MVMODE_HPEL_1MV)
        {
            scaleX  = scaleX >> 1;
            scaleY  = scaleY >> 1;

            scaleX = ((scaleX*scaleopp)>>8);
            scaleY = ((scaleY*scaleopp)>>8);

            scaleX  = scaleX << 1;
            scaleY  = scaleY << 1;
        }
        else
        {
            scaleX = ((scaleX*scaleopp)>>8);
            scaleY = ((scaleY*scaleopp)>>8);
        }
    }
    (*x) = scaleX;
    (*y) = scaleY;
}

void ScaleSamePredBPic(VC1PictureLayerHeader* picLayerHeader,int16_t *x, int16_t *y,
                       int32_t dominant, int32_t fieldFlag, int32_t back)
{
    int16_t scaleX = *x;
    int16_t scaleY = *y;

    if(back && (picLayerHeader->CurrField == 0))
    {
        const VC1PredictScaleValuesBPic* ScaleValuesTable = picLayerHeader->m_pCurrPredScaleValueB_BPictbl;
        int16_t scalesame = ScaleValuesTable->scalesame;

        if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE == VC1_MVMODE_HPEL_1MV)
        {
            scaleX  = scaleX >> 1;
            scaleY  = scaleY >> 1;

            scaleX = (int16_t)((scaleX*scalesame)>>8);
            scaleY = (int16_t)((scaleY*scalesame)>>8);

            scaleX  = scaleX << 1;
            scaleY  = scaleY << 1;
        }
        else
        {
            scaleX = (int16_t)((scaleX*scalesame)>>8);
            scaleY = (int16_t)((scaleY*scalesame)>>8);
        }

    }
    else
    {
        const VC1PredictScaleValuesPPic* ScaleValuesTable = picLayerHeader->m_pCurrPredScaleValueP_BPictbl[back];

        int16_t range_x = picLayerHeader->m_pCurrMVRangetbl->r_x;
        int16_t range_y = picLayerHeader->m_pCurrMVRangetbl->r_y;


        range_y  = range_y >> 1;

        if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE == VC1_MVMODE_HPEL_1MV)
        {
            scaleX  = scaleX >> 1;
            scaleY  = scaleY >> 1;
        }

        // X
        if(vc1_abs_16s(scaleX) <= 255)
        {
            if(vc1_abs_16s(scaleX) < ScaleValuesTable->scalezone1_x)
                scaleX = (scaleX*ScaleValuesTable->scalesame1)>>8;
            else
            {
                if(scaleX < 0)
                    scaleX = ((scaleX * ScaleValuesTable->scalesame2)>>8) - (ScaleValuesTable->zone1offset_x);
                else
                    scaleX = ( (scaleX * ScaleValuesTable->scalesame2)>>8 ) + (ScaleValuesTable->zone1offset_x);
            }
        }
        if(scaleX > range_x - 1)
            scaleX = range_x - 1;

        if(scaleX < - range_x)
            scaleX = -range_x;

        // Y
        if(vc1_abs_16s(scaleY) <= 63)
        {
            if(abs(scaleY) < ScaleValuesTable->scalezone1_y)
                scaleY = (scaleY*ScaleValuesTable->scalesame1)>>8;
            else
            {
                if(scaleY<0)
                    scaleY = ((scaleY*ScaleValuesTable->scalesame2)>>8) - ScaleValuesTable->zone1offset_y;
                else
                    scaleY = ((scaleY*ScaleValuesTable->scalesame2)>>8) + ScaleValuesTable->zone1offset_y;
            }
        }

        if(dominant && fieldFlag)
        {
            if(scaleY > (range_y))
            {
                scaleY = (range_y);
            }
            else if(scaleY < -(range_y) + 1)
            {
                scaleY = -(range_y) + 1;
            }
        }
        else
        {
            if(scaleY > range_y - 1)
            {
                scaleY = range_y - 1;
            }
            else if(scaleY < -(range_y))
            {
                scaleY = -(range_y);
            }
        }

        if (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV || picLayerHeader->MVMODE==VC1_MVMODE_HPEL_1MV)
        {
            scaleX  = scaleX << 1;
            scaleY  = scaleY << 1;
        }
    }

    (*x) = scaleX;
    (*y) = scaleY;
}

void HybridFieldMV(VC1Context* pContext,int16_t *pPredMVx,int16_t *pPredMVy,
                   int16_t MV_px[3],int16_t MV_py[3])
{
    uint32_t hybridmv_thresh;
    uint16_t sumA;
    uint16_t sumC;
    uint32_t HybridPred;

    hybridmv_thresh = 32;

    sumA = vc1_abs_16s(*pPredMVx - MV_px[0]) +vc1_abs_16s(*pPredMVy - MV_py[0]);

    if(sumA > hybridmv_thresh)
    {
        VC1_GET_BITS(1,HybridPred);

        *pPredMVx = MV_px[2 - HybridPred*2];
        *pPredMVy = MV_py[2 - HybridPred*2];
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
            VM_STRING("hybrid\n"));
#endif
    }
    else
    {
        sumC = vc1_abs_16s(*pPredMVx - MV_px[2]) +vc1_abs_16s(*pPredMVy - MV_py[2]);

        if(sumC > hybridmv_thresh)
        {
            VC1_GET_BITS(1,HybridPred);

            *pPredMVx = MV_px[2 - HybridPred*2];
            *pPredMVy = MV_py[2 - HybridPred*2];
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                VM_STRING("hybrid\n"));
#endif
        }
#ifdef VC1_DEBUG_ON
        else
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
            VM_STRING("no hybrid\n"));
#endif
    }
}

void Field1MVPrediction(VC1Context* pContext)
{
    VC1MVPredictors MVPred;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1MB *pA = NULL, *pB = NULL, *pC = NULL;

    uint32_t LeftTopRight = pCurrMB->LeftTopRightPositionFlag;
    int32_t width = pContext->m_pSingleMB->MaxWidthMB;

    memset(&MVPred,0,sizeof(VC1MVPredictors));

    if(LeftTopRight == VC1_COMMON_MB)
    {
        //all predictors are available
        pA = pCurrMB - width;
        pB = pCurrMB - width + 1;
        pC = pCurrMB - 1;

        if(!pA->IntraFlag)
            MVPred.AMVPred[0] = &pA->m_pBlocks[2];

        if(!pB->IntraFlag)
            MVPred.BMVPred[0] = &pB->m_pBlocks[2];

        if(!pC->IntraFlag)
            MVPred.CMVPred[0] = &pC->m_pBlocks[1];
    }
    else if(VC1_IS_TOP_MB(LeftTopRight))
    {
        //A and B predictors are unavailable
        pC = pCurrMB - 1;

        if(!pC->IntraFlag)
            MVPred.CMVPred[0] = &pC->m_pBlocks[1];
    }
    else if(VC1_IS_LEFT_MB(LeftTopRight))
    {
        //C predictor is unavailable
        pA = pCurrMB - width;
        pB = pCurrMB - width + 1;

        if(!pA->IntraFlag)
            MVPred.AMVPred[0] = &pA->m_pBlocks[2];
        if(!pB->IntraFlag)
            MVPred.BMVPred[0] = &pB->m_pBlocks[2];
    }
    else if(VC1_IS_RIGHT_MB(LeftTopRight))
    {
        //all predictors are available
        pA = pCurrMB - width;
        pB = pCurrMB - width - 1;
        pC = pCurrMB - 1;

        if(!pA->IntraFlag)
            MVPred.AMVPred[0] = &pA->m_pBlocks[2];

        if(!pB->IntraFlag)
            MVPred.BMVPred[0] = &pB->m_pBlocks[2];

        if(!pC->IntraFlag)
            MVPred.CMVPred[0] = &pC->m_pBlocks[1];
    }
    else if(VC1_IS_TOP_RIGHT_MB(LeftTopRight))
    {
        pC = pCurrMB - 1;

        if(!pC->IntraFlag)
            MVPred.CMVPred[0] = &pC->m_pBlocks[1];
    }
    pContext->MVPred = MVPred;
}


void Field4MVPrediction(VC1Context* pContext)
{
    VC1MVPredictors MVPred;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1MB *pA = NULL, *pB0 = NULL,*pB1 = NULL, *pC = NULL;

    uint32_t LeftTopRight = pCurrMB->LeftTopRightPositionFlag;
    int32_t width = pContext->m_pSingleMB->MaxWidthMB;

    memset(&MVPred,0,sizeof(VC1MVPredictors));

    if(LeftTopRight == VC1_COMMON_MB)
    {
        //all predictors are available
        pA  = pCurrMB - width;
        pB0 = pCurrMB - width - 1;
        pB1 = pCurrMB - width + 1;
        pC  = pCurrMB - 1;

        if(!pA->IntraFlag)
        {
            MVPred.AMVPred[0] = &pA->m_pBlocks[2];
            MVPred.AMVPred[1] = &pA->m_pBlocks[3];
        }
        if(!pC->IntraFlag)
        {
            MVPred.CMVPred[0] = &pC->m_pBlocks[1];
            MVPred.CMVPred[2] = &pC->m_pBlocks[3];
        }

        if(!pB0->IntraFlag)
            MVPred.BMVPred[0] = &pB0->m_pBlocks[3];

        if(!pB1->IntraFlag)
            MVPred.BMVPred[1] = &pB1->m_pBlocks[2];
    }
    else if(VC1_IS_TOP_MB(LeftTopRight))
    {
        pC = pCurrMB - 1;

        if(!pC->IntraFlag)
        {
            MVPred.CMVPred[0] = &pC->m_pBlocks[1];
            MVPred.CMVPred[2] = &pC->m_pBlocks[3];
        }
    }
    else if(VC1_IS_LEFT_MB(LeftTopRight))
    {
        pA = pCurrMB - width;
        pB1 = pCurrMB - width + 1;

        if(!pA->IntraFlag)
        {
            MVPred.AMVPred[0] = &pA->m_pBlocks[2];
            MVPred.BMVPred[0] = &pA->m_pBlocks[3];
            MVPred.AMVPred[1] = &pA->m_pBlocks[3];
        }
        if(!pB1->IntraFlag)
            MVPred.BMVPred[1] = &pB1->m_pBlocks[2];
    }
    else if (VC1_IS_RIGHT_MB(LeftTopRight))
    {
        pA = pCurrMB - width;
        pB0 = pCurrMB - width - 1;
        pC = pCurrMB - 1;

        if(!pA->IntraFlag)
        {
            MVPred.AMVPred[0] = &pA->m_pBlocks[2];
            MVPred.AMVPred[1] = &pA->m_pBlocks[3];
            MVPred.BMVPred[1] = &pA->m_pBlocks[2];
        }

        if(!pC->IntraFlag)
        {
            MVPred.CMVPred[0] = &pC->m_pBlocks[1];
            MVPred.CMVPred[2] = &pC->m_pBlocks[3];
        }

        if(!pB0->IntraFlag)
        {
            MVPred.BMVPred[0] = &pB0->m_pBlocks[3];
        }
    }
    else
    {
        if(VC1_IS_TOP_RIGHT_MB(LeftTopRight))
        {
            pC = pCurrMB - 1;

            if(!pC->IntraFlag)
            {
                MVPred.CMVPred[0] = &pC->m_pBlocks[1];
                MVPred.CMVPred[2] = &pC->m_pBlocks[3];
            }
        }
        else if (VC1_IS_LEFT_RIGHT_MB(LeftTopRight))
        {
            pA = pCurrMB - width;

            if(!pA->IntraFlag)
            {
                MVPred.AMVPred[0] = &pA->m_pBlocks[2];
                MVPred.BMVPred[0] = &pA->m_pBlocks[3];
                MVPred.BMVPred[1] = &pA->m_pBlocks[2];
                MVPred.AMVPred[1] = &pA->m_pBlocks[3];
            }
        }
    }

    MVPred.CMVPred[1] = &pCurrMB->m_pBlocks[0];
    MVPred.AMVPred[2] = &pCurrMB->m_pBlocks[0];
    MVPred.BMVPred[3] = &pCurrMB->m_pBlocks[0];

    MVPred.BMVPred[2] = &pCurrMB->m_pBlocks[1];
    MVPred.AMVPred[3] = &pCurrMB->m_pBlocks[1];

    MVPred.CMVPred[3] = &pCurrMB->m_pBlocks[2];

    pContext->MVPred = MVPred;
}

void CalculateField1MVOneReferencePPic(VC1Context* pContext,
                                       int16_t *pPredMVx,int16_t *pPredMVy)
{
    VC1MVPredictors* MVPred = &pContext->MVPred;
    uint32_t validPredictors = 0;
    uint32_t hybryd = 0;

    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    if(MVPred->AMVPred[0])
    {
        MV_px[0] = MVPred->AMVPred[0]->mv[0][0];
        MV_py[0] = MVPred->AMVPred[0]->mv[0][1];
        validPredictors++;
        hybryd++;
    }

    if(MVPred->BMVPred[0])
    {
        MV_px[1] = MVPred->BMVPred[0]->mv[0][0];
        MV_py[1] = MVPred->BMVPred[0]->mv[0][1];
        validPredictors++;
    }


    if(MVPred->CMVPred[0])
    {
        MV_px[2] = MVPred->CMVPred[0]->mv[0][0];
        MV_py[2] = MVPred->CMVPred[0]->mv[0][1];
        validPredictors++;
        hybryd++;
    }

    //Calaculte predictors
    if(validPredictors > 1)
    {
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else
    {
        *pPredMVx = MV_px[0] + MV_px[1] + MV_px[2];
        *pPredMVy = MV_py[0] + MV_py[1] + MV_py[2];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("CurrFlag = %d\n"), pContext->m_pCurrMB->fieldFlag[0]);

    if(MVPred->AMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("pA->fieldFlag = %d\n"), MVPred->AMVPred[0]->fieldFlag[0]);

    if(MVPred->BMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("pB->fieldFlag = %d\n"), MVPred->BMVPred[0]->fieldFlag[0]);

    if(MVPred->CMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("pC->fieldFlag = %d\n"), MVPred->CMVPred[0]->fieldFlag[0]);
#endif

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[0] = %d MV_py[0] = %d\n"), MV_px[0], MV_py[0]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[1] = %d MV_py[1] = %d\n"), MV_px[1], MV_py[1]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[2] = %d MV_py[2] = %d\n"), MV_px[2], MV_py[2]);
#endif
    if(hybryd == 2)
        HybridFieldMV(pContext,pPredMVx,pPredMVy,MV_px,MV_py);
#ifdef VC1_DEBUG_ON
    else
       VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
       VM_STRING("no hybrid\n"));
#endif

}

void CalculateField1MVTwoReferencePPic(VC1Context* pContext,
                                       int16_t *pPredMVx,
                                       int16_t *pPredMVy,
                                       uint8_t* PredFlag)
{
    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    VC1MVPredictors* MVPred = &pContext->MVPred;

    uint32_t validPredictorsOppositeField = 0;
    uint32_t validPredictorsSameField = 0;

    uint32_t flag_A, flag_B, flag_C, curr_flag;
    uint32_t hybryd = 0;

    flag_A = flag_B = flag_C = curr_flag = pContext->m_pCurrMB->fieldFlag[0];

    if(MVPred->AMVPred[0])
    {
        MV_px[0] = MVPred->AMVPred[0]->mv[0][0];
        MV_py[0] = MVPred->AMVPred[0]->mv[0][1];
        flag_A = MVPred->AMVPred[0]->fieldFlag[0];
        validPredictorsOppositeField = validPredictorsOppositeField +
           (flag_A^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
             1 - (flag_A^curr_flag);

        hybryd++;
    }

    if(MVPred->BMVPred[0])
    {
        MV_px[1] = MVPred->BMVPred[0]->mv[0][0];
        MV_py[1] = MVPred->BMVPred[0]->mv[0][1];
        flag_B = MVPred->BMVPred[0]->fieldFlag[0];
        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_B^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_B^curr_flag);
    }


    if(MVPred->CMVPred[0])
    {
        MV_px[2] = MVPred->CMVPred[0]->mv[0][0];
        MV_py[2] = MVPred->CMVPred[0]->mv[0][1];
        flag_C = MVPred->CMVPred[0]->fieldFlag[0];
        hybryd++;
        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_C^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_C^curr_flag);
    }

    //Calaculte predictors
    if(validPredictorsSameField <= validPredictorsOppositeField)
    {
        *PredFlag = 1 - *PredFlag;      //opposite
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                            VM_STRING("opposite\n"));
#endif
    }

#ifdef VC1_DEBUG_ON
    else
    {
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                            VM_STRING("same\n"));
    }
#endif

    if(*PredFlag == 0)
    {
        //same
        if(flag_A != curr_flag)
            ScaleSamePredPPic(pContext->m_picLayerHeader, &MV_px[0], &MV_py[0],*PredFlag,curr_flag);

        if(flag_B != curr_flag)
            ScaleSamePredPPic(pContext->m_picLayerHeader, &MV_px[1], &MV_py[1],*PredFlag,curr_flag);

        if(flag_C != curr_flag)
            ScaleSamePredPPic(pContext->m_picLayerHeader, &MV_px[2], &MV_py[2],*PredFlag,curr_flag);
    }
    else
    {
        pContext->m_pCurrMB->fieldFlag[0] = 1 - pContext->m_pCurrMB->fieldFlag[0];
        curr_flag = pContext->m_pCurrMB->fieldFlag[0];

        if(flag_A != curr_flag)
            ScaleOppositePredPPic(pContext->m_picLayerHeader,&MV_px[0], &MV_py[0]);

        if(flag_B != curr_flag)
            ScaleOppositePredPPic(pContext->m_picLayerHeader,&MV_px[1], &MV_py[1]);

        if(flag_C != curr_flag)
            ScaleOppositePredPPic(pContext->m_picLayerHeader,&MV_px[2], &MV_py[2]);
    }


    if(validPredictorsSameField + validPredictorsOppositeField > 1)
    {
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else
    {
        *pPredMVx = MV_px[0] + MV_px[1] + MV_px[2];
        *pPredMVy = MV_py[0] + MV_py[1] + MV_py[2];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                    VM_STRING("PredFlag = %d\n"), *PredFlag);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                    VM_STRING("CurrFlag = %d\n"),
                                    pContext->m_pCurrMB->fieldFlag[0]);


    if(MVPred->AMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("pA->fieldFlag = %d\n"),
                                            MVPred->AMVPred[0]->fieldFlag[0]);

    if(MVPred->BMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                VM_STRING("pB->fieldFlag = %d\n"),
                                                MVPred->BMVPred[0]->fieldFlag[0]);

    if(MVPred->CMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                VM_STRING("pC->fieldFlag = %d\n"),
                                                MVPred->CMVPred[0]->fieldFlag[0]);

    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[0] = %d MV_py[0] = %d\n"),
                                            MV_px[0], MV_py[0]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[1] = %d MV_py[1] = %d\n"),
                                            MV_px[1], MV_py[1]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[2] = %d MV_py[2] = %d\n"),
                                            MV_px[2], MV_py[2]);
#endif

    if(hybryd == 2)
        HybridFieldMV(pContext,pPredMVx,pPredMVy,MV_px,MV_py);
#ifdef VC1_DEBUG_ON
    else
       VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                VM_STRING("no hybrid\n"));
#endif

}

void CalculateField4MVOneReferencePPic(VC1Context* pContext, int16_t *pPredMVx,int16_t *pPredMVy, int32_t blk_num)
{
    uint32_t validPredictors = 0;
    VC1MVPredictors* MVPred = &pContext->MVPred;
    uint32_t hybryd = 0;

    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    if(MVPred->AMVPred[blk_num])
    {
        MV_px[0] = MVPred->AMVPred[blk_num]->mv[0][0];
        MV_py[0] = MVPred->AMVPred[blk_num]->mv[0][1];
        validPredictors++;
        hybryd++;
    }

    if(MVPred->BMVPred[blk_num])
    {
        MV_px[1] = MVPred->BMVPred[blk_num]->mv[0][0];
        MV_py[1] = MVPred->BMVPred[blk_num]->mv[0][1];
        validPredictors++;
    }

    if(MVPred->CMVPred[blk_num])
    {
        MV_px[2] = MVPred->CMVPred[blk_num]->mv[0][0];
        MV_py[2] = MVPred->CMVPred[blk_num]->mv[0][1];
        validPredictors++;
        hybryd++;
    }

    //Calaculte predictors
    if(validPredictors>=2)
    {
       *pPredMVx = (int16_t)median3(MV_px);
       *pPredMVy = (int16_t)median3(MV_py);
    }
    else
    {
        *pPredMVx = MV_px[0] + MV_px[1] + MV_px[2];
        *pPredMVy = MV_py[0] + MV_py[1] + MV_py[2];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("CurrFlag = %d\n"), pContext->m_pCurrMB->fieldFlag[0]);

    if(MVPred->AMVPred[blk_num])
      VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
      VM_STRING("pA->fieldFlag = %d\n"), MVPred->AMVPred[blk_num]->fieldFlag[0]);

    if(MVPred->BMVPred[blk_num])
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
     VM_STRING("pB->fieldFlag = %d\n"), MVPred->BMVPred[blk_num]->fieldFlag[0]);

    if(MVPred->CMVPred[blk_num])
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
     VM_STRING("pC->fieldFlag = %d\n"), MVPred->CMVPred[blk_num]->fieldFlag[0]);
#endif

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[0] = %d MV_py[0] = %d\n"), MV_px[0], MV_py[0]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[1] = %d MV_py[1] = %d\n"), MV_px[1], MV_py[1]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[2] = %d MV_py[2] = %d\n"), MV_px[2], MV_py[2]);
#endif
    if(hybryd == 2)
        HybridFieldMV(pContext,pPredMVx,pPredMVy,MV_px,MV_py);
#ifdef VC1_DEBUG_ON
    else
       VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                    VM_STRING("no hybrid\n"));
#endif
}

void CalculateField4MVTwoReferencePPic(VC1Context* pContext,
                                              int16_t *pPredMVx,int16_t *pPredMVy,
                                              int32_t blk_num,  uint8_t* PredFlag)
{
    VC1MVPredictors* MVPred = &pContext->MVPred;
    uint32_t hybryd = 0;

    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    uint32_t validPredictorsOppositeField = 0;
    uint32_t validPredictorsSameField = 0;


    uint32_t flag_A, flag_B, flag_C, curr_flag;
    flag_A = flag_B = flag_C = curr_flag = pContext->m_pCurrMB->fieldFlag[0];

    if(MVPred->AMVPred[blk_num])
    {
        MV_px[0] = MVPred->AMVPred[blk_num]->mv[0][0];
        MV_py[0] = MVPred->AMVPred[blk_num]->mv[0][1];

        flag_A = MVPred->AMVPred[blk_num]->fieldFlag[0];

        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_A^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_A^curr_flag);
        hybryd++;
    }

    if(MVPred->BMVPred[blk_num])
    {
        MV_px[1] = MVPred->BMVPred[blk_num]->mv[0][0];
        MV_py[1] = MVPred->BMVPred[blk_num]->mv[0][1];

        flag_B = MVPred->BMVPred[blk_num]->fieldFlag[0];

        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_B^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_B^curr_flag);
    }

    if(MVPred->CMVPred[blk_num])
    {
        MV_px[2] = MVPred->CMVPred[blk_num]->mv[0][0];
        MV_py[2] = MVPred->CMVPred[blk_num]->mv[0][1];

        flag_C = MVPred->CMVPred[blk_num]->fieldFlag[0];

        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_C^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_C^curr_flag);
        hybryd++;
    }


    //Calculate
    if(validPredictorsSameField <= validPredictorsOppositeField)
    {
        //opposite
        *PredFlag = 1 - *PredFlag;
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                VM_STRING("opposite\n"));
#endif
    }
    else
    {
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                VM_STRING("same\n"));
#endif
    }

    if(*PredFlag == 0)
    {
        //same
        if(flag_A != curr_flag)
            ScaleSamePredPPic(pContext->m_picLayerHeader, &MV_px[0], &MV_py[0],*PredFlag,curr_flag);

        if(flag_B!=curr_flag)
            ScaleSamePredPPic(pContext->m_picLayerHeader, &MV_px[1], &MV_py[1],*PredFlag,curr_flag);

        if(flag_C != curr_flag)
            ScaleSamePredPPic(pContext->m_picLayerHeader, &MV_px[2], &MV_py[2],*PredFlag,curr_flag);
    }
    else
    {
        //opposite
        pContext->m_pCurrMB->m_pBlocks[blk_num].fieldFlag[0] = 1 - pContext->m_pCurrMB->m_pBlocks[blk_num].fieldFlag[0];
        curr_flag = pContext->m_pCurrMB->m_pBlocks[blk_num].fieldFlag[0];

        if(flag_A != curr_flag)
             ScaleOppositePredPPic(pContext->m_picLayerHeader,&MV_px[0], &MV_py[0]);

        if(flag_B != curr_flag)
             ScaleOppositePredPPic(pContext->m_picLayerHeader,&MV_px[1], &MV_py[1]);

        if(flag_C != curr_flag)
            ScaleOppositePredPPic(pContext->m_picLayerHeader,&MV_px[2], &MV_py[2]);
    }


    if(validPredictorsSameField + validPredictorsOppositeField >1)
    {
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else
    {
        *pPredMVx = MV_px[0] + MV_px[1] + MV_px[2];
        *pPredMVy = MV_py[0] + MV_py[1] + MV_py[2];
     }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                    VM_STRING("PredFlag = %d\n"), *PredFlag);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                    VM_STRING("CurrFlag = %d\n"),
                                    pContext->m_pCurrMB->m_pBlocks[blk_num].fieldFlag[0]);

    if(MVPred->AMVPred[blk_num])
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("pA->fieldFlag = %d\n"),
                                            MVPred->AMVPred[blk_num]->fieldFlag[0]);

    if(MVPred->BMVPred[blk_num])
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("pB->fieldFlag = %d\n"),
                                            MVPred->BMVPred[blk_num]->fieldFlag[0]);

    if(MVPred->CMVPred[blk_num])
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("pC->fieldFlag = %d\n"),
                                            MVPred->CMVPred[blk_num]->fieldFlag[0]);
#endif

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[0] = %d MV_py[0] = %d\n"),
        MV_px[0], MV_py[0]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[1] = %d MV_py[1] = %d\n"),
        MV_px[1], MV_py[1]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[2] = %d MV_py[2] = %d\n"),
        MV_px[2], MV_py[2]);
#endif

    if(hybryd == 2)
        HybridFieldMV(pContext,pPredMVx,pPredMVy,MV_px,MV_py);
#ifdef VC1_DEBUG_ON
    else
       VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                    VM_STRING("no hybrid\n"));
#endif

}

void CalculateField1MVTwoReferenceBPic(VC1Context* pContext,
                                       int16_t *pPredMVx,
                                       int16_t *pPredMVy,
                                       int32_t Back,
                                       uint8_t* PredFlag)
{
    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    VC1MVPredictors* MVPred = &pContext->MVPred;

    uint32_t validPredictorsOppositeField = 0;
    uint32_t validPredictorsSameField = 0;

    uint32_t flag_A, flag_B, flag_C, curr_flag;
    uint32_t hybryd = 0;

    flag_A = flag_B = flag_C = curr_flag = pContext->m_pCurrMB->fieldFlag[Back];

    if(MVPred->AMVPred[0])
    {
        MV_px[0] = MVPred->AMVPred[0]->mv[Back][0];
        MV_py[0] = MVPred->AMVPred[0]->mv[Back][1];
        flag_A = MVPred->AMVPred[0]->fieldFlag[Back];
        validPredictorsOppositeField = validPredictorsOppositeField +
           (flag_A^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
             1 - (flag_A^curr_flag);

        hybryd++;
    }

    if(MVPred->BMVPred[0])
    {
        MV_px[1] = MVPred->BMVPred[0]->mv[Back][0];
        MV_py[1] = MVPred->BMVPred[0]->mv[Back][1];
        flag_B = MVPred->BMVPred[0]->fieldFlag[Back];
        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_B^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_B^curr_flag);
    }


    if(MVPred->CMVPred[0])
    {
        MV_px[2] = MVPred->CMVPred[0]->mv[Back][0];
        MV_py[2] = MVPred->CMVPred[0]->mv[Back][1];
        flag_C = MVPred->CMVPred[0]->fieldFlag[Back];
        hybryd++;
        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_C^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_C^curr_flag);
    }

    //Calaculte predictors
    if(validPredictorsSameField <= validPredictorsOppositeField)
    {
        *PredFlag = 1 - *PredFlag;      //opposite
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                        VM_STRING("opposite\n"));
#endif
    }

#ifdef VC1_DEBUG_ON
    else
    {
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                            VM_STRING("same\n"));
    }
#endif
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[0] = %d MV_py[0] = %d\n"),
        MV_px[0], MV_py[0]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[1] = %d MV_py[1] = %d\n"),
        MV_px[1], MV_py[1]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("MV_px[2] = %d MV_py[2] = %d\n"),
        MV_px[2], MV_py[2]);
#endif

    if(*PredFlag == 0)
    {
        //same
        if(flag_A != curr_flag)
            ScaleSamePredBPic(pContext->m_picLayerHeader, &MV_px[0], &MV_py[0],*PredFlag,curr_flag, Back);

        if(flag_B != curr_flag)
            ScaleSamePredBPic(pContext->m_picLayerHeader, &MV_px[1], &MV_py[1],*PredFlag,curr_flag, Back);

        if(flag_C != curr_flag)
            ScaleSamePredBPic(pContext->m_picLayerHeader, &MV_px[2], &MV_py[2],*PredFlag,curr_flag, Back);
    }
    else
    {
        pContext->m_pCurrMB->fieldFlag[Back] = 1 - pContext->m_pCurrMB->fieldFlag[Back];
        curr_flag = pContext->m_pCurrMB->fieldFlag[Back];

        if(flag_A != curr_flag)
            ScaleOppositePredBPic(pContext->m_picLayerHeader,&MV_px[0], &MV_py[0],*PredFlag,curr_flag, Back);

        if(flag_B != curr_flag)
            ScaleOppositePredBPic(pContext->m_picLayerHeader,&MV_px[1], &MV_py[1],*PredFlag,curr_flag, Back);

        if(flag_C != curr_flag)
            ScaleOppositePredBPic(pContext->m_picLayerHeader,&MV_px[2], &MV_py[2],*PredFlag,curr_flag, Back);
    }

    if(validPredictorsSameField + validPredictorsOppositeField > 1)
    {
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else
    {
        *pPredMVx = MV_px[0] + MV_px[1] + MV_px[2];
        *pPredMVy = MV_py[0] + MV_py[1] + MV_py[2];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("PredFlag = %d\n"), *PredFlag);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("CurrFlag = %d\n"), pContext->m_pCurrMB->fieldFlag[Back]);

    if(MVPred->AMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
        VM_STRING("pA->fieldFlag = %d\n"), MVPred->AMVPred[0]->fieldFlag[Back]);

    if(MVPred->BMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("pB->fieldFlag = %d\n"),
                                            MVPred->BMVPred[0]->fieldFlag[Back]);

    if(MVPred->CMVPred[0])
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("pC->fieldFlag = %d\n"),
                                            MVPred->CMVPred[0]->fieldFlag[Back]);

    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[0] = %d MV_py[0] = %d\n"),
                                            MV_px[0], MV_py[0]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[1] = %d MV_py[1] = %d\n"),
                                            MV_px[1], MV_py[1]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[2] = %d MV_py[2] = %d\n"),
                                            MV_px[2], MV_py[2]);

    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("no hybrid\n"));
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("Back = %d\n"), Back);
#endif
}

void CalculateField4MVTwoReferenceBPic(VC1Context* pContext,
                                       int16_t *pPredMVx,int16_t *pPredMVy,
                                       int32_t blk_num,int32_t Back,
                                       uint8_t* PredFlag)
{
    VC1MVPredictors* MVPred = &pContext->MVPred;

    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    uint32_t validPredictorsOppositeField = 0;
    uint32_t validPredictorsSameField = 0;


    uint32_t flag_A, flag_B, flag_C, curr_flag;
    flag_A = flag_B = flag_C = curr_flag = pContext->m_pCurrMB->fieldFlag[Back];

    if(MVPred->AMVPred[blk_num])
    {
        MV_px[0] = MVPred->AMVPred[blk_num]->mv[Back][0];
        MV_py[0] = MVPred->AMVPred[blk_num]->mv[Back][1];

        flag_A = MVPred->AMVPred[blk_num]->fieldFlag[Back];

        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_A^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_A^curr_flag);
    }

    if(MVPred->BMVPred[blk_num])
    {
        MV_px[1] = MVPred->BMVPred[blk_num]->mv[Back][0];
        MV_py[1] = MVPred->BMVPred[blk_num]->mv[Back][1];

        flag_B = MVPred->BMVPred[blk_num]->fieldFlag[Back];

        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_B^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_B^curr_flag);
    }

    if(MVPred->CMVPred[blk_num])
    {
        MV_px[2] = MVPred->CMVPred[blk_num]->mv[Back][0];
        MV_py[2] = MVPred->CMVPred[blk_num]->mv[Back][1];

        flag_C = MVPred->CMVPred[blk_num]->fieldFlag[Back];

        validPredictorsOppositeField = validPredictorsOppositeField +
            (flag_C^curr_flag);

        validPredictorsSameField = validPredictorsSameField +
            1 - (flag_C^curr_flag);
    }


    //Calculate
    if(validPredictorsSameField <= validPredictorsOppositeField)
    {
        //opposite
        *PredFlag = 1 - *PredFlag;
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                        VM_STRING("opposite\n"));
#endif
    }
    else
    {
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                            VM_STRING("same\n"));
#endif
    }

    if(*PredFlag == 0)
    {
        //same
        if(flag_A != curr_flag)
            ScaleSamePredBPic(pContext->m_picLayerHeader, &MV_px[0], &MV_py[0],*PredFlag,curr_flag, Back);

        if(flag_B!=curr_flag)
            ScaleSamePredBPic(pContext->m_picLayerHeader, &MV_px[1], &MV_py[1],*PredFlag,curr_flag, Back);

        if(flag_C != curr_flag)
            ScaleSamePredBPic(pContext->m_picLayerHeader, &MV_px[2], &MV_py[2],*PredFlag,curr_flag, Back);
    }
    else
    {
        pContext->m_pCurrMB->m_pBlocks[blk_num].fieldFlag[Back] = 1 - pContext->m_pCurrMB->m_pBlocks[blk_num].fieldFlag[Back];
        curr_flag = pContext->m_pCurrMB->m_pBlocks[blk_num].fieldFlag[Back];

        if(flag_A != curr_flag)
             ScaleOppositePredBPic(pContext->m_picLayerHeader,&MV_px[0], &MV_py[0],*PredFlag,curr_flag, Back);

        if(flag_B != curr_flag)
             ScaleOppositePredBPic(pContext->m_picLayerHeader,&MV_px[1], &MV_py[1],*PredFlag,curr_flag, Back);

        if(flag_C != curr_flag)
            ScaleOppositePredBPic(pContext->m_picLayerHeader,&MV_px[2], &MV_py[2],*PredFlag,curr_flag, Back);
    }


    if(validPredictorsSameField + validPredictorsOppositeField >1)
    {
        *pPredMVx = (int16_t)median3(MV_px);
        *pPredMVy = (int16_t)median3(MV_py);
    }
    else
    {
        *pPredMVx = MV_px[0] + MV_px[1] + MV_px[2];
        *pPredMVy = MV_py[0] + MV_py[1] + MV_py[2];
    }

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                    VM_STRING("PredFlag = %d\n"), *PredFlag);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                    VM_STRING("CurrFlag = %d\n"), curr_flag);

    if(MVPred->AMVPred[blk_num])
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                    VM_STRING("pA->fieldFlag = %d\n"),
                                    MVPred->AMVPred[blk_num]->fieldFlag[Back]);

    if(MVPred->BMVPred[blk_num])
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                    VM_STRING("pB->fieldFlag = %d\n"),
                                    MVPred->BMVPred[blk_num]->fieldFlag[Back]);

    if(MVPred->CMVPred[blk_num])
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("pC->fieldFlag = %d\n"),
                                            MVPred->CMVPred[blk_num]->fieldFlag[Back]);

    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[0] = %d MV_py[0] = %d\n"),
                                            MV_px[0], MV_py[0]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[1] = %d MV_py[1] = %d\n"),
                                            MV_px[1], MV_py[1]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("MV_px[2] = %d MV_py[2] = %d\n"),
                                            MV_px[2], MV_py[2]);

     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("no hybrid\n"));
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                            VM_STRING("Back = %d\n"), Back);
#endif
}


#endif //UMC_ENABLE_VC1_VIDEO_DECODER
