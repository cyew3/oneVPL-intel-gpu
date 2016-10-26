//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include <umc_defs.h>
#ifdef UMC_ENABLE_ME

#include "umc_vec_prediction.h"
#include "umc_me.h"

namespace UMC
{
inline
Ipp32s ScaleValue(Ipp32s value, Ipp32s blockDist, Ipp32s otherBlockDist)
{
    Ipp32s signExtended = value >> 31;

    // get the absolute value
    value = (value ^ signExtended) - signExtended;

    // scale value
    value = ((value * blockDist) * (512 / otherBlockDist) + 256) >> 9;

    // restore the sign of the value
    value = (value + signExtended) ^ signExtended;

    return value;

} // Ipp32s ScaleValue(Ipp32s value, Ipp32s blockDist, Ipp32s otherBlockDist)

#define AvsAbs(value) \
    ((0 < (value)) ? (value) : (-(value)))

inline
Ipp32s VectorDistance(MeMV mvOne, MeMV mvTwo)
{
    Ipp32s dist;

    dist = AvsAbs(mvOne.x - mvTwo.x) +
           AvsAbs(mvOne.y - mvTwo.y);

    return dist;

} // Ipp32s VectorDistance(AVSMVector mvOne, AVSMVector mvTwo)

// Some MeAVS methods

bool MeAVS::EstimateFrame(MeParams *par)
{
    //1 workaround
    par->ChromaInterpolation=ME_VC1_Bilinear;
    //1 end of workaround

    //check input params, init some variables
    m_par = par;
    if(!CheckParams())
        return false;

    //m_SkippedThreshold = 150*m_par->Quant/4;
    //for AVS
    m_SkippedThreshold/=5;

    DownsampleFrames();

    //init feedback
    LoadRegressionPreset();

    //init auxiliary classes
    m_PredCalc->Init(par, &m_cur);
    MFX_INTERNAL_CPY(m_PredCalc->m_blockDist,m_blockDist,sizeof(m_blockDist)); 
    MFX_INTERNAL_CPY(m_PredCalc->m_distIdx,m_distIdx,sizeof(m_distIdx));

    //estiamte all MB
    for(Ipp32s MbAdr = m_par->FirstMB; MbAdr <= m_par->LastMB; MbAdr++)
    {
        //reset costs
        m_cur.Reset();
        m_cur.y = MbAdr/par->pSrc->WidthMB;
        m_cur.x = MbAdr%par->pSrc->WidthMB;
        m_cur.adr = MbAdr;

        //m_par->pSrc->MBs[m_cur.adr].Reset();
        m_par->pSrc->MBs[m_cur.adr].Reset(m_par->pSrc->NumOfMVs);
        // TODO: rewrite this
        if(m_cur.adr!=0)m_par->pSrc->MBs[m_cur.adr].NumOfNZ=m_par->pSrc->MBs[m_cur.adr-1].NumOfNZ;

        //choose estimation function
        if(m_par->MbPart == ME_Mb16x16)
        {
            EstimateMB16x16();
        }
        else if(m_par->MbPart == (ME_Mb8x8 | ME_Mb16x16))
        {
            EstimateMB8x8();
        }
        else
        {
            SetError((vm_char*)"Unsupported partitioning");
        }
    }

    return true;
}

bool MeAVS::EstimateSkip()
{
    if(!IsSkipEnabled())
        return false;

    if(m_cur.MbPart == ME_Mb16x16)
    {
        return EstimateSkip16x16();
    }

    if(m_cur.MbPart == ME_Mb8x8)
    {
        if(m_cur.BlkIdx > 0)
            return false;
        return EstimateSkip8x8();
    }
    return false;
}// bool MeAVS::EstimateSkip()

bool MeAVS::EstimateSkip16x16()
{
    Ipp32s i, j;
    Ipp32s tmpCost;
    bool OutBoundF = false;
    bool OutBoundB = false;
    // P_Skip  & B_Skip
    // Note!!! In the AVS predictions are differ for P_Skip and B_Skip Macro Blocks
    // Prediction for B_Skip and for B_Direct are the same
    for(i=0; i< m_par->FRefFramesNum; i++)
    {
        // for P_Skip
        if(m_par->SearchDirection == ME_ForwardSearch)
        {
            SetReferenceFrame(frw,i);
            MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
            //AVS style: type 1 is forward
            mb->predType[0] = mb->predType[1] = mb->predType[2] = mb->predType[3] = 1;
            //m_cur.PredMV[frw][i][m_cur.BlkIdx]=m_PredCalc->GetPrediction16x16(true);
            //m_cur.PredMV[frw][i][m_cur.BlkIdx] = m_PredCalc->GetSkipPrediction();
            m_cur.PredMV[frw][i][m_cur.BlkIdx] = m_PredCalcAVS.GetSkipPrediction();
            tmpCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[frw][i][m_cur.BlkIdx]);
            if(m_cur.SkipCost[0] > tmpCost)
            {
                m_cur.SkipCost[0]     = tmpCost;
                m_cur.SkipType[0]     = ME_MbFrwSkipped;
                m_cur.SkipIdx[frw][0] = i;
            }
        }
        //OutBoundF = (tmpCost == ME_BIG_COST ? true: false);
        //if(m_cur.adr==ME_MB_ADR)printf("skip FRW cost=%d\n", m_cur.SkipCost[0]);

        //B Skip and Direct
        if(m_par->SearchDirection == ME_BidirSearch){
            for(j=0; j<m_par->BRefFramesNum; j++){

                SetReferenceFrame(bkw,j);
                //AVS style: type is backward
                //m_cur.PredMV[bkw][j][m_cur.BlkIdx] = m_PredCalc->GetPrediction();
                //tmpCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[bkw][j][0]);
                ////if(m_cur.adr==ME_MB_ADR)printf("skip BKW cost=%d\n", tmpCost);
                //if(tmpCost<m_cur.SkipCost[m_cur.BlkIdx]){
                //    m_cur.SkipCost[0]     = tmpCost;
                //    m_cur.SkipType[0]     = ME_MbBkwSkipped;
                //    m_cur.SkipIdx[bkw][0] = j;
                //}
                //OutBoundB = (tmpCost == ME_BIG_COST ? true: false);
                //AVS style: type 3 is bidir
                MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
                mb->predType[0] = mb->predType[1] = mb->predType[2] = mb->predType[3] = 3;
                m_PredCalcAVS.ReconstructMotionVectorsBSliceDirect(  &m_cur.PredMV[frw][i][m_cur.BlkIdx],
                                                       &m_cur.PredMV[bkw][i][m_cur.BlkIdx]);

                if(!OutBoundF && !OutBoundB)
                {
                    tmpCost = EstimatePointAverage(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, frw, 0, m_cur.PredMV[frw][i][0], bkw, 0, m_cur.PredMV[bkw][j][0]);
                    tmpCost +=32; // ???
                    //if(m_cur.adr==ME_MB_ADR)printf("skip BIDIR cost=%d\n", tmpCost);
                    if(tmpCost<m_cur.SkipCost[0]){
                        m_cur.SkipCost[0]     = tmpCost;
                        m_cur.SkipType[0]     = ME_MbBidirSkipped;
                        m_cur.SkipIdx[bkw][0] = j;
                    }
                }
            }//for(j)
        }//B
    }//for(i)
    return EarlyExitForSkip();
}// bool MeAVS::EstimateSkip16x16()

