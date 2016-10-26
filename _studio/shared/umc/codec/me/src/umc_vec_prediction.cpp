//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2009 Intel Corporation. All Rights Reserved.
//

#include <umc_defs.h>
#ifdef UMC_ENABLE_ME

#include "umc_vec_prediction.h"
#include "umc_me.h"

Ipp16s VC1ABS(Ipp16s value)
{
  Ipp16u s = value>>15;
  s = (value + s)^s;
  return s;
}

namespace UMC
{
void MePredictCalculator::Init(MeParams* pMeParams, MeCurrentMB* pCur)
{
    m_pMeParams = pMeParams;
    m_pCur = pCur;
    m_pRes = pMeParams->pSrc->MBs;
}

MeMV MePredictCalculator::GetPrediction()
{
    switch(m_pCur->MbPart){
        case ME_Mb16x16:
            return GetPrediction16x16();
        case ME_Mb8x8:
            return GetPrediction8x8();
    }

    return 0;
}

MeMV MePredictCalculator::GetPrediction(MeMV mv)
{
    return mv;
}

bool MePredictCalculator::IsOutOfBound(MeMbPart mt, MePixelType pix, MeMV mv)
{
    if( mv.x<-4*m_pMeParams->SearchRange.x|| mv.x>=4*m_pMeParams->SearchRange.x ||
        mv.y<-4*m_pMeParams->SearchRange.y || mv.y>=4*m_pMeParams->SearchRange.y)
        return true;

    Ipp32s w=4*16, h=4*16;
    switch(mt)
    {
//    case ME_Mb16x16:
//        w = 16;
//        h = 16;
//        break;
    case ME_Mb16x8:
        w = 4*16;
        h = 4*8;
        break;
    case ME_Mb8x16:
        w = 4*8;
        h = 4*16;
        break;
    case ME_Mb8x8:
        w = 4*8;
        h = 4*8;
        break;
    case ME_Mb4x4:
        w = 4*4;
        h = 4*4;
        break;
    case ME_Mb2x2:
        w = 4*2;
        h = 4*2;
        break;
    }

    mv.x+=4*16*m_pCur->x;
    mv.y+=4*16*m_pCur->y;

    if(m_pCur->BlkIdx&1) mv.x+=4*8;
    if(m_pCur->BlkIdx>1) mv.y+=4*8;

    if(mv.x<4*m_pMeParams->PicRange.top_left.x)return true;
    if(mv.y<4*m_pMeParams->PicRange.top_left.y)return true;
    if(mv.x>4*m_pMeParams->PicRange.bottom_right.x-w)return true;
    if(mv.y>4*m_pMeParams->PicRange.bottom_right.y-h)return true;
    bool isField = (m_pMeParams->PredictionType == ME_VC1Field1 || m_pMeParams->PredictionType == ME_VC1Field2
        || m_pMeParams->PredictionType == ME_VC1Field2Hybrid);

    if(isField)
    {
        Ipp32s y_shift = 0;
        Ipp32s level=0;
        if(pix == ME_DoublePixel) level = 1;
        if(pix == ME_QuadPixel) level = 2;
        if(pix == ME_OctalPixel) level = 3;

        MeFrame **fr;
        if(m_pCur->RefDir==frw)fr=m_pMeParams->pRefF;
        else fr=m_pMeParams->pRefB;

        if(m_pMeParams->pSrc->bBottom != fr[m_pCur->RefIdx]->bBottom)
        {
            if(fr[m_pCur->RefIdx]->bBottom == false) y_shift = 2;
            else y_shift = -2;
        }
        else
        {
            y_shift = 0;
        }
        if(y_shift != 0)
        {
            mv.y+=y_shift;
            if(mv.y<4*m_pMeParams->PicRange.top_left.y)return true;
            if(mv.y>4*m_pMeParams->PicRange.bottom_right.y-h)return true;
        }
    }
    return false;
}

void MePredictCalculator::TrimSearchRange(MeMbPart mt, MePixelType /*pix*/, Ipp32s &x0, Ipp32s &x1, Ipp32s &y0, Ipp32s &y1 )
{
    Ipp32s x=4*16*m_pCur->x;
    Ipp32s y=4*16*m_pCur->y;

    Ipp32s w=4*16, h=4*16;
    switch(mt)
    {
//    case ME_Mb16x16:
//        w = 16;
//        h = 16;
//        break;
    case ME_Mb16x8:
        w = 4*16;
        h = 4*8;
        break;
    case ME_Mb8x16:
        w = 4*8;
        h = 4*16;
        break;
    case ME_Mb8x8:
        w = 4*8;
        h = 4*8;
        break;
    case ME_Mb4x4:
        w = 4*4;
        h = 4*4;
        break;
    case ME_Mb2x2:
        w = 4*2;
        h = 4*2;
        break;
    }

    x0 = IPP_MAX(x0,IPP_MAX(4*m_pMeParams->PicRange.top_left.x-x, -4*m_pMeParams->SearchRange.x));
    x1 = IPP_MIN(x1,IPP_MIN((4*m_pMeParams->PicRange.bottom_right.x-w)+1-x, 4*m_pMeParams->SearchRange.x));
    y0= IPP_MAX(y0,IPP_MAX(4*m_pMeParams->PicRange.top_left.y-y, -4*m_pMeParams->SearchRange.y));
    y1= IPP_MIN(y1,IPP_MIN((4*m_pMeParams->PicRange.bottom_right.y-h)+1-y, 4*m_pMeParams->SearchRange.y));
}

MeMV MePredictCalculator::GetCurrentBlockMV(int isBkw, Ipp32s idx)
{
    if(m_pCur->InterType[idx] == ME_MbIntra) return MeMV(0);
    return m_pCur->BestMV[isBkw][m_pCur->BestIdx[isBkw][idx]][idx];
}
bool MePredictCalculator::GetCurrentBlockSecondRef(int isBkw, Ipp32s idx)
{
    return (m_pCur->BestIdx[isBkw][idx] == 1);
}
MeMbType MePredictCalculator::GetCurrentBlockType(Ipp32s idx)
{
    return m_pCur->InterType[idx];
}

Ipp32s MePredictCalculator::GetT2()
{
    switch(m_pCur->MbPart){
        case ME_Mb16x16:
            return GetT2_16x16();
        case ME_Mb8x8:
            return GetT2_8x8();
        //default:
            //SetError((vm_char*));
    }
    return 0; //forse full search
}

Ipp32s MePredictCalculator::GetT2_16x16()
{
    Ipp32s T2, SadA=ME_BIG_COST, SadB=ME_BIG_COST, SadC=ME_BIG_COST;
    Ipp32s adr=m_pCur->adr;
    Ipp32s w=m_pMeParams->pSrc->WidthMB;

    if(MbTopEnable && m_pRes[adr -w].MbType != ME_MbIntra)
        SadA = m_pRes[adr-w].MbCosts[0];
    if(MbTopRightEnable && m_pRes[adr-w+1].MbType != ME_MbIntra)
        SadB = m_pRes[adr-w+1].MbCosts[0];
    if(MbLeftEnable && m_pRes[adr-1].MbType != ME_MbIntra)
        SadC = m_pRes[adr-1].MbCosts[0];

    if(SadA == ME_BIG_COST && SadB == ME_BIG_COST && SadC == ME_BIG_COST)
    {
        T2 = -1;
    }
    else
    {
        T2 = 6*IPP_MIN(IPP_MIN(SadA,SadB), SadC)/5 + 128;
    }
    //T2 = IPP_MIN(IPP_MIN(SadA,SadB), IPP_MIN(SadC,SadCol));

    return T2;
}

Ipp32s MePredictCalculator::GetT2_8x8()
{
    Ipp32s T2, SadA=ME_BIG_COST, SadB=ME_BIG_COST, SadC=ME_BIG_COST;
    Ipp32s adr=m_pCur->adr;
    Ipp32s w=m_pMeParams->pSrc->WidthMB;

    switch(m_pCur->BlkIdx)
    {
    case 0:
        if (MbLeftEnable && m_pRes[adr-1].BlockType[1] != ME_MbIntra)
        {
            SadC = m_pRes[adr-1].MbCosts[2];
        }
        if (MbTopEnable && m_pRes[adr-w].BlockType[2] != ME_MbIntra)
        {
            SadA = m_pRes[adr-w].MbCosts[3];
        }
        if (MbTopLeftEnable && m_pRes[adr-w-1].BlockType[3] != ME_MbIntra)
        {
            SadB = m_pRes[adr-w-1].MbCosts[4];
        }
        else if(MbTopEnable && m_pRes[adr-w].BlockType[3] != ME_MbIntra)
        {
            SadB = m_pRes[adr-w].MbCosts[4];
        }
        break;
    case 1:
        SadC = m_pCur->InterCost[0];

        if (MbTopEnable && m_pRes[adr-w].BlockType[3] != ME_MbIntra)
        {
            SadA = m_pRes[adr-w].MbCosts[4];
        }
        if (MbTopRightEnable && m_pRes[adr-w+1].BlockType[2] != ME_MbIntra)
        {
            SadB = m_pRes[adr-w+1].MbCosts[3];
        }
        else if(MbTopEnable && m_pRes[adr-w].BlockType[2] != ME_MbIntra)
        {
            SadB = m_pRes[adr-w].MbCosts[3];
        }
        break;
    case 2:
        if (MbLeftEnable && m_pRes[adr-1].BlockType[3] != ME_MbIntra)
        {
            SadC = m_pRes[adr-1].MbCosts[4];
        }
        SadA = m_pCur->InterCost[0];;
        SadB = m_pCur->InterCost[1];
        break;
    case 3:
        SadC = m_pCur->InterCost[2];
        SadA = m_pCur->InterCost[1];
        SadB = m_pCur->InterCost[0];
        break;
    default:
        VM_ASSERT(0);
        break;
    }
    if(SadA == ME_BIG_COST && SadB == ME_BIG_COST && SadC == ME_BIG_COST)
    {
        T2 = -1;
    }
    else
    {
        T2 = (6*IPP_MIN(IPP_MIN(SadA,SadB), SadC)/5 + 32);///2;
    }
    return T2;
}

void MePredictCalculator::SetMbEdges()
{
    // Edge MBs definitions.

    if(m_pCur->x == 0 || m_pCur->adr == m_pMeParams->FirstMB)MbLeftEnable = false;
    else MbLeftEnable = true;

    if(m_pCur->x == m_pMeParams->pSrc->WidthMB-1 || m_pCur->adr == m_pMeParams->LastMB)
        MbRightEnable = false;
    else MbRightEnable = true;

    if(m_pCur->y == 0 || m_pCur->adr - m_pMeParams->pSrc->WidthMB < m_pMeParams->FirstMB)
    {
        MbTopEnable = MbTopLeftEnable = MbTopRightEnable = false;
    }
    else
    {
        MbTopEnable = MbTopLeftEnable = MbTopRightEnable = true;
        if(!MbLeftEnable)  MbTopLeftEnable  = false;
        if(!MbRightEnable) MbTopRightEnable = false;
    }
}

MeMV MePredictCalculatorH264::GetPrediction16x16()
{
    SetMbEdges();
    MeMV mv;
    ComputePredictors16x16((Ipp8s)m_pCur->RefDir,m_pCur->RefIdx,&mv);
    return mv;
}

MeMV MePredictCalculatorH264::GetPrediction8x8()
{
    //SetError
    MeMV mv;
    return mv;
}


void MePredictCalculatorH264::SetPredictionPSkip()
{
    SetMbEdges();

    if ((MbLeftEnable)&&
        (MbTopEnable))
    {
        // above neighbour
        AMV = MeMV(0);
        Ipp32s RefIndxA = -1;
        // left neighbour
        CMV = MeMV(0);
        Ipp32s RefIndxC = -1;

        //CMV = m_pRes[m_pCur->adr-1].MV[0][3];
        //AMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[0][12];
        //RefIndxA = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[0][12];
        //RefIndxC = m_pRes[m_pCur->adr-1].Refindex[0][3];

        // without MBAF mode
        SetMVNeighbBlkParamDepPart(Left, 0, &CMV, &RefIndxC);
        SetMVNeighbBlkParamDepPart(Top, 0, &AMV, &RefIndxA);

        Ipp32s CurRefIndx = m_pRes[m_pCur->adr].Refindex[0][0];

        if ((AMV.x | AMV.y | RefIndxA)&&
            (CMV.x | CMV.y | RefIndxC))
        {
            Ipp32s RefIndxB = -1;
            Ipp8u Equal = 0;
            // right neighbour
            BMV = MeMV(0);
            if (MbTopRightEnable)
            {
                // rigth above
                //BMV = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB + 1].MV[0][12];
                //RefIndxB = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB + 1].Refindex[0][12];
                SetMVNeighbBlkParamDepPart(TopRight, 0, &BMV, &RefIndxB);
            }
            else if (MbTopLeftEnable)
            {
                // left above
                //BMV = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB - 1].MV[0][15];
                //RefIndxB = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB -1].Refindex[0][15];
                SetMVNeighbBlkParamDepPart(TopLeft, 0, &BMV, &RefIndxB);
            }

            Equal += (CurRefIndx != RefIndxA) ? 1 : 0;
            Equal += (CurRefIndx != RefIndxB) ? 1 : 0;
            Equal += (CurRefIndx != RefIndxC) ? 1 : 0;

            if (1 == Equal)
            {
                if (CurRefIndx == RefIndxA)
                    m_pCur->PredMV[0][CurRefIndx][0] = AMV;
                else if (CurRefIndx == RefIndxB)
                    m_pCur->PredMV[0][CurRefIndx][0] = BMV;
                else
                    m_pCur->PredMV[0][CurRefIndx][0] = CMV;

                return;
            }
            // real median of A, B, C
            else
            {
                m_pCur->PredMV[0][CurRefIndx][0].x = median3(AMV.x, BMV.x, CMV.x);
                m_pCur->PredMV[0][CurRefIndx][0].y = median3(AMV.y, BMV.y, CMV.y);
            }

        }
    }
    else
    {
        m_pCur->PredMV[0][0][0].x = 0;
        m_pCur->PredMV[0][0][0].y = 0;
    }

}

void MePredictCalculatorH264::SetPredictionSpatialDirectSkip16x16()
{

    Ipp32s  RefIndexL0, RefIndexL1;
    Ipp32s refIdxL0, refIdxL1;
    //set reference index
    SetSpatialDirectRefIdx(&refIdxL0,&refIdxL1);
    RefIndexL0 = (Ipp8s)refIdxL0;
    RefIndexL1 = (Ipp8s)refIdxL1;
    Ipp32u uPredDir;

    // set ref index array
    {
        if (0 <= (RefIndexL0 & RefIndexL1))
        {
            memset(&m_pRes[m_pCur->adr].Refindex[0],RefIndexL0,16);
            memset(&m_pRes[m_pCur->adr].Refindex[1],RefIndexL1,16);
        }
        else
        {
            memset(&m_pRes[m_pCur->adr].Refindex[0],0,16);
            memset(&m_pRes[m_pCur->adr].Refindex[1],0,16);
        }
    }

    if ((0 <= RefIndexL0) && (0 > RefIndexL1))
    {
        // forward
        ComputePredictors16x16(0,RefIndexL0,&m_pCur->PredMV[0][RefIndexL0][0]);
        RestrictDirection = 0;
    }
    else if ((0 > RefIndexL0) && (0 <= RefIndexL1))
    {
        // backward
        ComputePredictors16x16(1,RefIndexL1,&m_pCur->PredMV[1][RefIndexL1][0]);
        RestrictDirection = 1;
    }
    else
    {
        // bidirectional
        ComputePredictors16x16(0,RefIndexL0,&m_pCur->PredMV[0][RefIndexL0][0]);
        ComputePredictors16x16(1,RefIndexL1,&m_pCur->PredMV[1][RefIndexL1][0]);
        RestrictDirection = 2;
    }

    // we sure that collacated MB in ref frame has 16x16 partition.

    // Encoder should orginize MVDirect
    MeMV pMVRefL0 = m_pMeParams->pRefF[0]->MVDirect[m_pCur->adr];
    MeMV pMVRefL1 = m_pMeParams->pRefB[0]->MVDirect[m_pCur->adr];

    MeMV* pMVCurRef;

    Ipp8s colRefIndexL0 = (Ipp8s)m_pMeParams->pRefF[0]->RefIndx[m_pCur->adr];
    Ipp8s colRefIndexL1 = (Ipp8s)m_pMeParams->pRefB[0]->RefIndx[m_pCur->adr];
    Ipp8s CurColRefIndex = colRefIndexL0;

    if (colRefIndexL0 >= 0)
        pMVCurRef = &pMVRefL0;
    else
    {
        pMVCurRef = &pMVRefL1;
        CurColRefIndex = colRefIndexL1;
    }
    // TO_DO. isInter for collaceted MVs
    bool isNeedToCheck = (RefIndexL0 != -1)&&(RefIndexL1 != -1)&&(CurColRefIndex == 0);

    if (isNeedToCheck)
    {
        bool bUseZeroPredL0 = false;
        bool bUseZeroPredL1 = false;

        if (pMVCurRef->x >= -1 &&
            pMVCurRef->x <= 1 &&
            pMVCurRef->y >= -1 &&
            pMVCurRef->y <= 1)
        {
            bUseZeroPredL0 = (RefIndexL0 == 0);
            bUseZeroPredL1 = (RefIndexL1 == 0);
        }
        if (bUseZeroPredL0)
        {
            m_pCur->PredMV[0][RefIndexL0][0].x = 0;
            m_pCur->PredMV[0][RefIndexL0][0].y = 0;
        }
        if (bUseZeroPredL1)
        {
            m_pCur->PredMV[1][RefIndexL1][0].x = 0;
            m_pCur->PredMV[1][RefIndexL1][0].y = 0;
        }
    }

}
void MePredictCalculatorH264::ComputePredictors16x16(Ipp8s ListNum, Ipp32s RefIndex, MeMV* res)
{
    if ((MbLeftEnable)||
        (MbTopEnable))
    {
        Ipp32s RefIdxl = -1;
        Ipp32s RefIdxa = -1;
        Ipp32s RefIdxr = -1;
        // above
        AMV = MeMV(0);
        // above right(left)
        BMV = MeMV(0);
        // left
        CMV = MeMV(0);

        Ipp32u RefPicCounter = 3;

        MeMV* pTmpMV = NULL;

        if (MbLeftEnable)
        {
            SetMVNeighbBlkParamDepPart(Left, ListNum, &CMV, &RefIdxl);
            //CMV = m_pRes[m_pCur->adr-1].MV[ListNum][3];
            //RefIdxl = m_pRes[m_pCur->adr-1].Refindex[ListNum][3];
        }

        if (MbTopEnable)
        {
            SetMVNeighbBlkParamDepPart(Top, ListNum, &AMV, &RefIdxa);
            //AMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[ListNum][12];
            //RefIdxa = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[ListNum][12];
        }

        if (MbTopRightEnable)
        {
            SetMVNeighbBlkParamDepPart(TopRight, ListNum, &BMV, &RefIdxr);
            //BMV = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB + 1].MV[ListNum][12];
            //RefIdxr = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB + 1].Refindex[ListNum][12];
        }
        else if (MbTopLeftEnable)
        {
            // left above
            SetMVNeighbBlkParamDepPart(TopLeft, ListNum, &BMV, &RefIdxr);
            //BMV = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB - 1].MV[ListNum][15];
            //RefIdxr = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB -1].Refindex[ListNum][15];
        }
        // If above AND right are unavailable AND left is available, use left
        if ((!MbTopEnable)&&
            (!MbTopRightEnable)&&
            (!MbTopLeftEnable)&&
            (MbLeftEnable))
        {
            res->x = CMV.x;
            res->y = CMV.y;
        }
        else
        {
            if (RefIdxl != RefIndex)
                --RefPicCounter;
            else
            {
                pTmpMV = &CMV;
            }
            if (RefIdxa != RefIndex)
                --RefPicCounter;
            else
            {
                pTmpMV = &AMV;
            }
            if (RefIdxa != RefIndex)
                --RefPicCounter;
            else
            {
                pTmpMV = &BMV;
            }

            // use median. all predictors are exist
            if (RefPicCounter != 1)
            {
                res->x = median3(AMV.x, BMV.x, CMV.x);
                res->y = median3(AMV.y, BMV.y, CMV.y);
            }
            else
            {
                VM_ASSERT(pTmpMV);
                res->x = pTmpMV->x;
                res->y = pTmpMV->y;
            }
        }

    }
    else
    {
        res->x = 0;
        res->y = 0;
    }
    //printf("res->x = %d\n",res->x);
    //printf("res->y = %d\n",res->y);
}
void MePredictCalculatorH264::SetMVNeighbBlkParamDepPart(MeH264Neighbour direction,
                                                              Ipp8s ListNum,
                                                              MeMV* pMV,
                                                              Ipp32s* RefIdx)
{
    MeMB* neib_mv;
    // posible partitions: 16x16, 16x8, 8x16, 8x8, 4x4
    static Ipp32u LeftTbl[5] = {0, 0, 0, 0, 3};
    static Ipp32u TopTbl[5] = {0, 1, 1, 3, 12}; //use for top and topleft
    static Ipp32u TopRigthTbl[5] = {0, 1, 1, 3, 15};

    switch (direction)
    {
    case Left:
        neib_mv =  &m_pRes[m_pCur->adr - 1];
        *pMV = neib_mv->MV[ListNum][LeftTbl[neib_mv->MbPart]];
        *RefIdx = neib_mv->Refindex[ListNum][LeftTbl[neib_mv->MbPart]];
        return;
    case Top:
        neib_mv = &m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB];
        *pMV = neib_mv->MV[ListNum][TopTbl[neib_mv->MbPart]];
        *RefIdx = neib_mv->Refindex[ListNum][TopTbl[neib_mv->MbPart]];
        return;
    case TopRight:
        neib_mv =  &m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB  + 1];
        *pMV = neib_mv->MV[ListNum][TopTbl[neib_mv->MbPart]];
        *RefIdx = neib_mv->Refindex[ListNum][TopTbl[neib_mv->MbPart]];
        return;
    case TopLeft:
        neib_mv = &m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB - 1];
        *pMV = neib_mv->MV[ListNum][TopRigthTbl[neib_mv->MbPart]];
        *RefIdx = neib_mv->Refindex[ListNum][TopRigthTbl[neib_mv->MbPart]];
        return;
    default:
        return;
    }
}


