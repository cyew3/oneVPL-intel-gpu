/*********************************************************************************\
**
** Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.
**
** Project:                Hardware Motion Estimation C Model
** File:                MEforGen75_INTRA.cpp
** Original Created:    02/11/2007 Ning Lu 
** Last Modified:        09/11/2007 Ning Lu
** 
\*********************************************************************************/

#include "umc_defs.h"
#include "ipps.h"

#include "meforgen7_5.h"

#pragma warning( disable : 4244 )
#pragma warning( disable : 4100 )

// 16x16 intra prediction modes
#define VER_16           0
#define HOR_16           1
#define DCP_16           2
#define PLN_16           3
#define DEFAULT_DC        128

// Maintain Same as I16x16 as per RTL Request for simplicity.
#define    CHROMA_PRED8x8_DC   0
#define    CHROMA_PRED8x8_HORZ  1
#define    CHROMA_PRED8x8_VERT  2
#define    CHROMA_PRED8x8_PLANAR 3


#define    PRED16x16_VERT    0
#define    PRED16x16_HORZ    1
#define    PRED16x16_DC      2
#define    PRED16x16_PLANAR  3

/*********************************************************************************/
int MEforGen75::GetSad8x8(U8 *src, U8 *blk)
/*********************************************************************************/
{
    int        d = 0;
     d += GetSad4x4(src, blk, 8);
    d += GetSad4x4(src+4, blk+4, 8);
    d += GetSad4x4(src+64, blk+32, 8);
    d += GetSad4x4(src+68, blk+36, 8);
    return d;
}

/*********************************************************************************/
int MEforGen75::GetSad16x16(U8 *src, U8 *blk)
/*********************************************************************************/
{
    int    d = 0;
    for (int i=0;i<4;i++){
        d += GetSad4x4(src, blk, 16);
        d += GetSad4x4(src+4, blk+4, 16);
        d += GetSad4x4(src+8, blk+8, 16);
        d += GetSad4x4(src+12, blk+12, 16);
         src += 64;    blk += 64;
    }
     return d;
}

#if (__HAAR_SAD_OPT == 1)
/*********************************************************************************/
int MEforGen75::GetSad16x16_dist(U8 *src, U8 *blk, int dist)
/*********************************************************************************/
{
    int    d = 0;
    for (int i=0;i<4;i++){
         if (d < dist)
            d += GetSad4x4(src, blk, 16);
        if (d < dist)
            d += GetSad4x4(src+4, blk+4, 16);
        if (d < dist)
            d += GetSad4x4(src+8, blk+8, 16);
        if (d < dist)
            d += GetSad4x4(src+12, blk+12, 16);
         src += 64;    blk += 64;
    }
     return d;
}
#endif

/*********************************************************************************/
int MEforGen75::Intra16x16SearchUnit( )
/*********************************************************************************/
{
    int        i,j,k,m;
    int        d, dist;
    int        s0,s1,s2;
    U8      ref[256];
    U8      *lft = VSICsta.LeftPels;
    U8      *top = VSICsta.TopMinus2Pels+2;
    int     tempDCsad = 0;
    s2 = s1 = 8;
    for(k=0;k<16;k++){
        s1 += lft[k]; s2 += top[k];
    }

    // DC
    s0 = (Vsta.IntraAvail&INTRA_AVAIL_AEF) ? 
        ((Vsta.IntraAvail&INTRA_AVAIL_B) ? ((s1+s2)>>5) : (s1>>4)):((Vsta.IntraAvail&INTRA_AVAIL_B) ? (s2>>4) : 128);
    memset(ref,s0,256);
    dist = tempDCsad = GetSad16x16(SrcMB,ref);
    m = DCP_16;
    if((IntraSkipMask&(INTRAMASK16X16<<DCP_16))) dist = 0x77777777;

    // VERTICAL
    if(!(IntraSkipMask&(INTRAMASK16X16<<VER_16))){
        if(Vsta.IntraAvail&INTRA_AVAIL_B){
            for(k=0;k<16;k++){
                MFX_INTERNAL_CPY(ref+(k<<4),top,16);
            }
#if (__HAAR_SAD_OPT == 1)
            d = GetSad16x16_dist(SrcMB,ref,dist) + VSICsta.IntraNonDCPenalty16x16;
#else
            d = GetSad16x16(SrcMB,ref) + VSICsta.IntraNonDCPenalty16x16;
#endif
            if(d<dist){ dist = d; m = VER_16; }
        }
    }

    // HORIZONTAL
    if(!(IntraSkipMask&(INTRAMASK16X16<<HOR_16))){
        if(Vsta.IntraAvail&INTRA_AVAIL_AEF){
            for(k=0;k<16;k++){
                memset(ref+(k<<4),lft[k],16);
            }
#if (__HAAR_SAD_OPT == 1)
            d = GetSad16x16_dist(SrcMB,ref,dist) + VSICsta.IntraNonDCPenalty16x16;
#else
            d = GetSad16x16(SrcMB,ref) + VSICsta.IntraNonDCPenalty16x16;
#endif
            if(d<dist){ dist = d; m = HOR_16; }
        }
    }

    // PLAIN
    if(!(IntraSkipMask&(INTRAMASK16X16<<PLN_16))){
        if((Vsta.IntraAvail&INTRA_AVAIL_AEF)&&(Vsta.IntraAvail&INTRA_AVAIL_B)&&(Vsta.IntraAvail&INTRA_AVAIL_D)){
            s2 =( 5*(top[ 8]-top[6])+10*(top[ 9]-top[5])+15*(top[10]-top[4])+20*(top[11]-top[3])
                + 25*(top[12]-top[2])+30*(top[13]-top[1])+35*(top[14]-top[0])+40*(top[15]-top[-1]) + 32)>>6;
            s1 =( 5*(lft[ 8]-lft[6])+10*(lft[ 9]-lft[5])+15*(lft[10]-lft[4])+20*(lft[11]-lft[3])
                + 25*(lft[12]-lft[2])+30*(lft[13]-lft[1])+35*(lft[14]-lft[0])+40*(lft[15]-top[-1]) + 32)>>6;
            s0 = ((lft[15]+top[15]+1)<<4) - ((s1+s2)<<3);

            for(j=0;j<256;j+=16){
                k = (s0+=s1);
                for(i=0;i<16;i++){
                    k += s2;
                    ref[j+i] = (k<0) ? 0 : ((k>=0x2000) ? 255 : (k>>5));
                }
            }
#if (__HAAR_SAD_OPT == 1)
            d = GetSad16x16_dist(SrcMB,ref,dist) + VSICsta.IntraNonDCPenalty16x16;
#else
            d = GetSad16x16(SrcMB,ref) + VSICsta.IntraNonDCPenalty16x16;
#endif
            if(d<dist){ dist = d; m = PLN_16; }
        }
    }
    // If no other mode than DC is selected when DC is masked
    // we default back to DC mode
    if (dist == 0x77777777)
    {
        dist = tempDCsad;
        m = DCP_16; 
    }
    dist += LutMode[LUTMODE_INTRA_16x16];
    DistI16 = (dist<MBMAXDIST) ? dist : MBMAXDIST;
    Vout->IntraPredModes[0] = m;
    return 0;
}

