//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "telecine.h"
#include "../deinterlacer/deinterlacer.h"

#if (defined(LINUX32) || defined(LINUX64))
#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif
#endif


void Pattern_init(Pattern *ptrn)
{
    ptrn->ucLatch.ucFullLatch = FALSE;
    ptrn->ucLatch.ucSHalfLatch = FALSE;
    ptrn->ucLatch.ucThalfLatch = FALSE;
    ptrn->ucPatternFound = FALSE;
    ptrn->ucPatternType = 0;
    ptrn->ucLatch.ucParity = 0;
    ptrn->blendedCount = 0.0;
    ptrn->uiIFlush = 0;
    ptrn->uiPFlush = 0;
    ptrn->uiOverrideHalfFrameRate = FALSE;
    ptrn->uiCountFullyInterlacedBuffers = 0;
    ptrn->uiInterlacedFramesNum = 0;
    ptrn->uiPreviousPartialPattern = 0;
    ptrn->uiPartialPattern = 0;
    ptrn->ulCountInterlacedDetections = 0;
}
void Rotate_Buffer(Frame *frmBuffer[LASTFRAME])
{
    unsigned char i;
    Frame *pfrmBkp;

    pfrmBkp = frmBuffer[0];
    for(i = 0; i < BUFMINSIZE - 1; i++)
        frmBuffer[i] = frmBuffer[i + 1];
    frmBuffer[BUFMINSIZE - 1] = pfrmBkp;
}
void Rotate_Buffer_borders(Frame *frmBuffer[LASTFRAME], unsigned int LastInLatch)
{
    unsigned char i;
    Frame *pfrmBkp;

    for(i = 0; i < (BUFMINSIZE - 1 - (LastInLatch - 1)); i++)
    {
        pfrmBkp = frmBuffer[i];
        frmBuffer[i] = frmBuffer[i + LastInLatch];
        frmBuffer[i + LastInLatch] = pfrmBkp;
    }
}
void Rotate_Buffer_deinterlaced(Frame *frmBuffer[LASTFRAME])
{
    unsigned char i;
    Frame *pfrmBkp;

    pfrmBkp = frmBuffer[0];
    for(i = 0; i < BUFMINSIZE - 1; i++)
        frmBuffer[i] = frmBuffer[i + 1];
    frmBuffer[BUFMINSIZE - 1] = pfrmBkp;
    //pfrmBkp = frmBuffer[BUFMINSIZE];
    //frmBuffer[BUFMINSIZE] = frmBuffer[BUFMINSIZE + 1];
    //frmBuffer[BUFMINSIZE + 1] = pfrmBkp;
}

#ifdef USE_SSE4

/* NOTE: abs() not necessary since we are just summing squares of differences:  (x-y)*(x-y) = abs(x-y)*abs(x-y) */
void SSAD8x2_SSE4(unsigned char *pLine[10], unsigned int offset, double *pGlobalSum, double *pTopSum, double *pBottomSum)
{
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    /* load 8 bytes, zero-extend to 16-bit */
    xmm0 = _mm_loadl_epi64((__m128i *)(pLine[0] + offset));
    xmm1 = _mm_loadl_epi64((__m128i *)(pLine[1] + offset));
    xmm2 = _mm_loadl_epi64((__m128i *)(pLine[2] + offset));
    xmm3 = _mm_loadl_epi64((__m128i *)(pLine[3] + offset));

    xmm0 = _mm_cvtepu8_epi16(xmm0);
    xmm1 = _mm_cvtepu8_epi16(xmm1);
    xmm2 = _mm_cvtepu8_epi16(xmm2);
    xmm3 = _mm_cvtepu8_epi16(xmm3);

    /* G: 0-1 (accumulator xmm5 = globalSum) */
    xmm5 = xmm0;
    xmm5 = _mm_sub_epi16(xmm5, xmm1);
    xmm5 = _mm_madd_epi16(xmm5, xmm5);

    /* T: 0-2 (accumulator xmm6 = topSum) */
    xmm6 = xmm0;
    xmm6 = _mm_sub_epi16(xmm6, xmm2);
    xmm6 = _mm_madd_epi16(xmm6, xmm6);

    /* G: 1-2 */
    xmm4 = xmm1;
    xmm4 = _mm_sub_epi16(xmm4, xmm2);
    xmm4 = _mm_madd_epi16(xmm4, xmm4);
    xmm5 = _mm_add_epi32(xmm5, xmm4);

    /* B: 1-3 (accumulator xmm7 = bottomSum) */
    xmm7 = xmm1;
    xmm7 = _mm_sub_epi16(xmm7, xmm3);
    xmm7 = _mm_madd_epi16(xmm7, xmm7);

    /* load 4,5 */
    xmm0 = _mm_loadl_epi64((__m128i *)(pLine[4] + offset));
    xmm1 = _mm_loadl_epi64((__m128i *)(pLine[5] + offset));
    xmm0 = _mm_cvtepu8_epi16(xmm0);
    xmm1 = _mm_cvtepu8_epi16(xmm1);

    /* G: 2-3 */
    xmm4 = xmm2;
    xmm4 = _mm_sub_epi16(xmm4, xmm3);
    xmm4 = _mm_madd_epi16(xmm4, xmm4);
    xmm5 = _mm_add_epi32(xmm5, xmm4);
    
    /* T: 2-4 */
    xmm2 = _mm_sub_epi16(xmm2, xmm0);
    xmm2 = _mm_madd_epi16(xmm2, xmm2);
    xmm6 = _mm_add_epi32(xmm6, xmm2);
    
    /* G: 3-4 */
    xmm4 = xmm3;
    xmm4 = _mm_sub_epi16(xmm4, xmm0);
    xmm4 = _mm_madd_epi16(xmm4, xmm4);
    xmm5 = _mm_add_epi32(xmm5, xmm4);

    /* B: 3-5 */
    xmm3 = _mm_sub_epi16(xmm3, xmm1);
    xmm3 = _mm_madd_epi16(xmm3, xmm3);
    xmm7 = _mm_add_epi32(xmm7, xmm3);

    /* load 6,7 */
    xmm2 = _mm_loadl_epi64((__m128i *)(pLine[6] + offset));
    xmm3 = _mm_loadl_epi64((__m128i *)(pLine[7] + offset));
    xmm2 = _mm_cvtepu8_epi16(xmm2);
    xmm3 = _mm_cvtepu8_epi16(xmm3);

    /* G: 4-5 */
    xmm4 = xmm0;
    xmm4 = _mm_sub_epi16(xmm4, xmm1);
    xmm4 = _mm_madd_epi16(xmm4, xmm4);
    xmm5 = _mm_add_epi32(xmm5, xmm4);

    /* T: 4-6 */
    xmm0 = _mm_sub_epi16(xmm0, xmm2);
    xmm0 = _mm_madd_epi16(xmm0, xmm0);
    xmm6 = _mm_add_epi32(xmm6, xmm0);

    /* G: 5-6 */
    xmm4 = xmm1;
    xmm4 = _mm_sub_epi16(xmm4, xmm2);
    xmm4 = _mm_madd_epi16(xmm4, xmm4);
    xmm5 = _mm_add_epi32(xmm5, xmm4);
    
    /* B: 5-7 */
    xmm1 = _mm_sub_epi16(xmm1, xmm3);
    xmm1 = _mm_madd_epi16(xmm1, xmm1);
    xmm7 = _mm_add_epi32(xmm7, xmm1);

    /* load 8,9 */
    xmm0 = _mm_loadl_epi64((__m128i *)(pLine[8] + offset));
    xmm1 = _mm_loadl_epi64((__m128i *)(pLine[9] + offset));
    xmm0 = _mm_cvtepu8_epi16(xmm0);
    xmm1 = _mm_cvtepu8_epi16(xmm1);
    
    /* G: 6-7 */
    xmm4 = xmm2;
    xmm4 = _mm_sub_epi16(xmm4, xmm3);
    xmm4 = _mm_madd_epi16(xmm4, xmm4);
    xmm5 = _mm_add_epi32(xmm5, xmm4);
    
    /* T: 6-8 */
    xmm2 = _mm_sub_epi16(xmm2, xmm0);
    xmm2 = _mm_madd_epi16(xmm2, xmm2);
    xmm6 = _mm_add_epi32(xmm6, xmm2);

    /* G: 7-8 */
    xmm4 = xmm3;
    xmm4 = _mm_sub_epi16(xmm4, xmm0);
    xmm4 = _mm_madd_epi16(xmm4, xmm4);
    xmm5 = _mm_add_epi32(xmm5, xmm4);

    /* B: 7-9 */
    xmm3 = _mm_sub_epi16(xmm3, xmm1);
    xmm3 = _mm_madd_epi16(xmm3, xmm3);
    xmm7 = _mm_add_epi32(xmm7, xmm3);

    /* add together 4 32-bit values for overall globalSum */
    xmm5 = _mm_hadd_epi32(xmm5, xmm5);
    xmm5 = _mm_hadd_epi32(xmm5, xmm5);

    /* add together 2 32-bit values each for topSum, bottomSum */
    xmm6 = _mm_hadd_epi32(xmm6, xmm7);
    xmm6 = _mm_hadd_epi32(xmm6, xmm6);

    /* convert to doubles only at end */
    *pGlobalSum = (double)_mm_extract_epi32(xmm5, 0);
    *pTopSum    = (double)_mm_extract_epi32(xmm6, 0);
    *pBottomSum = (double)_mm_extract_epi32(xmm6, 1);
}

#endif    /* USE_SSE4 */