void MePredictCalculatorH264::SetSpatialDirectRefIdx(Ipp32s* RefIdxL0, Ipp32s* RefIdxL1)
{
    Ipp32s refIdxL0 = -1;
    Ipp32s refIdxL1 = -1;
    Ipp32s tmp;
    // without MBAFF
    // first check Left block
    if (MbLeftEnable)
    {
         refIdxL0 = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[0][3];
         refIdxL1 = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[1][3];
    }
    // above check
    if (MbTopEnable)
    {
        tmp = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[0][12];
        refIdxL0 = (( refIdxL0) <= tmp) ? (refIdxL0) : (tmp);
        tmp = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[1][12];
        refIdxL1 = (( refIdxL1) <= tmp) ? (refIdxL1) : (tmp);
    }
    // above right(left)
    if (MbTopRightEnable)
    {
        tmp = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].Refindex[0][12];
        refIdxL0 = (( refIdxL0) <= tmp) ? (refIdxL0) : (tmp);
        tmp = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].Refindex[1][12];
        refIdxL1 = (( refIdxL1) <= tmp) ? (refIdxL1) : (tmp);
    }
    else
    {
        tmp = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-11].Refindex[0][15];
        refIdxL0 = ((refIdxL0) <= tmp) ? (refIdxL0) : (tmp);
        tmp = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+-1].Refindex[1][15];
        refIdxL1 = (( refIdxL1) <= tmp) ? (refIdxL1) : (tmp);
    }
    *RefIdxL0 = refIdxL0;
    *RefIdxL1 = refIdxL0;
}

