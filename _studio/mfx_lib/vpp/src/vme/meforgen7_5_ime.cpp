//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include "ipps.h"

#include "meforgen7_5.h"
#include "ipp.h"

#pragma warning( disable : 4244 )
#pragma warning( disable : 4127 )
#pragma warning( disable : 4706 )
#pragma warning( disable : 4189 )
#pragma warning( disable : 4100 )
#pragma warning( disable : 4702 )


//short const mvs[4] = {1,2,2,4};

bool MEforGen75::CombineReducuedMBResults(short isb)
{
    int        j, k;
    I32        *dist = Dist[isb];
    I16PAIR    *mv = Best[isb];

    if(Vsta.SrcType&3){ // reduced macroblocks
        //Vsta.DoIntraInter &= 0xFFFD; // no intra;
        dist[BHXH] = MBMAXDIST;
        if(Vsta.SrcType&2){ // 8x16 or 8x8
            dist[BHX8] = MBMAXDIST;
            dist[BHX8+1] = MBMAXDIST;
             
            if(dist[B8XH+1]<dist[B8XH]){ dist[B8XH] = dist[B8XH+1]; mv[B8XH+1].x += 16; mv[B8XH].x =  mv[B8XH+1].x; mv[B8XH].y = mv[B8XH+1].y; }
            else{ dist[B8XH+1] = dist[B8XH]; mv[B8XH+1].x = mv[B8XH].x; mv[B8XH+1].y = mv[B8XH].y; }

            if(dist[B8X8+1]<dist[B8X8]){ dist[B8X8] = dist[B8X8+1]; mv[B8X8+1].x += 16; mv[B8X8].x = mv[B8X8+1].x; mv[B8X8].y = mv[B8X8+1].y; }
            else{ dist[B8X8+1] = dist[B8X8]; mv[B8X8+1].x = mv[B8X8].x; mv[B8X8+1].y = mv[B8X8].y; }
            if(dist[B8X8+3]<dist[B8X8+2]){ dist[B8X8+2] = dist[B8X8+3]; mv[B8X8+3].x += 16; mv[B8X8+2].x = mv[B8X8+3].x; mv[B8X8+2].y = mv[B8X8+3].y; }
            else{ dist[B8X8+3] = dist[B8X8+2]; mv[B8X8+3].x = mv[B8X8+2].x; mv[B8X8+3].y = mv[B8X8+2].y; }
 
            for(j=0;j<6;j++){
                if(j==2) j = 4;
                if(dist[B8X4+2+j]<dist[B8X4+j]){ dist[B8X4+j] = dist[B8X4+2+j]; mv[B8X4+2+j].x +=16; mv[B8X4+j].x = mv[B8X4+2+j].x; mv[B8X4+j].y = mv[B8X4+2+j].y; }
                else{ dist[B8X4+2+j] = dist[B8X4+j]; mv[B8X4+2+j].x = mv[B8X4+j].x; mv[B8X4+2+j].y = mv[B8X4+j].y; }
                if(dist[B4X8+2+j]<dist[B4X8+j]){ dist[B4X8+j] = dist[B4X8+2+j]; mv[B4X8+2+j].x += 16; mv[B4X8+j].x = mv[B4X8+2+j].x; mv[B4X8+j].y = mv[B4X8+2+j].y; }
                else{ dist[B4X8+2+j] = dist[B4X8+j]; mv[B4X8+2+j].x = mv[B4X8+j].x; mv[B4X8+2+j].y = mv[B4X8+j].y; }
            }
            for(j=0;j<12;j++){
                if(j==4) j = 8;
                if(dist[B4X4+4+j]<dist[B4X4+j]){ dist[B4X4+j] = dist[B4X4+4+j]; mv[B4X4+4+j].x += 16; mv[B4X4+j].x = mv[B4X4+4+j].x; mv[B4X4+j].y = mv[B4X4+4+j].y; }
                else{ dist[B4X4+4+j] = dist[B4X4+j]; mv[B4X4+4+j].x = mv[B4X4+j].x; mv[B4X4+4+j].y = mv[B4X4+j].y; }
            }        
        }
        if(Vsta.SrcType&1){ // 16x8 or 8x8        
            dist[B8XH+1] = dist[B8XH] = MBMAXDIST;             
            if(dist[BHX8+1]<dist[BHX8]){ dist[BHX8] = dist[BHX8+1]; mv[BHX8].x = mv[BHX8+1].x; mv[BHX8+1].y += 16; mv[BHX8].y = mv[BHX8+1].y; }
            else{ dist[BHX8+1] = dist[BHX8]; mv[BHX8+1].x = mv[BHX8].x; mv[BHX8+1].y = mv[BHX8].y; }

            if(dist[B8X8+2]<dist[B8X8]){ dist[B8X8] = dist[B8X8+2]; mv[B8X8].x = mv[B8X8+2].x; mv[B8X8+2].y += 16; mv[B8X8].y = mv[B8X8+2].y; }
            else{ dist[B8X8+2] = dist[B8X8]; mv[B8X8+2].x = mv[B8X8].x; mv[B8X8+2].y = mv[B8X8].y; }
            if(dist[B8X8+3]<dist[B8X8+1]){ dist[B8X8+1] = dist[B8X8+3]; mv[B8X8+1].x = mv[B8X8+3].x; mv[B8X8+3].y += 16; mv[B8X8+1].y = mv[B8X8+3].y; }
            else{ dist[B8X8+3] = dist[B8X8+1]; mv[B8X8+3].x = mv[B8X8+1].x; mv[B8X8+3].y = mv[B8X8+1].y; }

            for(j=0;j<4;j++){
                if(dist[B8X4+4+j]<dist[B8X4+j]){ dist[B8X4+j] = dist[B8X4+4+j]; mv[B8X4+j].x = mv[B8X4+4+j].x; mv[B8X4+4+j].y += 16; mv[B8X4+j].y = mv[B8X4+4+j].y; }
                else{ dist[B8X4+4+j] = dist[B8X4+j]; mv[B8X4+4+j].x = mv[B8X4+j].x; mv[B8X4+4+j].y = mv[B8X4+j].y; }
                if(dist[B4X8+4+j]<dist[B4X8+j]){ dist[B4X8+j] = dist[B4X8+4+j]; mv[B4X8+j].x = mv[B4X8+4+j].x; mv[B4X8+4+j].y += 16; mv[B4X8+j].y = mv[B4X8+4+j].y; }
                else{ dist[B4X8+4+j] = dist[B4X8+j]; mv[B4X8+4+j].x = mv[B4X8+j].x; mv[B4X8+4+j].y = mv[B4X8+j].y; }
            }
            for(j=0;j<8;j++){
                if(dist[B4X4+8+j]<dist[B4X4+j]){ dist[B4X4+j] = dist[B4X4+8+j]; mv[B4X4+j].x = mv[B4X4+8+j].x; mv[B4X4+8+j].y += 16; mv[B4X4+j].y = mv[B4X4+8+j].y; }
                else{ dist[B4X4+8+j] = dist[B4X4+j]; mv[B4X4+8+j].x = mv[B4X4+j].x; mv[B4X4+8+j].y = mv[B4X4+j].y; }
            }
        }

        //store the result
        for(k=0;k<42;k++){
            if(dist[k]<DistForReducedMB[isb][k]){ DistForReducedMB[isb][k] = dist[k]; BestForReducedMB[isb][k].x = mv[k].x; BestForReducedMB[isb][k].y = mv[k].y; }
        }

        return true;
    }
    return false;
}

/*****************************************************************************************************/
int MEforGen75::IntegerMESearchUnit( )
/*****************************************************************************************************/
{
    int        e, e1, i = 0;
    int        x = 0, y = 0;
    //int        j = 0, k = 0;
    int        refIdx = 0;
    PipelineCalls = 0;

    //Vout->VmeDecisionLog |= PERFORM_IME; // Dropped for Gen75
     
    bdualAdaptive = false;
    PipelinedSUFlg = false;
    UsePreviousMVFlg = false;
    AdaptiveCntSu[0] = AdaptiveCntSu[1] = 0;
    FixedCntSu[0] = FixedCntSu[1] = 0;
    startedAdaptive = false;
    CheckNextReqValid = false;

    for(i=0;i<42;i++){ Dist[1][i] = Dist[0][i] = DistForReducedMB[1][i] = DistForReducedMB[0][i] = MBINVALID_DIST; }
    if(!(VIMEsta.Multicall&STREAM_IN)){
        VIMEsta.RecordDst16[0] = VIMEsta.RecordDst16[1] = MBMAXDIST; 
        for(i=0;i<8;i++) VIMEsta.RecordDst[0][i] = VIMEsta.RecordDst[1][i] = MBMAXDIST; 
    }
    for(i=0;i<4;i++) UniOpportunitySad[1][i] = UniOpportunitySad[0][i] = MBINVALID_DIST;
    DistInter = MBINVALID_DIST;

    if((e = GetNextSU( ))) return e;
    do{
        x = NextSU.x; y = NextSU.y; i = NextRef; 
        if((e = OneSearchUnit(x,y,i))>1) return e;
        if((e1 = GetNextSU( ))) return e1;
        if(NextRef==1){--AltMoreSU;}
        refIdx = (Vsta.VmeModes&4) ? i : 0;
        if (CheckNextReqValid==false || (CheckNextReqValid==true && NextRef<3))
        {
            MoreSU[refIdx]--;
        }
    }while(((MoreSU[0]>0 || MoreSU[1]>0) && NextRef<2)||(bdualAdaptive));

    // put results back
    if(Vsta.SrcType&3) // reduced macroblocks
    {
        // put the stored results back
        for (int refIndex = 0; refIndex<2; refIndex++)
        {
            for (i = 0; i < 42; i++)
            {                     
                Dist[refIndex][i] = DistForReducedMB[refIndex][i];
                Best[refIndex][i].x = BestForReducedMB[refIndex][i].x;
                Best[refIndex][i].y = BestForReducedMB[refIndex][i].y;
            }
        }
    }

    memset(MajorDist,0x77,8);

    return e;
}

/*****************************************************************************************************/
int MEforGen75::GetNextSU( )
/*****************************************************************************************************/
{
    int        x,y;
    U8 chromamode = Vsta.SadType&INTER_CHROMA_MODE;
    short    rw = (((Vsta.RefW<<chromamode)-16)>>2);
    short    rh = ((Vsta.RefH-(16>>chromamode))>>2);

    if(NextRef == 2 && CheckNextReqValid==false) return 0;
    switch(Vsta.VmeModes&7){
        case 1: // single reference, extra starter, single record
        case 0: // single reference, single starter, single record
            NextRef = 0;
            while(CntSU[0]<LenSP[0]){
                x = ActiveSP[0][CntSU[0]].x;
                y = ActiveSP[0][CntSU[0]].y;
                CntSU[0]++;
                if((!(Record[0][y]&(1<<x)))||((CntSU[0]==2)&&((Vsta.VmeModes&7)==1))){
                    if (Vsta.SadType&1)/*chroma*/
                    {
                        Record[0][y] |= (1<<x);
                    }
                    else
                    {
                        switch(Vsta.SrcType&3){
                        case 0:    Record[0][y] |= (1<<x); break;
                        case 1:    Record[0][y] |= (1<<x); Record[0][y+1] |= (1<<x); break;
                        case 2:    Record[0][y] |= (3<<x); break;
                        case 3:    Record[0][y] |= (3<<x); Record[0][y+1] |= (3<<x); break;
                        }
                    }
                    NextSU.x = x;
                    NextSU.y = y;
                    return 0;
                }
            }
            if(Dynamic[0]){
                GetDynamicSU(0);
                if(NextRef<2)//if(Dynamic[0]) 
                {
                    if(CheckNextReqValid==true){
                        NextRef = 2;
                        bdualAdaptive = false;
                        return 0; 
                    }
                    x = NextSU.x;
                    y = NextSU.y;
                    if (Vsta.SadType&1)/*chroma*/
                    {
                        Record[0][y] |= (1<<x);
                    }
                    else
                    {
                        switch(Vsta.SrcType&3){
                        case 0:    Record[0][y] |= (1<<x); break;
                        case 1:    Record[0][y] |= (1<<x); Record[0][y+1] |= (1<<x); break;
                        case 2:    Record[0][y] |= (3<<x); break;
                        case 3:    Record[0][y] |= (3<<x); Record[0][y+1] |= (3<<x); break;
                        }
                    }
                    bdualAdaptive = true;
                    return 0; 
                }
                else{bdualAdaptive = false;}
#ifdef VMETESTGEN
                Vout->Test.usedAdaptiveSU = true;
#endif
            }
            NextRef = 3; bdualAdaptive = false; return 0; 
        case 2: case 3: // single reference, dual starters, dual record
            NextRef = 1 - NextRef;
            while((NextRef>=0&&NextRef<2) && 
                (CntSU[NextRef]<LenSP[NextRef])&&((CntSU[NextRef]+CntSU[1-NextRef])<Vsta.MaxLenSP)){
                x = ActiveSP[NextRef][CntSU[NextRef]].x;
                y = ActiveSP[NextRef][CntSU[NextRef]].y;
                CntSU[NextRef]++;
                if(((!(Record[NextRef][y]&(1<<x)))&&((x<rw) && (y<rh)))||((NextRef==1)&&(CntSU[1]==1)&&(x<rw) && (y<rh))){
                    if (Vsta.SadType&1)/*chroma*/
                    {
                        Record[NextRef][y] |= (1<<x);
                    }
                    else
                    {
                        switch(Vsta.SrcType&3){
                        case 0:    Record[NextRef][y] |= (1<<x); break;
                        case 1:    Record[NextRef][y] |= (1<<x); Record[NextRef][y+1] |= (1<<x); break;
                        case 2:    Record[NextRef][y] |= (3<<x); break;
                        case 3:    Record[NextRef][y] |= (3<<x); Record[NextRef][y+1] |= (3<<x); break;
                        }
                    }
                    NextSU.x = x;
                    NextSU.y = y;
                    return 0;
                }
            }
            NextRef = 1 - NextRef;
            while((NextRef>=0&&NextRef<2) &&
                (CntSU[NextRef]<LenSP[NextRef])&&((CntSU[NextRef]+CntSU[1-NextRef])<Vsta.MaxLenSP)){
                x = ActiveSP[NextRef][CntSU[NextRef]].x;
                y = ActiveSP[NextRef][CntSU[NextRef]].y;
                CntSU[NextRef]++;
                if((!(Record[NextRef][y]&(1<<x)))&&((x<rw) && (y<rh))){
                    if (Vsta.SadType&1)/*chroma*/
                    {
                        Record[NextRef][y] |= (1<<x);
                    }
                    else
                    {
                        switch(Vsta.SrcType&3){
                        case 0:    Record[NextRef][y] |= (1<<x); break;
                        case 1:    Record[NextRef][y] |= (1<<x); Record[NextRef][y+1] |= (1<<x); break;
                        case 2:    Record[NextRef][y] |= (3<<x); break;
                        case 3:    Record[NextRef][y] |= (3<<x); Record[NextRef][y+1] |= (3<<x); break;
                        }
                    }
                    NextSU.x = x;
                    NextSU.y = y;
                    return 0;
                }
            }
            if(Dynamic[0]||Dynamic[1]){
                GetDynamicSU(1);
                if(NextRef<2)
                {
                    if(CheckNextReqValid==true){
                        if(NextRef==1){--AltMoreSU;} // Don't miss counting the last one
                        NextRef = 2;
                        bdualAdaptive = false;
                        return 0; 
                    }
                    x = NextSU.x;
                    y = NextSU.y;
                    if (Vsta.SadType&1)/*chroma*/
                    {
                        Record[NextRef][y] |= (1<<x);
                    }
                    else
                    {
                        switch(Vsta.SrcType&3){
                        case 0:    Record[NextRef][y] |= (1<<x); break;
                        case 1:    Record[NextRef][y] |= (1<<x); Record[NextRef][y+1] |= (1<<x); break;
                        case 2:    Record[NextRef][y] |= (3<<x); break;
                        case 3:    Record[NextRef][y] |= (3<<x); Record[NextRef][y+1] |= (3<<x); break;
                        }
                    }
                    bdualAdaptive = true;
                    return 0;
                }
                else{bdualAdaptive = false;}
            }
            NextRef = 3; bdualAdaptive = false; return 0; 
        case 6: case 7: // dual reference, dual starters, dual record
            NextRef = 1 - NextRef;
            while((NextRef>=0&&NextRef<2) && (CntSU[NextRef]<LenSP[NextRef])){
                x = ActiveSP[NextRef][CntSU[NextRef]].x;
                y = ActiveSP[NextRef][CntSU[NextRef]].y;
                CntSU[NextRef]++;
                if(!(Record[NextRef][y]&(1<<x))){
                    if (Vsta.SadType&1)/*chroma*/
                    {
                        Record[NextRef][y] |= (1<<x);
                    }
                    else
                    {
                        switch(Vsta.SrcType&3){
                        case 0:    Record[NextRef][y] |= (1<<x); break;
                        case 1:    Record[NextRef][y] |= (1<<x); Record[NextRef][y+1] |= (1<<x); break;
                        case 2:    Record[NextRef][y] |= (3<<x); break;
                        case 3:    Record[NextRef][y] |= (3<<x); Record[NextRef][y+1] |= (3<<x); break;
                        }
                    }
                    NextSU.x = x;
                    NextSU.y = y;
                    return 0;
                }
            }

            NextRef = 1 - NextRef;
            while((NextRef>=0&&NextRef<2) && (CntSU[NextRef]<LenSP[NextRef])){
                x = ActiveSP[NextRef][CntSU[NextRef]].x;
                y = ActiveSP[NextRef][CntSU[NextRef]].y;
                CntSU[NextRef]++;
                if(!(Record[NextRef][y]&(1<<x))){
                    if (Vsta.SadType&1)/*chroma*/
                    {
                        Record[NextRef][y] |= (1<<x);
                    }
                    else
                    {
                        switch(Vsta.SrcType&3){
                        case 0:    Record[NextRef][y] |= (1<<x); break;
                        case 1:    Record[NextRef][y] |= (1<<x); Record[NextRef][y+1] |= (1<<x); break;
                        case 2:    Record[NextRef][y] |= (3<<x); break;
                        case 3:    Record[NextRef][y] |= (3<<x); Record[NextRef][y+1] |= (3<<x); break;
                        }
                    }
                    NextSU.x = x;
                    NextSU.y = y;
                    return 0;
                }
            }
            if(Dynamic[0]||Dynamic[1]){
                GetDynamicSU(1);
                if(NextRef<2)
                {
                    if(CheckNextReqValid==true){
                        if(NextRef==1){--AltMoreSU;} // Don't miss counting the last one
                        NextRef = 2;
                        bdualAdaptive = false;
                        return 0; 
                    }
                    x = NextSU.x;
                    y = NextSU.y;
                    if (Vsta.SadType&1)/*chroma*/
                    {
                        Record[NextRef][y] |= (1<<x);
                    }
                    else
                    {
                        switch(Vsta.SrcType&3){
                        case 0:    Record[NextRef][y] |= (1<<x); break;
                        case 1:    Record[NextRef][y] |= (1<<x); Record[NextRef][y+1] |= (1<<x); break;
                        case 2:    Record[NextRef][y] |= (3<<x); break;
                        case 3:    Record[NextRef][y] |= (3<<x); Record[NextRef][y+1] |= (3<<x); break;
                        }
                    }
                    bdualAdaptive = true;
                    return 0;
                }
                else{bdualAdaptive = false;}
#ifdef VMETESTGEN
                Vout->Test.usedAdaptiveSU = true;
#endif
            }
            NextRef = 3; bdualAdaptive = false; return 0; 
    }
    NextRef = 3; bdualAdaptive = false; return ERR_ILLEGAL; 
}

