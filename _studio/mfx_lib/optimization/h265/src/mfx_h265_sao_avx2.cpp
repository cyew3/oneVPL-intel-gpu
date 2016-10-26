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

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO)

#include <immintrin.h>
#include <assert.h>

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

    const int   g_skipLinesR[3] = {1, 1, 1};// YCbCr
    const int   g_skipLinesB[3] = {1, 1, 1};// YCbCr

//#ifndef PixType
//#define PixType Ipp8u
//#endif

    // Encoder part of SAO
ALIGN_DECL(32) static const Ipp16u tab_killmask_avx2[16][16] = {
    { 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead, 0xdead },
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xdead },
};

template <typename PixType>
static void h265_sao_EO_general_avx2(
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

    __m256i diff0 = _mm256_setzero_si256();
    __m256i diff1 = _mm256_setzero_si256();
    __m256i diff2 = _mm256_setzero_si256();
    __m256i diff3 = _mm256_setzero_si256();
    __m256i diff4 = _mm256_setzero_si256();
    __m256i count0 = _mm256_setzero_si256();
    __m256i count1 = _mm256_setzero_si256();
    __m256i count2 = _mm256_setzero_si256();
    __m256i count3 = _mm256_setzero_si256();
    __m256i count4 = _mm256_setzero_si256();
    __m256i negOne = _mm256_cmpeq_epi8(_mm256_setzero_si256(), _mm256_setzero_si256());
    __m256i recn, rec0, rec1, org0;
    __m256i sign0, sign1, type;
    __m256i mask, diff;

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
        // process 16 pixels per iteration
        for (x = startX; x < endX - startX - 15; x += 16)
        {
            if(sizeof(PixType)==1) {
            // promote to 16-bit
            recn = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&recLine[x + offset])));
            rec0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&recLine[x])));
            rec1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&recLine[x - offset])));
            org0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&orgLine[x+0])));
            } else {
            recn = _mm256_loadu_si256((__m256i *)(&recLine[x + offset]));
            rec0 = _mm256_loadu_si256((__m256i *)(&recLine[x]));
            rec1 = _mm256_loadu_si256((__m256i *)(&recLine[x - offset]));
            org0 = _mm256_loadu_si256((__m256i *)(&orgLine[x+0]));
            }

            // compute signLeft, signRight
            sign0 = _mm256_sign_epi16(negOne, _mm256_sub_epi16(recn, rec0));
            sign1 = _mm256_sign_epi16(negOne, _mm256_sub_epi16(rec1, rec0));
            sign0 = _mm256_sub_epi16(sign0, negOne);

            // compute diff
            diff = _mm256_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm256_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count0 = _mm256_sub_epi16(count0, mask);
            diff0 = _mm256_sub_epi32(diff0, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));

            type = _mm256_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count1 = _mm256_sub_epi16(count1, mask);
            diff1 = _mm256_sub_epi32(diff1, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));

            type = _mm256_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count2 = _mm256_sub_epi16(count2, mask);
            diff2 = _mm256_sub_epi32(diff2, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));

            type = _mm256_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count3 = _mm256_sub_epi16(count3, mask);
            diff3 = _mm256_sub_epi32(diff3, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));

            type = _mm256_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count4 = _mm256_sub_epi16(count4, mask);
            diff4 = _mm256_sub_epi32(diff4, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));
        }

        // process remaining 1..15 pixels
        if (x < endX)
        {
            // kill out-of-bound values
            mask = _mm256_load_si256((__m256i *)tab_killmask_avx2[endX - x]);

            if(sizeof(PixType)==1){
            // promote to 16-bit
            recn = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&recLine[x + offset])));
            rec0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&recLine[x])));
            rec1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&recLine[x - offset])));
            org0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(&orgLine[x])));
            } else {
            recn = _mm256_loadu_si256((__m256i *)(&recLine[x + offset]));
            rec0 = _mm256_loadu_si256((__m256i *)(&recLine[x]));
            rec1 = _mm256_loadu_si256((__m256i *)(&recLine[x - offset]));
            org0 = _mm256_loadu_si256((__m256i *)(&orgLine[x]));
            }

            // compute signLeft, signRight
            sign0 = _mm256_sign_epi16(negOne, _mm256_sub_epi16(recn, rec0));
            sign1 = _mm256_sign_epi16(negOne, _mm256_sub_epi16(rec1, rec0));
            sign0 = _mm256_sub_epi16(sign0, mask);

            // compute diff
            diff = _mm256_sub_epi16(org0, rec0);

            // accumulate for each type
            // promote masked diff to 32-bit, using diff -= madd(diff, -1)
            type = _mm256_add_epi16(sign0, sign1);  // type = [-1,+3]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count0 = _mm256_sub_epi16(count0, mask);
            diff0 = _mm256_sub_epi32(diff0, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));

            type = _mm256_add_epi16(type, negOne);  // type = [-2,+2]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count1 = _mm256_sub_epi16(count1, mask);
            diff1 = _mm256_sub_epi32(diff1, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));

            type = _mm256_add_epi16(type, negOne);  // type = [-3,+1]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count2 = _mm256_sub_epi16(count2, mask);
            diff2 = _mm256_sub_epi32(diff2, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));

            type = _mm256_add_epi16(type, negOne);  // type = [-4, 0]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count3 = _mm256_sub_epi16(count3, mask);
            diff3 = _mm256_sub_epi32(diff3, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));

            type = _mm256_add_epi16(type, negOne);  // type = [-5,-1]
            mask = _mm256_cmpeq_epi16(type, negOne);
            count4 = _mm256_sub_epi16(count4, mask);
            diff4 = _mm256_sub_epi32(diff4, _mm256_madd_epi16(_mm256_and_si256(mask, diff), negOne));
        }

        recLine  += recStride;
        orgLine  += orgStride;
    }

    // horizontal sum of diffs, using adder tree
    diff0 = _mm256_hadd_epi32(diff0, diff1);
    diff2 = _mm256_hadd_epi32(diff2, diff3);
    diff4 = _mm256_hadd_epi32(diff4, diff4);

    diff0 = _mm256_hadd_epi32(diff0, diff2);
    diff4 = _mm256_hadd_epi32(diff4, diff4);

    diff1 = _mm256_permute2x128_si256(diff0, diff4, 0x20);
    diff2 = _mm256_permute2x128_si256(diff0, diff4, 0x31);
    diff0 = _mm256_add_epi32(diff1, diff2);

    _mm256_storeu_si256((__m256i *)&sse4_diff[0], diff0);   // store diff[0..4]

    // horizontal sum of counts, using adder tree
    count0 = _mm256_hadd_epi16(count0, count1);
    count2 = _mm256_hadd_epi16(count2, count3);
    count4 = _mm256_hadd_epi16(count4, count4);

    count0 = _mm256_hadd_epi16(count0, count2);
    count4 = _mm256_hadd_epi16(count4, count4);

    count0 = _mm256_hadd_epi16(count0, count4);

    count1 = _mm256_permute2x128_si256(count0, count0, 0x01);
    count0 = _mm256_add_epi16(count0, count1);

    _mm_storeu_si128((__m128i *)&sse4_count[0], _mm256_castsi256_si128(count0));   // store count[0..4]

} // h265_sao_EO_general_avx2(...)