bool MeAVS::EstimateSkip8x8()
{
    Ipp32s i;
    Ipp32s tmpCostF[ME_NUM_OF_BLOCKS];
    Ipp32s tmpCostB[ME_NUM_OF_BLOCKS];
    Ipp32s tmpIdxF[ME_NUM_OF_BLOCKS];
    Ipp32s tmpIdxB[ME_NUM_OF_BLOCKS];
    Ipp32s tmpCost;

    for(i = 0; i < ME_NUM_OF_BLOCKS; i++)
    {
        tmpCostF[i] = tmpCostB[i] = ME_BIG_COST;
    }

    //P & B
    for(m_cur.BlkIdx=0; m_cur.BlkIdx<4; m_cur.BlkIdx++)
    {
        for(i=0; i< m_par->FRefFramesNum; i++)
        {
            SetReferenceFrame(frw,i);
            m_cur.PredMV[frw][i][m_cur.BlkIdx] = m_PredCalc->GetPrediction();
            tmpCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[frw][i][m_cur.BlkIdx]);
            if(tmpCost < tmpCostF[m_cur.BlkIdx])
            {
                m_cur.SkipCost[m_cur.BlkIdx]= tmpCost;
                tmpCostF[m_cur.BlkIdx] = tmpCost;
                tmpIdxF[m_cur.BlkIdx]  = i;
            }

        }//for(i)
        if(!EarlyExitForSkip())
        {
            for(i=0; i<ME_NUM_OF_BLOCKS; i++)
                tmpCostF[i]=ME_BIG_COST;
            break;
        }
    }//for(m_cur.BlkIdx)

    if(m_par->SearchDirection ==ME_BidirSearch)
    {
        for(m_cur.BlkIdx=0; m_cur.BlkIdx<4; m_cur.BlkIdx++)
        {
            for(i=0; i< m_par->BRefFramesNum; i++)
            {
                SetReferenceFrame(bkw,i);
                m_cur.PredMV[bkw][i][m_cur.BlkIdx] = m_PredCalc->GetPrediction();
                tmpCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[frw][i][m_cur.BlkIdx]);
                if(tmpCost < tmpCostB[m_cur.BlkIdx])
                {
                    m_cur.SkipCost[m_cur.BlkIdx]= tmpCost;
                    tmpCostB[m_cur.BlkIdx] = tmpCost;
                    tmpIdxB[m_cur.BlkIdx]  = i;
                }

            }//for(i)
            if(!EarlyExitForSkip())
            {
                for(i=0; i<ME_NUM_OF_BLOCKS; i++)
                    tmpCostB[i]=ME_BIG_COST;
                break;
            }
        }//for(m_cur.BlkIdx)
    }//if(m_par->SearchDirection == ME_BidirSearch)

    m_cur.BlkIdx = 0;

    if(tmpCostF[0] == ME_BIG_COST && tmpCostB[0] == ME_BIG_COST)
        return false;

    if(tmpCostF[0] != ME_BIG_COST && tmpCostB[0] != ME_BIG_COST)
    {
        Ipp32s sumF = tmpCostF[0] + tmpCostF[1] + tmpCostF[2] + tmpCostF[3];
        Ipp32s sumB = tmpCostB[0] + tmpCostB[1] + tmpCostB[2] + tmpCostB[3];

        if(sumF < sumB)
        {
            for(i=0; i<ME_NUM_OF_BLOCKS; i++)
            {
                m_cur.SkipCost[i]     = tmpCostF[i];
                m_cur.SkipType[i]     = ME_MbFrwSkipped;
                m_cur.SkipIdx[frw][i] = tmpIdxF[i];
            }
        }
        else
        {
            for(i=0; i<ME_NUM_OF_BLOCKS; i++)
            {
                m_cur.SkipCost[i]     = tmpCostB[i];
                m_cur.SkipType[i]     = ME_MbBkwSkipped;
                m_cur.SkipIdx[bkw][i] = tmpIdxB[i];
            }
        }
        return true;
    }

    if(tmpCostF[0] != ME_BIG_COST)
    {
        for(i=0; i<ME_NUM_OF_BLOCKS; i++)
        {
            m_cur.SkipCost[i]     = tmpCostF[i];
            m_cur.SkipType[i]     = ME_MbFrwSkipped;
            m_cur.SkipIdx[frw][i] = tmpIdxF[i];
        }
        return true;
    }

    if(tmpCostB[0] != ME_BIG_COST)
    {
        for(i=0; i<ME_NUM_OF_BLOCKS; i++)
        {
            m_cur.SkipCost[i]     = tmpCostB[i];
            m_cur.SkipType[i]     = ME_MbBkwSkipped;
            m_cur.SkipIdx[bkw][i] = tmpIdxB[i];
        }
    }

    return true;
}//bool MeAVS::EstimateSkip8x8()

void MeAVS::Estimate16x16Bidir()
{
    if(m_par->SearchDirection != ME_BidirSearch)
        return;

    if(m_par->UseFeedback)
        return;

    MeMV tmpSymVec(0);

    //find best bidir
    for(Ipp32s i=0; i< m_par->FRefFramesNum; i++){
        for(Ipp32s j=0; j<m_par->BRefFramesNum; j++){
            //check bidir
            tmpSymVec = m_PredCalcAVS.CreateSymmetricalMotionVector(0, m_cur.BestMV[frw][i][0]);
            if (m_PredCalcAVS.IsOutOfBound(ME_Mb16x16, ME_IntegerPixel, tmpSymVec) )
                continue;
            //tmpSymVec = CreateSymmetricalMotionVector(0, m_cur.PredMV[frw][i][0]);
            //Ipp32s tmpCost=EstimatePointAverage(ME_Mb16x16, m_par->PixelType, m_par->CostMetric, frw, i, m_cur.BestMV[frw][i][0], bkw, j, m_cur.BestMV[bkw][j][0]);
            Ipp32s tmpCost=EstimatePointAverage(ME_Mb16x16, m_par->PixelType,
                                                 m_par->CostMetric, frw, i,
                                                 m_cur.BestMV[frw][i][0], bkw, j,
                                                 tmpSymVec);
            tmpCost +=WeightMV(m_cur.BestMV[frw][i][0], m_cur.PredMV[frw][i][0]);
            //tmpCost +=WeightMV(m_cur.BestMV[bkw][j][0], m_cur.PredMV[bkw][j][0]);
            tmpCost +=WeightMV(m_cur.BestMV[bkw][j][0], tmpSymVec);
            tmpCost +=32; //some additional penalty for bidir type of MB
            if(tmpCost<m_cur.InterCost[0]){
                m_cur.InterCost[0] = tmpCost;
                m_cur.InterType[0] = ME_MbBidir;
                m_cur.BestIdx[frw][0] = i; 
                m_cur.BestIdx[bkw][0] = j;
                //m_cur.PredMV[bkw][j][0] = tmpSymVec;
                m_cur.BestMV[bkw][j][0] = tmpSymVec;
            }
        }
    }
} // void MeAVS::Estimate16x16Bidir()


bool MeAVS::Estimate16x16Direct()
{
    if((m_par->SearchDirection != ME_BidirSearch) ||
        (m_par->ProcessDirect == false))
        return false;

    MeMV MVDirectFW, MVDirectBW;
    Ipp32s IdxF = 0, IdxB = 0;

    m_PredCalcAVS.ReconstructMotionVectorsBSliceDirect(&MVDirectFW, &MVDirectBW);
    //for(IdxF = 0; IdxF < m_par->FRefFramesNum; IdxF++)
    //{
    //    if(!m_par->pRefF[IdxF]->MVDirect[m_cur.adr].IsInvalid())
    //    {
    //        MVDirectFW = m_par->pRefF[IdxF]->MVDirect[m_cur.adr];
    //        break;
    //    }
    //}

    //for(IdxB = 0; IdxB < m_par->BRefFramesNum; IdxB++)
    //{
    //    if(!m_par->pRefB[IdxB]->MVDirect[m_cur.adr].IsInvalid())
    //    {
    //        MVDirectBW = m_par->pRefB[IdxB]->MVDirect[m_cur.adr];
    //        break;
    //    }
    //}

    //if(IdxF == m_par->FRefFramesNum || IdxB == m_par->BRefFramesNum)
    //    SetError("Wrong MVDirect in MeBase::Estimate16x16Direct");

    //check direct
    Ipp32s tmpCost=EstimatePointAverage(ME_Mb16x16, m_par->PixelType, m_par->CostMetric,
                                        frw, IdxF, MVDirectFW,
                                        bkw, IdxB, MVDirectBW);
    m_cur.DirectCost     = tmpCost;
    m_cur.DirectType     = ME_MbDirect;
    m_cur.DirectIdx[frw] = IdxF;
    m_cur.DirectIdx[bkw] = IdxB;

    if(m_cur.DirectCost<m_SkippedThreshold){
        m_cur.DirectType= ME_MbDirectSkipped;
        return true;
    }

    return false;
} // bool MeAVS::Estimate16x16Direct()

