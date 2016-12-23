//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include "ipps.h"
#include "meforgen7_5.h"

#pragma warning( disable : 4244 )
#pragma warning( disable : 4100 )
#pragma warning( disable : 4702 )

#if (__HAAR_SAD_OPT == 1)
#include "emmintrin.h"
#endif

/*    Notice:
    When Bidirectional is off, the refinement performs only for the chosen direction.
    When Bidirectional is on, the refinement for both directions are required. However, 
    if the refinement gives a better result for not-chosen direction, should we change the mode?
    In the C-model here, such a change is performed.
*/
    

/*****************************************************************************************************/
void MEforGen75::FractionalMESearchUnit(U8 mode, U8 submbshape, U8 subpredmode, U8 precision, bool chroma)
/*****************************************************************************************************/
{
    int    i,j,x,y;
    U8 m = mode;
    U8 s = submbshape;
    U8 k = subpredmode;
    //bool bwd_penalty = false;
    //int additionalCost = 0;
    bool bidirValid = false;
    //bool checkPruning = (Vsta.BidirMask&B_PRUNING_ENABLE) ? true : false;
    bidirValid = CheckBidirValid();//m, checkPruning, true); 
    
    if(mode == MODE_INTER_16X16){ //16x16
        if(bidirValid){             
            SubpelRefineBlock(SrcMB, BHXH,16,16,0,0,0,precision,false,RefID[0][0]); 
            SubpelRefineBlock(SrcMB, BHXH,16,16,0,0,1,precision,false,RefID[1][0]); 
        }
        else{
            //k = (!(*mode&0x0100));
            k = subpredmode&0x1;
            SubpelRefineBlock(SrcMB, BHXH,16,16,0,0,k,precision,false,RefID[k][0]);
        }
        MajorDist[0] = Dist[subpredmode&0x1][BHXH] + LutMode[LUTMODE_INTER_16x16];
    }
    else if(mode == MODE_INTER_8X16){ // 8x16
        if(bidirValid){             
            
            SubpelRefineBlock(SrcMB, B8XH  ,8,16,0,0,0,precision,false,RefID[0][0]); 
            SubpelRefineBlock(SrcMB, B8XH  ,8,16,0,0,1,precision,false,RefID[1][0]); 
            SubpelRefineBlock(SrcMB, B8XH+1,8,16,8,0,0,precision,false,RefID[0][1]); 
            SubpelRefineBlock(SrcMB, B8XH+1,8,16,8,0,1,precision,false,RefID[1][1]);
        }
        else{
            //k = (!(*mode&0x0100));
            k = subpredmode&0x1;
            SubpelRefineBlock(SrcMB, B8XH,8,16,0,0,k,precision,false,RefID[k][0]); 
            //k = (!(*mode&0x0400));
            k = (subpredmode>>2)&0x1;
            SubpelRefineBlock(SrcMB, B8XH+1,8,16,8,0,k,precision,false,RefID[k][1]);
        }
        MajorDist[0] = Dist[(subpredmode>>0)&0x1][B8XH+0] + 
                       Dist[(subpredmode>>2)&0x1][B8XH+1] + LutMode[LUTMODE_INTER_8x16];
    }
    else if(mode == MODE_INTER_16X8){ // 16x8
        if(bidirValid){             
            SubpelRefineBlock(SrcMB, BHX8,16,8,0,0,0,precision,false,RefID[0][0]); 
            SubpelRefineBlock(SrcMB, BHX8,16,8,0,0,1,precision,false,RefID[1][0]);              
            if(!(Vsta.SrcType&0x1))
            {
                SubpelRefineBlock(SrcMB, BHX8+1,16,8,0,8,0,precision,false,RefID[0][1]); 
                SubpelRefineBlock(SrcMB, BHX8+1,16,8,0,8,1,precision,false,RefID[1][1]);
            }
        }
        else{
            //k = (!(*mode&0x0100));
            k = subpredmode&0x1;
            SubpelRefineBlock(SrcMB, BHX8,16,8,0,0,k,precision,false,RefID[k][0]);
            if(!(Vsta.SrcType&0x1))
            {
                //k = (!(*mode&0x1000));
                k = (subpredmode>>2)&0x1;
                SubpelRefineBlock(SrcMB, BHX8+1,16,8,0,8,k,precision,false,RefID[k][1]); 
            }
        }
        MajorDist[0] = Dist[(subpredmode>>0)&0x1][BHX8+0];
        if(!(Vsta.SrcType&0x1))
            MajorDist[0] += Dist[(subpredmode>>2)&0x1][BHX8+1];
        // 16x8 srcsize's modecost has already been doubled in setuplutcost
        MajorDist[0] += (LutMode[LUTMODE_INTER_16x8]>>(Vsta.SrcType&0x1));
    }
    else
    {
        // Minors with no change of directions
        //m = (*mode)&0xFF; 
        //s = (*mode)>>8;      
        m = submbshape;
        s = subpredmode;
        i = 0; //variable initialization
        I32 tempdist = 0;

        int loops = 4;
        if(Vsta.SrcType & 0x1){ loops >>= 1; }
        if(Vsta.SrcType & 0x2){ loops >>= 1; }

        for(k=0;k<4;k++){
            x = ((k&1)<<3);
            y = ((k&2)<<2);
            //i = ((s-1)&1);
            i = s&1;
            if (k>=loops)
            {
                break;
            }
            switch(m&3){
            case 0: // 8x8
                j = B8X8+k;
                if(bidirValid){
                    SubpelRefineBlock(SrcMB, j,8,8,x,y,0,precision,chroma,RefID[0][k]); 
                    SubpelRefineBlock(SrcMB, j,8,8,x,y,1,precision,chroma,RefID[1][k]); 
                }
                else{
                    SubpelRefineBlock(SrcMB, j,8,8,x,y,i,precision,chroma,RefID[i][k]); 
                }
                tempdist += Dist[i][j] + LutMode[LUTMODE_INTER_8x8q];
                break;
            case 1: // 8x4
                //only best directions are checked for minor shapes
                j = B8X4 + (k<<1);
                SubpelRefineBlock(SrcMB, j  ,8,4,x,y,i,precision,false,RefID[i][k]); 
                SubpelRefineBlock(SrcMB, j+1,8,4,x,y+4,i,precision,false,RefID[i][k]); 
                tempdist += (Dist[i][j+0] + Dist[i][j+1]) + LutMode[LUTMODE_INTER_8x4q];
                break;
            case 2: // 4x8
                //only best directions are checked for minor shapes
                j = B4X8 + (k<<1);
                SubpelRefineBlock(SrcMB, j  ,4,8,x,y,i,precision,false,RefID[i][k]); 
                SubpelRefineBlock(SrcMB, j+1,4,8,x+4,y,i,precision,false,RefID[i][k]); 
                tempdist += (Dist[i][j+0] + Dist[i][j+1]) + LutMode[LUTMODE_INTER_4x8q];
                break;
            case 3: // 4x4
                //only best directions are checked for minor shapes
                j = B4X4 + (k<<2);
                if(bidirValid && (Vsta.SadType&INTER_CHROMA_MODE)){
                    SubpelRefineBlock(SrcMB, j  ,4,4,x,y,0,precision,chroma,RefID[0][k]); 
                    SubpelRefineBlock(SrcMB, j  ,4,4,x,y,1,precision,chroma,RefID[1][k]); 
                    SubpelRefineBlock(SrcMB, j+1,4,4,x+4,y,0,precision,chroma,RefID[0][k+1]); 
                    SubpelRefineBlock(SrcMB, j+1,4,4,x+4,y,1,precision,chroma,RefID[1][k+1]); 
                    SubpelRefineBlock(SrcMB, j+2,4,4,x,y+4,0,precision,chroma,RefID[0][k+2]); 
                    SubpelRefineBlock(SrcMB, j+2,4,4,x,y+4,1,precision,chroma,RefID[1][k+2]); 
                    SubpelRefineBlock(SrcMB, j+3,4,4,x+4,y+4,0,precision,chroma,RefID[0][k+3]);
                    SubpelRefineBlock(SrcMB, j+3,4,4,x+4,y+4,1,precision,chroma,RefID[1][k+3]);
                }
                else
                {
                    if (Vsta.SadType&INTER_CHROMA_MODE)
                    {
                        //chroma 4x4 only 1 outerloop
                        SubpelRefineBlock(SrcMB, j  ,4,4,x,y,s&1,precision,chroma,RefID[s&1][k]); 
                        SubpelRefineBlock(SrcMB, j+1,4,4,x+4,y,(s>>2)&1,precision,chroma,RefID[(s>>2)&1][k+1]); 
                        SubpelRefineBlock(SrcMB, j+2,4,4,x,y+4,(s>>4)&1,precision,chroma,RefID[(s>>4)&1][k+2]); 
                        SubpelRefineBlock(SrcMB, j+3,4,4,x+4,y+4,(s>>6)&1,precision,chroma,RefID[(s>>6)&1][k+3]);
                        loops = 1; // JMHREFID double check via email this code is correct... should only do 1 loop
                    }
                    else
                    {
                        SubpelRefineBlock(SrcMB, j  ,4,4,x,y,i,precision,chroma,RefID[i][k]); 
                        SubpelRefineBlock(SrcMB, j+1,4,4,x+4,y,i,precision,chroma,RefID[i][k]); 
                        SubpelRefineBlock(SrcMB, j+2,4,4,x,y+4,i,precision,chroma,RefID[i][k]); 
                        SubpelRefineBlock(SrcMB, j+3,4,4,x+4,y+4,i,precision,chroma,RefID[i][k]);
                    }
                }
                  
                if (Vsta.SadType&INTER_CHROMA_MODE)
                {
                    tempdist += (Dist[s&1][j+0] + Dist[(s>>2)&1][j+1] + Dist[(s>>4)&1][j+2] + Dist[(s>>6)&1][j+3]) + 
                        LutMode[LUTMODE_INTER_4x4q];
                }
                else
                {
                    tempdist += (Dist[i][j+0] + Dist[i][j+1] + Dist[i][j+2] + Dist[i][j+3]) + 
                        LutMode[LUTMODE_INTER_4x4q];
                }
                break;
            }
            m>>=2;
            s>>=2;
        }
        MajorDist[0] = tempdist;// + (LutMode[LUTMODE_INTER_8x8q]<<2);
    }

    ReplicateSadMV();
}


