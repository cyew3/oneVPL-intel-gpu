/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: deinterlacer.c

\* ****************************************************************************** */
#include "deinterlacer.h"
#include <assert.h>

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

ATTR_INLINE __inline unsigned int EDIError(BYTE* PrvLinePixel, BYTE* NxtLinePixel, int dir)
{
    return abs(PrvLinePixel[dir] - NxtLinePixel[-dir]);
}

#ifdef USE_SSE4

void FilterMask_Main(Plane *s, Plane *d, unsigned int BotBase, unsigned int ybegin, unsigned int yend)
{
    unsigned int x, y;
    unsigned int prevN, curN, spacer, lastCol, borderLen, borderStart;
    BYTE *tmpCur, *CurLine;

    __stosb(d->ucData, 0, d->uiSize);

    for (y = ybegin; y < yend; y += 2)
    {
        tmpCur      = s->ucCorner + s->uiStride * y;
        CurLine     = d->ucCorner + d->uiStride * y;

        prevN  = 0;
        curN   = 0;
        spacer = 0;

        lastCol = 128;

        for (x = 2; x < d->uiWidth - 2;)
        {
            __m128i mask;
            int bits;
            unsigned long count;
            
            //
            // process a run of empty pixels
            //
            mask = _mm_cmpeq_epi8(_mm_loadu_si128((__m128i *)&tmpCur[x]), _mm_set1_epi8(128));
            bits = _mm_movemask_epi8(mask);
#if defined(_WIN32) || defined(_WIN64)
            _BitScanForward(&count, ~bits);
#else
            int nbits = ~bits;
            count = __builtin_ctz(nbits);
#endif

            if (count > 0)
            {
                if (spacer <= 2 && curN > 0 && prevN > 0) //different colors is neighbours
                {
                    borderStart = (min(curN, prevN) * 3  + 2) >> 2;
                    borderLen   = borderStart * 2 + spacer;
                    borderStart += spacer + curN;
                    __stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask

                    prevN  = 0;
                    spacer = 0;
                }

                spacer += count;
                if (spacer >= 3)
                {
                    curN = 0;
                    prevN = 0;
                    lastCol = 128;
                }

                x += count;
            }
            //
            // process a run of non-empty pixels
            //
            else
            {
#if defined(_WIN32) || defined(_WIN64)
                _BitScanForward(&count, 0xffff0000 | bits);
#else
                int nbits = 0xffff0000 | bits;
                count = __builtin_ctz(nbits);
#endif
                for (count += x; x < count; x++)
                {
                    if (tmpCur[x] == lastCol)//same Color
                    {
                        curN++;
                    }
                    else
                    {
                        if (lastCol == 128)//new Line
                        {
                            prevN  = 0;
                            spacer = 0;
                        }
                        else              //color changing
                        {
                            if (prevN > 0) //second(or more) color change per line
                            { 
                                borderStart = (min(curN, prevN) * 3  + 2) >> 2;
                                borderLen   = borderStart * 2 + spacer;
                                borderStart += spacer + curN;
                                __stosb(&CurLine[x - borderStart], 255, borderLen);    //filing mask

                                spacer = 0;
                            }
                            prevN  = curN;
                        }
                        curN = 1;
                        lastCol = tmpCur[x];
                    }
                }
            }
        }
    }    
}

#else

void FilterMask_Main(Plane *s, Plane *d, unsigned int BotBase, unsigned int ybegin, unsigned int yend)
{
    unsigned int x, y, i;
    unsigned int prevN, curN, spacer, lastCol, borderLen, borderStart;
    BYTE *tmpCur, *CurLine;

    memset(d->ucData, 0, d->uiSize);

    for (y = ybegin; y < yend; y += 2)
    {
        tmpCur      = s->ucCorner + s->uiStride * y;
        CurLine     = d->ucCorner + d->uiStride * y;

        prevN  = 0;
        curN   = 0;
        spacer = 0;

        lastCol = 128;

        for (x = 2; x < d->uiWidth - 2; x ++)
        {
            if (tmpCur[x] == 128)//empty pixel found
            {
                if (spacer <= 2 && curN > 0 && prevN > 0) //different colors is neighbours
                {
                    borderStart = (min(curN, prevN) * 3  + 2) >> 2;
                    borderLen   = (borderStart << 1) + spacer;
                    borderStart += spacer + curN;

                    for (i = 0; i < borderLen; i++)//filing mask
                        CurLine[x - borderStart + i] = 255;

                    prevN  = 0;
                    spacer = 0;
                }

                spacer ++;
                if (spacer >= 3)
                {
                    prevN = curN = 0;
                    lastCol = 128;
                }
            }else
            {
                if (tmpCur[x] == lastCol)//same Color
                {
                    curN++;
                    if (tmpCur[x] == 128)
                        spacer = 0;
                }else
                {
                    if (lastCol == 128)//new Line
                    {
                        curN   = 1;
                        prevN  = 0;
                        spacer = 0;
                    }else              //color changing
                    {
                        if (prevN > 0) //second(or more) color change per line
                        { 
                            borderStart = (min(curN, prevN) * 3  + 2) >> 2;
                            borderLen   = borderStart * 2 + spacer;
                            borderStart += spacer + curN;

                            for (i = 0; i < borderLen; i++)//filing mask
                                CurLine[x - borderStart + i] = 255;

                            spacer = 0;
                        }
                        prevN  = curN;
                        curN   = 1;
                    }
                    lastCol = tmpCur[x];
                }
            }
        }
    }    
}

#endif

void FillBaseLinesIYUV(Frame *pSrc, Frame* pDst, int BottomLinesBaseY, int BottomLinesBaseUV)
{
    unsigned int i, val = 0;
    const unsigned char* pSrcLine;
    unsigned char* pDstLine;

    // copying Y-component
    for (i = 0; i < pSrc->plaY.uiHeight; i++)
    {
        pSrcLine = pSrc->plaY.ucCorner + i * pSrc->plaY.uiStride;
        pDstLine = pDst->plaY.ucCorner + i * pDst->plaY.uiStride;
        assert(pDstLine != 0 && pSrcLine != 0);
        ptir_memcpy(pDstLine, pSrcLine, pDst->plaY.uiWidth);
    }

    // copying U-component
    for (i = 0; i < pSrc->plaU.uiHeight; i++)
    {
        pSrcLine = pSrc->plaU.ucCorner + i * pSrc->plaU.uiStride;
        pDstLine = pDst->plaU.ucCorner + i * pDst->plaU.uiStride;
        assert(pDstLine != 0 && pSrcLine != 0);
        ptir_memcpy(pDstLine, pSrcLine, pDst->plaU.uiWidth);
    }
    // copying V-component
    for (i = 0; i < pSrc->plaV.uiHeight; i++)
    {
        pSrcLine = pSrc->plaV.ucCorner + i * pSrc->plaV.uiStride;
        pDstLine = pDst->plaV.ucCorner + i * pDst->plaV.uiStride;
        assert(pDstLine != 0 && pSrcLine != 0);
        ptir_memcpy(pDstLine, pSrcLine, pDst->plaV.uiWidth);
    }
}
void BilinearDeint(Frame *This, int BotBase)
{
    const int wminus4 = This->plaY.uiWidth - 4;
    const int hminus4 = This->plaY.uiHeight - 4;

    int x, y, xDiv2;

    BYTE* CurLine,* PrvLine,* NxtLine;
    BYTE* CurLine2,* PrvLine2,* NxtLine2;
    BYTE* CurLineU,* PrvLineU,* NxtLineU;
    BYTE* CurLineV,* PrvLineV,* NxtLineV;

    // loop inner area
    for (y = 4 + (BotBase?0:1); y < hminus4; y += 4)
    {
        int ColorOffset;

        PrvLine  = This->plaY.ucCorner + (y - 1) * This->plaY.uiStride;
        CurLine  = PrvLine  + This->plaY.uiStride;
        PrvLine2 = NxtLine  = CurLine + This->plaY.uiStride;
        CurLine2 = PrvLine2 + This->plaY.uiStride;
        NxtLine2 = CurLine2 + This->plaY.uiStride;

        ColorOffset = (((y << 1) + 2) >> 2) * This->plaU.uiStride;

        CurLineU = This->plaU.ucCorner + ColorOffset;
        PrvLineU = CurLineU - This->plaU.uiStride;
        NxtLineU = CurLineU + This->plaU.uiStride;

        CurLineV = This->plaV.ucCorner + ColorOffset;
        PrvLineV = CurLineV - This->plaV.uiStride;
        NxtLineV = CurLineV + This->plaV.uiStride;

        // two lines loop (one line color component)
        for (x = 4; x < wminus4; x += 2)
        {
            xDiv2 = x >> 1;

            // processing Y component first line
            CurLine [x    ] = (unsigned char)((PrvLine [x    ] + NxtLine [x    ] + 1) >> 1);

            CurLine [x + 1] = (unsigned char)((PrvLine [x + 1] + NxtLine [x + 1] + 1) >> 1);
            
            // processing Y component second line
            CurLine2[x    ] = (unsigned char)((PrvLine2[x    ] + NxtLine2[x    ] + 1) >> 1);

            CurLine2[x + 1] = (unsigned char)((PrvLine2[x + 1] + NxtLine2[x + 1] + 1) >> 1);
            
            // processing U-component
            CurLineU[xDiv2] = (unsigned char)((PrvLineU[xDiv2] + NxtLineU[xDiv2] + 1) >> 1);

            // processing V-component
            CurLineV[xDiv2] = (unsigned char)((PrvLineV[xDiv2] + NxtLineV[xDiv2] + 1) >> 1);
        }
    }
    if(This->frmProperties.processed == 0)
        This->frmProperties.processed = 1;
    This->frmProperties.drop = 0;
}