double SSAD8x2(unsigned char *line1, unsigned char *line2, unsigned int offset)
{
    int
        i = 0;
    double
        totalSAD = 0.0, preSAD = 0.0;
    
    for (i = 0; i < 8; i++)
    {
        preSAD = PASAD(line1[offset + i],line2[offset + i]);
        totalSAD += (preSAD * preSAD);
    }


    return totalSAD;
}
void Rs_measurement(Frame *pfrmIn)
{
    unsigned int
        i = 0, j = 0;
    int
        k = 0, off = 0,
        limit = pfrmIn->plaY.uiHeight - 8,
        halfsize = pfrmIn->plaY.uiSize >> 1;//,
    double
        globalSum = 0,
        topSum = 0, 
        bottomSum = 0,
        data = 0.0;
    double
        *Rs = pfrmIn->plaY.ucStats.ucRs;
#if DEBUG_RS
    FILE *dataout = NULL;
#endif
    

    __int64 diffVal = 0, RsCollector[12];
            
    unsigned char *pLine[10];
    unsigned char pixelVal = 0;

    for(i = 0; i < 12; i++)
    {
        RsCollector[i] = 0;
        Rs[i] = 0.0;
    }
#if DEBUG_RS
    dataout = fopen("Rs_data.txt","a");
#endif
    for(i = 0; i < pfrmIn->plaY.uiHeight; i += 8)
    {
        off = i * pfrmIn->plaY.uiStride;
        setLinePointers(pLine, *pfrmIn, off, (i == limit));
        pixelVal = pLine[0][0];
        for(j = 0; j < pfrmIn->plaY.uiWidth; j += 8)
        {
#ifdef USE_SSE4
            SSAD8x2_SSE4(pLine, j, &globalSum, &topSum, &bottomSum);
#else
            for(k = 0; k < 8; k++)
            {
                globalSum += SSAD8x2(pLine[k], pLine[k + 1], j);
                if(k%2)
                {
                    bottomSum += SSAD8x2(pLine[k], pLine[k + 2], j);
                }
                else
                {
                    topSum += SSAD8x2(pLine[k], pLine[k + 2], j);
                }
            }
#endif
            data = globalSum;
            Rs[0] += globalSum;
            Rs[1] += bottomSum;
            Rs[2] += topSum;

            if(max(pfrmIn->plaY.ucStats.ucSAD[0],pfrmIn->plaY.ucStats.ucSAD[1]) < 0.45)
                data = 31.0;
            else if(max(pfrmIn->plaY.ucStats.ucSAD[0], pfrmIn->plaY.ucStats.ucSAD[1]) < 0.5)
                data = 63.0;
            else if(max(pfrmIn->plaY.ucStats.ucSAD[0], pfrmIn->plaY.ucStats.ucSAD[1]) < 1)
                data = 127.0;
            else if(max(pfrmIn->plaY.ucStats.ucSAD[0], pfrmIn->plaY.ucStats.ucSAD[1]) < 1.5)
                data = 255.0;
            else if(max(pfrmIn->plaY.ucStats.ucSAD[0], pfrmIn->plaY.ucStats.ucSAD[1]) < 2)
                data = 512.0;
            else if(max(pfrmIn->plaY.ucStats.ucSAD[0], pfrmIn->plaY.ucStats.ucSAD[1]) < 2.5)
                data = 1023.0;
            else if(max(pfrmIn->plaY.ucStats.ucSAD[0], pfrmIn->plaY.ucStats.ucSAD[1]) < 3)
                data = 2047.0;
            else
                data = 4095.0;
            
            if((globalSum - topSum) > 4095.0)
            {
                Rs[4]++;
                Rs[5] += (globalSum - topSum);
            }
            
            if((globalSum - topSum) > data)
            {
                Rs[6]++;
                Rs[7] += (globalSum - topSum);
            }

            if(globalSum == 0.0 && pixelVal != TVBLACK)
                Rs[9]++;
            if(globalSum >= 16383.0)
                Rs[10]++;

#if DEBUG_RS
            fprintf(dataout, "%i\t", globalSum);
#endif
            topSum = PASAD(bottomSum, topSum);
            Rs[3] += topSum;


            globalSum = 0;
            bottomSum = 0;
            topSum = 0;
        }
#if DEBUG_RS
        fprintf(dataout, "\n");
#endif
    }
#if DEBUG_RS
    fprintf(dataout, "\n");
#endif
    Rs[0] = sqrt(Rs[0] / pfrmIn->plaY.uiSize);
    Rs[1] = sqrt(Rs[1] / halfsize);
    Rs[2] = sqrt(Rs[2] / halfsize);
    Rs[3] = sqrt(Rs[3] / halfsize);
    Rs[5] /= 1000;
    if(Rs[6] != 0)
        Rs[8] = atan((Rs[7] / pfrmIn->plaY.uiSize)/ Rs[6]);
    else
        Rs[8] = 1000;
    Rs[7] /= 1000;
    Rs[9] = (Rs[9] / pfrmIn->plaY.uiSize) * 1000;
    Rs[10] = (Rs[10] / pfrmIn->plaY.uiSize) * 1000;

#if DEBUG_RS
    fclose(dataout);
#endif
}
void setLinePointers(unsigned char *pLine[10], Frame pfrmIn, int offset, BOOL lastStripe)
{
    int i = 0;
    pLine[0] = pfrmIn.plaY.ucCorner + offset;
    for(i = 1; i < 8; i++)
        pLine[i] = pLine[i - 1] + pfrmIn.plaY.uiStride;
    if(lastStripe)
    {
        pLine[8] = pLine[6] + pfrmIn.plaY.uiStride;
        pLine[9] = pLine[6] + pfrmIn.plaY.uiStride;
    }
    else
    {
        pLine[8] = pLine[7] + pfrmIn.plaY.uiStride;
        pLine[9] = pLine[8] + pfrmIn.plaY.uiStride;
    }
}
void Line_rearrangement(unsigned char *pFrmDstTop,unsigned char *pFrmDstBottom, unsigned char **pucIn, Plane planeOut,unsigned int *off)
{
    ptir_memcpy(pFrmDstTop + *off, *pucIn, planeOut.uiWidth);
    *pucIn += planeOut.uiWidth;

    ptir_memcpy(pFrmDstBottom + *off, *pucIn, planeOut.uiWidth);
    *pucIn += planeOut.uiWidth;

    *off += planeOut.uiStride;
}
void Extract_Fields_I420(unsigned char *pucLine, Frame *pfrmOut, BOOL TopFieldFirst)
{
    unsigned int i = 0, j = 0, off = 0,
                 lines = pfrmOut->plaY.uiHeight >> 1,
                 chroma_lines =  pfrmOut->plaU.uiHeight >> 1;
    unsigned char *phFrmY = pfrmOut->plaY.ucCorner + pfrmOut->plaY.uiStride * lines,
                  *phFrmU = pfrmOut->plaU.ucCorner + pfrmOut->plaU.uiStride * chroma_lines,
                  *phFrmV = pfrmOut->plaV.ucCorner + pfrmOut->plaV.uiStride * chroma_lines,
                  *pFrmY = pfrmOut->plaY.ucCorner,
                  *pFrmU = pfrmOut->plaU.ucCorner,
                  *pFrmV = pfrmOut->plaV.ucCorner;

    if(!TopFieldFirst)
    {
        pFrmY = pfrmOut->plaY.ucCorner + pfrmOut->plaY.uiStride * lines;
        pFrmU = pfrmOut->plaU.ucCorner + pfrmOut->plaU.uiStride * chroma_lines;
        pFrmV = pfrmOut->plaV.ucCorner + pfrmOut->plaV.uiStride * chroma_lines;

        phFrmY = pfrmOut->plaY.ucCorner;
        phFrmU = pfrmOut->plaU.ucCorner;
        phFrmV = pfrmOut->plaV.ucCorner;
    }
    for(i = 0; i < lines; i++)
        Line_rearrangement(pFrmY, phFrmY, &pucLine, pfrmOut->plaY, &off);

    off = 0;
    for(i = 0; i < chroma_lines; i++)
        Line_rearrangement(pFrmU, phFrmU, &pucLine, pfrmOut->plaU, &off);

    off = 0;
    for(i = 0; i < chroma_lines; i++)
        Line_rearrangement(pFrmV, phFrmV, &pucLine, pfrmOut->plaV, &off);

}

#ifdef USE_SSE4