void MEforGen75::ScaleDist(I32& meDist)
{
    if(meDist == MBINVALID_DIST)
    {
        return;
    }

    if(Vsta.SrcType&1)
    {
        meDist>>=1;
    }
    if(Vsta.SrcType&2)
    {
        meDist>>=1;
    }
}

/*****************************************************************************************************/
int MEforGen75::SubpelRefineBlock(U8   *qsrc, int dist_idx, int bw, int bh, int dx, int dy, int isb, U8 scale, bool chroma, int refid)
/*****************************************************************************************************/
{
    int        i, j;
    //U8      *ref = RefPix[isb];
    U8      *src = qsrc + dx + (dy<<4);
    U8      *srcU = SrcMBU + dx + (dy<<4);
    U8      *srcV = SrcMBV + dx + (dy<<4);
    I16        orig_x = Best[isb][dist_idx].x + (dx<<2);
    I16        orig_y = Best[isb][dist_idx].y + (dy<<2);
    I16        winner_x = Best[isb][dist_idx].x;
    I16        winner_y = Best[isb][dist_idx].y ;
    //short    qx = orig_x  - (Vsta.Ref[isb].x<<2) + (dx<<2);
    //short    qy = orig_y  - (Vsta.Ref[isb].y<<2) + (dy<<2);
    I32 d;
    int refidcost = GetCostRefID(refid); // can be either avc or linear
    if ((bw==8 && bh==4) || (bw==4 && bh==8)) refidcost >>= 1; // minor8x4/4x8
    if (bw==4 && bh==4) refidcost >>= 2; // minor4x4

    d = DISTMAX4X4; 

    U8      rblk[256];
#if (__HAAR_SAD_OPT == 1)
    bool    copied;
    U8        *src_shifted=0;
    int        src_pitch;
#endif

    IsBottom[0] = false;
    IsBottom[1] = false;
    if(Vsta.SrcType&ST_REF_FIELD)
    {
        IsField = true;    
        if(refid&0x01)
            IsBottom[isb] = true;
    }


    //// quarter pel is not enabled, do nothing
    //if(!half_pel && !(Vsta.VmeModes&(VM_QUARTER_PEL-VM_HALF_PEL)))
    //    return 0;
    
    //int scale = half_pel?2:1;
    for(j = -1*scale; j <= scale; j += scale)
    {
        for(i = -1*scale; i <= scale; i += scale)
         {
            if(i || j || (scale == 0))
            {
                I16PAIR tmp_mv;
                if(!chroma){
                    tmp_mv.x = (orig_x >> 2) - 2;
                    tmp_mv.y = (orig_y >> 2) - 2;
#if (__HAAR_SAD_OPT == 1)
                    LoadReference2(Vsta.RefLuma[isb][refid], SkipRef[isb], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[isb],copied,src_shifted,src_pitch);
#else
                    LoadReference(Vsta.RefLuma[isb][refid], SkipRef[isb], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[isb]);
#endif
                    tmp_mv.x = orig_x - (tmp_mv.x << 2);
                    tmp_mv.y = orig_y - (tmp_mv.y << 2);
#if (__HAAR_SAD_OPT == 1)
                    if (copied) {
                        GetReferenceBlock(rblk,SkipRef[isb],tmp_mv.x+i,tmp_mv.y+j,bw,bh);
                    } else {
                        GetReferenceBlock(rblk,src_shifted,tmp_mv.x+i,tmp_mv.y+j,bw,bh,src_pitch);
                    }
#else
                    GetReferenceBlock(rblk,SkipRef[isb],tmp_mv.x+i,tmp_mv.y+j,bw,bh);
#endif
                    d = GetSadBlock(src,rblk,bw,bh) + GetCostXY(winner_x+i,winner_y+j,isb) + refidcost;
                }
                else{
                    tmp_mv.x = (orig_x >> 2) - 2;
                    tmp_mv.y = (orig_y >> 2) - 2;
#if (__HAAR_SAD_OPT == 1)
                    LoadReference2(Vsta.RefCb[isb][refid], SkipRef[isb], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[isb],copied,src_shifted,src_pitch);
#else
                    LoadReference(Vsta.RefCb[isb][refid], SkipRef[isb], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[isb]);
#endif
                    tmp_mv.x = orig_x - (tmp_mv.x << 2);
                    tmp_mv.y = orig_y - (tmp_mv.y << 2);
#if (__HAAR_SAD_OPT == 1)
                    if (copied) {
                        GetReferenceBlock(rblk,SkipRef[isb],tmp_mv.x+i,tmp_mv.y+j,bw,bh);
                    } else {
                        GetReferenceBlock(rblk,src_shifted,tmp_mv.x+i,tmp_mv.y+j,bw,bh,src_pitch);
                    }
#else
                    GetReferenceBlock(rblk,SkipRef[isb],tmp_mv.x+i,tmp_mv.y+j,bw,bh);
#endif
                    d = GetSadBlock(srcU,rblk,bw,bh) + GetCostXY(winner_x+i,winner_y+j,isb) + refidcost;
            
                    tmp_mv.x = (orig_x >> 2) - 2;
                    tmp_mv.y = (orig_y >> 2) - 2;
#if (__HAAR_SAD_OPT == 1)
                    LoadReference2(Vsta.RefCr[isb][refid], SkipRef[isb], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[isb],copied,src_shifted,src_pitch);
#else
                    LoadReference(Vsta.RefCr[isb][refid], SkipRef[isb], SKIPWIDTH, SKIPWIDTH, tmp_mv, SKIPWIDTH, IsBottom[isb]);
#endif
                    tmp_mv.x = orig_x - (tmp_mv.x << 2);
                    tmp_mv.y = orig_y - (tmp_mv.y << 2);
#if (__HAAR_SAD_OPT == 1)
                    if (copied) {
                        GetReferenceBlock(rblk,SkipRef[isb],tmp_mv.x+i,tmp_mv.y+j,bw,bh);
                    } else {
                        GetReferenceBlock(rblk,src_shifted,tmp_mv.x+i,tmp_mv.y+j,bw,bh,src_pitch);
                    }
#else
                    GetReferenceBlock(rblk,SkipRef[isb],tmp_mv.x+i,tmp_mv.y+j,bw,bh);
#endif
                    d += GetSadBlock(srcV,rblk,bw,bh);
                }

                if(d<Dist[isb][dist_idx])
                { 
                    Dist[isb][dist_idx] = d; 
                    Best[isb][dist_idx].x = winner_x+i; 
                    Best[isb][dist_idx].y = winner_y+j;
                }
            }
            if(scale == 0) return 0;
        }
    }

    return 0;
}
    
