/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "mfx_vpp_scd.h"

#if defined (MFX_ENABLE_VPP) && defined(MFX_VA_LINUX) && defined(MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP)

#define EXTRANEIGHBORS
#define SAD_SEARCH_VSTEP 2  // 1=FS 2=FHS

#define _mm_loadh_epi64(a, ptr) _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(a), (__m64 *)(ptr)))

static const mfxU16 tab_killmask[8][8] = {
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff,
};

#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
void ME_SAD_8x8_Block_Search_AVX2(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange,
           mfxU32 *bestSAD, int *bestX, int *bestY)
{
    __m256i
        s0 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[0*pitch]), (__m128i *)&pSrc[1*pitch])),
        s1 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[2*pitch]), (__m128i *)&pSrc[3*pitch])),
        s2 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[4*pitch]), (__m128i *)&pSrc[5*pitch])),
        s3 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[6*pitch]), (__m128i *)&pSrc[7*pitch]));

    for (int y = 0; y < yrange; y += SAD_SEARCH_VSTEP) {
        for (int x = 0; x < xrange; x += 8) {
            pmfxU8 pr = pRef + (y * pitch) + x;
            __m256i
                r0 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[0*pitch])),
                r1 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[1*pitch])),
                r2 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[2*pitch])),
                r3 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[3*pitch])),
                r4 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[4*pitch])),
                r5 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[5*pitch])),
                r6 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[6*pitch])),
                r7 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[7*pitch]));
            r0 = _mm256_mpsadbw_epu8(r0, s0, 0x28);
            r1 = _mm256_mpsadbw_epu8(r1, s0, 0x3a);
            r2 = _mm256_mpsadbw_epu8(r2, s1, 0x28);
            r3 = _mm256_mpsadbw_epu8(r3, s1, 0x3a);
            r4 = _mm256_mpsadbw_epu8(r4, s2, 0x28);
            r5 = _mm256_mpsadbw_epu8(r5, s2, 0x3a);
            r6 = _mm256_mpsadbw_epu8(r6, s3, 0x28);
            r7 = _mm256_mpsadbw_epu8(r7, s3, 0x3a);
            r0 = _mm256_add_epi16(r0, r1);
            r2 = _mm256_add_epi16(r2, r3);
            r4 = _mm256_add_epi16(r4, r5);
            r6 = _mm256_add_epi16(r6, r7);
            r0 = _mm256_add_epi16(r0, r2);
            r4 = _mm256_add_epi16(r4, r6);
            r0 = _mm256_add_epi16(r0, r4);
            // horizontal sum
            __m128i
                t = _mm_add_epi16(_mm256_castsi256_si128(r0), _mm256_extractf128_si256(r0, 1));
            // kill out-of-bound values
            if (xrange - x < 8)
                t = _mm_or_si128(t, _mm_load_si128((__m128i *)tab_killmask[xrange - x]));
            t = _mm_minpos_epu16(t);
            mfxU32
                SAD = _mm_extract_epi16(t, 0);
            if (SAD < *bestSAD) {
                *bestSAD = SAD;
                *bestX = x + _mm_extract_epi16(t, 1);
                *bestY = y;
            }
        }
    }
}
#endif

void ME_SAD_8x8_Block_Search_SSE4(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange,
           mfxU32 *bestSAD, int *bestX, int *bestY)
{
    __m128i
        s0 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[0*pitch]), (__m128i *)&pSrc[1*pitch]),
        s1 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[2*pitch]), (__m128i *)&pSrc[3*pitch]),
        s2 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[4*pitch]), (__m128i *)&pSrc[5*pitch]),
        s3 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[6*pitch]), (__m128i *)&pSrc[7*pitch]);
    for (int y = 0; y < yrange; y += SAD_SEARCH_VSTEP) {
        for (int x = 0; x < xrange; x += 8) {
            pmfxU8
                pr = pRef + (y * pitch) + x;
            __m128i
                r0 = _mm_loadu_si128((__m128i *)&pr[0*pitch]),
                r1 = _mm_loadu_si128((__m128i *)&pr[1*pitch]),
                r2 = _mm_loadu_si128((__m128i *)&pr[2*pitch]),
                r3 = _mm_loadu_si128((__m128i *)&pr[3*pitch]),
                r4 = _mm_loadu_si128((__m128i *)&pr[4*pitch]),
                r5 = _mm_loadu_si128((__m128i *)&pr[5*pitch]),
                r6 = _mm_loadu_si128((__m128i *)&pr[6*pitch]),
                r7 = _mm_loadu_si128((__m128i *)&pr[7*pitch]);
            r0 = _mm_add_epi16(_mm_mpsadbw_epu8(r0, s0, 0), _mm_mpsadbw_epu8(r0, s0, 5));
            r1 = _mm_add_epi16(_mm_mpsadbw_epu8(r1, s0, 2), _mm_mpsadbw_epu8(r1, s0, 7));
            r2 = _mm_add_epi16(_mm_mpsadbw_epu8(r2, s1, 0), _mm_mpsadbw_epu8(r2, s1, 5));
            r3 = _mm_add_epi16(_mm_mpsadbw_epu8(r3, s1, 2), _mm_mpsadbw_epu8(r3, s1, 7));
            r4 = _mm_add_epi16(_mm_mpsadbw_epu8(r4, s2, 0), _mm_mpsadbw_epu8(r4, s2, 5));
            r5 = _mm_add_epi16(_mm_mpsadbw_epu8(r5, s2, 2), _mm_mpsadbw_epu8(r5, s2, 7));
            r6 = _mm_add_epi16(_mm_mpsadbw_epu8(r6, s3, 0), _mm_mpsadbw_epu8(r6, s3, 5));
            r7 = _mm_add_epi16(_mm_mpsadbw_epu8(r7, s3, 2), _mm_mpsadbw_epu8(r7, s3, 7));
            r0 = _mm_add_epi16(r0, r1);
            r2 = _mm_add_epi16(r2, r3);
            r4 = _mm_add_epi16(r4, r5);
            r6 = _mm_add_epi16(r6, r7);
            r0 = _mm_add_epi16(r0, r2);
            r4 = _mm_add_epi16(r4, r6);
            r0 = _mm_add_epi16(r0, r4);
            // kill out-of-bound values
            if (xrange - x < 8)
                r0 = _mm_or_si128(r0, _mm_load_si128((__m128i *)tab_killmask[xrange - x]));
            r0 = _mm_minpos_epu16(r0);
            mfxU32
                SAD = _mm_extract_epi16(r0, 0);
            if (SAD < *bestSAD) {
                *bestSAD = SAD;
                *bestX = x + _mm_extract_epi16(r0, 1);
                *bestY = y;
            }
        }
    }
}

void ME_SAD_8x8_Block_Search_C(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange,
        mfxU32 *bestSAD, int *bestX, int *bestY) {
    for (int y = 0; y < yrange; y += SAD_SEARCH_VSTEP) {
        for (int x = 0; x < xrange; x++) {
            pmfxU8
                pr = pRef + (y * pitch) + x,
                ps = pSrc;
            mfxU32
                SAD = 0;
            for (int i = 0; i < 8; i++) {
                SAD += abs(pr[0] - ps[0]);
                SAD += abs(pr[1] - ps[1]);
                SAD += abs(pr[2] - ps[2]);
                SAD += abs(pr[3] - ps[3]);
                SAD += abs(pr[4] - ps[4]);
                SAD += abs(pr[5] - ps[5]);
                SAD += abs(pr[6] - ps[6]);
                SAD += abs(pr[7] - ps[7]);
                pr += pitch;
                ps += pitch;
            }
            if (SAD < *bestSAD) {
                *bestSAD = SAD;
                *bestX = x;
                *bestY = y;
            }
        }
    }
}

#define _mm_loadh_epi64(a, ptr) _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(a), (__m64 *)(ptr)))
#define _mm_movehl_epi64(a, b) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b)))

mfxU32 ME_SAD_8x8_Block(mfxU8 *pSrc, mfxU8 *pRef, mfxU32 srcPitch, mfxU32 refPitch)
{
    __m128i s0 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[0*srcPitch]), (__m128i *)&pSrc[1*srcPitch]);
    __m128i r0 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pRef[0*refPitch]), (__m128i *)&pRef[1*refPitch]);
    __m128i s1 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[2*srcPitch]), (__m128i *)&pSrc[3*srcPitch]);
    __m128i r1 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pRef[2*refPitch]), (__m128i *)&pRef[3*refPitch]);
    __m128i s2 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[4*srcPitch]), (__m128i *)&pSrc[5*srcPitch]);
    __m128i r2 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pRef[4*refPitch]), (__m128i *)&pRef[5*refPitch]);
    __m128i s3 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[6*srcPitch]), (__m128i *)&pSrc[7*srcPitch]);
    __m128i r3 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pRef[6*refPitch]), (__m128i *)&pRef[7*refPitch]);
    s0 = _mm_sad_epu8(s0, r0);
    s1 = _mm_sad_epu8(s1, r1);
    s2 = _mm_sad_epu8(s2, r2);
    s3 = _mm_sad_epu8(s3, r3);
    s0 = _mm_add_epi32(s0, s1);
    s2 = _mm_add_epi32(s2, s3);
    s0 = _mm_add_epi32(s0, s2);
    s2 = _mm_movehl_epi64(s2, s0);
    s0 = _mm_add_epi32(s0, s2);
    return _mm_cvtsi128_si32(s0);
}

BOOL MVcalcSAD8x8(MVector MV, pmfxU8 curY, pmfxU8 refY, ImDetails dataIn, mfxU32 *bestSAD, mfxI32 *distance) {
    mfxI32
        preDist = (MV.x * MV.x) + (MV.y * MV.y);
    pmfxU8
        fRef = refY + MV.x + (MV.y * dataIn.Extended_Width);
    mfxU32 SAD = 0;
    SAD = ME_SAD_8x8_Block(curY, fRef, dataIn.Extended_Width, dataIn.Extended_Width);
    if((SAD < *bestSAD) || ((SAD == *(bestSAD)) && *distance > preDist))
    {
        *distance = preDist;
        *(bestSAD) = SAD;
        return TRUE;
    }
    return FALSE;
}

void MotionRangeDeliveryF(mfxI32 xLoc, mfxI32 yLoc, mfxI32 *limitXleft, mfxI32 *limitXright, mfxI32 *limitYup, mfxI32 *limitYdown, ImDetails dataIn, VidData limits) {
    limits;
    mfxI32
        locX = 0,
        locY = 0;
    locY = yLoc / ((3 * (16 / dataIn.block_height)) / 2);
    locX = xLoc / ((3 * (16 / dataIn.block_width)) / 2);
    *limitXleft = NMAX(-16, -(xLoc * dataIn.block_width) - dataIn.horizontal_pad);
    *limitXright = NMIN(15, dataIn.Extended_Width - (((xLoc + 1) * dataIn.block_width) + dataIn.horizontal_pad));
    *limitYup = NMAX(-12, -(yLoc * dataIn.block_height) - dataIn.vertical_pad);
    *limitYdown = NMIN(11, dataIn.Extended_Height - (((yLoc + 1) * dataIn.block_width) + dataIn.vertical_pad));
}

mfxI32 DistInt(MVector vector) {
    return (vector.x*vector.x) + (vector.y*vector.y);;
}

void MVpropagationCheck(mfxI32 xLoc, mfxI32 yLoc, ImDetails dataIn, MVector *propagatedMV) {
    mfxI32
        left = (xLoc * dataIn.block_width) + dataIn.horizontal_pad,
        right = (dataIn.Extended_Width - left - dataIn.block_width),
        up = (yLoc * dataIn.block_height) + dataIn.vertical_pad,
        down = (dataIn.Extended_Height - up - dataIn.block_height);
    if(propagatedMV->x < 0) {
        if(left + propagatedMV->x < 0)
            propagatedMV->x = -left;
    }
    else {
        if(right - propagatedMV->x < 0)
            propagatedMV->x = right;
    }
    if(propagatedMV->y < 0) {
        if(up + propagatedMV->y < 0)
            propagatedMV->y = -up;
    }
    else {
        if(down - propagatedMV->y < 0)
            propagatedMV->y = down;
    }
}

void SearchLimitsCalc(mfxI32 xLoc, mfxI32 yLoc, mfxI32 *limitXleft, mfxI32 *limitXright, mfxI32 *limitYup, mfxI32 *limitYdown, ImDetails dataIn, mfxI32 range, MVector mv, VidData limits) {
    mfxI32
        locX = (xLoc * dataIn.block_width) + dataIn.horizontal_pad + mv.x,
        locY = (yLoc * dataIn.block_height) + dataIn.vertical_pad + mv.y;
    *limitXleft = NMAX(-locX,-range);
    *limitXright = NMIN(dataIn.Extended_Width - ((xLoc + 1) * dataIn.block_width) - dataIn.horizontal_pad - mv.x,range);
    *limitYup = NMAX(-locY,-range);
    *limitYdown = NMIN(dataIn.Extended_Height - ((yLoc + 1) * dataIn.block_height) - dataIn.vertical_pad - mv.y,range);
    if(limits.limitRange) {
        *limitXleft = NMAX(*limitXleft,-limits.maxXrange);
        *limitXright = NMIN(*limitXright,limits.maxXrange);
        *limitYup = NMAX(*limitYup,-limits.maxYrange);
        *limitYdown = NMIN(*limitYdown,limits.maxYrange);
    }
}

#define MAXSPEED 0
#define EXTRANEIGHBORS
const MVector zero = {0,0};

