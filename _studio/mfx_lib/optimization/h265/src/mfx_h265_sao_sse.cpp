//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_ATOM) && defined(MFX_TARGET_OPTIMIZATION_ATOM) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4) || \
    defined(MFX_MAKENAME_SSSE3) && defined(MFX_TARGET_OPTIMIZATION_SSSE3) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_AVX2)

#include <immintrin.h>
#ifdef MFX_EMULATE_SSSE3
#include "mfx_ssse3_emulation.h"
#endif

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
        if (num & 8)
        {
            _mm_storel_epi64((__m128i *)ptr, xmm);
            xmm = _mm_srli_si128(xmm, 8);
            ptr += 8;
        }
        if (num & 4)
        {
            *(Ipp32u *)ptr = _mm_cvtsi128_si32(xmm);
            xmm = _mm_srli_si128(xmm, 4);
            ptr += 4;
        }
        if (num & 2)
        {
            *(Ipp16u *)ptr = (Ipp16u)_mm_extract_epi16(xmm, 0);
            xmm = _mm_srli_si128(xmm, 2);
            ptr += 2;
        }
        if (num & 1)
        {
            *(Ipp8u *)ptr = (Ipp8u)_mm_extract_epi8(xmm, 0);
        }
    }

    void MAKE_NAME(h265_ProcessSaoCuOrg_Luma_8u)(SAOCU_ORG_PARAMETERS_LIST)
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


    void MAKE_NAME(h265_ProcessSaoCuOrg_Luma_16u)(SAOCU_ORG_PARAMETERS_LIST_U16)
    {
        Ipp16s tmpUpBuff1[66];
        Ipp16s tmpUpBuff2[66];
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

        __m128i offset0 = _mm_set1_epi16((Ipp16s)pOffsetEo[0]);
        __m128i offset1 = _mm_set1_epi16((Ipp16s)pOffsetEo[1]);
        __m128i offset2 = _mm_set1_epi16((Ipp16s)pOffsetEo[2]);
        __m128i offset3 = _mm_set1_epi16((Ipp16s)pOffsetEo[3]);
        __m128i offset4 = _mm_set1_epi16((Ipp16s)pOffsetEo[4]);
        __m128i negOne = _mm_cmpeq_epi16(offset0, offset0);
        __m128i recn, rec0, rec1;
        __m128i sign0, sign1, type, offs;
        __m128i xmm_min, xmm_max;

        /* bitDepth not passed in explicitly so determine from clip table */
        if (pClipTable[(1 << 8) - 1] == pClipTable[(1 << 8)]) {
            /* 8 bits - clip to [0, 2^8) */
            xmm_min = _mm_setzero_si128();
            xmm_max = _mm_set1_epi16((1 << 8) - 1);
        } else if (pClipTable[(1 << 9) - 1] == pClipTable[(1 << 9)]) {
            /* 9 bits - clip to [0, 2^9) */
            xmm_min = _mm_setzero_si128();
            xmm_max = _mm_set1_epi16((1 << 9) - 1);
        } else if (pClipTable[(1 << 10) - 1] == pClipTable[(1 << 10)]) {
            /* 10 bits - clip to [0, 2^10) */
            xmm_min = _mm_setzero_si128();
            xmm_max = _mm_set1_epi16((1 << 10) - 1);
        } else if (pClipTable[(1 << 11) - 1] == pClipTable[(1 << 11)]) {
            /* 11 bits - clip to [0, 2^11) */
            xmm_min = _mm_setzero_si128();
            xmm_max = _mm_set1_epi16((1 << 11) - 1);
        } else if (pClipTable[(1 << 12) - 1] == pClipTable[(1 << 12)]) {
            /* 12 bits - clip to [0, 2^12) */
            xmm_min = _mm_setzero_si128();
            xmm_max = _mm_set1_epi16((1 << 12) - 1);
        }

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
                    rec0 = _mm_insert_epi16(_mm_setzero_si128(), tmpL[y], 7);

                    // store 8 pixels per iteration
                    for (x = startX; x < endX - startX - 7; x += 8)
                    {
                        recn = rec0;
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x+0]));
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+1]));
                        recn = _mm_alignr_epi8(rec0, recn, 14);

                        // compute signLeft, signRight
                        sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
                        sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi16(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi16(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_add_epi16(rec0, offs);

                        // clip to [0, 2^bitDepth) */
                        recn = _mm_max_epi16(recn, xmm_min);    
                        recn = _mm_min_epi16(recn, xmm_max);

                        _mm_storeu_si128((__m128i *)(&pRec[x]), recn);                    
                    }

                    // store remaining 1..7 pixels
                    if (x < endX)
                    {
                        recn = rec0;
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x+0]));
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+1]));
                        recn = _mm_alignr_epi8(rec0, recn, 14);

                        // compute signLeft, signRight
                        sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
                        sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi16(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi16(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_add_epi16(rec0, offs);

                        // clip to [0, 2^bitDepth) */
                        recn = _mm_max_epi16(recn, xmm_min);    
                        recn = _mm_min_epi16(recn, xmm_max);

                        XmmPartialStore((Ipp8u *)(&pRec[x]), recn, 2*(endX - x));
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
                    // store 8 pixels per iteration
                    for (x = 0; x < LCUWidth - 7; x += 8)
                    {
                        // compute signUp, signDown
                        recn = _mm_loadu_si128((__m128i *)(&tmpUpBuff1[x]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride]));
                        _mm_storeu_si128((__m128i *)(&tmpUpBuff1[x]), rec0);

                        sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
                        sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi16(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi16(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_add_epi16(rec0, offs);

                        // clip to [0, 2^bitDepth) */
                        recn = _mm_max_epi16(recn, xmm_min);    
                        recn = _mm_min_epi16(recn, xmm_max);

                        _mm_storeu_si128((__m128i *)(&pRec[x]), recn);                    
                    }

                    // store remaining 1..7 pixels
                    if (x < LCUWidth)
                    {
                        // compute signUp, signDown
                        recn = _mm_loadu_si128((__m128i *)(&tmpUpBuff1[x]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride]));
                        _mm_storeu_si128((__m128i *)(&tmpUpBuff1[x]), rec0);

                        sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
                        sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi16(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi16(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_add_epi16(rec0, offs);

                        // clip to [0, 2^bitDepth) */
                        recn = _mm_max_epi16(recn, xmm_min);    
                        recn = _mm_min_epi16(recn, xmm_max);

                        XmmPartialStore((Ipp8u *)(&pRec[x]), recn, 2*(LCUWidth - x));
                    }

                    pRec += stride;
                }
                break;
            }

        case SAO_EO_2: // dir: 135
            {
                Ipp16s *pUpBuff = tmpUpBuff1;
                Ipp16s *pUpBufft = tmpUpBuff2;
                Ipp16s *swapPtr;

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

                    // store 8 pixels per iteration
                    for (x = startX; x < endX - startX - 7; x += 8)
                    {
                        // compute signUp, signDown
                        recn = _mm_loadu_si128((__m128i *)(&pUpBuff[x]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&pUpBufft[x+1]), rec0);
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride+1]));

                        sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
                        sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi16(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi16(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_add_epi16(rec0, offs);

                        // clip to [0, 2^bitDepth) */
                        recn = _mm_max_epi16(recn, xmm_min);    
                        recn = _mm_min_epi16(recn, xmm_max);

                        _mm_storeu_si128((__m128i *)(&pRec[x]), recn);                    
                    }

                    // store remaining 1..7 pixels
                    if (x < endX)
                    {
                        // compute signUp, signDown
                        recn = _mm_loadu_si128((__m128i *)(&pUpBuff[x]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&pUpBufft[x+1]), rec0);
                        rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride+1]));

                        sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
                        sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi16(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi16(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_add_epi16(rec0, offs);

                        // clip to [0, 2^bitDepth) */
                        recn = _mm_max_epi16(recn, xmm_min);    
                        recn = _mm_min_epi16(recn, xmm_max);

                        XmmPartialStore((Ipp8u *)(&pRec[x]), recn, 2*(endX - x));
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
                    // store 8 pixels per iteration
                    for (x = startX; x < endX - startX - 7; x += 8)
                    {
                        // compute signUp, signDown
                        recn = _mm_loadu_si128((__m128i *)(&tmpUpBuff1[x+1]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&tmpUpBuff1[x]), rec0);
                        if (x == startX) {
                            rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride]));
                            rec1 = _mm_slli_si128(rec1, 2);
                            rec1 = _mm_insert_epi16(rec1, tmpL[y+1], 0);
                        } else
                            rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride-1]));

                        sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
                        sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi16(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi16(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_add_epi16(rec0, offs);

                        // clip to [0, 2^bitDepth) */
                        recn = _mm_max_epi16(recn, xmm_min);    
                        recn = _mm_min_epi16(recn, xmm_max);

                        _mm_storeu_si128((__m128i *)(&pRec[x]), recn);                    
                    }

                    // store remaining 1..15 pixels
                    if (x < endX)
                    {
                        // compute signUp, signDown
                        recn = _mm_loadu_si128((__m128i *)(&tmpUpBuff1[x+1]));
                        rec0 = _mm_loadu_si128((__m128i *)(&pRec[x]));
                        _mm_storeu_si128((__m128i *)(&tmpUpBuff1[x]), rec0);
                        if (x == startX) {
                            rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride]));
                            rec1 = _mm_slli_si128(rec1, 2);
                            rec1 = _mm_insert_epi16(rec1, tmpL[y+1], 0);
                        } else
                            rec1 = _mm_loadu_si128((__m128i *)(&pRec[x+stride-1]));

                        sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
                        sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));

                        // gather offsets
                        offs = offset0;
                        type = _mm_add_epi16(sign0, sign1);  // type = [-2,+2]
                        offs = _mm_blendv_epi8(offs, offset1, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
                        offs = _mm_blendv_epi8(offs, offset2, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
                        offs = _mm_blendv_epi8(offs, offset3, _mm_cmpeq_epi16(type, negOne));
                        type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
                        offs = _mm_blendv_epi8(offs, offset4, _mm_cmpeq_epi16(type, negOne));

                        // add offset, saturate, and store
                        recn = _mm_add_epi16(rec0, offs);

                        // clip to [0, 2^bitDepth) */
                        recn = _mm_max_epi16(recn, xmm_min);    
                        recn = _mm_min_epi16(recn, xmm_max);

                        XmmPartialStore((Ipp8u *)(&pRec[x]), recn, 2*(endX - x));
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

    }

    void MAKE_NAME(h265_ProcessSaoCu_Luma_8u)(SAOCU_PARAMETERS_LIST)
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
                if (pbBorderAvail.m_left)
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

                endX   = (pbBorderAvail.m_right) ? LCUWidth : (LCUWidth - 1);

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
                if (pbBorderAvail.m_top)
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

                endY = (pbBorderAvail.m_bottom) ? LCUHeight : LCUHeight - 1;

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

                if (pbBorderAvail.m_left)
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

                endX = (pbBorderAvail.m_right) ? LCUWidth : (LCUWidth-1);

                //prepare 2nd line upper sign
                pUpBuff[startX] = getSign(pRec[startX+stride] - startPtr[0]);
                for (x = startX; x < endX; x++)
                {
                    pUpBuff[x+1] = getSign(pRec[x+stride+1] - pRec[x]);
                }

                //1st line
                if (pbBorderAvail.m_top_left)
                {
                    edgeType = getSign(pRec[0] - tmpU[-1]) - pUpBuff[1] + 2;
                    pRec[0]  = pClipTable[pRec[0] + pOffsetEo[edgeType]];
                }

                if (pbBorderAvail.m_top)
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
                if (pbBorderAvail.m_bottom)
                {
                    for (x = startX; x < LCUWidth - 1; x++)
                    {
                        edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                        pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                }

                if (pbBorderAvail.m_bottom_right)
                {
                    x = LCUWidth - 1;
                    edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                    pRec[x]  = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                }
                break;
            }
        case SAO_EO_3: // dir: 45
            {
                if (pbBorderAvail.m_left)
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

                endX   = (pbBorderAvail.m_right) ? LCUWidth : (LCUWidth -1);

                //prepare 2nd line upper sign
                tmpUpBuff1[startX] = getSign(startPtr[startStride] - pRec[startX]);
                for (x = startX; x < endX; x++)
                {
                    tmpUpBuff1[x+1] = getSign(pRec[x+stride] - pRec[x+1]);
                }

                //first line
                if (pbBorderAvail.m_top)
                {
                    for (x = startX; x < LCUWidth - 1; x++)
                    {
                        edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                        pRec[x] = pClipTable[pRec[x] + pOffsetEo[edgeType]];
                    }
                }

                if (pbBorderAvail.m_top_right)
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
                if (pbBorderAvail.m_bottom_left)
                {
                    edgeType = getSign(pRec[0] - pRec[stride-1]) + tmpUpBuff1[1] + 2;
                    pRec[0] = pClipTable[pRec[0] + pOffsetEo[edgeType]];

                }

                if (pbBorderAvail.m_bottom)
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


    const int   g_skipLinesR[3] = {1, 1, 1};// YCbCr
    const int   g_skipLinesB[3] = {1, 1, 1};// YCbCr

#ifndef PixType
#define PixType Ipp8u
#endif

    // Encoder part of SAO
ALIGN_DECL(16) static const Ipp16u tab_killmask[8][8] = {
    { 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead },
};

static
void h265_sao_EO_0_sse4(
        const PixType* recBlk,
        int recStride,
        const PixType* orgBlk,
        int orgStride,
        IppiRect roi,
        Ipp32s* sse4_diff,
        Ipp16s* sse4_count)
{
    int x, y, startX, startY, endX, endY;

    const PixType *recLine = recBlk;
    const PixType *orgLine = orgBlk;

    __m128i diff0 = _mm_setzero_si128();
    __m128i diff1 = _mm_setzero_si128();
    __m128i diff2 = _mm_setzero_si128();
    __m128i diff3 = _mm_setzero_si128();
    __m128i diff4 = _mm_setzero_si128();
    __m128i count0 = _mm_setzero_si128();
    __m128i count1 = _mm_setzero_si128();
    __m128i count2 = _mm_setzero_si128();
    __m128i count3 = _mm_setzero_si128();
    __m128i count4 = _mm_setzero_si128();
    __m128i negOne = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
    __m128i recn, rec0, rec1, org0;
    __m128i sign0, sign1, type;
    __m128i mask, diff;

    // for testing, stay entirely inside block
    startY = roi.y;
    endY = startY + roi.height;
    startX = roi.x;
    endX = startX + roi.width;

    for (y = startY; y < endY; y++)
    {
        // process 8 pixels per iteration
        for (x = startX; x < endX - startX - 7; x += 8)
        {
            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x-1])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x+1])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signLeft, signRight
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, negOne);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        // process remaining 1..7 pixels
        if (x < endX)
        {
            // kill out-of-bound values
            mask = _mm_load_si128((__m128i *)tab_killmask[endX - x]);

            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x-1])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x+1])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signLeft, signRight
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, mask);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        recLine  += recStride;
        orgLine  += orgStride;
    }

    // horizontal sum of diffs, using adder tree
    diff0 = _mm_hadd_epi32(diff0, diff1);
    diff2 = _mm_hadd_epi32(diff2, diff3);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    diff0 = _mm_hadd_epi32(diff0, diff2);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    _mm_storeu_si128((__m128i *)&sse4_diff[0], diff0);
    sse4_diff[4] = _mm_cvtsi128_si32(diff4);

    // horizontal sum of diffs, using adder tree
    count0 = _mm_hadd_epi16(count0, count1);
    count2 = _mm_hadd_epi16(count2, count3);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count2);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count4); 

    _mm_storel_epi64((__m128i *)&sse4_count[0], count0);
    sse4_count[4] = _mm_extract_epi16(count0, 4);

} // void h265_sao_EO_0_sse4(...)