void sadCalc_I420_frame(Frame *pfrmCur, Frame *pfrmPrv)
{
    unsigned int i, j, lines, halflines, width, halfpixels, rem;
    unsigned int sadU32[5];
    double *SADVal;
    unsigned char *pCur0, *pCur1, *pPrv0, *pPrv1;

    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    lines = pfrmCur->plaY.uiHeight;
    halflines = pfrmCur->plaY.uiHeight >> 1;
    width = pfrmCur->plaY.uiWidth;
    halfpixels = pfrmCur->plaY.uiSize >> 1;
    SADVal = pfrmCur->plaY.ucStats.ucSAD;

    rem = width & 0x0f;
    width -= rem;

    pCur0 = pfrmCur->plaY.ucCorner;
    pCur1 = pfrmCur->plaY.ucCorner + pfrmCur->plaY.uiStride;
    pPrv0 = pfrmPrv->plaY.ucCorner;
    pPrv1 = pfrmPrv->plaY.ucCorner + pfrmPrv->plaY.uiStride;

    xmm3 = _mm_setzero_si128();        /* C0*C1 */
    xmm4 = _mm_setzero_si128();        /* C0*P0 */
    xmm5 = _mm_setzero_si128();        /* C0*P1 */
    xmm6 = _mm_setzero_si128();        /* C1*P0 */
    xmm7 = _mm_setzero_si128();        /* C1*P1 */

    for (i = 0; i < lines; i += 2) {
        for (j = 0; j < width; j += 16) {
            xmm0 = _mm_loadu_si128((__m128i *)(pCur0 + j));        /* C0 */
            xmm1 = _mm_loadu_si128((__m128i *)(pPrv0 + j));        /* P0 */
            xmm2 = _mm_loadu_si128((__m128i *)(pCur1 + j));        /* C1 */

            /* C0 x C1 */
            xmm2 = _mm_sad_epu8(xmm2, xmm0);
            xmm3 = _mm_add_epi32(xmm3, xmm2);

            /* C0 x P0 */
            xmm2 = xmm1;
            xmm2 = _mm_sad_epu8(xmm2, xmm0);
            xmm4 = _mm_add_epi32(xmm4, xmm2);

            /* C0 x P1 */
            xmm2 = _mm_loadu_si128((__m128i *)(pPrv1 + j));        /* P1 */
            xmm0 = _mm_sad_epu8(xmm0, xmm2);
            xmm5 = _mm_add_epi32(xmm5, xmm0);

            /* C1 x P0 */
            xmm0 = _mm_loadu_si128((__m128i *)(pCur1 + j));        /* C1 */
            xmm1 = _mm_sad_epu8(xmm1, xmm0);
            xmm6 = _mm_add_epi32(xmm6, xmm1);

            /* C1 x P1 */
            xmm0 = _mm_sad_epu8(xmm0, xmm2);
            xmm7 = _mm_add_epi32(xmm7, xmm0);
        }

        pCur0 += 2*pfrmCur->plaY.uiStride;
        pCur1 += 2*pfrmCur->plaY.uiStride;
        pPrv0 += 2*pfrmPrv->plaY.uiStride;
        pPrv1 += 2*pfrmPrv->plaY.uiStride;
    }

    sadU32[InterStraightTop]    = (_mm_extract_epi32(xmm4, 0) + _mm_extract_epi32(xmm4, 2));
    sadU32[InterStraightBottom] = (_mm_extract_epi32(xmm7, 0) + _mm_extract_epi32(xmm7, 2));
    sadU32[InterCrossTop]       = (_mm_extract_epi32(xmm5, 0) + _mm_extract_epi32(xmm5, 2));
    sadU32[InterCrossBottom]    = (_mm_extract_epi32(xmm6, 0) + _mm_extract_epi32(xmm6, 2));
    sadU32[IntraSAD]            = (_mm_extract_epi32(xmm3, 0) + _mm_extract_epi32(xmm3, 2));

    /* handle tails (skipped unless frame width != multiple of 16) */
    if (rem) {
        pCur0 = pfrmCur->plaY.ucCorner;
        pCur1 = pfrmCur->plaY.ucCorner + pfrmCur->plaY.uiStride;
        pPrv0 = pfrmPrv->plaY.ucCorner;
        pPrv1 = pfrmPrv->plaY.ucCorner + pfrmPrv->plaY.uiStride;

        for (i = 0; i < lines; i += 2) {
            for (j = width; j < width + rem; j++) {
                sadU32[InterStraightTop]    += abs(pCur0[j] - pPrv0[j]);
                sadU32[InterStraightBottom] += abs(pCur1[j] - pPrv1[j]);
                sadU32[InterCrossTop]       += abs(pCur0[j] - pPrv1[j]);
                sadU32[InterCrossBottom]    += abs(pCur1[j] - pPrv0[j]);
                sadU32[IntraSAD]            += abs(pCur0[j] - pCur1[j]);
            }

            pCur0 += 2*pfrmCur->plaY.uiStride;
            pCur1 += 2*pfrmCur->plaY.uiStride;
            pPrv0 += 2*pfrmPrv->plaY.uiStride;
            pPrv1 += 2*pfrmPrv->plaY.uiStride;
        }
    }

    SADVal[InterStraightTop]    = (double)sadU32[InterStraightTop] / halfpixels;
    SADVal[InterStraightBottom] = (double)sadU32[InterStraightBottom] / halfpixels;
    SADVal[InterCrossTop]       = (double)sadU32[InterCrossTop] / halfpixels;
    SADVal[InterCrossBottom]    = (double)sadU32[InterCrossBottom] / halfpixels;
    SADVal[IntraSAD]            = (double)sadU32[IntraSAD] / halfpixels;
}

#else    /* USE_SSE4 */

void sadCalc_I420_frame(Frame *pfrmCur, Frame *pfrmPrv)
{
    unsigned int i = 0, j = 0, off = 0, sad = 0,
                 lines = pfrmCur->plaY.uiHeight,
                 halflines = pfrmCur->plaY.uiHeight >> 1,
                 width = pfrmCur->plaY.uiWidth,
                 halfpixels = pfrmCur->plaY.uiSize >> 1,
                 pixelLinePosition = 0,
                 pixelPosition = 0;

    unsigned char *pfrmPrvY = pfrmPrv->plaY.ucCorner,
                  *pfrmCurY = pfrmCur->plaY.ucCorner;

    double data = 0.0;
    double *SADVal = pfrmCur->plaY.ucStats.ucSAD;

    //Inter Straight SAD
    for(i = 0; i < lines; i += 2)
    {
        pixelLinePosition = i * pfrmCur->plaY.uiStride;
        for(j = 0; j < width; j ++)
        {
            pixelPosition = pixelLinePosition + j;
            sad += PASAD(pfrmCurY[pixelPosition], pfrmPrvY[pixelPosition]);
        }
    }
    data = (double) sad;// / halfpixels;
    SADVal[InterStraightTop] = (double) sad;
    SADVal[InterStraightTop] /= halfpixels;

    sad = 0;
    for(i = 1; i < lines; i += 2)
    {
        pixelLinePosition = i * pfrmCur->plaY.uiStride;
        for(j = 0; j < width; j ++)
        {
            pixelPosition = pixelLinePosition + j;
            sad += PASAD(pfrmCurY[pixelPosition], pfrmPrvY[pixelPosition]);
        }
    }
    data = (double) sad;// / halfpixels;
    SADVal[InterStraightBottom] = (double) sad;
    SADVal[InterStraightBottom] /= halfpixels;

    //Inter Cross SAD - Current Top vs Previous Bottom
    sad = 0;
    for(i = 0; i < lines; i += 2)
    {
        pixelLinePosition = i * pfrmCur->plaY.uiStride;
        for(j = 0; j < width; j ++)
        {
            pixelPosition = pixelLinePosition + j;
            sad += PASAD(pfrmCurY[pixelPosition], pfrmPrvY[pixelPosition + pfrmCur->plaY.uiStride]);
        }
    }
    data = (double) sad;// / halfpixels;
    SADVal[InterCrossTop] = (double) sad;
    SADVal[InterCrossTop] /= halfpixels;

    //Inter Cross SAD - Current Top vs Previous Bottom
    sad = 0;
    for(i = 1; i < lines; i += 2)
    {
        pixelLinePosition = i * pfrmCur->plaY.uiStride;
        for(j = 0; j < width; j ++)
        {
            pixelPosition = pixelLinePosition + j;
            sad += PASAD(pfrmCurY[pixelPosition], pfrmPrvY[pixelPosition - pfrmCur->plaY.uiStride]);
        }
    }
    data = (double) sad;// / halfpixels;
    SADVal[InterCrossBottom] = (double) sad;
    SADVal[InterCrossBottom] /= halfpixels;

    //Intra SAD
    sad = 0;
    for(i = 0; i < lines; i += 2)
    {
        pixelLinePosition = i * pfrmCur->plaY.uiStride;
        for(j = 0; j < width; j ++)
        {
            pixelPosition = pixelLinePosition + j;
            sad += PASAD(pfrmCurY[pixelPosition], pfrmCurY[pixelPosition + pfrmCur->plaY.uiStride]);
        }
    }
    data = (double) sad;// / halfpixels;
    SADVal[IntraSAD] = (double) sad;
    SADVal[IntraSAD] /= halfpixels;
}

#endif

void Detect_Solve_32RepeatedFramePattern(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    unsigned int i, position,
                 countDrop = 0,
                 countStay = 0;

    const unsigned int framesToDispatch[6] = {2, 3, 4, 5, 1, 5};

    position = BUFMINSIZE;

    for(i = 0; i < BUFMINSIZE - 1; i++)
    {
        if((pFrm[i]->plaY.ucStats.ucSAD[0] < 0.002) &&
           (pFrm[i]->plaY.ucStats.ucSAD[1] < 0.002) &&
           (pFrm[i]->plaY.ucStats.ucRs[8] < 0.060) &&
           ((i > 0 ? (pFrm[i]->plaY.ucStats.ucRs[8] < pFrm[i - 1]->plaY.ucStats.ucRs[8]) : 1)) &&
           ((i < 5 ? (pFrm[i]->plaY.ucStats.ucRs[8] < pFrm[i + 1]->plaY.ucStats.ucRs[8]) : 1)))
        {
            position = i;
            countDrop++;
        }
        else if((pFrm[i]->plaY.ucStats.ucSAD[0] >= 0.002) || (pFrm[i]->plaY.ucStats.ucSAD[1] >= 0.002))
            countStay++;
    }
    
    if((position < BUFMINSIZE) && (countDrop == 1) && (countStay == BUFMINSIZE - 2))
    {
        ptrn->ucPatternFound = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE; 
        ptrn->ucPatternType = 1;
        pFrm[position]->frmProperties.drop = TRUE;
        *dispatch = 5;
    }
}