void MeAVS::ProcessFeedback(MeParams *pPar)
{
    //skip I frames
    //if ((pPar->pSrc->type == ME_FrmIntra) ||
    //    (pPar->pRefF[0]->type == ME_FrmIntra))
    //    return;

    // swap pPar->pSrc and pPar->pSrc pPar->pRefF[0]
    MeFrame *tmpFrame = pPar->pSrc;
    pPar->pSrc = pPar->pRefF[0];
    pPar->pRefF[0] = tmpFrame;

    // process feedback
    MeBase::ProcessFeedback(pPar);

    // swap again, to normal order
    tmpFrame = pPar->pSrc;
    pPar->pSrc = pPar->pRefF[0];
    pPar->pRefF[0] = tmpFrame;

} // void MeAVS::ProcessFeedback(MeParams *pPar)

MeMV MeAVS::GetChromaMV (MeMV mv)
{
    MeMV cmv;
    if(m_par->FastChroma)
        GetChromaMVFast(mv, &cmv);
    else
        GetChromaMV (mv, &cmv);
    
    return cmv;
} // MeMV MeAVS::GetChromaMV (MeMV mv)

void MeAVS::GetChromaMV(MeMV LumaMV, MeMV* pChroma)
{
    static Ipp16s round[4]= {0,0,0,1};

    pChroma->x = (LumaMV.x + round[LumaMV.x&0x03])>>1;
    pChroma->y = (LumaMV.y + round[LumaMV.y&0x03])>>1;
} // void MeAVS::GetChromaMV(MeMV LumaMV, MeMV* pChroma)

void MeAVS::GetChromaMVFast(MeMV LumaMV, MeMV* pChroma)
{
    static Ipp16s round [4]= {0,0,0,1};
    static Ipp16s round1[2][2] = {
        {0, -1}, //sign = 0;
        {0,  1}  //sign = 1
    };

    pChroma->x = (LumaMV.x + round[LumaMV.x&0x03])>>1;
    pChroma->y = (LumaMV.y + round[LumaMV.y&0x03])>>1;
    pChroma->x = pChroma->x + round1[pChroma->x < 0][pChroma->x & 1];
    pChroma->y = pChroma->y + round1[pChroma->y < 0][pChroma->y & 1];
} // void MeAVS::GetChromaMV(MeMV LumaMV, MeMV* pChroma)

// functions GetExpGolombCodeSize and GetSESize
// need for calculation size of mv difference 
inline
Ipp32u GetExpGolombCodeSize(Ipp32u code)
{
    Ipp32s leadingZeroBits = 0;
    Ipp32s zeroMask = (-1 << 1);

    // update code
    code += 1;

    while (code & zeroMask)
    {
        // increment number of zeros
        leadingZeroBits += 1;
        // shift zero mask
        zeroMask <<= 1;
    }

    return leadingZeroBits * 2 + 1;

} // Ipp32u GetExpGolombCodeSize(Ipp32u code)

inline
Ipp32u GetSESize(Ipp32s element)
{
    if (element)
    {
        Ipp32u code;
        Ipp32s signExtended;

        // get element's sign
        signExtended = element >> 31;
        // we use simple scheme of abs calculation,
        // using one ADD and one XOR operation
        element = (element ^ signExtended) - signExtended;

        // create code
        code = (element << 1) - 1 - signExtended;

        // put Exp-Glolomb code
        return GetExpGolombCodeSize(code);
    }
    else
    {
        return 1;
    }

} // Ipp32u GetSESize(Ipp32s element)

Ipp32s MeAVS::GetMvSize(Ipp32s dx, Ipp32s dy, bool bNonDominant, bool hybrid)
{
    return (GetSESize(dx) + GetSESize(dy) );
}

void MeAVS::ModeDecision16x16ByFBFastFrw()
{
    SetError((vm_char*)"Wrong MbPart in MakeMBModeDecision16x16ByFB", m_par->MbPart != ME_Mb16x16);
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];

    // SKIP
    Ipp32s SkipCost = ME_BIG_COST;
    MeMV skipMv = m_PredCalcAVS.GetSkipPrediction();
    if(m_par->ProcessSkipped)
        SkipCost= (Ipp32s) EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SSD, skipMv);

    MeMV mv;
    Ipp32s InterCost, IntraCost;

        // INTER
        mv = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0];
        MeMV pred = m_cur.PredMV[frw][m_cur.BestIdx[frw][0]][0];
        mb->PureSAD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sad, mv);
        mb->InterCostRD.D = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sadt, mv);
        InterCost = (Ipp32s)( m_InterRegFunD.Weight(mb->InterCostRD.D)+m_lambda*m_MvRegrFunR.Weight(mv,pred));
        //InterCost =(Ipp32s) ( 0.7*(double)InterCost);
        // INTRA
        mb->IntraCostRD.D=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SadtSrcNoDC, 0);
        IntraCost = (Ipp32s)(m_IntraRegFunD.Weight(mb->IntraCostRD.D));
        //IntraCost =(Ipp32s) ( 0.7*(double)IntraCost);

        //for preset calculation
        #ifdef ME_GENERATE_PRESETS
            SkipCost=IPP_MAX_32S;
            InterCost  = rand()/2;
            IntraCost  = rand()/2;
        #endif

        //make decision
        Ipp32s bestCost=IPP_MIN(IPP_MIN(SkipCost,InterCost),IntraCost);
        if(bestCost==SkipCost){
            //skip
            SetMB16x16B(ME_MbFrwSkipped, skipMv, 0, m_cur.SkipCost[0]);
        }else if(bestCost == IntraCost){
            //intra
            SetMB16x16B(ME_MbIntra, 0, 0, 0xBAD);
    }else{
            //inter
            SetMB16x16B(ME_MbFrw, m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0], 0, m_cur.InterCost[0]);
            SetError((vm_char*)"Wrong decision in MakeMBModeDecision16x16ByFB", m_cur.InterCost[0]>=ME_BIG_COST);
        }
} // void MeAVS::ModeDecision16x16ByFBFastFrw()

