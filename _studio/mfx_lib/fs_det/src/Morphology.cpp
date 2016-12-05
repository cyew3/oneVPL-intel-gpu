//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
* 
* File: Morphology.cpp
*
* Structures and routines for morphological operations
* 
********************************************************************************/
#include <assert.h>
#include "Morphology.h"
#include <immintrin.h>

//structure which stores parameters of window which can be used for morphology
struct struct_WindowMask {
    int     m_iDiameter;    //window diameter
    int     m_iRadius;      //window radius
    const BYTE* m_pcWindowMask; //binary mask of the window defining the window form
};


//defines the mask of circle window 3x3
static const BYTE MASK_CIRCLE_3x3[] = {
    0,	1,	0,
    1,	1,	1,
    0,	1,	0
};

//defines the mask of circle window 5x5
static const BYTE MASK_CIRCLE_5x5[] = {
    0,	1,	1,	1,	0,
    1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,
    0,	1,	1,	1,	0
};

//defines the mask of circle window 7x7
static const BYTE MASK_CIRCLE_7x7[] = {
    0,	0,	1,	1,	1,	0,	0,
    0,	1,	1,	1,	1,	1,	0,
    1,	1,	1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,	1,	1,
    0,	1,	1,	1,	1,	1,	0,
    0,	0,	1,	1,	1,	0,	0
};

//defines the mask of circle window 9x9
static const BYTE MASK_CIRCLE_9x9[] = {
    0, 0, 0, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0
};

//defines the mask of square window 3x3
static const BYTE MASK_SQUARE_3x3[] = {
    1,	1,	1,
    1,	1,	1,
    1,	1,	1
};

//defines the mask of square window 5x5
static const BYTE MASK_SQUARE_5x5[] = {
    1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,
    1,	1,	1,	1,	1
};

//defines the mask of square window 7x7
static const BYTE MASK_SQUARE_7x7[] = {
    1,	1,	1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,	1,	1,
    1,	1,	1,	1,	1,	1,	1
};

//defines the mask of square window 9x9
static const BYTE MASK_SQUARE_9x9[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1
};

//defines radius and diameter of circle window 3x3
#define WINDOW_CIRCLE_3x3  {3,	1,	MASK_CIRCLE_3x3}

//defines radius and diameter of circle window 5x5
#define WINDOW_CIRCLE_5x5  {5,	2,	MASK_CIRCLE_5x5}

//defines radius and diameter of circle window 7x7
#define WINDOW_CIRCLE_7x7  {7,	3,	MASK_CIRCLE_7x7}

//defines radius and diameter of circle window 9x9
#define WINDOW_CIRCLE_9x9  {9,	4,	MASK_CIRCLE_9x9}

//defines radius and diameter of square window 3x3
#define WINDOW_SQUARE_3x3  {3,	1,	MASK_SQUARE_3x3}

//defines radius and diameter of square window 5x5
#define WINDOW_SQUARE_5x5  {5,	2,	MASK_SQUARE_5x5}

//defines radius and diameter of square window 7x7
#define WINDOW_SQUARE_7x7  {7,	3,	MASK_SQUARE_7x7}

//defines radius and diameter of square window 9x9
#define WINDOW_SQUARE_9x9  {9,	4,	MASK_SQUARE_9x9}

//array of available square windows
static const WindowMask MORPHOLOGY_SQUARE_WINDOWS[4] = {WINDOW_SQUARE_3x3, WINDOW_SQUARE_5x5, WINDOW_SQUARE_7x7, WINDOW_SQUARE_9x9};

//array of available circle windows
static const WindowMask MORPHOLOGY_CIRCLE_WINDOWS[4] = {WINDOW_CIRCLE_3x3, WINDOW_CIRCLE_5x5, WINDOW_CIRCLE_7x7, WINDOW_CIRCLE_9x9};


//defines mask for window with specified form and radius
const WindowMask* Morphology_GetWindowMask(int r, int circle) 
{
    if (circle)		return &MORPHOLOGY_CIRCLE_WINDOWS[r-1];
    else			return &MORPHOLOGY_SQUARE_WINDOWS[r-1]; 
}

//performs morphology dilation operation
void Dilation_Value_byte(BYTE *src, BYTE *dst, int w, int h, WindowMask *wnd) 
{
    int y, x;
    const BYTE *wnd_mask = wnd->m_pcWindowMask;
    int r = wnd->m_iRadius;
    //int d = wnd->m_iDiameter;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int xm, ym;
            int k = 0;
            int index0 = y * w + x;
            BYTE max_value = src[index0];

            if (x >= r && x < w-r && y >=r && y < h-r) {
                for (ym = y - r; ym <= y + r; ym++) {
                    int index = ym * w;
                    for (xm = x - r; xm <= x + r; xm++) {
                        if (wnd_mask[k]) {
                            if (src[index + xm] > max_value) max_value = src[index + xm];
                        }
                        k++;
                    }
                }
            }
            else {
                for (ym = y - r; ym <= y + r; ym++) {
                    int index = ym * w;
                    for(xm = x - r; xm <= x + r; xm++) {
                        if (INNER_POINT(ym, xm, h, w) && wnd_mask[k]) {
                            if (src[index + xm] > max_value) max_value = src[index + xm];
                        }
                        k++;
                    }
                }
            }
            dst[index0] = max_value;
        }
    }
}

