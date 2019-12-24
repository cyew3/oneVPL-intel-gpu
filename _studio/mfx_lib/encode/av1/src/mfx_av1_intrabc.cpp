// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "algorithm"
#include "mfx_av1_defs.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_copy.h"
#include "mfx_av1_get_context.h"
#include "mfx_av1_dispatcher_wrappers.h"
#include "mfx_av1_get_intra_pred_pels.h"
#include "mfx_av1_trace.h"
#include "mfx_av1_quant.h"

#include <immintrin.h>

namespace AV1PP {
    template <int w, int h> int sad_special_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2);
    template <int w, int h> int sad_general_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
};

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))

namespace AV1Enc {

    int32_t sad8(const uint8_t *ref, int32_t pitch, const __m128i *src)
    {
        const int32_t pitch2 = pitch * 2;
        __m128i sad1, sad2;

        sad1 = _mm_sad_epu8(src[0], loadu2_epi64(ref, ref + pitch));
        ref += pitch2;
        sad2 = _mm_sad_epu8(src[1], loadu2_epi64(ref, ref + pitch));
        ref += pitch2;
        sad1 = _mm_add_epi32(sad1, _mm_sad_epu8(src[2], loadu2_epi64(ref, ref + pitch)));
        ref += pitch2;
        sad2 = _mm_add_epi32(sad2, _mm_sad_epu8(src[3], loadu2_epi64(ref, ref + pitch)));
        sad1 = _mm_add_epi32(sad1, sad2);
        sad2 = _mm_movehl_epi64(sad2, sad1);
        sad2 = _mm_add_epi32(sad2, sad1);
        return _mm_cvtsi128_si32(sad2);
    }

    int32_t sad16(const uint8_t *ref, int32_t pitch, const __m128i *src)
    {
        pitch *= 4;
        const int32_t pitch2 = pitch * 2;
        __m128i sad1, sad2;
        sad1 = _mm_sad_epu8(src[0], loadu_si128(ref));
        sad2 = _mm_sad_epu8(src[1], loadu_si128(ref + pitch));
        ref += pitch2;
        sad1 = _mm_add_epi32(sad1, _mm_sad_epu8(src[2], loadu_si128(ref)));
        sad2 = _mm_add_epi32(sad2, _mm_sad_epu8(src[3], loadu_si128(ref + pitch)));
        sad1 = _mm_add_epi32(sad1, sad2);
        sad2 = _mm_movehl_epi64(sad2, sad1);
        sad2 = _mm_add_epi32(sad2, sad1);
        return _mm_cvtsi128_si32(sad2) * 4;
    }

    int32_t MvCost(const BlockHash &block, const AV1MV &refBlock)
    {
        __m128i b = loadl_epi64(&block);
        b = _mm_shufflelo_epi16(b, 6);
        __m128i r = _mm_cvtsi32_si128(*(int32_t *)&refBlock);
        __m128i diff = _mm_abs_epi16(_mm_sub_epi16(b, r));
        diff = _mm_srli_epi16(diff, 1);
        uint32_t tmp = (uint32_t)_mm_cvtsi128_si32(diff) + 0x00010001;
        int32_t mvbits = ((BSR(tmp & 0xffff) + BSR(tmp >> 16)) << 10) + 1600;
        return mvbits;
        //return (BSR((abs(block.x - refBlock.mvx) >> 1) + 1) << 10) + 800 + (BSR((abs(block.y - refBlock.mvy) >> 1) + 1) << 10) + 800;
    }

#define ENALBE_ONLY_SAD8X8     1
#define ENABLE_8X8_FULLSEARCH  1

    template <typename PixType> int32_t AV1CU<PixType>::CheckIntraBlockCopy(int32_t absPartIdx, int16_t *qcoef, RdCost& rd, uint16_t* varTxInfo, bool search, CostType bestIntraCost)
    {
        const int32_t rasterIdx = av1_scan_z2r4[absPartIdx];
        const int32_t x4 = (rasterIdx & 15);
        const int32_t y4 = (rasterIdx >> 4);
        const int32_t sbx = x4 << 2;
        const int32_t sby = y4 << 2;

        ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
        const BlockSize bsz = mi->sbType;
        if (bsz > BLOCK_16X16)
            return 0;

        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t bsize = num4x4w << 2;

        const BitCounts &bc = *m_currFrame->bitCount;
        const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
        const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);