void Detect_Solve_41FramePattern(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    unsigned int i, position,
                 countUndo = 0,
                 countStay = 0;

    position = BUFMINSIZE;

    for(i = 0; i < BUFMINSIZE - 1; i++)
    {
        if((pFrm[i]->plaY.ucStats.ucSAD[4] < pFrm[i]->plaY.ucStats.ucSAD[2]) &&
           (pFrm[i]->plaY.ucStats.ucSAD[4] < pFrm[i]->plaY.ucStats.ucSAD[3]) &&
           ((i > 0 ? (pFrm[i]->plaY.ucStats.ucRs[0] < pFrm[i - 1]->plaY.ucStats.ucRs[0]) : 1)) &&
           ((i < 5 ? (pFrm[i]->plaY.ucStats.ucRs[0] < pFrm[i + 1]->plaY.ucStats.ucRs[0]) : 1)) &&
           ((i > 0 ? ((pFrm[i]->plaY.ucStats.ucRs[5] / pFrm[i]->plaY.uiSize * 1000 * 2) < (pFrm[i - 1]->plaY.ucStats.ucRs[5] / pFrm[i - 1]->plaY.uiSize * 1000)) : 1)) &&
           ((i < 5 ? ((pFrm[i]->plaY.ucStats.ucRs[5] / pFrm[i]->plaY.uiSize * 1000 * 2) < (pFrm[i + 1]->plaY.ucStats.ucRs[5] / pFrm[i + 1]->plaY.uiSize * 1000)) : 1)) &&
           ((i > 0 ? ((pFrm[i]->plaY.ucStats.ucRs[7] / pFrm[i]->plaY.uiSize * 1000 * 2) < (pFrm[i - 1]->plaY.ucStats.ucRs[7] / pFrm[i - 1]->plaY.uiSize * 1000)) : 1)) &&
           ((i < 5 ? ((pFrm[i]->plaY.ucStats.ucRs[7] / pFrm[i]->plaY.uiSize * 1000 * 2) < (pFrm[i + 1]->plaY.ucStats.ucRs[7] / pFrm[i + 1]->plaY.uiSize * 1000)) : 1)))
        {
            position = i;
            countStay++;
        }
        else
        {
            countUndo++;
        }
    }

    if((position < BUFMINSIZE) && (countStay == 1) && (countUndo == BUFMINSIZE - 2) && (pFrm[position]->plaY.uiHeight < 720))
    {
        if(position == BUFMINSIZE - 2)
            pFrm[0]->frmProperties.drop = TRUE;
        else
            pFrm[position + 1]->frmProperties.drop = TRUE;

        /*if(position == 0)
        {
            if(pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3])
                ptrn->ucLatch.ucParity = 1;
            else
                ptrn->ucLatch.ucParity = 0;
            Undo2Frames(pFrm[1],pFrm[2],ptrn->ucLatch.ucParity);
            Undo2Frames(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
            Undo2Frames(pFrm[3],pFrm[4],ptrn->ucLatch.ucParity);
            Undo2Frames(pFrm[4],pFrm[5],ptrn->ucLatch.ucParity);
            pFrm[1]->frmProperties.candidate = TRUE;
            pFrm[2]->frmProperties.candidate = TRUE;
            pFrm[3]->frmProperties.candidate = TRUE;
            pFrm[4]->frmProperties.candidate = TRUE;
        }*/

        //else if(position == 1)
        //{
        //    if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3])
        //        ptrn->ucLatch.ucParity = 1;
        //    else
        //        ptrn->ucLatch.ucParity = 0;

        //    Undo2Frames(pFrm[0],pFrm[1],ptrn->ucLatch.ucParity);
        //    Undo2Frames(pFrm[3],pFrm[4],ptrn->ucLatch.ucParity);
        //    Undo2Frames(pFrm[4],pFrm[5],ptrn->ucLatch.ucParity);
        //    pFrm[0]->frmProperties.candidate = TRUE;
        //    pFrm[3]->frmProperties.candidate = TRUE;
        //    pFrm[4]->frmProperties.candidate = TRUE;
        //}

        //else if(position == 2)
        //{
        //    if(pFrm[4]->plaY.ucStats.ucSAD[2] > pFrm[4]->plaY.ucStats.ucSAD[3])
        //        ptrn->ucLatch.ucParity = 1;
        //    else
        //        ptrn->ucLatch.ucParity = 0;

        //    Undo2Frames(pFrm[0],pFrm[1],ptrn->ucLatch.ucParity);
        //    Undo2Frames(pFrm[1],pFrm[2],ptrn->ucLatch.ucParity);
        //    Undo2Frames(pFrm[4],pFrm[5],ptrn->ucLatch.ucParity);
        //    pFrm[0]->frmProperties.candidate = TRUE;
        //    pFrm[1]->frmProperties.candidate = TRUE;
        //    pFrm[4]->frmProperties.candidate = TRUE;
        //}

        //else if(position == 3)
        //{
        //    if(pFrm[1]->plaY.ucStats.ucSAD[2] > pFrm[1]->plaY.ucStats.ucSAD[3])
        //        ptrn->ucLatch.ucParity = 1;
        //    else
        //        ptrn->ucLatch.ucParity = 0;

        //    Undo2Frames(pFrm[0],pFrm[1],ptrn->ucLatch.ucParity);
        //    Undo2Frames(pFrm[1],pFrm[2],ptrn->ucLatch.ucParity);
        //    Undo2Frames(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
        //    pFrm[0]->frmProperties.candidate = TRUE;
        //    pFrm[1]->frmProperties.candidate = TRUE;
        //    pFrm[2]->frmProperties.candidate = TRUE;
        //}

        //else/* if(position == 4)*/
        //{
        //    if(pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3])
        //        ptrn->ucLatch.ucParity = 1;
        //    else
        //        ptrn->ucLatch.ucParity = 0;

        //    Undo2Frames(pFrm[1],pFrm[2],ptrn->ucLatch.ucParity);
        //    Undo2Frames(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
        //    Undo2Frames(pFrm[3],pFrm[4],ptrn->ucLatch.ucParity);
        //    pFrm[1]->frmProperties.candidate = TRUE;
        //    pFrm[2]->frmProperties.candidate = TRUE;
        //    pFrm[3]->frmProperties.candidate = TRUE;
        //}

        ptrn->ucPatternFound = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE; 
        ptrn->ucPatternType = 1;
        *dispatch = 5;
    }
}