/*********************************************************************************/
int MEforGen75::IntraPredict4x4(U8   *src, U8   *lft, U8   *top, int edge, U8 pmode, U8 *mode, I32 *dist)
/*********************************************************************************/
{
    int        i,j,m;
    U8      Pblk[9][16];
     int        d[9];
    U8      nonDCmodePicked = 0;
    memset(d,0x77,9*sizeof(int));
    
    if(edge&NO_AVAIL_B){
        memset(top-1,DEFAULT_DC,9);
        if(!(edge&NO_AVAIL_A)) top[-1] = lft[0];
    }
    
    if(edge&NO_AVAIL_C)    ///!!!if((edge&NO_AVAIL_C) && !(edge&NO_AVAIL_B))
        memset(top+4,top[3],4);

    if(edge&NO_AVAIL_A){
        memset(lft,DEFAULT_DC,4);
        if(!(edge&NO_AVAIL_B)){
            ///!!!if(edge&NO_AVAIL_D)
                top[-1] = top[0];
            m = ((top[0]+top[1]+top[2]+top[3]+2)>>2);
        }
        else m = DEFAULT_DC;
    }
    else if(edge&NO_AVAIL_B) m = ((lft[0]+lft[1]+lft[2]+lft[3]+2)>>2);
    else m = ((lft[0]+lft[1]+lft[2]+lft[3]+top[0]+top[1]+top[2]+top[3]+4)>>3);

// DC_Prediction (O)
    for(i=0;i<16;i++ ) Pblk[DCP][i] = m;
    d[DCP] = GetSad4x4(src,Pblk[DCP],4);

// Vertical Prediction (|) 
    if(!(edge&NO_AVAIL_B)){
        for(i=0;i< 4;i++ ){
            Pblk[VER][i] = Pblk[VER][i+4] = Pblk[VER][i+8] = Pblk[VER][i+12] = top[i];
        }
        d[VER] = GetSad4x4(src,Pblk[VER],4) + VSICsta.IntraNonDCPenalty4x4;
    }
// Horizontal Pred (-)
    if(!(edge&NO_AVAIL_A)){
        for(i=0;i<16;i+=4){
            Pblk[HOR][i] = Pblk[HOR][i+1] = Pblk[HOR][i+2] = Pblk[HOR][i+3] = lft[(i>>2)];
        }
        d[HOR] = GetSad4x4(src,Pblk[HOR],4) + VSICsta.IntraNonDCPenalty4x4;
    }

    if(!(edge&NO_AVAIL_B))
    {
// DIAG_DOWN_LEFT  (/)
    Pblk[DDL][ 0]                                                     = (top[0]+top[1]*2+top[2]+2)>>2; 
    Pblk[DDL][ 4] = Pblk[DDL][ 1]                                    = (top[1]+top[2]*2+top[3]+2)>>2; 
       Pblk[DDL][ 8] = Pblk[DDL][ 5] = Pblk[DDL][ 2]                    = (top[2]+top[3]*2+top[4]+2)>>2; 
       Pblk[DDL][12] = Pblk[DDL][ 9] = Pblk[DDL][ 6] = Pblk[DDL][ 3]    = (top[3]+top[4]*2+top[5]+2)>>2; 
                       Pblk[DDL][13] = Pblk[DDL][10] = Pblk[DDL][ 7]    = (top[4]+top[5]*2+top[6]+2)>>2; 
                                       Pblk[DDL][14] = Pblk[DDL][11]    = (top[5]+top[6]*2+top[7]+2)>>2; 
                                                       Pblk[DDL][15]    = (top[6]+top[7]*3+2)>>2; 
        d[DDL] = GetSad4x4(src,Pblk[DDL],4) + VSICsta.IntraNonDCPenalty4x4;
// VERT_LEFT_PRED
    Pblk[VLP][ 0]                                                     = (top[0]+top[1]+1)>>1; 
    Pblk[VLP][ 8] = Pblk[VLP][ 1]                                      = (top[1]+top[2]+1)>>1; 
                    Pblk[VLP][ 9] = Pblk[VLP][ 2]                      = (top[2]+top[3]+1)>>1; 
                                    Pblk[VLP][10] = Pblk[VLP][ 3]   = (top[3]+top[4]+1)>>1; 
                                                    Pblk[VLP][11]     = (top[4]+top[5]+1)>>1; 
    Pblk[VLP][ 4]                                                     = (top[0]+top[1]*2+top[2]+2)>>2; 
    Pblk[VLP][12] = Pblk[VLP][ 5]                                      = (top[1]+top[2]*2+top[3]+2)>>2;  
                    Pblk[VLP][13] = Pblk[VLP][ 6]                      = (top[2]+top[3]*2+top[4]+2)>>2; 
                                    Pblk[VLP][14] = Pblk[VLP][ 7]     = (top[3]+top[4]*2+top[5]+2)>>2; 
                                                    Pblk[VLP][15]     = (top[4]+top[5]*2+top[6]+2)>>2; 
        d[VLP] = GetSad4x4(src,Pblk[VLP],4) + VSICsta.IntraNonDCPenalty4x4;
    }

    if(!(edge&NO_AVAIL_A))
    {
// HOR_UP_PRED
    Pblk[HUP][ 0]                                                     = (lft[0]+lft[1]+1)>>1; 
                    Pblk[HUP][ 1]                                     = (lft[0]+lft[1]*2+lft[2]+2)>>2; 
    Pblk[HUP][ 4] =                    Pblk[HUP][ 2]                    = (lft[1]+lft[2]+1)>>1; 
                    Pblk[HUP][ 5] =                    Pblk[HUP][ 3]    = (lft[1]+lft[2]*2+lft[3]+2)>>2; 
    Pblk[HUP][ 8] =                    Pblk[HUP][ 6]                    = (lft[2]+lft[3]+1)>>1; 
                    Pblk[HUP][ 9] =                    Pblk[HUP][ 7]    = (lft[2]+lft[3]*3+2)>>2; 
                                    Pblk[HUP][10] =    Pblk[HUP][11]    = 
    Pblk[HUP][12] =    Pblk[HUP][13] = Pblk[HUP][14] =    Pblk[HUP][15]    = lft[3];
          d[HUP] = GetSad4x4(src,Pblk[HUP],4) + VSICsta.IntraNonDCPenalty4x4;

  
        if(!(edge&NO_AVAIL_B) && !(edge&NO_AVAIL_D))
    {
// DIAG_DOWN_RIGHT_PRED
    Pblk[DDR][12]                                                     = (lft[3]+lft[2]*2+lft[1]+2)>>2; 
    Pblk[DDR][ 8] = Pblk[DDR][13]                                    = (lft[2]+lft[1]*2+lft[0]+2)>>2; 
    Pblk[DDR][ 4] = Pblk[DDR][ 9] = Pblk[DDR][14]                    = (lft[1]+lft[0]*2+top[-1]+2)>>2; 
    Pblk[DDR][ 0] = Pblk[DDR][ 5] = Pblk[DDR][10] = Pblk[DDR][15]    = (lft[0]+top[-1]*2+top[0]+2)>>2; 
                    Pblk[DDR][ 1] = Pblk[DDR][ 6] = Pblk[DDR][11]    = (top[-1]+top[0]*2+top[1]+2)>>2; 
                                    Pblk[DDR][ 2] = Pblk[DDR][ 7]    = (top[0]+top[1]*2+top[2]+2)>>2; 
                                                    Pblk[DDR][ 3]    = (top[1]+top[2]*2+top[3]+2)>>2; 
        d[DDR] = GetSad4x4(src,Pblk[DDR],4) + VSICsta.IntraNonDCPenalty4x4;
// VERT_RIGHT_PRED
    Pblk[VRP][ 8]                                                     = (lft[1]+lft[0]*2+top[-1]+2)>>2; 
    Pblk[VRP][ 0] =    Pblk[VRP][ 9]                                    = (top[-1]+top[0]+1)>>1; 
                    Pblk[VRP][ 1] =    Pblk[VRP][10]                    = (top[0]+top[1]+1)>>1; 
                                    Pblk[VRP][ 2] =    Pblk[VRP][11]    = (top[1]+top[2]+1)>>1; 
                                                    Pblk[VRP][ 3]    = (top[2]+top[3]+1)>>1; 
    Pblk[VRP][12]                                                     = (lft[2]+lft[1]*2+lft[0]+2)>>2; 
    Pblk[VRP][ 4] = Pblk[VRP][13]                                    = (lft[0]+top[-1]*2+top[0]+2)>>2; 
                    Pblk[VRP][ 5] = Pblk[VRP][14]                    = (top[-1]+top[0]*2+top[1]+2)>>2; 
                                    Pblk[VRP][ 6] = Pblk[VRP][15]    = (top[0]+top[1]*2+top[2]+2)>>2; 
                                                    Pblk[VRP][ 7]    = (top[1]+top[2]*2+top[3]+2)>>2; 
        d[VRP] = GetSad4x4(src,Pblk[VRP],4) + VSICsta.IntraNonDCPenalty4x4;

// HOR_DOWN_PRED
    Pblk[HDP][12]                                                    = (lft[3]+lft[2]+1)>>1; 
                    Pblk[HDP][13]                                      = (lft[3]+lft[2]*2+lft[1]+2)>>2; 
    Pblk[HDP][ 8] =                    Pblk[HDP][14]                    = (lft[2]+lft[1]+1)>>1; 
                    Pblk[HDP][ 9] =                    Pblk[HDP][15]    = (lft[2]+lft[1]*2+lft[0]+2)>>2; 
    Pblk[HDP][ 4] =                    Pblk[HDP][10]                    = (lft[1]+lft[0]+1)>>1; 
                    Pblk[HDP][ 5] =                    Pblk[HDP][11]    = (lft[1]+lft[0]*2+top[-1]+2)>>2; 
    Pblk[HDP][ 0] =                    Pblk[HDP][ 6]                    = (lft[0]+top[-1]+1)>>1; 
                    Pblk[HDP][ 1] =                    Pblk[HDP][ 7]    = (lft[0]+top[-1]*2+top[0]+2)>>2; 
                                    Pblk[HDP][ 2]                    = (top[-1]+top[0]*2+top[1]+2)>>2; 
                                                    Pblk[HDP][ 3]    = (top[0]+top[1]*2+top[2]+2)>>2; 
        d[HDP] = GetSad4x4(src,Pblk[HDP],4) + VSICsta.IntraNonDCPenalty4x4;
    }}

    d[pmode] -= LutMode[LUTMODE_INTRA_NONPRED];

    int tempDC = d[2];
    for(j=0;j<9;j++){ if(IntraSkipMask&(INTRAMASK4X4<<j)) d[j] = 0x77777777; }
    for(j=0;j<9;j++)
    { 
        if((j!=2) && (d[j] != 0x77777777))
        {
            nonDCmodePicked = 1;
            break;
        }
    }
    // If no other mode than DC is selected when DC is masked
    // we default back to DC mode
    if (nonDCmodePicked==0)
    {
        d[2] = tempDC;
    }
    m = 0; 
    for(j=1;j<9;j++) if(d[j]<d[m]) m = j; 
    m = (d[m]==d[2])?2:m;
    *mode = m;
    *dist = d[m]+LutMode[LUTMODE_INTRA_NONPRED];
    
    top[-1] = lft[3];
#if 0
    lft[0] = Pblk[m][3];    lft[1] = Pblk[m][7];
    lft[2] = Pblk[m][11];    lft[3] = Pblk[m][15];
    top[0] = Pblk[m][12];    top[1] = Pblk[m][13];
    top[2] = Pblk[m][14];    // top[3] = Pblk[m][15];
#else // from original
    lft[0] = src[0x03];    lft[1] = src[0x13];    lft[2] = src[0x23];    lft[3] = src[0x33];
    top[0] = src[0x30];    top[1] = src[0x31];    top[2] = src[0x32];    // top[3] = src[0x33];
#endif
    return 0;
}