//$$$$$$$$$$$$$$$$$$$$$$$$ VC1 realization
void MePredictCalculatorVC1::Init(MeParams* pMeParams, MeCurrentMB* pCur)
{
    MePredictCalculator::Init(pMeParams,pCur);
    if(pMeParams->SearchDirection == ME_ForwardSearch)
    {
        MVPredMin = MeMV(-60,-60);
        MVPredMax = MeMV(pMeParams->pSrc->WidthMB*64-4,pMeParams->pSrc->HeightMB*64-4);
        Mult = 64;
    }
    else
    {
        MVPredMin = MeMV(-28,-28);
        MVPredMax = MeMV(pMeParams->pSrc->WidthMB*32-4,pMeParams->pSrc->HeightMB*32-4);
        Mult = 32;
    }
}

bool MePredictCalculatorVC1::IsOutOfBound(MeMbPart mt, MePixelType pix, MeMV mv)
{
    if( mv.x<-4*m_pMeParams->SearchRange.x|| mv.x>=4*m_pMeParams->SearchRange.x ||
        mv.y<-4*m_pMeParams->SearchRange.y || mv.y>=4*m_pMeParams->SearchRange.y)
        return true;

    Ipp32s w=4*16, h=4*16;

    //mt = ME_Mb16x16 only; to suppose BlkIdx = 0

    mv.x+=4*16*m_pCur->x;
    mv.y+=4*16*m_pCur->y;

    if(mv.x<4*m_pMeParams->PicRange.top_left.x)return true;
    if(mv.y<4*m_pMeParams->PicRange.top_left.y)return true;
    if(mv.x>4*m_pMeParams->PicRange.bottom_right.x-w)return true;
    if(mv.y>4*m_pMeParams->PicRange.bottom_right.y-h)return true;
    bool isField = (m_pMeParams->PredictionType == ME_VC1Field1 || m_pMeParams->PredictionType == ME_VC1Field2
        || m_pMeParams->PredictionType == ME_VC1Field2Hybrid);

    if(isField)
    {
        Ipp32s y_shift = 0;
        Ipp32s level=0;
        if(pix == ME_DoublePixel) level = 1;
        if(pix == ME_QuadPixel) level = 2;
        if(pix == ME_OctalPixel) level = 3;

        MeFrame **fr;
        if(m_pCur->RefDir==frw)fr=m_pMeParams->pRefF;
        else fr=m_pMeParams->pRefB;

        if(m_pMeParams->pSrc->bBottom != fr[m_pCur->RefIdx]->bBottom)
        {
            if(fr[m_pCur->RefIdx]->bBottom == false) y_shift = 2;
            else y_shift = -2;
        }
        else
        {
            y_shift = 0;
        }
        if(y_shift != 0)
        {
            mv.y+=y_shift;
            if(mv.y<4*m_pMeParams->PicRange.top_left.y)return true;
            if(mv.y>4*m_pMeParams->PicRange.bottom_right.y-h)return true;
        }
    }
    return false;
}

