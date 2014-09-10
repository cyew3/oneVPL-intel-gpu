//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_H265_PAQ)

#include <math.h>
#include "vm_debug.h"
#include "mfx_h265_paq.h"
#include "mfx_h265_optimization.h"

#define NGV_ALIGN(X, ...) ALIGN_DECL(X) __VA_ARGS__

namespace H265Enc {

    /* primitivies of ME FOR PAQ */
    void ME_SUM_cross_8x8_C(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);
    void ME_SUM_cross_8x8_SSE(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);
    void ME_SUM_cross_8x8_Intrin_SSE2(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);

    void ME_SUM_Line_8x8_C(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);
    void ME_SUM_Line_8x8_SSE(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);
    void ME_SUM_Line_8x8_Intrin_SSE2(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);

    void ME_SUM_cross_C(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);
    void ME_SUM_cross_SSE(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);
    void ME_SUM_cross_Intrin_SSE2(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt);

    void ME_SUM_Line_C(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);
    void ME_SUM_Line_SSE(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);
    void ME_SUM_Line_Intrin_SSE2(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt);

    void    calcLimits(Ipp32u xLoc, Ipp32u yLoc, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup,
        Ipp32s *limitYdown, Ipp8u *objFrame, Ipp8u *refFrame, ImDetails inData, Ipp32u fPos);
    void    calcLimits2(Ipp32u xLoc, Ipp32u yLoc, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup,
        Ipp32s *limitYdown, Ipp8u *objFrame, Ipp8u *refFrame, ImDetails inData, MVector *MV, Ipp32u fPos);
    void    calcLimits3(Ipp32u xLoc, Ipp32u yLoc, ImDetails inData, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup, Ipp32s *limitYdown,
        Ipp8u *objFrame, Ipp8u *refFrame, MVector *MV, Ipp32u fPos);

    void    correctMVectorOpt(MVector *MV, Ipp32s xpos, Ipp32s ypos, ImDetails inData);

    void    halfpixel(MVector *MV, Ipp32u *bestSAD, Ipp8u* curY, Ipp8u* refY, Ipp32u blockSize, Ipp32u fPos, Ipp32u wBlocks, Ipp32u xLoc,
        Ipp32u yLoc, Ipp32u offset, VidRead *videoIn, Ipp8u **pRslt, Ipp32u *Rstride, Ipp32u exWidth,
        NGV_Bool q, MVector *stepq, Ipp32u accuracy, Ipp64f *Rs, Ipp64f *Cs);
    void    quarterpixel(VidRead *videoIn, Ipp32u blockSize, MVector *MV, MVector tMV, Ipp32u offset, Ipp8u* fCur, Ipp8u* fRef,
        Ipp32u *bestSAD, Ipp8u **pRslt, Ipp32u *Rstride, Ipp32u exWidth);

    void    corner(Ipp8u*    dd, Ipp8u* ss, Ipp32u exWidth, Ipp32u blockSize);
    void    top(Ipp8u* dd, Ipp8u* ss1, Ipp32u exWidth, Ipp32u blockSize, Ipp32u filter);
    void    left(Ipp8u* dd, Ipp8u* ss1, Ipp32u exWidth, Ipp32u blockSize, Ipp32u filter);

    NGV_Bool    searchMV(MVector MV, Ipp8u* curY, Ipp8u* refY, Ipp32u blockSize, Ipp32u fPos, Ipp32u xLoc, Ipp32u yLoc, Ipp32u *bestSAD,
        Ipp32u *distance, Ipp32u exWidth, Ipp32u *preSAD);
    NGV_Bool    searchMV2(MVector MV, Ipp8u* curY, Ipp8u* refY, ImDetails inData, Ipp32u fPos,
        Ipp32u xLoc, Ipp32u yLoc, Ipp32u *bestSAD, Ipp32u *distance, Ipp32u *preSAD);
    Ipp32u  searchMV3(MVector Nmv, Ipp8u* curY, Ipp8u* refY, Ipp32u exWidth1, Ipp32u exWidth2);

    Ipp64f  Dist(MVector vector);

    NGV_Bool ShotDetect(TSCstat *preAnalysis, Ipp32s position);

    Ipp32u SAD_8x8_Block_SSE(Ipp8u* src, Ipp8u* ref, Ipp32u sPitch, Ipp32u rPitch);
    Ipp32u SAD_8x8_Block_Intrin_SSE2(Ipp8u* src, Ipp8u* ref, Ipp32u sPitch, Ipp32u rPitch);

    typedef int DIR;
    enum                        {forward, backward, average};


    //#define TEST_ASM
#ifdef TEST_ASM
#define SAD_8x8_Block SAD_8x8_Block_SSE
#define LoadShufStore_ObjFrame LoadShufStore_ObjFrame_SSE4
#define SAD_8x8_BlocksAtTime SAD_8x8_BlocksAtTime_SSE4
#define ME_SUM_cross_8x8 ME_SUM_cross_8x8_SSE
#define ME_SUM_Line_8x8 ME_SUM_Line_8x8_SSE
#define ME_SUM_cross ME_SUM_cross_SSE
#define ME_SUM_Line ME_SUM_Line_SSE
#else
#define SAD_8x8_Block SAD_8x8_Block_Intrin_SSE2
#define LoadShufStore_ObjFrame LoadShufStore_ObjFrame_SSE4
#define SAD_8x8_BlocksAtTime SAD_8x8_BlocksAtTime_SSE4
#define ME_SUM_cross_8x8 ME_SUM_cross_8x8_Intrin_SSE2
#define ME_SUM_Line_8x8 ME_SUM_Line_8x8_Intrin_SSE2
#define ME_SUM_cross ME_SUM_cross_Intrin_SSE2
#define ME_SUM_Line ME_SUM_Line_Intrin_SSE2
#endif

    double    filter[4]            =    {0.94,0.5,0.5,0.95};
    double    conFil[4]            =    {0.06,0.5,0.5,0.05};
    double    filter2[4]            =    {0.67,0.95,0.73,0.82};
    double    conFil2[4]            =    {0.33,0.05,0.23,0.18};
    double    filter3[4]            =    {1.0,1.0,1.0,1.0};
    MVector    zero                =    {0,0};

    void LoadShufStore_ObjFrame_C(Ipp8u* objFrame, Ipp32s ExtWidth, Ipp8u* pTempArray1)
    {
        objFrame, ExtWidth, pTempArray1;
        /* unused for C version */
    }

    /* pTempArray1 must be 16-byte aligned */
    void LoadShufStore_ObjFrame_SSE4(Ipp8u* objFrame, Ipp32s ExtWidth, Ipp8u* pTempArray1)
    {
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

        xmm0 = _mm_loadl_epi64((__m128i*)(objFrame + 0*ExtWidth));
        xmm1 = _mm_loadl_epi64((__m128i*)(objFrame + 1*ExtWidth));
        xmm2 = _mm_loadl_epi64((__m128i*)(objFrame + 2*ExtWidth));
        xmm3 = _mm_loadl_epi64((__m128i*)(objFrame + 3*ExtWidth));
        xmm4 = _mm_loadl_epi64((__m128i*)(objFrame + 4*ExtWidth));
        xmm5 = _mm_loadl_epi64((__m128i*)(objFrame + 5*ExtWidth));
        xmm6 = _mm_loadl_epi64((__m128i*)(objFrame + 6*ExtWidth));
        xmm7 = _mm_loadl_epi64((__m128i*)(objFrame + 7*ExtWidth));

        xmm0 = _mm_unpacklo_epi64(xmm0, xmm1);
        xmm2 = _mm_unpacklo_epi64(xmm2, xmm3);
        xmm4 = _mm_unpacklo_epi64(xmm4, xmm5);
        xmm6 = _mm_unpacklo_epi64(xmm6, xmm7);

        _mm_store_si128 ((__m128i*)(pTempArray1 +  0), xmm0);
        _mm_store_si128 ((__m128i*)(pTempArray1 + 16), xmm2);
        _mm_store_si128 ((__m128i*)(pTempArray1 + 32), xmm4);
        _mm_store_si128 ((__m128i*)(pTempArray1 + 48), xmm6);
    }

    /* pTempArray1 and TempSAD must be 16-byte aligned */
    Ipp32u SAD_8x8_BlocksAtTime_SSE4(Ipp16s *TempSAD, Ipp8u* fRef, Ipp8u* objFrame, Ipp32s ExtWidth, Ipp8u* pTempArray1)
    {
        objFrame;

        unsigned int preSAD;
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5;

        /* rows 0-1 from test block */
        xmm5 = _mm_load_si128((__m128i*)(pTempArray1 + 0));

        /* row 0 */
        xmm0 = _mm_loadu_si128((__m128i*)(fRef));        /* load 8 pixels from ref frame */
        xmm2 = _mm_shuffle_epi32(xmm0, 0xf9);            /* >> 4 bytes */
        xmm0 = _mm_mpsadbw_epu8(xmm0, xmm5, 0x00);        /* SAD for lower 4 bytes, 8 positions */
        xmm2 = _mm_mpsadbw_epu8(xmm2, xmm5, 0x01);        /* SAD for upper 4 bytes, 8 positions */
        xmm0 = _mm_add_epi16(xmm0, xmm2);                /* running accumulator for SAD at offsets 0-7 */

        /* row 1 */
        xmm3 = _mm_loadu_si128((__m128i*)(fRef + ExtWidth));
        xmm4 = _mm_shuffle_epi32(xmm3, 0xf9);
        xmm3 = _mm_mpsadbw_epu8(xmm3, xmm5, 0x02);
        xmm4 = _mm_mpsadbw_epu8(xmm4, xmm5, 0x03);
        xmm0 = _mm_add_epi16(xmm0, xmm3);
        xmm0 = _mm_add_epi16(xmm0, xmm4);

        /* rows 2-3 from test block */
        xmm5 = _mm_load_si128((__m128i*)(pTempArray1 + 16));

        /* row 2 */
        xmm1 = _mm_loadu_si128((__m128i*)(fRef + 2*ExtWidth));
        xmm2 = _mm_shuffle_epi32(xmm1, 0xf9);
        xmm1 = _mm_mpsadbw_epu8(xmm1, xmm5, 0x00);
        xmm2 = _mm_mpsadbw_epu8(xmm2, xmm5, 0x01);
        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);

        /* row 3 */
        xmm3 = _mm_loadu_si128((__m128i*)(fRef + 3*ExtWidth));
        xmm4 = _mm_shuffle_epi32(xmm3, 0xf9);
        xmm3 = _mm_mpsadbw_epu8(xmm3, xmm5, 0x02);
        xmm4 = _mm_mpsadbw_epu8(xmm4, xmm5, 0x03);
        xmm0 = _mm_add_epi16(xmm0, xmm3);
        xmm0 = _mm_add_epi16(xmm0, xmm4);

        /* rows 4-5 from test block */
        xmm5 = _mm_load_si128((__m128i*)(pTempArray1 + 32));