void Detect_Solve_32BlendedPatterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    unsigned int i, count, start, previousPattern;
    BOOL condition[10];

    ptrn->ucLatch.ucFullLatch = FALSE;
    ptrn->ucLatch.ucThalfLatch = FALSE;
    previousPattern = ptrn->ucPatternType;
    ptrn->ucPatternType = 0;
    if (pFrm[0]->plaY.uiHeight > 720)
    {
        for (i = 0; i < 10; i++)
            condition[i] = 0;

        if(previousPattern != 5 && previousPattern != 7)
        {
            condition[0] = (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[1]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[2]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[3]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[0]->plaY.ucStats.ucRs[4] + 2 < pFrm[4]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[0]->plaY.ucStats.ucRs[4] < 100);

            condition[1] = (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[0]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[1]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[2]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[4]->plaY.ucStats.ucRs[4] + 2 < pFrm[3]->plaY.ucStats.ucRs[4]) &&
                           (pFrm[4]->plaY.ucStats.ucRs[4] < 100);

            if(condition[0] && !condition[1])
                start = 1;
            else if(!condition[0] && condition[1])
                start = 0;
            else
            {
                if(!pFrm[5]->frmProperties.interlaced)
                    ptrn->ucLatch.ucSHalfLatch = TRUE;
                return;
            }

            for(i = 0; i < 4; i++)
                condition[i + start] = (pFrm[i + start]->plaY.ucStats.ucRs[6] > 100) && (pFrm[i + start]->plaY.ucStats.ucRs[7] > 0.1);

            count = 1;
            for(i = 1; i < 4; i++)
                count += condition[i];
            if(count > 3)
            {
                if(condition[0])
                {
                    start = 1;
                    pFrm[4]->frmProperties.drop = TRUE;
                    *dispatch = 5;
                }
                else
                {
                    start = 0;
                    pFrm[3]->frmProperties.drop = TRUE;//FALSE;
                    pFrm[3]->frmProperties.candidate = TRUE;
                    *dispatch = 4;
                }
                UndoPatternTypes5and7(pFrm, start);
                ptrn->ucLatch.ucFullLatch = TRUE;
                if(previousPattern != 6)
                    ptrn->ucPatternType = 5;
                else
                    ptrn->ucPatternType = 7;
            }
        }
        else
        {
            count = 0;
            for(i = 0; i < 3; i++)
                count += pFrm[i]->frmProperties.interlaced;
            if(count < 3)
            {
                if(previousPattern != 7)
                {
                    ptrn->ucLatch.ucFullLatch = TRUE;
                    ptrn->blendedCount += BLENDEDOFF;
                    if(ptrn->blendedCount > 1)
                    {
                        DeinterlaceMedianFilter(pFrm, 1, BUFMINSIZE, 0);
                        ptrn->blendedCount -= 1;
                    }
                    else
                    {
                        pFrm[1]->frmProperties.drop = FALSE;//TRUE;
                        /*pFrm[1]->frmProperties.candidate;*/
                    }
                    ptrn->ucPatternType = 6;
                    *dispatch = 2;
                }
                else
                {
                    count = 0;
                    for(i = 3; i < 5; i++)
                        count += pFrm[i]->frmProperties.interlaced;
                    if(count > 1 && pFrm[5]->frmProperties.interlaced)
                    {
                        ptrn->ucLatch.ucFullLatch = TRUE;
                        ptrn->ucPatternType = 8;
                        *dispatch = 2;
                    }
                    else
                    {
                        ptrn->ucLatch.ucFullLatch = TRUE;
                        ptrn->ucPatternType = 1;
                        *dispatch = 1;
                    }
                }
            }
        }
    }
}
void Detect_Solve_32Patterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    BOOL condition[10];

    //Case: First and last frames have pattern, need to move buffer two frames up
    //Texture Analysis
    condition[0] = (pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[0]->frmProperties.interlaced;
    condition[1] = (pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[4]->frmProperties.interlaced;
    condition[2] = (pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[5]->frmProperties.interlaced;
    //SAD analysis
    condition[3] = pFrm[0]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[0]->plaY.ucStats.ucSAD[2]),(pFrm[0]->plaY.ucStats.ucSAD[3]));
    condition[4] = pFrm[4]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[4]->plaY.ucStats.ucSAD[2]),(pFrm[4]->plaY.ucStats.ucSAD[3]));
    condition[5] = pFrm[5]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[5]->plaY.ucStats.ucSAD[2]),(pFrm[5]->plaY.ucStats.ucSAD[3]));


    if(condition[0] && condition[1] && condition[2] && condition[3] && condition[4] && condition[5])
    {
        pFrm[0]->frmProperties.drop = FALSE;//TRUE;
        pFrm[0]->frmProperties.drop24fps = TRUE;
        pFrm[0]->frmProperties.candidate = TRUE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE;
        if(pFrm[4]->plaY.ucStats.ucSAD[2] > pFrm[4]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        *dispatch = 2;
        return;
    }
    
    //Case: First and second frames have pattern, need to move buffer three frames up
    //Texture Analysis
    condition[0] = (pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[0]->frmProperties.interlaced;
    condition[1] = (pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[1]->frmProperties.interlaced;
    condition[2] = (pFrm[5]->plaY.ucStats.ucRs[7] >= (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] >= (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] >= (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[5]->frmProperties.interlaced;
    //SAD analysis
    condition[3] = pFrm[0]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[0]->plaY.ucStats.ucSAD[2]),(pFrm[0]->plaY.ucStats.ucSAD[3]));
    condition[4] = pFrm[1]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[1]->plaY.ucStats.ucSAD[2]),(pFrm[1]->plaY.ucStats.ucSAD[3]));
    //condition[5] = pFrm[5]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[5]->plaY.ucStats.ucSAD[2]),(pFrm[5]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3] && condition[4]/* && condition[5]*/)
    {
        pFrm[0]->frmProperties.drop = FALSE;
        pFrm[1]->frmProperties.drop = FALSE;//TRUE;
        pFrm[1]->frmProperties.drop24fps = TRUE;
        pFrm[1]->frmProperties.candidate = TRUE;
        pFrm[2]->frmProperties.drop = FALSE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE;

        if(pFrm[1]->plaY.ucStats.ucSAD[2] > pFrm[1]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;

        *dispatch = 3;
        Undo2Frames(pFrm[0],pFrm[1],ptrn->ucLatch.ucParity);
        return;
    }

    //Case: second and third frames have pattern, need to move buffer four frames up
    //Texture Analysis
    condition[0] = (pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[1]->frmProperties.interlaced;
    condition[1] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[2]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[1]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[1]->plaY.ucStats.ucSAD[2]),(pFrm[1]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[2]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3])
    {
        pFrm[0]->frmProperties.drop = FALSE;
        pFrm[1]->frmProperties.drop = FALSE;
        pFrm[2]->frmProperties.drop = FALSE;/*TRUE*/
        pFrm[2]->frmProperties.drop24fps = TRUE;
        pFrm[2]->frmProperties.candidate = TRUE;
        pFrm[3]->frmProperties.drop = FALSE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE;
        if(pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;

        *dispatch = 4;
        Undo2Frames(pFrm[1],pFrm[2],ptrn->ucLatch.ucParity);
        return;
    }

    //Case: third and fourth frames have pattern, pattern is synchronized
    //Texture Analysis
    condition[0] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[2]->frmProperties.interlaced;
    condition[1] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[3]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[2]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[3]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
    if(condition[0] && condition[1] && condition[2] && condition[3])
    {
        pFrm[3]->frmProperties.drop = TRUE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = FALSE;
        ptrn->ucLatch.ucFullLatch = TRUE;
        if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        *dispatch = 5;
        Undo2Frames(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
        return;
    }

    //Case: fourth and fifth frames have pattern, one frame needs to be moved out
    //Texture Analysis
    condition[0] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7])) ||
                   pFrm[3]->frmProperties.interlaced;
    condition[1] = (pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7])) ||
                   pFrm[4]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[3]->plaY.ucStats.ucSAD[4] /** T1*/ > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[4]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[4]->plaY.ucStats.ucSAD[2]),(pFrm[4]->plaY.ucStats.ucSAD[3]));
    if((condition[0] && condition[1] && condition[2]) || (condition[0] && condition[1] && condition[3]))
    {
        pFrm[0]->frmProperties.drop = FALSE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE;
        if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3])
            ptrn->ucLatch.ucParity = 1;
        else
            ptrn->ucLatch.ucParity = 0;
        ptrn->ucPatternType = 1;
        pFrm[0]->frmProperties.candidate = TRUE;

        *dispatch = 1;
        return;
    }
}
void Detect_Solve_3223Patterns(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch)
{
    BOOL condition[10];

    //Case: First frame has the pattern, need to move buffer two frames up
    //Texture Analysis
    condition[0] = (pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[0]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[0]->frmProperties.interlaced;
    condition[1] = (pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[5]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[5]->frmProperties.interlaced;
    //SAD analysis
    condition[2] = pFrm[0]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[0]->plaY.ucStats.ucSAD[2]),(pFrm[0]->plaY.ucStats.ucSAD[3]));
    condition[3] = pFrm[5]->plaY.ucStats.ucSAD[4] /** T1*/ > min((pFrm[5]->plaY.ucStats.ucSAD[2]),(pFrm[5]->plaY.ucStats.ucSAD[3]));


    if(condition[0] && condition[1] && condition[2] && condition[3])
    {
        pFrm[0]->frmProperties.drop = FALSE;//TRUE;
        pFrm[0]->frmProperties.drop24fps = TRUE;
        pFrm[1]->frmProperties.drop = FALSE;//TRUE;
        pFrm[1]->frmProperties.candidate = TRUE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = TRUE;
        ptrn->ucLatch.ucFullLatch = FALSE;
        ptrn->ucPatternType = 2;

        *dispatch = 2;

        return;
    }
    
    //Case: Second frame has the pattern, need to move buffer three frames up
    //Texture Analysis
    condition[0] = (pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[1]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[1]->frmProperties.interlaced;

    //SAD analysis
    condition[1] = pFrm[1]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[1]->plaY.ucStats.ucSAD[2]),(pFrm[1]->plaY.ucStats.ucSAD[3]));
    condition[2] = (pFrm[1]->plaY.ucStats.ucSAD[0] < pFrm[1]->plaY.ucStats.ucSAD[1] && pFrm[2]->plaY.ucStats.ucSAD[1] < pFrm[2]->plaY.ucStats.ucSAD[0]) ||
                   (pFrm[2]->plaY.ucStats.ucSAD[0] < pFrm[2]->plaY.ucStats.ucSAD[1] && pFrm[1]->plaY.ucStats.ucSAD[1] < pFrm[1]->plaY.ucStats.ucSAD[0]);
    if(condition[0] && condition[1] && condition[2])
    {
        pFrm[0]->frmProperties.candidate = TRUE;
        pFrm[1]->frmProperties.drop = FALSE;/*TRUE*/
        pFrm[1]->frmProperties.drop24fps = TRUE;
        pFrm[1]->frmProperties.candidate = TRUE;
        pFrm[2]->frmProperties.candidate = TRUE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE;
        ptrn->ucPatternType = 2;

        *dispatch = 3;
        return;
    }

    //Case: Third frame has the pattern, need to move buffer four frames up
    //Texture Analysis
    condition[0] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7]) &&
                   pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[2]->frmProperties.interlaced;
    //SAD analysis
    condition[1] = pFrm[2]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
    condition[2] = (pFrm[2]->plaY.ucStats.ucSAD[0] < pFrm[2]->plaY.ucStats.ucSAD[1] && pFrm[3]->plaY.ucStats.ucSAD[1] < pFrm[3]->plaY.ucStats.ucSAD[0]) ||
                   (pFrm[3]->plaY.ucStats.ucSAD[0] < pFrm[3]->plaY.ucStats.ucSAD[1] && pFrm[2]->plaY.ucStats.ucSAD[1] < pFrm[2]->plaY.ucStats.ucSAD[0]);
    if(condition[0] && condition[1] && condition[2])
    {
        pFrm[1]->frmProperties.candidate = TRUE;
        pFrm[2]->frmProperties.drop = FALSE; /*TRUE*/
        pFrm[2]->frmProperties.drop24fps = TRUE;
        pFrm[2]->frmProperties.candidate = TRUE;
        pFrm[3]->frmProperties.candidate = TRUE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE;
        ptrn->ucPatternType = 2;

        *dispatch = 4;
        return;
    }

    //Case: Fourth frame has the pattern, pattern is synchronized
    //Texture Analysis
    condition[0] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                   pFrm[3]->frmProperties.interlaced;
    //SAD analysis
    condition[1] = pFrm[3]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
    condition[2] = (pFrm[3]->plaY.ucStats.ucSAD[0] < pFrm[3]->plaY.ucStats.ucSAD[1] && pFrm[4]->plaY.ucStats.ucSAD[1] < pFrm[4]->plaY.ucStats.ucSAD[0]) ||
                   (pFrm[4]->plaY.ucStats.ucSAD[0] < pFrm[4]->plaY.ucStats.ucSAD[1] && pFrm[3]->plaY.ucStats.ucSAD[1] < pFrm[3]->plaY.ucStats.ucSAD[0]);
    if(condition[0] && condition[1] && condition[2])
    {
        pFrm[2]->frmProperties.candidate = TRUE;
        pFrm[3]->frmProperties.drop = TRUE;
        pFrm[4]->frmProperties.candidate = TRUE;
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = FALSE;
        ptrn->ucLatch.ucFullLatch = TRUE;
        ptrn->ucPatternType = 2;
        *dispatch = 5;
        return;
    }

    //Case: Fifth frame has the pattern, one frame needs to be moved out
    //Texture Analysis
    condition[0] = (pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[0]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                   pFrm[4]->plaY.ucStats.ucRs[7] > (pFrm[3]->plaY.ucStats.ucRs[7])) ||
                   pFrm[4]->frmProperties.interlaced;
    //SAD analysis
    condition[1] = pFrm[4]->plaY.ucStats.ucSAD[4] * T1 > min((pFrm[4]->plaY.ucStats.ucSAD[2]),(pFrm[4]->plaY.ucStats.ucSAD[3]));
    condition[2] = (pFrm[4]->plaY.ucStats.ucSAD[0] < pFrm[4]->plaY.ucStats.ucSAD[1] && pFrm[5]->plaY.ucStats.ucSAD[1] < pFrm[5]->plaY.ucStats.ucSAD[0]) ||
                   (pFrm[5]->plaY.ucStats.ucSAD[0] < pFrm[5]->plaY.ucStats.ucSAD[1] && pFrm[4]->plaY.ucStats.ucSAD[1] < pFrm[4]->plaY.ucStats.ucSAD[0]);
    if(condition[0] && condition[1] && condition[2])
    {
        ptrn->uiPreviousPartialPattern = ptrn->uiPartialPattern;
        ptrn->uiPartialPattern = TRUE;
        ptrn->ucLatch.ucFullLatch = TRUE;
        ptrn->ucPatternType = 2;
        *dispatch = 1;
        return;
    }
}