void MePredictCalculatorVC1::ScalePredict(MeMV* MV)
{
    Ipp32s x,y;

    x = m_pCur->x*Mult;
    y = m_pCur->y*Mult;

    x += MV->x;
    y += MV->y;

    if (x < MVPredMin.x)
    {
        MV->x = MV->x - (Ipp16s)(x- MVPredMin.x);
    }
    else if (x > MVPredMax.x)
    {
        MV-> x = MV-> x - (Ipp16s)(x-MVPredMax.x);
    }

    if (y < MVPredMin.y)
    {
        MV->y = MV->y - (Ipp16s)(y - MVPredMin.y);
    }
    else if (y > MVPredMax.y)
    {
        MV->y = MV->y - (Ipp16s)(y - MVPredMax.y);
    }
}
MeMV MePredictCalculatorVC1::GetPrediction(MeMV mv)
{
    // TODO: add other type of predictor here
    MeBase::SetError((vm_char*)"Wrong predictor in MePredictCalculatorVC1::GetPrediction", m_pMeParams->PredictionType!=ME_VC1Hybrid);

    MeMV res;
    if(m_pMeParams->PredictionType == ME_VC1Hybrid)
        GetPredictorVC1_hybrid(mv, &res);
    else
        res = m_pCur->PredMV[m_pCur->RefDir][m_pCur->RefIdx][m_pCur->BlkIdx];
    return res;
}
void MePredictCalculatorVC1::SetDefFrwBkw(MeMV &mvF, MeMV &mvB)
{
    if(m_pMeParams->PredictionType != ME_VC1Field2)
    {
        mvF = m_pMeParams->pRefF[m_pCur->RefIdx]->MVDirect[m_pCur->adr];
        mvB = m_pMeParams->pRefB[m_pCur->RefIdx]->MVDirect[m_pCur->adr];
        m_pCur->BestIdx[frw][0] = 0;
        m_pCur->BestIdx[bkw][0] = 0;
    }
    else //if(PredictionType == ME_VC1Field2)
    {
        mvF = GetDef_FieldMV(frw,0);
        mvB = GetDef_FieldMV(bkw,0);
        m_pCur->BestIdx[frw][0] = GetDef_Field(frw,0);
        m_pCur->BestIdx[bkw][0] = GetDef_Field(bkw,0);
    }

    mv1MVField[frw] = mvF;
    mv1MVField[bkw] = mvB;
    prefField1MV[frw] = m_pCur->BestIdx[frw][0];
    prefField1MV[bkw] = m_pCur->BestIdx[bkw][0];
}

void MePredictCalculatorVC1::SetDefMV(MeMV &mv, Ipp32s dir)
{
    mv = mv1MVField[dir];
    m_pCur->BestIdx[dir][0] = prefField1MV[dir];
}

MeMV MePredictCalculatorVC1::GetPrediction16x16()
{
    if(!m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx].IsInvalid())
        return m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx];

    switch(m_pMeParams->PredictionType)
    {
    case ME_MPEG2:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorMPEG2;
        break;
    case ME_VC1:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1;
        break;
    case ME_VC1Hybrid:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1_hybrid;
        break;
    case ME_VC1Field1:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1Field1;
        break;
    case ME_VC1Field2:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1Field2;
        break;
    case ME_VC1Field2Hybrid:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1Field2Hybrid;
        break;
    default:
        return false;
    }

     // Edge MBs definitions.
    SetMbEdges();

    m_FieldPredictPrefer[m_pCur->RefDir][0] = (this->*(GetPredictor))(ME_Macroblock,m_pCur->RefDir,m_CurPrediction[m_pCur->RefDir][0]);
    return m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx];
}

MeMV MePredictCalculatorVC1::GetPrediction8x8()
{

  //  if(!m_CurPrediction[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx].IsInvalid())
   //     return m_CurPrediction[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];

    switch(m_pMeParams->PredictionType)
    {
    case ME_MPEG2:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorMPEG2;
        break;
    case ME_VC1:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1;
        break;
    case ME_VC1Hybrid:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1_hybrid;
        break;
    case ME_VC1Field1:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1Field1;
        break;
    case ME_VC1Field2:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1Field2;
        break;
    case ME_VC1Field2Hybrid:
        GetPredictor = &UMC::MePredictCalculatorVC1::GetPredictorVC1Field2Hybrid;
        break;
    default:
        return false;
    }

     // Edge MBs definitions.
    SetMbEdges();

    m_FieldPredictPrefer[m_pCur->RefDir][m_pCur->BlkIdx] = (this->*(GetPredictor))(m_pCur->BlkIdx,m_pCur->RefDir,m_CurPrediction[m_pCur->RefDir][m_pCur->BlkIdx]);
    return m_CurPrediction[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];
}


void MePredictCalculatorVC1::GetBlockVectorsABC_0(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

    if (MbLeftEnable)
    {
        CMV = m_pRes[m_pCur->adr-1].MV[isBkw][1];
    }
    else
    {
        isCOutOfBound = true;
    }
    if (!MbTopEnable)
    {
        isAOutOfBound = true;
        isBOutOfBound = true;
        return;
    }
    //MbTopEnable == true:
    AMV = m_pRes[m_pCur->adr - m_pMeParams->pSrc->WidthMB].MV[isBkw][2];

    if (MbTopLeftEnable)
    {
        BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-1].MV[isBkw][3];
    }
    else
    {
        //MbTopEnable == true:
        BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][3];
    }
}


void MePredictCalculatorVC1::GetBlockVectorsABC_1(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

//    CMV = m_pRes[m_pCur->adr].MV[isBkw][0];
    CMV = GetCurrentBlockMV(isBkw,0);

    if (!MbTopEnable)
    {
        isAOutOfBound = true;
        isBOutOfBound = true;
        return;
    }
    //MbTopEnable == true:
    AMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][3];

    if (MbTopRightEnable)
    {
        BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].MV[isBkw][2];
    }
    else
    {
        //MbTopEnable == true:
        BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][2];
    }
}

void MePredictCalculatorVC1::GetBlockVectorsABC_2(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

    if (MbLeftEnable)
    {
        CMV = m_pRes[m_pCur->adr-1].MV[isBkw][3];
    }
    else
    {
        isCOutOfBound = true;
    }
//    AMV = m_pRes[m_pCur->adr].MV[isBkw][0];
//    BMV = m_pRes[m_pCur->adr].MV[isBkw][1];
    AMV = GetCurrentBlockMV(isBkw,0);
    BMV = GetCurrentBlockMV(isBkw,1);
}

void MePredictCalculatorVC1::GetBlockVectorsABC_3(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

//    CMV = m_pRes[m_pCur->adr].MV[isBkw][2];
//    AMV = m_pRes[m_pCur->adr].MV[isBkw][1];
//    BMV = m_pRes[m_pCur->adr].MV[isBkw][0];
    CMV = GetCurrentBlockMV(isBkw,2);
    AMV = GetCurrentBlockMV(isBkw,1);
    BMV = GetCurrentBlockMV(isBkw,0);
}

void MePredictCalculatorVC1::GetMacroBlockVectorsABC(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

    if (MbLeftEnable)
    {
        CMV = m_pRes[m_pCur->adr-1].MV[isBkw][1];
    }
    else
    {
        isCOutOfBound = true;
    }

    if (MbTopEnable)
    {
        AMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][2];
    }
    else
    {
        isAOutOfBound = true;
    }

    if (MbTopRightEnable)
    {
        BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].MV[isBkw][2];
    }
    else
    {
        if(MbTopLeftEnable)
        {
            BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-1].MV[isBkw][3];
        }
        else
        {
            isBOutOfBound = true;
        }
    }
}

void MePredictCalculatorVC1::GetBlockVectorsABCField_0(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);


    AMVbSecond  = false;
    BMVbSecond  = false;
    CMVbSecond  = false;
    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

    if (MbLeftEnable)
    {
        if(m_pRes[m_pCur->adr-1].BlockType[1] == ME_MbIntra)
        {
            isCOutOfBound = true;
        }
        else
        {
            CMV = m_pRes[m_pCur->adr-1].MV[isBkw][1];
            CMVbSecond =  m_pRes[m_pCur->adr-1].Refindex[isBkw][1] == 1;
        }
    }
    else
    {
        isCOutOfBound = true;
    }
    if(!MbTopEnable)
    {
        isAOutOfBound = true;
        isBOutOfBound = true;
        return;
    }
    //MbTopEnable == true;
    if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].BlockType[2] == ME_MbIntra)
    {
        isAOutOfBound = true;
    }
    else
    {
        AMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][2];
        AMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[isBkw][2] == 1;
    }

    if (MbTopLeftEnable)
    {
        if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-1].BlockType[3] == ME_MbIntra)
        {
            isBOutOfBound = true;
        }
        else
        {
            BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-1].MV[isBkw][3];
            BMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-1].Refindex[isBkw][3] == 1;
        }
    }
    else
    {
        //MbTopEnable == true:
        if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].BlockType[3] == ME_MbIntra)
        {
            isBOutOfBound = true;
        }
        else
        {
            BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][3];
            BMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[isBkw][3] == 1;
        }
    }

}

void MePredictCalculatorVC1::GetBlockVectorsABCField_1(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    AMVbSecond  = false;
    BMVbSecond  = false;
    CMVbSecond  = false;
    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

    if(GetCurrentBlockType(0) == ME_MbIntra)
    {
        isCOutOfBound = true;
    }
    else
    {
       // CMV = m_pRes[m_pCur->adr].MV[isBkw][0];
        CMV = GetCurrentBlockMV(isBkw,0);
        //CMVbSecond =  m_pRes[m_pCur->adr].Refindex[isBkw][0] == 1;
        CMVbSecond = GetCurrentBlockSecondRef(isBkw,0);
    }

    if(!MbTopEnable)
    {
        isAOutOfBound = true;
        isBOutOfBound = true;
        return;
    }
    //MbTopEnable == true:
    if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].BlockType[3] == ME_MbIntra)
    {
        isAOutOfBound = true;
    }
    else
    {
        AMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][3];
        AMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[isBkw][3] == 1;
    }

    if (MbTopRightEnable)
    {
        if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].BlockType[2] == ME_MbIntra)
        {
            isBOutOfBound = true;
        }
        else
        {
            BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].MV[isBkw][2];
            BMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].Refindex[isBkw][2] == 1;
        }
    }
    else //MbTopEnable == true:
    {
        if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].BlockType[2] == ME_MbIntra)
        {
            isBOutOfBound = true;
        }
        else
        {
            BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][2];
            BMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[isBkw][2] == 1;
        }
    }
}