/*****************************************************************************************************/
int MEforGen75::SubpelRefineBlockField(U8   *qsrc, I16PAIR *mvs, I32 *dist, int bw, int bh, int dx, int dy, int isb, bool half_pel)
/*****************************************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
    int        j, d;
    U8      *ref = (isb?RefPix[1]:RefPix[0]);
    U8      *src = qsrc + dx + (dy<<4);
    short    qx = mvs->x + (dx<<2);
    short    qy = mvs->y + (dy<<2);

    if(qx< 8) return (isb? BORDER_REF1_LEFT:BORDER_REF0_LEFT);
    if(qy<16) return (isb? BORDER_REF1_TOP:BORDER_REF0_TOP);
    if(qx>((Vsta.RefW-bw-2)<<2)) return (isb? BORDER_REF1_RIGHT:BORDER_REF0_RIGHT);
    if(qy>((Vsta.RefH-(bh<<1)-4)<<2)) return (isb? BORDER_REF1_BOTTOM:BORDER_REF0_BOTTOM);

    U8      rblk[256];

    j = 0x84;
    GetReferenceBlockField(rblk,ref,qx-2,qy-6,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x-2,mvs->y-6,isb);
    if(d<*dist){ *dist = d; j = 0x22; }
    GetReferenceBlockField(rblk,ref,qx,qy-6,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x,mvs->y-6,isb);
    if(d<*dist){ *dist = d; j = 0x24; }
    GetReferenceBlockField(rblk,ref,qx+2,qy-6,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x+2,mvs->y-6,isb);
    if(d<*dist){ *dist = d; j = 0x26; }
    
    GetReferenceBlockField(rblk,ref,qx-2,qy,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x-2,mvs->y,isb);
    if(d<*dist){ *dist = d; j = 0x82; }
    GetReferenceBlockField(rblk,ref,qx+2,qy,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x+2,mvs->y,isb);
    if(d<*dist){ *dist = d; j = 0x86; }

    GetReferenceBlockField(rblk,ref,qx-2,qy+2,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x-2,mvs->y+2,isb);
    if(d<*dist){ *dist = d; j = 0xA2; }
    GetReferenceBlockField(rblk,ref,qx,qy+2,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x,mvs->y+2,isb);
    if(d<*dist){ *dist = d; j = 0xA4; }
    GetReferenceBlockField(rblk,ref,qx+2,qy+2,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x+2,mvs->y+2,isb);
    if(d<*dist){ *dist = d; j = 0xA6; }

    mvs->x += (j&15)-4; mvs->y += (j>>4)-8;
    if(!(Vsta.VmeModes&(VM_QUARTER_PEL-VM_HALF_PEL))) return 0;

    qx = mvs->x + (dx<<2);
    qy = mvs->y + (dy<<2);
    
    j = 0x84;

    if(!(qy&2)){ // vertical integer location
        GetReferenceBlockField(rblk,ref,qx-1,qy-5,bw,bh);
        d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x-1,mvs->y-5,isb);
        if(d<*dist){ *dist = d; j = 0x33; }
        GetReferenceBlockField(rblk,ref,qx,qy-5,bw,bh);
        d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x,mvs->y-5,isb);
        if(d<*dist){ *dist = d; j = 0x34; }
        GetReferenceBlockField(rblk,ref,qx+1,qy-5,bw,bh);
        d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x+1,mvs->y-5,isb);
        if(d<*dist){ *dist = d; j = 0x35; }
    }
    else{        // vertical half-pel location
        GetReferenceBlockField(rblk,ref,qx-1,qy-1,bw,bh);
        d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x-1,mvs->y-1,isb);
        if(d<*dist){ *dist = d; j = 0x73; }
        GetReferenceBlockField(rblk,ref,qx,qy-1,bw,bh);
        d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x,mvs->y-1,isb);
        if(d<*dist){ *dist = d; j = 0x74; }
        GetReferenceBlockField(rblk,ref,qx+1,qy-1,bw,bh);
        d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x+1,mvs->y-1,isb);
        if(d<*dist){ *dist = d; j = 0x75; }
    }

    GetReferenceBlockField(rblk,ref,qx-1,qy,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x-1,mvs->y,isb);
    if(d<*dist){ *dist = d; j = 0x83; }
    GetReferenceBlockField(rblk,ref,qx+1,qy,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x+1,mvs->y,isb);
    if(d<*dist){ *dist = d; j = 0x85; }

    GetReferenceBlockField(rblk,ref,qx-1,qy+1,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x-1,mvs->y+1,isb);
    if(d<*dist){ *dist = d; j = 0x93; }
    GetReferenceBlockField(rblk,ref,qx,qy+1,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x,mvs->y+1,isb);
    if(d<*dist){ *dist = d; j = 0x94; }
    GetReferenceBlockField(rblk,ref,qx+1,qy+1,bw,bh);
    d = GetSadBlockField(src,rblk,bw,bh) + GetCostXY(mvs->x+1,mvs->y+1,isb);
    if(d<*dist){ *dist = d; j = 0x95; }

    mvs->x += (j&15)-4; mvs->y += (j>>4)-8;
    return 0;
    */
}    



#define  ABS(X)  (((X)<0)?-(X):(X))
/*****************************************************************************************************/
int  MEforGen75::GetSadBlock(U8 *src, U8 *ref, int bw, int bh, bool bChroma, bool VSelect)
/*****************************************************************************************************/
{
    int d = 0;
    int tmpSad[16] = {};
    int sbCount = 0;
    U8 BlkSubblkMapping[16] =  {0,1,4,5,2,3,6,7,8,9,12,13,10,11,14,15};

    for(int j=(bh>>2);j>0;j--){
        for(int i=0;i<bw;i+=4){
            tmpSad[sbCount] = GetSad4x4(src+i,ref+i,16);
            if(bSkipSbCheckMode==SKIP4X4) 
            {
                if (bChroma)
                {
                    // for chroma BBS, we need to buffer the U Sads, so that we can calculate SubBlockMaxSad based on U+V sad per block
                    int UVSubBlkSad = 0;

                    m_BBSChromaDistortion[(int) VSelect][sbCount] = tmpSad[sbCount];

                    if (VSelect)
                    {
                        UVSubBlkSad = m_BBSChromaDistortion[0][sbCount] + m_BBSChromaDistortion[1][sbCount];
                        if (UVSubBlkSad > SubBlockMaxSad)
                            SubBlockMaxSad = UVSubBlkSad;
                    }
                }
                else
                {
                    if (tmpSad[sbCount]>SubBlockMaxSad)
                        SubBlockMaxSad = tmpSad[sbCount];
                }
            }
            d += tmpSad[sbCount];
            sbCount++;
        }
        src += 64; ref += REFWINDOWWIDTH;
    }

    if(bSkipSbCheckMode==SKIP8X8)
    {
        if(sbCount == 4)
        {
            //luma 4MVP case or chroma 1mvp case
            int currBlockSad = tmpSad[0] + tmpSad[1] + tmpSad[2] + tmpSad[3];

            if (bChroma)
                SubBlockMaxSad += currBlockSad;
            else
            {
                if( currBlockSad > SubBlockMaxSad)
                {
                    SubBlockMaxSad = currBlockSad;
                }
            }
        }
        else
        {
            //1MVP case, chroma cases will not reach here
            char srcdiv = bChroma?16:(16*16)/(bw*bh);
            for(int i = 0; i < 16/srcdiv; i+=4)
            {
                int currBlockSad = tmpSad[BlkSubblkMapping[i]] + tmpSad[BlkSubblkMapping[i+1]] + tmpSad[BlkSubblkMapping[i+2]] + tmpSad[BlkSubblkMapping[i+3]];
                if(currBlockSad > SubBlockMaxSad)
                {
                   SubBlockMaxSad = currBlockSad;
                }
            }
        }
    }
    else if (bSkipSbCheckMode==SKIP16X16)
    {
        SubBlockMaxSad += d;
    }

    return d;
}

/*****************************************************************************************************/
int  MEforGen75::GetFtqBlock(U8 *src, U8 *ref, int bw, int bh, U8 blkidx)
/*****************************************************************************************************/
{
    U16 tmpNZC[16] = {};
    U16 tmpMag[16] = {};
    U32 tmpRet = 0;
    int sbCount = 0;
    U8 BlkSubblkMapping[16] =  {0,1,4,5,2,3,6,7,8,9,12,13,10,11,14,15};

    for(int j=(bh>>2);j>0;j--){
        for(int i=0;i<bw;i+=4){
            tmpRet = GetFtq4x4(src+i,ref+i,16);
            tmpNZC[sbCount] = (tmpRet>>16);
            tmpMag[sbCount] = (tmpRet&0x0000FFFF);
            sbCount++;
        }
        src += 64; ref += REFWINDOWWIDTH;
    }

    if(sbCount == 4)
    {
        //4MVP case
        Vout->LumaNonZeroCoeff[blkidx]    = tmpNZC[0] + tmpNZC[1] + tmpNZC[2] + tmpNZC[3];
        Vout->LumaCoeffSum[blkidx]        = tmpMag[0] + tmpMag[1] + tmpMag[2] + tmpMag[3];
    }
    else
    {
        //1MVP case
        for(int i = 0; i < 16; i+=4)
        {
            Vout->LumaNonZeroCoeff[i/4]    = tmpNZC[BlkSubblkMapping[i]] + tmpNZC[BlkSubblkMapping[i+1]] + tmpNZC[BlkSubblkMapping[i+2]] + tmpNZC[BlkSubblkMapping[i+3]];
            Vout->LumaCoeffSum[i/4]        = tmpMag[BlkSubblkMapping[i]] + tmpMag[BlkSubblkMapping[i+1]] + tmpMag[BlkSubblkMapping[i+2]] + tmpMag[BlkSubblkMapping[i+3]];
        }

    }

    // zero out last 2 Coefficients 16x8 AND last 3 for 8x8 source types
    if ( ((Vsta.SrcType&3) == 3) && !(Vsta.SadType&INTER_CHROMA_MODE) ) // 8x8
    {
        for(int i=1;i<4; i++)
        {
            Vout->LumaNonZeroCoeff[i]    = 0;
            Vout->LumaCoeffSum[i]        = 0;
        }
    }

    if ((Vsta.SrcType&3) == 1) //16x8
    {
        for(int i=2;i<4; i++)
        {
            Vout->LumaNonZeroCoeff[i]    = 0;
            Vout->LumaCoeffSum[i]        = 0;
        }
    }

    return 0;
}

/*****************************************************************************************************/
int  MEforGen75::GetFtqBlockUV(U8 *src, U8 *ref, int bw, int bh, U8 blkidx)
/*****************************************************************************************************/
{
    U16 tmpNZC[16] = {};
    U16 tmpMag[16] = {};
    U32 tmpRet = 0;
    int sbCount = 0;

    for(int j=(bh>>2);j>0;j--){
        for(int i=0;i<bw;i+=4){
            tmpRet = GetFtq4x4(src+i,ref+i,16);
            tmpNZC[sbCount] = (tmpRet>>16);
            tmpMag[sbCount] = (tmpRet&0x0000FFFF);
            sbCount++;
        }
        src += 64; ref += REFWINDOWWIDTH;
    }

    if(sbCount == 1)
    {
        //4MVP case
        Vout->ChromaNonZeroCoeff[blkidx] += tmpNZC[0];
        Vout->ChromaCoeffSum[blkidx] += tmpMag[0];
    }
    else // sbCount == 4
    {
        // 1MVP case
        Vout->ChromaNonZeroCoeff[blkidx]    = tmpNZC[0] + tmpNZC[1] + tmpNZC[2] + tmpNZC[3];
        Vout->ChromaCoeffSum[blkidx]        = tmpMag[0] + tmpMag[1] + tmpMag[2] + tmpMag[3];
    }

    return 0;
}