//performs morphology erosion operation
void Erosion_Value_byte(BYTE *src, BYTE *dst, int w, int h, WindowMask *wnd) 
{
    int y, x;
    const BYTE *wnd_mask = wnd->m_pcWindowMask;
    int r = wnd->m_iRadius;
    //int d = wnd->m_iDiameter;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int xm, ym;
            int k = 0;
            int index0 = y * w + x;
            BYTE min_value = src[index0];

            if (x >= r && x < w-r && y >=r && y < h-r) {
                for (ym = y - r; ym <= y + r; ym++) {
                    int index = ym * w;
                    for (xm = x - r; xm <= x + r; xm++) {
                        if (wnd_mask[k]) {
                            if(src[index + xm] < min_value) min_value = src[index + xm];
                        }
                        k++;
                    }
                }
            }
            else {
                for (ym = y - r; ym <= y + r; ym++) {
                    int index = ym * w;
                    for (xm = x - r; xm <= x + r; xm++) {
                        if (INNER_POINT(ym, xm, h, w) && wnd_mask[k]) {
                            if (src[index + xm] < min_value) min_value = src[index + xm];
                        }
                        k++;
                    }
                }
            }
            dst[index0] = min_value;
        }
    }
}

// Dilation with 5x5 circular mask, using border replication
void Dilate_5x5_C(BYTE *pSrc, BYTE *pDst, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            int result = 0;

            int k = 0;
            for (int ym = y-2; ym <= y+2; ym++) {
                for (int xm = x-2; xm <= x+2; xm++) {

                    // replicate border pixels
                    int tx = MIN(MAX(xm, 0), width-1);
                    int ty = MIN(MAX(ym, 0), height-1);

                    if (MASK_CIRCLE_5x5[k++])
                        result = MAX(result, pSrc[ty * width + tx]);
                }
            }
            pDst[y * width + x] = (BYTE)result;
        }
    }
}

// Erosion with 5x5 circular mask, using border replication
void Erode_5x5_C(BYTE *pSrc, BYTE *pDst, int width, int height)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            int result = 0xff;

            int k = 0;
            for (int ym = y-2; ym <= y+2; ym++) {
                for (int xm = x-2; xm <= x+2; xm++) {

                    // replicate border pixels
                    int tx = MIN(MAX(xm, 0), width-1);
                    int ty = MIN(MAX(ym, 0), height-1);

                    if (MASK_CIRCLE_5x5[k++])
                        result = MIN(result, pSrc[ty * width + tx]);
                }
            }
            pDst[y * width + x] = (BYTE)result;
        }
    }
}

