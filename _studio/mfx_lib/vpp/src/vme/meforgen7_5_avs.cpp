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

#pragma warning( disable : 4100 )

/*********************************************************************************/
/*static int GetSad8x8(U8   *src, U8   *blk)

{
    int        d = 0;
    for(int i=0; i<8; i++)
    {
        d += ((src[0]>blk[0]) ? src[0]-blk[0] : blk[0]-src[0])
          +  ((src[1]>blk[1]) ? src[1]-blk[1] : blk[1]-src[1])
          +  ((src[2]>blk[2]) ? src[2]-blk[2] : blk[2]-src[2])
          +  ((src[3]>blk[3]) ? src[3]-blk[3] : blk[3]-src[3])
          +  ((src[4]>blk[4]) ? src[4]-blk[4] : blk[4]-src[4])
          +  ((src[5]>blk[5]) ? src[5]-blk[5] : blk[5]-src[5])
          +  ((src[6]>blk[6]) ? src[6]-blk[6] : blk[6]-src[6])
          +  ((src[7]>blk[7]) ? src[7]-blk[7] : blk[7]-src[7]);
        src += 16;
        blk += 8;
    }
    return d;
}*/

// Note: Check AVS-spec for the correctness of the following table:


/*********************************************************************************/
int MEforGen75::IntraAVS8x8SearchUnit( )
/*********************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
    int        d, dist;
    U8      *src, *lft, *top;
    U8        lb[8], rt[8];
    int        IsEdgeBlock = 0;

    src = SrcMB;
    lft = Vsta.LeftPels;
    top = Vsta.TopMinus8Pels+8;

    if(!(Vsta.IntraAvail&INTRA_AVAIL_AEF)) IsEdgeBlock |= NO_AVAIL_A;
    if(!(Vsta.IntraAvail&INTRA_AVAIL_B)) IsEdgeBlock |= NO_AVAIL_B;
    if(!(Vsta.IntraAvail&INTRA_AVAIL_D)){
        if     (Vsta.IntraAvail&INTRA_AVAIL_B)  top[-1] = top[0];
        else if(Vsta.IntraAvail&INTRA_AVAIL_AEF) top[-1] = lft[0];
        else top[-1] = 128;
    }

    IntraPredictAVS8x8(src,lft,lft+8,top,top+8,IsEdgeBlock ,&Intra8x8PredMode[0], &d );    
    dist = LutMode[LUTMODE_INTRA_8x8] + d;

    if(Vsta.IntraAvail&INTRA_AVAIL_C) MFX_INTERNAL_CPY(rt,&top[16],8);
    else memset(rt,top[15],8);
    memset(lb,lft[7],8);
    IntraPredictAVS8x8(src+8,lft,lb,top+8,rt,(IsEdgeBlock&NO_AVAIL_B), &Intra8x8PredMode[1], &d );    
    dist += d;

    top[15] = lft[7]; src += 128; lft += 8; 
    memset(lb,lft[7],8);
    IntraPredictAVS8x8(src,lft,lb,top,top+8,(IsEdgeBlock&NO_AVAIL_A), &Intra8x8PredMode[2], &d );    
    dist += d;

    memset(rt,top[15],8);
    memset(lb,lft[7],8);
    IntraPredictAVS8x8(src+8,lft,lb,top+8,rt,0,&Intra8x8PredMode[3],&d );    
    dist += d;

    Vout->DistIntra4or8 = (dist<MBMAXDIST) ? dist : MBMAXDIST;
    return 0;
    */
}