void MeAVS::ModeDecision16x16ByFBFullFrw()
{
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];

    // SKIP
    Ipp32s SkipCost = ME_BIG_COST;
    MeMV skipMv = m_PredCalcAVS.GetSkipPrediction();
    if(m_par->ProcessSkipped)
        SkipCost=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SSD, skipMv);
        

        MeMV mv;
        Ipp32s InterCost, IntraCost;

        // INTER
        // TODO: rewrite this, currently we use previous MB value for MV weighting, should we use another?
        mv = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0];
        MeMV pred = m_PredCalc->GetPrediction();
        mb->PureSAD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sad, mv);
        if(m_par->UseVarSizeTransform){
            mb->InterCostRD = MakeTransformDecisionInter(m_cur.MbPart, m_par->PixelType, ME_MbFrw, mv, MeMV(0),0,0);
        }else{
            // TODO: replace this by EstimatePoint or MakeTransformDecisionInter
            MeAdr src, ref; src.chroma=ref.chroma=true; //true means load also chroma plane addresses
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mv, &ref);
            MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);
            mb->InterCostRD = GetCostRD(ME_InterRD,ME_Mb16x16,ME_Tranform8x8,&src,&ref);
            AddHeaderCost(mb->InterCostRD, NULL, ME_MbFrw, 0, 0);
        }
        m_par->pSrc->MBs[m_cur.adr].NumOfNZ=mb->InterCostRD.NumOfCoeff;
        InterCost = (Ipp32s)( m_InterRegFunD.Weight(mb->InterCostRD.D)+m_lambda*(m_InterRegFunR.Weight(mb->InterCostRD.R)+GetMvSize(mv.x-pred.x,mv.y-pred.y)));
        if(InterCost<0) InterCost= ME_BIG_COST;

        // INTRA
        MeAdr src; src.chroma=true;
        MakeSrcAdr(m_cur.MbPart, ME_IntegerPixel, &src);
        mb->IntraCostRD = GetCostRD(ME_IntraRD,ME_Mb16x16,ME_Tranform8x8,&src,NULL);
        AddHeaderCost(mb->IntraCostRD, NULL, ME_MbIntra, 0, 0);
        IntraCost = (Ipp32s)(m_IntraRegFunD.Weight(mb->IntraCostRD.D) + m_lambda*m_IntraRegFunR.Weight(mb->IntraCostRD.R));
        if(IntraCost<0) IntraCost= ME_BIG_COST;

        //for preset calculation
        #ifdef ME_GENERATE_PRESETS
            SkipCost=IPP_MAX_32S;
            InterCost  = rand()/2;
            IntraCost  = rand()/2;
        #endif

        //make decision
        Ipp32s bestCost=IPP_MIN(IPP_MIN(SkipCost,InterCost),IntraCost);
        if(bestCost==SkipCost){
            //skip
            SetMB16x16B(ME_MbFrwSkipped, m_cur.PredMV[frw][0][0], 0, bestCost);
        }else if(bestCost == IntraCost){
            //intra
            SetMB16x16B(ME_MbIntra, 0, 0, bestCost);
        }else{
            //inter
            SetMB16x16B(ME_MbFrw, m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0], 0, bestCost);
            SetError((vm_char*)"Wrong decision in MakeMBModeDecision16x16ByFB", m_cur.InterCost[0]>=ME_BIG_COST);
        }
} // void MeAVS::ModeDecision16x16ByFBFullFrw()

void MeAVS::SetMB16x16B(MeMbType mbt, MeMV mvF, MeMV mvB, Ipp32s cost)
{
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
    int i = 0;
    mb->MbPart = ME_Mb16x16;
    mb->McType = ME_FrameMc;
    mb->MbType = (Ipp8u)mbt;
    mb->MbCosts[0] = cost;

    if(m_cur.BestIdx[frw][0] >= 0)
        mb->MVPred[frw][0] = m_cur.PredMV[frw][m_cur.BestIdx[frw][0]][0];
    if(m_cur.BestIdx[bkw][0] >= 0)
        mb->MVPred[bkw][0] = m_cur.PredMV[bkw][m_cur.BestIdx[bkw][0]][0];

    for(i=0; i<4; i++){
        mb->MV[frw][i] = mvF;
        mb->MV[bkw][i] = mvB;
        mb->Refindex[frw][i]= m_cur.BestIdx[frw][0];
        mb->Refindex[bkw][i]= m_cur.BestIdx[bkw][0];
        mb->BlockType[i] = mb->MbType;
        // avs style
        if (ME_MbIntra == mb->MbType)
            mb->predType[i] = 0;
        else if ((ME_MbFrw == mb->MbType) || (ME_MbFrwSkipped == mb->MbType))
            mb->predType[i] = 1;
        else if ((ME_MbBkw == mb->MbType) || (ME_MbBkwSkipped == mb->MbType))
            mb->predType[i] = 2;
        else if ((ME_MbBidir == mb->MbType) || (mb->MbType >= ME_MbBidirSkipped))
            mb->predType[i] = 3;
    }

    Ipp32s cost_4 = cost/4+1;
    for(i=1; i<5; i++)
        mb->MbCosts[i] = cost_4;

    //transform size
    for(i=5; i>=0; i--){
        mb->BlockTrans<<=2;
        mb->BlockTrans|=3&m_cur.InterTransf[mb->MbType][i];
    }

    //quantized coefficients, rearrange during copy
    if(m_par->UseTrellisQuantization){
        if(mbt == ME_MbIntra){
                MFX_INTERNAL_CPY(mb->Coeff,m_cur.TrellisCoefficients[4],(6*64)*sizeof(mb->Coeff[0][0]));
        }else{
            for(Ipp32s blk=0; blk<6; blk++){
                MeTransformType tr=m_cur.InterTransf[mb->MbType][blk];
                switch(tr){
                    case ME_Tranform4x4:
                        for(Ipp32s subblk=0; subblk<4; subblk++)
                            for(Ipp32s y=0; y<4; y++)
                                for(Ipp32s x=0; x<4; x++)
                                    //MeQuantTable[m_cur.adr][intra?0:1][m_cur.BlkIdx][(32*(subblk/2))+(4*(subblk&1))+8*y+x]=buf2[16*subblk+4*y+x];
                                    mb->Coeff[blk][(32*(subblk/2))+(4*(subblk&1))+8*y+x]=m_cur.TrellisCoefficients[tr][blk][16*subblk+4*y+x];
                        break;
                    case ME_Tranform4x8:
                        for(Ipp32s subblk=0; subblk<2; subblk++)
                            for(Ipp32s y=0; y<8; y++)
                                for(Ipp32s x=0; x<4; x++)
                                    mb->Coeff[blk][4*subblk+8*y+x]=m_cur.TrellisCoefficients[tr][blk][32*subblk+4*y+x];
                                    //MeQuantTable[m_cur.adr][intra?0:1][m_cur.BlkIdx][4*subblk+8*y+x]=buf2[32*subblk+4*y+x];
                        break;
                    case ME_Tranform8x4:
                    case ME_Tranform8x8:
                        MFX_INTERNAL_CPY(mb->Coeff[blk],m_cur.TrellisCoefficients[tr][blk],64*sizeof(mb->Coeff[0][0]));
                        break;
                }
            }
        }
    }

} // void MeAVS::SetMB16x16B(MeMbType mbt, MeMV mvF, MeMV mvB, Ipp32s cost)

// AVS realization

//void MePredictCalculatorAVS::Init(MeParams* pMeParams, MeCurrentMB* pCur)
//{
//    MePredictCalculator::Init(pMeParams, pCur);
//    m_pMeParams = pMeParams;
//    m_pCur = pCur;
//    m_pRes = pMeParams->pSrc->MBs;
//    //MFX_INTERNAL_CPY(m_blockDist,m_blockDist,sizeof(m_blockDist)); 
//    //MFX_INTERNAL_CPY(m_distIdx,m_distIdx, sizeof(m_blockDist));
//}
//MeMV MePredictCalculator::GetPrediction( bool isSkipMBlock)
//{
//    switch(m_pCur->MbPart){
//        case ME_Mb16x16:
//            return GetPrediction16x16(isSkipMBlock);
//        case ME_Mb8x8:
//            return GetPrediction8x8(isSkipMBlock);
//    }
//
//    return 0;
//}


//MeMV MePredictCalculatorAVS::GetPrediction( bool isSkipMBlock)
//{
//    switch(m_pCur->MbPart){
//        case ME_Mb16x16:
//            return GetPrediction16x16(isSkipMBlock);
//        case ME_Mb8x8:
//            return GetPrediction8x8(isSkipMBlock);
//    }
//
//    return 0;
//}