static
void h265_sao_EO_90_sse4(
        const PixType* recBlk,
        int recStride,
        const PixType* orgBlk,
        int orgStride,
        IppiRect roi,
        Ipp32s* sse4_diff,
        Ipp16s* sse4_count)
{
    int x, y, startX, startY, endX, endY;

    const PixType *recLine = recBlk;
    const PixType *orgLine = orgBlk;

    __m128i diff0 = _mm_setzero_si128();
    __m128i diff1 = _mm_setzero_si128();
    __m128i diff2 = _mm_setzero_si128();
    __m128i diff3 = _mm_setzero_si128();
    __m128i diff4 = _mm_setzero_si128();
    __m128i count0 = _mm_setzero_si128();
    __m128i count1 = _mm_setzero_si128();
    __m128i count2 = _mm_setzero_si128();
    __m128i count3 = _mm_setzero_si128();
    __m128i count4 = _mm_setzero_si128();
    __m128i negOne = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
    __m128i recn, rec0, rec1, org0;
    __m128i sign0, sign1, type;
    __m128i mask, diff;

    // for testing, stay entirely inside block
    startY = roi.y;
    endY = startY + roi.height;
    startX = roi.x;
    endX = startX + roi.width;

    for (y = startY; y < endY; y++)
    {
        // process 8 pixels per iteration
        for (x = startX; x < endX - startX - 7; x += 8)
        {
            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x-recStride])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x+recStride])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signUp(Left), signDown(Right)
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, negOne);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        // process remaining 1..7 pixels
        if (x < endX)
        {
            // kill out-of-bound values
            mask = _mm_load_si128((__m128i *)tab_killmask[endX - x]);

            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x-recStride])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x+recStride])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signLeft, signRight
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, mask);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        recLine  += recStride;
        orgLine  += orgStride;
    }

    // horizontal sum of diffs, using adder tree
    diff0 = _mm_hadd_epi32(diff0, diff1);
    diff2 = _mm_hadd_epi32(diff2, diff3);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    diff0 = _mm_hadd_epi32(diff0, diff2);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    _mm_storeu_si128((__m128i *)&sse4_diff[0], diff0);
    sse4_diff[4] = _mm_cvtsi128_si32(diff4);

    // horizontal sum of diffs, using adder tree
    count0 = _mm_hadd_epi16(count0, count1);
    count2 = _mm_hadd_epi16(count2, count3);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count2);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count4); 

    _mm_storel_epi64((__m128i *)&sse4_count[0], count0);
    sse4_count[4] = _mm_extract_epi16(count0, 4);

}
// void h265_sao_EO_90_sse4(...)


static
void h265_sao_EO_135_sse4(
        const PixType* recBlk,
        int recStride,
        const PixType* orgBlk,
        int orgStride,
        IppiRect roi,
        Ipp32s* sse4_diff,
        Ipp16s* sse4_count)
{
    int x, y, startX, startY, endX, endY;

    const PixType *recLine = recBlk;
    const PixType *orgLine = orgBlk;

    __m128i diff0 = _mm_setzero_si128();
    __m128i diff1 = _mm_setzero_si128();
    __m128i diff2 = _mm_setzero_si128();
    __m128i diff3 = _mm_setzero_si128();
    __m128i diff4 = _mm_setzero_si128();
    __m128i count0 = _mm_setzero_si128();
    __m128i count1 = _mm_setzero_si128();
    __m128i count2 = _mm_setzero_si128();
    __m128i count3 = _mm_setzero_si128();
    __m128i count4 = _mm_setzero_si128();
    __m128i negOne = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
    __m128i recn, rec0, rec1, org0;
    __m128i sign0, sign1, type;
    __m128i mask, diff;

    // for testing, stay entirely inside block
    startY = roi.y;
    endY = startY + roi.height;
    startX = roi.x;
    endX = startX + roi.width;

    for (y = startY; y < endY; y++)
    {
        // process 8 pixels per iteration
        for (x = startX; x < endX - startX - 7; x += 8)
        {
            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x-1 -recStride])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x+1+recStride])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signUp(Left), signDown(Right)
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, negOne);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        // process remaining 1..7 pixels
        if (x < endX)
        {
            // kill out-of-bound values
            mask = _mm_load_si128((__m128i *)tab_killmask[endX - x]);

            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x-1-recStride])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x+1+recStride])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signLeft, signRight
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, mask);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        recLine  += recStride;
        orgLine  += orgStride;
    }

    // horizontal sum of diffs, using adder tree
    diff0 = _mm_hadd_epi32(diff0, diff1);
    diff2 = _mm_hadd_epi32(diff2, diff3);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    diff0 = _mm_hadd_epi32(diff0, diff2);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    _mm_storeu_si128((__m128i *)&sse4_diff[0], diff0);
    sse4_diff[4] = _mm_cvtsi128_si32(diff4);

    // horizontal sum of diffs, using adder tree
    count0 = _mm_hadd_epi16(count0, count1);
    count2 = _mm_hadd_epi16(count2, count3);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count2);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count4); 

    _mm_storel_epi64((__m128i *)&sse4_count[0], count0);
    sse4_count[4] = _mm_extract_epi16(count0, 4);

} // h265_sao_EO_135_sse4(...)


static
void h265_sao_EO_45_sse4(
        const PixType* recBlk,
        int recStride,
        const PixType* orgBlk,
        int orgStride,
        IppiRect roi,
        Ipp32s* sse4_diff,
        Ipp16s* sse4_count)
{
    int x, y, startX, startY, endX, endY;

    const PixType *recLine = recBlk;
    const PixType *orgLine = orgBlk;

    __m128i diff0 = _mm_setzero_si128();
    __m128i diff1 = _mm_setzero_si128();
    __m128i diff2 = _mm_setzero_si128();
    __m128i diff3 = _mm_setzero_si128();
    __m128i diff4 = _mm_setzero_si128();
    __m128i count0 = _mm_setzero_si128();
    __m128i count1 = _mm_setzero_si128();
    __m128i count2 = _mm_setzero_si128();
    __m128i count3 = _mm_setzero_si128();
    __m128i count4 = _mm_setzero_si128();
    __m128i negOne = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
    __m128i recn, rec0, rec1, org0;
    __m128i sign0, sign1, type;
    __m128i mask, diff;

    // for testing, stay entirely inside block
    startY = roi.y;
    endY = startY + roi.height;
    startX = roi.x;
    endX = startX + roi.width;

    for (y = startY; y < endY; y++)
    {
        // process 8 pixels per iteration
        for (x = startX; x < endX - startX - 7; x += 8)
        {
            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x+1 -recStride])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x-1+recStride])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signUp(Left), signDown(Right)
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, negOne);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        // process remaining 1..7 pixels
        if (x < endX)
        {
            // kill out-of-bound values
            mask = _mm_load_si128((__m128i *)tab_killmask[endX - x]);

            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x+1-recStride])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x-1+recStride])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signLeft, signRight
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, mask);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        recLine  += recStride;
        orgLine  += orgStride;
    }

    // horizontal sum of diffs, using adder tree
    diff0 = _mm_hadd_epi32(diff0, diff1);
    diff2 = _mm_hadd_epi32(diff2, diff3);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    diff0 = _mm_hadd_epi32(diff0, diff2);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    _mm_storeu_si128((__m128i *)&sse4_diff[0], diff0);
    sse4_diff[4] = _mm_cvtsi128_si32(diff4);

    // horizontal sum of diffs, using adder tree
    count0 = _mm_hadd_epi16(count0, count1);
    count2 = _mm_hadd_epi16(count2, count3);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count2);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count4); 

    _mm_storel_epi64((__m128i *)&sse4_count[0], count0);
    sse4_count[4] = _mm_extract_epi16(count0, 4);

} // h265_sao_EO_45_sse4(...)


static
void h265_sao_EO_general_sse4(
        const PixType* recBlk,
        int recStride,
        const PixType* orgBlk,
        int orgStride,
        IppiRect roi,
        Ipp32s* sse4_diff,
        Ipp16s* sse4_count,
        int typeId)
{
    int x, y, startX, startY, endX, endY;

    const PixType *recLine = recBlk;
    const PixType *orgLine = orgBlk;

    __m128i diff0 = _mm_setzero_si128();
    __m128i diff1 = _mm_setzero_si128();
    __m128i diff2 = _mm_setzero_si128();
    __m128i diff3 = _mm_setzero_si128();
    __m128i diff4 = _mm_setzero_si128();
    __m128i count0 = _mm_setzero_si128();
    __m128i count1 = _mm_setzero_si128();
    __m128i count2 = _mm_setzero_si128();
    __m128i count3 = _mm_setzero_si128();
    __m128i count4 = _mm_setzero_si128();
    __m128i negOne = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
    __m128i recn, rec0, rec1, org0;
    __m128i sign0, sign1, type;
    __m128i mask, diff;

    // for testing, stay entirely inside block
    startY = roi.y;
    endY = startY + roi.height;
    startX = roi.x;
    endX = startX + roi.width;

    // offset
    const int tab_offsetX[] = {-1, 0, -1, 1};
    const int offsetX = tab_offsetX[typeId];
    const int offsetStride = (SAO_EO_0 == typeId) ? 0 : -recStride;
    const int offset = offsetX + offsetStride;

    for (y = startY; y < endY; y++)
    {
        // process 8 pixels per iteration
        for (x = startX; x < endX - startX - 7; x += 8)
        {
            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x + offset])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x - offset])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signUp(Left), signDown(Right)
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, negOne);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        // process remaining 1..7 pixels
        if (x < endX)
        {
            // kill out-of-bound values
            mask = _mm_load_si128((__m128i *)tab_killmask[endX - x]);

            // promote to 16-bit
            recn = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x + offset])));
            rec0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x])));
            rec1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&recLine[x - offset])));
            org0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(&orgLine[x])));

            // compute signLeft, signRight
            sign0 = _mm_sign_epi16(negOne, _mm_sub_epi16(recn, rec0));
            sign1 = _mm_sign_epi16(negOne, _mm_sub_epi16(rec1, rec0));
            sign0 = _mm_sub_epi16(sign0, mask);

            // compute diff
            diff = _mm_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm_cmpeq_epi16(type, negOne);
            count0 = _mm_sub_epi16(count0, mask);
            diff0 = _mm_sub_epi32(diff0, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm_cmpeq_epi16(type, negOne);
            count1 = _mm_sub_epi16(count1, mask);
            diff1 = _mm_sub_epi32(diff1, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count2 = _mm_sub_epi16(count2, mask);
            diff2 = _mm_sub_epi32(diff2, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm_cmpeq_epi16(type, negOne);
            count3 = _mm_sub_epi16(count3, mask);
            diff3 = _mm_sub_epi32(diff3, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));

            type = _mm_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm_cmpeq_epi16(type, negOne);
            count4 = _mm_sub_epi16(count4, mask);
            diff4 = _mm_sub_epi32(diff4, _mm_madd_epi16(_mm_and_si128(mask, diff), negOne));
        }

        recLine  += recStride;
        orgLine  += orgStride;
    }

    // horizontal sum of diffs, using adder tree
    diff0 = _mm_hadd_epi32(diff0, diff1);
    diff2 = _mm_hadd_epi32(diff2, diff3);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    diff0 = _mm_hadd_epi32(diff0, diff2);
    diff4 = _mm_hadd_epi32(diff4, diff4);

    _mm_storeu_si128((__m128i *)&sse4_diff[0], diff0);
    sse4_diff[4] = _mm_cvtsi128_si32(diff4);

    // horizontal sum of diffs, using adder tree
    count0 = _mm_hadd_epi16(count0, count1);
    count2 = _mm_hadd_epi16(count2, count3);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count2);
    count4 = _mm_hadd_epi16(count4, count4);

    count0 = _mm_hadd_epi16(count0, count4); 

    _mm_storel_epi64((__m128i *)&sse4_count[0], count0);
    sse4_count[4] = _mm_extract_epi16(count0, 4);

} // h265_sao_EO_general_sse4(...)


static void h265_sao_BO_sse(
    const PixType* recLine,
    int recStride,
    const PixType* orgLine,
    int orgStride,
    Ipp64s *diff,
    Ipp64s* count,
    int endX,
    int endY)
{
    const int shiftBits = sizeof(PixType) == 1 ? 3 : 5;
    for (int y=0; y<endY; y++)
    {
        for (int x=0; x<endX; x++)
        {
            int bandIdx= recLine[x] >> shiftBits;
            diff [bandIdx] += (orgLine[x] - recLine[x]);
            count[bandIdx]++;
        }
        recLine += recStride;
        orgLine += orgStride;
    }

    return;

}


    void MAKE_NAME(h265_GetCtuStatistics_8u)(SAOCU_ENCODE_PARAMETERS_LIST)
    {
        Ipp16s signLineBuf1[64+1];
        Ipp16s signLineBuf2[64+1];

        int x, startX, startY, endX, endY;
        int firstLineStartX, firstLineEndX;
        int edgeType;
        Ipp64s *diff, *count;

        //const int compIdx = SAO_Y;
        int skipLinesR = g_skipLinesR[compIdx];
        int skipLinesB = g_skipLinesB[compIdx];

        const PixType *recLine, *orgLine;
        const PixType* recLineAbove;
        const PixType* recLineBelow;


        for (int typeIdx = 0; typeIdx < numSaoModes; typeIdx++)  { // numSaoModes!!!
            SaoCtuStatistics &statsData = statsDataTypes[typeIdx];
            statsData.Reset();

            recLine = recBlk;
            orgLine = orgBlk;
            diff    = statsData.diff;
            count   = statsData.count;
            switch(typeIdx) {
            case SAO_EO_0: {
                startY = 0;
                endY   = height - skipLinesB;

                startX = borders.m_left ? 0 : 1;
                endX   = width - skipLinesR;

                IppiRect roi = {startX, startY, endX-startX, endY-startY};
                Ipp32s sse4_diff[5];
                Ipp16s sse4_count[5];

                h265_sao_EO_general_sse4(recLine, recStride, orgLine, orgStride, roi, sse4_diff,
                                         sse4_count, typeIdx);

                for(int idxType = 0; idxType < 5; idxType++) {
                    diff[idxType]  = sse4_diff[idxType];
                    count[idxType]  = sse4_count[idxType];
                }
            }
            break;

            case SAO_EO_1: {
                if (0 == borders.m_top) {
                    recLine += recStride;
                    orgLine += orgStride;
                }

                startX = 0;
                endX = width - skipLinesR;

                startY = borders.m_top ? 0 : 1;
                endY = height - skipLinesB;

                IppiRect roi = {startX, startY, endX-startX, endY-startY};
                Ipp32s sse4_diff[5];
                Ipp16s sse4_count[5];

                h265_sao_EO_general_sse4(recLine, recStride, orgLine, orgStride, roi,
                                            sse4_diff, sse4_count, typeIdx);

                for(int idxType = 0; idxType < 5; idxType++) {
                    diff[idxType]  = sse4_diff[idxType];
                    count[idxType]  = sse4_count[idxType];
                }
            }
            break;

            case SAO_EO_2: {
                bool isGeneralCase = (borders.m_top_left) || (!borders.m_top && borders.m_left) ||
                       (borders.m_top && !borders.m_left) || (!borders.m_top && !borders.m_left);

                if(isGeneralCase) {
                    if(borders.m_top_left) {
                        startX = 0;
                        endX   = width - skipLinesR;
                        startY = 0;
                        endY = height - skipLinesB;
                    }
                    else if(!borders.m_top && borders.m_left) {
                        startX = 0;
                        endX   = width - skipLinesR;
                        startY = 1;
                        endY = height - skipLinesB;
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                    else if( borders.m_top && !borders.m_left) {
                        startX = 1;
                        endX   = width - skipLinesR;
                        startY = 0;
                        endY = height - skipLinesB;
                    }
                    else if(!borders.m_top && !borders.m_left) {
                        startX = 1;
                        endX   = width - skipLinesR;
                        startY = 1;
                        endY = height - skipLinesB;
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }

                    IppiRect roi = {startX, startY, endX-startX, endY-startY};
                    Ipp32s sse4_diff[5] = {0};
                    Ipp16s sse4_count[5]= {0};

                    h265_sao_EO_general_sse4(recLine, recStride, orgLine, orgStride, roi,
                                             sse4_diff, sse4_count, typeIdx);

                    for(int idxType = 0; idxType < 5; idxType++) {
                        diff[idxType]  = sse4_diff[idxType];
                        count[idxType] = sse4_count[idxType];
                    }
                }
                else {
                    diff  +=2;
                    count +=2;
                    startX = borders.m_left ? 0 : 1 ;
                    endX   = width - skipLinesR;
                    endY   = height - skipLinesB;

                    //prepare 2nd line's upper sign
                    Ipp16s *signUpLine, *signDownLine;
                    signUpLine  = signLineBuf1;
                    signDownLine= signLineBuf2;
                    recLineBelow = recLine + recStride;
                    for (x = startX; x < endX + 1; x++)
                        signUpLine[x] = getSign(recLineBelow[x] - recLine[x-1]);

                    //1st line
                    recLineAbove = recLine - recStride;
                    firstLineStartX = borders.m_top_left ? 0 : 1;
                    firstLineEndX   = borders.m_top      ? endX : 1;

                    for (x = firstLineStartX; x < firstLineEndX; x++) {
                        edgeType = getSign(recLine[x] - recLineAbove[x-1]) - signUpLine[x+1];
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine  += recStride;
                    orgLine  += orgStride;

                    startY = 1;
                    IppiRect roi = {startX, startY, endX-startX, endY-startY};
                    Ipp32s sse4_diff[5] = {0};
                    Ipp16s sse4_count[5]= {0};

                    h265_sao_EO_general_sse4(recLine, recStride, orgLine, orgStride, roi,
                                             sse4_diff, sse4_count, typeIdx);

                    for(int idxType = 0; idxType < 5; idxType++) {
                        diff[idxType - 2]  += sse4_diff[idxType];
                        count[idxType - 2] += sse4_count[idxType];
                    }
                }
            }
            break;

            case SAO_EO_3: {
                startY = borders.m_top ? 0 : 1;
                endY   = height - skipLinesB;
                startX = borders.m_left ? 0 : 1;
                endX   = width - skipLinesR;

                if (!borders.m_top) {
                    recLine += recStride;
                    orgLine += orgStride;
                }
                        
                IppiRect roi = {startX, startY, endX-startX, endY-startY};
                Ipp32s sse4_diff[5] = {0};
                Ipp16s sse4_count[5]= {0};

                h265_sao_EO_general_sse4(recLine, recStride, orgLine, orgStride, roi,
                                            sse4_diff, sse4_count, typeIdx);

                for(int idxType = 0; idxType < 5; idxType++) {
                    diff[idxType]  = sse4_diff[idxType];
                    count[idxType] = sse4_count[idxType];
                }
            }
            break;

            case SAO_BO: {
                endX = width- skipLinesR;
                endY = height- skipLinesB;
                h265_sao_BO_sse(recLine, recStride, orgLine, orgStride, diff, count, endX, endY);
            }
            break;

            default: {
                VM_ASSERT(!"Not a supported SAO types\n");
            }
            }
        }
    }
};

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