/****************************************************************************************************/
void MEforGen75::PartitionBeforeModeDecision()
/****************************************************************************************************/
{
    int        i;
    int        x = 0, y = 0;
    U8        savedShapeMask = Vsta.ShapeMask;
    U16        mode = 0;
    U16        bestMajorRef = 0;
    U16        localMajorRef = 0;
    I32        BestDist = MBINVALID_DIST;
    I32        FreeMVDist = MBINVALID_DIST;

    U8        ref = 0;
    //Reset everytime we do partitioning. Need to report the result from only the last partitioning call
    Vout->ImeStop_MaxMV_AltNumSU &= 0xfd; //~CAPPED_MAXMV

    // reduce to maximal 4 candidates
    DistInter = MajorDist[4] = MajorDist[3] = MajorDist[2] = MajorDist[1] = MajorDist[0] = MBINVALID_DIST;
    MajorDistFreeMV[3] = MajorDistFreeMV[2] = MajorDistFreeMV[1] = MajorDistFreeMV[0] = MBINVALID_DIST;

    for (i =0 ; i<5; i++)
    {
        UniDirMajorDist[0][i] = MBINVALID_DIST;
        UniDirMajorDist[1][i] = MBINVALID_DIST;
        UniDirMajorMode[0][i] = 0;
        UniDirMajorMode[1][i] = 0;
    }

    // put Shape 16x16 in Candidate 0 
    if(!(Vsta.ShapeMask&SHP_NO_16X16)){ 
        if((Dist[0][BHXH]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][BHXH]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]))
        { 
            MajorDist[0] = Dist[0][BHXH] + L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[0] = 0x100; 
            bestMajorRef = 0;
        }else{                               
            MajorDist[0] = Dist[1][BHXH] + (!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[0] = 0x200; 
            bestMajorRef = 1;
        }
        if((MajorDist[0]+= LutMode[LUTMODE_INTER_16x16])>MBMAXDIST) MajorDist[0] = MBMAXDIST;
        DistInter = MajorDist[0]; 
        UniOpportunitySad[0][0] = Dist[0][BHXH] + L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD] + LutMode[LUTMODE_INTER_16x16];
        UniOpportunitySad[1][0] = Dist[1][BHXH] + (!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD] + LutMode[LUTMODE_INTER_16x16];

        UniDirMajorDist[0][0] = Dist[0][BHXH] + L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD] + LutMode[LUTMODE_INTER_16x16];
        UniDirMajorDist[1][0] = Dist[1][BHXH] + (!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD] + LutMode[LUTMODE_INTER_16x16];

        UniDirMajorMode[0][0] = 0x100;
        UniDirMajorMode[1][0] = 0x200;
    }
    // put Shape 16x8 in Candidate 1 & Shape 8x16 in Cadidate 2:  
    if(Vsta.BidirMask&UNIDIR_NO_MIX){ // no unidirectional mix
        if(!(Vsta.ShapeMask&SHP_NO_16X8)){ 
            if((x = LutMode[LUTMODE_INTER_16x8]+Dist[0][BHX8]+Dist[0][BHX8+1]+   L0_BWD_Penalty *(LutMode[LUTMODE_INTER_BWD]<<1))>MBMAXDIST) x = MBMAXDIST; 
            if((y = LutMode[LUTMODE_INTER_16x8]+Dist[1][BHX8]+Dist[1][BHX8+1]+ (!L0_BWD_Penalty)*(LutMode[LUTMODE_INTER_BWD]<<1))>MBMAXDIST) y = MBMAXDIST;
            if(x<=y){ MajorDist[1] = x; MajorMode[1] = 0x1100;  localMajorRef = 0;}
            else{        MajorDist[1] = y; MajorMode[1] = 0x2200;  localMajorRef = 1;}
            if((MajorDist[1]<DistInter) || ( (MajorDist[1]==DistInter) && (localMajorRef < bestMajorRef) )) { DistInter = MajorDist[1]; bestMajorRef = localMajorRef; }
            UniOpportunitySad[0][1] = x;
            UniOpportunitySad[1][1] = y;

            UniDirMajorDist[0][1] = x;
            UniDirMajorDist[1][1] = y;

            UniDirMajorMode[0][1] = 0x1100;
            UniDirMajorMode[1][1] = 0x2200;
        }
        else if (ShapeValidMVCheck[1])
        {
            if((x = LutMode[LUTMODE_INTER_16x8]+Dist[0][BHX8]+Dist[0][BHX8+1]+   L0_BWD_Penalty *(LutMode[LUTMODE_INTER_BWD]<<1))>MBMAXDIST) x = MBMAXDIST; 
            if((y = LutMode[LUTMODE_INTER_16x8]+Dist[1][BHX8]+Dist[1][BHX8+1]+ (!L0_BWD_Penalty)*(LutMode[LUTMODE_INTER_BWD]<<1))>MBMAXDIST) y = MBMAXDIST;
            if(x<=y){ MajorDistFreeMV[1] = x; }
            else{        MajorDistFreeMV[1] = y; }
            UniOpportunitySad[0][1] = x;
            UniOpportunitySad[1][1] = y;
        }
        if(!(Vsta.ShapeMask&SHP_NO_8X16)){ 
            if((x = LutMode[LUTMODE_INTER_8x16]+Dist[0][B8XH]+Dist[0][B8XH+1]+   L0_BWD_Penalty *(LutMode[LUTMODE_INTER_BWD]<<1))>MBMAXDIST) x = MBMAXDIST; 
            if((y = LutMode[LUTMODE_INTER_8x16]+Dist[1][B8XH]+Dist[1][B8XH+1]+ (!L0_BWD_Penalty)*(LutMode[LUTMODE_INTER_BWD]<<1))>MBMAXDIST) y = MBMAXDIST; 
            if(x<=y){ MajorDist[2] = x; MajorMode[2] = 0x0500; localMajorRef = 0;}
            else{        MajorDist[2] = y; MajorMode[2] = 0x0A00; localMajorRef = 1;}
            if((MajorDist[2]<DistInter) || ((MajorDist[2]==DistInter) && (localMajorRef < bestMajorRef) )) {DistInter = MajorDist[2]; bestMajorRef = localMajorRef; }
            UniOpportunitySad[0][2] = x;
            UniOpportunitySad[1][2] = y;
            UniDirMajorDist[0][2] = x;
            UniDirMajorDist[1][2] = y;
            UniDirMajorMode[0][2] = 0x0500;
            UniDirMajorMode[1][2] = 0x0A00;
        }
        else if (ShapeValidMVCheck[2])
        {
            if((x = LutMode[LUTMODE_INTER_8x16]+Dist[0][B8XH]+Dist[0][B8XH+1]+   L0_BWD_Penalty *(LutMode[LUTMODE_INTER_BWD]<<1))>MBMAXDIST) x = MBMAXDIST; 
            if((y = LutMode[LUTMODE_INTER_8x16]+Dist[1][B8XH]+Dist[1][B8XH+1]+(!L0_BWD_Penalty)*(LutMode[LUTMODE_INTER_BWD]<<1))>MBMAXDIST) y = MBMAXDIST; 
            if(x<=y){ MajorDistFreeMV[2] = x; }
            else{        MajorDistFreeMV[2] = y; }
            UniOpportunitySad[0][2] = x;
            UniOpportunitySad[1][2] = y;
        }
        if(!(Vsta.ShapeMask&SHP_NO_8X8)){ // 8x8 frame case only
            if((x = (LutMode[LUTMODE_INTER_8x8q]<<2)+Dist[0][B8X8]+Dist[0][B8X8+1]+Dist[0][B8X8+2]+Dist[0][B8X8+3]+   L0_BWD_Penalty *(LutMode[LUTMODE_INTER_BWD]<<2))>MBMaxDist) x = MBMaxDist; 
            if((y = (LutMode[LUTMODE_INTER_8x8q]<<2)+Dist[1][B8X8]+Dist[1][B8X8+1]+Dist[1][B8X8+2]+Dist[1][B8X8+3]+(!L0_BWD_Penalty)*(LutMode[LUTMODE_INTER_BWD]<<2))>MBMaxDist) y = MBMaxDist; 
            if(x<=y){ MajorDist[4] = x; MajorMode[4] = 0x5500; localMajorRef = 0;}
            else{        MajorDist[4] = y; MajorMode[4] = 0xAA00; localMajorRef = 1;}
            if((MajorDist[4]<DistInter) || ( (MajorDist[4]==DistInter) && (localMajorRef < bestMajorRef) )) {DistInter = MajorDist[4]; bestMajorRef = localMajorRef; }

            UniDirMajorDist[0][4] = x;
            UniDirMajorDist[1][4] = y;
            UniDirMajorMode[0][4] = 0x5500;
            UniDirMajorMode[1][4] = 0xAA00;
        }
    }
    else{
        if(!(Vsta.ShapeMask&SHP_NO_16X8)){ 
            MajorDist[1] = LutMode[LUTMODE_INTER_16x8];
            if((Dist[0][BHX8]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][BHX8  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[1]+= Dist[0][BHX8]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[1] = 0x0100; }
            else{                                  MajorDist[1]+= Dist[1][BHX8]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[1] = 0x0200; }
            if((Dist[0][BHX8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][BHX8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[1]+= Dist[0][BHX8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[1]|= 0x1000; }
            else{                                  MajorDist[1]+= Dist[1][BHX8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[1]|= 0x2000; }
            if(MajorDist[1]<DistInter){
                if(MajorDist[1]<MBMAXDIST) DistInter = MajorDist[1];
                else DistInter = MajorDist[1] = MBMAXDIST;
            }
        }
        else if (ShapeValidMVCheck[1])
        {
            MajorDistFreeMV[1] = LutMode[LUTMODE_INTER_16x8];
            if((Dist[0][BHX8]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][BHX8  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDistFreeMV[1]+= Dist[0][BHX8]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD];}
            else{                                  MajorDistFreeMV[1]+= Dist[1][BHX8  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD];}
            if((Dist[0][BHX8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][BHX8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDistFreeMV[1]+= Dist[0][BHX8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD];}
            else{                                  MajorDistFreeMV[1]+= Dist[1][BHX8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD];}
        }
        if(!(Vsta.ShapeMask&SHP_NO_8X16)){ 
            MajorDist[2] = LutMode[LUTMODE_INTER_8x16];
            if((Dist[0][B8XH]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8XH  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[2]+= Dist[0][B8XH]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[2] = 0x0100; }
            else{                                  MajorDist[2]+= Dist[1][B8XH  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[2] = 0x0200; }
            if((Dist[0][B8XH+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8XH+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[2]+= Dist[0][B8XH+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[2]|= 0x0400; }
            else{                                  MajorDist[2]+= Dist[1][B8XH+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[2]|= 0x0800; }
            if(MajorDist[2]<DistInter){
                if(MajorDist[2]<MBMAXDIST) DistInter = MajorDist[2];
                else DistInter = MajorDist[2] = MBMAXDIST;
            }
        }
        else if (ShapeValidMVCheck[2])
        {
            MajorDistFreeMV[2] = LutMode[LUTMODE_INTER_8x16];
            if((Dist[0][B8XH]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8XH  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDistFreeMV[2]+= Dist[0][B8XH]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD];}
            else{                                  MajorDistFreeMV[2]+= Dist[1][B8XH  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD];}
            if((Dist[0][B8XH+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8XH+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDistFreeMV[2]+= Dist[0][B8XH+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD];}
            else{                                  MajorDistFreeMV[2]+= Dist[1][B8XH+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD];}
        }
        if(!(Vsta.ShapeMask&SHP_NO_8X8)){ // 8x8 frame case only
            MajorDist[4] = (LutMode[LUTMODE_INTER_8x8q]<<2);
            if((Dist[0][B8X8  ]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8X8  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[4]+= Dist[0][B8X8  ]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] = 0x0100; }
            else{                                  MajorDist[4]+= Dist[1][B8X8  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] = 0x0200; }
            if((Dist[0][B8X8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8X8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[4]+= Dist[0][B8X8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] |= 0x0400; }
            else{                                  MajorDist[4]+= Dist[1][B8X8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] |= 0x0800; }
            if((Dist[0][B8X8+2]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8X8+2]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[4]+= Dist[0][B8X8+2]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] |= 0x1000; }
            else{                                  MajorDist[4]+= Dist[1][B8X8+2]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] |= 0x2000; }
            if((Dist[0][B8X8+3]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8X8+3]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[4]+= Dist[0][B8X8+3]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] |= 0x4000; }
            else{                                  MajorDist[4]+= Dist[1][B8X8+3]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] |= 0x8000; }
            if(MajorDist[4]<DistInter){
                if(MajorDist[4]<MBMaxDist) DistInter = MajorDist[4];
                else DistInter = MajorDist[4] = MBMaxDist;
            }
        }
    }
    Pick8x8Minor = false;
    // Put Best Minor in Candidate 3 in pure minor cases.
    if((Vsta.ShapeMask&0x78)!=0x78){ 
        SelectBestMinorShape(&MajorDist[3],&MajorMode[3], 0);
        if(MajorDist[3]<DistInter){
            if(MajorDist[3]<MBMaxDist) DistInter = MajorDist[3];
            else DistInter = MajorDist[3] = MBMaxDist;
        }
        if(MajorDist[3]==DistInter && (Vsta.BidirMask&UNIDIR_NO_MIX)){
            if (bestMajorRef==1 && ((MajorMode[3]&0x5500)==0x5500)) {
                // Set Pick8x8Minor = true when tie break (MajorDist[3]==DistInter and UNIDIR mix is not allowed) 
                // is between major partitions and 8x8 shapes (can have minor shapes). Also the major is coming from L1
                // and 8x8 with the same distortion is coming from L0. 
                // This will allow the VME to pick L0 distortion over the same L1 distortion in this specific condition
                Pick8x8Minor = true;
                if(MajorDist[3]<MBMaxDist) DistInter = MajorDist[3];
                else DistInter = MajorDist[3] = MBMaxDist;
            }
        }
    }
    else if(ShapeValidMVCheck[3])
    {
        Vsta.ShapeMask = OrigShapeMask;
        SelectBestMinorShape(&MajorDistFreeMV[3],&MajorMode[3], 1);
        Vsta.ShapeMask = savedShapeMask;
    }
    // put Shape 8x8 in Candidate 3 otherwise
    // For FIELD case, merge Shapes 16x8 & 8x16 into Candidate 1, and put Best Field in Cndidate 2.
    else{  // with possible filed modes
        if(Vsta.SrcType&(ST_FIELD_16X8|ST_FIELD_8X8)){ // Support Field Mode
            // merge frame candidates 1,2 into 1, and use cadidate 2 for field cases .
            if(MajorDist[2]<MajorDist[1]){ MajorDist[1]=MajorDist[2]; MajorMode[1]=MajorMode[2]; }
            MajorDist[2] = MBMAXDIST;
        }
        if(Vsta.BidirMask&UNIDIR_NO_MIX){ // no unidirectional mix
            if(Vsta.SrcType&ST_FIELD_16X8){ 
                if((x = LutMode[LUTMODE_INTER_16x8_FIELD]+Dist[0][FHX8]+Dist[0][FHX8+1])>MBMAXDIST) x = MBMAXDIST; 
                if((y = LutMode[LUTMODE_INTER_16x8_FIELD]+Dist[1][FHX8]+Dist[1][FHX8+1])>MBMAXDIST) y = MBMAXDIST;  
                if(x<=y){ MajorDist[2] = x; MajorMode[2] = 0x3F11; }
                else{        MajorDist[2] = y; MajorMode[2] = 0x3F22; }
                if(MajorDist[2]<DistInter) DistInter = MajorDist[2];
            }
            if(Vsta.SrcType&ST_FIELD_8X8){ 
                if((x = LutMode[LUTMODE_INTER_8x8_FIELD]+Dist[0][F8X8]+Dist[0][F8X8+1]+Dist[0][F8X8+2]+Dist[0][F8X8+3])>MBMAXDIST) x = MBMAXDIST; 
                if((y = LutMode[LUTMODE_INTER_8x8_FIELD]+Dist[1][F8X8]+Dist[1][F8X8+1]+Dist[1][F8X8+2]+Dist[1][F8X8+3])>MBMAXDIST) y = MBMAXDIST;  
                if(x<=MajorDist[2]){ MajorDist[2] = x; MajorMode[2] = 0x3F55; }
                if(y<=MajorDist[2]){ MajorDist[2] = y; MajorMode[2] = 0x3FAA; }
                if(MajorDist[2]<DistInter) DistInter = MajorDist[2];
            }
            if(!(Vsta.ShapeMask&8)){ // 8x8 frame case only
                if((x = (LutMode[LUTMODE_INTER_8x8q]<<2)+Dist[0][B8X8]+Dist[0][B8X8+1]+Dist[0][B8X8+2]+Dist[0][B8X8+3])>MBMAXDIST) x = MBMAXDIST; 
                if((y = (LutMode[LUTMODE_INTER_8x8q]<<2)+Dist[1][B8X8]+Dist[1][B8X8+1]+Dist[1][B8X8+2]+Dist[1][B8X8+3])>MBMAXDIST) y = MBMAXDIST; 
                if(x<=y){ MajorDist[4] = x; MajorMode[4] = 0x5500; }
                else{        MajorDist[4] = y; MajorMode[4] = 0xAA00; }
                if(MajorDist[4]<DistInter) DistInter = MajorDist[4];
            }
        }
        else{ // Support Field Mode with mixing direction
            if(Vsta.SrcType&ST_FIELD_16X8){ 
                MajorDist[2] = LutMode[LUTMODE_INTER_16x8_FIELD];
                if(Dist[0][FHX8  ]<=Dist[1][FHX8  ]){ MajorDist[2]+= Dist[0][FHX8  ]; MajorMode[2] = 0x3F01; }
                else{                                  MajorDist[2]+= Dist[1][FHX8  ]; MajorMode[2] = 0x3F02; }
                if(Dist[0][FHX8+1]<=Dist[1][FHX8+1]){ MajorDist[2]+= Dist[0][FHX8+1]; MajorMode[2]|= 0x3F10; }
                else{                                  MajorDist[2]+= Dist[1][FHX8+1]; MajorMode[2]|= 0x3F20; }
                if(MajorDist[2]<DistInter){
                    if(MajorDist[2]<MBMAXDIST) DistInter = MajorDist[2];
                    else DistInter = MajorDist[2] = MBMAXDIST;
                }
            }
            if(Vsta.SrcType&ST_FIELD_8X8){ 
                x = LutMode[LUTMODE_INTER_8x8_FIELD];
                if(Dist[0][F8X8  ]<=Dist[1][F8X8  ]){ x+= Dist[0][F8X8  ]; y = 0x3F01; }
                else{                                  x+= Dist[1][F8X8  ]; y = 0x3F02; }
                if(Dist[0][F8X8+1]<=Dist[1][F8X8+1]){ x+= Dist[0][F8X8+1]; y|= 0x3F04; }
                else{                                  x+= Dist[1][F8X8+1]; y|= 0x3F08; }
                if(Dist[0][F8X8+2]<=Dist[1][F8X8+2]){ x+= Dist[0][F8X8+2]; y|= 0x3F10; }
                else{                                  x+= Dist[1][F8X8+2]; y|= 0x3F20; }
                if(Dist[0][F8X8+3]<=Dist[1][F8X8+3]){ x+= Dist[0][F8X8+3]; y|= 0x3F40; }
                else{                                  x+= Dist[1][F8X8+3]; y|= 0x3F80; }
                if(x>MBMAXDIST) x = MBMAXDIST;
                if(x<DistInter) DistInter = x;
                if(x<MajorDist[2]){ MajorDist[2]=x; MajorMode[2]=y; }
            }
            if(!(Vsta.ShapeMask&8)){ // 8x8 frame case only
                MajorDist[4] = (LutMode[LUTMODE_INTER_8x8q]<<2);
                if((Dist[0][B8X8  ]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8X8  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[4]+= Dist[0][B8X8  ]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] = 0x0100; }
                else{                                  MajorDist[4]+= Dist[1][B8X8  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[4] = 0x0200; }
                if((Dist[0][B8X8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8X8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[4]+= Dist[0][B8X8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[4]|= 0x0400; }
                else{                                  MajorDist[4]+= Dist[1][B8X8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[4]|= 0x0800; }
                if((Dist[0][B8X8+2]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8X8+2]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[4]+= Dist[0][B8X8+2]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[4]|= 0x1000; }
                else{                                  MajorDist[4]+= Dist[1][B8X8+2]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[4]|= 0x2000; }
                if((Dist[0][B8X8+3]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][B8X8+3]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])){ MajorDist[4]+= Dist[0][B8X8+3]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD]; MajorMode[4]|= 0x4000; }
                else{                                  MajorDist[4]+= Dist[1][B8X8+3]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]; MajorMode[4]|= 0x8000; }
                if(MajorDist[4]>MBMAXDIST) MajorDist[4] = MBMAXDIST;
                if(MajorDist[4]<DistInter) DistInter = MajorDist[4];
            }
        }
    }

    if(Vsta.BidirMask&UNIDIR_NO_MIX)
    { // no unidirectional mix
        BestDist = MajorDist[2];
        mode = MajorMode[2];
        for(i = 2; i >=0; i--) // compare in this order 8x16, 16x8, 16x16
        {
            if((MajorDist[i] < BestDist) || ((MajorDist[i] == BestDist) && ((MajorMode[i]&0x0300) <= (mode&0x0300))) )
            {
                BestDist = MajorDist[i];
                mode = MajorMode[i];
            }
        }
        if((MajorDist[3]<BestDist) || ((MajorDist[3] == BestDist) && ((MajorMode[3]&0x0300) < (mode&0x0300))) ){
            BestDist = MajorDist[3];
            mode = MajorMode[3];
        }
        /*if(MajorDist[3]==BestDist){
            if (bestMajorRef==1 && ((MajorMode[3]&0x5500)==0x5500)) {
                BestDist = MajorDist[3];
                mode = MajorMode[3];
            }
        }*/
        MajorMode[0]=mode;
        ref = ((mode>>8)&0x3) - 1;
        for(i = 0; i < 4; i++)
        {
            if(UniOpportunitySad[ref][i]<FreeMVDist)
                FreeMVDist = UniOpportunitySad[ref][i];
        }
        
    }
    else
    {
        for(i = 1; i < 4; i++)
        {
            if(MajorDistFreeMV[i]<FreeMVDist)
                FreeMVDist = MajorDistFreeMV[i];
        }
    }

    if(FreeMVDist<DistInter)
        Vout->ImeStop_MaxMV_AltNumSU |= CAPPED_MAXMV;

    // For reduced macroblock
    for(i=0; i<5;i++)
    { 
        ScaleDist(MajorDist[i]);
        ScaleDist(UniDirMajorDist[0][i]);
        ScaleDist(UniDirMajorDist[1][i]);
    }
    ScaleDist(DistInter);
}


bool MEforGen75::SelectBestAndAltShape(U8 RefIndex, U8& BestShape, U8& AltShape)
{    
    U8 i;
    U16 MinSad = MBMAXDIST;
    BestShape = 0;
    AltShape = 0;
    
    if (RefIndex >= 2)
    {
        return false;
    }

    for (i=0; i<5; i++)
    {
        if (UniDirMajorDist[RefIndex][i] < MinSad)
        {
            MinSad = UniDirMajorDist[RefIndex][i];
            BestShape = i;
        }
    }

    if(BestShape == 0){
        // Best=16x16, Alt=8x8
        AltShape = 4;         
    }
    else if(BestShape == 1 || BestShape == 2){
        // Best=16x8 or 8x16, Alt=min(16x16,8x8)
        if(UniDirMajorDist[RefIndex][0]<=UniDirMajorDist[RefIndex][4]){
            AltShape = 0;             
        }
        else{
            AltShape = 4;             
        }        
    }
    else{
        // Best=8x8 minor or 8x8, Alt=16x16 
        AltShape = 0;
    }

    return true;
}


/****************************************************************************************************/
void MEforGen75::MajorModeDecision()//U8&    disableAltMask, U8& AltNumMVsRequired, bool& disableAltCandidate)
/****************************************************************************************************/
{    
    /* // Removal of alternate candidate for Gen7.5
    disableAltCandidate = false;
    if (Vsta.VmeFlags&VF_EXTRA_CANDIDATE && Vsta.BidirMask&UNIDIR_NO_MIX)
    {
        // Major mode decision on L0 and L1
        U8 BestShape[2] = {0};
        U8 AltShape[2] = {0};
        for (U8 i=0; i<2; i++)
        {
            SelectBestAndAltShape(i, BestShape[i], AltShape[i]);
        }

        // Final mode decision
        U8 FinalBestShape = 0;
        U8 FinalAltShape = 0;

        // Best mode
        if (UniDirMajorDist[1][BestShape[1]]<UniDirMajorDist[0][BestShape[0]])
        {
            FinalBestShape = BestShape[1];    
            MajorDist[0] = UniDirMajorDist[1][FinalBestShape];
            MajorMode[0] = UniDirMajorMode[1][FinalBestShape];
        }
        else
        {
            FinalBestShape = BestShape[0];
            MajorDist[0] = UniDirMajorDist[0][FinalBestShape];
            MajorMode[0] = UniDirMajorMode[0][FinalBestShape];
        }

        // Alternate mode
        if (UniDirMajorDist[1][AltShape[1]]<UniDirMajorDist[0][AltShape[0]])
        {
            FinalAltShape = AltShape[1];

            if ((FinalAltShape == FinalBestShape) || ((FinalBestShape == 3)&&(FinalAltShape == 4)))
            {
                //Disable Alternate if shapes are same
                //FinalAltShape = AltShape[0];
                disableAltCandidate = true;
            }

            MajorDist[1] = UniDirMajorDist[1][FinalAltShape];
            MajorMode[1] = UniDirMajorMode[1][FinalAltShape];

        }
        else
        {
            FinalAltShape = AltShape[0];
            if ((FinalAltShape == FinalBestShape) || ((FinalBestShape == 3)&&(FinalAltShape == 4)))
            {
                //Disable Alternate if shapes are same
                //FinalAltShape = AltShape[1];
                disableAltCandidate = true;
            }

            MajorDist[1] = UniDirMajorDist[0][FinalAltShape];
            MajorMode[1] = UniDirMajorMode[0][FinalAltShape];
        }

        if(FinalAltShape == 0){
            // 16x16
            disableAltMask = SHP_NO_16X16;
            AltNumMVsRequired = 1;
        }
        else { 
            // 8x8
            disableAltMask = SHP_NO_8X8;
            AltNumMVsRequired = 4;
        } 
    }
    else 
    {*/
        U16 tmpMajorMode1 = 0;
        I32 tmpMajorSad1 = 0;

        if(MajorDist[0]==DistInter && (!Pick8x8Minor)){
            MajorDist[1] = MajorDist[4];    MajorMode[1] = MajorMode[4];
            //disableAltMask = SHP_NO_8X8;
            //AltNumMVsRequired = 4;
        }
        
        if(MajorDist[1]==DistInter && (!Pick8x8Minor)){
            if ((MajorDist[1] < MajorDist[0]) || (MajorDist[1] == MajorDist[0] && (Vsta.BidirMask&UNIDIR_NO_MIX) 
                && (((MajorMode[1]>>8)&0x3)<((MajorMode[0]>>8)&0x3)))){ // if equal, and no uni mix, pick L0 record over L1
                    if(MajorDist[0]<=MajorDist[4]){
                        tmpMajorSad1 = MajorDist[0];    tmpMajorMode1 = MajorMode[0];
                        //disableAltMask = SHP_NO_16X16;
                        //AltNumMVsRequired = 1;
                    }
                    else{
                        tmpMajorSad1 = MajorDist[4];    tmpMajorMode1 = MajorMode[4];
                        //disableAltMask = SHP_NO_8X8;
                        //AltNumMVsRequired = 4;
                    }
                    MajorDist[0] = MajorDist[1];    MajorMode[0] = MajorMode[1];
                    MajorDist[1] = tmpMajorSad1;    MajorMode[1] = tmpMajorMode1;
            }
        }
        
        if(MajorDist[2]==DistInter && (!Pick8x8Minor)){
            if ((MajorDist[2] < MajorDist[0]) || (MajorDist[2] == MajorDist[0] && (Vsta.BidirMask&UNIDIR_NO_MIX) 
                && (((MajorMode[2]>>8)&0x3)<((MajorMode[0]>>8)&0x3)))){ // if equal, and no uni mix, pick L0 record over L1
                    if(MajorDist[0]<=MajorDist[4]){
                        MajorDist[1] = MajorDist[0];    MajorMode[1] = MajorMode[0];
                        //disableAltMask = SHP_NO_16X16;
                        //AltNumMVsRequired = 1;
                    }
                    else{
                        MajorDist[1] = MajorDist[4];    MajorMode[1] = MajorMode[4];
                        //disableAltMask = SHP_NO_8X8;
                        //AltNumMVsRequired = 4;
                    }
                    MajorDist[0] = MajorDist[2];    MajorMode[0] = MajorMode[2];
            }
        }
        
        //Note: MajorDist[4] stores 8x8, MajorDist[3] stores minor
        if (MajorDist[4]==DistInter)
        {
            if ((MajorDist[4] < MajorDist[0]) || (MajorDist[4] == MajorDist[0] && (Vsta.BidirMask&UNIDIR_NO_MIX) && (!(Vsta.SadType & INTER_CHROMA_MODE))
                && (((MajorMode[4]>>8)&0x3)<((MajorMode[0]>>8)&0x3))))
            {// if equal, and no uni mix , pick L0 record over L1(luma only). chroma, pick larger shaper over direction. will pick the same way for luma and chroma in Gen8
                MajorDist[1] = MajorDist[0];    MajorMode[1] = MajorMode[0];
                MajorDist[0] = MajorDist[4];    MajorMode[0] = MajorMode[4];
            }
        }

        if(MajorDist[3]==DistInter){
            if ((MajorDist[3] < MajorDist[0]) || (MajorDist[3] == MajorDist[0] && (Vsta.BidirMask&UNIDIR_NO_MIX) && (!(Vsta.SadType & INTER_CHROMA_MODE))
                && (((MajorMode[3]>>8)&0x3)<((MajorMode[0]>>8)&0x3)))){ // if equal, and no uni mix, pick L0 record over L1 (luma only)
                    MajorDist[1] = MajorDist[0];    MajorMode[1] = MajorMode[0];
                    MajorDist[0] = MajorDist[3];    MajorMode[0] = MajorMode[3];
            }
        }
}

/*****************************************************************************************************/
void MEforGen75::PreBMEPartition(I32 *dist, U16 *mode, bool benablecopy)
/*****************************************************************************************************/
{
    
    int    i,j,k,m,s,df,d;
    m = *mode;
    int bwdCost = LutMode[LUTMODE_INTER_BWD];
    bool bwd_penalty = false;
    int additionalCost = 0;
    bool bidirValid = false;
    bool InterChromaEn = (Vsta.SadType & INTER_CHROMA_MODE);
//    bool checkPruning = (Vsta.BidirMask&B_PRUNING_ENABLE) ? true : false;
    int tempd = 0;
    I32 useDist = 0;
    I32 useDist8x16_0 = 0;
    I32 useDist8x16_1 = 0;
    I32 useDist16x8_0 = 0;
    I32 useDist16x8_1 = 0;
    I32 useDistF16x8_0 = 0;
    I32 useDistF16x8_1 = 0;
    I32 useDistF8x8_0 = 0;
    I32 useDistF8x8_1 = 0;
    I32 useDistF8x8_2 = 0;
    I32 useDistF8x8_3 = 0;
    I32 useDistM8x8 = 0;
    I32 useDistM8x4_0 = 0;
    I32 useDistM8x4_1 = 0;
    I32 useDistM4x8_0 = 0;
    I32 useDistM4x8_1 = 0;
    I32 useDistM4x4_0 = 0; 
    I32 useDistM4x4_1 = 0;
    I32 useDistM4x4_2 = 0; 
    I32 useDistM4x4_3 = 0;
    // I32 Blk8x8MaxDist = ((Vsta.SrcType&3) == 3)?0xffff:0x3fff;
    I32 Blk8x8MaxDist = 0xffff;

    if(!(Vsta.VmeModes&4))    Vsta.BidirMask |= 0x0F;    // No bidirectional for single reference

    if(*mode<0x0500){ //16x16
        useDist=Dist[0][BHXH];
        if(false){//CheckBidirValid(m, checkPruning, true)){             
            if(Vsta.BidirMask&UNIDIR_NO_MIX){ // if Unidirectional mixing is not allowed, retain previous direction
                k = (!(*mode&0x0100));
                if (benablecopy) 
                    useDist=Dist[0][BHXH]=Dist[k][BHXH]; 
                else 
                    useDist=Dist[k][BHXH];
                bwd_penalty = k ? !L0_BWD_Penalty : L0_BWD_Penalty;
                additionalCost = bwd_penalty*bwdCost;
            }
            else{
                if((Dist[0][BHXH]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=(Dist[1][BHXH]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD])) 
                { 
                    m = 0x0100;  additionalCost = L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD];
                }else
                { 
                    if (benablecopy) 
                        useDist=Dist[0][BHXH]=Dist[1][BHXH]; 
                    else 
                        useDist=Dist[1][BHXH];
                    m = 0x0200; additionalCost = (!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD];
                }
            }
        }
        else{
            k = (!(*mode&0x0100)); 
            if (benablecopy) 
                useDist=Dist[0][BHXH]=Dist[k][BHXH]; 
            else 
                useDist=Dist[k][BHXH];
            bwd_penalty = k ? !L0_BWD_Penalty : L0_BWD_Penalty;
            additionalCost = bwd_penalty*LutMode[LUTMODE_INTER_BWD];
        }
        d = LutMode[LUTMODE_INTER_16x16]+useDist+additionalCost;
        //if(d<=DistInter){*dist = d; DistInter = *dist; *mode = m;}
        if(d<=*dist){*dist = d; *mode = m;}
    }
    else if(*mode<0x0F00){ // 8x16
        useDist8x16_0=Dist[0][B8XH];
        useDist8x16_1=Dist[0][B8XH+1];
        if(false){//CheckBidirValid(m, checkPruning, true)){             
            if(Vsta.BidirMask&UNIDIR_NO_MIX){ // if Unidirectional mixing is not allowed, retain previous direction
                k = (!(*mode&0x0100));
                if (benablecopy){
                    useDist8x16_0=Dist[0][B8XH]=Dist[k][B8XH];
                    useDist8x16_1=Dist[0][B8XH+1]=Dist[k][B8XH+1];
                }
                else {
                    useDist8x16_0=Dist[k][B8XH];
                    useDist8x16_1=Dist[k][B8XH+1];
                }
            }
            else{
                if((Dist[0][B8XH  ]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=Dist[1][B8XH  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]) { m = 0x0100; }
                else{ 
                    if (benablecopy){
                        useDist8x16_0=Dist[0][B8XH]=Dist[1][B8XH]; 
                    }
                    else {
                        useDist8x16_0=Dist[1][B8XH]; 
                    }
                    m = 0x0200; 
                }
                if((Dist[0][B8XH+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=Dist[1][B8XH+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]) { m |= 0x0400; }
                else{ 
                    if (benablecopy){
                        useDist8x16_1=Dist[0][B8XH+1]=Dist[1][B8XH+1]; 
                    }
                    else {
                        useDist8x16_1=Dist[1][B8XH+1]; 
                    }
                    m |= 0x0800; 
                }
            }
        }
        else{
            k = (!(*mode&0x0100)); 
            if (benablecopy) {
                useDist8x16_0=Dist[0][B8XH]=Dist[k][B8XH];
            }
            else {
                useDist8x16_0=Dist[k][B8XH];
            }
            k = (!(*mode&0x0400)); 
            if (benablecopy) {
                useDist8x16_1=Dist[0][B8XH+1]=Dist[k][B8XH+1];
            }
            else {
                useDist8x16_1=Dist[k][B8XH+1];
            }
        }
        additionalCost = 0;
        if((m&0x0200&&(!L0_BWD_Penalty)) || ((!(m&0x0200))&&L0_BWD_Penalty))
            additionalCost +=1;
        if((m&0x0800&&(!L0_BWD_Penalty)) || ((!(m&0x0800))&&L0_BWD_Penalty))
            additionalCost +=1;
        additionalCost *= LutMode[LUTMODE_INTER_BWD];
        d = LutMode[LUTMODE_INTER_8x16]+useDist8x16_0+useDist8x16_1+additionalCost;
        ScaleDist(d);
        //if(d<=DistInter){*dist = d; DistInter = *dist; *mode = m;}
        if(d<=*dist){*dist = d; *mode = m;}
    }
    else if(*mode<0x3400){ // 16x8
        useDist16x8_0=Dist[0][BHX8];
        useDist16x8_1=Dist[0][BHX8+1];
        if(false){//CheckBidirValid(m, checkPruning, true)){
            if(Vsta.BidirMask&UNIDIR_NO_MIX){ // if Unidirectional mixing is not allowed, retain previous direction
                k = (!(*mode&0x0100)); 
                if (benablecopy) {
                    useDist16x8_0=Dist[0][BHX8]=Dist[k][BHX8];
                    useDist16x8_1=Dist[0][BHX8+1]=Dist[k][BHX8+1];
                }
                else {
                    useDist16x8_0=Dist[k][BHX8];
                    useDist16x8_1=Dist[k][BHX8+1];
                }
            }
            else{
                if((Dist[0][BHX8  ]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=Dist[1][BHX8  ]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]) { m = 0x0100; }
                else{ 
                    if (benablecopy) {
                        useDist16x8_0=Dist[0][BHX8  ]=Dist[1][BHX8  ];
                    }
                    else {
                        useDist16x8_0=Dist[1][BHX8  ];
                    }
                    m = 0x0200; 
                }
                if((Dist[0][BHX8+1]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])<=Dist[1][BHX8+1]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]) { m |= 0x1000; }
                else{ 
                    if (benablecopy) {
                        useDist16x8_1=Dist[0][BHX8+1]=Dist[1][BHX8+1]; 
                    }
                    else {
                        useDist16x8_1=Dist[1][BHX8+1];
                    }
                    m |= 0x2000; 
                }
            }
        }
        else{
            k = (!(*mode&0x0100)); 
            if (benablecopy) {
                useDist16x8_0=Dist[0][BHX8]=Dist[k][BHX8];
            }
            else {
                useDist16x8_0=Dist[k][BHX8];
            }
            k = (!(*mode&0x1000)); 
            if (benablecopy) {
                useDist16x8_1=Dist[0][BHX8+1]=Dist[k][BHX8+1];
            }
            else{
                useDist16x8_1=Dist[k][BHX8+1];
            }
        }
        additionalCost = 0;
        if((m&0x0200&&(!L0_BWD_Penalty)) || ((!(m&0x0200))&&L0_BWD_Penalty))
            additionalCost +=1;
        if((m&0x2000&&(!L0_BWD_Penalty)) || ((!(m&0x2000))&&L0_BWD_Penalty))
            additionalCost +=1;
        additionalCost *= LutMode[LUTMODE_INTER_BWD];

        d = LutMode[LUTMODE_INTER_16x8]+useDist16x8_0+useDist16x8_1+additionalCost; 
        ScaleDist(d);
        //if(d<=DistInter){*dist = d; DistInter = *dist; *mode = m;}
        if(d<=*dist){*dist = d; *mode = m;}
    }
    else if(*mode<0x3F40){ // 16x8 Field Modes
        useDistF16x8_0=Dist[0][FHX8]; 
        useDistF16x8_1=Dist[0][FHX8+1]; 
        if(!(Vsta.VmeModes&4)){ 
            if(Vsta.BidirMask&UNIDIR_NO_MIX){
                if(Dist[0][FHX8]+Dist[0][FHX8+1]<=Dist[1][FHX8]+Dist[1][FHX8+1]) { m = 0x3F11; }
                else{ 
                    if (benablecopy){
                        useDistF16x8_0=Dist[0][FHX8]=Dist[1][FHX8]; 
                        useDistF16x8_1=Dist[0][FHX8+1]=Dist[1][FHX8+1]; 
                    }
                    else {
                        useDistF16x8_0=Dist[1][FHX8]; 
                        useDistF16x8_1=Dist[1][FHX8+1]; 
                    }
                    m = 0x3F22; 
                }
            }
            else{
                if(Dist[0][FHX8  ]<=Dist[1][FHX8  ]) { m = 0x3F01; }
                else{ 
                    if (benablecopy){
                        useDistF16x8_0=Dist[0][FHX8]=Dist[1][FHX8]; 
                    }
                    else {
                        useDistF16x8_0=Dist[1][FHX8]; 
                    }
                    m = 0x3F02; 
                }
                if(Dist[0][FHX8+1]<=Dist[1][FHX8+1]) { m |= 0x3F10; }
                else{ 
                    if (benablecopy){
                        useDistF16x8_1=Dist[0][FHX8+1]=Dist[1][FHX8+1]; 
                    }
                    else {
                        useDistF16x8_1=Dist[1][FHX8+1]; 
                    }
                    
                    m |= 0x3F20; 
                }
            }
        }
        else{
            k = (!(*mode&0x0001)); 
            if (benablecopy){
                useDistF16x8_0=Dist[0][FHX8]=Dist[k][FHX8];
            }
            else {
                useDistF16x8_0=Dist[k][FHX8]; 
            }            
            k = (!(*mode&0x0010)); 
            if (benablecopy){
                useDistF16x8_1=Dist[0][FHX8+1]=Dist[k][FHX8+1];
            }
            else {
                useDistF16x8_1=Dist[k][FHX8+1]; 
            }    
        }
        d = LutMode[LUTMODE_INTER_16x8_FIELD]+useDistF16x8_0+useDistF16x8_1; 
        if(d<=DistInter){*dist = d; DistInter = *dist; *mode = m;}
    }
    else if(*mode<0x4000){ // 8x8 Field Modes
        useDistF8x8_0=Dist[0][F8X8]; 
        useDistF8x8_1=Dist[0][F8X8+1]; 
        useDistF8x8_2=Dist[0][F8X8+2]; 
        useDistF8x8_3=Dist[0][F8X8+3];
        if(!(Vsta.VmeModes&4)){             
            if(Vsta.BidirMask&UNIDIR_NO_MIX){
                if(Dist[0][F8X8]+Dist[0][F8X8+1]+Dist[0][F8X8+2]+Dist[0][F8X8+3]<=
                    Dist[1][F8X8]+Dist[1][F8X8+1]+Dist[1][F8X8+2]+Dist[1][F8X8+3]) { m = 0x3F55; }
                else{ 
                    if (benablecopy){
                        useDistF8x8_0=Dist[0][F8X8]=Dist[1][F8X8]; 
                        useDistF8x8_1=Dist[0][F8X8+1]=Dist[1][F8X8+1]; 
                        useDistF8x8_2=Dist[0][F8X8+2]=Dist[1][F8X8+2]; 
                        useDistF8x8_3=Dist[0][F8X8+3]=Dist[1][F8X8+3]; 
                    }
                    else{
                        useDistF8x8_0=Dist[1][F8X8]; 
                        useDistF8x8_1=Dist[1][F8X8+1]; 
                        useDistF8x8_2=Dist[1][F8X8+2]; 
                        useDistF8x8_3=Dist[1][F8X8+3]; 
                    }
                    m = 0x3FAA; 
                }
            }
            else{
                if(Dist[0][F8X8  ]<=Dist[1][F8X8  ]) { m = 0x3F01; }
                else{ 
                    if (benablecopy){
                        useDistF8x8_0=Dist[0][F8X8]=Dist[1][F8X8];
                    }
                    else {
                        useDistF8x8_0=Dist[1][F8X8];
                    }                             
                    m = 0x3F02; 
                }
                if(Dist[0][F8X8+1]<=Dist[1][F8X8+1]) { m |= 0x3F04; }
                else{ 
                    if (benablecopy){
                        useDistF8x8_1=Dist[0][F8X8+1]=Dist[1][F8X8+1];
                    }
                    else {
                        useDistF8x8_1=Dist[1][F8X8+1];
                    }                            
                    m |= 0x3F08; 
                }
                if(Dist[0][F8X8+2]<=Dist[1][F8X8+2]) { m |= 0x3F10; }
                else{ 
                    if (benablecopy){
                        useDistF8x8_2=Dist[0][F8X8+2]=Dist[1][F8X8+2];
                    }
                    else {
                        useDistF8x8_2=Dist[1][F8X8+2];
                    }         
                    m |= 0x3F20; 
                }
                if(Dist[0][F8X8+3]<=Dist[1][F8X8+3]) { m |= 0x3F40; }
                else{
                    if (benablecopy){
                        useDistF8x8_3=Dist[0][F8X8+3]=Dist[1][F8X8+3];
                    }
                    else {
                        useDistF8x8_3=Dist[1][F8X8+3];
                    }    
                    m |= 0x3F80; 
                }
            }
        }
        else{
            k = (!(*mode&0x0001)); 
            if (benablecopy){
                useDistF8x8_0=Dist[0][F8X8]=Dist[k][F8X8];
            }
            else {
                useDistF8x8_0=Dist[k][F8X8];
            }    
            k = (!(*mode&0x0004)); 
            if (benablecopy){
                useDistF8x8_1=Dist[0][F8X8+1]=Dist[k][F8X8+1];
            }
            else {
                useDistF8x8_1=Dist[k][F8X8+1];
            }
            k = (!(*mode&0x0010)); 
            if (benablecopy){
                useDistF8x8_2=Dist[0][F8X8+2]=Dist[k][F8X8+2];
            }
            else {
                useDistF8x8_2=Dist[k][F8X8+2];
            }
            k = (!(*mode&0x0040)); 
            if (benablecopy){
                useDistF8x8_3=Dist[0][F8X8+3]=Dist[k][F8X8+3];
            }
            else {
                useDistF8x8_3=Dist[k][F8X8+3];
            }
        }
        d = LutMode[LUTMODE_INTER_8x8_FIELD]+useDistF8x8_0+useDistF8x8_1+useDistF8x8_2+useDistF8x8_3;
        if(d<=DistInter){*dist = d; DistInter = *dist; *mode = m;}
    }
    else 
    {
        bidirValid = false;//CheckBidirValid(m, checkPruning, true);
        // Minors with no change of directions
        m = (*mode)&0xFF; 
        s = (*mode)>>8; 
        d = 0;
        i = 0; //variable initialization
        int imode = 0;
        df = 0;
        I32     BlkMaxDist = (Vsta.SadType & INTER_CHROMA_MODE)? (MBMAXDIST>>1):(MBMAXDIST>>2); //0x3fff for luma, 0x7fff for chroma
        for(k=0;k<4;k++){
            i = ((s-1)&1);
            switch(m&3){
                case 0: // 8x8                    
                    j = B8X8+k;
                    useDistM8x8 = Dist[0][j];
                    if(bidirValid){
                        if(!(Vsta.BidirMask&UNIDIR_NO_MIX))
                            i = ((Dist[0][j]+L0_BWD_Penalty*LutMode[LUTMODE_INTER_BWD])>Dist[1][j]+(!L0_BWD_Penalty)*LutMode[LUTMODE_INTER_BWD]);
                        else df += Dist[1][j]-Dist[0][j];
                    }
                    if (benablecopy){
                        useDistM8x8=Dist[0][j] = Dist[i][j];
                    }
                    else {
                        useDistM8x8=Dist[i][j];;
                    }
                    
                    if((i == 1&&(!L0_BWD_Penalty)) || ((i != 1)&&L0_BWD_Penalty))
                        additionalCost = LutMode[LUTMODE_INTER_BWD];
                    else
                        additionalCost = 0;
                    tempd = (LutMode[LUTMODE_INTER_8x8q]+useDistM8x8+additionalCost);
                    tempd = (tempd > Blk8x8MaxDist) ? Blk8x8MaxDist : tempd;
                    d += tempd;
                    imode |= i ? 0x20000 : 0x10000;
                    break;
                case 1: // 8x4                    
                    //only best directions are checked for minor shapes
                    j = B8X4 + (k<<1);
                    useDistM8x4_0 = Dist[i][j]; 
                    useDistM8x4_1 = Dist[i][j+1];
                    if (benablecopy){
                        useDistM8x4_0=Dist[0][j] = Dist[i][j]; 
                        useDistM8x4_1=Dist[0][j+1] = Dist[i][j+1];
                    }
                    else {
                        useDistM8x4_0 = Dist[i][j]; 
                        useDistM8x4_1 = Dist[i][j+1];
                    }                    
                    if((i == 1&&(!L0_BWD_Penalty)) || ((i != 1)&&L0_BWD_Penalty))
                        additionalCost = LutMode[LUTMODE_INTER_BWD];
                    else
                        additionalCost = 0;
                    tempd = LutMode[LUTMODE_INTER_8x4q]+useDistM8x4_0+useDistM8x4_1+additionalCost;
                    tempd = (tempd > Blk8x8MaxDist) ? Blk8x8MaxDist : tempd;
                    d += tempd;
                    imode |= i ? 0x20100 : 0x10100;
                    break;
                case 2: // 4x8
                    //only best directions are checked for minor shapes
                    j = B4X8 + (k<<1);
                    useDistM4x8_0 = Dist[i][j]; 
                    useDistM4x8_1 = Dist[i][j+1];
                    if (benablecopy){
                        useDistM4x8_0=Dist[0][j] = Dist[i][j]; 
                        useDistM4x8_1=Dist[0][j+1] = Dist[i][j+1];
                    }
                    else {
                        useDistM4x8_0 = Dist[i][j]; 
                        useDistM4x8_1 = Dist[i][j+1];
                    }                    
                    if((i == 1&&(!L0_BWD_Penalty)) || ((i != 1)&&L0_BWD_Penalty))
                        additionalCost = LutMode[LUTMODE_INTER_BWD];
                    else
                        additionalCost = 0;
                    tempd = LutMode[LUTMODE_INTER_4x8q]+useDistM4x8_0+useDistM4x8_1+additionalCost;
                    tempd = (tempd > Blk8x8MaxDist) ? Blk8x8MaxDist : tempd;
                    d += tempd;
                    imode |= i ? 0x20200 : 0x10200;
                    break;
                case 3: // 4x4
                    //only best directions are checked for minor shapes
                    j = B4X4 + (k<<2);
                    if (!InterChromaEn){
                    useDistM4x4_0 = Dist[i][j  ]; 
                    useDistM4x4_1 = Dist[i][j+1];
                    useDistM4x4_2 = Dist[i][j+2]; 
                    useDistM4x4_3 = Dist[i][j+3];
                    }
                    else {
                        imode = 0;
                        i = (s-1)&1;        useDistM4x4_0 = Dist[i][j  ]; imode |= i ? 0x20300 : 0x10300; imode>>=2;
                        i = ((s>>2)-1)&1;    useDistM4x4_1 = Dist[i][j+1]; imode |= i ? 0x20300 : 0x10300; imode>>=2;
                        i = ((s>>4)-1)&1;    useDistM4x4_2 = Dist[i][j+2]; imode |= i ? 0x20300 : 0x10300; imode>>=2;
                        i = ((s>>6)-1)&1;    useDistM4x4_3 = Dist[i][j+3]; imode |= i ? 0x20300 : 0x10300; imode>>=2;
                    }
                    if (benablecopy){
                        Dist[0][j  ] = useDistM4x4_0; 
                        Dist[0][j+1] = useDistM4x4_1;
                        Dist[0][j+2] = useDistM4x4_2; 
                        Dist[0][j+3] = useDistM4x4_3;
                    }                                    
                    if((i == 1&&(!L0_BWD_Penalty)) || ((i != 1)&&L0_BWD_Penalty))
                        additionalCost = LutMode[LUTMODE_INTER_BWD];
                    else
                        additionalCost = 0;
                    tempd = LutMode[LUTMODE_INTER_4x4q]+useDistM4x4_0+useDistM4x4_1+useDistM4x4_2+useDistM4x4_3+additionalCost;
                    tempd = (tempd > Blk8x8MaxDist) ? Blk8x8MaxDist : tempd;
                    d += tempd;
                    if(!InterChromaEn)    imode |= i ? 0x20300 : 0x10300;
                    break;
            }
            
            if(!(InterChromaEn && (m&3) == 3)) {
            s>>=2;
            imode>>=2;
            }
            m>>=2;
        }
        if(Vsta.BidirMask&UNIDIR_NO_MIX){

            //if(df>=0 && i){ d -= df; //imode = 0x5500|(*mode&0xFF); 
            //}
            //else if(df<0 && !i){ d += df; //imode = 0xAA00|(*mode&0xFF); 
            //}
        }
        ScaleDist(d);
        //if(d<=DistInter){*dist = d; DistInter = *dist; *mode = imode;}
        if(d<=*dist){*dist = d; *mode = imode;}
    }
}

/*****************************************************************************************************/
void MEforGen75::GetControlledDynamicSetting(int id)
/*****************************************************************************************************/
{
#if 0 // Original Dynamic Code - Replaced by else below

    for(int k=B8X8;k<B8X8+4;k++){
        int         x = (Best[id][k].x>>4);
        int         y = (Best[id][k].y>>4);
        if((Best[id][k].x&12)==0){ // left
            if(x>0){
                if(!(Record[id][y]&(1<<(x-1)))){ NextSU.x = x-1; NextSU.y = y; NextRef = id; return; }
                if(y>0 && (Best[id][k].y&12)==0){ // top
                    if(!(Record[id][y-1]&(1<<(x-1)))){ NextSU.x = x-1; NextSU.y = y-1; NextRef = id; return; }
                }
                if((Best[id][k].y&12)==12){ // bottom
                    if(!(Record[id][y+1]&(1<<(x-1)))){ NextSU.x = x-1; NextSU.y = y+1; NextRef = id; return; }
                }
            }
        }
        if((Best[id][k].x&12)==12){ // right
            if(!(Record[id][y]&(1<<(x+1)))){ NextSU.x = x+1; NextSU.y = y; NextRef = id; return; }
            if(y>0 && (Best[id][k].y&12)==0){ // top
                if(!(Record[id][y-1]&(1<<(x+1)))){ NextSU.x = x+1; NextSU.y = y-1; NextRef = id; return; }
            }
            if((Best[id][k].y&12)==12){ // bottom
                if(!(Record[id][y+1]&(1<<(x+1)))){ NextSU.x = x+1; NextSU.y = y+1; NextRef = id; return; }
            }
        }
        if(y>0 && (Best[id][k].y&12)==0){ // top
            if(!(Record[id][y-1]&(1<<x))){ NextSU.x = x; NextSU.y = y-1; NextRef = id; return; }
        }
        if((Best[id][k].y&12)==12){ // bottom
            if(!(Record[id][y+1]&(1<<x))){ NextSU.x = x; NextSU.y = y+1; NextRef = id; return; }
        }
    }

#else // Replaced with following by JMH to match RTL algorithm


    U8 numValidDynMVs = 0;
    I16PAIR ValidMV[2][4];
    U8 refId = 0;
    U8 numRef = id+1;
    //I16 SuCount[2] = {CntSU[0], CntSU[1]};
    if(!CheckDynamicEnable(FixedCntSu[0] + AdaptiveCntSu[0], FixedCntSu[1] + AdaptiveCntSu[1]))
    {
        Dynamic[0] = false;
        Dynamic[1] = false;
        NextRef = 2;
        return;
    }

    for(refId = 0; refId < numRef; refId++)
    {
        switch(Vsta.SrcType&3)
        {
        case 0:
            numValidDynMVs = 4;
            for(int i = 0; i < numValidDynMVs; i++)
            {
                ValidMV[refId][i].x = Best[refId][B8X8+i].x - (Vsta.Ref[refId].x<<2);
                ValidMV[refId][i].y = Best[refId][B8X8+i].y - (Vsta.Ref[refId].y<<2);
            }
            break;
        case 1:
            numValidDynMVs = 2;
            for(int i = 0; i < numValidDynMVs; i++)
            {
                ValidMV[refId][i].x = BestForReducedMB[refId][B8X8+i].x - (Vsta.Ref[refId].x<<2);
                ValidMV[refId][i].y = BestForReducedMB[refId][B8X8+i].y - (Vsta.Ref[refId].y<<2);
            }
            break;
        case 3:
            numValidDynMVs = 1;
            for(int i = 0; i < numValidDynMVs; i++)
            {
                ValidMV[refId][i].x = BestForReducedMB[refId][B8X8+i].x - (Vsta.Ref[refId].x<<2);
                ValidMV[refId][i].y = BestForReducedMB[refId][B8X8+i].y - (Vsta.Ref[refId].y<<2);
            }
            break;
        default:
            break;
        }
    }

    int max_x = (Vsta.RefW>>2) - 5; // Horizontal SU maximum boundary
    int max_y = (Vsta.RefH>>2) - 5; // Vertical SU maximum boundary
    
    U8 dvh[2][4]; // b2: Diagonal, b1: Vertical, b0: Horizontal
    memset(&dvh[0][0],0,sizeof(dvh[0][0])*8);

    for(refId = 0; refId < numRef; refId++)
    {
        for(int k=0; k<numValidDynMVs; k++){
            if((ValidMV[refId][k].x&12)==0 || (ValidMV[refId][k].x&12)==12){dvh[refId][k] |= 1;}
            if((ValidMV[refId][k].y&12)==0 || (ValidMV[refId][k].y&12)==12){dvh[refId][k] |= 2;}
            if(dvh[refId][k] == 3)                                                 {dvh[refId][k] |= 4;}
        }
    }

    for(refId = 0; refId < numRef; refId++)
    {
        if(!Dynamic[refId])
        {
            continue;
        }
        for(int k=0;k<numValidDynMVs && id>=0;k++){ // Look at all 4 blocks for any horizontal winners
            int x = ValidMV[refId][k].x>>4;
            int y = ValidMV[refId][k].y>>4;

            if(dvh[refId][k]&1){
                if(     (ValidMV[refId][k].x&12)==0  && !(Record[refId][y]&(1<<(x-1))) && x>0)
                    { NextSU.x = x-1; NextSU.y = y; NextRef = refId; return; } // Go left
                else if((ValidMV[refId][k].x&12)==12 && !(Record[refId][y]&(1<<(x+1))) && x<max_x)                
                    { NextSU.x = x+1; NextSU.y = y; NextRef = refId; return; } // Go right
            }
        }
    }

    for(refId = 0; refId < numRef; refId++)
    {
        if(!Dynamic[refId])
        {
            continue;
        }
        for(int k=0;k<numValidDynMVs && id>=0;k++){ // Look at all 4 blocks for any vertical winners
            int x = ValidMV[refId][k].x>>4;
            int y = ValidMV[refId][k].y>>4;

            if(dvh[refId][k]&2){
                if(     (ValidMV[refId][k].y&12)==0  && !(Record[refId][y-1]&(1<<x)) && y>0)
                    { NextSU.x = x; NextSU.y = y-1; NextRef = refId; return; } // Go top
                else if((ValidMV[refId][k].y&12)==12 && !(Record[refId][y+1]&(1<<x)) && y<max_y)
                    { NextSU.x = x; NextSU.y = y+1; NextRef = refId; return; } // Go bot
            }
        }
    }

    for(refId = 0; refId < numRef; refId++)
    {
        if(!Dynamic[refId])
        {
            continue;
        }
        for(int k=0;k<numValidDynMVs && id>=0;k++){ // Look at all 4 blocks for any diagonal winners
            int x = ValidMV[refId][k].x>>4;
            int y = ValidMV[refId][k].y>>4;

            if(dvh[refId][k]&4){
                if(     (ValidMV[refId][k].x&12)==0  && (ValidMV[refId][k].y&12)==0  && !(Record[refId][y-1]&(1<<(x-1))) && x>0 && y>0)
                    { NextSU.x = x-1; NextSU.y = y-1; NextRef = refId; return; } // Go topleft
                else if((ValidMV[refId][k].x&12)==12 && (ValidMV[refId][k].y&12)==0  && !(Record[refId][y-1]&(1<<(x+1))) && x<max_x && y>0)
                    { NextSU.x = x+1; NextSU.y = y-1; NextRef = refId; return; } // Go topright
                else if((ValidMV[refId][k].x&12)==0  && (ValidMV[refId][k].y&12)==12 && !(Record[refId][y+1]&(1<<(x-1))) && x>0 && y<max_y)
                    { NextSU.x = x-1; NextSU.y = y+1; NextRef = refId; return; } // Go botleft
                else if((ValidMV[refId][k].x&12)==12 && (ValidMV[refId][k].y&12)==12 && !(Record[refId][y+1]&(1<<(x+1))) && x<max_x && y<max_y)
                    { NextSU.x = x+1; NextSU.y = y+1; NextRef = refId; return; } // Go botright
            }
        }
        Dynamic[refId] = 0;
    }

#endif

    NextRef = 2; bdualAdaptive = false;
}

/*****************************************************************************************************/
int MEforGen75::SetupLutCosts( )
/*****************************************************************************************************/
{
    int        i; 
    short    x0;
    L0_BWD_Penalty = false;
    I16 BWD_PENALTY_MAX = 0x7ff;
    
    for(i=0;i<12;i++){ //JMHREFID should this go to 13 for REFID cost?
        if(i == LUTMODE_INTER_BWD)
        {
            if(Vsta.ModeCost[i] & (1<<7))
            {
                L0_BWD_Penalty = true;
            }
            else
            {
                L0_BWD_Penalty = false;
            }
            if((LutMode[i] = (Vsta.ModeCost[i]&15)<<((Vsta.ModeCost[i]>>4)&0x7))>BWD_PENALTY_MAX)
            {
                //printf("\n warning: LutMoce Limit exceeded: LutMode[%d] = %d",i,LutMode[i]);
                LutMode[i] = LutMode[i]&BWD_PENALTY_MAX;
            }
        }
        else
        {
            if(i == LUTMODE_INTRA_NONPRED || i == LUTMODE_INTER_8x8q || i == LUTMODE_INTER_8x4q || i == LUTMODE_INTER_4x4q){ // JMHREFID does this need to include REFID cost?
                if((LutMode[i] = (Vsta.ModeCost[i]&0xF)<<(Vsta.ModeCost[i]>>4))>MAXLUTMODE0)
                {
                    //printf("\n warning: LutMoce Limit exceeded: LutMode[%d] = %d",i,LutMode[i]);
                    LutMode[i] = LutMode[i]&MAXLUTMODE0;
                }
            }
            else
            {
                if((LutMode[i] = (Vsta.ModeCost[i]&0xF)<<(Vsta.ModeCost[i]>>4))>MAXLUTMODE1) 
                {
                    //printf("\n warning: LutMoce Limit exceeded: LutMode[%d] = %d",i,LutMode[i]);
                    LutMode[i] = LutMode[i]&MAXLUTMODE1;
                }
            }
        }
    }

    if((Vsta.SrcType&3)==1)//src type 16x8
    {//distortion is divided by 2 in scale dist.
        LutMode[LUTMODE_INTER_16x8] = 2*LutMode[LUTMODE_INTER_16x8];
    }

    if((LutXY[ 0] = (Vsta.MvCost[0]&15)<<(Vsta.MvCost[0]>>4))>MAXLUTXY) LutXY[ 0] = MAXLUTXY&LutXY[ 0]; // Henry:Drop MSB if exceed 3FF to match RTL/Fulsim 
    if((LutXY[ 1] = (Vsta.MvCost[1]&15)<<(Vsta.MvCost[1]>>4))>MAXLUTXY) LutXY[ 1] = MAXLUTXY&LutXY[ 1];
    if((LutXY[ 2] = (Vsta.MvCost[2]&15)<<(Vsta.MvCost[2]>>4))>MAXLUTXY) LutXY[ 2] = MAXLUTXY&LutXY[ 2];
    if((LutXY[ 4] = (Vsta.MvCost[3]&15)<<(Vsta.MvCost[3]>>4))>MAXLUTXY) LutXY[ 4] = MAXLUTXY&LutXY[ 4];
    if((LutXY[ 8] = (Vsta.MvCost[4]&15)<<(Vsta.MvCost[4]>>4))>MAXLUTXY) LutXY[ 8] = MAXLUTXY&LutXY[ 8];
    if((LutXY[16] = (Vsta.MvCost[5]&15)<<(Vsta.MvCost[5]>>4))>MAXLUTXY) LutXY[16] = MAXLUTXY&LutXY[16];
    if((LutXY[32] = (Vsta.MvCost[6]&15)<<(Vsta.MvCost[6]>>4))>MAXLUTXY) LutXY[32] = MAXLUTXY&LutXY[32];
    if((LutXY[64] = (Vsta.MvCost[7]&15)<<(Vsta.MvCost[7]>>4))>MAXLUTXY) LutXY[64] = MAXLUTXY&LutXY[64];

    LutXY[3] = ((LutXY[4]+LutXY[2])>>1);
    x0 = LutXY[ 8] - LutXY[4];
    for(i=1;i< 4;i++) LutXY[ 4+i] = LutXY[ 4]+((x0*i)>>2);
    x0 = LutXY[16] - LutXY[8];
    for(i=1;i< 8;i++) LutXY[ 8+i] = LutXY[ 8]+((x0*i)>>3);
    x0 = LutXY[32] - LutXY[16];
    for(i=1;i<16;i++) LutXY[16+i] = LutXY[16]+((x0*i)>>4);
    x0 = LutXY[64] - LutXY[32];
    for(i=1;i<32;i++) LutXY[32+i] = LutXY[32]+((x0*i)>>5);

    return 0;
}

/*****************************************************************************************************/
int MEforGen75::SetupSearchPath( )
/*****************************************************************************************************/
{
    U8        *pp = &SearchPath[0];

    U8 chromamode = Vsta.SadType&1;
    short    rw = (((Vsta.RefW<<chromamode)-16)>>2);
    short    rh = ((Vsta.RefH-(16>>chromamode))>>2);
    int        i; 
    short    x0,y0,x1,y1;
    int NumStartCentersPerReference;
    
    x0 = ActiveSP[0][0].x = (Vsta.SPCenter[0]&0xf);
    y0 = ActiveSP[0][0].y = ((Vsta.SPCenter[0]>>4)&0xf);
    x1 = ActiveSP[1][0].x = (Vsta.SPCenter[1]&0xf);
    y1 = ActiveSP[1][0].y = ((Vsta.SPCenter[1]>>4)&0xf);

    LenSP[0] = LenSP[1] = 1;
    
    if(Vsta.VmeModes&4) //if dual reference, ref width/height needs to be less than or equal to 32
    {
        if((Vsta.RefW > 32) ||(Vsta.RefH > 32))
        {
            //printf("MEforGen75::SetupSearchPath >> Test Error: Reference width/height needs to be less than or equal to 32 in case of dual reference.\n");
            return (-1);
        }
    }
    if((x0>=rw || y0>=rh))// && (Vsta.DoIntraInter&DO_INTER)) 
    {
        LenSP[0] = 0;
        //printf("MEforGen75::SetupSearchPath >> Test Error: Search Path Start Center not valid\n");    
        return (-1);
    }

    if(((Vsta.VmeModes&7)>0)&&(x1>=rw || y1>=rh))// && (Vsta.DoIntraInter&DO_INTER))
    {
        LenSP[1] = 0;
        //printf("MEforGen75::SetupSearchPath >> Test Error: Search Path Start Center not valid\n");    
        return (-1);
    }

    if((Vsta.VmeModes&7)==1){ //simple dual center
        x0 = ActiveSP[0][1].x = (U8) x1;
        y0 = ActiveSP[0][1].y = (U8) y1;
        LenSP[0] += LenSP[1];
    }

    MFX_INTERNAL_CPY(pp,        VIMEsta.IMESearchPath0to31,    32);
    MFX_INTERNAL_CPY(pp+32,    VIMEsta.IMESearchPath32to55,24);
    
    int k = (!(Vsta.VmeModes&2)) ? 1 : ((Vsta.VmeModes&VM_2PATH_ENABLE) ? 7 : 3);  // dual or single records

    short MaxLenSP = Vsta.MaxLenSP;
    if ((Vsta.VmeModes&7)==1)
    {
        // single reference, single record, dual start
        NumStartCentersPerReference = 2;
    }
    else
    {
        NumStartCentersPerReference = 1;
    }
    if (MaxLenSP > SEARCHPATHSIZE+1)
    {
        // Make sure (MaxLenSP-1) do not exceed 56, which is maximum possible SUs
        MaxLenSP = SEARCHPATHSIZE+NumStartCentersPerReference;
    } 

    if(MaxLenSP <= 1)
    {
        if((Vsta.VmeModes&7)==3) // single ref, dual record,dual start
        {
            Vsta.MaxLenSP = 2;
            if (Vsta.MaxNumSU < 2)
            {
            Vsta.MaxNumSU = 2;
            }
            MaxLenSP = 2; 
            y1 = y0; x1 = x0; // startcenter1 changed to startcenter0
            ActiveSP[1][0].x = ActiveSP[0][0].x;
            ActiveSP[1][0].y = ActiveSP[0][0].y;
        }
        else
        {
            Vsta.MaxLenSP = 1;
            MaxLenSP = 1; 
            if (Vsta.MaxNumSU < 1)
            {
            Vsta.MaxNumSU = 1;
            }
            if ((Vsta.VmeModes&7)==1)
                Vsta.VmeModes &= 0xfe; //change search control from 1 to 0
        }
    }

    AltMoreSU = 0;

    int numEntry = MaxLenSP-NumStartCentersPerReference;
    if ((Vsta.VmeModes&VM_2PATH_ENABLE) && (Vsta.VmeModes&7)==3)
    {
        // single reference, dual start, dual record, dual search path enabled, fix to match RTL
        numEntry = SEARCHPATHSIZE;
    }
    numEntry = ((Vsta.VmeModes&4) && (Vsta.VmeModes&VM_2PATH_ENABLE)) ? 2*numEntry : numEntry;
    for(i=0;i<numEntry;i++){
        if (i >= 56) break;
        if(k&1){
            if(!pp[0]) k &= 6;  // done with first record
            else{
                x0 = ((x0+pp[0])&15);
                y0 = ((y0+(pp[0]>>4))&15);
                if((Vsta.VmeModes&7)==3)
                {
                    ActiveSP[0][LenSP[0]].x = (U8) x0;
                    ActiveSP[0][LenSP[0]++].y = (U8) y0;
                }
                else
                {
                    if(x0<rw && y0<rh){
                        ActiveSP[0][LenSP[0]].x = (U8) x0;
                        ActiveSP[0][LenSP[0]++].y = (U8) y0;
                    }
                }
            }
        }
        if(k&4){ i++; pp++; }
        if(k&2){
            if(!pp[0]) k &= 5;
            else{
                x1 = ((x1+pp[0])&15);
                y1 = ((y1+(pp[0]>>4))&15);
                if((Vsta.VmeModes&7)==3)
                {
                    ActiveSP[1][LenSP[1]].x = (U8) x1;
                    ActiveSP[1][LenSP[1]++].y = (U8) y1;
                    AltMoreSU++;
                }
                else
                {
                    if(x1<rw && y1<rh){
                        ActiveSP[1][LenSP[1]].x = (U8) x1;
                        ActiveSP[1][LenSP[1]++].y = (U8) y1;
                        AltMoreSU++;
                    }
                }
            }
        }
        pp++;
    }
    
    if(LenSP[0]>Vsta.MaxLenSP) LenSP[0] = Vsta.MaxLenSP;
    if(LenSP[1]>Vsta.MaxLenSP) LenSP[1] = Vsta.MaxLenSP;

    if(Vsta.VmeModes&4){ // dual reference windows
        RefPix[1] = RefPix[0] + REFWINDOWWIDTH*REFWINDOWHEIGHT;
        RefPixCb[1] = RefPixCb[0] + REFWINDOWWIDTH*REFWINDOWHEIGHT;
        RefPixCr[1] = RefPixCr[0] + REFWINDOWWIDTH*REFWINDOWHEIGHT;
        Record[1] = (Record[0] = Records) + 16; 
    }
    else {
        Vsta.RefLuma[1][0] = Vsta.RefLuma[0][0];
        RefPix[1] = RefPix[0];
        RefPixCb[1] = RefPixCb[0];
        RefPixCr[1] = RefPixCr[0];
        Record[1] = Record[0] = Records; 
    }
    if(!(Vsta.VmeModes&6)){ // single record
        LenSP[1] = 0;
        Dynamic[0] = !!(Vsta.VmeFlags&VF_ADAPTIVE_ENABLE);
        Dynamic[1] = 0;
        NextRef = 0;
    }
    else{
        Dynamic[0] = Dynamic[1] = !!(Vsta.VmeFlags&VF_ADAPTIVE_ENABLE);
        NextRef = 1;
    }
    
    Vout->NumSUinIME = MoreSU[0] = Vsta.MaxNumSU;

    int altNumSUinIME = Vout->ImeStop_MaxMV_AltNumSU>>2;
    int decisionLog = Vout->ImeStop_MaxMV_AltNumSU&0x3;
    altNumSUinIME = AltMoreSU > 0 ? ++AltMoreSU : 0;
    Vout->ImeStop_MaxMV_AltNumSU = (altNumSUinIME<<2) | decisionLog;

    MoreSU[1] = (Vsta.VmeModes&4) ? Vsta.MaxNumSU : 0;
    
    for(i= 0;i<rh;i++)  Record[0][i] = Record[1][i] = (0xFFFF<<rw);
    for(i=rh;i<16;i++)  Record[0][i] = Record[1][i] = 0xFFFF;

    CntSU[0] = CntSU[1] = 0;
    if(!LenSP[0]){    // total out-boundary loop, bad case, set to window center as default.
        ActiveSP[0][0].x = (rw>>1);
        ActiveSP[0][0].y = (rh>>1);
        LenSP[0] = 1;
    }
    return 0;
}


/*****************************************************************************************************/
int MEforGen75::OneSearchUnit(U8   qx, U8   qy, short isb)
/*****************************************************************************************************/
{
    if (isb >= 2) return ERR_UNKNOWN;

    int        realx,realy,i,j,k,k0=0,k1=0,k2=0,k3=0,e[42] = {},fbits,SUw,r,rb2,rb4;
    SUw = ((Vsta.SadType&INTER_CHROMA_MODE) ? 1 : 2);
    int        x = (qx<<SUw);
    int        y = (qy<<2);
    U8      *ref = RefPix[isb];
    I32        *dist = Dist[isb];
    I32        d[16];
    memset(d,0,sizeof(d[0])*16);
    I16PAIR    *mv = Best[isb];
    fbits = 2;
    r   = GetCostRefID(BestRef[isb][0]);
    rb2 = r>>1;
    rb4 = r>>2;

    if(startedAdaptive) AdaptiveCntSu[isb]++;
    else FixedCntSu[isb]++;
    // reset to initial value for next search
    if(Vsta.SrcType&3){ for(k=0;k<42;k++){ dist[k] = MBMAXDIST; }}

    Record[isb][qy] |= (1<<qx);

    if(!(Vsta.SrcType&0x0C)){  // no field modes

        for(j=0;j<4;j++,y++){
            for(i=0, x = (qx<<SUw);i<(1<<SUw);i++,x++){
                realx = x + Vsta.Ref[isb].x;
                realy = y + Vsta.Ref[isb].y;

                memset(d, 0, 16*sizeof(I32));
                if(!(Vsta.SadType&INTER_CHROMA_MODE)){
                    GetDistortionSU(SrcMB, ref+x+y*REFWINDOWWIDTH,d);
                }
                else{
                    GetDistortionSUChroma(SrcMBU, SrcMBV, RefPixCb[isb]+x+y*REFWINDOWWIDTH,RefPixCr[isb]+x+y*REFWINDOWWIDTH,d);
                }
                switch(Vsta.SrcType&3){
                    case 0: k3 = k2 = k1 = k0 = GetCostXY((realx<<fbits),    (realy<<fbits),isb); break;
                    case 1: k3 = k2 =            GetCostXY((realx<<fbits),    (realy<<fbits)+16,isb);
                            k1 = k0 =            GetCostXY((realx<<fbits),    (realy<<fbits),isb); break;
                    case 2: k3 = k1 =            GetCostXY((realx<<fbits)+16,(realy<<fbits),isb);
                            k2 = k0 =            GetCostXY((realx<<fbits),    (realy<<fbits),isb); break;
                    case 3: k3 =                GetCostXY((realx<<fbits)+16,(realy<<fbits)+16,isb);
                            k2 =                GetCostXY((realx<<fbits),    (realy<<fbits)+16,isb);
                            k1 =                GetCostXY((realx<<fbits)+16,(realy<<fbits),isb);
                            k0 =                GetCostXY((realx<<fbits),    (realy<<fbits),isb); break;
                }

#if (__HAAR_SAD_OPT == 1)
                int e_pre[42];
                
                e_pre[B8X4+0] = k0 + rb2;
                //e[B8X4+1] = k0 + rb2;
                e_pre[B8X4+2] = k1 + rb2;    
                //e[B8X4+3] = k1 + rb2;
                e_pre[B8X4+4] = k2 + rb2;    
                //e[B8X4+5] = k2 + rb2;
                e_pre[B8X4+6] = k3 + rb2;
                //e[B8X4+7] = k3 + rb2;

                //e[B4X8+0] = k0 + rb2;
                //e[B4X8+1] = k0 + rb2;
                //e[B4X8+2] = k1 + rb2;
                //e[B4X8+3] = k1 + rb2;
                //e[B4X8+4] = k2 + rb2;
                //e[B4X8+5] = k2 + rb2;
                //e[B4X8+6] = k3 + rb2;    
                //e[B4X8+7] = k3 + rb2;

                e_pre[B4X4+ 0] = k0 + rb4;        //e[B4X4+ 1] = k0 + rb4;
                //e[B4X4+ 2] = k0 + rb4;        e[B4X4+ 3] = k0 + rb4;
                e_pre[B4X4+ 4] = k1 + rb4;        //e[B4X4+ 5] = k1 + rb4;
                //e[B4X4+ 6] = k1 + rb4;        e[B4X4+ 7] = k1 + rb4;
                e_pre[B4X4+ 8] = k2 + rb4;        //e[B4X4+ 9] = k2 + rb4;
                //e[B4X4+10] = k2 + rb4;        e[B4X4+11] = k2 + rb4;
                e_pre[B4X4+12] = k3 + rb4;        //e[B4X4+13] = k3 + rb4;
                //e[B4X4+14] = k3 + rb4;        e[B4X4+15] = k3 + rb4;                
                
                e_pre[B8X8+0] = k0+r;
                e_pre[B8X8+1] = k1+r;                
                e_pre[B8X8+2] = k2+r;                
                e_pre[B8X8+3] = k3+r;
                
                //e_pre[BHX8+0] = k1+r;    e_pre[B8XH+0] = k2+r;
                //e_pre[BHX8+1] = k2+r;    e_pre[B8XH+1] = k1+r;
                e_pre[BHXH] = e_pre[B8X8+1]+e_pre[B8X8+2]-k0-r;

                int e1[42];
                e1[B8X4+0] = d[0] + d[1];        e1[B8X4+1] = d[4] + d[5];
                //e[B4X8+0] = k0 + d[0] + d[4] + rb2;        e[B4X8+1] = k0 + d[1] + d[5] + rb2;
                e1[B8X8+0] = e1[B8X4+0]+e1[B8X4+1];
                e1[B8X4+2] = d[2] + d[3];        e1[B8X4+3] = d[6] + d[7];
                //e[B4X8+2] = k1 + d[2] + d[6] + rb2;        e[B4X8+3] = k1 + d[3] + d[7] + rb2;
                e1[B8X8+1] = e1[B8X4+2]+e1[B8X4+3];
                e1[B8X4+4] = d[8] + d[9];        e1[B8X4+5] = d[12] + d[13];
                //e[B4X8+4] = k2 + d[8] + d[12] + rb2;    e[B4X8+5] = k2 + d[ 9] + d[13] + rb2;
                e1[B8X8+2] = e1[B8X4+4]+e1[B8X4+5];
                e1[B8X4+6] = d[10] + d[11];    e1[B8X4+7] = d[14] + d[15];
                //e[B4X8+6] = k3 + d[10] + d[14] + rb2;    e[B4X8+7] = k3 + d[11] + d[15] + rb2;
                e1[B8X8+3] = e1[B8X4+6]+e1[B8X4+7];
                e1[BHX8+0] = e1[B8X8+0]+e1[B8X8+1];    e1[B8XH+0] = e1[B8X8+0]+e1[B8X8+2];
                e1[BHX8+1] = e1[B8X8+2]+e1[B8X8+3];    e1[B8XH+1] = e1[B8X8+1]+e1[B8X8+3];
                e1[BHXH] = e1[BHX8+0]+e1[BHX8+1];                
                
                e[B8X4+0] = e_pre[B8X4+0] + e1[B8X4+0];
                e[B8X4+1] = e_pre[B8X4+0] + e1[B8X4+1];
                e[B8X4+2] = e_pre[B8X4+2] + e1[B8X4+2];    
                e[B8X4+3] = e_pre[B8X4+2] + e1[B8X4+3];
                e[B8X4+4] = e_pre[B8X4+4] + e1[B8X4+4];    
                e[B8X4+5] = e_pre[B8X4+4] + e1[B8X4+5];
                e[B8X4+6] = e_pre[B8X4+6] + e1[B8X4+6];
                e[B8X4+7] = e_pre[B8X4+6] + e1[B8X4+7];

                e[B4X8+0] = e_pre[B8X4+0] + d[0] + d[4];
                e[B4X8+1] = e_pre[B8X4+0] + d[1] + d[5];
                e[B4X8+2] = e_pre[B8X4+2] + d[2] + d[6];
                e[B4X8+3] = e_pre[B8X4+2] + d[3] + d[7];
                e[B4X8+4] = e_pre[B8X4+4] + d[8] + d[12];
                e[B4X8+5] = e_pre[B8X4+4] + d[9] + d[13];
                e[B4X8+6] = e_pre[B8X4+6] + d[10] + d[14];    
                e[B4X8+7] = e_pre[B8X4+6] + d[11] + d[15];

                e[B4X4+ 0] = e_pre[B4X4+ 0] + d[0];        e[B4X4+ 1] = e_pre[B4X4+ 0] + d[1];
                e[B4X4+ 2] = e_pre[B4X4+ 0] + d[4];        e[B4X4+ 3] = e_pre[B4X4+ 0] + d[5];
                e[B4X4+ 4] = e_pre[B4X4+ 4] + d[2];        e[B4X4+ 5] = e_pre[B4X4+ 4] + d[3];
                e[B4X4+ 6] = e_pre[B4X4+ 4] + d[6];        e[B4X4+ 7] = e_pre[B4X4+ 4] + d[7];
                e[B4X4+ 8] = e_pre[B4X4+ 8] + d[8];        e[B4X4+ 9] = e_pre[B4X4+ 8] + d[9];
                e[B4X4+10] = e_pre[B4X4+ 8] + d[12];        e[B4X4+11] = e_pre[B4X4+ 8] + d[13];
                e[B4X4+12] = e_pre[B4X4+12] + d[10];        e[B4X4+13] = e_pre[B4X4+12] + d[11];
                e[B4X4+14] = e_pre[B4X4+12] + d[14];        e[B4X4+15] = e_pre[B4X4+12] + d[15];                                
                
                e[B8X8+0] = e_pre[B8X8+0] + e1[B8X8+0];
                e[B8X8+1] = e_pre[B8X8+1] + e1[B8X8+1];                
                e[B8X8+2] = e_pre[B8X8+2] + e1[B8X8+2];                
                e[B8X8+3] = e_pre[B8X8+3] + e1[B8X8+3];
                
                e[BHX8+0] = e_pre[B8X8+1] + e1[BHX8+0];    e[B8XH+0] = e_pre[B8X8+2] + e1[B8XH+0];
                e[BHX8+1] = e_pre[B8X8+2] + e1[BHX8+1];    e[B8XH+1] = e_pre[B8X8+1] + e1[B8XH+1];
                e[BHXH] = e_pre[BHXH] + e1[BHXH];
#else
                e[B8X4+0] = k0 + d[0] + d[1] + rb2;        e[B8X4+1] = k0 + d[4] + d[5] + rb2;
                e[B4X8+0] = k0 + d[0] + d[4] + rb2;        e[B4X8+1] = k0 + d[1] + d[5] + rb2;
                e[B8X8+0] = e[B8X4+0]+e[B8X4+1]-k0-rb2-rb2+r;
                e[B8X4+2] = k1 + d[2] + d[3] + rb2;        e[B8X4+3] = k1 + d[6] + d[7] + rb2;
                e[B4X8+2] = k1 + d[2] + d[6] + rb2;        e[B4X8+3] = k1 + d[3] + d[7] + rb2;
                e[B8X8+1] = e[B8X4+2]+e[B8X4+3]-k1-rb2-rb2+r;
                e[B8X4+4] = k2 + d[8] + d[9] + rb2;        e[B8X4+5] = k2 + d[12] + d[13] + rb2;
                e[B4X8+4] = k2 + d[8] + d[12] + rb2;    e[B4X8+5] = k2 + d[ 9] + d[13] + rb2;
                e[B8X8+2] = e[B8X4+4]+e[B8X4+5]-k2-rb2-rb2+r;
                e[B8X4+6] = k3 + d[10] + d[11] + rb2;    e[B8X4+7] = k3 + d[14] + d[15] + rb2;
                e[B4X8+6] = k3 + d[10] + d[14] + rb2;    e[B4X8+7] = k3 + d[11] + d[15] + rb2;
                e[B8X8+3] = e[B8X4+6]+e[B8X4+7]-k3-rb2-rb2+r;
                e[BHX8+0] = e[B8X8+0]+e[B8X8+1]-k0-r;    e[B8XH+0] = e[B8X8+0]+e[B8X8+2]-k0-r;
                e[BHX8+1] = e[B8X8+2]+e[B8X8+3]-k3-r;    e[B8XH+1] = e[B8X8+1]+e[B8X8+3]-k3-r;
                e[BHXH] = e[BHX8+0]+e[BHX8+1]-k0-r;
                
                e[B4X4+ 0] = k0 + d[0] + rb4;        e[B4X4+ 1] = k0 + d[1] + rb4;
                e[B4X4+ 2] = k0 + d[4] + rb4;        e[B4X4+ 3] = k0 + d[5] + rb4;
                e[B4X4+ 4] = k1 + d[2] + rb4;        e[B4X4+ 5] = k1 + d[3] + rb4;
                e[B4X4+ 6] = k1 + d[6] + rb4;        e[B4X4+ 7] = k1 + d[7] + rb4;
                e[B4X4+ 8] = k2 + d[8] + rb4;        e[B4X4+ 9] = k2 + d[9] + rb4;
                e[B4X4+10] = k2 + d[12] + rb4;        e[B4X4+11] = k2 + d[13] + rb4;
                e[B4X4+12] = k3 + d[10] + rb4;        e[B4X4+13] = k3 + d[11] + rb4;
                e[B4X4+14] = k3 + d[14] + rb4;        e[B4X4+15] = k3 + d[15] + rb4;
#endif            

                for(k=1;k<42;k++){
                    if(e[k]<dist[k]){ dist[k] = e[k]; mv[k].x = (realx<<fbits); mv[k].y = (realy<<fbits); }
                }
            }
        }
    }
    else{
        for(j=0;j<4;j++,y++){
            for(i=0, x = (qx<<2);i<4;i++,x++){
                GetDistortionSUField(ref+x+(y<<6),d);
                k = GetCostXY((x<<2),(y<<2),isb);
                e[F8X8+0] = k + d[0] + d[1] + d[8] + d[9];
                e[F8X8+1] = k + d[2] + d[3] + d[10] + d[11];
                e[F8X8+2] = k + d[4] + d[5] + d[12] + d[13];
                e[F8X8+3] = k + d[6] + d[7] + d[14] + d[15];
                e[B8X8+0] = k + d[0] + d[1] + d[4] + d[5];
                e[B8X8+1] = k + d[2] + d[3] + d[6] + d[7];
                e[B8X8+2] = k + d[8] + d[9] + d[12]+ d[13];
                e[B8X8+3] = k + d[10]+ d[11]+ d[14]+ d[15];
                e[FHX8+0] = e[F8X8+0]+e[F8X8+1]-k; 
                e[FHX8+1] = e[F8X8+2]+e[F8X8+3]-k; 
                e[BHX8+0] = e[B8X8+0]+e[B8X8+1]-k; e[B8XH+0] = e[B8X8+0]+e[B8X8+2]-k;
                e[BHX8+1] = e[B8X8+2]+e[B8X8+3]-k; e[B8XH+1] = e[B8X8+1]+e[B8X8+3]-k;
                e[BHXH] = e[BHX8+0]+e[BHX8+1]-k;
                for(k=1;k<42;k++){
                    if(e[k]<dist[k]){ dist[k] = e[k]; mv[k].x = (x<<2); mv[k].y = (y<<2); }
                }
            }
        }
    }

    CombineReducuedMBResults(isb);

    if((Vsta.VmeFlags&VF_EARLY_IME_GOOD)== VF_EARLY_IME_GOOD)
    {
        //if dual record is enabled, the first IME stop check is done only after the first SUs
        //of both records have been used.
        if(isb==0)
        {
            if(((Vsta.VmeModes&0x2)!=0x2)||(((Vsta.VmeModes&0x2)==0x2)&&((FixedCntSu[0] + AdaptiveCntSu[0])>1)))
            {
                switch(Vsta.SrcType&3){
                    case 0: if(dist[BHXH]>=EarlyImeStop) return 0;
                        break;
                    case 1: if(dist[BHX8]>=EarlyImeStop && dist[BHX8+1]>=EarlyImeStop) return 0;
                        break;
                    case 2: if(dist[B8XH]>=EarlyImeStop && dist[B8XH+1]>=EarlyImeStop) return 0;
                        break;
                    case 3: if(dist[B8X8]>=EarlyImeStop && dist[B8X8+1]>=EarlyImeStop &&
                             dist[B8X8+2]>=EarlyImeStop && dist[B8X8+3]>=EarlyImeStop) return 0;
                        break;
                }
                // Early success stop
#ifdef VMETESTGEN
        Vout->Test.EarlyExitCriteria = Early_IME_Stop;
#endif
                Vout->ImeStop_MaxMV_AltNumSU |= OCCURRED_IME_STOP;
                //if(bdualAdaptive == true && PipelinedSUFlg==true)
                //{
                //    MoreSU[NextRef]--;
                //    CheckNextReqValid = true; //do not do dummy next req in any case. code review(to do): remove all CheckNextReqValid
                //}
                NextRef = 2; bdualAdaptive = false; return 1;
            }
        }
        else
        {
            if(((Vsta.VmeModes&0x2)==0x2)&&(CntSU[1]==1))
            {
                switch(Vsta.SrcType&3){
                case 0: if(Dist[0][BHXH]>=EarlyImeStop) return 0;
                    break;
                case 1: if(Dist[0][BHX8]>=EarlyImeStop && Dist[0][BHX8+1]>=EarlyImeStop) return 0;
                    break;
                case 2: if(Dist[0][B8XH]>=EarlyImeStop && Dist[0][B8XH+1]>=EarlyImeStop) return 0;
                    break;
                case 3: if(Dist[0][B8X8]>=EarlyImeStop && Dist[0][B8X8+1]>=EarlyImeStop &&
                         Dist[0][B8X8+2]>=EarlyImeStop && Dist[0][B8X8+3]>=EarlyImeStop) return 0;
                    break;
                }
                // Early success stop
#ifdef VMETESTGEN
        Vout->Test.EarlyExitCriteria = Early_IME_Stop;
#endif
                NextRef = 2; bdualAdaptive = false; return 1;
            }
        }
    }

    return 0;
}


/*****************************************************************************************************/
void MEforGen75::SelectBestMinorShape(I32 *MinorDist, U16 *MinorMode, U8 IgnoreMaxMv )
/*****************************************************************************************************/
{
    int        i,j,k,d;
    int        tempModChroma4x4;
    I32        dst1[2][4][4], mod1[2][4][4];    // 0,1,2,4
    I32        dst2[2][2][6], mod2[2][2][6];    // 2,3,4,5,6,8
    I32        dst0[2], mod0[2];                // 4,5,6,7,8,9,10,11,12,13,14,16
    I32        *dist;
    int        minorMode[2][4];                    // set whether in the 2 mv case, 4x8 won or 8x4 won
    I32        tempdist[2];
    I32        FreeMVDistInter = DistInter;

    // Initialize arrays and variables
    for(i = 0; i < 2; i++) {
        dst0[i] = 0;
        mod0[i] = 0;
        tempdist[i] = 0;
        for(j = 1; j < 4; j++) {
            minorMode[i][j] = 0;
            for(k = 1; k < 4; k++) {
                dst1[i][j][k] = 0;
                mod1[i][j][k] = 0;
            }
        }
    }

    // Initialize arrays and variables
    for(i = 1; i < 2; i++) {
        for(j = 1; j < 2; j++) {
            for(k = 1; k < 6; k++) {
                dst2[i][j][k] = 0;
                mod2[i][j][k] = 0;
            }
        }
    }
    for(i = 1; i < 4; i++)
    {
        if(MajorDistFreeMV[i]<FreeMVDistInter)
            FreeMVDistInter = MajorDistFreeMV[i];
    }

    dist = &dst1[0][0][0];
    for(k=0;k<32;k++) dist[k] = MBINVALID_DIST; // MBMAXDIST; // 8x8 - 12 bits
    // [i][k][1/2/3] stores 1MV/2MV/4MV cases
    for(i=0;i<2;i++){
        dist = &Dist[i][0];
        for(k=0;k<4;k++){
            mod1[i][k][2] = (1<<(k<<1));
            if(!(Vsta.ShapeMask&8)){
                dst1[i][k][1] = LutMode[LUTMODE_INTER_8x8q] + dist[B8X8+k];
                dst1[i][k][1] = (dst1[i][k][1] > MBMAXDIST) ? MBMAXDIST    : dst1[i][k][1];
            }
            else // If this shape is disabled 
            {
                // Assign very high distortion to make sure this 
                // shape can never be selected
                dst1[i][k][1] = MBINVALID_DIST;
            }
            if(!(Vsta.ShapeMask&0x10)){
                j = B8X4+(k<<1);
                dst1[i][k][2] = LutMode[LUTMODE_INTER_8x4q]+dist[j]+dist[j+1];
                dst1[i][k][2] = (dst1[i][k][2] > MBMAXDIST) ? MBMAXDIST : dst1[i][k][2];
                minorMode[i][k] = 0;
            }
            else // If this shape is disabled 
            {
                // Assign very high distortion to make sure this 
                // shape can never be selected
                dst1[i][k][2] = MBINVALID_DIST;
            }
            if(!(Vsta.ShapeMask&0x20)){
                j = B4X8+(k<<1);
                d = LutMode[LUTMODE_INTER_4x8q]+dist[j]+dist[j+1];
                d = (d > MBMAXDIST) ? MBMAXDIST : d;
                minorMode[i][k] = 0;
                if(d<dst1[i][k][2]){ 
                    dst1[i][k][2] = d;
                    mod1[i][k][2] <<= 1;
                    minorMode[i][k] = 1;
                }
            }

            if(!(Vsta.ShapeMask&0x40)){
                j = B4X4+(k<<2);
                dst1[i][k][3] = LutMode[LUTMODE_INTER_4x4q]+dist[j]+dist[j+1]+dist[j+2]+dist[j+3];
                dst1[i][k][3] = (dst1[i][k][3] > MBMAXDIST) ? MBMAXDIST : dst1[i][k][3];
            }
            else // If this shape is disabled 
            {
                // Assign very high distortion to make sure this 
                // shape can never be selected
                dst1[i][k][3] = MBINVALID_DIST;
            }
            mod1[i][k][1] = (0x100<<((k<<1)+i));
            mod1[i][k][2] |= mod1[i][k][1];
            mod1[i][k][3] = mod1[i][k][1]|(3<<(k<<1));
        }
    }
    
    if(Vsta.BidirMask&UNIDIR_NO_MIX){ // No mixing
        for(i=0;i<2;i++){
// Stage 2: 
        // MV 2, MV 4, MV 8:
            mod2[i][0][0] = mod1[i][0][1]|mod1[i][1][1];    mod2[i][1][0] = mod1[i][2][1]|mod1[i][3][1];
            dst2[i][0][0] = dst1[i][0][1]+dst1[i][1][1];    dst2[i][1][0] = dst1[i][2][1]+dst1[i][3][1];
            mod2[i][0][2] = mod1[i][0][2]|mod1[i][1][2];    mod2[i][1][2] = mod1[i][2][2]|mod1[i][3][2];
            dst2[i][0][2] = dst1[i][0][2]+dst1[i][1][2];    dst2[i][1][2] = dst1[i][2][2]+dst1[i][3][2];
            mod2[i][0][5] = mod1[i][0][3]|mod1[i][1][3];    mod2[i][1][5] = mod1[i][2][3]|mod1[i][3][3];
            dst2[i][0][5] = dst1[i][0][3]+dst1[i][1][3];    dst2[i][1][5] = dst1[i][2][3]+dst1[i][3][3];
        // MV 3: 1+2, 2+1
            mod2[i][0][1] = mod1[i][0][1]|mod1[i][1][2];
            dst2[i][0][1] = dst1[i][0][1]+dst1[i][1][2];
            if((j = dst1[i][0][2]+dst1[i][1][1])<dst2[i][0][1]){
                dst2[i][0][1] = j; 
                mod2[i][0][1] = mod1[i][0][2]|mod1[i][1][1];
            }
            mod2[i][1][1] = mod1[i][2][1]|mod1[i][3][2];
            dst2[i][1][1] = dst1[i][2][1]+dst1[i][3][2];
            if((j = dst1[i][2][2]+dst1[i][3][1])<dst2[i][1][1]){
                dst2[i][1][1] = j; 
                mod2[i][1][1] = mod1[i][2][2]|mod1[i][3][1];
            }
        // MV 5: 1+4, 4+1
            mod2[i][0][3] = mod1[i][0][1]|mod1[i][1][3];
            dst2[i][0][3] = dst1[i][0][1]+dst1[i][1][3];
            if((j = dst1[i][0][3]+dst1[i][1][1])<dst2[i][0][3]){
                dst2[i][0][3] = j; 
                mod2[i][0][3] = mod1[i][0][3]|mod1[i][1][1];
            }
            mod2[i][1][3] = mod1[i][2][1]|mod1[i][3][3];
            dst2[i][1][3] = dst1[i][2][1]+dst1[i][3][3];
            if((j = dst1[i][2][3]+dst1[i][3][1])<dst2[i][1][3]){
                dst2[i][1][3] = j; 
                mod2[i][1][3] = mod1[i][2][3]|mod1[i][3][1];
            }
        // MV 6: 2+4, 4+2
            mod2[i][0][4] = mod1[i][0][2]|mod1[i][1][3];
            dst2[i][0][4] = dst1[i][0][2]+dst1[i][1][3];
            if((j = dst1[i][0][3]+dst1[i][1][2])<dst2[i][0][4]){
                dst2[i][0][4] = j; 
                mod2[i][0][4] = mod1[i][0][3]|mod1[i][1][2];
            }
            mod2[i][1][4] = mod1[i][2][2]|mod1[i][3][3];
            dst2[i][1][4] = dst1[i][2][2]+dst1[i][3][3];
            if((j = dst1[i][2][3]+dst1[i][3][2])<dst2[i][1][4]){
                dst2[i][1][4] = j; 
                mod2[i][1][4] = mod1[i][2][3]|mod1[i][3][2];
            }
// Stage 3: 
        // MV 4 = 2+2
            dst0[i] = dst2[i][0][0]+dst2[i][1][0];
            mod0[i] = mod2[i][0][0]|mod2[i][1][0];
            if(Vsta.MaxMvs<5){ goto NEXT1; }
        // MV 5 = 3+2, 2+3
            if((d=dst2[i][0][1]+dst2[i][1][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][1]|mod2[i][1][0]; }
            if((d=dst2[i][1][1]+dst2[i][0][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][1]|mod2[i][0][0]; }
            if(Vsta.MaxMvs<6){ goto NEXT1; }
        // MV 6 = 4+2, 2+4, 3+3 
            if((d=dst2[i][0][2]+dst2[i][1][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][2]|mod2[i][1][0]; }
            if((d=dst2[i][1][2]+dst2[i][0][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][2]|mod2[i][0][0]; }
            if((d=dst2[i][1][1]+dst2[i][0][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][1]|mod2[i][0][1]; }
            if(Vsta.MaxMvs<7){ goto NEXT1; }
        // MV 7 = 4+3, 3+4, 5+2, 2+5
            if((d=dst2[i][0][2]+dst2[i][1][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][2]|mod2[i][1][1]; }
            if((d=dst2[i][1][2]+dst2[i][0][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][2]|mod2[i][0][1]; }
            if((d=dst2[i][0][3]+dst2[i][1][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][3]|mod2[i][1][0]; }
            if((d=dst2[i][1][3]+dst2[i][0][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][3]|mod2[i][0][0]; }
            if(Vsta.MaxMvs<8){ goto NEXT1; }
        // MV 8 = 5+3, 3+5, 4+4, 6+2, 2+6
            if((d=dst2[i][0][3]+dst2[i][1][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][3]|mod2[i][1][1]; }
            if((d=dst2[i][1][3]+dst2[i][0][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][3]|mod2[i][0][1]; }
            if((d=dst2[i][0][2]+dst2[i][1][2])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][2]|mod2[i][1][2]; }
            if((d=dst2[i][0][4]+dst2[i][1][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][4]|mod2[i][1][0]; }
            if((d=dst2[i][1][4]+dst2[i][0][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][4]|mod2[i][0][0]; }
            if(Vsta.MaxMvs<9){ goto NEXT1; }
        // MV 9 = 5+4, 4+5, 6+3, 3+6
            if((d=dst2[i][0][3]+dst2[i][1][2])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][3]|mod2[i][1][2]; }
            if((d=dst2[i][1][3]+dst2[i][0][2])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][3]|mod2[i][0][2]; }
            if((d=dst2[i][0][4]+dst2[i][1][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][4]|mod2[i][1][1]; }
            if((d=dst2[i][1][4]+dst2[i][0][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][4]|mod2[i][0][1]; }
            if(Vsta.MaxMvs<10){ goto NEXT1; }
        // MV 10 = 6+4, 4+6, 5+5, 8+2, 2+8
            if((d=dst2[i][0][4]+dst2[i][1][2])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][4]|mod2[i][1][2]; }
            if((d=dst2[i][1][4]+dst2[i][0][2])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][4]|mod2[i][0][2]; }
            if((d=dst2[i][0][3]+dst2[i][1][3])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][3]|mod2[i][1][3]; }
            if((d=dst2[i][0][5]+dst2[i][1][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][5]|mod2[i][1][0]; }
            if((d=dst2[i][1][5]+dst2[i][0][0])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][5]|mod2[i][0][0]; }
            if(Vsta.MaxMvs<11){ goto NEXT1; }
        // MV 11 = 6+5, 5+6, 8+3, 3+8
            if((d=dst2[i][0][4]+dst2[i][1][3])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][4]|mod2[i][1][3]; }
            if((d=dst2[i][1][4]+dst2[i][0][3])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][4]|mod2[i][0][3]; }
            if((d=dst2[i][0][5]+dst2[i][1][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][5]|mod2[i][1][1]; }
            if((d=dst2[i][1][5]+dst2[i][0][1])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][5]|mod2[i][0][1]; }
            if(Vsta.MaxMvs<12){ goto NEXT1; }
        // MV 12 = 8+4, 4+8, 6+6
            if((d=dst2[i][0][5]+dst2[i][1][2])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][5]|mod2[i][1][2]; }
            if((d=dst2[i][1][5]+dst2[i][0][2])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][5]|mod2[i][0][2]; }
            if((d=dst2[i][0][4]+dst2[i][1][4])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][4]|mod2[i][1][4]; }
            if(Vsta.MaxMvs<13){ goto NEXT1; }
        // MV 13 = 8+5, 5+8
            if((d=dst2[i][0][5]+dst2[i][1][3])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][5]|mod2[i][1][3]; }
            if((d=dst2[i][1][5]+dst2[i][0][3])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][5]|mod2[i][0][3]; }
            if(Vsta.MaxMvs<14){ goto NEXT1; }
        // MV 14 = 8+6, 6+8
            if((d=dst2[i][0][5]+dst2[i][1][4])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][5]|mod2[i][1][4]; }
            if((d=dst2[i][1][5]+dst2[i][0][4])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][1][5]|mod2[i][0][4]; }
            if(Vsta.MaxMvs<16){ goto NEXT1; }
        // MV 16 = 8+8
            if((d=dst2[i][0][5]+dst2[i][1][5])<dst0[i]){ dst0[i] = d; mod0[i] = mod2[i][0][5]|mod2[i][1][5]; }

NEXT1:    
            // MAX MV ANALYSIS (DEBUG DCN)-- Todo: Abstract this to new function (JMH)
            tempdist[i] = dst0[i];
            if((d=dst2[i][0][0]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][1]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][1]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][2]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][2]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][1]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][2]+dst2[i][1][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][2]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][3]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][3]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][3]+dst2[i][1][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][3]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][2]+dst2[i][1][2])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][4]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][4]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][3]+dst2[i][1][2])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][3]+dst2[i][0][2])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][4]+dst2[i][1][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][4]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][4]+dst2[i][1][2])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][4]+dst2[i][0][2])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][3]+dst2[i][1][3])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][5]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][5]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][4]+dst2[i][1][3])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][4]+dst2[i][0][3])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][5]+dst2[i][1][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][5]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][5]+dst2[i][1][2])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][5]+dst2[i][0][2])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][4]+dst2[i][1][4])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][5]+dst2[i][1][3])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][5]+dst2[i][0][3])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][5]+dst2[i][1][4])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][1][5]+dst2[i][0][4])<tempdist[i]){ tempdist[i] = d; }
            if((d=dst2[i][0][5]+dst2[i][1][5])<tempdist[i]){ tempdist[i] = d; }
            // END MAX MV ANALYSIS

            continue;
        }
        dst0[!L0_BWD_Penalty] += (LutMode[LUTMODE_INTER_BWD]<<2);
        tempdist[!L0_BWD_Penalty] += (LutMode[LUTMODE_INTER_BWD]<<2);
        UniOpportunitySad[0][3] = tempdist[0];
        UniOpportunitySad[1][3] = tempdist[1];

        if (!IgnoreMaxMv)
        {
            UniDirMajorDist[0][3] = dst0[0]; 
            UniDirMajorDist[1][3] = dst0[1]; 
            UniDirMajorMode[0][3] = mod0[0];
            UniDirMajorMode[1][3] = mod0[1];
        }

        if(tempdist[1] < tempdist[0]) tempdist[0] = tempdist[1];
        if(dst0[0]<=dst0[1]){ 
            if(!IgnoreMaxMv) {*MinorDist = dst0[0]; *MinorMode = mod0[0];}
            else {*MinorDist = tempdist[0];}
        }
        else{ 
            if(!IgnoreMaxMv) {*MinorDist = dst0[1]; *MinorMode = mod0[1];}
            else {*MinorDist = tempdist[0];}
        }

        return;
    }

    // if allow UNIDIR mixing:
    if ((Vsta.SadType &INTER_CHROMA_MODE) && (!(Vsta.ShapeMask&0x40))){
        // chroma 4x4 partition, able to mix from two directions within a 8x8 block, copy the best to dst1[1]
            j = B4X4; //only computes 1st 8x8 block, 2nd-4th duplicated from 1st block
            tempModChroma4x4 = 0;
            dst1[1][0][3] = LutMode[LUTMODE_INTER_4x4q];
            for (i = 0; i < 4; i++){
                if (Dist[0][j+i]<= Dist[1][j+i]){
                    dst1[1][0][3] += Dist[0][j+i];
                    tempModChroma4x4 |= mod1[0][i][3];
                }
                else {
                    dst1[1][0][3] += Dist[1][j+i];
                    tempModChroma4x4 |= mod1[1][i][3];    
                }
            }
            dst1[1][0][3] = (dst1[1][0][3] > 0xffff) ? 0xffff : dst1[1][0][3];    
            dst1[1][3][3] = dst1[1][2][3] = dst1[1][1][3] = dst1[1][0][3];
        for (k=0;k<4;k++)
            mod1[1][k][3] = tempModChroma4x4;
    }
        for(k=0;k<4;k++){
            for(j=1;j<4;j++){
                dst1[!L0_BWD_Penalty][k][j] += LutMode[LUTMODE_INTER_BWD];
                if((dst1[0][k][j]<dst1[1][k][j]) || (dst1[0][k][j]== dst1[1][k][j] && j!= 2 ) || 
                    (j == 2 && dst1[0][k][j]==dst1[1][k][j] && minorMode[0][k] == minorMode[1][k]) ||
                    (j == 2 && dst1[0][k][j]==dst1[1][k][j] && minorMode[0][k] == 0)){
                    dst1[1][k][j] = dst1[0][k][j];
                    mod1[1][k][j] = mod1[0][k][j];
                }
            }
        }