unsigned int Classifier(double dTextureLevel, double dDynDif, double dStatDif, double dStatCount, double dCountDif,
                                  double dZeroTexture, double dRsT, double dAngle, double dSADv, double dBigTexture,
                                  double dCount, double dRsG, double dRsDif, double dRsB, double SADCBPT, double SADCTPB)
{
    unsigned int ui_cls = 2;

    const double dCoeff[12][4] = { {     0.0,   0.997, -0.0512,   0.2375 },
                                   {  0.1665, -3.3511,  26.426,     0.95 },
                                   {     0.0,  0.0176,  0.6726,      0.0 },
                                   {   0.058,  -0.673,   2.671,  -2.8396 },
                                   { -2.1491,  51.764, -389.04,    943.0 },
                                   {     0.0,   5.122, -76.323,   286.58 },
                                   { -2.3272,  68.832, -649.82,  2003.68 },
                                   {     0.0,  1.0189,  -19.57,   118.84 },
                                   {     0.0,  23.314, -581.74,   3671.1 },
                                   {     0.0, -4.4652,  189.79, -1624.82 },
                                   {     0.0, -0.4282,  26.396,  -241.83 },
                                   {     0.0, -1.8092,  180.44, -3081.81 } };

    if (dAngle >= 999)
        return 0;
    else
    {
        if (dAngle > 0.73)
            return 1;
    }

    if (ui_cls == 2)
    {
        if (dTextureLevel <= -3.5 || dCountDif <= ((dTextureLevel * dTextureLevel * dCoeff[0][1]) + (dTextureLevel * dCoeff[0][2]) + dCoeff[0][3]))
            ui_cls = 2;
        else
            return 1;

        if (ui_cls == 2)
        {
            if (dDynDif <= ((dSADv * dSADv * dSADv * dCoeff[1][0]) + (dSADv * dSADv * dCoeff[1][1]) + (dSADv * dCoeff[1][2]) + dCoeff[1][3]) || dSADv > 7)
            {
                if (dSADv < 4.5 && dDynDif < (dCoeff[2][1] * exp(dSADv * dCoeff[2][2])))
                    return 0;
                else
                {
                    if (dSADv >= 4.5 && dSADv < 7 && dDynDif <= ((dSADv * dSADv * dSADv * dCoeff[3][0]) + (dSADv * dSADv * dCoeff[3][1]) + (dSADv * dCoeff[3][2]) + dCoeff[3][3]))
                        return 0;
                    else
                        ui_cls = 2;
                }
            }
            else
                return 1;

            if (ui_cls == 2)
            {
                if (dSADv >= 7 && dSADv < 9.6 && dDynDif >((dSADv * dSADv * dSADv * dCoeff[4][0]) + (dSADv * dSADv * dCoeff[4][1]) + (dSADv * dCoeff[4][2]) + dCoeff[4][3]))
                    return 1;
                else
                {
                    if (dSADv >= 7 && dSADv < 8.3 && dDynDif <= ((dSADv * dSADv * dSADv * dCoeff[3][0]) + (dSADv * dSADv * dCoeff[3][1]) + (dSADv * dCoeff[3][2]) + dCoeff[3][3]))
                        return 0;
                    else
                    {
                        if (dSADv >= 8.3 && dSADv < 9.6 && dDynDif <= ((dSADv * dSADv * dCoeff[5][1]) + (dSADv * dCoeff[5][2]) + dCoeff[5][3]))
                            return 0;
                        else
                            ui_cls = 2;
                    }
                }

                if (ui_cls == 2)
                {
                    if (dSADv >= 9.6 && dSADv < 12.055)
                    {
                        if (dDynDif >((dSADv * dSADv * dSADv * dCoeff[6][0]) + (dSADv * dSADv * dCoeff[6][1]) + (dSADv * dCoeff[6][2]) + dCoeff[6][3]))
                            return 1;
                        else
                        {
                            if (dDynDif < ((dSADv * dSADv * dCoeff[7][1]) + (dSADv * dCoeff[7][2]) + dCoeff[7][3]))
                                return 0;
                            else
                                ui_cls = 2;
                        }
                    }
                    else
                        ui_cls = 2;

                    if (ui_cls == 2)
                    {
                        if (dSADv >= 12.055 && dSADv < 15.5)
                        {
                            if (dDynDif > ((dSADv * dSADv * dCoeff[8][1]) + (dSADv * dCoeff[8][2]) + dCoeff[8][3]))
                                return 1;
                            else
                            {
                                if (dDynDif < ((dSADv * dSADv * dCoeff[7][1]) + (dSADv * dCoeff[7][2]) + dCoeff[7][3]))
                                    return 0;
                                else
                                    ui_cls = 2;
                            }
                        }
                        else
                            ui_cls = 2;

                        if (ui_cls == 2)
                        {
                            if (dSADv >= 15.5 && dSADv < 22.7)
                            {
                                if (dDynDif > ((dSADv * dSADv * dCoeff[9][1]) + (dSADv * dCoeff[9][2]) + dCoeff[9][3]))
                                    return 1;
                                else
                                {
                                    if (dDynDif < ((dSADv * dSADv * dCoeff[10][1]) + (dSADv * dCoeff[10][2]) + dCoeff[10][3]))
                                        return 0;
                                    else
                                        ui_cls = 2;
                                }

                            }
                            else
                                ui_cls = 2;

                            if (ui_cls == 2)
                            {
                                if (dSADv >= 22.7 && dDynDif > ((dSADv * dSADv * dCoeff[11][1]) + (dSADv * dCoeff[11][2]) + dCoeff[11][3]))
                                    return 1;
                                else
                                    ui_cls = 2;
                            }
                        }
                    }
                }
            }
        }
    }

    if (ui_cls == 2)
        ui_cls = classify(dTextureLevel, dDynDif, dStatDif, dStatCount, dCountDif, dZeroTexture, dRsT, dAngle, dSADv, dBigTexture, dCount, dRsG, dRsDif, dRsB, SADCBPT, SADCTPB);

    if (ui_cls > 1)
        return 0;
    else
        return ui_cls;
}

double EquationPw3Check(double ddataIn, unsigned int uiyCoeffMtxPos)
{
    const double dCoeff[2][4] =  { {  0.01637, -0.01124,  4.20553,  2.44986 },
                                   {        0,  0.06672,  1.46968,      0.5 } };

    return ((ddataIn * ddataIn * ddataIn * dCoeff[uiyCoeffMtxPos][0]) + (ddataIn * ddataIn * dCoeff[uiyCoeffMtxPos][1]) + (ddataIn * dCoeff[uiyCoeffMtxPos][2]) + dCoeff[uiyCoeffMtxPos][3]);
}

unsigned int Classifier_ML(double dTextureLevel, double dDynDif, double dStatDif, double dStatCount, double dCountDif,
    double dZeroTexture, double dRsT, double dAngle, double dSADv, double dBigTexture,
    double dCount, double dRsG, double dRsDif, double dRsB, double SADCBPT, double SADCTPB, unsigned int *ui_cls, unsigned int uiHeight)
{
    *ui_cls = 0;
    if(dAngle >= 1000.0)
        return 0;
    else if(dStatDif >= EquationPw3Check(dRsDif,0))
        return 1;
    else if((dStatDif >= EquationPw3Check(dRsDif,1)) && (uiHeight < 487) && (uiHeight > 479))
        return 1;
    else
    {
        *ui_cls += classify_f1(dTextureLevel, dRsG, dStatDif, dBigTexture, dSADv);
        *ui_cls += classify_f2(dRsG, dDynDif, dAngle, dSADv, dZeroTexture);
        *ui_cls += classify_f3(dTextureLevel, dRsG, dCountDif, dRsT, dCount);
        *ui_cls += classify_f5(dTextureLevel, dDynDif, dCountDif, dStatCount, dStatDif);
        *ui_cls += classify_f6(dTextureLevel, dRsG, dRsT, dStatDif, dCount);
        *ui_cls += classify_f7(dTextureLevel, dDynDif, dRsDif, dStatCount, dZeroTexture);
        *ui_cls += classify_f8(dDynDif, dAngle, SADCTPB, dRsB, dRsDif);
        *ui_cls += classify_f9(SADCBPT, dAngle, dRsT, SADCTPB, dCount);
        *ui_cls += classify_fA(dRsG, dStatDif, dBigTexture, dStatCount, dSADv);
        *ui_cls += classify_fB(dRsT, dCountDif, dStatDif, SADCTPB, dZeroTexture);
        *ui_cls += classify_fD(dTextureLevel, dSADv, dStatDif, dStatCount, dZeroTexture);
        *ui_cls += classify_fE(dRsT, dCountDif, SADCBPT, dAngle, dRsDif);
        *ui_cls += classify_fF(dRsT, dStatDif, SADCTPB, dRsB, dCount);
        *ui_cls += classify_f10(dRsG, dDynDif, dCountDif, dRsB, dStatDif);
        *ui_cls += classify_f11(dRsG, dDynDif, dAngle, dStatDif, dCount);
        *ui_cls += classify_f4(dTextureLevel, dStatDif, dZeroTexture, dStatCount, dSADv);
        *ui_cls += classify_fC(dRsG, dBigTexture, dAngle, SADCBPT, dZeroTexture);

        if (*ui_cls > 8)
            return 1;
        else
            return 0;
    }
}

void Artifacts_Detection_frame(Frame **pFrm, unsigned int frameNum)
{
    unsigned int uiIACount = 0, uiConStatus = 0;
    double dSADv, dSADt, dSADb, dCount, dStatCount, dCountDif, dAngle, dRsG, dRsT, dRsB, dRsDif, dDynDif, dStatDif, dZeroTexture, dBigTexture, dTextureLevel, SADCBPT, SADCTPB;

    dSADv = pFrm[frameNum]->plaY.ucStats.ucSAD[4];
    dSADt = pFrm[frameNum]->plaY.ucStats.ucSAD[0];
    dSADb = pFrm[frameNum]->plaY.ucStats.ucSAD[1];
    SADCBPT = pFrm[frameNum]->plaY.ucStats.ucSAD[3];
    SADCTPB = pFrm[frameNum]->plaY.ucStats.ucSAD[2];

    dRsDif = pFrm[frameNum]->plaY.ucStats.ucRs[3];
    dDynDif = pFrm[frameNum]->plaY.ucStats.ucRs[7] / pFrm[frameNum]->plaY.uiSize * 1000;
    dStatDif = pFrm[frameNum]->plaY.ucStats.ucRs[5] / pFrm[frameNum]->plaY.uiSize * 1000;
    dCountDif = dDynDif - dStatDif;
    dStatCount = pFrm[frameNum]->plaY.ucStats.ucRs[4];
    dCount = pFrm[frameNum]->plaY.ucStats.ucRs[6];
    dAngle = pFrm[frameNum]->plaY.ucStats.ucRs[8];
    dRsG = pFrm[frameNum]->plaY.ucStats.ucRs[0];
    dRsT = pFrm[frameNum]->plaY.ucStats.ucRs[1];
    dRsB = pFrm[frameNum]->plaY.ucStats.ucRs[2];
    dZeroTexture = pFrm[frameNum]->plaY.ucStats.ucRs[9];
    dBigTexture = pFrm[frameNum]->plaY.ucStats.ucRs[10];
    dTextureLevel = (dRsG - ((dRsT + dRsB) / 2));

    pFrm[frameNum]->frmProperties.interlaced = FALSE;
    pFrm[frameNum]->frmProperties.detection = FALSE;

    pFrm[frameNum]->frmProperties.interlaced = Classifier_ML(dTextureLevel, dDynDif, dStatDif, dStatCount, dCountDif, dZeroTexture, dRsT, dAngle, dSADv, dBigTexture, dCount, dRsG, dRsDif, dRsB, SADCBPT, SADCTPB, &pFrm[frameNum]->frmProperties.uiInterlacedDetectionValue, pFrm[frameNum]->plaY.uiHeight);
    pFrm[frameNum]->frmProperties.detection = pFrm[frameNum]->frmProperties.interlaced;
}