        /* row 4 */
        xmm1 = _mm_loadu_si128((__m128i*)(fRef + 4*ExtWidth));
        xmm2 = _mm_shuffle_epi32(xmm1, 0xf9);
        xmm1 = _mm_mpsadbw_epu8(xmm1, xmm5, 0x00);
        xmm2 = _mm_mpsadbw_epu8(xmm2, xmm5, 0x01);
        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);

        /* row 5 */
        xmm3 = _mm_loadu_si128((__m128i*)(fRef + 5*ExtWidth));
        xmm4 = _mm_shuffle_epi32(xmm3, 0xf9);
        xmm3 = _mm_mpsadbw_epu8(xmm3, xmm5, 0x02);
        xmm4 = _mm_mpsadbw_epu8(xmm4, xmm5, 0x03);
        xmm0 = _mm_add_epi16(xmm0, xmm3);
        xmm0 = _mm_add_epi16(xmm0, xmm4);

        /* rows 6-7 from test block */
        xmm5 = _mm_load_si128((__m128i*)(pTempArray1 + 48));

        /* row 6 */
        xmm1 = _mm_loadu_si128((__m128i*)(fRef + 6*ExtWidth));
        xmm2 = _mm_shuffle_epi32(xmm1, 0xf9);
        xmm1 = _mm_mpsadbw_epu8(xmm1, xmm5, 0x00);
        xmm2 = _mm_mpsadbw_epu8(xmm2, xmm5, 0x01);
        xmm0 = _mm_add_epi16(xmm0, xmm1);
        xmm0 = _mm_add_epi16(xmm0, xmm2);

        /* row 7 */
        xmm3 = _mm_loadu_si128((__m128i*)(fRef + 7*ExtWidth));
        xmm4 = _mm_shuffle_epi32(xmm3, 0xf9);
        xmm3 = _mm_mpsadbw_epu8(xmm3, xmm5, 0x02);
        xmm4 = _mm_mpsadbw_epu8(xmm4, xmm5, 0x03);
        xmm0 = _mm_add_epi16(xmm0, xmm3);
        xmm0 = _mm_add_epi16(xmm0, xmm4);

        /* save the 8 16-bit SAD's */
        _mm_store_si128((__m128i*)TempSAD, xmm0);

        /* return minimum SAD: bits 18-16 = min index, bits 15-0 = min value */    
        xmm1 = _mm_minpos_epu16(xmm0);
        preSAD = _mm_cvtsi128_si32(xmm1);

        return preSAD;
    }

    Ipp32u ME_One(VidRead *videoIn, Ipp32u fPos, ImDetails inData, ImDetails outData, 
        imageData *scale, imageData *scaleRef,
        imageData *outImage, NGV_Bool first, DIR fdir, Ipp32u accuracy, Ipp32u thres, Ipp32u pdist)    
    {
        MVector        tMV, ttMV = {0}, *current, *cHalf, *cQuarter, *outMV, Nmv;
        Ipp32s            limitXleft, limitXright, limitYup, limitYdown, x, y;
        Ipp32s            cor;
        Ipp32u            halfLoc, Rstride, tl, a        =    0;
        Ipp32u            *outSAD, preSAD, preDist;
        Ipp64f        *Rs,*Cs;
        Ipp8u            *prLoc, *objFrame, *refFrame, *fRef;

        struct Rsad {
            Ipp32u                   SAD;
            Ipp32u                   distance;
            MVector                  BestMV;
            Ipp32s                   angle;
            Ipp32u                   direction;
        };
        Rsad        range;

        Ipp8u            *pRslt;
        double        *fil;
        double        corFil,*cFil;
        Ipp32u            xLoc        =    (fPos % inData.wBlocks);
        Ipp32u            yLoc        =    (fPos / inData.wBlocks);
        Ipp32u         fPos4       =   yLoc*2*inData.wBlocks*2+xLoc*2;
        Ipp32u            bestSAD        =    INT_MAX;
        Ipp32u            distance    =    INT_MAX;
        Ipp32u            rSAD        =    INT_MAX;
        Ipp32u            lSAD        =    INT_MAX;
        Ipp32u            pen            =    50;
        NGV_Bool        bottom        =    false;
        NGV_Bool        extraBlock    =    fPos != 0 && xLoc == (inData.wBlocks - 1) && outData.wBlocks % 2;
        NGV_Bool        extraLine    =    yLoc == (inData.hBlocks - 1) && outData.hBlocks % 2;
        Ipp32s            xpos = xLoc * inData.block_size_w;
        Ipp32s            ypos = yLoc * inData.block_size_h;
        Ipp32s            ExtWidth = inData.exwidth;
        Ipp32u            offset        =    inData.initPoint + ypos * ExtWidth + xpos;

        ALIGN_DECL(16)    unsigned char TempArray1[4*16];

        objFrame                =    scale->exImage.Y + offset;
        refFrame                =    scaleRef->exImage.Y + offset;

        Rs                        =    scale->DiffRs;
        Cs                        =    scale->DiffCs;

        LoadShufStore_ObjFrame(objFrame, ExtWidth, TempArray1);

        if(fdir == forward)        
        {
            /* quiet KW */
            printf("Error - (fdir == forward) invalid\n");
            return 0xffffffff;
        }
        else    
        {
            current                =    scale->pBackward;
            cHalf                =    scale->pBhalf;
            cQuarter            =    scale->pBquarter;
            outSAD                =    scale->BSAD;
            prLoc                =    scale->Backward.Y + offset;
            outMV                =    outImage->pBackward;
        }
        cHalf[fPos]                =    zero;
        cQuarter[fPos]            =    zero;

        if(!first)
        {
            Nmv.x = current[fPos].x;  /* Future note: potentially unitialized variable current used */
            Nmv.y = current[fPos].y;
            correctMVectorOpt(&(Nmv), xpos, ypos, inData);
            current[fPos].x = Nmv.x;
            current[fPos].y = Nmv.y;
        }
        else
        {
            current[fPos].x        =    0;
            current[fPos].y        =    0;
        }

        cor                        =    current[fPos].x + (current[fPos].y * inData.exwidth);

        if(thres == 0)
        {
            fil                    =    filter;
            cFil                =    conFil;
            if(Rs[fPos4] < 1.0)
                current[fPos].y    =    0;
            if(Cs[fPos4] < 1.0)
                current[fPos].x    =    0;
            calcLimits(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, objFrame, refFrame + cor, inData,  fPos);
        }
        else
        {
            fil                    =    filter2;
            cFil                =    conFil2;
            searchMV(zero, objFrame, refFrame, inData.block_size_w, fPos, xLoc, yLoc, &bestSAD, &distance, inData.exwidth, &preSAD);
            rSAD                =    bestSAD;
            bestSAD                =    INT_MAX;
            distance            =    INT_MAX;

            searchMV(current[fPos], objFrame, refFrame, inData.block_size_w, fPos, xLoc, yLoc, &bestSAD, &distance, inData.exwidth, &preSAD);
            lSAD                =    bestSAD;
            bestSAD                =    INT_MAX;
            distance            =    INT_MAX;


            if(rSAD <= (lSAD * 1.0) && !((current[fPos].x == 0) && (current[fPos].y == 0)))    {
                current[fPos].x    =    0;
                current[fPos].y    =    0;
            }

            calcLimits2(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, objFrame, refFrame + cor, inData, &(current[fPos]), fPos);
        }

        range.SAD        =    INT_MAX; /* k=0 +-15, k=1 +-8, k=2 +-4, k=3 +-2, k=4 +-1*/
        range.BestMV.x    =    0;
        range.BestMV.y    =    0;
        range.distance    =    INT_MAX;

        {
            Ipp32s    distanceY;
            Ipp32s maxLeft = -xpos -(Ipp32s)(inData.block_size_w);
            Ipp32s maxRight = (Ipp32s)(inData.exwidth) -xpos -2*(Ipp32s)inData.block_size_w - 8;
            Ipp32s maxUp = -ypos -(Ipp32s)(inData.block_size_h);
            Ipp32s maxDown = (Ipp32s)(inData.exheight) -ypos -2*(Ipp32s)inData.block_size_h - 8;

            if (((current[fPos].x+limitXleft+4)>=maxLeft)&&((current[fPos].x+limitXright-4)<=maxRight))
            {
                Ipp32s xx = ((limitXright-limitXleft-8)>>3);
                Ipp32s x1;

                ALIGN_DECL(16) Ipp16s TempSAD[8];

                Ipp16s *pTempSAD = TempSAD;
                for(y=limitYup;y<=limitYdown;y++)
                {
                    ttMV.y    =    y + current[fPos].y;

                    if (ttMV.y < maxUp)            ttMV.y    =    maxUp;
                    else if (ttMV.y > maxDown)    ttMV.y    =    maxDown;

                    distanceY = ttMV.y * ttMV.y;
                    for(x=limitXleft;x<(limitXleft+4);x++)
                    {
                        ttMV.x            =    x + current[fPos].x;

                        if (ttMV.x < maxLeft)        ttMV.x    =    maxLeft;
                        else if (ttMV.x > maxRight)    ttMV.x    =    maxRight;

                        fRef    =    refFrame + ttMV.x + (ttMV.y * ExtWidth);
                        preSAD = SAD_8x8_Block(fRef, objFrame, ExtWidth, ExtWidth);

                        if (preSAD <= range.SAD)
                        {
                            preDist        =    (ttMV.x * ttMV.x) + distanceY;
                            if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                            {
                                range.SAD        =    preSAD;
                                range.distance    =    preDist;
                                range.BestMV    =    ttMV;
                            }
                        }
                    }

                    x = (limitXleft+4);

                    for(x1 = 0; x1 < xx; /*(limitXright-4);*/ x1++, x+=8)
                    {
                        ttMV.x    =    x + current[fPos].x;

                        fRef    =    refFrame + ttMV.x + (ttMV.y * ExtWidth);

                        preSAD = SAD_8x8_BlocksAtTime(TempSAD, fRef, objFrame, ExtWidth, TempArray1);

                        // Macro path has extra bits set versus C path
                        // Only the first 16 bits are to be used and should be the same.
                        if ((Ipp16s)preSAD <= (int)range.SAD)
                        {
                            pTempSAD = TempSAD;

                            preSAD = *(pTempSAD++);
                            if (preSAD <= range.SAD)
                            {
                                preDist     =    (ttMV.x * ttMV.x) + distanceY;

                                if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                                {
                                    range.SAD        =    preSAD;
                                    range.distance    =    preDist;
                                    range.BestMV    =    ttMV;
                                }
                            }
                            preSAD = *(pTempSAD++);
                            ttMV.x++;
                            if (preSAD <= range.SAD)
                            {
                                preDist     =    (ttMV.x * ttMV.x) + distanceY;

                                if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                                {
                                    range.SAD        =    preSAD;
                                    range.distance    =    preDist;
                                    range.BestMV    =    ttMV;
                                }
                            }
                            preSAD = *(pTempSAD++);
                            ttMV.x++;
                            if (preSAD <= range.SAD)
                            {
                                preDist     =    (ttMV.x * ttMV.x) + distanceY;

                                if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                                {
                                    range.SAD        =    preSAD;
                                    range.distance    =    preDist;
                                    range.BestMV    =    ttMV;
                                }
                            }
                            preSAD = *(pTempSAD++);
                            ttMV.x++;
                            if (preSAD <= range.SAD)
                            {
                                preDist     =    (ttMV.x * ttMV.x) + distanceY;

                                if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                                {
                                    range.SAD        =    preSAD;
                                    range.distance    =    preDist;
                                    range.BestMV    =    ttMV;
                                }
                            }
                            preSAD = *(pTempSAD++);
                            ttMV.x++;
                            if (preSAD <= range.SAD)
                            {
                                preDist     =    (ttMV.x * ttMV.x) + distanceY;

                                if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                                {
                                    range.SAD        =    preSAD;
                                    range.distance    =    preDist;
                                    range.BestMV    =    ttMV;
                                }
                            }
                            preSAD = *(pTempSAD++);
                            ttMV.x++;
                            if (preSAD <= range.SAD)
                            {
                                preDist     =    (ttMV.x * ttMV.x) + distanceY;

                                if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                                {
                                    range.SAD        =    preSAD;
                                    range.distance    =    preDist;
                                    range.BestMV    =    ttMV;
                                }
                            }
                            preSAD = *(pTempSAD++);
                            ttMV.x++;
                            if (preSAD <= range.SAD)
                            {
                                preDist     =    (ttMV.x * ttMV.x) + distanceY;

                                if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                                {
                                    range.SAD        =    preSAD;
                                    range.distance    =    preDist;
                                    range.BestMV    =    ttMV;
                                }
                            }
                            preSAD = *(pTempSAD);
                            ttMV.x++;
                            if (preSAD <= range.SAD)
                            {
                                preDist     =    (ttMV.x * ttMV.x) + distanceY;

                                if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                                {
                                    range.SAD        =    preSAD;
                                    range.distance    =    preDist;
                                    range.BestMV    =    ttMV;
                                }
                            }
                        }
                    }

                    x = (limitXleft+4)+(xx<<3);
                    for(/*x = (limitXright-4)*/; x <= limitXright; x++)
                    {
                        ttMV.x            =    x + current[fPos].x;

                        if (ttMV.x < maxLeft)        ttMV.x    =    maxLeft;
                        else if (ttMV.x > maxRight)    ttMV.x    =    maxRight;

                        fRef    =    refFrame + ttMV.x + (ttMV.y * ExtWidth);

                        preSAD = SAD_8x8_Block(fRef, objFrame, ExtWidth, ExtWidth);
                        if (preSAD <= range.SAD)
                        {
                            preDist        =    (ttMV.x * ttMV.x) + distanceY;
                            if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                            {
                                range.SAD        =    preSAD;
                                range.distance    =    preDist;
                                range.BestMV    =    ttMV;
                            }
                        }
                    }
                }
            }
            else
            {
                for(y=limitYup;y<=limitYdown;y++)
                {
                    ttMV.y    =    y + current[fPos].y;

                    if (ttMV.y < maxUp)            ttMV.y            =    maxUp;
                    else if (ttMV.y > maxDown)    ttMV.y            =    maxDown;

                    distanceY = (ttMV.y * ttMV.y);
                    for(x=limitXleft;x<=limitXright;x++)
                    {
                        ttMV.x            =    x + current[fPos].x;

                        if (ttMV.x < maxLeft)            ttMV.x    =    maxLeft;
                        else if (ttMV.x > maxRight)        ttMV.x    =    maxRight;

                        fRef            =    refFrame + ttMV.x + (ttMV.y * ExtWidth);

                        preSAD = SAD_8x8_Block(fRef, objFrame, ExtWidth, ExtWidth);
                        if (preSAD <= range.SAD)
                        {
                            preDist        =    (ttMV.x * ttMV.x) + distanceY;
                            if(!((range.distance <= preDist) && (preSAD == range.SAD)))
                            {
                                range.SAD        =    preSAD;
                                range.distance    =    preDist;
                                range.BestMV    =    ttMV;
                            }
                        }
                    }
                }
            }
        }

        outSAD[fPos]            =    range.SAD; /* future note: potentially unitialized elements of outSAD used */
        current[fPos]            =    range.BestMV;

        if(pdist > 2)
            corFil                =    1 - ((double)(pdist - 2)/(double)pdist);
        else
            corFil                =    0;

        if(yLoc == inData.hBlocks - 1)
            bottom                =    false;

        if((fPos > inData.wBlocks) && (xLoc > 0))
        {
            tl                    =    outSAD[fPos] + pen;
            Nmv.x                =    current[fPos - inData.wBlocks - 1].x;
            Nmv.y                =    current[fPos - inData.wBlocks - 1].y;

            correctMVectorOpt(&(Nmv), xpos, ypos, inData);

            fRef                    =    refFrame + Nmv.x + (Nmv.y * ExtWidth);
            preDist                    =    (Nmv.x * Nmv.x) + (Nmv.y * Nmv.y);

            preSAD = SAD_8x8_Block(fRef, objFrame, ExtWidth, ExtWidth);

            if (preSAD <= tl)
            {
                preDist        =    (ttMV.x * ttMV.x) + (ttMV.y * ttMV.y);
                distance        =    preDist;
                tl                =    preSAD;

                if (((Dist(Nmv)*tl/outSAD[fPos] <= Dist(current[fPos])) || (xLoc == inData.wBlocks - 1) || bottom))
                {
                    current[fPos].x    =    Nmv.x;
                    current[fPos].y    =    Nmv.y;
                    outSAD[fPos]    =    tl;
                }
            }
        }

        if(fPos > inData.wBlocks)
        {
            tl                    =    outSAD[fPos] + pen;
            Nmv.x                =    current[fPos - inData.wBlocks].x;
            Nmv.y                =    current[fPos - inData.wBlocks].y;
            correctMVectorOpt(&(Nmv), xpos, ypos, inData);

            fRef                    =    refFrame + Nmv.x + (Nmv.y * ExtWidth);
            preDist                    =    (Nmv.x * Nmv.x) + (Nmv.y * Nmv.y);

            preSAD = SAD_8x8_Block(fRef, objFrame, ExtWidth, ExtWidth);

            if (preSAD <= tl)
            {
                preDist        =    (ttMV.x * ttMV.x) + (ttMV.y * ttMV.y);
                distance        =    preDist;
                tl                =    preSAD;

                if (((Dist(Nmv)*tl/outSAD[fPos] <= Dist(current[fPos])) || (xLoc == 0) || (xLoc == inData.wBlocks - 1) || bottom))
                {
                    current[fPos].x    =    Nmv.x;
                    current[fPos].y    =    Nmv.y;
                    outSAD[fPos]    =    tl;
                }
            }
        }

        if((fPos> inData.wBlocks) && (xLoc < inData.wBlocks - 1))
        {
            tl                    =    outSAD[fPos] + pen;
            Nmv.x                =    current[fPos - inData.wBlocks + 1].x;
            Nmv.y                =    current[fPos - inData.wBlocks + 1].y;
            correctMVectorOpt(&(Nmv), xpos, ypos, inData);

            fRef                    =    refFrame + Nmv.x + (Nmv.y * ExtWidth);
            preDist                    =    (Nmv.x * Nmv.x) + (Nmv.y * Nmv.y);

            preSAD = SAD_8x8_Block(fRef, objFrame, ExtWidth, ExtWidth);

            if (preSAD <= tl)
            {
                preDist        =    (ttMV.x * ttMV.x) + (ttMV.y * ttMV.y);
                distance        =    preDist;
                tl                =    preSAD;

                if (((Dist(Nmv)*tl/outSAD[fPos] <= Dist(current[fPos])) || (xLoc == 0) || bottom))
                {
                    current[fPos].x    =    Nmv.x;
                    current[fPos].y    =    Nmv.y;
                    outSAD[fPos]    =    tl;
                }
            }
        }

        if(xLoc > 0)
        {
            Nmv.x                =    current[fPos - 1].x;
            Nmv.y                =    current[fPos - 1].y;
            tl                    =    outSAD[fPos] + pen;
            distance            =    INT_MAX;
            correctMVectorOpt(&(Nmv), xpos, ypos, inData);
            fRef                    =    refFrame + Nmv.x + (Nmv.y * ExtWidth);
            preDist                    =    (Nmv.x * Nmv.x) + (Nmv.y * Nmv.y);

            preSAD = SAD_8x8_Block(fRef, objFrame, ExtWidth, ExtWidth);

            if (preSAD <= tl)
            {
                preDist        =    (ttMV.x * ttMV.x) + (ttMV.y * ttMV.y);
                distance        =    preDist;
                tl                =    preSAD;

                if (((Dist(Nmv)*tl/outSAD[fPos] <= Dist(current[fPos])) || (xLoc == inData.wBlocks - 1) || bottom))
                {
                    current[fPos].x    =    Nmv.x;
                    current[fPos].y    =    Nmv.y;
                    outSAD[fPos]    =    tl;
                }
            }
        }

        cHalf[fPos]                =    current[fPos];
        Rstride                    =    inData.exwidth;
        pRslt                    =    refFrame + cHalf[fPos].x + (cHalf[fPos].y * Rstride);

        if(accuracy > 1)
        {
            halfpixel(&(cHalf[fPos]), &outSAD[fPos], objFrame, refFrame, inData.block_size_w,
                fPos, inData.wBlocks, xLoc, yLoc, offset, videoIn, &pRslt, &Rstride, inData.exwidth, 0, cQuarter, accuracy, Rs, Cs);
        }

        halfLoc                    =    (xLoc * 2) + (yLoc * 2 * outData.wBlocks);
        tMV.x                    =    (current[fPos].x * 2) + cHalf[fPos].x;
        tMV.y                    =    (current[fPos].y * 2) + cHalf[fPos].y;

        outMV                    +=    halfLoc; /* Potentially unitialized elements of outMV used */

        outMV->x                =    tMV.x;
        outMV->y                =    tMV.y;
        outMV                    +=    1;
        outMV->x                =    tMV.x;
        outMV->y                =    tMV.y;
        if(extraBlock)
        {
            outMV                +=    1;
            outMV->x            =    tMV.x;
            outMV->y            =    tMV.y;
            outMV                -=    1;
        }
        outMV                    +=    (outData.wBlocks - 1);
        outMV->x                =    tMV.x;
        outMV->y                =    tMV.y;
        outMV                    +=    1;
        outMV->x                =    tMV.x;
        outMV->y                =    tMV.y;

        if(extraBlock)
        {
            outMV                +=    1;
            outMV->x            =    tMV.x;
            outMV->y            =    tMV.y;
            outMV                -=    1;
        }

        if(extraLine)
        {
            outMV                    +=    (outData.wBlocks - 1);
            outMV->x                =    tMV.x;
            outMV->y                =    tMV.y;
            outMV                    +=    1;
            outMV->x                =    tMV.x;
            outMV->y                =    tMV.y;

            if(extraBlock)
            {
                outMV                +=    1;
                outMV->x            =    tMV.x;
                outMV->y            =    tMV.y;
                outMV                -=    1;
            }
        }

        videoIn->average        +=    (tMV.x * tMV.x) + (tMV.y * tMV.y);

        return(a);
    }

    Ipp32u ME_One3(VidRead *video, Ipp32u fPos, ImDetails inData, ImDetails /*outData*/, 
        imageData *scale, imageData *scaleRef,
        imageData * /*outImage*/, NGV_Bool first, DIR fdir, Ipp32u accuracy)    
    {
        MVector        tMV, ttMV, *current, *cHalf, *cQuarter, Nmv, preMV;            
        Ipp32s            txMV, tyMV;
        Ipp32s            limitXleft;
        Ipp32s            limitXright;
        Ipp32s            limitYup;
        Ipp32s            limitYdown;
        Ipp32s            cor;
        Ipp32u            pen            =    10;
        Ipp8u            *pRslt;
        Ipp32u            Rstride;
        Ipp32u            *outSAD, preSAD;
        Ipp64f        *Rs,*Cs;
        Ipp32u            xLoc        =    (fPos % inData.wBlocks);
        Ipp32u            yLoc        =    (fPos / inData.wBlocks);
        Ipp8u            *prLoc, *objFrame, *refFrame;
        Ipp32u            tSAD        =    INT_MAX;
        Ipp32u            lSAD        =    INT_MAX;
        Ipp32u            bestSAD        =    INT_MAX;
        Ipp32u            tl, vid;
        Ipp32u            distance    =    INT_MAX;
        Ipp32u            uprow, left;
        NGV_Bool        q            =    false;
        NGV_Bool        foundBetter    =    false;
        NGV_Bool        bottom        =    false;

        Ipp32s            xpos = xLoc * inData.block_size_w;
        Ipp32s            ypos = yLoc * inData.block_size_h;
        Ipp32u            offset        =    inData.initPoint + (yLoc * inData.exwidth * inData.block_size_h) + (xLoc * inData.block_size_w);

        objFrame                =    scale->exImage.Y + offset;
        refFrame                =    scaleRef->exImage.Y + offset;

        Rs                        =    scale->DiffRs;
        Cs                        =    scale->DiffCs;


        if(fdir == forward)        
        {
            /* quiet KW */
            printf("Error - (fdir == forward) invalid\n");
            return 0xffffffff;
        }
        else                    
        {
            current                =    scale->pBackward;
            cHalf                =    scale->pBhalf;
            cQuarter            =    scale->pBquarter;
            outSAD                =    scale->BSAD;
            prLoc                =    scale->Backward.Y + offset;
        }
        cHalf[fPos]                =    zero;
        cQuarter[fPos]            =    zero;

        Nmv.x = current[fPos].x;
        Nmv.y = current[fPos].y;

        correctMVectorOpt(&(Nmv), xpos, ypos, inData);

        current[fPos].x = Nmv.x;
        current[fPos].y = Nmv.y;

        searchMV2(zero, objFrame, refFrame, inData, fPos, xLoc, yLoc, &bestSAD, &distance, &preSAD);
        tSAD                    =    bestSAD;
        bestSAD                    =    INT_MAX;
        distance                =    INT_MAX;

        if(!((current[fPos].x == 0) && (current[fPos].y == 0)))        
        {
            searchMV2(current[fPos], objFrame, refFrame, inData, fPos, xLoc, yLoc, &bestSAD, &distance, &preSAD);
            lSAD                =    bestSAD;
            bestSAD                =    INT_MAX;
            distance            =    INT_MAX;

            if(tSAD <= (lSAD * 1.0))    
            {
                current[fPos].x    =    0;
                current[fPos].y    =    0;
            }
        }

        if(!first)
            cor                    =    current[fPos].x + (current[fPos].y * inData.exwidth);
        else
            cor                    =    0;

        ttMV.x                    =    current[fPos].x;
        ttMV.y                    =    current[fPos].y;
        outSAD[fPos]            =    bestSAD;

        calcLimits3(xLoc, yLoc, inData, &limitXleft, &limitXright, &limitYup, &limitYdown, objFrame, refFrame + cor, &(ttMV), fPos);
        for(tMV.y=limitYup;tMV.y<=limitYdown;tMV.y++)        {
            for(tMV.x=limitXleft;tMV.x<=limitXright;tMV.x++)    {
                preMV.x            =    tMV.x + ttMV.x;
                preMV.y            =    tMV.y + ttMV.y;
                correctMVectorOpt(&(preMV), xpos, ypos, inData);
                foundBetter        =    searchMV2(preMV, objFrame, refFrame, inData, fPos, xLoc,
                    yLoc, &bestSAD, &distance, &preSAD);
                if(foundBetter)    {
                    current[fPos].x    =    preMV.x;
                    current[fPos].y    =    preMV.y;
                    outSAD[fPos]=    bestSAD;
                    foundBetter    =    false;
                }
            }
        }

        //Neighbor search//
        uprow                    =    fPos - inData.wBlocks - 1;
        left                    =    fPos - 1;
        vid                        =    accuracy / 2;
        if(yLoc == inData.hBlocks - 1)
            bottom                =    true;
        if((fPos > inData.wBlocks) && (xLoc > 0))   {
            tl                  =   outSAD[fPos] + pen;
            distance            =   INT_MAX;
            Nmv.x               =   current[uprow].x;
            Nmv.y               =   current[uprow].y;
            correctMVectorOpt(&(Nmv), xpos, ypos, inData);
            foundBetter         =   searchMV2(Nmv, objFrame, refFrame, inData, fPos, xLoc, yLoc, &tl, &distance, &preSAD);

            if(foundBetter && ((Dist(Nmv)*tl/outSAD[fPos] <= Dist(current[fPos])) || (xLoc == inData.wBlocks - 1) || bottom))   {
                current[fPos].x =   Nmv.x;
                current[fPos].y =   Nmv.y;
                outSAD[fPos]    =   tl;
                foundBetter     =   false;
            }
        }
        uprow++;
        if(fPos > inData.wBlocks)                       {
            tl                  =   outSAD[fPos] + pen;
            distance            =   INT_MAX;
            Nmv.x               =   current[uprow].x;
            Nmv.y               =   current[uprow].y;
            correctMVectorOpt(&(Nmv), xpos, ypos, inData);
            foundBetter         =   searchMV2(Nmv, objFrame, refFrame, inData, fPos, xLoc,
                yLoc, &tl, &distance, &preSAD);

            if(foundBetter && ((Dist(Nmv)*tl/outSAD[fPos] <= Dist(current[fPos])) || (xLoc == 0) || (xLoc == inData.wBlocks - 1) || bottom))    
            {
                current[fPos].x =   Nmv.x;
                current[fPos].y =   Nmv.y;
                outSAD[fPos]     =   tl;
                foundBetter     =   false;
            }
        }
        uprow++;
        if((fPos > inData.wBlocks) && (xLoc < inData.wBlocks))      
        {
            tl                  =   outSAD[fPos] + pen;
            distance            =   INT_MAX;
            Nmv.x               =   current[uprow].x;
            Nmv.y               =   current[uprow].y;
            correctMVectorOpt(&(Nmv), xpos, ypos, inData);
            foundBetter         =   searchMV2(Nmv, objFrame, refFrame, inData, fPos, xLoc,
                yLoc, &tl, &distance, &preSAD);
            //if(foundBetter)       {
            if(foundBetter && ((Dist(Nmv)*tl/outSAD[fPos] <= Dist(current[fPos])) || (xLoc == 0) || bottom))        
            {
                current[fPos].x =   Nmv.x;
                current[fPos].y =   Nmv.y;
                outSAD[fPos]    =   tl;
                foundBetter     =   false;
            }
        }
        if(xLoc > 0)            {
            tl                  =   outSAD[fPos] + pen;
            distance            =   INT_MAX;
            Nmv.x               =   current[left].x;
            Nmv.y               =   current[left].y;
            correctMVectorOpt(&(Nmv), xpos, ypos, inData);
            foundBetter         =   searchMV2(Nmv, objFrame, refFrame, inData, fPos, xLoc,
                yLoc, &tl, &distance, &preSAD);
            //if(foundBetter)       {
            if(foundBetter && ((Dist(Nmv)*tl/outSAD[fPos] <= Dist(current[fPos])) || (xLoc == inData.wBlocks - 1) || bottom))   {
                current[fPos].x =   Nmv.x;
                current[fPos].y =   Nmv.y;
                outSAD[fPos]    =   tl;
                foundBetter     =   false;
            }
        }
        cHalf[fPos]                =    current[fPos];
        Rstride                    =    inData.exwidth;
        txMV                    =    current[fPos].x;
        tyMV                    =    current[fPos].y;
        pRslt                    =    refFrame + cHalf[fPos].x + (cHalf[fPos].y * Rstride);

        if(accuracy > 2)
            q                =    true;
        if(accuracy > 1)
            halfpixel(&(cHalf[fPos]), &outSAD[fPos], objFrame, refFrame, inData.block_size_w,
            fPos, inData.wBlocks, xLoc, yLoc, offset, video, &pRslt, &Rstride, inData.exwidth, q, cQuarter, accuracy, Rs, Cs);



        txMV                    =   (current[fPos].x * accuracy) + (cHalf[fPos].x * (accuracy >> 1)) + (cQuarter[fPos].x * (accuracy >> 2));
        tyMV                    =   (current[fPos].y * accuracy) + (cHalf[fPos].y * (accuracy >> 1)) + (cQuarter[fPos].y * (accuracy >> 2));

        video->average          +=  (txMV * txMV) + (tyMV * tyMV);

        txMV                    =   ((128 + current[fPos].x > 254)?254:((128 + current[fPos].x < 0)?0:(128 + current[fPos].x)));
        tyMV                    =   ((128 + current[fPos].y > 254)?254:((128 + current[fPos].y < 0)?0:(128 + current[fPos].y)));

        return(tSAD);
    }

