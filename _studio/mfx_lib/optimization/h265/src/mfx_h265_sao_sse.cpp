/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

#include <immintrin.h>

namespace MFX_HEVC_PP
{
    enum SAOType
    {
        SAO_EO_0 = 0,
        SAO_EO_1,
        SAO_EO_2,
        SAO_EO_3,
        SAO_BO,
        MAX_NUM_SAO_TYPE
    };

    enum SGUBorderID
    {
        SGU_L = 0,
        SGU_R,
        SGU_T,
        SGU_B,
        SGU_TL,
        SGU_TR,
        SGU_BL,
        SGU_BR,
        NUM_SGU_BORDER
    };

    /** get the sign of input variable
    * \param   x
    */
    inline Ipp32s getSign(Ipp32s x)
    {
        return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
    }

    // Stores 0..15 bytes of the xmm register to memory
    static inline void XmmPartialStore(Ipp8u *ptr, __m128i xmm, int num)
    {
        if (num >= 8)
        {
            _mm_storel_epi64((__m128i *)ptr, xmm);
            xmm = _mm_srli_si128(xmm, 8);
            ptr += 8;
            num -= 8;
        }
        if (num >= 4)
        {
            *(Ipp32u *)ptr = _mm_cvtsi128_si32(xmm);
            xmm = _mm_srli_si128(xmm, 4);
            ptr += 4;
            num -= 4;
        }
        if (num >= 2)
        {
            *(Ipp16u *)ptr = (Ipp16u)_mm_extract_epi16(xmm, 0);
            xmm = _mm_srli_si128(xmm, 2);
            ptr += 2;
            num -= 2;
        }
        if (num >= 1)
        {
            *(Ipp8u *)ptr = (Ipp8u)_mm_extract_epi8(xmm, 0);
        }
    }

#ifndef MFX_TARGET_OPTIMIZATION_AUTO
    void h265_ProcessSaoCuOrg_Luma_8u(
#else
    void h265_ProcessSaoCuOrg_Luma_8u_sse(
#endif
        Ipp8u* pRec,
        Ipp32s stride,
        Ipp32s saoType, 
        Ipp8u* tmpL,
        Ipp8u* tmpU,
        Ipp32u maxCUWidth,
        Ipp32u maxCUHeight,
        Ipp32s picWidth,
        Ipp32s picHeight,
        Ipp32s* pOffsetEo,
        Ipp8u* pOffsetBo,
        Ipp8u* pClipTable,
        Ipp32u CUPelX,
        Ipp32u CUPelY)
    {
        pClipTable;

        Ipp8u tmpUpBuff1[66];   // startX + 64 + 1
        Ipp8u tmpUpBuff2[66];
        Ipp32s LCUWidth  = maxCUWidth;
        Ipp32s LCUHeight = maxCUHeight;
        Ipp32s LPelX     = CUPelX;
        Ipp32s TPelY     = CUPelY;
        Ipp32s RPelX;
        Ipp32s BPelY;
        Ipp32s picWidthTmp;
        Ipp32s picHeightTmp;
        Ipp32s startX;
        Ipp32s startY;
        Ipp32s endX;
        Ipp32s endY;
        Ipp32s x, y;

        __m128i offset0 = _mm_set1_epi8((Ipp8s)pOffsetEo[0]);
        __m128i offset1 = _mm_set1_epi8((Ipp8s)pOffsetEo[1]);
        __m128i offset2 = _mm_set1_epi8((Ipp8s)pOffsetEo[2]);
        __m128i offset3 = _mm_set1_epi8((Ipp8s)pOffsetEo[3]);
        __m128i offset4 = _mm_set1_epi8((Ipp8s)pOffsetEo[4]);
        __m128i negOne = _mm_cmpeq_epi8(offset0, offset0);
        __m128i recn, rec0, rec1, rmin;
        __m128i sign0, sign1, type, offs;

        picWidthTmp  = picWidth;
        picHeightTmp = picHeight;
        LCUWidth     = LCUWidth;
        LCUHeight    = LCUHeight;
        LPelX        = LPelX;
        TPelY        = TPelY;
        RPelX        = LPelX + LCUWidth;
        BPelY        = TPelY + LCUHeight;
        RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
        BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
        LCUWidth     = RPelX - LPelX;
        LCUHeight    = BPelY - TPelY;

        switch (saoType)
        {
        case SAO_EO_0: // dir: -
            {
                startX = (LPelX == 0) ? 1 : 0;
                endX   = (RPelX == picWidthTmp) ? LCUWidth-1 : LCUWidth;

                for (y = 0; y < LCUHeight; y++)
                {
                    // reused from previous iteration
                    rec0 = _mm_insert_epi8(_mm_setzero_si128(), tmpL[y], 15);

                    // store 16 pixels per iteration
                    for (x = startX; x < endX - startX - 15; x += 16)
                    {
                        // compute signLeft
                        recn = rec0;
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        recn = _mm_alignr_epi8(rec0, recn, 15);

                        rmin = _mm_min_epu8(rec0, recn);
                        sign0 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, recn));

                        // compute signRight
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+1]));

                        rmin = _mm_min_epu8(rec0, rec1);
                        sign1 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, rec1));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi8(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi8(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_sub_epi8(rec0, _mm_set1_epi8(-128)); // convert to signed
                        recn = _mm_adds_epi8(recn, offs);               // signed saturate
                        recn = _mm_add_epi8(recn, _mm_set1_epi8(-128)); // convert to unsigned

                        _mm_storeu_si128((__m128i *)(&pRec[x]), recn);
                    }

                    // store remaining 1..15 pixels
                    if (x < endX)
                    {
                        // compute signLeft
                        recn = rec0;
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        recn = _mm_alignr_epi8(rec0, recn, 15);

                        rmin = _mm_min_epu8(rec0, recn);
                        sign0 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, recn));

                        // compute signRight
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+1]));

                        rmin = _mm_min_epu8(rec0, rec1);
                        sign1 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, rec1));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi8(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi8(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_sub_epi8(rec0, _mm_set1_epi8(-128)); // convert to signed
                        recn = _mm_adds_epi8(recn, offs);               // signed saturate
                        recn = _mm_add_epi8(recn, _mm_set1_epi8(-128)); // convert to unsigned

                        XmmPartialStore(&pRec[x], recn, endX - x);
                    }

                    pRec += stride;
                }
                break;
            }
        case SAO_EO_1: // dir: |
            {
                startY = (TPelY == 0) ? 1 : 0;
                endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

                if (TPelY == 0)
                {
                    pRec += stride;
                }

                for (x = 0; x < LCUWidth; x++)
                {
                    tmpUpBuff1[x] = tmpU[x];
                }

                for (y = startY; y < endY; y++)
                {
                    // store 16 pixels per iteration
                    for (x = 0; x < LCUWidth - 15; x += 16)
                    {
                        // compute signUp
                        recn = _mm_loadu_si128((__m128i *)(&tmpUpBuff1[x]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&tmpUpBuff1[x]), rec0);

                        rmin = _mm_min_epu8(rec0, recn);
                        sign0 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, recn));

                        // compute signDown
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride]));

                        rmin = _mm_min_epu8(rec0, rec1);
                        sign1 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, rec1));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi8(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi8(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_sub_epi8(rec0, _mm_set1_epi8(-128)); // convert to signed
                        recn = _mm_adds_epi8(recn, offs);               // signed saturate
                        recn = _mm_add_epi8(recn, _mm_set1_epi8(-128)); // convert to unsigned

                        _mm_storeu_si128((__m128i *)(&pRec[x]), recn);
                    }

                    // store remaining 1..15 pixels
                    if (x < LCUWidth)
                    {
                        // compute signUp
                        recn = _mm_loadu_si128((__m128i *)(&tmpUpBuff1[x]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&tmpUpBuff1[x]), rec0);

                        rmin = _mm_min_epu8(rec0, recn);
                        sign0 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, recn));

                        // compute signDown
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride]));

                        rmin = _mm_min_epu8(rec0, rec1);
                        sign1 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, rec1));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi8(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi8(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_sub_epi8(rec0, _mm_set1_epi8(-128)); // convert to signed
                        recn = _mm_adds_epi8(recn, offs);               // signed saturate
                        recn = _mm_add_epi8(recn, _mm_set1_epi8(-128)); // convert to unsigned

                        XmmPartialStore(&pRec[x], recn, LCUWidth - x);
                    }

                    pRec += stride;
                }
                break;
            }
        case SAO_EO_2: // dir: 135
            {
                Ipp8u *pUpBuff = tmpUpBuff1;
                Ipp8u *pUpBufft = tmpUpBuff2;
                Ipp8u *swapPtr;

                startX = (LPelX == 0)           ? 1 : 0;
                endX   = (RPelX == picWidthTmp) ? LCUWidth - 1 : LCUWidth;

                startY = (TPelY == 0) ?            1 : 0;
                endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

                if (TPelY == 0)
                {
                    pRec += stride;
                }

                for (x = startX; x < endX; x++)
                {
                    pUpBuff[x] = tmpU[x-1];
                }

                for (y = startY; y < endY; y++)
                {
                    pUpBufft[startX] = tmpL[y];  // prime the first left value

                    // store 16 pixels per iteration
                    for (x = startX; x < endX - startX - 15; x += 16)
                    {
                        // compute signUp
                        recn = _mm_loadu_si128((__m128i *)(&pUpBuff[x]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&pUpBufft[x+1]), rec0);

                        rmin = _mm_min_epu8(rec0, recn);
                        sign0 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, recn));

                        // compute signDown
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride+1]));

                        rmin = _mm_min_epu8(rec0, rec1);
                        sign1 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, rec1));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi8(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi8(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_sub_epi8(rec0, _mm_set1_epi8(-128)); // convert to signed
                        recn = _mm_adds_epi8(recn, offs);               // signed saturate
                        recn = _mm_add_epi8(recn, _mm_set1_epi8(-128)); // convert to unsigned

                        _mm_storeu_si128((__m128i *)(&pRec[x]), recn);
                    }

                    // store remaining 1..15 pixels
                    if (x < endX)
                    {
                        // compute signUp
                        recn = _mm_loadu_si128((__m128i *)(&pUpBuff[x]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&pUpBufft[x+1]), rec0);

                        rmin = _mm_min_epu8(rec0, recn);
                        sign0 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, recn));

                        // compute signDown
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride+1]));

                        rmin = _mm_min_epu8(rec0, rec1);
                        sign1 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, rec1));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi8(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi8(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_sub_epi8(rec0, _mm_set1_epi8(-128)); // convert to signed
                        recn = _mm_adds_epi8(recn, offs);               // signed saturate
                        recn = _mm_add_epi8(recn, _mm_set1_epi8(-128)); // convert to unsigned

                        XmmPartialStore(&pRec[x], recn, endX - x);
                    }

                    swapPtr  = pUpBuff;
                    pUpBuff  = pUpBufft;
                    pUpBufft = swapPtr;

                    pRec += stride;
                }
                break;
            }
        case SAO_EO_3: // dir: 45
            {
                startX = (LPelX == 0) ? 1 : 0;
                endX   = (RPelX == picWidthTmp) ? LCUWidth - 1 : LCUWidth;

                startY = (TPelY == 0) ? 1 : 0;
                endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

                if (startY == 1)
                {
                    pRec += stride;
                }

                for (x = startX; x <= endX; x++)
                {
                    tmpUpBuff1[x] = tmpU[x];
                }

                for (y = startY; y < endY; y++)
                {
                    // store 16 pixels per iteration
                    for (x = startX; x < endX - startX - 15; x += 16)
                    {
                        // compute signUp
                        recn = _mm_loadu_si128((__m128i *)(&tmpUpBuff1[x+1]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&tmpUpBuff1[x]), rec0);

                        rmin = _mm_min_epu8(rec0, recn);
                        sign0 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, recn));

                        // compute signDown
                        if (x == startX) {
                            rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride]));
                            rec1 = _mm_slli_si128(rec1, 1);
                            rec1 = _mm_insert_epi8(rec1, tmpL[y+1], 0);
                        } else
                            rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride-1]));

                        rmin = _mm_min_epu8(rec0, rec1);
                        sign1 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, rec1));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi8(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi8(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_sub_epi8(rec0, _mm_set1_epi8(-128)); // convert to signed
                        recn = _mm_adds_epi8(recn, offs);               // signed saturate
                        recn = _mm_add_epi8(recn, _mm_set1_epi8(-128)); // convert to unsigned

                        _mm_storeu_si128((__m128i *)(&pRec[x]), recn);
                    }

                    // store remaining 1..15 pixels
                    if (x < endX)
                    {
                        // compute signUp
                        recn = _mm_loadu_si128((__m128i *)(&tmpUpBuff1[x+1]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&tmpUpBuff1[x]), rec0);

                        rmin = _mm_min_epu8(rec0, recn);
                        sign0 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, recn));

                        // compute signDown
                        if (x == startX) {
                            rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride]));
                            rec1 = _mm_slli_si128(rec1, 1);
                            rec1 = _mm_insert_epi8(rec1, tmpL[y+1], 0);
                        } else
                            rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride-1]));

                        rmin = _mm_min_epu8(rec0, rec1);
                        sign1 = _mm_sub_epi8(_mm_cmpeq_epi8(rmin, rec0), _mm_cmpeq_epi8(rmin, rec1));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi8(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi8(type, negOne));
                        type = _mm_add_epi8(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi8(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_sub_epi8(rec0, _mm_set1_epi8(-128)); // convert to signed
                        recn = _mm_adds_epi8(recn, offs);               // signed saturate
                        recn = _mm_add_epi8(recn, _mm_set1_epi8(-128)); // convert to unsigned

                        XmmPartialStore(&pRec[x], recn, endX - x);
                    }

                    tmpUpBuff1[endX] = pRec[endX];

                    pRec += stride;
                }
                break;
            }
        case SAO_BO:
            {
                for (y = 0; y < LCUHeight; y++)
                {
                    for (x = 0; x < LCUWidth; x++)
                    {
                        pRec[x] = pOffsetBo[pRec[x]];
                    }
                    pRec += stride;
                }
                break;
            }
        default: break;
        }

    } //  void h265_processSaoCuOrg_Luma_8u(...)

    //aya: FIXME!!! must be replaced by intrinsic version 