/*********************************************************************************/
int MEforGen75::Intra4x4SearchUnit( )
/*********************************************************************************/
{
    int        x, y, d, dist;
    U8      *src, *lft, *top, pmode;
    int        IsEdgeBlock = 0;
    U8        tmptop[4];

    if(!(Vsta.IntraAvail&INTRA_AVAIL_AEF)) IsEdgeBlock |= NO_AVAIL_A;
    if(!(Vsta.IntraAvail&INTRA_AVAIL_B)) IsEdgeBlock |= NO_AVAIL_B;
    if(!(Vsta.IntraAvail&INTRA_AVAIL_C)) IsEdgeBlock |= NO_AVAIL_C;
    if(!(Vsta.IntraAvail&INTRA_AVAIL_D)) IsEdgeBlock |= NO_AVAIL_D;

    src = SrcMB;
    lft = VSICsta.LeftPels;
    top = VSICsta.TopMinus2Pels+2;

    dist = LutMode[LUTMODE_INTRA_4x4];

    x = (VSICsta.LeftModes&15);
    y = (VSICsta.TopModes&15);
    pmode = (IsEdgeBlock&5) ? DCP : ((x<y)?x:y);
    IntraPredict4x4(src,    lft, top,    IsEdgeBlock&13, pmode, &Intra4x4PredMode[ 0], &d );    dist += d;
    x = Intra4x4PredMode[ 0];
    y = ((VSICsta.TopModes>>4)&15);
    pmode = (IsEdgeBlock&4) ? DCP : ((x<y)?x:y);
    IntraPredict4x4(src+ 4, lft, top+ 4, IsEdgeBlock&4, pmode, &Intra4x4PredMode[ 1], &d );    dist += d;
    x = Intra4x4PredMode[ 1];
    y = ((VSICsta.TopModes>>8)&15);
    pmode = (IsEdgeBlock&4) ? DCP : ((x<y)?x:y);
    IntraPredict4x4(src+ 8, lft, top+ 8, IsEdgeBlock&4, pmode, &Intra4x4PredMode[ 4], &d );    dist += d;
    x = Intra4x4PredMode[ 4];
    y = ((VSICsta.TopModes>>12)&15);
    pmode = (IsEdgeBlock&4) ? DCP : ((x<y)?x:y);
    IntraPredict4x4(src+12, lft, top+12, IsEdgeBlock&6, pmode, &Intra4x4PredMode[ 5], &d );    dist += d;
    top[15] = lft[3]; src += 64; lft += 4;
    MFX_INTERNAL_CPY(tmptop,top+8,4);
    x = ((VSICsta.LeftModes>>4)&15);
    y = Intra4x4PredMode[ 0];
    pmode = (IsEdgeBlock&1) ? DCP : ((x<y)?x:y);
    IntraPredict4x4(src,    lft, top,    IsEdgeBlock&1, pmode, &Intra4x4PredMode[ 2], &d );    dist += d;
    x = Intra4x4PredMode[ 2];
    y = Intra4x4PredMode[ 1];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+ 4, lft, top+ 4, 2,                pmode, &Intra4x4PredMode[ 3], &d );    dist += d;
    MFX_INTERNAL_CPY(top+8,tmptop,4);
    x = Intra4x4PredMode[ 3];
    y = Intra4x4PredMode[ 4];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+ 8, lft, top+ 8, 0,                pmode, &Intra4x4PredMode[ 6], &d );    dist += d;
    x = Intra4x4PredMode[ 6];
    y = Intra4x4PredMode[ 5];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+12, lft, top+12, 2,             pmode, &Intra4x4PredMode[ 7], &d );    dist += d;
    top[15] = lft[3]; src += 64; lft += 4; 
    x = ((VSICsta.LeftModes>>8)&15);
    y = Intra4x4PredMode[ 2];
    pmode = (IsEdgeBlock&1) ? DCP : ((x<y)?x:y);
    IntraPredict4x4(src,    lft, top,    IsEdgeBlock&1, pmode, &Intra4x4PredMode[ 8], &d );    dist += d;
    x = Intra4x4PredMode[ 8];
    y = Intra4x4PredMode[ 3];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+ 4, lft, top+ 4, 0,                pmode, &Intra4x4PredMode[ 9], &d );    dist += d;
    x = Intra4x4PredMode[ 9];
    y = Intra4x4PredMode[ 6];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+ 8, lft, top+ 8, 0,                pmode, &Intra4x4PredMode[12], &d );    dist += d;
    x = Intra4x4PredMode[12];
    y = Intra4x4PredMode[ 7];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+12, lft, top+12, 2,             pmode, &Intra4x4PredMode[13], &d );    dist += d;
    top[15] = lft[3]; src += 64; lft += 4; 
    MFX_INTERNAL_CPY(tmptop,top+8,4);
    x = ((VSICsta.LeftModes>>12)&15);
    y = Intra4x4PredMode[ 8];
    pmode = (IsEdgeBlock&1) ? DCP : ((x<y)?x:y);
    IntraPredict4x4(src,    lft, top,    IsEdgeBlock&1, pmode, &Intra4x4PredMode[10], &d );    dist += d;
    x = Intra4x4PredMode[10];
    y = Intra4x4PredMode[ 9];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+ 4, lft, top+ 4, 2,                pmode, &Intra4x4PredMode[11], &d );    dist += d;
    MFX_INTERNAL_CPY(top+8,tmptop,4);
    x = Intra4x4PredMode[11];
    y = Intra4x4PredMode[12];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+ 8, lft, top+ 8, 0,                pmode, &Intra4x4PredMode[14], &d );    dist += d;
    x = Intra4x4PredMode[14];
    y = Intra4x4PredMode[13];
    pmode = ((x<y)?x:y);
    IntraPredict4x4(src+12, lft, top+12, 2,             pmode, &Intra4x4PredMode[15], &d );    dist += d;

    if(dist<(DistIntra4or8&MBMAXDIST)) DistIntra4or8 = dist;//intra 4x4 saturate to 0xffff hsd 3993165
    return 0;
}

 