/*****************************************************************************************************/
int  MEforGen75::GetSadBlockField(U8   *src, U8   *ref, int bw, int bh)
/*****************************************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
    int d = 0;
    for(int j=(bh>>2);j>0;j--){
        for(int i=0;i<bw;i+=4){
            d += GetSad4x4(ref+i,src+i,32);
        }
        src += 128; ref += 64;
    }

    //for(int j=bh;j>0;j--){
    //    for(int i=0;i<bw;i++) d += ABS(ref[i]-src[i]);
    //    ref += 16; src += 32;
    //}

    return d;
    */
}

/*****************************************************************************************************/
int MEforGen75::GetFractionalPixelRow(U8   *bb, U8   *ref, short qx, short bw, short *mx)
/*****************************************************************************************************/
{
    int    k;
    int    w = bw;
    if(qx<1){
        if(qx<0){
            k = ((ref[0]*(mx[0]+mx[1]+mx[2])+ref[1]*mx[3]+8)>>4);
            *(bb++) = (k<0)?0:((k>255)?255:k);
            w--;
        }
        k = ((ref[0]*(mx[0]+mx[1])+ref[1]*mx[2]+ref[2]*mx[3]+8)>>4);
        *(bb++) = (k<0)?0:((k>255)?255:k);
        w--; 
    }
    else ref += qx-1;
    while((w--)>0){
        k = ((ref[0]*mx[0]+ref[1]*mx[1]+ref[2]*mx[2]+ref[3]*mx[3]+8)>>4);
        *(bb++) = (k<0)?0:((k>255)?255:k);
        ref++;
    }
    if(qx+bw>=Vsta.RefW){
        if(qx+bw>Vsta.RefW){
            --ref;
            k = ((ref[0]*mx[0]+ref[1]*(mx[1]+mx[2]+mx[3])+8)>>4);
            *(--bb) = (k<0)?0:((k>255)?255:k);
        }
        --ref;
        k = ((ref[0]*mx[0]+ref[1]*mx[1]+ref[2]*(mx[2]+mx[3])+8)>>4);
        *(--bb) = (k<0)?0:((k>255)?255:k);
    }
    return 0;
}

/*****************************************************************************************************/
int MEforGen75::CheckAndGetReferenceBlock(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh)
/*****************************************************************************************************/
{
    bool bCond1 = (qx<0);
    bool bCond2 = (qy<0);
    int Cond3 = (qx>>2)+bw;
    int Cond4 = (qy>>2)+bh;

    if(bCond1||bCond2||((Cond3)>Vsta.RefW)||((Cond4)>Vsta.RefH))
        return WRN_MV_OUTREF; // out off range MVs
    if(qx<4 && qx!=0)
        return WRN_MV_OUTREF; // out off range MVs for FME
    if(qy<4 && qy!=0)
        return WRN_MV_OUTREF; // out off range MVs for FME
    int    x = ((Vsta.RefW-bw)<<2);
    int    y = ((Vsta.RefH-bh)<<2);
    if(qx>x-4 && qx!=x)
        return WRN_MV_OUTREF; // out off range MVs for FME
    if(qy>y-4 && qy!=y)
        return WRN_MV_OUTREF; // out off range MVs for FME
//     if(qx==255||qy==255) return 0x222bad; // out off range MVs

    return GetReferenceBlock(blk, ref, qx, qy, bw, bh);
}

/*****************************************************************************************************/
int MEforGen75::GetReferenceBlock(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh
#if (__HAAR_SAD_OPT == 1)
                                  , int src_pitch
#endif
                                  )
/*****************************************************************************************************/
{
    if(Vsta.MvFlags&INTER_BILINEAR) 
#if (__HAAR_SAD_OPT == 1)
        return GetReferenceBlock2Tap(blk,ref,qx,qy,bw,bh,src_pitch);
#else
        return GetReferenceBlock2Tap(blk,ref,qx,qy,bw,bh);
#endif
//    else if(Vsta.SWflags&INTER_AVC6TAP) 
//        return GetReferenceBlock6Tap(blk,ref,qx,qy,bw,bh);
#if (__HAAR_SAD_OPT == 1)
    return GetReferenceBlock4Tap(blk,ref,qx,qy,bw,bh,src_pitch);
#else
    return GetReferenceBlock4Tap(blk,ref,qx,qy,bw,bh);
#endif
}

#if (__HAAR_SAD_OPT == 1)
// Vertical Interpolation
inline void MEforGen75::VertInterp4Tap (U8 *input0, U8 *output, int pitch, int bh, int bw, int pitch_out)
{
    VME_ALIGN_DECL(16) U16 Coeff[8] = {5,5,5,5,5,5,5,5};
    __m128i coeff_reg = _mm_load_si128 ((__m128i const*)Coeff);
    
    U8 *input;
    int x=0;
    while (x<bw) {
        
        input = input0;

        // Initial load
        __m128i row0 = _mm_loadl_epi64((__m128i const*)input); input+=pitch;
        __m128i row1 = _mm_loadl_epi64((__m128i const*)input); input+=pitch;
        __m128i row2 = _mm_loadl_epi64((__m128i const*)input); input+=pitch;
        __m128i row3 = row2;    
    
        // Create an all-zero 128-bit 
        __m128i zero_reg = _mm_setzero_si128();

        // Unpack from 8 bits to 16 bits
        row0 = _mm_unpacklo_epi8(row0, zero_reg);
        row1 = _mm_unpacklo_epi8(row1, zero_reg);
        row2 = _mm_unpacklo_epi8(row2, zero_reg);

        // Multiply by 5
        __m128i row1b = _mm_mullo_epi16 (row1, coeff_reg);
        __m128i row2b = row1b;
        __m128i result = row1b;
        __m128i tmp0;
        int y;
    
        for (y = 0; y < bh; y++) {
            switch (y&3) {
                case 0:                
                    row3 = _mm_loadl_epi64((__m128i const*)input);
                    row3 = _mm_unpacklo_epi8(row3, zero_reg);
                    row2b = _mm_mullo_epi16 (row2, coeff_reg);
                    result = _mm_add_epi16(row1b, row2b);
                    result = _mm_sub_epi16(result, row0);
                    result = _mm_sub_epi16(result, row3);                
                    break;
                case 1:
                    row0 = _mm_loadl_epi64((__m128i const*)input);
                    row0 = _mm_unpacklo_epi8(row0, zero_reg);
                    row1b = _mm_mullo_epi16 (row3, coeff_reg);
                    result = _mm_add_epi16(row1b, row2b);
                    result = _mm_sub_epi16(result, row1);
                    result = _mm_sub_epi16(result, row0);
                    break;
                case 2:
                    row1 = _mm_loadl_epi64((__m128i const*)input);
                    row1 = _mm_unpacklo_epi8(row1, zero_reg);
                    row2b = _mm_mullo_epi16 (row0, coeff_reg);
                    result = _mm_add_epi16(row1b, row2b);
                    result = _mm_sub_epi16(result, row2);
                    result = _mm_sub_epi16(result, row1);
                    break;
                case 3:
                    row2 = _mm_loadl_epi64((__m128i const*)input);
                    row2 = _mm_unpacklo_epi8(row2, zero_reg);
                    row1b = _mm_mullo_epi16 (row1, coeff_reg);
                    result = _mm_add_epi16(row1b, row2b);
                    result = _mm_sub_epi16(result, row3);
                    result = _mm_sub_epi16(result, row2);
                    break;
            }

            result = _mm_srai_epi16(result, 3);
            tmp0 = _mm_cmpgt_epi16(result, zero_reg);
            result = _mm_and_si128 (result, tmp0);
            input+=pitch;

            // Packing (with clipping to 255)
            result = _mm_packus_epi16 (result, zero_reg);
            
            // Store the result
            _mm_storel_epi64((__m128i*)&output[(y<<pitch_out)+x], result);

        }
        x += 8;
        input0 += 8;
    }

}

inline void MEforGen75::HorInterp4Tap (U8 *input0, U8 *output, int pitch_in, int bh, int bw, int pitch_out)
{
    
    // Create an all-zero 128-bit 
    __m128i zero_reg = _mm_setzero_si128();

    VME_ALIGN_DECL(16) U16 Coeff[8] = {5,5,5,5,5,5,5,5};
    __m128i coeff_reg = _mm_load_si128 ((__m128i const*)Coeff);
    
    U8 *input;
    for (int y = 0; y < bh; y++) {
        input = input0;

        int x=0;
        while (x<bw) {
    
            // Load
            __m128i row0 = _mm_loadl_epi64((__m128i const*)input);
        
            // Unpack from 8 bits to 16 bits
            row0 = _mm_unpacklo_epi8(row0, zero_reg);
            
            // Multiply by 5
            __m128i row0b = _mm_mullo_epi16 (row0, coeff_reg);

            // Shift
            __m128i row1 = _mm_srli_si128 (row0b, 2);
            __m128i row2 = _mm_srli_si128 (row0b, 4);
            __m128i row3 = _mm_srli_si128 (row0, 6);

            __m128i result = _mm_add_epi16(row1, row2);
            result = _mm_sub_epi16(result, row0);
            result = _mm_sub_epi16(result, row3);

            result = _mm_srai_epi16(result, 3);
            __m128i tmp0 = _mm_cmpgt_epi16(result, zero_reg);
            result = _mm_and_si128 (result, tmp0);        

            // Packing (with clipping to 255)
            result = _mm_packus_epi16 (result, zero_reg);
                
            // Store the result
            _mm_storel_epi64((__m128i*)&output[(y<<pitch_out)+x], result);

            x = x+5;
            input = input + 5;
        }

        input0+=pitch_in;
    }

}