mfxI32 __cdecl SceneChangeDetector::HME_Low8x8fast(VidRead *videoIn, mfxI32 fPos, ImDetails dataIn, imageData *scale, imageData *scaleRef, BOOL first, mfxU32 accuracy, VidData limits) {
    MVector
        lineMV[2],
        tMV,
        ttMV,
        *current,
        preMV = zero,
        Nmv;
    mfxU8
        *objFrame,
        *refFrame[4];
    mfxI32
        limitXleft = 0,
        limitXright = 0,
        limitYup = 0,
        limitYdown = 0,
        k,
        direction[2],
        acc,
        counter,
        lineSAD[2],
        xLoc = (fPos % dataIn.Width_in_blocks),
        yLoc = (fPos / dataIn.Width_in_blocks),
        distance = INT_MAX,
        plane = 0,
        bestPlane = 0,
        bestPlaneN = 0,
        zeroSAD = INT_MAX,
        offset = (yLoc * dataIn.Extended_Width * dataIn.block_height) + (xLoc * dataIn.block_width);
    mfxU32
        tl,
        *outSAD,
        bestSAD = INT_MAX;
    Rsad
        range;
    BOOL
        foundBetter = FALSE,
        truneighbor = FALSE;
    objFrame = scale->Image.Y + offset;
    refFrame[0] = scaleRef->Image.Y + offset;
    current = scale->pInteger;
    outSAD = scale->SAD;
    if (accuracy == 1)
        acc = 1;
    else
        acc = 4;
    if (first) {
        MVcalcSAD8x8(zero, objFrame, refFrame[0], dataIn, &bestSAD, &distance);
        zeroSAD = bestSAD;
        current[fPos] = zero;
        outSAD[fPos] = bestSAD;
        if (zeroSAD == 0)
            return zeroSAD;
        MotionRangeDeliveryF(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, dataIn, limits);
    }
    for (k = 0; k<5; k++) {
        range.SAD = outSAD[fPos];
        range.BestMV = current[fPos];
        range.distance = INT_MAX;
        range.angle = 361;
        range.direction = 0;
    }
    direction[0] = 0;
    lineMV[0].x = 0;
    lineMV[0].y = 0;
    k = 0;
    {
        mfxU8
            *ps = objFrame,
            *pr = refFrame[plane] + (limitYup * dataIn.Extended_Width) + limitXleft;
        int xrange = limitXright - limitXleft + 1,
            yrange = limitYdown - limitYup + 1,
            bX = 0,
            bY = 0;
        mfxU32
            bSAD = INT_MAX;
        SCD_CPU_DISP(m_cpuOpt, ME_SAD_8x8_Block_Search, ps, pr, dataIn.Extended_Width, xrange, yrange, &bSAD, &bX, &bY);
        if (bSAD < range.SAD) {
            range.SAD = bSAD;
            range.BestMV.x = bX + limitXleft;
            range.BestMV.y = bY + limitYup;
            range.distance = DistInt(range.BestMV);
        }
    }
    outSAD[fPos] = range.SAD;
    current[fPos] = range.BestMV;

    /*ExtraNeighbors*/
#ifdef EXTRANEIGHBORS
    if (fPos > (dataIn.Width_in_blocks * 3)) {
        tl = range.SAD;
        Nmv.x = current[fPos - (dataIn.Width_in_blocks * 3)].x;
        Nmv.y = current[fPos - (dataIn.Width_in_blocks * 3)].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
    if ((fPos > (dataIn.Width_in_blocks * 2)) && (xLoc > 0)) {
        tl = range.SAD;
        Nmv.x = current[fPos - (dataIn.Width_in_blocks * 2) - 1].x;
        Nmv.y = current[fPos - (dataIn.Width_in_blocks * 2) - 1].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
    if (fPos > (dataIn.Width_in_blocks * 2)) {
        tl = range.SAD;
        Nmv.x = current[fPos - (dataIn.Width_in_blocks * 2)].x;
        Nmv.y = current[fPos - (dataIn.Width_in_blocks * 2)].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
    if ((fPos > (dataIn.Width_in_blocks * 2)) && (xLoc < dataIn.Width_in_blocks - 1)) {
        tl = range.SAD;
        Nmv.x = current[fPos - (dataIn.Width_in_blocks * 2) + 1].x;
        Nmv.y = current[fPos - (dataIn.Width_in_blocks * 2) + 1].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
#endif
    if ((fPos > dataIn.Width_in_blocks) && (xLoc > 0)) {
        tl = range.SAD;
        Nmv.x = current[fPos - dataIn.Width_in_blocks - 1].x;
        Nmv.y = current[fPos - dataIn.Width_in_blocks - 1].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
    if (fPos > dataIn.Width_in_blocks) {
        tl = range.SAD;
        Nmv.x = current[fPos - dataIn.Width_in_blocks].x;
        Nmv.y = current[fPos - dataIn.Width_in_blocks].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
    if ((fPos> dataIn.Width_in_blocks) && (xLoc < dataIn.Width_in_blocks - 1)) {
        tl = range.SAD;
        Nmv.x = current[fPos - dataIn.Width_in_blocks + 1].x;
        Nmv.y = current[fPos - dataIn.Width_in_blocks + 1].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
    if (xLoc > 0) {
        tl = range.SAD;
        distance = INT_MAX;
        Nmv.x = current[fPos - 1].x;
        Nmv.y = current[fPos - 1].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
    if (xLoc > 1) {
        tl = range.SAD;
        distance = INT_MAX;
        Nmv.x = current[fPos - 2].x;
        Nmv.y = current[fPos - 2].y;
        distance = DistInt(Nmv);
        MVpropagationCheck(xLoc, yLoc, dataIn, &Nmv);
        if (MVcalcSAD8x8(Nmv, objFrame, refFrame[plane], dataIn, &tl, &distance)) {
            range.BestMV = Nmv;
            range.SAD = tl;
            foundBetter = FALSE;
            truneighbor = TRUE;
            bestPlaneN = plane;
        }
    }
    if (truneighbor) {
        ttMV = range.BestMV;
        bestSAD = range.SAD;
        SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, dataIn, 1, range.BestMV, limits);
        for (tMV.y = limitYup; tMV.y <= limitYdown; tMV.y++) {
            for (tMV.x = limitXleft; tMV.x <= limitXright; tMV.x++) {
                preMV.x = tMV.x + ttMV.x;
                preMV.y = tMV.y + ttMV.y;
                foundBetter = MVcalcSAD8x8(preMV, objFrame, refFrame[plane], dataIn, &bestSAD, &distance);
                if (foundBetter) {
                    range.BestMV = preMV;
                    range.SAD = bestSAD;
                    bestPlaneN = plane;
                    foundBetter = FALSE;
                }
            }
        }
        if (truneighbor) {
            outSAD[fPos] = range.SAD;
            current[fPos] = range.BestMV;
            truneighbor = FALSE;
            bestPlane = bestPlaneN;
        }
    }
    range.SAD = outSAD[fPos];
    /*Zero search*/
    if (!first) {
        SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, dataIn, 16/*32*/, zero, limits);
        counter = 0;
        for (tMV.y = limitYup; tMV.y <= limitYdown; tMV.y += 2) {
            lineSAD[0] = INT_MAX;
            for (tMV.x = limitXleft + ((counter % 2) * 2); tMV.x <= limitXright; tMV.x += 4) {
                ttMV.x = tMV.x + zero.x;
                ttMV.y = tMV.y + zero.y;
                bestSAD = range.SAD;
                distance = range.distance;
                foundBetter = MVcalcSAD8x8(ttMV, objFrame, refFrame[0], dataIn, &bestSAD, &distance);
                if (foundBetter) {
                    range.BestMV = ttMV;
                    range.SAD = bestSAD;
                    range.distance = distance;
                    foundBetter = FALSE;
                    bestPlane = 0;
                }
            }
            counter++;
        }
    }
    ttMV = range.BestMV;
    SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, dataIn, 1, range.BestMV, limits);
    for (tMV.y = limitYup; tMV.y <= limitYdown; tMV.y++) {
        for (tMV.x = limitXleft; tMV.x <= limitXright; tMV.x++) {
            preMV.x = tMV.x + ttMV.x;
            preMV.y = tMV.y + ttMV.y;
            foundBetter = MVcalcSAD8x8(preMV, objFrame, refFrame[0], dataIn, &bestSAD, &distance);
            if (foundBetter) {
                range.BestMV = preMV;
                range.SAD = bestSAD;
                range.distance = distance;
                bestPlane = 0;
                foundBetter = FALSE;
            }
        }
    }
    outSAD[fPos] = range.SAD;
    current[fPos] = range.BestMV;
    videoIn->average += (range.BestMV.x * range.BestMV.x) + (range.BestMV.y * range.BestMV.y);
    return(zeroSAD);
}

void SceneChangeDetector::MotionAnalysis(VidRead *support, VidData *dataIn, VidSample *videoIn, VidSample *videoRef, mfxF32 *TSC, mfxF32 *AFD, mfxF32 *MVdiffVal, Layers lyrIdx) {
    support;
    mfxU32
        valNoM = 0,
        valb = 0;
    /*--Motion Estimation--*/
    *MVdiffVal = 0;
    for (int fPos = 0; fPos<dataIn->layer[lyrIdx].Blocks_in_Frame; fPos++) {
        valNoM += HME_Low8x8fast(support, fPos, dataIn->layer[lyrIdx], &(videoIn->layer[lyrIdx]), &(videoRef->layer[lyrIdx]), TRUE, 1, *dataIn);
        valb += videoIn->layer[lyrIdx].SAD[fPos];
        *MVdiffVal += (videoIn->layer[lyrIdx].pInteger[fPos].x - videoRef->layer[lyrIdx].pInteger[fPos].x) * (videoIn->layer[lyrIdx].pInteger[fPos].x - videoRef->layer[lyrIdx].pInteger[fPos].x);
        *MVdiffVal += (videoIn->layer[lyrIdx].pInteger[fPos].y - videoRef->layer[lyrIdx].pInteger[fPos].y) * (videoIn->layer[lyrIdx].pInteger[fPos].y - videoRef->layer[lyrIdx].pInteger[fPos].y);
    }
    *TSC = (mfxF32)valb / (mfxF32)dataIn->layer[lyrIdx].Pixels_in_Y_layer;
    *AFD = (mfxF32)valNoM / (mfxF32)dataIn->layer[lyrIdx].Pixels_in_Y_layer;
    *MVdiffVal = (mfxF32)((mfxF64)*MVdiffVal / (mfxF64)dataIn->layer[lyrIdx].Blocks_in_Frame);
}

int TableLookUp(mfxI32 limit, mfxF32 *table, mfxF32 comparisonValue) {
    for (int pos = 0; pos < limit; pos++) {
        if (comparisonValue < table[pos])
            return pos;
    }
    return limit;
}

// Load 0..3 floats to XMM register from memory
// NOTE: elements of XMM are permuted [ 2 - 1 ]
static inline __m128 LoadPartialXmm(float *pSrc, mfxI32 len)
{
    __m128 xmm = _mm_setzero_ps();
    if (len & 2)
    {
        xmm = _mm_loadh_pi(xmm, (__m64 *)pSrc);
        pSrc += 2;
    }
    if (len & 1)
    {
        xmm = _mm_move_ss(xmm, _mm_load_ss(pSrc));
    }
    return xmm;
}

// Store 0..3 floats from XMM register to memory
// NOTE: elements of XMM are permuted [ 2 - 1 ]
static inline void StorePartialXmm(float *pDst, __m128 xmm, mfxI32 len)
{
    if (len & 2)
    {
        _mm_storeh_pi((__m64 *)pDst, xmm);
        pDst += 2;
    }
    if (len & 1)
    {
        _mm_store_ss(pDst, xmm);
    }
}

#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
// Load 0..7 floats to YMM register from memory
// NOTE: elements of YMM are permuted [ 4 2 - 1 ]
static inline __m256 LoadPartialYmm(float *pSrc, mfxI32 len)
{
    __m128 xlo = _mm_setzero_ps();
    __m128 xhi = _mm_setzero_ps();
    if (len & 4) {
        xhi = _mm_loadu_ps(pSrc);
        pSrc += 4;
    }
    if (len & 2) {
        xlo = _mm_loadh_pi(xlo, (__m64 *)pSrc);
        pSrc += 2;
    }
    if (len & 1) {
        xlo = _mm_move_ss(xlo, _mm_load_ss(pSrc));
    }
    return _mm256_insertf128_ps(_mm256_castps128_ps256(xlo), xhi, 1);
}

// Store 0..7 floats from YMM register to memory
// NOTE: elements of YMM are permuted [ 4 2 - 1 ]
static inline void StorePartialYmm(float *pDst, __m256 ymm, mfxI32 len)
{
    __m128 xlo = _mm256_castps256_ps128(ymm);
    __m128 xhi = _mm256_extractf128_ps(ymm, 1);
    if (len & 4) {
        _mm_storeu_ps(pDst, xhi);
        pDst += 4;
    }
    if (len & 2) {
        _mm_storeh_pi((__m64 *)pDst, xlo);
        pDst += 2;
    }
    if (len & 1) {
        _mm_store_ss(pDst, xlo);
    }
}
#endif

// Load 0..15 bytes to XMM register from memory
// NOTE: elements of XMM are permuted [ 8 4 2 - 1 ]
template <char init>
static inline __m128i LoadPartialXmm(unsigned char *pSrc, mfxI32 len)
{
    __m128i xmm = _mm_set1_epi8(init);
    if (len & 8) {
        xmm = _mm_loadh_epi64(xmm, (__m64 *)pSrc);
        pSrc += 8;
    }
    if (len & 4) {
        xmm = _mm_insert_epi32(xmm, *((int *)pSrc), 1);
        pSrc += 4;
    }
    if (len & 2) {
        xmm = _mm_insert_epi16(xmm, *((short *)pSrc), 1);
        pSrc += 2;
    }
    if (len & 1) {
        xmm = _mm_insert_epi8(xmm, *pSrc, 0);
    }
    return xmm;
}

#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
// Load 0..31 bytes to YMM register from memory
// NOTE: elements of YMM are permuted [ 16 8 4 2 - 1 ]
template <char init>
static inline __m256i LoadPartialYmm(unsigned char *pSrc, mfxI32 len)
{
    __m128i xlo = _mm_set1_epi8(init);
    __m128i xhi = _mm_set1_epi8(init);
    if (len & 16) {
        xhi = _mm_loadu_si128((__m128i *)pSrc);
        pSrc += 16;
    }
    if (len & 8) {
        xlo = _mm_loadh_epi64(xlo, (__m64 *)pSrc);
        pSrc += 8;
    }
    if (len & 4) {
        xlo = _mm_insert_epi32(xlo, *((int *)pSrc), 1);
        pSrc += 4;
    }
    if (len & 2) {
        xlo = _mm_insert_epi16(xlo, *((short *)pSrc), 1);
        pSrc += 2;
    }
    if (len & 1) {
        xlo = _mm_insert_epi8(xlo, *pSrc, 0);
    }
    return _mm256_inserti128_si256(_mm256_castsi128_si256(xlo), xhi, 1);
}
#endif

/*
 *   RsCsCalc_diff versions
 */
#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
void RsCsCalc_diff_AVX2(pmfxF32 pRs0, pmfxF32 pCs0, pmfxF32 pRs1, pmfxF32 pCs1, int wblocks, int hblocks,
                        pmfxF32 pRsDiff, pmfxF32 pCsDiff)
{
    mfxI32 i, len = wblocks * hblocks;
    __m256d accRs = _mm256_setzero_pd();
    __m256d accCs = _mm256_setzero_pd();

    for (i = 0; i < len - 7; i += 8)
    {
        __m256 rs = _mm256_sub_ps(_mm256_loadu_ps(&pRs0[i]), _mm256_loadu_ps(&pRs1[i]));
        __m256 cs = _mm256_sub_ps(_mm256_loadu_ps(&pCs0[i]), _mm256_loadu_ps(&pCs1[i]));

        rs = _mm256_mul_ps(rs, rs);
        cs = _mm256_mul_ps(cs, cs);

        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_castps256_ps128(rs)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_castps256_ps128(cs)));
        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_extractf128_ps(rs, 1)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_extractf128_ps(cs, 1)));
    }

    if (i < len)
    {
        __m256 rs = _mm256_sub_ps(LoadPartialYmm(&pRs0[i], len & 0x7), LoadPartialYmm(&pRs1[i], len & 0x7));
        __m256 cs = _mm256_sub_ps(LoadPartialYmm(&pCs0[i], len & 0x7), LoadPartialYmm(&pCs1[i], len & 0x7));

        rs = _mm256_mul_ps(rs, rs);
        cs = _mm256_mul_ps(cs, cs);

        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_castps256_ps128(rs)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_castps256_ps128(cs)));
        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_extractf128_ps(rs, 1)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_extractf128_ps(cs, 1)));
    }

    // horizontal sum
    accRs = _mm256_hadd_pd(accRs, accCs);
    __m128d s = _mm_add_pd(_mm256_castpd256_pd128(accRs), _mm256_extractf128_pd(accRs, 1));

    __m128 t = _mm_cvtpd_ps(s);
    _mm_store_ss(pRsDiff, t);
    _mm_store_ss(pCsDiff, _mm_shuffle_ps(t, t, _MM_SHUFFLE(0,0,0,1)));
}
#endif