        const uint8_t *const srcSb = (const uint8_t *)m_ySrc + sbx + sby * SRC_PITCH;

        AV1MV dv_ref = GetMvRefsAV1TU7I(bsz, miRow, miCol);

        if (bsz == BLOCK_8X8 && m_cachedBestSad8x8Raw[miRow & 1][miCol & 1] == 0) {
            mi->mv[0] = m_cachedBestMv8x8;
        }
        else {
            const int32_t active_sb_row = miRow >> 3;
            const int32_t active_sb_col = miCol >> 3;
            const int32_t active_sb_diagonal = active_sb_row + active_sb_col;

            const int32_t sbColStart = m_tileBorders.colStart >> 3;
            const int32_t sbColEnd = (m_tileBorders.colEnd + 7) >> 3;

            int32_t firstInvalidSrcSbRow = active_sb_row;
            int32_t firstInvalidSrcSbCol = active_sb_col;
            for (int32_t i = 0; i < INTRABC_DELAY_SB64; i++) {
                if (--firstInvalidSrcSbCol < sbColStart) {
                    firstInvalidSrcSbCol = sbColEnd - 1;
                    --firstInvalidSrcSbRow;
                }
            }
            int32_t lastValidSrcSbRow = firstInvalidSrcSbRow;
            int32_t lastValidSrcSbCol = firstInvalidSrcSbCol - 1;
            if (lastValidSrcSbCol < sbColStart) {
                lastValidSrcSbCol = sbColEnd - 1;
                --lastValidSrcSbRow;
            }
            if (firstInvalidSrcSbRow < 0) {
                firstInvalidSrcSbRow = 0;
                firstInvalidSrcSbCol = 0;
            }

            BlockHash::Packed2Bytes firstInvalidSrcSb = {};
            firstInvalidSrcSb.x = (uint8_t)firstInvalidSrcSbCol;
            firstInvalidSrcSb.y = (uint8_t)firstInvalidSrcSbRow;

            const int32_t bsize_minus1 = bsize - 1;
            const int32_t lastValidSrcY = std::min((lastValidSrcSbRow + 1) << 6, m_tileBorders.rowEnd << 3) - bsize;
            const int32_t tileIdx = GetTileIndex(m_currFrame->m_tileParam, miRow >> 3, miCol >> 3);
#if ENABLE_ONLY_HASH8
            const int32_t log2w_search = 1;
#else
            const int32_t log2w_search = log2w;
#endif
            const int32_t blockIdx = (miRow >> (log2w_search - 1)) * (m_par->Width >> (log2w_search + 2)) + (miCol >> (log2w_search - 1));
#if !ENABLE_HASH_MEM_REDUCTION
            const uint16_t sumCur = m_currFrame->m_fenc->m_hashIndexes[log2w_search - 1][blockIdx].sum;
#endif
            const uint16_t hashIdx = m_currFrame->m_fenc->m_hashIndexes[log2w_search - 1][blockIdx].hash;

            const int32_t grad = (hashIdx >> 12) & 15;
            const int32_t dc0 = (hashIdx >> 9) & 7;
            const int32_t dc1 = (hashIdx >> 6) & 7;
            const int32_t dc2 = (hashIdx >> 3) & 7;
            const int32_t dc3 = (hashIdx >> 0) & 7;
            const int32_t flatDc = (dc0 == dc1 && dc0 == dc2 && dc0 == dc3);
            if (grad == 0 && flatDc)
                return 0;

            const AV1MV curPos = { int16_t(miCol << 3), int16_t(miRow << 3) };
            const AV1MV refPos = { int16_t(curPos.mvx + (dv_ref.mvx >> 3)), int16_t(curPos.mvy + (dv_ref.mvy >> 3)) };

            const int32_t MAX_INT_PEL_MV = 2047;
            const int32_t MAX_INT_PEL_MV_DIFF = 2048;
            const int32_t maxSrcBlockX = std::min(curPos.mvx + MAX_INT_PEL_MV, refPos.mvx + MAX_INT_PEL_MV_DIFF);
            const int32_t maxSrcBlockY = std::min(std::min(curPos.mvy + MAX_INT_PEL_MV, refPos.mvy + MAX_INT_PEL_MV_DIFF), lastValidSrcY);
            int32_t minSrcBlockX = std::max(curPos.mvx - MAX_INT_PEL_MV, refPos.mvx - MAX_INT_PEL_MV_DIFF);
            int32_t minSrcBlockY = std::max(curPos.mvy - MAX_INT_PEL_MV, refPos.mvy - MAX_INT_PEL_MV_DIFF);

            HashTable &hashTable = m_currFrame->m_fenc->m_hash[log2w_search - 1][tileIdx];
            const BlockHash *bucketBegin = hashTable.GetBucketBegin(hashIdx);

            //hashTable.m_mutex.lock();
            const BlockHash *bucketEnd = hashTable.GetBucketEnd(hashIdx);
            const int32_t bucketSize = hashTable.GetBucketSize(hashIdx);
            const int32_t useReducedSearch = hashTable.GetNumBlocks() > (m_par->Width * m_par->Height >> 1) && bucketSize > 0.0025 * (m_par->Width * m_par->Height);
            //hashTable.m_mutex.unlock();

            if (useReducedSearch) {
                const int32_t MAX_INT_PEL_MVX = 5000 >> 3;
                const int32_t MAX_INT_PEL_MVY = 10000 >> 3;
                minSrcBlockX = std::max(curPos.mvx - MAX_INT_PEL_MVX, minSrcBlockX);
                minSrcBlockY = std::max(curPos.mvy - MAX_INT_PEL_MVY, minSrcBlockY);
            }

            int32_t bestSad = INT_MAX;
            int32_t bestCost = INT_MAX;
            BlockHash bestBlock = {};

            const uint8_t * const recY = m_currFrame->m_recon->y;

            __m128i srcBlock[4];
            if (bsize == 8) {
                srcBlock[0] = loadu2_epi64(srcSb + 0 * SRC_PITCH, srcSb + 1 * SRC_PITCH);
                srcBlock[1] = loadu2_epi64(srcSb + 2 * SRC_PITCH, srcSb + 3 * SRC_PITCH);
                srcBlock[2] = loadu2_epi64(srcSb + 4 * SRC_PITCH, srcSb + 5 * SRC_PITCH);
                srcBlock[3] = loadu2_epi64(srcSb + 6 * SRC_PITCH, srcSb + 7 * SRC_PITCH);
            } else {
                srcBlock[0] = loada_si128(srcSb + 0 * SRC_PITCH);
                srcBlock[1] = loada_si128(srcSb + 4 * SRC_PITCH);
                srcBlock[2] = loada_si128(srcSb + 8 * SRC_PITCH);
                srcBlock[3] = loada_si128(srcSb + 12 * SRC_PITCH);
            }

            auto testDv = [curPos, refPos, srcBlock, recY, this, bsize, active_sb_diagonal, firstInvalidSrcSb, maxSrcBlockY]
                          (AV1MV dv, int32_t *bestCost, int32_t *bestSad, BlockHash *bestBlock)
            {
                bool found = false;
                BlockHash block;
                block.x = curPos.mvx + (dv.mvx >> 3);
                block.y = curPos.mvy + (dv.mvy >> 3);
                if (block.x >= m_tileBorders.colStart * 8 && block.x + bsize < m_tileBorders.colEnd * 8 &&
                    block.y >= m_tileBorders.rowStart * 8 && block.y + bsize - 1 <= maxSrcBlockY)
                {
                    block.srcSb.x = uint8_t((block.x + bsize - 1) >> 6);
                    block.srcSb.y = uint8_t((block.y + bsize - 1) >> 6);
                    if (block.srcSb.x + block.srcSb.y < active_sb_diagonal &&  // wavefront condition (stricter than spec)
                        block.srcSb.asInt16 < firstInvalidSrcSb.asInt16) {     // delay condition (re-formulated to avoid multiplication)

                        const uint8_t *ref = recY + block.x + block.y * m_pitchRecLuma;
#if ENALBE_ONLY_SAD8X8
                        const int32_t sad = (bsize == 8)
                            ? sad8(ref, m_pitchRecLuma, srcBlock)
                            : sad16(ref, m_pitchRecLuma, srcBlock);
#else
                        const int32_t sad = (bsize == 8)
                            ? AV1PP::sad_special_avx2< 8, 8>(ref, m_pitchRecLuma, srcSb)
                            : AV1PP::sad_special_avx2<16, 16>(ref, m_pitchRecLuma, srcSb);
#endif
                        if (*bestCost > sad) {
                            int32_t cost = sad + int32_t(MvCost(block, refPos) * m_rdLambdaSqrt + 0.5f);
                            if (*bestCost > cost) {
                                *bestBlock = block;
                                *bestCost = cost;
                                *bestSad = sad;
                                found = true;
                            }
                        }
                    }
                }
                return found;
            };

            testDv(dv_ref, &bestCost, &bestSad, &bestBlock);
            AV1MV bestMv = dv_ref;
            const ModeInfo *neighMi = mi;
            if (miCol > m_tileBorders.colStart && (neighMi = mi - 1)->interp0 == BILINEAR && neighMi->mv[0]!=bestMv)
                if (testDv(neighMi->mv[0], &bestCost, &bestSad, &bestBlock)) bestMv = neighMi->mv[0];

            if (miRow > m_tileBorders.rowStart && (neighMi = mi - m_par->miPitch)->interp0 == BILINEAR && neighMi->mv[0] != bestMv)
                if (testDv(neighMi->mv[0], &bestCost, &bestSad, &bestBlock)) bestMv = neighMi->mv[0];

            if (miRow > m_tileBorders.rowStart && miCol > m_tileBorders.colStart && (neighMi = mi - m_par->miPitch - 1)->interp0 == BILINEAR && neighMi->mv[0] != bestMv)
                if (testDv(neighMi->mv[0], &bestCost, &bestSad, &bestBlock)) bestMv = neighMi->mv[0];

            if (miRow > m_tileBorders.rowStart && miCol + (num4x4w >> 1) < m_tileBorders.colEnd && (neighMi = mi - m_par->miPitch + (num4x4w >> 1))->interp0 == BILINEAR && neighMi->mv[0] != bestMv)
                if (testDv(neighMi->mv[0], &bestCost, &bestSad, &bestBlock)) bestMv = neighMi->mv[0];

            if (miCol > m_tileBorders.colStart+2 && (neighMi = mi - 3)->interp0 == BILINEAR && neighMi->mv[0] != bestMv)
                if (testDv(neighMi->mv[0], &bestCost, &bestSad, &bestBlock)) bestMv = neighMi->mv[0];

            if (miRow > m_tileBorders.rowStart+2 && (neighMi = mi - 3*m_par->miPitch)->interp0 == BILINEAR && neighMi->mv[0] != bestMv)
                if (testDv(neighMi->mv[0], &bestCost, &bestSad, &bestBlock)) bestMv = neighMi->mv[0];

            if (bsz == BLOCK_8X8 && m_cachedBestSad8x8Rec[miRow & 1][miCol & 1] < bestSad) {
                BlockHash block;
                block.x = uint16_t((miCol << 3) + (m_cachedBestMv8x8.mvx >> 3));
                block.y = uint16_t((miRow << 3) + (m_cachedBestMv8x8.mvy >> 3));
                const int32_t sad = m_cachedBestSad8x8Rec[miRow & 1][miCol & 1];
                const int32_t cost = sad + int32_t(MvCost(block, refPos) * m_rdLambdaSqrt + 0.5f);
                if (bestCost > cost) {
                    bestBlock = block;
                    bestCost = cost;
                    bestSad = sad;
                }
            }

            int32_t srcSad = INT_MAX;

            if (bestCost != INT_MAX) {
            const uint8_t *srcref = m_currFrame->m_origin->y + bestBlock.x + bestBlock.y * m_currFrame->m_origin->pitch_luma_pix;
#if ENALBE_ONLY_SAD8X8
                srcSad = (bsize == 8)
                    ? sad8(srcref, m_currFrame->m_origin->pitch_luma_pix, srcBlock)
                    : sad16(srcref, m_currFrame->m_origin->pitch_luma_pix, srcBlock);
#else
                srcSad = (bsize == 8)
                ? AV1PP::sad_special_avx2< 8, 8>(srcref, m_currFrame->m_origin->pitch_luma_pix, srcSb)
                : AV1PP::sad_special_avx2<16, 16>(srcref, m_currFrame->m_origin->pitch_luma_pix, srcSb);
#endif
            }

            if (srcSad != 0 && bestSad > 0.186278f*m_aqparamY.dequant[0] && search) {
                // skip candidates which are beyond left and top borders of valid area
                for (; bucketBegin != bucketEnd && (bucketBegin->y < minSrcBlockY || bucketBegin->x < minSrcBlockX); bucketBegin++);

                BlockHash::Packed2Bytes currSrcSb;
                currSrcSb.asInt16 = 0xffff;

                const int32_t allowEarlyExit = bsize == 16 && bestCost < 10000 && (bucketSize > 20000);
                int32_t bestCostInit = bestCost;
                if (bsize == 8) {
#if ENABLE_8X8_FULLSEARCH
                    for (const BlockHash *bh = bucketBegin; bh != bucketEnd; bh++) {

                        if (currSrcSb.asInt16 != bh->srcSb.asInt16) {
                            currSrcSb.asInt16 = bh->srcSb.asInt16;
                            if (currSrcSb.asInt16 >= firstInvalidSrcSb.asInt16) // delay condition (re-formulated to avoid multiplication))
                                break;

                            while (bh != bucketEnd && bh->y <= maxSrcBlockY && (
                                bh->x < minSrcBlockX || bh->x > maxSrcBlockX ||
                                currSrcSb.y + int32_t(currSrcSb.x) >= active_sb_diagonal)) { // wavefront condition (stricter than spec)
                                // skip one sb
                                for (bh++; bh != bucketEnd && bh->srcSb.asInt16 == currSrcSb.asInt16; bh++);
                                currSrcSb.asInt16 = bh->srcSb.asInt16;
                            }

                            if (bh == bucketEnd || bh->y > maxSrcBlockY ||
                                currSrcSb.asInt16 >= firstInvalidSrcSb.asInt16) // delay condition (re-formulated to avoid multiplication))
                                break;
                        }
#if !ENABLE_HASH_MEM_REDUCTION
                        if (abs(bh->sum - sumCur) > bestSad)
                            continue;
#endif
                        const uint8_t *ref = recY + bh->x + bh->y * m_pitchRecLuma;
    #if ENALBE_ONLY_SAD8X8
                        const int32_t sad = sad8(ref, m_pitchRecLuma, srcBlock);
    #else
                        const int32_t sad = AV1PP::sad_special_avx2< 8, 8>(ref, m_pitchRecLuma, (const uint8_t *)srcSb);
    #endif

                        if (bestCost > sad) {
                            int32_t cost = sad + int32_t(MvCost(*bh, refPos) * m_rdLambdaSqrt + 0.5f);
                            if (bestCost > cost) {
                                bestCost = cost;
                                bestSad = sad;
                                bestBlock = *bh;

                                if (allowEarlyExit && 2 * bestCostInit > 3 * bestCost) // still there is some room to speedup
                                    break;
                            }
                        }
                    }
#endif
                } else {
                    assert(bsize == 16);
                for (const BlockHash *bh = bucketBegin; bh != bucketEnd; bh++)
                {
                    if (currSrcSb.asInt16 != (int16_t)(((bh->x + bsize - 1) >> 6) + (((bh->y + bsize - 1) >> 6) << 8))) {
                        currSrcSb.asInt16 = (int16_t)(((bh->x + bsize - 1) >> 6) + (((bh->y + bsize - 1) >> 6) << 8));
                        if (currSrcSb.asInt16 >= firstInvalidSrcSb.asInt16) // delay condition (re-formulated to avoid multiplication))
                            break;

                            while (bh != bucketEnd && bh->y <= maxSrcBlockY && (
                            bh->x < minSrcBlockX || bh->x > maxSrcBlockX ||
                            currSrcSb.y + int32_t(currSrcSb.x) >= active_sb_diagonal))
                            { // wavefront condition (stricter than spec)
                            // skip one sb
                                for (bh++; bh != bucketEnd && /*bh->srcSb.asInt16 == currSrcSb.asInt16*/((int16_t)(((bh->x + bsize - 1) >> 6) + (((bh->y + bsize - 1) >> 6) << 8)) == currSrcSb.asInt16); bh++);
                                //currSrcSb.asInt16 = bh->srcSb.asInt16;
                                currSrcSb.asInt16 = (int16_t)(((bh->x + bsize - 1) >> 6) + (((bh->y + bsize - 1) >> 6) << 8));
                            }

                        if (bh == bucketEnd || bh->y > maxSrcBlockY ||
                            currSrcSb.asInt16 >= firstInvalidSrcSb.asInt16) // delay condition (re-formulated to avoid multiplication))
                            break;
                    }

#if ENABLE_ONLY_HASH8
                    // one more check (tile border)
                    {
                        int max_x = bh->x + bsize - 1;
                        int max_y = bh->y + bsize - 1;
                        if (max_x >= m_tileBorders.colEnd * 8 || max_y >= m_tileBorders.rowEnd * 8) {
                            continue;
                        }
                    }
#endif
#if !ENABLE_HASH_MEM_REDUCTION
                    if (abs(bh->sum - sumCur) > bestSad)
                        continue;
#endif
                    const uint8_t *ref = recY + bh->x + bh->y * m_pitchRecLuma;
#if ENALBE_ONLY_SAD8X8
                    const int32_t sad = sad16(ref, m_pitchRecLuma, srcBlock);
#else
                    const int32_t sad = AV1PP::sad_special_avx2<16, 16>(ref, m_pitchRecLuma, (const uint8_t *)srcSb);
#endif

                    if (bestCost > sad) {
                        int32_t cost = sad + int32_t(MvCost(*bh, refPos) * m_rdLambdaSqrt + 0.5f);
                        if (bestCost > cost) {
                            bestCost = cost;
                            bestSad = sad;
                            bestBlock = *bh;

                            if (allowEarlyExit && 2 * bestCostInit > 3 * bestCost) // still there is some room to speedup
                                break;
                        }
                    }
                }
            }
            }
            if (bestCost == INT_MAX)
                return 0;

            int16_t mvx = mi->mv[0].mvx = (bestBlock.x - curPos.mvx) << 3;
            int16_t mvy = mi->mv[0].mvy = (bestBlock.y - curPos.mvy) << 3;
            int16_t dmvx = mvx - dv_ref.mvx;
            int16_t dmvy = mvy - dv_ref.mvy;

            if ((dmvx < -MAX_INT_PEL_MV_DIFF * 8 || dmvx > MAX_INT_PEL_MV_DIFF * 8) || (dmvy < -MAX_INT_PEL_MV_DIFF * 8 || dmvy > MAX_INT_PEL_MV_DIFF * 8)) {
                return 0;
            }
        }