// Vertical Interpolation
inline void MEforGen75::FinalInterp4Tap (U8 *input0, U8 *input1, U8 *output, int bh, int bw)
{
    // Create an all-zero 128-bit 
    __m128i zero_reg = _mm_setzero_si128();
    
    U8 *tmp0, *tmp1;
    int i,j;
    for(j=0;j<bh;j++){
        i = 0;        
        tmp0 = input0;
        tmp1 = input1;
        while (i<bw) {
            // Load
            __m128i row0 = _mm_loadl_epi64((__m128i const*)tmp0);
            __m128i row1 = _mm_loadl_epi64((__m128i const*)tmp1);
            
            // Unpack from 8 bits to 16 bits
            row0 = _mm_unpacklo_epi8(row0, zero_reg);
            row1 = _mm_unpacklo_epi8(row1, zero_reg);
            
            __m128i result = _mm_add_epi16(row0, row1);
            result = _mm_srai_epi16(result, 1);

            // Packing (with clipping to 255)
            result = _mm_packus_epi16 (result, zero_reg);
                
            // Store the result
            _mm_storel_epi64((__m128i*)&output[(j<<4)+i], result);

            i+=8;
            tmp0+=8;
            tmp1+=8;
        }
        input0 += 16;
        input1 += 16;
    }
}
#endif

/*****************************************************************************************************/
int MEforGen75::GetReferenceBlock4Tap(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh
#if (__HAAR_SAD_OPT == 1)
                                      , int src_pitch
#endif
                                      )