void RsCsCalc_diff_SSE4(pmfxF32 pRs0, pmfxF32 pCs0, pmfxF32 pRs1, pmfxF32 pCs1, int wblocks, int hblocks,
                        pmfxF32 pRsDiff, pmfxF32 pCsDiff)
{
    mfxI32 i, len = wblocks * hblocks;
    __m128d accRs = _mm_setzero_pd();
    __m128d accCs = _mm_setzero_pd();

    for (i = 0; i < len - 3; i += 4)
    {
        __m128 rs = _mm_sub_ps(_mm_loadu_ps(&pRs0[i]), _mm_loadu_ps(&pRs1[i]));
        __m128 cs = _mm_sub_ps(_mm_loadu_ps(&pCs0[i]), _mm_loadu_ps(&pCs1[i]));

        rs = _mm_mul_ps(rs, rs);
        cs = _mm_mul_ps(cs, cs);

        accRs = _mm_add_pd(accRs, _mm_cvtps_pd(rs));
        accCs = _mm_add_pd(accCs, _mm_cvtps_pd(cs));
        accRs = _mm_add_pd(accRs, _mm_cvtps_pd(_mm_movehl_ps(rs, rs)));
        accCs = _mm_add_pd(accCs, _mm_cvtps_pd(_mm_movehl_ps(cs, cs)));
    }

    if (i < len)
    {
        __m128 rs = _mm_sub_ps(LoadPartialXmm(&pRs0[i], len & 0x3), LoadPartialXmm(&pRs1[i], len & 0x3));
        __m128 cs = _mm_sub_ps(LoadPartialXmm(&pCs0[i], len & 0x3), LoadPartialXmm(&pCs1[i], len & 0x3));

        rs = _mm_mul_ps(rs, rs);
        cs = _mm_mul_ps(cs, cs);

        accRs = _mm_add_pd(accRs, _mm_cvtps_pd(rs));
        accCs = _mm_add_pd(accCs, _mm_cvtps_pd(cs));
        accRs = _mm_add_pd(accRs, _mm_cvtps_pd(_mm_movehl_ps(rs, rs)));
        accCs = _mm_add_pd(accCs, _mm_cvtps_pd(_mm_movehl_ps(cs, cs)));
    }

    // horizontal sum
    accRs = _mm_hadd_pd(accRs, accCs);

    __m128 t = _mm_cvtpd_ps(accRs);
    _mm_store_ss(pRsDiff, t);
    _mm_store_ss(pCsDiff, _mm_shuffle_ps(t, t, _MM_SHUFFLE(0,0,0,1)));
}

void RsCsCalc_diff_C(pmfxF32 pRs0, pmfxF32 pCs0, pmfxF32 pRs1, pmfxF32 pCs1, int wblocks, int hblocks,
                     pmfxF32 pRsDiff, pmfxF32 pCsDiff)
{
    mfxI32 len = wblocks * hblocks;
    double accRs = 0.0;
    double accCs = 0.0;

    for (mfxI32 i = 0; i < len; i++)
    {
        accRs += (pRs0[i] - pRs1[i]) * (pRs0[i] - pRs1[i]);
        accCs += (pCs0[i] - pCs1[i]) * (pCs0[i] - pCs1[i]);
    }
    *pRsDiff = (float)accRs;
    *pCsDiff = (float)accCs;
}

/*
 *  ImageDiffHistogram versions
 */
static const int HIST_THRESH_LO = 4;
static const int HIST_THRESH_HI = 12;

#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
void ImageDiffHistogram_AVX2(pmfxU8 pSrc, pmfxU8 pRef, int pitch, int width, int height,
                             mfxI32 histogram[5], mfxI64 *pSrcDC, mfxI64 *pRefDC)
{
    __m256i sDC = _mm256_setzero_si256();
    __m256i rDC = _mm256_setzero_si256();

    __m256i h0 = _mm256_setzero_si256();
    __m256i h1 = _mm256_setzero_si256();
    __m256i h2 = _mm256_setzero_si256();
    __m256i h3 = _mm256_setzero_si256();

    __m256i zero = _mm256_setzero_si256();

    for (mfxI32 i = 0; i < height; i++)
    {
        // process 32 pixels per iteration
        mfxI32 j;
        for (j = 0; j < width - 31; j += 32)
        {
            __m256i s = _mm256_loadu_si256((__m256i *)(&pSrc[j]));
            __m256i r = _mm256_loadu_si256((__m256i *)(&pRef[j]));

            sDC = _mm256_add_epi64(sDC, _mm256_sad_epu8(s, zero));    //accumulate horizontal sums
            rDC = _mm256_add_epi64(rDC, _mm256_sad_epu8(r, zero));

            r = _mm256_sub_epi8(r, _mm256_set1_epi8(-128));   // convert to signed
            s = _mm256_sub_epi8(s, _mm256_set1_epi8(-128));

            __m256i dn = _mm256_subs_epi8(r, s);   // -d saturated to [-128,127]
            __m256i dp = _mm256_subs_epi8(s, r);   // +d saturated to [-128,127]

            __m256i m0 = _mm256_cmpgt_epi8(dn, _mm256_set1_epi8(HIST_THRESH_HI)); // d < -12
            __m256i m1 = _mm256_cmpgt_epi8(dn, _mm256_set1_epi8(HIST_THRESH_LO)); // d < -4
            __m256i m2 = _mm256_cmpgt_epi8(_mm256_set1_epi8(HIST_THRESH_LO), dp); // d < +4
            __m256i m3 = _mm256_cmpgt_epi8(_mm256_set1_epi8(HIST_THRESH_HI), dp); // d < +12

            m0 = _mm256_sub_epi8(zero, m0);    // negate masks from 0xff to 1
            m1 = _mm256_sub_epi8(zero, m1);
            m2 = _mm256_sub_epi8(zero, m2);
            m3 = _mm256_sub_epi8(zero, m3);

            h0 = _mm256_add_epi32(h0, _mm256_sad_epu8(m0, zero)); // accumulate horizontal sums
            h1 = _mm256_add_epi32(h1, _mm256_sad_epu8(m1, zero));
            h2 = _mm256_add_epi32(h2, _mm256_sad_epu8(m2, zero));
            h3 = _mm256_add_epi32(h3, _mm256_sad_epu8(m3, zero));
        }

        // process remaining 1..31 pixels
        if (j < width)
        {
            __m256i s = LoadPartialYmm<0>(&pSrc[j], width & 0x1f);
            __m256i r = LoadPartialYmm<0>(&pRef[j], width & 0x1f);

            sDC = _mm256_add_epi64(sDC, _mm256_sad_epu8(s, zero));    //accumulate horizontal sums
            rDC = _mm256_add_epi64(rDC, _mm256_sad_epu8(r, zero));

            s = LoadPartialYmm<-1>(&pSrc[j], width & 0x1f);   // ensure unused elements not counted

            r = _mm256_sub_epi8(r, _mm256_set1_epi8(-128));   // convert to signed
            s = _mm256_sub_epi8(s, _mm256_set1_epi8(-128));

            __m256i dn = _mm256_subs_epi8(r, s);   // -d saturated to [-128,127]
            __m256i dp = _mm256_subs_epi8(s, r);   // +d saturated to [-128,127]

            __m256i m0 = _mm256_cmpgt_epi8(dn, _mm256_set1_epi8(HIST_THRESH_HI)); // d < -12
            __m256i m1 = _mm256_cmpgt_epi8(dn, _mm256_set1_epi8(HIST_THRESH_LO)); // d < -4
            __m256i m2 = _mm256_cmpgt_epi8(_mm256_set1_epi8(HIST_THRESH_LO), dp); // d < +4
            __m256i m3 = _mm256_cmpgt_epi8(_mm256_set1_epi8(HIST_THRESH_HI), dp); // d < +12

            m0 = _mm256_sub_epi8(zero, m0);    // negate masks from 0xff to 1
            m1 = _mm256_sub_epi8(zero, m1);
            m2 = _mm256_sub_epi8(zero, m2);
            m3 = _mm256_sub_epi8(zero, m3);

            h0 = _mm256_add_epi32(h0, _mm256_sad_epu8(m0, zero)); // accumulate horizontal sums
            h1 = _mm256_add_epi32(h1, _mm256_sad_epu8(m1, zero));
            h2 = _mm256_add_epi32(h2, _mm256_sad_epu8(m2, zero));
            h3 = _mm256_add_epi32(h3, _mm256_sad_epu8(m3, zero));
        }
        pSrc += pitch;
        pRef += pitch;
    }

    // finish horizontal sums
    __m128i tsDC = _mm_add_epi64(_mm256_castsi256_si128(sDC), _mm256_extractf128_si256(sDC, 1));
    __m128i trDC = _mm_add_epi64(_mm256_castsi256_si128(rDC), _mm256_extractf128_si256(rDC, 1));
    tsDC = _mm_add_epi64(tsDC, _mm_movehl_epi64(tsDC, tsDC));
    trDC = _mm_add_epi64(trDC, _mm_movehl_epi64(trDC, trDC));

    __m128i th0 = _mm_add_epi32(_mm256_castsi256_si128(h0), _mm256_extractf128_si256(h0, 1));
    __m128i th1 = _mm_add_epi32(_mm256_castsi256_si128(h1), _mm256_extractf128_si256(h1, 1));
    __m128i th2 = _mm_add_epi32(_mm256_castsi256_si128(h2), _mm256_extractf128_si256(h2, 1));
    __m128i th3 = _mm_add_epi32(_mm256_castsi256_si128(h3), _mm256_extractf128_si256(h3, 1));
    th0 = _mm_add_epi32(th0, _mm_movehl_epi64(th0, th0));
    th1 = _mm_add_epi32(th1, _mm_movehl_epi64(th1, th1));
    th2 = _mm_add_epi32(th2, _mm_movehl_epi64(th2, th2));
    th3 = _mm_add_epi32(th3, _mm_movehl_epi64(th3, th3));

    _mm_storel_epi64((__m128i *)pSrcDC, tsDC);
    _mm_storel_epi64((__m128i *)pRefDC, trDC);

    histogram[0] = _mm_cvtsi128_si32(th0);
    histogram[1] = _mm_cvtsi128_si32(th1);
    histogram[2] = _mm_cvtsi128_si32(th2);
    histogram[3] = _mm_cvtsi128_si32(th3);
    histogram[4] = width * height;

    // undo cumulative counts, by differencing
    histogram[4] -= histogram[3];
    histogram[3] -= histogram[2];
    histogram[2] -= histogram[1];
    histogram[1] -= histogram[0];
}
#endif