// Stage 2: 
        // MV 2, MV 4, MV 8:
            mod2[1][0][0] = mod1[1][0][1]|mod1[1][1][1];    mod2[1][1][0] = mod1[1][2][1]|mod1[1][3][1];
            dst2[1][0][0] = dst1[1][0][1]+dst1[1][1][1];    dst2[1][1][0] = dst1[1][2][1]+dst1[1][3][1];
            mod2[1][0][2] = mod1[1][0][2]|mod1[1][1][2];    mod2[1][1][2] = mod1[1][2][2]|mod1[1][3][2];
            dst2[1][0][2] = dst1[1][0][2]+dst1[1][1][2];    dst2[1][1][2] = dst1[1][2][2]+dst1[1][3][2];
            mod2[1][0][5] = mod1[1][0][3]|mod1[1][1][3];    mod2[1][1][5] = mod1[1][2][3]|mod1[1][3][3];
            dst2[1][0][5] = dst1[1][0][3]+dst1[1][1][3];    dst2[1][1][5] = dst1[1][2][3]+dst1[1][3][3];
        // MV 3: 1+2, 2+1
            mod2[1][0][1] = mod1[1][0][1]|mod1[1][1][2];
            dst2[1][0][1] = dst1[1][0][1]+dst1[1][1][2];
            if((j = dst1[1][0][2]+dst1[1][1][1])<dst2[1][0][1]){
                dst2[1][0][1] = j; 
                mod2[1][0][1] = mod1[1][0][2]|mod1[1][1][1];
            }
            mod2[1][1][1] = mod1[1][2][1]|mod1[1][3][2];
            dst2[1][1][1] = dst1[1][2][1]+dst1[1][3][2];
            if((j = dst1[1][2][2]+dst1[1][3][1])<dst2[1][1][1]){
                dst2[1][1][1] = j; 
                mod2[1][1][1] = mod1[1][2][2]|mod1[1][3][1];
            }
        // MV 5: 1+4, 4+1
            mod2[1][0][3] = mod1[1][0][1]|mod1[1][1][3];
            dst2[1][0][3] = dst1[1][0][1]+dst1[1][1][3];
            if((j = dst1[1][0][3]+dst1[1][1][1])<dst2[1][0][3]){
                dst2[1][0][3] = j; 
                mod2[1][0][3] = mod1[1][0][3]|mod1[1][1][1];
            }
            mod2[1][1][3] = mod1[1][2][1]|mod1[1][3][3];
            dst2[1][1][3] = dst1[1][2][1]+dst1[1][3][3];
            if((j = dst1[1][2][3]+dst1[1][3][1])<dst2[1][1][3]){
                dst2[1][1][3] = j; 
                mod2[1][1][3] = mod1[1][2][3]|mod1[1][3][1];
            }
        // MV 6: 2+4, 4+2
            mod2[1][0][4] = mod1[1][0][2]|mod1[1][1][3];
            dst2[1][0][4] = dst1[1][0][2]+dst1[1][1][3];
            if((j = dst1[1][0][3]+dst1[1][1][2])<dst2[1][0][4]){
                dst2[1][0][4] = j; 
                mod2[1][0][4] = mod1[1][0][3]|mod1[1][1][2];
            }
            mod2[1][1][4] = mod1[1][2][2]|mod1[1][3][3];
            dst2[1][1][4] = dst1[1][2][2]+dst1[1][3][3];
            if((j = dst1[1][2][3]+dst1[1][3][2])<dst2[1][1][4]){
                dst2[1][1][4] = j; 
                mod2[1][1][4] = mod1[1][2][3]|mod1[1][3][2];
            }