void MePredictCalculatorAVS::SetMbEdges()
{
    // Edge MBs definitions.
    Ipp32s widthMB = m_pMeParams->pSrc->WidthMB;
    if(0 == m_pCur->x) 
        m_pMbInfoLeft = NULL;
    else 
        m_pMbInfoLeft = m_pRes + (m_pCur->x -1) + (m_pCur->y)*widthMB;

    if(0 == m_pCur->y)
    {
        m_pMbInfoTop = m_pMbInfoTopRight = m_pMbInfoTopLeft = NULL;
    }
    else
    {
        if (m_pMbInfoLeft) //( 1 <= m_pCur->x)
        {
            m_pMbInfoTopLeft = m_pRes + (m_pCur->x -1) + (m_pCur->y-1)*widthMB;
            m_pMbInfoTop = m_pRes + (m_pCur->x) + (m_pCur->y-1)*widthMB;
        }
        if(m_pCur->x == widthMB - 1)
            m_pMbInfoTopRight = NULL;
        else
            m_pMbInfoTopRight = m_pRes + (m_pCur->x +1) + (m_pCur->y -1 )*widthMB;
    }
    //and finally set current block
    m_pMbInfo = m_pRes + (m_pCur->x) + (m_pCur->y)*widthMB;
    if (-1 == (m_pMbInfo->Refindex[1][0]) )
    {
        memset(m_pMbInfo->Refindex[1], 0, 4*sizeof(Ipp32s));
        //__asm int 3;
    }
}

MeMV MePredictCalculatorAVS::GetSkipPrediction()
{
    // Edge MBs definitions.
    SetMbEdges();
    // additional condition for Skip Macro Blocks
    Ipp32s PredForward = m_pCur->RefDir + 1; // AVS style
    MeMV zeroMv(0);
    if ((NULL == m_pMbInfoTop) ||
        ((0 == m_pMbInfoTop->Refindex[0/*AVS_FORWARD*/][2]) &&
         (m_pMbInfoTop->predType[2] & PredForward) &&
         ( (m_pMbInfoTop->MV[0/*AVS_FORWARD*/][2].x == 0) &&
           (m_pMbInfoTop->MV[0/*AVS_FORWARD*/][2].y == 0)))  )
    {
        //m_mvPred.scalar = 0;
        return zeroMv;
    }
    else if ((NULL == m_pMbInfoLeft) ||
             ((0 == m_pMbInfoLeft->Refindex[0/*AVS_FORWARD*/][1]) &&
              (m_pMbInfoLeft->predType[1] & PredForward) &&
              ( (m_pMbInfoLeft->MV[0/*AVS_FORWARD*/][1].x == 0) &&
                (m_pMbInfoLeft->MV[0/*AVS_FORWARD*/][1].y == 0))) )
    {
        //m_mvPred.scalar = 0;
        return zeroMv;
    }
    else
    {
        // get the motion vector prediction
        m_pPredMV = &m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx];
        GetMotionVectorPredictor16x16(m_pCur->RefDir, 0);
    }
    
    return m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx];
} // MePredictCalculatorAVS::GetSkipPrediction()

void MePredictCalculatorAVS::GetSkipDirectPrediction()
{
    //ReconstructMotionVectorsBSliceDirect(  &m_cur.PredMV[frw][i][m_cur.BlkIdx],
    //                                       &m_cur.PredMV[frw][i][m_cur.BlkIdx]);
} // MeMV MePredictCalculatorAVS::GetSkipDirectPrediction()

MeMV MePredictCalculatorAVS::GetPrediction16x16(/*bool isSkipMBlock*/)
{
    //if (!isSkipMBlock)
    if(!m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx].IsInvalid())
        return m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx];

    // Edge MBs definitions.
    SetMbEdges();
    // additional condition for Skip MBlocks
    //if (isSkipMBlock)
    //{
    //    Ipp32s PredForward = m_pCur->RefDir + 1; // AVS style
    //    MeMV zeroMv(0);
    //    if ((NULL == m_pMbInfoTop) ||
    //        ((0 == m_pMbInfoTop->Refindex[0/*AVS_FORWARD*/][2]) &&
    //         (m_pMbInfoTop->predType[2] & PredForward) &&
    //         (m_pMbInfoTop->MV[0/*AVS_FORWARD*/][2] == 0)))
    //    {
    //        //m_mvPred.scalar = 0;
    //        return zeroMv;
    //    }
    //    else if ((NULL == m_pMbInfoLeft) ||
    //             ((0 == m_pMbInfoLeft->Refindex[0/*AVS_FORWARD*/][1]) &&
    //              (m_pMbInfoLeft->predType[1] & PredForward) &&
    //              (m_pMbInfoLeft->MV[0/*AVS_FORWARD*/][1] == 0)))
    //    {
    //        //m_mvPred.scalar = 0;
    //        return zeroMv;
    //    }
    //}
    //else
    {
        // get the motion vector prediction
        m_pPredMV = &m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx];
        GetMotionVectorPredictor16x16(m_pCur->RefDir, 0);
    }
    
    //m_FieldPredictPrefer[m_pCur->RefDir][0] = (this->*(GetPredictor))(ME_Macroblock,m_pCur->RefDir,m_CurPrediction[m_pCur->RefDir][0]);
    return m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx];
}// MeMV MePredictCalculatorAVS::GetPrediction16x16()

MeMV MePredictCalculatorAVS::GetPrediction8x8(/*bool isSkipMBlock*/)
{
    //if (!isSkipMBlock)
        if(!m_CurPrediction[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx].IsInvalid())
            return m_CurPrediction[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];

    // only one type of prediction is available: ME_AVS
    //switch(m_pMeParams->PredictionType)
    //{
    //case ME_MPEG2:
    //    GetPredictor = &UMC::MePredictCalculatorAVS::GetPredictorMPEG2;
    //    break;
    //case ME_AVS:
    //    GetPredictor = &UMC::MePredictCalculatorAVS::GetPredictorAVS;
    //    break;
    //default:
    //    return false;
    //}

    // Edge MBs definitions.
    SetMbEdges();
    // get the motion vector prediction
    m_neighbours.pLeft = m_pMbInfoLeft;
    m_neighbours.pTop = m_pMbInfoTop;
    m_neighbours.pTopRight = m_pMbInfoTop;
    m_neighbours.pTopLeft = NULL;
    m_pPredMV = &m_CurPrediction[m_pCur->RefDir][0][m_pCur->RefIdx];
    GetMotionVectorPredictor8x8(m_pCur->RefDir, 0);

    // get the motion vector prediction & reconstruct motion vector
    m_neighbours.pLeft = m_pMbInfo;
    m_neighbours.pTopRight = m_pMbInfoTopRight;
    m_neighbours.pTopLeft = m_pMbInfoTop;
    m_pPredMV = &m_CurPrediction[m_pCur->RefDir][1][m_pCur->RefIdx];
    GetMotionVectorPredictor8x8(m_pCur->RefDir, 1);
    
    // get the motion vector prediction & reconstruct motion vector
    m_neighbours.pLeft = m_pMbInfoLeft;
    m_neighbours.pTop = m_pMbInfo;
    m_neighbours.pTopRight = m_pMbInfo;
    m_neighbours.pTopLeft = NULL;
    m_pPredMV = &m_CurPrediction[m_pCur->RefDir][2][m_pCur->RefIdx];
    GetMotionVectorPredictor8x8(m_pCur->RefDir, 2);

    // get the motion vector prediction & reconstruct motion vector
    m_neighbours.pLeft = m_pMbInfo;
    m_neighbours.pTopRight = NULL;
    m_neighbours.pTopLeft = m_pMbInfo;
    m_pPredMV = &m_CurPrediction[m_pCur->RefDir][3][m_pCur->RefIdx];
    GetMotionVectorPredictor8x8(m_pCur->RefDir, 3);

    //m_FieldPredictPrefer[m_pCur->RefDir][m_pCur->BlkIdx] = (this->*(GetPredictor))(ME_Macroblock,m_pCur->RefDir,m_CurPrediction[m_pCur->RefDir][m_pCur->BlkIdx]);
    return m_CurPrediction[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];
}// MeMV MePredictCalculatorAVS::GetPrediction8x8()

