//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
/********************************************************************************
 * 
 * File: SkinDetAlg.c
 *
 * Structures and routines for skin detection algorithm
 * 
 ********************************************************************************/
#include "SkinDetAlg.h"
#include "FaceDetTmpl.h"
#include <assert.h>
#include "SkinHisto.h"
#include "Cleanup.h"
#include "Video.h"
#include "Morphology.h"
#include <math.h>
#include <immintrin.h>


//histograms declaration
extern const BYTE histoYrg [];
extern const BYTE histoYrg2[];
extern const BYTE histoYrg3[];



// fills mask of segments' probabilities decreasing 
void InitCentersProb(BYTE* buf, Dim* dim, int mode) {
    FS_UNREFERENCED_PARAMETER(mode);
    uint w, h, minWH, x, y, px, pyl, pyh;
    double low, high;

    //initialize
    { memset(buf, 255, dim->ubOff); w = dim->wb; h = dim->hb; }
    minWH = MIN(w,h);
    px    = (w + 3*minWH)/4 * 30  / 100 + 1;
    pyl   =         minWH * 33    / 100 + 1;
    pyh   =         minWH * 30    / 100 + 1;
    low   = (double)minWH * 25.0f / 100.0f;
    high  = (double)minWH * 35.0f / 100.0f;

    //draw left bottom corner
    for (y = 0; y < pyl; y++) {
        BYTE* dst = buf + y * w;
        for (x = 0; x < px; x++) {
            double r = sqrt( (double)((px-1-x)*(px-1-x) + (pyl-1-y)*(pyl-1-y)) );
            if (r > high) dst[x] = 0;
            else if (r > low) {
                dst[x] = (BYTE)CLIP(255.0 - 255.0 * (r - low) / (high - low) , 0, 255);
            }
        }
    }
    //right bottom corner
    for (y = 0; y < pyl; y++) {
        BYTE* dst = buf + y * w;
        for (x = w - px; x < w; x++) {
            dst[x] = dst[w - 1 - x];
        }
    } 
    //bottom line
    for (y = 0; y < pyl; y++) {
        memset(buf + y*w + px, buf[y*w + px - 1], w - px*2);
    }
    //upper border
    for (y = 0; y < pyh; y++) {
        memcpy(buf + (h - 1 - y)*w, buf + (y + (pyl - pyh))*w, w);
    }
    //left/right border
    for (y = pyl; y < h - pyh; y++) {
        memcpy(buf + y*w, buf + (pyl-1)*w, w);
    }
}

//initialize skin detection resources
int SkinDetectionInit(SDet* sd, int w, int h, int mode, SkinDetParameters* prms) 
{
    Dim* dim = &sd->dim;

    sd->prms = prms;

    //mode initialization
    sd->mode = mode;

    //dimensions initialization
    sd->frameNum    = 1;
    cDimInit(dim, w, h, BLOCKSIZE, prms->numFr);
    sd->minSegSizeBl = dim->blTotal * prms->minSegSize / 100;
    sd->maxSegSizeBl = dim->blTotal * prms->maxSegSize / 100;

    //Geometry
    INIT_MEMORY_C(sd->centersProb   , 0, dim->ubOff      , BYTE);
    if (sd->centersProb == NULL)      return 1;
    InitCentersProb(sd->centersProb , dim, mode);

    //Motion Data
    INIT_MEMORY_C(sd->motionMask    , 0, dim->blTotal    , BYTE);
    if (sd->motionMask == NULL)       return 1;
    
    INIT_MEMORY_C(sd->curMotionMask , 0, dim->blTotal    , BYTE);
    if (sd->curMotionMask == NULL)    return 1;

    INIT_MEMORY_C(sd->sadMax        , 0, dim->blTotal    , BYTE);
    if (sd->sadMax == NULL)           return 1;

    INIT_MEMORY_C(sd->prvProb       , 0, dim->blTotal    , BYTE);
    if (sd->prvProb == NULL)          return 1;

    //Skin
    INIT_MEMORY_C(sd->skinProbB     , 0, dim->blTotal    , BYTE);
    if (sd->skinProbB == NULL)        return 1;

    //Globals
    sd->maxSegSz = 0;
    sd->upd_skin = 0;
    sd->faceskin_perc = 100;
    sd->hist_ind = 0;
    INIT_MEMORY_C(sd->prvMask       , 0, dim->blTotal    , BYTE);
    if (sd->prvMask == NULL)          return 1;

    sd->sceneNum = 1;

    INIT_MEMORY_C(sd->stack         , 0, dim->blTotal    , int);
    if (sd->stack == NULL)            return 1;

    sd->stackptr = 0;
    return 0;   // Success
}

//frees skin detection resources
void SkinDetectionDeInit(SDet* sd, int mode) 
{
    FS_UNREFERENCED_PARAMETER(mode);
    //Geometry
    DEINIT_MEMORY_C(sd->centersProb);

    //Motion Data
    DEINIT_MEMORY_C(sd->motionMask   );
    DEINIT_MEMORY_C(sd->curMotionMask);
    DEINIT_MEMORY_C(sd->sadMax       );
    DEINIT_MEMORY_C(sd->prvProb      );

    //Skin
    DEINIT_MEMORY_C(sd->skinProbB);

    //Globals
    DEINIT_MEMORY_C(sd->prvMask);
    DEINIT_MEMORY_C(sd->stack);
}

void FDet_Init(FDet *fd) 
{
    fd->sum = NULL;
    fd->sqsum = NULL;
    fd->mat = NULL;
    fd->mat_norm = NULL;
    memset(&fd->cascade, 0, sizeof(haar_cascade));
    fd->class_cascade = NULL;
    fd->faces = NULL;
    fd->prevfc = NULL;
}

int FDet_Alloc(FDet *fd, int w, int h, int bsf) 
{
    int failed = 0;
    fd->sum = (int*   )malloc(sizeof(int   )*(((h/bsf)+1)*((w/bsf)+1)));
    if (!fd->sum) return 1;
    fd->sqsum     = (double*)malloc(sizeof(double)*(((h/bsf)+1)*((w/bsf)+1)));
    if (!fd->sqsum) return 1;
    fd->mat       = (BYTE*  )malloc(sizeof(BYTE  )*((h/bsf)*(w/bsf)));
    if (!fd->mat) return 1;
    fd->mat_norm  = (BYTE*  )malloc(sizeof(BYTE  )*((h/bsf)*(w/bsf)));
    if (!fd->mat_norm) return 1;
    
    load_cascade_features(&fd->cascade);
    
    failed = init_haar_classifier(&fd->class_cascade, &fd->cascade);
    if (failed) return failed;
    
    fd->faces = new std::vector<Rct>();
    fd->prevfc = new std::vector<Pt>();

    return 0;
}

void FDet_Free(FDet *fd) 
{
    if(fd->sum) {
        free(fd->sum); 
    }
    fd->sum = NULL;
    if(fd->sqsum) {
        free(fd->sqsum);
    }
    fd->sqsum = NULL;
    if(fd->mat) {
        free(fd->mat); 
    }
    fd->mat = NULL;
    if(fd->mat_norm) {
        free(fd->mat_norm);
    } 
    fd->mat_norm = NULL;
    if(fd->class_cascade) {
        free(fd->class_cascade);
    }
    fd->class_cascade = NULL;
    if(fd->faces) {
        delete fd->faces;
    }
    fd->faces = NULL;
    if(fd->prevfc) {
        delete fd->prevfc;
    }
    fd->prevfc;
}

//
// Fast (approximate) 5x5 Median
//

// unsigned byte absolute difference
static BYTE absdiff(BYTE a, BYTE b) 
{
    return (BYTE)abs((int)a - (int)b);
}
// unsigned byte comparison
static BYTE cmplt(BYTE a, BYTE b) 
{
    return (a < b ? 0xff : 0);
}
// unsigned byte select
static BYTE blendv(BYTE a, BYTE b, BYTE mask) 
{
    return (mask ? b : a);
}

static BYTE sum25_C(BYTE buf[25])
{
    uint sum = 0;
    for (int i = 0; i < 25; i++)
        sum += buf[i];

    sum = (sum * 5243) >> 17;   // exactly sum/25 for all possible sums
    return (BYTE)sum;
}

// approximate median estimation
static BYTE fast_median25_C(BYTE buf[25], BYTE sum)
{
    BYTE diff, mask;
    BYTE best = 0;
    BYTE dmin = 0xff;

    for (int i = 0; i < 25; i++) {
        diff = absdiff(sum, buf[i]);
        mask = cmplt(diff, dmin);
        best = blendv(best, buf[i], mask);
        dmin = MIN(dmin, diff);
    }
    return best;
}

// 5x5 approximate median, using border replication
static void FastMedian_5x5_C(const BYTE* pSrc, BYTE* pDst, int width, int height)
{
    BYTE buf[25];

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            BYTE *pb = buf;
            for (int ym = y-2; ym <= y+2; ym++) {
                for (int xm = x-2; xm <= x+2; xm++) {

                    // replicate border pixels
                    int tx = MIN(MAX(xm, 0), width-1);
                    int ty = MIN(MAX(ym, 0), height-1);
                    *pb++ = pSrc[ty * width + tx];
                }
            }
            BYTE sum = sum25_C(buf);
            pDst[y * width + x] = fast_median25_C(buf, sum);
        }
    }
}

// 5x5 sums for the 1x16 block
static __m128i sum5_SSE4(__m128i r[5], __m128i s[5])
{
    __m128i zero = _mm_setzero_si128();

    // 5x1 vertical sums
    __m128i t0 =           _mm_unpacklo_epi8(r[0], zero); // 8 x 16-bit
    t0 = _mm_add_epi16(t0, _mm_unpacklo_epi8(r[1], zero));
    t0 = _mm_add_epi16(t0, _mm_unpacklo_epi8(r[2], zero));
    t0 = _mm_add_epi16(t0, _mm_unpacklo_epi8(r[3], zero));
    t0 = _mm_add_epi16(t0, _mm_unpacklo_epi8(r[4], zero));

    __m128i t1 =           _mm_unpackhi_epi8(r[0], zero);
    t1 = _mm_add_epi16(t1, _mm_unpackhi_epi8(r[1], zero));
    t1 = _mm_add_epi16(t1, _mm_unpackhi_epi8(r[2], zero));
    t1 = _mm_add_epi16(t1, _mm_unpackhi_epi8(r[3], zero));
    t1 = _mm_add_epi16(t1, _mm_unpackhi_epi8(r[4], zero));

    __m128i t2 =           _mm_unpacklo_epi8(s[0], zero);
    t2 = _mm_add_epi16(t2, _mm_unpacklo_epi8(s[1], zero));
    t2 = _mm_add_epi16(t2, _mm_unpacklo_epi8(s[2], zero));
    t2 = _mm_add_epi16(t2, _mm_unpacklo_epi8(s[3], zero));
    t2 = _mm_add_epi16(t2, _mm_unpacklo_epi8(s[4], zero));

    // 1x5 horizontal sums
    __m128i sum0 = t0;
    sum0 = _mm_add_epi16(sum0, _mm_alignr_epi8(t1, t0, 2));
    sum0 = _mm_add_epi16(sum0, _mm_alignr_epi8(t1, t0, 4));
    sum0 = _mm_add_epi16(sum0, _mm_alignr_epi8(t1, t0, 6));
    sum0 = _mm_add_epi16(sum0, _mm_alignr_epi8(t1, t0, 8));
    sum0 = _mm_srli_epi16(_mm_mulhi_epu16(sum0, _mm_set1_epi16(5243)), 1);  // sum/25

    __m128i sum1 = t1;
    sum1 = _mm_add_epi16(sum1, _mm_alignr_epi8(t2, t1, 2));
    sum1 = _mm_add_epi16(sum1, _mm_alignr_epi8(t2, t1, 4));
    sum1 = _mm_add_epi16(sum1, _mm_alignr_epi8(t2, t1, 6));
    sum1 = _mm_add_epi16(sum1, _mm_alignr_epi8(t2, t1, 8));
    sum1 = _mm_srli_epi16(_mm_mulhi_epu16(sum1, _mm_set1_epi16(5243)), 1);

    return _mm_packus_epi16(sum0, sum1);    // 16 x 8-bit sums
}

// unsigned byte absolute difference
static inline __m128i _mm_absdiff_epu8(__m128i a, __m128i b) 
{
    return _mm_or_si128(_mm_subs_epu8(a, b), _mm_subs_epu8(b, a));  // subtract both ways using unsigned saturation
}
// unsigned byte comparison
static inline __m128i _mm_cmplt_epu8(__m128i a, __m128i b) 
{
    return _mm_andnot_si128(_mm_cmpeq_epi8(a, b), _mm_cmpeq_epi8(_mm_min_epu8(a, b), a));
}

// approximate median estimation
static void fast_median5_SSE4(__m128i r[5], __m128i s[5], __m128i sum)
{
    __m128i diff, mask;
    __m128i best = _mm_setzero_si128();
    __m128i dmin = _mm_cmpeq_epi8(best, best);  // 0xff

#ifdef __INTEL_COMPILER
#pragma unroll(5)
#endif
    for (int i = 0; i < 5; i++) {

        __m128i x0 = r[i];
        __m128i x5 = s[i];
        __m128i x1 = _mm_alignr_epi8(x5, x0, 1);
        __m128i x2 = _mm_alignr_epi8(x5, x0, 2);
        __m128i x3 = _mm_alignr_epi8(x5, x0, 3);
        __m128i x4 = _mm_alignr_epi8(x5, x0, 4);

        diff = _mm_absdiff_epu8(sum, x0);
        mask = _mm_cmplt_epu8(diff, dmin);
        best = _mm_blendv_epi8(best, x0, mask);
        dmin = _mm_min_epu8(dmin, diff);

        diff = _mm_absdiff_epu8(sum, x1);
        mask = _mm_cmplt_epu8(diff, dmin);
        best = _mm_blendv_epi8(best, x1, mask);
        dmin = _mm_min_epu8(dmin, diff);

        diff = _mm_absdiff_epu8(sum, x2);
        mask = _mm_cmplt_epu8(diff, dmin);
        best = _mm_blendv_epi8(best, x2, mask);
        dmin = _mm_min_epu8(dmin, diff);

        diff = _mm_absdiff_epu8(sum, x3);
        mask = _mm_cmplt_epu8(diff, dmin);
        best = _mm_blendv_epi8(best, x3, mask);
        dmin = _mm_min_epu8(dmin, diff);

        diff = _mm_absdiff_epu8(sum, x4);
        mask = _mm_cmplt_epu8(diff, dmin);
        best = _mm_blendv_epi8(best, x4, mask);
        dmin = _mm_min_epu8(dmin, diff);
    }
    r[0] = best;    // store approximate medians
}