// Stage 3: 
        // MV 4 = 2+2
            dst0[1] = dst2[1][0][0]+dst2[1][1][0];
            mod0[1] = mod2[1][0][0]|mod2[1][1][0];
    if(Vsta.MaxMvs<5){ goto NEXT2; }    
        // MV 5 = 3+2, 2+3
            if((d=dst2[1][0][1]+dst2[1][1][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][1]|mod2[1][1][0]; }
    if((d=dst2[1][1][1]+dst2[1][0][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][1]|mod2[1][0][0]; }
            if(Vsta.MaxMvs<6){ goto NEXT2; }
        // MV 6 = 4+2, 2+4, 3+3 
            if((d=dst2[1][0][2]+dst2[1][1][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][2]|mod2[1][1][0]; }
            if((d=dst2[1][1][2]+dst2[1][0][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][2]|mod2[1][0][0]; }
            if((d=dst2[1][1][1]+dst2[1][0][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][1]|mod2[1][0][1]; }
    if(Vsta.MaxMvs<7){ goto NEXT2; }
        // MV 7 = 4+3, 3+4, 5+2, 2+5
            if((d=dst2[1][0][2]+dst2[1][1][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][2]|mod2[1][1][1]; }
            if((d=dst2[1][1][2]+dst2[1][0][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][2]|mod2[1][0][1]; }
            if((d=dst2[1][0][3]+dst2[1][1][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][3]|mod2[1][1][0]; }
            if((d=dst2[1][1][3]+dst2[1][0][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][3]|mod2[1][0][0]; }
    if(Vsta.MaxMvs<8){ goto NEXT2; }
        // MV 8 = 5+3, 3+5, 4+4, 6+2, 2+6
            if((d=dst2[1][0][3]+dst2[1][1][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][3]|mod2[1][1][1]; }
            if((d=dst2[1][1][3]+dst2[1][0][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][3]|mod2[1][0][1]; }
            if((d=dst2[1][0][2]+dst2[1][1][2])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][2]|mod2[1][1][2]; }
            if((d=dst2[1][0][4]+dst2[1][1][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][4]|mod2[1][1][0]; }
            if((d=dst2[1][1][4]+dst2[1][0][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][4]|mod2[1][0][0]; }
    if(Vsta.MaxMvs<9){ goto NEXT2; }
        // MV 9 = 5+4, 4+5, 6+3, 3+6
            if((d=dst2[1][0][3]+dst2[1][1][2])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][3]|mod2[1][1][2]; }
            if((d=dst2[1][1][3]+dst2[1][0][2])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][3]|mod2[1][0][2]; }
            if((d=dst2[1][0][4]+dst2[1][1][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][4]|mod2[1][1][1]; }
            if((d=dst2[1][1][4]+dst2[1][0][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][4]|mod2[1][0][1]; }
    if(Vsta.MaxMvs<10){ goto NEXT2; }
        // MV 10 = 6+4, 4+6, 5+5, 8+2, 2+8
            if((d=dst2[1][0][4]+dst2[1][1][2])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][4]|mod2[1][1][2]; }
            if((d=dst2[1][1][4]+dst2[1][0][2])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][4]|mod2[1][0][2]; }
            if((d=dst2[1][0][3]+dst2[1][1][3])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][3]|mod2[1][1][3]; }
            if((d=dst2[1][0][5]+dst2[1][1][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][5]|mod2[1][1][0]; }
            if((d=dst2[1][1][5]+dst2[1][0][0])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][5]|mod2[1][0][0]; }
    if(Vsta.MaxMvs<11){ goto NEXT2; }
        // MV 11 = 6+5, 5+6, 8+3, 3+8
            if((d=dst2[1][0][4]+dst2[1][1][3])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][4]|mod2[1][1][3]; }
            if((d=dst2[1][1][4]+dst2[1][0][3])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][4]|mod2[1][0][3]; }
            if((d=dst2[1][0][5]+dst2[1][1][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][5]|mod2[1][1][1]; }
            if((d=dst2[1][1][5]+dst2[1][0][1])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][5]|mod2[1][0][1]; }
    if(Vsta.MaxMvs<12){ goto NEXT2; }
        // MV 12 = 8+4, 4+8, 6+6
            if((d=dst2[1][0][5]+dst2[1][1][2])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][5]|mod2[1][1][2]; }
            if((d=dst2[1][1][5]+dst2[1][0][2])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][5]|mod2[1][0][2]; }
            if((d=dst2[1][0][4]+dst2[1][1][4])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][4]|mod2[1][1][4]; }
    if(Vsta.MaxMvs<13){ goto NEXT2; }
        // MV 13 = 8+5, 5+8
            if((d=dst2[1][0][5]+dst2[1][1][3])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][5]|mod2[1][1][3]; }
            if((d=dst2[1][1][5]+dst2[1][0][3])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][5]|mod2[1][0][3]; }
    if(Vsta.MaxMvs<14){ goto NEXT2; }
        // MV 14 = 8+6, 6+8
            if((d=dst2[1][0][5]+dst2[1][1][4])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][5]|mod2[1][1][4]; }
            if((d=dst2[1][1][5]+dst2[1][0][4])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][1][5]|mod2[1][0][4]; }
    if(Vsta.MaxMvs<16){ goto NEXT2; }
        // MV 16 = 8+8
            if((d=dst2[1][0][5]+dst2[1][1][5])<dst0[1]){ dst0[1] = d; mod0[1] = mod2[1][0][5]|mod2[1][1][5]; }