#ifdef USE_SSE4

// Stores 0..15 bytes of the xmm register to memory
static void XmmPartialStore(BYTE *ptr, __m128i xmm, int num)
{
    if (num & 8)
    {
        _mm_storel_epi64((__m128i *)ptr, xmm);
        xmm = _mm_srli_si128(xmm, 8);
        ptr += 8;
    }
    if (num & 4)
    {
        *(int *)ptr = _mm_cvtsi128_si32(xmm);
        xmm = _mm_srli_si128(xmm, 4);
        ptr += 4;
    }
    if (num & 2)
    {
        *(short *)ptr = _mm_extract_epi16(xmm, 0);
        xmm = _mm_srli_si128(xmm, 2);
        ptr += 2;
    }
    if (num & 1)
    {
        *ptr = (BYTE)_mm_extract_epi8(xmm, 0);
    }
}

// Replaces src1[0..width-1] with the median
void FilterMedian1x3(BYTE *src0, BYTE *src1, BYTE *src2, int width)
{
    __m128i x0, x1, x2, x3, x4;
    int i;

    // process 16 pixels at a time
    for (i = 0; i < width - 15; i += 16)
    {
        x0 = _mm_loadu_si128((__m128i const *)&src0[i]);
        x1 = _mm_loadu_si128((__m128i const *)&src1[i]);
        x2 = _mm_loadu_si128((__m128i const *)&src2[i]);
        x3 = _mm_min_epu8(x1, x2);
        x4 = _mm_max_epu8(x1, x2);
        x0 = _mm_max_epu8(x0, x3);
        x0 = _mm_min_epu8(x0, x4);
        _mm_storeu_si128((__m128i *)&src1[i], x0);
    }

    // remaining 1..15 pixels
    if (i < width)
    {
        x0 = _mm_loadu_si128((__m128i const *)&src0[i]);
        x1 = _mm_loadu_si128((__m128i const *)&src1[i]);
        x2 = _mm_loadu_si128((__m128i const *)&src2[i]);
        x3 = _mm_min_epu8(x1, x2);
        x4 = _mm_max_epu8(x1, x2);
        x0 = _mm_max_epu8(x0, x3);
        x0 = _mm_min_epu8(x0, x4);
        XmmPartialStore(&src1[i], x0, width - i);
    }
}

// Filter and clip pixels into dst[]
void FilterClip3x3(BYTE *src0, BYTE *src1, BYTE *src2, BYTE *dst, int width)
{
    __m128i x0, x1, x2;
    __m128i y0, y1, y2;
    int i;

    // process 16 pixels at a time
    for (i = 0; i < width - 15; i += 16)
    {
        x0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i-1])));
        y0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+7])));
        x0 = _mm_add_epi16(x0, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+0]))));
        y0 = _mm_add_epi16(y0, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+8]))));
        x0 = _mm_add_epi16(x0, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+1]))));
        y0 = _mm_add_epi16(y0, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+9]))));

        x1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i-1])));
        y1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+7])));
        x1 = _mm_add_epi16(x1, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+0]))));
        y1 = _mm_add_epi16(y1, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+8]))));
        x1 = _mm_add_epi16(x1, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+1]))));
        y1 = _mm_add_epi16(y1, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+9]))));

        x2 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i-1])));
        y2 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+7])));
        x2 = _mm_add_epi16(x2, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+0]))));
        y2 = _mm_add_epi16(y2, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+8]))));
        x2 = _mm_add_epi16(x2, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+1]))));
        y2 = _mm_add_epi16(y2, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+9]))));

        x1 = _mm_sub_epi16(_mm_add_epi16(x1, x1), _mm_add_epi16(x0, x2));    // 2 * x1 - (x0 + x2)
        y1 = _mm_sub_epi16(_mm_add_epi16(y1, y1), _mm_add_epi16(y0, y2));
        x1 = _mm_packs_epi16(x1, y1);

        x0 = _mm_set1_epi8(128);
        x0 = _mm_or_si128 (x0, _mm_cmpgt_epi8(x1, _mm_set1_epi8(+23)));        // if (x1 > +23) x0 = 255;
        x0 = _mm_and_si128(x0, _mm_cmpgt_epi8(x1, _mm_set1_epi8(-25)));        // if (x1 < -24) x0 = 0;
        _mm_storeu_si128((__m128i *)&dst[i], x0);
    }

    // remaining 1..15 pixels
    if (i < width)
    {
        x0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i-1])));
        y0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+7])));
        x0 = _mm_add_epi16(x0, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+0]))));
        y0 = _mm_add_epi16(y0, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+8]))));
        x0 = _mm_add_epi16(x0, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+1]))));
        y0 = _mm_add_epi16(y0, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src0[i+9]))));

        x1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i-1])));
        y1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+7])));
        x1 = _mm_add_epi16(x1, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+0]))));
        y1 = _mm_add_epi16(y1, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+8]))));
        x1 = _mm_add_epi16(x1, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+1]))));
        y1 = _mm_add_epi16(y1, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src1[i+9]))));

        x2 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i-1])));
        y2 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+7])));
        x2 = _mm_add_epi16(x2, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+0]))));
        y2 = _mm_add_epi16(y2, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+8]))));
        x2 = _mm_add_epi16(x2, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+1]))));
        y2 = _mm_add_epi16(y2, _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&src2[i+9]))));

        x1 = _mm_sub_epi16(_mm_add_epi16(x1, x1), _mm_add_epi16(x0, x2));
        y1 = _mm_sub_epi16(_mm_add_epi16(y1, y1), _mm_add_epi16(y0, y2));
        x1 = _mm_packs_epi16(x1, y1);

        x0 = _mm_set1_epi8(128);
        x0 = _mm_or_si128 (x0, _mm_cmpgt_epi8(x1, _mm_set1_epi8(+23)));
        x0 = _mm_and_si128(x0, _mm_cmpgt_epi8(x1, _mm_set1_epi8(-25)));
        XmmPartialStore(&dst[i], x0, width - i);
    }
}

#else   // C reference

void FilterMedian1x3(BYTE *src0, BYTE *src1, BYTE *src2, int width)
{
    int i, t0, t1, t2;
    int buf[3];

    for (i = 0; i < width; i++) {
        buf[0] = (int)src0[i];
        buf[1] = (int)src1[i];
        buf[2] = (int)src2[i];

        t0 = (buf[0] - buf[1]) >> 31;
        t1 = (buf[0] - buf[2]) >> 31;
        t2 = (buf[1] - buf[2]) >> 31;
        t0 ^= t1;
        t1 ^= t2;
        t0 = ~t0;
        t1 = 1 - t1;

        src1[i]= (BYTE)buf[t0 & t1];
    }
}

void FilterClip3x3(BYTE *src0, BYTE *src1, BYTE *src2, BYTE *dst, int width)
{
    int i, t0, t1, t2;

    for (i = 0; i < width; i++)
    {
        t0 = src0[i-1] + src0[i+0] + src0[i+1];
        t1 = src1[i-1] + src1[i+0] + src1[i+1];
        t2 = src2[i-1] + src2[i+0] + src2[i+1];

        t1 = 2 * t1 - (t0 + t2);

        t0 = 128;
        if (t1 > +23) t0 = 255;
        if (t1 < -24) t0 = 0;
        dst[i] = t0;
    }
}

#endif