unsigned int Artifacts_Detection(Frame **pFrm)
{
    unsigned int ui, uiIACount = 0, uiConStatus = 0;

    for (ui = 1; ui < BUFMINSIZE; ui++)
    {
        Artifacts_Detection_frame(pFrm, ui);
        if (ui < BUFMINSIZE - 1)
            uiIACount += pFrm[ui]->frmProperties.interlaced;
        if (pFrm[ui]->frmProperties.interlaced)
            pFrm[ui]->frmProperties.candidate = TRUE;
    }
    return uiIACount;
}

void Init_drop_frames(Frame **pFrm)
{
    unsigned int i;

    for (i = 0; i < BUFMINSIZE; i++)
    {
        pFrm[i]->frmProperties.drop = FALSE;
        pFrm[i]->frmProperties.drop24fps = FALSE;
        pFrm[i]->frmProperties.candidate = FALSE;
    }
}

void Detect_Interlacing_Artifacts_fast(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch, unsigned int *uiisInterlaced)
{
    unsigned int i;

    ptrn->ucLatch.ucFullLatch = FALSE;
    ptrn->ucPatternType = 0;

    Init_drop_frames(pFrm);

    ptrn->ucAvgSAD = CalcSAD_Avg_NoSC(pFrm);

    if(ptrn->uiInterlacedFramesNum < 3)
        Detect_Solve_32RepeatedFramePattern(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch &&(ptrn->uiInterlacedFramesNum >= 4))
        Detect_Solve_41FramePattern(pFrm, ptrn, dispatch);

    if (!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum >= 3 || ptrn->ucPatternType == 5 || ptrn->ucPatternType == 6))
        Detect_Solve_32BlendedPatterns(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch)
        Detect_Solve_32Patterns(pFrm, ptrn, dispatch);
    
    if(!ptrn->ucLatch.ucFullLatch)
        Detect_Solve_3223Patterns(pFrm, ptrn, dispatch);

    for(i = 0; i < BUFMINSIZE; i++)
        pFrm[i]->frmProperties.processed = TRUE;
    
}
double CalcSAD_Avg_NoSC(Frame **pFrm)
{
    unsigned int i, sc = 0, control = 0;
    double max1 = 0.0, max2 = 0.0, avg = 0.0;

    for(i = 0; i < 5; i++)
    {
        max2 = max(max1, max2);
        max1 = max(max1, pFrm[i]->plaY.ucStats.ucSAD[0]);
        if(max1 != max2)
            control = i;
        //sc += pFrm[i]->frmProperties.sceneChange;
    }

    if(max1 - max2 > 20)
    {
        for(i = 0; i < control; i++)
            avg += pFrm[i]->plaY.ucStats.ucSAD[0];

        for(i = control + 1; i < 5; i++)
            avg += pFrm[i]->plaY.ucStats.ucSAD[0];

        avg /= 4;
    }
    else
    {
        for(i = control + 1; i < 5; i++)
            avg += pFrm[i]->plaY.ucStats.ucSAD[0];

        avg /= 5;
    }

    return avg;
}
void Detect_32Pattern_rigorous(Frame **pFrm, Pattern *ptrn, unsigned int *dispatch, unsigned int *uiisInterlaced)
{
    BOOL condition[10];
    int previousPattern = ptrn->ucPatternType;
    double previousAvgSAD = ptrn->ucAvgSAD;

    ptrn->ucLatch.ucFullLatch = FALSE;
    ptrn->ucAvgSAD = CalcSAD_Avg_NoSC(pFrm);

    Init_drop_frames(pFrm);

    condition[8] = ptrn->ucAvgSAD < previousAvgSAD * T2;

    if(ptrn->uiInterlacedFramesNum < 3)
        Detect_Solve_32RepeatedFramePattern(pFrm, ptrn, dispatch);

    if(!ptrn->ucLatch.ucFullLatch &&(ptrn->uiInterlacedFramesNum >= 4))
        Detect_Solve_41FramePattern(pFrm, ptrn, dispatch);

    if (!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum >= 3 || previousPattern == 5 || previousPattern == 6 || previousPattern == 7))
        Detect_Solve_32BlendedPatterns(pFrm, ptrn, dispatch);

    if((!ptrn->ucLatch.ucFullLatch && !(ptrn->uiInterlacedFramesNum >= 3)) || ((previousPattern == 1 || previousPattern == 2) && ptrn->ucLatch.ucSHalfLatch))
    {
        ptrn->ucLatch.ucSHalfLatch = FALSE;
        ptrn->ucPatternType = 0;
        //Texture Analysis  - To override previous pattern if needed
        condition[0] = pFrm[2]->plaY.ucStats.ucRs[0] > (pFrm[2]->plaY.ucStats.ucRs[1]) ||
                       pFrm[2]->plaY.ucStats.ucRs[0] > (pFrm[2]->plaY.ucStats.ucRs[2]) ||
                       pFrm[2]->frmProperties.interlaced;
        condition[1] = pFrm[3]->plaY.ucStats.ucRs[0] > (pFrm[3]->plaY.ucStats.ucRs[1]) ||
                       pFrm[3]->plaY.ucStats.ucRs[0] > (pFrm[3]->plaY.ucStats.ucRs[2]) ||
                       pFrm[3]->frmProperties.interlaced;


        if(previousPattern == 1 || (condition[0] && condition[1]))
        {
            //Assuming pattern is synchronized
            //Texture Analysis 2
            condition[2] = (pFrm[2]->plaY.ucStats.ucRs[7] > (pFrm[1]->plaY.ucStats.ucRs[7])) || pFrm[2]->frmProperties.interlaced;
            condition[3] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) || pFrm[3]->frmProperties.interlaced;

            condition[9] = (pFrm[0]->plaY.ucStats.ucRs[5] == 0) && (pFrm[1]->plaY.ucStats.ucRs[5] == 0) && (pFrm[4]->plaY.ucStats.ucRs[5] == 0);

            //SAD analysis
            condition[4] = pFrm[2]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
            condition[5] = pFrm[3]->plaY.ucStats.ucSAD[4] * T4 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));

            condition[6] = pFrm[2]->plaY.ucStats.ucSAD[4] * T4 > min((pFrm[2]->plaY.ucStats.ucSAD[2]),(pFrm[2]->plaY.ucStats.ucSAD[3]));
            condition[7] = pFrm[3]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));


            if((condition[0] && condition[1] && ptrn->uiInterlacedFramesNum > 0) || (condition[2] && condition[3]) || (condition[4] && condition[5]) || (condition[6] && condition[7]))
            {
                pFrm[3]->frmProperties.drop = TRUE;
                ptrn->ucLatch.ucFullLatch = TRUE;
                if(pFrm[3]->plaY.ucStats.ucSAD[2] > pFrm[3]->plaY.ucStats.ucSAD[3] && pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3] && pFrm[2]->plaY.ucStats.ucSAD[0] > pFrm[2]->plaY.ucStats.ucSAD[1] && pFrm[4]->plaY.ucStats.ucSAD[1] > pFrm[4]->plaY.ucStats.ucSAD[0])
                    ptrn->ucLatch.ucParity = 1;
                else
                    ptrn->ucLatch.ucParity = 0;
                ptrn->ucPatternType = 1;
                *dispatch = 5;
                Undo2Frames(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
                return;
            }
            if((condition[0] || condition[1] || condition[2] || condition[3] || (condition[4] || condition[5]) || condition[6] || condition[7] || condition[9]) && (ptrn->ucAvgSAD < 2.3))
            {
                pFrm[3]->frmProperties.drop = TRUE;
                ptrn->ucLatch.ucFullLatch = TRUE;
                if(pFrm[2]->plaY.ucStats.ucSAD[2] > pFrm[2]->plaY.ucStats.ucSAD[3])
                    ptrn->ucLatch.ucParity = 1;
                else
                    ptrn->ucLatch.ucParity = 0;
                if(condition[8] || condition[9])
                    ptrn->ucPatternType = 1;
                else
                    ptrn->ucPatternType = 0;
                *dispatch = 5;
                Undo2Frames(pFrm[2],pFrm[3],ptrn->ucLatch.ucParity);
                return;
            }
        }
        else if(previousPattern == 2)
        {
            //Assuming pattern is synchronized
            //Texture Analysis
            condition[0] = (pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[2]->plaY.ucStats.ucRs[7]) &&
                           pFrm[3]->plaY.ucStats.ucRs[7] > (pFrm[4]->plaY.ucStats.ucRs[7])) ||
                           pFrm[3]->frmProperties.interlaced;
            //SAD analysis
            condition[1] = pFrm[3]->plaY.ucStats.ucSAD[4] * T3 > min((pFrm[3]->plaY.ucStats.ucSAD[2]),(pFrm[3]->plaY.ucStats.ucSAD[3]));
            if(condition[0] || condition[1])
            {
                pFrm[2]->frmProperties.candidate = TRUE;
                pFrm[3]->frmProperties.drop = TRUE;
                pFrm[4]->frmProperties.candidate = TRUE;
                ptrn->ucLatch.ucFullLatch = TRUE;
                ptrn->ucPatternType = 2;
                *dispatch = 5;
                return;
            }
            else if(pFrm[3]->plaY.ucStats.ucRs[7] > 0)
            {
                pFrm[2]->frmProperties.candidate = TRUE;
                pFrm[3]->frmProperties.drop = TRUE;
                pFrm[4]->frmProperties.candidate = TRUE;
                ptrn->ucLatch.ucFullLatch = TRUE;
                ptrn->ucPatternType = 0;
                *dispatch = 5;
                return;
            }
        }
    }

    if(!ptrn->ucLatch.ucFullLatch && !(ptrn->uiInterlacedFramesNum > 2) && (ptrn->uiInterlacedFramesNum > 0))
    {
        Detect_Solve_32Patterns(pFrm, ptrn, dispatch);
        if(!ptrn->ucLatch.ucFullLatch)
            Detect_Solve_3223Patterns(pFrm, ptrn, dispatch);
    }
    if(!ptrn->ucLatch.ucFullLatch && (ptrn->uiInterlacedFramesNum == 4))
    {
        ptrn->ucLatch.ucFullLatch = TRUE;
        ptrn->ucPatternType = 3;
        *dispatch = 5;
    }
}
void UndoPatternTypes5and7(Frame *frmBuffer[LASTFRAME], unsigned int firstPos)
{
    unsigned int 
        start = firstPos;
    //Frame 1
    FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    DeinterlaceMedianFilter(frmBuffer, start, BUFMINSIZE, start < (firstPos + 2));

    //Frame 2
    start++;
    FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    DeinterlaceMedianFilter(frmBuffer, start, BUFMINSIZE, start < (firstPos + 2));

    //Frame 3
    start++;
    frmBuffer[start]->frmProperties.drop = TRUE;
    
    
    //Frame 4
    start++;
    FillBaseLinesIYUV(frmBuffer[start], frmBuffer[BUFMINSIZE], start < (firstPos + 2), start < (firstPos + 2));
    DeinterlaceMedianFilter(frmBuffer, start, BUFMINSIZE, start < (firstPos + 2));
}
void Undo2Frames(Frame *frmBuffer1, Frame *frmBuffer2, BOOL BFF)
{
    unsigned int 
        i,
        start;

    start = BFF;

    for(i = start; i < frmBuffer1->plaY.uiHeight; i += 2)
        ptir_memcpy(frmBuffer1->plaY.ucData + (i * frmBuffer1->plaY.uiStride), frmBuffer2->plaY.ucData + (i * frmBuffer2->plaY.uiStride),frmBuffer2->plaY.uiStride);
    for(i = start; i < frmBuffer1->plaU.uiHeight; i += 2)
        ptir_memcpy(frmBuffer1->plaU.ucData + (i * frmBuffer1->plaU.uiStride), frmBuffer2->plaU.ucData + (i * frmBuffer2->plaU.uiStride),frmBuffer2->plaU.uiStride);
    for(i = start; i < frmBuffer1->plaV.uiHeight; i += 2)
        ptir_memcpy(frmBuffer1->plaV.ucData + (i * frmBuffer1->plaV.uiStride), frmBuffer2->plaV.ucData + (i * frmBuffer2->plaV.uiStride),frmBuffer2->plaV.uiStride);

    frmBuffer1->frmProperties.candidate = TRUE;
    frmBuffer1->frmProperties.interlaced = FALSE;
}
void Analyze_Buffer_Stats(Frame *frmBuffer[LASTFRAME], Pattern *ptrn, unsigned int *pdispatch, unsigned int *uiisInterlaced)
{
    unsigned int uiDropCount = 0,
        i = 0,
        uiPrevInterlacedFramesNum = (*pdispatch == 1 ? 0:ptrn->uiInterlacedFramesNum);

    ptrn->uiInterlacedFramesNum = Artifacts_Detection(frmBuffer) + frmBuffer[0]->frmProperties.interlaced;
    
    if ((*uiisInterlaced != 1) && !(ptrn->uiInterlacedFramesNum > 4 && ptrn->ucPatternType < 1))
    {
        if(!frmBuffer[0]->frmProperties.processed)
            Detect_Interlacing_Artifacts_fast(frmBuffer, ptrn, pdispatch, uiisInterlaced);
        else
            Detect_32Pattern_rigorous(frmBuffer, ptrn, pdispatch, uiisInterlaced);
        if(!ptrn->ucLatch.ucFullLatch)
        {
            if(ptrn->uiInterlacedFramesNum < 2 && !ptrn->ucLatch.ucThalfLatch)
            {
                ptrn->ucLatch.ucFullLatch = TRUE;
                ptrn->ucPatternType = 0;
                *pdispatch = 5;
            }
            else
            {
                *pdispatch = 1;
                ptrn->uiOverrideHalfFrameRate = FALSE;
            }
        }
        if (*uiisInterlaced == 2)
        {
            if (*pdispatch >= BUFMINSIZE - 1)
            {
                for (i = 0; i < *pdispatch; i++)
                    uiDropCount += frmBuffer[i]->frmProperties.drop;

                if (!uiDropCount)
                    frmBuffer[BUFMINSIZE - 1]->frmProperties.drop = 1;
            }
        }
    }
    else
    {
        for (i = 0; i < BUFMINSIZE - 1; i++)
        {
            frmBuffer[i]->frmProperties.drop = 0;
            frmBuffer[i]->frmProperties.interlaced = 1;
        }

        *pdispatch = (BUFMINSIZE - 1);

        if(uiPrevInterlacedFramesNum == BUFMINSIZE - 1)
        {
            if(ptrn->uiInterlacedFramesNum + frmBuffer[BUFMINSIZE - 1]->frmProperties.interlaced == BUFMINSIZE)
            {
                ptrn->uiOverrideHalfFrameRate = TRUE;
                ptrn->uiCountFullyInterlacedBuffers = min(ptrn->uiCountFullyInterlacedBuffers + 1, 2);
            }
        }
        else
        {
            if(ptrn->uiInterlacedFramesNum + frmBuffer[BUFMINSIZE - 1]->frmProperties.interlaced == BUFMINSIZE)
                ptrn->uiOverrideHalfFrameRate = TRUE;
                //ptrn->uiCountFullyInterlacedBuffers = min(ptrn->uiCountFullyInterlacedBuffers + 1, 2);
            else
            {
                if(ptrn->uiCountFullyInterlacedBuffers > 0)
                    ptrn->uiCountFullyInterlacedBuffers--;
                else
                    ptrn->uiOverrideHalfFrameRate = FALSE;
            }
        }
    }

    if(!ptrn->ucLatch.ucFullLatch)
    {
        for (i = 0; i < *pdispatch; i++)
            ptrn->ulCountInterlacedDetections += frmBuffer[i]->frmProperties.interlaced;
    }
    else
        ptrn->ulCountInterlacedDetections += ptrn->uiInterlacedFramesNum;

    ptrn->ucPatternFound = ptrn->ucLatch.ucFullLatch;
}