NEXT2:        

    i = 1;
    // MAX MV ANALYSIS (DEBUG DCN)-- Todo: Abstract this to new function (JMH)
    tempdist[i] = dst0[i];
    if((d=dst2[i][0][0]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][1]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][1]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][2]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][2]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][1]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][2]+dst2[i][1][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][2]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][3]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][3]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][3]+dst2[i][1][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][3]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][2]+dst2[i][1][2])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][4]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][4]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][3]+dst2[i][1][2])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][3]+dst2[i][0][2])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][4]+dst2[i][1][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][4]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][4]+dst2[i][1][2])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][4]+dst2[i][0][2])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][3]+dst2[i][1][3])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][5]+dst2[i][1][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][5]+dst2[i][0][0])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][4]+dst2[i][1][3])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][4]+dst2[i][0][3])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][5]+dst2[i][1][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][5]+dst2[i][0][1])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][5]+dst2[i][1][2])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][5]+dst2[i][0][2])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][4]+dst2[i][1][4])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][5]+dst2[i][1][3])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][5]+dst2[i][0][3])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][5]+dst2[i][1][4])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][1][5]+dst2[i][0][4])<tempdist[i]){ tempdist[i] = d; }
    if((d=dst2[i][0][5]+dst2[i][1][5])<tempdist[i]){ tempdist[i] = d; }

    if(!IgnoreMaxMv)
    {
        if((dst0[i]>tempdist[i])&&(FreeMVDistInter>tempdist[i]))
            Vout->ImeStop_MaxMV_AltNumSU |= CAPPED_MAXMV;
    }
    else
    {
        if(FreeMVDistInter>tempdist[i])
            Vout->ImeStop_MaxMV_AltNumSU |= CAPPED_MAXMV;
    }
    // END MAX MV ANALYSIS
    
    if(!IgnoreMaxMv) {*MinorDist = dst0[1]; *MinorMode = mod0[1]; }
    else {*MinorDist = tempdist[i];}
    
    return;
}