void MePredictCalculatorVC1::GetBlockVectorsABCField_2(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    AMVbSecond  = false;
    BMVbSecond  = false;
    CMVbSecond  = false;
    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

    if (MbLeftEnable)
    {
        if(m_pRes[m_pCur->adr-1].BlockType[3] == ME_MbIntra)
        {
            isCOutOfBound = true;
        }
        else
        {
            CMV = m_pRes[m_pCur->adr-1].MV[isBkw][3];
            CMVbSecond =  m_pRes[m_pCur->adr-1].Refindex[isBkw][3] == 1;
        }
    }
    else
    {
        isCOutOfBound = true;
    }

    if(GetCurrentBlockType(0) == ME_MbIntra)
    {
        isAOutOfBound = true;
    }
    else
    {
        //AMV = m_pRes[m_pCur->adr].MV[isBkw][0];
        AMV = GetCurrentBlockMV(isBkw,0);
        //AMVbSecond =  m_pRes[m_pCur->adr].Refindex[isBkw][0] == 1;
        AMVbSecond = GetCurrentBlockSecondRef(isBkw,0);
    }

    if(GetCurrentBlockType(1) == ME_MbIntra)
    {
        isBOutOfBound = true;
    }
    else
    {
        //BMV = m_pRes[m_pCur->adr].MV[isBkw][1];
        BMV = GetCurrentBlockMV(isBkw,1);
       // BMVbSecond =  m_pRes[m_pCur->adr].Refindex[isBkw][1] == 1;
        BMVbSecond = GetCurrentBlockSecondRef(isBkw,1);
    }
}

void MePredictCalculatorVC1::GetBlockVectorsABCField_3(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    AMVbSecond  = false;
    BMVbSecond  = false;
    CMVbSecond  = false;
    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

    if(GetCurrentBlockType(2) == ME_MbIntra)
    {
        isCOutOfBound = true;
    }
    else
    {
        //CMV = m_pRes[m_pCur->adr].MV[isBkw][2];
        CMV = GetCurrentBlockMV(isBkw,2);
        //CMVbSecond =  m_pRes[m_pCur->adr].Refindex[isBkw][2] == 1;
        CMVbSecond = GetCurrentBlockSecondRef(isBkw,2);
    }

    if(GetCurrentBlockType(1) == ME_MbIntra)
    {
        isAOutOfBound = true;
    }
    else
    {
        //AMV = m_pRes[m_pCur->adr].MV[isBkw][1];
        AMV = GetCurrentBlockMV(isBkw,1);
        //AMVbSecond =  m_pRes[m_pCur->adr].Refindex[isBkw][1] == 1;
        AMVbSecond = GetCurrentBlockSecondRef(isBkw,1);
    }

    if(GetCurrentBlockType(0) == ME_MbIntra)
    {
        isBOutOfBound = true;
    }
    else
    {
        //BMV = m_pRes[m_pCur->adr].MV[isBkw][0];
        BMV = GetCurrentBlockMV(isBkw,0);
        //BMVbSecond =  m_pRes[m_pCur->adr].Refindex[isBkw][0]  == 1;
        BMVbSecond = GetCurrentBlockSecondRef(isBkw,0);
    }
}

void MePredictCalculatorVC1::GetMacroBlockVectorsABCField(int isBkw)
{
    AMV = MeMV(0);
    BMV = MeMV(0);
    CMV = MeMV(0);

    AMVbSecond = false;
    BMVbSecond = false;
    CMVbSecond = false;
    isAOutOfBound = false;
    isBOutOfBound = false;
    isCOutOfBound = false;

    if (MbLeftEnable)
    {
        if(m_pRes[m_pCur->adr-1].BlockType[1] == ME_MbIntra)
        {
            isCOutOfBound = true;
        }
        else
        {
            CMV = m_pRes[m_pCur->adr-1].MV[isBkw][1];
            CMVbSecond =  m_pRes[m_pCur->adr-1].Refindex[isBkw][1] == 1;
        }

    }
    else
    {
        isCOutOfBound = true;
    }

    if (MbTopEnable)
    {
        if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].BlockType[2] == ME_MbIntra)
        {
            isAOutOfBound = true;
        }
        else
        {
            AMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].MV[isBkw][2];
            AMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB].Refindex[isBkw][2] == 1;
        }
    }
    else
    {
        isAOutOfBound = true;
    }
    if (MbTopRightEnable)
    {
        if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].BlockType[2] == ME_MbIntra)
        {
            isBOutOfBound = true;
        }
        else
        {
            BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].MV[isBkw][2];
            BMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB+1].Refindex[isBkw][2] == 1;
        }
    }
    else
    {
        if(MbTopLeftEnable)
        {
            if(m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-1].BlockType[2] == ME_MbIntra)
            {
                isBOutOfBound = true;
            }
            else
            {
                BMV = m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-1].MV[isBkw][2];
                BMVbSecond =  m_pRes[m_pCur->adr-m_pMeParams->pSrc->WidthMB-1].Refindex[isBkw][2] == 1;
            }
        }
        else
        {
            isBOutOfBound = true;
        }
    }
}

Ipp32s MePredictCalculatorVC1::GetPredictorVC1(int index, int isBkw, MeMV* res)
{
    switch(index)
    {
    case ME_Block0:
        GetBlockVectorsABC_0(isBkw);
        break;
    case ME_Block1:
        GetBlockVectorsABC_1(isBkw);
        break;
    case ME_Block2:
        GetBlockVectorsABC_2(isBkw);
        break;
    case ME_Block3:
        GetBlockVectorsABC_3(isBkw);
        break;
    case ME_Macroblock:
        GetMacroBlockVectorsABC(isBkw);
        break;
    default:
        VM_ASSERT(false);
        break;
    }

    Ipp32s pIdx = index == ME_Macroblock ? 0:index;

    m_AMV[isBkw][pIdx][0] = AMV;
    m_BMV[isBkw][pIdx][0] = BMV;
    m_CMV[isBkw][pIdx][0] = CMV;

    if(!isAOutOfBound)
    {
        if(isCOutOfBound && isBOutOfBound)
        {
            res->x = AMV.x;
            res->y = AMV.y;
        }
        else
        {
            res->x=median3(AMV.x, BMV.x, CMV.x);
            res->y=median3(AMV.y, BMV.y, CMV.y);
        }
    }
    else
    {
        if(!isCOutOfBound)
        {
            res->x = CMV.x;
            res->y = CMV.y;
        }
        else
        {
            res->x = 0;
            res->y = 0;
        }
    }
    if(m_pMeParams->IsSMProfile)
        ScalePredict(res);
    m_pCur->HybridPredictor[0]=false;
    return 0;
}

Ipp32s MePredictCalculatorVC1::GetPredictorVC1_hybrid(int index,int isBkw, MeMV* res)
{
    Ipp32s sumA = 0, sumC = 0;
    //forward direction only

    switch(index)
    {
    case ME_Block0:
        GetBlockVectorsABC_0(0);
        break;
    case ME_Block1:
        GetBlockVectorsABC_1(0);
        break;
    case ME_Block2:
        GetBlockVectorsABC_2(0);
        break;
    case ME_Block3:
        GetBlockVectorsABC_3(0);
        break;
    case ME_Macroblock:
        GetMacroBlockVectorsABC(0);
        break;
    default:
        VM_ASSERT(false);
        break;
    }

    Ipp32s pIdx = index == ME_Macroblock ? 0:index;

    m_AMV[isBkw][pIdx][0] = AMV;
    m_BMV[isBkw][pIdx][0] = BMV;
    m_CMV[isBkw][pIdx][0] = CMV;

    if(!isAOutOfBound)
    {
        if(isCOutOfBound && isBOutOfBound)
        {
            res->x = AMV.x;
            res->y = AMV.y;
        }
        else
        {
            res->x=median3(AMV.x, BMV.x, CMV.x);
            res->y=median3(AMV.y, BMV.y, CMV.y);
        }
    }
    else
    {
        if(!isCOutOfBound)
        {
            res->x = CMV.x;
            res->y = CMV.y;
        }
        else
        {
            res->x = 0;
            res->y = 0;
        }
    }

    if(m_pMeParams->IsSMProfile)
        ScalePredict(res);
    if(!isAOutOfBound && !isCOutOfBound)
    {
        sumA = VC1ABS(AMV.x - res->x) + VC1ABS(AMV.y - res->y);
        sumC = VC1ABS(CMV.x - res->x) + VC1ABS(CMV.y - res->y);
        if (sumA > 32 || sumC>32)
        {
            sumA < sumC ? res[0] = AMV : res[0] = CMV;
        }
    }
    return 0;
}