template <int Y0, int Y1, int Y2, int Y3, int Y4>
static void RowFastMedian_5x5_SSE4(const BYTE* pSrc, BYTE* pDst, int width, int height)
{
    FS_UNREFERENCED_PARAMETER(height);
    __m128i r[5], s[5], sum;
    __m128i borderLeft = _mm_setr_epi8(0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13);
    __m128i borderRight = _mm_setr_epi8(0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1);

    assert(width >= 16);
    assert((width & 0x1) == 0);

    r[0] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y0*width]), borderLeft);
    r[1] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y1*width]), borderLeft);
    r[2] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y2*width]), borderLeft);
    r[3] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y3*width]), borderLeft);
    r[4] = _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&pSrc[Y4*width]), borderLeft);
    pSrc += 14;

    int i;
    for (i = 0; i < width-16; i += 16) {
        s[0] = _mm_loadu_si128((__m128i *)&pSrc[Y0*width]);
        s[1] = _mm_loadu_si128((__m128i *)&pSrc[Y1*width]);
        s[2] = _mm_loadu_si128((__m128i *)&pSrc[Y2*width]);
        s[3] = _mm_loadu_si128((__m128i *)&pSrc[Y3*width]);
        s[4] = _mm_loadu_si128((__m128i *)&pSrc[Y4*width]);
        pSrc += 16;

        sum = sum5_SSE4(r, s);
        fast_median5_SSE4(r, s, sum);
        _mm_storeu_si128((__m128i *)pDst, r[0]);
        pDst += 16;

        r[0] = s[0];    // reuse
        r[1] = s[1];
        r[2] = s[2];
        r[3] = s[3];
        r[4] = s[4];
    }

    // align last 16 with right border, with overlap if needed
    int overlap = 16 - (width - i);
    if (overlap) {
        pSrc -= overlap;
        pDst -= overlap;
        r[0] = _mm_loadu_si128((__m128i *)&pSrc[Y0*width-16]);
        r[1] = _mm_loadu_si128((__m128i *)&pSrc[Y1*width-16]);
        r[2] = _mm_loadu_si128((__m128i *)&pSrc[Y2*width-16]);
        r[3] = _mm_loadu_si128((__m128i *)&pSrc[Y3*width-16]);
        r[4] = _mm_loadu_si128((__m128i *)&pSrc[Y4*width-16]);
    }
    s[0] = _mm_shuffle_epi8(_mm_insert_epi16(s[0], *(int *)&pSrc[Y0*width], 0), borderRight);
    s[1] = _mm_shuffle_epi8(_mm_insert_epi16(s[1], *(int *)&pSrc[Y1*width], 0), borderRight);
    s[2] = _mm_shuffle_epi8(_mm_insert_epi16(s[2], *(int *)&pSrc[Y2*width], 0), borderRight);
    s[3] = _mm_shuffle_epi8(_mm_insert_epi16(s[3], *(int *)&pSrc[Y3*width], 0), borderRight);
    s[4] = _mm_shuffle_epi8(_mm_insert_epi16(s[4], *(int *)&pSrc[Y4*width], 0), borderRight);

    sum = sum5_SSE4(r, s);
    fast_median5_SSE4(r, s, sum);
    _mm_storeu_si128((__m128i *)pDst, r[0]);
}

// 5x5 approximate median, using border replication
static void FastMedian_5x5_SSE4(const BYTE* pSrc, BYTE* pDst, int width, int height)
{
    RowFastMedian_5x5_SSE4<0,0,0,1,2>(pSrc, pDst, width, height); pDst += width;
    RowFastMedian_5x5_SSE4<0,0,1,2,3>(pSrc, pDst, width, height); pDst += width;

    for (int j = 2; j < height-2; j++) {
        RowFastMedian_5x5_SSE4<0,1,2,3,4>(pSrc, pDst, width, height); pDst += width;
        pSrc += width;
    }

    RowFastMedian_5x5_SSE4<0,1,2,3,3>(pSrc, pDst, width, height); pDst += width;
    RowFastMedian_5x5_SSE4<1,2,3,3,3>(pSrc, pDst, width, height); pDst += width;
}


static void BlockSum_4x4_C(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width; x += 4) {

            BYTE *ps = pSrc + (y * pitch) + x;
            uint sum = 0;

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    sum += ps[i * pitch + j];
                }
            }
            sum = (sum + 8) >> 4;
            *pDst++ = (BYTE)sum;
        }
    }
}

static void BlockSum_4x4_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    assert((width & 0xf) == 0);

    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            // 4 horizontal blocks at a time
            __m128i r0 = _mm_loadu_si128((__m128i *)&ps[0*pitch]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&ps[1*pitch]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&ps[2*pitch]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&ps[3*pitch]);

            __m128i s0 = _mm_sad_epu8(_mm_unpacklo_epi32(r0, r1), zero);
            __m128i s1 = _mm_sad_epu8(_mm_unpacklo_epi32(r2, r3), zero);
            __m128i s2 = _mm_sad_epu8(_mm_unpackhi_epi32(r0, r1), zero);
            __m128i s3 = _mm_sad_epu8(_mm_unpackhi_epi32(r2, r3), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);

            s0 = _mm_packs_epi32(s0, s2);

            s0 = _mm_packs_epi32(s0, s0);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-4)));
            s0 = _mm_packus_epi16(s0, s0);

            *(int *)pDst = _mm_cvtsi128_si32(s0);   // 4 x 8-bit
            pDst += 4;
        }
    }
}

static void BlockSum_4x4_AVX2(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    assert((width & 0xf) == 0);

    for (int y = 0; y < height; y += 4) {
        int x;
        for (x = 0; x < width-31; x += 32) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m256i zero = _mm256_setzero_si256();

            // 8 horizontal blocks at a time
            __m256i r0 = _mm256_loadu_si256((__m256i *)&ps[0*pitch]);
            __m256i r1 = _mm256_loadu_si256((__m256i *)&ps[1*pitch]);
            __m256i r2 = _mm256_loadu_si256((__m256i *)&ps[2*pitch]);
            __m256i r3 = _mm256_loadu_si256((__m256i *)&ps[3*pitch]);

            __m256i s0 = _mm256_sad_epu8(_mm256_unpacklo_epi32(r0, r1), zero);
            __m256i s1 = _mm256_sad_epu8(_mm256_unpacklo_epi32(r2, r3), zero);
            __m256i s2 = _mm256_sad_epu8(_mm256_unpackhi_epi32(r0, r1), zero);
            __m256i s3 = _mm256_sad_epu8(_mm256_unpackhi_epi32(r2, r3), zero);

            s0 = _mm256_add_epi32(s0, s1);
            s2 = _mm256_add_epi32(s2, s3);

            s0 = _mm256_packs_epi32(s0, s2);

            __m128i t0 = _mm_packs_epi32(_mm256_castsi256_si128(s0), _mm256_extractf128_si256(s0, 1));
            t0 = _mm_mulhrs_epi16(t0, _mm_set1_epi16(1<<(15-4)));
            t0 = _mm_packus_epi16(t0, t0);

            _mm_storel_epi64((__m128i *)pDst, t0);  // 8 x 8-bit
            pDst += 8;
        }

        for (; x < width; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            // 4 horizontal blocks at a time
            __m128i r0 = _mm_loadu_si128((__m128i *)&ps[0*pitch]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&ps[1*pitch]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&ps[2*pitch]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&ps[3*pitch]);

            __m128i s0 = _mm_sad_epu8(_mm_unpacklo_epi32(r0, r1), zero);
            __m128i s1 = _mm_sad_epu8(_mm_unpacklo_epi32(r2, r3), zero);
            __m128i s2 = _mm_sad_epu8(_mm_unpackhi_epi32(r0, r1), zero);
            __m128i s3 = _mm_sad_epu8(_mm_unpackhi_epi32(r2, r3), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);

            s0 = _mm_packs_epi32(s0, s2);

            s0 = _mm_packs_epi32(s0, s0);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-4)));
            s0 = _mm_packus_epi16(s0, s0);

            *(int *)pDst = _mm_cvtsi128_si32(s0);   // 4 x 8-bit
            pDst += 4;
        }
    }
}

static void BlockSum_8x4_C(BYTE *pSrc, BYTE *pDstU, BYTE *pDstV, int width2, int height, int pitch)
{
    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width2; x += 8) {

            BYTE *ps = pSrc + (y * pitch) + x;
            uint sumU = 0;
            uint sumV = 0;

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 8; j+=2) {
                    sumU+= ps[i * pitch + j];
                    sumV+= ps[i * pitch + j+1];
                }
            }
            sumU = (sumU + 8) >> 4;
            sumV = (sumV + 8) >> 4;
            *pDstU++ = (BYTE)sumU;
            *pDstV++ = (BYTE)sumV;			
        }
    }
}

static void BlockSum_8x4_SSE4(BYTE *pSrc, BYTE *pDstU, BYTE *pDstV, int width2, int height, int pitch)
{
    assert((width2/2 & 0xf) == 0);
    __m128i pi = _mm_set_epi8(15, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 0);
    //unsigned a[8] = {0x06040200u, 0x0E0C0A08u, 0x07050301u, 0x0F0D0B09u};
    //__m128i pi = _mm_load_si128((__m128i*)a);

    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width2; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            __m128i r0 = _mm_loadu_si128((__m128i *)&ps[0*pitch]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&ps[1*pitch]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&ps[2*pitch]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&ps[3*pitch]);

            r0 = _mm_shuffle_epi8(r0, pi);
            r1 = _mm_shuffle_epi8(r1, pi);
            r2 = _mm_shuffle_epi8(r2, pi);
            r3 = _mm_shuffle_epi8(r3, pi);

            __m128i s0 = _mm_sad_epu8(_mm_unpacklo_epi32(r0, r1), zero);
            __m128i s1 = _mm_sad_epu8(_mm_unpacklo_epi32(r2, r3), zero);
            __m128i s2 = _mm_sad_epu8(_mm_unpackhi_epi32(r0, r1), zero);
            __m128i s3 = _mm_sad_epu8(_mm_unpackhi_epi32(r2, r3), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);

            s0 = _mm_packs_epi32(s0, s2);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-4)));

            //pDstU[0] = s0.m128i_u8[0];
            //pDstU[1] = s0.m128i_u8[4];
            //pDstV[0] = s0.m128i_u8[8];
            //pDstV[1] = s0.m128i_u8[12];
            pDstU[0] = _mm_extract_epi8(s0, 0);
            pDstU[1] = _mm_extract_epi8(s0, 4);
            pDstV[0] = _mm_extract_epi8(s0, 8);
            pDstV[1] = _mm_extract_epi8(s0, 12);
            pDstU+= 2;
            pDstV+= 2;
        }
    }
}

static void BlockSum_8x4_AVX2(BYTE *pSrc, BYTE *pDstU, BYTE *pDstV, int width2, int height, int pitch)
{
    assert((width2/2 & 0xf) == 0);

    __FS_ALIGN32 unsigned int a[8] = {0x06040200u, 0x0E0C0A08u, 0x07050301u, 0x0F0D0B09u, 0x16141210u, 0x1E1C1A18u, 0x17151311u, 0x1F1D1B19u};
    __m256i pi = _mm256_load_si256((__m256i*)a);

    for (int y = 0; y < height; y += 4) {
        int x;
        for (x = 0; x < width2-31; x += 32) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m256i zero = _mm256_setzero_si256();

            // 8 horizontal blocks at a time
            __m256i r0 = _mm256_loadu_si256((__m256i *)&ps[0*pitch]);
            __m256i r1 = _mm256_loadu_si256((__m256i *)&ps[1*pitch]);
            __m256i r2 = _mm256_loadu_si256((__m256i *)&ps[2*pitch]);
            __m256i r3 = _mm256_loadu_si256((__m256i *)&ps[3*pitch]);

            r0 = _mm256_shuffle_epi8(r0, pi);
            r1 = _mm256_shuffle_epi8(r1, pi);
            r2 = _mm256_shuffle_epi8(r2, pi);
            r3 = _mm256_shuffle_epi8(r3, pi);

            __m256i s0 = _mm256_sad_epu8(_mm256_unpacklo_epi32(r0, r1), zero);
            __m256i s1 = _mm256_sad_epu8(_mm256_unpacklo_epi32(r2, r3), zero);
            __m256i s2 = _mm256_sad_epu8(_mm256_unpackhi_epi32(r0, r1), zero);
            __m256i s3 = _mm256_sad_epu8(_mm256_unpackhi_epi32(r2, r3), zero);

            s0 = _mm256_add_epi32(s0, s1);
            s2 = _mm256_add_epi32(s2, s3);

            __m128i t0 = _mm_packs_epi32(_mm256_castsi256_si128(s0), _mm256_extractf128_si256(s0, 1));
            t0 = _mm_packs_epi32(t0, t0);
            t0 = _mm_mulhrs_epi16(t0, _mm_set1_epi16(1<<(15-4)));
            t0 = _mm_packus_epi16(t0, t0);
            *(int *)pDstU = _mm_cvtsi128_si32(t0);

            __m128i t2 = _mm_packs_epi32(_mm256_castsi256_si128(s2), _mm256_extractf128_si256(s2, 1));
            t2 = _mm_packs_epi32(t2, t2);
            t2 = _mm_mulhrs_epi16(t2, _mm_set1_epi16(1<<(15-4)));
            t2 = _mm_packus_epi16(t2, t2);
            *(int *)pDstV = _mm_cvtsi128_si32(t2);

            pDstU+= 4;
            pDstV+= 4;
        }

        __m128i pi128 = _mm_set_epi8(15, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 0);
        for (; x < width2; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            __m128i r0 = _mm_loadu_si128((__m128i *)&ps[0*pitch]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&ps[1*pitch]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&ps[2*pitch]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&ps[3*pitch]);

            r0 = _mm_shuffle_epi8(r0, pi128);
            r1 = _mm_shuffle_epi8(r1, pi128);
            r2 = _mm_shuffle_epi8(r2, pi128);
            r3 = _mm_shuffle_epi8(r3, pi128);

            __m128i s0 = _mm_sad_epu8(_mm_unpacklo_epi32(r0, r1), zero);
            __m128i s1 = _mm_sad_epu8(_mm_unpacklo_epi32(r2, r3), zero);
            __m128i s2 = _mm_sad_epu8(_mm_unpackhi_epi32(r0, r1), zero);
            __m128i s3 = _mm_sad_epu8(_mm_unpackhi_epi32(r2, r3), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);

            s0 = _mm_packs_epi32(s0, s2);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-4)));

            //pDstU[0] = s0.m128i_u8[0];
            //pDstU[1] = s0.m128i_u8[4];
            //pDstV[0] = s0.m128i_u8[8];
            //pDstV[1] = s0.m128i_u8[12];
            pDstU[0] = _mm_extract_epi8(s0, 0);
            pDstU[1] = _mm_extract_epi8(s0, 4);
            pDstV[0] = _mm_extract_epi8(s0, 8);
            pDstV[1] = _mm_extract_epi8(s0, 12);
            pDstU+= 2;
            pDstV+= 2;
        }
    }
}

static void BlockSum_8x8_C(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    for (int y = 0; y < height; y += 8) {
        for (int x = 0; x < width; x += 8) {

            BYTE *ps = pSrc + (y * pitch) + x;
            uint sum = 0;

            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    sum += ps[i * pitch + j];
                }
            }
            sum = (sum + 32) >> 6;
            *pDst++ = (BYTE)sum;
        }
    }
}

static void BlockSum_8x8_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    assert((width & 0xf) == 0);

    for (int y = 0; y < height; y += 8) {
        for (int x = 0; x < width; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0*pitch]), zero);
            __m128i s1 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[1*pitch]), zero);
            __m128i s2 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[2*pitch]), zero);
            __m128i s3 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[3*pitch]), zero);
            __m128i s4 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[4*pitch]), zero);
            __m128i s5 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[5*pitch]), zero);
            __m128i s6 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[6*pitch]), zero);
            __m128i s7 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[7*pitch]), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);
            s4 = _mm_add_epi32(s4, s5);
            s6 = _mm_add_epi32(s6, s7);

            s0 = _mm_add_epi32(s0, s2);
            s4 = _mm_add_epi32(s4, s6);

            s0 = _mm_add_epi32(s0, s4);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-6)));

            pDst[0] = _mm_extract_epi8(s0, 0);
            pDst[1] = _mm_extract_epi8(s0, 8);
            pDst += 2;
        }
    }
}