void MedianDeinterlace(Frame *This, int BotBase)
{
    unsigned int y;
    BYTE *CurLine , *PrvLine , *NxtLine;
    BYTE *CurLine2, *PrvLine2, *NxtLine2;
    BYTE *CurLineU, *PrvLineU, *NxtLineU;
    BYTE *CurLineV, *PrvLineV, *NxtLineV;


    // loop inner area
    for (y = 4 + (BotBase?0:1); y < This->plaY.uiHeight - (3 * !BotBase); y += 4)
    {
        int ColorOffset;

        PrvLine  = This->plaY.ucCorner + (y - 1) * This->plaY.uiStride;
        CurLine  = PrvLine  + This->plaY.uiStride;
        PrvLine2 = NxtLine = CurLine + This->plaY.uiStride;
        CurLine2 = PrvLine2 + This->plaY.uiStride;
        NxtLine2 = CurLine2 + This->plaY.uiStride;

        ColorOffset = ((y+1)>>1) * This->plaU.uiStride;

        CurLineU = This->plaU.ucCorner + ColorOffset;
        PrvLineU = CurLineU - This->plaU.uiStride;
        NxtLineU = CurLineU + This->plaU.uiStride;

        CurLineV = This->plaV.ucCorner + ColorOffset;
        PrvLineV = CurLineV - This->plaV.uiStride;
        NxtLineV = CurLineV + This->plaV.uiStride;

        // process two lines of Y
        FilterMedian1x3(PrvLine, CurLine, NxtLine, This->plaY.uiWidth);
        FilterMedian1x3(PrvLine2, CurLine2, NxtLine2, This->plaY.uiWidth);

        // process U and V
        FilterMedian1x3(PrvLineU, CurLineU, NxtLineU, This->plaY.uiWidth/2);
        FilterMedian1x3(PrvLineV, CurLineV, NxtLineV, This->plaY.uiWidth/2);
    }

    if(This->frmProperties.processed == 0)
        This->frmProperties.processed = 1;
    This->frmProperties.drop = 0;
}

void BuildLowEdgeMask_Main(Frame **frmBuffer, unsigned int frame, unsigned int BotBase)
{
    const unsigned int wDiv2 = frmBuffer[frame]->plaY.uiWidth / 2;
    const unsigned int w2    = frmBuffer[frame]->plaY.uiWidth * 2;

    unsigned int y;
    BYTE *CurLine , *PrvLine , *NxtLine , *PrvPrvLine  , *NxtNxtLine;
    BYTE *CurLine2, *PrvLine2, *NxtLine2, *PrvPrvLine2 , *NxtNxtLine2;
    BYTE *CurLineU, *PrvLineU, *NxtLineU, *PrvPrvLineU , *NxtNxtLineU;
    BYTE *CurLineV, *PrvLineV, *NxtLineV, *PrvPrvLineV , *NxtNxtLineV;
    BYTE *tmpCur  , *tmpCur2,  *tmpCurU,  *tmpCurV;

    Plane *Ytemp = &frmBuffer[TEMPBUF]->plaY,
          *Utemp = &frmBuffer[TEMPBUF]->plaU,
          *Vtemp = &frmBuffer[TEMPBUF]->plaV,
          *Ybadmc = &frmBuffer[BADMCBUF]->plaY,
          *Ubadmc = &frmBuffer[BADMCBUF]->plaU,
          *Vbadmc = &frmBuffer[BADMCBUF]->plaV;

    // loop inner area
    for (y = (BotBase?8:5); y < frmBuffer[frame]->plaY.uiHeight - (BotBase?5:8); y += 4)
    {
        int ColorOffset;

        tmpCur      = frmBuffer[TEMPBUF]->plaY.ucCorner + frmBuffer[TEMPBUF]->plaY.uiStride * y;
        CurLine     = frmBuffer[frame]->plaY.ucCorner + frmBuffer[frame]->plaY.uiStride * y;
        PrvLine     = CurLine  - frmBuffer[frame]->plaY.uiStride;
        NxtLine     = CurLine  + frmBuffer[frame]->plaY.uiStride;
        PrvPrvLine  = PrvLine  - frmBuffer[frame]->plaY.uiStride;
        NxtNxtLine  = NxtLine  + frmBuffer[frame]->plaY.uiStride;

        tmpCur2     = tmpCur   + frmBuffer[TEMPBUF]->plaY.uiStride + frmBuffer[TEMPBUF]->plaY.uiStride;
        CurLine2    = CurLine  + frmBuffer[frame]->plaY.uiStride + frmBuffer[frame]->plaY.uiStride;
        PrvLine2    = CurLine2 - frmBuffer[frame]->plaY.uiStride;
        NxtLine2    = CurLine2 + frmBuffer[frame]->plaY.uiStride;
        PrvPrvLine2 = PrvLine2 - frmBuffer[frame]->plaY.uiStride;
        NxtNxtLine2 = NxtLine2 + frmBuffer[frame]->plaY.uiStride;

        ColorOffset = (((y << 1) + 2) >> 2) * wDiv2;

        tmpCurU     = frmBuffer[TEMPBUF]->plaU.ucCorner + ColorOffset;
        CurLineU    = frmBuffer[frame]->plaU.ucCorner + ColorOffset;
        PrvLineU    = CurLineU - frmBuffer[frame]->plaU.uiStride;
        NxtLineU    = CurLineU + frmBuffer[frame]->plaU.uiStride;
        PrvPrvLineU = PrvLineU - frmBuffer[frame]->plaU.uiStride;
        NxtNxtLineU = NxtLineU + frmBuffer[frame]->plaU.uiStride;

        tmpCurV     = frmBuffer[TEMPBUF]->plaV.ucCorner + ColorOffset;
        CurLineV    = frmBuffer[frame]->plaV.ucCorner + ColorOffset;
        PrvLineV    = CurLineV - frmBuffer[frame]->plaV.uiStride;
        NxtLineV    = CurLineV + frmBuffer[frame]->plaV.uiStride;
        PrvPrvLineV = PrvLineV - frmBuffer[frame]->plaV.uiStride;
        NxtNxtLineV = NxtLineV + frmBuffer[frame]->plaV.uiStride;

        // process two lines of Y
        FilterClip3x3(PrvPrvLine +4, CurLine +4, NxtNxtLine +4, tmpCur +4, frmBuffer[frame]->plaY.uiWidth - 8);
        FilterClip3x3(PrvPrvLine2+4, CurLine2+4, NxtNxtLine2+4, tmpCur2+4, frmBuffer[frame]->plaY.uiWidth - 8);

        // process U and V
        FilterClip3x3(PrvPrvLineU+2, CurLineU+2, NxtNxtLineU+2, tmpCurU+2, frmBuffer[frame]->plaY.uiWidth/2 - 4);
        FilterClip3x3(PrvPrvLineV+2, CurLineV+2, NxtNxtLineV+2, tmpCurV+2, frmBuffer[frame]->plaY.uiWidth/2 - 4);

        // fill borders
        *(int *)(tmpCur  + 0) = 0x80808080;
        *(int *)(tmpCur2 + 0) = 0x80808080;
        *(int *)(tmpCur  + frmBuffer[frame]->plaY.uiWidth - 4) = 0x80808080;
        *(int *)(tmpCur2 + frmBuffer[frame]->plaY.uiWidth - 4) = 0x80808080;

        *(short *)(tmpCurU + 0) = 0x8080;
        *(short *)(tmpCurV + 0) = 0x8080;
        *(short *)(tmpCurU + frmBuffer[frame]->plaY.uiWidth/2 - 2) = 0x8080;
        *(short *)(tmpCurV + frmBuffer[frame]->plaY.uiWidth/2 - 2) = 0x8080;
    }

    FilterMask_Main(Ytemp, Ybadmc, BotBase, (BotBase?8:5), frmBuffer[frame]->plaY.uiHeight - (BotBase?5:8));
    FilterMask_Main(Utemp, Ubadmc, BotBase, (BotBase?4:3), frmBuffer[frame]->plaU.uiHeight - (BotBase?3:4));
    FilterMask_Main(Vtemp, Vbadmc, BotBase, (BotBase?4:3), frmBuffer[frame]->plaV.uiHeight - (BotBase?3:4));
}

#ifdef USE_SSE4