void MePredictCalculatorAVS::GetMotionVectorPredictor( MeMV mvA,
                                                       MeMV mvB,
                                                       MeMV mvC,
                                                       Ipp32s blockDist,
                                                       Ipp32s blockDistA,
                                                       Ipp32s blockDistB,
                                                       Ipp32s blockDistC)
{
    Ipp32s distAB, distBC, distCA;
    Ipp32s median;

    // scale motion vectors
    mvA.x = (Ipp16s) ScaleValue(mvA.x, blockDist, blockDistA);
    mvA.y = (Ipp16s) ScaleValue(mvA.y, blockDist, blockDistA);
    mvB.x = (Ipp16s) ScaleValue(mvB.x, blockDist, blockDistB);
    mvB.y = (Ipp16s) ScaleValue(mvB.y, blockDist, blockDistB);
    mvC.x = (Ipp16s) ScaleValue(mvC.x, blockDist, blockDistC);
    mvC.y = (Ipp16s) ScaleValue(mvC.y, blockDist, blockDistC);

    // get distance between vectors
    distAB = VectorDistance(mvA, mvB);
    distBC = VectorDistance(mvB, mvC);
    distCA = VectorDistance(mvC, mvA);

    // get the Median()
    median = distAB + distBC + distCA -
             IPP_MIN(distAB, IPP_MIN(distBC, distCA)) -
             IPP_MAX(distAB, IPP_MAX(distBC, distCA));

    if (distAB == median)
    {
        *m_pPredMV = mvC;
    }
    else if (distBC == median)
    {
        *m_pPredMV = mvA;
    }
    else
    {
        *m_pPredMV = mvB;
    }

} // void MePredictCalculatorAVS::GetMotionVectorPredictor( MeMV mvA,

MeMbPart MePredictCalculatorAVS::GetCollocatedBlockDivision(void)
{
    MeFrame *pRefBw;
    Ipp32s MbIndex = m_pCur->adr;

    // get backward reference
    pRefBw = m_pMeParams->pRefB[0];
    
    // field not supported
    // the backward reference is a frame coded picture
    //if (pRefBw->m_picHeader.picture_structure)
    {
        MeMB *pMbInfo;

        // get collocated block's info
        pMbInfo = pRefBw->MBs + MbIndex;

        // if it is an intra MB,
        // then we can process all motion vector at once
        if (ME_MbIntra == pMbInfo->MbType)
        {
            return ME_Mb16x16;
        }
        else
        {
            return ((MeMbPart)(pMbInfo->MbPart));
        }
    }
    // the backward reference is a field coded picture,
    // we have to adjust variables
    //else
    //{
    //    AVS_MB_INFO *pMbInfo;
    //    Ipp32s corrIndex;

    //    corrIndex = (((MbIndex / m_decCtx.MbWidth) / 2) * m_decCtx.MbWidth) +
    //                (MbIndex % m_decCtx.MbWidth);

    //    // get collocated block's info
    //    pMbInfo = pRefBw->m_pMbInfo + corrIndex;

    //    // if it is an intra MB,
    //    // then we can process all motion vector at once
    //    if (I_8x8 == pMbInfo->MbType)
    //    {
    //        return Div_16x16;
    //    }
    //    else
    //    {
    //        // mask only vertical division
    //        return (eAVSDivBlockType) (pMbInfo->divType & 2);
    //    }
    //}

} // MeMbPart MePredictCalculatorAVS::GetCollocatedBlockDivision(void)

    /*if ((neighbour_info)->predType[block_num] & predType) \*/
    /*if ((neighbour_info)->MbType & predType) \*/
#define GET_NEIGHBOUR_MV(vector, dist, neighbour_info, block_num) \
    if (((neighbour_info)->predType[block_num]) & predType) \
    { \
        /* get the block reference index */ \
        Ipp32s ind = (neighbour_info)->Refindex[predType - 1][block_num]; \
        /* copy the vector */ \
        vector = (neighbour_info)->MV[predType - 1][block_num]; \
        lastVector = vector; \
        /* get the distance to the reference pixture */ \
        dist = m_blockDist[predType - 1][ind]; \
        availVectors += 1; \
    }

//inline
//void GET_NEIGHBOUR_MV(MeMV &vector, Ipp32s &dist, MeMB *neighbour_info, Ipp32s &block_num)
//{
//    if ((neighbour_info->predType[block_num]) & predType) 
//    { 
//        /* get the block reference index */ 
//        Ipp32s ind = (neighbour_info)->Refindex[predType - 1][block_num]; 
//        /* copy the vector */ 
//        vector = (neighbour_info)->MV[predType - 1][block_num]; 
//        lastVector = vector; 
//        /* get the distance to the reference pixture */ 
//        dist = m_blockDist[predType - 1][ind]; 
//        availVectors += 1; 
//    }
//}


void MePredictCalculatorAVS::GetMotionVectorPredictor16x16(int isBkw, Ipp32s blockNum)
{
    MeMV lastVector(0,0);
    Ipp32u availVectors = 0;
    MeMV mvA(0,0), mvB(0,0), mvC(0,0);
    Ipp32s blockDistA, blockDistB, blockDistC;
    //Ipp32s predType = m_pMeParams->SearchDirection + 1;// so, fwd 1, bidir 2
    Ipp32s predType = isBkw + 1;// so, fwd 1, bwd 2, ; it is AVS style
    Ipp32s block_num;
    MeMB *neighbour_info;
    //m_pMbInfo->SubMbType[0] = m_pMbInfo->SubMbType[1] = 
    //    m_pMbInfo->SubMbType[2] = m_pMbInfo->SubMbType[3] = predType;
    
    //WRITE_TRACE_LOG(_vec_tst, "start prediction for width", 16);
    //WRITE_TRACE_LOG(_vec_tst, "start prediction for height", 16);

    //
    // select available neighbour vectors
    //

    //mvA.scalar = 0;
    blockDistA = 1;
    if (m_pMbInfoLeft)
    {
        // get top-right block's vector of A macroblock
        GET_NEIGHBOUR_MV(mvA, blockDistA,
                         m_pMbInfoLeft, 1);
        
        //neighbour_info = m_pMbInfoLeft;
        //block_num = 1;
        //if ((neighbour_info->predType[block_num]) & predType) 
        //{ 
        //    Ipp32s ind = neighbour_info->Refindex[predType - 1][block_num]; 
        //    lastVector = neighbour_info->MV[predType - 1][block_num]; 
        //    blockDistA = m_blockDist[predType - 1][ind]; 
        //    availVectors += 1; 
        //}
    }

    //mvB.scalar = 0;
    blockDistB = 1;
    //mvC.scalar = 0;
    blockDistC = 1;
    if (m_pMbInfoTop)
    {
        // get bottom-left block's vector of B macroblock
        GET_NEIGHBOUR_MV(mvB, blockDistB,
                        m_pMbInfoTop, 2);
        
        //neighbour_info = m_pMbInfoTop;
        //block_num = 2;
        //if ((neighbour_info->predType[block_num]) & predType) 
        //{ 
        //    Ipp32s ind = neighbour_info->Refindex[predType - 1][block_num]; 
        //    lastVector = neighbour_info->MV[predType - 1][block_num]; 
        //    blockDistA = m_blockDist[predType - 1][ind]; 
        //    availVectors += 1; 
        //}

        // get bottom-left block's vector of C macroblock
        if (m_pMbInfoTopRight)
        {
            GET_NEIGHBOUR_MV(mvC, blockDistC,
                             m_pMbInfoTopRight, 2);
            //neighbour_info = m_pMbInfoTopRight;
            //block_num = 2;
            //if ((neighbour_info->predType[block_num]) & predType) 
            //{ 
            //    Ipp32s ind = neighbour_info->Refindex[predType - 1][block_num]; 
            //    lastVector = neighbour_info->MV[predType - 1][block_num]; 
            //    blockDistA = m_blockDist[predType - 1][ind]; 
            //    availVectors += 1; 
            //}
        }
        // get bottom-right block's vector of D macroblock
        else if (m_pMbInfoTopLeft)
        {
            GET_NEIGHBOUR_MV(mvC, blockDistC,
                             m_pMbInfoTopLeft, 3);
            //neighbour_info = m_pMbInfoTopLeft;
            //block_num = 3;
            //if ((neighbour_info->predType[block_num]) & predType) 
            //{ 
            //    Ipp32s ind = neighbour_info->Refindex[predType - 1][block_num]; 
            //    lastVector = neighbour_info->MV[predType - 1][block_num]; 
            //    blockDistA = m_blockDist[predType - 1][ind]; 
            //    availVectors += 1; 
            //}
        }
    }

    //
    // calculate prediction for the current block
    //

    // step 1: available only one neighbour reference
    if (1 == availVectors)
    {
        *m_pPredMV = lastVector;
        //m_cur.PredMV[dir][mb->Refindex[dir][i]][i]
    }
    // step 3: perform full calculation
    else
    {
        Ipp32s blockDist;

        //blockDist = m_blockDist[predType - 1][m_pMbInfo->Refindex[predType - 1][blockNum]];
        blockDist = m_blockDist[predType - 1][m_pMbInfo->Refindex[predType - 1][blockNum]];
        GetMotionVectorPredictor(mvA, mvB, mvC,
                                 blockDist,
                                 blockDistA, blockDistB, blockDistC);
    }
    //WRITE_TRACE_LOG(_vec_tst, "motion vector pred X", m_mvPred.vector.x);
    //WRITE_TRACE_LOG(_vec_tst, "motion vector pred Y", m_mvPred.vector.y);
    // typically m_pCur->RefIdx is 0
    m_AMV[isBkw][0][m_pCur->RefIdx] = AMV = mvA;
    m_BMV[isBkw][0][m_pCur->RefIdx] = BMV = mvB;
    m_CMV[isBkw][0][m_pCur->RefIdx] = CMV = mvC;

} // void MePredictCalculatorAVS::GetMotionVectorPredictor16x16(int isBkw, Ipp32s blockNum)