template <int Y0, int Y1, int Y2, int Y3, int Y4>
static void RowDilate_5x5_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height)
{
    FS_UNREFERENCED_PARAMETER(height);
    __m128i r3, s3; // column max of height 3
    __m128i r5, s5; // column max of height 5
    __m128i borderLeft = _mm_setr_epi8(0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13);
    __m128i borderRight = _mm_setr_epi8(0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1);
    __m128i t0;

    assert(width >= 16);
    assert((width & 0x1) == 0);

    r3 =                  _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y1*width]), borderLeft);
    r3 = _mm_max_epu8(r3, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y2*width]), borderLeft));
    r3 = _mm_max_epu8(r3, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y3*width]), borderLeft));
    r5 = _mm_max_epu8(r3, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y0*width]), borderLeft));
    r5 = _mm_max_epu8(r5, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y4*width]), borderLeft));
    pSrc += 14;

    int i;
    for (i = 0; i < width-16; i += 16) {
        s3 =                  _mm_loadu_si128((__m128i *)&pSrc[Y1*width]);
        s3 = _mm_max_epu8(s3, _mm_loadu_si128((__m128i *)&pSrc[Y2*width]));
        s3 = _mm_max_epu8(s3, _mm_loadu_si128((__m128i *)&pSrc[Y3*width]));
        s5 = _mm_max_epu8(s3, _mm_loadu_si128((__m128i *)&pSrc[Y0*width]));
        s5 = _mm_max_epu8(s5, _mm_loadu_si128((__m128i *)&pSrc[Y4*width]));
        pSrc += 16;

        t0 = r3;
        t0 = _mm_max_epu8(t0, _mm_alignr_epi8(s5, r5, 1));
        t0 = _mm_max_epu8(t0, _mm_alignr_epi8(s5, r5, 2));
        t0 = _mm_max_epu8(t0, _mm_alignr_epi8(s5, r5, 3));
        t0 = _mm_max_epu8(t0, _mm_alignr_epi8(s3, r3, 4));
        _mm_storeu_si128((__m128i *)pDst, t0);
        pDst += 16;

        r3 = s3;    // reuse
        r5 = s5;
    }

    // align last 16 with right border, with overlap if needed
    int overlap = 16 - (width - i);
    if (overlap) {
        pSrc -= overlap;
        pDst -= overlap;
        r3 =                  _mm_loadu_si128((__m128i *)&pSrc[Y1*width-16]);
        r3 = _mm_max_epu8(r3, _mm_loadu_si128((__m128i *)&pSrc[Y2*width-16]));
        r3 = _mm_max_epu8(r3, _mm_loadu_si128((__m128i *)&pSrc[Y3*width-16]));
        r5 = _mm_max_epu8(r3, _mm_loadu_si128((__m128i *)&pSrc[Y0*width-16]));
        r5 = _mm_max_epu8(r5, _mm_loadu_si128((__m128i *)&pSrc[Y4*width-16]));
    }
    s3 =                  _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y1*width], 0), borderRight);
    s3 = _mm_max_epu8(s3, _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y2*width], 0), borderRight));
    s3 = _mm_max_epu8(s3, _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y3*width], 0), borderRight));
    s5 = _mm_max_epu8(s3, _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y0*width], 0), borderRight));
    s5 = _mm_max_epu8(s5, _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y4*width], 0), borderRight));

    t0 = r3;
    t0 = _mm_max_epu8(t0, _mm_alignr_epi8(s5, r5, 1));
    t0 = _mm_max_epu8(t0, _mm_alignr_epi8(s5, r5, 2));
    t0 = _mm_max_epu8(t0, _mm_alignr_epi8(s5, r5, 3));
    t0 = _mm_max_epu8(t0, _mm_alignr_epi8(s3, r3, 4));
    _mm_storeu_si128((__m128i *)pDst, t0);
}

template <int Y0, int Y1, int Y2, int Y3, int Y4>
static void RowErode_5x5_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height)
{
    FS_UNREFERENCED_PARAMETER(height);
    __m128i r3, s3; // column max of height 3
    __m128i r5, s5; // column max of height 5
    __m128i borderLeft = _mm_setr_epi8(0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13);
    __m128i borderRight = _mm_setr_epi8(0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1);
    __m128i t0;

    assert(width >= 16);
    assert((width & 0x1) == 0);

    r3 =                  _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y1*width]), borderLeft);
    r3 = _mm_min_epu8(r3, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y2*width]), borderLeft));
    r3 = _mm_min_epu8(r3, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y3*width]), borderLeft));
    r5 = _mm_min_epu8(r3, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y0*width]), borderLeft));
    r5 = _mm_min_epu8(r5, _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y4*width]), borderLeft));
    pSrc += 14;

    int i;
    for (i = 0; i < width-16; i += 16) {
        s3 =                  _mm_loadu_si128((__m128i *)&pSrc[Y1*width]);
        s3 = _mm_min_epu8(s3, _mm_loadu_si128((__m128i *)&pSrc[Y2*width]));
        s3 = _mm_min_epu8(s3, _mm_loadu_si128((__m128i *)&pSrc[Y3*width]));
        s5 = _mm_min_epu8(s3, _mm_loadu_si128((__m128i *)&pSrc[Y0*width]));
        s5 = _mm_min_epu8(s5, _mm_loadu_si128((__m128i *)&pSrc[Y4*width]));
        pSrc += 16;

        t0 = r3;
        t0 = _mm_min_epu8(t0, _mm_alignr_epi8(s5, r5, 1));
        t0 = _mm_min_epu8(t0, _mm_alignr_epi8(s5, r5, 2));
        t0 = _mm_min_epu8(t0, _mm_alignr_epi8(s5, r5, 3));
        t0 = _mm_min_epu8(t0, _mm_alignr_epi8(s3, r3, 4));
        _mm_storeu_si128((__m128i *)pDst, t0);
        pDst += 16;

        r3 = s3;    // reuse
        r5 = s5;
    }

    // align last 16 with right border, with overlap if needed
    int overlap = 16 - (width - i);
    if (overlap) {
        pSrc -= overlap;
        pDst -= overlap;
        r3 =                  _mm_loadu_si128((__m128i *)&pSrc[Y1*width-16]);
        r3 = _mm_min_epu8(r3, _mm_loadu_si128((__m128i *)&pSrc[Y2*width-16]));
        r3 = _mm_min_epu8(r3, _mm_loadu_si128((__m128i *)&pSrc[Y3*width-16]));
        r5 = _mm_min_epu8(r3, _mm_loadu_si128((__m128i *)&pSrc[Y0*width-16]));
        r5 = _mm_min_epu8(r5, _mm_loadu_si128((__m128i *)&pSrc[Y4*width-16]));
    }
    s3 =                  _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y1*width], 0), borderRight);
    s3 = _mm_min_epu8(s3, _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y2*width], 0), borderRight));
    s3 = _mm_min_epu8(s3, _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y3*width], 0), borderRight));
    s5 = _mm_min_epu8(s3, _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y0*width], 0), borderRight));
    s5 = _mm_min_epu8(s5, _mm_shuffle_epi8(_mm_insert_epi16(t0, *(int *)&pSrc[Y4*width], 0), borderRight));

    t0 = r3;
    t0 = _mm_min_epu8(t0, _mm_alignr_epi8(s5, r5, 1));
    t0 = _mm_min_epu8(t0, _mm_alignr_epi8(s5, r5, 2));
    t0 = _mm_min_epu8(t0, _mm_alignr_epi8(s5, r5, 3));
    t0 = _mm_min_epu8(t0, _mm_alignr_epi8(s3, r3, 4));
    _mm_storeu_si128((__m128i *)pDst, t0);
}

