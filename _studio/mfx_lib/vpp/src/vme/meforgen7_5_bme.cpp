//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2013 Intel Corporation. All Rights Reserved.
//

#include "meforgen7_5.h"

#pragma warning( disable : 4244 )
#pragma warning( disable : 4100 )

/*****************************************************************************************************/
int MEforGen75::BidirectionalMESearchUnit(I32 *dist, U8 *subpredmode)
/*****************************************************************************************************/
{
    I32        m, d, d4, fb, dst[4], k, i;
    m = 0;
    FoundBi = fb = 0;
    U8 numBi8x8 = 0;
    U8 penalty=0;
    U8 partition = 0;
    bool bwd_penalty = false;
    i = 0;

    bool bidirValid = true;
    bidirValid = CheckBidirValid();//m, checkPruning, true);

    if(!bidirValid){
        if(Vsta.FBRMbInfo==MODE_INTER_16X16)  partition = 1;
        if(Vsta.FBRMbInfo==MODE_INTER_8X16 || Vsta.FBRMbInfo==MODE_INTER_16X8)  partition = 2;
        if(Vsta.FBRMbInfo==MODE_INTER_8X8)  partition = 4;
        
        U8 loop = 1;
        if(!(Vsta.SadType&INTER_CHROMA_MODE))
            loop = ((Vsta.SrcType&3)+1);

        for(U8 u=0;u<(partition/loop);u++){
             penalty +=(Vsta.FBRSubPredMode>>2*u)&3 ? !L0_BWD_Penalty: L0_BWD_Penalty ; 
        }
        *dist += penalty*LutMode[LUTMODE_INTER_BWD];
        return 1;
    }
    
    if(Vsta.FBRMbInfo==MODE_INTER_16X16){
        //if(Vsta.BidirMask&BID_NO_16X16) return 0;
        d = DistInter + 0 - LutMode[LUTMODE_INTER_16x16];
        //if(Dist[0][BHXH]>=d||Dist[1][BHXH]>=d) return 0;
        bwd_penalty = (*subpredmode&0x1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
        CheckBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],Best[0][BHXH],Best[1][BHXH],16,16,0,0,&d,&dst[0],RefID[0][0],RefID[1][0]);
        if((dst[0]+LutMode[LUTMODE_INTER_BWD])<(Dist[*subpredmode&0x1][BHXH]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){
            Dist[*subpredmode&0x1][BHXH] = dst[0];
            d = LutMode[LUTMODE_INTER_16x16]+dst[0]+LutMode[LUTMODE_INTER_BWD];
            //if(d<DistInter){*dist = d; DistInter = *dist; FoundBi |= (1<<BHXH); *mode = 0x0300;}
            if(d<*dist+bwd_penalty*LutMode[LUTMODE_INTER_BWD]){*dist = d; FoundBi |= (1<<BHXH); *subpredmode=0x02;}
        }
        else
        {
            *dist += bwd_penalty*LutMode[LUTMODE_INTER_BWD] ;
        }
        return 0;
    }
    if(Vsta.FBRMbInfo==MODE_INTER_8X16){ // 8x16
        //if(Vsta.BidirMask&BID_NO_8X16) return 0;
        d = DistInter + 0 - LutMode[LUTMODE_INTER_8x16];
        //if(Dist[0][B8XH]+Dist[0][B8XH+1]>=d) return 0;
        //if(Dist[1][B8XH]+Dist[1][B8XH+1]>=d) return 0;
        CheckBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],Best[0][B8XH],Best[1][B8XH],8,16,0,0,&d,&dst[0],RefID[0][0],RefID[1][0]);
        CheckBidirectional(SrcMB+8,Vsta.RefLuma[0][RefID[0][1]],Vsta.RefLuma[1][RefID[1][1]],Best[0][B8XH+1],Best[1][B8XH+1],8,16,32,0,&d,&dst[1],RefID[0][1],RefID[1][1]);
        if(Vsta.VmeFlags & VF_BIMIX_DISABLE){
            d = LutMode[LUTMODE_INTER_8x16] + dst[0] + dst[1] + (LutMode[LUTMODE_INTER_BWD]<<1);
            partition = 2;
            for(U8 u=0;u<partition;u++)  penalty +=(Vsta.FBRSubPredMode>>2*u)&3 ? !L0_BWD_Penalty: L0_BWD_Penalty ; 
                *dist += penalty*LutMode[LUTMODE_INTER_BWD];
            ScaleDist(d);
            if(d<*dist){
                Dist[(*subpredmode>>0)&0x03][B8XH+0] = dst[0];
                Dist[(*subpredmode>>2)&0x03][B8XH+1] = dst[1]; 
                //if(d<DistInter){*dist = d; DistInter = *dist; FoundBi |= (3<<B8XH); *mode = 0x0F00;}
                *dist = d; FoundBi |= (3<<B8XH); *subpredmode = 0x0A;
            }/*
            else
            {   // output the Hpel or Qpel after adding the Backward penalty
                partition = 2;
                for(U8 u=0;u<partition;u++)  penalty +=(Vsta.FBRSubPredMode>>2*u)&3 ? !L0_BWD_Penalty: L0_BWD_Penalty ; 
                *dist += penalty*LutMode[LUTMODE_INTER_BWD];
            }*/
        }
        else{
            bwd_penalty = (((*subpredmode>>0)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
            if((dst[0]+LutMode[LUTMODE_INTER_BWD])<(Dist[(*subpredmode>>0)&0x03][B8XH  ]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ Dist[(*subpredmode>>0)&0x03][B8XH  ] = dst[0]; fb |= (1<<B8XH); m |= 0x2; numBi8x8 += 1;}
            else{numBi8x8 += bwd_penalty; m |= (*subpredmode>>0)&0x03;}
            bwd_penalty = (((*subpredmode>>2)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
            if((dst[1]+LutMode[LUTMODE_INTER_BWD])<(Dist[(*subpredmode>>2)&0x03][B8XH+1]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ Dist[(*subpredmode>>2)&0x03][B8XH+1] = dst[1]; fb |= (2<<B8XH); m |= 0x8; numBi8x8 += 1;}
            else{numBi8x8 += bwd_penalty;m |= ((*subpredmode>>2)&0x03)<<2;}
            d = LutMode[LUTMODE_INTER_8x16]+Dist[(*subpredmode>>0)&0x03][B8XH]+Dist[(*subpredmode>>2)&0x03][B8XH+1] + numBi8x8*LutMode[LUTMODE_INTER_BWD];
            //if(d<DistInter){*dist = d; DistInter = *dist; *mode = m; FoundBi = fb;}
            ScaleDist(d);
            if(d<(*dist+numBi8x8*LutMode[LUTMODE_INTER_BWD])){*dist = d; *subpredmode = m; FoundBi = fb;}
            else
            {
                *subpredmode = m;
                *dist += numBi8x8*LutMode[LUTMODE_INTER_BWD];
            }
        }
        return 0;
    }
    if(Vsta.FBRMbInfo==MODE_INTER_16X8){ // 16x8
        //if(Vsta.BidirMask&BID_NO_16X8) return 0;
        d = DistInter + 0- LutMode[LUTMODE_INTER_16x8];
        //if(Dist[0][BHX8]+Dist[0][BHX8+1]>=d) return 0;
        //if(Dist[1][BHX8]+Dist[1][BHX8+1]>=d) return 0;
        CheckBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],Best[0][BHX8],Best[1][BHX8],16,8,0,0,&d,&dst[0],RefID[0][0],RefID[1][0]);
        if(Vsta.SrcType&0x1)// 16x8
        {
            dst[1] = dst[0];
        }else
        {
            CheckBidirectional(SrcMB+128,Vsta.RefLuma[0][RefID[0][1]],Vsta.RefLuma[1][RefID[1][1]],Best[0][BHX8+1],Best[1][BHX8+1],16,8,0,32,&d,&dst[1],RefID[0][1],RefID[1][1]);
        }
        if(Vsta.VmeFlags & VF_BIMIX_DISABLE){

            d = LutMode[LUTMODE_INTER_16x8] + dst[0] + dst[1] + (LutMode[LUTMODE_INTER_BWD]<<1);
            partition = 2;
            for(U8 u=0;u<partition;u++)  penalty +=(Vsta.FBRSubPredMode>>2*u)&3 ? !L0_BWD_Penalty: L0_BWD_Penalty ; 
            if (Vsta.SrcType&ST_SRC_16X8) {   penalty = Vsta.FBRSubPredMode&3 ? !L0_BWD_Penalty: L0_BWD_Penalty ;  }

            *dist += penalty*LutMode[LUTMODE_INTER_BWD];
            ScaleDist(d);
            if(d<*dist){
                Dist[(*subpredmode>>0)&0x03][BHX8+0] = dst[0];
                Dist[(*subpredmode>>2)&0x03][BHX8+1] = dst[1]; 
                //if(d<DistInter){*dist = d; DistInter = *dist; FoundBi |= (3<<BHX8); *mode = 0x3300;}
                *dist = d; FoundBi |= (3<<BHX8); *subpredmode = 0x0A;
            }

        }
        else{
            bwd_penalty = (((*subpredmode>>0)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
            if((dst[0]+LutMode[LUTMODE_INTER_BWD])<(Dist[(*subpredmode>>0)&0x03][BHX8  ]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ Dist[(*subpredmode>>0)&0x03][BHX8  ] = dst[0]; fb |= (1<<BHX8); m |= 0x02; numBi8x8 += 1;}
            else{numBi8x8 += bwd_penalty; m |= (*subpredmode>>0)&0x03; }

            if (!(Vsta.SrcType&ST_SRC_16X8)) // in case of reduced size
            {
                bwd_penalty = (((*subpredmode>>2)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
                if((dst[1]+LutMode[LUTMODE_INTER_BWD])<(Dist[(*subpredmode>>2)&0x03][BHX8+1]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ Dist[(*subpredmode>>2)&0x03][BHX8+1] = dst[1]; fb |= (2<<BHX8); m |= 0x08; numBi8x8 += 1;}
                else{numBi8x8 += bwd_penalty; m |= ((*subpredmode>>2)&0x03)<<2; }
                d = LutMode[LUTMODE_INTER_16x8]+Dist[(*subpredmode>>0)&0x03][BHX8]+Dist[(*subpredmode>>2)&0x03][BHX8+1] + numBi8x8*LutMode[LUTMODE_INTER_BWD];
            }
            else
            {
                d = (LutMode[LUTMODE_INTER_16x8]>>1)+Dist[(*subpredmode>>0)&0x03][BHX8]+ numBi8x8*LutMode[LUTMODE_INTER_BWD];
            }
            if(d<(*dist+numBi8x8*LutMode[LUTMODE_INTER_BWD])){*dist = d; *subpredmode = m; FoundBi = fb;}
            else
            {
                *subpredmode = m;
                *dist += numBi8x8*LutMode[LUTMODE_INTER_BWD];
            }        
        }
        return 0;
    }
    if(Vsta.FBRMbInfo==MODE_INTER_16X8 && Vsta.SrcType&ST_REF_FIELD){ // Field 16x8
        //if(Vsta.BidirMask&BID_NO_16X8) return 0;
        d = DistInter + 0 - LutMode[LUTMODE_INTER_16x8_FIELD];
        if(Dist[0][FHX8]+Dist[0][FHX8+1]>=d) return 0;
        if(Dist[1][FHX8]+Dist[1][FHX8+1]>=d) return 0;
        CheckBidirectionalField(SrcMB,Best[0][FHX8],Best[1][FHX8],16,8,&d,&dst[0]);
        CheckBidirectionalField(SrcMB+16,Best[0][FHX8+1],Best[1][FHX8+1],16,8,&d,&dst[1]);
        if(Vsta.VmeFlags & VF_BIMIX_DISABLE){
            if((d = LutMode[LUTMODE_INTER_16x8_FIELD] + dst[0] + dst[1])<*dist){
                Dist[0][FHX8  ] = dst[0]; Dist[0][FHX8+1] = dst[1]; 
                if(d<DistInter){*dist = d; DistInter = *dist; FoundBi |= (3<<FHX8); *subpredmode = 0x0A;}
            }
        }
        else{
            if(dst[0]<Dist[0][FHX8  ]){ Dist[0][FHX8  ] = dst[0]; fb |= (1<<FHX8); m |= 0x02; }
            if(dst[1]<Dist[0][FHX8+1]){ Dist[0][FHX8+1] = dst[1]; fb |= (2<<FHX8); m |= 0x08; }
            d = LutMode[LUTMODE_INTER_16x8]+Dist[0][FHX8]+Dist[0][FHX8+1];
            if(d<DistInter){*dist = d; DistInter = *dist; *subpredmode = m; FoundBi = fb;}
        }
        return 0;
    }

    //if(Vsta.BidirMask&BID_NO_8X8) return 0;

    U16 bwd_cost =  LutMode[LUTMODE_INTER_BWD];

    if(!(Vsta.FBRSubMbShape&0xFF)){ // All 4 subblocks are 8x8
        d = DistInter + 0 - (LutMode[LUTMODE_INTER_8x8q]<<2);
        //if(Dist[0][B8X8]+Dist[0][B8X8+1]+Dist[0][B8X8+2]+Dist[0][B8X8+3]>=d) return 0;
        //if(Dist[1][B8X8]+Dist[1][B8X8+1]+Dist[1][B8X8+2]+Dist[1][B8X8+3]>=d) return 0;
        
        if (Vsta.SrcType&1 && Vsta.SrcType&2)// 8x8
        {
            CheckBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],Best[0][B8X8],Best[1][B8X8],8,8,0,0,&k,&dst[0],RefID[0][0],RefID[1][0]);
            dst[3] = dst[2] = dst[1] = dst[0];
        }
        else if (Vsta.SrcType&1 && !(Vsta.SrcType&2))// 16x8
        {
            CheckBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],Best[0][B8X8],Best[1][B8X8],8,8,0,0,&k,&dst[0],RefID[0][0],RefID[1][0]);
            CheckBidirectional(SrcMB+8,Vsta.RefLuma[0][RefID[0][1]],Vsta.RefLuma[1][RefID[1][1]],Best[0][B8X8+1],Best[1][B8X8+1],8,8,32,0,&k,&dst[1],RefID[0][1],RefID[1][1]);
            dst[2] = dst[0];
            dst[3] = dst[1]; 
        }
        else if (!(Vsta.SrcType&1) && Vsta.SrcType&2)// 8x16
        {
            CheckBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],Best[0][B8X8],Best[1][B8X8],8,8,0,0,&k,&dst[0],RefID[0][0],RefID[1][0]);
            CheckBidirectional(SrcMB+128,Vsta.RefLuma[0][RefID[0][2]],Vsta.RefLuma[1][RefID[1][2]],Best[0][B8X8+2],Best[1][B8X8+2],8,8,0,32,&k,&dst[2],RefID[0][2],RefID[1][2]);
            dst[1] = dst[0];
            dst[3] = dst[2]; 
        }
        else // 16x16
        {
            CheckBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],Best[0][B8X8],Best[1][B8X8],8,8,0,0,&k,&dst[0],RefID[0][0],RefID[1][0]);
            CheckBidirectional(SrcMB+8,Vsta.RefLuma[0][RefID[0][1]],Vsta.RefLuma[1][RefID[1][1]],Best[0][B8X8+1],Best[1][B8X8+1],8,8,32,0,&k,&dst[1],RefID[0][1],RefID[1][1]);
            CheckBidirectional(SrcMB+128,Vsta.RefLuma[0][RefID[0][2]],Vsta.RefLuma[1][RefID[1][2]],Best[0][B8X8+2],Best[1][B8X8+2],8,8,0,32,&k,&dst[2],RefID[0][2],RefID[1][2]);
            CheckBidirectional(SrcMB+136,Vsta.RefLuma[0][RefID[0][3]],Vsta.RefLuma[1][RefID[1][3]],Best[0][B8X8+3],Best[1][B8X8+3],8,8,32,32,&k,&dst[3],RefID[0][3],RefID[1][3]);
        }
        if(Vsta.VmeFlags & VF_BIMIX_DISABLE){
            d = (LutMode[LUTMODE_INTER_8x8q]<<2) + (LutMode[LUTMODE_INTER_BWD]<<2) + dst[0] + dst[1] + dst[2]+ dst[3];
            ScaleDist(d);

            partition = 4/((Vsta.SrcType&3)+1);
            for(U8 u=0;u<partition;u++)  penalty +=(Vsta.FBRSubPredMode>>2*u)&3 ? !L0_BWD_Penalty: L0_BWD_Penalty ; 
            *dist += penalty*LutMode[LUTMODE_INTER_BWD];

            if(d<*dist){
                Dist[((*subpredmode>>0)&0x03)][B8X8  ] = dst[0]; 
                Dist[((*subpredmode>>2)&0x03)][B8X8+1] = dst[1]; 
                Dist[((*subpredmode>>4)&0x03)][B8X8+2] = dst[2];  
                Dist[((*subpredmode>>6)&0x03)][B8X8+3] = dst[3]; 
                *dist = d; FoundBi |= (15<<B8X8); *subpredmode = 0xAA;
            }
            /*else{     // output the Hpel or Qpel after adding the Backward penalty
                partition = 4/(Vsta.SrcType+1);
                for(U8 u=0;u<partition;u++)  penalty +=(Vsta.FBRSubPredMode>>2*u)&3 ? !L0_BWD_Penalty: L0_BWD_Penalty ; 
                *dist += penalty*LutMode[LUTMODE_INTER_BWD];}*/
                }
        else{
            bwd_penalty = (((*subpredmode>>0)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
            if((dst[0]+bwd_cost)<(Dist[((*subpredmode>>0)&0x03)][B8X8  ]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ Dist[((*subpredmode>>0)&0x03)][B8X8  ] = dst[0]; fb |= (1<<B8X8); m |= 0x02; numBi8x8++;}
            else{if(bwd_penalty) numBi8x8++; m |= (*subpredmode>>0)&0x03;}

            if ((Vsta.SrcType&ST_SRC_8X8)!=ST_SRC_8X8)
            {
                bwd_penalty = (((*subpredmode>>2)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
                if((dst[1]+bwd_cost)<(Dist[((*subpredmode>>2)&0x03)][B8X8+1]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ Dist[((*subpredmode>>2)&0x03)][B8X8+1] = dst[1]; fb |= (2<<B8X8); m |= 0x08; numBi8x8++;}
                else{if(bwd_penalty) numBi8x8++;m |= ((*subpredmode>>2)&0x03)<<2;}
                
                if ((Vsta.SrcType&ST_SRC_8X8)!=ST_SRC_16X8)
                {
                    bwd_penalty = (((*subpredmode>>4)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
                    if((dst[2]+bwd_cost)<(Dist[((*subpredmode>>4)&0x03)][B8X8+2]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ Dist[((*subpredmode>>4)&0x03)][B8X8+2] = dst[2]; fb |= (4<<B8X8); m |= 0x20; numBi8x8++;}
                    else{if(bwd_penalty) numBi8x8++;m |= ((*subpredmode>>4)&0x03)<<4;}
                    
                    bwd_penalty = (((*subpredmode>>6)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
                    if((dst[3]+bwd_cost)<(Dist[((*subpredmode>>6)&0x03)][B8X8+3]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ Dist[((*subpredmode>>6)&0x03)][B8X8+3] = dst[3]; fb |= (8<<B8X8); m |= 0x80; numBi8x8++;}
                    else{if(bwd_penalty) numBi8x8++;m |= ((*subpredmode>>6)&0x03)<<6;}
                    
                    d = (LutMode[LUTMODE_INTER_8x8q]<<2)+ LutMode[LUTMODE_INTER_BWD]*numBi8x8+ Dist[((*subpredmode>>0)&0x03)][B8X8]+Dist[((*subpredmode>>2)&0x03)][B8X8+1]+Dist[((*subpredmode>>4)&0x03)][B8X8+2]+Dist[((*subpredmode>>6)&0x03)][B8X8+3];
                }
                else
                {
                    d = LutMode[LUTMODE_INTER_8x8q]*2 + LutMode[LUTMODE_INTER_BWD]*numBi8x8 + Dist[((*subpredmode>>0)&0x03)][B8X8]+Dist[((*subpredmode>>2)&0x03)][B8X8+1];
                }
            }
            else
            {
                d = LutMode[LUTMODE_INTER_8x8q] + LutMode[LUTMODE_INTER_BWD]*numBi8x8 + Dist[((*subpredmode>>0)&0x03)][B8X8];
            }

            if(d<*dist+numBi8x8*LutMode[LUTMODE_INTER_BWD]){*dist = d; *subpredmode = m; FoundBi = fb;}
            else
            {
                *subpredmode = m;
                *dist += numBi8x8*LutMode[LUTMODE_INTER_BWD];
            }
        }
        return 0;
    }
    // No bidirectional for real minors if not allowed mixing:
    if(Vsta.VmeFlags & VF_BIMIX_DISABLE) 
    {
         partition = 4;
         for(U8 u=0;u<partition;u++)  penalty +=(Vsta.FBRSubPredMode>>2*u)&3 ? !L0_BWD_Penalty: L0_BWD_Penalty ; 
        *dist += penalty*LutMode[LUTMODE_INTER_BWD];
        return 0;
    }

    // Some of the 4 subblocks is not 8x8:
    bwd_penalty = (((*subpredmode>>0)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
    d = DistInter + 0 - (LutMode[LUTMODE_INTER_8x8q]<<2);
    d4 = 0;     
    //if(Dist[0][B8X8]+Dist[0][B8X8+1]+Dist[0][B8X8+2]+Dist[0][B8X8+3]>=d) return 0;
    //if(Dist[1][B8X8]+Dist[1][B8X8+1]+Dist[1][B8X8+2]+Dist[1][B8X8+3]>=d) return 0;
    

        unsigned char t = Vsta.FBRSubMbShape;
        unsigned char s = Vsta.FBRSubPredMode;
        int    u,j,x,y;
        int loops = 4;
        //int srcindex[4]={0,8,128,136};
        for(u=0;u<4;u++){
             x = ((u&1)<<3);
             y = ((u&2)<<2);

             if (u>=loops)
             {
                 break;
             }
             if(Vsta.SrcType & 0x1){ loops >>= 1; }
             if(Vsta.SrcType & 0x2){ loops >>= 1; }
             bwd_penalty = (((*subpredmode>>(2*u))&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
             switch(t&3){
            case 0: // 8x8
                j = B8X8+u;
                if(u==0)
                CheckBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],Best[0][B8X8],Best[1][B8X8],8,8,0,0,&k,&d,RefID[0][0],RefID[1][0]);
                if(u==1)
                CheckBidirectional(SrcMB+8,Vsta.RefLuma[0][RefID[0][1]],Vsta.RefLuma[1][RefID[1][1]],Best[0][B8X8+1],Best[1][B8X8+1],8,8,32*((Vsta.SrcType&0x2)?0:1),0,&k,&d,RefID[0][1],RefID[1][1]);
                if(u==2)
                CheckBidirectional(SrcMB+128,Vsta.RefLuma[0][RefID[0][2]],Vsta.RefLuma[1][RefID[1][2]],Best[0][B8X8+2],Best[1][B8X8+2],8,8,0,32*((Vsta.SrcType&0x1)?0:1),&k,&d,RefID[0][2],RefID[1][2]);
                if(u==3)
                CheckBidirectional(SrcMB+136,Vsta.RefLuma[0][RefID[0][3]],Vsta.RefLuma[1][RefID[1][3]],Best[0][B8X8+3],Best[1][B8X8+3],8,8,32*((Vsta.SrcType&0x2)?0:1),32*((Vsta.SrcType&0x1)?0:1),&k,&d,RefID[0][3],RefID[1][3]);
                if((d+bwd_cost)<(Dist[(Vsta.FBRSubPredMode>>2*u)&3][j]+bwd_penalty*LutMode[LUTMODE_INTER_BWD]))
                { 
                    d4 += Dist[(Vsta.FBRSubPredMode>>2*u)&3][j] - d; 
                    Dist[((Vsta.FBRSubPredMode>>2*u)&0x3)][j] = d; 
                    m |= (0x02 << 2*u); 
                    //if(!bwd_penalty) 
                    //    d4 -= LutMode[LUTMODE_INTER_BWD];
                    *dist += bwd_cost ;
                }
                else
                {
                    m |= ((*subpredmode>>2*u)&0x03)<<2*u;
                    *dist += bwd_penalty*LutMode[LUTMODE_INTER_BWD] ;
                }
                break;
            case 1:
            case 2:
            case 3:
                m |= ((*subpredmode>>2*u)&0x03)<<2*u;
                *dist += bwd_penalty*LutMode[LUTMODE_INTER_BWD] ;
                break;
            }
            t>>=2;
            s>>=2;
        }

    d = *dist - d4;
    //if(d<DistInter){ DistInter = *dist = d; ; *mode = m;}
    if(d<*dist){ *dist = d; ; *subpredmode = m;}            
    else
    {
        *subpredmode = m;
    }
    return 0;
}


/*****************************************************************************************************/
int MEforGen75::BidirectionalMESearchUnitChroma(I32 *dist, U8 *subpredmode)
/*****************************************************************************************************/
{
    I32        m, d, fb, dst[4], k, t[4], i;
    m = 0;
    FoundBi = fb = 0;
    U8 numBi8x8 = 0;
    i = 0;

    bool bidirValid = true;
    bidirValid = CheckBidirValid();//m, checkPruning, true);

    if(!bidirValid)    return 1;

    //if(Vsta.FBRMbInfo==MODE_INTER_8X8 && ((Vsta.FBRSubMbShape&0x03)==0) && Vsta.BidirMask&BID_NO_8X8) return 0;
    //if(Vsta.FBRMbInfo==MODE_INTER_8X8 && ((Vsta.FBRSubMbShape&0x03)==3) && Vsta.BidirMask&BID_NO_MINORS) return 0;

    if((Vsta.FBRSubMbShape&0x03)==0){
        CheckBidirectional(SrcMBU,Vsta.RefCb[0][RefID[0][0]],Vsta.RefCb[1][RefID[1][0]],Best[0][B8X8],Best[1][B8X8],8,8,0,0,&t[0],&dst[0],RefID[0][0],RefID[1][0]);
        CheckBidirectional(SrcMBV,Vsta.RefCr[0][RefID[0][0]],Vsta.RefCr[1][RefID[1][0]],Best[0][B8X8],Best[1][B8X8],8,8,0,0,&k,&dst[0],RefID[0][0],RefID[1][0]);
        dst[0] += t[0]; // only add MV cost once

        if(dst[0]<(Dist[*subpredmode&0x1][B8X8])){
            Dist[*subpredmode&0x1][B8X8] = dst[0];
            d = LutMode[LUTMODE_INTER_8x8q]+dst[0];
            if(d<*dist){*dist = d; FoundBi |= (1<<BHXH); *subpredmode=0x02;}
        }
        return 0;
    }
    else if((Vsta.FBRSubMbShape&0x03)==3){
        CheckBidirectional(SrcMBU+0,Vsta.RefCb[0][RefID[0][0]],Vsta.RefCb[1][RefID[1][0]],Best[0][B4X4],Best[1][B4X4],4,4,0,0,&t[0],&dst[0],RefID[0][0],RefID[1][0]);
        CheckBidirectional(SrcMBU+4,Vsta.RefCb[0][RefID[0][1]],Vsta.RefCb[1][RefID[1][1]],Best[0][B4X4+1],Best[1][B4X4+1],4,4,16,0,&t[1],&dst[1],RefID[0][1],RefID[1][1]);
        CheckBidirectional(SrcMBU+64,Vsta.RefCb[0][RefID[0][2]],Vsta.RefCb[1][RefID[1][2]],Best[0][B4X4+2],Best[1][B4X4+2],4,4,0,16,&t[2],&dst[2],RefID[0][2],RefID[1][2]);
        CheckBidirectional(SrcMBU+68,Vsta.RefCb[0][RefID[0][3]],Vsta.RefCb[1][RefID[1][3]],Best[0][B4X4+3],Best[1][B4X4+3],4,4,16,16,&t[3],&dst[3],RefID[0][3],RefID[1][3]);
        CheckBidirectional(SrcMBV+0,Vsta.RefCr[0][RefID[0][0]],Vsta.RefCr[1][RefID[1][0]],Best[0][B4X4],Best[1][B4X4],4,4,0,0,&k,&dst[0],RefID[0][0],RefID[1][0]);
        CheckBidirectional(SrcMBV+4,Vsta.RefCr[0][RefID[0][1]],Vsta.RefCr[1][RefID[1][1]],Best[0][B4X4+1],Best[1][B4X4+1],4,4,16,0,&k,&dst[1],RefID[0][1],RefID[1][1]);
        CheckBidirectional(SrcMBV+64,Vsta.RefCr[0][RefID[0][2]],Vsta.RefCr[1][RefID[1][2]],Best[0][B4X4+2],Best[1][B4X4+2],4,4,0,16,&k,&dst[2],RefID[0][2],RefID[1][2]);
        CheckBidirectional(SrcMBV+68,Vsta.RefCr[0][RefID[0][3]],Vsta.RefCr[1][RefID[1][3]],Best[0][B4X4+3],Best[1][B4X4+3],4,4,16,16,&k,&dst[3],RefID[0][3],RefID[1][3]);
        dst[0] += t[0]; // only add MV cost once
        dst[1] += t[1]; // only add MV cost once
        dst[2] += t[2]; // only add MV cost once
        dst[3] += t[3]; // only add MV cost once

        if(Vsta.VmeFlags & VF_BIMIX_DISABLE){
            d = LutMode[LUTMODE_INTER_4x4q] + dst[0] + dst[1] + dst[2]+ dst[3];

            if(d<*dist){
                Dist[(*subpredmode>>0)&0x03][B4X4  ] = dst[0];  
                Dist[(*subpredmode>>2)&0x03][B4X4+1] = dst[1]; 
                Dist[(*subpredmode>>4)&0x03][B4X4+2] = dst[2];  
                Dist[(*subpredmode>>6)&0x03][B4X4+3] = dst[3]; 
                *dist = d; FoundBi |= (15<<B4X4); *subpredmode = 0xAA;
            }
        }
        else{
            if((dst[0])<(Dist[(*subpredmode>>0)&0x03][B4X4  ]))
            { 
                Dist[(*subpredmode>>0)&0x03][B4X4  ] = dst[0]; 
                fb |= (1<<B4X4); 
                m |= 0x02; 
                numBi8x8++;
            }
            else
            {
                m |= (*subpredmode>>0)&0x03;
            }
            if((dst[1])<(Dist[(*subpredmode>>2)&0x03][B4X4+1]))
            { 
                Dist[(*subpredmode>>2)&0x03][B4X4+1] = dst[1]; 
                fb |= (2<<B4X4); 
                m |= 0x08; 
                numBi8x8++;
            }
            else
            {
                m |= ((*subpredmode>>2)&0x03)<<2;
            }
            if((dst[2])<(Dist[(*subpredmode>>4)&0x03][B4X4+2]))
            { 
                Dist[(*subpredmode>>4)&0x03][B4X4+2] = dst[2]; 
                fb |= (4<<B4X4); 
                m |= 0x20; 
                numBi8x8++;
            }
            else
            {
                m |= ((*subpredmode>>4)&0x03)<<4;
            }
            if((dst[3])<(Dist[(*subpredmode>>6)&0x03][B4X4+3]))
            { 
                Dist[(*subpredmode>>6)&0x03][B4X4+3] = dst[3]; 
                fb |= (8<<B4X4); 
                m |= 0x80; 
                numBi8x8++;
            }
            else
            {
                m |= ((*subpredmode>>6)&0x03)<<6;
            }
            d = LutMode[LUTMODE_INTER_4x4q] + Dist[(*subpredmode>>0)&0x01][B4X4]+
                Dist[(*subpredmode>>2)&0x01][B4X4+1]+Dist[(*subpredmode>>4)&0x01][B4X4+2]+Dist[(*subpredmode>>6)&0x01][B4X4+3];

            if(d<*dist){*dist = d; *subpredmode = m; FoundBi = fb;}
        }
    }

/*
    // No bidirectional for real minors if not allowed mixing:
    if(Vsta.VmeFlags & VF_BIMIX_DISABLE) return 0;

    // Some of the 4 subblocks is not 8x8:
    d = DistInter + ErrTolerance - (LutMode[LUTMODE_INTER_8x8q]<<2);
    //if(Dist[0][B8X8]+Dist[0][B8X8+1]+Dist[0][B8X8+2]+Dist[0][B8X8+3]>=d) return 0;
    //if(Dist[1][B8X8]+Dist[1][B8X8+1]+Dist[1][B8X8+2]+Dist[1][B8X8+3]>=d) return 0;
    
    d4 = 0;     
    bwd_penalty = (((*subpredmode>>0)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
    if(!(Vsta.FBRSubMbShape&0x03)){
         CheckBidirectional(SrcMB,Best[0][B8X8],Best[1][B8X8],8,8,0,0,&k,&d);
        if((d+bwd_cost)<(Dist[0][B8X8]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ d4 += Dist[0][B8X8] - d; Dist[0][B8X8] = d; m |= 0x02; if(!bwd_penalty) d4 -= LutMode[LUTMODE_INTER_BWD];}
    }

    bwd_penalty = (((*subpredmode>>2)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
    if(!(Vsta.FBRSubMbShape&0x0C)){
        CheckBidirectional(SrcMB+8,Best[0][B8X8+1],Best[1][B8X8+1],8,8,32*((Vsta.SrcType&0x2)?0:1),0,&k,&d);
        if((d+bwd_cost)<(Dist[0][B8X8+1]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ d4 += Dist[0][B8X8+1] - d; Dist[0][B8X8+1] = d; m |= 0x08; if(!bwd_penalty) d4 -= LutMode[LUTMODE_INTER_BWD];}
    }

    bwd_penalty = (((*subpredmode>>4)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
    if(!(Vsta.FBRSubMbShape&0x30)){
        CheckBidirectional(SrcMB+128,Best[0][B8X8+2],Best[1][B8X8+2],8,8,0,32*((Vsta.SrcType&0x1)?0:1),&k,&d);
        if((d+bwd_cost)<(Dist[0][B8X8+2]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ d4 += Dist[0][B8X8+2] - d; Dist[0][B8X8+2] = d; m |= 0x20; if(!bwd_penalty) d4 -= LutMode[LUTMODE_INTER_BWD];}
    }

    bwd_penalty = (((*subpredmode>>6)&0x03)==1) ? !L0_BWD_Penalty : L0_BWD_Penalty;
    if(!(Vsta.FBRSubMbShape&0xC0)){
        CheckBidirectional(SrcMB+136,Best[0][B8X8+3],Best[1][B8X8+3],8,8,32*((Vsta.SrcType&0x2)?0:1),32*((Vsta.SrcType&0x1)?0:1),&k,&d);
        if((d+bwd_cost)<(Dist[0][B8X8+3]+bwd_penalty*LutMode[LUTMODE_INTER_BWD])){ d4 += Dist[0][B8X8+3] - d; Dist[0][B8X8+3] = d; m |= 0x80; if(!bwd_penalty) d4 -= LutMode[LUTMODE_INTER_BWD];}
    }

    ScaleDist(d4);

    d = *dist - d4;
    //if(d<DistInter){ DistInter = *dist = d; ; *mode = m;}
    if(d<*dist){ *dist = d; ; *subpredmode = m;}
*/
    return 0;
}

/*****************************************************************************************************/
int MEforGen75::CheckBidirectionalField(U8 *src, I16PAIR mv0, I16PAIR mv1, I32 bw, I32 bh, I32 *d0, I32 *d1)
/*****************************************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
    U8    rblk0[256], rblk1[256];
    U8    *ps = src; 
    U8    *pr = rblk0; 
    U8    *qr = rblk1; 
    
    if(GetReferenceBlockField(rblk0,RefPix[0],mv0.x,mv0.y,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
    if(GetReferenceBlockField(rblk1,RefPix[1],mv1.x,mv1.y,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
    for(int j=0;j<bh;j++){
        for(int i=0;i<bw;i++){
            pr[i] = ((qr[i] + (pr[i]-qr[i])* Vsta.BiWeight + 128)>>8);
        }
        ps += 16; pr += 16;    qr += 16;
    }
    *d0 = GetSadBlock(src,rblk0,bw,bh);
    *d1 = *d0 + GetCostXY(mv0.x,mv0.y,0)+GetCostXY(mv1.x,mv1.y,1);
    */
}

/*****************************************************************************************************/
void MEforGen75::CheckBidirectional(U8 *src, U8 *ref0, U8 *ref1, I16PAIR mv0, I16PAIR mv1, I32 bw, I32 bh, I32 mvx_offset, I32 mvy_offset, I32 *d0, I32 *d1, int refid0, int refid1)
/*****************************************************************************************************/
{
    U8    rblk0[256], rblk1[256];
    U8    *ps = src; 
    U8    *pr = rblk0; 
    U8    *qr = rblk1;
    int MVCost = 0;
    int RefIdCost = 0;
    int refCostScale = (bw==4&&bh==4) ? 2 : 0;

    IsBottom[0] = false;
    IsBottom[1] = false;
    if(Vsta.SrcType&ST_REF_FIELD)
    {
        IsField = true;    
        if(refid0&0x01)
            IsBottom[0] = true;
        if(refid1&0x01)
            IsBottom[1] = true;
    }

    I16PAIR tmp_mv;
    tmp_mv.x = (mv0.x >> 2) - 2;
    tmp_mv.y = (mv0.y >> 2) - 2;
#if (__HAAR_SAD_OPT == 1)
    bool copied;
    U8 *src_shifted=0;
    int        src_pitch;
    LoadReference2(ref0, SkipRef[0], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[0],copied,src_shifted,src_pitch);
#else
    LoadReference(ref0, SkipRef[0], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[0]);
#endif
    tmp_mv.x = mv0.x - (tmp_mv.x << 2);
    tmp_mv.y = mv0.y - (tmp_mv.y << 2);
#if (__HAAR_SAD_OPT == 1)
    if (copied) {
        if(GetReferenceBlock(rblk0,SkipRef[0],tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
    } else {
        if(GetReferenceBlock(rblk0,src_shifted,tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh,src_pitch)){ *d0 = *d1 = MBMAXDIST; return; }
    }
#else
    if(GetReferenceBlock(rblk0,SkipRef[0],tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
#endif
    tmp_mv.x = (mv1.x >> 2) - 2;
    tmp_mv.y = (mv1.y >> 2) - 2;
#if (__HAAR_SAD_OPT == 1)
    LoadReference2(ref1, SkipRef[1], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[1],copied,src_shifted,src_pitch);
#else
    LoadReference(ref1, SkipRef[1], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[1]);
#endif
    tmp_mv.x = mv1.x - (tmp_mv.x << 2);
    tmp_mv.y = mv1.y - (tmp_mv.y << 2);
#if (__HAAR_SAD_OPT == 1)
    if (copied) {
        if(GetReferenceBlock(rblk1,SkipRef[1],tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
    } else {
        if(GetReferenceBlock(rblk1,src_shifted,tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh,src_pitch)){ *d0 = *d1 = MBMAXDIST; return; }
    }
#else
    if(GetReferenceBlock(rblk1,SkipRef[1],tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
#endif

    /*if(Vsta.ShapeMask&SHP_BIDIR_AVS){
        for(int j=0;j<bh;j++){
            for(int i=0;i<bw;i++) pr[i] = ((pr[i]+qr[i]+1)>>1);
            ps += 16; pr += 16;    qr += 16;
        }
    }
    else{*/
    for(int j=0;j<bh;j++){
        for(int i=0;i<bw;i++){
            pr[i] = ((64-Vsta.BiWeight)*pr[i] + Vsta.BiWeight*qr[i])>>6;
        }
        ps += 16; pr += 16;    qr += 16;
    }
    //}
    
    *d0 = GetSadBlock(src,rblk0,bw,bh);
    MVCost = GetCostXY(mv0.x,mv0.y,0)+GetCostXY(mv1.x,mv1.y,1);
    MVCost = (MVCost > 0x3ff) ? 0x3ff : MVCost;
    RefIdCost = (GetCostRefID(refid0)>>refCostScale) + (GetCostRefID(refid1)>>refCostScale);
    RefIdCost = (RefIdCost > 0xfff) ? 0xfff : RefIdCost;
    
    *d1 = *d0 + MVCost + RefIdCost;
}

/*****************************************************************************************************/
void MEforGen75::CheckSkipBidirectional(U8 *src, U8 *ref0, U8 *ref1, I16PAIR mv0, I16PAIR mv1, I32 bw, I32 bh, I32 mvx_offset, I32 mvy_offset, I32 *d0, I32 *d1, U8 blkidx, bool chromaU, bool chromaV)
/*****************************************************************************************************/
{
    U8    rblk0[256], rblk1[256];
    U8    *ps = src; 
    U8    *pr = rblk0; 
    U8    *qr = rblk1;
    int MVCost = 0;
    int RefIdCost = 0;
    int refid = 0;
    int refCostScale = (bw==4&&bh==4) ? 2 : 0;
    I16PAIR tmp_mv;
    tmp_mv.x = (mv0.x >> 2) - 2;
    tmp_mv.y = (mv0.y >> 2) - 2;
#if (__HAAR_SAD_OPT == 1)
    bool copied;
    U8 *src_shifted=0;
    int        src_pitch;
    LoadReference2(ref0, SkipRef[0], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[0],copied,src_shifted,src_pitch);
#else
    LoadReference(ref0, SkipRef[0], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[0]);
#endif
    tmp_mv.x = mv0.x - (tmp_mv.x << 2);
    tmp_mv.y = mv0.y - (tmp_mv.y << 2);
#if (__HAAR_SAD_OPT == 1)
    if (copied) {
        if(GetReferenceBlock(rblk0,SkipRef[0],tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
    } else {
        if(GetReferenceBlock(rblk0,src_shifted,tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh,src_pitch)){ *d0 = *d1 = MBMAXDIST; return; }
    }
#else
    if(GetReferenceBlock(rblk0,SkipRef[0],tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
#endif
    tmp_mv.x = (mv1.x >> 2) - 2;
    tmp_mv.y = (mv1.y >> 2) - 2;
#if (__HAAR_SAD_OPT == 1)
    LoadReference2(ref1, SkipRef[1], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[1],copied,src_shifted,src_pitch);
#else
    LoadReference(ref1, SkipRef[1], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[1]);
#endif
    tmp_mv.x = mv1.x - (tmp_mv.x << 2);
    tmp_mv.y = mv1.y - (tmp_mv.y << 2);
#if (__HAAR_SAD_OPT == 1)
    if (copied) {
        if(GetReferenceBlock(rblk1,SkipRef[1],tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
    } else {
        if(GetReferenceBlock(rblk1,src_shifted,tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh,src_pitch)){ *d0 = *d1 = MBMAXDIST; return; }
    }
#else
    if(GetReferenceBlock(rblk1,SkipRef[1],tmp_mv.x+mvx_offset,tmp_mv.y+mvy_offset,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
#endif

    for(int j=0;j<bh;j++){
        for(int i=0;i<bw;i++){
            pr[i] = ((64-Vsta.BiWeight)*pr[i] + Vsta.BiWeight*qr[i])>>6;
        }
        ps += 16; pr += 16;    qr += 16;
    }
    
    bool bChroma = chromaU || chromaV;
    *d0 = GetSadBlock(src,rblk0,bw,bh, bChroma, chromaV);
    if(Vsta.SadType&FWD_TX_SKIP_CHECK){ 
        if(!chromaU && !chromaV){GetFtqBlock(src,rblk0,bw,bh,blkidx); }
        else{GetFtqBlockUV(src,rblk0,bw,bh,(chromaU ? 0 : 1)); }
    }

    tmp_mv= VSICsta.SkipCenter[blkidx][0];
    MVCost = GetCostXY(tmp_mv.x,tmp_mv.y,0);

    refid = RefID[0][blkidx];
    RefIdCost += GetCostRefID(refid)>>refCostScale;

    tmp_mv= VSICsta.SkipCenter[blkidx][1];
    MVCost += GetCostXY(tmp_mv.x,tmp_mv.y,1);

    refid = RefID[1][blkidx];
    RefIdCost += GetCostRefID(refid)>>refCostScale;

    MVCost = (MVCost > 0x3ff) ? 0x3ff : MVCost;
    RefIdCost = (RefIdCost > 0xfff) ? 0xfff : RefIdCost;
    MVCost +=RefIdCost;

    *d1 = *d0 + MVCost;
}

/*****************************************************************************************************/
void MEforGen75::CheckUnidirectional( U8 *src_mb, U8 *ref, I16PAIR mv, I32 refidx, I32 bw, I32 bh, I32 *d0, I32 *d1, U8 blkidx, bool chromaU, bool chromaV)
/*****************************************************************************************************/
{
    U8    rblk[256];
    I16PAIR tmp_mv;
    int refid = 0;
    int refCostScale = (bw==4&&bh==4) ? 2 : 0;
    tmp_mv.x = (mv.x >> 2) - 2;
    tmp_mv.y = (mv.y >> 2) - 2;
#if (__HAAR_SAD_OPT == 1)
    bool copied;
    U8 *src_shifted=0;
    int src_pitch;
    LoadReference2(ref, SkipRef[refidx], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[refidx],copied,src_shifted,src_pitch);
#else
    LoadReference(ref, SkipRef[refidx], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[refidx]);
#endif
    tmp_mv.x = mv.x - (tmp_mv.x << 2);
    tmp_mv.y = mv.y - (tmp_mv.y << 2);
#if (__HAAR_SAD_OPT == 1)
    if (copied) {
        if(GetReferenceBlock(rblk,SkipRef[refidx],tmp_mv.x,tmp_mv.y,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
    } else {
        if(GetReferenceBlock(rblk,src_shifted,tmp_mv.x,tmp_mv.y,bw,bh,src_pitch)){ *d0 = *d1 = MBMAXDIST; return; }
    }
#else
    if(GetReferenceBlock(rblk,SkipRef[refidx],tmp_mv.x,tmp_mv.y,bw,bh)){ *d0 = *d1 = MBMAXDIST; return; }
#endif

    bool bChroma = chromaU || chromaV;
    *d0 = GetSadBlock(src_mb,rblk,bw,bh, bChroma, chromaV);
    if(Vsta.SadType&FWD_TX_SKIP_CHECK){ 
        if(!chromaU && !chromaV){GetFtqBlock(src_mb,rblk,bw,bh,blkidx);}
        else{GetFtqBlockUV(src_mb,rblk,bw,bh,(chromaU ? 0 : 1));}
    }
    tmp_mv= VSICsta.SkipCenter[blkidx][refidx];
    *d1 = *d0 + GetCostXY(tmp_mv.x,tmp_mv.y,refidx);
    refid = RefID[refidx][blkidx];
    *d1 = *d1 + (GetCostRefID(refid)>>refCostScale);
}

/*****************************************************************************************************/
int MEforGen75::CheckSkipUnit( int t8x8 )
/*****************************************************************************************************/
{
    //I32        n=0;
    I32        d=0;
    int skip_cnt = 0;
    int centers = 2;
    U8 skipCenterMask = Vsta.SkipCenterEnables;
    memset(&SkipDist,0,sizeof(SkipDist));
    memset(&SkipDistAddMV8x8,0,sizeof(SkipDistAddMV8x8));

    if (Vsta.SrcType&0x3)    // no max mv check for reduced macroblock case
    {
        Vsta.MaxMvs = 32;
    }

    if(t8x8)
    {
        centers = 8;
    }
    for(int i = 0; i < centers; i+=2)
    {
        switch((skipCenterMask>>i) & 0x3)
        {
        case 1:
            skip_cnt++;
            break;
        case 2:
            if((Vsta.VmeModes & 0x7) == VM_MODE_DDD) // dual ref
            {
                skip_cnt++;
            }
            break;
        case 3:
            skip_cnt++;
            if((Vsta.VmeModes & 0x7) == VM_MODE_DDD)
            {
                skip_cnt++;
            }
            break;
        default:
            break;
        }
    }

    Vout->MbSubPredMode = 0x0;
    for(int i = 0; i < centers; i+=2)    Vout->MbSubPredMode |= ((((skipCenterMask>>i)&0x3)-1)<<i);

    if (Vsta.SadType&BLOCK_BASED_SKIP)
    {
        bSkipSbCheckMode = (Vsta.VmeFlags&VF_T8X8_FOR_INTER) ? SKIP8X8 : SKIP4X4;
    }
    else
    {
        bSkipSbCheckMode = SKIP16X16; //default A-step
    }

    I32 tempDist;
    if(!t8x8) //16x16 
    {
        IsBottom[0] = false;
        IsBottom[1] = false;
        if(Vsta.SrcType&ST_REF_FIELD)
        {
            IsField = true;    
            if(Vsta.BlockRefId[0]&0x01)
                IsBottom[0] = true;
            if(Vsta.BlockRefId[0]&0x10)
                IsBottom[1] = true;
        }

        SkipMask = 0x001F;
        int x_size = 16, y_size = 16;
        if(Vsta.SrcType & 0x1) y_size >>= 1;
        if(Vsta.SrcType & 0x2) x_size >>= 1;
        switch(skipCenterMask&0x3){
            case 0: DistInter = MBMAXDIST;    break; // Illegal case
            case 1:
                if(!(Vsta.SadType&INTER_CHROMA_MODE))
                    CheckUnidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],VSICsta.SkipCenter[0][0],0,x_size,y_size,&DistInter,&SkipDistMb,0,false,false);
                else
                {
                    CheckUnidirectional(SrcMBU,Vsta.RefCb[0][RefID[0][0]],VSICsta.SkipCenter[0][0],0,x_size,y_size,&tempDist,&SkipDistMb,0,true,false);
                    CheckUnidirectional(SrcMBV,Vsta.RefCr[0][RefID[0][0]],VSICsta.SkipCenter[0][0],0,x_size,y_size,&DistInter,&SkipDistMb,0,false,true);
                    DistInter += tempDist;
                    SkipDistMb += tempDist;
                }
                Vout->InterMbType = TYPE_INTER_16X16_0;
                break; // Check L0
            case 2:
                if(!(Vsta.SadType&INTER_CHROMA_MODE))
                    CheckUnidirectional(SrcMB,Vsta.RefLuma[1][RefID[1][0]],VSICsta.SkipCenter[0][1],1,x_size,y_size,&DistInter,&SkipDistMb,0,false,false);
                else
                {
                    CheckUnidirectional(SrcMBU,Vsta.RefCb[1][RefID[1][0]],VSICsta.SkipCenter[0][1],1,x_size,y_size,&tempDist,&SkipDistMb,0,true,false);
                    CheckUnidirectional(SrcMBV,Vsta.RefCr[1][RefID[1][0]],VSICsta.SkipCenter[0][1],1,x_size,y_size,&DistInter,&SkipDistMb,0,false,true);
                    DistInter += tempDist;
                    SkipDistMb += tempDist;
                }
                Vout->InterMbType = TYPE_INTER_16X16_1;
                break; // Check L1
            case 3: 
                if(!(Vsta.SadType&INTER_CHROMA_MODE))
                    CheckSkipBidirectional(SrcMB,Vsta.RefLuma[0][RefID[0][0]],Vsta.RefLuma[1][RefID[1][0]],VSICsta.SkipCenter[0][0],VSICsta.SkipCenter[0][1],x_size,y_size,0,0,&DistInter,&SkipDistMb,0,false,false);
                else{
                    CheckSkipBidirectional(SrcMBU,Vsta.RefCb[0][RefID[0][0]],Vsta.RefCb[1][RefID[1][0]],VSICsta.SkipCenter[0][0],VSICsta.SkipCenter[0][1],x_size,y_size,0,0,&tempDist,&SkipDistMb,0,true,false);
                    CheckSkipBidirectional(SrcMBV,Vsta.RefCr[0][RefID[0][0]],Vsta.RefCr[1][RefID[1][0]],VSICsta.SkipCenter[0][0],VSICsta.SkipCenter[0][1],x_size,y_size,0,0,&DistInter,&SkipDistMb,0,false,true);
                    DistInter += tempDist;
                    SkipDistMb += tempDist;
                }
                Vout->InterMbType = TYPE_INTER_16X16_2;
                break; // Check Bi
        }
        if (Vsta.SadType&INTER_CHROMA_MODE) Vout->InterMbType = TYPE_INTER_8X8;
        if(!(Vsta.IntraFlags&S2D_ADD_LUTMV)) SkipDistMb = DistInter;
        SkipDistAddMV16x16 = SkipDistMb;
        SkipDistAddMV8x8[0] = SkipDistAddMV16x16;
        if(Vsta.IntraFlags&S2D_ADD_LUTMODE) 
        {
            int LutModeType = (Vsta.SadType&INTER_CHROMA_MODE) ? LUTMODE_INTER_8x8q : LUTMODE_INTER_16x16;
            if ((Vsta.SrcType&3) == 1) //16x8
            {
                LutModeType = LUTMODE_INTER_16x8;
            }
            if ( ((Vsta.SrcType&3) == 3) && !(Vsta.SadType&INTER_CHROMA_MODE) ) //8x8
            {
                LutModeType = LUTMODE_INTER_8x8q;
            }
            if(LutModeType == LUTMODE_INTER_16x8)
                SkipDistMb += (LutMode[LutModeType]/2);
            else
                SkipDistMb += LutMode[LutModeType];
        }
    }
    else
    {
        d = 0;
        U8 ptr_offset[4] = {0,8,128,136};
        U8 ptr_offsetUV[4] = {0,4,64,68};
        Vout->InterMbType = TYPE_INTER_8X8;
        SkipMask = 0x002F|((Vsta.SkipCenterEnables-0x55)<<8);
        DistInter = 0;
        SkipDistMb = 0;
        int loops = 4;
        if (!(Vsta.SadType&INTER_CHROMA_MODE))
        {
            if(Vsta.SrcType & 0x1) loops >>= 1;
            if(Vsta.SrcType & 0x2) loops >>= 1;
        }
        for(int b = 0; b < loops; b++)
        {
            if(Vsta.SrcType&ST_REF_FIELD)
            {
                IsField = true;    
                IsBottom[0] = false;
                IsBottom[1] = false;
                if(Vsta.BlockRefId[b]&0x01)
                    IsBottom[0] = true;
                if(Vsta.BlockRefId[b]&0x10)
                    IsBottom[1] = true;
            }

            I16PAIR    SkipCenter[2];

            SkipCenter[0]= VSICsta.SkipCenter[b][0];
            SkipCenter[1]= VSICsta.SkipCenter[b][1];

            if (b & 1)
            {
                SkipCenter[0].x += (Vsta.SadType&INTER_CHROMA_MODE) ? 16 : 32;
                SkipCenter[1].x += (Vsta.SadType&INTER_CHROMA_MODE) ? 16 : 32;
            }
            if (b & 2)
            {
                SkipCenter[0].y += (Vsta.SadType&INTER_CHROMA_MODE) ? 16 : 32;
                SkipCenter[1].y += (Vsta.SadType&INTER_CHROMA_MODE) ? 16 : 32;
            }
            switch((skipCenterMask>>(b<<1))&0x3){
                case 0: d = MBMAXDIST;    break; // Illegal case
                case 1:
                    if(!(Vsta.SadType&INTER_CHROMA_MODE))
                        CheckUnidirectional(SrcMB+ptr_offset[b],Vsta.RefLuma[0][RefID[0][b]],SkipCenter[0],0,8,8,&d,&SkipDist[b],b,false,false); 
                    else{
                        CheckUnidirectional(SrcMBU+ptr_offsetUV[b],Vsta.RefCb[0][RefID[0][b]],SkipCenter[0],0,4,4,&tempDist,&SkipDist[b],b,true,false);
                        CheckUnidirectional(SrcMBV+ptr_offsetUV[b],Vsta.RefCr[0][RefID[0][b]],SkipCenter[0],0,4,4,&d,&SkipDist[b],b,false,true);
                        d += tempDist;
                        SkipDist[b] += tempDist;
                    }
                    break; // Check L0
                case 2: 
                    if(!(Vsta.SadType&INTER_CHROMA_MODE))
                        CheckUnidirectional(SrcMB+ptr_offset[b],Vsta.RefLuma[1][RefID[1][b]],SkipCenter[1],1,8,8,&d,&SkipDist[b],b,false,false); 
                    else{
                        CheckUnidirectional(SrcMBU+ptr_offsetUV[b],Vsta.RefCb[1][RefID[1][b]],SkipCenter[1],1,4,4,&tempDist,&SkipDist[b],b,true,false);
                        CheckUnidirectional(SrcMBV+ptr_offsetUV[b],Vsta.RefCr[1][RefID[1][b]],SkipCenter[1],1,4,4,&d,&SkipDist[b],b,false,true);
                        d += tempDist;
                        SkipDist[b] += tempDist;
                    }
                    break; // Check L1
                case 3:    
                    if(!(Vsta.SadType&INTER_CHROMA_MODE))
                        CheckSkipBidirectional(SrcMB+ptr_offset[b],Vsta.RefLuma[0][RefID[0][b]],Vsta.RefLuma[1][RefID[1][b]], SkipCenter[0], SkipCenter[1],8,8,0,0,&d,&SkipDist[b],b,false,false); 
                    else{
                        CheckSkipBidirectional(SrcMBU+ptr_offsetUV[b],Vsta.RefCb[0][RefID[0][b]],Vsta.RefCb[1][RefID[1][b]],SkipCenter[0],SkipCenter[1],4,4,0,0,&tempDist,&SkipDist[b],b,true,false);
                        CheckSkipBidirectional(SrcMBV+ptr_offsetUV[b],Vsta.RefCr[0][RefID[0][b]],Vsta.RefCr[1][RefID[1][b]],SkipCenter[0],SkipCenter[1],4,4,0,0,&d,&SkipDist[b],b,false,true);
                        d += tempDist;
                        SkipDist[b] += tempDist;
                    }
                    break; // Check Bi
            }
            if(!(Vsta.IntraFlags&S2D_ADD_LUTMV)) SkipDist[b] = d;
            SkipDistAddMV8x8[b] = SkipDist[b];
            DistInter += d;        
            if((Vsta.IntraFlags&S2D_ADD_LUTMODE) && !(Vsta.SadType&INTER_CHROMA_MODE))  SkipDist[b] += LutMode[LUTMODE_INTER_8x8q];
            SkipDistMb += SkipDist[b];
            // add the 4x4cost for chroma4mvp ONLY ONCE
            if ((Vsta.IntraFlags&S2D_ADD_LUTMODE) && (Vsta.SadType&INTER_CHROMA_MODE) && (b==0)) SkipDistMb += LutMode[LUTMODE_INTER_4x4q];
        }

        if (!(Vsta.SadType&INTER_CHROMA_MODE))
        {
            if(Vsta.SrcType & 0x1){SkipDist[2] = SkipDist[0]; SkipDist[3] = SkipDist[1];}
            if(Vsta.SrcType & 0x2){SkipDist[1] = SkipDist[0]; SkipDist[3] = SkipDist[2];}
        }
    }
    
    OutSkipDist = DistInter;
    if(OutSkipDist>MBMAXDIST) OutSkipDist = MBMAXDIST;

    //if(Vsta.IntraFlags&S2D_ADD_LUTMODE)  SkipDistMb += LutMode[LUTMODE_INTER_16x16];
    if(DistInter>MBMAXDIST) DistInter = MBMAXDIST;
    if(SkipDistMb>MBMAXDIST) SkipDistMb = MBMAXDIST;
    bSkipSbCheckMode = 0;
    return 1;
}

/*****************************************************************************************************/
//void MEforGen75::Direct8x8Replacement(  )
/*****************************************************************************************************/
/*{
    int m, n, d, p;
    d = 0;
    m = MajorMode[0];
    p = CheckBidirValid(MajorMode[0], 0, false);
    m -= 0x5500;
    n = 0;

    U8 skip_enable = 0;

    switch(m&3){
        case 0: d = Dist[0][B8X8] + LutMode[LUTMODE_INTER_8x8q]; n = !(m&0x0200); break;
        case 1:    d = Dist[0][B8X4] + Dist[0][B8X4+1] + LutMode[LUTMODE_INTER_8x4q]; break; 
        case 2:    d = Dist[0][B4X8] + Dist[0][B4X8+1] + LutMode[LUTMODE_INTER_4x8q]; break; 
        case 3:    d = Dist[0][B4X4] + Dist[0][B4X4+1] + Dist[0][B4X4+2] + Dist[0][B4X4+3] + LutMode[LUTMODE_INTER_4x4q]; break; 
    }
    if((((m&0x0300) || (m&0x0100)) && (!L0_BWD_Penalty)) || (~m&0x0100 && L0_BWD_Penalty))
        d += LutMode[LUTMODE_INTER_BWD];
    d = (d > 0xfff) ? 0xfff : d;
    if(( SkipDist[0]<=d) && (p || (!n || !(SkipMask&0x0200)))){
        m = (m&0xFCFC)|(SkipMask&0x0300);
        skip_enable = (Vsta.SkipCenterEnables)&3;
        Best[0][B8X8] = (skip_enable&1)?VSICsta.SkipCenter[0][0]:nullMV;
        Best[1][B8X8] = (skip_enable&2)?VSICsta.SkipCenter[0][1]:nullMV;
        MajorDist[0] -= d - SkipDist[0];
        bDirectReplaced = true;
        if(Vsta.IntraFlags&S2D_ADD_LUTMODE)  
        {
            Dist[0][B8X8] = SkipDist[0] - LutMode[LUTMODE_INTER_8x8q];
        }
        else
        {
            Dist[0][B8X8] = SkipDist[0];
        }
    }
    else SkipMask &= 0xFFFE;
    n = 0;
    switch(m&0x0C){
        case 0x00: d = Dist[0][B8X8+1] + LutMode[LUTMODE_INTER_8x8q]; n = !(m&0x0800); break;
        case 0x04: d = Dist[0][B8X4+2] + Dist[0][B8X4+3] + LutMode[LUTMODE_INTER_8x4q]; break; 
        case 0x08: d = Dist[0][B4X8+2] + Dist[0][B4X8+3] + LutMode[LUTMODE_INTER_4x8q]; break; 
        case 0x0C: d = Dist[0][B4X4+4] + Dist[0][B4X4+5] + Dist[0][B4X4+6] + Dist[0][B4X4+7] + LutMode[LUTMODE_INTER_4x4q]; break; 
    }
    if((m&0x0C00 && !L0_BWD_Penalty) || (m&0x0400 && !L0_BWD_Penalty) || (~m&0x0400 && L0_BWD_Penalty))
        d += LutMode[LUTMODE_INTER_BWD];
    d = (d > 0xfff) ? 0xfff : d;
    if(( SkipDist[1]<=d) && (p || (!n || !(SkipMask&0x0800)))){
        m = (m&0xF3F3)|(SkipMask&0x0C00);
        skip_enable = (Vsta.SkipCenterEnables>>2)&3;
        Best[0][B8X8+1] = (skip_enable&1)?VSICsta.SkipCenter[1][0]:nullMV;
        Best[1][B8X8+1] = (skip_enable&2)?VSICsta.SkipCenter[1][1]:nullMV;
        MajorDist[0] -= d - SkipDist[1];
        bDirectReplaced = true;
        if(Vsta.IntraFlags&S2D_ADD_LUTMODE)  
        {
            Dist[0][B8X8+1] = SkipDist[1] - LutMode[LUTMODE_INTER_8x8q];
        }
        else
        {
            Dist[0][B8X8+1] = SkipDist[1];
        }
    }
    else SkipMask &= 0xFFFD;
    n = 0;
    switch(m&0x30){
        case 0x00: d = Dist[0][B8X8+2] + LutMode[LUTMODE_INTER_8x8q]; n = !(m&0x2000); break;
        case 0x10: d = Dist[0][B8X4+4] + Dist[0][B8X4+5] + LutMode[LUTMODE_INTER_8x4q]; break; 
        case 0x20: d = Dist[0][B4X8+4] + Dist[0][B4X8+5] + LutMode[LUTMODE_INTER_4x8q]; break; 
        case 0x30: d = Dist[0][B4X4+8] + Dist[0][B4X4+9] + Dist[0][B4X4+10] + Dist[0][B4X4+11] + LutMode[LUTMODE_INTER_4x4q]; break; 
    }
    if((m&0x3000 && !L0_BWD_Penalty) || (m&0x1000 && !L0_BWD_Penalty) || (~m&0x1000 && L0_BWD_Penalty))
        d += LutMode[LUTMODE_INTER_BWD];
    d = (d > 0xfff) ? 0xfff : d;
    if(( SkipDist[2]<=d) && (p || (!n || !(SkipMask&0x2000)))){
        m = (m&0xCFCF)|(SkipMask&0x3000);
        skip_enable = (Vsta.SkipCenterEnables>>4)&3;
        Best[0][B8X8+2] = (skip_enable&1)?VSICsta.SkipCenter[2][0]:nullMV;
        Best[1][B8X8+2] = (skip_enable&2)?VSICsta.SkipCenter[2][1]:nullMV;
        MajorDist[0] -= d - SkipDist[2];
        bDirectReplaced = true;
        if(Vsta.IntraFlags&S2D_ADD_LUTMODE)  
        {
            Dist[0][B8X8+2] = SkipDist[2] - LutMode[LUTMODE_INTER_8x8q];
        }
        else
        {
            Dist[0][B8X8+2] = SkipDist[2];
        }
    }
    else SkipMask &= 0xFFFB;
    n = 0;
    switch(m&0xC0){
        case 0x00: d = Dist[0][B8X8+3] + LutMode[LUTMODE_INTER_8x8q]; n = !(m&0x8000); break;
        case 0x40: d = Dist[0][B8X4+6] + Dist[0][B8X4+7] + LutMode[LUTMODE_INTER_8x4q]; break; 
        case 0x80: d = Dist[0][B4X8+6] + Dist[0][B4X8+7] + LutMode[LUTMODE_INTER_4x8q]; break; 
        case 0xC0: d = Dist[0][B4X4+12] + Dist[0][B4X4+13] + Dist[0][B4X4+14] + Dist[0][B4X4+15] + LutMode[LUTMODE_INTER_4x4q]; break; 
    }
    if((m&0xC000 && !L0_BWD_Penalty) || (m&0x4000 && !L0_BWD_Penalty) || (~m&0x4000 && L0_BWD_Penalty))
        d += LutMode[LUTMODE_INTER_BWD];
    d = (d > 0xfff) ? 0xfff : d;
    if(( SkipDist[3]<=d)&& (p || (!n || !(SkipMask&0x8000)))){
        m = (m&0x3F3F)|(SkipMask&0xC000);
        skip_enable = (Vsta.SkipCenterEnables>>6)&3;
        Best[0][B8X8+3] = (skip_enable&1)?VSICsta.SkipCenter[3][0]:nullMV;
        Best[1][B8X8+3] = (skip_enable&2)?VSICsta.SkipCenter[3][1]:nullMV;
        MajorDist[0] -= d - SkipDist[3];
        bDirectReplaced = true;
        if(Vsta.IntraFlags&S2D_ADD_LUTMODE)  
        {
            Dist[0][B8X8+3] = SkipDist[3] - LutMode[LUTMODE_INTER_8x8q];
        }
        else
        {
            Dist[0][B8X8+3] = SkipDist[3];
        }
    }
    else SkipMask &= 0xFFF7;

    MajorMode[0] = m + 0x5500;
}*/