#define ME_SCALE1 1

    void    calcLimits(Ipp32u xLoc, Ipp32u yLoc, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup,
        Ipp32s *limitYdown, Ipp8u * /*objFrame*/, Ipp8u * /*refFrame*/, ImDetails inData, Ipp32u /*fPos*/)    
    {
        Ipp32s            preLimit;
        Ipp32u            blockSize    =    inData.block_size_w;
        Ipp32u            rangeX        =    32;
        Ipp32u            rangeY        =    32;

        preLimit                =    (xLoc + 1) * blockSize;
        *limitXleft                =    -IPP_MIN(preLimit, (Ipp32s) rangeX);
        *limitXright            =    IPP_MIN((inData.exwidth - blockSize - 6) - preLimit, rangeX);
        preLimit                =    (yLoc + 1) * blockSize;
        *limitYup                =    -IPP_MIN(preLimit, (Ipp32s) rangeY);
        *limitYdown                =    IPP_MIN((inData.exheight - blockSize - 6) - preLimit, rangeY);
    }

    void    calcLimits2(Ipp32u xLoc, Ipp32u yLoc, Ipp32s *limitXleft, Ipp32s *limitXright, Ipp32s *limitYup,
        Ipp32s *limitYdown, Ipp8u * /*objFrame*/, Ipp8u * /*refFrame*/, ImDetails inData, MVector *MV, Ipp32u /*fPos*/)    
    {
        Ipp32s            preLimit, Xdim, Ydim;
        Ipp32s            xVal, yVal, exWval, exHval;
        Ipp32u            rangeX        =    8;
        Ipp32u            rangeY        =    8;

        xVal                    =    (Ipp32s)((xLoc + 1) * inData.block_size_w);
        exWval                    =    (Ipp32s)(inData.exwidth - inData.block_size_w);
        yVal                    =    (Ipp32s)((yLoc + 1) * inData.block_size_h);
        exHval                    =    (Ipp32s)(inData.exheight - inData.block_size_h);


        rangeY = 16;
        rangeX = 16;

        Xdim                    =    xVal + MV->x;
        if(Xdim < 0)
            MV->x                =    - (xVal);
        else if(Xdim > exWval)
            MV->x                =    exWval - (Ipp32s)(xLoc * inData.block_size_w);

        Ydim                    =    yVal + MV->y;
        if(Ydim < 0)
            MV->y                =    -(yVal);
        else if(Ydim > exHval)
            MV->y                =    exHval - (Ipp32s)(yLoc * inData.block_size_h);

        preLimit                =    xVal + MV->x;
        *limitXleft                =    -IPP_MIN(preLimit, (Ipp32s) rangeX);
        *limitXright            =    IPP_MIN((Ipp32u)((exWval - 6) - preLimit), rangeX);
        preLimit                =    yVal + MV->y;
        *limitYup                =    -IPP_MIN(preLimit, (Ipp32s) rangeY);
        *limitYdown                =    IPP_MIN((Ipp32u)((exHval - 6) - preLimit), rangeY);
    }

    void    calcLimits3(Ipp32u xLoc, Ipp32u yLoc, ImDetails inData, Ipp32s *limitXleft,
        Ipp32s *limitXright, Ipp32s *limitYup, Ipp32s *limitYdown, Ipp8u * /*objFrame*/,
        Ipp8u * /*refFrame*/, MVector *MV, Ipp32u /*fPos*/)    
    {
        Ipp32s            preLimit;
        Ipp32s            xVal, yVal, exWval, exHval;
        Ipp32u            rangeX        =    8;
        Ipp32u            rangeY        =    8;

        xVal                    =    (Ipp32s)((xLoc + 1) * inData.block_size_w);
        exWval                    =    (Ipp32s)(inData.exwidth - inData.block_size_w);
        yVal                    =    (Ipp32s)((yLoc + 1) * inData.block_size_h);
        exHval                    =    (Ipp32s)(inData.exheight - inData.block_size_h);

        rangeY = 8;
        rangeX = 8;


        preLimit                =    xVal + MV->x;
        *limitXleft                =    -IPP_MIN(preLimit, (Ipp32s) rangeX);
        *limitXright            =    IPP_MIN((Ipp32u)((exWval - 6) - preLimit), rangeX);
        preLimit                =    yVal + MV->y;
        *limitYup                =    -IPP_MIN(preLimit, (Ipp32s) rangeY);
        *limitYdown                =    IPP_MIN((Ipp32u)((exHval - 6) - preLimit), rangeY);
    }

    void    correctMVectorOpt(MVector *MV, Ipp32s xPos, Ipp32s yPos, ImDetails inData)
    {
        Ipp32s            exWH, maxright;
        Ipp32s            size_wh = inData.block_size_w;

        exWH    =    (Ipp32s)(inData.exwidth);

        if (MV->x < (-xPos -size_wh))
            MV->x            =    -xPos -size_wh;
        else if (MV->x > (maxright = (exWH -xPos -2*size_wh - 8)))
            MV->x            =    maxright;

        size_wh                = inData.block_size_h;
        exWH                = (Ipp32s)(inData.exheight);

        if (MV->y < (-yPos -size_wh))
            MV->y            =    -yPos -size_wh;
        else if (MV->y > (maxright = (exWH -yPos -2*size_wh - 8)))
            MV->y            =    maxright;
    }

    NGV_Bool    searchMV(MVector MV, Ipp8u* curY, Ipp8u* refY, Ipp32u blockSize, Ipp32u /*fPos*/,
        Ipp32u /*xLoc*/, Ipp32u /*yLoc*/, Ipp32u *bestSAD, Ipp32u *distance, Ipp32u exWidth, Ipp32u *preSAD)    
    {
        Ipp32u            SAD            =    0;
        Ipp32u            preDist;//, tSAD;
        Ipp8u            *fCur, *fRef;

        fCur                    =    curY;
        fRef                    =    refY + MV.x + (MV.y * (signed int) exWidth);
        preDist                    =    (MV.x * MV.x) + (MV.y * MV.y);

        if(blockSize == 4)
            VM_ASSERT(0);
        else 
        {
            SAD                    =    SAD_8x8_Block(fCur, fRef, exWidth, exWidth);
        }

        *preSAD                    =    SAD;
        if((SAD < *bestSAD) ||((SAD == *(bestSAD)) && *distance > preDist))    {
            *distance            = preDist;
            *(bestSAD)            = SAD;
            return true;
        }
        return false;
    }

    NGV_Bool    searchMV2(MVector MV, Ipp8u* curY, Ipp8u* refY, ImDetails inData, Ipp32u /*fPos*/,
        Ipp32u /*xLoc*/, Ipp32u /*yLoc*/, Ipp32u *bestSAD, Ipp32u *distance, Ipp32u *preSAD)    
    {
        Ipp32u            SAD            =    0;
        Ipp32u            preDist;
        Ipp8u*            fRef;
        Ipp32u            penalty        =    0;

        fRef                    =    refY + MV.x + (MV.y * (signed int)inData.exwidth);
        preDist                    =    (MV.x * MV.x) + (MV.y * MV.y);
        penalty                    *=    (preDist + 1)/(*distance + 1);
        if(inData.block_size_w == 4)
            VM_ASSERT(0);
        else 
        {
            SAD                    =    SAD_8x8_Block(curY, fRef, inData.exwidth, inData.exwidth);
        }
        *preSAD                    =    SAD + penalty;
        if((*preSAD < *bestSAD) ||((SAD == *(bestSAD)) && *distance > preDist))    {
            *distance            =    preDist;
            *(bestSAD)            =    SAD;
            return true;
        }
        return false;
    }

    void    halfpixel(MVector *MV, Ipp32u *bestSAD, Ipp8u* curY, Ipp8u* refY, Ipp32u blockSize, Ipp32u fPos, Ipp32u wBlocks, Ipp32u xLoc,
        Ipp32u yLoc, Ipp32u offset, VidRead *videoIn, Ipp8u **pRslt, Ipp32u *Rstride, Ipp32u exWidth,
        NGV_Bool q, MVector *stepq, Ipp32u accuracy, Ipp64f* Rs, Ipp64f* Cs)        
    {
        Ipp32u            i,j;
        Ipp32u            blockj, maxMov;
        Ipp32u            loc;
        Ipp32u            SAD    = INT_MAX, preSAD = INT_MAX;
        Ipp32s            txMV, tyMV;
        Ipp32s            nuOff            =    0;
        Ipp64f            RsCs;
        Ipp8u            *fCur, *fRef;
        Ipp8u*            searchLoc;
        MVector        tMV                =    {0,0};
        NGV_Bool        betterfound        =    false;

        Ipp8u*            objFrame;
        Ipp32u            ExtWidth = exWidth;
        Ipp32u         fPos4       =   yLoc*2*wBlocks*2+xLoc*2;

        nuOff                        =    MV->x + (MV->y * exWidth);
        maxMov                        =    exWidth * 4;

        objFrame                    =    fCur            =    curY;
        fRef                        =    refY + nuOff;
        blockj                        =    blockSize * exWidth;

        corner(videoIn->cornerBox.Y, fRef, exWidth, blockSize);
        top(videoIn->topBox.Y, fRef, exWidth, blockSize, 1);
        left(videoIn->leftBox.Y, fRef, exWidth, blockSize, 1);

        MV->x                        =    0;
        MV->y                        =    0;

        if(blockSize == 4)
        {
            VM_ASSERT(0);
        }
        else
        {
            int iSrc2Width = blockSize + 3;


            Ipp8u* fRef;

            searchLoc                =    videoIn->cornerBox.Y + blockSize + 3 + 1;
            ExtWidth                =    iSrc2Width    = blockSize + 3;

            for(i=0;i<2;i++)        
            {
                loc                    =    i * (blockSize + 3); 
                fRef                =    searchLoc + loc; 
                for(j=0;j<2;j++)    
                {        

                    preSAD = SAD_8x8_Block(objFrame, fRef, exWidth, ExtWidth);

                    txMV            =    ((j * 2) - 1) + MV->x;
                    tyMV            =    ((i * 2) - 1) + MV->y;

                    if((preSAD<*bestSAD))
                    {
                        *bestSAD    =    preSAD;
                        tMV.x        =    (j * 2) - 1;
                        tMV.y        =    (i * 2) - 1;
                        *Rstride    =    blockSize + 3;
                        *pRslt        =    searchLoc + loc;
                        betterfound    =    true;
                    }
                    loc++;
                    fRef++;
                }
            }

            searchLoc                =    videoIn->topBox.Y + blockSize + 3;
            for(i=0;i<2;i++)        
            {
                loc                    =    i * (blockSize + 2);
                ExtWidth            =    iSrc2Width    = blockSize + 2;
                fRef                =    searchLoc + loc;

                preSAD                =    SAD_8x8_Block(objFrame, fRef, exWidth, blockSize + 2);

                //SAD                =    ME_SAD_8x8_Block(fCur,searchLoc + loc,exWidth, blockSize + 2);
                if((preSAD<*bestSAD))    
                {
                    *bestSAD        =    preSAD;
                    tMV.x            =    0;
                    tMV.y            =    (i * 2) - 1;
                    betterfound        =    true;
                }
            }

            searchLoc                =    videoIn->leftBox.Y + blockSize + 3 + 1;
            for(i=0;i<2;i++)        
            {
                ExtWidth            =    iSrc2Width  = blockSize + 3;
                fRef                =    searchLoc + i;

                preSAD                =    SAD_8x8_Block(objFrame, fRef, exWidth, blockSize + 3);

                //SAD                =    ME_SAD_8x8_Block(fCur,searchLoc + i,exWidth,blockSize + 3);
                if((preSAD<*bestSAD))    {
                    *bestSAD        =    preSAD;
                    tMV.x            =    (i * 2) - 1;
                    tMV.y            =    0;
                    *Rstride        =    blockSize + 3;
                    *pRslt            =    searchLoc + i;
                    betterfound        =    true;
                }
            }
        }

        if(betterfound)            
        {
            MV->x                =    tMV.x;
            MV->y                =    tMV.y;
        }
        RsCs                    =    IPP_MAX(Rs[fPos4],Cs[fPos4]);
        if(q &&(RsCs <= 1.0))    
        {
            q                    =    false;
            stepq[fPos].x        =    0;
            stepq[fPos].y        =    0;
        }


        if(accuracy > 2 && q == true)
            quarterpixel(videoIn, blockSize, &(stepq[fPos]), tMV, offset, fCur, fRef, bestSAD, pRslt, Rstride, exWidth);

    }

    void    quarterpixel(VidRead *videoIn, Ipp32u blockSize, MVector *MV, MVector tMV, Ipp32u /*offset*/, Ipp8u* fCur, Ipp8u* fRef,
        Ipp32u *bestSAD, Ipp8u **pRslt, Ipp32u *Rstride, Ipp32u exWidth)    
    {
        Ipp32u            i,j;
        Ipp32u            bi;
        Ipp32u            tSAD;
        Ipp8u            *sLA, *sLB, *sLC, *sLI;
        NGV_Bool        betterfound    =    false;

        sLA                        =    videoIn->cornerBox.Y + blockSize + 3 + 1;
        sLB                        =    videoIn->topBox.Y + blockSize + 3;
        sLC                        =    videoIn->leftBox.Y + blockSize + 3 + 1;
        sLI                        =    fRef;
        MV->x                    =    0;
        MV->y                    =    0;

        if(blockSize == 8)
        {
            if(tMV.x == 0 && tMV.y == 0)
            {    
                for(i=0;i<blockSize;i++)    
                {
                    bi                =    (i * blockSize);
                    ME_SUM_cross_8x8(sLI, sLA,                        sLB,                sLC,                    videoIn->spBuffer[0].Y + bi);
                    ME_SUM_cross_8x8(sLI, sLA + 1,                    sLB,                sLC + 1,                videoIn->spBuffer[2].Y + bi);
                    ME_SUM_cross_8x8(sLI, sLA + blockSize + 3,        sLB + blockSize + 2,sLC,                    videoIn->spBuffer[5].Y + bi);
                    ME_SUM_cross_8x8(sLI, sLA + blockSize + 3 + 1,    sLB + blockSize + 2,sLC + 1,                videoIn->spBuffer[7].Y + bi);
                    ME_SUM_Line_8x8 (sLI, sLB,                        sLI - exWidth,        sLB + blockSize + 2,    videoIn->spBuffer[1].Y + bi);                    
                    ME_SUM_Line_8x8 (sLI, sLC,                        sLI - 1,            sLC + 1,                videoIn->spBuffer[3].Y + bi);
                    ME_SUM_Line_8x8 (sLI, sLC + 1,                    sLI + 1,            sLC,                    videoIn->spBuffer[4].Y + bi);                    
                    ME_SUM_Line_8x8 (sLI, sLB + blockSize + 2,        sLI + exWidth,        sLB,                    videoIn->spBuffer[6].Y + bi);

                    sLA            +=    11;
                    sLB            +=    10;
                    sLC            +=    11;
                    sLI            +=    exWidth;
                }        
            }
            else if(tMV.x != 0 && tMV.y != 0)    
            {
                sLI                    +=    ((1 + tMV.x) / 2) + (((1 + tMV.y) / 2) * exWidth);
                sLA                    +=    ((1 + tMV.x) / 2) + (((1 + tMV.y) / 2) * (blockSize + 3));
                sLB                    +=    ((1 + tMV.x) / 2) + (((1 + tMV.y) / 2) * (blockSize + 2));
                sLC                    +=    ((1 + tMV.x) / 2) + (((1 + tMV.y) / 2) * (blockSize + 3));

                for(i=0;i<blockSize;i++)    
                {
                    bi            =    (i * blockSize);
                    ME_SUM_cross_8x8(sLI - 1 - exWidth, sLA, sLB - 1, sLC - (blockSize + 3),videoIn->spBuffer[0].Y + bi);                    
                    ME_SUM_cross_8x8(sLI - exWidth,sLA,sLB,sLC - (blockSize + 3),videoIn->spBuffer[2].Y + bi);
                    ME_SUM_cross_8x8(sLI - 1,sLA,sLB - 1,sLC,videoIn->spBuffer[5].Y + bi);
                    ME_SUM_cross_8x8(sLI,sLA,sLB,sLC,videoIn->spBuffer[7].Y + bi);

                    ME_SUM_Line_8x8(sLA, sLC - (blockSize + 3), sLA - (blockSize + 3), sLC, videoIn->spBuffer[1].Y + bi);
                    ME_SUM_Line_8x8(sLA, sLB - 1, sLA - 1, sLB, videoIn->spBuffer[3].Y + bi);
                    ME_SUM_Line_8x8(sLA, sLB, sLA + 1, sLB - 1, videoIn->spBuffer[4].Y + bi);
                    ME_SUM_Line_8x8(sLA, sLC, sLC - (blockSize + 3), sLA + blockSize + 3, videoIn->spBuffer[6].Y + bi);

                    sLA                +=    11;
                    sLB                +=    10;
                    sLC                +=    11;
                    sLI                +=    exWidth;
                }
            }
            else if(tMV.x == 0 && tMV.y != 0)    {
                sLI                    +=    (((1 + tMV.y) / 2) * exWidth);
                sLA                    +=    (((1 + tMV.y) / 2) * (blockSize + 3));
                sLB                    +=    (((1 + tMV.y) / 2) * (blockSize + 2));
                sLC                    +=    (((1 + tMV.y) / 2) * (blockSize + 3));

                for(i=0;i<blockSize;i++)    
                {
                    bi            =    (i * blockSize);

                    ME_SUM_cross_8x8(sLB ,    sLA ,        sLI  - exWidth,    sLC  - (blockSize + 3),videoIn->spBuffer[0].Y + bi);
                    ME_SUM_cross_8x8(sLB ,    sLA  + 1,    sLI  - exWidth,    sLC  - (blockSize + 2),videoIn->spBuffer[2].Y + bi);
                    ME_SUM_cross_8x8(sLB ,    sLA ,        sLI ,                sLC ,                    videoIn->spBuffer[5].Y + bi);
                    ME_SUM_cross_8x8(sLB ,    sLA  + 1,    sLI ,                sLC  + 1,                videoIn->spBuffer[7].Y + bi);

                    ME_SUM_Line_8x8(sLB , sLI  - exWidth,    sLI ,            sLB  - (blockSize + 2),    videoIn->spBuffer[1].Y + bi);
                    ME_SUM_Line_8x8(sLB , sLA ,                sLA  + 1,        sLB  - 1,                    videoIn->spBuffer[3].Y + bi);
                    ME_SUM_Line_8x8(sLB , sLA  + 1,            sLA ,            sLB  + 1,                    videoIn->spBuffer[4].Y + bi);
                    ME_SUM_Line_8x8(sLB , sLI ,                sLI  - exWidth, sLB  + (blockSize + 2),    videoIn->spBuffer[6].Y + bi);

                    sLA            +=    11;
                    sLB            +=    10;
                    sLC            +=    11;
                    sLI            +=    exWidth;
                }
            }
            else if(tMV.x != 0 && tMV.y == 0)    {
                sLI                    +=    ((1 + tMV.x) / 2);
                sLA                    +=    ((1 + tMV.x) / 2);
                sLB                    +=    ((1 + tMV.x) / 2);
                sLC                    +=    ((1 + tMV.x) / 2);

                for(i=0;i<blockSize;i++)    
                {
                    bi            =    (i * blockSize);
                    ME_SUM_cross_8x8(sLC ,    sLI  - 1,    sLA ,                    sLB  - 1,                videoIn->spBuffer[0].Y + bi);
                    ME_SUM_cross_8x8(sLC ,    sLI ,        sLA ,                    sLB ,                    videoIn->spBuffer[2].Y + bi);
                    ME_SUM_cross_8x8(sLC ,    sLI  - 1,    sLA  + blockSize + 3,    sLB  + blockSize + 1,    videoIn->spBuffer[5].Y + bi);
                    ME_SUM_cross_8x8(sLC ,    sLI ,        sLA  + blockSize + 3,    sLB  + blockSize + 2,    videoIn->spBuffer[7].Y + bi);

                    ME_SUM_Line_8x8(sLA ,sLC ,sLA  + blockSize + 3,sLC  - (blockSize + 3), videoIn->spBuffer[1].Y + bi);
                    ME_SUM_Line_8x8(sLI  - 1,sLC ,sLI ,sLC  - 1, videoIn->spBuffer[3].Y + bi);
                    ME_SUM_Line_8x8(sLI ,sLC ,sLI  - 1,sLC  + 1, videoIn->spBuffer[4].Y + bi);
                    ME_SUM_Line_8x8(sLA  + blockSize + 3, sLC , sLA , sLC  + blockSize + 3, videoIn->spBuffer[6].Y + bi);                    

                    sLA                +=    11;
                    sLB                +=    10;
                    sLC                +=    11;
                    sLI                +=    exWidth;
                }
            }
            bi                        =    8;

            for(i=0;i<8;i++)        
            {
                if(blockSize == 4)
                    VM_ASSERT(0);
                else
                    tSAD            =    SAD_8x8_Block(fCur,videoIn->spBuffer[i].Y,exWidth,blockSize);

                if(tSAD<*bestSAD)    
                {
                    *bestSAD        =    tSAD;
                    *Rstride        =    blockSize;
                    *pRslt            =    videoIn->spBuffer[i].Y;
                    betterfound        =    true;
                    bi                =    i;
                }
            }
        }
        else // if(blockSize == 4) -  current settings not enabled. So can be removed.
        {
            if(tMV.x == 0 && tMV.y == 0)    
            {
                for(i=0;i<blockSize;i++)    
                {
                    for(j=0;j<(blockSize / 4);j++)    
                    {
                        bi                =    (i * blockSize) + (j * 4);
                        ME_SUM_cross(sLI, sLA, sLB, sLC,videoIn->spBuffer[0].Y + bi);
                        ME_SUM_Line(sLI, sLB, sLI - exWidth, sLB + blockSize + 2, videoIn->spBuffer[1].Y + bi);
                        ME_SUM_cross(sLI,sLA + 1,sLB,sLC + 1,videoIn->spBuffer[2].Y + bi);
                        ME_SUM_Line(sLI, sLC, sLI - 1, sLC + 1, videoIn->spBuffer[3].Y + bi);
                        ME_SUM_Line(sLI, sLC + 1, sLI + 1, sLC, videoIn->spBuffer[4].Y + bi);
                        ME_SUM_cross(sLI,sLA + blockSize + 3,sLB + blockSize + 2,sLC,videoIn->spBuffer[5].Y + bi);
                        ME_SUM_Line(sLI, sLB + blockSize + 2, sLI + exWidth, sLB, videoIn->spBuffer[6].Y + bi);
                        ME_SUM_cross(sLI,sLA + blockSize + 3 + 1,sLB + blockSize + 2,sLC + 1,videoIn->spBuffer[7].Y + bi);
                        sLA            +=    4;
                        sLB            +=    4;
                        sLC            +=    4;
                        sLI            +=    4;
                    }
                    sLA                +=    3;
                    sLB                +=    2;
                    sLC                +=    3;
                    sLI                +=    exWidth - blockSize;
                }
            }
            else if(tMV.x != 0 && tMV.y != 0)    {
                sLI                    +=    ((1 + tMV.x) / 2) + (((1 + tMV.y) / 2) * exWidth);
                sLA                    +=    ((1 + tMV.x) / 2) + (((1 + tMV.y) / 2) * (blockSize + 3));
                sLB                    +=    ((1 + tMV.x) / 2) + (((1 + tMV.y) / 2) * (blockSize + 2));
                sLC                    +=    ((1 + tMV.x) / 2) + (((1 + tMV.y) / 2) * (blockSize + 3));
                for(i=0;i<blockSize;i++)    {
                    for(j=0;j<(blockSize / 4);j++)    {
                        bi            =    (i * blockSize) + (j * 4);
                        ME_SUM_cross(sLI - 1 - exWidth, sLA, sLB - 1, sLC - (blockSize + 3),videoIn->spBuffer[0].Y + bi);
                        ME_SUM_Line(sLA, sLC - (blockSize + 3), sLA - (blockSize + 3), sLC, videoIn->spBuffer[1].Y + bi);
                        ME_SUM_cross(sLI - exWidth,sLA,sLB,sLC - (blockSize + 3),videoIn->spBuffer[2].Y + bi);
                        ME_SUM_Line(sLA, sLB - 1, sLA - 1, sLB, videoIn->spBuffer[3].Y + bi);
                        ME_SUM_Line(sLA, sLB, sLA + 1, sLB - 1, videoIn->spBuffer[4].Y + bi);
                        ME_SUM_cross(sLI - 1,sLA,sLB - 1,sLC,videoIn->spBuffer[5].Y + bi);
                        ME_SUM_Line(sLA, sLC, sLC - (blockSize + 3), sLA + blockSize + 3, videoIn->spBuffer[6].Y + bi);
                        ME_SUM_cross(sLI,sLA,sLB,sLC,videoIn->spBuffer[7].Y + bi);
                        sLA            +=    4;
                        sLB            +=    4;
                        sLC            +=    4;
                        sLI            +=    4;
                    }
                    sLA                +=    3;
                    sLB                +=    2;
                    sLC                +=    3;
                    sLI                +=    exWidth - blockSize;
                }
            }
            else if(tMV.x == 0 && tMV.y != 0)    {
                sLI                    +=    (((1 + tMV.y) / 2) * exWidth);
                sLA                    +=    (((1 + tMV.y) / 2) * (blockSize + 3));
                sLB                    +=    (((1 + tMV.y) / 2) * (blockSize + 2));
                sLC                    +=    (((1 + tMV.y) / 2) * (blockSize + 3));
                for(i=0;i<blockSize;i++)    {
                    for(j=0;j<(blockSize / 4);j++)    {
                        bi            =    (i * blockSize) + (j * 4);
                        ME_SUM_cross(sLI - exWidth, sLA, sLB, sLC - (blockSize + 3),videoIn->spBuffer[0].Y + bi);
                        ME_SUM_Line(sLI - exWidth, sLB, sLI, sLB - (blockSize + 2), videoIn->spBuffer[1].Y + bi);
                        ME_SUM_cross(sLI - exWidth,sLA + 1,sLB,sLC - (blockSize + 2),videoIn->spBuffer[2].Y + bi);
                        ME_SUM_Line(sLA, sLB, sLA + 1, sLB - 1, videoIn->spBuffer[3].Y + bi);
                        ME_SUM_Line(sLA + 1, sLB, sLA, sLB + 1, videoIn->spBuffer[4].Y + bi);
                        ME_SUM_cross(sLI,sLA,sLB,sLC,videoIn->spBuffer[5].Y + bi);
                        ME_SUM_Line(sLI, sLB, sLI - exWidth, sLB + (blockSize + 2), videoIn->spBuffer[6].Y + bi);
                        ME_SUM_cross(sLI,sLA + 1,sLB,sLC + 1,videoIn->spBuffer[7].Y + bi);
                        sLA            +=    4;
                        sLB            +=    4;
                        sLC            +=    4;
                        sLI            +=    4;
                    }
                    sLA                +=    3;
                    sLB                +=    2;
                    sLC                +=    3;
                    sLI                +=    exWidth - blockSize;
                }
            }
            else if(tMV.x != 0 && tMV.y == 0)    {
                sLI                    +=    ((1 + tMV.x) / 2);
                sLA                    +=    ((1 + tMV.x) / 2);
                sLB                    +=    ((1 + tMV.x) / 2);
                sLC                    +=    ((1 + tMV.x) / 2);
                for(i=0;i<blockSize;i++)    {
                    for(j=0;j<(blockSize / 4);j++)    {
                        bi            =    (i * blockSize) + (j * 4);
                        ME_SUM_cross(sLI - 1, sLA, sLB - 1, sLC,videoIn->spBuffer[0].Y + bi);
                        ME_SUM_Line(sLA,sLC,sLA + blockSize + 3,sLC - (blockSize + 3), videoIn->spBuffer[1].Y + bi);
                        ME_SUM_cross(sLI,sLA,sLB,sLC,videoIn->spBuffer[2].Y + bi);
                        ME_SUM_Line(sLI - 1,sLC,sLI,sLC - 1, videoIn->spBuffer[3].Y + bi);
                        ME_SUM_Line(sLI,sLC,sLI - 1,sLC + 1, videoIn->spBuffer[4].Y + bi);
                        ME_SUM_cross(sLI - 1,sLA + blockSize + 3,sLB + blockSize + 1,sLC,videoIn->spBuffer[5].Y + bi);
                        ME_SUM_Line(sLA + blockSize + 3, sLC, sLA, sLC + blockSize + 3, videoIn->spBuffer[6].Y + bi);
                        ME_SUM_cross(sLI,sLA + blockSize + 3,sLB + blockSize + 2,sLC,videoIn->spBuffer[7].Y + bi);
                        sLA            +=    4;
                        sLB            +=    4;
                        sLC            +=    4;
                        sLI            +=    4;
                    }
                    sLA                +=    3;
                    sLB                +=    2;
                    sLC                +=    3;
                    sLI                +=    exWidth - blockSize;
                }
            }
            bi                        =    8;
            for(i=0;i<8;i++)        
            {
                if(blockSize == 4)
                    VM_ASSERT(0);
                else
                    tSAD            =    SAD_8x8_Block(fCur,videoIn->spBuffer[i].Y,exWidth,blockSize);
                if(tSAD<*bestSAD)    
                {
                    *bestSAD        =    tSAD;
                    *Rstride        =    blockSize;
                    *pRslt            =    videoIn->spBuffer[i].Y;
                    betterfound        =    true;
                    bi                =    i;
                }
            }
        }

        if(betterfound)            
        {
            if(bi == 8)
            {}
            else if(bi == 0)    {
                MV->x            -=    1;
                MV->y            -=    1;
            }
            else if(bi == 1)
                MV->y            -=    1;
            else if(bi == 2)    {
                MV->x            +=    1;
                MV->y            -=    1;
            }
            else if(bi == 3)
                MV->x            -=    1;
            else if(bi == 4)
                MV->x            +=    1;
            else if(bi == 5)    {
                MV->x            -=    1;
                MV->y            +=    1;
            }
            else if(bi == 6)
                MV->y            +=    1;
            else                {
                MV->x            +=    1;
                MV->y            +=    1;
            }
        }
    }


    void    corner(Ipp8u* dd, Ipp8u* ss, Ipp32u exWidth, Ipp32u blockSize)        
    {
        Ipp32u        i,j, val;
        Ipp32u        rdd, rss;
        Ipp8u*        loc;
        Ipp8u*        ddv;
        Ipp8u*        ss_ext;

        ss_ext                    =    ss - exWidth - 1;

        loc                        =    ss_ext - exWidth;
        for(i=0;i<(blockSize + 3);i++)        {
            rdd                    =    i * (blockSize + 3);
            rss                    =    i * exWidth;
            for(j=0;j<(blockSize + 3);j++)    {
                ddv                =    dd + rdd;
                val                =    *(ss_ext + rss) + *(ss_ext + rss - 1);
                val                +=    *(loc + rss) + *(loc + rss - 1);
                val                =    ((val + 2) >> 2);

                if(val >= 256)
                    *ddv        =    255;
                else
                    *ddv        =    (Ipp8u)val;
                rdd++;
                rss++;
            }
        }
    }

    void    top(Ipp8u* dd, Ipp8u* ss1, Ipp32u exWidth, Ipp32u blockSize, Ipp32u filter)    {
        Ipp32u        i,j;
        Ipp32s        val;
        Ipp32u        rdd, rss;
        Ipp8u*        loc;
        Ipp8u*        ddv;
        Ipp8u*        ss1_ext;

        ss1_ext                    =    ss1 - exWidth;
        loc                        =    ss1_ext - exWidth;
        for(i=0;i<(blockSize + 3);i++)        {
            rdd                    =    i * (blockSize + 2);
            rss                    =    i * exWidth;
            for(j=0;j<(blockSize + 2);j++)    {
                ddv                =    dd + rdd;
                if(filter < 2)    {
                    val            =    (*(ss1_ext + rss) + *(loc + rss)) * 20;
                    if(filter == 0)
                        val        -=    (*(ss1_ext + rss + exWidth) + *(loc + rss - exWidth))*4;
                    else            {
                        val        -=    (*(ss1_ext + rss + exWidth) + *(loc + rss - exWidth))*5;
                        val        +=    (*(ss1_ext + rss + (2 * exWidth)) + *(loc + rss - (2 * exWidth)));
                    }
                    if(val <= 0)
                        *ddv    =    0;
                    else            {
                        val        =    ((val + 16) >> 5);
                        if(val >= 256)
                            *ddv    =    255;
                        else
                            *ddv    =    (Ipp8u) val;
                    }
                }
                else            {
                    val            =    (*(ss1_ext + rss) + *(loc + rss)) * 2;
                    val            +=    (*(ss1_ext + rss + exWidth) + *(loc + rss - exWidth));
                    val            =    ((val + 2) >> 2);
                    if(val >= 256)
                        *ddv    =    255;
                    else
                        *ddv    =    (Ipp8u) val;
                }

                rdd++;
                rss++;
            }
        }
    }

    void    left(Ipp8u* dd, Ipp8u* ss1, Ipp32u exWidth, Ipp32u blockSize, Ipp32u filter)    
    {
        Ipp32u        i,j;
        Ipp32s        val;
        Ipp32u        rdd, rss;
        Ipp8u*        loc;
        Ipp8u*        ddv;
        Ipp8u*        ss1_ext;

        ss1_ext                    =    ss1 - exWidth - 1;

        for(i=0;i<(blockSize + 2);i++)        {
            rdd                    =    i * (blockSize + 3);
            rss                    =    i * exWidth;
            for(j=0;j<(blockSize + 3);j++)    {
                ddv                =    dd + rdd;
                loc                =    ss1_ext + rss;
                if(filter < 2)    {
                    val            =    ((Ipp32s)*loc + (Ipp32s)*(loc - 1)) * 20;
                    if(filter == 0)
                        val        -=    (*(loc + 1) + *(loc - 2)) * 4;
                    else        {
                        val        -=    (*(loc + 1) + *(loc - 2)) * 5;
                        val        +=    (*(loc + 2) + *(loc - 3));
                    }
                    if(val <= 0)
                        *ddv    =    0;
                    else        {
                        val        =    ((val + 16) >> 5);
                        if(val >= 256)
                            *ddv    =    255;
                        else
                            *ddv    =    (Ipp8u) val;
                    }
                }
                else            {
                    val            =    ((Ipp32s)*loc + (Ipp32s)*(loc - 1)) * 2;
                    val            +=    (*(loc + 1) + *(loc - 2));
                    val            =    ((val + 2) >> 2);
                    if(val >= 256)
                        *ddv    =    255;
                    else
                        *ddv    =    (Ipp8u) val;
                }


                rdd++;
                rss++;
            }
        }
    }

    Ipp32u        searchMV3(MVector Nmv, Ipp8u* curY, Ipp8u* refY, Ipp32u exWidth1, Ipp32u exWidth2)    
    {
        Ipp32u            SAD            =    INT_MAX;
        Ipp8u*            fRef;

        fRef                    =    refY + Nmv.x + (Nmv.y * exWidth2);
        SAD                        =    SAD_8x8_Block(curY, fRef, exWidth1, exWidth2);

        return SAD;
    }

    Ipp64f        Dist(MVector vector)
    {
        Ipp64f        size;
        size                    =    sqrt(((float)vector.x*vector.x) + (vector.y*vector.y));
        return size;
    }

    Ipp32u SAD_8x8_Block_SSE(Ipp8u* src, Ipp8u* ref, Ipp32u sPitch, Ipp32u rPitch)
    {
        Ipp32u SAD = 0;

        Ipp8u* psrc = src;
        Ipp8u* pref = ref;
        Ipp32u out, in;
        for (out = 0; out < 8; out++)
        {
            for (in = 0; in < 8; in++)    
            {
                SAD += (psrc[in]>pref[in]) ? (psrc[in]-pref[in]) : (pref[in]-psrc[in]);
            }
            psrc += sPitch; pref += rPitch;
        }
        return SAD;
    }



    Ipp32u SAD_8x8_Block_Intrin_SSE2(Ipp8u* src, Ipp8u* ref, Ipp32u sPitch, Ipp32u rPitch)
    {
        Ipp32u SAD = 0;
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;
        __m128i xmm7 = _mm_setzero_si128(); 
        {

            xmm0 = _mm_loadl_epi64((__m128i*)ref);
            xmm0 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(xmm0), (__m64*)(ref+rPitch)));
            xmm1 = _mm_loadl_epi64((__m128i*)src);
            xmm1 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(xmm1), (__m64*)(src+sPitch)));
            xmm0 = _mm_sad_epu8(xmm0, xmm1);

            xmm2 = _mm_loadl_epi64((__m128i*)(ref+2*rPitch));
            xmm2 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(xmm2), (__m64*)(ref+3*rPitch)));
            xmm3 = _mm_loadl_epi64((__m128i*)(src+2*sPitch));
            xmm3 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(xmm3), (__m64*)(src+3*sPitch)));
            xmm2 = _mm_sad_epu8(xmm2, xmm3);

            xmm4 = _mm_loadl_epi64((__m128i*)(ref+4*rPitch));
            xmm4 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(xmm4), (__m64*)(ref+5*rPitch)));
            xmm5 = _mm_loadl_epi64((__m128i*)(src+4*sPitch));
            xmm5 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(xmm5), (__m64*)(src+5*sPitch)));
            xmm4 = _mm_sad_epu8(xmm4, xmm5);

            xmm6 = _mm_loadl_epi64((__m128i*)(ref+6*rPitch));
            xmm6 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(xmm6), (__m64*)(ref+7*rPitch)));
            xmm7 = _mm_loadl_epi64((__m128i*)(src+6*sPitch));
            xmm7 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(xmm7), (__m64*)(src+7*sPitch)));
            xmm6 = _mm_sad_epu8(xmm6, xmm7);

            xmm0 = _mm_adds_epi16(xmm0, xmm2);
            xmm0 = _mm_adds_epi16(xmm0, xmm4);
            xmm0 = _mm_adds_epi16(xmm0, xmm6);

            SAD = _mm_extract_epi16(xmm0, 0) + _mm_extract_epi16(xmm0, 4);
        }
        return SAD;
    }


    const Ipp64s               x1        =    0x0014001400140014;
    const Ipp64s                x2        =    0x0004000400040004;
    const Ipp64s                x3        =    0x0010001000100010;
    const Ipp64s                x4        =    0x0002000200020002;

    void ME_SUM_cross_8x8_C(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt)
    {
        Ipp32u i, Sum;

        for (i = 0; i < 8; i++)
        {
            Sum = (((*(sLI++))+(*(sLA++))+(*(sLB++))+(*(sLC++))+2)>>2);

            (*(rslt++)) = (Sum > 255) ? 255 : (Ipp8u)Sum;
        }
    }

    void ME_SUM_Line_8x8_C(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt)
    {
        Ipp32s i, Sum;

        for (i = 0; i < 8; i++)
        {
            Sum = ((20*((Ipp32s)(*(sLx++))+(Ipp32s)(*(sLy++)))-4*((Ipp32s)(*(sLx2++))+(Ipp32s)(*(sLy2++)))+16)>>5);

            (*(rslt++)) = (Sum > 255) ? 255 : (Ipp8u)Sum;
        }
    }

    void ME_SUM_cross_C(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt)
    {
        Ipp32u i, Sum;

        for (i = 0; i < 4; i++)
        {
            Sum = (((*(sLI++))+(*(sLA++))+(*(sLB++))+(*(sLC++))+2)>>2);

            (*(rslt++)) = (Sum > 255) ? 255 : (Ipp8u)Sum;
        }
    }

    void ME_SUM_Line_C(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt)
    {
        Ipp32s i, Sum;

        for (i = 0; i < 4; i++)
        {
            Sum = ((20*((Ipp32s)(*(sLx++))+(Ipp32s)(*(sLy++)))-4*((Ipp32s)(*(sLx2++))+(Ipp32s)(*(sLy2++)))+16)>>5);

            (*(rslt++)) = (Sum > 255) ? 255 : (Ipp8u)Sum;
        }
    }

    void ME_SUM_cross_8x8_SSE(Ipp8u * sLI, Ipp8u * sLA, Ipp8u * sLB, Ipp8u * sLC, Ipp8u * rslt)
    {
        // For 64-bit can start with using C code.
        VM_ASSERT(0);
    }

    static const NGV_ALIGN(16, short add2_8[8]) = {2, 2, 2, 2, 2, 2, 2, 2};

    void ME_SUM_cross_8x8_Intrin_SSE2(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt)
    {
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4;
        __m128i xmm7 = _mm_setzero_si128(); // Set Acc to Zero

        {
            xmm4 = _mm_loadu_si128((__m128i*)add2_8);
            xmm0 = _mm_loadl_epi64((__m128i*)sLI);
            xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);
            xmm1 = _mm_loadl_epi64((__m128i*)sLA);
            xmm1 = _mm_unpacklo_epi8(xmm1, xmm7);
            xmm2 = _mm_loadl_epi64((__m128i*)sLB);
            xmm2 = _mm_unpacklo_epi8(xmm2, xmm7);
            xmm3 = _mm_loadl_epi64((__m128i*)sLC);
            xmm3 = _mm_unpacklo_epi8(xmm3, xmm7);
            xmm4 = _mm_adds_epu16(xmm4, xmm0);
            xmm4 = _mm_adds_epu16(xmm4, xmm1);
            xmm4 = _mm_adds_epu16(xmm4, xmm2);
            xmm4 = _mm_adds_epu16(xmm4, xmm3);
            xmm4 = _mm_srai_epi16(xmm4, 2);
            xmm4 = _mm_packus_epi16(xmm4, xmm7);
            //8 times Sum = (((*(sLI++))+(*(sLA++))+(*(sLB++))+(*(sLC++))+2)>>2);
            /* write 8 */
            _mm_storel_epi64((__m128i*)rslt, xmm4);
        }
    }


    void ME_SUM_Line_8x8_SSE(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt)
    {
    }

    static const NGV_ALIGN(16, short k16_8[8]) = {16, 16, 16, 16, 16, 16, 16, 16};
    static const NGV_ALIGN(16, short k20_8[8]) = {20, 20, 20, 20, 20, 20, 20, 20};
    static const NGV_ALIGN(16, short k4_8[8]) = {4, 4, 4, 4, 4, 4, 4, 4};

    void ME_SUM_Line_8x8_Intrin_SSE2(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt)
    {

        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;
        __m128i xmm7 = _mm_setzero_si128(); 

        {
            xmm4 = _mm_loadu_si128((__m128i*)k16_8);
            xmm0 = _mm_loadl_epi64((__m128i*)sLx);
            xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);
            xmm1 = _mm_loadl_epi64((__m128i*)sLy);
            xmm1 = _mm_unpacklo_epi8(xmm1, xmm7);
            xmm2 = _mm_loadl_epi64((__m128i*)sLx2);
            xmm2 = _mm_unpacklo_epi8(xmm2, xmm7);
            xmm3 = _mm_loadl_epi64((__m128i*)sLy2);
            xmm3 = _mm_unpacklo_epi8(xmm3, xmm7);
            xmm5 = _mm_adds_epu16(xmm0, xmm1);
            xmm5 = _mm_mullo_epi16(xmm5, *(__m128i*)k20_8);
            xmm6 = _mm_adds_epu16(xmm2, xmm3);
            xmm6 = _mm_mullo_epi16(xmm6, *(__m128i*)k4_8);
            xmm5 = _mm_subs_epu16(xmm5, xmm6);
            xmm4 = _mm_adds_epu16(xmm4, xmm5);
            xmm4 = _mm_srai_epi16(xmm4, 5);
            xmm4 = _mm_packus_epi16(xmm4, xmm7);
            //Sum = ((20*((Ipp32s)(*(sLx++))+(Ipp32s)(*(sLy++)))-4*((Ipp32s)(*(sLx2++))+(Ipp32s)(*(sLy2++)))+16)>>5);
            // Write 8
            _mm_storel_epi64((__m128i*)rslt, xmm4);
        }
    }

    void ME_SUM_cross_SSE(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt)
    {
    }

    void ME_SUM_cross_Intrin_SSE2(Ipp8u *sLI, Ipp8u *sLA, Ipp8u *sLB, Ipp8u *sLC, Ipp8u *rslt)
    {
        __m128i xmm0, xmm1, xmm2, xmm3, xmm4;
        __m128i xmm7 = _mm_setzero_si128(); 

        {
            xmm4 = _mm_loadu_si128((__m128i*)add2_8);
            xmm0 = _mm_loadl_epi64((__m128i*)sLI);
            xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);
            xmm1 = _mm_loadl_epi64((__m128i*)sLA);
            xmm1 = _mm_unpacklo_epi8(xmm1, xmm7);
            xmm2 = _mm_loadl_epi64((__m128i*)sLB);
            xmm2 = _mm_unpacklo_epi8(xmm2, xmm7);
            xmm3 = _mm_loadl_epi64((__m128i*)sLC);
            xmm3 = _mm_unpacklo_epi8(xmm3, xmm7);
            xmm4 = _mm_adds_epu16(xmm4, xmm0);
            xmm4 = _mm_adds_epu16(xmm4, xmm1);
            xmm4 = _mm_adds_epu16(xmm4, xmm2);
            xmm4 = _mm_adds_epu16(xmm4, xmm3);
            xmm4 = _mm_srai_epi16(xmm4, 2);
            xmm4 = _mm_packus_epi16(xmm4, xmm7);
            //8 times Sum = (((*(sLI++))+(*(sLA++))+(*(sLB++))+(*(sLC++))+2)>>2);
            /* write 4 */
            *(int*)(rslt) = _mm_cvtsi128_si32(xmm4);
        }
    }


    void    ME_SUM_Line_SSE(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u * rslt)
    {
        // For 64-bit can start with C code.
        VM_ASSERT(0);

    }


    void ME_SUM_Line_Intrin_SSE2(Ipp8u *sLx, Ipp8u *sLy, Ipp8u *sLx2, Ipp8u *sLy2, Ipp8u *rslt)
    {

        __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6;
        __m128i xmm7 = _mm_setzero_si128(); 

        {
            xmm4 = _mm_loadu_si128((__m128i*)k16_8);
            xmm0 = _mm_loadl_epi64((__m128i*)sLx);
            xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);
            xmm1 = _mm_loadl_epi64((__m128i*)sLy);
            xmm1 = _mm_unpacklo_epi8(xmm1, xmm7);
            xmm2 = _mm_loadl_epi64((__m128i*)sLx2);
            xmm2 = _mm_unpacklo_epi8(xmm2, xmm7);
            xmm3 = _mm_loadl_epi64((__m128i*)sLy2);
            xmm3 = _mm_unpacklo_epi8(xmm3, xmm7);
            xmm5 = _mm_adds_epu16(xmm0, xmm1);
            xmm5 = _mm_mullo_epi16(xmm5, *(__m128i*)k20_8);
            xmm6 = _mm_adds_epu16(xmm2, xmm3);
            xmm6 = _mm_mullo_epi16(xmm6, *(__m128i*)k4_8);
            xmm5 = _mm_subs_epu16(xmm5, xmm6);
            xmm4 = _mm_adds_epu16(xmm4, xmm5);
            xmm4 = _mm_srai_epi16(xmm4, 5);
            xmm4 = _mm_packus_epi16(xmm4, xmm7);
            //Sum = ((20*((Ipp32s)(*(sLx++))+(Ipp32s)(*(sLy++)))-4*((Ipp32s)(*(sLx2++))+(Ipp32s)(*(sLy2++)))+16)>>5);
            /* write 4 */
            *(int*)(rslt) = _mm_cvtsi128_si32(xmm4);
        }
    }


    const Ipp32s    PDISTTbl[NumTSC*NumSC]    =
    {
        0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
        0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
        0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
        0x02,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
        0x01,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
        0x01,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x03,0x03,
        0x01,0x02,0x02,0x02,0x02,0x03,0x03,0x02,0x02,0x02,
        0x01,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x01,0x01,
        0x01,0x01,0x02,0x02,0x02,0x02,0x02,0x02,0x01,0x01,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
    };

    static Ipp64f lmt_sc[NumSC]            =    {4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
    // 9 ranges of SC are: 0 0-4, 1 4-9, 2 9-15, 3 15-23, 4 23-32, 5 32-44, 6 42-53, 7 53-65, 8 65-78, 9 78->??

    static Ipp64f lmt_tsc[NumTSC]            =    {0.75, 1.5, 2.25, 3.0, 4.0, 5.0, 6.0, 7.5, 9.25, INT_MAX};               // lower limit of AFD
    // 8 ranges of TSC (based on FD) are:0 0-0.75 1 0.75-1.5, 2 1.5-2.25. 3 2.25-3, 4 3-4, 5 4-5, 6 5-6, 7 6-7.5, 8 7.5-9.25, 9 9.25->??

    static Ipp64f TH[4]                    =    {-12,-4,4,12};
    static Ipp64f SDetection[5]            =    {1.4602,-17.763,70.524,21.34,100.0};

    void    SceneDetect(imageData Data, imageData DataRef, ImDetails imageInfo, Ipp32s scVal, Ipp64f TSC, Ipp64f AFD, NGV_Bool *Schange, NGV_Bool *Gchange)    
    {
        Ipp8u        *objFrame, *ssFrame, *refFrame;
        Ipp64f        objDCval                =    0;
        Ipp64f        ssDCval                    =    0;
        Ipp64f        refDCval                =    0;
        Ipp64f        RsDiff                    =    0;
        Ipp64f        CsDiff                    =    0;
        Ipp64f        RsCsDiff;
        Ipp64f        gchDC, posBalance, negBalance, histTOT;
        Ipp64f    *objRs, *objCs, *refRs, *refCs;
        Ipp32u        i, j, vpos, pos, rpos, rcpos;
        Ipp32u        histogram[5];
        Ipp32s        imDiff;

        objFrame                        =    Data.Backward.Y + imageInfo.initPoint;
        ssFrame                            =    Data.exImage.Y + imageInfo.initPoint;
        refFrame                        =    DataRef.exImage.Y + imageInfo.initPoint;
        objRs                            =    Data.Rs;
        objCs                            =    Data.Cs;
        refRs                            =    DataRef.Rs;
        refCs                            =    DataRef.Cs;

        for(i=0;i<5;i++)                {
            histogram[i]                =    0;
        }
        for(i=0;i<imageInfo.height;i++)    {
            vpos                        =    imageInfo.exwidth * i;
            for(j=0;j<imageInfo.width;j++)    {
                pos                        =    vpos + j;
                //objDCval                +=    *(objFrame + pos);
                ssDCval                    +=    *(ssFrame + pos);
                refDCval                +=    *(refFrame + pos);
                imDiff                    =    *(ssFrame + pos) - *(refFrame + pos);
                if(imDiff < TH[0])
                    histogram[0]++;
                else if(imDiff < TH[1])
                    histogram[1]++;
                else if(imDiff < TH[2])
                    histogram[2]++;
                else if(imDiff < TH[3])
                    histogram[3]++;
                else
                    histogram[4]++;
            }
        }

        for(i=0;i<imageInfo.hBlocks*2;i++)    {
            rpos                        =    i*imageInfo.wBlocks*2;
            for(j=0;j<imageInfo.wBlocks*2;j++)    {
                rcpos                    =    rpos + j;
                RsDiff                    +=    (*(objRs + rcpos) - *(refRs + rcpos)) * (*(objRs + rcpos) - *(refRs + rcpos));
                CsDiff                    +=    (*(objCs + rcpos) - *(refCs + rcpos)) * (*(objCs + rcpos) - *(refCs + rcpos));
            }
        }
        RsDiff                            /=    imageInfo.hBlocks * imageInfo.wBlocks * 4;
        CsDiff                            /=    imageInfo.hBlocks * imageInfo.wBlocks * 4;
        RsCsDiff                        =    sqrt((RsDiff*RsDiff)+(CsDiff*CsDiff));
        histTOT                            =    (Ipp64f)(histogram[0]+histogram[1]+histogram[2]+histogram[3]+histogram[4]);
        objDCval                        /=    imageInfo.Yframe;
        ssDCval                            /=    imageInfo.Yframe;
        refDCval                        /=    imageInfo.Yframe;
        gchDC                            =    abs(ssDCval-refDCval);
        posBalance                        =    histogram[3] + histogram[4];
        negBalance                        =    histogram[0] + histogram[1];
        posBalance                        -=    negBalance;
        posBalance                        =    abs(posBalance);
        posBalance                        /=    histTOT;
        if((RsCsDiff > 7.6) && (RsCsDiff < 10) && (gchDC > 2.9) && (posBalance > 0.18))
            *Gchange                    =    true;
        if((RsCsDiff >= 10) && (gchDC > 1.6) && (posBalance > 0.18))
            *Gchange                    =    true;

        if(scVal <= 3)                    {
            if(TSC > 18)
                if(AFD > 36 && RsCsDiff > 10)
                    *Schange            =    true;
                else
                    *Schange            =    false;
            else if(TSC > 15)
                if(AFD > 35 && RsCsDiff > 19)
                    *Schange            =    true;
                else
                    *Schange            =    false;
            else if(TSC > 12)
                if(AFD > 30 && RsCsDiff > 19)
                    *Schange            =    true;
                else
                    *Schange            =    false;
            else if(TSC > 10.5)
                if(AFD > 26 && RsCsDiff > 19)
                    *Schange            =    true;
                else
                    *Schange            =    false;
            else if(TSC > 6.5)
                if(AFD > 22 && RsCsDiff > 60)
                    *Schange            =    true;
                else
                    *Schange            =    false;
        }
        else                            {
            if(TSC > 35)
                if(AFD > 80 && RsCsDiff > 1000)
                    *Schange            =    true;
                else
                    *Schange            =    false;
        }
    }

    NGV_Bool    ShotDetect(TSCstat *preAnalysis, Ipp32s position)    
    {
        Ipp32s        current                    =    position,
            past                    =    position - 1;
        Ipp64f        DAFD                    =    0.0,
            radius                    =    0.0;
        NGV_Bool    condition1                =    false,
            condition2                =    false,
            condition3                =    false;
        DAFD                            =    preAnalysis[current].AFD - preAnalysis[past].AFD;
        radius                            =    (preAnalysis[current].AFD * preAnalysis[current].AFD) + (preAnalysis[current].TSC * preAnalysis[current].TSC);

        condition1                        =    ((DAFD*DAFD*DAFD*SDetection[0])+(DAFD*DAFD*SDetection[1])+(DAFD*SDetection[2])+SDetection[3]) > radius;
        condition2                        =    radius > SDetection[4];
        condition3                        =    preAnalysis[current].TSCindex > 1;

        return (condition1 && condition2 && condition3);
    }

} // namespace

#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_H265_PAQ)
/* EOF */