static void BlockSum_8x8_AVX2(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    assert((width & 0xf) == 0);

    for (int y = 0; y < height; y += 8) {
        int x;
        for (x = 0; x < width-31; x += 32) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m256i zero = _mm256_setzero_si256();

            // 4 horizontal blocks at a time
            __m256i s0 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[0*pitch]), zero);
            __m256i s1 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[1*pitch]), zero);
            __m256i s2 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[2*pitch]), zero);
            __m256i s3 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[3*pitch]), zero);
            __m256i s4 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[4*pitch]), zero);
            __m256i s5 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[5*pitch]), zero);
            __m256i s6 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[6*pitch]), zero);
            __m256i s7 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[7*pitch]), zero);

            s0 = _mm256_add_epi32(s0, s1);
            s2 = _mm256_add_epi32(s2, s3);
            s4 = _mm256_add_epi32(s4, s5);
            s6 = _mm256_add_epi32(s6, s7);

            s0 = _mm256_add_epi32(s0, s2);
            s4 = _mm256_add_epi32(s4, s6);

            s0 = _mm256_add_epi32(s0, s4);
            s0 = _mm256_mulhrs_epi16(s0, _mm256_set1_epi16(1<<(15-6)));

            pDst[0] = _mm_extract_epi8(_mm256_castsi256_si128(s0), 0);
            pDst[1] = _mm_extract_epi8(_mm256_castsi256_si128(s0), 8);
            pDst[2] = _mm_extract_epi8(_mm256_extractf128_si256(s0, 1), 0);
            pDst[3] = _mm_extract_epi8(_mm256_extractf128_si256(s0, 1), 8);
            pDst += 4;
        }

        for (; x < width; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0*pitch]), zero);
            __m128i s1 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[1*pitch]), zero);
            __m128i s2 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[2*pitch]), zero);
            __m128i s3 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[3*pitch]), zero);
            __m128i s4 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[4*pitch]), zero);
            __m128i s5 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[5*pitch]), zero);
            __m128i s6 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[6*pitch]), zero);
            __m128i s7 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[7*pitch]), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);
            s4 = _mm_add_epi32(s4, s5);
            s6 = _mm_add_epi32(s6, s7);

            s0 = _mm_add_epi32(s0, s2);
            s4 = _mm_add_epi32(s4, s6);

            s0 = _mm_add_epi32(s0, s4);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-6)));

            pDst[0] = _mm_extract_epi8(s0, 0);
            pDst[1] = _mm_extract_epi8(s0, 8);
            pDst += 2;
        }
    }
}


static void BlockSum_4x4_slice_C(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    for (int y = 0; y < slice_nlines_fr; y += 4) {
        for (int x = 0; x < width; x += 4) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            uint sum = 0;

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    sum += ps[i * pitch + j];
                }
            }
            sum = (sum + 8) >> 4;
            *pDst++ = (BYTE)sum;
        }
    }
}

static void BlockSum_4x4_slice_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    assert((width & 0xf) == 0);

    for (int y = 0; y < slice_nlines_fr; y += 4) {
        for (int x = 0; x < width; x += 16) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            // 4 horizontal blocks at a time
            __m128i r0 = _mm_loadu_si128((__m128i *)&ps[0*pitch]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&ps[1*pitch]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&ps[2*pitch]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&ps[3*pitch]);

            __m128i s0 = _mm_sad_epu8(_mm_unpacklo_epi32(r0, r1), zero);
            __m128i s1 = _mm_sad_epu8(_mm_unpacklo_epi32(r2, r3), zero);
            __m128i s2 = _mm_sad_epu8(_mm_unpackhi_epi32(r0, r1), zero);
            __m128i s3 = _mm_sad_epu8(_mm_unpackhi_epi32(r2, r3), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);

            s0 = _mm_packs_epi32(s0, s2);

            s0 = _mm_packs_epi32(s0, s0);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-4)));
            s0 = _mm_packus_epi16(s0, s0);

            *(int *)pDst = _mm_cvtsi128_si32(s0);   // 4 x 8-bit
            pDst += 4;
        }
    }
}

static void BlockSum_4x4_slice_AVX2(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    assert((width & 0xf) == 0);

    for (int y = 0; y < slice_nlines_fr; y += 4) {
        int x;
        for (x = 0; x < width-31; x += 32) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m256i zero = _mm256_setzero_si256();

            // 8 horizontal blocks at a time
            __m256i r0 = _mm256_loadu_si256((__m256i *)&ps[0*pitch]);
            __m256i r1 = _mm256_loadu_si256((__m256i *)&ps[1*pitch]);
            __m256i r2 = _mm256_loadu_si256((__m256i *)&ps[2*pitch]);
            __m256i r3 = _mm256_loadu_si256((__m256i *)&ps[3*pitch]);

            __m256i s0 = _mm256_sad_epu8(_mm256_unpacklo_epi32(r0, r1), zero);
            __m256i s1 = _mm256_sad_epu8(_mm256_unpacklo_epi32(r2, r3), zero);
            __m256i s2 = _mm256_sad_epu8(_mm256_unpackhi_epi32(r0, r1), zero);
            __m256i s3 = _mm256_sad_epu8(_mm256_unpackhi_epi32(r2, r3), zero);

            s0 = _mm256_add_epi32(s0, s1);
            s2 = _mm256_add_epi32(s2, s3);

            s0 = _mm256_packs_epi32(s0, s2);

            __m128i t0 = _mm_packs_epi32(_mm256_castsi256_si128(s0), _mm256_extractf128_si256(s0, 1));
            t0 = _mm_mulhrs_epi16(t0, _mm_set1_epi16(1<<(15-4)));
            t0 = _mm_packus_epi16(t0, t0);

            _mm_storel_epi64((__m128i *)pDst, t0);  // 8 x 8-bit
            pDst += 8;
        }

        for (; x < width; x += 16) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            // 4 horizontal blocks at a time
            __m128i r0 = _mm_loadu_si128((__m128i *)&ps[0*pitch]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&ps[1*pitch]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&ps[2*pitch]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&ps[3*pitch]);

            __m128i s0 = _mm_sad_epu8(_mm_unpacklo_epi32(r0, r1), zero);
            __m128i s1 = _mm_sad_epu8(_mm_unpacklo_epi32(r2, r3), zero);
            __m128i s2 = _mm_sad_epu8(_mm_unpackhi_epi32(r0, r1), zero);
            __m128i s3 = _mm_sad_epu8(_mm_unpackhi_epi32(r2, r3), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);

            s0 = _mm_packs_epi32(s0, s2);

            s0 = _mm_packs_epi32(s0, s0);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-4)));
            s0 = _mm_packus_epi16(s0, s0);

            *(int *)pDst = _mm_cvtsi128_si32(s0);   // 4 x 8-bit
            pDst += 4;
        }
    }
}

static void BlockSum_8x4_slice_C(BYTE *pSrc, BYTE *pDstU, BYTE *pDstV, int width2, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    for (int y = 0; y < slice_nlines_fr; y += 4) {
        for (int x = 0; x < width2; x += 8) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            uint sumU = 0;
            uint sumV = 0;

            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 8; j+=2) {
                    sumU+= ps[i * pitch + j];
                    sumV+= ps[i * pitch + j+1];
                }
            }
            sumU = (sumU + 8) >> 4;
            sumV = (sumV + 8) >> 4;
            *pDstU++ = (BYTE)sumU;
            *pDstV++ = (BYTE)sumV;
        }
    }
}

static void BlockSum_8x4_slice_SSE4(BYTE *pSrc, BYTE *pDstU, BYTE *pDstV, int width2, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    assert((width2/2 & 0xf) == 0);
    __m128i pi = _mm_set_epi8(15, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 0);
    //unsigned a[8] = {0x06040200u, 0x0E0C0A08u, 0x07050301u, 0x0F0D0B09u};
    //__m128i pi = _mm_load_si128((__m128i*)a);

    for (int y = 0; y < slice_nlines_fr; y += 4) {
        for (int x = 0; x < width2; x += 16) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            __m128i r0 = _mm_loadu_si128((__m128i *)&ps[0*pitch]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&ps[1*pitch]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&ps[2*pitch]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&ps[3*pitch]);

            r0 = _mm_shuffle_epi8(r0, pi);
            r1 = _mm_shuffle_epi8(r1, pi);
            r2 = _mm_shuffle_epi8(r2, pi);
            r3 = _mm_shuffle_epi8(r3, pi);

            __m128i s0 = _mm_sad_epu8(_mm_unpacklo_epi32(r0, r1), zero);
            __m128i s1 = _mm_sad_epu8(_mm_unpacklo_epi32(r2, r3), zero);
            __m128i s2 = _mm_sad_epu8(_mm_unpackhi_epi32(r0, r1), zero);
            __m128i s3 = _mm_sad_epu8(_mm_unpackhi_epi32(r2, r3), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);

            s0 = _mm_packs_epi32(s0, s2);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-4)));

            //pDstU[0] = s0.m128i_u8[0];
            //pDstU[1] = s0.m128i_u8[4];
            //pDstV[0] = s0.m128i_u8[8];
            //pDstV[1] = s0.m128i_u8[12];
            
            pDstU[0] = _mm_extract_epi8(s0, 0);
            pDstU[1] = _mm_extract_epi8(s0, 4);
            pDstV[0] = _mm_extract_epi8(s0, 8);
            pDstV[1] = _mm_extract_epi8(s0, 12);
            pDstU+= 2;
            pDstV+= 2;
        }
    }
}

static void BlockSum_8x4_slice_AVX2(BYTE *pSrc, BYTE *pDstU, BYTE *pDstV, int width2, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);

    assert((width2/2 & 0xf) == 0);

    __FS_ALIGN32 unsigned int a[8] = {0x06040200u, 0x0E0C0A08u, 0x07050301u, 0x0F0D0B09u, 0x16141210u, 0x1E1C1A18u, 0x17151311u, 0x1F1D1B19u};
    __m256i pi = _mm256_load_si256((__m256i*)a);

    for (int y = 0; y < slice_nlines_fr; y += 4) {
        int x;
        for (x = 0; x < width2-31; x += 32) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m256i zero = _mm256_setzero_si256();

            // 8 horizontal blocks at a time
            __m256i r0 = _mm256_loadu_si256((__m256i *)&ps[0*pitch]);
            __m256i r1 = _mm256_loadu_si256((__m256i *)&ps[1*pitch]);
            __m256i r2 = _mm256_loadu_si256((__m256i *)&ps[2*pitch]);
            __m256i r3 = _mm256_loadu_si256((__m256i *)&ps[3*pitch]);

            r0 = _mm256_shuffle_epi8(r0, pi);
            r1 = _mm256_shuffle_epi8(r1, pi);
            r2 = _mm256_shuffle_epi8(r2, pi);
            r3 = _mm256_shuffle_epi8(r3, pi);

            __m256i s0 = _mm256_sad_epu8(_mm256_unpacklo_epi32(r0, r1), zero);
            __m256i s1 = _mm256_sad_epu8(_mm256_unpacklo_epi32(r2, r3), zero);
            __m256i s2 = _mm256_sad_epu8(_mm256_unpackhi_epi32(r0, r1), zero);
            __m256i s3 = _mm256_sad_epu8(_mm256_unpackhi_epi32(r2, r3), zero);

            s0 = _mm256_add_epi32(s0, s1);
            s2 = _mm256_add_epi32(s2, s3);

            __m128i t0 = _mm_packs_epi32(_mm256_castsi256_si128(s0), _mm256_extractf128_si256(s0, 1));
            t0 = _mm_packs_epi32(t0, t0);
            t0 = _mm_mulhrs_epi16(t0, _mm_set1_epi16(1<<(15-4)));
            t0 = _mm_packus_epi16(t0, t0);
            *(int *)pDstU = _mm_cvtsi128_si32(t0);

            __m128i t2 = _mm_packs_epi32(_mm256_castsi256_si128(s2), _mm256_extractf128_si256(s2, 1));
            t2 = _mm_packs_epi32(t2, t2);
            t2 = _mm_mulhrs_epi16(t2, _mm_set1_epi16(1<<(15-4)));
            t2 = _mm_packus_epi16(t2, t2);
            *(int *)pDstV = _mm_cvtsi128_si32(t2);

            pDstU+= 4;
            pDstV+= 4;
        }

        __m128i pi128 = _mm_set_epi8(15, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 0);
        for (; x < width2; x += 16) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            __m128i r0 = _mm_loadu_si128((__m128i *)&ps[0*pitch]);
            __m128i r1 = _mm_loadu_si128((__m128i *)&ps[1*pitch]);
            __m128i r2 = _mm_loadu_si128((__m128i *)&ps[2*pitch]);
            __m128i r3 = _mm_loadu_si128((__m128i *)&ps[3*pitch]);

            r0 = _mm_shuffle_epi8(r0, pi128);
            r1 = _mm_shuffle_epi8(r1, pi128);
            r2 = _mm_shuffle_epi8(r2, pi128);
            r3 = _mm_shuffle_epi8(r3, pi128);

            __m128i s0 = _mm_sad_epu8(_mm_unpacklo_epi32(r0, r1), zero);
            __m128i s1 = _mm_sad_epu8(_mm_unpacklo_epi32(r2, r3), zero);
            __m128i s2 = _mm_sad_epu8(_mm_unpackhi_epi32(r0, r1), zero);
            __m128i s3 = _mm_sad_epu8(_mm_unpackhi_epi32(r2, r3), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);

            s0 = _mm_packs_epi32(s0, s2);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-4)));

            //pDstU[0] = s0.m128i_u8[0];
            //pDstU[1] = s0.m128i_u8[4];
            //pDstV[0] = s0.m128i_u8[8];
            //pDstV[1] = s0.m128i_u8[12];
            
            pDstU[0] = _mm_extract_epi8(s0, 0);
            pDstU[1] = _mm_extract_epi8(s0, 4);
            pDstV[0] = _mm_extract_epi8(s0, 8);
            pDstV[1] = _mm_extract_epi8(s0, 12);
            pDstU+= 2;
            pDstV+= 2;
        }
    }
}

static void BlockSum_8x8_slice_C(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);

    for (int y = 0; y < slice_nlines_fr; y += 8) {
        for (int x = 0; x < width; x += 8) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            uint sum = 0;

            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    sum += ps[i * pitch + j];
                }
            }
            sum = (sum + 32) >> 6;
            *pDst++ = (BYTE)sum;
        }
    }
}

static void BlockSum_8x8_slice_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    assert((width & 0xf) == 0);

    for (int y = 0; y < slice_nlines_fr; y += 8) {
        for (int x = 0; x < width; x += 16) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0*pitch]), zero);
            __m128i s1 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[1*pitch]), zero);
            __m128i s2 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[2*pitch]), zero);
            __m128i s3 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[3*pitch]), zero);
            __m128i s4 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[4*pitch]), zero);
            __m128i s5 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[5*pitch]), zero);
            __m128i s6 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[6*pitch]), zero);
            __m128i s7 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[7*pitch]), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);
            s4 = _mm_add_epi32(s4, s5);
            s6 = _mm_add_epi32(s6, s7);

            s0 = _mm_add_epi32(s0, s2);
            s4 = _mm_add_epi32(s4, s6);

            s0 = _mm_add_epi32(s0, s4);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-6)));

            pDst[0] = _mm_extract_epi8(s0, 0);
            pDst[1] = _mm_extract_epi8(s0, 8);
            pDst += 2;
        }
    }
}