ALIGN_DECL(16) static const unsigned char shufTabLumaEdges[18][16] = {
    { 0x00, 0xff, 0x01, 0xff, 0x02, 0xff, 0x03, 0xff, 0x04, 0xff, 0x05, 0xff, 0x06, 0xff, 0x07, 0xff }, 
    { 0x08, 0xff, 0x07, 0xff, 0x06, 0xff, 0x05, 0xff, 0x04, 0xff, 0x03, 0xff, 0x02, 0xff, 0x01, 0xff },

    { 0x01, 0xff, 0x02, 0xff, 0x03, 0xff, 0x04, 0xff, 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff }, 
    { 0x09, 0xff, 0x08, 0xff, 0x07, 0xff, 0x06, 0xff, 0x05, 0xff, 0x04, 0xff, 0x03, 0xff, 0x02, 0xff },

    { 0x02, 0xff, 0x03, 0xff, 0x04, 0xff, 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff }, 
    { 0x0a, 0xff, 0x09, 0xff, 0x08, 0xff, 0x07, 0xff, 0x06, 0xff, 0x05, 0xff, 0x04, 0xff, 0x03, 0xff },

    { 0x03, 0xff, 0x04, 0xff, 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff }, 
    { 0x0b, 0xff, 0x0a, 0xff, 0x09, 0xff, 0x08, 0xff, 0x07, 0xff, 0x06, 0xff, 0x05, 0xff, 0x04, 0xff },

    { 0x04, 0xff, 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff }, 
    { 0x0c, 0xff, 0x0b, 0xff, 0x0a, 0xff, 0x09, 0xff, 0x08, 0xff, 0x07, 0xff, 0x06, 0xff, 0x05, 0xff },

    { 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff }, 
    { 0x0d, 0xff, 0x0c, 0xff, 0x0b, 0xff, 0x0a, 0xff, 0x09, 0xff, 0x08, 0xff, 0x07, 0xff, 0x06, 0xff },

    { 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff }, 
    { 0x0e, 0xff, 0x0d, 0xff, 0x0c, 0xff, 0x0b, 0xff, 0x0a, 0xff, 0x09, 0xff, 0x08, 0xff, 0x07, 0xff },

    { 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff, 0x0e, 0xff }, 
    { 0x0f, 0xff, 0x0e, 0xff, 0x0d, 0xff, 0x0c, 0xff, 0x0b, 0xff, 0x0a, 0xff, 0x09, 0xff, 0x08, 0xff },

    { 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff, 0x0e, 0xff, 0x0f, 0xff }, 
    { 0x00, 0xff, 0x01, 0xff, 0x02, 0xff, 0x03, 0xff, 0x04, 0xff, 0x05, 0xff, 0x06, 0xff, 0x07, 0xff }, 
};

ALIGN_DECL(16) static const unsigned char shufTabChromaEdges[10][16] = {
    { 0x04, 0xff, 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff }, 
    { 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff, 0x0e, 0xff, 0x0f, 0xff }, 

    { 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff }, 
    { 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff, 0x0e, 0xff }, 

    { 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff }, 
    { 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff }, 

    { 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff, 0x0e, 0xff }, 
    { 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff }, 

    { 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff, 0x0c, 0xff, 0x0d, 0xff, 0x0e, 0xff, 0x0f, 0xff }, 
    { 0x04, 0xff, 0x05, 0xff, 0x06, 0xff, 0x07, 0xff, 0x08, 0xff, 0x09, 0xff, 0x0a, 0xff, 0x0b, 0xff }, 
};

static __inline void CalculateEdgesLuma_SSE4(unsigned char *PrvLine, unsigned char *NxtLine, char* CurEdgeDirLine, int xStart, int xEnd, int yStart, int yEnd, int strideLine, int strideEdge)
{
    int x, y;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    PrvLine -= 4;
    NxtLine -= 4;

    for (y = yStart; y < yEnd; y += 2) {
        for (x = xStart; x < xEnd; x += 8) {
            /* working set for 8 outputs = 8+9-1 = 16 pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(PrvLine + x));
            xmm1 = _mm_loadu_si128((__m128i *)(NxtLine + x));
            
            /* col 0, dirs -4 to +3 */
            xmm2 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[0]));
            xmm6 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[1]));
            xmm2 = _mm_sub_epi16(xmm2, xmm6);
            xmm2 = _mm_abs_epi16(xmm2);
            xmm2 = _mm_minpos_epu16(xmm2);

            /* col 1, dirs -4 to +3 */
            xmm3 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[2]));
            xmm6 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[3]));
            xmm3 = _mm_sub_epi16(xmm3, xmm6);
            xmm3 = _mm_abs_epi16(xmm3);
            xmm3 = _mm_minpos_epu16(xmm3);

            /* col 2, dirs -4 to +3 */
            xmm4 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[4]));
            xmm6 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[5]));
            xmm4 = _mm_sub_epi16(xmm4, xmm6);
            xmm4 = _mm_abs_epi16(xmm4);
            xmm4 = _mm_minpos_epu16(xmm4);

            /* col 3, dirs -4 to +3 */
            xmm5 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[6]));
            xmm6 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[7]));
            xmm5 = _mm_sub_epi16(xmm5, xmm6);
            xmm5 = _mm_abs_epi16(xmm5);
            xmm5 = _mm_minpos_epu16(xmm5);

            /* xmm6 = cols 0-3: bottom 4 words = errors, top 4 words = dirs */
            xmm2 = _mm_unpacklo_epi16(xmm2, xmm3);
            xmm4 = _mm_unpacklo_epi16(xmm4, xmm5);
            xmm6 = _mm_unpacklo_epi32(xmm2, xmm4);

            /* col 4, dirs -4 to +3 */
            xmm2 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[8]));
            xmm7 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[9]));
            xmm2 = _mm_sub_epi16(xmm2, xmm7);
            xmm2 = _mm_abs_epi16(xmm2);
            xmm2 = _mm_minpos_epu16(xmm2);

            /* col 5, dirs -4 to +3 */
            xmm3 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[10]));
            xmm7 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[11]));
            xmm3 = _mm_sub_epi16(xmm3, xmm7);
            xmm3 = _mm_abs_epi16(xmm3);
            xmm3 = _mm_minpos_epu16(xmm3);

            /* col 6, dirs -4 to +3 */
            xmm4 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[12]));
            xmm7 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[13]));
            xmm4 = _mm_sub_epi16(xmm4, xmm7);
            xmm4 = _mm_abs_epi16(xmm4);
            xmm4 = _mm_minpos_epu16(xmm4);

            /* col 7, dirs -4 to +3 */
            xmm5 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[14]));
            xmm7 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[15]));
            xmm5 = _mm_sub_epi16(xmm5, xmm7);
            xmm5 = _mm_abs_epi16(xmm5);
            xmm5 = _mm_minpos_epu16(xmm5);

            /* xmm7 = cols 4-7: bottom 4 words = errors, top 4 words = dirs */
            xmm2 = _mm_unpacklo_epi16(xmm2, xmm3);
            xmm4 = _mm_unpacklo_epi16(xmm4, xmm5);
            xmm7 = _mm_unpacklo_epi32(xmm2, xmm4);

            /* xmm2 = errors, xmm3 = dirs */
            xmm2 = _mm_unpacklo_epi64(xmm6, xmm7);
            xmm3 = _mm_unpackhi_epi64(xmm6, xmm7);

            /* vertical + blendvb for final dir (+4) */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabLumaEdges[16]));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabLumaEdges[17]));
            xmm0 = _mm_sub_epi16(xmm0, xmm1);
            xmm0 = _mm_abs_epi16(xmm0);

            /* set xmm2 to 1's mask if error of dir +4 is less than current minimum */
            xmm4 = _mm_set1_epi16(8);
            xmm2 = _mm_cmpgt_epi16(xmm2, xmm0);
            xmm3 = _mm_blendv_epi8(xmm3, xmm4, xmm2);

            /* map min dirs to [-4, +4], pack to bytes, store 8 dirs */
            xmm4 = _mm_set1_epi16(4);
            xmm3 = _mm_sub_epi16(xmm3, xmm4);
            xmm3 = _mm_packs_epi16(xmm3, xmm3);
            _mm_storel_epi64((__m128i *)(CurEdgeDirLine + x), xmm3);
        }
        PrvLine += 2*strideLine;
        NxtLine += 2*strideLine;
        CurEdgeDirLine += 2*strideEdge;
    }
}