void ImageDiffHistogram_SSE4(pmfxU8 pSrc, pmfxU8 pRef, int pitch, int width, int height,
                             mfxI32 histogram[5], mfxI64 *pSrcDC, mfxI64 *pRefDC)
{
    __m128i sDC = _mm_setzero_si128();
    __m128i rDC = _mm_setzero_si128();

    __m128i h0 = _mm_setzero_si128();
    __m128i h1 = _mm_setzero_si128();
    __m128i h2 = _mm_setzero_si128();
    __m128i h3 = _mm_setzero_si128();

    __m128i zero = _mm_setzero_si128();

    for (mfxI32 i = 0; i < height; i++)
    {
        // process 16 pixels per iteration
        mfxI32 j;
        for (j = 0; j < width - 15; j += 16)
        {
            __m128i s = _mm_loadu_si128((__m128i *)(&pSrc[j]));
            __m128i r = _mm_loadu_si128((__m128i *)(&pRef[j]));

            sDC = _mm_add_epi64(sDC, _mm_sad_epu8(s, zero));    //accumulate horizontal sums
            rDC = _mm_add_epi64(rDC, _mm_sad_epu8(r, zero));

            r = _mm_sub_epi8(r, _mm_set1_epi8(-128));   // convert to signed
            s = _mm_sub_epi8(s, _mm_set1_epi8(-128));

            __m128i dn = _mm_subs_epi8(r, s);   // -d saturated to [-128,127]
            __m128i dp = _mm_subs_epi8(s, r);   // +d saturated to [-128,127]

            __m128i m0 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(HIST_THRESH_HI)); // d < -12
            __m128i m1 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(HIST_THRESH_LO)); // d < -4
            __m128i m2 = _mm_cmpgt_epi8(_mm_set1_epi8(HIST_THRESH_LO), dp); // d < +4
            __m128i m3 = _mm_cmpgt_epi8(_mm_set1_epi8(HIST_THRESH_HI), dp); // d < +12

            m0 = _mm_sub_epi8(zero, m0);    // negate masks from 0xff to 1
            m1 = _mm_sub_epi8(zero, m1);
            m2 = _mm_sub_epi8(zero, m2);
            m3 = _mm_sub_epi8(zero, m3);

            h0 = _mm_add_epi32(h0, _mm_sad_epu8(m0, zero)); // accumulate horizontal sums
            h1 = _mm_add_epi32(h1, _mm_sad_epu8(m1, zero));
            h2 = _mm_add_epi32(h2, _mm_sad_epu8(m2, zero));
            h3 = _mm_add_epi32(h3, _mm_sad_epu8(m3, zero));
        }

        // process remaining 1..15 pixels
        if (j < width)
        {
            __m128i s = LoadPartialXmm<0>(&pSrc[j], width & 0xf);
            __m128i r = LoadPartialXmm<0>(&pRef[j], width & 0xf);

            sDC = _mm_add_epi64(sDC, _mm_sad_epu8(s, zero));    //accumulate horizontal sums
            rDC = _mm_add_epi64(rDC, _mm_sad_epu8(r, zero));

            s = LoadPartialXmm<-1>(&pSrc[j], width & 0xf);      // ensure unused elements not counted

            r = _mm_sub_epi8(r, _mm_set1_epi8(-128));   // convert to signed
            s = _mm_sub_epi8(s, _mm_set1_epi8(-128));

            __m128i dn = _mm_subs_epi8(r, s);   // -d saturated to [-128,127]
            __m128i dp = _mm_subs_epi8(s, r);   // +d saturated to [-128,127]

            __m128i m0 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(HIST_THRESH_HI)); // d < -12
            __m128i m1 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(HIST_THRESH_LO)); // d < -4
            __m128i m2 = _mm_cmpgt_epi8(_mm_set1_epi8(HIST_THRESH_LO), dp); // d < +4
            __m128i m3 = _mm_cmpgt_epi8(_mm_set1_epi8(HIST_THRESH_HI), dp); // d < +12

            m0 = _mm_sub_epi8(zero, m0);    // negate masks from 0xff to 1
            m1 = _mm_sub_epi8(zero, m1);
            m2 = _mm_sub_epi8(zero, m2);
            m3 = _mm_sub_epi8(zero, m3);

            h0 = _mm_add_epi32(h0, _mm_sad_epu8(m0, zero)); // accumulate horizontal sums
            h1 = _mm_add_epi32(h1, _mm_sad_epu8(m1, zero));
            h2 = _mm_add_epi32(h2, _mm_sad_epu8(m2, zero));
            h3 = _mm_add_epi32(h3, _mm_sad_epu8(m3, zero));
        }
        pSrc += pitch;
        pRef += pitch;
    }

    // finish horizontal sums
    sDC = _mm_add_epi64(sDC, _mm_movehl_epi64(sDC, sDC));
    rDC = _mm_add_epi64(rDC, _mm_movehl_epi64(rDC, rDC));

    h0 = _mm_add_epi32(h0, _mm_movehl_epi64(h0, h0));
    h1 = _mm_add_epi32(h1, _mm_movehl_epi64(h1, h1));
    h2 = _mm_add_epi32(h2, _mm_movehl_epi64(h2, h2));
    h3 = _mm_add_epi32(h3, _mm_movehl_epi64(h3, h3));

    _mm_storel_epi64((__m128i *)pSrcDC, sDC);
    _mm_storel_epi64((__m128i *)pRefDC, rDC);

    histogram[0] = _mm_cvtsi128_si32(h0);
    histogram[1] = _mm_cvtsi128_si32(h1);
    histogram[2] = _mm_cvtsi128_si32(h2);
    histogram[3] = _mm_cvtsi128_si32(h3);
    histogram[4] = width * height;

    // undo cumulative counts, by differencing
    histogram[4] -= histogram[3];
    histogram[3] -= histogram[2];
    histogram[2] -= histogram[1];
    histogram[1] -= histogram[0];
}

void ImageDiffHistogram_C(pmfxU8 pSrc, pmfxU8 pRef, int pitch, int width, int height,
                          mfxI32 histogram[5], mfxI64 *pSrcDC, mfxI64 *pRefDC)
{
    mfxI64 srcDC = 0;
    mfxI64 refDC = 0;

    histogram[0] = 0;
    histogram[1] = 0;
    histogram[2] = 0;
    histogram[3] = 0;
    histogram[4] = 0;

    for (mfxI32 i = 0; i < height; i++)
    {
        for (mfxI32 j = 0; j < width; j++)
        {
            int s = pSrc[j];
            int r = pRef[j];
            int d = s - r;

            srcDC += s;
            refDC += r;

            if (d < -HIST_THRESH_HI)
                histogram[0]++;
            else if (d < -HIST_THRESH_LO)
                histogram[1]++;
            else if (d < HIST_THRESH_LO)
                histogram[2]++;
            else if (d < HIST_THRESH_HI)
                histogram[3]++;
            else
                histogram[4]++;
        }
        pSrc += pitch;
        pRef += pitch;
    }
    *pSrcDC = srcDC;
    *pRefDC = refDC;
}

/*
 * RsCsCalc_4x4 Versions
 */

#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
void RsCsCalc_4x4_AVX2(pmfxU8 pSrc, int srcPitch, int wblocks, int hblocks, pmfxF32 pRs, pmfxF32 pCs)
{
    for (mfxI32 i = 0; i < hblocks; i++)
    {
        // 4 horizontal blocks at a time
        mfxI32 j;
        for (j = 0; j < wblocks - 3; j += 4)
        {
            __m256i rs = _mm256_setzero_si256();
            __m256i cs = _mm256_setzero_si256();
            __m256i a0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[-srcPitch + 0]));

#ifdef __INTEL_COMPILER
#pragma unroll(4)
#endif
            for (mfxI32 k = 0; k < 4; k++)
            {
                __m256i b0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[-1]));
                __m256i c0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pSrc[0]));
                pSrc += srcPitch;

                // accRs += dRs * dRs
                a0 = _mm256_sub_epi16(c0, a0);
                a0 = _mm256_madd_epi16(a0, a0);
                rs = _mm256_add_epi32(rs, a0);

                // accCs += dCs * dCs
                b0 = _mm256_sub_epi16(c0, b0);
                b0 = _mm256_madd_epi16(b0, b0);
                cs = _mm256_add_epi32(cs, b0);

                // reuse next iteration
                a0 = c0;
            }

            // horizontal sum
            rs = _mm256_hadd_epi32(rs, cs);
            rs = _mm256_permute4x64_epi64(rs, _MM_SHUFFLE(3, 1, 2, 0));    // [ cs3 cs2 cs1 cs0 rs3 rs2 rs1 rs0 ]

            // scale by 1.0f/N and store
            __m256 t = _mm256_mul_ps(_mm256_cvtepi32_ps(rs), _mm256_set1_ps(1.0f / 16));
            _mm_storeu_ps(&pRs[i * wblocks + j], _mm256_extractf128_ps(t, 0));
            _mm_storeu_ps(&pCs[i * wblocks + j], _mm256_extractf128_ps(t, 1));

            pSrc -= 4 * srcPitch;
            pSrc += 16;
        }

        // remaining blocks
        for (; j < wblocks; j++)
        {
            mfxI32 accRs = 0;
            mfxI32 accCs = 0;

            for (mfxI32 k = 0; k < 4; k++)
            {
                for (mfxI32 l = 0; l < 4; l++)
                {
                    mfxI32 dRs = pSrc[l] - pSrc[l - srcPitch];
                    mfxI32 dCs = pSrc[l] - pSrc[l - 1];
                    accRs += dRs * dRs;
                    accCs += dCs * dCs;
                }
                pSrc += srcPitch;
            }
            pRs[i * wblocks + j] = accRs * (1.0f / 16);
            pCs[i * wblocks + j] = accCs * (1.0f / 16);

            pSrc -= 4 * srcPitch;
            pSrc += 4;
        }
        pSrc -= 4 * wblocks;
        pSrc += 4 * srcPitch;
    }
}
#endif

void RsCsCalc_4x4_SSE4(pmfxU8 pSrc, int srcPitch, int wblocks, int hblocks, pmfxF32 pRs, pmfxF32 pCs)
{
    for (mfxI32 i = 0; i < hblocks; i++)
    {
        // 4 horizontal blocks at a time
        mfxI32 j;
        for (j = 0; j < wblocks - 3; j += 4)
        {
            __m128i rs = _mm_setzero_si128();
            __m128i cs = _mm_setzero_si128();
            __m128i a0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-srcPitch + 0]));
            __m128i a1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-srcPitch + 8]));

#ifdef __INTEL_COMPILER
#pragma unroll(4)
#endif
            for (mfxI32 k = 0; k < 4; k++)
            {
                __m128i b0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-1]));
                __m128i b1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[7]));
                __m128i c0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[0]));
                __m128i c1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[8]));
                pSrc += srcPitch;

                // accRs += dRs * dRs
                a0 = _mm_sub_epi16(c0, a0);
                a1 = _mm_sub_epi16(c1, a1);
                a0 = _mm_madd_epi16(a0, a0);
                a1 = _mm_madd_epi16(a1, a1);
                a0 = _mm_hadd_epi32(a0, a1);
                rs = _mm_add_epi32(rs, a0);

                // accCs += dCs * dCs
                b0 = _mm_sub_epi16(c0, b0);
                b1 = _mm_sub_epi16(c1, b1);
                b0 = _mm_madd_epi16(b0, b0);
                b1 = _mm_madd_epi16(b1, b1);
                b0 = _mm_hadd_epi32(b0, b1);
                cs = _mm_add_epi32(cs, b0);

                // reuse next iteration
                a0 = c0;
                a1 = c1;
            }

            // scale by 1.0f/N and store
            _mm_storeu_ps(&pRs[i * wblocks + j], _mm_mul_ps(_mm_cvtepi32_ps(rs), _mm_set1_ps(1.0f / 16)));
            _mm_storeu_ps(&pCs[i * wblocks + j], _mm_mul_ps(_mm_cvtepi32_ps(cs), _mm_set1_ps(1.0f / 16)));

            pSrc -= 4 * srcPitch;
            pSrc += 16;
        }

        // remaining blocks
        for (; j < wblocks; j++)
        {
            mfxI32 accRs = 0;
            mfxI32 accCs = 0;

            for (mfxI32 k = 0; k < 4; k++)
            {
                for (mfxI32 l = 0; l < 4; l++)
                {
                    mfxI32 dRs = pSrc[l] - pSrc[l - srcPitch];
                    mfxI32 dCs = pSrc[l] - pSrc[l - 1];
                    accRs += dRs * dRs;
                    accCs += dCs * dCs;
                }
                pSrc += srcPitch;
            }
            pRs[i * wblocks + j] = accRs * (1.0f / 16);
            pCs[i * wblocks + j] = accCs * (1.0f / 16);

            pSrc -= 4 * srcPitch;
            pSrc += 4;
        }
        pSrc -= 4 * wblocks;
        pSrc += 4 * srcPitch;
    }
}