/*********************************************************************************/
int MEforGen75::IntraPredictAVS8x8(U8 *src, U8 *lft, U8 *l_b, U8 *top, U8 *r_t, int edge, U8 *mode, int *dist)
/*********************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
    U8      tmp[8],lmp[8];
    U8      Pblk[5][64];
    U8      *pp; 
    int        i,j,k,m,mark;

// Low Pass For DC
    tmp[ 0] = ((top[-1]+ (top[0]<<1) + top[1] + 2)>>2);
    tmp[ 1] = ((top[0] + (top[1]<<1) + top[2] + 2)>>2);
    tmp[ 2] = ((top[1] + (top[2]<<1) + top[3] + 2)>>2);
    tmp[ 3] = ((top[2] + (top[3]<<1) + top[4] + 2)>>2);
    tmp[ 4] = ((top[3] + (top[4]<<1) + top[5] + 2)>>2);
    tmp[ 5] = ((top[4] + (top[5]<<1) + top[6] + 2)>>2);
    tmp[ 6] = ((top[5] + (top[6]<<1) + top[7] + 2)>>2);
    tmp[ 7] = ((top[6] + (top[7]<<1) + r_t[0] + 2)>>2);
    lmp[ 0] = ((top[-1]+ (lft[0]<<1) + lft[1] + 2)>>2);
    lmp[ 1] = ((lft[0] + (lft[1]<<1) + lft[2] + 2)>>2);
    lmp[ 2] = ((lft[1] + (lft[2]<<1) + lft[3] + 2)>>2);
    lmp[ 3] = ((lft[2] + (lft[3]<<1) + lft[4] + 2)>>2);
    lmp[ 4] = ((lft[3] + (lft[4]<<1) + lft[5] + 2)>>2);
    lmp[ 5] = ((lft[4] + (lft[5]<<1) + lft[6] + 2)>>2);
    lmp[ 6] = ((lft[5] + (lft[6]<<1) + lft[7] + 2)>>2);
    lmp[ 7] = ((lft[6] + (lft[7]<<1) + l_b[0] + 2)>>2);

    switch(edge&(NO_AVAIL_A|NO_AVAIL_B)){
    case 0:
        pp = Pblk[DCP];
        for(j=0;j<8;j++ ){
            for(i=0;i<8;i++ ) *(pp++) = ((lmp[j]+tmp[i])>>1);
        }
        pp = Pblk[VER];
        MFX_INTERNAL_CPY(pp,  top, 8);
        MFX_INTERNAL_CPY(pp+ 8,pp, 8);
        MFX_INTERNAL_CPY(pp+16,pp,16);
        MFX_INTERNAL_CPY(pp+32,pp,32);
        pp = Pblk[HOR];
        for(i=0;i<8;i++ ){ memset(pp,lft[i],8); pp += 8; }
        mark = 31;

// DIAG_DOWN_LEFT  (/)
        pp = Pblk[DDL];
        pp[ 0]=                                                            ((tmp[1]+lmp[1])>>1);
        pp[ 1]= pp[ 8]=                                                    ((tmp[2]+lmp[2])>>1);
        pp[ 2]= pp[ 9]=    pp[16]=                                            ((tmp[3]+lmp[3])>>1);
        pp[ 3]= pp[10]=    pp[17]= pp[24]=                                    ((tmp[4]+lmp[4])>>1);
        pp[ 4]= pp[11]=    pp[18]= pp[25]= pp[32]=                            ((tmp[5]+lmp[5])>>1);
        pp[ 5]= pp[12]=    pp[19]= pp[26]=    pp[33]= pp[40]=                    ((tmp[6]+lmp[6])>>1);
        pp[ 6]= pp[13]=    pp[20]= pp[27]=    pp[34]= pp[41]= pp[48]=            ((tmp[7]+lmp[7])>>1);
        pp[ 7]= pp[14]=    pp[21]= pp[28]=    pp[35]= pp[42]= pp[49]= pp[56]= ((top[7]+lft[7]+r_t[1]+l_b[1]+((r_t[0]+l_b[0])<<1)+4)>>3);
                pp[15]=    pp[22]= pp[29]=    pp[36]= pp[43]= pp[50]= pp[57]= ((r_t[0]+l_b[0]+r_t[2]+l_b[2]+((r_t[1]+l_b[1])<<1)+4)>>3);
                        pp[23]= pp[30]=    pp[37]= pp[44]= pp[51]= pp[58]= ((r_t[1]+l_b[1]+r_t[3]+l_b[3]+((r_t[2]+l_b[2])<<1)+4)>>3);
                                pp[31]=    pp[38]= pp[45]= pp[52]= pp[59]= ((r_t[2]+l_b[2]+r_t[4]+l_b[4]+((r_t[3]+l_b[3])<<1)+4)>>3);
                                        pp[39]= pp[46]= pp[53]= pp[60]= ((r_t[3]+l_b[3]+r_t[5]+l_b[5]+((r_t[4]+l_b[4])<<1)+4)>>3);
                                                pp[47]=    pp[54]= pp[61]= ((r_t[4]+l_b[4]+r_t[6]+l_b[6]+((r_t[5]+l_b[5])<<1)+4)>>3);
                                                        pp[55]= pp[62]= ((r_t[5]+l_b[5]+r_t[7]+l_b[7]+((r_t[6]+l_b[6])<<1)+4)>>3);
                                                                pp[63]= ((r_t[6]+l_b[6]+r_t[7]+l_b[7]+((r_t[7]+l_b[7])<<1)+4)>>3);
// DIAG_DOWN_RIGHT (\)
        pp = Pblk[DDR];
        pp[56]=                                                            lmp[6];
        pp[57]= pp[48]=                                                    lmp[5];
        pp[58]= pp[49]=    pp[40]=                                            lmp[4];
        pp[59]= pp[50]=    pp[41]= pp[32]=                                    lmp[3];
        pp[60]= pp[51]=    pp[42]= pp[33]=    pp[24]=                            lmp[2];
        pp[61]= pp[52]=    pp[43]= pp[34]=    pp[25]= pp[16]=                    lmp[1];
        pp[62]= pp[53]=    pp[44]= pp[35]=    pp[26]= pp[17]= pp[ 8]=            lmp[0];
        pp[63]= pp[54]=    pp[45]= pp[36]=    pp[27]= pp[18]= pp[ 9]= pp[ 0]= ((lft[ 0]+top[ 0]+(top[-1]<<1)+2)>>2);
                pp[55]=    pp[46]= pp[37]=    pp[28]= pp[19]= pp[10]= pp[ 1]= tmp[0];
                        pp[47]= pp[38]=    pp[29]= pp[20]= pp[11]= pp[ 2]= tmp[1];
                                pp[39]=    pp[30]= pp[21]= pp[12]= pp[ 3]= tmp[2];
                                        pp[31]= pp[22]= pp[13]= pp[ 4]= tmp[3];
                                                pp[23]=    pp[14]= pp[ 5]= tmp[4];
                                                        pp[15]= pp[ 6]= tmp[5];
                                                                pp[ 7]= tmp[6];
        break;
    case NO_AVAIL_A:
        pp = Pblk[DCP];
        MFX_INTERNAL_CPY(pp,  tmp, 8);
        MFX_INTERNAL_CPY(pp+ 8,pp, 8);
        MFX_INTERNAL_CPY(pp+16,pp,16);
        MFX_INTERNAL_CPY(pp+32,pp,32);
        pp = Pblk[VER];
        MFX_INTERNAL_CPY(pp,  top, 8);
        MFX_INTERNAL_CPY(pp+ 8,pp, 8);
        MFX_INTERNAL_CPY(pp+16,pp,16);
        MFX_INTERNAL_CPY(pp+32,pp,32);
        mark = 5;
        break;
    case NO_AVAIL_B:
        pp = Pblk[DCP];
        for(i=0;i<8;i++ ){ memset(pp,lmp[i],8); pp += 8; }
        pp = Pblk[HOR];
        for(i=0;i<8;i++ ){ memset(pp,lft[i],8); pp += 8; }
        mark = 6;
        break;
    default:
        memset(Pblk[DCP],128,64);
        mark = 4;
    }

    k = MBMAXDIST;
    for(j=1;j<32;j<<=1){
        if(j&mark){
            if((i = GetSad8x8(src,Pblk[j]))<k){ m = j; k = i; }
    }}
    top[-1] = lft[7];
#if 0
    lft[0] = Pblk[m][ 7];    lft[1] = Pblk[m][15];    lft[2] = Pblk[m][23];    lft[3] = Pblk[m][31];
    lft[4] = Pblk[m][39];    lft[5] = Pblk[m][47];    lft[6] = Pblk[m][55];    lft[7] = Pblk[m][63];
    top[0] = Pblk[m][56];    top[1] = Pblk[m][57];    top[2] = Pblk[m][58];    top[3] = Pblk[m][59];
    top[4] = Pblk[m][60];    top[5] = Pblk[m][61];    top[6] = Pblk[m][62];    // top[7] = Pblk[m][63];
#else // from original
    lft[0] = src[0x07];    lft[1] = src[0x17];    lft[2] = src[0x27];    lft[3] = src[0x37];
    lft[4] = src[0x47];    lft[5] = src[0x57];    lft[6] = src[0x67];    lft[7] = src[0x77];
    top[0] = src[0x70];    top[1] = src[0x71];    top[2] = src[0x72];    top[3] = src[0x73];
    top[4] = src[0x74];    top[5] = src[0x75];    top[6] = src[0x76];    // top[7] = src[0x77];
#endif
    *mode = m;
    *dist = k;
    return 0;
    */
}