template <typename PixType>
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


template <typename PixType>
static void h265_GetCtuStatistics_Kernel(int compIdx, const PixType* recBlk, int recStride, const PixType* orgBlk, int orgStride, int width,
        int height, int shift,  const MFX_HEVC_PP::CTBBorders& borders, int numSaoModes, MFX_HEVC_PP::SaoCtuStatistics* statsDataTypes)
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
            Ipp32s sse4_diff[8];
            Ipp16s sse4_count[8];

            h265_sao_EO_general_avx2(recLine, recStride, orgLine, orgStride, roi, sse4_diff,
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
            Ipp32s sse4_diff[8];
            Ipp16s sse4_count[8];

            h265_sao_EO_general_avx2(recLine, recStride, orgLine, orgStride, roi,
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
                Ipp32s sse4_diff[8];
                Ipp16s sse4_count[8];

                h265_sao_EO_general_avx2(recLine, recStride, orgLine, orgStride, roi,
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
                Ipp32s sse4_diff[8];
                Ipp16s sse4_count[8];

                h265_sao_EO_general_avx2(recLine, recStride, orgLine, orgStride, roi,
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
            Ipp32s sse4_diff[8];
            Ipp16s sse4_count[8];

            h265_sao_EO_general_avx2(recLine, recStride, orgLine, orgStride, roi,
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

void MAKE_NAME(h265_GetCtuStatistics_8u)(SAOCU_ENCODE_PARAMETERS_LIST)
{
    h265_GetCtuStatistics_Kernel<Ipp8u>(SAOCU_ENCODE_PARAMETERS_LIST_CALL);
}

void MAKE_NAME(h265_GetCtuStatistics_16u)(SAOCU_ENCODE_PARAMETERS_LIST_16U)
{
    h265_GetCtuStatistics_Kernel<Ipp16u>(SAOCU_ENCODE_PARAMETERS_LIST_16U_CALL);
}


namespace SplitChromaCtbDetails {
    ALIGN_DECL(32) static const Ipp8s shuffleTabs[2][32] = {
        { 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15, 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15 }, // T=8u
        { 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15, 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15 }  // T=16u
    };

    template <class T, Ipp32s width>
    void Impl(const T *nv12, Ipp32s pitchNv12, T *u, Ipp32s pitchU, T *v, Ipp32s pitchV, Ipp32s height)
    {
        assert(width >= 16);
        assert((width & 15) == 0);

        Ipp32s wstep = 16 / sizeof(T);
        __m256i shuffleTab = *(const __m256i *)shuffleTabs[sizeof(T) - 1];

        for (Ipp32s y = 0; y < height; y++) {
            for (Ipp32s x = 0; x < width; x += wstep, nv12 += 2 * wstep, u += wstep, v += wstep) {
                __m256i src = _mm256_load_si256((const __m256i *)nv12);
                src = _mm256_shuffle_epi8(src, shuffleTab);
                src = _mm256_permute4x64_epi64(src, 0xD8);
                _mm_store_si128((__m128i *)u, _mm256_castsi256_si128(src));
                _mm_store_si128((__m128i *)v, _mm256_extracti128_si256(src, 1));
            }

            nv12 += pitchNv12 - 2*width;
            u += pitchU - width;
            v += pitchV - width;
        }
    }
};

template <class T>
void H265_FASTCALL MAKE_NAME(h265_SplitChromaCtb)(const T *nv12, Ipp32s pitchNv12, T *u, Ipp32s pitchU, T *v, Ipp32s pitchV, Ipp32s width, Ipp32s height)
{
    if      (width == 16) SplitChromaCtbDetails::Impl<T,16>(nv12, pitchNv12, u, pitchU, v, pitchV, height);
    else if (width == 32) SplitChromaCtbDetails::Impl<T,32>(nv12, pitchNv12, u, pitchU, v, pitchV, height);
    else if (width == 64) SplitChromaCtbDetails::Impl<T,64>(nv12, pitchNv12, u, pitchU, v, pitchV, height);
    else    assert(0);
}
template void H265_FASTCALL MAKE_NAME(h265_SplitChromaCtb)<Ipp8u>(const Ipp8u *nv12, Ipp32s pitchNv12, Ipp8u *u, Ipp32s pitchU, Ipp8u *v, Ipp32s pitchV, Ipp32s width, Ipp32s height);
template void H265_FASTCALL MAKE_NAME(h265_SplitChromaCtb)<Ipp16u>(const Ipp16u *nv12, Ipp32s pitchNv12, Ipp16u *u, Ipp32s pitchU, Ipp16u *v, Ipp32s pitchV, Ipp32s width, Ipp32s height);

};

#endif // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