void GetFourSAD4x4_viaIPP(U8* pMB, int SrcStep, U8* pSrc, U32* dif)
{
    Ipp32s mcType = 0;
    Ipp32s sad;

    for(int i = 0; i < 4; i++)
    {
        ippiSAD4x4_8u32s(
            pSrc + i*4, 
            SrcStep, 
            pMB + i*4, 
            16, 
            &sad, 
            mcType);

        dif[i] = (mfxU32)sad;
    }
}
/*****************************************************************************************************/
void MEforGen75::GetDistortionSU(U8 *src, U8   *ref, I32 *dif)
/*****************************************************************************************************/
{
    int        xoff = (Vsta.SrcType&2) ? 4 : 8;
//Pattern is applicable to IME only as per BSpec/DCN for HSW
    unsigned char Pattern[4][4] = {{2,1,1,0},{1,2,0,1},{1,0,2,1},{0,1,1,2}};
    if (((Vsta.SrcType&0x3) != ST_SRC_16X16) || !(Vsta.MvFlags&WEIGHTED_SAD_HAAR)) //only apply to luma 16x16
        memset(Pattern,0,sizeof(unsigned char)*16);
    
    bool SadOpt = false;
#if (__SAD_OPT)
    if (SadMethod == 0) { // SAD
        U32 sad[4];
        GetFourSAD4x4(src, REFWINDOWWIDTH, ref, sad);
        dif[0] += sad[0] >> (Pattern[0][0]);
        dif[1] += sad[1] >> (Pattern[0][1]);
        dif[2] += sad[2] >> (Pattern[1][0]);
        dif[3] += sad[3] >> (Pattern[1][1]);
        src += 64; ref += REFWINDOWWIDTH<<2;
        GetFourSAD4x4(src, REFWINDOWWIDTH, ref, sad);
        dif[4] += sad[0] >> (Pattern[0][2]);
        dif[5] += sad[1] >> (Pattern[0][3]);
        dif[6] += sad[2] >> (Pattern[1][2]);
        dif[7] += sad[3] >> (Pattern[1][3]);
        src += 64; 
        if(!(Vsta.SrcType&1)) ref += REFWINDOWWIDTH<<2;    
        GetFourSAD4x4(src, REFWINDOWWIDTH, ref, sad);
        dif[8] += sad[0] >> (Pattern[2][0]);
        dif[9] += sad[1] >> (Pattern[2][1]);
        dif[10] += sad[2] >> (Pattern[3][0]);
        dif[11] += sad[3] >> (Pattern[3][1]);
        src += 64; ref += REFWINDOWWIDTH<<2;
        GetFourSAD4x4(src, REFWINDOWWIDTH, ref, sad);
        dif[12] += sad[0] >> (Pattern[2][2]);
        dif[13] += sad[1] >> (Pattern[2][3]);
        dif[14] += sad[2] >> (Pattern[3][2]);
        dif[15] += sad[3] >> (Pattern[3][3]);
        SadOpt = true;
    }
#endif
#if (__HAAR_SAD_OPT)
    if (SadMethod == 2) { // HAAR
        int sad[2];
        GetSad4x4_par(src,ref,REFWINDOWWIDTH,sad);
        dif[0] += sad[0] >> (Pattern[0][0]);
        dif[1] += sad[1] >> (Pattern[0][1]);
        GetSad4x4_par(src+8,ref+xoff,REFWINDOWWIDTH,sad);
        dif[2] += sad[0] >> (Pattern[1][0]);
        dif[3] += sad[1] >> (Pattern[1][1]);
        src += 64; ref += REFWINDOWWIDTH<<2;
        GetSad4x4_par(src,ref,REFWINDOWWIDTH,sad);
        dif[4] += sad[0] >> (Pattern[0][2]);
        dif[5] += sad[1] >> (Pattern[0][3]);
        GetSad4x4_par(src+8,ref+xoff,REFWINDOWWIDTH,sad);
        dif[6] += sad[0] >> (Pattern[1][2]);
        dif[7] += sad[1] >> (Pattern[1][3]);
        src += 64;
        if(!(Vsta.SrcType&1)) ref += REFWINDOWWIDTH<<2;
        GetSad4x4_par(src,ref,REFWINDOWWIDTH,sad);
        dif[8] += sad[0] >> (Pattern[2][0]);
        dif[9] += sad[1] >> (Pattern[2][1]);
        GetSad4x4_par(src+8,ref+xoff,REFWINDOWWIDTH,sad);
        dif[10] += sad[0] >> (Pattern[3][0]);
        dif[11] += sad[1] >> (Pattern[3][1]);
        src += 64; ref += REFWINDOWWIDTH<<2;
        GetSad4x4_par(src,ref,REFWINDOWWIDTH,sad);
        dif[12] += sad[0] >> (Pattern[2][2]);
        dif[13] += sad[1] >> (Pattern[2][3]);
        GetSad4x4_par(src+8,ref+xoff,REFWINDOWWIDTH,sad);
        dif[14] += sad[0] >> (Pattern[3][2]);
        dif[15] += sad[1] >> (Pattern[3][3]);
        SadOpt = true;
    }
#endif
    if (!SadOpt) {
        /*dif[0] += GetSad4x4(src,ref,REFWINDOWWIDTH)>>(Pattern[0][0]);
        dif[1] += GetSad4x4(src+4,ref+4,REFWINDOWWIDTH)>>(Pattern[0][1]);    
        dif[2] += GetSad4x4(src+8,ref+xoff,REFWINDOWWIDTH)>>(Pattern[1][0]);
        dif[3] += GetSad4x4(src+12,ref+xoff+4,REFWINDOWWIDTH)>>(Pattern[1][1]);

        src += 64; ref += REFWINDOWWIDTH<<2;    
        dif[4] += GetSad4x4(src,ref,REFWINDOWWIDTH)>>(Pattern[0][2]);
        dif[5] += GetSad4x4(src+4,ref+4,REFWINDOWWIDTH)>>(Pattern[0][3]);
        dif[6] += GetSad4x4(src+8,ref+xoff,REFWINDOWWIDTH)>>(Pattern[1][2]);
        dif[7] += GetSad4x4(src+12,ref+xoff+4,REFWINDOWWIDTH)>>(Pattern[1][3]);
        src += 64; 
        if(!(Vsta.SrcType&1)) ref += REFWINDOWWIDTH<<2;    

        dif[8] += GetSad4x4(src,ref,REFWINDOWWIDTH)>>(Pattern[2][0]);
        dif[9] += GetSad4x4(src+4,ref+4,REFWINDOWWIDTH)>>(Pattern[2][1]);
        dif[10] += GetSad4x4(src+8,ref+xoff,REFWINDOWWIDTH)>>(Pattern[3][0]);
        dif[11] += GetSad4x4(src+12,ref+xoff+4,REFWINDOWWIDTH)>>(Pattern[3][1]);
        src += 64; ref += REFWINDOWWIDTH<<2;    

        dif[12] += GetSad4x4(src,ref,REFWINDOWWIDTH)>>(Pattern[2][2]);
        dif[13] += GetSad4x4(src+4,ref+4,REFWINDOWWIDTH)>>(Pattern[2][3]);
        dif[14] += GetSad4x4(src+8,ref+xoff,REFWINDOWWIDTH)>>(Pattern[3][2]);
        dif[15] += GetSad4x4(src+12,ref+xoff+4,REFWINDOWWIDTH)>>(Pattern[3][3]);*/

        U32 sad[4];

        GetFourSAD4x4_viaIPP(src, REFWINDOWWIDTH, ref, sad);

        dif[0] += sad[0] >> (Pattern[0][0]);
        dif[1] += sad[1] >> (Pattern[0][1]);
        dif[2] += sad[2] >> (Pattern[1][0]);
        dif[3] += sad[3] >> (Pattern[1][1]);
        src += 64; ref += REFWINDOWWIDTH<<2;

        GetFourSAD4x4_viaIPP(src, REFWINDOWWIDTH, ref, sad);

        dif[4] += sad[0] >> (Pattern[0][2]);
        dif[5] += sad[1] >> (Pattern[0][3]);
        dif[6] += sad[2] >> (Pattern[1][2]);
        dif[7] += sad[3] >> (Pattern[1][3]);
        src += 64; 
        if(!(Vsta.SrcType&1)) ref += REFWINDOWWIDTH<<2;    

        GetFourSAD4x4_viaIPP(src, REFWINDOWWIDTH, ref, sad);
        
        dif[8] += sad[0] >> (Pattern[2][0]);
        dif[9] += sad[1] >> (Pattern[2][1]);
        dif[10] += sad[2] >> (Pattern[3][0]);
        dif[11] += sad[3] >> (Pattern[3][1]);
        src += 64; ref += REFWINDOWWIDTH<<2;

        GetFourSAD4x4_viaIPP(src, REFWINDOWWIDTH, ref, sad);
        
        dif[12] += sad[0] >> (Pattern[2][2]);
        dif[13] += sad[1] >> (Pattern[2][3]);
        dif[14] += sad[2] >> (Pattern[3][2]);
        dif[15] += sad[3] >> (Pattern[3][3]);
        SadOpt = true;
    }
}

/*****************************************************************************************************/
void MEforGen75::GetDistortionSUChroma(U8 *srcU, U8 *srcV, U8 *refU, U8 *refV, I32 *dif)
/*****************************************************************************************************/
{
    U8 srcMBUV[128];
    U8 refUV[REFWINDOWWIDTH*8];//ref data for one search location
    unsigned char i,j;
    U8 *src = srcMBUV;
    U8 *ref = refUV;

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            srcMBUV[16*i + 2*j] = srcU[16*i + j];
            srcMBUV[16*i + 2*j+1] = srcV[16*i + j];
        }
    }

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            refUV[REFWINDOWWIDTH*i + 2*j] = refU[REFWINDOWWIDTH*i + j];
            refUV[REFWINDOWWIDTH*i + 2*j+1] = refV[REFWINDOWWIDTH*i + j];
        }
    }

    dif[0] = GetSad4x4(src,ref,REFWINDOWWIDTH) + GetSad4x4(src+4,ref+4,REFWINDOWWIDTH);
    dif[1] = GetSad4x4(src+8,ref+8,REFWINDOWWIDTH) + GetSad4x4(src+12,ref+12,REFWINDOWWIDTH);
    src += 64; ref += REFWINDOWWIDTH<<2;
    dif[4] = GetSad4x4(src,ref,REFWINDOWWIDTH) + GetSad4x4(src+4,ref+4,REFWINDOWWIDTH);
    dif[5] = GetSad4x4(src+8,ref+8,REFWINDOWWIDTH) + GetSad4x4(src+12,ref+12,REFWINDOWWIDTH);

    dif[2] = MBMAXDIST;
    dif[3] = MBMAXDIST;
    for(i = 6; i < 16; i++)
        dif[i] = MBMAXDIST;
}

/*****************************************************************************************************/
int MEforGen75::GetDistortionSUField(U8   *ref, I32 *dif)
/*****************************************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
    U8      *src = SrcFieldMB;
    int        xoff = (Vsta.SrcType&2) ? 4 : 8;
    if(Vsta.SrcType&1) exit(1); // error case: 16x8 and 8x8 modes not allowed for field mode.
    dif[0] = GetSad4x4F(src,ref,128);
    dif[1] = GetSad4x4F(src+4,ref+4,128);
    dif[2] = GetSad4x4F(src+8,ref+xoff,128);
    dif[3] = GetSad4x4F(src+12,ref+xoff+4,128);
    src += 16; ref += 64;
    dif[4] = GetSad4x4F(src,ref,128);
    dif[5] = GetSad4x4F(src+4,ref+4,128);
    dif[6] = GetSad4x4F(src+8,ref+xoff,128);
    dif[7] = GetSad4x4F(src+12,ref+xoff+4,128);
    src += 112; ref += 448;
    dif[8] = GetSad4x4F(src,ref,128);
    dif[9] = GetSad4x4F(src+4,ref+4,128);
    dif[10] = GetSad4x4F(src+8,ref+xoff,128);
    dif[11] = GetSad4x4F(src+12,ref+xoff+4,128);
    src += 16; ref += 64;
    dif[12] = GetSad4x4F(src,ref,128);
    dif[13] = GetSad4x4F(src+4,ref+4,128);
    dif[14] = GetSad4x4F(src+8,ref+xoff,128);
    dif[15] = GetSad4x4F(src+12,ref+xoff+4,128);
    */
}