/*********************************************************************************/
int MEforGen75::AvsMirrorBestMajorMVs( )
/*********************************************************************************/
{
    return ERR_UNSUPPORTED;
    /*
    int        dwh[10][2];
    int        j, x, y, dx, dy;
    U8PAIR    *mva, *mvb;
    U8PAIR    za, zb;
    I32        *dfa, *dfb;

    // TO DO: Mirror calculation need to be adjusted when Src or Ref is of Field type.
    if(Vsta.SrcType&(ST_SRC_FIELD|ST_REF_FIELD)) return ERR_UNDERDEVELOP;

    x = Vsta.BiWeight;
    if(x<32){
        mva = &Best[0][0];    dfa = &Dist[0][0];
        mvb = &Best[1][0];    dfb = &Dist[1][0]; 
        za.x = (Vsta.Ref0.x - Vsta.SrcMB.x)<<2;    za.y = (Vsta.Ref0.y - Vsta.SrcMB.y)<<2;
        zb.x = (Vsta.Ref1.x - Vsta.SrcMB.x)<<2;    zb.y = (Vsta.Ref1.y - Vsta.SrcMB.y)<<2;
    }
    else{
        mva = &Best[1][0];    dfa = &Dist[1][0];
        mvb = &Best[0][0];    dfb = &Dist[0][0];
        za.x = (Vsta.Ref1.x - Vsta.SrcMB.x)<<2;    za.y = (Vsta.Ref1.y - Vsta.SrcMB.y)<<2;
        zb.x = (Vsta.Ref0.x - Vsta.SrcMB.x)<<2;    zb.y = (Vsta.Ref0.y - Vsta.SrcMB.y)<<2;
        x = 64 - x;
    }

    switch(x){
    case  9:    x = 384; y = 11; break; // 1/7
    case 11:    x = 320; y = 13; break; // 1/6
    case 13:    x = 256; y = 16; break; // 1/5
    case 16:    x = 192; y = 21; break; // 1/4
    case 18:    x = 160; y = 26; break; // 2/7
    case 21:    x = 128; y = 32; break; // 1/3
    case 26:    x =  96; y = 43; break; // 2/5
    case 27:    x =  85; y = 48; break; // 3/7
    case 32:    x =  64; y = 64; break; // 1/2
    }

    dwh[1][0] = dwh[2][0] =    dwh[3][0] = ((Vsta.RefW-16)<<2); 
    dwh[4][0] = dwh[5][0] =    dwh[6][0] = 
    dwh[7][0] = dwh[8][0] =    dwh[9][0] = ((Vsta.RefW-8)<<2); 
    dwh[1][1] = dwh[4][1] =    dwh[5][1] = ((Vsta.RefH-16)<<2); 
    dwh[1][1] = dwh[3][1] =    dwh[6][1] = 
    dwh[7][1] = dwh[8][1] =    dwh[9][1] = ((Vsta.RefH-8)<<2); 


    for(j=1;j<10;j++){
        if(dfb[j]<dfa[j]){
            dx = -za.x - (((mvb[j].x+zb.x)*x + 32)>>6);
            dy = -za.y - (((mvb[j].y+zb.y)*x + 32)>>6);
            mva[j].x = (dx<0||dx>=dwh[j][0]) ? 255 : dx;
            mva[j].y = (dy<0||dy>=dwh[j][1]) ? 255 : dy;
        }
        else{
            dx = -zb.x - (((mva[j].x+za.x)*y + 32)>>6);
            dy = -zb.y - (((mva[j].y+za.y)*y + 32)>>6);
            mvb[j].x = (dx<0||dx>=dwh[j][0]) ? 255 : dx;
            mvb[j].y = (dy<0||dy>=dwh[j][1]) ? 255 : dy;
        }
    }

    return 0;
    */
}