        if (bsz == BLOCK_16X16) {
            const int32_t bx = miCol * 8 + (mi->mv[0].mvx >> 3);
            const int32_t by = miRow * 8 + (mi->mv[0].mvy >> 3);
            const int32_t rawPitch = m_currFrame->m_origin->pitch_luma_pix;
            const int32_t refPitch = m_pitchRecLuma;
            const uint8_t *raw = m_currFrame->m_origin->y + bx + by * rawPitch;
            const uint8_t *ref = m_currFrame->m_recon->y + bx + by * refPitch;

            m_cachedBestMv8x8 = mi->mv[0];
            m_cachedBestSad8x8Raw[0][0] = AV1PP::sad_special_avx2<8, 8>(raw, rawPitch, srcSb);
            m_cachedBestSad8x8Rec[0][0] = AV1PP::sad_special_avx2<8, 8>(ref, refPitch, srcSb);
            m_cachedBestSad8x8Raw[0][1] = AV1PP::sad_special_avx2<8, 8>(raw + 8, rawPitch, srcSb + 8);
            m_cachedBestSad8x8Rec[0][1] = AV1PP::sad_special_avx2<8, 8>(ref + 8, refPitch, srcSb + 8);
            m_cachedBestSad8x8Raw[1][0] = AV1PP::sad_special_avx2<8, 8>(raw + 8 * rawPitch, rawPitch, srcSb + 8 * SRC_PITCH);
            m_cachedBestSad8x8Rec[1][0] = AV1PP::sad_special_avx2<8, 8>(ref + 8 * refPitch, refPitch, srcSb + 8 * SRC_PITCH);
            m_cachedBestSad8x8Raw[1][1] = AV1PP::sad_special_avx2<8, 8>(raw + 8 * rawPitch + 8, rawPitch, srcSb + 8 * SRC_PITCH + 8);
            m_cachedBestSad8x8Rec[1][1] = AV1PP::sad_special_avx2<8, 8>(ref + 8 * refPitch + 8, refPitch, srcSb + 8 * SRC_PITCH + 8);
        }