/*****************************************************************************************************/
void MEforGen75::LoadSrcMB( )
/*****************************************************************************************************/
{
    int        j, w = Vsta.SrcPitch;
    int        i;
    U8      *blk = SrcMB;
    U8        *pix = ((Vsta.MvFlags & SRC_FIELD_POLARITY) && (Vsta.SrcType&ST_SRC_FIELD)) ? Vsta.SrcLuma + w : Vsta.SrcLuma;
    if(Vsta.SrcType&ST_SRC_FIELD)
        w <<= 1;

    pix += Vsta.SrcMB.x + Vsta.SrcMB.y*w;
    MFX_INTERNAL_CPY(blk,pix,16);
    for(j=7;j>0;j--)
    {
        blk+=16; pix+=w;
        MFX_INTERNAL_CPY(blk,pix,16);
    }
    if(Vsta.SrcType&1){ pix = SrcMB-16; w = 16; } // for 16x8 and 8x8  
    for(j=8;j>0;j--)
    {
        blk+=16; pix+=w;
        MFX_INTERNAL_CPY(blk,pix,16);
    }
    if(Vsta.SrcType&2){    // for 8x16 ans 8x8
        blk=SrcMB+8;
        pix=SrcMB;
        MFX_INTERNAL_CPY(blk,pix,8);
        for(j=15;j>0;j--)
        {
            blk+=16; pix+=16;
            MFX_INTERNAL_CPY(blk,pix,8);
        }
    }
    if(Vsta.SrcType&0x0C){ // field search allowed
        blk = SrcMB;
        pix = SrcFieldMB;
        for(j=8;j>0;j--){
            MFX_INTERNAL_CPY(pix,blk,16);
            MFX_INTERNAL_CPY(pix+128,blk+16,16);
            blk += 32; pix += 16;
        }
    }

    // no need to load chroma if this happens
    if ((!(Vsta.SadType&INTER_CHROMA_MODE)) && (VSICsta.IntraComputeType!=INTRA_LUMA_CHROMA)) return;

    // we don't use U/V VME
    return;

    // Chroma
    w = Vsta.SrcPitch;
    U8          *pixCb = ((Vsta.MvFlags & SRC_FIELD_POLARITY) && (Vsta.SrcType&ST_SRC_FIELD)) ? Vsta.SrcCb + w : Vsta.SrcCb;
    U8          *pixCr = ((Vsta.MvFlags & SRC_FIELD_POLARITY) && (Vsta.SrcType&ST_SRC_FIELD)) ? Vsta.SrcCr + w : Vsta.SrcCr;
    if(Vsta.SrcType&ST_SRC_FIELD)
        w <<= 1;
    pixCb += (Vsta.SrcMB.x>>1) + (Vsta.SrcMB.y>>1)*w;
    pixCr += (Vsta.SrcMB.x>>1) + (Vsta.SrcMB.y>>1)*w;
    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
              //SrcUV_NV12[2*(8*i+j)+0] = pixCb[j];
              //SrcUV_NV12[2*(8*i+j)+1] = pixCr[j];

              SrcUV[(16*i+j)+0] = pixCb[j];
              SrcUV[(16*i+j)+8] = pixCr[j];

              SrcMBU[(16*i+j)] = SrcMBU[(16*i+j+8)] = SrcMBU[(16*(i+8)+j)] = SrcMBU[(16*(i+8)+j+8)] = pixCb[j];
              SrcMBV[(16*i+j)] = SrcMBV[(16*i+j+8)] = SrcMBV[(16*(i+8)+j)] = SrcMBV[(16*(i+8)+j+8)] = pixCr[j];
        }
        pixCb += w;
        pixCr += w;
    }

}

/*****************************************************************************************************/
void MEforGen75::LoadReference(U8 *src, U8 *dest, U8 dest_w, U8 dest_h, I16PAIR mvs, U8 mem_width, bool isBottom)
/*****************************************************************************************************/
{
    int        i,j;
    int        pitch = Vsta.RefPitch;
    int        x0,x1,x2,x3,y0,y1,y2,y3;
    U8      *tmp_dest;
    I16PAIR rxy;
    U8 chromamode = (Vsta.SadType&INTER_CHROMA_MODE) ? 1 : 0;
    rxy.x = mvs.x + (Vsta.SrcMB.x>>chromamode);
    rxy.y = mvs.y + (Vsta.SrcMB.y>>chromamode);
    
    if(isBottom)
        src += pitch;

    // x0 = number for left padding, x1 = left start, x2 = normal x, x3 = right padding
    x1 = rxy.x + (x0 = (rxy.x<0) ? -rxy.x : 0);
    if((x2 = rxy.x + dest_w)>(Vsta.RefWidth >> chromamode)) x3 = x2 - (Vsta.RefWidth >> chromamode);
    else x3 = 0;
    if(x3>=dest_w){ x3 = dest_w-1; x1 = (Vsta.RefWidth >> chromamode) - 1; x0 = 0; }        // way off right
    else if(x0>=dest_w){ x0 = dest_w-1; x1 = x3 = 0; }                    // way off left
     x2 = dest_w - (x0+x3);

    // y0 = number for top padding, y1 = top start, y2 = normal y, y3 = bottom padding
    if(!(Vsta.SrcType&ST_REF_FIELD)){
        y1 = rxy.y + (y0 = (rxy.y<0) ? -rxy.y : 0);  
        if((y2 = rxy.y + dest_h)>(Vsta.RefHeight>>chromamode)) y3 = y2 - (Vsta.RefHeight>>chromamode);
        else y3 = 0;
        if(y3>=dest_h){ y3 = dest_h-1; y1 = (Vsta.RefHeight>>chromamode) - 1; y0 = 0; }        // way off bottom
        else if(y0>=dest_h){ y0 = dest_h-1; y1 = y3 = 0; }                    // way off top 
    }
    else{ // FIELD CASE
        pitch <<= 1;
        y1 = rxy.y + (y0 = (rxy.y<0) ? -rxy.y : 0);  
        if((y2 = rxy.y + dest_h)>((Vsta.RefHeight>>chromamode)>>1)) y3 = y2 - ((Vsta.RefHeight>>chromamode)>>1);
        else y3 = 0;
        if(y3>=dest_h){ y3 = dest_h-1; y1 = ((Vsta.RefHeight>>chromamode)>>1) - 1; y0 = 0; }        // way off bottom
        else if(y0>=dest_h){ y0 = dest_h-1; y1 = y3 = 0; }                    // way off top 
    }
    y2 = dest_h - (y0+y3);

    tmp_dest = dest + (y0*mem_width);
    src += y1*pitch + x1;
    for(j=0;j<y2;j++){
        if(x0) memset(tmp_dest,src[0],x0);
        for(i=0;i<x2;i++) tmp_dest[x0+i] = src[i];
        if(x3) memset(tmp_dest+x0+x2,tmp_dest[x0+x2-1],x3);
        tmp_dest += mem_width; src += pitch;
    }
    for(j=0;j<y3;j++){ MFX_INTERNAL_CPY(tmp_dest,tmp_dest-mem_width,mem_width); tmp_dest += mem_width; }
    tmp_dest = dest + (y0*mem_width);
    for(j=0;j<y0;j++){ 
        MFX_INTERNAL_CPY(tmp_dest-mem_width,tmp_dest,mem_width); 
        tmp_dest -= mem_width; 
    }
}

#if (__HAAR_SAD_OPT == 1)
/*****************************************************************************************************/
void MEforGen75::LoadReference2(U8 *src, U8 *dest, U8 dest_w, U8 dest_h, I16PAIR mvs, U8 mem_width, bool isBottom, 
                                bool &copied, U8* &src_shifted, int &src_pitch)
/*****************************************************************************************************/
{
    int        i,j;
    int        pitch = Vsta.RefPitch;
    int        x0,x1,x2,x3,y0,y1,y2,y3;
    U8      *tmp_dest;
    I16PAIR rxy;
    U8 chromamode = (Vsta.SadType&INTER_CHROMA_MODE) ? 1 : 0;
    rxy.x = mvs.x + (Vsta.SrcMB.x>>chromamode);
    rxy.y = mvs.y + (Vsta.SrcMB.y>>chromamode);
    
    if(isBottom)
        src += pitch;

    // x0 = number for left padding, x1 = left start, x2 = normal x, x3 = right padding
    x1 = rxy.x + (x0 = (rxy.x<0) ? -rxy.x : 0);
    if((x2 = rxy.x + dest_w)>(Vsta.RefWidth >> chromamode)) x3 = x2 - (Vsta.RefWidth >> chromamode);
    else x3 = 0;
    if(x3>=dest_w){ x3 = dest_w-1; x1 = (Vsta.RefWidth >> chromamode) - 1; x0 = 0; }        // way off right
    else if(x0>=dest_w){ x0 = dest_w-1; x1 = x3 = 0; }                    // way off left
     x2 = dest_w - (x0+x3);

    // y0 = number for top padding, y1 = top start, y2 = normal y, y3 = bottom padding
    if(!(Vsta.SrcType&ST_REF_FIELD)){
        y1 = rxy.y + (y0 = (rxy.y<0) ? -rxy.y : 0);  
        if((y2 = rxy.y + dest_h)>(Vsta.RefHeight>>chromamode)) y3 = y2 - (Vsta.RefHeight>>chromamode);
        else y3 = 0;
        if(y3>=dest_h){ y3 = dest_h-1; y1 = (Vsta.RefHeight>>chromamode) - 1; y0 = 0; }        // way off bottom
        else if(y0>=dest_h){ y0 = dest_h-1; y1 = y3 = 0; }                    // way off top 
    }
    else{ // FIELD CASE
        pitch <<= 1;
        y1 = rxy.y + (y0 = (rxy.y<0) ? -rxy.y : 0);  
        if((y2 = rxy.y + dest_h)>((Vsta.RefHeight>>chromamode)>>1)) y3 = y2 - ((Vsta.RefHeight>>chromamode)>>1);
        else y3 = 0;
        if(y3>=dest_h){ y3 = dest_h-1; y1 = ((Vsta.RefHeight>>chromamode)>>1) - 1; y0 = 0; }        // way off bottom
        else if(y0>=dest_h){ y0 = dest_h-1; y1 = y3 = 0; }                    // way off top 
    }
    y2 = dest_h - (y0+y3);

    tmp_dest = dest + (y0*mem_width);
    src += y1*pitch + x1;
    
    src_shifted = src;
    src_pitch = pitch;
    if (x0 == 0 && x3 == 0 && y0 == 0 && y3 == 0) {
        copied = false;
        return;
    }
    copied = true;
    for(j=0;j<y2;j++){
        if(x0) memset(tmp_dest,src[0],x0);
        for(i=0;i<x2;i++) tmp_dest[x0+i] = src[i];
        if(x3) memset(tmp_dest+x0+x2,tmp_dest[x0+x2-1],x3);
        tmp_dest += mem_width; src += pitch;
    }
    for(j=0;j<y3;j++){ MFX_INTERNAL_CPY(tmp_dest,tmp_dest-mem_width,mem_width); tmp_dest += mem_width; }
    tmp_dest = dest + (y0*mem_width);
    for(j=0;j<y0;j++){ 
        MFX_INTERNAL_CPY(tmp_dest-mem_width,tmp_dest,mem_width); 
        tmp_dest -= mem_width; 
    }
}
#endif

/*****************************************************************************************************/
int MEforGen75::GetCostXY(int x, int y, int isb)
/*****************************************************************************************************/
{
    int chroma_mode = ((Vsta.SadType&INTER_CHROMA_MODE)!=0);
    int shift = Vsta.MvFlags&0x3;
    x -= (Vsta.CostCenter[isb].x>>chroma_mode);
    y -= (Vsta.CostCenter[isb].y>>chroma_mode);
    x = ((x<0) ? -x : x); 
    y = ((y<0) ? -y : y); 

    if((x>>shift) >= 64){
        if(LutXY[64]){
            x = LutXY[64]+(((x>>shift)-64)>>2);
        }
        else {
            x = 0; 
        }
    }
    else {
        if( (x>>shift) >= 0){
            x = LutXY[x>>shift];
        }
        else {
            //int p = x>>shift ; 
            //printf("MEforGen75_IME::GetCostXY >> Error :: Incorrect(negative) index to LutXY[].\n", p);
            return (-1);
        }
    }

    if( (y>>shift) >= 64){
        if(LutXY[64]){
            y = LutXY[64]+(((y>>shift)-64)>>2);
        }
        else {
            y = 0; 
        }
    }
    else {
        if( (y>>shift) >= 0){
            y = LutXY[y>>shift];
        }
        else {
            //int p = y>>shift ; 
            //printf("MEforGen75_IME::GetCostXY >> Error :: Incorrect(negative) index to LutXY[].\n", p);
            return (-1);
        }
    }
    //x = (x>>shift>=64) ? (LutXY[64] ? LutXY[64]+(((x>>shift)-64)>>2) : 0) : LutXY[x>>shift]; 
    //y = (y>>shift>=64) ? (LutXY[64] ? LutXY[64]+(((y>>shift)-64)>>2) : 0) : LutXY[y>>shift]; 
    if((x+=y)>MAXLUTMODE0) x = MAXLUTMODE0;
    return (x);
}

/*****************************************************************************************************/
int MEforGen75::GetCostRefID(int refid)
/*****************************************************************************************************/
{
    int toRet;

    if(Vsta.MvFlags & REFID_COST_MODE){ // Linear
        toRet = LutMode[LUTMODE_REF_ID] * refid;
    }
    else{ // AVC CAVLC Penalties
        int                                            scale = 0;
        if(refid > 0 && refid < 3)                    scale = 1;
        else if(refid >= 3 && refid < 7)            scale = 2;
        else if    (refid >= 7)                        scale = 3;
        toRet = LutMode[LUTMODE_REF_ID] * scale;
    }
    return ((toRet > MAXLUTMODE1) ? MAXLUTMODE1 : toRet);
}

/*****************************************************************************************************/
void MEforGen75::GetDynamicSU(int bDualRecord)
/*****************************************************************************************************/
{
    startedAdaptive = true;
    GetPipelinedDynamicSetting(bDualRecord);
}

/*****************************************************************************************************/
void MEforGen75::GetPipelinedDynamicSetting(int bDualRecord)
/*****************************************************************************************************/
{
    PipelineCalls++; 
    if(PipelinedSUFlg)
    {
        //Select in-flight SU from memory
        PickSavedSU(bDualRecord);
    }
    else
    {
        //Check for 2 SUs to be dispatched back-to-back
        Select2SUs(bDualRecord, false);
        if(!PipelinedSUFlg && NextRef != 2)PipelineCalls++;
    }
}

/*****************************************************************************************************/
void MEforGen75::Select2SUs(int bDualRecord, bool in_bCheckInFlightSU)
/*****************************************************************************************************/
{
    U8 refNum = 0;
    U8 numReferences = bDualRecord ? 2 : 1;
    //U8 RefWidth = Vsta.RefW;
    //U8 RefHeight = Vsta.RefH;
    //int max_x = (RefWidth>>2) - 5; // Horizontal SU maximum boundary
    //int max_y = (RefHeight>>2) - 5; // Vertical SU maximum boundary
    //U8 offset = 0;
    U8 numValidDynMVs = 0;
    bool NbSPWalkState[4];
    I16PAIR ValidMV[2][4];
    U8 ShapeX = 0;
    U8 ShapeY = 0;
    //U8 i = 0;
    //U8 j = 0;
    U8 k = 0;
    U8 dvh[2][4]; // b2: Diagonal, b1: Vertical, b0: Horizontal
    bool bCheckInFlightSU = in_bCheckInFlightSU;
    bool bSetDynOff = !in_bCheckInFlightSU;
    //bool bStop = false;
    bool bAtLeastOneSU = false;
    //U8 FirstSUDxn = 0; //HORIZONTAL, VERTICAL OR DIAGONAL
    U8 numSUsChecked = 0;
    U8 numValidSUs = 0;
    int x = 0;
    int y = 0;
    I16 SuCount[2] = {static_cast<I16>(FixedCntSu[0] + AdaptiveCntSu[0]), static_cast<I16>(FixedCntSu[1] + AdaptiveCntSu[1])};

    if(bCheckInFlightSU)
    {
        SuCount[SavedRefId]++;
    }

    if(!CheckDynamicEnable(SuCount[0],SuCount[1]))
    {
        Dynamic[0] = false;
        Dynamic[1] = false;
        if(bSetDynOff) NextRef = 2;
        return;
    }
    
    numValidDynMVs = GetValid8x8MVs(&ValidMV[0][0], (bDualRecord==1));

    if(bCheckInFlightSU)
    {
        ShapeX = SavedSU.x;
        ShapeY = SavedSU.y;
        switch(Vsta.SrcType&3){//non 16x16 src type, more 4x4 SUs are searched 
        case 0:    Record[SavedRefId][ShapeY] |= (1<<ShapeX); break;
        case 1:    Record[SavedRefId][ShapeY] |= (1<<ShapeX); Record[SavedRefId][ShapeY+1] |= (1<<ShapeX); break;
        case 2:    Record[SavedRefId][ShapeY] |= (3<<ShapeX); break;
        case 3:    Record[SavedRefId][ShapeY] |= (3<<ShapeX); Record[SavedRefId][ShapeY+1] |= (3<<ShapeX); break;
        }
    }

    memset(&dvh[0][0],0,sizeof(dvh[0][0])*8);

    for(refNum = 0; refNum < numReferences; refNum++)
    {
        for(k = 0; k < numValidDynMVs; k++)
        {
            if((ValidMV[refNum][k].x&12)==0 || (ValidMV[refNum][k].x&12)==12)
            {
                dvh[refNum][k] |= 1;
            }
            if((ValidMV[refNum][k].y&12)==0 || (ValidMV[refNum][k].y&12)==12)
            {
                dvh[refNum][k] |= 2;
            }
            if(dvh[refNum][k] == 3)                                                 
            {
                dvh[refNum][k] |= 4;
            }
        }
    }

    numValidSUs = 3*numReferences*numValidDynMVs; //These are the total number of SUs that can be checked

    while(numSUsChecked < numValidSUs)
    {
        //HORIZONTAL
        for(refNum = 0; refNum < numReferences; refNum++)
        {
            if(!Dynamic[refNum])
            {
                continue;
            }
            //offset = 4*refNum;
            k = 0;
            while(k < numValidDynMVs)
            {
                numSUsChecked++;
                if(dvh[refNum][k]&1)
                {
                    x = ValidMV[refNum][k].x>>4;
                    y = ValidMV[refNum][k].y>>4;
                    NbSPWalkState[0] = ((Record[refNum][y]&(1<<(x-1)))==0) ? false : true; //Left
                    NbSPWalkState[1] = ((Record[refNum][y]&(1<<(x+1)))==0) ? false : true; //Right

                    CheckHorizontalAdaptiveSU(ValidMV[refNum][k],
                                         bCheckInFlightSU, 
                                         &NbSPWalkState[0],
                                         refNum,
                                         bAtLeastOneSU);

                    if((!bCheckInFlightSU)&&(bAtLeastOneSU))
                    {
                        bCheckInFlightSU = true;
                        ShapeX = NextSU.x;
                        ShapeY = NextSU.y;
                        switch(Vsta.SrcType&3){
                        case 0:    Record[refNum][ShapeY] |= (1<<ShapeX); break;
                        case 1:    Record[refNum][ShapeY] |= (1<<ShapeX); Record[refNum][ShapeY+1] |= (1<<ShapeX); break;
                        case 2:    Record[refNum][ShapeY] |= (3<<ShapeX); break;
                        case 3:    Record[refNum][ShapeY] |= (3<<ShapeX); Record[refNum][ShapeY+1] |= (3<<ShapeX); break;
                        }
                        SuCount[refNum]++;
                        if(!CheckDynamicEnable(SuCount[0],SuCount[1]))
                        {
                            return;
                        }
                        k = numValidDynMVs;
                        numSUsChecked = 0;
                    }
                    else if(PipelinedSUFlg)
                    {
                        return;
                    }
                    else
                    {
                        k++;
                    }
                }
                else
                {
                    k++;
                }
            }
        }

        //VERTICAL
        for(refNum = 0; refNum < numReferences; refNum++)
        {
            if(!Dynamic[refNum])
            {
                continue;
            }
            //offset = 4*refNum;
            k = 0;
            while(k < numValidDynMVs)
            {
                numSUsChecked++;
                if(dvh[refNum][k]&2)
                {
                    x = ValidMV[refNum][k].x>>4;
                    y = ValidMV[refNum][k].y>>4;
                    NbSPWalkState[0] = ((Record[refNum][y-1]&(1<<x))==0) ? false : true; //Top
                    NbSPWalkState[1] = ((Record[refNum][y+1]&(1<<x))==0) ? false : true; //Bottom

                    CheckVerticalAdaptiveSU(ValidMV[refNum][k],
                                       bCheckInFlightSU, 
                                       &NbSPWalkState[0],
                                       refNum,
                                       bAtLeastOneSU);

                    if((!bCheckInFlightSU)&&(bAtLeastOneSU))
                    {
                        bCheckInFlightSU = true;
                        ShapeX = NextSU.x;
                        ShapeY = NextSU.y;
                        switch(Vsta.SrcType&3){
                        case 0:    Record[refNum][ShapeY] |= (1<<ShapeX); break;
                        case 1:    Record[refNum][ShapeY] |= (1<<ShapeX); Record[refNum][ShapeY+1] |= (1<<ShapeX); break;
                        case 2:    Record[refNum][ShapeY] |= (3<<ShapeX); break;
                        case 3:    Record[refNum][ShapeY] |= (3<<ShapeX); Record[refNum][ShapeY+1] |= (3<<ShapeX); break;
                        }
                        SuCount[refNum]++;
                        if(!CheckDynamicEnable(SuCount[0],SuCount[1]))
                        {
                            return;
                        }
                        k = numValidDynMVs;
                        numSUsChecked = 0;
                    }
                    else if(PipelinedSUFlg)
                    {
                        return;
                    }
                    else
                    {
                        k++;
                    }
                }
                else
                {
                    k++;
                }
            }
        }

        //DIAGONAL
        for(refNum = 0; refNum < numReferences; refNum++)
        {
            if(!Dynamic[refNum])
            {
                continue;
            }
            //offset = 4*refNum;
            k = 0;
            while(k < numValidDynMVs)
            {
                numSUsChecked++;
                if(dvh[refNum][k]&4)
                {
                    x = ValidMV[refNum][k].x>>4;
                    y = ValidMV[refNum][k].y>>4;
                    NbSPWalkState[0] = ((Record[refNum][y-1]&(1<<(x-1)))==0) ? false : true; //TopLeft
                    NbSPWalkState[1] = ((Record[refNum][y-1]&(1<<(x+1)))==0) ? false : true; //TopRight
                    NbSPWalkState[2] = ((Record[refNum][y+1]&(1<<(x-1)))==0) ? false : true; //BotLeft
                    NbSPWalkState[3] = ((Record[refNum][y+1]&(1<<(x+1)))==0) ? false : true; //BotRight

                    CheckDiagonalAdaptiveSU(ValidMV[refNum][k],
                                       bCheckInFlightSU, 
                                       &NbSPWalkState[0],
                                       refNum,
                                       bAtLeastOneSU);

                    if((!bCheckInFlightSU)&&(bAtLeastOneSU))
                    {
                        bCheckInFlightSU = true;
                        ShapeX = NextSU.x;
                        ShapeY = NextSU.y;
                        switch(Vsta.SrcType&3){
                        case 0:    Record[refNum][ShapeY] |= (1<<ShapeX); break;
                        case 1:    Record[refNum][ShapeY] |= (1<<ShapeX); Record[refNum][ShapeY+1] |= (1<<ShapeX); break;
                        case 2:    Record[refNum][ShapeY] |= (3<<ShapeX); break;
                        case 3:    Record[refNum][ShapeY] |= (3<<ShapeX); Record[refNum][ShapeY+1] |= (3<<ShapeX); break;
                        }
                        SuCount[refNum]++;
                        if(!CheckDynamicEnable(SuCount[0],SuCount[1]))
                        {
                            return;
                        }
                        k = numValidDynMVs;
                        numSUsChecked = 0;
                    }
                    else if(PipelinedSUFlg)
                    {
                        return;
                    }
                    else
                    {
                        k++;
                    }
                }
                else
                {
                    k++;
                }
            }
        }
    }
    
    if((!bAtLeastOneSU)&&(bSetDynOff))
    {
        Dynamic[0] = false;
        Dynamic[1] = false;
        NextRef = 2;
    }
}