static void BlockSum_8x8_slice_AVX2(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    assert((width & 0xf) == 0);

    for (int y = 0; y < slice_nlines_fr; y += 8) {
        int x;
        for (x = 0; x < width-31; x += 32) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m256i zero = _mm256_setzero_si256();

            // 4 horizontal blocks at a time
            __m256i s0 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[0*pitch]), zero);
            __m256i s1 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[1*pitch]), zero);
            __m256i s2 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[2*pitch]), zero);
            __m256i s3 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[3*pitch]), zero);
            __m256i s4 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[4*pitch]), zero);
            __m256i s5 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[5*pitch]), zero);
            __m256i s6 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[6*pitch]), zero);
            __m256i s7 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[7*pitch]), zero);

            s0 = _mm256_add_epi32(s0, s1);
            s2 = _mm256_add_epi32(s2, s3);
            s4 = _mm256_add_epi32(s4, s5);
            s6 = _mm256_add_epi32(s6, s7);

            s0 = _mm256_add_epi32(s0, s2);
            s4 = _mm256_add_epi32(s4, s6);

            s0 = _mm256_add_epi32(s0, s4);
            s0 = _mm256_mulhrs_epi16(s0, _mm256_set1_epi16(1<<(15-6)));

            pDst[0] = _mm_extract_epi8(_mm256_castsi256_si128(s0), 0);
            pDst[1] = _mm_extract_epi8(_mm256_castsi256_si128(s0), 8);
            pDst[2] = _mm_extract_epi8(_mm256_extractf128_si256(s0, 1), 0);
            pDst[3] = _mm_extract_epi8(_mm256_extractf128_si256(s0, 1), 8);
            pDst += 4;
        }

        for (; x < width; x += 16) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            __m128i zero = _mm_setzero_si128();

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0*pitch]), zero);
            __m128i s1 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[1*pitch]), zero);
            __m128i s2 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[2*pitch]), zero);
            __m128i s3 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[3*pitch]), zero);
            __m128i s4 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[4*pitch]), zero);
            __m128i s5 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[5*pitch]), zero);
            __m128i s6 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[6*pitch]), zero);
            __m128i s7 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[7*pitch]), zero);

            s0 = _mm_add_epi32(s0, s1);
            s2 = _mm_add_epi32(s2, s3);
            s4 = _mm_add_epi32(s4, s5);
            s6 = _mm_add_epi32(s6, s7);

            s0 = _mm_add_epi32(s0, s2);
            s4 = _mm_add_epi32(s4, s6);

            s0 = _mm_add_epi32(s0, s4);
            s0 = _mm_mulhrs_epi16(s0, _mm_set1_epi16(1<<(15-6)));

            pDst[0] = _mm_extract_epi8(s0, 0);
            pDst[1] = _mm_extract_epi8(s0, 8);
            pDst += 2;
        }
    }
}

// Fast version computes 1x8 SAD per 8x8 block
static uint SAD_8x8fast_C(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch)
{
    uint sum = 0;

    for (int y = 0; y < height; y += 8) {
        for (int x = 0; x < width; x += 8) {

            BYTE *ps = pSrc + (y * pitch) + x;
            BYTE *pr = pRef + (y * pitch) + x;

            uint s = 0;
            for (int j = 0; j < 8; j++) {
                s += abs(ps[j] - pr[j]);
            }

            sum += s;
            *pDst++ = MIN(s, 255);
        }
    }
    return sum;
}

static uint SADNull_8x8fast_C(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    uint sum = 0;

    for (int y = 0; y < height; y += 8) {
        for (int x = 0; x < width; x += 8) {

            BYTE *ps = pSrc + (y * pitch) + x;

            uint s = 0;
            for (int j = 0; j < 8; j++) {
                s += abs(ps[j]);
            }

            sum += s;
            *pDst++ = MIN(s, 255);
        }
    }
    return sum;
}

// Fast version computes 1x8 SAD per 8x8 block
static uint SAD_8x8fast_SSE4(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch)
{
    __m128i sum = _mm_setzero_si128();

    assert((width & 0xf) == 0);

    for (int y = 0; y < height; y += 8) {
        int x;
        for (x = 0; x < width-127; x += 128) {

            BYTE *ps = pSrc + (y * pitch) + x;
            BYTE *pr = pRef + (y * pitch) + x;

            // 16 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[  0]), _mm_loadu_si128((__m128i *)&pr[  0]));
            __m128i s1 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 16]), _mm_loadu_si128((__m128i *)&pr[ 16]));
            __m128i s2 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 32]), _mm_loadu_si128((__m128i *)&pr[ 32]));
            __m128i s3 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 48]), _mm_loadu_si128((__m128i *)&pr[ 48]));
            __m128i s4 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 64]), _mm_loadu_si128((__m128i *)&pr[ 64]));
            __m128i s5 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 80]), _mm_loadu_si128((__m128i *)&pr[ 80]));
            __m128i s6 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 96]), _mm_loadu_si128((__m128i *)&pr[ 96]));
            __m128i s7 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[112]), _mm_loadu_si128((__m128i *)&pr[112]));

            s0 = _mm_packs_epi32(s0, s1);
            s2 = _mm_packs_epi32(s2, s3);
            s4 = _mm_packs_epi32(s4, s5);
            s6 = _mm_packs_epi32(s6, s7);

            sum = _mm_add_epi32(sum, _mm_add_epi32(s0, s2));    // 4 x 32-bit partial sums
            sum = _mm_add_epi32(sum, _mm_add_epi32(s4, s6));

            s0 = _mm_packs_epi32(s0, s2);    
            s4 = _mm_packs_epi32(s4, s6);    
            s0 = _mm_packus_epi16(s0, s4);  // MIN(SAD, 255)

            _mm_storeu_si128((__m128i *)pDst, s0);  // 16 x 8-bit
            pDst += 16;
        }

        for (; x < width; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            BYTE *pr = pRef + (y * pitch) + x;

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0]), _mm_loadu_si128((__m128i *)&pr[0]));

            sum = _mm_add_epi32(sum, s0);   // 2 x 32-bit partial sums

            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packus_epi16(s0, s0);  // MIN(SAD, 255)

            *(short *)pDst = _mm_extract_epi16(s0, 0);  // 2 x 8-bit
            pDst += 2;
        }
    }

    // horizontal sum
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi64(sum, sum));
    sum = _mm_add_epi32(sum, _mm_srli_epi64(sum, 32));

    return _mm_cvtsi128_si32(sum);
}

static uint SADNull_8x8fast_SSE4(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    __m128i sum = _mm_setzero_si128();
    __m128i pr = _mm_setzero_si128();

    assert((width & 0xf) == 0);

    for (int y = 0; y < height; y += 8) {
        int x;
        for (x = 0; x < width-127; x += 128) {

            BYTE *ps = pSrc + (y * pitch) + x;

            // 16 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[  0]), pr);
            __m128i s1 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 16]), pr);
            __m128i s2 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 32]), pr);
            __m128i s3 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 48]), pr);
            __m128i s4 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 64]), pr);
            __m128i s5 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 80]), pr);
            __m128i s6 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 96]), pr);
            __m128i s7 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[112]), pr);

            s0 = _mm_packs_epi32(s0, s1);
            s2 = _mm_packs_epi32(s2, s3);
            s4 = _mm_packs_epi32(s4, s5);
            s6 = _mm_packs_epi32(s6, s7);

            sum = _mm_add_epi32(sum, _mm_add_epi32(s0, s2));    // 4 x 32-bit partial sums
            sum = _mm_add_epi32(sum, _mm_add_epi32(s4, s6));

            s0 = _mm_packs_epi32(s0, s2);    
            s4 = _mm_packs_epi32(s4, s6);    
            s0 = _mm_packus_epi16(s0, s4);  // MIN(SAD, 255)

            _mm_storeu_si128((__m128i *)pDst, s0);  // 16 x 8-bit
            pDst += 16;
        }

        for (; x < width; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0]), pr);

            sum = _mm_add_epi32(sum, s0);   // 2 x 32-bit partial sums

            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packus_epi16(s0, s0);  // MIN(SAD, 255)

            *(short *)pDst = _mm_extract_epi16(s0, 0);  // 2 x 8-bit
            pDst += 2;
        }
    }

    // horizontal sum
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi64(sum, sum));
    sum = _mm_add_epi32(sum, _mm_srli_epi64(sum, 32));

    return _mm_cvtsi128_si32(sum);
}

// Fast version computes 1x8 SAD per 8x8 block
static uint SAD_8x8fast_AVX2(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch)
{
    __m256i sum = _mm256_setzero_si256();

    assert((width & 0xf) == 0);

    for (int y = 0; y < height; y += 8) {
        int x;
        for (x = 0; x < width-127; x += 128) {

            BYTE *ps = pSrc + (y * pitch) + x;
            BYTE *pr = pRef + (y * pitch) + x;

            // 16 horizontal blocks at a time
            __m256i s0 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[ 0]), _mm256_loadu_si256((__m256i *)&pr[ 0]));
            __m256i s1 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[32]), _mm256_loadu_si256((__m256i *)&pr[32]));
            __m256i s2 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[64]), _mm256_loadu_si256((__m256i *)&pr[64]));
            __m256i s3 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[96]), _mm256_loadu_si256((__m256i *)&pr[96]));

            s0 = _mm256_packs_epi32(s0, s1);
            s2 = _mm256_packs_epi32(s2, s3);

            sum = _mm256_add_epi32(sum, _mm256_add_epi32(s0, s2));  // 8 x 32-bit partial sums

            s0 = _mm256_packs_epi32(s0, s2);
            s0 = _mm256_packus_epi16(s0, s0);   // MIN(SAD, 255)

            __m128i t0 = _mm_unpacklo_epi16(_mm256_castsi256_si128(s0),  _mm256_extractf128_si256(s0, 1));
            _mm_storeu_si128((__m128i *)pDst, t0);  // 16 x 8-bit
            pDst += 16;
        }
        for (; x < width; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            BYTE *pr = pRef + (y * pitch) + x;

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0]), _mm_loadu_si128((__m128i *)&pr[0]));

            sum = _mm256_add_epi32(sum, _mm256_castsi128_si256(s0));    // 2 x 32-bit partial sums

            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packus_epi16(s0, s0);  // MIN(SAD, 255)

            *(short *)pDst = _mm_extract_epi16(s0, 0);  // 2 x 8-bit
            pDst += 2;
        }
    }

    // horizontal sum
    __m128i t0 = _mm256_castsi256_si128(sum);
    t0 = _mm_add_epi32(t0, _mm256_extractf128_si256(sum, 1));
    t0 = _mm_add_epi32(t0, _mm_unpackhi_epi64(t0, t0));
    t0 = _mm_add_epi32(t0, _mm_srli_epi64(t0, 32));

    return _mm_cvtsi128_si32(t0);
}

// Fast version computes 1x8 SAD per 8x8 block
static uint SADNull_8x8fast_AVX2(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    __m256i sum = _mm256_setzero_si256();
    __m256i pr = _mm256_setzero_si256();
    assert((width & 0xf) == 0);

    for (int y = 0; y < height; y += 8) {
        int x;
        for (x = 0; x < width-127; x += 128) {

            BYTE *ps = pSrc + (y * pitch) + x;

            // 16 horizontal blocks at a time
            __m256i s0 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[ 0]), pr);
            __m256i s1 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[32]), pr);
            __m256i s2 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[64]), pr);
            __m256i s3 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[96]), pr);

            s0 = _mm256_packs_epi32(s0, s1);
            s2 = _mm256_packs_epi32(s2, s3);

            sum = _mm256_add_epi32(sum, _mm256_add_epi32(s0, s2));  // 8 x 32-bit partial sums

            s0 = _mm256_packs_epi32(s0, s2);
            s0 = _mm256_packus_epi16(s0, s0);   // MIN(SAD, 255)

            __m128i t0 = _mm_unpacklo_epi16(_mm256_castsi256_si128(s0),  _mm256_extractf128_si256(s0, 1));
            _mm_storeu_si128((__m128i *)pDst, t0);  // 16 x 8-bit
            pDst += 16;
        }
        for (; x < width; x += 16) {

            BYTE *ps = pSrc + (y * pitch) + x;
            __m128i pr1 = _mm_setzero_si128();
            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0]), pr1);

            sum = _mm256_add_epi32(sum, _mm256_castsi128_si256(s0));    // 2 x 32-bit partial sums

            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packus_epi16(s0, s0);  // MIN(SAD, 255)

            *(short *)pDst = _mm_extract_epi16(s0, 0);  // 2 x 8-bit
            pDst += 2;
        }
    }

    // horizontal sum
    __m128i t0 = _mm256_castsi256_si128(sum);
    t0 = _mm_add_epi32(t0, _mm256_extractf128_si256(sum, 1));
    t0 = _mm_add_epi32(t0, _mm_unpackhi_epi64(t0, t0));
    t0 = _mm_add_epi32(t0, _mm_srli_epi64(t0, 32));

    return _mm_cvtsi128_si32(t0);
}

// Fast version computes 1x8 SAD per 8x8 block (slice-based)
static uint SAD_8x8fast_slice_C(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    uint sum = 0;
    for (int y = 0; y < slice_nlines_fr; y += 8) {
        for (int x = 0; x < width; x += 8) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            BYTE *pr = pRef + slice_offset_fr + (y * pitch) + x;

            uint s = 0;
            for (int j = 0; j < 8; j++) {
                s += abs(ps[j] - pr[j]);
            }

            sum += s;
            *pDst++ = MIN(s, 255);
        }
    }
    return sum;
}

// Fast version computes 1x8 SAD per 8x8 block
static uint SAD_8x8fast_slice_SSE4(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    __m128i sum = _mm_setzero_si128();

    assert((width & 0xf) == 0);

    for (int y = 0; y < slice_nlines_fr; y += 8) {
        int x;
        for (x = 0; x < width-127; x += 128) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            BYTE *pr = pRef + slice_offset_fr + (y * pitch) + x;

            // 16 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[  0]), _mm_loadu_si128((__m128i *)&pr[  0]));
            __m128i s1 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 16]), _mm_loadu_si128((__m128i *)&pr[ 16]));
            __m128i s2 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 32]), _mm_loadu_si128((__m128i *)&pr[ 32]));
            __m128i s3 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 48]), _mm_loadu_si128((__m128i *)&pr[ 48]));
            __m128i s4 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 64]), _mm_loadu_si128((__m128i *)&pr[ 64]));
            __m128i s5 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 80]), _mm_loadu_si128((__m128i *)&pr[ 80]));
            __m128i s6 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[ 96]), _mm_loadu_si128((__m128i *)&pr[ 96]));
            __m128i s7 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[112]), _mm_loadu_si128((__m128i *)&pr[112]));

            s0 = _mm_packs_epi32(s0, s1);
            s2 = _mm_packs_epi32(s2, s3);
            s4 = _mm_packs_epi32(s4, s5);
            s6 = _mm_packs_epi32(s6, s7);

            sum = _mm_add_epi32(sum, _mm_add_epi32(s0, s2));    // 4 x 32-bit partial sums
            sum = _mm_add_epi32(sum, _mm_add_epi32(s4, s6));

            s0 = _mm_packs_epi32(s0, s2);    
            s4 = _mm_packs_epi32(s4, s6);    
            s0 = _mm_packus_epi16(s0, s4);  // MIN(SAD, 255)

            _mm_storeu_si128((__m128i *)pDst, s0);  // 16 x 8-bit
            pDst += 16;
        }

        for (; x < width; x += 16) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            BYTE *pr = pRef + slice_offset_fr + (y * pitch) + x;

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0]), _mm_loadu_si128((__m128i *)&pr[0]));

            sum = _mm_add_epi32(sum, s0);   // 2 x 32-bit partial sums

            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packus_epi16(s0, s0);  // MIN(SAD, 255)

            *(short *)pDst = _mm_extract_epi16(s0, 0);  // 2 x 8-bit
            pDst += 2;
        }
    }

    // horizontal sum
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi64(sum, sum));
    sum = _mm_add_epi32(sum, _mm_srli_epi64(sum, 32));

    return _mm_cvtsi128_si32(sum);
}