        mi->mvd[0].mvx = mi->mv[0].mvx - dv_ref.mvx;
        mi->mvd[0].mvy = mi->mv[0].mvy - dv_ref.mvy;

        AV1MV dv_ref_ = { dv_ref.mvx >> 3, dv_ref.mvy >> 3 };
        AV1MV dv_ = { mi->mv[0].mvx >> 3, mi->mv[0].mvy >> 3 };

        const ModeInfo *above = GetAbove(mi, m_par->miPitch, miRow, m_tileBorders.rowStart);
        const ModeInfo *left = GetLeft(mi, miCol, m_tileBorders.colStart);
        const int32_t ctxSkip = GetCtxSkip(above, left);
        const uint16_t *skipBits = bc.skip[ctxSkip];

        uint32_t modeBits = MvCost(dv_, dv_ref_, 0);
        modeBits += bc.intrabc_cost[1];

        if ((modeBits + skipBits[1]) * m_rdLambda > bestIntraCost)
            return 0;

        const int32_t intx = mi->mv[0].mvx >> 3;
        const int32_t inty = mi->mv[0].mvy >> 3;

        uint8_t *recSb = (uint8_t *)m_yRec + sbx + sby * m_pitchRecLuma;
        const uint8_t *bestPredY = recSb + intx + inty * m_pitchRecLuma;
        uint8_t *rec = (uint8_t *)vp9scratchpad.recY[0];
        int16_t *diff = vp9scratchpad.diffY;
        CopyFromUnalignedNxN(bestPredY, m_pitchRecLuma, rec, 64, bsize);
        AV1PP::diff_nxn(srcSb, rec, diff, log2w);