void RsCsCalc_4x4_C(pmfxU8 pSrc, int srcPitch, int wblocks, int hblocks, pmfxF32 pRs, pmfxF32 pCs)
{
    for (mfxI32 i = 0; i < hblocks; i++)
    {
        for (mfxI32 j = 0; j < wblocks; j++)
        {
            mfxI32 accRs = 0;
            mfxI32 accCs = 0;

            for (mfxI32 k = 0; k < 4; k++)
            {
                for (mfxI32 l = 0; l < 4; l++)
                {
                    mfxI32 dRs = pSrc[l] - pSrc[l - srcPitch];
                    mfxI32 dCs = pSrc[l] - pSrc[l - 1];
                    accRs += dRs * dRs;
                    accCs += dCs * dCs;
                }
                pSrc += srcPitch;
            }
            pRs[i * wblocks + j] = accRs * (1.0f / 16);
            pCs[i * wblocks + j] = accCs * (1.0f / 16);

            pSrc -= 4 * srcPitch;
            pSrc += 4;
        }
        pSrc -= 4 * wblocks;
        pSrc += 4 * srcPitch;
    }
}

/*
 * RsCsCalc_sqrt2 Versions
 */

#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
void RsCsCalc_sqrt2_AVX2(pmfxF32 pRs, pmfxF32 pCs, pmfxF32 pRsCs, pmfxF32 pRsFrame, pmfxF32 pCsFrame, int wblocks, int hblocks)
{
    mfxI32 i, len = wblocks * hblocks;
    __m256d accRs = _mm256_setzero_pd();
    __m256d accCs = _mm256_setzero_pd();
    __m256 zero = _mm256_setzero_ps();

    for (i = 0; i < len - 7; i += 8)
    {
        __m256 rs = _mm256_loadu_ps(&pRs[i]);
        __m256 cs = _mm256_loadu_ps(&pCs[i]);
        __m256 rc = _mm256_add_ps(rs, cs);

        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_castps256_ps128(rs)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_castps256_ps128(cs)));
        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_extractf128_ps(rs, 1)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_extractf128_ps(cs, 1)));

        // x = x * rsqrt(x), with rsqrt(0.0f) flushed to 0.0f
        rs = _mm256_mul_ps(rs, _mm256_andnot_ps(_mm256_cmp_ps(rs, zero, _CMP_EQ_OQ), _mm256_rsqrt_ps(rs)));
        cs = _mm256_mul_ps(cs, _mm256_andnot_ps(_mm256_cmp_ps(cs, zero, _CMP_EQ_OQ), _mm256_rsqrt_ps(cs)));
        rc = _mm256_mul_ps(rc, _mm256_andnot_ps(_mm256_cmp_ps(rc, zero, _CMP_EQ_OQ), _mm256_rsqrt_ps(rc)));

        _mm256_storeu_ps(&pRs[i], rs);
        _mm256_storeu_ps(&pCs[i], cs);
        _mm256_storeu_ps(&pRsCs[i], rc);
    }

    if (i < len)
    {
        __m256 rs = LoadPartialYmm(&pRs[i], len & 0x7);
        __m256 cs = LoadPartialYmm(&pCs[i], len & 0x7);
        __m256 rc = _mm256_add_ps(rs, cs);

        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_castps256_ps128(rs)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_castps256_ps128(cs)));
        accRs = _mm256_add_pd(accRs, _mm256_cvtps_pd(_mm256_extractf128_ps(rs, 1)));
        accCs = _mm256_add_pd(accCs, _mm256_cvtps_pd(_mm256_extractf128_ps(cs, 1)));

        // x = x * rsqrt(x), with rsqrt(0.0f) flushed to 0.0f
        rs = _mm256_mul_ps(rs, _mm256_andnot_ps(_mm256_cmp_ps(rs, zero, _CMP_EQ_OQ), _mm256_rsqrt_ps(rs)));
        cs = _mm256_mul_ps(cs, _mm256_andnot_ps(_mm256_cmp_ps(cs, zero, _CMP_EQ_OQ), _mm256_rsqrt_ps(cs)));
        rc = _mm256_mul_ps(rc, _mm256_andnot_ps(_mm256_cmp_ps(rc, zero, _CMP_EQ_OQ), _mm256_rsqrt_ps(rc)));

        StorePartialYmm(&pRs[i], rs, len & 0x7);
        StorePartialYmm(&pCs[i], cs, len & 0x7);
        StorePartialYmm(&pRsCs[i], rc, len & 0x7);
    }

    // horizontal sum
    accRs = _mm256_hadd_pd(accRs, accCs);
    __m128d s = _mm_add_pd(_mm256_castpd256_pd128(accRs), _mm256_extractf128_pd(accRs, 1));

    s = _mm_sqrt_pd(_mm_div_pd(s, _mm_set1_pd(hblocks * wblocks)));

    __m128 t = _mm_cvtpd_ps(s);
    _mm_store_ss(pRsFrame, t);
    _mm_store_ss(pCsFrame, _mm_shuffle_ps(t, t, _MM_SHUFFLE(0,0,0,1)));
}
#endif

void RsCsCalc_sqrt2_SSE4(pmfxF32 pRs, pmfxF32 pCs, pmfxF32 pRsCs, pmfxF32 pRsFrame, pmfxF32 pCsFrame, int wblocks, int hblocks)
{
    mfxI32 i, len = wblocks * hblocks;
    __m128d accRs = _mm_setzero_pd();
    __m128d accCs = _mm_setzero_pd();
    __m128 zero = _mm_setzero_ps();

    for (i = 0; i < len - 3; i += 4)
    {
        __m128 rs = _mm_loadu_ps(&pRs[i]);
        __m128 cs = _mm_loadu_ps(&pCs[i]);
        __m128 rc = _mm_add_ps(rs, cs);

        accRs = _mm_add_pd(accRs, _mm_cvtps_pd(rs));
        accCs = _mm_add_pd(accCs, _mm_cvtps_pd(cs));
        accRs = _mm_add_pd(accRs, _mm_cvtps_pd(_mm_movehl_ps(rs, rs)));
        accCs = _mm_add_pd(accCs, _mm_cvtps_pd(_mm_movehl_ps(cs, cs)));

        // x = x * rsqrt(x), with rsqrt(0.0f) flushed to 0.0f
        rs = _mm_mul_ps(rs, _mm_andnot_ps(_mm_cmpeq_ps(rs, zero), _mm_rsqrt_ps(rs)));
        cs = _mm_mul_ps(cs, _mm_andnot_ps(_mm_cmpeq_ps(cs, zero), _mm_rsqrt_ps(cs)));
        rc = _mm_mul_ps(rc, _mm_andnot_ps(_mm_cmpeq_ps(rc, zero), _mm_rsqrt_ps(rc)));

        _mm_storeu_ps(&pRs[i], rs);
        _mm_storeu_ps(&pCs[i], cs);
        _mm_storeu_ps(&pRsCs[i], rc);
    }

    if (i < len)
    {
        __m128 rs = LoadPartialXmm(&pRs[i], len & 0x3);
        __m128 cs = LoadPartialXmm(&pCs[i], len & 0x3);
        __m128 rc = _mm_add_ps(rs, cs);

        accRs = _mm_add_pd(accRs, _mm_cvtps_pd(rs));
        accCs = _mm_add_pd(accCs, _mm_cvtps_pd(cs));
        accRs = _mm_add_pd(accRs, _mm_cvtps_pd(_mm_movehl_ps(rs, rs)));
        accCs = _mm_add_pd(accCs, _mm_cvtps_pd(_mm_movehl_ps(cs, cs)));

        // x = x * rsqrt(x), with rsqrt(0.0f) flushed to 0.0f
        rs = _mm_mul_ps(rs, _mm_andnot_ps(_mm_cmpeq_ps(rs, zero), _mm_rsqrt_ps(rs)));
        cs = _mm_mul_ps(cs, _mm_andnot_ps(_mm_cmpeq_ps(cs, zero), _mm_rsqrt_ps(cs)));
        rc = _mm_mul_ps(rc, _mm_andnot_ps(_mm_cmpeq_ps(rc, zero), _mm_rsqrt_ps(rc)));

        StorePartialXmm(&pRs[i], rs, len & 0x3);
        StorePartialXmm(&pCs[i], cs, len & 0x3);
        StorePartialXmm(&pRsCs[i], rc, len & 0x3);
    }

    // horizontal sum
    accRs = _mm_hadd_pd(accRs, accCs);

    accRs = _mm_sqrt_pd(_mm_div_pd(accRs, _mm_set1_pd(hblocks * wblocks)));

    __m128 t = _mm_cvtpd_ps(accRs);
    _mm_store_ss(pRsFrame, t);
    _mm_store_ss(pCsFrame, _mm_shuffle_ps(t, t, _MM_SHUFFLE(0,0,0,1)));
}

void RsCsCalc_sqrt2_C(pmfxF32 pRs, pmfxF32 pCs, pmfxF32 pRsCs, pmfxF32 pRsFrame, pmfxF32 pCsFrame, int wblocks, int hblocks)
{
    mfxI32 len = wblocks * hblocks;
    double accRs = 0.0;
    double accCs = 0.0;

    for (mfxI32 i = 0; i < len; i++)
    {
        float rs = pRs[i];
        float cs = pCs[i];
        accRs += rs;
        accCs += cs;
        pRs[i] = sqrt(rs);
        pCs[i] = sqrt(cs);
        pRsCs[i] = sqrt(rs + cs);
    }

    *pRsFrame = (float)sqrt(accRs / (hblocks * wblocks));
    *pCsFrame = (float)sqrt(accCs / (hblocks * wblocks));
}


void SceneChangeDetector::RsCsCalc(imageData *exBuffer, ImDetails vidCar) {
    pmfxU8
        ss = exBuffer->Image.Y;
    mfxI32
        hblocks = (mfxI32)vidCar._cheight / BLOCK_SIZE,
        wblocks = (mfxI32)vidCar._cwidth / BLOCK_SIZE;

    SCD_CPU_DISP(m_cpuOpt, RsCsCalc_4x4, ss, vidCar.Extended_Width, wblocks, hblocks, exBuffer->Rs, exBuffer->Cs);
    SCD_CPU_DISP(m_cpuOpt, RsCsCalc_sqrt2, exBuffer->Rs, exBuffer->Cs, exBuffer->RsCs, &exBuffer->RsVal, &exBuffer->CsVal, wblocks, hblocks);
}

mfxStatus SceneChangeDetector::MapFrame(mfxFrameSurface1 *frame)
{
    MFX_CHECK_NULL_PTR1(frame);
    CmDevice* cmdevice  = device->GetDevice();
    MFX_CHECK_NULL_PTR1(cmdevice);
    CmSurface2D *surface;

    std::map<void *, CmSurface2D *>::iterator it;
    it = m_tableCmRelations.find(frame->Data.MemId);

    if (m_tableCmRelations.end() == it)
    {
        int res = cmdevice->CreateSurface2D(frame->Data.MemId, surface);
        if (res!=0)
        {
            return MFX_ERR_DEVICE_FAILED;
        }
        m_tableCmRelations.insert(std::pair<void *, CmSurface2D *>(frame->Data.MemId, surface));
    }
    else
    {
        surface = it->second;
    }

    kernel_p->SetKernelArg(0, *surface);
    kernel_p->SetKernelArg(1, *surfaceOut);
    kernel_p->SetKernelArg(2, subWidth);
    kernel_p->SetKernelArg(3, gpustep_w);
    kernel_p->SetKernelArg(4, gpustep_h);
    kernel_p->SetThreadCount(threadSpace->ThreadCount());

    kernel_t->SetKernelArg(0, *surface);
    kernel_t->SetKernelArg(1, *surfaceOut);
    kernel_t->SetKernelArg(2, subWidth);
    kernel_t->SetKernelArg(3, gpustep_w);
    kernel_t->SetKernelArg(4, gpustep_h / 2);   // interlaced
    kernel_t->SetThreadCount(threadSpace->ThreadCount());

    kernel_b->SetKernelArg(0, *surface);
    kernel_b->SetKernelArg(1, *surfaceOut);
    kernel_b->SetKernelArg(2, subWidth);
    kernel_b->SetKernelArg(3, gpustep_w);
    kernel_b->SetKernelArg(4, gpustep_h / 2);   // interlaced
    kernel_b->SetThreadCount(threadSpace->ThreadCount());
    return MFX_ERR_NONE;
}