// Dilation with 5x5 circular mask, using border replication
void Dilate_5x5_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height)
{
    RowDilate_5x5_SSE4<0,0,0,1,2>(pSrc, pDst, width, height); pDst += width;
    RowDilate_5x5_SSE4<0,0,1,2,3>(pSrc, pDst, width, height); pDst += width;

    for (int j = 2; j < height-2; j++) {
        RowDilate_5x5_SSE4<0,1,2,3,4>(pSrc, pDst, width, height); pDst += width;
        pSrc += width;
    }

    RowDilate_5x5_SSE4<0,1,2,3,3>(pSrc, pDst, width, height); pDst += width;
    RowDilate_5x5_SSE4<1,2,3,3,3>(pSrc, pDst, width, height); pDst += width;
}

// Erosion with 5x5 circular mask, using border replication
void Erode_5x5_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height)
{
    RowErode_5x5_SSE4<0,0,0,1,2>(pSrc, pDst, width, height); pDst += width;
    RowErode_5x5_SSE4<0,0,1,2,3>(pSrc, pDst, width, height); pDst += width;

    for (int j = 2; j < height-2; j++) {
        RowErode_5x5_SSE4<0,1,2,3,4>(pSrc, pDst, width, height); pDst += width;
        pSrc += width;
    }

    RowErode_5x5_SSE4<0,1,2,3,3>(pSrc, pDst, width, height); pDst += width;
    RowErode_5x5_SSE4<1,2,3,3,3>(pSrc, pDst, width, height); pDst += width;
}

typedef void(*t_Dilate_5x5)(BYTE *pSrc, BYTE *pDst, int width, int height);
typedef void(*t_Erode_5x5)(BYTE *pSrc, BYTE *pDst, int width, int height);

void Dilate_5x5(BYTE *pSrc, BYTE *pDst, int width, int height)
{
    t_Dilate_5x5 f = g_FS_OPT_SSE4 ? Dilate_5x5_SSE4 : Dilate_5x5_C;
    (*f)(pSrc, pDst, width, height);
}

void Erode_5x5(BYTE *pSrc, BYTE *pDst, int width, int height)
{
    t_Erode_5x5 f = g_FS_OPT_SSE4 ? Erode_5x5_SSE4 : Erode_5x5_C;
    (*f)(pSrc, pDst, width, height);
}

//
// end Dispatcher
//

//perform morphological opening (erosion followed by dilation with same morphology window)
void Open_Value_byte(BYTE *mask, BYTE *mask_buf, int w, int h, const WindowMask *wnd)
{
    FS_UNREFERENCED_PARAMETER(wnd);
    //Erosion_Value_byte(mask, mask_buf, w, h, wnd);
    //Dilation_Value_byte(mask_buf, mask, w, h, wnd);
    Erode_5x5(mask, mask_buf, w, h);
    Dilate_5x5(mask_buf, mask, w, h);
}

//perform morphological closing (dilation followed by erosion with same morphology window)
void Close_Value_byte(BYTE *mask, BYTE *mask_buf, int w, int h, const WindowMask *wnd)
{
    FS_UNREFERENCED_PARAMETER(wnd);
    //Dilation_Value_byte(mask, mask_buf, w, h, wnd);
    //Erosion_Value_byte(mask_buf, mask, w, h, wnd);
    Dilate_5x5(mask, mask_buf, w, h);
    Erode_5x5(mask_buf, mask, w, h);
}