void MePredictCalculatorAVS::GetMotionVectorPredictor8x8(int isBkw, Ipp32s blockNum)
{
    MeMV lastVector(0,0);
    Ipp32u availVectors = 0;
    MeMV mvA(0,0), mvB(0,0), mvC(0,0);
    Ipp32s blockDistA, blockDistB, blockDistC;
    //Ipp32s predType = m_pMeParams->SearchDirection + 1;// so, fwd 1, bidir 2
    Ipp32s predType = isBkw + 1;// so, fwd 1, bidir 2; it is AVS style

    //WRITE_TRACE_LOG(_vec_tst, "start prediction for width", 8);
    //WRITE_TRACE_LOG(_vec_tst, "start prediction for height", 8);

    //
    // step 1. select available neighbour vectors
    //

    //mvA.scalar = 0;
    blockDistA = 1;
    if (m_neighbours.pLeft)
    {
        // get A block
        GET_NEIGHBOUR_MV(mvA, blockDistA,
                         m_neighbours.pLeft, blockNum ^ 1);
        AMV = mvA;
    }

    //mvB.scalar = 0;
    blockDistB = 1;
    //mvC.scalar = 0;
    blockDistC = 1;
    if (m_neighbours.pTop)
    {
        // get B macro
        GET_NEIGHBOUR_MV(mvB, blockDistB,
                         m_neighbours.pTop, (blockNum + 2) % 4);
        BMV = mvB;

        // get C block
        if (m_neighbours.pTopRight)
        {
            GET_NEIGHBOUR_MV(mvC, blockDistC,
                             m_neighbours.pTopRight, 3 - blockNum);
            CMV = mvC;
        }
        // get D block
        else if (m_neighbours.pTopLeft)
        {
            GET_NEIGHBOUR_MV(mvC, blockDistC,
                             m_neighbours.pTopLeft, 3 - blockNum);
            CMV = mvC;
        }
    }

    if (1 == availVectors)
    {
        //m_mvPred = lastVector;
        *m_pPredMV = lastVector;
    }
    else
    {
        Ipp32s blockDist;

        //blockDist = m_blockDist[predType - 1][m_pMbInfo->refIdx[predType - 1][blockNum]];
        blockDist = m_blockDist[predType - 1][m_pMbInfo->Refindex[predType - 1][blockNum]];
        GetMotionVectorPredictor(mvA, mvB, mvC,
                                 blockDist,
                                 blockDistA, blockDistB, blockDistC);
    }

    //WRITE_TRACE_LOG(_vec_tst, "motion vector pred X", m_mvPred.vector.x);
    //WRITE_TRACE_LOG(_vec_tst, "motion vector pred Y", m_mvPred.vector.y);
    // typically m_pCur->RefIdx is 0
    m_AMV[isBkw][blockNum][m_pCur->RefIdx] = AMV;
    m_BMV[isBkw][blockNum][m_pCur->RefIdx] = BMV;
    m_CMV[isBkw][blockNum][m_pCur->RefIdx] = CMV;

} // void MePredictCalculatorAVS::GetMotionVectorPredictor8x8(int isBkw, Ipp32s blockNum)

// methods for direct motion vector calculation
void MePredictCalculatorAVS::ReconstructMotionVectorsBSliceDirect(MeMV *mvFw, MeMV *mvBw)
{
    MeMbPart divCollBlock;

    divCollBlock = GetCollocatedBlockDivision();

    if (ME_Mb16x16 == divCollBlock)
    {
        // reconstruct the motion vector...
        ReconstructDirectMotionVector(0);

    }
    //else if (Div_16x8 == divCollBlock)
    //else if (Div_8x16 == divCollBlock)
    else if (ME_Mb8x8 == divCollBlock)
    {
        // reconstruct the motion vectors
        ReconstructDirectMotionVector(0);
        ReconstructDirectMotionVector(1);
        ReconstructDirectMotionVector(2);
        ReconstructDirectMotionVector(3);

        // correct type division type
        //m_pMbInfo->divType = Div_8x8;
    }
    *mvFw = m_predMVDirect[0][0];
    *mvBw = m_predMVDirect[1][0];
} // void MePredictCalculatorAVS::ReconstructMotionVectorsBSliceDirect(void)