/*********************************************************************************/
int MEforGen75::Intra8x8SearchUnit( )
/*********************************************************************************/
{
    int        x, y, d, dist;
    U8      *src, *lft, *top, pmode;
    int        IsEdgeBlock = 0;
    U8        lft_pixel[16], top_pixel[32];        // shlee

    if(!(Vsta.IntraAvail&INTRA_AVAIL_AEF)) IsEdgeBlock |= NO_AVAIL_A;
    if(!(Vsta.IntraAvail&INTRA_AVAIL_B)) IsEdgeBlock |= NO_AVAIL_B;
    if(!(Vsta.IntraAvail&INTRA_AVAIL_C)) IsEdgeBlock |= NO_AVAIL_C;
    if(!(Vsta.IntraAvail&INTRA_AVAIL_D)) IsEdgeBlock |= NO_AVAIL_D;

    src = SrcMB;
    // shlee
    //lft = Vsta.LeftPels;
    //top = Vsta.TopMinus8Pels+8;
    MFX_INTERNAL_CPY(lft_pixel, VSICsta.LeftPels, 16);
    MFX_INTERNAL_CPY(top_pixel+6, VSICsta.TopMinus2Pels, 26);
    lft = lft_pixel;
    top = top_pixel + 8;

    dist = LutMode[LUTMODE_INTRA_8x8];
    x = (VSICsta.LeftModes&15);
    y = (VSICsta.TopModes&15);
    pmode = (IsEdgeBlock&5) ? DCP : ((x<y)?x:y);

    int LeftDisableFlag = IsEdgeBlock&NO_AVAIL_A;
    int TopDisableFlag = IsEdgeBlock&NO_AVAIL_B;
    
    // 2st 8x8 depends on A,B,D
    IntraPredict8x8(src,    lft, top,    IsEdgeBlock&(NO_AVAIL_A | NO_AVAIL_B | NO_AVAIL_D), pmode, &Intra8x8PredMode[0], &d );
    dist += d;
    x = Intra8x8PredMode[0];
    y = ((VSICsta.TopModes>>8)&15);
    pmode = (IsEdgeBlock&4) ? DCP : ((x<y)?x:y);

    // 2nd 8x8 depends on B, C.  D is set based on B avail
    IntraPredict8x8(src+ 8, lft, top+ 8, (IsEdgeBlock&(NO_AVAIL_B | NO_AVAIL_C)) | (TopDisableFlag ? NO_AVAIL_D : 0), pmode, &Intra8x8PredMode[1], &d );
    dist += d;
    x = ((VSICsta.LeftModes>>8)&15);
    y = Intra8x8PredMode[0];
    pmode = (IsEdgeBlock&1) ? DCP : ((x<y)?x:y);
    top[15] = lft[7]; src += 128; lft += 8; 
    
    // 3rd 8x8 depends on A.  D is set based on A avail
    IntraPredict8x8(src,    lft, top, (IsEdgeBlock&NO_AVAIL_A) | (LeftDisableFlag ? NO_AVAIL_D : 0), pmode, &Intra8x8PredMode[2], &d );
    dist += d;    
    x = Intra8x8PredMode[2];
    y = Intra8x8PredMode[1];
    pmode = ((x<y)?x:y);
    
    // 4th 8x8 disables C
    IntraPredict8x8(src+ 8, lft, top+ 8, NO_AVAIL_C, pmode, &Intra8x8PredMode[3], &d );
    dist += d;

    //intra 8x8 saturate to 0xffff hsd 3993165, or 0x10000 to distinguish from intra4x4
    if(dist<=(DistIntra4or8&MBMAXDIST)) DistIntra4or8 = (dist | 0x10000);
    return 0;
}

/*********************************************************************************/
int MEforGen75::Intra8x8ChromaSearchUnit( )
/*********************************************************************************/
{
    U8 i, j, mode;
    
    U8  ref[128];
    U32    d, dist, tempDCsad = 0;    
    I32 s0[2][4],s1[2],s2[2],s3[2],s4[2],s[2],k;
    U8    left[2][9], *lft[2];
    U8 top[2][8];

    // Load top-neighboring pixels
    if(Vsta.IntraAvail & INTRA_AVAIL_B) 
    {
        for(int i=0; i<8; i++){
            top[0][i] = VSICsta.TopChromaPels[i*2+0];
            top[1][i] = VSICsta.TopChromaPels[i*2+1];
        }
    }

    // Load left-neighboring pixels
    if(Vsta.IntraAvail & INTRA_AVAIL_D) 
    {
        left[0][0] = VSICsta.NeighborChromaPel&0xFF;
        left[1][0] = VSICsta.NeighborChromaPel>>8;
    }

    if(Vsta.IntraAvail& INTRA_AVAIL_AE) 
    {
        for(i = 0; i < 8; i++)
        {
            left[0][i+1] = VSICsta.LeftChromaPels[i*2+0];
            left[1][i+1] = VSICsta.LeftChromaPels[i*2+1];
        }
    }
    lft[0] = &left[0][1];
    lft[1] = &left[1][1];

    // Mode decision
    for(i = 0; i<2; i++) 
    {
        s1[i] = lft[i][0] + lft[i][1] + lft[i][2] + lft[i][3] + 2;
        s2[i] = lft[i][4] + lft[i][5] + lft[i][6] + lft[i][7] + 2;
        s3[i] = top[i][0] + top[i][1] + top[i][2] + top[i][3] + 2;
        s4[i] = top[i][4] + top[i][5] + top[i][6] + top[i][7] + 2;

        s0[i][0] = !(Vsta.IntraAvail & INTRA_AVAIL_AE) ? 
            (!(Vsta.IntraAvail & INTRA_AVAIL_B) ? 128 : (s3[i]>>2)) :
            (!(Vsta.IntraAvail & INTRA_AVAIL_B) ? (s1[i]>>2) : ((s1[i]+s3[i])>>3));
        s0[i][1] = !(Vsta.IntraAvail & INTRA_AVAIL_B) ? 
            (!(Vsta.IntraAvail & INTRA_AVAIL_AE) ? 128 : (s1[i]>>2)) :(s4[i]>>2);
        s0[i][2] = !(Vsta.IntraAvail & INTRA_AVAIL_AE) ? 
            (!(Vsta.IntraAvail & INTRA_AVAIL_B) ? 128 : (s3[i]>>2)) : (s2[i]>>2);
        s0[i][3] = !(Vsta.IntraAvail & INTRA_AVAIL_AE) ? 
            (!(Vsta.IntraAvail & INTRA_AVAIL_B) ? 128 : (s4[i]>>2)) :
            (!(Vsta.IntraAvail & INTRA_AVAIL_B) ? (s2[i]>>2) : ((s2[i]+s4[i])>>3));
    }

    // DC
    for(i=0; i<4; i++)
    {
        memset(ref+(i<<4),s0[0][0],4);
        memset(ref+(i<<4)+4,s0[0][1],4);
        memset(ref+(i<<4)+8,s0[1][0],4);
        memset(ref+(i<<4)+12,s0[1][1],4);
    }

    for(i=4; i<8; i++)
    {
        memset(ref+(i<<4),s0[0][2],4);
        memset(ref+(i<<4)+4,s0[0][3],4);
        memset(ref+(i<<4)+8,s0[1][2],4);
        memset(ref+(i<<4)+12,s0[1][3],4);
    }

    dist = tempDCsad = GetSadChroma(SrcUV,ref,128);
    mode = CHROMA_PRED8x8_DC;
    if((IntraSkipMask&(INTRAMASK8X8UV<<PRED16x16_DC))) dist = 0x77777777;

    // VERTICAL
    if(!(IntraSkipMask&(INTRAMASK8X8UV<<PRED16x16_VERT)))
    {
        if(Vsta.IntraAvail & INTRA_AVAIL_B) 
        {
            for(i=0; i<8; i++)
            {    
                MFX_INTERNAL_CPY(ref+(i<<4),top[0],8);
                MFX_INTERNAL_CPY(ref+(i<<4)+8,top[1],8);
            }
            d = GetSadChroma(SrcUV,ref,128) + LutMode[LUTMODE_INTRA_CHROMA]*1;
            if(d<dist)
            { 
                dist = d; 
                mode = CHROMA_PRED8x8_VERT; 
            }
        }
    }

    // HORIZONTAL
    if(!(IntraSkipMask&(INTRAMASK8X8UV<<PRED16x16_HORZ)))
    {
        if(Vsta.IntraAvail & INTRA_AVAIL_AE) 
        {
            for(i=0; i<8; i++)
            {
                memset(ref+(i<<4),lft[0][i],8);
                memset(ref+(i<<4)+8,lft[1][i],8);
            }
            d = GetSadChroma(SrcUV,ref,128) + LutMode[LUTMODE_INTRA_CHROMA]*1;
            if(d<dist)
            { 
                dist = d; 
                mode = CHROMA_PRED8x8_HORZ; 
            }
        }
    }

    // PLANE
    if(!(IntraSkipMask&(INTRAMASK8X8UV<<PRED16x16_PLANAR)))
    {
        if((Vsta.IntraAvail & INTRA_AVAIL_AE) && (Vsta.IntraAvail & INTRA_AVAIL_B) && (Vsta.IntraAvail & INTRA_AVAIL_D))
        {
            s2[0] =( (1*(top[0][4]-top[0][2])+2*(top[0][5]-top[0][1])
                   +  3*(top[0][6]-top[0][0])+4*(top[0][7]-lft[0][-1]))*17 + 16)>>5;
            s2[1] =( (1*(top[1][4]-top[1][2])+2*(top[1][5]-top[1][1])
                   +  3*(top[1][6]-top[1][0])+4*(top[1][7]-lft[1][-1]))*17 + 16)>>5;
            s1[0] =( (1*(lft[0][4]-lft[0][2])+2*(lft[0][5]-lft[0][1])
                   +  3*(lft[0][6]-lft[0][0])+4*(lft[0][7]-lft[0][-1]))*17 + 16)>>5;
            s1[1] =( (1*(lft[1][4]-lft[1][2])+2*(lft[1][5]-lft[1][1])
                   +  3*(lft[1][6]-lft[1][0])+4*(lft[1][7]-lft[1][-1]))*17 + 16)>>5;

            s[0] = ((lft[0][7]+top[0][7]+1)<<4) - ((s1[0]+s2[0])<<2);    
            s[1] = ((lft[1][7]+top[1][7]+1)<<4) - ((s1[1]+s2[1])<<2);

            for(j=0; j<128; j+=16)
            {
                k = (s[0]+=s1[0]);
                for(i=0; i<8; i++)
                {
                    k += s2[0];
                    ref[j+i] = (k<0) ? 0 : ((k>=0x2000) ? 255 : (k>>5));
                }

                k = (s[1]+=s1[1]);
                for(i=8; i<16; i++)
                {
                    k += s2[1];
                    ref[j+i] = (k<0) ? 0 : ((k>=0x2000) ? 255 : (k>>5));
                }
            }
            d = GetSadChroma(SrcUV,ref,128) + LutMode[LUTMODE_INTRA_CHROMA]*2;
            if(d<dist)
            { 
                dist = d; 
                mode = CHROMA_PRED8x8_PLANAR; 
            }
        }
    }

    // If no other mode than DC is selected when DC is masked
    // we default back to DC mode
    if (dist == 0x77777777)
    {
        dist = tempDCsad;
        mode = CHROMA_PRED8x8_DC; 
    }

    BestIntraChromaDist = dist;
    return mode;
}