void MePredictCalculatorVC1::GetPredictorVC1_hybrid(MeMV cur, MeMV* res)
{
    //should be called only after GetPredictorVC1_hybrid

    Ipp32s sumA = 0, sumC = 0;
    //forward direction only

    if(!isAOutOfBound)
    {
        if(isCOutOfBound && isBOutOfBound)
        {
            res->x = AMV.x;
            res->y = AMV.y;
        }
        else
        {
            res->x=median3(AMV.x, BMV.x, CMV.x);
            res->y=median3(AMV.y, BMV.y, CMV.y);
        }
    }
    else
    {
        if(!isCOutOfBound)
        {
            res->x = CMV.x;
            res->y = CMV.y;
        }
        else
        {
            res->x = 0;
            res->y = 0;
        }
    }
    if(m_pMeParams->IsSMProfile)
        ScalePredict(res);

    m_pCur->HybridPredictor[0]=false;
    if(!isAOutOfBound && !isCOutOfBound)
    {
        sumA = VC1ABS(AMV.x - res->x) + VC1ABS(AMV.y - res->y);
        sumC = VC1ABS(CMV.x - res->x) + VC1ABS(CMV.y - res->y);
        if (sumA > 32 || sumC>32)
        {
            sumA < sumC ? res[0] = AMV : res[0] = CMV;

            if (VC1ABS(AMV.x - cur.x) + VC1ABS(AMV.y - cur.y)<
                VC1ABS(CMV.x - cur.x) + VC1ABS(CMV.y - cur.y))
            {
                //hybrid = 2;
                res->x = AMV.x;
                res->y = AMV.y;
                m_pCur->HybridPredictor[0]=true;
            }
            else
            {
               // hybrid = 1;
                res->x = CMV.x;
                res->y = CMV.y;
                m_pCur->HybridPredictor[0]=true;
            }
        }
    }
}


static Ipp16s scale_sameX(Ipp16s n,MeVC1fieldScaleInfo* pInfo, bool bHalf)
{
    Ipp16s abs_n = VC1ABS(n = (n >> bHalf));
    Ipp32s s;
    if (abs_n>255)
    {
        return n << bHalf;
    }
    else if (abs_n<pInfo->ME_ScaleZoneX)
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->ME_ScaleSame1))>>8);
    }
    else
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->ME_ScaleSame2))>>8);
        s = (n<0)? s - pInfo->ME_ZoneOffsetX:s + pInfo->ME_ZoneOffsetX;
    }
    s = (s>pInfo->ME_RangeX-1)? pInfo->ME_RangeX-1:s;
    s = (s<-pInfo->ME_RangeX) ? -pInfo->ME_RangeX :s;

    return (Ipp16s) (s << bHalf);
}
static Ipp16s scale_oppX(Ipp16s n,MeVC1fieldScaleInfo* pInfo, bool bHalf)
{
    Ipp32s s = (((Ipp32s)((n >> bHalf)*pInfo->ME_ScaleOpp))>>8);
    return (Ipp16s) (s << bHalf);
}
/*
static Ipp16s scale_sameY(Ipp16s n,MeVC1fieldScaleInfo* pInfo, bool bHalf)
{
    Ipp16s abs_n = VC1ABS(n = (n >> bHalf));
    Ipp32s s     = 0;
    if (abs_n>63)
    {
        return n << bHalf;
    }
    else if (abs_n<pInfo->ME_ScaleZoneY)
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->ME_ScaleSame1))>>8);
    }
    else
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->ME_ScaleSame2))>>8);
        s = (n<0)? s - pInfo->ME_ZoneOffsetY:s + pInfo->ME_ZoneOffsetY;
    }
    if (pInfo->ME_Bottom)
    {
        s = (s>pInfo->ME_RangeY/2)? pInfo->ME_RangeY/2:s;
        s = (s<-pInfo->ME_RangeY/2+1) ? -pInfo->ME_RangeY/2+1 :s;
    }
    else
    {
        s = (s>pInfo->ME_RangeY/2-1)? pInfo->ME_RangeY/2-1:s;
        s = (s<-pInfo->ME_RangeY/2) ? -pInfo->ME_RangeY/2 :s;
    }
    return (Ipp16s) (s << bHalf);
}
*/

static Ipp16s scale_sameY(Ipp16s n,MeVC1fieldScaleInfo* pInfo, bool bHalf)
{
    Ipp16s abs_n = VC1ABS(n = (n>>((Ipp16u)bHalf)));
    Ipp32s s     = 0;
    if (abs_n>63)
    {
        return n<<((Ipp16u)bHalf);
    }

    else if (abs_n<pInfo->ME_ScaleZoneY)
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->ME_ScaleSame1))>>8);
    }
    else
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->ME_ScaleSame2))>>8);
        s = (n<0)? s - pInfo->ME_ZoneOffsetY:s + pInfo->ME_ZoneOffsetY;
    }

    s = (s>pInfo->ME_RangeY/2-1)? pInfo->ME_RangeY/2-1:s;
    s = (s<-pInfo->ME_RangeY/2) ? -pInfo->ME_RangeY/2 :s;

    return (Ipp16s) (s<<((Ipp16u)bHalf));
}

static Ipp16s scale_sameY_B(Ipp16s n,MeVC1fieldScaleInfo* pInfo, bool bHalf)
{
    Ipp16s abs_n = VC1ABS(n = (n>>bHalf));
    Ipp32s s     = 0;
    if (abs_n>63)
    {
        return n<<bHalf;
    }
    else if (abs_n<pInfo->ME_ScaleZoneY)
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->ME_ScaleSame1))>>8);
    }
    else
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->ME_ScaleSame2))>>8);
        s = (n<0)? s - pInfo->ME_ZoneOffsetY:s + pInfo->ME_ZoneOffsetY;
    }
    if (pInfo->ME_Bottom)
    {
        s = (s>pInfo->ME_RangeY/2)? pInfo->ME_RangeY/2:s;
        s = (s<-pInfo->ME_RangeY/2+1) ? -pInfo->ME_RangeY/2+1 :s;
    }
    else
    {
        s = (s>pInfo->ME_RangeY/2-1)? pInfo->ME_RangeY/2-1:s;
        s = (s<-pInfo->ME_RangeY/2) ? -pInfo->ME_RangeY/2 :s;
    }
    return (Ipp16s) (s<<bHalf);
}

static Ipp16s scale_oppY(Ipp16s n,MeVC1fieldScaleInfo* pInfo,  bool bHalf)
{
    Ipp32s s = (((Ipp32s)((n >> bHalf)*pInfo->ME_ScaleOpp))>>8);
    return (Ipp16s) (s << bHalf);

}
typedef Ipp16s (*fScaleX)(Ipp16s n, MeVC1fieldScaleInfo* pInfo, bool bHalf);
typedef Ipp16s (*fScaleY)(Ipp16s n, MeVC1fieldScaleInfo* pInfo, bool bHalf);

static fScaleX pScaleX[2][2] = {{scale_oppX, scale_sameX},
                                {scale_sameX,scale_oppX}};
static fScaleY pScaleY[2][2] = {{scale_oppY, scale_sameY},
                                {scale_sameY_B, scale_oppY}};