void MePredictCalculatorAVS::ReconstructDirectMotionVector(Ipp32s blockNum)
{
    Ipp32u AVS_FORWARD = 0, AVS_BACKWARD = 1;
    Ipp32s corrBlock, corrIndex;
    Ipp32s MbIndex = m_pCur->adr;
    Ipp32u MbWidth = m_pMeParams->pSrc->WidthMB;
    //AVS_MB_INFO *pMbInfo;
    //AVSPicture *pRefBw;
    MeMB *pMbInfo = m_pMeParams->pSrc->MBs + m_pCur->adr;
    //MeMV *currMVDirect = m_pMeParams->pSrc->MVDirect + m_pCur->adr;
    MeFrame *pRefBw;

    //WRITE_TRACE_LOG(_vec_dir_tst, "direct vectors for block", blockNum);

    // get backward reference
    //pRefBw = m_pRefs[AVS_BACKWARD][0];
    pRefBw = m_pMeParams->pRefB[0];

    // the backward reference is a frame coded picture
    //if (pRefBw->m_picHeader.picture_structure)
    if (!m_pMeParams->bSecondField)
    {
        corrIndex = MbIndex;
        corrBlock = blockNum;
    }
    // the backward reference is a field coded picture,
    // we have to adjust variables
    else
    {
        corrIndex = (((MbIndex / MbWidth) / 2) * MbWidth) +
                    (MbIndex % MbWidth);
        corrBlock = (((MbIndex / MbWidth) & 1) << 1) + (blockNum & 1);
    }

    // check the type of a corresponding block in the backward reference picture
    //pMbInfo = pRefBw->m_pMbInfo + corrIndex;
    pMbInfo = pRefBw->MBs + corrIndex;
    if (ME_MbIntra == pMbInfo->MbType)
    {
        // we must use the reference picture #1
        m_pMbInfo->Refindex[AVS_FORWARD][blockNum] = 0;
        m_pMbInfo->Refindex[AVS_BACKWARD][blockNum] = 0;

        // calculate the forward motion vector
        GetMotionVectorPredictor16x16(0/*fwd*/, blockNum);
        //m_pMbInfo->mv[AVS_FORWARD][blockNum] = m_mvPred;
        m_predMVDirect[0][blockNum] = *m_pPredMV;

        // calculate the backward motion vector
        GetMotionVectorPredictor16x16(1/*bkw*/, blockNum);
        //m_pMbInfo->mv[AVS_BACKWARD][blockNum] = m_mvPred;
        m_predMVDirect[1][blockNum] = *m_pPredMV;
    }
    else
    {
        Ipp32s BlockDistanceFw, BlockDistanceBw;
        Ipp32s DistanceIndexFw, DistanceIndexBw;
        Ipp32s BlockDistanceRef;
        Ipp32s DistanceIndexRef;
        Ipp32s DistanceIndexCur;
        MeMV mvRef;

        //
        // FIRST STEP
        //

        {
            Ipp32s refIdx;

            // get reference motion vector and distance index
            mvRef = pMbInfo->MV[AVS_FORWARD][corrBlock];
            refIdx = pMbInfo->Refindex[AVS_FORWARD][corrBlock];
            DistanceIndexRef = pRefBw->m_distIdx[AVS_FORWARD][refIdx];

            // get distances
            DistanceIndexFw = m_distIdx[AVS_FORWARD][0];
            DistanceIndexBw = m_distIdx[AVS_BACKWARD][0];

            BlockDistanceFw = m_blockDist[AVS_FORWARD][0];
            BlockDistanceBw = m_blockDist[AVS_BACKWARD][0];

            //DistanceIndexCur = m_decCtx.m_pPicHeader->picture_distance * 2;
            DistanceIndexCur = m_pMeParams->pSrc->picture_distance * 2;
        }

        //
        // SECOND STEP
        //
        BlockDistanceRef = IPP_MAX(1, (DistanceIndexBw - DistanceIndexRef + 512) % 512);

        BlockDistanceFw = (DistanceIndexCur - DistanceIndexFw + 512) % 512;
        BlockDistanceBw = (DistanceIndexBw - DistanceIndexCur + 512) % 512;

        // so, field doesn't supported
        // but backward reference is a field pair
        //if (0 == pRefBw->m_picHeader.picture_structure)
        //{
        //    mvRef.vector.y *= 2;
        //}

        //
        // THIRD STEP
        //
        {
            MeMV mvFw, mvBw;

            if (0 > mvRef.x)
            {
                mvFw.x = (Ipp16s) -(((16384 / BlockDistanceRef) *
                                            (1 - mvRef.x * BlockDistanceFw) - 1) >> 14);
                mvBw.x = (Ipp16s) (((16384 / BlockDistanceRef) *
                                           (1 - mvRef.x * BlockDistanceBw) - 1) >> 14);
            }
            else
            {
                mvFw.x = (Ipp16s) (((16384 / BlockDistanceRef) *
                                           (1 + mvRef.x * BlockDistanceFw) - 1) >> 14);
                mvBw.x = (Ipp16s) -(((16384 / BlockDistanceRef) *
                                            (1 + mvRef.x * BlockDistanceBw) - 1) >> 14);
            }

            if (0 > mvRef.y)
            {
                mvFw.y = (Ipp16s) -(((16384 / BlockDistanceRef) *
                                            (1 - mvRef.y * BlockDistanceFw) - 1) >> 14);
                mvBw.y = (Ipp16s) (((16384 / BlockDistanceRef) *
                                           (1 - mvRef.y * BlockDistanceBw) - 1) >> 14);
            }
            else
            {
                mvFw.y = (Ipp16s) (((16384 / BlockDistanceRef) *
                                           (1 + mvRef.y * BlockDistanceFw) - 1) >> 14);
                mvBw.y = (Ipp16s) -(((16384 / BlockDistanceRef) *
                                            (1 + mvRef.y * BlockDistanceBw) - 1) >> 14);
            }

            //m_pMbInfo->mv[AVS_FORWARD][blockNum] = mvFw;
            //m_pMbInfo->mv[AVS_BACKWARD][blockNum] = mvBw;
            m_predMVDirect[0][blockNum] = mvFw;
            m_predMVDirect[1][blockNum] = mvBw;
        }
    }

    // set the prediction type
    //m_pMbInfo->predType[blockNum] = PredBiDir;

    //WRITE_TRACE_LOG(_vec_dir_tst, "FW motion vector X", m_pMbInfo->mv[AVS_FORWARD][blockNum].vector.x);
    //WRITE_TRACE_LOG(_vec_dir_tst, "FW motion vector Y", m_pMbInfo->mv[AVS_FORWARD][blockNum].vector.y);
    //WRITE_TRACE_LOG(_vec_dir_tst, "BW motion vector X", m_pMbInfo->mv[AVS_BACKWARD][blockNum].vector.x);
    //WRITE_TRACE_LOG(_vec_dir_tst, "BW motion vector Y", m_pMbInfo->mv[AVS_BACKWARD][blockNum].vector.y);

} // void MePredictCalculatorAVS::ReconstructDirectMotionVector(Ipp32s blockNum)

//MeMV MePredictCalculatorAVS::CreateSymmetricalMotionVector(Ipp32s blockNum, MeMV mvFw)
MeMV MePredictCalculatorAVS::CreateSymmetricalMotionVector(Ipp32s /*blockNum*/, MeMV mvFw)
{
    MeMV /*mvFw,*/ m_mvPred;
    Ipp32s DistanceMul;

    // calculate vector distance
    DistanceMul = m_blockDist[1/*AVS_BACKWARD*/][0] * (512 / m_blockDist[0/*AVS_FORWARD*/][0]);

    // compute the vector
    //mvFw =  m_cur.BestMV[frw][0/*AVS_FORWARD*/][blockNum];
    m_mvPred.x = (Ipp16s) -((mvFw.x * DistanceMul + 256) >> 9);
    m_mvPred.y = (Ipp16s) -((mvFw.y * DistanceMul + 256) >> 9);
    
    return m_mvPred;
} // void MePredictCalculatorAVS::CreateSymmetricalMotionVector(Ipp32s blockNum)


// Two functions below don't use

//bool MePredictCalculatorAVS::GetPredictorAVS(int index, int isBkw, MeMV* res)
//{
//    m_pPredMV = res;
//
//    switch(index)
//    {
//    //case ME_Block0:
//    //    GetBlockVectorsABC_0(isBkw);
//    //    break;
//    //case ME_Block1:
//    //    GetBlockVectorsABC_1(isBkw);
//    //    break;
//    //case ME_Block2:
//    //    GetBlockVectorsABC_2(isBkw);
//    //    break;
//    //case ME_Block3:
//    //    GetBlockVectorsABC_3(isBkw);
//    //    break;
//    case ME_Macroblock:
//        GetMotionVectorPredictor16x16(isBkw,0);
//        break;
//    default:
//        assert(false);
//        break;
//    }
//
//    return false;
//}
//
//bool MePredictCalculatorAVS::GetPredictorMPEG2(int index,int isBkw, MeMV* res)
//{
//    if (MbLeftEnable)
//    {
//        if(isBkw != 0) res[0] = m_pRes[m_pCur->adr-1].MV[1][0];
//        res[0] = m_pRes[m_pCur->adr-1].MV[0][0];
//    }
//    else
//    {
//        res[0] = MeMV(0);
//    }
//    return true;
//}

}//namespace UMC
#endif