void SceneChangeDetector::Setup_UFast_Environment() {

    static mfxU32 scaleVal = 8;

    m_dataIn->accuracy = 1;

    m_dataIn->layer[0].Original_Width  = SMALL_WIDTH;
    m_dataIn->layer[0].Original_Height = SMALL_HEIGHT;
    m_dataIn->layer[0]._cwidth  = SMALL_WIDTH;
    m_dataIn->layer[0]._cheight = SMALL_HEIGHT;

    m_dataIn->layer[0].block_width = 8;
    m_dataIn->layer[0].block_height = 8;
    m_dataIn->layer[0].vertical_pad = 16 / scaleVal;
    m_dataIn->layer[0].horizontal_pad = 16 / scaleVal;
    m_dataIn->layer[0].Extended_Height = m_dataIn->layer[0].vertical_pad + m_dataIn->layer[0]._cheight + m_dataIn->layer[0].vertical_pad;
    m_dataIn->layer[0].Extended_Width = m_dataIn->layer[0].horizontal_pad + m_dataIn->layer[0]._cwidth + m_dataIn->layer[0].horizontal_pad;
    m_dataIn->layer[0].pitch = m_dataIn->layer[0].Extended_Width;
    m_dataIn->layer[0].Total_non_corrected_pixels = m_dataIn->layer[0].Original_Height * m_dataIn->layer[0].Original_Width * 6 / CHROMASUBSAMPLE;
    m_dataIn->layer[0].Pixels_in_Y_layer = m_dataIn->layer[0]._cwidth * m_dataIn->layer[0]._cheight;
    m_dataIn->layer[0].Pixels_in_U_layer = m_dataIn->layer[0].Pixels_in_Y_layer / CHROMASUBSAMPLE;
    m_dataIn->layer[0].Pixels_in_V_layer = m_dataIn->layer[0].Pixels_in_Y_layer / CHROMASUBSAMPLE;
    m_dataIn->layer[0].Pixels_in_full_frame = m_dataIn->layer[0].Pixels_in_Y_layer + m_dataIn->layer[0].Pixels_in_U_layer + m_dataIn->layer[0].Pixels_in_V_layer;
    m_dataIn->layer[0].Pixels_in_block = m_dataIn->layer[0].block_height * m_dataIn->layer[0].block_width;
    m_dataIn->layer[0].Height_in_blocks = m_dataIn->layer[0]._cheight / m_dataIn->layer[0].block_height;
    m_dataIn->layer[0].Width_in_blocks = m_dataIn->layer[0]._cwidth / m_dataIn->layer[0].block_width;
    m_dataIn->layer[0].Blocks_in_Frame = (m_dataIn->layer[0]._cheight / m_dataIn->layer[0].block_height) * (m_dataIn->layer[0]._cwidth / m_dataIn->layer[0].block_width);
    m_dataIn->layer[0].sidesize = m_dataIn->layer[0]._cheight + (1 * m_dataIn->layer[0].vertical_pad);
    m_dataIn->layer[0].initial_point = (m_dataIn->layer[0].Extended_Width * m_dataIn->layer[0].vertical_pad) + m_dataIn->layer[0].horizontal_pad;
    m_dataIn->layer[0].endPoint = ((m_dataIn->layer[0]._cheight + m_dataIn->layer[0].vertical_pad) * m_dataIn->layer[0].Extended_Width) - m_dataIn->layer[0].horizontal_pad;
    m_dataIn->layer[0].MVspaceSize = (m_dataIn->layer[0]._cheight / m_dataIn->layer[0].block_height) * (m_dataIn->layer[0]._cwidth / m_dataIn->layer[0].block_width);
}

void SceneChangeDetector::ImDetails_Init(ImDetails *Rdata) {
    Rdata->block_height = 0;
    Rdata->block_width = 0;
    Rdata->horizontal_pad = 0;
    Rdata->vertical_pad = 0;
    Rdata->Original_Height = 0;
    Rdata->Original_Width = 0;
    Rdata->Pixels_in_Y_layer = 0;
    Rdata->Pixels_in_U_layer = 0;
    Rdata->Pixels_in_V_layer = 0;
    Rdata->Pixels_in_full_frame = 0;
    Rdata->Blocks_in_Frame = 0;
    Rdata->endPoint = 0;
    Rdata->Extended_Width = 0;
    Rdata->Extended_Height = 0;
    Rdata->Height_in_blocks = 0;
    Rdata->initial_point = 0;
    Rdata->MVspaceSize = 0;
    Rdata->pitch = 0;
    Rdata->Pixels_in_block = 0;
    Rdata->sidesize = 0;
    Rdata->Total_non_corrected_pixels = 0;
    Rdata->Width_in_blocks = 0;
    Rdata->_cheight = 0;
    Rdata->_cwidth = 0;
}

void SceneChangeDetector::Params_Init() {
    m_dataIn->accuracy  = 1;
    m_dataIn->processed_frames = 0;
    m_dataIn->total_number_of_frames = -1;
    m_dataIn->starting_frame = 0;
    m_dataIn->key_frame_frequency = INT_MAX;
    m_dataIn->limitRange = 0;
    m_dataIn->maxXrange = 32;
    m_dataIn->maxYrange = 32;
    m_dataIn->interlaceMode = 0;
    m_dataIn->StartingField = TopField;
    m_dataIn->currentField  = TopField;

    ImDetails_Init(&m_dataIn->layer[0]);
}


void SceneChangeDetector::ShotDetect(imageData Data, imageData DataRef, ImDetails imageInfo, TSCstat *current, TSCstat *reference)
{
    pmfxU8
        ssFrame = Data.Image.Y,
        refFrame = DataRef.Image.Y;
    pmfxF32
        objRs = Data.Rs,
        objCs = Data.Cs,
        refRs = DataRef.Rs,
        refCs = DataRef.Cs;
    mfxF64
        histTOT = 0.0;

    current->RsCsDiff = 0.0;
    current->Schg = -1;
    current->Gchg = 0;

    SCD_CPU_DISP(m_cpuOpt, RsCsCalc_diff, objRs, objCs, refRs, refCs, 2*imageInfo.Width_in_blocks, 2*imageInfo.Height_in_blocks, &current->RsDiff, &current->CsDiff);
    SCD_CPU_DISP(m_cpuOpt, ImageDiffHistogram, ssFrame, refFrame, imageInfo.Extended_Width, imageInfo._cwidth, imageInfo._cheight, current->histogram, &current->ssDCint, &current->refDCint);

    if(reference->Schg)
        current->last_shot_distance = 1;
    else
        current->last_shot_distance++;

    current->RsDiff /= imageInfo.Blocks_in_Frame * (2 / (8 / imageInfo.block_height)) * (2 / (8 / imageInfo.block_width));
    current->CsDiff /= imageInfo.Blocks_in_Frame * (2 / (8 / imageInfo.block_height)) * (2 / (8 / imageInfo.block_width));
    current->RsCsDiff = (mfxF32)sqrt((current->RsDiff*current->RsDiff) + (current->CsDiff*current->CsDiff));
    histTOT = (mfxF32)(current->histogram[0] + current->histogram[1] + current->histogram[2] + current->histogram[3] + current->histogram[4]);
    current->ssDCval = current->ssDCint * (1.0 / imageInfo.Pixels_in_Y_layer);
    current->refDCval = current->refDCint * (1.0 / imageInfo.Pixels_in_Y_layer);
    current->gchDC = NABS(current->ssDCval - current->refDCval);
    current->posBalance = (mfxF32)(current->histogram[3] + current->histogram[4]);
    current->negBalance = (mfxF32)(current->histogram[0] + current->histogram[1]);
    current->posBalance -= current->negBalance;
    current->posBalance = NABS(current->posBalance);
    current->posBalance /= histTOT;
    current->negBalance /= histTOT;
    if ((current->RsCsDiff > 7.6) && (current->RsCsDiff < 10) && (current->gchDC > 2.9) && (current->posBalance > 0.18))
        current->Gchg = TRUE;
    if ((current->RsCsDiff >= 10) && (current->gchDC > 1.6) && (current->posBalance > 0.18))
        current->Gchg = TRUE;
    current->diffAFD = current->AFD - reference->AFD;
    current->diffTSC = current->TSC - reference->TSC;
    current->diffRsCsDiff = current->RsCsDiff - reference->RsCsDiff;
    current->diffMVdiffVal = current->MVdiffVal - reference->MVdiffVal;
    current->Schg = SCDetectGPU(current->diffMVdiffVal, current->RsCsDiff, current->MVdiffVal, current->Rs, current->AFD, current->CsDiff, current->diffTSC, current->TSC, current->gchDC, current->diffRsCsDiff, current->posBalance, current->SC, current->TSCindex, current->SCindex, current->Cs, current->diffAFD, current->negBalance, current->ssDCval, current->refDCval, current->RsDiff);
    reference->lastFrameInShot = (mfxU8)current->Schg;
}


void CorrectionForGoPSize(VidRead *support, mfxU32 PdIndex) {
    support->detectedSch = 0;
    if(support->logic[PdIndex]->Schg) {
        if(support->lastSCdetectionDistance % support->gopSize)
            support->pendingSch = 1;
        else {
            support->lastSCdetectionDistance = 0;
            support->pendingSch = 0;
            support->detectedSch = 1;
        }
    }
    else if(support->pendingSch) {
        if(!(support->lastSCdetectionDistance % support->gopSize)) {
            support->lastSCdetectionDistance = 0;
            support->pendingSch = 0;
            support->detectedSch = 1;
        }
    }
    support->lastSCdetectionDistance++;
}

BOOL SceneChangeDetector::CompareStats(mfxU8 current, mfxU8 reference, BOOL isInterlaced){
    if(current > 2 || reference > 2 || current == reference) {
        printf("SCD Error: Invalid stats comparison\n");
        exit(-666);
    }
    mfxU8 comparison = 0;
    comparison += abs(support->logic[current]->Rs - support->logic[reference]->Rs) < 0.075;
    comparison += abs(support->logic[current]->Cs - support->logic[reference]->Cs) < 0.075;
    comparison += abs(support->logic[current]->SC - support->logic[reference]->SC) < 0.075;
    comparison += (!isInterlaced && support->logic[current]->TSCindex == 0) || isInterlaced;
    if(comparison == 4)
        return Same;
    return Not_same;
}

BOOL SceneChangeDetector::FrameRepeatCheck(BOOL isInterlaced) {
    mfxU8 reference = previous_frame_data;
    if(isInterlaced)
        reference = previous_previous_frame_data;
    return(CompareStats(current_frame_data, reference, isInterlaced));
}

void SceneChangeDetector::processFrame() {
    support->logic[current_frame_data]->frameNum = videoData[Current_Frame]->frame_number;
    support->logic[current_frame_data]->firstFrame = support->firstFrame;
    /*---------RsCs data--------*/
    support->logic[current_frame_data]->Rs = videoData[Current_Frame]->layer[0].RsVal;
    support->logic[current_frame_data]->Cs = videoData[Current_Frame]->layer[0].CsVal;
    support->logic[current_frame_data]->SC = (mfxF32)sqrt(( videoData[Current_Frame]->layer[0].RsVal * videoData[Current_Frame]->layer[0].RsVal) + (videoData[Current_Frame]->layer[0].CsVal * videoData[Current_Frame]->layer[0].CsVal));
    if (support->firstFrame) {
        support->logic[current_frame_data]->TSC = 0;
        support->logic[current_frame_data]->AFD = 0;
        support->logic[current_frame_data]->TSCindex = 0;
        support->logic[current_frame_data]->SCindex = 0;
        support->logic[current_frame_data]->Schg = 0;
        support->logic[current_frame_data]->Gchg = 0;
        support->logic[current_frame_data]->picType = 0;
        support->logic[current_frame_data]->lastFrameInShot = 0;
        support->logic[current_frame_data]->pdist = 0;
        support->logic[current_frame_data]->MVdiffVal = 0;
        support->logic[current_frame_data]->RsCsDiff = 0;
        support->logic[current_frame_data]->last_shot_distance = 0;
        support->firstFrame = FALSE;
    }
    else {
        /*--------Motion data-------*/
        MotionAnalysis(support, m_dataIn, videoData[Current_Frame], videoData[Reference_Frame], &support->logic[current_frame_data]->TSC, &support->logic[current_frame_data]->AFD, &support->logic[current_frame_data]->MVdiffVal, (Layers)0);
        support->logic[current_frame_data]->TSCindex = TableLookUp(NumTSC, lmt_tsc2, support->logic[current_frame_data]->TSC);
        support->logic[current_frame_data]->SCindex = TableLookUp(NumSC, lmt_sc2, support->logic[current_frame_data]->SC);
        support->logic[current_frame_data]->pdist = support->PDistanceTable[(support->logic[current_frame_data]->TSCindex * NumSC) + support->logic[current_frame_data]->SCindex];
        /*------Shot Detection------*/
        ShotDetect(videoData[Current_Frame]->layer[0], videoData[Reference_Frame]->layer[0], m_dataIn->layer[0], support->logic[current_frame_data], support->logic[previous_frame_data]);
        support->logic[current_frame_data]->repeatedFrame = FrameRepeatCheck(m_dataIn->interlaceMode != progressive_frame);
    }
    m_dataIn->processed_frames++;
    CorrectionForGoPSize(support, current_frame_data);
}

void SceneChangeDetector::GeneralBufferRotation() {
    VidSample      *videoTransfer;
    videoTransfer = videoData[0];
    videoData[0]  = videoData[1];
    videoData[1]  = videoTransfer;
    TSCstat        *metaTransfer;
    metaTransfer  = support->logic[previous_previous_frame_data];
    support->logic[previous_previous_frame_data] = support->logic[previous_frame_data];
    support->logic[previous_frame_data]          = support->logic[current_frame_data];
    support->logic[current_frame_data]           = metaTransfer;
}

BOOL SceneChangeDetector::ProcessField()
{
    PutFrameInterlaced();
    return(dataReady);
}

mfxU32 SceneChangeDetector::Get_frame_number() {
    if(dataReady)
        return support->logic[current_frame_data]->frameNum;
    else
        return 0;
}

mfxU32 SceneChangeDetector::Get_frame_shot_Decision() {
    if(dataReady)
        return support->logic[current_frame_data]->Schg;
    else
        return 0;
}

mfxU32 SceneChangeDetector::Get_frame_last_in_scene() {
    if(dataReady)
        return support->logic[current_frame_data]->lastFrameInShot;
    else
        return 0;
}

BOOL SceneChangeDetector::Query_is_frame_repeated() {
    if(dataReady)
        return support->logic[current_frame_data]->repeatedFrame;
    else
        return 0;
}

BOOL SceneChangeDetector::Get_Last_frame_Data() {
    if(dataReady)
        GeneralBufferRotation();

    return(dataReady);
}

BOOL SceneChangeDetector::RunFrame(mfxU32 parity){
    videoData[Current_Frame]->frame_number = videoData[Reference_Frame]->frame_number + 1;
    GpuSubSampleASC_Image(_width, _height, _pitch, (Layers)0, parity);
    RsCsCalc(&videoData[Current_Frame]->layer[0], m_dataIn->layer[0]);
    processFrame();
    GeneralBufferRotation();
    return(!support->logic[previous_frame_data]->firstFrame);
}