void Analyze_Buffer_Stats_Automode(Frame *frmBuffer[LASTFRAME], Pattern *ptrn, unsigned int *pdispatch)
{
    unsigned int uiAutomode = AUTOMODE;

    Analyze_Buffer_Stats(frmBuffer, ptrn, pdispatch, &uiAutomode);

}

unsigned char TelecineParityCheck(Frame frmBuffer)
{
    if(frmBuffer.plaY.ucStats.ucSAD[2] > frmBuffer.plaY.ucStats.ucSAD[3])
        return 1;
    else
        return 0;
}

void Pattern32Removal(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity)
{
    if(uiInitFramePosition < BUFMINSIZE - 2)
    {
        frmBuffer[uiInitFramePosition + 1]->frmProperties.drop = TRUE;
        Undo2Frames(frmBuffer[uiInitFramePosition],frmBuffer[uiInitFramePosition + 1], parity);//TelecineParityCheck(*frmBuffer[uiInitFramePosition + 1]));
        *pdispatch = 5;
    }
    else
    {
        frmBuffer[uiInitFramePosition]->frmProperties.interlaced = TRUE;
        *pdispatch = 2;
    }
}

void Pattern41aRemoval(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity)
{
    unsigned int i = 0;
    if(uiInitFramePosition < BUFMINSIZE - 4)
    {
        frmBuffer[uiInitFramePosition + 3]->frmProperties.drop = TRUE;
        Undo2Frames(frmBuffer[uiInitFramePosition],frmBuffer[uiInitFramePosition + 1], parity);
        Undo2Frames(frmBuffer[uiInitFramePosition + 1],frmBuffer[uiInitFramePosition + 2], parity);
        Undo2Frames(frmBuffer[uiInitFramePosition + 2],frmBuffer[uiInitFramePosition + 3], parity);
        *pdispatch = 5;
    }
    else
    {
        for (i = 0; i < BUFMINSIZE - 1; i++)
            frmBuffer[uiInitFramePosition]->frmProperties.interlaced = FALSE;
        frmBuffer[uiInitFramePosition - 1]->frmProperties.interlaced = FALSE;
        *pdispatch = uiInitFramePosition;
    }
}

void Pattern2332Removal(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch)
{
    frmBuffer[uiInitFramePosition]->frmProperties.drop = TRUE;
    *pdispatch = 5;
}

void Pattern41Removal(Frame **frmBuffer, unsigned int uiInitFramePosition, unsigned int *pdispatch)
{
    const unsigned int fwr[5] = {4,3,2,1,0};
    int unsigned i;

    for(i = uiInitFramePosition + 1; i <= fwr[uiInitFramePosition]; i++)
        frmBuffer[i]->frmProperties.interlaced = TRUE;

    for(i = 0; i < uiInitFramePosition; i++)
        frmBuffer[i]->frmProperties.interlaced = TRUE;

    frmBuffer[uiInitFramePosition]->frmProperties.interlaced = FALSE;

    if(uiInitFramePosition == BUFMINSIZE - 2)
        frmBuffer[0]->frmProperties.drop = TRUE;
    else
        frmBuffer[uiInitFramePosition + 1]->frmProperties.drop = TRUE;

    *pdispatch = 5;

}

void RemovePattern(Frame **frmBuffer, unsigned int uiPatternNumber, unsigned int uiInitFramePosition, unsigned int *pdispatch, unsigned int parity)
{
    Init_drop_frames(frmBuffer);

    //Artifacts_Detection(frmBuffer);

    if(uiPatternNumber == 0)
        Pattern32Removal(frmBuffer, uiInitFramePosition, pdispatch, parity);
    else if(uiPatternNumber <= 2)
        Pattern2332Removal(frmBuffer, uiInitFramePosition, pdispatch);
    else if(uiPatternNumber == 3)
        Pattern41aRemoval(frmBuffer, uiInitFramePosition, pdispatch, parity);
    else
    {
        return;
    }
    Clean_Frame_Info(frmBuffer);
}