#ifndef MFX_TARGET_OPTIMIZATION_AUTO
    void h265_ProcessSaoCu_Luma_8u(
#else
    void h265_ProcessSaoCu_Luma_8u_sse(
#endif
        Ipp8u* pRec,
        Ipp32s stride,
        Ipp32s saoType, 
        Ipp8u* tmpL,
        Ipp8u* tmpU,
        Ipp32u maxCUWidth,
        Ipp32u maxCUHeight,
        Ipp32s picWidth,
        Ipp32s picHeight,
        Ipp32s* pOffsetEo,
        Ipp8u* pOffsetBo,
        Ipp8u* pClipTable,
        Ipp32u CUPelX,
        Ipp32u CUPelY,
        bool* pbBorderAvail)
    {
        Ipp32s tmpUpBuff1[65];
        Ipp32s tmpUpBuff2[65];
        Ipp32s LCUWidth  = maxCUWidth;
        Ipp32s LCUHeight = maxCUHeight;
        Ipp32s LPelX     = CUPelX;
        Ipp32s TPelY     = CUPelY;
        Ipp32s RPelX;
        Ipp32s BPelY;
        Ipp32s signLeft;
        Ipp32s signRight;
        Ipp32s signDown;
        Ipp32s signDown1;
        Ipp32u edgeType;
        Ipp32s picWidthTmp;
        Ipp32s picHeightTmp;
        Ipp32s startX;
        Ipp32s startY;
        Ipp32s endX;
        Ipp32s endY;
        Ipp32s x, y;
        Ipp8u* startPtr;
        Ipp32s startStride;

        picWidthTmp  = picWidth;
        picHeightTmp = picHeight;
        LCUWidth     = LCUWidth;
        LCUHeight    = LCUHeight;
        LPelX        = LPelX;
        TPelY        = TPelY;
        RPelX        = LPelX + LCUWidth;
        BPelY        = TPelY + LCUHeight;
        RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
        BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
        LCUWidth     = RPelX - LPelX;
        LCUHeight    = BPelY - TPelY;

        switch (saoType)
        {
        case SAO_EO_0: // dir: -
            {
                if (pbBorderAvail[SGU_L])
                {
                    startX = 0;
                    startPtr = tmpL;
                    startStride = 1;
                }
                else
                {
                    startX = 1;
                    startPtr = pRec;
                    startStride = stride;
                }

                endX   = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth - 1);

                for (y = 0; y < LCUHeight; y++)
                {
                    signLeft = getSign(pRec[startX] - startPtr[y*startStride]);

                    for (x = startX; x < endX; x++)
                    {
                        signRight =  getSign(pRec[x] - pRec[x+1]);
                        edgeType  =  signRight + signLeft + 2;
                        signLeft  = -signRight;

                        pRec[x]   = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                    pRec  += stride;
                }
                break;
            }
        case SAO_EO_1: // dir: |
            {
                if (pbBorderAvail[SGU_T])
                {
                    startY = 0;
                    startPtr = tmpU;
                }
                else
                {
                    startY = 1;
                    startPtr = pRec;
                    pRec += stride;
                }

                endY = (pbBorderAvail[SGU_B]) ? LCUHeight : LCUHeight - 1;

                for (x = 0; x < LCUWidth; x++)
                {
                    tmpUpBuff1[x] = getSign(pRec[x] - startPtr[x]);
                }

                for (y = startY; y < endY; y++)
                {
                    for (x = 0; x < LCUWidth; x++)
                    {
                        signDown      = getSign(pRec[x] - pRec[x+stride]);
                        edgeType      = signDown + tmpUpBuff1[x] + 2;
                        tmpUpBuff1[x] = -signDown;

                        pRec[x]      = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                    pRec  += stride;
                }
                break;
            }
        case SAO_EO_2: // dir: 135
            {
                Ipp32s *pUpBuff = tmpUpBuff1;
                Ipp32s *pUpBufft = tmpUpBuff2;
                Ipp32s *swapPtr;

                if (pbBorderAvail[SGU_L])
                {
                    startX = 0;
                    startPtr = tmpL;
                    startStride = 1;
                }
                else
                {
                    startX = 1;
                    startPtr = pRec;
                    startStride = stride;
                }

                endX = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth-1);

                //prepare 2nd line upper sign
                pUpBuff[startX] = getSign(pRec[startX+stride] - startPtr[0]);
                for (x = startX; x < endX; x++)
                {
                    pUpBuff[x+1] = getSign(pRec[x+stride+1] - pRec[x]);
                }

                //1st line
                if (pbBorderAvail[SGU_TL])
                {
                    edgeType = getSign(pRec[0] - tmpU[-1]) - pUpBuff[1] + 2;
                    pRec[0]  = pClipTable[pRec[0] + pOffsetEo[edgeType]];
                }

                if (pbBorderAvail[SGU_T])
                {
                    for (x = 1; x < endX; x++)
                    {
                        edgeType = getSign(pRec[x] - tmpU[x-1]) - pUpBuff[x+1] + 2;
                        pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                }

                pRec += stride;

                //middle lines
                for (y = 1; y < LCUHeight - 1; y++)
                {
                    pUpBufft[startX] = getSign(pRec[stride+startX] - startPtr[y*startStride]);

                    for (x = startX; x < endX; x++)
                    {
                        signDown1 = getSign(pRec[x] - pRec[x+stride+1]);
                        edgeType  =  signDown1 + pUpBuff[x] + 2;
                        pRec[x]   = pClipTable[pRec[x] + pOffsetEo[edgeType]];

                        pUpBufft[x+1] = -signDown1;
                    }

                    swapPtr  = pUpBuff;
                    pUpBuff  = pUpBufft;
                    pUpBufft = swapPtr;

                    pRec += stride;
                }

                //last line
                if (pbBorderAvail[SGU_B])
                {
                    for (x = startX; x < LCUWidth - 1; x++)
                    {
                        edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                        pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                }

                if (pbBorderAvail[SGU_BR])
                {
                    x = LCUWidth - 1;
                    edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                    pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                }
                break;
            }
        case SAO_EO_3: // dir: 45
            {
                if (pbBorderAvail[SGU_L])
                {
                    startX = 0;
                    startPtr = tmpL;
                    startStride = 1;
                }
                else
                {
                    startX = 1;
                    startPtr = pRec;
                    startStride = stride;
                }

                endX   = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth -1);

                //prepare 2nd line upper sign
                tmpUpBuff1[startX] = getSign(startPtr[startStride] - pRec[startX]);
                for (x = startX; x < endX; x++)
                {
                    tmpUpBuff1[x+1] = getSign(pRec[x+stride] - pRec[x+1]);
                }

                //first line
                if (pbBorderAvail[SGU_T])
                {
                    for (x = startX; x < LCUWidth - 1; x++)
                    {
                        edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                        pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                }

                if (pbBorderAvail[SGU_TR])
                {
                    x= LCUWidth - 1;
                    edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                    pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                }

                pRec += stride;

                //middle lines
                for (y = 1; y < LCUHeight - 1; y++)
                {
                    signDown1 = getSign(pRec[startX] - startPtr[(y+1)*startStride]) ;

                    for (x = startX; x < endX; x++)
                    {
                        edgeType       = signDown1 + tmpUpBuff1[x+1] + 2;
                        pRec[x]        = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                        tmpUpBuff1[x] = -signDown1;
                        signDown1      = getSign(pRec[x+1] - pRec[x+stride]) ;
                    }

                    tmpUpBuff1[endX] = -signDown1;

                    pRec  += stride;
                }

                //last line
                if (pbBorderAvail[SGU_BL])
                {
                    edgeType = getSign(pRec[0] - pRec[stride-1]) + tmpUpBuff1[1] + 2;
                    pRec[0] = pClipTable[pRec[0] + pOffsetEo[edgeType]];

                }

                if (pbBorderAvail[SGU_B])
                {
                    for (x = 1; x < endX; x++)
                    {
                        edgeType = getSign(pRec[x] - pRec[x+stride-1]) + tmpUpBuff1[x+1] + 2;
                        pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                }
                break;
            }
        case SAO_BO:
            {
                for (y = 0; y < LCUHeight; y++)
                {
                    for (x = 0; x < LCUWidth; x++)
                    {
                        pRec[x] = pOffsetBo[pRec[x]];
                    }

                    pRec += stride;
                }
                break;
            }
        default: break;
        }

    } // void h265_ProcessSaoCu_Luma_8u(...)

}; // namespace MFX_HEVC_PP

#endif // #if defined(MFX_TARGET_OPTIMIZATION_PX) 
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