void SceneChangeDetector::PutFrameProgressive() {
    dataReady = RunFrame(TopField);
}

void SceneChangeDetector::SetInterlaceMode(mfxU32 interlaceMode)
{
    m_dataIn->interlaceMode = interlaceMode;
    m_dataIn->StartingField = TopField;
    if(m_dataIn->interlaceMode != progressive_frame)
    {
        if(m_dataIn->interlaceMode == bottomfieldFirst_frame)
            m_dataIn->StartingField = BottomField;
    }
    m_dataIn->currentField = m_dataIn->StartingField;
}

void SceneChangeDetector::SetParityTFF()
{
    SetInterlaceMode(topfieldfirst_frame);
}

void SceneChangeDetector::SetParityBFF()
{
    SetInterlaceMode(bottomfieldFirst_frame);
}

void SceneChangeDetector::SetProgressiveOp()
{
    SetInterlaceMode(progressive_frame);
}

void SceneChangeDetector::PutFrameInterlaced()
{
    dataReady = RunFrame(m_dataIn->currentField);
    SetNextField();
}

mfxI32 logBase2aligned(mfxI32 number) {
    mfxI32 data = (mfxI32)(ceil((log((double)number) / LN2)));
    return (mfxI32)pow(2,(double)data);
}

void U8AllocandSet(pmfxU8 *ImageLayer, mfxI32 imageSize) {
#if ! defined(MFX_VA_LINUX)
    *ImageLayer = (pmfxU8) _aligned_malloc(imageSize, 16);
#else
    *ImageLayer = (pmfxU8) memalign(16, imageSize);
#endif
    if(*ImageLayer == NULL)
        exit(MEMALLOCERRORU8);
    else
        memset(*ImageLayer,0,imageSize);
}

void PdYuvImage_Alloc(YUV *pImage, mfxI32 dimVideoExtended) {
    mfxI32
        dimAligned;
    dimAligned = logBase2aligned(dimVideoExtended);
    U8AllocandSet(&pImage->data,dimAligned);
}

void PdMVector_Alloc(MVector **MV, mfxI32 mvArraysize){
    mfxI32
        MVspace = sizeof(MVector) * mvArraysize;
    *MV = new MVector[MVspace];//(MVector*) malloc(MVspace);
    if(*MV == NULL)
        exit(MEMALLOCERRORMV);
    else
        memset(*MV,0,MVspace);
}

void PdRsCs_Alloc(pmfxF32 *RCs, mfxI32 mvArraysize) {
    mfxI32
        MVspace = sizeof(mfxF32) * mvArraysize;
    *RCs = (pmfxF32) malloc(MVspace);
    if(*RCs == NULL)
        exit(MEMALLOCERROR);
    else
        memset(*RCs,0,MVspace);
}

void PdSAD_Alloc(pmfxU32 *SAD, mfxI32 mvArraysize) {
    mfxI32
        MVspace = sizeof(mfxI32) * mvArraysize;
    *SAD = (pmfxU32) malloc(MVspace);
    if(*SAD == NULL)
        exit(MEMALLOCERROR);
    else
        memset(*SAD,0,MVspace);
}

void Pdmem_AllocGeneral(imageData *Buffer, ImDetails Rval) {
    /*YUV frame mem allocation*/
    PdYuvImage_Alloc(&(Buffer->Image), Rval.Extended_Width * Rval.Extended_Height);
    Buffer->Image.Y = Buffer->Image.data + Rval.initial_point;

    /*Motion Vector array mem allocation*/
    PdMVector_Alloc(&(Buffer->pInteger),Rval.Blocks_in_Frame);

    /*RsCs array mem allocation*/
    PdRsCs_Alloc(&(Buffer->Cs),Rval.Pixels_in_Y_layer / 16);
    PdRsCs_Alloc(&(Buffer->Rs),Rval.Pixels_in_Y_layer / 16);
    PdRsCs_Alloc(&(Buffer->RsCs),Rval.Pixels_in_Y_layer / 16);

    /*SAD array mem allocation*/
    PdSAD_Alloc(&(Buffer->SAD),Rval.Blocks_in_Frame);
}


void SceneChangeDetector::VidSample_Alloc() {
    for(mfxI32 i = 0; i < 2; i++)
        Pdmem_AllocGeneral(&(videoData[i]->layer[0]),m_dataIn->layer[0]);
#if ! defined(MFX_VA_LINUX)
    videoData[2]->layer[0].Image.data = (mfxU8*)_aligned_malloc(m_dataIn->layer[0].Extended_Width * m_dataIn->layer[0].Extended_Height, 0x1000);
#else
    videoData[2]->layer[0].Image.data = (mfxU8*)memalign(0x1000, m_dataIn->layer[0].Extended_Width * m_dataIn->layer[0].Extended_Height);
#endif
    videoData[2]->layer[0].Image.Y = videoData[2]->layer[0].Image.data;
}

void Pdmem_disposeGeneral(imageData *Buffer) {
    free(Buffer->Cs);
    Buffer->Cs = NULL;
    free(Buffer->Rs);
    Buffer->Rs = NULL;
    free(Buffer->RsCs);
    Buffer->RsCs = NULL;
    //free(Buffer->pInteger);
    delete[] Buffer->pInteger;
    Buffer->pInteger = NULL;
    free(Buffer->SAD);
    Buffer->SAD = NULL;
    Buffer->Image.Y = NULL;
    Buffer->Image.U = NULL;
    Buffer->Image.V = NULL;
#if ! defined(MFX_VA_LINUX)
    _aligned_free(Buffer->Image.data);
#else
    free(Buffer->Image.data);
#endif
    Buffer->Image.data = NULL;

}

void SceneChangeDetector::VidSample_dispose() {
    for(mfxI32 i = 1; i >= 0; i--) {
        if(videoData[i] != NULL) {
            Pdmem_disposeGeneral(videoData[i]->layer);
            delete videoData[i]->layer;
            delete (videoData[i]);
        }
    }
#if ! defined(MFX_VA_LINUX)
    _aligned_free(videoData[2]->layer[0].Image.data);
#else
    free(videoData[2]->layer[0].Image.data);
#endif
}

void SceneChangeDetector::VidRead_dispose() {
    if(support->logic != NULL) {
        for(mfxI32 i = 0; i < TSCSTATBUFFER; i++)
            delete support->logic[i];
        delete[] support->logic;
    }
}

void SceneChangeDetector::alloc() {
    VidSample_Alloc();
}

void SceneChangeDetector::IO_Setup() {
    alloc();
}

void TSCstat_Init(TSCstat **logic) {
    for(int i = 0; i < TSCSTATBUFFER; i++)
    {
        logic[i] = new TSCstat;
        logic[i]->AFD = 0.0;
        logic[i]->Cs = 0.0;
        logic[i]->CsDiff = 0.0;
        logic[i]->diffAFD = 0.0;
        logic[i]->diffMVdiffVal = 0.0;
        logic[i]->diffRsCsDiff = 0.0;
        logic[i]->diffTSC = 0.0;
        logic[i]->frameNum = 0;
        logic[i]->gchDC = 0.0;
        logic[i]->Gchg = 0;
        for(int j = 0; j < 5; j++)
            logic[i]->histogram[j] = 0;
        logic[i]->MVdiffVal = 0.0;
        logic[i]->negBalance = 0.0;
        logic[i]->pdist = 0;
        logic[i]->picType = 0;
        logic[i]->posBalance = 0.0;
        logic[i]->refDCint = 0;
        logic[i]->refDCval = 0.0;
        logic[i]->repeatedFrame = 0;
        logic[i]->Rs = 0.0;
        logic[i]->RsCsDiff = 0.0;
        logic[i]->RsDiff = 0.0;
        logic[i]->SC = 0.0;
        logic[i]->Schg = 0;
        logic[i]->SCindex = 0;
        logic[i]->scVal = 0;
        logic[i]->ssDCint = 0;
        logic[i]->ssDCval = 0;
        logic[i]->TSC = 0.0;
        logic[i]->TSCindex = 0;
        logic[i]->tscVal = 0;
        logic[i]->last_shot_distance = 0;
        logic[i]->lastFrameInShot = 0;
    }
}

void SceneChangeDetector::VidRead_Init() {
    support->average = 0.0;
    support->avgSAD = 0.0;
    support->gopSize = 1;
    support->pendingSch = 0;
    support->lastSCdetectionDistance = 0;
    support->detectedSch = 0;
    support->logic = new TSCstat *[2];
    TSCstat_Init(support->logic);
    support->PDistanceTable = PDISTTbl2;
    support->size = Small_Size;
    support->firstFrame = TRUE;
}

void SceneChangeDetector::nullifier(imageData *Buffer) {
    Buffer->pInteger = NULL;
    Buffer->Cs = NULL;
    Buffer->Rs = NULL;
    Buffer->RsCs = NULL;
    Buffer->SAD = NULL;
    Buffer->CsVal = -1.0;
    Buffer->RsVal = -1.0;
}

void SceneChangeDetector::imageInit(YUV *buffer) {
    buffer->data = NULL;
    buffer->Y = NULL;
    buffer->U = NULL;
    buffer->V = NULL;
}

void SceneChangeDetector::VidSample_Init() {
    for(mfxI32 i = 0; i < 2; i++) {
        nullifier(&videoData[i]->layer[0]);
        imageInit(&videoData[i]->layer[0].Image);
        videoData[i]->frame_number = -1;
        videoData[i]->forward_reference = -1;
        videoData[i]->backward_reference = -1;
    }
}

const mfxF64
    shotcurve1[4] = { -171.86, 3058.3, -16801, 36509 },        //DiffMVdiff vs TSCindex curve
    shotcurve2[4] = { -0.0233, 0.9025, -6.4414, 25.857 },      //AFDdiff vs TSCindex curve
    shotcurve3[4] = { 6.0685, -12.718, 7.4263, 0.1565 };       //RsCsDiff vs SCindex curve

BOOL SceneChangeDetector::SCDetectGPU(mfxF64 diffMVdiffVal, mfxF64 RsCsDiff,   mfxF64 MVDiff,   mfxF64 Rs,       mfxF64 AFD,
                 mfxF64 CsDiff,        mfxF64 diffTSC,    mfxF64 TSC,      mfxF64 gchDC,    mfxF64 diffRsCsdiff,
                 mfxF64 posBalance,    mfxF64 SC,         mfxF64 TSCindex, mfxF64 Scindex,  mfxF64 Cs,
                 mfxF64 diffAFD,       mfxF64 negBalance, mfxF64 ssDCval,  mfxF64 refDCval, mfxF64 RsDiff) {
   TSC;
  if (diffMVdiffVal <= 96.054) {
    if (diffRsCsdiff <= 82.132) {
      if (diffMVdiffVal <= 64.071) {
        if (refDCval <= 17.421) {
          if (diffTSC <= 0.141)
            return 0;
          else {
            if (Scindex <= 1)
              return 0;
            else {
              if (posBalance <= 0.017)
                return 1;
              else
                return 0;
            }
          }
        }
        else
          return 0;
      }
      else {
        if (diffAFD <= 7.636)
          return 0;
        else {
          if (negBalance <= 0.304)
            return 0;
          else {
            if (refDCval <= 80.664) {
              if (diffTSC <= 5.489) {
                if (Rs <= 9.317)
                  return 1;
                else
                  return 0;
              }
              else
                return 1;
            }
            else
              return 0;
          }
        }
      }
    }
    else {
      if (refDCval <= 34.272) {
        if (diffMVdiffVal <= 40.241)
          return 0;
        else
          return 1;
      }
      else {
        if (negBalance <= 0.425) {
          if (diffMVdiffVal <= 66.902) {
            if (MVDiff <= 186.5)
              return 0;
            else {
              if (negBalance <= 0.138)
                return 0;
              else {
                if (TSCindex <= 8)
                  return 0;
                else {
                  if (diffTSC <= 21.251) {
                    if (negBalance <= 0.278)
                      return 0;
                    else {
                      if (SC <= 19.961)
                        return 1;
                      else {
                        if (diffTSC <= 4.68)
                          return 0;
                        else {
                          if (ssDCval <= 74.127)
                            return 0;
                          else
                            return 1;
                        }
                      }
                    }
                  }
                  else
                    return 1;
                }
              }
            }
          }
          else {
            if (negBalance <= 0.176)
              return 0;
            else {
              if (diffTSC <= 5.687) {
                if (refDCval <= 49.887)
                  return 1;
                else
                  return 0;
              }
              else {
                if (RsCsDiff <= 218.953)
                  return 1;
                else {
                  if (CsDiff <= 151.646)
                    return 0;
                  else
                    return 1;
                }
              }
            }
          }
        }
        else{
          if (diffTSC <= 6.095) {
            if (diffMVdiffVal <= 73.036)
              return 0;
            else {
              if (refDCval <= 92.183) {
                if (AFD <= 35.653)
                  return 0;
                else
                  return 1;
              }
              else
                return 0;
            }
          }
          else {
            if (Scindex <= 2)
              return 0;
            else {
              if (Cs <= 25.221) {
                if (diffRsCsdiff <= 107.741)
                  return 0;
                else {
                  if (AFD <= 75.473) {
                    if (diffTSC <= 7.945) {
                      if (Rs <= 19.753)
                        return 1;
                      else
                        return 0;
                    }
                    else
                      return 1;
                  }
                  else
                    return 0;
                }
              }
              else if (Cs > 25.221)
                return 0;
            }
          }
        }
      }
    }
  }
  else {
    if (diffAFD <= 9.426) {
      if (diffAFD <= 7.648) {
        if (diffRsCsdiff <= 73.979)
          return 0;
        else {
          if (Rs <= 13.14)
            return 1;
          else {
            if (CsDiff <= 264.415)
              return 0;
            else
              return 1;
          }
        }
      }
      else {
        if (refDCval <= 48.939) {
          if (gchDC <= 7.758)
            return 1;
          else
            return 0;
        }
        else
          return 0;
      }
    }
    else {
      if (negBalance <= 0.086) {
        if (diffAFD <= 61.744) {
          if (refDCval <= 31.316) {
            if (diffMVdiffVal <= 162.08)
              return 0;
            else
              return 1;
          }
          else {
            if (diffMVdiffVal <= 242.884)
              return 0;
            else {
              if (diffTSC <= 12.015)
                return 0;
              else
                return 1;
            }
          }
        }
        else {
          if (Cs <= 17.495)
            return 1;
          else {
            if (posBalance <= 0.946) {
              if (diffAFD <= 69.333)
                return 0;
              else
                return 1;
            }
            else {
              if (AFD <= 123.453)
                return 0;
              else {
                if (MVDiff <= 390.089)
                  return 1;
                else
                  return 0;
              }
            }
          }
        }
      }
      else {
        if (MVDiff <= 338.661) {
          if (diffRsCsdiff <= 83.809) {
            if (Scindex <= 2) {
              if (posBalance <= 0.771) {
                if (RsDiff <= 47.97)
                  return 1;
                else {
                  if (diffAFD <= 13.356) {
                    if (ssDCval <= 46.02)
                      return 1;
                    else
                      return 0;
                  }
                  else
                    return 1;
                }
              }
              else
                return 0;
            }
            else {
              if (diffTSC <= 9.984)
                return 0;
              else
                return 1;
            }
          }
          else {
            if (diffTSC <= 5.491) {
              if (Scindex <= 3) {
                if (diffMVdiffVal <= 120.482) {
                  if (CsDiff <= 163.098)
                    return 0;
                  else
                    return 1;
                }
                else
                  return 1;
              }
              else
                return 0;
            }
            else {
              if (negBalance <= 0.869)
                return 1;
              else {
                if (diffMVdiffVal <= 158.384)
                  return 0;
                else
                  return 1;
              }
            }
          }
        }
        else {
          if (diffAFD <= 22.956) {
            if (ssDCval <= 63.413) {
              if (diffMVdiffVal <= 284.893)
                return 0;
              else
                return 1;
            }
            else
              return 0;
          }
          else {
            if (diffMVdiffVal <= 273.75) {
              if (diffMVdiffVal <= 204.973) {
                if (Rs <= 25.296) {
                  if (diffRsCsdiff <= 94.765)
                    return 0;
                  else
                    return 1;
                }
                else
                  return 0;
              }
              else
                return 0;
            }
            else {
              if (refDCval <= 193.346) {
                if (Scindex <= 3)
                  return 1;
                else {
                  if (SC <= 25.937)
                    return 0;
                  else
                    return 1;
                }
              }
              else
                return 0;
            }
          }
        }
      }
    }
   }
   return 0;
}