static __inline void CalculateEdgesChroma_SSE4(unsigned char *PrvLine, unsigned char *NxtLine, char* CurEdgeDirLine, int xStart, int xEnd, int yStart, int yEnd, int strideLine, int strideEdge)
{
    int x, y;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm5, xmm6, xmm7;

    PrvLine += 2;
    NxtLine += 2;

    /* avoid warnings */
    xmm0 = _mm_setzero_si128();
    xmm1 = _mm_setzero_si128();

    for (y = yStart; y < yEnd; y += 4) {
        xmm0 = _mm_insert_epi32(xmm0, *(int *)(PrvLine + xStart - 4), 0x03);
        xmm1 = _mm_insert_epi32(xmm1, *(int *)(NxtLine + xStart - 4), 0x03);
        for (x = xStart; x < xEnd; x += 8) {
            /* working set for 8 outputs = 8+5-1 = 12 pixels - load first 4 at start then shift in new 8 every pass */
            xmm2 = xmm0;
            xmm3 = xmm1;
            xmm0 = _mm_loadl_epi64((__m128i *)(PrvLine + x));
            xmm1 = _mm_loadl_epi64((__m128i *)(NxtLine + x));
            xmm0 = _mm_alignr_epi8(xmm0, xmm2, 8);
            xmm1 = _mm_alignr_epi8(xmm1, xmm3, 8);

            /* dir -2 */
            xmm2 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabChromaEdges[0]));
            xmm3 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabChromaEdges[1]));
            xmm2 = _mm_sub_epi16(xmm2, xmm3);
            xmm6 = _mm_abs_epi16(xmm2);
            xmm7 = _mm_set1_epi16(-2);

            /* dir -1 */
            xmm2 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabChromaEdges[2]));
            xmm3 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabChromaEdges[3]));
            xmm2 = _mm_sub_epi16(xmm2, xmm3);
            xmm2 = _mm_abs_epi16(xmm2);
            xmm3 = _mm_set1_epi16(-1);
            xmm5 = _mm_cmpgt_epi16(xmm6, xmm2);
            xmm6 = _mm_min_epi16(xmm6, xmm2);
            xmm7 = _mm_blendv_epi8(xmm7, xmm3, xmm5);

            /* dir 0 */
            xmm2 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabChromaEdges[4]));
            xmm3 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabChromaEdges[5]));
            xmm2 = _mm_sub_epi16(xmm2, xmm3);
            xmm2 = _mm_abs_epi16(xmm2);
            xmm3 = _mm_set1_epi16(0);
            xmm5 = _mm_cmpgt_epi16(xmm6, xmm2);
            xmm6 = _mm_min_epi16(xmm6, xmm2);
            xmm7 = _mm_blendv_epi8(xmm7, xmm3, xmm5);

            /* dir +1 */
            xmm2 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabChromaEdges[6]));
            xmm3 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabChromaEdges[7]));
            xmm2 = _mm_sub_epi16(xmm2, xmm3);
            xmm2 = _mm_abs_epi16(xmm2);
            xmm3 = _mm_set1_epi16(+1);
            xmm5 = _mm_cmpgt_epi16(xmm6, xmm2);
            xmm6 = _mm_min_epi16(xmm6, xmm2);
            xmm7 = _mm_blendv_epi8(xmm7, xmm3, xmm5);

            /* dir +2 */
            xmm2 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabChromaEdges[8]));
            xmm3 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabChromaEdges[9]));
            xmm2 = _mm_sub_epi16(xmm2, xmm3);
            xmm2 = _mm_abs_epi16(xmm2);
            xmm3 = _mm_set1_epi16(+2);
            xmm5 = _mm_cmpgt_epi16(xmm6, xmm2);
            xmm7 = _mm_blendv_epi8(xmm7, xmm3, xmm5);

            /* pack to bytes, store 8 dirs */
            xmm7 = _mm_packs_epi16(xmm7, xmm7);
            _mm_storel_epi64((__m128i *)(CurEdgeDirLine + x), xmm7);
        }
        PrvLine += 2*strideLine;
        NxtLine += 2*strideLine;
        CurEdgeDirLine += 2*strideEdge;
    }
}

static __inline void CalculateEdgesLuma_C(unsigned char *PrvLine, unsigned char *NxtLine, char* CurEdgeDirLine, int xStart, int xEnd, int yStart, int yEnd, int strideLine, int strideEdge)
{
    int x, y;
    char EdgeDir, dir;
    unsigned int EdgeError, error;

    for (y = yStart; y < yEnd; y += 2) {
        for (x = xStart; x < xEnd; x++) {
            EdgeError = 4096;
            EdgeDir   = 0;
            for (dir = -4; dir < 5; dir++) {
                error = abs(PrvLine[x+dir] - NxtLine[x-dir]);
                if (error < EdgeError) {
                    EdgeError = error;
                    EdgeDir = dir;
                }
            }
            CurEdgeDirLine[x] = EdgeDir;
        }
        PrvLine += 2*strideLine;
        NxtLine += 2*strideLine;
        CurEdgeDirLine += 2*strideEdge;
    }
}

static __inline void CalculateEdgesChroma_C(unsigned char *PrvLine, unsigned char *NxtLine, char* CurEdgeDirLine, int xStart, int xEnd, int yStart, int yEnd, int strideLine, int strideEdge)
{
    int x, y;
    char EdgeDir, dir;
    unsigned int EdgeError, error;

    for (y = yStart; y < yEnd; y += 4) {
        for (x = xStart; x < xEnd; x++) {
            EdgeError = 4096;
            EdgeDir   = 0;
            for (dir = -2; dir < 3; dir++) {
                error = abs(PrvLine[x+dir] - NxtLine[x-dir]);
                if (error < EdgeError) {
                    EdgeError = error;
                    EdgeDir = dir;
                }
            }
            CurEdgeDirLine[x] = EdgeDir;
        }
        PrvLine += 2*strideLine;
        NxtLine += 2*strideLine;
        CurEdgeDirLine += 2*strideEdge;
    }
}

void CalculateEdgesIYUV(Frame **frmBuffer, unsigned int frame, int BotBase)
{
    int xStart, xEnd, xRem, yStart, yEnd, ColorOffset;
    unsigned char * PrvLine,* NxtLine;
    char* CurEdgeDirLine;

    if (BotBase) {
        yStart = 8;
        yEnd   = frmBuffer[frame]->plaY.uiHeight - 5;
    } else {
        yStart = 5;
        yEnd =   frmBuffer[frame]->plaY.uiHeight - 8;
    }

    /* optimized kernels process 8 at a time, use C for tails */
    xStart = 4;
    xEnd   = frmBuffer[frame]->plaY.uiWidth - 4;
    xRem   = (xEnd - xStart) & 0x07;
    xEnd  -= xRem;

    /* Y plane */
    PrvLine = frmBuffer[frame]->plaY.ucCorner + (yStart - 1) * frmBuffer[frame]->plaY.uiStride;
    NxtLine = frmBuffer[frame]->plaY.ucCorner + (yStart + 1) * frmBuffer[frame]->plaY.uiStride;
    CurEdgeDirLine = (char *)(frmBuffer[EDGESBUF]->plaY.ucCorner + yStart * frmBuffer[EDGESBUF]->plaY.uiStride);

    if (xEnd - xStart > 0)
        CalculateEdgesLuma_SSE4(PrvLine, NxtLine, CurEdgeDirLine, xStart, xEnd, yStart, yEnd, frmBuffer[frame]->plaY.uiStride, frmBuffer[EDGESBUF]->plaY.uiStride);

    if (xRem > 0)
        CalculateEdgesLuma_C(PrvLine, NxtLine, CurEdgeDirLine, xEnd, xEnd + xRem, yStart, yEnd, frmBuffer[frame]->plaY.uiStride, frmBuffer[EDGESBUF]->plaY.uiStride);

    /* chroma ranges */
    xStart = 2;
    xEnd   = frmBuffer[frame]->plaU.uiWidth - 2;
    xRem   = (xEnd - xStart) & 0x07;
    xEnd  -= xRem;

    ColorOffset = (((yStart << 1) + 2) >> 2);

    /* U plane */
    PrvLine = frmBuffer[frame]->plaU.ucCorner + (ColorOffset - 1) * frmBuffer[frame]->plaU.uiStride;
    NxtLine = frmBuffer[frame]->plaU.ucCorner + (ColorOffset + 1) * frmBuffer[frame]->plaU.uiStride;
    CurEdgeDirLine = (char *)(frmBuffer[EDGESBUF]->plaU.ucCorner + ColorOffset * frmBuffer[frame]->plaU.uiStride);
    
    if (xEnd - xStart > 0)
        CalculateEdgesChroma_SSE4(PrvLine, NxtLine, CurEdgeDirLine, xStart, xEnd, yStart, yEnd, frmBuffer[frame]->plaU.uiStride, frmBuffer[EDGESBUF]->plaU.uiStride);

    if (xRem > 0)
        CalculateEdgesChroma_C(PrvLine, NxtLine, CurEdgeDirLine, xEnd, xEnd + xRem, yStart, yEnd, frmBuffer[frame]->plaU.uiStride, frmBuffer[EDGESBUF]->plaU.uiStride);

    /* V plane */
    PrvLine = frmBuffer[frame]->plaV.ucCorner + (ColorOffset - 1) * frmBuffer[frame]->plaV.uiStride;
    NxtLine = frmBuffer[frame]->plaV.ucCorner + (ColorOffset + 1) * frmBuffer[frame]->plaV.uiStride;
    CurEdgeDirLine = (char *)(frmBuffer[EDGESBUF]->plaV.ucCorner + ColorOffset * frmBuffer[frame]->plaV.uiStride);

    if (xEnd - xStart > 0)
        CalculateEdgesChroma_SSE4(PrvLine, NxtLine, CurEdgeDirLine, xStart, xEnd, yStart, yEnd, frmBuffer[frame]->plaV.uiStride, frmBuffer[EDGESBUF]->plaV.uiStride);

    if (xRem > 0)
        CalculateEdgesChroma_C(PrvLine, NxtLine, CurEdgeDirLine, xEnd, xEnd + xRem, yStart, yEnd, frmBuffer[frame]->plaV.uiStride, frmBuffer[EDGESBUF]->plaV.uiStride);
}