// Fast version computes 1x8 SAD per 8x8 block
static uint SAD_8x8fast_slice_AVX2(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(height);
    __m256i sum = _mm256_setzero_si256();

    assert((width & 0xf) == 0);

    for (int y = 0; y < slice_nlines_fr; y += 8) {
        int x;
        for (x = 0; x < width-127; x += 128) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            BYTE *pr = pRef + slice_offset_fr + (y * pitch) + x;

            // 16 horizontal blocks at a time
            __m256i s0 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[ 0]), _mm256_loadu_si256((__m256i *)&pr[ 0]));
            __m256i s1 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[32]), _mm256_loadu_si256((__m256i *)&pr[32]));
            __m256i s2 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[64]), _mm256_loadu_si256((__m256i *)&pr[64]));
            __m256i s3 = _mm256_sad_epu8(_mm256_loadu_si256((__m256i *)&ps[96]), _mm256_loadu_si256((__m256i *)&pr[96]));

            s0 = _mm256_packs_epi32(s0, s1);
            s2 = _mm256_packs_epi32(s2, s3);

            sum = _mm256_add_epi32(sum, _mm256_add_epi32(s0, s2));  // 8 x 32-bit partial sums

            s0 = _mm256_packs_epi32(s0, s2);
            s0 = _mm256_packus_epi16(s0, s0);   // MIN(SAD, 255)

            __m128i t0 = _mm_unpacklo_epi16(_mm256_castsi256_si128(s0),  _mm256_extractf128_si256(s0, 1));
            _mm_storeu_si128((__m128i *)pDst, t0);  // 16 x 8-bit
            pDst += 16;
        }
        for (; x < width; x += 16) {

            BYTE *ps = pSrc + slice_offset_fr + (y * pitch) + x;
            BYTE *pr = pRef + slice_offset_fr + (y * pitch) + x;

            // 2 horizontal blocks at a time
            __m128i s0 = _mm_sad_epu8(_mm_loadu_si128((__m128i *)&ps[0]), _mm_loadu_si128((__m128i *)&pr[0]));

            sum = _mm256_add_epi32(sum, _mm256_castsi128_si256(s0));    // 2 x 32-bit partial sums

            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packs_epi32(s0, s0);    
            s0 = _mm_packus_epi16(s0, s0);  // MIN(SAD, 255)

            *(short *)pDst = _mm_extract_epi16(s0, 0);  // 2 x 8-bit
            pDst += 2;
        }
    }

    // horizontal sum
    __m128i t0 = _mm256_castsi256_si128(sum);
    t0 = _mm_add_epi32(t0, _mm256_extractf128_si256(sum, 1));
    t0 = _mm_add_epi32(t0, _mm_unpackhi_epi64(t0, t0));
    t0 = _mm_add_epi32(t0, _mm_srli_epi64(t0, 32));

    return _mm_cvtsi128_si32(t0);
}

// Compute small histogram of rscs-like measure
static void RsCs_8x8_hist_C(BYTE *pSrc, int srcPitch, BYTE *pMask, int maskPitch, int min_x, int max_x, int min_y, int max_y, int hist[4])
{
    hist[0] = 0; hist[1] = 0;
    hist[2] = 0; hist[3] = 0;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {

            BYTE *ps = pSrc + (8*y * srcPitch) + 8*x;

            if (pMask[y*maskPitch + x] == 255) {

                // compute 8x8 Rs, Cs
                int rs = 0;
                int cs = 0;
                for (int k = 1; k < 8; k++) {
                    for (int l = 1; l < 8; l++) {
                        rs += ABS(ps[1*srcPitch + (l-1)] - ps[1*srcPitch + (l+0)]);
                        cs += ABS(ps[0*srcPitch + (l+0)] - ps[1*srcPitch + (l+0)]);
                    }
                    ps += srcPitch;
                }

                // update histogram
                int s = MAX(rs, cs);
                if		(s <  100)	hist[0]++;
                else if (s <  400)	hist[1]++;
                else if (s <  800)	hist[2]++;
                else				hist[3]++;
            }
        }
    }
}

#define _mm_loadh_epi64(a, ptr) _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(a), (__m64 *)(ptr)))
#define _mm_movehl_epi64(a, b) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b)))

// Compute small histogram of rscs-like measure
static void RsCs_8x8_hist_SSE4(BYTE *pSrc, int srcPitch, BYTE *pMask, int maskPitch, int min_x, int max_x, int min_y, int max_y, int hist[4])
{
    __m128i hvec = _mm_setzero_si128();
    __m128i zero = _mm_setzero_si128();

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {

            BYTE *ps = pSrc + (8*y * srcPitch) + 8*x;

            if (pMask[y*maskPitch + x] == 255) {

                // load 8x8 block, 2 rows per register
                __m128i s0 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&ps[0*srcPitch]), (__m128i *)&ps[4*srcPitch]);
                __m128i s1 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&ps[1*srcPitch]), (__m128i *)&ps[5*srcPitch]);
                __m128i s2 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&ps[2*srcPitch]), (__m128i *)&ps[6*srcPitch]);
                __m128i s3 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&ps[3*srcPitch]), (__m128i *)&ps[7*srcPitch]);
                __m128i s4 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&ps[4*srcPitch]), (__m128i *)&ps[7*srcPitch]);
                ps += srcPitch;

                // shifted rows
                __m128i r0 = _mm_srli_epi64(s0, 8);
                __m128i r1 = _mm_srli_epi64(s1, 8);
                __m128i r2 = _mm_srli_epi64(s2, 8);
                __m128i r3 = _mm_srli_epi64(s3, 8);
                __m128i r4 = _mm_srli_epi64(s4, 8);

                //
                // compute Cs
                //
                __m128i c0 = _mm_absdiff_epu8(r0, r1);
                __m128i c1 = _mm_absdiff_epu8(r1, r2);
                __m128i c2 = _mm_absdiff_epu8(r2, r3);
                __m128i c3 = _mm_absdiff_epu8(r3, r4);

                c0 = _mm_sad_epu8(c0, zero);
                c1 = _mm_sad_epu8(c1, zero);
                c2 = _mm_sad_epu8(c2, zero);
                c3 = _mm_sad_epu8(c3, zero);

                c0 = _mm_add_epi32(c0, c1);
                c2 = _mm_add_epi32(c2, c3);
                c0 = _mm_add_epi32(c0, c2);
                c2 = _mm_movehl_epi64(c2, c0);
                c0 = _mm_add_epi32(c0, c2);

                //
                // compute Rs
                //
                r0 = _mm_absdiff_epu8(r0, s0);
                r1 = _mm_absdiff_epu8(r1, s1);
                r2 = _mm_absdiff_epu8(r2, s2);
                r3 = _mm_absdiff_epu8(r3, s3);

                r0 = _mm_srli_si128(r0, 8);  // discard row(0)
                r0 = _mm_slli_epi64(r0, 8);
                r1 = _mm_slli_epi64(r1, 8);
                r2 = _mm_slli_epi64(r2, 8);
                r3 = _mm_slli_epi64(r3, 8);

                r0 = _mm_sad_epu8(r0, zero);
                r1 = _mm_sad_epu8(r1, zero);
                r2 = _mm_sad_epu8(r2, zero);
                r3 = _mm_sad_epu8(r3, zero);

                r0 = _mm_add_epi32(r0, r1);
                r2 = _mm_add_epi32(r2, r3);
                r0 = _mm_add_epi32(r0, r2);
                r2 = _mm_movehl_epi64(r2, r0);
                r0 = _mm_add_epi32(r0, r2);

                //
                // update histogram
                //
                r0 = _mm_max_epi32(c0, r0);
                r0 = _mm_shuffle_epi32(r0, _MM_SHUFFLE(0,0,0,0));
                r0 = _mm_cmplt_epi32(r0, _mm_setr_epi32(100, 400, 800, 0x7fffffff));
                hvec = _mm_sub_epi32(hvec, r0);
            }
        }
    }
    _mm_storeu_si128((__m128i *)hist, hvec);

    // undo cumulative counts, by differencing
    hist[3] -= hist[2];
    hist[2] -= hist[1];
    hist[1] -= hist[0];
}


typedef void(*t_BlockSum_4x4)(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch);
typedef void(*t_BlockSum_8x4)(BYTE *pSrc, BYTE *pDst, BYTE *pDst2, int width, int height, int pitch);
typedef void(*t_BlockSum_8x8)(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch);
typedef uint(*t_SAD_8x8)(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch);
typedef uint(*t_SADNull_8x8)(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch);

typedef void(*t_BlockSum_4x4_slice)(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr);
typedef void(*t_BlockSum_8x4_slice)(BYTE *pSrc, BYTE *pDst, BYTE *pDst2, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr);
typedef void(*t_BlockSum_8x8_slice)(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr);
typedef uint(*t_SAD_8x8_slice)(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines);

typedef void(*t_Median_5x5)(const BYTE* pSrc, BYTE* pDst, int width, int height);
typedef void(*t_RsCs_8x8_hist)(BYTE *pSrc, int srcPitch, BYTE *pMask, int maskPitch, int min_x, int max_x, int min_y, int max_y, int hist[4]);


static void BlockSum_4x4(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    t_BlockSum_4x4 f = g_FS_OPT_AVX2 ? BlockSum_4x4_AVX2 : (g_FS_OPT_SSE4 ? BlockSum_4x4_SSE4 : BlockSum_4x4_C);
    (*f)(pSrc, pDst, width, height, pitch);
}

static void BlockSum_8x4(BYTE *pSrc, BYTE *pDst, BYTE *pDst2, int width, int height, int pitch)
{
    t_BlockSum_8x4 f = g_FS_OPT_AVX2 ? BlockSum_8x4_AVX2 : (g_FS_OPT_SSE4 ? BlockSum_8x4_SSE4 : BlockSum_8x4_C);
    (*f)(pSrc, pDst, pDst2, width, height, pitch);
}

static void BlockSum_8x8(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    t_BlockSum_8x8 f = g_FS_OPT_AVX2 ? BlockSum_8x8_AVX2 : (g_FS_OPT_SSE4 ? BlockSum_8x8_SSE4 : BlockSum_8x8_C);
    (*f)(pSrc, pDst, width, height, pitch);
}

static uint SAD_8x8fast(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch)
{
    t_SAD_8x8 f = g_FS_OPT_AVX2 ? SAD_8x8fast_AVX2 : (g_FS_OPT_SSE4 ? SAD_8x8fast_SSE4 : SAD_8x8fast_C);
    return (*f)(pSrc, pRef, pDst, width, height, pitch);
}

static uint SADNull_8x8fast(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch)
{
    t_SADNull_8x8 f = g_FS_OPT_AVX2 ? SADNull_8x8fast_AVX2 : (g_FS_OPT_SSE4 ? SADNull_8x8fast_SSE4 : SADNull_8x8fast_C);
    return (*f)(pSrc, pDst, width, height, pitch);
}


static void BlockSum_4x4_slice(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    t_BlockSum_4x4_slice f = g_FS_OPT_AVX2 ? BlockSum_4x4_slice_AVX2 : (g_FS_OPT_SSE4 ? BlockSum_4x4_slice_SSE4 : BlockSum_4x4_slice_C);
    (*f)(pSrc, pDst, width, height, pitch, slice_offset_fr, slice_nlines_fr);
}