void SceneChangeDetector::SetUltraFastDetection() {
    support->size = Small_Size;
}

mfxStatus SceneChangeDetector::SetWidth(mfxI32 Width) {
    if(Width < SMALL_WIDTH) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    _width = Width;

    return MFX_ERR_NONE;
}

mfxStatus SceneChangeDetector::SetHeight(mfxI32 Height) {
    if(Height < SMALL_HEIGHT) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    _height = Height;
    return MFX_ERR_NONE;
}

mfxStatus SceneChangeDetector::SetPitch(mfxI32 Pitch) {
    if(_width < SMALL_WIDTH) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if(Pitch < _width) {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    _pitch = Pitch;
    return MFX_ERR_NONE;
}

void SceneChangeDetector::SetNextField() {
    if(m_dataIn->interlaceMode != progressive_frame)
        m_dataIn->currentField = !m_dataIn->currentField;
}

mfxStatus SceneChangeDetector::SetDimensions(mfxI32 Width, mfxI32 Height, mfxI32 Pitch) {
    MFX_CHECK_STS(SetWidth(Width));
    MFX_CHECK_STS(SetHeight(Height));
    MFX_CHECK_STS(SetPitch(Pitch));
    return MFX_ERR_NONE;
}

mfxStatus SceneChangeDetector::SubSampleImage(mfxU32 srcWidth, mfxU32 srcHeight, const unsigned char * pData)
{
    m_gpuwidth  = srcWidth;
    m_gpuheight = srcHeight;

    gpustep_w = srcWidth / subWidth;
    gpustep_h = srcHeight / subHeight;

    eMFXHWType  platform = m_pCore->GetHWType();;

    if(platform == MFX_HW_HSW || platform == MFX_HW_HSW_ULT)
        device = std::auto_ptr<mdfut::CmDeviceEx>(new mdfut::CmDeviceEx(asc_genx_hsw, sizeof(asc_genx_hsw), m_pCmDevice, m_mfxDeviceType, m_mfxDeviceHdl));
    else if (platform == MFX_HW_SCL)
        device = std::auto_ptr<mdfut::CmDeviceEx>(new mdfut::CmDeviceEx(asc_genx_skl, sizeof(asc_genx_skl), m_pCmDevice, m_mfxDeviceType, m_mfxDeviceHdl));
    else
        device = std::auto_ptr<mdfut::CmDeviceEx>(new mdfut::CmDeviceEx(asc_genx_bdw, sizeof(asc_genx_bdw), m_pCmDevice, m_mfxDeviceType, m_mfxDeviceHdl));

    queue  = std::auto_ptr<mdfut::CmQueueEx>(new mdfut::CmQueueEx(DeviceEx()));

    // select the fastest kernels for the given input surface size
    if (gpustep_w == 6 && gpustep_h == 7) {
        kernel_p = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_480p)));
        kernel_t = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_480t)));
        kernel_b = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_480b)));
    }
    else if (gpustep_w == 11 && gpustep_h == 11) {
        kernel_p = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_720p)));
        kernel_t = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_720t)));
        kernel_b = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_720b)));
    }
    else if (gpustep_w == 17 && gpustep_h == 16) {
        kernel_p = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_1080p)));
        kernel_t = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_1080t)));
        kernel_b = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_1080b)));
    }
    else if (gpustep_w == 34 && gpustep_h == 33) {
        kernel_p = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_2160p)));
        kernel_t = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_2160t)));
        kernel_b = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_2160b)));
    }
    else if (gpustep_w <= 32) {
        kernel_p = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_var_p)));
        kernel_t = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_var_t)));
        kernel_b = std::auto_ptr<mdfut::CmKernelEx>(new mdfut::CmKernelEx(DeviceEx(), CM_KERNEL_FUNCTION(SubSample_var_b)));
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    surfaceOut = std::auto_ptr<mdfut::CmBufferUPEx>(new mdfut::CmBufferUPEx(DeviceEx(), pData, subWidth * subHeight));

    assert((subWidth % OUT_BLOCK) == 0);
    UINT threadsWidth = subWidth / OUT_BLOCK;
    UINT threadsHeight = subHeight;
    threadSpace = std::auto_ptr<mdfut::CmThreadSpaceEx>(new mdfut::CmThreadSpaceEx(DeviceEx(), threadsWidth, threadsHeight));

    return MFX_ERR_NONE;
}

void SceneChangeDetector::ReadOutputImage()
{
    mfxU8
        *pDst = videoData[Current_Frame]->layer[0].Image.Y,
        *pSrc = videoData[2]->layer[0].Image.Y;
    const mfxU32
        w = m_dataIn->layer[0].Original_Width,
        h = m_dataIn->layer[0].Original_Height,
        pitch = m_dataIn->layer[0].pitch;
    h;
    for( mfxI32 i = 0; i < m_dataIn->layer[0].Original_Height; i++) {
        memcpy_s(pDst, w, pSrc, w);
        pDst += pitch;
        pSrc += w;
    }
}

void SceneChangeDetector::GPUProcess()
{
    if (m_dataIn->interlaceMode) {
        if (m_dataIn->currentField == TopField) {
            QueueEx().Enqueue(*kernel_t, *threadSpace); // interlaced, top field
        } else {
            QueueEx().Enqueue(*kernel_b, *threadSpace); // interlaced, bottom field
        }
    } else {
        QueueEx().Enqueue(*kernel_p, *threadSpace);     // progressive
    }
    QueueEx().WaitForFinished();
}

mfxStatus SceneChangeDetector::Init(VideoCORE   *core, mfxI32 Width, mfxI32 Height, mfxI32 Pitch, mfxU32 interlaceMode, CmDevice  *pCmDevice, mfxHandleType _mfxDeviceType, mfxHDL _mfxDeviceHdl) {

    mfxStatus sts   = MFX_ERR_NONE;
    m_mfxDeviceType = _mfxDeviceType;
    m_mfxDeviceHdl  = _mfxDeviceHdl;
    m_pCmDevice     =  pCmDevice;

    if ( m_bInited )
        return MFX_ERR_NONE;

    m_dataIn    = NULL;
    support   = NULL;
    videoData = NULL;

    m_pCore = core;

    m_dataIn        = new VidData;
    MFX_CHECK_NULL_PTR1(m_dataIn);
    m_dataIn->layer = NULL;
    m_dataIn->layer = new ImDetails;
    MFX_CHECK_NULL_PTR1(m_dataIn->layer);

    Params_Init();

    // [1] GetPlatformType()
    Ipp32u cpuIdInfoRegs[4];
    Ipp64u featuresMask;
    IppStatus ippSts = ippGetCpuFeatures( &featuresMask, cpuIdInfoRegs);

    m_cpuOpt = CPU_NONE;

    if(ippStsNoErr == ippSts && featuresMask & (Ipp64u)(ippCPUID_SSE42) ) // means AVX2 + BMI_I + BMI_II to prevent issues with BMI
    {
        m_cpuOpt = CPU_SSE4;
    }

#ifdef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP_AVX2
    if(ippStsNoErr == ippSts && featuresMask & (Ipp64u)(ippCPUID_AVX2) ) // means AVX2 + BMI_I + BMI_II to prevent issues with BMI
    {
        m_cpuOpt = CPU_AVX2;
    }
#endif

    GPUProc = true; // UseGPU;
    m_dataIn->interlaceMode = ( interlaceMode & MFX_PICSTRUCT_PROGRESSIVE ) ? progressive_frame :
                              ( interlaceMode & MFX_PICSTRUCT_FIELD_TFF )   ? topfieldfirst_frame : bottomfieldFirst_frame;

    support = new VidRead;
    MFX_CHECK_NULL_PTR1(support);
    VidRead_Init();

    videoData = new VidSample *[3];/**[2];*/
    MFX_CHECK_NULL_PTR1(videoData);
    for(mfxI32 i = 0; i < 3; i++)
    {
        videoData[i] = new VidSample;
        MFX_CHECK_NULL_PTR1(videoData[i]);
        videoData[i]->layer = new imageData;
        MFX_CHECK_NULL_PTR1(videoData[i]->layer);
    }
    VidSample_Init();

    Setup_UFast_Environment();

    IO_Setup();

    SetDimensions(Width, Height, Pitch);
    if(GPUProc)
        SubSampleImage((mfxU32) Width, (mfxU32) Height, videoData[2]->layer[0].Image.Y);

    SetUltraFastDetection();

    SetInterlaceMode(m_dataIn->interlaceMode);
    dataReady = FALSE;

    m_bInited = true;
    return sts;
}


mfxStatus SceneChangeDetector::SetGoPSize(mfxU32 GoPSize)
{
    if(GoPSize > Double_HEVC_Gop)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    else if(GoPSize == Forbidden_GoP)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    support->gopSize = GoPSize;
    support->pendingSch = 0;

    return MFX_ERR_NONE;
}

void SceneChangeDetector::ResetGoPSize()
{
    SetGoPSize(Immediate_GoP);
}

mfxStatus SceneChangeDetector::Close()
{
    if ( ! m_bInited )
        return MFX_ERR_NONE;

    if(videoData != NULL)
    {
        VidSample_dispose();
        delete[] videoData;
        videoData = NULL;
    }

    if(support != NULL)
    {
        VidRead_dispose();
        delete support;
        support = NULL;
    }

    if(m_dataIn != NULL)
    {
        delete m_dataIn->layer;
        delete m_dataIn;
        m_dataIn = NULL;
    }

    CmDevice* cmdevice  = device->GetDevice();
    MFX_CHECK_NULL_PTR1(cmdevice);

    std::map<void *, CmSurface2D *>::iterator itSrc;

    for (itSrc = m_tableCmRelations.begin() ; itSrc != m_tableCmRelations.end(); itSrc++)
    {
        CmSurface2D *temp = itSrc->second;
        cmdevice->DestroySurface(temp);
    }
    m_tableCmRelations.clear();


    m_bInited = false;

    return MFX_ERR_NONE;
}

void SceneChangeDetector::GpuSubSampleASC_Image(mfxI32 srcWidth, mfxI32 srcHeight, mfxI32 inputPitch, Layers dstIdx, mfxU32 parity)
{
    parity;  inputPitch; srcHeight; srcWidth;
    void*
        pDst = videoData[Current_Frame]->layer[dstIdx].Image.Y; pDst;
    mfxU32
        pitch = m_dataIn->layer[dstIdx].pitch; pitch;
    GPUProcess();
    ReadOutputImage();
}

#endif //defined (MFX_ENABLE_VPP) && defined(MFX_VA_LINUX)