#else    /* USE_SSE4 */

void CalculateEdgesIYUV(Frame **frmBuffer, unsigned int frame, int BotBase)
{
    unsigned int x, y, xover2;
    char EdgeDir, dir;
    unsigned int EdgeError;
    unsigned int error;

    BYTE* CurLine,* PrvLine,* NxtLine;
    BYTE* CurLine2,* PrvLine2,* NxtLine2;
    BYTE* CurLineU,* PrvLineU,* NxtLineU;
    BYTE* CurLineV,* PrvLineV,* NxtLineV;

    char* CurEdgeDirLine,* CurEdgeDirLine2,* CurEdgeDirLineU,* CurEdgeDirLineV;

    for (y = (BotBase?8:5); y < frmBuffer[frame]->plaY.uiHeight - (BotBase?5:8); y += 4)
    {
        int ColorOffset;

        PrvLine = frmBuffer[frame]->plaY.ucCorner + (y - 1) * frmBuffer[frame]->plaY.uiStride;
        CurLine = PrvLine + frmBuffer[frame]->plaY.uiStride;
        PrvLine2 = NxtLine = CurLine + frmBuffer[frame]->plaY.uiStride;
        CurLine2 = PrvLine2 + frmBuffer[frame]->plaY.uiStride;
        NxtLine2 = CurLine2 + frmBuffer[frame]->plaY.uiStride;

        ColorOffset = (((y << 1) + 2) >> 2) * frmBuffer[frame]->plaU.uiStride;  // this is correct, it was calculated on the paper :)

        CurLineU = frmBuffer[frame]->plaU.ucCorner + ColorOffset;
        PrvLineU = CurLineU - frmBuffer[frame]->plaU.uiStride;
        NxtLineU = CurLineU + frmBuffer[frame]->plaU.uiStride;

        CurLineV = frmBuffer[frame]->plaV.ucCorner + ColorOffset;
        PrvLineV = CurLineV - frmBuffer[frame]->plaV.uiStride;
        NxtLineV = CurLineV + frmBuffer[frame]->plaV.uiStride;

        CurEdgeDirLine = frmBuffer[EDGESBUF]->plaY.ucCorner + y * frmBuffer[EDGESBUF]->plaY.uiStride;
        CurEdgeDirLine2 = CurEdgeDirLine + frmBuffer[EDGESBUF]->plaY.uiStride + frmBuffer[EDGESBUF]->plaY.uiStride;
        CurEdgeDirLineU = frmBuffer[EDGESBUF]->plaU.ucCorner + ColorOffset;
        CurEdgeDirLineV = frmBuffer[EDGESBUF]->plaV.ucCorner + ColorOffset;

        for (x = 4; x < frmBuffer[frame]->plaY.uiWidth - 4; x += 2)
        {
            xover2 = x >> 1;
            // calculating first Y line edge errors
            EdgeError = 4096;
            EdgeDir   = 0;
            for (dir =- 4; dir < 5; dir++)
            {
                error = EDIError(PrvLine + x, NxtLine + x, dir);
                if (error < EdgeError)
                {
                    EdgeError = error;
                    EdgeDir = dir;
                }
            }
            CurEdgeDirLine[x] = EdgeDir;

            EdgeError = 4096;
            EdgeDir   = 0;
            for (dir =- 4; dir < 5; dir++)
            {
                error = EDIError(PrvLine + x + 1, NxtLine + x + 1, dir);
                if (error < EdgeError)
                {
                    EdgeError = error;
                    EdgeDir = dir;
                }
            }
            CurEdgeDirLine[x + 1] = EdgeDir;
        
            // calculating second Y line edge errors
            EdgeError = 4096;
            EdgeDir   = 0;
            for (dir =- 4; dir < 5; dir++)
            {
                error = EDIError(PrvLine2 + x, NxtLine2 + x, dir);
                if (error < EdgeError)
                {
                    EdgeError = error;
                    EdgeDir = dir;
                }
            }
            CurEdgeDirLine2[x] = EdgeDir;

            EdgeError = 4096;
            EdgeDir   = 0;
            for (dir =- 4; dir < 5; dir++)
            {
                error = EDIError(PrvLine2 + x + 1, NxtLine2 + x + 1, dir);
                if (error < EdgeError)
                {
                    EdgeError = error;
                    EdgeDir = dir;
                }
            }
            CurEdgeDirLine2[x + 1] = EdgeDir;

            // calculating U line edge errors
            EdgeError = 4096;
            EdgeDir   = 0;
            for (dir =- 2; dir < 3; dir++)
            {
                error = EDIError(PrvLineU + xover2, NxtLineU + xover2, dir);
                if (error < EdgeError)
                {
                    EdgeError = error;
                    EdgeDir = dir;
                }
            }
            CurEdgeDirLineU[xover2] = EdgeDir;

            // calculating V line edge errors
            EdgeError = 4096;
            EdgeDir   = 0;
            for (dir =- 2; dir < 3; dir++)
            {
                assert(xover2 + frmBuffer[frame]->plaV.uiStride + ColorOffset + dir <= 518400);
                error = EDIError(PrvLineV + xover2, NxtLineV + xover2, dir);
                if (error < EdgeError)
                {
                    EdgeError = error;
                    EdgeDir = dir;
                }
            }
            CurEdgeDirLineV[xover2] = EdgeDir;
        }
    }
}

#endif    /* USE_SSE4 */