Ipp32s MePredictCalculatorVC1::GetPredictorVC1Field2Hybrid(int index, int isBkw, MeMV* res)
{
    Ipp32s rs;
    MeMV A[2];
    MeMV B[2];
    MeMV C[2];

    switch(index)
    {
    case ME_Block0:
        GetBlockVectorsABCField_0(isBkw);
        break;
    case ME_Block1:
        GetBlockVectorsABCField_1(isBkw);
        break;
    case ME_Block2:
        GetBlockVectorsABCField_2(isBkw);
        break;
    case ME_Block3:
        GetBlockVectorsABCField_3(isBkw);
        break;
    case ME_Macroblock:
        GetMacroBlockVectorsABCField(isBkw);
        break;
    default:
        VM_ASSERT(false);
        break;
    }

    Ipp32s pIdx = index == ME_Macroblock ? 0:index;

    m_AMV[isBkw][pIdx][0] = MeMV(0);
    m_BMV[isBkw][pIdx][0] = MeMV(0);
    m_CMV[isBkw][pIdx][0] = MeMV(0);

    m_AMV[isBkw][pIdx][1] = MeMV(0);
    m_BMV[isBkw][pIdx][1] = MeMV(0);
    m_CMV[isBkw][pIdx][1] = MeMV(0);

    bool bBackwardFirst = (isBkw && !m_pMeParams->bSecondField);
    bool bHalf = (m_pMeParams->PixelType == ME_HalfPixel);
    Ipp8u n = ((isAOutOfBound)<<2) + ((isBOutOfBound)<<1) + (isCOutOfBound);

    switch (n)
    {
    case 7: //111
        {
            res[0].x=0;
            res[0].y=0;
            res[1].x=0;
            res[1].y=0;
            rs =  0; //opposite field
            break;
        }
    case 6: //110
        {
            bool  bSame = CMVbSecond;
            res[CMVbSecond].x  =  CMV.x;
            res[CMVbSecond].y  =  CMV.y;
            m_CMV[isBkw][pIdx][!CMVbSecond].x = res[!CMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](CMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_CMV[isBkw][pIdx][!CMVbSecond].y = res[!CMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](CMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_CMV[isBkw][pIdx][CMVbSecond]  = CMV;
            rs = CMVbSecond == true? 1:0;
            break;
        }
    case 5: //101
        {
            bool  bSame = BMVbSecond;
            res[BMVbSecond].x  =  BMV.x;
            res[BMVbSecond].y  =  BMV.y;
            m_BMV[isBkw][pIdx][!BMVbSecond].x = res[!BMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](BMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_BMV[isBkw][pIdx][!BMVbSecond].y = res[!BMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](BMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_BMV[isBkw][pIdx][BMVbSecond]  = BMV;
            rs = BMVbSecond == true? 1:0;
            break;
        }
    case 3: //011
        {
            bool  bSame = AMVbSecond;
            res[AMVbSecond].x  =  AMV.x;
            res[AMVbSecond].y  =  AMV.y;
            m_AMV[isBkw][pIdx][!BMVbSecond].x = res[!AMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](AMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_AMV[isBkw][pIdx][!BMVbSecond].y = res[!AMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](AMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_AMV[isBkw][pIdx][BMVbSecond]  = AMV;
            rs = AMVbSecond==true? 1:0;
            break;
        }
    case 4: //100
        {
            bool  bSame = CMVbSecond;

            C[CMVbSecond].x  =  CMV.x;
            C[CMVbSecond].y  =  CMV.y;
            C[!CMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](CMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            C[!CMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](CMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = BMVbSecond;
            B[BMVbSecond].x  =  BMV.x;
            B[BMVbSecond].y  =  BMV.y;
            B[!BMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](BMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            B[!BMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](BMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            res[0].x = median3(C[0].x,B[0].x,0);
            res[1].x = median3(C[1].x,B[1].x,0);
            res[0].y = median3(C[0].y,B[0].y,0);
            res[1].y = median3(C[1].y,B[1].y,0);
            m_BMV[isBkw][pIdx][0] = B[0];
            m_BMV[isBkw][pIdx][1] = B[1];
            m_CMV[isBkw][pIdx][0] = C[0];
            m_CMV[isBkw][pIdx][1] = C[1];
            rs = (BMVbSecond != CMVbSecond)? 0 : (CMVbSecond == true? 1:0);
            break;
        }
    case 2: //010
        {
            bool  bSame = CMVbSecond;

            C[CMVbSecond].x  =  CMV.x;
            C[CMVbSecond].y  =  CMV.y;
            C[!CMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](CMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            C[!CMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](CMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = AMVbSecond ;
            A[AMVbSecond].x  =  AMV.x;
            A[AMVbSecond].y  =  AMV.y;
            A[!AMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](AMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            A[!AMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](AMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            res[0].x = median3(C[0].x,A[0].x,0);
            res[1].x = median3(C[1].x,A[1].x,0);
            res[0].y = median3(C[0].y,A[0].y,0);
            res[1].y = median3(C[1].y,A[1].y,0);
            m_AMV[isBkw][pIdx][0] = A[0];
            m_AMV[isBkw][pIdx][1] = A[1];
            m_CMV[isBkw][pIdx][0] = C[0];
            m_CMV[isBkw][pIdx][1] = C[1];
            rs = (AMVbSecond != CMVbSecond)? 0 : (AMVbSecond==true? 1:0);
            break;
        }
    case 1: //001
        {
            bool  bSame = BMVbSecond;

            B[BMVbSecond].x  =  BMV.x;
            B[BMVbSecond].y  =  BMV.y;
            B[!BMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](BMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            B[!BMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](BMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = AMVbSecond;
            A[AMVbSecond].x  =  AMV.x;
            A[AMVbSecond].y  =  AMV.y;
            A[!AMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](AMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            A[!AMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](AMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            res[0].x = median3(B[0].x,A[0].x,0);
            res[1].x = median3(B[1].x,A[1].x,0);
            res[0].y = median3(B[0].y,A[0].y,0);
            res[1].y = median3(B[1].y,A[1].y,0);
            m_AMV[isBkw][pIdx][0] = A[0];
            m_AMV[isBkw][pIdx][1] = A[1];
            m_BMV[isBkw][pIdx][0] = B[0];
            m_BMV[isBkw][pIdx][1] = B[1];
            rs = (AMVbSecond != BMVbSecond)? 0 : (BMVbSecond==true? 1:0);
            break;
        }
    case 0:
        {
            bool  bSame = CMVbSecond;

            C[CMVbSecond].x  =  CMV.x;
            C[CMVbSecond].y  =  CMV.y;
            C[!CMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](CMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            C[!CMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](CMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = BMVbSecond;

            B[BMVbSecond].x  =  BMV.x;
            B[BMVbSecond].y  =  BMV.y;
            B[!BMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](BMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            B[!BMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](BMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = AMVbSecond;

            A[AMVbSecond].x  =  AMV.x;
            A[AMVbSecond].y  =  AMV.y;
            A[!AMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](AMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            A[!AMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](AMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            res[0].x = median3(B[0].x,A[0].x,C[0].x);
            res[1].x = median3(B[1].x,A[1].x,C[1].x);
            res[0].y = median3(B[0].y,A[0].y,C[0].y);
            res[1].y = median3(B[1].y,A[1].y,C[1].y);
            m_AMV[isBkw][pIdx][0] = A[0];
            m_AMV[isBkw][pIdx][1] = A[1];
            m_BMV[isBkw][pIdx][0] = B[0];
            m_BMV[isBkw][pIdx][1] = B[1];
            m_CMV[isBkw][pIdx][0] = C[0];
            m_CMV[isBkw][pIdx][1] = C[1];

            rs = (AMVbSecond + BMVbSecond + CMVbSecond > 1)? 1 : 0;
            break;
        }
    default:
        {
            VM_ASSERT(0);
        }
    }
    m_pCur->HybridPredictor[0]=false;
    m_pCur->HybridPredictor[1]=false;

    if (!isAOutOfBound && !isCOutOfBound)
    {
       // Ipp32u th = m_par->PixelType ==  ME_HalfPixel ? 16:32;
        Ipp32u th = 32;
        //0 field
        MeMV  mvCPred = C[0];
        MeMV  mvAPred = A[0];

        Ipp32u sumA = VC1ABS(mvAPred.x - res[0].x) + VC1ABS(mvAPred.y - res[0].y);
        Ipp32u sumC = VC1ABS(mvCPred.x - res[0].x) + VC1ABS(mvCPred.y - res[0].y);
        if (sumA > th)
        {
            res[0].x = mvAPred.x;
            res[0].y = mvAPred.y;
            m_pCur->HybridPredictor[0]=true;
        }
        else if (sumC > th)
        {
            res[0].x = mvCPred.x;
            res[0].y = mvCPred.y;
            m_pCur->HybridPredictor[0]=true;
        }

        //1 field
        mvCPred = C[1];
        mvAPred = A[1];

        sumA = VC1ABS(mvAPred.x - res[1].x) + VC1ABS(mvAPred.y - res[1].y);
        sumC = VC1ABS(mvCPred.x - res[1].x) + VC1ABS(mvCPred.y - res[1].y);
        if (sumA > th)
        {
            res[1].x = mvAPred.x;
            res[1].y = mvAPred.y;
            m_pCur->HybridPredictor[1]=true;
        }
        else if (sumC > th)
        {
            res[1].x = mvCPred.x;
            res[1].y = mvCPred.y;
            m_pCur->HybridPredictor[1]=true;
        }
    }//if (!isAOutOfBound && !isCOutOfBound)
    return rs;
}

Ipp32s MePredictCalculatorVC1::GetPredictorVC1Field2(int index, int isBkw, MeMV* res)
{
    Ipp32s rs;
    switch(index)
    {
    case ME_Block0:
        GetBlockVectorsABCField_0(isBkw);
        break;
    case ME_Block1:
        GetBlockVectorsABCField_1(isBkw);
        break;
    case ME_Block2:
        GetBlockVectorsABCField_2(isBkw);
        break;
    case ME_Block3:
        GetBlockVectorsABCField_3(isBkw);
        break;
    case ME_Macroblock:
        GetMacroBlockVectorsABCField(isBkw);
        break;
    default:
        VM_ASSERT(false);
        break;
    }

    Ipp32s pIdx = index == ME_Macroblock ? 0:index;

    m_AMV[isBkw][pIdx][0] = MeMV(0);
    m_BMV[isBkw][pIdx][0] = MeMV(0);
    m_CMV[isBkw][pIdx][0] = MeMV(0);

    m_AMV[isBkw][pIdx][1] = MeMV(0);
    m_BMV[isBkw][pIdx][1] = MeMV(0);
    m_CMV[isBkw][pIdx][1] = MeMV(0);

    bool bBackwardFirst = (isBkw && !m_pMeParams->bSecondField);
    bool bHalf = (m_pMeParams->PixelType == ME_HalfPixel);
    Ipp8u n = ((isAOutOfBound)<<2) + ((isBOutOfBound)<<1) + (isCOutOfBound);

    m_pCur->HybridPredictor[0]=false;
    m_pCur->HybridPredictor[1]=false;

    switch (n)
    {
    case 7: //111
        {
            res[0].x=0;
            res[0].y=0;
            res[1].x=0;
            res[1].y=0;
            rs =  0; //opposite field
            break;
        }
    case 6: //110
        {
            bool  bSame = CMVbSecond;
            res[CMVbSecond].x  =  CMV.x;
            res[CMVbSecond].y  =  CMV.y;
            m_CMV[isBkw][pIdx][!CMVbSecond].x = res[!CMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](CMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_CMV[isBkw][pIdx][!CMVbSecond].y = res[!CMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](CMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_CMV[isBkw][pIdx][CMVbSecond]  = CMV;
            rs = CMVbSecond == true? 1:0;
            break;
        }
    case 5: //101
        {
            MeMV B;
            bool  bSame = BMVbSecond;
            res[BMVbSecond].x  =  BMV.x;
            res[BMVbSecond].y  =  BMV.y;
            B.x = res[!BMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](BMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            B.y = res[!BMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](BMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_BMV[isBkw][pIdx][BMVbSecond]  = BMV;
            m_BMV[isBkw][pIdx][!BMVbSecond] = B;
            rs = BMVbSecond == true? 1:0;
            break;
        }
    case 3: //011
        {
            bool  bSame = AMVbSecond;
            res[AMVbSecond].x  =  AMV.x;
            res[AMVbSecond].y  =  AMV.y;
            m_AMV[isBkw][pIdx][!AMVbSecond].x = res[!AMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](AMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_AMV[isBkw][pIdx][!AMVbSecond].y = res[!AMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](AMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            m_AMV[isBkw][pIdx][AMVbSecond]  = AMV;
            rs = AMVbSecond==true? 1:0;
            break;
        }
    case 4: //100
        {
            MeMV B[2];
            MeMV C[2];
            bool  bSame = CMVbSecond;

            C[CMVbSecond].x  =  CMV.x;
            C[CMVbSecond].y  =  CMV.y;
            C[!CMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](CMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            C[!CMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](CMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = BMVbSecond;
            B[BMVbSecond].x  =  BMV.x;
            B[BMVbSecond].y  =  BMV.y;
            B[!BMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](BMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            B[!BMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](BMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            res[0].x = median3(C[0].x,B[0].x,0);
            res[1].x = median3(C[1].x,B[1].x,0);
            res[0].y = median3(C[0].y,B[0].y,0);
            res[1].y = median3(C[1].y,B[1].y,0);
            m_BMV[isBkw][pIdx][0] = B[0];
            m_BMV[isBkw][pIdx][1] = B[1];
            m_CMV[isBkw][pIdx][0] = C[0];
            m_CMV[isBkw][pIdx][1] = C[1];
            rs = (BMVbSecond != CMVbSecond)? 0 : (CMVbSecond==true? 1:0);
            break;
        }
    case 2: //010
        {
            MeMV A[2];
            MeMV C[2];
            bool  bSame = CMVbSecond;

            C[CMVbSecond].x  =  CMV.x;
            C[CMVbSecond].y  =  CMV.y;
            C[!CMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](CMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            C[!CMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](CMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = AMVbSecond ;
            A[AMVbSecond].x  =  AMV.x;
            A[AMVbSecond].y  =  AMV.y;
            A[!AMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](AMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            A[!AMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](AMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            res[0].x = median3(C[0].x,A[0].x,0);
            res[1].x = median3(C[1].x,A[1].x,0);
            res[0].y = median3(C[0].y,A[0].y,0);
            res[1].y = median3(C[1].y,A[1].y,0);
            m_AMV[isBkw][pIdx][0] = A[0];
            m_AMV[isBkw][pIdx][1] = A[1];
            m_CMV[isBkw][pIdx][0] = C[0];
            m_CMV[isBkw][pIdx][1] = C[1];
            rs = (AMVbSecond != CMVbSecond)? 0 : (AMVbSecond == true? 1:0);
            break;
        }
    case 1: //001
        {
            MeMV A[2];
            MeMV B[2];
            bool  bSame = BMVbSecond;

            B[BMVbSecond].x  =  BMV.x;
            B[BMVbSecond].y  =  BMV.y;
            B[!BMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](BMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            B[!BMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](BMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = AMVbSecond;
            A[AMVbSecond].x  =  AMV.x;
            A[AMVbSecond].y  =  AMV.y;
            A[!AMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](AMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            A[!AMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](AMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            res[0].x = median3(B[0].x,A[0].x,0);
            res[1].x = median3(B[1].x,A[1].x,0);
            res[0].y = median3(B[0].y,A[0].y,0);
            res[1].y = median3(B[1].y,A[1].y,0);
            m_AMV[isBkw][pIdx][0] = A[0];
            m_AMV[isBkw][pIdx][1] = A[1];
            m_BMV[isBkw][pIdx][0] = B[0];
            m_BMV[isBkw][pIdx][1] = B[1];
            rs = (AMVbSecond != BMVbSecond)? 0 : (BMVbSecond==true? 1:0);
            break;
        }
    case 0:
        {
            MeMV A[2];
            MeMV B[2];
            MeMV C[2];

            bool  bSame = CMVbSecond;

            C[CMVbSecond].x  =  CMV.x;
            C[CMVbSecond].y  =  CMV.y;
            C[!CMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](CMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            C[!CMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](CMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = BMVbSecond;

            B[BMVbSecond].x  =  BMV.x;
            B[BMVbSecond].y  =  BMV.y;
            B[!BMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](BMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            B[!BMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](BMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            bSame = AMVbSecond;

            A[AMVbSecond].x  =  AMV.x;
            A[AMVbSecond].y  =  AMV.y;
            A[!AMVbSecond].x =  pScaleX[bBackwardFirst][!bSame](AMV.x,&m_pMeParams->ScaleInfo[isBkw], bHalf);
            A[!AMVbSecond].y =  pScaleY[bBackwardFirst][!bSame](AMV.y,&m_pMeParams->ScaleInfo[isBkw], bHalf);

            res[0].x = median3(B[0].x,A[0].x,C[0].x);
            res[1].x = median3(B[1].x,A[1].x,C[1].x);
            res[0].y = median3(B[0].y,A[0].y,C[0].y);
            res[1].y = median3(B[1].y,A[1].y,C[1].y);
            m_AMV[isBkw][pIdx][0] = A[0];
            m_AMV[isBkw][pIdx][1] = A[1];
            m_BMV[isBkw][pIdx][0] = B[0];
            m_BMV[isBkw][pIdx][1] = B[1];
            m_CMV[isBkw][pIdx][0] = C[0];
            m_CMV[isBkw][pIdx][1] = C[1];

            rs = (AMVbSecond + BMVbSecond + CMVbSecond > 1)? 1 : 0;
            break;
        }
    default:
        {
            VM_ASSERT(0);
        }
    }
    return rs;
}

Ipp32s MePredictCalculatorVC1::GetPredictorVC1Field1(int index, int isBkw, MeMV* res)
{
    switch(index)
    {
    case ME_Block0:
        GetBlockVectorsABCField_0(isBkw);
        break;
    case ME_Block1:
        GetBlockVectorsABCField_1(isBkw);
        break;
    case ME_Block2:
        GetBlockVectorsABCField_2(isBkw);
        break;
    case ME_Block3:
        GetBlockVectorsABCField_3(isBkw);
        break;
    case ME_Macroblock:
        GetMacroBlockVectorsABCField(isBkw);
        break;
    default:
        VM_ASSERT(false);
        break;
    }

    m_pCur->HybridPredictor[0]=false;
    m_pCur->HybridPredictor[1]=false;

    Ipp8u n = ((isAOutOfBound)<<2) + ((isBOutOfBound)<<1) + (isCOutOfBound);
    switch (n)
    {
    case 7: //111
        {
            res[0].x = 0;
            res[0].y = 0;
            break;
        }
    case 6: //110
        {
            res[0].x  =  CMV.x;
            res[0].y  =  CMV.y;
            break;
        }
    case 5: //101
        {
            res[0].x  =  BMV.x;
            res[0].y  =  BMV.y;
            break;
        }
    case 3: //011
        {
            res[0].x  =  AMV.x;
            res[0].y  =  AMV.y;
            break;
        }
    case 4: //100
        {
            res[0].x = median3(CMV.x,BMV.x,0);
            res[0].y = median3(CMV.y,BMV.y,0);
            break;
        }
    case 2: //010
        {
            res[0].x = median3(CMV.x,AMV.x,0);
            res[0].y = median3(CMV.y,AMV.y,0);
            break;
        }
    case 1: //001
        {
            res[0].x = median3(BMV.x,AMV.x,0);
            res[0].y = median3(BMV.y,AMV.y,0);
            break;
        }
    case 0:
        {
            res[0].x = median3(BMV.x,AMV.x,CMV.x);
            res[0].y = median3(BMV.y,AMV.y,CMV.y);
            break;
        }
    default:
        {
            VM_ASSERT(0);
        }
    }
    Ipp32s pIdx = index == ME_Macroblock ? 0:index;

    m_AMV[isBkw][pIdx][0] = AMV;
    m_BMV[isBkw][pIdx][0] = BMV;
    m_CMV[isBkw][pIdx][0] = CMV;

    if (!isAOutOfBound && !isCOutOfBound)
    {
        Ipp32u th = 32;
        if(m_pMeParams->SearchDirection == ME_BidirSearch)
            th = (m_pMeParams->PixelType ==  ME_HalfPixel) ? 16:32;
        //0 field
        Ipp32u sumA = VC1ABS(m_AMV[isBkw][pIdx][0].x - res[0].x) + VC1ABS(m_AMV[isBkw][pIdx][0].y - res[0].y);
        Ipp32u sumC = VC1ABS(m_CMV[isBkw][pIdx][0].x - res[0].x) + VC1ABS(m_CMV[isBkw][pIdx][0].y - res[0].y);
        if (sumA > th)
        {
            res[0].x = m_AMV[isBkw][pIdx][0].x;
            res[0].y = m_AMV[isBkw][pIdx][0].y;
            m_pCur->HybridPredictor[0]=true;
        }
        else if (sumC > th)
        {
            res[0].x = m_CMV[isBkw][pIdx][0].x;
            res[0].y = m_CMV[isBkw][pIdx][0].y;
            m_pCur->HybridPredictor[0]=true;
        }
    }
    return 0;
}


Ipp32s MePredictCalculatorVC1::GetPredictorMPEG2(int index,int isBkw, MeMV* res)
{
    if (MbLeftEnable)
    {
        if(isBkw != 0) res[0] = m_pRes[m_pCur->adr-1].MV[1][0];
        res[0] = m_pRes[m_pCur->adr-1].MV[0][0];
    }
    else
    {
        res[0] = MeMV(0);
    }
    return 0;
}

}//namespace UMC
#endif