/*********************************************************************************/
U32 MEforGen75::GetSadChroma(U8 *in_Src, U8 *in_Blk, U32 in_N)
/*********************************************************************************/
{
    U32    i, j, d = 0;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 2; j++)
        {
            d += GetSad4x4(in_Src + (4*i + 64*j), in_Blk + (4*i + 64*j), 16);
        }
    }

    return d;
}


/*********************************************************************************/
int MEforGen75::IntraPredict8x8(U8   *src, U8   *lft, U8   *top, int edge, U8 pmode, U8   *mode, I32 *dist)
/*********************************************************************************/
{
    U8      tmp[26];
    U8      Pblk[9][64];
    U8      *pp; 
    int        i,j,m;
     int        d[9];
    U8      nonDCmodePicked = 0;
    memset(d,0x77,9*sizeof(int));

    // if there is NO top
    if(edge&NO_AVAIL_B){
        memset(top,DEFAULT_DC,16);
        // if no TOP but there is LEFT, UL pixel copied from LEFT[0]
        if(!(edge&NO_AVAIL_A))
            if(edge&NO_AVAIL_D)    // only copy if UL not available
                top[-1] = lft[0];
    }
    // if there is TOP, but not TOP RIGHT, copy TOP to TOP RIGHT
    if((edge&NO_AVAIL_C) && !(edge&NO_AVAIL_B))
        memset(top+8,top[7],8);

    // if no LEFT
    if(edge&NO_AVAIL_A){
        memset(lft,DEFAULT_DC,8);
        if(!(edge&NO_AVAIL_B))     // if no LEFT and there is TOP, UL pixel copied from TOP[0]
            if(edge&NO_AVAIL_D)
                top[-1] = top[0];

    }

    // Low Pass For Intra8x8 Pblks    8.3.2.2.1 Reference sample filtering process for Intra_8x8 sample prediction
    {
        // Top Left
        if (!(edge&NO_AVAIL_D))
        {
            if ((edge&NO_AVAIL_A) || (edge&NO_AVAIL_B))    // if no Top or no Left
            {
                if (!(edge&NO_AVAIL_B))    // top available
                {
                     tmp[ 0] = ( ((top[-1]*3)+ top[0] +2) >> 2);
                }
                else
                {
                    if (!(edge&NO_AVAIL_A))    // no Top and has Left
                        tmp[ 0] = ( ((top[-1]*3)+ lft[0] +2) >> 2);
                    else    // no Top and no Left
                        tmp[ 0] = top[-1];
                }
            }
            else    // Left and Top are available
                tmp[ 0] = ((top[0] + (top[-1]<<1)+ lft[0] + 2 )>>2);
        }
        
        
        // Top
        if (!(edge&NO_AVAIL_D))    // Top Left available
        {
            tmp[ 1] = ((top[-1]+ (top[0]<<1) + top[1] + 2)>>2);
        }
        else
        {
            tmp[ 1] = (((top[0] * 3) + top[1] + 2)>>2);
        }

        tmp[ 2] = ((top[0] + (top[1]<<1) + top[2] + 2)>>2);
        tmp[ 3] = ((top[1] + (top[2]<<1) + top[3] + 2)>>2);
        tmp[ 4] = ((top[2] + (top[3]<<1) + top[4] + 2)>>2);
        tmp[ 5] = ((top[3] + (top[4]<<1) + top[5] + 2)>>2);
        tmp[ 6] = ((top[4] + (top[5]<<1) + top[6] + 2)>>2);
        tmp[ 7] = ((top[5] + (top[6]<<1) + top[7] + 2)>>2);
        tmp[ 8] = ((top[6] + (top[7]<<1) + top[8] + 2)>>2);
        tmp[ 9] = ((top[7] + (top[8]<<1) + top[9] + 2)>>2);
        tmp[10] = ((top[8] + (top[9]<<1) + top[10]+ 2)>>2);
        tmp[11] = ((top[9] + (top[10]<<1)+ top[11]+ 2)>>2);
        tmp[12] = ((top[10]+ (top[11]<<1)+ top[12]+ 2)>>2);
        tmp[13] = ((top[11]+ (top[12]<<1)+ top[13]+ 2)>>2);
        tmp[14] = ((top[12]+ (top[13]<<1)+ top[14]+ 2)>>2);
        tmp[15] = ((top[13]+ (top[14]<<1)+ top[15]+ 2)>>2);
        tmp[16] = ((top[14]+ (top[15]<<1)+ top[15]+ 2)>>2);

        // Left
        if (!(edge&NO_AVAIL_D))    // Top Left available
        {
            tmp[17] = (top[-1]+ (lft[0]<<1) + lft[1] + 2)>>2;
        }
        else
        {
            tmp[17] = ((lft[0]*3) + lft[1] + 2)>>2;
        }
        tmp[18] = (lft[0] + (lft[1]<<1) + lft[2] + 2)>>2;
        tmp[19] = (lft[1] + (lft[2]<<1) + lft[3] + 2)>>2;
        tmp[20] = (lft[2] + (lft[3]<<1) + lft[4] + 2)>>2;
        tmp[21] = (lft[3] + (lft[4]<<1) + lft[5] + 2)>>2;
        tmp[22] = (lft[4] + (lft[5]<<1) + lft[6] + 2)>>2;
        tmp[23] = (lft[5] + (lft[6]<<1) + lft[7] + 2)>>2;
        tmp[24] = (lft[6] + (lft[7]<<1) + lft[7] + 2)>>2;
    }

    // DC_Prediction (O)
    if(edge&NO_AVAIL_A){
        if(edge&NO_AVAIL_B)
            m = top[0];
        else
            m = ((tmp[1]+tmp[2]+tmp[3]+tmp[4]+tmp[5]+tmp[6]+tmp[7]+tmp[8]+4)>>3);
    }
    else{
        m = tmp[17]+tmp[18]+tmp[19]+tmp[20]+tmp[21]+tmp[22]+tmp[23]+tmp[24];
        if(edge&NO_AVAIL_B)
            m = ((m+4)>>3);
        else
            m = ((m+tmp[1]+tmp[2]+tmp[3]+tmp[4]+tmp[5]+tmp[6]+tmp[7]+tmp[8]+8)>>4);
    }

    memset(Pblk[DCP],m,64);
    d[DCP] = GetSad8x8(src,Pblk[DCP]);

// Vertical Prediction (|) 
    if(!(edge&NO_AVAIL_B)){
        pp = Pblk[VER];
        MFX_INTERNAL_CPY(pp,&tmp[1],8);
        MFX_INTERNAL_CPY(pp+ 8,pp, 8);
        MFX_INTERNAL_CPY(pp+16,pp,16);
        MFX_INTERNAL_CPY(pp+32,pp,32);
        d[VER] = GetSad8x8(src,Pblk[VER]) + VSICsta.IntraNonDCPenalty8x8;
    }

// Horizontal Pred (-)
    if(!(edge&NO_AVAIL_A)){
        pp = Pblk[HOR];
        for(i=0;i<8;i++ ){ memset(pp,tmp[17+i],8); pp += 8; }
        d[HOR] = GetSad8x8(src,Pblk[HOR]) + VSICsta.IntraNonDCPenalty8x8;
    }

    if(!(edge&NO_AVAIL_B)){
// DIAG_DOWN_LEFT  (/)
        pp = Pblk[DDL];
        pp[ 0]=                                                            ((tmp[ 1]+tmp[ 3]+(tmp[ 2]<<1)+2)>>2);
        pp[ 1]= pp[ 8]=                                                    ((tmp[ 2]+tmp[ 4]+(tmp[ 3]<<1)+2)>>2);
        pp[ 2]= pp[ 9]=    pp[16]=                                            ((tmp[ 3]+tmp[ 5]+(tmp[ 4]<<1)+2)>>2);
        pp[ 3]= pp[10]=    pp[17]= pp[24]=                                    ((tmp[ 4]+tmp[ 6]+(tmp[ 5]<<1)+2)>>2);
        pp[ 4]= pp[11]=    pp[18]= pp[25]= pp[32]=                            ((tmp[ 5]+tmp[ 7]+(tmp[ 6]<<1)+2)>>2);
        pp[ 5]= pp[12]=    pp[19]= pp[26]=    pp[33]= pp[40]=                    ((tmp[ 6]+tmp[ 8]+(tmp[ 7]<<1)+2)>>2);
        pp[ 6]= pp[13]=    pp[20]= pp[27]=    pp[34]= pp[41]= pp[48]=            ((tmp[ 7]+tmp[ 9]+(tmp[ 8]<<1)+2)>>2);
        pp[ 7]= pp[14]=    pp[21]= pp[28]=    pp[35]= pp[42]= pp[49]= pp[56]= ((tmp[ 8]+tmp[10]+(tmp[ 9]<<1)+2)>>2);
                pp[15]=    pp[22]= pp[29]=    pp[36]= pp[43]= pp[50]= pp[57]= ((tmp[ 9]+tmp[11]+(tmp[10]<<1)+2)>>2);
                        pp[23]= pp[30]=    pp[37]= pp[44]= pp[51]= pp[58]= ((tmp[10]+tmp[12]+(tmp[11]<<1)+2)>>2);
                                pp[31]=    pp[38]= pp[45]= pp[52]= pp[59]= ((tmp[11]+tmp[13]+(tmp[12]<<1)+2)>>2);
                                        pp[39]= pp[46]= pp[53]= pp[60]= ((tmp[12]+tmp[14]+(tmp[13]<<1)+2)>>2);
                                                pp[47]=    pp[54]= pp[61]= ((tmp[13]+tmp[15]+(tmp[14]<<1)+2)>>2);
                                                        pp[55]= pp[62]= ((tmp[14]+tmp[16]+(tmp[15]<<1)+2)>>2);
                                                                pp[63]= ((tmp[15]+tmp[16]+(tmp[16]<<1)+2)>>2);
        d[DDL] = GetSad8x8(src,Pblk[DDL]) + VSICsta.IntraNonDCPenalty8x8;
        
// VERT_LEFT_Pblk
        pp = Pblk[VLP];
        pp[ 0]=                            ((tmp[ 1]+tmp[ 2]+1)>>1);
        pp[ 1]= pp[16]=                    ((tmp[ 2]+tmp[ 3]+1)>>1);
        pp[ 2]= pp[17]= pp[32]=            ((tmp[ 3]+tmp[ 4]+1)>>1);
        pp[ 3]= pp[18]= pp[33]= pp[48]= ((tmp[ 4]+tmp[ 5]+1)>>1);
        pp[ 4]= pp[19]= pp[34]= pp[49]= ((tmp[ 5]+tmp[ 6]+1)>>1);
        pp[ 5]= pp[20]= pp[35]= pp[50]= ((tmp[ 6]+tmp[ 7]+1)>>1);
        pp[ 6]= pp[21]= pp[36]= pp[51]= ((tmp[ 7]+tmp[ 8]+1)>>1);
        pp[ 7]= pp[22]= pp[37]= pp[52]= ((tmp[ 8]+tmp[ 9]+1)>>1);
                pp[23]= pp[38]= pp[53]=    ((tmp[ 9]+tmp[10]+1)>>1);
                        pp[39]= pp[54]=    ((tmp[10]+tmp[11]+1)>>1);
                                pp[55]=    ((tmp[11]+tmp[12]+1)>>1);
        pp[ 8]=                            ((tmp[ 1]+tmp[ 3]+(tmp[ 2]<<1)+2)>>2);
        pp[ 9]= pp[24]=                    ((tmp[ 2]+tmp[ 4]+(tmp[ 3]<<1)+2)>>2);
        pp[10]= pp[25]= pp[40]=            ((tmp[ 3]+tmp[ 5]+(tmp[ 4]<<1)+2)>>2);
        pp[11]= pp[26]= pp[41]= pp[56]= ((tmp[ 4]+tmp[ 6]+(tmp[ 5]<<1)+2)>>2);
        pp[12]= pp[27]= pp[42]= pp[57]= ((tmp[ 5]+tmp[ 7]+(tmp[ 6]<<1)+2)>>2);
        pp[13]= pp[28]= pp[43]= pp[58]= ((tmp[ 6]+tmp[ 8]+(tmp[ 7]<<1)+2)>>2);
        pp[14]= pp[29]= pp[44]= pp[59]= ((tmp[ 7]+tmp[ 9]+(tmp[ 8]<<1)+2)>>2);
        pp[15]= pp[30]= pp[45]= pp[60]= ((tmp[ 8]+tmp[10]+(tmp[ 9]<<1)+2)>>2);
                pp[31]= pp[46]= pp[61]=    ((tmp[ 9]+tmp[11]+(tmp[10]<<1)+2)>>2);
                        pp[47]= pp[62]=    ((tmp[10]+tmp[12]+(tmp[11]<<1)+2)>>2);
                                pp[63]=    ((tmp[11]+tmp[13]+(tmp[12]<<1)+2)>>2);
        d[VLP] = GetSad8x8(src,Pblk[VLP]) + VSICsta.IntraNonDCPenalty8x8;
    }    

    if(!(edge&NO_AVAIL_A)){

        pp = Pblk[HUP];
        pp[ 0]=                            ((tmp[17]+tmp[18]+1)>>1);
        pp[ 1]=                            ((tmp[17]+tmp[19]+(tmp[18]<<1)+2)>>2);
        pp[ 2]= pp[ 8]=                    ((tmp[18]+tmp[19]+1)>>1);
        pp[ 3]= pp[ 9]=                    ((tmp[18]+tmp[20]+(tmp[19]<<1)+2)>>2);
        pp[ 4]= pp[10]= pp[16]=            ((tmp[19]+tmp[20]+1)>>1);
        pp[ 5]= pp[11]= pp[17]=            ((tmp[19]+tmp[21]+(tmp[20]<<1)+2)>>2);
        pp[ 6]= pp[12]= pp[18]= pp[24]= ((tmp[20]+tmp[21]+1)>>1);
        pp[ 7]= pp[13]= pp[19]= pp[25]= ((tmp[20]+tmp[22]+(tmp[21]<<1)+2)>>2);
        pp[32]= pp[14]= pp[20]= pp[26]= ((tmp[21]+tmp[22]+1)>>1);
        pp[33]= pp[15]= pp[21]= pp[27]= ((tmp[21]+tmp[23]+(tmp[22]<<1)+2)>>2);
        pp[34]= pp[40]= pp[22]= pp[28]= ((tmp[22]+tmp[23]+1)>>1);
        pp[35]= pp[41]= pp[23]= pp[29]= ((tmp[22]+tmp[24]+(tmp[23]<<1)+2)>>2);
        pp[36]= pp[42]= pp[48]= pp[30]= ((tmp[23]+tmp[24]+1)>>1);
        pp[37]= pp[43]= pp[49]= pp[31]= ((tmp[23]+tmp[24]+(tmp[24]<<1)+2)>>2);
        pp[38]= pp[44]= pp[50]= pp[56]= 
        pp[39]= pp[45]= pp[51]= pp[57]= 
                pp[46]= pp[52]= pp[58]=    
                pp[47]= pp[53]= pp[59]=    
                        pp[54]= pp[60]=    
                        pp[55]= pp[61]=    pp[62]=    pp[63]=    tmp[24];
        d[HUP] = GetSad8x8(src,Pblk[HUP]) + VSICsta.IntraNonDCPenalty8x8;

    if((!(edge&NO_AVAIL_B)) && (!(edge&NO_AVAIL_D))){
// Mode DIAG_DOWN_RIGHT_PRED
        pp = Pblk[DDR];
        pp[56]=                                                            ((tmp[24]+tmp[22]+(tmp[23]<<1)+2)>>2);
        pp[57]= pp[48]=                                                    ((tmp[23]+tmp[21]+(tmp[22]<<1)+2)>>2);
        pp[58]= pp[49]=    pp[40]=                                            ((tmp[22]+tmp[20]+(tmp[21]<<1)+2)>>2);
        pp[59]= pp[50]=    pp[41]= pp[32]=                                    ((tmp[21]+tmp[19]+(tmp[20]<<1)+2)>>2);
        pp[60]= pp[51]=    pp[42]= pp[33]=    pp[24]=                            ((tmp[20]+tmp[18]+(tmp[19]<<1)+2)>>2);
        pp[61]= pp[52]=    pp[43]= pp[34]=    pp[25]= pp[16]=                    ((tmp[19]+tmp[17]+(tmp[18]<<1)+2)>>2);
        pp[62]= pp[53]=    pp[44]= pp[35]=    pp[26]= pp[17]= pp[ 8]=            ((tmp[18]+tmp[ 0]+(tmp[17]<<1)+2)>>2);
        pp[63]= pp[54]=    pp[45]= pp[36]=    pp[27]= pp[18]= pp[ 9]= pp[ 0]= ((tmp[17]+tmp[ 1]+(tmp[ 0]<<1)+2)>>2);
                pp[55]=    pp[46]= pp[37]=    pp[28]= pp[19]= pp[10]= pp[ 1]= ((tmp[ 0]+tmp[ 2]+(tmp[ 1]<<1)+2)>>2);
                        pp[47]= pp[38]=    pp[29]= pp[20]= pp[11]= pp[ 2]= ((tmp[ 1]+tmp[ 3]+(tmp[ 2]<<1)+2)>>2);
                                pp[39]=    pp[30]= pp[21]= pp[12]= pp[ 3]= ((tmp[ 2]+tmp[ 4]+(tmp[ 3]<<1)+2)>>2);
                                        pp[31]= pp[22]= pp[13]= pp[ 4]= ((tmp[ 3]+tmp[ 5]+(tmp[ 4]<<1)+2)>>2);
                                                pp[23]=    pp[14]= pp[ 5]= ((tmp[ 4]+tmp[ 6]+(tmp[ 5]<<1)+2)>>2);
                                                        pp[15]= pp[ 6]= ((tmp[ 5]+tmp[ 7]+(tmp[ 6]<<1)+2)>>2);
                                                                pp[ 7]= ((tmp[ 6]+tmp[ 8]+(tmp[ 7]<<1)+2)>>2);

        d[DDR] = GetSad8x8(src,Pblk[DDR]) + VSICsta.IntraNonDCPenalty8x8;
// VERT_RIGHT_PRED
        pp = Pblk[VRP];
                                                                pp[56]=    ((tmp[23]+tmp[21]+(tmp[22]<<1) + 2)>>2);
                                                        pp[48]=            ((tmp[22]+tmp[20]+(tmp[21]<<1) + 2)>>2);
                                                pp[40]=            pp[57]=    ((tmp[21]+tmp[19]+(tmp[20]<<1) + 2)>>2);
                                        pp[32]=            pp[49]=            ((tmp[20]+tmp[18]+(tmp[19]<<1) + 2)>>2);
                                pp[24]=            pp[41]=            pp[58]=    ((tmp[19]+tmp[17]+(tmp[18]<<1) + 2)>>2);
                        pp[16]=            pp[33]=            pp[50]=            ((tmp[18]+tmp[ 0]+(tmp[17]<<1) + 2)>>2);
                pp[ 8]=            pp[25]=            pp[42]=            pp[59]=    ((tmp[17]+tmp[ 1]+(tmp[ 0]<<1) + 2)>>2);
        pp[ 0]=            pp[17]=            pp[34]=            pp[51]=            ((tmp[ 0]+tmp[ 1]+1)>>1);
                pp[ 9]=            pp[26]=            pp[43]=            pp[60]=    ((tmp[ 0]+tmp[ 2]+(tmp[ 1]<<1) + 2)>>2);
        pp[ 1]=            pp[18]=            pp[35]=            pp[52]=            ((tmp[ 1]+tmp[ 2]+1)>>1);
                pp[10]=            pp[27]=            pp[44]=            pp[61]=    ((tmp[ 1]+tmp[ 3]+(tmp[ 2]<<1) + 2)>>2);
        pp[ 2]=            pp[19]=            pp[36]=            pp[53]=            ((tmp[ 2]+tmp[ 3]+1)>>1);
                pp[11]=            pp[28]=            pp[45]=            pp[62]=    ((tmp[ 2]+tmp[ 4]+(tmp[ 3]<<1) + 2)>>2);
        pp[ 3]=            pp[20]=            pp[37]=            pp[54]=            ((tmp[ 3]+tmp[ 4]+1)>>1);
                pp[12]=            pp[29]=            pp[46]=            pp[63]=    ((tmp[ 3]+tmp[ 5]+(tmp[ 4]<<1) + 2)>>2);
        pp[ 4]=            pp[21]=            pp[38]=            pp[55]=            ((tmp[ 4]+tmp[ 5]+1)>>1);
                pp[13]=            pp[30]=            pp[47]=                  ((tmp[ 4]+tmp[ 6]+(tmp[ 5]<<1) + 2)>>2);
        pp[ 5]=            pp[22]=            pp[39]=                            ((tmp[ 5]+tmp[ 6]+1)>>1);
                pp[14]=            pp[31]=                                    ((tmp[ 5]+tmp[ 7]+(tmp[ 6]<<1) + 2)>>2);
        pp[ 6]=            pp[23]=                                            ((tmp[ 6]+tmp[ 7]+1)>>1);
                pp[15]=                                                    ((tmp[ 6]+tmp[ 8]+(tmp[ 7]<<1) + 2)>>2);
        pp[ 7]=                                                            ((tmp[ 7]+tmp[ 8]+1)>>1);
        d[VRP] = GetSad8x8(src,Pblk[VRP]) + VSICsta.IntraNonDCPenalty8x8;

// HOR_DOWN_PRED
    
        pp = Pblk[HDP];
        pp[ 7]=                                                            ((tmp[ 5]+tmp[ 7]+(tmp[ 6]<<1)+2)>>2);
        pp[ 6]=                                                            ((tmp[ 4]+tmp[ 6]+(tmp[ 5]<<1)+2)>>2);
        pp[ 5]= pp[15]=                                                    ((tmp[ 3]+tmp[ 5]+(tmp[ 4]<<1)+2)>>2);
        pp[ 4]= pp[14]=                                                    ((tmp[ 2]+tmp[ 4]+(tmp[ 3]<<1)+2)>>2);
        pp[ 3]=    pp[13]= pp[23]=                                            ((tmp[ 1]+tmp[ 3]+(tmp[ 2]<<1)+2)>>2);
        pp[ 2]=    pp[12]= pp[22]=                                            ((tmp[ 0]+tmp[ 2]+(tmp[ 1]<<1)+2)>>2);
        pp[ 1]= pp[11]=    pp[21]= pp[31]=                                    ((tmp[ 1]+tmp[17]+(tmp[ 0]<<1)+2)>>2);
        pp[ 0]= pp[10]=    pp[20]= pp[30]=                                    ((tmp[17]+tmp[ 0]+1)>>1);
                pp[ 9]= pp[19]=    pp[29]= pp[39]=                            ((tmp[18]+tmp[ 0]+(tmp[17]<<1)+2)>>2);
                pp[ 8]= pp[18]=    pp[28]= pp[38]=                            ((tmp[18]+tmp[17]+1)>>1);
                        pp[17]= pp[27]=    pp[37]= pp[47]=                    ((tmp[19]+tmp[17]+(tmp[18]<<1)+2)>>2);
                        pp[16]= pp[26]=    pp[36]= pp[46]=                    ((tmp[19]+tmp[18]+1)>>1);
                                pp[25]= pp[35]=    pp[45]= pp[55]=            ((tmp[20]+tmp[18]+(tmp[19]<<1)+2)>>2);
                                pp[24]= pp[34]=    pp[44]= pp[54]=            ((tmp[20]+tmp[19]+1)>>1);
                                        pp[33]= pp[43]=    pp[53]= pp[63]= ((tmp[21]+tmp[19]+(tmp[20]<<1)+2)>>2);
                                        pp[32]= pp[42]=    pp[52]= pp[62]= ((tmp[21]+tmp[20]+1)>>1);
                                                pp[41]=    pp[51]= pp[61]=    ((tmp[22]+tmp[20]+(tmp[21]<<1)+2)>>2);
                                                pp[40]=    pp[50]= pp[60]=    ((tmp[22]+tmp[21]+1)>>1);
                                                        pp[49]= pp[59]=    ((tmp[23]+tmp[21]+(tmp[22]<<1)+2)>>2);
                                                        pp[48]= pp[58]=    ((tmp[23]+tmp[22]+1)>>1);
                                                                pp[57]=    ((tmp[24]+tmp[22]+(tmp[23]<<1)+2)>>2);
                                                                pp[56]=    ((tmp[24]+tmp[23]+1)>>1);

    
        d[HDP] = GetSad8x8(src,Pblk[HDP]) + VSICsta.IntraNonDCPenalty8x8;
    }}

    d[pmode] -= LutMode[LUTMODE_INTRA_NONPRED];

    int tempDC = d[2];
    for(j=0;j<9;j++){ if(IntraSkipMask&(INTRAMASK8X8<<j)) d[j] = 0x77777777; }
    // If no other mode than DC is selected when DC is masked
    // we default back to DC mode
    for(j=0;j<9;j++)
    { 
        if((j!=2) && (d[j] != 0x77777777))
        {
            nonDCmodePicked = 1;
            break;
        }
    }

    if (nonDCmodePicked==0)
    {
        d[2] = tempDC;
    }
    m = 0; 
    for(j=1;j<9;j++) if(d[j]<d[m]) m = j; 
    m = (d[m]==d[2])?2:m;
    *mode = m;
    *dist = d[m]+LutMode[LUTMODE_INTRA_NONPRED];

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
    return 0;
}