/*****************************************************************************************************/
void MEforGen75::PickSavedSU(int bDualRecord)
/*****************************************************************************************************/
{
    NextSU.x = SavedSU.x;
    NextSU.y = SavedSU.y;
    NextRef = SavedRefId;

    PipelinedSUFlg = false;

    //Save MV[n] to MV[n-1]
    U8 numValidDynMVs = 0;
    U8 i = 0;

    switch(Vsta.SrcType&3)
    {
    case 0:
        numValidDynMVs = 4;
        for(i = 0; i < numValidDynMVs; i++)
        {
            PrevMV[0][i].x = Best[0][B8X8+i].x - (Vsta.Ref[0].x<<2);
            PrevMV[0][i].y = Best[0][B8X8+i].y - (Vsta.Ref[0].y<<2);
            if(bDualRecord)
            {
                PrevMV[1][i].x = Best[1][B8X8+i].x - (Vsta.Ref[1].x<<2);
                PrevMV[1][i].y = Best[1][B8X8+i].y - (Vsta.Ref[1].y<<2);
            }
        }
        break;
    case 1:
        numValidDynMVs = 2;
        for(i = 0; i < numValidDynMVs; i++)
        {
            PrevMV[0][i].x = BestForReducedMB[0][B8X8+i].x - (Vsta.Ref[0].x<<2);
            PrevMV[0][i].y = BestForReducedMB[0][B8X8+i].y - (Vsta.Ref[0].y<<2);
            if(bDualRecord)
            {
                PrevMV[1][i].x = BestForReducedMB[1][B8X8+i].x - (Vsta.Ref[1].x<<2);
                PrevMV[1][i].y = BestForReducedMB[1][B8X8+i].y - (Vsta.Ref[1].y<<2);
            }
        }
        break;
    case 3:
        numValidDynMVs = 1;
        for(i = 0; i < numValidDynMVs; i++)
        {
            PrevMV[0][i].x = BestForReducedMB[0][B8X8+i].x - (Vsta.Ref[0].x<<2);
            PrevMV[0][i].y = BestForReducedMB[0][B8X8+i].y - (Vsta.Ref[0].y<<2);
            if(bDualRecord)
            {
                PrevMV[1][i].x = BestForReducedMB[1][B8X8+i].x - (Vsta.Ref[1].x<<2);
                PrevMV[1][i].y = BestForReducedMB[1][B8X8+i].y - (Vsta.Ref[1].y<<2);
            }
        }
        break;
    default:
        break;
    }

    UsePreviousMVFlg = true;
    Select2SUs(bDualRecord, true);
}

/*****************************************************************************************************/
void MEforGen75::CheckHorizontalAdaptiveSU(I16PAIR ValidMV, 
                                   bool bCheckInFlightSU, 
                                   bool *NbSPWalkState,
                                   U8 refNum,
                                   bool &out_bAtLeastOneSU)
/*****************************************************************************************************/
{
    U8 RefWidth = Vsta.RefW;
    U8 RefHeight = Vsta.RefH;

    int max_x = (RefWidth>>2) - 5; // Horizontal SU maximum boundary
    int max_y = (RefHeight>>2) - 5; // Vertical SU maximum boundary

    int x = ValidMV.x>>4;
    int y = ValidMV.y>>4;

    if(((ValidMV.x&12)==0)  && (!NbSPWalkState[0]) && (x>0) && (y<=max_y))
    { 
        if(!bCheckInFlightSU)
        {
            NextSU.x = x-1; // Go left
            NextSU.y = y; 
            NextRef = refNum; 
            out_bAtLeastOneSU = true;
        }
        else
        {
            SavedSU.x = x-1; // Go left
            SavedSU.y = y;
            SavedRefId = refNum;
            PipelinedSUFlg = true;
        }
    } 
    else if(((ValidMV.x&12)==12) && (!NbSPWalkState[1]) && (x<max_x) && (y<=max_y))
    { 
        if(!bCheckInFlightSU)
        {
            NextSU.x = x+1; // Go right
            NextSU.y = y; 
            NextRef = refNum; 
            out_bAtLeastOneSU = true;
        }
        else
        {
            SavedSU.x = x+1; // Go right
            SavedSU.y = y;
            SavedRefId = refNum;
            PipelinedSUFlg = true;
        }
    } 
}

/*****************************************************************************************************/
void MEforGen75::CheckVerticalAdaptiveSU(I16PAIR ValidMV, 
                                 bool bCheckInFlightSU, 
                                 bool *NbSPWalkState,
                                 U8 refNum,
                                 bool &out_bAtLeastOneSU)
/*****************************************************************************************************/
{
    U8 RefWidth = Vsta.RefW;
    U8 RefHeight = Vsta.RefH; 

    int max_x = (RefWidth>>2) - 5; // Horizontal SU maximum boundary
    int max_y = (RefHeight>>2) - 5; // Vertical SU maximum boundary

    int x = ValidMV.x>>4;
    int y = ValidMV.y>>4;

    if(((ValidMV.y&12)==0)  && (!NbSPWalkState[0]) && (y>0) && (x<=max_x))
    { 
        if(!bCheckInFlightSU)
        {
            NextSU.x = x; 
            NextSU.y = y - 1; // Go top
            NextRef = refNum; 
            out_bAtLeastOneSU = true;
        }
        else
        {
            SavedSU.x = x;
            SavedSU.y = y - 1; // Go top
            SavedRefId = refNum;
            PipelinedSUFlg = true;
        }
    } 
    else if(((ValidMV.y&12)==12) && (!NbSPWalkState[1]) && (y<max_y) && (x<=max_x))
    { 
        if(!bCheckInFlightSU)
        {
            NextSU.x = x;
            NextSU.y = y + 1; // Go bottom
            NextRef = refNum; 
            out_bAtLeastOneSU = true;
        }
        else
        {
            SavedSU.x = x;
            SavedSU.y = y + 1; // Go bottom
            SavedRefId = refNum;
            PipelinedSUFlg = true;
        }
    } 
}


/*****************************************************************************************************/
void MEforGen75::CheckDiagonalAdaptiveSU(I16PAIR ValidMV, 
                                 bool bCheckInFlightSU, 
                                 bool *NbSPWalkState,
                                 U8 refNum,
                                 bool &out_bAtLeastOneSU)
/*****************************************************************************************************/
{
    U8 RefWidth = Vsta.RefW;
    U8 RefHeight = Vsta.RefH; 
    
    int max_x = (RefWidth>>2) - 5; // Horizontal SU maximum boundary
    int max_y = (RefHeight>>2) - 5; // Vertical SU maximum boundary

    int x = ValidMV.x>>4;
    int y = ValidMV.y>>4;

    if(((ValidMV.x&12)==0)  && ((ValidMV.y&12)==0)  && (!NbSPWalkState[0]) && (x>0) && (y>0))
    { 
        if(!bCheckInFlightSU)
        {
            NextSU.x = x - 1; // Go topleft 
            NextSU.y = y - 1;
            NextRef = refNum; 
            out_bAtLeastOneSU = true;
        }
        else
        {
            SavedSU.x = x - 1; // Go topleft 
            SavedSU.y = y - 1;
            SavedRefId = refNum;
            PipelinedSUFlg = true;
        }
    } 
    else if(((ValidMV.x&12)==12) && ((ValidMV.y&12)==0)  && (!NbSPWalkState[1]) && (x<max_x) && (y>0)) 
    { 
        if(!bCheckInFlightSU)
        {
            NextSU.x = x + 1; //Go topright
            NextSU.y = y - 1; 
            NextRef = refNum; 
            out_bAtLeastOneSU = true;
        }
        else
        {
            SavedSU.x = x + 1; //Go topright
            SavedSU.y = y - 1; 
            SavedRefId = refNum;
            PipelinedSUFlg = true;
        }
    }
    else if(((ValidMV.x&12)==0)  && ((ValidMV.y&12)==12) && (!NbSPWalkState[2]) && (x>0) && (y<max_y))
    { 
        if(!bCheckInFlightSU)
        {
            NextSU.x = x - 1; // Go botleft
            NextSU.y = y + 1; 
            NextRef = refNum;
            out_bAtLeastOneSU = true;
        }
        else
        {
            SavedSU.x = x - 1; // Go botleft
            SavedSU.y = y + 1; 
            SavedRefId = refNum;
            PipelinedSUFlg = true;
        }
    }
    else if(((ValidMV.x&12)==12) && ((ValidMV.y&12)==12) && (!NbSPWalkState[3]) && (x<max_x) && (y<max_y))
    { 
        if(!bCheckInFlightSU)
        {
            NextSU.x = x + 1; // Go botright
            NextSU.y = y + 1; 
            NextRef = refNum;
            out_bAtLeastOneSU = true;    
        }
        else
        {
            SavedSU.x = x + 1; // Go botright
            SavedSU.y = y + 1; 
            SavedRefId = refNum;
            PipelinedSUFlg = true;
        }
    } 

}

/*****************************************************************************************************/
U8 MEforGen75::GetValid8x8MVs(I16PAIR *out_ValidMVs, bool bDualRecord)
/*****************************************************************************************************/
{
    I16PAIR* ValidMVs = out_ValidMVs;
    U8 i = 0;
    U8 j = 0;
    U8 numValidDynMVs = 0;
    I16PAIR ValidMV[2][4];

    if(UsePreviousMVFlg)
    {
        MFX_INTERNAL_CPY(&ValidMV[0][0], &PrevMV[0][0], 8*sizeof(I16PAIR));
        numValidDynMVs = ((Vsta.SrcType&3)==0) ? 4 : (((Vsta.SrcType&3)==1) ? 2 : 1);
        UsePreviousMVFlg = false;
    }
    else
    {
        switch(Vsta.SrcType&3)
        {
        case 0:
            numValidDynMVs = 4;
            for(i = 0; i < numValidDynMVs; i++)
            {
                ValidMV[0][i].x = Best[0][B8X8+i].x - (Vsta.Ref[0].x<<2);
                ValidMV[0][i].y = Best[0][B8X8+i].y - (Vsta.Ref[0].y<<2);
                if(bDualRecord)
                {
                    ValidMV[1][i].x = Best[1][B8X8+i].x - (Vsta.Ref[1].x<<2);
                    ValidMV[1][i].y = Best[1][B8X8+i].y - (Vsta.Ref[1].y<<2);
                }
            }
            break;
        case 1:
            numValidDynMVs = 2;
            for(i = 0; i < numValidDynMVs; i++)
            {
                ValidMV[0][i].x = BestForReducedMB[0][B8X8+i].x - (Vsta.Ref[0].x<<2);
                ValidMV[0][i].y = BestForReducedMB[0][B8X8+i].y - (Vsta.Ref[0].y<<2);
                if(bDualRecord)
                {
                    ValidMV[1][i].x = BestForReducedMB[1][B8X8+i].x - (Vsta.Ref[1].x<<2);
                    ValidMV[1][i].y = BestForReducedMB[1][B8X8+i].y - (Vsta.Ref[1].y<<2);
                }
            }
            break;
        case 3:
            numValidDynMVs = 1;
            for(i = 0; i < numValidDynMVs; i++)
            {
                ValidMV[0][i].x = BestForReducedMB[0][B8X8+i].x - (Vsta.Ref[0].x<<2);
                ValidMV[0][i].y = BestForReducedMB[0][B8X8+i].y - (Vsta.Ref[0].y<<2);
                if(bDualRecord)
                {
                    ValidMV[1][i].x = BestForReducedMB[1][B8X8+i].x - (Vsta.Ref[1].x<<2);
                    ValidMV[1][i].y = BestForReducedMB[1][B8X8+i].y - (Vsta.Ref[1].y<<2);
                }
            }
            break;
        default:
            break;
        }
    }

    for(i=0; i<2; i++)
    {
        for(j=0; j<4; j++)
        {
            ValidMVs[i*4+j] = ValidMV[i][j];
        }
    }    

    return numValidDynMVs;
}

/*****************************************************************************************************/
bool MEforGen75::CheckDynamicEnable(I16 in_SuCntRef0, I16 in_SuCntRef1)
/*****************************************************************************************************/
{
    if((Vsta.VmeModes&7) == 3)
    {//DDS
        if((in_SuCntRef0 + in_SuCntRef1) >= Vsta.MaxNumSU)
        {
            Dynamic[0] = false;
            Dynamic[1] = false;
            return false;
        }
    }
    else
    {//SSS, DSS, DDD
        if(in_SuCntRef0 >= Vsta.MaxNumSU)
        {
            Dynamic[0] = false;
        }
        
        if(in_SuCntRef1 >= Vsta.MaxNumSU)
        {
            Dynamic[1] = false;
        }

        if((!Dynamic[0]) && (!Dynamic[1]))
        {
            return false;
        }
    }

    return true; //fall through case
}

bool MEforGen75::CheckTooGoodOrBad()
{
//    if((Vsta.VmeFlags&VF_EARLY_IME_GOOD)==VF_EARLY_IME_GOOD && (DistInter<ImeTooGood))
//    {
//#ifdef VMETESTGEN
//    Vout->Test.QuitCriteria = Ime_Too_Good;
//#endif
//        Vout->VmeDecisionLog |= OCCURRED_TOO_GOOD;
//        if((Vsta.VmeFlags&VF_EARLY_IME_BAD)==VF_EARLY_IME_BAD && (DistInter>ImeTooBad))
//        {
//            Vout->VmeDecisionLog |= OCCURRED_TOO_BAD;
//        }
//        return true;
//    }
//
//    if((Vsta.VmeFlags&VF_EARLY_IME_BAD)==VF_EARLY_IME_BAD && (DistInter>ImeTooBad))
//    {
//#ifdef VMETESTGEN
//    Vout->Test.QuitCriteria = Ime_Too_Bad;
//#endif
//        Vout->VmeDecisionLog |= OCCURRED_TOO_BAD;
//        return true;
//    }
    return false;
}

void MEforGen75::UpdateBest()
{
    int i,j;
    int numRec = ((Vsta.VmeModes&7) > 1) ? 2 : 1;
    bool isRecValid = true;
    for(i = 0; i < 2; i++)
    {
        if((i==1)&&(numRec==1)) isRecValid = false;
        if(!isRecValid) continue;
        if(!(Vsta.SadType&INTER_CHROMA_MODE)){
            if(VIMEsta.RecordDst16[i] < Dist[i][BHXH])
            {
                Dist[i][BHXH] = VIMEsta.RecordDst16[i];
                Best[i][BHXH].x = VIMEsta.RecordMvs16[i].x;
                Best[i][BHXH].y = VIMEsta.RecordMvs16[i].y;
                BestRef[i][BHXH] = ((U8)VIMEsta.RecRefID16[i]);
                MultiReplaced[i][BHXH] = 1;
            }
            for(j = 0; j < 8; j++){
                if(VIMEsta.RecordDst[i][j] < Dist[i][BHX8+j])
                {
                    Dist[i][BHX8+j] = VIMEsta.RecordDst[i][j];
                    Best[i][BHX8+j].x = VIMEsta.RecordMvs[i][j].x;
                    Best[i][BHX8+j].y = VIMEsta.RecordMvs[i][j].y;
                    BestRef[i][BHX8+j] = (VIMEsta.RecRefID[i][j/2]>>(4*(j%2))&0xF);
                    MultiReplaced[i][BHX8+j] = 1;
                }
            }
        }
        else{
            if(VIMEsta.RecordDst16[i] < Dist[i][B8X8])
            {
                Dist[i][B8X8] = VIMEsta.RecordDst16[i];
                Best[i][B8X8].x = VIMEsta.RecordMvs16[i].x;
                Best[i][B8X8].y = VIMEsta.RecordMvs16[i].y;
                BestRef[i][BHXH] = ((U8)VIMEsta.RecRefID16[i]);
                MultiReplaced[i][BHXH] = 1;
            }
            for(j = 0; j < 4; j++){
                if((VIMEsta.RecordDst[i][j+4]&0x3fff) < Dist[i][B4X4+j])
                {
                    Dist[i][B4X4+j] = (VIMEsta.RecordDst[i][j+4]&0x3fff);
                    Best[i][B4X4+j].x = VIMEsta.RecordMvs[i][j+4].x;
                    Best[i][B4X4+j].y = VIMEsta.RecordMvs[i][j+4].y;
                    BestRef[i][B8X8+j] = (VIMEsta.RecRefID[i][(j+4)/2]>>(4*(j%2))&0xF);
                    MultiReplaced[i][B8X8+j] = 1;
                }
            }
        }
    }
}

void MEforGen75::UpdateRecordOut()
{
    int i,j;
    int numRef = ((Vsta.VmeModes&7) > 1) ? 2 : 1;
    for(i = 0; i < numRef; i++)
    {
        if(!(Vsta.SadType&INTER_CHROMA_MODE)){
            if(Dist[i][BHXH] <= VIMEsta.RecordDst16[i])
            {
                VIMEout->RecordDst16[i] = Dist[i][BHXH];
                VIMEout->RecordMvs16[i].x = Best[i][BHXH].x;
                VIMEout->RecordMvs16[i].y = Best[i][BHXH].y;
                VIMEout->RecordRefId16[i] = BestRef[i][BHXH];
                if(Dist[i][BHXH] < VIMEsta.RecordDst16[i])
                {
#ifdef VMETESTGEN
                    Vout->Test.NStrInOutReplaced[i].SHP_16x16 |= 0x1;
#endif 
                }
            }
            for(j=0;j<8;j++){
                if(Dist[i][BHX8+j] <= VIMEsta.RecordDst[i][j])
                {
                    VIMEout->RecordDst[i][j] = Dist[i][BHX8+j];
                    VIMEout->RecordMvs[i][j].x = Best[i][BHX8+j].x;
                    VIMEout->RecordMvs[i][j].y = Best[i][BHX8+j].y;
                    //zero out initial value before or. lower 4 bits: &= 0xf0, higher bits: &= 0xf
                    VIMEout->RecordRefId[i][j/2] &= (0xf)<<(4*((j+1)%2)); 
                    VIMEout->RecordRefId[i][j/2] |= ((BestRef[i][BHX8+j])<<(4*(j%2)));
#ifdef VMETESTGEN        
                    if(Dist[i][BHX8+j] < VIMEsta.RecordDst[i][j])
                    {

                        switch(j){
                        case 0: Vout->Test.NStrInOutReplaced[i].SHP_16x8_0 |= 0x1; break;
                        case 1: Vout->Test.NStrInOutReplaced[i].SHP_16x8_1 |= 0x1; break;
                        case 2: Vout->Test.NStrInOutReplaced[i].SHP_8x16_0 |= 0x1; break;
                        case 3: Vout->Test.NStrInOutReplaced[i].SHP_8x16_1 |= 0x1; break;
                        case 4: Vout->Test.NStrInOutReplaced[i].SHP_8x8_0 |= 0x1; break;
                        case 5: Vout->Test.NStrInOutReplaced[i].SHP_8x8_1 |= 0x1; break;
                        case 6: Vout->Test.NStrInOutReplaced[i].SHP_8x8_2 |= 0x1; break;
                        case 7: Vout->Test.NStrInOutReplaced[i].SHP_8x8_3 |= 0x1; break;
                        }
                    }
#endif         
                }
            }
        }
        else{ // Chroma inter
            if(Dist[i][B8X8] <= VIMEsta.RecordDst16[i])
            {
                VIMEout->RecordDst16[i] = Dist[i][B8X8];
                if (MultiReplaced[i][BHXH])
                {//stream in Mv wins, pass through, able to take any precision streamIn Mv
                    VIMEout->RecordMvs16[i].x = VIMEsta.RecordMvs16[i].x;
                    VIMEout->RecordMvs16[i].y = VIMEsta.RecordMvs16[i].y;
                }
                else
                {//local Mv wins,map chroma Mv to luma surface,precision of 8
                    VIMEout->RecordMvs16[i].x = Best[i][B8X8].x<<1; 
                    VIMEout->RecordMvs16[i].y = Best[i][B8X8].y<<1;
                }
                VIMEout->RecordRefId16[i] = BestRef[i][BHXH];
                if(Dist[i][B8X8] < VIMEsta.RecordDst16[i])
                {
#ifdef VMETESTGEN
                    Vout->Test.NStrInOutReplaced[i].SHP_16x16 |= 0x1;
#endif 
                }
            }
            for(j=0;j<4;j++){
                if(Dist[i][B4X4+j] <= VIMEsta.RecordDst[i][j+4])
                {
                    VIMEout->RecordDst[i][j+4] = Dist[i][B4X4+j];
                    if (MultiReplaced[i][B8X8+j])
                    {//stream in Mv wins, pass through, able to take any precision streamIn Mv
                        VIMEout->RecordMvs[i][j+4].x = VIMEsta.RecordMvs[i][j+4].x;
                        VIMEout->RecordMvs[i][j+4].y = VIMEsta.RecordMvs[i][j+4].y;
                    }
                    else
                    {//local Mv wins,map chroma Mv to luma surface
                        VIMEout->RecordMvs[i][j+4].x = Best[i][B4X4+j].x<<1;
                        VIMEout->RecordMvs[i][j+4].y = Best[i][B4X4+j].y<<1;
                    }
                    VIMEout->RecordRefId[i][(j+4)/2] &= (0xf)<<(4*((j+1)%2)); //zero out initial value
                    VIMEout->RecordRefId[i][(j+4)/2] |= ((BestRef[i][B8X8+j])<<(4*(j%2)));
#ifdef VMETESTGEN        
                    if(Dist[i][B4X4+j] < VIMEsta.RecordDst[i][j+4])
                    {

                        switch(j+4){
                        case 4: Vout->Test.NStrInOutReplaced[i].SHP_8x8_0 |= 0x1; break;
                        case 5: Vout->Test.NStrInOutReplaced[i].SHP_8x8_1 |= 0x1; break;
                        case 6: Vout->Test.NStrInOutReplaced[i].SHP_8x8_2 |= 0x1; break;
                        case 7: Vout->Test.NStrInOutReplaced[i].SHP_8x8_3 |= 0x1; break;
                        }
                    }
#endif         
                }
            }
        }
    }
}


void MEforGen75::CleanRecordOut()
{
    int i, j;
    int numRef = ((Vsta.VmeModes&7) > 1) ? 2 : 1;
    for(i = 0; i < numRef; i++)
    {
        if (!(Vsta.SadType&INTER_CHROMA_MODE))
        {
            switch(Vsta.SrcType&3){
            case 0: return;                // 16x16 source MB
            case 2: exit(1); // return ERR_UNSUPPORTED;    // 8x16 source MB [Note: should return error/warn status for all functions instead of "void" as coding standard for the future]
            case 3: // 8x8 source MB: only index 4 used for 8x8
                VIMEout->RecordDst[i][0] = 0;
                VIMEout->RecordMvs[i][0].x = 0;
                VIMEout->RecordMvs[i][0].y = 0;
                VIMEout->RecordDst[i][5] = 0;
                VIMEout->RecordMvs[i][5].x = 0;
                VIMEout->RecordMvs[i][5].y = 0;
                VIMEout->RecordRefId[i][2] &= 0xf; //zero out 8x8_1
                VIMEout->RecordRefId[i][0] = 0; //zero out 16x8
            case 1: // 16x8 source MB: only index 0 used for 16x8, and indices 4 & 5 used for two 8x8
                VIMEout->RecordDst16[i] = 0;
                VIMEout->RecordMvs16[i].x = 0;
                VIMEout->RecordMvs16[i].y = 0;
                VIMEout->RecordRefId16[i] = 0;
                for(j = 1; j < 4; j++){
                    VIMEout->RecordDst[i][j] = 0;
                    VIMEout->RecordMvs[i][j].x = 0;
                    VIMEout->RecordMvs[i][j].y = 0;
                }
                for(j = 6; j < 8; j++){
                    VIMEout->RecordDst[i][j] = 0;
                    VIMEout->RecordMvs[i][j].x = 0;
                    VIMEout->RecordMvs[i][j].y = 0;
                }
                VIMEout->RecordRefId[i][0] &= 0xf; //zero out 16x8_1
                VIMEout->RecordRefId[i][1] = 0; //zero out 8x16 and 8x8_2_3
                VIMEout->RecordRefId[i][3] = 0; 
            }
        }
        else
        {//chroma inter
            for (j=0; j<4;j++){
                    VIMEout->RecordDst[i][j] = 0; //zero out 16x8 and 8x16 (index 0-3)
                    VIMEout->RecordMvs[i][j].x = 0;
                    VIMEout->RecordMvs[i][j].y = 0;
                    VIMEout->RecordRefId[i][j/2] = 0;//zero out 16x8 and 8x16 index 0-1
            }
        }
    }
    return;
}

void MEforGen75::ReplicateSadMVStreamIn()
{
    ReplicateSadMV();

    //replicate whether stream in wins for reduced MB size
    int i,j;
    for (i = 0; i<2; i++) 
    {    
        if(Vsta.SrcType&1)
        {
            // src 16x8            
            // 8x8
            for(j=0;j<2;j++)
            {
                if (!(Vsta.SadType&INTER_CHROMA_MODE)) //luma 16x8 and 8x8
                MultiReplaced[i][B8X8+2+j] = MultiReplaced[i][B8X8+j];
            }

            // 16x8
            MultiReplaced[i][BHX8+1] = MultiReplaced[i][BHX8];
        }
        if(Vsta.SrcType&2)
        {
            // src 8x16
            // 8x8
            for(j=0;j<3;j++)
            {
                if(j==1) j = 2;
                if (!(Vsta.SadType&INTER_CHROMA_MODE)) 
                MultiReplaced[i][B8X8+1+j] = MultiReplaced[i][B8X8+j];
            }

            // 8x16
            MultiReplaced[i][B8XH+1] = MultiReplaced[i][B8XH];
        }
    }
}