        uint8_t *anz = m_contexts.aboveNonzero[0] + x4;
        uint8_t *lnz = m_contexts.leftNonzero[0] + y4;
        uint8_t *aboveTxfmCtx = m_contexts.aboveTxfm + x4;
        uint8_t *leftTxfmCtx = m_contexts.leftTxfm + y4;

        rd = TransformInterYSbVarTxByRdo(
            bsz, anz, lnz,
            srcSb, rec, diff, qcoef,
            m_lumaQp, bc, m_rdLambda,
            aboveTxfmCtx, leftTxfmCtx,
            varTxInfo,
            m_aqparamY, m_roundFAdjY,
            vp9scratchpad.varTxCoefs, 0, 0);

        CopyNxN(rec, 64, recSb, m_pitchRecLuma, bsize);

        mi->skip = (rd.eob == 0);

        rd.modeBits += modeBits;
        rd.modeBits += skipBits[mi->skip];
        rd.sse = AV1PP::sse(srcSb, SRC_PITCH, recSb, m_pitchRecLuma, log2w, log2w);

        return 1;
    }

    template int32_t AV1CU<uint8_t>::CheckIntraBlockCopy(int32_t, int16_t *qcoef, RdCost& rd, uint16_t* varTxInfo, bool search, CostType bestCost);