/*****************************************************************************************************/
{
#if 1 // sync with HW
#if (__HAAR_SAD_OPT == 0)
    int        i, j, k;
#else
    int        /*i,*/ j/*, k*/;
#endif
    int        x = (qx>>2);
    int        y = (qy>>2);
#if (__HAAR_SAD_OPT == 0)
    U8      tmp0[256],tmp1[512];
#else
    U8      tmp0[256+8],tmp1[512];
#endif
#if (__HAAR_SAD_OPT == 0)
    U8        *pp,*p0=NULL,*p1=NULL;
#else
    U8        /**pp,*/*p0=NULL,*p1=NULL;
#endif
    int        m0=0,m1=0;
    int    width;

#if (__HAAR_SAD_OPT == 1)    
    if (src_pitch != 0) {
        width = src_pitch;
    } else 
#endif
    if (ref == SkipRef[0] || ref == SkipRef[1])
    {
        // skip surface
        width = SKIPWIDTH;
    }
    else
    {
        // reference surface
        width = REFWINDOWWIDTH;
    }

    switch(qy&3){
        case 0:
        switch(qx&3){
            case 0: m0 = 0; m1 = 8; p1 = p0 = ref + y*width + x; break;
            case 1: m0 = 0; m1 = 1; p1 = p0 = ref + y*width + x; break;
            case 2: m0 = 1; m1 = 8; p1 = p0 = ref + y*width + x; break;
            case 3: m0 = 1; m1 = 0; p1 =(p0 = ref + y*width + x) + 1; break;
        }
        break;
        case 1:
        switch(qx&3){
            case 0: m0 = 0; m1 = 2; p1 = p0 = ref + y*width + x; break;
            case 1: m0 = 1; m1 = 2; p1 = p0 = ref + y*width + x; break;
            case 2: m0 = 3; m1 = 1; p1 = p0 = ref + y*width + x; break;
            case 3: m0 = 1; m1 = 2; p1 =(p0 = ref + y*width + x) + 1; break;
        }
        break;
        case 2:
        switch(qx&3){
            case 0: m0 = 2; m1 = 8; p1 = p0 = ref + y*width + x; break;
            case 1: m0 = 3; m1 = 2; p1 = p0 = ref + y*width + x; break;
            case 2: m0 = 3; m1 = 8; p1 = p0 = ref + y*width + x; break;
            case 3: m0 = 3; m1 = 2; p1 =(p0 = ref + y*width + x) + 1; break;
        }
        break;
        case 3:
        switch(qx&3){
            case 0: m0 = 2; m1 = 0; p1 =(p0 = ref + y*width + x) + width; break;
            case 1: m0 = 2; m1 = 1; p1 =(p0 = ref + y*width + x) + width; break;
            case 2: m0 = 3; m1 = 1; p1 =(p0 = ref + y*width + x) + width; break;
            case 3: m0 = 2; m1 = 1; p1 =(p0 = ref + y*width + x + 1) + width - 1; break;
        }
        break;
    }
    switch(m0){
        case 0:
            for(j=0;j<bh;j++) MFX_INTERNAL_CPY(&tmp0[j<<4],&p0[j*width],bw);
            break;
        case 1:
#if (__HAAR_SAD_OPT == 0)
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    pp = &p0[(j*width)+i];
                    k = (((pp[0]+pp[1])*5-pp[-1]-pp[2])>>3);
                    tmp0[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
#else
            // Horizontal interpolation
            HorInterp4Tap (p0-1, tmp0, width, bh, bw, 4);
#endif
            break;
        case 2:
#if (__HAAR_SAD_OPT == 0)
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    pp = &p0[(j*width)+i];
                    k = (((pp[0]+pp[width])*5-pp[-width]-pp[width<<1])>>3);
                    tmp0[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
#else
            // Vertical Interpolation
            VertInterp4Tap (p0-width, tmp0, width, bh, bw, 4);
#endif
            break;
        case 3:
#if (__HAAR_SAD_OPT == 0)
            for(j=0;j<bh;j++){
                for(i=0;i<bw+3;i++){
                    pp = &p0[(j*width)+i-1];
                    k = (((pp[0]+pp[width])*5-pp[-width]-pp[width<<1])>>3);
                    tmp1[(j<<5)+i] = (k<0)?0:((k>255)?255:k);
                }
                for(i=0;i<bw;i++){
                    pp = &tmp1[(j<<5)+i+1];
                    k = (((pp[0]+pp[1])*5-pp[-1]-pp[2])>>3);
                    tmp0[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
#else            
            // Vertical Interpolation
            VertInterp4Tap (p0-width-1, tmp1, width, bh, bw+3, 5);
            // Horizontal Interpolation
            HorInterp4Tap (tmp1, tmp0, 32, bh, bw, 4);
#endif            
            break;
    }
    switch(m1){
        case 0:
            for(j=0;j<bh;j++) MFX_INTERNAL_CPY(&tmp1[j<<4],&p1[j*width],bw);
            break;
        case 1:
#if (__HAAR_SAD_OPT == 0)
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    pp = &p1[(j*width)+i];
                    k = (((pp[0]+pp[1])*5-pp[-1]-pp[2])>>3);
                    tmp1[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
#else
            // Horizontal interpolation
            HorInterp4Tap (p1-1, tmp1, width, bh, bw, 4);
#endif
            break;
        case 2:
#if (__HAAR_SAD_OPT == 0)
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    pp = &p1[(j*width)+i];
                    k = (((pp[0]+pp[width])*5-pp[-width]-pp[width<<1])>>3);
                    tmp1[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
#else
            // Vertical Interpolation
            VertInterp4Tap (p1-width, tmp1, width, bh, bw, 4);
#endif
            break;
        case 8:
            for(j=0;j<bh;j++){
                MFX_INTERNAL_CPY(&blk[j<<4],&tmp0[j<<4], bw);
            }
            return 0;
    }
#if (__HAAR_SAD_OPT == 0)
    for(j=0;j<bh;j++){
        for(i=0;i<bw;i++){
            blk[(j<<4)+i] = ((tmp0[(j<<4)+i]+tmp1[(j<<4)+i])>>1);
        }
    }
#else
    FinalInterp4Tap (tmp0, tmp1, blk, bh, bw);
#endif
    return 0;

#else
    short    mul[4][4] = {{0,16,0,0},{-1,13,5,-1},{-2,10,10,-2},{-1,5,13,-1}};
    int        i, j, k;
    int        x = (qx>>2);
    int        y = (qy>>2)+2;
    short    *mx = mul[qx&3];
    short    *my = mul[qy&3];
    U8      *bb = blk;
    int        y1 = y + bh;
    int        y2 = (y1>Vsta.RefH)?(y1-Vsta.RefH):0;
    U8      tmp[4][16];
    y1 -= y2;

    if(y<3)    GetFractionalPixelRow(&tmp[(y+1)&3][0],ref,x,bw,mx);
    else     GetFractionalPixelRow(tmp[(y+1)&3],ref+((y-3)*width),x,bw,mx);
    if(y<2)    GetFractionalPixelRow(tmp[(y+2)&3],ref,x,bw,mx);
    else     GetFractionalPixelRow(tmp[(y+2)&3],ref+((y-2)*width),x,bw,mx);
    if(y<1)    GetFractionalPixelRow(tmp[(y+3)&3],ref,x,bw,mx);
    else     GetFractionalPixelRow(tmp[(y+3)&3],ref+((y-1)*width),x,bw,mx);
    for(j=y;j<y1;j++){
        GetFractionalPixelRow(tmp[j&3],ref+(j*width),x,bw,mx);
        for(i=0;i<bw;i++){
            k = ((tmp[(j+1)&3][i]*my[0]+tmp[(j+2)&3][i]*my[1]+tmp[(j+3)&3][i]*my[2]+tmp[j&3][i]*my[3]+8)>>4);
            bb[i] = (k<0)?0:((k>255)?255:k);
        }
        bb += 16;
    }
    if(y2){
        for(i=0;i<bw;i++){
            k = ((tmp[(y1+1)&3][i]*my[0]+tmp[(y1+2)&3][i]*my[1]+tmp[(y1+3)&3][i]*(my[2]+my[3])+8)>>4);
            bb[i] = (k<0)?0:((k>255)?255:k);
        }
        if(y2>1){
            bb += 16;
            for(i=0;i<bw;i++){
                k = ((tmp[(y1+2)&3][i]*my[0]+tmp[(y1+3)&3][i]*(my[1]+my[2]+my[3])+8)>>4);
                bb[i] = (k<0)?0:((k>255)?255:k);
            }
        }
    }
    return 0;
#endif
}

/*****************************************************************************************************/
int MEforGen75::GetReferenceBlock2Tap(U8 *blk, U8 *ref, short qx, short qy, int bw, int bh
#if (__HAAR_SAD_OPT == 1)
                                      , int src_pitch
#endif
                                      )
/*****************************************************************************************************/
{
    int        ix = (qx>>2);
    int        iy = (qy>>2);
    int        dx = (qx&3);
    int        dy = (qy&3);
    int        width = 0;

#if (__HAAR_SAD_OPT == 1)    
    if (src_pitch != 0) {
        width = src_pitch;
    } else 
#endif
    if (ref == SkipRef[0] || ref == SkipRef[1])
    {
        // skip surface
        width = SKIPWIDTH;
    }
    else
    {
        // reference surface
        width = REFWINDOWWIDTH;
    }

    U8        *p0 = ref + (iy*width) + ix; 
    U8        *p1 = p0 + width; 
    int        q0=0, q1=0, q2=0, q3=0; // integer locs
    int        r0=0, r1=0, r2=0, r3=0, r4=0; // halfpel locs
    for(int j=0;j<bh;j++){
        for(int i=0;i<bw;i++){
            
            // q0....r0....q1
            // .............
            // r3....r4....r1
            // .............
            // q2....r2....q3
            q0 = p0[i+j*width];
            q1 = p0[i+1+j*width];
            q2 = p1[i+j*width];
            q3 = p1[i+1+j*width];
            r0 = (q0+q1+1)/2;
            r1 = (q1+q3+1)/2;
            r2 = (q3+q2+1)/2;
            r3 = (q2+q0+1)/2;
            r4 = (q0+q1+q2+q3+2)/4;

            if ((!(dx&1)) && (!(dy&1))) // half pel locs
            {
                ix = (p0[i+j*width]<<2)+(p0[i+1+j*width]-p0[i+j*width])*dx;
                iy = (p1[i+j*width]<<2)+(p1[i+1+j*width]-p1[i+j*width])*dx;
                blk[(j<<4)+i] = (((ix<<2)+(iy-ix)*dy + 8)>>4);
            }
            else if (dx == 1 && dy == 0)
            {
                blk[(j<<4)+i] = (q0+r0)/2;
            }
            else if (dx == 3 && dy == 0)
            {
                blk[(j<<4)+i] = (r0+q1)/2;
            }
            else if (dx == 0 && dy == 1)
            {
                blk[(j<<4)+i] = (q0+r3)/2;
            }
            else if (dx == 1 && dy == 1)
            {
                blk[(j<<4)+i] = (r0+r3)/2;
            }
            else if (dx == 2 && dy == 1)
            {
                blk[(j<<4)+i] = (r0+r4)/2;
            }
            else if (dx == 3 && dy == 1)
            {
                blk[(j<<4)+i] = (r0+r1)/2;
            }
            else if (dx == 1 && dy == 2)
            {
                blk[(j<<4)+i] = (r3+r4)/2;
            }
            else if (dx == 3 && dy == 2)
            {
                blk[(j<<4)+i] = (r4+r1)/2;
            }
            else if (dx == 0 && dy == 3)
            {
                blk[(j<<4)+i] = (r3+q2)/2;
            }
            else if (dx == 1 && dy == 3)
            {
                blk[(j<<4)+i] = (r3+r2)/2;
            }
            else if (dx == 2 && dy == 3)
            {
                blk[(j<<4)+i] = (r4+r2)/2;
            }
            else if (dx == 3 && dy == 3)
            {
                blk[(j<<4)+i] = (r1+r2)/2;
            }
        }
    }
    return 0;
}

/*****************************************************************************************************/
int MEforGen75::GetReferenceBlockField(U8   *blk, U8   *ref, short qx, short qy, int bw, int bh)
/*****************************************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
#if 1 // sync with HW
    int        i, j, k;
    int        x = (qx>>2);
    int        y = (qy>>2);
    U8      tmp0[256],tmp1[512];
    U8        *pp,*p0,*p1;
    int        m0,m1;
    switch(qy&3){
        case 0:
        switch(qx&3){
            case 0: m0 = 0; m1 = 8; p1 = p0 = ref + (y<<6) + x; break;
            case 1: m0 = 0; m1 = 1; p1 = p0 = ref + (y<<6) + x; break;
            case 2: m0 = 1; m1 = 8; p1 = p0 = ref + (y<<6) + x; break;
            case 3: m0 = 1; m1 = 0; p1 =(p0 = ref + (y<<6) + x) + 1; break;
        }
        break;
        case 1:
        switch(qx&3){
            case 0: m0 = 0; m1 = 2; p1 = p0 = ref + (y<<6) + x; break;
            case 1: m0 = 1; m1 = 2; p1 = p0 = ref + (y<<6) + x; break;
            case 2: m0 = 3; m1 = 1; p1 = p0 = ref + (y<<6) + x; break;
            case 3: m0 = 1; m1 = 2; p1 =(p0 = ref + (y<<6) + x) + 1; break;
        }
        break;
        case 2:
        switch(qx&3){
            case 0: m0 = 2; m1 = 8; p1 = p0 = ref + (y<<6) + x; break;
            case 1: m0 = 3; m1 = 2; p1 = p0 = ref + (y<<6) + x; break;
            case 2: m0 = 3; m1 = 8; p1 = p0 = ref + (y<<6) + x; break;
            case 3: m0 = 3; m1 = 2; p1 =(p0 = ref + (y<<6) + x) + 1; break;
        }
        break;
        case 3:
        switch(qx&3){
            case 0: m0 = 2; m1 = 0; p1 =(p0 = ref + (y<<6) + x) + 128; break;
            case 1: m0 = 2; m1 = 1; p1 =(p0 = ref + (y<<6) + x) + 128; break;
            case 2: m0 = 3; m1 = 1; p1 =(p0 = ref + (y<<6) + x) + 128; break;
            case 3: m0 = 2; m1 = 1; p1 =(p0 = ref + (y<<6) + x + 1) + 127; break;
        }
        break;
    }
    switch(m0){
        case 0:
            for(j=0;j<bh;j++) MFX_INTERNAL_CPY(&tmp0[j<<4],&p0[j<<7],bw);
            break;
        case 1:
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    pp = &p0[(j<<7)+i];
                    k = (((pp[0]+pp[1])*5-pp[-1]-pp[2])>>4);
                    tmp0[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
            break;
        case 2:
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    pp = &p0[(j<<7)+i];
                    k = (((pp[0]+pp[128])*5-pp[-128]-pp[256])>>4);
                    tmp0[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
            break;
        case 3:
            for(j=0;j<bh;j++){
                for(i=0;i<bw+3;i++){
                    pp = &p0[(j<<7)+i-1];
                    k = (((pp[0]+pp[128])*5-pp[-128]-pp[256])>>4);
                    tmp1[(j<<5)+i] = (k<0)?0:((k>255)?255:k);
                }
                for(i=0;i<bw;i++){
                    pp = &tmp1[(j<<5)+i+1];
                    k = (((pp[0]+pp[1])*5-pp[-1]-pp[2])>>4);
                    tmp0[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
            break;
    }
    switch(m1){
        case 0:
            for(j=0;j<bh;j++) MFX_INTERNAL_CPY(&tmp1[j<<4],&p1[j<<7],bw);
            break;
        case 1:
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    pp = &p1[(j<<7)+i];
                    k = (((pp[0]+pp[1])*5-pp[-1]-pp[2])>>4);
                    tmp1[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
            break;
        case 2:
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    pp = &p1[(j<<7)+i];
                    k = (((pp[0]+pp[128])*5-pp[-128]-pp[256])>>4);
                    tmp1[(j<<4)+i] = (k<0)?0:((k>255)?255:k);
                }
            }
            break;
        case 8:
            for(j=0;j<bh;j++){
                MFX_INTERNAL_CPY(&tmp1[j<<4],&tmp0[j<<4], bw);
            }
            break;
    }

    for(j=0;j<bh;j++){
        for(i=0;i<bw;i++){
            blk[(j<<5)+i] = ((tmp0[(j<<4)+i]+tmp1[(j<<4)+i])>>1);
        }
    }
    return 0;

#else
    short    mul[4][4] = {{0,16,0,0},{-1,13,5,-1},{-2,10,10,-2},{-1,5,13,-1}};
    int        i, j;
    int        x = (qx>>2);
    int        y = (qy>>2)+4;
    short    *mx = mul[qx&3];
    short    *my = mul[qy&3];
    U8      *bb = blk;
    int        y1 = y + (bh<<1);
    int        y2 = (y1>=Vsta.RefH)?(y1-Vsta.RefH+1):0;
    U8      tmp[4][16];
    if(y2&1) y2++;
    y1 -= y2;

    if(y<3)    GetFractionalPixelRow(tmp[1],ref,x,bw,mx);
    else     GetFractionalPixelRow(tmp[1],ref+((y-3)<<6),x,bw,mx);
    if(y<2)    GetFractionalPixelRow(tmp[2],ref,x,bw,mx);
    else     GetFractionalPixelRow(tmp[2],ref+((y-2)<<6),x,bw,mx);
    if(y<1)    GetFractionalPixelRow(tmp[3],ref,x,bw,mx);
    else     GetFractionalPixelRow(tmp[3],ref+((y-1)<<6),x,bw,mx);
    int        k = 0;
    for(j=y;j<y1;j+=2){
        GetFractionalPixelRow(tmp[k&3],ref+(j<<6),x,bw,mx);
        for(i=0;i<bw;i++){
            bb[i] = ((tmp[(k+1)&3][i]*my[0]+tmp[(k+2)&3][i]*my[1]+tmp[(k+3)&3][i]*my[2]+tmp[k&3][i]*my[3]+8)>>4);
        }
        bb += 16; k++;
    }
    if(y2){
        for(i=0;i<bw;i++){
            bb[i] = ((tmp[(k+1)&3][i]*my[0]+tmp[(k+2)&3][i]*my[1]+tmp[(k+3)&3][i]*(my[2]+my[3])+8)>>4);
        }
        if(y2>1){
            bb += 16;
            for(i=0;i<bw;i++){
                bb[i] = ((tmp[(k+2)&3][i]*my[0]+tmp[(k+3)&3][i]*(my[1]+my[2]+my[3])+8)>>4);
            }
        }
    }
    return 0;
#endif
    */
}

/*****************************************************************************************************/
int MEforGen75::GetReferenceBlock6Tap(U8 *blk, U8   *ref, short mvx, short mvy, int bw, int bh)
/*****************************************************************************************************/
{
    if(bw>8){
        GetReferenceBlock6Tap(blk,   ref, mvx,    mvy, 8,    bh);
        GetReferenceBlock6Tap(blk+8, ref, mvx+32, mvy, bw-8, bh);
        return 0;
    }
    else if(bh>8){
        GetReferenceBlock6TapS(blk,     ref, mvx, mvy,    bw, 8   );
        GetReferenceBlock6TapS(blk+128, ref, mvx, mvy+32, bw, bh-8);
        return 0;
    }
    return GetReferenceBlock6TapS(blk, ref, mvx, mvy, bw, bh);
}

/*****************************************************************************************************/
int MEforGen75::GetReferenceBlock6TapS(U8 *blk, U8   *ref, short mvx, short mvy, int bw, int bh)
/*****************************************************************************************************/
{
    I16     tmp[512];
    I16     i,j,m,s;
    U8        *rr;
    I16        *pp, *qq = tmp;
    U8      msk[4][4] = {{0,0x12,2,0x22},{0x18,10,0x16,10},{8,0x1C,6,0x2C},{0x28,10,0x26,10}};

    // (0,0) at temp[68]
    m = (mvx>>2)+12;
    s = (mvy>>2)+12;
    for(j=s-16;j<s;j++){
        rr = (j<0) ? ref : ((j>=Vsta.RefH) ? ref+((Vsta.RefH-1)*REFWINDOWWIDTH) : ref+(j*REFWINDOWWIDTH));
        for(i=m-16;i<m;i++){
            *(qq++) = (i<0) ? rr[0] : ((i>=Vsta.RefW) ? rr[Vsta.RefW-1] : rr[i]);
        }
    }

    m = msk[mvy&3][mvx&3];
    if(m&2){
        pp = &tmp[4];
        qq = &tmp[256];
        // [2] (0,0) at tmp[256+64]
        for(j=0;j<16;j++){
            for(i=0;i<9;i++){
                s = ((pp[0]+pp[1])<<2) - (pp[-1]+pp[2]);
                *qq = (s<<2)+s+pp[-2]+pp[3]+16;
                pp++; qq++;
            }
            pp += 7; qq += 7;
        }
        if(m&4){
            // [10] (0,0) at pb[0]
            pp = &tmp[256+64];
            rr = blk;
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    s = ((pp[0]+pp[16])<<2) - (pp[-16]+pp[32]);
                    s = (((s<<2)+s+pp[-32]+pp[48])>>10);
                    *rr = (s<0) ? 0 : ((s>255) ? 255 : s); 
                    pp++; rr++;
                }
                pp += 16-bw; rr += 16-bw;
            }
        }
    }
    if(m&8){
        qq = (m&2) ? tmp : &tmp[256+60];
        pp = &tmp[64];
        // [8] (0,0) at tmp[4] or tmp[256+64]
        for(j=0;j<9;j++){
            for(i=0;i<16;i++){
                s = ((pp[0]+pp[16])<<2) - (pp[-16]+pp[32]);
                *qq = (s<<2)+s+pp[-32]+pp[48]+16; 
                pp++; qq++;
            }
        }
        if(m&4){
            // [10] (0,0) at pb[0]
            pp = &tmp[256+64];
            rr = blk;
            for(j=0;j<bh;j++){
                for(i=0;i<bw;i++){
                    s = ((pp[0]+pp[1])<<2) - (pp[-1]+pp[2]);
                    s = (((s<<2)+s+pp[-2]+pp[3])>>10);
                    *rr = (s<0) ? 0 : ((s>255) ? 255 : s); 
                    pp++; rr++;
                }
                pp += 16-bw; rr += 16-bw;
            }
        }
    }

    pp = &tmp[68];
    qq = &tmp[256+64];
    switch(m){
    case 0: // (0,0)
        pp = &tmp[68]; 
        for(j=0;j<bh;j++){
            for(i=0;i<bw;i++) blk[i] = (U8) pp[i]; 
            blk += 16; pp += 16;
        }
        return 0;
    case 2: // (2,0)
    case 8: // (0,2)
        pp = &tmp[256+64]; 
        for(j=0;j<bh;j++){
            for(i=0;i<bw;i++){ 
                s = (pp[i]>>5);
                blk[i] = (s<0) ? 0 : ((s>255) ? 255 : s);  
            }
            blk += 16; pp += 16;
        }
        return 0;
    case 6: // (2,2)
        return 0;
    case 0x28:    pp += 15;    // (0,3)
    case 0x22:    pp += 1;    // (3,0)
    case 0x18:                // (0,1)
    case 0x12:                // (1,0)
        for(j=0;j<bh;j++){
            for(i=0;i<bw;i++){ 
                s = (qq[i]<0) ? 0 : ((qq[i]>0x1FFF) ? 255 : (qq[i]>>5));  
                blk[i] = ((pp[i]+s+1)>>1);
            }
            blk += 16; pp += 16; qq += 16;
        }
        return 0;
    case 0x26:    qq += 15;    // (2,3)
    case 0x2C:    qq += 1;    // (3,2)
    case 0x16:                // (2,1)
    case 0x1C:                // (1,2)
        for(j=0;j<bh;j++){
            for(i=0;i<bw;i++){ 
                s = (qq[i]<0) ? 0 : ((qq[i]>0x1FFF) ? 255 : (qq[i]>>5));  
                blk[i] = ((blk[i]+s+1)>>1);
            }
            blk += 16; qq += 16;
        }
        return 0;
    default:    // (1,1) (3,1) (1,3) (3,3)
        pp = (mvy&2) ? &tmp[256+80] : &tmp[256+64];
        qq = (mvx&2) ? &tmp[5] : &tmp[4];
        for(j=0;j<bh;j++){
            for(i=0;i<bw;i++){ 
                s = ((pp[i]+qq[i]+32)>>6);
                blk[i] = (s<0) ? 0 : ((s>255) ? 255 : s);  
            }
            blk += 16; pp += 16; qq += 16;
        }
        return 0;
    }
    return 0;
}

/*
void MEforGen75::CheckPruning(int mode)
{
    int m = mode&0xFF; //subshapes for 8x8
    unsigned short pruningThreshold = 0;
    unsigned short diffSAD = 0;
    
    if(!(Vsta.BidirMask&B_PRUNING_ENABLE))
    {
        return;
    }
    
    if(mode<0x0500)
    {    //16x16
        pruningThreshold = (PruningThr<<4);
        diffSAD = ABS(Dist[0][BHXH] - Dist[1][BHXH]);
        if(diffSAD > pruningThreshold) bPruneBi[BHXH] = true;
        return;
    }

    if(mode<0x0F00)
    {    // 8x16
        pruningThreshold = (PruningThr<<3);
        diffSAD = ABS(Dist[0][B8XH] - Dist[1][B8XH]);
        if(diffSAD > pruningThreshold) bPruneBi[B8XH] = true;
        diffSAD = ABS(Dist[0][B8XH+1] - Dist[1][B8XH+1]);
        if(diffSAD > pruningThreshold) bPruneBi[B8XH+1] = true;
        return;
    }

    if(mode<0x3400)
    {    // 16x8
        pruningThreshold = (PruningThr<<3);
        diffSAD = ABS(Dist[0][BHX8] - Dist[1][BHX8]);
        if(diffSAD > pruningThreshold) bPruneBi[BHX8] = true;
        diffSAD = ABS(Dist[0][BHX8+1] - Dist[1][BHX8+1]);
        if(diffSAD > pruningThreshold) bPruneBi[BHX8+1] = true;
        return;
    }

    // 8x8 (fall through)
    pruningThreshold = (PruningThr<<2);
    for(int k=0;k<4;k++)
    {
        switch(m&3)
        {
        case 0: // 8x8
            diffSAD = ABS(Dist[0][B8X8+k] - Dist[1][B8X8+k]);
            if(diffSAD > pruningThreshold) bPruneBi[B8X8+k] = true;        
            break;
        case 1: // 8x4 - no pruning check required
        case 2: // 4x8 - no pruning check required
        case 3: // 4x4 - no pruning check required
            break;
        default:
            break;
        }
        m>>=2;
    }
}
*/

/*****************************************************************************************************/
bool MEforGen75::CheckBidirValid()//int mode, bool checkPruning, bool checkGlobalReplacement)
/*****************************************************************************************************/
{
    /*We do BidirCheck: it is TRUE if all of the below are true 
     
    (1) dual references 
    (2) at least one active subshape is Bi-enabled 
    (3) not all in minors 
    (4) max_mv >= 2*major + minor
    (5) if checkPruning is 0 OR if checkPruning is 1 and bPruneBi[shape] is false*/

    //int m = mode&0xFF; //subshapes for 8x8
    /*int num8x8 = 0;
    int numBiMVs = 0;*/

    //U8 replacedByGlobal_one = 0;
    //U8 replacedByGlobal_all = 0;
    ///*U8 isMinor[4] = {0, 0, 0, 0};
    //bool bGlobalCheckInValid = false;
    //U8 sub8x8shape = Vsta.FBRSubMbShape;*/
    //U8 prune_one = 0;
    //U8 prune_all = 0;
    //if(Vsta.FBRMbInfo==MODE_INTER_16X16)
    //{    //16x16
    //    //prune_all = bPruneBi[BHXH];
    //    //replacedByGlobal_one = (MultiReplaced[0][BHXH] | MultiReplaced[1][BHXH]);
    //    //bGlobalCheckInValid = checkGlobalReplacement && replacedByGlobal_one;
    //    if((Vsta.BidirMask&BID_NO_16X16)||bGlobalCheckInValid) //||(Vsta.MaxMvs < 2)
    //    {
    //        return false;
    //    }
    //    else
    //    {
    //        /*if((checkPruning)&&(prune_all))
    //        {
    //            return false;
    //        }*/
    //        return true;
    //    }
    //}

    //if(Vsta.FBRMbInfo==MODE_INTER_8X16)
    //{    // 8x16
    //    //prune_all = bPruneBi[B8XH] & bPruneBi[B8XH+1];
    //    //prune_one = bPruneBi[B8XH] | bPruneBi[B8XH+1];
    //    /*replacedByGlobal_one = (MultiReplaced[0][B8XH] | MultiReplaced[1][B8XH]);
    //    replacedByGlobal_one |= (MultiReplaced[0][B8XH+1] | MultiReplaced[1][B8XH+1]);
    //    replacedByGlobal_all = (MultiReplaced[0][B8XH] | MultiReplaced[1][B8XH]);
    //    replacedByGlobal_all &= (MultiReplaced[0][B8XH+1] | MultiReplaced[1][B8XH+1]);*/
    //    //bGlobalCheckInValid = checkGlobalReplacement && (((replacedByGlobal_one==1)&&(Vsta.VmeFlags&VF_BIMIX_DISABLE))||(replacedByGlobal_all==1));
    //    if((Vsta.BidirMask&BID_NO_8X16)||bGlobalCheckInValid) //||(Vsta.MaxMvs < 4)
    //    {
    //        return false;
    //    }
    //    else
    //    {
    //        /*if(checkPruning && (prune_all ||(prune_one && Vsta.VmeFlags&VF_BIMIX_DISABLE)))
    //        {
    //            return false;
    //        }*/
    //        return true;
    //    }
    //}

    //if(Vsta.FBRMbInfo==MODE_INTER_16X8)
    //{    // 16x8
    //    //prune_all = bPruneBi[BHX8] & bPruneBi[BHX8+1];
    //    //prune_one = bPruneBi[BHX8] | bPruneBi[BHX8+1];
    //    /*replacedByGlobal_one = (MultiReplaced[0][BHX8] | MultiReplaced[1][BHX8]);
    //    replacedByGlobal_one |= (MultiReplaced[0][BHX8+1] | MultiReplaced[1][BHX8+1]);
    //    replacedByGlobal_all = (MultiReplaced[0][BHX8] | MultiReplaced[1][BHX8]);
    //    replacedByGlobal_all &= (MultiReplaced[0][BHX8+1] | MultiReplaced[1][BHX8+1]);*/
    //    //bGlobalCheckInValid = checkGlobalReplacement && (((replacedByGlobal_one==1)&&(Vsta.VmeFlags&VF_BIMIX_DISABLE))||(replacedByGlobal_all==1));
    //    if((Vsta.BidirMask&BID_NO_16X8)||bGlobalCheckInValid) //||(Vsta.MaxMvs < 4)
    //    {
    //        return false;
    //    }
    //    else
    //    {
    //        /*if(checkPruning && (prune_all ||(prune_one && Vsta.VmeFlags&VF_BIMIX_DISABLE)))
    //        {
    //            return false;
    //        }*/
    //        return true;
    //    }
    //}

    ////8x8
    //for(int blk=0;blk<4;blk++)
    //{
    //    //set replaced flag to ON if shape is not 8x8. This will help in bypassing BME state altogether in cases where all
    //    //8x8 partitions have been replaced by global.
    //    if((sub8x8shape&3)!=0)
    //    {
    //        isMinor[blk] = 1;
    //    }
    //    sub8x8shape>>=2;
    //}
    //prune_all = bPruneBi[B8X8] & bPruneBi[B8X8+1] & bPruneBi[B8X8+2] & bPruneBi[B8X8+3];
    //prune_one = bPruneBi[B8X8] | bPruneBi[B8X8+1] | bPruneBi[B8X8+2] | bPruneBi[B8X8+3];
    /*replacedByGlobal_one = (MultiReplaced[0][B8X8] | MultiReplaced[1][B8X8] | isMinor[0]);
    replacedByGlobal_one |= (MultiReplaced[0][B8X8+1] | MultiReplaced[1][B8X8+1] | isMinor[1]);
    replacedByGlobal_one |= (MultiReplaced[0][B8X8+2] | MultiReplaced[1][B8X8+2] | isMinor[2]);
    replacedByGlobal_one |= (MultiReplaced[0][B8X8+3] | MultiReplaced[1][B8X8+3] | isMinor[3]);
    replacedByGlobal_all = (MultiReplaced[0][B8X8] | MultiReplaced[1][B8X8] | isMinor[0]);
    replacedByGlobal_all &= (MultiReplaced[0][B8X8+1] | MultiReplaced[1][B8X8+1] | isMinor[1]);
    replacedByGlobal_all &= (MultiReplaced[0][B8X8+2] | MultiReplaced[1][B8X8+2] | isMinor[2]);
    replacedByGlobal_all &= (MultiReplaced[0][B8X8+3] | MultiReplaced[1][B8X8+3] | isMinor[3]);*/
    //bGlobalCheckInValid = checkGlobalReplacement && (((replacedByGlobal_one==1)&&(Vsta.VmeFlags&VF_BIMIX_DISABLE))||(replacedByGlobal_all==1));
    //if((Vsta.BidirMask&BID_NO_8X8)||((m!=0)&&(Vsta.VmeFlags&VF_BIMIX_DISABLE))||bGlobalCheckInValid)
    //if((Vsta.BidirMask&BID_NO_8X8))//||((m!=0)&&(Vsta.VmeFlags&VF_BIMIX_DISABLE))||bGlobalCheckInValid) Why must m == 0 at this point with BiMix disable?  could be all FWD or all BWD..?// JMH
    //{
    //    return false;
    //}

 //   sub8x8shape = Vsta.FBRSubMbShape;
    //for(int k=0;k<4;k++)
    //{
    //    switch(sub8x8shape&3)
    //    {
    //    case 0: // 8x8
    //        num8x8++;
    //        numBiMVs += 2;
    //        break;
    //    case 1: // 8x4
    //    case 2: // 4x8
    //        numBiMVs += 2;
    //        break;
    //    case 3: // 4x4
    //        numBiMVs += 4;
    //        break;
    //    default:
    //        break;
    //    }
    //    sub8x8shape>>=2;
    //}

    //if((num8x8 == 0)||(Vsta.MaxMvs < numBiMVs))
    //{ // JMHTODO: Update with Gen7.5 greedy algorithm (partial bi check based on # of MVs)
    //    return false;
    //}

    //if(checkPruning && (prune_all ||(prune_one && Vsta.VmeFlags&VF_BIMIX_DISABLE)))
    //{
    //    return false;
    //}

    if(Vsta.SadType&DISABLE_BME)
    {
        return false;
    }
    return true;
}

bool MEforGen75::IsMinorShape(U8 dist_idx) const
{
    if (dist_idx >= B4X4 && dist_idx < (16+B4X4))
    {
        return true;
    }

    if (dist_idx >= B8X4 && dist_idx < (8+B8X4))
    {
        return true;
    }

    if (dist_idx >= B4X8 && dist_idx < (8+B4X8))
    {
        return true;
    }

    return false;
}