void EdgeDirectionalIYUV_Main(Frame **frmBuffer, unsigned int curFrame, int BotBase)
{
    unsigned int x, y, xover2;//, dir;
    int EdgeDir;
    BYTE* CurLine,* PrvLine,* NxtLine,* RefLine;
    BYTE* CurLine2,* PrvLine2,* NxtLine2,* RefLine2;
    BYTE* CurLineU,* PrvLineU,* NxtLineU,* RefLineU;
    BYTE* CurLineV,* PrvLineV,* NxtLineV,* RefLineV;

    char* CurEdgeDirLine,* CurEdgeDirLine2,* CurEdgeDirLineU,* CurEdgeDirLineV;
    BYTE* CurBadMCLine,* CurBadMCLine2,* CurBadMCLineU,* CurBadMCLineV;
    int curW, prevW;

    BYTE tmpCurVal;


    //BuildStaticAreasMask(This);

    // loop inner area
    for (y = (BotBase?8:5); y < frmBuffer[curFrame]->plaY.uiHeight - (BotBase?5:8); y += 4)
    {
        int ColorOffset;

        PrvLine  = frmBuffer[curFrame]->plaY.ucCorner + (y - 1) * frmBuffer[curFrame]->plaY.uiStride;
        CurLine  = PrvLine + frmBuffer[curFrame]->plaY.uiStride;
        PrvLine2 = NxtLine = CurLine + frmBuffer[curFrame]->plaY.uiStride;
        CurLine2 = PrvLine2 + frmBuffer[curFrame]->plaY.uiStride;
        NxtLine2 = CurLine2 + frmBuffer[curFrame]->plaY.uiStride;
        RefLine  = frmBuffer[PREVBUF]->plaY.ucCorner + y * frmBuffer[PREVBUF]->plaY.uiStride;
        RefLine2 = RefLine + frmBuffer[PREVBUF]->plaY.uiStride + frmBuffer[PREVBUF]->plaY.uiStride;

        ColorOffset = (((y << 1) + 2) >> 2) * frmBuffer[curFrame]->plaU.uiStride;  // this is correct, it was calculated on the paper :)

        CurLineU = frmBuffer[curFrame]->plaU.ucCorner + ColorOffset;
        PrvLineU = CurLineU - frmBuffer[curFrame]->plaU.uiStride;
        NxtLineU = CurLineU + frmBuffer[curFrame]->plaU.uiStride;
        RefLineU = frmBuffer[PREVBUF]->plaU.ucCorner + ColorOffset;

        CurLineV = frmBuffer[curFrame]->plaV.ucCorner + ColorOffset;
        PrvLineV = CurLineV - frmBuffer[curFrame]->plaV.uiStride;
        NxtLineV = CurLineV + frmBuffer[curFrame]->plaV.uiStride;
        RefLineV = frmBuffer[PREVBUF]->plaV.ucCorner + ColorOffset;

        CurEdgeDirLine = (char *)(frmBuffer[EDGESBUF]->plaY.ucCorner + y * frmBuffer[EDGESBUF]->plaY.uiStride);
        CurEdgeDirLine2 = CurEdgeDirLine  + frmBuffer[EDGESBUF]->plaY.uiStride + frmBuffer[EDGESBUF]->plaY.uiStride;
        CurEdgeDirLineU = (char *)(frmBuffer[EDGESBUF]->plaU.ucCorner + ColorOffset);
        CurEdgeDirLineV = (char *)(frmBuffer[EDGESBUF]->plaV.ucCorner + ColorOffset);

        CurBadMCLine = frmBuffer[BADMCBUF]->plaY.ucCorner + y * frmBuffer[BADMCBUF]->plaY.uiStride;
        CurBadMCLine2 = CurBadMCLine + frmBuffer[BADMCBUF]->plaY.uiStride + frmBuffer[BADMCBUF]->plaY.uiStride;
        CurBadMCLineU = frmBuffer[BADMCBUF]->plaU.ucCorner + ColorOffset;
        CurBadMCLineV = frmBuffer[BADMCBUF]->plaV.ucCorner + ColorOffset;

        // two lines loop (one line color component)
        for (x = 4; x < frmBuffer[curFrame]->plaY.uiWidth - 4; x += 2)
        {
            xover2 = x>>1;

            prevW = 0;
            curW  = 255;

            // processing Y component first line
#define YPROT 15
#define UVPROT 10

            if (CurBadMCLine[x])
            {
                EdgeDir = CurEdgeDirLine[x];

                tmpCurVal = (BYTE)((PrvLine[x+EdgeDir] + NxtLine[x-EdgeDir] + 1) >> 1);
                    
                if ((tmpCurVal - YPROT > PrvLine[x] && tmpCurVal - YPROT > NxtLine[x]) ||
                    (tmpCurVal + YPROT < PrvLine[x] && tmpCurVal + YPROT < NxtLine[x]))
                    tmpCurVal = (BYTE)((PrvLine[x] + NxtLine[x]) >> 1);

                CurLine [x    ] = (CurLine [x    ] * prevW + tmpCurVal * curW) / 255;
            }
            if (CurBadMCLine[x+1])
            {
                EdgeDir = CurEdgeDirLine[x+1];
                    
                tmpCurVal = (BYTE)((PrvLine[x+1+EdgeDir] + NxtLine[x+1-EdgeDir]+1)>>1);
                    
                if ((tmpCurVal - YPROT > PrvLine[x+1] && tmpCurVal - YPROT > NxtLine[x+1]) ||
                    (tmpCurVal + YPROT < PrvLine[x+1] && tmpCurVal + YPROT < NxtLine[x+1]))
                    tmpCurVal = (BYTE)((PrvLine[x+1] + NxtLine[x+1]) >> 1);

                CurLine [x + 1] = (CurLine [x + 1] * prevW + tmpCurVal * curW) / 255;
            }
                
            // processing Y component second line
            if (CurBadMCLine2[x])
            {
                EdgeDir = CurEdgeDirLine2[x];
            
                tmpCurVal = (BYTE)((PrvLine2[x+EdgeDir] + NxtLine2[x-EdgeDir]+1)>>1);
                    
                if ((tmpCurVal - YPROT > PrvLine2[x] && tmpCurVal - YPROT > NxtLine2[x]) ||
                    (tmpCurVal + YPROT < PrvLine2[x] && tmpCurVal + YPROT < NxtLine2[x]))
                    tmpCurVal = (BYTE)((PrvLine2[x] + NxtLine2[x]) >> 1);

                CurLine2[x    ] = (CurLine2[x    ] * prevW + tmpCurVal * curW) / 255;
            }
            if (CurBadMCLine2[x+1])
            {
                EdgeDir = CurEdgeDirLine2[x+1];

                tmpCurVal = (BYTE)((PrvLine2[x+1+EdgeDir] + NxtLine2[x+1-EdgeDir]+1)>>1);

                if ((tmpCurVal - YPROT > PrvLine2[x+1] && tmpCurVal - YPROT > NxtLine2[x+1]) ||
                    (tmpCurVal + YPROT < PrvLine2[x+1] && tmpCurVal + YPROT < NxtLine2[x+1]))
                    tmpCurVal = (BYTE)((PrvLine2[x+1] + NxtLine2[x+1]) >> 1);

                CurLine2[x+1] = (CurLine2[x + 1] * prevW + tmpCurVal * curW) / 255;
            }

            // processing U-component
            if (CurBadMCLineU[xover2])
            {
                EdgeDir = CurEdgeDirLineU[xover2];

                tmpCurVal = (BYTE)((PrvLineU[xover2+EdgeDir] + NxtLineU[xover2-EdgeDir]+1)>>1);

                if ((tmpCurVal - YPROT > PrvLineU[xover2] && tmpCurVal - YPROT > NxtLineU[xover2]) ||
                    (tmpCurVal + YPROT < PrvLineU[xover2] && tmpCurVal + YPROT < NxtLineU[xover2]))
                    tmpCurVal = (BYTE)((PrvLineU[xover2] + NxtLineU[xover2]) >> 1);

                CurLineU[xover2] = (CurLineU[xover2] * prevW + tmpCurVal * curW) / 255;
            }

            // processing V-component
            if (CurBadMCLineV[xover2])
            {
                EdgeDir = CurEdgeDirLineV[xover2];

                tmpCurVal = (BYTE)((PrvLineV[xover2+EdgeDir] + NxtLineV[xover2-EdgeDir]+1)>>1);

                if ((tmpCurVal - YPROT > PrvLineV[xover2] && tmpCurVal - YPROT > NxtLineV[xover2]) ||
                    (tmpCurVal + YPROT < PrvLineV[xover2] && tmpCurVal + YPROT < NxtLineV[xover2]))
                    tmpCurVal = (BYTE)((PrvLineV[xover2] + NxtLineV[xover2]) >> 1);

                CurLineV[xover2] = (CurLineV[xover2] * prevW + tmpCurVal * curW) / 255;
            }
        }
    }
}
void DeinterlaceBorders(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase)
{
    BYTE *curLine;
    BYTE *prevLine;
    BYTE *nextLine;

    unsigned int x, y;
    unsigned int val = 0;

    if (BotBase)
    {
//BottomBase, non MC; upper and bottom lines processing
//Y processing
        ptir_memcpy(frmBuffer[curFrame]->plaY.ucCorner, frmBuffer[curFrame]->plaY.ucCorner + frmBuffer[curFrame]->plaY.uiStride, frmBuffer[curFrame]->plaY.uiWidth);
        
        curLine  = frmBuffer[curFrame]->plaY.ucCorner + frmBuffer[curFrame]->plaY.uiStride + frmBuffer[curFrame]->plaY.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaY.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaY.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaY.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;

        curLine  = frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiHeight * frmBuffer[curFrame]->plaY.uiStride) - frmBuffer[curFrame]->plaY.uiStride - frmBuffer[curFrame]->plaY.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaY.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaY.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaY.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;

        curLine  = frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiHeight * frmBuffer[curFrame]->plaY.uiStride) - frmBuffer[curFrame]->plaY.uiStride - frmBuffer[curFrame]->plaY.uiStride - frmBuffer[curFrame]->plaY.uiStride - frmBuffer[curFrame]->plaY.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaY.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaY.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaY.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;

        //memcpy(frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiStride * (frmBuffer[curFrame]->plaY.uiHeight - 4)), frmBuffer[refFrame]->plaY.ucCorner + (frmBuffer[refFrame]->plaY.uiStride * (frmBuffer[refFrame]->plaY.uiHeight - 4)), frmBuffer[curFrame]->plaY.uiWidth);
        //memcpy(frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiStride * (frmBuffer[curFrame]->plaY.uiHeight - 3)), frmBuffer[refFrame]->plaY.ucCorner + (frmBuffer[refFrame]->plaY.uiStride * (frmBuffer[refFrame]->plaY.uiHeight - 3)), frmBuffer[curFrame]->plaY.uiWidth);
        //memcpy(frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiStride * (frmBuffer[curFrame]->plaY.uiHeight - 2)), frmBuffer[refFrame]->plaY.ucCorner + (frmBuffer[refFrame]->plaY.uiStride * (frmBuffer[refFrame]->plaY.uiHeight - 2)), frmBuffer[curFrame]->plaY.uiWidth);
        //memcpy(frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiStride * (frmBuffer[curFrame]->plaY.uiHeight - 1)), frmBuffer[0]->plaY.ucCorner + (frmBuffer[0]->plaY.uiStride * (frmBuffer[0]->plaY.uiHeight - 1)), frmBuffer[curFrame]->plaY.uiWidth);