#if ENABLE_10BIT
    template int32_t AV1CU<uint16_t>::CheckIntraBlockCopy(int32_t, int16_t *qcoef, RdCost& rd, uint16_t* varTxInfo);
#endif

    template <typename PixType> int32_t AV1CU<PixType>::TransformInterChroma(int32_t absPartIdx, uint16_t* varTxInfo)
    {
        const int32_t i = absPartIdx;
        const int32_t rasterIdx = av1_scan_z2r4[i];
        const int32_t x4 = (rasterIdx & 15);
        const int32_t y4 = (rasterIdx >> 4);
        const int32_t sbx = x4 << 2;
        const int32_t sby = y4 << 2;
        ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * m_par->miPitch;
        const BlockSize bsz = mi->sbType;
        const int32_t log2w = block_size_wide_4x4_log2[bsz];
        const int32_t log2h = block_size_high_4x4_log2[bsz];
        const int32_t num4x4w = 1 << log2w;
        const int32_t num4x4h = 1 << log2h;
        const BitCounts &bc = *m_currFrame->bitCount;

        int32_t uvEob = 1;

        const QuantParam(&qparamUv)[2] = m_aqparamUv;
        float *roundFAdjUv[2] = { &m_roundFAdjUv[0][0], &m_roundFAdjUv[1][0] };

        const int32_t subX = m_par->subsamplingX;
        const int32_t subY = m_par->subsamplingY;
        const BlockSize bszUv = ss_size_lookup[bsz][m_par->subsamplingX][m_par->subsamplingY];
        const int32_t txSizeUv = max_txsize_rect_lookup[bszUv];

        const int32_t uvsbx = sbx >> 1;
        const int32_t uvsby = sby >> 1;
        const int32_t uvsbw = (num4x4w << 2) >> 1;
        const int32_t uvsbh = (num4x4h << 2) >> 1;

        const PixType *srcUv = m_uvSrc + uvsbx * 2 + uvsby * SRC_PITCH;
        PixType *recUv = m_uvRec + uvsbx * 2 + uvsby * m_pitchRecChroma;

        PixType *predUv = vp9scratchpad.predUv[0];
        int16_t *diffU = vp9scratchpad.diffU;
        int16_t *diffV = vp9scratchpad.diffV;
        int16_t *coefU = vp9scratchpad.coefU;
        int16_t *coefV = vp9scratchpad.coefV;

        int16_t *qcoefU = m_coeffWorkU + i * 4;
        int16_t *qcoefV = m_coeffWorkV + i * 4;

        uint8_t *anzU = m_contexts.aboveNonzero[1] + (uvsbx >> 2);
        uint8_t *anzV = m_contexts.aboveNonzero[2] + (uvsbx >> 2);
        uint8_t *lnzU = m_contexts.leftNonzero[1] + (uvsby >> 2);
        uint8_t *lnzV = m_contexts.leftNonzero[2] + (uvsby >> 2);

        const PixType *ref0Uv = recUv;
        InterpolateAv1SingleRefChroma(ref0Uv, m_pitchRecChroma, predUv, mi->mv[0], uvsbh, log2w - 1, BILINEAR + (BILINEAR << 4));

        AV1PP::diff_nv12(srcUv, predUv, diffU, diffV, uvsbh, log2w - 1);
        CopyNxM(predUv, PRED_PITCH, recUv, m_pitchRecChroma, uvsbw << 1, uvsbh);

        int16_t *coefWork = (int16_t *)vp9scratchpad.varTxCoefs;
        uvEob = TransformInterUvSbTxType(
            (BlockSize)bszUv, (TxSize)txSizeUv, DCT_DCT, anzU, anzV, lnzU, lnzV, srcUv,
            recUv, m_pitchRecChroma, diffU, diffV, coefU, coefV, qcoefU,
            qcoefV, coefWork, *m_par, bc, m_chromaQp,
            varTxInfo,
            qparamUv, &roundFAdjUv[0],
            m_rdLambda, 1);

        return uvEob;
    }

    template int32_t AV1CU<uint8_t>::TransformInterChroma(int32_t absPartIdx, uint16_t* varTxInfo);
#if ENABLE_10BIT
    template int32_t AV1CU<uint16_t>::TransformInterChroma(int32_t absPartIdx, uint16_t* varTxInfo);
#endif

} // namespace

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE