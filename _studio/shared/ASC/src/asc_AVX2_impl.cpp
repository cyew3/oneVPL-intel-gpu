//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#include "asc_AVX2_impl.h"

#if defined(__AVX2__)

void ME_SAD_8x8_Block_Search_AVX2(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange,
    mfxU16 *bestSAD, int *bestX, int *bestY) {
    __m256i
        s0 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[0 * pitch]), (__m128i *)&pSrc[1 * pitch])),
        s1 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[2 * pitch]), (__m128i *)&pSrc[3 * pitch])),
        s2 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[4 * pitch]), (__m128i *)&pSrc[5 * pitch])),
        s3 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[6 * pitch]), (__m128i *)&pSrc[7 * pitch]));

    for (int y = 0; y < yrange; y += SAD_SEARCH_VSTEP) {
        for (int x = 0; x < xrange; x += 8) {
            pmfxU8 pr = pRef + (y * pitch) + x;
            __m256i
                r0 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[0 * pitch])),
                r1 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[1 * pitch])),
                r2 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[2 * pitch])),
                r3 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[3 * pitch])),
                r4 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[4 * pitch])),
                r5 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[5 * pitch])),
                r6 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[6 * pitch])),
                r7 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[7 * pitch]));
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
            // kill every other SAD results, simulating search every two in X dimension
            t = _mm_or_si128(t, _mm_load_si128((__m128i *)tab_twostep));
            // kill out-of-bound values
            if (xrange - x < 8)
                t = _mm_or_si128(t, _mm_load_si128((__m128i *)tab_killmask[xrange - x]));
            t = _mm_minpos_epu16(t);
            mfxU16
                SAD = (mfxU16)_mm_extract_epi16(t, 0);
            if (SAD < *bestSAD) {
                *bestSAD = SAD;
                *bestX = x + _mm_extract_epi16(t, 1);
                *bestY = y;
            }
        }
    }
}

void ME_SAD_8x8_Block_FSearch_AVX2(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange,
    mfxU32 *bestSAD, int *bestX, int *bestY) {
    __m256i
        s0 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[0 * pitch]), (__m128i *)&pSrc[1 * pitch])),
        s1 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[2 * pitch]), (__m128i *)&pSrc[3 * pitch])),
        s2 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[4 * pitch]), (__m128i *)&pSrc[5 * pitch])),
        s3 = _mm256_broadcastsi128_si256(_mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[6 * pitch]), (__m128i *)&pSrc[7 * pitch]));

    for (int y = 0; y < yrange; y++) {
        for (int x = 0; x < xrange; x += 8) {
            pmfxU8
                pr = pRef + (y * pitch) + x;
            __m256i r0 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[0 * pitch])),
                r1 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[1 * pitch])),
                r2 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[2 * pitch])),
                r3 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[3 * pitch])),
                r4 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[4 * pitch])),
                r5 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[5 * pitch])),
                r6 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[6 * pitch])),
                r7 = _mm256_broadcastsi128_si256(_mm_loadu_si128((__m128i *)&pr[7 * pitch]));
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

#if (defined( _WIN32 ) || defined ( _WIN64 )) && !defined (__GNUC__)
void RsCsCalc_4x4_AVX2(pmfxU8 pSrc, int srcPitch, int wblocks, int hblocks, pmfxU16 pRs, pmfxU16 pCs)
{
    const __m256i zero = _mm256_setzero_si256();
    mfxU32 count = 0;
    pSrc += (4 * srcPitch) + 4;
    for (mfxI32 i = 0; i < hblocks - 2; i++)
    {
        // 4 horizontal blocks at a time
        mfxI32 j;
        for (j = 0; j < wblocks - 5; j += 4)
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
                a0 = _mm256_srai_epi16(_mm256_abs_epi16(_mm256_sub_epi16(c0, a0)), 2);
                a0 = _mm256_madd_epi16(a0, a0);
                rs = _mm256_add_epi32(rs, a0);

                // accCs += dCs * dCs
                b0 = _mm256_srai_epi16(_mm256_abs_epi16(_mm256_sub_epi16(c0, b0)), 2);
                b0 = _mm256_madd_epi16(b0, b0);
                cs = _mm256_add_epi32(cs, b0);

                // reuse next iteration
                a0 = c0;
            }

            // horizontal sum
            rs = _mm256_hadd_epi32(rs, cs);
            rs = _mm256_permute4x64_epi64(rs, _MM_SHUFFLE(3, 1, 2, 0));    // [ cs3 cs2 cs1 cs0 rs3 rs2 rs1 rs0 ]

                                                                           // store
            rs = _mm256_packus_epi32(rs, rs);
#ifdef __INTEL_COMPILER
            _mm_storeu_si64(&pRs[i * wblocks + j], _mm256_castsi256_si128(rs));
            _mm_storeu_si64(&pCs[i * wblocks + j], _mm256_extracti128_si256(rs, 1));
#else
            *((mfxU64*)&pRs[i * wblocks + j]) = ((rs).m256i_u64)[0];
            *((mfxU64*)&pCs[i * wblocks + j]) = ((rs).m256i_u64)[2];
#endif
            pSrc -= 4 * srcPitch;
            pSrc += 16;
            count += 4;
        }

        // remaining blocks
        for (; j < wblocks - 2; j++)
        {
            mfxU16 accRs = 0;
            mfxU16 accCs = 0;

            for (mfxU32 k = 0; k < 4; k++)
            {
                for (mfxI32 l = 0; l < 4; l++)
                {
                    mfxU16 dRs = (mfxU16)abs(pSrc[l] - pSrc[l - srcPitch]) >> 2;
                    mfxU16 dCs = (mfxU16)abs(pSrc[l] - pSrc[l - 1]) >> 2;
                    accRs += dRs * dRs;
                    accCs += dCs * dCs;
                }
                pSrc += srcPitch;
            }
            pRs[i * wblocks + j] = accRs;
            pCs[i * wblocks + j] = accCs;

            pSrc -= 4 * srcPitch;
            pSrc += 4;
            count++;
        }
        pSrc -= 4 * (wblocks - 2);
        pSrc += 4 * srcPitch;
    }
}

void RsCsCalc_bound_AVX2(pmfxU16 pRs, pmfxU16 pCs, pmfxU16 pRsCs, pmfxU32 pRsFrame, pmfxU32 pCsFrame, int wblocks, int hblocks) {
    mfxI32 i, len = wblocks * hblocks;
    __m256i accRs = _mm256_setzero_si256();
    __m256i accCs = _mm256_setzero_si256();

    for (i = 0; i < len - 7; i += 8)
    {
        __m256i rs = _mm256_srai_epi32(_mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i*)(&pRs[i]))), 7);
        __m256i cs = _mm256_srai_epi32(_mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i*)&(pCs[i]))), 7);
        __m256i rc = _mm256_srai_epi32(_mm256_add_epi32(rs, cs), 1);

        accRs = _mm256_add_epi32(accRs, rs);
        accCs = _mm256_add_epi32(accCs, cs);

        rc = _mm256_packus_epi32(rc, rc);
        _mm_store_si128((__m128i*)&pRsCs[i], _mm256_extracti128_si256(rc, 0));
    }

    if (i < len)
    {
        __m256i rs = _mm256_srai_epi32(LoadPartialYmm<0>((unsigned char*)&pRs[i], len & 0x7), 7);
        __m256i cs = _mm256_srai_epi32(LoadPartialYmm<0>((unsigned char*)&pCs[i], len & 0x7), 7);
        __m256i rc = _mm256_srai_epi32(_mm256_add_epi32(rs, cs), 1);

        accRs = _mm256_add_epi32(accRs, rs);
        accCs = _mm256_add_epi32(accCs, cs);

        //        StorePartialYmm<0>((unsigned char*)&pRsCs[i], rc, len & 0xF);
#if __INTEL_COMPILER
        StorePartialYmm<0>((unsigned char*)&pRsCs[i], rc, len & 0xF);
#else
        __m256
            t = _mm256_castsi256_ps(rc);
        StorePartialYmm((float*)&pRsCs[i], t, len & 0x7);
#endif
    }

    // horizontal sum
    accRs = _mm256_hadd_epi32(accRs, accCs);    //Count 4
    accRs = _mm256_hadd_epi32(accRs, accRs);    //Count 2
    __m128i t = _mm_add_epi32(_mm256_castsi256_si128(accRs), _mm256_extracti128_si256(accRs, 1));   //Count 1

#ifdef __INTEL_COMPILER
    _mm_storeu_si32(pRsFrame, t);
    _mm_storeu_si32(pCsFrame, _mm_shuffle_epi32(t), _MM_SHUFFLE(0, 0, 0, 1));
#else
    *pRsFrame = ((t).m128i_u32)[0];
    *pCsFrame = ((t).m128i_u32)[1];
#endif
}

void RsCsCalc_diff_AVX2(pmfxU16 pRs0, pmfxU16 pCs0, pmfxU16 pRs1, pmfxU16 pCs1, int wblocks, int hblocks,
    pmfxU32 pRsDiff, pmfxU32 pCsDiff)
{
    mfxU32 i, len = wblocks * hblocks;
    __m256i accRs = _mm256_setzero_si256();
    __m256i accCs = _mm256_setzero_si256();

    for (i = 0; i < len - 7; i += 8)
    {
        __m256i rs = _mm256_sub_epi32(_mm256_srai_epi32(_mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i*)(&pRs0[i]))), 5), _mm256_srai_epi32(_mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i*)(&pRs1[i]))), 5));
        __m256i cs = _mm256_sub_epi32(_mm256_srai_epi32(_mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i*)(&pCs0[i]))), 5), _mm256_srai_epi32(_mm256_cvtepu16_epi32(_mm_loadu_si128((__m128i*)(&pCs1[i]))), 5));

        rs = _mm256_abs_epi32(rs);
        cs = _mm256_abs_epi32(cs);

        accRs = _mm256_add_epi32(accRs, rs);
        accCs = _mm256_add_epi32(accCs, cs);
    }

    if (i < len)
    {
        __m256i rs = _mm256_sub_epi32(_mm256_srai_epi32(_mm256_cvtepu16_epi32(LoadPartialXmm<0>((unsigned char*)(&pRs0[i]), len & 0x7)), 5), _mm256_srai_epi32(_mm256_cvtepu16_epi32(LoadPartialXmm<0>((unsigned char*)(&pRs1[i]), len & 0x7)), 5));
        __m256i cs = _mm256_sub_epi32(_mm256_srai_epi32(_mm256_cvtepu16_epi32(LoadPartialXmm<0>((unsigned char*)(&pCs0[i]), len & 0x7)), 5), _mm256_srai_epi32(_mm256_cvtepu16_epi32(LoadPartialXmm<0>((unsigned char*)(&pCs1[i]), len & 0x7)), 5));

        rs = _mm256_abs_epi32(rs);
        cs = _mm256_abs_epi32(cs);

        accRs = _mm256_add_epi32(accRs, rs);
        accCs = _mm256_add_epi32(accCs, cs);
    }

    // horizontal sum
    accRs = _mm256_hadd_epi32(accRs, accCs);//8 to 4
    accRs = _mm256_hadd_epi32(accRs, accRs);//4 to 2

    __m128i t = _mm_add_epi32(_mm256_castsi256_si128(accRs), _mm256_extracti128_si256(accRs, 1));//2 to 1
#ifdef __INTEL_COMPILER
    _mm_storeu_si32(pRsDiff, t);
    _mm_storeu_si32(pCsDiff, _mm_shuffle_epi32(t, _MM_SHUFFLE(0, 0, 0, 1)));
#else
    *pRsDiff = ((t).m128i_u32)[0];
    *pCsDiff = ((t).m128i_u32)[1];
#endif
}

void ImageDiffHistogram_AVX2(pmfxU8 pSrc, pmfxU8 pRef, mfxU32 pitch, mfxU32 width, mfxU32 height, mfxI32 histogram[5], mfxI64 *pSrcDC, mfxI64 *pRefDC) {
    __m256i sDC = _mm256_setzero_si256();
    __m256i rDC = _mm256_setzero_si256();

    __m256i h0 = _mm256_setzero_si256();
    __m256i h1 = _mm256_setzero_si256();
    __m256i h2 = _mm256_setzero_si256();
    __m256i h3 = _mm256_setzero_si256();

    __m256i zero = _mm256_setzero_si256();

    for (mfxU32 i = 0; i < height; i++)
    {
        // process 32 pixels per iteration
        mfxU32 j;
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
    histogram[4] = 8192;// (mfxI32)(width * height);

                        // undo cumulative counts, by differencing
    histogram[4] -= histogram[3];
    histogram[3] -= histogram[2];
    histogram[2] -= histogram[1];
    histogram[1] -= histogram[0];
}

void GainOffset_AVX2(pmfxU8 *pSrc, pmfxU8 *pDst, mfxU16 width, mfxU16 height, mfxU16 pitch, mfxI16 gainDiff) {
    __m256i
        diff = _mm256_set1_epi16(gainDiff);
    const __m256i
        minVal = _mm256_set1_epi16(-1);
    pmfxU8
        ss = *pSrc,
        dd = *pDst;
    for (mfxU16 i = 0; i < height; i++)
    {
        // 16 pixels at a time
        mfxI16 j;
        for (j = 0; j < width - 15; j += 16)
        {
            __m256i
                a = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i*)&ss[j]));
            a = _mm256_sub_epi16(a, diff);
            __m256i
                r = _mm256_cmpgt_epi16(a, minVal);
            a = _mm256_and_si256(a, r);
            // store
            a = _mm256_packus_epi16(a, a);
            a = _mm256_permute4x64_epi64(a, _MM_SHUFFLE(3, 1, 2, 0));
            _mm_storeu_si128((__m128i*)&dd[j], _mm256_castsi256_si128(a));
        }

        // remaining blocks
        for (; j < width - 7; j += 8) {
            __m128i
                diff2 = _mm_set1_epi16(gainDiff);
            const __m128i
                minVal2 = _mm_set1_epi16(-1);
            __m128i
                b = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&ss[j]));
            b = _mm_sub_epi16(b, diff2);
            __m128i
                s = _mm_cmpgt_epi16(b, minVal2);
            b = _mm_and_si128(b, s);
            // store
            b = _mm_packus_epi16(b, b);
#ifdef __INTEL_COMPILER
            _mm_storeu_si64(&dd[j], b);
#else
            *((mfxU64*)&dd[j]) = ((b).m128i_u64)[0];
#endif
        }
        for (; j < width; j++)
        {
            mfxI16
                val = ss[j] - gainDiff;
            dd[j] = (mfxU8)min(max(val, 0), 255);
        }
        ss += pitch;
        dd += pitch;
    }

    *pSrc = *pDst;
}

mfxStatus Calc_RaCa_pic_AVX2(mfxU8 *pSrc, mfxI32 width, mfxI32 height, mfxI32 pitch, mfxF64 &RsCs) {
    const __m256i
        zero = _mm256_setzero_si256(),
        mask = { 0x0,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x1,0x0,0x0,0x0,0x5,0x0,0x0,0x0,0x2,0x0,0x0,0x0,0x3,0x0,0x0,0x0,0x6,0x0,0x0,0x0,0x7,0x0,0x0,0x0 };
    mfxU32
        count = 0;
    mfxU8*
        pY = pSrc + (4 * pitch) + 4;
    mfxI32
        RS = 0,
        CS = 0,
        i;
    for (i = 0; i < height - 8; i += 4)
    {
        // 4 horizontal blocks at a time
        mfxI32 j;
        for (j = 0; j < width - 20; j += 16)
        {
            __m256i rs = _mm256_setzero_si256();
            __m256i cs = _mm256_setzero_si256();
            __m256i c0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pY[0]));
#ifdef __INTEL_COMPILER
#pragma unroll(4)
#endif
            for (mfxI32 k = 0; k < 4; k++)
            {
                __m256i b0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pY[1]));
                __m256i a0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)&pY[pitch + 0]));
                pY += pitch;

                // Cs += (pS[j] > pS[j + 1]) ? (pS[j] - pS[j + 1]) : (pS[j + 1] - pS[j]);
                b0 = _mm256_abs_epi16(_mm256_sub_epi16(c0, b0));
                cs = _mm256_add_epi16(cs, b0);

                // Rs += (pS[j] > pS2[j]) ? (pS[j] - pS2[j]) : (pS2[j] - pS[j]);
                c0 = _mm256_abs_epi16(_mm256_sub_epi16(c0, a0));
                rs = _mm256_add_epi16(rs, c0);

                // reuse next iteration
                c0 = a0;
            }

            // horizontal sum
            //4 values to 2 values
            rs = _mm256_hadd_epi16(rs, cs);
            //2 values to 1 value
            rs = _mm256_hadd_epi16(rs, rs);
            //organize data so rs is followed by rs and cs is followed by cs
            rs = _mm256_permutevar8x32_epi32(rs, mask);
            //Cs >> 4; Rs >> 4;
            rs = _mm256_srai_epi16(rs, 4);
            //*CS += Cs; *RS += Rs;
            rs = _mm256_hadd_epi16(rs, rs);
            rs = _mm256_hadd_epi16(rs, rs);
            RS += rs.m256i_u16[0];
            CS += rs.m256i_u16[1];

            pY -= 4 * pitch;
            pY += 16;
            count += 4;
        }

        // remaining blocks
        for (; j < width - 8; j += 4)
        {
            calc_RACA_4x4_C(pY, pitch, &RS, &CS);
            pY += 4;
            count++;
        }

        pY -= width - 8;
        pY += 4 * pitch;
    }
    mfxI32 w4 = (width - 8) >> 2;
    mfxI32 h4 = (height - 8) >> 2;
    mfxF64 d1 = 1.0 / (mfxF64)(w4*h4);
    mfxF64 drs = (mfxF64)RS * d1;
    mfxF64 dcs = (mfxF64)CS * d1;

    RsCs = sqrt(drs * drs + dcs * dcs);
    return MFX_ERR_NONE;
}

#endif

mfxI16 AvgLumaCalc_AVX2(pmfxU32 pAvgLineVal, int len) {
    __m256i
        acc = _mm256_setzero_si256();
    __m128i
        shiftVal = _mm_setr_epi32(9, 0, 0, 0);
    mfxI16
        avgVal = 0;

    for (int i = 0; i < len - 7; i += 8) {
        acc = _mm256_add_epi32(_mm256_loadu_si256((__m256i*)&pAvgLineVal[i]), acc);
    }
    acc = _mm256_hadd_epi32(acc, acc);
    acc = _mm256_hadd_epi32(acc, acc);
    __m128i
        tmp = _mm_add_epi32(_mm256_extracti128_si256(acc, 0), _mm256_extracti128_si256(acc, 1));
    tmp = _mm_sra_epi32(tmp, shiftVal);

    avgVal = (mfxI16)_mm_extract_epi32(tmp, 0);
    return avgVal;
}
#endif //defined(__AVX2__)