//U processing
        ptir_memcpy(frmBuffer[curFrame]->plaU.ucCorner, frmBuffer[curFrame]->plaU.ucCorner + frmBuffer[curFrame]->plaU.uiStride, frmBuffer[curFrame]->plaU.uiWidth);

        curLine  = frmBuffer[curFrame]->plaU.ucCorner + (frmBuffer[curFrame]->plaU.uiHeight * frmBuffer[curFrame]->plaU.uiStride) - frmBuffer[curFrame]->plaU.uiStride - frmBuffer[curFrame]->plaU.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaU.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaU.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaU.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;

//V processing
        ptir_memcpy(frmBuffer[curFrame]->plaV.ucCorner, frmBuffer[curFrame]->plaV.ucCorner + frmBuffer[curFrame]->plaV.uiStride, frmBuffer[curFrame]->plaV.uiWidth);
            
        curLine  = frmBuffer[curFrame]->plaV.ucCorner + (frmBuffer[curFrame]->plaV.uiHeight * frmBuffer[curFrame]->plaV.uiStride) - frmBuffer[curFrame]->plaV.uiStride - frmBuffer[curFrame]->plaV.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaV.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaV.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaV.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;
    }
    else
    {
//UpperBase, non MC; upper and bottom lines processing
//Y processing
        ptir_memcpy(frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiStride * frmBuffer[curFrame]->plaY.uiHeight) - frmBuffer[curFrame]->plaY.uiStride, frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiStride * (frmBuffer[curFrame]->plaY.uiHeight - 2)) /*- frmBuffer[curFrame]->plaY.uiStride - frmBuffer[curFrame]->plaY.uiStride*/, frmBuffer[curFrame]->plaY.uiWidth);

        curLine  = frmBuffer[curFrame]->plaY.ucCorner + frmBuffer[curFrame]->plaY.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaY.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaY.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaY.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;

        curLine  = frmBuffer[curFrame]->plaY.ucCorner + frmBuffer[curFrame]->plaY.uiStride + frmBuffer[curFrame]->plaY.uiStride + frmBuffer[curFrame]->plaY.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaY.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaY.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaY.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;

        curLine  = frmBuffer[curFrame]->plaY.ucCorner + (frmBuffer[curFrame]->plaY.uiHeight * frmBuffer[curFrame]->plaY.uiStride) - frmBuffer[curFrame]->plaY.uiStride - frmBuffer[curFrame]->plaY.uiStride - frmBuffer[curFrame]->plaY.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaY.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaY.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaY.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;

            
//U processing
        ptir_memcpy(frmBuffer[curFrame]->plaU.ucCorner + (frmBuffer[curFrame]->plaU.uiHeight * frmBuffer[curFrame]->plaU.uiStride) - frmBuffer[curFrame]->plaU.uiStride, frmBuffer[curFrame]->plaU.ucCorner + (frmBuffer[curFrame]->plaU.uiHeight * frmBuffer[curFrame]->plaU.uiStride) - frmBuffer[curFrame]->plaU.uiStride - frmBuffer[curFrame]->plaU.uiStride, frmBuffer[curFrame]->plaU.uiWidth);

        curLine  = frmBuffer[curFrame]->plaU.ucCorner + frmBuffer[curFrame]->plaU.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaU.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaU.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaU.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;
            
//V processing
        ptir_memcpy(frmBuffer[curFrame]->plaV.ucCorner + (frmBuffer[curFrame]->plaV.uiHeight * frmBuffer[curFrame]->plaV.uiStride) - frmBuffer[curFrame]->plaV.uiStride, frmBuffer[curFrame]->plaV.ucCorner + (frmBuffer[curFrame]->plaV.uiHeight * frmBuffer[curFrame]->plaV.uiStride) - frmBuffer[curFrame]->plaV.uiStride - frmBuffer[curFrame]->plaV.uiStride, frmBuffer[curFrame]->plaV.uiWidth);

        curLine  = frmBuffer[curFrame]->plaV.ucCorner + frmBuffer[curFrame]->plaV.uiStride;
        prevLine = curLine - frmBuffer[curFrame]->plaV.uiStride;
        nextLine = curLine + frmBuffer[curFrame]->plaV.uiStride;
        for(x = 0; x < frmBuffer[curFrame]->plaV.uiWidth; x++)
            curLine[x] = (prevLine[x] + nextLine[x]) / 2;
    }
//Non MC; left and right borders processing
//Y processing
    for (y = 4 + (BotBase?0:1); y < frmBuffer[curFrame]->plaY.uiHeight - 4; y += 2)
    {
        curLine  = frmBuffer[curFrame]->plaY.ucCorner + frmBuffer[curFrame]->plaY.uiStride * y;
        prevLine = curLine  - frmBuffer[curFrame]->plaY.uiStride;
        nextLine = curLine  + frmBuffer[curFrame]->plaY.uiStride;

        for (x = 0; x <= 4; x++)
        {
            curLine[    x    ] = (prevLine[    x    ] + nextLine[    x    ]) / 2;
            curLine[frmBuffer[curFrame]->plaY.uiStride - x - 1] = (prevLine[frmBuffer[curFrame]->plaY.uiStride - x - 1] + nextLine[frmBuffer[curFrame]->plaY.uiStride - x - 1]) / 2;
        }
    }

//U processing
    for (y = 1 + (BotBase?1:0); y < frmBuffer[curFrame]->plaU.uiHeight - 1; y += 2)
    {
        curLine  = frmBuffer[curFrame]->plaU.ucCorner + frmBuffer[curFrame]->plaU.uiStride * y;
        prevLine = curLine  - frmBuffer[curFrame]->plaU.uiStride;
        nextLine = curLine  + frmBuffer[curFrame]->plaU.uiStride;

        for (x = 0; x < 2; x++)
        {
            curLine[x            ] = (prevLine[        x    ] + nextLine[        x    ]) / 2;
            curLine[frmBuffer[curFrame]->plaU.uiStride - x - 1] = (prevLine[frmBuffer[curFrame]->plaU.uiStride - x - 1] + nextLine[frmBuffer[curFrame]->plaU.uiStride - x - 1]) / 2;
        }
    }

//V processing
    for (y = 1 + (BotBase?1:0); y < frmBuffer[curFrame]->plaV.uiHeight - 1; y += 2)
    {
        curLine  = frmBuffer[curFrame]->plaV.ucCorner + frmBuffer[curFrame]->plaV.uiStride * y;
        prevLine = curLine  - frmBuffer[curFrame]->plaV.uiStride;
        nextLine = curLine  + frmBuffer[curFrame]->plaV.uiStride;

        for (x = 0; x < 2; x++)
        {
            curLine[x            ] = (prevLine[        x    ] + nextLine[        x    ]) / 2;
            curLine[frmBuffer[curFrame]->plaV.uiStride - x - 1] = (prevLine[frmBuffer[curFrame]->plaV.uiStride - x - 1] + nextLine[frmBuffer[curFrame]->plaV.uiStride - x - 1]) / 2;
        }
    }
}
void DeinterlaceBilinearFilter(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase)
{
        BilinearDeint(frmBuffer[curFrame], BotBase);
        BuildLowEdgeMask_Main(frmBuffer, curFrame, BotBase);
        CalculateEdgesIYUV(frmBuffer, curFrame, BotBase);
        EdgeDirectionalIYUV_Main(frmBuffer, curFrame, BotBase);
        DeinterlaceBorders(frmBuffer, curFrame, refFrame, BotBase);
}
void DeinterlaceMedianFilter(Frame **frmBuffer, unsigned int curFrame, unsigned int refFrame, int BotBase)
{
        MedianDeinterlace(frmBuffer[curFrame], BotBase);
        BuildLowEdgeMask_Main(frmBuffer, curFrame, BotBase);
        CalculateEdgesIYUV(frmBuffer, curFrame, BotBase);
        EdgeDirectionalIYUV_Main(frmBuffer, curFrame, BotBase);
        DeinterlaceBorders(frmBuffer, curFrame, refFrame, BotBase);
}