#if 0
// 8x8 intra chroma prediction modes
#define VER_C8           2
#define HOR_C8           1
#define DCP_C8           0
#define PLN_C8           3

/*********************************************************************************/
int MEforGen75::IntraPredChromaUnit( )
/*********************************************************************************/
{
    int        i,j,k,m;
    int        d, dist;
    int        s0[2],s1[2],s2[2];
    U8      *ref = RefMB[0];
    U8      *lft[2],*top[2];
    
    lft[1] = (lft[0] = LeftNBC+1) + 9;
    top[1] = (top[0] = TopNBC) + 16;
    
    for(i = 0; i<2; i++){
        s2[i] = s1[i] = 4;
        for(k=0;k<8;k++){
            s1[i] += lft[i][k]; s2[i] += top[i][k];
        }

        s0[i] = (IsEdgeBlock&EDGE_LEFT) ? 
            ((IsEdgeBlock&EDGE_TOP) ? 128 : (s2[i]>>3)) :
            ((IsEdgeBlock&EDGE_TOP) ? (s1[i]>>3) : ((s1[i]+s2[i])>>4));
    }

    // DC
    for(k=0;k<8;k++){
        memset(ref+(k<<4),s0[0],8);
        memset(ref+(k<<4)+8,s0[1],8);
    }
    dist = GetSadN(SrcMB,ref,128);
    m = DCP_C8;

    // VERTICAL
    for(k=0;k<16;k++){
        MFX_INTERNAL_CPY(ref+(k<<4),top[0],8);
        MFX_INTERNAL_CPY(ref+(k<<4)+8,top[1],8);
    }
    d = GetSadN(SrcMB,ref,128);
    if(d<dist){ dist = d; m = VER_C8; }

    // HORIZONTAL
    for(k=0;k<16;k++){
        memset(ref+(k<<4),lft[0][k],8);
        memset(ref+(k<<4)+8,lft[1][k],8);
    }
    d = GetSadN(SrcMB,ref,128);
    if(d<dist){ dist = d; m = HOR_C8; }

    // PLAIN
    if((IsEdgeBlock&EDGE_LEFT)&&(IsEdgeBlock&EDGE_TOP)){
        s2[0] =( 5*(top[0][4]-top[0][2])+10*(top[0][5]-top[0][1])
               +15*(top[0][6]-top[0][4])+20*(top[0][7]-lft[0][-1]) + 32)>>6;
        s2[1] =( 5*(top[1][4]-top[1][2])+10*(top[1][5]-top[1][1])
               +15*(top[1][6]-top[1][4])+20*(top[1][7]-lft[1][-1]) + 32)>>6;
        s1[0] =( 5*(lft[0][4]-lft[0][2])+10*(lft[0][5]-lft[0][1])
               +15*(lft[0][6]-lft[0][4])+20*(lft[0][7]-lft[0][-1]) + 32)>>6;
        s1[1] =( 5*(lft[1][4]-lft[1][2])+10*(lft[1][5]-lft[1][1])
               +15*(lft[1][6]-lft[1][4])+20*(lft[1][7]-lft[1][-1]) + 32)>>6;

        s0[0] = ((lft[0][7]+top[0][7]+1)<<4) - ((s1[0]+s2[0])<<2);
        s0[1] = ((lft[1][7]+top[1][7]+1)<<4) - ((s1[1]+s2[1])<<2);

        for(j=0;j<128;j+=16){
            k = (s0[0]+=s1[0]);
            for(i=0;i<8;i++){
                k += s2[0];
                ref[j+i] = (k<0) ? 0 : ((k>=0x2000) ? 255 : (k>>5));
            }
            k = (s0[1]+=s1[1]);
            for(i=8;i<16;i++){
                k += s2[1];
                ref[j+i] = (k<0) ? 0 : ((k>=0x2000) ? 255 : (k>>5));
            }
        }
        d = GetSadN(SrcMB,ref,128);
        if(d<dist){ dist = d; m = PLN_C8; }
    }
    IntraMode[16] = m; 
    return 0;
}

#endif