static void BlockSum_8x4_slice(BYTE *pSrc, BYTE *pDst, BYTE *pDst2, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{	
    t_BlockSum_8x4_slice f = g_FS_OPT_AVX2 ? BlockSum_8x4_slice_AVX2 : (g_FS_OPT_SSE4 ? BlockSum_8x4_slice_SSE4 : BlockSum_8x4_slice_C);
    (*f)(pSrc, pDst, pDst2, width, height, pitch, slice_offset_fr, slice_nlines_fr);
}

static void BlockSum_8x8_slice(BYTE *pSrc, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    t_BlockSum_8x8_slice f = g_FS_OPT_AVX2 ? BlockSum_8x8_slice_AVX2 : (g_FS_OPT_SSE4 ? BlockSum_8x8_slice_SSE4 : BlockSum_8x8_slice_C);
    (*f)(pSrc, pDst, width, height, pitch, slice_offset_fr, slice_nlines_fr);
}

static uint SAD_8x8fast_slice(BYTE *pSrc, BYTE *pRef, BYTE *pDst, int width, int height, int pitch, uint slice_offset_fr, int slice_nlines_fr)
{
    t_SAD_8x8_slice f = g_FS_OPT_AVX2 ? SAD_8x8fast_slice_AVX2 : (g_FS_OPT_SSE4 ? SAD_8x8fast_slice_SSE4 : SAD_8x8fast_slice_C);
    return (*f)(pSrc, pRef, pDst, width, height, pitch, slice_offset_fr, slice_nlines_fr);
}

static void Median_5x5(const BYTE* pSrc, BYTE* pDst, int width, int height)
{
    t_Median_5x5 f = g_FS_OPT_SSE4 ? FastMedian_5x5_SSE4 : FastMedian_5x5_C;
    (*f)(pSrc, pDst, width, height);
}

static void RsCs_8x8_hist(BYTE *pSrc, int srcPitch, BYTE *pMask, int maskPitch, int min_x, int max_x, int min_y, int max_y, int hist[4])
{
    t_RsCs_8x8_hist f = g_FS_OPT_SSE4 ? RsCs_8x8_hist_SSE4 : RsCs_8x8_hist_C;
    (*f)(pSrc, srcPitch, pMask, maskPitch, min_x, max_x, min_y, max_y, hist);
}

//
// end Dispatcher
//

//subsample Yuv from pixel to block accuracy
void SubsampleYUV12(BYTE* pSrc, BYTE* pDst, int width, int height, int pitch, int blksz) {
    BlockSum_8x8(pSrc, pDst, width, height, pitch);
    BlockSum_4x4(pSrc + (width*height), pDst + (width/blksz)*(height/blksz), width/2, height/2, pitch/2);
    BlockSum_4x4(pSrc + (width*height) + (width/2)*(height/2), pDst + 2*(width/blksz)*(height/blksz), width/2, height/2, pitch/2);
}

//subsample NV12 from pixel to block accuracy
void SubsampleNV12(BYTE* pYSrc, BYTE* pUVSrc, BYTE* pDst, int width, int height, int pitch, int blksz) {
    BlockSum_8x8(pYSrc, pDst, width, height, pitch);
    BlockSum_8x4(pUVSrc, pDst + (width/blksz)*(height/blksz), pDst + 2*(width/blksz)*(height/blksz), width, height/2, pitch);
}

//subsample Yuv from pixel to block accuracy (slice-based)
void SubsampleYUV12_slice(BYTE* pSrc, BYTE* pDst, int width, int height, int pitch, int blksz, uint slice_offset, uint slice_offset_fr, int slice_nlines_fr) {
    BlockSum_8x8_slice(pSrc, pDst + slice_offset, width, height, pitch, slice_offset_fr, slice_nlines_fr);
    BlockSum_4x4_slice(pSrc + (width*height), pDst + (width/blksz)*(height/blksz) + slice_offset, width/2, height/2, pitch/2, slice_offset_fr/4, slice_nlines_fr/2);
    BlockSum_4x4_slice(pSrc + (width*height) + (width/2)*(height/2), pDst + 2*(width/blksz)*(height/blksz) + slice_offset, width/2, height/2, pitch/2, slice_offset_fr/4, slice_nlines_fr/2);
}

//subsample NV12 from pixel to block accuracy (slice-based)
void SubsampleNV12_slice(BYTE* pYSrc, BYTE* pUVSrc, BYTE* pDst, int width, int height, int pitch, int pitchC, int blksz, SliceInfoNv12 *si, int sliceId) {
    FS_UNREFERENCED_PARAMETER(blksz);
    BlockSum_8x8_slice(pYSrc, pDst + si->blockOffset[sliceId], width, height, pitch, si->sampleOffset[sliceId], si->numSampleRows[sliceId]);
    BlockSum_8x4_slice(pUVSrc, pDst + si->picSizeInBlocks + si->blockOffset[sliceId], pDst + 2 * si->picSizeInBlocks + si->blockOffset[sliceId], width, height/2, pitchC, si->sampleOffsetC[sliceId], si->numSampleRowsC[sliceId]);
}

//subsample NV12 from pixel to block accuracy (slice-based)
void SubsampleLuma_slice(BYTE* pYSrc, BYTE* pDst, int width, int height, int pitch, int blksz, SliceInfoNv12 *si, int sliceId) {
    FS_UNREFERENCED_PARAMETER(blksz);
    BlockSum_8x8_slice(pYSrc, pDst + si->blockOffset[sliceId], width, height, pitch, si->sampleOffset[sliceId], si->numSampleRows[sliceId]);
}


//calculates motion between first and specified frames of a frame stack
uint CalculateMotion(FrameBuffElementPtr *fsc, FrameBuffElementPtr *fsp, BYTE *maMaskB, uint NL, Dim *dim)
{
    FS_UNREFERENCED_PARAMETER(NL);
    FS_UNREFERENCED_PARAMETER(dim);

    BYTE* fF = fsc->frameY;         //first  frame
    BYTE* sF = fsp->frameY;         //second frame

    if(!fF) return 0;
    if(!sF) {
        // HACK 
        return SADNull_8x8fast(fF, maMaskB, fsc->w, fsc->h, fsc->p);
    }
    assert(NL == 0);
    assert(dim->blSz == 8);

    return SAD_8x8fast(fF, sF, maMaskB, fsc->w, fsc->h, fsc->p);
}

//calculates motion between first and specified frames of a frame stack (slice-based)
uint CalculateMotion_slice(FrameBuffElementPtr *fsc, FrameBuffElementPtr *fsp, BYTE *maMaskB, uint NL, Dim *dim, uint slice_offset, uint slice_offset_fr, int slice_nlines_fr)
{
    FS_UNREFERENCED_PARAMETER(NL);
    FS_UNREFERENCED_PARAMETER(dim);
    BYTE* fF = fsc->frameY;  //first  frame
    BYTE* sF = fsp->frameY;  //second frame

    assert(NL == 0);
    assert(dim->blSz == 8);

    return SAD_8x8fast_slice(fF, sF, maMaskB + slice_offset, fsc->w, fsc->h, fsc->p, slice_offset_fr, slice_nlines_fr);
}

//smoothing motion probabilities and calculates overall motion
static void SmothMotionMask(BYTE* overal, BYTE* cur, BYTE* sadMax, FrameBuffElementPtr** fs, Dim* dim) 
{
    uint i, k;

    for (i=0; i < dim->blTotal; i++) {
        sadMax[i] = fs[0]->bSAD[i];
        for (k = 1; k < dim->numFr - 1; k++) {
            if (sadMax[i] < fs[k]->bSAD[i]) {
                sadMax[i] = fs[k]->bSAD[i];
            }
        }
    }

    for (i=0; i<dim->blTotal; i++) {
        overal[i] = MIN(MAX(cur[i] - sadMax[i], 0) + fs[0]->bSAD[i] * 2, 255);
    }
}

//mixing of motion and skin probabilities masks
static void MixMotionSkinMask(BYTE* dst, BYTE* motion, BYTE* skin, BYTE* tmp1, BYTE* tmp2, 
                       BYTE* sadMax, BYTE* prvProb, uint maxSegSize, 
                       uint averSAD, Dim* dim) 
{
    uint i;
    uint sTh;
    uint hist[256];

    uint segSz = maxSegSize < 20 ? 0 : (maxSegSize > 52 ? 32 : maxSegSize - 20);  // segSz 0..32
    BYTE bottomTh = 17 - segSz / 2; // 1..17
    BYTE skinPerc = 28 + segSz * 2; //28..92

    averSAD = averSAD < 800 ? 1024 : (averSAD > 1200 ? 0 : (1200 - averSAD) * 1024 / 400);

    //skin hystogram thresholding
    {
        uint k;
        uint sum   = 0;
        uint sadTh = dim->blTotal * skinPerc / 100;//40

        memset(hist, 0, 256*sizeof(uint));
        k = 255;

        for (i = 0; i < dim->blTotal; i++)
        {
            hist[skin[i]]++;
        }

        while(sum < sadTh && k > bottomTh)//15
        {
            sum += hist[k];
            k--;
        }
        sTh = k;
    }

    for (i = 0; i < dim->blTotal; i++) {
        uint s = 0;
        uint m = 0;
        if (skin[i] > sTh) s = skin[i];
        m = motion[i];
        dst[i] = (BYTE)(MIN(s, m));
        tmp1[i] = (s > m && prvProb[i] > 30) ? MIN((s - m), 32) : 0;
        tmp2[i] = 32 - tmp1[i]/2;
    }

    //mix prob results with previous
    for (i = 0; i < dim->blTotal; i++) {
        uint c = MAX((int)sadMax[i] * tmp2[i] / 32 - tmp1[i], 0);
        c = MIN(c, 200);
        c = (sadMax[i] * (1024 - averSAD) + c * averSAD) / 1024;
        dst[i] = (dst[i] * c + prvProb[i] * (256 - c)) / 256;
    }

    Close_Value_byte(dst, tmp2, dim->blHNum, dim->blVNum, Morphology_GetWindowMask(2, 1));
    Open_Value_byte(dst, tmp2, dim->blHNum, dim->blVNum, Morphology_GetWindowMask(2, 1));

    for (i = 0; i < dim->blTotal; i++) {
        if (prvProb[i] < 30 &&  dst[i] >= 30) {
            dst[i] = MIN(dst[i] + 150, 255);
        }
    }

    memcpy(prvProb, dst, sizeof(BYTE) * dim->blTotal);
}

//performs face detection
static void DetectFaces(FrameBuffElementPtr* frm, int w, int h, FDet *fd) 
{
    int i, j;
    int bsf = get_bsz_fd(w, h);
    if (bsf > 1) {
        for (i=0; i<h; i+=bsf) {
            for (j=0; j<w; j+=bsf) {
                fd->mat[(i/bsf)*(w/bsf) + (j/bsf)] = frm->frameY[i*frm->p + j];
            }
        }
    }
    else { 
        for(int i=0; i<h; i++)
            memcpy(fd->mat+i*w, frm->frameY+i*frm->p, sizeof(BYTE)*w);
    }

    //equalize histogram
    normalize_contrast(fd->mat, fd->mat_norm, w/bsf, h/bsf);

    std::vector<Rct> &faces = *fd->faces;
    //do face detection
    do_face_detection(faces, fd->mat_norm, w/bsf, h/bsf, &fd->cascade, fd->class_cascade, fd->sum, fd->sqsum, 30, 30, 2);

    //resize detected faces
    for (size_t n=0; n<faces.size(); n++) {	
        faces[n].x*= bsf;
        faces[n].y*= bsf;
        faces[n].w*= bsf;
        faces[n].h*= bsf;
    }
}

//check if scene has low illumination
static int is_low_illumination(BYTE *y, int w, int h) 
{
    int i, a = 0, T_li = 64;
    for (i=0; i<w*h; i++) if (y[i] < T_li) a++;
    if ((a*100)/(w*h) > 60) return 1; //60% or more of frame has low luminance value
    else return 0;
}

//initialize skin map based on faces
static int init_New_Skin_Map_from_face_mode1_NV12(FrameBuffElementPtr *in, BYTE *skinProb, int w2, int h2, int wb, int hb, int bs, std::vector<Rct> faces, int square[][256], BYTE hist[256][256]) 
{
    FS_UNREFERENCED_PARAMETER(hb);
    int i, j, x_start, x_end, y_start, y_end;
    int max_val, max_i, max_j, c, k;
    int ath_limits[] = {  3	,   4,   5,   6,   7,   8,   9,  10};
    int ath_values[] = {255, 224, 192, 160, 128,  96,  64,  32};
    int ath_num = 8, s[9] = {0};
    int bs2 = bs / 2;
    //int uoff = w2 * h2 * 4;

    //quit if there are no faces detected
    if (faces.size()==0) return 0;

    //init color histogram square to 0
    for (i=0; i<256; i++) {
        memset(square[i], 0, sizeof(int)*256);
    }

    //gather face skin color histogram
    for (size_t n=0; n<faces.size(); n++) {
        x_start = (faces[n].x)/2;
        y_start = (faces[n].y)/2;
        x_end = (faces[n].x + faces[n].w)/2;
        y_end = (faces[n].y + faces[n].h)/2;
        for (i=y_start; i<y_end; i++) {
            for (j=x_start; j<x_end; j++) {
                if (skinProb[(i/bs2)*wb + (j/bs2)]>30) {

                    square[ in->frameU[(i*in->pc + j*2)] ][ in->frameU[(i*in->pc + (j*2+1))] ]+= skinProb[(i/bs2)*wb + (j/bs2)];

                }
            }
        }
    }

    //find histogram peak
    max_val = 0;
    for (i=0; i<256; i++) {
        for (j=0; j<256; j++) {
            if (max_val < square[i][j]) {
                max_val = square[i][j];
                max_i = i;
                max_j = j;
            }
        }
    }

    //create adaptive skin probability color map
    for (i=0; i<256; i++) {
        for (j=0; j<256; j++) {
            c = MAX(abs(i-max_i), abs(j-max_j));
            if (c > ath_limits[ath_num-1]) square[i][j] = 0;
            else {
                for (k=ath_num-1; k>=0; k--) {
                    if (c==ath_limits[k]) {
                        square[i][j] = ath_values[k];
                        break;
                    }
                }
                if (k < 0) square[i][j] = ath_values[0];
            }
        }
    }

    //adjust the map based on skin area size
    for (i=0; i<h2; i++) {
        for (j=0; j<w2; j++) {

            s[ (square[ in->frameU[ (i*in->pc + j*2)]][in->frameU[ (i*in->pc + (j*2+1))]] + 1)/32 ]++;

        }
    }

    for (k=0; k<9; k++) {
        c = 0;
        for (j=0; j<=k; j++) c+= s[j];
        if ((c*100)/(w2*h2) >= 70) break; //30% of frame is skin area
    }
    if (k>=8) return 0; //quit if there are too many skin pixels

    //adjust LUT values for faster processing
    for (i=0; i<256; i++) {
        for (j=0; j<256; j++) {
            hist[i][j] = ((square[i][j] + 1)/32 <= k) ? 0 : square[i][j];
        }
    }

    return 1; //success
}


//use skin-region stability statistics to choose between static and dynamic skin probability LUT
static int Update_skinProbB_mode1(BYTE *frmY, BYTE *skinProbB, BYTE *skinProbNewB, int wb, int hb, int bs, std::vector<Rct> faces, int *faceskin_perc) 
{
    int i, j, x_start, x_end, y_start, y_end;
    unsigned long faceskin_perc1 = 0, tot;
    unsigned long frm_skin_perc1 = 0;
    unsigned long faceskin_perc2 = 0;
    unsigned long frm_skin_perc2 = 0;

    tot = 0;
    for (size_t n=0; n<faces.size(); n++) {
        x_start = (faces[n].x)/bs;
        y_start = (faces[n].y)/bs;
        x_end = (faces[n].x + faces[n].w)/bs;
        y_end = (faces[n].y + faces[n].h)/bs;
        for (i=y_start; i<y_end; i++) {
            for (j=x_start; j<x_end; j++) {
                if (skinProbB[i*wb + j] > 30) faceskin_perc1++;
                tot++;
            }
        }
    }
    faceskin_perc1 = (faceskin_perc1*100) / tot;

    tot = 0;
    for (i=0; i<wb*hb; i++) {
        if (skinProbB[i] > 30) frm_skin_perc1++;
        tot++;
    }
    frm_skin_perc1 = (frm_skin_perc1*100) / tot;

    tot = 0;
    for (size_t n=0; n<faces.size(); n++) {
        x_start = (faces[n].x)/bs;
        y_start = (faces[n].y)/bs;
        x_end = (faces[n].x + faces[n].w)/bs;
        y_end = (faces[n].y + faces[n].h)/bs;
        for (i=y_start; i<y_end; i++) {
            for (j=x_start; j<x_end; j++) {
                if (skinProbNewB[i*wb + j] > 30) faceskin_perc2++;
                tot++;
            }
        }
    }
    faceskin_perc2 = (faceskin_perc2*100) / tot;

    tot = 0;
    for (i=0; i<hb; i++) {
        for (j=0; j<wb; j++) {
            if (skinProbNewB[i*wb + j] > 30) frm_skin_perc2++;
            tot++;
        }
    }
    frm_skin_perc2 = (frm_skin_perc2*100) / tot;

    //for (size_t n=0; n<faces.size(); n++) {	
    //   int ccc = ((faces[n].w/bs)*(faces[n].h/bs)*100)/(wb*hb);
    //}

    //check for low illumination scenes
    if (is_low_illumination(frmY, wb, hb)) { *faceskin_perc = faceskin_perc1; return 0; }

    //choose more stable region
    int cc = (faceskin_perc1 - faceskin_perc2) - 3*(frm_skin_perc1 - frm_skin_perc2);
    if (cc <= 0) { *faceskin_perc = faceskin_perc2; return 1; }
    else         { *faceskin_perc = faceskin_perc1; return 0; }
}

//marks face regions
static void mark_face_regions(BYTE *tmp, int w, int h, int bs, std::vector<Rct> faces, BYTE *frm, BYTE *frm_2, int stack[], int& stackptr, int blTotal) 
{
    int i, j, ii, jj, sz, ind;
    int min_x, min_y, max_x, max_y;

    for (i=0; i<h; i++) {
        for (j=0; j<w; j++) {
            ind = i*w + j;
            if (tmp[ind]==255) {

                flood_fill4_sz_bb(j, i, 128, tmp[ind], w, h, tmp, &sz, &min_x, &max_x, &min_y, &max_y, stack, stackptr, blTotal);

                //search for "2"
                int col = 64, fn;
                for (size_t n=0; n<faces.size(); n++) {
                    int tot = 0, two = 0;
                    for	(ii=faces[n].y/bs; ii<(faces[n].y + faces[n].h)/bs; ii++) {
                        for (jj=faces[n].x/bs; jj<(faces[n].x + faces[n].w)/bs; jj++) {
                            if (tmp[ii*w+jj]==128) two++;
                            tot++;
                        }
                    }
                    if ((two*100 / tot) > 45) { col = 192; fn = (int)(n); break; } //condition for face region
                }

                //mark object with color "col" (64=non-face 192=face)
                for (ii=min_y; ii<=max_y; ii++) {
                    for (jj=min_x; jj<=max_x; jj++) {
                        ind = ii*w + jj;
                        if (tmp[ind]==128) {
                            tmp[ind] = col;
                            if (col==192) {
                                int skip = 0;
                                if (ii<faces[fn].y/bs - 2) skip = 1;
                                if (jj<faces[fn].x/bs - 1) skip = 1;
                                if (jj>(faces[fn].x + faces[fn].w)/bs + 1) skip = 1;
                                if (!skip) frm_2[ind] = frm[ind] = 255;
                            }
                        }
                    }
                }

            }
        }
    }
}

//marks face areas in the frame including the area around the face that is has high probability of a skin-tone
static void DrawFaces_small_smart(BYTE* frm, BYTE* frm_2, BYTE *frm_skin_prob, BYTE *tmp, int w, int h, int bs, std::vector<Rct> faces, int stack[], int& stackptr, int blTotal) 
{
    int i;
    for (i=0; i<w*h; i++) {
        if (frm_skin_prob[i] > 30) tmp[i] = 255;
        else tmp[i] = 0;
    }
    mark_face_regions(tmp, w, h, bs, faces, frm, frm_2, stack, stackptr, blTotal);
}

//fixes large holes in the skin mask
static void fix_large_holes(BYTE *mask, BYTE *prv_mask, BYTE *tmp, BYTE *frm_mot, BYTE *frm_sad, BYTE *frm_skin, int wb, int hb) 
{
    int i, c = 0, avg_mot = 0, avg_sad = 0, avg_skin = 0;
    for (i=0; i<wb*hb; i++) {
        if (prv_mask[i] && mask[i]==0) { 
            tmp[i] = 255; c++;
            avg_mot += frm_mot [i]; 
            avg_sad += frm_sad [i]; 
            avg_skin+= frm_skin[i]; 
        }
        else tmp[i] = 0;
    }
    if (c) { avg_mot/= c; avg_sad/= c; avg_skin/= c; }

    //fix mask
    if (c>100 && avg_mot<100 && avg_sad<100 && avg_skin>60) {
        for (i=0; i<wb*hb; i++) {
            if (tmp[i] && frm_skin[i]>avg_skin/2) {
                mask[i] = 255;
            }
        }
    }
}

//track blob history and potentially overrule the classification based on decision tree
static int track_history(FrmSegmFeat *fsg, SegmFeat *sf_probe, int last, int skin_probe) 
{
    int i, j, score, first = 1, skin = -1;
    int min_x, min_y, max_x, max_y, Th;
    int area = 0, area_int = 0, n = 0;
    FrmSegmFeat *fseg;
    SegmFeat *sf;

    score = 0;
    for (i=last-1; i>=0; i--) {
        fseg = &fsg[i];
        for (j=0; j<fseg->num_segments; j++) {

            sf = &fseg->features[j]; 

            //check is sf and sf_probe are the same blob
            min_x = MAX(sf_probe->min_x, sf->min_x);
            max_x = MIN(sf_probe->max_x, sf->max_x);
            min_y = MAX(sf_probe->min_y, sf->min_y);
            max_y = MIN(sf_probe->max_y, sf->max_y);

            //compute area of the bounding box and the intersection box
            area = (sf_probe->max_x - sf_probe->min_x + 1) * (sf_probe->max_y - sf_probe->min_y + 1);
            area_int = MAX(0, (max_x - min_x + 1)) * MAX(0, (max_y - min_y + 1));

            if (area < 2*area_int) break;
        }
        if (j<fseg->num_segments) { //blobs sf and sf_probe are the same blob
            if (first) {
                first = 0;
                skin = sf->skin;
            }
            if (skin==sf->skin) {
                score++;
            }
            n++; //number of tracks
        }
    }

    //derive score threshold based on availabe history
    if (n < NUM_SEG_TRACK_FRMS-1) Th = MAX(2, n - 3);
    else Th = n - 1;

    if (score>=Th) return skin; //overrule decision tree classification
    else return skin_probe; //keep the decision tree classification
}

//drop undesired segments
static int drop_segments_fast(BYTE *mask, BYTE *prob, BYTE *centersProb, BYTE *skinprob, int w, int h, FrmSegmFeat *fsg, BYTE *frmY, BYTE *frmYB, int pitch, int frameNum, int stack[], int& stackptr, int blTotal) 
{
    FS_UNREFERENCED_PARAMETER(frmYB);
    int i, j, sz, ind, amp, asp, center, cx, cy;
    int min_x, min_y, max_x, max_y;
    int max_size = 0, d, x, y;
    int sz_prc, shape;
    int hist[4];
    float orient, shp_cpx;
    FrmSegmFeat *fseg;

    //create binary mask from probabilities
    for (i=0; i<(w*h); i++) {
        mask[i] = (prob[i] > 30) ? 1 : 0;
    }

    const int frIdx = MIN(frameNum-1, NUM_SEG_TRACK_FRMS-1);

    //initialize tracking segments struct
    fseg = &fsg[frIdx];
    fseg->num_segments = 0;

    //check for undesired segments
    for (i=0; i<h; i++) {
        for (j=0; j<w; j++) {

            ind = i*w + j;
            if (mask[ind]==1) {

                flood_fill4_sz_bb_sum(j, i, 255, mask[ind], w, h, mask, &sz, &min_x, &max_x, &min_y, &max_y, prob, &amp, skinprob, &asp, stack, stackptr, blTotal);

                amp = (amp + sz/2) / sz;			//average segment mixed probability
                asp = (asp + sz/2) / sz;			//average segment skin probability
                cx = (min_x + max_x) / 2;			//segment center x-coord
                cy = (min_y + max_y) / 2;			//segment center x-coord
                center = centersProb[cy*w + cx];	//segment center stability probability

                d = 0;
                if ((amp / (256 - center)) <= 30) {
                    d = 1;
                }
                else {
                    sz_prc = (sz*100) / (w*h);
                    orient = ((float)(max_x - min_x + 1)) / (max_y - min_y + 1);

                    //set basic segment features
                    fseg->features[fseg->num_segments].x = j;
                    fseg->features[fseg->num_segments].y = i;
                    fseg->features[fseg->num_segments].min_x = min_x;
                    fseg->features[fseg->num_segments].max_x = max_x;
                    fseg->features[fseg->num_segments].min_y = min_y;
                    fseg->features[fseg->num_segments].max_y = max_y;
                    fseg->features[fseg->num_segments].size = sz_prc;
                    fseg->features[fseg->num_segments].orient = orient;
                    fseg->features[fseg->num_segments].avgprob = asp;
                    fseg->features[fseg->num_segments].skin = track_history(fsg, &fseg->features[fseg->num_segments], frIdx, -1);

                    //simple decision based on extreme size and orientation
                    if ((sz_prc > 60) && (orient > 2.0f)) fseg->features[fseg->num_segments].skin = 0;

                    if (fseg->features[fseg->num_segments].skin==-1) { //we need to compute advanced features for full decision tree

                        //compute small histogram of rscs-like measure
                        RsCs_8x8_hist(frmY, pitch, mask, w, min_x, max_x, min_y, max_y, hist);

                        //compute shape complexity measure
                        shape = 0;
                        for (y=min_y+1; y<=max_y; y++) {
                            for (x=min_x+1; x<=max_x; x++) {
                                if (mask[y*w + x] && !mask[y*w + (x-1)]) shape++;
                                if (mask[y*w + x] && !mask[(y-1)*w + x]) shape++;
                            }
                        }
                        shp_cpx = (float)(shape) / ((max_x - min_x + 1) + (max_y - min_y + 1));

                        //set advanced segment features (texture and shape complexity)
                        fseg->features[fseg->num_segments].rscs0 = (hist[0]*100)/sz;
                        fseg->features[fseg->num_segments].rscs1 = (hist[1]*100)/sz;
                        fseg->features[fseg->num_segments].rscs2 = (hist[2]*100)/sz;
                        fseg->features[fseg->num_segments].rscs3 = (hist[3]*100)/sz;
                        fseg->features[fseg->num_segments].shape = shp_cpx;
                    }
                    if (fseg->num_segments < MAX_NUM_TRACK_SEGS) fseg->num_segments++;
                }

                if (d) {
                    //drop segment
                }
                else {
                    //keep the segment and update max size
                    if (max_size < sz) max_size = sz;
                }
            }
        }
    }

    return max_size;
}

//performs segment feature-based classification
static void final_segment_postprocessing(BYTE *mask, int w, int h, FrmSegmFeat *fseg, int stack[], int& stackptr, int blTotal) 
{
    SegmFeat *sf; int i;

    for (i=0; i<fseg->num_segments; i++) {
        sf = &fseg->features[i];

        if (sf->skin==-1) { //we need to apply decision tree to classify blob
            sf->skin = 1;
        }

        if (!sf->skin) { //drop blob based on skin-tone classification
            flood_fill4(sf->x, sf->y, 0, 255, w, h, mask, stack, stackptr, blTotal);
        }
    }
}

//checks the shape complexity of the dynamic hystogram mask 
static int get_mask_shape_complexity(BYTE *prob, BYTE *sprob, BYTE *tmp, BYTE *tmp2, int wb, int hb, int w, int bs, int mode) 
{
    FS_UNREFERENCED_PARAMETER(w);
    FS_UNREFERENCED_PARAMETER(bs);
    FS_UNREFERENCED_PARAMETER(mode);
    int i, j;
    int szdiff = 0;

    //binarize probability masks
    {
        for (i=0; i<wb*hb; i++) {
            if (prob[i] > 30) tmp[i] = 255;
            else tmp[i] = 0;
        }
    }
    smooth_mask(tmp, wb, hb, wb);
    for (i=0; i<wb*hb; i++) {
        if (sprob[i] > 30) tmp2[i] = 255;
        else tmp2[i] = 0;
    }
    smooth_mask(tmp2, wb, hb, wb);

    //compute size of the difference mask
    for (i=0; i<wb*hb; i++) {
        if (tmp[i]!=tmp2[i]) szdiff++;
    }

    //compute shape complexity measure
    int shape = 0;
    for (i=1; i<hb; i++) {
        for (j=1; j<wb; j++) {
            if (tmp[i*wb + j] && !tmp[i*wb + (j-1)]) shape++;
            if (tmp[i*wb + j] && !tmp[(i-1)*wb + j]) shape++;
        }
    }
    float shp_cpx = (float)(shape) / (wb + hb);

    //condition for reverting to static histogram
    if ( (shp_cpx > 1.5f) && ((szdiff*100)/(wb*hb) < 20) ) return 0;
    else return 1;
}

//initialize previous face centroids for tracking
static void init_face_centroids(FDet* fd) 
{
    uint i; Pt p;
    p.x = -1; p.y = -1;
    for (i=0; i<fd->faces->size(); i++) {
        fd->prevfc->push_back(p);
    }
}

//tracks faces
static void do_face_tracking(int w, int h, int bs, FrmSegmFeat *fseg, std::vector<Rct> *faces, std::vector<Pt> *prevfc) 
{
    SegmFeat *sf; int i;
    int x_start, y_start, x_end, y_end;
    int cx_start, cy_start, cx_end, cy_end;
    int dx, dy, sz_int, sz;

    for (i=0; i<fseg->num_segments; i++) {
        sf = &fseg->features[i];

        //check if the segment belongs to a face
        for (size_t n=0; n<faces->size(); n++) {
            x_start = ((*faces)[n].x)/bs;
            y_start = ((*faces)[n].y)/bs;
            x_end	= ((*faces)[n].x + (*faces)[n].w)/bs;
            y_end	= ((*faces)[n].y + (*faces)[n].h)/bs;

            cx_start = MAX(x_start, sf->min_x);
            cy_start = MAX(y_start, sf->min_y);
            cx_end	 = MIN(x_end,   sf->max_x);
            cy_end	 = MIN(y_end,   sf->max_y);

            sz_int = (cx_end - cx_start + 1)*(cy_end - cy_start + 1);
            sz     = ( x_end -  x_start + 1)*( y_end -  y_start + 1);

            if ((sz_int*100)/sz > 50) {
                //match face with segment
                if ((*prevfc)[n].x!=-1) {
                    dx = (sf->min_x + sf->max_x)/2 - (*prevfc)[n].x;
                    dy = (sf->min_y + sf->max_y)/2 - (*prevfc)[n].y;

                    if (dx<=8 && dy<=8 && 
                        ((*faces)[n].x + (dx*bs))>=0 && 
                        ((*faces)[n].x + (dx*bs) + (*faces)[n].w)<w && 
                        ((*faces)[n].y + (dy*bs))>=0 && 
                        ((*faces)[n].y + (dy*bs) + (*faces)[n].h)<h) {
                            //update face coords
                            (*faces)[n].x+= dx*bs;
                            (*faces)[n].y+= dy*bs;

                            (*prevfc)[n].x = (sf->min_x + sf->max_x)/2;
                            (*prevfc)[n].y = (sf->min_y + sf->max_y)/2;
                    }
                }
                else {
                    (*prevfc)[n].x = (sf->min_x + sf->max_x)/2;
                    (*prevfc)[n].y = (sf->min_y + sf->max_y)/2;
                }
                break;
            }
        }
    }
}

//do final hole filling
static void fill_holes(BYTE *mask, BYTE *tmp, int w, int h, FrmSegmFeat *fseg, int stack[], int& stackptr, int blTotal) 
{
    int n, i, j, bw, bh, sz;
    BYTE *frm, *buf;
    SegmFeat *sf;

    for (n=0; n<fseg->num_segments; n++) {
        sf = &fseg->features[n];

        bw = sf->max_x - sf->min_x + 1;
        bh = sf->max_y - sf->min_y + 1;
        frm = mask + (sf->min_y * w) + sf->min_x;
        buf = tmp;
        for (i=0; i<bh; i++) {
            memcpy(buf, frm, sizeof(BYTE)*bw);
            frm+= w;
            buf+= bw;
        }
        /*
        int m = 0;
        for (int ii=sf->min_y; ii<=sf->max_y; ii++) {
        for (int jj=sf->min_x; jj<=sf->max_x; jj++) {
        tmp[m++] = mask[ii*w+jj];
        }
        }
        */

        //fill outter area
        for (j=0; j<bw; j++) {
            if (tmp[j]==0) flood_fill4(j, 0, 128, tmp[j], bw, bh, tmp, stack, stackptr, blTotal);
        }
        j = 0;
        for (i=0; i<bh; i++) {
            if (tmp[j]==0) flood_fill4(0, i, 128, tmp[j], bw, bh, tmp, stack, stackptr, blTotal);
            j+= bw;
        }
        i = bw*(bh-1);
        for (j=0; j<bw; j++) {
            if (tmp[i+j]==0) flood_fill4(j, bh-1, 128, tmp[i+j], bw, bh, tmp, stack, stackptr, blTotal);
        }
        j = 0;
        for (i=0; i<bh; i++) {
            if (tmp[j+(bw-1)]==0) flood_fill4(bw-1, i, 128, tmp[j+(bw-1)], bw, bh, tmp, stack, stackptr, blTotal);
            j+= bw;
        }

        for (i=1; i<bh-1; i++) {
            for (j=1; j<bw-1; j++) {
                if (tmp[i*bw+j]==0) {
                    flood_fill4_sz(j, i, 64, tmp[i*bw+j], bw, bh, tmp, &sz, stack, stackptr, blTotal);

                    if ((sz*100)/(w*h) < 5) {
                        flood_fill4(sf->min_x + j, sf->min_y + i, 255, 0, w, h, mask, stack, stackptr, blTotal);
                    }
                }
            }
        }
    }
}

//checks all pairs of faces and if they overlap drops the one with less % of skin area
static int drop_overlapping_faces_mode1(BYTE *frmProb, int w, int h, int bs, std::vector<Rct> *faces, BYTE *frm_Yrg, Dim *dim) 
{
    FS_UNREFERENCED_PARAMETER(h);
    int ii, jj, n, x_start[2], x_end[2], y_start[2], y_end[2], pos[2], sz[2];
    unsigned long ap[2]={0}, ap1[2]={0}, ap2[2]={0}, ap3[2]={0}, apmax[2];
    uint xS = dim->vbOff;
    uint yS = dim->ubOff;
    size_t i, j;

    for (i=0; i<faces->size()-1; i++) {
        for (j=i+1; j<faces->size(); j++) {

            //i-th face
            x_start[0] = ((*faces)[i].x)/bs;
            y_start[0] = ((*faces)[i].y)/bs;
            x_end  [0] = ((*faces)[i].x + (*faces)[i].w)/bs;
            y_end  [0] = ((*faces)[i].y + (*faces)[i].h)/bs;

            //j-th face
            x_start[1] = ((*faces)[j].x)/bs;
            y_start[1] = ((*faces)[j].y)/bs;
            x_end  [1] = ((*faces)[j].x + (*faces)[j].w)/bs;
            y_end  [1] = ((*faces)[j].y + (*faces)[j].h)/bs;

            //check if the faces overlap
            if (MIN(x_end[0], x_end[1]) - MAX(x_start[0], x_start[1]) > 0 &&
                MIN(y_end[0], y_end[1]) - MAX(y_start[0], y_start[1]) > 0) {
                    sz[0] = (((*faces)[i].w / bs) * ((*faces)[i].h / bs));
                    sz[1] = (((*faces)[j].w / bs) * ((*faces)[j].h / bs));

                    //skin prob computing
                    for (n=0; n<=1; n++) {
                        for (ii=y_start[n]; ii<y_end[n]; ii++) {
                            for (jj=x_start[n]; jj<x_end[n]; jj++) {
                                pos[n] = frm_Yrg[yS + (ii*w + jj)]*256 + frm_Yrg[xS + (ii*w + jj)];
                                ap1[n]+= histoYrg [pos[n]];
                                ap2[n]+= histoYrg2[pos[n]];
                                ap3[n]+= histoYrg3[pos[n]];
                                ap [n]+= frmProb[ii*w + jj];
                            }
                        }
                        ap1[n]/= sz[n]; ap2[n]/= sz[n];
                        ap3[n]/= sz[n]; ap [n]/= sz[n];
                        apmax[n] = MAX(MAX(ap[n], ap1[n]), MAX(ap2[n], ap3[n]));
                    }

                    if (apmax[0] < apmax[1]) { //delete i-th face
                        faces->erase(faces->begin() + i);
                        return 1;
                    }
                    else { //delete j-th face
                        faces->erase(faces->begin() + j);
                        return 1;
                    }
            }
        }
    }
    return 0;
}

//performs advanced validation of detected faces (unstable/invalid faces are dropped)
static void validate_faces_advanced_mode1(BYTE *frmProb, int w, int h, int bs, std::vector<Rct> *faces, BYTE *frm_Yrg, Dim *dim) 
{
    int i, j, x_start, x_end, y_start, y_end, pos, sz;
    unsigned long ap=0, ap1=0, ap2=0, ap3=0, apmax;
    uint xS = dim->vbOff;
    uint yS = dim->ubOff;

    //drop overlapping faces
    if (faces->size() > 1) {
        while (drop_overlapping_faces_mode1(frmProb, w, h, bs, faces, frm_Yrg, dim));
    }

    //validate faces
    for (size_t n=0; n<faces->size(); n++) {
        x_start = ((*faces)[n].x)/bs;
        y_start = ((*faces)[n].y)/bs;
        x_end = ((*faces)[n].x + (*faces)[n].w)/bs;
        y_end = ((*faces)[n].y + (*faces)[n].h)/bs;
        sz = (((*faces)[n].w / bs) * ((*faces)[n].h / bs));

        //initial skin prob
        for (i=y_start; i<y_end; i++) {
            for (j=x_start; j<x_end; j++) {
                pos = frm_Yrg[yS + (i*w + j)]*256 + frm_Yrg[xS + (i*w + j)];
                ap1+= histoYrg [pos];
                ap2+= histoYrg2[pos];
                ap3+= histoYrg3[pos];
                ap += frmProb[i*w + j];
            }
        }
        ap1/= sz; ap2/= sz;
        ap3/= sz; ap /= sz;

        apmax = MAX(MAX(ap, ap1), MAX(ap2, ap3));

        if (!ap || apmax < 40) {
            faces->erase(faces->begin() + n);
            n--;
        }
    }
}


//check for empty skin-tone face and fix
static int face_check(BYTE *mask, int w, int h, int bs, std::vector<Rct> faces) 
{
    FS_UNREFERENCED_PARAMETER(h);
    int i, j, c, x_start, x_end, y_start, y_end, sz;

    if (faces.size() != 1) return 0;

    for (size_t n=0; n<faces.size(); n++) {

        x_start = (faces[n].x)/bs;
        y_start = (faces[n].y)/bs;
        x_end = (faces[n].x + faces[n].w)/bs;
        y_end = (faces[n].y + faces[n].h)/bs;
        sz = ((faces[n].w / bs) * (faces[n].h / bs));

        c = 0;
        for (i=y_start; i<y_end; i++) {
            for (j=x_start; j<x_end; j++) {
                if (mask[i*w + j]) c++;
            }
        }

        if (c==0) return 1;
    }

    return 0;
}

static void smooth_histogr(int square[][256], BYTE hist[][256]) 
{
    int i, j, max_val = 0;
    for (i=0; i<256; i++) {
        for (j=0; j<256; j++) {
            if (max_val < square[i][j]) {
                max_val = square[i][j];
            }
        }
    }
    for (i=0; i<256; i++) {
        for (j=0; j<256; j++) {
            hist[i][j] = MIN((255 * square[i][j]) / max_val, 255);
        }
    }
}

static void create_new_dynamic_histo_mode1_NV12(FrameBuffElementPtr *in, BYTE *skinProb, int w2, int h2, int wb, int hb, int bs, std::vector<Rct> faces, int square[][256], BYTE hist[256][256]) 
{
    FS_UNREFERENCED_PARAMETER(skinProb);
    FS_UNREFERENCED_PARAMETER(w2);
    FS_UNREFERENCED_PARAMETER(h2);
    FS_UNREFERENCED_PARAMETER(wb);
    FS_UNREFERENCED_PARAMETER(hb);
    FS_UNREFERENCED_PARAMETER(bs);
    int i, j, x_start, x_end, y_start, y_end;
    //int uoff = w2 * h2 * 4;
    int cx, cy, ew, eh;
    //int w = w2 * 2;
    //int h = h2 * 2;
    int x0, x1, dx;
    int ews, ehs, ewhs;
    int ind, ind2;
    int Ty_lo = 10;
    int Ty_hi = 250;

    //init dyn histo
    for (i=0; i<256; i++) {
        memset(square[i], 0, sizeof(int)*256);
    }

    //sample inner ellipse points
    for (size_t n=0; n<faces.size(); n++) {
        x_start = faces[n].x;
        y_start = faces[n].y;
        x_end = faces[n].x + faces[n].w;
        y_end = faces[n].y + faces[n].h;
        cx = (x_start + x_end) / 2;
        cy = (y_start + y_end) / 2;
        ew = MIN(cx - x_start, x_end - cx);
        eh = MIN(cy - y_start, y_end - cy);

        ew = (ew*3) / 4;
        eh = (eh*3) / 4;

        dx = 0;
        x0 = ew;
        ews = ew*ew;
        ehs = eh*eh;
        ewhs = ews * ehs;

        //fill line at the center
        for (j=-ew; j<=ew; j++) {
            ind = cy*in->p + (cx + j);
            ind2 = (cy/2)*in->pc + ((cx + j)/2)*2;
            if (in->frameY[ind] > Ty_lo && in->frameY[ind] < Ty_hi) {
                square[ in->frameU[ind2]][in->frameU[ind2+1]]++;
            }
        }

        //fill the remaining lines
        for (i=1; i<=eh; i++) {
            x1 = x0 - (dx - 1);
            for ( ; x1>0; x1--) if (x1*x1*ehs + i*i*ews <= ewhs) break;
            dx = x0 - x1;
            x0 = x1;
            for (j=-x0; j<=x0; j++) {
                ind = (cy + i)*in->p + (cx + j);
                ind2 = ((cy + i)/2)*in->pc + ((cx + j)/2)*2;
                if (in->frameY[ind] > Ty_lo && in->frameY[ind] < Ty_hi) {
                    square[in->frameU[ind2]][in->frameU[ind2+1]]++;
                }

                ind = (cy - i)*in->p + (cx + j);
                ind2 = ((cy - i)/2)*in->pc + ((cx + j)/2)*2;
                if (in->frameY[ind] > Ty_lo && in->frameY[ind] < Ty_hi) {
                    square[in->frameU[ind2]][in->frameU[ind2+1]]++;
                }
            }
        }
    }

    //smooth histogram
    smooth_histogr(square, hist);
}

//main skin detection function

// core function to generate face/skin segmentation map
void SkinDetectionMode1_NV12_slice_start(SDet *sd, FDet* fd, FrameBuffElementPtr **FrameBuff, BYTE *pInYUV, BYTE *pInYCbCr, BYTE *pOut) 
{
    BYTE *tmp1, *tmp2, *tmp3;
    Dim* dim = &sd->dim;
    int w  = dim->w;
    int h  = dim->h;
    int bs = BLOCKSIZE;
    int wb = dim->wb;
    int hb = dim->hb;

    {
        sd->tmpFr = pOut;
        tmp1 = sd->tmpFr;
        tmp2 = sd->tmpFr + wb*hb;
        tmp3 = sd->tmpFr + 2*wb*hb;
    }

    //calculate motion
    sd->prvTotalSAD = CalculateMotion(FrameBuff[0], FrameBuff[1], FrameBuff[0]->bSAD, 0, dim); //SAD with prv frame
    sd->prvTotalSAD = 64 * sd->prvTotalSAD / dim->blTotal;

    //initialize frame buffers
    for (uint i=0; i < dim->numFr; i++) {
        memset(FrameBuff[i]->bSAD, 0, dim->blTotal);
    }
    memset(sd->prvProb, 0, dim->blTotal);
    sd->frameNum = 1;
    sd->sceneNum++;	

    //build initial skin probability map
    Compute_dyn_luma_thresh(FrameBuff[0], dim, &(sd->bg), &(sd->yTh));

    BuildSkinMap_dyn_fast(sd->skinProbB, pInYCbCr, dim, sd->bg, sd->yTh, sd->hist_ind, sd->mode);

    //detect faces
    DetectFaces(FrameBuff[0], w, h, fd);

    //validate detected faces based on skin area
    validate_faces_advanced_mode1(sd->skinProbB, wb, hb, bs, fd->faces, pInYCbCr, dim);

    //initialize previous face centroids for tracking
    init_face_centroids(fd);

    //dynamic skin histogram generation and selection
    if (fd->faces->size() > 0) {

        init_New_Skin_Map_from_face_mode1_NV12(FrameBuff[0], sd->skinProbB, w/2, h/2, wb, hb, bs, *fd->faces, sd->square, sd->hist);

        New_Skin_Map_from_face(sd->tmpFr, pInYUV, wb, hb, sd->hist);

        sd->upd_skin = Update_skinProbB_mode1(pInYUV, sd->skinProbB, sd->tmpFr, wb, hb, bs, *fd->faces, &(sd->faceskin_perc));
        if (sd->upd_skin) {
            sd->upd_skin = get_mask_shape_complexity(sd->tmpFr, sd->skinProbB, tmp2, tmp3, wb, hb, w, bs, sd->mode);
        }
    }

    //content-based selection of static skin probability histogram
    if (!sd->upd_skin) {
        sd->hist_ind = SelSkinMap(sd->skinProbB, pInYCbCr, dim, sd->prms->enableHystoDynEnh, sd->mode);
    }
}


// core function to generate face/skin segmentation map
void SkinDetectionMode1_NV12_slice_end(SDet *sd, FDet* fd, FrameBuffElementPtr **FrameBuff, BYTE *pInYUV, BYTE *pInYCbCr, BYTE *pOut) 
{
    FS_UNREFERENCED_PARAMETER(pInYCbCr);
    BYTE *tmp1, *tmp2, *tmp3;
    Dim* dim = &sd->dim;
    int w  = dim->w;
    int h  = dim->h;
    int bs = BLOCKSIZE;
    int wb = dim->wb;
    int hb = dim->hb;

    {
        sd->tmpFr = pOut;
        tmp1 = sd->tmpFr;
        tmp2 = sd->tmpFr + wb*hb;
        tmp3 = sd->tmpFr + 2*wb*hb;
    }

    if(!sd->bSceneChange)
    {
        for(int k=0; k< NSLICES; k++) sd->prvTotalSAD += sd->prvSAD[k];
        sd->prvTotalSAD = (64 * sd->prvTotalSAD) / dim->blTotal;
    }

    //smooth block skin regions
    memcpy(tmp1, sd->skinProbB, dim->blTotal * sizeof(BYTE));
    Median_5x5(tmp1, sd->skinProbB, dim->blHNum, dim->blVNum);

    //smooth motion cues
    SmothMotionMask(sd->motionMask, sd->curMotionMask, sd->sadMax, FrameBuff, dim);

    //update (enhance) motion cues with face info
    if (fd->faces->size()) {
        DrawFaces_small_smart(sd->motionMask, sd->sadMax, sd->skinProbB, tmp1, wb, hb, bs, *(fd->faces), sd->stack, sd->stackptr, dim->blTotal);
    }

    //mix skin and motion masks
    MixMotionSkinMask(
        tmp3, sd->motionMask, sd->skinProbB, 
        tmp1, tmp2, sd->sadMax, sd->prvProb, 
        256 * sd->maxSegSz / dim->blTotal, sd->prvTotalSAD, dim);

    //segments postprocessing
    {
        uint curSegSz = drop_segments_fast(tmp1, tmp3, sd->centersProb, sd->skinProbB, dim->wb, dim->hb, sd->fsg, FrameBuff[0]->frameY, pInYUV, FrameBuff[0]->p, sd->frameNum, sd->stack, sd->stackptr, dim->blTotal);
        sd->maxSegSz = (sd->maxSegSz * 31 + curSegSz) / 32;
    }

    //check and fix large sudden holes in the mask
    if (!sd->bSceneChange) {
        fix_large_holes(tmp1, sd->prvMask, tmp2, sd->motionMask, sd->sadMax, sd->skinProbB, wb, hb);
    }

    //do final segment re-classification
    final_segment_postprocessing(tmp1, wb, hb, &sd->fsg[MIN(sd->frameNum-1, NUM_SEG_TRACK_FRMS-1)], sd->stack, sd->stackptr, dim->blTotal);

    //final mask smoothing
    smooth_mask(tmp1, wb, hb, wb);

    //final hole fixing
    fill_holes(tmp1, tmp2, wb, hb, &sd->fsg[MIN(sd->frameNum-1, NUM_SEG_TRACK_FRMS-1)], sd->stack, sd->stackptr, dim->blTotal);

    //check for empty skin-tone face and fix
    if (sd->bSceneChange) {
        if (face_check(tmp1, wb, hb, bs, *(fd->faces))) {
            create_new_dynamic_histo_mode1_NV12(FrameBuff[0], sd->skinProbB, w/2, h/2, wb, hb, bs, *(fd->faces), sd->square, sd->hist);
            sd->upd_skin = 1;
        }
    }

    //face tracking
    do_face_tracking(w, h, bs, &sd->fsg[MIN(sd->frameNum-1, NUM_SEG_TRACK_FRMS-1)], fd->faces, fd->prevfc);

    //finalizing
    memcpy(sd->prvMask, tmp1, dim->blTotal);

    SwapFrameSegFeatures(sd->fsg, sd->frameNum); //swapping segment features
    sd->frameNum++;
}


void SkinDetectionMode1_NV12_slice_main(SDet *sd, FDet* fd, FrameBuffElementPtr **FrameBuff, BYTE *pInYUV, BYTE *pInYCbCr, SliceInfoNv12 *si, int sliceId) 
{
    FS_UNREFERENCED_PARAMETER(fd);
    Dim* dim = &sd->dim;

    if(!sd->bSceneChange)
    {
        //subsample pixel-accurate frame to block-accurate YUV4:4:4		
        SubsampleNV12_slice(FrameBuff[0]->frameY, FrameBuff[0]->frameU, pInYUV, dim->w, dim->h, FrameBuff[0]->p, FrameBuff[0]->pc, BLOCKSIZE, si, sliceId);

        ConvertColorSpaces444_slice(pInYUV, pInYCbCr, dim->wb, dim->hb, dim->wb, si->blockOffset[sliceId], si->numBlockRows[sliceId]);

        sd->prvSAD[sliceId] = CalculateMotion_slice(FrameBuff[0], FrameBuff[1], FrameBuff[0]->bSAD, 0, dim, si->blockOffset[sliceId], si->sampleOffset[sliceId], si->numSampleRows[sliceId]);
    } 

    //build post-initial skin probability map
    if (sd->upd_skin) 
    {
        New_Skin_Map_from_face_slice(sd->skinProbB + si->blockOffset[sliceId], pInYUV, dim->wb, dim->hb, sd->hist, si->blockOffset[sliceId], si->numBlockRows[sliceId]);
    }
    else 
    {
        BuildSkinMap_dyn_fast_slice(sd->skinProbB + si->blockOffset[sliceId], pInYCbCr, dim, sd->bg, sd->yTh, sd->hist_ind, sd->mode, si->blockOffset[sliceId], si->numBlockRows[sliceId]);
    }

    //compute motion cues
    uint k = MIN(sd->frameNum - 1, dim->numFr - 1);
    CalculateMotion_slice(FrameBuff[0], FrameBuff[k], sd->curMotionMask, 0, dim, si->blockOffset[sliceId], si->sampleOffset[sliceId], si->numSampleRows[sliceId]);

}

void AvgLuma_slice(BYTE *pYSrc, BYTE *pLuma, int w, int h, int p, SliceInfoNv12 *si, int sliceId) 
{
    //subsample pixel-accurate frame to block-accurate
    SubsampleLuma_slice(pYSrc, pLuma, w, h, p, BLOCKSIZE, si, sliceId);
}

